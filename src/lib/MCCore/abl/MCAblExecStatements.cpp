#include "stdafx.h"
#include "abl/MCAblDebugger.h"
#include "abl/MCAblRoutines.h"
#include "abl/MCAblRuntime.h"
#include "abl/MCAblSymbolTable.h"

// Executing crunched statements, calls of declared functions (with the profile log) and the control structures.

auto MCAblRuntime::ExecStatement() -> void
{
    if (_Token == MCAblToken::StatementMarker)
    {
        _LineNumber = GetCodeStatementMarker();
        _StatementCount++;
        _StatementStart = _CodePosition;

        if (_Debugger)
        {
            _Debugger->TraceStatementExecution();
        }

        GetCodeToken();
    }

    switch (_Token)
    {
        case MCAblToken::Identifier:
        {
            const MCAblSymbol& idPtr = GetCodeSymbol();

            if (idPtr.Defn.Key == MCAblSymbolKind::Function)
            {
                // A function called as a statement: drop its result.
                if (ExecRoutineCall(idPtr) != MCAblTypeId::None)
                {
                    Pop();
                }
            }
            else
            {
                ExecAssignmentStatement(idPtr);
            }
            break;
        }

        case MCAblToken::Code:
        {
            GetCodeToken();

            // Original behaviour: the end token was chosen by curLibrary, which is only set while a library compiles,
            // so a block always ends at endfunction or endmodule.
            while (_Token != MCAblToken::EndFunction && _Token != MCAblToken::EndModule)
            {
                ExecStatement();
            }

            GetCodeToken();
            break;
        }

        case MCAblToken::Switch:
        {
            ExecSwitchStatement();
            break;
        }

        case MCAblToken::For:
        {
            ExecForStatement();
            break;
        }

        case MCAblToken::If:
        {
            ExecIfStatement();
            break;
        }

        case MCAblToken::Repeat:
        {
            ExecRepeatStatement();
            break;
        }

        case MCAblToken::While:
        {
            ExecWhileStatement();
            break;
        }

        case MCAblToken::Semicolon:
        case MCAblToken::Else:
        case MCAblToken::Until:
        {
            break;
        }

        default:
        {
            RuntimeError(MCAblRuntimeError::UnimplementedFeature);
        }
    }

    while (_Token == MCAblToken::Semicolon)
    {
        GetCodeToken();
    }
}

auto MCAblRuntime::ExecAssignmentStatement(const MCAblSymbol& idPtr) -> void
{
    const MCAblTypeId targetTypePtr = ExecVariable(idPtr, MCAblUse::Target);
    const MCAblAddress targetPtr = Top().Address;
    Pop();

    GetCodeToken();
    const MCAblTypeId expressionTypePtr = ExecExpression();

    // The target is a stack slot or an element in array memory: stores go through 4-byte integers and reals (1
    // byte for a char), as in the original.
    if (targetTypePtr == MCAblTypeId::Real && expressionTypePtr == MCAblTypeId::Integer)
    {
        Store(targetPtr, static_cast<float>(Top().Integer));
    }
    else if (const MCAblType& targetType = Type(targetTypePtr); targetType.Form == MCAblTypeForm::Array)
    {
        CopyBytes(targetPtr, Top().Address, targetType.Size);
    }
    else if (targetTypePtr == MCAblTypeId::Integer || targetType.Form == MCAblTypeForm::Enum)
    {
        Store(targetPtr, Top().Integer);
    }
    else if (targetTypePtr == MCAblTypeId::Char)
    {
        Store(targetPtr, Top().Byte);
    }
    else
    {
        Store(targetPtr, Top().Real);
    }

    Pop();

    if (_Debugger)
    {
        _Debugger->TraceDataStore(idPtr, idPtr.Type, targetPtr, targetTypePtr);
    }
}

