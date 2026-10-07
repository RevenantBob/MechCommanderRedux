#include "stdafx.h"
#include "abl/ablxstmt.h"
#include "abl/abldbug.h"
#include "abl/ablenv.h"
#include "abl/MCAblErrors.h"
#include "abl/ablexec.h"
#include "abl/ablrtn.h"
#include "abl/ablxexpr.h"
#include "abl/ablxstd.h"
#include "lib/MCFatal.h"

int32_t MaxLoopIterations = 100001;
int32_t ProfileLogFunctionTimeLimit = 5;
int ExitWithReturn = 0;

auto ExecStatement() -> void
{
    if (CodeToken == MCAblToken::StatementMarker)
    {
        ExecLineNumber = GetCodeStatementMarker();
        ExecStatementCount++;
        StatementStartPtr = CodeSegmentPtr;

        if (Debugger)
        {
            Debugger->TraceStatementExecution();
        }

        GetCodeToken();
    }

    int wasInOrdersBlock = InOrdersBlock;

    switch (CodeToken)
    {
        case MCAblToken::Identifier:
        {
            MCAblSymbol* idPtr = GetCodeSymTableNodePtr();

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
            InOrdersBlock = 0;
            GetCodeToken();

            // Original behaviour: the end token was chosen by curLibrary, which is only set while a library compiles,
            // so a block always ends at endfunction or endmodule.
            while (CodeToken != MCAblToken::EndFunction && CodeToken != MCAblToken::EndModule)
            {
                ExecStatement();
            }

            GetCodeToken();
            InOrdersBlock = wasInOrdersBlock;
            break;
        }

        case MCAblToken::Switch:
            ExecSwitchStatement();
            break;
        case MCAblToken::For:
            ExecForStatement();
            break;
        case MCAblToken::If:
            ExecIfStatement();
            break;
        case MCAblToken::Repeat:
            ExecRepeatStatement();
            break;
        case MCAblToken::While:
            ExecWhileStatement();
            break;
        case MCAblToken::Semicolon:
        case MCAblToken::Else:
        case MCAblToken::Until:
            break;
        default:
            RuntimeError(MCAblRuntimeError::UnimplementedFeature);
            break;
    }

    while (CodeToken == MCAblToken::Semicolon)
    {
        GetCodeToken();
    }
}

auto ExecAssignmentStatement(MCAblSymbol* idPtr) -> void
{
    MCAblType* targetTypePtr = ExecVariable(idPtr, MCAblUse::Target);
    MCStackItemPtr targetPtr = reinterpret_cast<MCStackItemPtr>(Tos->Address);
    Pop();
    MCAblType* targetBaseTypePtr = targetTypePtr;

    GetCodeToken();
    MCAblType* expressionTypePtr = ExecExpression();

    // The target is a stack slot or an element in array memory: stores go through 4-byte integers and reals (1
    // byte for a char), as in the original.
    if (targetTypePtr == RealTypePtr && expressionTypePtr == IntegerTypePtr)
    {
        *reinterpret_cast<float*>(targetPtr) = static_cast<float>(Tos->Integer);
    }
    else if (targetTypePtr->Form == MCAblTypeForm::Array)
    {
        std::memcpy(targetPtr, Tos->Address, static_cast<size_t>(targetTypePtr->Size));
    }
    else if (targetBaseTypePtr == IntegerTypePtr || targetTypePtr->Form == MCAblTypeForm::Enum)
    {
        *reinterpret_cast<int32_t*>(targetPtr) = Tos->Integer;
    }
    else if (targetBaseTypePtr == CharTypePtr)
    {
        *reinterpret_cast<uint8_t*>(targetPtr) = Tos->Byte;
    }
    else
    {
        *reinterpret_cast<float*>(targetPtr) = Tos->Real;
    }

    Pop();

    if (Debugger)
    {
        Debugger->TraceDataStore(idPtr, idPtr->TypePtr, targetPtr, targetTypePtr);
    }
}

auto ExecRoutineCall(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    if (routineIdPtr->Defn.Info.Routine.Key == MCAblRoutineKey::Declared)
    {
        return ExecDeclaredRoutineCall(routineIdPtr);
    }

    return ExecStandardRoutineCall(routineIdPtr);
}

