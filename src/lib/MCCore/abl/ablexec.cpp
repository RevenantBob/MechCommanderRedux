#include "stdafx.h"
#include "abl/ablexec.h"
#include "abl/abldbug.h"
#include "abl/ablenv.h"
#include "abl/ablerr.h"
#include "abl/ablrtn.h"
#include "abl/ablxstmt.h"
#include "lib/aerror.h"
#include "lib/heap.h"

int IncludeDebugInfo = 1;
int Crunch = 1;
char* codeBuffer = nullptr;
char* codeBufferPtr = nullptr;
int32_t MaxCodeBufferSize = 0;
char* codeSegmentPtr = nullptr;
char* codeSegmentLimit = nullptr;
char* statementStartPtr = nullptr;
TokenCodeType codeToken{};
StackItemPtr stack = nullptr;
StackItemPtr tos = nullptr;
StackItemPtr stackFrameBasePtr = nullptr;
StackItemPtr StaticDataPtr = nullptr;
StackItem returnValue{};
int32_t execStatementCount = 0;
int32_t execLineNumber = 0;
int ExitFromTacOrder = 0;

namespace
{
    /// <summary>Reports a code buffer overflow (fatal) when fewer than 100 bytes are left.</summary>
    void checkCodeBufferSpace()
    {
        if (codeBufferPtr >= codeBuffer + MaxCodeBufferSize - 100)
        {
            syntaxError(ABL_ERR_SYNTAX_CODE_SEGMENT_OVERFLOW);
        }
    }

    /// <summary>Advances tos to a new item, cleared (stack overflow is a runtime error).</summary>
    StackItemPtr pushItem()
    {
        StackItemPtr item = ++tos;

        if (item >= stack + MAXSIZE_STACK)
        {
            runtimeError(ABL_ERR_RUNTIME_STACK_OVERFLOW);
        }

        // Port fix: the original stored only the value's own bytes; the whole 8-byte slot is cleared first.
        *item = StackItem{};
        return item;
    }
}

auto crunchToken() -> void
{
    if (Crunch)
    {
        checkCodeBufferSpace();
        *codeBufferPtr++ = static_cast<char>(curToken);
    }
}

auto crunchSymTableNodePtr(SymTableNodePtr nodePtr) -> void
{
    if (Crunch)
    {
        checkCodeBufferSpace();
        std::memcpy(codeBufferPtr, &nodePtr, CODE_SYMBOL_PTR_SIZE);
        codeBufferPtr += CODE_SYMBOL_PTR_SIZE;
    }
}

auto crunchStatementMarker() -> void
{
    if (Crunch)
    {
        checkCodeBufferSpace();
        char saveCode = codeBufferPtr[-1];
        codeBufferPtr[-1] = static_cast<char>(TKN_STATEMENT_MARKER);

        if (IncludeDebugInfo)
        {
            *codeBufferPtr = static_cast<char>(FileNumber);
            int32_t line = lineNumber;
            std::memcpy(codeBufferPtr + 1, &line, sizeof(line));
            codeBufferPtr += CODE_STATEMENT_MARKER_SIZE;
        }

        *codeBufferPtr++ = saveCode;
    }
}

auto uncrunchStatementMarker() -> void
{
    // The marker, its debug info and the displaced token.
    if (IncludeDebugInfo)
    {
        codeBufferPtr -= CODE_STATEMENT_MARKER_SIZE + 2;
    }
    else
    {
        codeBufferPtr -= 2;
    }
}

auto crunchAddressMarker(Address address) -> char*
{
    if (!Crunch)
    {
        return nullptr;
    }

    checkCodeBufferSpace();
    char saveCode = codeBufferPtr[-1];
    codeBufferPtr[-1] = static_cast<char>(TKN_ADDRESS_MARKER);
    char* slot = codeBufferPtr;
    // Port: the chain is an offset from codeBuffer (the original stored the 4-byte pointer).
    int32_t chain = address ? static_cast<int32_t>(address - codeBuffer) : CODE_ADDRESS_CHAIN_NULL;
    std::memcpy(slot, &chain, CODE_ADDRESS_SIZE);
    slot[CODE_ADDRESS_SIZE] = saveCode;
    codeBufferPtr += CODE_ADDRESS_SIZE + 1;
    return slot;
}

auto fixupAddressMarker(Address address) -> char*
{
    if (!Crunch)
    {
        return nullptr;
    }

    int32_t chain;
    std::memcpy(&chain, address, CODE_ADDRESS_SIZE);
    char* oldAddress = chain == CODE_ADDRESS_CHAIN_NULL ? nullptr : codeBuffer + chain;
    int32_t offset = static_cast<int32_t>(codeBufferPtr - address);
    std::memcpy(address, &offset, CODE_ADDRESS_SIZE);
    return oldAddress;
}

auto crunchInteger(int32_t value) -> void
{
    if (Crunch)
    {
        checkCodeBufferSpace();
        std::memcpy(codeBufferPtr, &value, CODE_INTEGER_SIZE);
        codeBufferPtr += CODE_INTEGER_SIZE;
    }
}