auto MCAblRuntime::ExecRoutineCall(const MCAblSymbol& routineIdPtr) -> MCAblTypeId
{
    if (routineIdPtr.Defn.Info.Routine.Key == MCAblRoutineKey::Declared)
    {
        return ExecDeclaredRoutineCall(routineIdPtr);
    }

    return ExecStandardRoutineCall(*this, routineIdPtr.Defn.Info.Routine.Key);
}

auto MCAblRuntime::ExecDeclaredRoutineCall(const MCAblSymbol& routineIdPtr) -> MCAblTypeId
{
    const int32_t oldLevel = _Level;
    const int32_t newLevel = routineIdPtr.Level + 1;

    if (_CallDepth >= MaxCallDepth)
    {
        RuntimeError(MCAblRuntimeError::StackOverflow);
    }

    _CallDepth++;

    // A function of another module (a library's) has no static link and runs in that module.
    const int32_t newFrame = _Tos + 1;
    const bool isLibraryCall =
        routineIdPtr.Library != MCAblLibraryId::None && routineIdPtr.Library != Symbol(_Routine).Library;

    if (isLibraryCall)
    {
        PushStackFrameHeader(-1, -1);
    }
    else
    {
        PushStackFrameHeader(_Level, newLevel);
    }

    GetCodeToken();

    if (_Token == MCAblToken::LParen)
    {
        ExecActualParams(routineIdPtr);
        GetCodeToken();
    }

    _Frame = newFrame;
    _Level = newLevel;
    StackItem(newFrame + std::to_underlying(MCAblFrameItem::ReturnAddress)).Address =
        MCAblAddress::Of(MCAblSpace::Code, std::to_underlying(_SegmentId), _CodePosition - 1);

    std::shared_ptr<MCAblModule> callerModule;

    if (isLibraryCall)
    {
        std::shared_ptr<MCAblModule> library =
            _Libraries[static_cast<size_t>(std::to_underlying(routineIdPtr.Library) - 1)];
        callerModule = _Module;
        _CallModuleInit = !library->_InitCalled;
        library->_InitCalled = true;
        SetModule(std::move(library));
    }

    if (!_ProfileLog)
    {
        Execute(routineIdPtr);
    }
    else
    {
        const uint32_t startTime = MCPort::Milliseconds();
        Execute(routineIdPtr);
        const auto runTime = static_cast<int32_t>(MCPort::Milliseconds() - startTime);

        if (runTime > ProfileLogFunctionTimeLimit)
        {
            LogCall(routineIdPtr, runTime);
        }
    }

    if (isLibraryCall)
    {
        SetModule(std::move(callerModule));
    }

    _Level = oldLevel;
    GetCodeToken();
    _CallDepth--;
    return routineIdPtr.Type;
}

auto MCAblRuntime::ExecActualParams(const MCAblSymbol& routineIdPtr) -> void
{
    for (MCAblSymbolId formal = routineIdPtr.Defn.Info.Routine.Params; formal != MCAblSymbolId::None;
         formal = Symbol(formal).Next)
    {
        const MCAblSymbol& formalIdPtr = Symbol(formal);
        const MCAblTypeId formalTypePtr = formalIdPtr.Type;
        GetCodeToken();

        if (formalIdPtr.Defn.Key == MCAblSymbolKind::ValueParam)
        {
            const MCAblTypeId actualTypePtr = ExecExpression();

            if (formalTypePtr == MCAblTypeId::Real && actualTypePtr == MCAblTypeId::Integer)
            {
                Top().Real = static_cast<float>(Top().Integer);
            }

            // An array passed by value gets its own copy.
            if (const MCAblType& formalType = Type(formalTypePtr); formalType.Form == MCAblTypeForm::Array)
            {
                const MCAblAddress source = Top().Address;
                const MCAblAddress copy = AllocateArray(formalType.Size, "actual array param");
                CopyBytes(copy, source, formalType.Size);
                Top().Address = copy;
            }
        }
        else
        {
            ExecVariable(GetCodeSymbol(), MCAblUse::RefParam);
        }
    }
}

