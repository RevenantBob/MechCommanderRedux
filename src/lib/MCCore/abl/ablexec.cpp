#include "stdafx.h"
#include "abl/ablexec.h"
#include "abl/abldbug.h"
#include "abl/ablenv.h"
#include "abl/MCAblErrors.h"
#include "abl/ablrtn.h"
#include "abl/ablxstmt.h"
#include "lib/MCFatal.h"

int IncludeDebugInfo = 1;
char* CodeSegmentPtr = nullptr;
char* StatementStartPtr = nullptr;
MCAblToken CodeToken{};
MCStackItemPtr Stack = nullptr;
MCStackItemPtr Tos = nullptr;
MCStackItemPtr StackFrameBasePtr = nullptr;
MCStackItemPtr StaticDataPtr = nullptr;
MCStackItem ReturnValue{};
int32_t ExecStatementCount = 0;
int32_t ExecLineNumber = 0;
int ExitFromTacOrder = 0;
int32_t Level = 0;
int32_t ExecFileNumber = 0;
MCBlockStore AblMemory;

namespace
{
    /// <summary>Advances tos to a new item, cleared (stack overflow is a runtime error).</summary>
    MCStackItemPtr PushItem()
    {
        MCStackItemPtr item = ++Tos;

        if (item >= Stack + MAXSIZE_STACK)
        {
            RuntimeError(MCAblRuntimeError::StackOverflow);
        }

        // Port fix: the original stored only the value's own bytes; the whole 8-byte slot is cleared first.
        *item = MCStackItem{};
        return item;
    }
}

auto GetCodeSymTableNodePtr() -> MCAblSymbol*
{
    MCAblSymbol* nodePtr;
    std::memcpy(&nodePtr, CodeSegmentPtr, CODE_SYMBOL_PTR_SIZE);
    CodeSegmentPtr += CODE_SYMBOL_PTR_SIZE;
    return nodePtr;
}

auto GetCodeStatementMarker() -> int32_t
{
    int32_t line = -1;

    if (CodeToken == MCAblToken::StatementMarker && IncludeDebugInfo)
    {
        ExecFileNumber = static_cast<uint8_t>(*CodeSegmentPtr);
        std::memcpy(&line, CodeSegmentPtr + 1, sizeof(line));
        CodeSegmentPtr += CODE_STATEMENT_MARKER_SIZE;
    }

    return line;
}

auto GetCodeAddressMarker() -> char*
{
    char* address = nullptr;

    if (CodeToken == MCAblToken::AddressMarker)
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
    CodeToken = static_cast<MCAblToken>(*CodeSegmentPtr++);
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
        RuntimeError(MCAblRuntimeError::NestedFunctionCall);
        return;
    }

    PushAddress(reinterpret_cast<MCAddress>(staticLink));
    // Dynamic link.
    PushAddress(reinterpret_cast<MCAddress>(StackFrameBasePtr));
    // Return address (set by the caller).
    PushAddress(nullptr);
}

auto AllocLocal(MCAblType* typePtr) -> void
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
            case MCAblTypeForm::Enum:
                PushInteger(0);
                break;
            case MCAblTypeForm::Array:
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

auto FreeLocal(MCAblSymbol* idPtr) -> void
{
    // Only local arrays own memory; a reference parameter's array belongs to the caller.
    if (idPtr->TypePtr->Form == MCAblTypeForm::Array && idPtr->Defn.Key != MCAblSymbolKind::RefParam)
    {
        MCStackItemPtr dataPtr = StackFrameBasePtr + idPtr->Defn.Info.Data.Offset;

        if (idPtr->Defn.Info.Data.VarType != MCAblStorage::Normal || !dataPtr)
        {
            RuntimeError(MCAblRuntimeError::StackOverflow);
            return;
        }

        AblMemory.Free(dataPtr->Address);
    }
}

auto RoutineEntry(MCAblSymbol* routineIdPtr) -> void
{
    if (Debugger)
    {
        Debugger->TraceRoutineEntry(routineIdPtr);
    }

    CodeSegmentPtr = routineIdPtr->Defn.Info.Routine.CodeSegment;
    ReturnValue = MCStackItem{};

    // Static and eternal locals live elsewhere.
    for (MCAblSymbol* varIdPtr = routineIdPtr->Defn.Info.Routine.Locals; varIdPtr; varIdPtr = varIdPtr->Next)
    {
        if (varIdPtr->Defn.Info.Data.VarType == MCAblStorage::Normal)
        {
            AllocLocal(varIdPtr->TypePtr);
        }
    }
}

auto RoutineExit(MCAblSymbol* routineIdPtr) -> void
{
    if (Debugger)
    {
        Debugger->TraceRoutineExit(routineIdPtr);
    }

    for (MCAblSymbol* idPtr = routineIdPtr->Defn.Info.Routine.Params; idPtr; idPtr = idPtr->Next)
    {
        FreeLocal(idPtr);
    }

    for (MCAblSymbol* idPtr = routineIdPtr->Defn.Info.Routine.Locals; idPtr; idPtr = idPtr->Next)
    {
        if (idPtr->Defn.Info.Data.VarType == MCAblStorage::Normal)
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

auto Execute(MCAblSymbol* routineIdPtr) -> void
{
    MCAblSymbol* thisRoutineIdPtr = CurRoutineIdPtr;
    CurRoutineIdPtr = routineIdPtr;
    RoutineEntry(routineIdPtr);

    if (CallModuleInit)
    {
        CallModuleInit = 0;
        MCAblSymbol* moduleIdPtr = ModuleRegistry[CurModule->Handle].ModuleIdPtr;
        MCAblSymbol* initIdPtr =
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

auto ExecuteChild(MCAblSymbol* moduleIdPtr, MCAblSymbol* childRoutineIdPtr, MCAblParam* /*paramList*/) -> void
{
    // paramList is unused in MCX.EXE.
    MCAblSymbol* thisRoutineIdPtr = CurRoutineIdPtr;
    CurRoutineIdPtr = moduleIdPtr;
    RoutineEntry(moduleIdPtr);
    MCAblSymbol* initIdPtr = nullptr;

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