auto crunchOffset(Address address) -> void
{
    if (Crunch)
    {
        checkCodeBufferSpace();
        int32_t offset = static_cast<int32_t>(address - codeBufferPtr);
        std::memcpy(codeBufferPtr, &offset, CODE_INTEGER_SIZE);
        codeBufferPtr += CODE_INTEGER_SIZE;
    }
}

auto createCodeSegment() -> char*
{
    uint32_t codeSize = static_cast<uint32_t>(codeBufferPtr - codeBuffer);
    // Port fix: one more byte, a TKN_NONE after the code. execStatement's semicolon loop reads the token after a
    // routine's final ";", one byte past its segment (OB-108). The original's heap always had bytes there; an
    // exact-size block can end on a page boundary, and the read faults.
    char* codeSegment = static_cast<char*>(AblCodeHeap->malloc(codeSize + 1));

    if (!codeSegment)
    {
        Fatal(0, " ABL: Unable to AblCodeHeap->malloc code segment ");
    }

    codeSegmentLimit = codeSegment + codeSize;
    std::memcpy(codeSegment, codeBuffer, codeSize);
    codeSegment[codeSize] = TKN_NONE;
    codeSegmentPtr = codeSegmentLimit;
    codeBufferPtr = codeBuffer;
    return codeSegment;
}

auto getCodeSymTableNodePtr() -> SymTableNodePtr
{
    SymTableNodePtr nodePtr;
    std::memcpy(&nodePtr, codeSegmentPtr, CODE_SYMBOL_PTR_SIZE);
    codeSegmentPtr += CODE_SYMBOL_PTR_SIZE;
    return nodePtr;
}

auto getCodeStatementMarker() -> int32_t
{
    int32_t line = -1;

    if (codeToken == TKN_STATEMENT_MARKER && IncludeDebugInfo)
    {
        FileNumber = static_cast<uint8_t>(*codeSegmentPtr);
        std::memcpy(&line, codeSegmentPtr + 1, sizeof(line));
        codeSegmentPtr += CODE_STATEMENT_MARKER_SIZE;
    }

    return line;
}

auto getCodeAddressMarker() -> char*
{
    char* address = nullptr;

    if (codeToken == TKN_ADDRESS_MARKER)
    {
        int32_t offset;
        std::memcpy(&offset, codeSegmentPtr, CODE_ADDRESS_SIZE);
        address = codeSegmentPtr + offset - 1;
        codeSegmentPtr += CODE_ADDRESS_SIZE;
    }

    return address;
}

auto getCodeInteger() -> int32_t
{
    int32_t value;
    std::memcpy(&value, codeSegmentPtr, CODE_INTEGER_SIZE);
    codeSegmentPtr += CODE_INTEGER_SIZE;
    return value;
}

auto getCodeAddress() -> char*
{
    int32_t offset;
    std::memcpy(&offset, codeSegmentPtr, CODE_INTEGER_SIZE);
    char* address = codeSegmentPtr + offset - 1;
    codeSegmentPtr += CODE_INTEGER_SIZE;
    return address;
}

auto pop() -> void
{
    --tos;
}

auto getCodeToken() -> void
{
    codeToken = static_cast<TokenCodeType>(*codeSegmentPtr++);
}

auto pushInteger(int32_t value) -> void
{
    pushItem()->integer = value;
}

auto pushReal(float value) -> void
{
    pushItem()->real = value;
}

auto pushByte(char value) -> void
{
    pushItem()->byte = static_cast<uint8_t>(value);
}

auto pushAddress(Address address) -> void
{
    pushItem()->address = address;
}

auto pushStackFrameHeader(int32_t oldLevel, int32_t newLevel) -> void
{
    StackFrameHeaderPtr headerPtr = reinterpret_cast<StackFrameHeaderPtr>(stackFrameBasePtr);
    // Function value.
    pushInteger(0);
    // Static link: none for a routine of another module; the caller's frame for a routine nested in it; the
    // caller's own static link for a routine at the caller's level.
    StackItemPtr staticLink;

    if (newLevel == -1)
    {
        staticLink = nullptr;
    }
    else if (newLevel == oldLevel + 1)
    {
        staticLink = stackFrameBasePtr;
    }
    else if (newLevel == oldLevel)
    {
        staticLink = reinterpret_cast<StackItemPtr>(headerPtr->staticLink.address);
    }
    else
    {
        runtimeError(ABL_ERR_RUNTIME_NESTED_FUNCTION_CALL);
        return;
    }

    pushAddress(reinterpret_cast<Address>(staticLink));
    // Dynamic link.
    pushAddress(reinterpret_cast<Address>(stackFrameBasePtr));
    // Return address (set by the caller).
    pushAddress(nullptr);
}