auto MCAblRuntime::ExecStatementsUntil(MCAblToken end) -> bool
{
    while (_Token != end)
    {
        ExecStatement();

        if (_ExitWithReturn)
        {
            return false;
        }
    }

    return true;
}

auto MCAblRuntime::CountLoopIteration(int32_t& iterations) -> void
{
    iterations++;

    if (iterations == _MaxLoopIterations)
    {
        RuntimeError(MCAblRuntimeError::InfiniteLoop);
    }
}

auto MCAblRuntime::ExecSwitchStatement() -> void
{
    GetCodeToken();
    const int32_t branchTableLocation = GetCodeAddressMarker();

    GetCodeToken();
    const MCAblTypeId switchExpressionTypePtr = ExecExpression();
    const int32_t switchExpressionValue =
        (switchExpressionTypePtr == MCAblTypeId::Integer || Type(switchExpressionTypePtr).Form == MCAblTypeForm::Enum)
            ? Top().Integer
            : Top().Byte;
    Pop();

    // The branch table: a count, then (label value, case location) pairs.
    _CodePosition = branchTableLocation;
    GetCodeToken();
    const int32_t caseLabelCount = GetCodeInteger();
    std::optional<int32_t> caseLocation;

    for (int32_t i = 0; i < caseLabelCount && !caseLocation; i++)
    {
        const int32_t labelValue = GetCodeInteger();
        const int32_t location = GetCodeAddress();

        if (labelValue == switchExpressionValue)
        {
            caseLocation = location;
        }
    }

    // No matching case: skip past the table.
    if (!caseLocation)
    {
        GetCodeToken();
        GetCodeToken();
        return;
    }

    _CodePosition = *caseLocation;
    GetCodeToken();

    if (!ExecStatementsUntil(MCAblToken::EndCase))
    {
        return;
    }

    GetCodeToken();
    GetCodeToken();
    _CodePosition = GetCodeAddressMarker();
    GetCodeToken();
}

auto MCAblRuntime::ExecForStatement() -> void
{
    GetCodeToken();
    const int32_t loopEndLocation = GetCodeAddressMarker();

    GetCodeToken();
    const MCAblSymbol& controlIdPtr = GetCodeSymbol();
    const MCAblTypeId controlTypePtr = ExecVariable(controlIdPtr, MCAblUse::Target);
    const MCAblAddress controlAddress = Top().Address;
    Pop();

    GetCodeToken();
    ExecExpression();
    const int32_t initialValue = controlTypePtr == MCAblTypeId::Integer ? Top().Integer : Top().Byte;
    Pop();

    const bool countUp = _Token == MCAblToken::To;
    GetCodeToken();
    ExecExpression();
    const int32_t finalValue = controlTypePtr == MCAblTypeId::Integer ? Top().Integer : Top().Byte;
    Pop();

    const int32_t loopStartLocation = _CodePosition;
    int32_t iterations = 0;
    // Counted in unsigned arithmetic, as in the original, so the step past INT_MAX wraps.
    auto controlValue = static_cast<uint32_t>(initialValue);

    while (countUp ? static_cast<int32_t>(controlValue) <= finalValue
                   : finalValue <= static_cast<int32_t>(controlValue))
    {
        _CodePosition = loopStartLocation;

        if (controlTypePtr == MCAblTypeId::Integer)
        {
            Store(controlAddress, controlValue);
        }
        else
        {
            Store(controlAddress, static_cast<char>(controlValue));
        }

        GetCodeToken();

        if (!ExecStatementsUntil(MCAblToken::EndFor))
        {
            return;
        }

        CountLoopIteration(iterations);
        controlValue += countUp ? 1u : static_cast<uint32_t>(-1);
    }

    _CodePosition = loopEndLocation;
    GetCodeToken();
}