auto ExecDeclaredRoutineCall(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    int32_t oldLevel = Level;
    int32_t newLevel = routineIdPtr->Level + 1;
    CallStackLevel++;

    // A function of another module (a library's) has no static link and runs in that module.
    MCStackItemPtr newStackFrameBasePtr = Tos + 1;
    bool isLibraryCall = routineIdPtr->Library && routineIdPtr->Library != CurRoutineIdPtr->Library;

    if (isLibraryCall)
    {
        PushStackFrameHeader(-1, -1);
    }
    else
    {
        PushStackFrameHeader(Level, newLevel);
    }

    GetCodeToken();

    if (CodeToken == MCAblToken::LParen)
    {
        ExecActualParams(routineIdPtr);
        GetCodeToken();
    }

    StackFrameBasePtr = newStackFrameBasePtr;
    Level = newLevel;
    reinterpret_cast<MCStackFrameHeaderPtr>(newStackFrameBasePtr)->ReturnAddress.Address = CodeSegmentPtr - 1;

    MCAblModule* callerModule = nullptr;

    if (isLibraryCall)
    {
        MCAblModule* library = routineIdPtr->Library;
        callerModule = CurModule;
        CurModuleHandle = library->Handle;
        CurModule = library;

        if (Debugger)
        {
            Debugger->SetModule(library);
        }

        StaticDataPtr = CurModule->StaticData;
        int32_t wasInitCalled = CurModule->InitCalled;
        CurModule->InitCalled = 1;
        CallModuleInit = wasInitCalled == 0;
    }

    if (!ProfileLog)
    {
        Execute(routineIdPtr);
    }
    else
    {
        uint32_t startTime = MCPort::Milliseconds();
        Execute(routineIdPtr);
        int32_t runTime = static_cast<int32_t>(MCPort::Milliseconds() - startTime);

        if (runTime > ProfileLogFunctionTimeLimit)
        {
            char profileEntry[512];
            std::snprintf(profileEntry, sizeof(profileEntry), "[%08d] ", NumExecutions);

            for (int32_t indent = CallStackLevel; indent > 0; indent--)
            {
                std::strcat(profileEntry, " ");
            }

            char routineEntry[512];
            std::snprintf(routineEntry, sizeof(routineEntry), "%s (%d)\n", routineIdPtr->Name.c_str(), runTime);
            std::strcat(profileEntry, routineEntry);
            AblAddToProfileLog(profileEntry);
        }
    }

    if (isLibraryCall)
    {
        CurModuleHandle = callerModule->Handle;
        CurModule = callerModule;

        if (Debugger)
        {
            Debugger->SetModule(callerModule);
        }

        StaticDataPtr = CurModule->StaticData;
    }

    Level = oldLevel;
    GetCodeToken();
    CallStackLevel--;
    return routineIdPtr->TypePtr;
}

auto SetOpenArray(MCAblType* arrayTypePtr, int32_t size) -> void
{
    // Faithful: the element count is the new size over the OLD total size, not over the element size.
    int32_t oldSize = arrayTypePtr->Size;
    arrayTypePtr->Size = size;
    MCAblType* lastDimensionTypePtr = arrayTypePtr;

    while (lastDimensionTypePtr->Array.ElementTypePtr->Form == MCAblTypeForm::Array)
    {
        lastDimensionTypePtr = lastDimensionTypePtr->Array.ElementTypePtr;
    }

    lastDimensionTypePtr->Array.ElementCount = size / oldSize;
}

auto ExecActualParams(MCAblSymbol* routineIdPtr) -> void
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
                Tos->Real = static_cast<float>(Tos->Integer);
            }

            // An array passed by value gets its own copy.
            if (formalTypePtr->Form == MCAblTypeForm::Array)
            {
                int32_t size = formalTypePtr->Size;
                MCAddress source = Tos->Address;
                MCAddress copy = static_cast<MCAddress>(AblMemory.Allocate(static_cast<size_t>(size)));

                if (!copy)
                {
                    char err[256];
                    std::snprintf(err, sizeof(err),
                                  " ABL: Unable to AblStackHeap->malloc actual array param in module %s)",
                                  CurModule->Name.c_str());
                    Fatal(0, err);
                }

                std::memcpy(copy, source, static_cast<size_t>(size));
                Tos->Address = copy;
            }
        }
        else
        {
            MCAblSymbol* actualIdPtr = GetCodeSymTableNodePtr();
            ExecVariable(actualIdPtr, MCAblUse::RefParam);
        }
    }
}

auto ExecSwitchStatement() -> void
{
    GetCodeToken();
    char* branchTableLocation = GetCodeAddressMarker();

    GetCodeToken();
    MCAblType* switchExpressionTypePtr = ExecExpression();
    int32_t switchExpressionValue;

    if (switchExpressionTypePtr == IntegerTypePtr || switchExpressionTypePtr->Form == MCAblTypeForm::Enum)
    {
        switchExpressionValue = Tos->Integer;
    }
    else
    {
        switchExpressionValue = Tos->Byte;
    }

    Pop();

    // The branch table: a count, then (label value, case location) pairs.
    CodeSegmentPtr = branchTableLocation;
    GetCodeToken();
    int32_t caseLabelCount = GetCodeInteger();
    int32_t remaining;
    char* caseLocation = nullptr;

    for (;;)
    {
        remaining = caseLabelCount - 1;

        if (caseLabelCount == 0)
        {
            break;
        }

        int32_t labelValue = GetCodeInteger();
        caseLocation = GetCodeAddress();
        caseLabelCount = remaining;

        if (labelValue == switchExpressionValue)
        {
            break;
        }
    }

    // No matching case: skip past the table.
    if (remaining < 0)
    {
        GetCodeToken();
        GetCodeToken();
        return;
    }

    CodeSegmentPtr = caseLocation;
    GetCodeToken();

    while (CodeToken != MCAblToken::EndCase)
    {
        ExecStatement();

        if (ExitWithReturn)
        {
            return;
        }
    }

    GetCodeToken();
    GetCodeToken();
    CodeSegmentPtr = GetCodeAddressMarker();
    GetCodeToken();
}

