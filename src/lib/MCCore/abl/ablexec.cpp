#include "stdafx.h"
#include "abl/ablexec.h"
#include "abl/abldbug.h"
#include "abl/ablenv.h"
#include "abl/ablerr.h"
#include "abl/ablrtn.h"
#include "abl/ablxstmt.h"
#include "lib/aerror.h"

int IncludeDebugInfo = 1;
int Crunch = 1;
char* CodeBuffer = nullptr;
char* CodeBufferPtr = nullptr;
int32_t MaxCodeBufferSize = 0;
char* CodeSegmentPtr = nullptr;
char* CodeSegmentLimit = nullptr;
char* StatementStartPtr = nullptr;
MCTokenCodeType CodeToken{};
MCStackItemPtr Stack = nullptr;
MCStackItemPtr Tos = nullptr;
MCStackItemPtr StackFrameBasePtr = nullptr;
MCStackItemPtr StaticDataPtr = nullptr;
MCStackItem ReturnValue{};
int32_t ExecStatementCount = 0;
int32_t ExecLineNumber = 0;
int ExitFromTacOrder = 0;

namespace
{
    /// <summary>Reports a code buffer overflow (fatal) when fewer than 100 bytes are left.</summary>
    void CheckCodeBufferSpace()
    {
        if (CodeBufferPtr >= CodeBuffer + MaxCodeBufferSize - 100)
        {
            SyntaxError(ABL_ERR_SYNTAX_CODE_SEGMENT_OVERFLOW);
        }
    }

    /// <summary>Advances tos to a new item, cleared (stack overflow is a runtime error).</summary>
    MCStackItemPtr PushItem()
    {
        MCStackItemPtr item = ++Tos;

        if (item >= Stack + MAXSIZE_STACK)
        {
            RuntimeError(ABL_ERR_RUNTIME_STACK_OVERFLOW);
        }

        // Port fix: the original stored only the value's own bytes; the whole 8-byte slot is cleared first.
        *item = MCStackItem{};
        return item;
    }
}

auto CrunchToken() -> void
{
    if (Crunch)
    {
        CheckCodeBufferSpace();
        *CodeBufferPtr++ = static_cast<char>(CurToken);
    }
}

auto CrunchSymTableNodePtr(MCSymTableNodePtr nodePtr) -> void
{
    if (Crunch)
    {
        CheckCodeBufferSpace();
        std::memcpy(CodeBufferPtr, &nodePtr, CODE_SYMBOL_PTR_SIZE);
        CodeBufferPtr += CODE_SYMBOL_PTR_SIZE;
    }
}

auto CrunchStatementMarker() -> void
{
    if (Crunch)
    {
        CheckCodeBufferSpace();
        char saveCode = CodeBufferPtr[-1];
        CodeBufferPtr[-1] = static_cast<char>(TKN_STATEMENT_MARKER);

        if (IncludeDebugInfo)
        {
            *CodeBufferPtr = static_cast<char>(FileNumber);
            int32_t line = LineNumber;
            std::memcpy(CodeBufferPtr + 1, &line, sizeof(line));
            CodeBufferPtr += CODE_STATEMENT_MARKER_SIZE;
        }

        *CodeBufferPtr++ = saveCode;
    }
}

auto UncrunchStatementMarker() -> void
{
    // The marker, its debug info and the displaced token.
    if (IncludeDebugInfo)
    {
        CodeBufferPtr -= CODE_STATEMENT_MARKER_SIZE + 2;
    }
    else
    {
        CodeBufferPtr -= 2;
    }
}

auto CrunchAddressMarker(MCAddress address) -> char*
{
    if (!Crunch)
    {
        return nullptr;
    }

    CheckCodeBufferSpace();
    char saveCode = CodeBufferPtr[-1];
    CodeBufferPtr[-1] = static_cast<char>(TKN_ADDRESS_MARKER);
    char* slot = CodeBufferPtr;
    // Port: the chain is an offset from codeBuffer (the original stored the 4-byte pointer).
    int32_t chain = address ? static_cast<int32_t>(address - CodeBuffer) : CODE_ADDRESS_CHAIN_NULL;
    std::memcpy(slot, &chain, CODE_ADDRESS_SIZE);
    slot[CODE_ADDRESS_SIZE] = saveCode;
    CodeBufferPtr += CODE_ADDRESS_SIZE + 1;
    return slot;
}