auto allocLocal(TypePtr typePtr) -> void
{
    if (typePtr == IntegerTypePtr)
    {
        pushInteger(0);
    }
    else if (typePtr == RealTypePtr)
    {
        pushReal(0.0f);
    }
    else if (typePtr == BooleanTypePtr)
    {
        pushByte(0);
    }
    else if (typePtr == CharTypePtr)
    {
        pushByte(0);
    }
    else
    {
        switch (typePtr->form)
        {
            case FRM_ENUM:
                pushInteger(0);
                break;
            case FRM_ARRAY:
            {
                char* localArray = static_cast<char*>(AblStackHeap->malloc(static_cast<uint32_t>(typePtr->size)));

                if (!localArray)
                {
                    Fatal(0, " ABL: Unable to AblStackHeap->malloc local array ");
                }

                pushAddress(localArray);
                break;
            }

            default:
                break;
        }
    }
}

auto freeLocal(SymTableNodePtr idPtr) -> void
{
    // Only local arrays own memory; a reference parameter's array belongs to the caller.
    if (idPtr->typePtr->form == FRM_ARRAY && idPtr->defn.key != DFN_REFPARAM)
    {
        StackItemPtr dataPtr = stackFrameBasePtr + idPtr->defn.info.data.offset;

        if (idPtr->defn.info.data.varType != VAR_TYPE_NORMAL || !dataPtr)
        {
            runtimeError(ABL_ERR_RUNTIME_STACK_OVERFLOW);
            return;
        }

        AblStackHeap->free(dataPtr->address);
    }
}

auto routineEntry(SymTableNodePtr routineIdPtr) -> void
{
    if (debugger)
    {
        debugger->traceRoutineEntry(routineIdPtr);
    }

    codeSegmentPtr = routineIdPtr->defn.info.routine.codeSegment;
    returnValue = StackItem{};

    // Static and eternal locals live elsewhere.
    for (SymTableNodePtr varIdPtr = routineIdPtr->defn.info.routine.locals; varIdPtr; varIdPtr = varIdPtr->next)
    {
        if (varIdPtr->defn.info.data.varType == VAR_TYPE_NORMAL)
        {
            allocLocal(varIdPtr->typePtr);
        }
    }
}

auto routineExit(SymTableNodePtr routineIdPtr) -> void
{
    if (debugger)
    {
        debugger->traceRoutineExit(routineIdPtr);
    }

    for (SymTableNodePtr idPtr = routineIdPtr->defn.info.routine.params; idPtr; idPtr = idPtr->next)
    {
        freeLocal(idPtr);
    }

    for (SymTableNodePtr idPtr = routineIdPtr->defn.info.routine.locals; idPtr; idPtr = idPtr->next)
    {
        if (idPtr->defn.info.data.varType == VAR_TYPE_NORMAL)
        {
            freeLocal(idPtr);
        }
    }

    StackFrameHeaderPtr headerPtr = reinterpret_cast<StackFrameHeaderPtr>(stackFrameBasePtr);
    codeSegmentPtr = headerPtr->returnAddress.address;

    // A function leaves its value (the frame's first item) on the stack.
    if (routineIdPtr->typePtr)
    {
        tos = stackFrameBasePtr;
    }
    else
    {
        tos = stackFrameBasePtr - 1;
    }

    stackFrameBasePtr = reinterpret_cast<StackItemPtr>(headerPtr->dynamicLink.address);
}

auto execute(SymTableNodePtr routineIdPtr) -> void
{
    SymTableNodePtr thisRoutineIdPtr = CurRoutineIdPtr;
    CurRoutineIdPtr = routineIdPtr;
    routineEntry(routineIdPtr);

    if (CallModuleInit)
    {
        CallModuleInit = 0;
        SymTableNodePtr moduleIdPtr = ModuleRegistry[CurModule->handle].moduleIdPtr;
        SymTableNodePtr initIdPtr =
            searchSymTable(const_cast<char*>("init"), moduleIdPtr->defn.info.routine.localSymTable);

        if (initIdPtr)
        {
            execRoutineCall(initIdPtr);
            // execRoutineCall reads the token after the call; back up to it.
            codeSegmentPtr--;
        }
    }

    getCodeToken();
    execStatement();
    ExitWithReturn = 0;
    ExitFromTacOrder = 0;
    routineExit(routineIdPtr);
    CurRoutineIdPtr = thisRoutineIdPtr;
}

auto executeChild(SymTableNodePtr moduleIdPtr, SymTableNodePtr childRoutineIdPtr, ABLParam* /*paramList*/) -> void
{
    // paramList is unused in MCX.EXE.
    SymTableNodePtr thisRoutineIdPtr = CurRoutineIdPtr;
    CurRoutineIdPtr = moduleIdPtr;
    routineEntry(moduleIdPtr);
    SymTableNodePtr initIdPtr = nullptr;

    if (CallModuleInit)
    {
        CallModuleInit = 0;
        initIdPtr = searchSymTable(const_cast<char*>("init"), moduleIdPtr->defn.info.routine.localSymTable);

        if (initIdPtr)
        {
            execRoutineCall(initIdPtr);
            codeSegmentPtr--;
        }
    }

    // When the child is init itself, it has just run.
    if (initIdPtr != childRoutineIdPtr)
    {
        execRoutineCall(childRoutineIdPtr);
        codeSegmentPtr--;
    }

    ExitWithReturn = 0;
    ExitFromTacOrder = 0;
    routineExit(moduleIdPtr);
    CurRoutineIdPtr = thisRoutineIdPtr;
}
