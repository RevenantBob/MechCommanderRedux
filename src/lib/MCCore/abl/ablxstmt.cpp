#include "stdafx.h"
#include "abl/ablxstmt.h"
#include "abl/abldbug.h"
#include "abl/ablenv.h"
#include "abl/ablerr.h"
#include "abl/ablexec.h"
#include "abl/ablrtn.h"
#include "abl/ablxexpr.h"
#include "abl/ablxstd.h"
#include "lib/aerror.h"

int32_t MaxLoopIterations = 100001;
int32_t ProfileLogFunctionTimeLimit = 5;
int ExitWithReturn = 0;

auto ExecStatement() -> void
{
    if (CodeToken == TKN_STATEMENT_MARKER)
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
        case TKN_IDENTIFIER:
        {
            MCSymTableNodePtr idPtr = GetCodeSymTableNodePtr();

            if (idPtr->Defn.Key == DFN_FUNCTION)
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

        case TKN_CODE:
        {
            InOrdersBlock = 0;
            GetCodeToken();
            MCTokenCodeType endToken = CurLibrary ? TKN_END_LIBRARY : TKN_END_MODULE;

            while (CodeToken != TKN_END_FUNCTION && CodeToken != endToken)
            {
                ExecStatement();
            }

            GetCodeToken();
            InOrdersBlock = wasInOrdersBlock;
            break;
        }

        case TKN_SWITCH:
            ExecSwitchStatement();
            break;
        case TKN_FOR:
            ExecForStatement();
            break;
        case TKN_IF:
            ExecIfStatement();
            break;
        case TKN_REPEAT:
            ExecRepeatStatement();
            break;
        case TKN_WHILE:
            ExecWhileStatement();
            break;
        case TKN_SEMICOLON:
        case TKN_ELSE:
        case TKN_UNTIL:
            break;
        default:
            RuntimeError(ABL_ERR_RUNTIME_UNIMPLEMENTED_FEATURE);
            break;
    }

    while (CodeToken == TKN_SEMICOLON)
    {
        GetCodeToken();
    }
}

