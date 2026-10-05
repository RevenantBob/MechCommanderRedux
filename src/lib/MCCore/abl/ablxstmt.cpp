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

auto execStatement() -> void
{
    if (codeToken == TKN_STATEMENT_MARKER)
    {
        execLineNumber = getCodeStatementMarker();
        execStatementCount++;
        statementStartPtr = codeSegmentPtr;

        if (debugger)
        {
            debugger->traceStatementExecution();
        }

        getCodeToken();
    }

    int wasInOrdersBlock = InOrdersBlock;

    switch (codeToken)
    {
        case TKN_IDENTIFIER:
        {
            SymTableNodePtr idPtr = getCodeSymTableNodePtr();

            if (idPtr->defn.key == DFN_FUNCTION)
            {
                // A function called as a statement: drop its result.
                if (execRoutineCall(idPtr))
                {
                    pop();
                }
            }
            else
            {
                execAssignmentStatement(idPtr);
            }
            break;
        }

        case TKN_CODE:
        {
            InOrdersBlock = 0;
            getCodeToken();
            TokenCodeType endToken = CurLibrary ? TKN_END_LIBRARY : TKN_END_MODULE;

            while (codeToken != TKN_END_FUNCTION && codeToken != endToken)
            {
                execStatement();
            }

            getCodeToken();
            InOrdersBlock = wasInOrdersBlock;
            break;
        }

        case TKN_SWITCH:
            execSwitchStatement();
            break;
        case TKN_FOR:
            execForStatement();
            break;
        case TKN_IF:
            execIfStatement();
            break;
        case TKN_REPEAT:
            execRepeatStatement();
            break;
        case TKN_WHILE:
            execWhileStatement();
            break;
        case TKN_SEMICOLON:
        case TKN_ELSE:
        case TKN_UNTIL:
            break;
        default:
            runtimeError(ABL_ERR_RUNTIME_UNIMPLEMENTED_FEATURE);
            break;
    }

    while (codeToken == TKN_SEMICOLON)
    {
        getCodeToken();
    }
}

auto execAssignmentStatement(SymTableNodePtr idPtr) -> void
{
    TypePtr targetTypePtr = execVariable(idPtr, USE_TARGET);
    StackItemPtr targetPtr = reinterpret_cast<StackItemPtr>(tos->address);
    pop();
    TypePtr targetBaseTypePtr = baseType(targetTypePtr);

    getCodeToken();
    TypePtr expressionTypePtr = execExpression();

    // The target is a stack slot or an element in array memory: stores go through 4-byte integers and reals (1
    // byte for a char), as in the original.
    if (targetTypePtr == RealTypePtr && baseType(expressionTypePtr) == IntegerTypePtr)
    {
        *reinterpret_cast<float*>(targetPtr) = static_cast<float>(tos->integer);
    }
    else if (targetTypePtr->form == FRM_ARRAY)
    {
        std::memcpy(targetPtr, tos->address, static_cast<size_t>(targetTypePtr->size));
    }
    else if (targetBaseTypePtr == IntegerTypePtr || targetTypePtr->form == FRM_ENUM)
    {
        *reinterpret_cast<int32_t*>(targetPtr) = tos->integer;
    }
    else if (targetBaseTypePtr == CharTypePtr)
    {
        *reinterpret_cast<uint8_t*>(targetPtr) = tos->byte;
    }
    else
    {
        *reinterpret_cast<float*>(targetPtr) = tos->real;
    }

    pop();

    if (debugger)
    {
        debugger->traceDataStore(idPtr, idPtr->typePtr, targetPtr, targetTypePtr);
    }
}

auto execRoutineCall(SymTableNodePtr routineIdPtr) -> TypePtr
{
    if (routineIdPtr->defn.info.routine.key == RTN_DECLARED)
    {
        return execDeclaredRoutineCall(routineIdPtr);
    }

    return execStandardRoutineCall(routineIdPtr);
}