auto FixupAddressMarker(MCAddress address) -> char*
{
    if (!Crunch)
    {
        return nullptr;
    }

    int32_t chain;
    std::memcpy(&chain, address, CODE_ADDRESS_SIZE);
    char* oldAddress = chain == CODE_ADDRESS_CHAIN_NULL ? nullptr : CodeBuffer + chain;
    int32_t offset = static_cast<int32_t>(CodeBufferPtr - address);
    std::memcpy(address, &offset, CODE_ADDRESS_SIZE);
    return oldAddress;
}

auto CrunchInteger(int32_t value) -> void
{
    if (Crunch)
    {
        CheckCodeBufferSpace();
        std::memcpy(CodeBufferPtr, &value, CODE_INTEGER_SIZE);
        CodeBufferPtr += CODE_INTEGER_SIZE;
    }
}

auto CrunchOffset(MCAddress address) -> void
{
    if (Crunch)
    {
        CheckCodeBufferSpace();
        int32_t offset = static_cast<int32_t>(address - CodeBufferPtr);
        std::memcpy(CodeBufferPtr, &offset, CODE_INTEGER_SIZE);
        CodeBufferPtr += CODE_INTEGER_SIZE;
    }
}

auto CreateCodeSegment() -> char*
{
    uint32_t codeSize = static_cast<uint32_t>(CodeBufferPtr - CodeBuffer);
    // Port fix: one more byte, a TKN_NONE after the code. execStatement's semicolon loop reads the token after a
    // routine's final ";", one byte past its segment (OB-108). The original's heap always had bytes there; an
    // exact-size block can end on a page boundary, and the read faults.
    char* codeSegment = AblMemory.AllocateArray<char>(codeSize + 1);
    CodeSegmentLimit = codeSegment + codeSize;
    std::memcpy(codeSegment, CodeBuffer, codeSize);
    codeSegment[codeSize] = TKN_NONE;
    CodeSegmentPtr = CodeSegmentLimit;
    CodeBufferPtr = CodeBuffer;
    return codeSegment;
}

auto GetCodeSymTableNodePtr() -> MCSymTableNodePtr
{
    MCSymTableNodePtr nodePtr;
    std::memcpy(&nodePtr, CodeSegmentPtr, CODE_SYMBOL_PTR_SIZE);
    CodeSegmentPtr += CODE_SYMBOL_PTR_SIZE;
    return nodePtr;
}

auto GetCodeStatementMarker() -> int32_t
{
    int32_t line = -1;

    if (CodeToken == TKN_STATEMENT_MARKER && IncludeDebugInfo)
    {
        FileNumber = static_cast<uint8_t>(*CodeSegmentPtr);
        std::memcpy(&line, CodeSegmentPtr + 1, sizeof(line));
        CodeSegmentPtr += CODE_STATEMENT_MARKER_SIZE;
    }

    return line;
}

auto GetCodeAddressMarker() -> char*
{
    char* address = nullptr;

    if (CodeToken == TKN_ADDRESS_MARKER)
    {
        int32_t offset;
        std::memcpy(&offset, CodeSegmentPtr, CODE_ADDRESS_SIZE);
        address = CodeSegmentPtr + offset - 1;
        CodeSegmentPtr += CODE_ADDRESS_SIZE;
    }

    return address;
}

auto GetCodeInteger() -> int32_t
{
    int32_t value;
    std::memcpy(&value, CodeSegmentPtr, CODE_INTEGER_SIZE);
    CodeSegmentPtr += CODE_INTEGER_SIZE;
    return value;
}