auto ExecAssignmentStatement(MCSymTableNodePtr idPtr) -> void
{
    MCTypePtr targetTypePtr = ExecVariable(idPtr, USE_TARGET);
    MCStackItemPtr targetPtr = reinterpret_cast<MCStackItemPtr>(Tos->Address);
    Pop();
    MCTypePtr targetBaseTypePtr = BaseType(targetTypePtr);

    GetCodeToken();
    MCTypePtr expressionTypePtr = ExecExpression();

    // The target is a stack slot or an element in array memory: stores go through 4-byte integers and reals (1
    // byte for a char), as in the original.
    if (targetTypePtr == RealTypePtr && BaseType(expressionTypePtr) == IntegerTypePtr)
    {
        *reinterpret_cast<float*>(targetPtr) = static_cast<float>(Tos->Integer);
    }
    else if (targetTypePtr->Form == FRM_ARRAY)
    {
        std::memcpy(targetPtr, Tos->Address, static_cast<size_t>(targetTypePtr->Size));
    }
    else if (targetBaseTypePtr == IntegerTypePtr || targetTypePtr->Form == FRM_ENUM)
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

auto ExecRoutineCall(MCSymTableNodePtr routineIdPtr) -> MCTypePtr
{
    if (routineIdPtr->Defn.Info.Routine.Key == RTN_DECLARED)
    {
        return ExecDeclaredRoutineCall(routineIdPtr);
    }

    return ExecStandardRoutineCall(routineIdPtr);
}

auto ExecDeclaredRoutineCall(MCSymTableNodePtr routineIdPtr) -> MCTypePtr
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

    if (CodeToken == TKN_LPAREN)
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
            std::snprintf(routineEntry, sizeof(routineEntry), "%s (%d)\n", routineIdPtr->Name, runTime);
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

auto SetOpenArray(MCTypePtr arrayTypePtr, int32_t size) -> void
{
    // Faithful: the element count is the new size over the OLD total size, not over the element size.
    int32_t oldSize = arrayTypePtr->Size;
    arrayTypePtr->Size = size;
    MCTypePtr lastDimensionTypePtr = arrayTypePtr;

    while (lastDimensionTypePtr->Info.Array.ElementTypePtr->Form == FRM_ARRAY)
    {
        lastDimensionTypePtr = lastDimensionTypePtr->Info.Array.ElementTypePtr;
    }

    lastDimensionTypePtr->Info.Array.ElementCount = size / oldSize;
}

auto ExecActualParams(MCSymTableNodePtr routineIdPtr) -> void
{
    for (MCSymTableNodePtr formalIdPtr = routineIdPtr->Defn.Info.Routine.Params; formalIdPtr;
         formalIdPtr = formalIdPtr->Next)
    {
        MCTypePtr formalTypePtr = formalIdPtr->TypePtr;
        GetCodeToken();

        if (formalIdPtr->Defn.Key == DFN_VALPARAM)
        {
            MCTypePtr actualTypePtr = ExecExpression();

            if (formalTypePtr == RealTypePtr && BaseType(actualTypePtr) == IntegerTypePtr)
            {
                Tos->Real = static_cast<float>(Tos->Integer);
            }

            // An array passed by value gets its own copy.
            if (formalTypePtr->Form == FRM_ARRAY)
            {
                int32_t size = formalTypePtr->Size;
                MCAddress source = Tos->Address;
                MCAddress copy = static_cast<MCAddress>(AblMemory.Allocate(static_cast<size_t>(size)));

                if (!copy)
                {
                    char err[256];
                    std::snprintf(err, sizeof(err),
                                  " ABL: Unable to AblStackHeap->malloc actual array param in module %s)",
                                  CurModule->Name);
                    Fatal(0, err);
                }

                std::memcpy(copy, source, static_cast<size_t>(size));
                Tos->Address = copy;
            }
        }
        else
        {
            MCSymTableNodePtr actualIdPtr = GetCodeSymTableNodePtr();
            ExecVariable(actualIdPtr, USE_REFPARAM);
        }
    }
}

auto ExecSwitchStatement() -> void
{
    GetCodeToken();
    char* branchTableLocation = GetCodeAddressMarker();

    GetCodeToken();
    MCTypePtr switchExpressionTypePtr = ExecExpression();
    int32_t switchExpressionValue;

    if (switchExpressionTypePtr == IntegerTypePtr || switchExpressionTypePtr->Form == FRM_ENUM)
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

    while (CodeToken != TKN_END_CASE)
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
    MCSymTableNodePtr controlIdPtr = GetCodeSymTableNodePtr();
    MCTypePtr controlTypePtr = ExecVariable(controlIdPtr, USE_TARGET);
    MCAddress controlAddress = Tos->Address;
    Pop();

    GetCodeToken();
    ExecExpression();
    uint32_t initialValue = controlTypePtr == IntegerTypePtr ? static_cast<uint32_t>(Tos->Integer) : Tos->Byte;
    Pop();

    bool countUp = CodeToken == TKN_TO;
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

        while (CodeToken != TKN_END_FOR)
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
            RuntimeError(ABL_ERR_RUNTIME_INFINITE_LOOP);
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

        while (CodeToken != TKN_END_IF)
        {
            if (CodeToken == TKN_ELSE)
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

        if (CodeToken == TKN_ELSE)
        {
            GetCodeToken();
            GetCodeAddressMarker();
            GetCodeToken();

            while (CodeToken != TKN_END_IF)
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

        while (CodeToken != TKN_UNTIL)
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
            RuntimeError(ABL_ERR_RUNTIME_INFINITE_LOOP);
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

        while (CodeToken != TKN_END_WHILE)
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
            RuntimeError(ABL_ERR_RUNTIME_INFINITE_LOOP);
        }
    }

    GetCodeToken();
}