auto ExecForStatement() -> void
{
    GetCodeToken();
    char* loopEndLocation = GetCodeAddressMarker();

    GetCodeToken();
    MCAblSymbol* controlIdPtr = GetCodeSymTableNodePtr();
    MCAblType* controlTypePtr = ExecVariable(controlIdPtr, MCAblUse::Target);
    MCAddress controlAddress = Tos->Address;
    Pop();

    GetCodeToken();
    ExecExpression();
    uint32_t initialValue = controlTypePtr == IntegerTypePtr ? static_cast<uint32_t>(Tos->Integer) : Tos->Byte;
    Pop();

    bool countUp = CodeToken == MCAblToken::To;
    GetCodeToken();
    ExecExpression();
    uint32_t finalValue = controlTypePtr == IntegerTypePtr ? static_cast<uint32_t>(Tos->Integer) : Tos->Byte;
    Pop();

    char* loopStartLocation = CodeSegmentPtr;
    int32_t iterations = 0;
    uint32_t controlValue = initialValue;

    while (countUp ? static_cast<int32_t>(controlValue) <= static_cast<int32_t>(finalValue)
                   : static_cast<int32_t>(finalValue) <= static_cast<int32_t>(controlValue))
    {
        CodeSegmentPtr = loopStartLocation;

        if (controlTypePtr == IntegerTypePtr)
        {
            *reinterpret_cast<uint32_t*>(controlAddress) = controlValue;
        }
        else
        {
            *reinterpret_cast<char*>(controlAddress) = static_cast<char>(controlValue);
        }

        GetCodeToken();

        while (CodeToken != MCAblToken::EndFor)
        {
            ExecStatement();

            if (ExitWithReturn)
            {
                return;
            }
        }

        iterations++;

        if (iterations == MaxLoopIterations)
        {
            RuntimeError(MCAblRuntimeError::InfiniteLoop);
        }

        controlValue += countUp ? 1 : static_cast<uint32_t>(-1);
    }

    CodeSegmentPtr = loopEndLocation;
    GetCodeToken();
}

auto ExecIfStatement() -> void
{
    GetCodeToken();
    char* falseLocation = GetCodeAddressMarker();

    GetCodeToken();
    ExecExpression();
    int32_t test = Tos->Integer;
    Pop();

    if (test == 1)
    {
        // The THEN part, up to END_IF or ELSE (then jump past the ELSE part).
        GetCodeToken();

        while (CodeToken != MCAblToken::EndIf)
        {
            if (CodeToken == MCAblToken::Else)
            {
                GetCodeToken();
                CodeSegmentPtr = GetCodeAddressMarker();
                GetCodeToken();
                break;
            }

            ExecStatement();

            if (ExitWithReturn)
            {
                return;
            }
        }
    }
    else
    {
        CodeSegmentPtr = falseLocation;
        GetCodeToken();

        if (CodeToken == MCAblToken::Else)
        {
            GetCodeToken();
            GetCodeAddressMarker();
            GetCodeToken();

            while (CodeToken != MCAblToken::EndIf)
            {
                ExecStatement();

                if (ExitWithReturn)
                {
                    return;
                }
            }
        }
    }

    GetCodeToken();
}

auto ExecRepeatStatement() -> void
{
    char* loopStartLocation = CodeSegmentPtr;
    int32_t iterations = 0;

    do
    {
        GetCodeToken();

        while (CodeToken != MCAblToken::Until)
        {
            ExecStatement();

            if (ExitWithReturn)
            {
                return;
            }
        }

        iterations++;

        if (iterations == MaxLoopIterations)
        {
            RuntimeError(MCAblRuntimeError::InfiniteLoop);
        }

        GetCodeToken();
        ExecExpression();

        if (Tos->Integer == 0)
        {
            CodeSegmentPtr = loopStartLocation;
        }

        Pop();
    } while (CodeSegmentPtr == loopStartLocation);
}

auto ExecWhileStatement() -> void
{
    GetCodeToken();
    char* loopEndLocation = GetCodeAddressMarker();
    char* loopStartLocation = CodeSegmentPtr;
    int32_t iterations = 0;

    for (;;)
    {
        GetCodeToken();
        ExecExpression();
        bool loopDone = Tos->Integer == 0;

        if (loopDone)
        {
            CodeSegmentPtr = loopEndLocation;
        }

        Pop();

        if (loopDone)
        {
            break;
        }

        GetCodeToken();

        while (CodeToken != MCAblToken::EndWhile)
        {
            ExecStatement();

            if (ExitWithReturn)
            {
                return;
            }
        }

        iterations++;
        CodeSegmentPtr = loopStartLocation;

        if (iterations == MaxLoopIterations)
        {
            RuntimeError(MCAblRuntimeError::InfiniteLoop);
        }
    }

    GetCodeToken();
}