auto MCAblRuntime::ExecIfStatement() -> void
{
    GetCodeToken();
    const int32_t falseLocation = GetCodeAddressMarker();

    GetCodeToken();
    ExecExpression();
    const int32_t test = Top().Integer;
    Pop();

    if (test == 1)
    {
        // The THEN part, up to END_IF or ELSE (then jump past the ELSE part).
        GetCodeToken();

        while (_Token != MCAblToken::EndIf)
        {
            if (_Token == MCAblToken::Else)
            {
                GetCodeToken();
                _CodePosition = GetCodeAddressMarker();
                GetCodeToken();
                break;
            }

            ExecStatement();

            if (_ExitWithReturn)
            {
                return;
            }
        }
    }
    else
    {
        _CodePosition = falseLocation;
        GetCodeToken();

        if (_Token == MCAblToken::Else)
        {
            GetCodeToken();
            GetCodeAddressMarker();
            GetCodeToken();

            if (!ExecStatementsUntil(MCAblToken::EndIf))
            {
                return;
            }
        }
    }

    GetCodeToken();
}

auto MCAblRuntime::ExecRepeatStatement() -> void
{
    const int32_t loopStartLocation = _CodePosition;
    int32_t iterations = 0;

    do
    {
        GetCodeToken();

        if (!ExecStatementsUntil(MCAblToken::Until))
        {
            return;
        }

        CountLoopIteration(iterations);
        GetCodeToken();
        ExecExpression();

        if (Top().Integer == 0)
        {
            _CodePosition = loopStartLocation;
        }

        Pop();
    } while (_CodePosition == loopStartLocation);
}

auto MCAblRuntime::ExecWhileStatement() -> void
{
    GetCodeToken();
    const int32_t loopEndLocation = GetCodeAddressMarker();
    const int32_t loopStartLocation = _CodePosition;
    int32_t iterations = 0;

    for (;;)
    {
        GetCodeToken();
        ExecExpression();
        const bool loopDone = Top().Integer == 0;
        Pop();

        if (loopDone)
        {
            _CodePosition = loopEndLocation;
            break;
        }

        GetCodeToken();

        if (!ExecStatementsUntil(MCAblToken::EndWhile))
        {
            return;
        }

        iterations++;
        _CodePosition = loopStartLocation;

        if (iterations == _MaxLoopIterations)
        {
            RuntimeError(MCAblRuntimeError::InfiniteLoop);
        }
    }

    GetCodeToken();
}

auto MCAblRuntime::ExecReturn() -> void
{
    _ReturnValue = MCAblStackItem{};
    const MCAblTypeId returnTypePtr = Symbol(_Routine).Type;

    if (returnTypePtr != MCAblTypeId::None)
    {
        const int32_t framePtr = CurrentRoutineFrame();
        MCAblStackItem& functionValue = StackItem(framePtr);
        GetCodeToken();
        GetCodeToken();
        const MCAblTypeId expressionTypePtr = ExecExpression();

        if (returnTypePtr == MCAblTypeId::Real && expressionTypePtr == MCAblTypeId::Integer)
        {
            functionValue.Real = static_cast<float>(Top().Integer);
        }
        else if (const MCAblType& returnType = Type(returnTypePtr); returnType.Form == MCAblTypeForm::Array)
        {
            // Original behaviour: the array is copied over the frame's function value slot (and past it).
            CopyBytes(MCAblAddress::StackItem(framePtr), Top().Address, returnType.Size);
        }
        else if (returnTypePtr == MCAblTypeId::Integer || returnType.Form == MCAblTypeForm::Enum)
        {
            functionValue.Integer = Top().Integer;
        }
        else
        {
            functionValue.Real = Top().Real;
        }

        Pop();
        _ReturnValue.Real = functionValue.Real;

        if (_Debugger)
        {
            const MCAblSymbol& routine = Symbol(_Routine);
            _Debugger->TraceDataStore(routine, routine.Type, MCAblAddress::StackItem(framePtr), returnTypePtr);
        }
    }

    // Continue at the end of a routine: the code a return jumps to.
    GetCodeToken();
    JumpTo(MCAblSegmentId::ExitRoutine, 0);
    _ExitWithReturn = true;
    GetCodeToken();
}