auto GetCodeAddress() -> char*
{
    int32_t offset;
    std::memcpy(&offset, CodeSegmentPtr, CODE_INTEGER_SIZE);
    char* address = CodeSegmentPtr + offset - 1;
    CodeSegmentPtr += CODE_INTEGER_SIZE;
    return address;
}

auto Pop() -> void
{
    --Tos;
}

auto GetCodeToken() -> void
{
    CodeToken = static_cast<MCTokenCodeType>(*CodeSegmentPtr++);
}

auto PushInteger(int32_t value) -> void
{
    PushItem()->Integer = value;
}

auto PushReal(float value) -> void
{
    PushItem()->Real = value;
}

auto PushByte(char value) -> void
{
    PushItem()->Byte = static_cast<uint8_t>(value);
}

auto PushAddress(MCAddress address) -> void
{
    PushItem()->Address = address;
}

auto PushStackFrameHeader(int32_t oldLevel, int32_t newLevel) -> void
{
    MCStackFrameHeaderPtr headerPtr = reinterpret_cast<MCStackFrameHeaderPtr>(StackFrameBasePtr);
    // Function value.
    PushInteger(0);
    // Static link: none for a routine of another module; the caller's frame for a routine nested in it; the
    // caller's own static link for a routine at the caller's level.
    MCStackItemPtr staticLink;

    if (newLevel == -1)
    {
        staticLink = nullptr;
    }
    else if (newLevel == oldLevel + 1)
    {
        staticLink = StackFrameBasePtr;
    }
    else if (newLevel == oldLevel)
    {
        staticLink = reinterpret_cast<MCStackItemPtr>(headerPtr->StaticLink.Address);
    }
    else
    {
        RuntimeError(ABL_ERR_RUNTIME_NESTED_FUNCTION_CALL);
        return;
    }

    PushAddress(reinterpret_cast<MCAddress>(staticLink));
    // Dynamic link.
    PushAddress(reinterpret_cast<MCAddress>(StackFrameBasePtr));
    // Return address (set by the caller).
    PushAddress(nullptr);
}

auto AllocLocal(MCTypePtr typePtr) -> void
{
    if (typePtr == IntegerTypePtr)
    {
        PushInteger(0);
    }
    else if (typePtr == RealTypePtr)
    {
        PushReal(0.0f);
    }
    else if (typePtr == BooleanTypePtr)
    {
        PushByte(0);
    }
    else if (typePtr == CharTypePtr)
    {
        PushByte(0);
    }
    else
    {
        switch (typePtr->Form)
        {
            case FRM_ENUM:
                PushInteger(0);
                break;
            case FRM_ARRAY:
            {
                char* localArray = AblMemory.AllocateArray<char>(static_cast<size_t>(typePtr->Size));

                if (!localArray)
                {
                    Fatal(0, " ABL: Unable to AblStackHeap->malloc local array ");
                }

                PushAddress(localArray);
                break;
            }

            default:
                break;
        }
    }
}

auto FreeLocal(MCSymTableNodePtr idPtr) -> void
{
    // Only local arrays own memory; a reference parameter's array belongs to the caller.
    if (idPtr->TypePtr->Form == FRM_ARRAY && idPtr->Defn.Key != DFN_REFPARAM)
    {
        MCStackItemPtr dataPtr = StackFrameBasePtr + idPtr->Defn.Info.Data.Offset;

        if (idPtr->Defn.Info.Data.VarType != VAR_TYPE_NORMAL || !dataPtr)
        {
            RuntimeError(ABL_ERR_RUNTIME_STACK_OVERFLOW);
            return;
        }

        AblMemory.Free(dataPtr->Address);
    }
}