auto execDeclaredRoutineCall(SymTableNodePtr routineIdPtr) -> TypePtr
{
    int32_t oldLevel = level;
    int32_t newLevel = routineIdPtr->level + 1;
    CallStackLevel++;

    // A function of another module (a library's) has no static link and runs in that module.
    StackItemPtr newStackFrameBasePtr = tos + 1;
    bool isLibraryCall = routineIdPtr->library && routineIdPtr->library != CurRoutineIdPtr->library;

    if (isLibraryCall)
    {
        pushStackFrameHeader(-1, -1);
    }
    else
    {
        pushStackFrameHeader(level, newLevel);
    }

    getCodeToken();

    if (codeToken == TKN_LPAREN)
    {
        execActualParams(routineIdPtr);
        getCodeToken();
    }

    stackFrameBasePtr = newStackFrameBasePtr;
    level = newLevel;
    reinterpret_cast<StackFrameHeaderPtr>(newStackFrameBasePtr)->returnAddress.address = codeSegmentPtr - 1;

    ABLModule* callerModule = nullptr;

    if (isLibraryCall)
    {
        ABLModule* library = routineIdPtr->library;
        callerModule = CurModule;
        CurModuleHandle = library->handle;
        CurModule = library;

        if (debugger)
        {
            debugger->setModule(library);
        }

        StaticDataPtr = CurModule->staticData;
        int32_t wasInitCalled = CurModule->initCalled;
        CurModule->initCalled = 1;
        CallModuleInit = wasInitCalled == 0;
    }

    if (!ProfileLog)
    {
        execute(routineIdPtr);
    }
    else
    {
        uint32_t startTime = MCPort::Milliseconds();
        execute(routineIdPtr);
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
            std::snprintf(routineEntry, sizeof(routineEntry), "%s (%d)\n", routineIdPtr->name, runTime);
            std::strcat(profileEntry, routineEntry);
            ABL_AddToProfileLog(profileEntry);
        }
    }

    if (isLibraryCall)
    {
        CurModuleHandle = callerModule->handle;
        CurModule = callerModule;

        if (debugger)
        {
            debugger->setModule(callerModule);
        }

        StaticDataPtr = CurModule->staticData;
    }

    level = oldLevel;
    getCodeToken();
    CallStackLevel--;
    return routineIdPtr->typePtr;
}

auto setOpenArray(TypePtr arrayTypePtr, int32_t size) -> void
{
    // Faithful: the element count is the new size over the OLD total size, not over the element size.
    int32_t oldSize = arrayTypePtr->size;
    arrayTypePtr->size = size;
    TypePtr lastDimensionTypePtr = arrayTypePtr;

    while (lastDimensionTypePtr->info.array.elementTypePtr->form == FRM_ARRAY)
    {
        lastDimensionTypePtr = lastDimensionTypePtr->info.array.elementTypePtr;
    }

    lastDimensionTypePtr->info.array.elementCount = size / oldSize;
}

auto execActualParams(SymTableNodePtr routineIdPtr) -> void
{
    for (SymTableNodePtr formalIdPtr = routineIdPtr->defn.info.routine.params; formalIdPtr;
         formalIdPtr = formalIdPtr->next)
    {
        TypePtr formalTypePtr = formalIdPtr->typePtr;
        getCodeToken();

        if (formalIdPtr->defn.key == DFN_VALPARAM)
        {
            TypePtr actualTypePtr = execExpression();

            if (formalTypePtr == RealTypePtr && baseType(actualTypePtr) == IntegerTypePtr)
            {
                tos->real = static_cast<float>(tos->integer);
            }

            // An array passed by value gets its own copy.
            if (formalTypePtr->form == FRM_ARRAY)
            {
                int32_t size = formalTypePtr->size;
                Address source = tos->address;
                Address copy = static_cast<Address>(AblMemory.Allocate(static_cast<size_t>(size)));

                if (!copy)
                {
                    char err[256];
                    std::snprintf(err, sizeof(err),
                                  " ABL: Unable to AblStackHeap->malloc actual array param in module %s)",
                                  CurModule->name);
                    Fatal(0, err);
                }

                std::memcpy(copy, source, static_cast<size_t>(size));
                tos->address = copy;
            }
        }
        else
        {
            SymTableNodePtr actualIdPtr = getCodeSymTableNodePtr();
            execVariable(actualIdPtr, USE_REFPARAM);
        }
    }
}

auto execSwitchStatement() -> void
{
    getCodeToken();
    char* branchTableLocation = getCodeAddressMarker();

    getCodeToken();
    TypePtr switchExpressionTypePtr = execExpression();
    int32_t switchExpressionValue;

    if (switchExpressionTypePtr == IntegerTypePtr || switchExpressionTypePtr->form == FRM_ENUM)
    {
        switchExpressionValue = tos->integer;
    }
    else
    {
        switchExpressionValue = tos->byte;
    }

    pop();

    // The branch table: a count, then (label value, case location) pairs.
    codeSegmentPtr = branchTableLocation;
    getCodeToken();
    int32_t caseLabelCount = getCodeInteger();
    int32_t remaining;
    char* caseLocation = nullptr;

    for (;;)
    {
        remaining = caseLabelCount - 1;

        if (caseLabelCount == 0)
        {
            break;
        }

        int32_t labelValue = getCodeInteger();
        caseLocation = getCodeAddress();
        caseLabelCount = remaining;

        if (labelValue == switchExpressionValue)
        {
            break;
        }
    }

    // No matching case: skip past the table.
    if (remaining < 0)
    {
        getCodeToken();
        getCodeToken();
        return;
    }

    codeSegmentPtr = caseLocation;
    getCodeToken();

    while (codeToken != TKN_END_CASE)
    {
        execStatement();

        if (ExitWithReturn)
        {
            return;
        }
    }

    getCodeToken();
    getCodeToken();
    codeSegmentPtr = getCodeAddressMarker();
    getCodeToken();
}

