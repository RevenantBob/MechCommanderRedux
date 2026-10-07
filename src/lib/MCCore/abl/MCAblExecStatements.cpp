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
        _StatementStart = _Code;

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
            MCAblSymbol* idPtr = GetCodeSymbol();

            if (idPtr->Defn.Key == MCAblSymbolKind::Function)
            {
                // A function called as a statement: drop its result.
                if (ExecRoutineCall(idPtr))
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

auto MCAblRuntime::ExecAssignmentStatement(MCAblSymbol* idPtr) -> void
{
    MCAblType* targetTypePtr = ExecVariable(idPtr, MCAblUse::Target);
    auto* targetPtr = reinterpret_cast<MCAblStackItem*>(_Tos->Address);
    Pop();

    GetCodeToken();
    MCAblType* expressionTypePtr = ExecExpression();

    // The target is a stack slot or an element in array memory: stores go through 4-byte integers and reals (1
    // byte for a char), as in the original.
    if (targetTypePtr == RealTypePtr && expressionTypePtr == IntegerTypePtr)
    {
        *reinterpret_cast<float*>(targetPtr) = static_cast<float>(_Tos->Integer);
    }
    else if (targetTypePtr->Form == MCAblTypeForm::Array)
    {
        std::memcpy(targetPtr, _Tos->Address, static_cast<size_t>(targetTypePtr->Size));
    }
    else if (targetTypePtr == IntegerTypePtr || targetTypePtr->Form == MCAblTypeForm::Enum)
    {
        *reinterpret_cast<int32_t*>(targetPtr) = _Tos->Integer;
    }
    else if (targetTypePtr == CharTypePtr)
    {
        *reinterpret_cast<uint8_t*>(targetPtr) = _Tos->Byte;
    }
    else
    {
        *reinterpret_cast<float*>(targetPtr) = _Tos->Real;
    }

    Pop();

    if (_Debugger)
    {
        _Debugger->TraceDataStore(idPtr, idPtr->TypePtr, targetPtr, targetTypePtr);
    }
}

auto MCAblRuntime::ExecRoutineCall(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    if (routineIdPtr->Defn.Info.Routine.Key == MCAblRoutineKey::Declared)
    {
        return ExecDeclaredRoutineCall(routineIdPtr);
    }

    return ExecStandardRoutineCall(*this, routineIdPtr->Defn.Info.Routine.Key);
}

auto MCAblRuntime::ExecDeclaredRoutineCall(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    const int32_t oldLevel = _Level;
    const int32_t newLevel = routineIdPtr->Level + 1;
    _CallDepth++;

    // A function of another module (a library's) has no static link and runs in that module.
    MCAblStackItem* newFrame = _Tos + 1;
    const bool isLibraryCall = routineIdPtr->Library && routineIdPtr->Library != _Routine->Library;

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
    reinterpret_cast<MCAblStackFrameHeader*>(newFrame)->ReturnAddress.Address = _Code - 1;

    MCAblModule* callerModule = nullptr;

    if (isLibraryCall)
    {
        MCAblModule* library = routineIdPtr->Library;
        callerModule = _Module;
        _ModuleHandle = library->_Handle;
        _Module = library;

        if (_Debugger)
        {
            _Debugger->SetModule(library);
        }

        _StaticData = library->_StaticData.empty() ? nullptr : library->_StaticData.data();
        _CallModuleInit = !library->_InitCalled;
        library->_InitCalled = true;
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
        _ModuleHandle = callerModule->_Handle;
        _Module = callerModule;

        if (_Debugger)
        {
            _Debugger->SetModule(callerModule);
        }

        _StaticData = callerModule->_StaticData.empty() ? nullptr : callerModule->_StaticData.data();
    }

    _Level = oldLevel;
    GetCodeToken();
    _CallDepth--;
    return routineIdPtr->TypePtr;
}

auto MCAblRuntime::ExecActualParams(MCAblSymbol* routineIdPtr) -> void
{
    for (MCAblSymbol* formalIdPtr = routineIdPtr->Defn.Info.Routine.Params; formalIdPtr;
         formalIdPtr = formalIdPtr->Next)
    {
        MCAblType* formalTypePtr = formalIdPtr->TypePtr;
        GetCodeToken();

        if (formalIdPtr->Defn.Key == MCAblSymbolKind::ValueParam)
        {
            MCAblType* actualTypePtr = ExecExpression();

            if (formalTypePtr == RealTypePtr && actualTypePtr == IntegerTypePtr)
            {
                _Tos->Real = static_cast<float>(_Tos->Integer);
            }

            // An array passed by value gets its own copy.
            if (formalTypePtr->Form == MCAblTypeForm::Array)
            {
                const MCAddress source = _Tos->Address;
                const MCAddress copy = AllocateArray(formalTypePtr->Size, "actual array param");
                std::memcpy(copy, source, static_cast<size_t>(formalTypePtr->Size));
                _Tos->Address = copy;
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
    char* branchTableLocation = GetCodeAddressMarker();

    GetCodeToken();
    MCAblType* switchExpressionTypePtr = ExecExpression();
    const int32_t switchExpressionValue =
        (switchExpressionTypePtr == IntegerTypePtr || switchExpressionTypePtr->Form == MCAblTypeForm::Enum)
            ? _Tos->Integer
            : _Tos->Byte;
    Pop();

    // The branch table: a count, then (label value, case location) pairs.
    _Code = branchTableLocation;
    GetCodeToken();
    const int32_t caseLabelCount = GetCodeInteger();
    char* caseLocation = nullptr;

    for (int32_t i = 0; i < caseLabelCount && !caseLocation; i++)
    {
        const int32_t labelValue = GetCodeInteger();
        char* location = GetCodeAddress();

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

    _Code = caseLocation;
    GetCodeToken();

    if (!ExecStatementsUntil(MCAblToken::EndCase))
    {
        return;
    }

    GetCodeToken();
    GetCodeToken();
    _Code = GetCodeAddressMarker();
    GetCodeToken();
}

auto MCAblRuntime::ExecForStatement() -> void
{
    GetCodeToken();
    char* loopEndLocation = GetCodeAddressMarker();

    GetCodeToken();
    MCAblSymbol* controlIdPtr = GetCodeSymbol();
    MCAblType* controlTypePtr = ExecVariable(controlIdPtr, MCAblUse::Target);
    const MCAddress controlAddress = _Tos->Address;
    Pop();

    GetCodeToken();
    ExecExpression();
    const int32_t initialValue = controlTypePtr == IntegerTypePtr ? _Tos->Integer : _Tos->Byte;
    Pop();

    const bool countUp = _Token == MCAblToken::To;
    GetCodeToken();
    ExecExpression();
    const int32_t finalValue = controlTypePtr == IntegerTypePtr ? _Tos->Integer : _Tos->Byte;
    Pop();

    char* loopStartLocation = _Code;
    int32_t iterations = 0;
    // Counted in unsigned arithmetic, as in the original, so the step past INT_MAX wraps.
    auto controlValue = static_cast<uint32_t>(initialValue);

    while (countUp ? static_cast<int32_t>(controlValue) <= finalValue
                   : finalValue <= static_cast<int32_t>(controlValue))
    {
        _Code = loopStartLocation;

        if (controlTypePtr == IntegerTypePtr)
        {
            *reinterpret_cast<uint32_t*>(controlAddress) = controlValue;
        }
        else
        {
            *reinterpret_cast<char*>(controlAddress) = static_cast<char>(controlValue);
        }

        GetCodeToken();

        if (!ExecStatementsUntil(MCAblToken::EndFor))
        {
            return;
        }

        CountLoopIteration(iterations);
        controlValue += countUp ? 1u : static_cast<uint32_t>(-1);
    }

    _Code = loopEndLocation;
    GetCodeToken();
}

auto MCAblRuntime::ExecIfStatement() -> void
{
    GetCodeToken();
    char* falseLocation = GetCodeAddressMarker();

    GetCodeToken();
    ExecExpression();
    const int32_t test = _Tos->Integer;
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
                _Code = GetCodeAddressMarker();
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
        _Code = falseLocation;
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
    char* loopStartLocation = _Code;
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

        if (_Tos->Integer == 0)
        {
            _Code = loopStartLocation;
        }

        Pop();
    } while (_Code == loopStartLocation);
}

auto MCAblRuntime::ExecWhileStatement() -> void
{
    GetCodeToken();
    char* loopEndLocation = GetCodeAddressMarker();
    char* loopStartLocation = _Code;
    int32_t iterations = 0;

    for (;;)
    {
        GetCodeToken();
        ExecExpression();
        const bool loopDone = _Tos->Integer == 0;
        Pop();

        if (loopDone)
        {
            _Code = loopEndLocation;
            break;
        }

        GetCodeToken();

        if (!ExecStatementsUntil(MCAblToken::EndWhile))
        {
            return;
        }

        iterations++;
        _Code = loopStartLocation;

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
    MCAblType* returnTypePtr = _Routine->TypePtr;

    if (returnTypePtr)
    {
        MCAblStackItem* framePtr = CurrentRoutineFrame();
        GetCodeToken();
        GetCodeToken();
        MCAblType* expressionTypePtr = ExecExpression();

        if (returnTypePtr == RealTypePtr && expressionTypePtr == IntegerTypePtr)
        {
            framePtr->Real = static_cast<float>(_Tos->Integer);
        }
        else if (returnTypePtr->Form == MCAblTypeForm::Array)
        {
            // Original behaviour: the array is copied over the frame's function value slot (and past it).
            std::memcpy(framePtr, _Tos->Address, static_cast<size_t>(returnTypePtr->Size));
        }
        else if (returnTypePtr == IntegerTypePtr || returnTypePtr->Form == MCAblTypeForm::Enum)
        {
            framePtr->Integer = _Tos->Integer;
        }
        else
        {
            framePtr->Real = _Tos->Real;
        }

        Pop();
        _ReturnValue.Real = framePtr->Real;

        if (_Debugger)
        {
            _Debugger->TraceDataStore(_Routine, _Routine->TypePtr, framePtr, returnTypePtr);
        }
    }

    // The code a return jumps to: the end of a routine.
    static const MCAblToken exitRoutineCode[] = {MCAblToken::EndFunction, MCAblToken::Semicolon, MCAblToken::None};
    GetCodeToken();
    _Code = const_cast<char*>(reinterpret_cast<const char*>(exitRoutineCode));
    _ExitWithReturn = true;
    GetCodeToken();
}

auto MCAblRuntime::ExecOrderReturn(int32_t returnValue) -> void
{
    MCAblStackItem* framePtr = CurrentRoutineFrame();
    framePtr->Integer = returnValue;
    _ReturnValue = MCAblStackItem{};
    _ReturnValue.Integer = returnValue;

    if (_Debugger)
    {
        _Debugger->TraceDataStore(_Routine, _Routine->TypePtr, framePtr, _Routine->TypePtr);
    }

    _ExitWithReturn = true;

    if (returnValue != 1)
    {
        // The code an order's return jumps to: the end of a routine.
        static const MCAblToken exitOrderCode[] = {MCAblToken::EndFunction, MCAblToken::Semicolon, MCAblToken::None};
        _Code = const_cast<char*>(reinterpret_cast<const char*>(exitOrderCode));
        GetCodeToken();
    }
}