auto RoutineEntry(MCSymTableNodePtr routineIdPtr) -> void
{
    if (Debugger)
    {
        Debugger->TraceRoutineEntry(routineIdPtr);
    }

    CodeSegmentPtr = routineIdPtr->Defn.Info.Routine.CodeSegment;
    ReturnValue = MCStackItem{};

    // Static and eternal locals live elsewhere.
    for (MCSymTableNodePtr varIdPtr = routineIdPtr->Defn.Info.Routine.Locals; varIdPtr; varIdPtr = varIdPtr->Next)
    {
        if (varIdPtr->Defn.Info.Data.VarType == VAR_TYPE_NORMAL)
        {
            AllocLocal(varIdPtr->TypePtr);
        }
    }
}

auto RoutineExit(MCSymTableNodePtr routineIdPtr) -> void
{
    if (Debugger)
    {
        Debugger->TraceRoutineExit(routineIdPtr);
    }

    for (MCSymTableNodePtr idPtr = routineIdPtr->Defn.Info.Routine.Params; idPtr; idPtr = idPtr->Next)
    {
        FreeLocal(idPtr);
    }

    for (MCSymTableNodePtr idPtr = routineIdPtr->Defn.Info.Routine.Locals; idPtr; idPtr = idPtr->Next)
    {
        if (idPtr->Defn.Info.Data.VarType == VAR_TYPE_NORMAL)
        {
            FreeLocal(idPtr);
        }
    }

    MCStackFrameHeaderPtr headerPtr = reinterpret_cast<MCStackFrameHeaderPtr>(StackFrameBasePtr);
    CodeSegmentPtr = headerPtr->ReturnAddress.Address;

    // A function leaves its value (the frame's first item) on the stack.
    if (routineIdPtr->TypePtr)
    {
        Tos = StackFrameBasePtr;
    }
    else
    {
        Tos = StackFrameBasePtr - 1;
    }

    StackFrameBasePtr = reinterpret_cast<MCStackItemPtr>(headerPtr->DynamicLink.Address);
}

auto Execute(MCSymTableNodePtr routineIdPtr) -> void
{
    MCSymTableNodePtr thisRoutineIdPtr = CurRoutineIdPtr;
    CurRoutineIdPtr = routineIdPtr;
    RoutineEntry(routineIdPtr);

    if (CallModuleInit)
    {
        CallModuleInit = 0;
        MCSymTableNodePtr moduleIdPtr = ModuleRegistry[CurModule->Handle].ModuleIdPtr;
        MCSymTableNodePtr initIdPtr =
            SearchSymTable(const_cast<char*>("init"), moduleIdPtr->Defn.Info.Routine.LocalSymTable);

        if (initIdPtr)
        {
            ExecRoutineCall(initIdPtr);
            // execRoutineCall reads the token after the call; back up to it.
            CodeSegmentPtr--;
        }
    }

    GetCodeToken();
    ExecStatement();
    ExitWithReturn = 0;
    ExitFromTacOrder = 0;
    RoutineExit(routineIdPtr);
    CurRoutineIdPtr = thisRoutineIdPtr;
}

auto ExecuteChild(MCSymTableNodePtr moduleIdPtr, MCSymTableNodePtr childRoutineIdPtr, MCAblParam* /*paramList*/) -> void
{
    // paramList is unused in MCX.EXE.
    MCSymTableNodePtr thisRoutineIdPtr = CurRoutineIdPtr;
    CurRoutineIdPtr = moduleIdPtr;
    RoutineEntry(moduleIdPtr);
    MCSymTableNodePtr initIdPtr = nullptr;

    if (CallModuleInit)
    {
        CallModuleInit = 0;
        initIdPtr = SearchSymTable(const_cast<char*>("init"), moduleIdPtr->Defn.Info.Routine.LocalSymTable);

        if (initIdPtr)
        {
            ExecRoutineCall(initIdPtr);
            CodeSegmentPtr--;
        }
    }

    // When the child is init itself, it has just run.
    if (initIdPtr != childRoutineIdPtr)
    {
        ExecRoutineCall(childRoutineIdPtr);
        CodeSegmentPtr--;
    }

    ExitWithReturn = 0;
    ExitFromTacOrder = 0;
    RoutineExit(moduleIdPtr);
    CurRoutineIdPtr = thisRoutineIdPtr;
}