auto execForStatement() -> void
{
    getCodeToken();
    char* loopEndLocation = getCodeAddressMarker();

    getCodeToken();
    SymTableNodePtr controlIdPtr = getCodeSymTableNodePtr();
    TypePtr controlTypePtr = execVariable(controlIdPtr, USE_TARGET);
    Address controlAddress = tos->address;
    pop();

    getCodeToken();
    execExpression();
    uint32_t initialValue = controlTypePtr == IntegerTypePtr ? static_cast<uint32_t>(tos->integer) : tos->byte;
    pop();

    bool countUp = codeToken == TKN_TO;
    getCodeToken();
    execExpression();
    uint32_t finalValue = controlTypePtr == IntegerTypePtr ? static_cast<uint32_t>(tos->integer) : tos->byte;
    pop();

    char* loopStartLocation = codeSegmentPtr;
    int32_t iterations = 0;
    uint32_t controlValue = initialValue;

    while (countUp ? static_cast<int32_t>(controlValue) <= static_cast<int32_t>(finalValue)
                   : static_cast<int32_t>(finalValue) <= static_cast<int32_t>(controlValue))
    {
        codeSegmentPtr = loopStartLocation;

        if (controlTypePtr == IntegerTypePtr)
        {
            *reinterpret_cast<uint32_t*>(controlAddress) = controlValue;
        }
        else
        {
            *reinterpret_cast<char*>(controlAddress) = static_cast<char>(controlValue);
        }

        getCodeToken();

        while (codeToken != TKN_END_FOR)
        {
            execStatement();

            if (ExitWithReturn)
            {
                return;
            }
        }

        iterations++;

        if (iterations == MaxLoopIterations)
        {
            runtimeError(ABL_ERR_RUNTIME_INFINITE_LOOP);
        }

        controlValue += countUp ? 1 : static_cast<uint32_t>(-1);
    }

    codeSegmentPtr = loopEndLocation;
    getCodeToken();
}

auto execIfStatement() -> void
{
    getCodeToken();
    char* falseLocation = getCodeAddressMarker();

    getCodeToken();
    execExpression();
    int32_t test = tos->integer;
    pop();

    if (test == 1)
    {
        // The THEN part, up to END_IF or ELSE (then jump past the ELSE part).
        getCodeToken();

        while (codeToken != TKN_END_IF)
        {
            if (codeToken == TKN_ELSE)
            {
                getCodeToken();
                codeSegmentPtr = getCodeAddressMarker();
                getCodeToken();
                break;
            }

            execStatement();

            if (ExitWithReturn)
            {
                return;
            }
        }
    }
    else
    {
        codeSegmentPtr = falseLocation;
        getCodeToken();

        if (codeToken == TKN_ELSE)
        {
            getCodeToken();
            getCodeAddressMarker();
            getCodeToken();

            while (codeToken != TKN_END_IF)
            {
                execStatement();

                if (ExitWithReturn)
                {
                    return;
                }
            }
        }
    }

    getCodeToken();
}

auto execRepeatStatement() -> void
{
    char* loopStartLocation = codeSegmentPtr;
    int32_t iterations = 0;

    do
    {
        getCodeToken();

        while (codeToken != TKN_UNTIL)
        {
            execStatement();

            if (ExitWithReturn)
            {
                return;
            }
        }

        iterations++;

        if (iterations == MaxLoopIterations)
        {
            runtimeError(ABL_ERR_RUNTIME_INFINITE_LOOP);
        }

        getCodeToken();
        execExpression();

        if (tos->integer == 0)
        {
            codeSegmentPtr = loopStartLocation;
        }

        pop();
    } while (codeSegmentPtr == loopStartLocation);
}

auto execWhileStatement() -> void
{
    getCodeToken();
    char* loopEndLocation = getCodeAddressMarker();
    char* loopStartLocation = codeSegmentPtr;
    int32_t iterations = 0;

    for (;;)
    {
        getCodeToken();
        execExpression();
        bool loopDone = tos->integer == 0;

        if (loopDone)
        {
            codeSegmentPtr = loopEndLocation;
        }

        pop();

        if (loopDone)
        {
            break;
        }

        getCodeToken();

        while (codeToken != TKN_END_WHILE)
        {
            execStatement();

            if (ExitWithReturn)
            {
                return;
            }
        }

        iterations++;
        codeSegmentPtr = loopStartLocation;

        if (iterations == MaxLoopIterations)
        {
            runtimeError(ABL_ERR_RUNTIME_INFINITE_LOOP);
        }
    }

    getCodeToken();
}
