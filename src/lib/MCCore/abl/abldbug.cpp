#include "stdafx.h"
#include "abl/abldbug.h"
#include "abl/ablerr.h"
#include "abl/ablexec.h"
#include "abl/ablexpr.h"
#include "abl/ablrtn.h"
#include "abl/ablscan.h"
#include "abl/ablsymt.h"
#include "abl/ablxexpr.h"
#include "gui/afont.h"
#include "gui/aport.h"
#include "gui/atextbox.h"
#include "gui/updisp.h"
#include "lib/MCFatal.h"
#include "network/multplyr.h"
#include "platform/MCInput.h"
#include "vfx/vfxfuncs.h"

char MCDebugger::Message[MAXLEN_DEBUGGER_MESSAGE];
MCDebugger* Debugger = nullptr;
MCScrollingTextWindow* AblDebuggerOut = nullptr;
MCGuiTextObject* AblDebuggerIn = nullptr;

namespace
{
    /// <summary>
    /// Where variable <paramref name="idPtr"/> lives in the running frame (a local of an enclosing level through the
    /// static links), in the module's statics or among the eternals.
    /// </summary>
    /// <returns>Its item, or <paramref name="fallback"/> for any other variable type (the original then read the
    /// caller's buffer).</returns>
    auto VariableItem(MCSymTableNodePtr idPtr, MCStackItemPtr fallback) -> MCStackItemPtr
    {
        switch (idPtr->Defn.Info.Data.VarType)
        {
            case VAR_TYPE_NORMAL:
            {
                MCStackItemPtr frame = StackFrameBasePtr;

                for (int32_t delta = Level - idPtr->Level; delta > 0; delta--)
                {
                    frame = reinterpret_cast<MCStackItemPtr>(
                        reinterpret_cast<MCStackFrameHeaderPtr>(frame)->StaticLink.Address);
                }

                return frame + idPtr->Defn.Info.Data.Offset;
            }

            case VAR_TYPE_STATIC:
                return StaticDataPtr + idPtr->Defn.Info.Data.Offset;
            case VAR_TYPE_ETERNAL:
                return Stack + idPtr->Defn.Info.Data.Offset;
            default:
                return fallback;
        }
    }
}

auto MCWatchManager::Init(int32_t max) -> int32_t
{
    MaxWatches = max;
    NumWatches = 0;
    Watches = AblMemory.AllocateArray<MCWatch>(static_cast<size_t>(max));
    return Watches ? 0 : -1;
}

auto MCWatchManager::Destroy() -> void
{
    if (Watches)
    {
        AblMemory.Free(Watches);
        Watches = nullptr;
    }

    MaxWatches = 0;
    NumWatches = 0;
}

auto MCWatchManager::Add(MCSymTableNodePtr idPtr) -> MCWatchPtr
{
    MCDefinitionType idDefn = idPtr->Defn.Key;

    if (idDefn != DFN_CONST && idDefn != DFN_VAR && idDefn != DFN_VALPARAM && idDefn != DFN_REFPARAM)
    {
        return nullptr;
    }

    if (idPtr->Info)
    {
        return idPtr->Info;
    }

    if (NumWatches >= MaxWatches)
    {
        return nullptr;
    }

    MCWatchPtr watch = &Watches[NumWatches];
    idPtr->Info = watch;
    watch->IdPtr = idPtr;
    watch->Store = 0;
    watch->BreakOnStore = 0;
    watch->Fetch = 0;
    watch->BreakOnFetch = 0;
    NumWatches++;
    return watch;
}

auto MCWatchManager::Remove(MCSymTableNodePtr idPtr) -> int32_t
{
    if (!idPtr)
    {
        return 1;
    }

    if (!idPtr->Info)
    {
        return 2;
    }

    int32_t removeIndex = 0;

    while (removeIndex < NumWatches && &Watches[removeIndex] != idPtr->Info)
    {
        removeIndex++;
    }

    NumWatches--;
    idPtr->Info = nullptr;

    // Original behaviour (OB-040): the shift never advances, so only the next watch moves down (the original copied
    // it numWatches - removeIndex times); the ones after it stay put and the last one drops off the count.
    if (removeIndex < NumWatches)
    {
        Watches[removeIndex] = Watches[removeIndex + 1];
        Watches[removeIndex].IdPtr->Info = &Watches[removeIndex];
    }

    return 0;
}

auto MCWatchManager::RemoveAll() -> int32_t
{
    int32_t removed = NumWatches;

    for (int32_t i = 0; i < NumWatches; i++)
    {
        MCSymTableNodePtr idPtr = Watches[i].IdPtr;
        Watches[i] = MCWatch{};
        idPtr->Info = nullptr;
    }

    NumWatches = 0;
    return removed;
}

auto MCWatchManager::SetStore(MCSymTableNodePtr idPtr, int on, int breakOnStore) -> int32_t
{
    if (!idPtr)
    {
        return 1;
    }

    MCWatchPtr watch = idPtr->Info;

    if (on)
    {
        if (!watch)
        {
            watch = Add(idPtr);

            if (!watch)
            {
                return 2;
            }
        }

        watch->Store = 1;
        watch->BreakOnStore = breakOnStore;
        return 0;
    }

    if (watch)
    {
        // Keep the watch while it still watches fetches.
        if (watch->Fetch)
        {
            watch->Store = 0;
            watch->BreakOnStore = 0;
            return 0;
        }

        Remove(idPtr);
    }

    return 0;
}

auto MCWatchManager::SetFetch(MCSymTableNodePtr idPtr, int on, int breakOnFetch) -> int32_t
{
    if (!idPtr)
    {
        return 1;
    }

    MCWatchPtr watch = idPtr->Info;

    if (on)
    {
        if (!watch)
        {
            watch = Add(idPtr);

            if (!watch)
            {
                return 2;
            }
        }

        watch->Fetch = 1;
        watch->BreakOnFetch = breakOnFetch;
        return 0;
    }

    if (watch)
    {
        if (watch->Store)
        {
            watch->Fetch = 0;
            watch->BreakOnFetch = 0;
            return 0;
        }

        Remove(idPtr);
    }

    return 0;
}

auto MCWatchManager::GetStore(MCSymTableNodePtr idPtr) -> int32_t
{
    if (!idPtr->Info)
    {
        return 0;
    }

    return idPtr->Info->Store;
}

auto MCWatchManager::GetFetch(MCSymTableNodePtr idPtr) -> int32_t
{
    if (!idPtr->Info)
    {
        return 0;
    }

    return idPtr->Info->Fetch;
}

auto MCWatchManager::Print() -> void
{
}

auto MCBreakPointManager::Init(int32_t max) -> int32_t
{
    MaxBreakPoints = max;
    NumBreakPoints = 0;
    BreakPoints = AblMemory.AllocateArray<int32_t>(static_cast<size_t>(max));
    return BreakPoints ? 0 : -1;
}

auto MCBreakPointManager::Destroy() -> void
{
    if (BreakPoints)
    {
        AblMemory.Free(BreakPoints);
        BreakPoints = nullptr;
    }

    MaxBreakPoints = 0;
    NumBreakPoints = 0;
}

auto MCBreakPointManager::Add(int32_t lineNumber) -> int32_t
{
    if (NumBreakPoints == MaxBreakPoints)
    {
        return 1;
    }

    if (lineNumber < 1)
    {
        return 2;
    }

    int32_t index = 0;

    for (; index < NumBreakPoints; index++)
    {
        if (BreakPoints[index] == lineNumber)
        {
            return 0;
        }

        if (lineNumber < BreakPoints[index])
        {
            break;
        }
    }

    // Original behaviour (OB-041): the make-room shift copies forwards, so every break point after the insertion
    // point becomes a copy of the one that was there.
    for (int32_t i = index; i < NumBreakPoints; i++)
    {
        BreakPoints[i + 1] = BreakPoints[i];
    }

    NumBreakPoints++;
    BreakPoints[index] = lineNumber;
    return 0;
}

auto MCBreakPointManager::Remove(int32_t lineNumber) -> int32_t
{
    int32_t index = 0;

    while (index < NumBreakPoints && BreakPoints[index] != lineNumber)
    {
        index++;
    }

    // Original behaviour (OB-042): the count drops even when there is no break point on that line, which loses the
    // last one.
    NumBreakPoints--;

    for (int32_t i = index; i < NumBreakPoints; i++)
    {
        BreakPoints[i] = BreakPoints[i + 1];
    }

    return 0;
}

auto MCBreakPointManager::RemoveAll() -> int32_t
{
    int32_t removed = NumBreakPoints;
    NumBreakPoints = 0;
    return removed;
}

auto MCBreakPointManager::IsBreakPoint(int32_t lineNumber) -> int
{
    for (int32_t i = 0; i < NumBreakPoints; i++)
    {
        if (BreakPoints[i] == lineNumber)
        {
            return 1;
        }
    }

    return 0;
}

auto MCBreakPointManager::Print() -> void
{
}

auto MCDebugger::Init(void (*callback)(char* s), MCAblModule* ablModule) -> int32_t
{
    PrintCallback = callback;
    Module = ablModule;

    if (ablModule)
    {
        WatchManager = ablModule->WatchManager;
        BreakPointManager = ablModule->BreakPointManager;
    }

    return 0;
}

auto MCDebugger::Destroy() -> void
{
}

auto MCDebugger::Print(char* s) -> int32_t
{
    if (PrintCallback)
    {
        PrintCallback(s);
    }

    return 0;
}

auto MCDebugger::SetModule(MCAblModule* ablModule) -> void
{
    Module = ablModule;
    BreakPointManager = ablModule->BreakPointManager;
    WatchManager = ablModule->WatchManager;
    // Faithful: the module's trace flag sets all three trace modes (its traceEntry/traceExit are not read).
    Step = ablModule->Step;
    TraceExit = ablModule->Trace;
    TraceEntry = ablModule->Trace;
    Trace = ablModule->Trace;
}

auto MCDebugger::SetWatch(int32_t states) -> int32_t
{
    GetToken();

    if (CurToken != TKN_IDENTIFIER)
    {
        if (CurToken == TKN_SEMICOLON)
        {
            Print(const_cast<char*>("Variables currently watched:\n"));
            WatchManager->Print();
        }

        return 0;
    }

    MCSymTableNodePtr idPtr = nullptr;
    SearchAndFindAllSymTables(idPtr);
    GetToken();
    // Bits: 1 store off, 2 store on, 4 fetch off, 8 fetch on, 16 break.
    int breakFlag = (states >> 4) & 1;

    if (states & 1)
    {
        WatchManager->SetStore(idPtr, 0, breakFlag);
    }
    else if (states & 2)
    {
        WatchManager->SetStore(idPtr, 1, breakFlag);
    }

    if (states & 4)
    {
        WatchManager->SetFetch(idPtr, 0, breakFlag);
    }
    else if (states & 8)
    {
        WatchManager->SetFetch(idPtr, 1, breakFlag);
    }

    return 0;
}

auto MCDebugger::AddBreakPoint() -> int32_t
{
    GetToken();

    if (CurToken == TKN_NUMBER)
    {
        if (CurLiteral.Type == LIT_INTEGER)
        {
            BreakPointManager->Add(CurLiteral.Value.Integer);
        }

        GetToken();
    }
    else if (CurToken == TKN_SEMICOLON)
    {
        // Faithful: the break point list is announced with the watch list's title.
        Print(const_cast<char*>("Variables currently watched:\n"));
        BreakPointManager->Print();
    }

    return 0;
}

auto MCDebugger::RemoveBreakPoint() -> int32_t
{
    GetToken();

    if (CurToken == TKN_NUMBER)
    {
        if (CurLiteral.Type == LIT_INTEGER)
        {
            BreakPointManager->Remove(CurLiteral.Value.Integer);
        }

        GetToken();
    }
    else if (CurToken == TKN_SEMICOLON)
    {
        BreakPointManager->RemoveAll();
    }

    return 0;
}

auto MCDebugger::SprintStatement(char* dest) -> void
{
    bool done = false;
    const char* code = StatementStartPtr;

    do
    {
        MCTokenCodeType token = static_cast<MCTokenCodeType>(*code);
        const char* next = code + 1;

        switch (token)
        {
            case TKN_SEMICOLON:
            case TKN_END_IF:
            case TKN_END_WHILE:
            case TKN_END_FOR:
            case TKN_END_FUNCTION:
            case TKN_END_MODULE:
            case TKN_END_LIBRARY:
            case TKN_END_CASE:
            case TKN_END_SWITCH:
            case TKN_THEN:
                done = true;
                break;
            case TKN_STATEMENT_MARKER:
                // The next statement.
                return;
            default:
                break;
        }

        switch (token)
        {
            case TKN_IDENTIFIER:
            case TKN_NUMBER:
            case TKN_STRING:
            {
                MCSymTableNodePtr idPtr;
                std::memcpy(&idPtr, next, sizeof(idPtr));
                std::strcat(dest, " ");
                std::strcat(dest, idPtr->Name);
                next += CODE_SYMBOL_PTR_SIZE;
                break;
            }

            case TKN_ADDRESS_MARKER:
                next += CODE_ADDRESS_SIZE;
                break;
            default:
            {
                std::strcat(dest, " ");
                std::strcat(dest, TokenStrings[token]);
                break;
            }
        }

        code = next;
    } while (!done);
}

auto MCDebugger::SprintLineNumber(char* dest) -> void
{
    std::sprintf(dest, "LINE#");
}

auto MCDebugger::SprintDataValue(char* dest, MCStackItemPtr data, MCTypePtr dataType) -> void
{
    if (dataType->Form == FRM_ENUM && dataType != BooleanTypePtr)
    {
        dataType = IntegerTypePtr;
    }

    if (dataType == IntegerTypePtr)
    {
        std::sprintf(dest, "%d", data->Integer);
    }
    else if (dataType == RealTypePtr)
    {
        std::sprintf(dest, "%0.6f", static_cast<double>(data->Real));
    }
    else if (dataType == BooleanTypePtr)
    {
        std::sprintf(dest, "%s", data->Integer == 1 ? "true" : "false");
    }
    else if (dataType == CharTypePtr)
    {
        std::sprintf(dest, "%c", data->Byte);
    }
    else if (dataType->Form == FRM_ARRAY)
    {
        if (dataType->Info.Array.ElementTypePtr == CharTypePtr)
        {
            std::sprintf(dest, "CHAR ARRAY");
        }
        else
        {
            std::sprintf(dest, "ARRAY");
        }
    }
}

auto MCDebugger::SprintSimpleValue(char* dest, MCSymTableNodePtr symbol) -> int32_t
{
    MCTypePtr typePtr = symbol->TypePtr;

    if (symbol->Defn.Key == DFN_CONST)
    {
        if (typePtr == IntegerTypePtr)
        {
            std::sprintf(dest, "%d", symbol->Defn.Info.Constant.Value.Integer);
        }
        else if (typePtr == CharTypePtr)
        {
            std::sprintf(dest, "%c", symbol->Defn.Info.Constant.Value.Character);
        }
        else
        {
            std::sprintf(dest, "%.4f", static_cast<double>(symbol->Defn.Info.Constant.Value.Real));
        }

        return 0;
    }

    MCStackItemPtr valuePtr = VariableItem(symbol, reinterpret_cast<MCStackItemPtr>(dest));

    if (symbol->Defn.Key == DFN_REFPARAM && typePtr->Form != FRM_ARRAY)
    {
        valuePtr = reinterpret_cast<MCStackItemPtr>(valuePtr->Address);
    }

    MCTypePtr baseTypePtr = BaseType(typePtr);

    if (typePtr->Form == FRM_ARRAY)
    {
        std::sprintf(dest, "ARRAY");
    }
    else if (baseTypePtr == IntegerTypePtr || typePtr->Form == FRM_ENUM)
    {
        std::sprintf(dest, "%d", valuePtr->Integer);
    }
    else if (baseTypePtr == CharTypePtr)
    {
        std::sprintf(dest, "\"%c\"", valuePtr->Byte);
    }
    else
    {
        std::sprintf(dest, "%.4f", static_cast<double>(valuePtr->Real));
    }

    return 0;
}

auto MCDebugger::SprintArrayValue(char* dest, MCSymTableNodePtr symbol, char* subscriptString) -> int32_t
{
    if (symbol->Defn.Key == DFN_CONST)
    {
        std::sprintf(dest, "\"%s\"", symbol->Defn.Info.Constant.Value.StringPtr);
        return 0;
    }

    MCStackItemPtr arrayItem = VariableItem(symbol, reinterpret_cast<MCStackItemPtr>(dest));
    MCTypePtr typePtr = symbol->TypePtr;
    char* element = arrayItem->Address;

    if (subscriptString)
    {
        // "[i][j]..." (or "[i,j]"): skip the first bracket, then split on commas and closing brackets.
        for (char* subscript = std::strtok(subscriptString + 1, ",]"); subscript;
             subscript = std::strtok(nullptr, ",]"))
        {
            int32_t index = std::atoi(subscript);

            if (index < 0 || index >= typePtr->Info.Array.ElementCount)
            {
                return 1;
            }

            typePtr = typePtr->Info.Array.ElementTypePtr;
            element += typePtr->Size * index;
        }
    }

    MCTypePtr baseTypePtr = BaseType(typePtr);

    if (typePtr->Form == FRM_ARRAY)
    {
        if (typePtr->Info.Array.ElementTypePtr == CharTypePtr)
        {
            std::sprintf(dest, "\"%s\"", element);
        }
        else
        {
            std::sprintf(dest, "Could you be more specific?");
        }
    }
    else if (baseTypePtr == IntegerTypePtr || typePtr->Form == FRM_ENUM)
    {
        std::sprintf(dest, "%d", *reinterpret_cast<int32_t*>(element));
    }
    else if (baseTypePtr == CharTypePtr)
    {
        std::sprintf(dest, "\"%c\"", *element);
    }
    else
    {
        std::sprintf(dest, "%.4f", static_cast<double>(*reinterpret_cast<float*>(element)));
    }

    return 0;
}

auto MCDebugger::SprintValue(char* dest, char* exprString) -> int32_t
{
    char* subscripts = std::strchr(exprString, '[');

    if (!subscripts)
    {
        MCSymTableNodePtr symbol = DebugModule->FindSymbol(exprString, CurRoutineIdPtr);

        if (!symbol)
        {
            return 1;
        }

        if (symbol->TypePtr->Form != FRM_ARRAY)
        {
            SprintSimpleValue(dest, symbol);
            return 0;
        }

        symbol = DebugModule->FindSymbol(exprString, CurRoutineIdPtr);

        if (!symbol)
        {
            return 1;
        }

        SprintArrayValue(dest, symbol, nullptr);
        return 0;
    }

    char subscriptString[256];
    std::strcpy(subscriptString, subscripts);
    *subscripts = '\0';
    MCSymTableNodePtr symbol = DebugModule->FindSymbol(exprString, CurRoutineIdPtr);

    if (!symbol)
    {
        return 1;
    }

    SprintArrayValue(dest, symbol, subscriptString);
    return 0;
}

auto MCDebugger::TraceStatementExecution() -> int32_t
{
    int32_t stepping = Step;

    if (BreakPointManager && BreakPointManager->IsBreakPoint(ExecLineNumber))
    {
        std::sprintf(Message, "HIT BP: (%d) %s [%d]", Module->Id, Module->Name, ExecLineNumber);
        Print(Message);
        DebugMode();
        return 0;
    }

    if (stepping)
    {
        DebugMode();
    }

    return 0;
}

auto MCDebugger::TraceRoutineEntry(MCSymTableNodePtr idPtr) -> int32_t
{
    if (TraceEntry)
    {
        std::sprintf(Message, "ENTER (%d) %s:%s", Module->Id, Module->Name, idPtr->Name);
        Print(Message);
    }

    return 0;
}

auto MCDebugger::TraceRoutineExit(MCSymTableNodePtr idPtr) -> int32_t
{
    if (TraceExit)
    {
        std::sprintf(Message, "EXIT (%d) %s:%s", Module->Id, Module->Name, idPtr->Name);
        Print(Message);
    }

    return 0;
}

auto MCDebugger::TraceDataStore(MCSymTableNodePtr id, MCTypePtr idType, MCStackItemPtr target, MCTypePtr targetType)
    -> int32_t
{
    MCWatchPtr watch = id->Info;

    if (watch && watch->Store)
    {
        char valueString[256];
        SprintDataValue(valueString, target, targetType);
        const char* format =
            idType->Form == FRM_ARRAY ? "STORE: (%d) %s [%d] -> %s[#] = %s\n" : "STORE: (%d) %s [%d] -> %s = %s\n";
        std::sprintf(Message, format, Module->Id, Module->Name, ExecLineNumber, id->Name, valueString);
        Print(Message);

        if (watch->BreakOnStore)
        {
            DebugMode();
        }
    }

    return 0;
}

auto MCDebugger::TraceDataFetch(MCSymTableNodePtr id, MCTypePtr idType, MCStackItemPtr data) -> int32_t
{
    MCWatchPtr watch = id->Info;

    if (watch && watch->Fetch)
    {
        char valueString[256];
        SprintDataValue(valueString, data, idType);
        const char* format =
            id->TypePtr->Form == FRM_ARRAY ? "FETCH: (%d) %s [%d] - %s[#] = %s\n" : "FETCH: (%d) %s [%d] - %s = %s\n";
        std::sprintf(Message, format, Module->Id, Module->Name, ExecLineNumber, id->Name, valueString);
        Print(Message);

        if (watch->BreakOnFetch)
        {
            DebugMode();
        }
    }

    return 0;
}

auto MCDebugger::ShowValue() -> void
{
    GetToken();

    if (CurToken == TKN_SEMICOLON)
    {
        Print(const_cast<char*>("Bad Expression.\n"));
        return;
    }

    MCTypePtr expressionTypePtr = Expression();

    if (ErrorCount > 0)
    {
        return;
    }

    char* savedCodeSegmentPtr = CodeSegmentPtr;
    MCTokenCodeType savedCodeToken = CodeToken;
    ExecExpression();

    if (expressionTypePtr->Form == FRM_ARRAY)
    {
        Print(const_cast<char*>("SHOW ARRAY\n"));
    }
    else
    {
        char valueString[256];
        SprintDataValue(valueString, Tos, expressionTypePtr);
        std::strcat(valueString, "\n");
        Print(valueString);
    }

    Pop();
    CodeSegmentPtr = savedCodeSegmentPtr;
    CodeToken = savedCodeToken;
}

auto MCDebugger::AssignVariable() -> void
{
    GetToken();
}

auto MCDebugger::DisplayModuleInstanceRegistry(int32_t) -> void
{
    // Two per line, whatever numCols says.
    for (int32_t row = 0; row < (NumModuleInstances + 1) / 2; row++)
    {
        char line[200];
        MCAblModule* left = ModuleInstanceRegistry[row * 2];
        std::sprintf(line, "(%02d) %-20s ", left->Id, left->Name);

        if (row * 2 + 1 < NumModuleInstances)
        {
            char column[40];
            MCAblModule* right = ModuleInstanceRegistry[row * 2 + 1];
            std::sprintf(column, "(%02d) %-20s ", right->Id, right->Name);
            std::strcat(line, column);
        }

        Print(line);
    }
}

auto MCDebugger::ProcessCommand(int32_t commandId, char* strParam1, int32_t numParam1, MCAblModule* moduleParam1)
    -> void
{
    static char blankLine[] = " ";

    switch (commandId)
    {
        case 0:
        {
            // Select the module (or list them).
            if (!moduleParam1)
            {
                Print(blankLine);
                DisplayModuleInstanceRegistry(2);
                std::sprintf(Message, "CURRENT MODULE: %s", DebugModule->Name);
            }
            else
            {
                DebugModule = moduleParam1;
                Print(blankLine);
                std::sprintf(Message, "SET MODULE: %s", DebugModule->Name);
            }
            break;
        }
        case 1:
        {
            // Trace on or off (tracing turns stepping off).
            if (numParam1 == 0)
            {
                DebugModule->Trace = 0;
                DebugModule->TraceEntry = 0;
                DebugModule->TraceExit = 0;

                if (Module == DebugModule)
                {
                    Trace = 0;
                    TraceEntry = 0;
                    TraceExit = 0;
                }
            }
            else
            {
                DebugModule->Trace = 1;
                DebugModule->TraceEntry = 1;
                DebugModule->TraceExit = 1;
                DebugModule->Step = 0;

                if (Module == DebugModule)
                {
                    Trace = 1;
                    TraceEntry = 1;
                    TraceExit = 1;
                    Step = 0;
                }
            }

            return;
        }
        case 2:
        {
            // Step on or off (stepping turns tracing off).
            if (numParam1 == 0)
            {
                DebugModule->Step = 0;

                if (Module == DebugModule)
                {
                    Step = 0;
                }
            }
            else
            {
                DebugModule->Step = 1;
                DebugModule->Trace = 0;
                DebugModule->TraceEntry = 0;
                DebugModule->TraceExit = 0;

                if (Module == DebugModule)
                {
                    Step = 1;
                    Trace = 0;
                    TraceEntry = 0;
                    TraceExit = 0;
                }
            }

            return;
        }
        case 3:
        {
            Print(blankLine);
            DebugModule->BreakPointManager->Add(numParam1);
            std::sprintf(Message, "SET BP: %s (%d)", DebugModule->Name, numParam1);
            break;
        }
        case 4:
        {
            Print(blankLine);
            DebugModule->BreakPointManager->Remove(numParam1);
            std::sprintf(Message, "REMOVE BP: %s (%d)", DebugModule->Name, numParam1);
            break;
        }
        case 5:
        {
            // Watch: numParam1 holds the setWatch bits.
            Print(blankLine);
            MCAblModule* watchModule = DebugModule;
            MCSymTableNodePtr idPtr = watchModule->FindSymbol(strParam1);

            if (!idPtr)
            {
                Print(const_cast<char*>("Unknown identifier in current scope.\n"));
                return;
            }

            MCWatchManagerPtr moduleWatches = watchModule->WatchManager;
            int breakFlag = (numParam1 >> 4) & 1;

            if ((numParam1 & 2) && moduleWatches->SetStore(idPtr, 1, breakFlag) == 2)
            {
                Print(const_cast<char*>("Reached max watch limit--unable to set watch.\n"));
                return;
            }

            if ((numParam1 & 8) && moduleWatches->SetFetch(idPtr, 1, breakFlag) == 2)
            {
                Print(const_cast<char*>("Reached max watch limit--unable to set watch.\n"));
                return;
            }

            if (numParam1 & 1)
            {
                moduleWatches->SetStore(idPtr, 0, breakFlag);
            }

            if (numParam1 & 4)
            {
                moduleWatches->SetFetch(idPtr, 0, breakFlag);
            }

            int32_t watchesStores = moduleWatches->GetStore(idPtr);
            int32_t watchesFetches = moduleWatches->GetFetch(idPtr);

            if (!watchesStores && !watchesFetches)
            {
                std::sprintf(Message, "REMOVE WATCH: %s.%s", watchModule->Name, strParam1);
            }
            else
            {
                std::sprintf(Message, "SET WATCH: %s.%s (", watchModule->Name, strParam1);

                if (watchesStores)
                {
                    std::strcat(Message, "s");
                }

                if (watchesFetches)
                {
                    std::strcat(Message, "f");
                }

                std::strcat(Message, ")");
            }
            break;
        }

        case 6:
        {
            DebugModule->WatchManager->RemoveAll();
            return;
        }
        case 7:
        {
            Print(blankLine);
            int32_t err = SprintValue(Message, strParam1);

            if (err != 0)
            {
                if (err == 1)
                {
                    Print(const_cast<char*>("Unknown identifier in current scope."));
                }

                return;
            }
            break;
        }

        case 8:
        {
            // Resume.
            DebugCommand = 0;
            return;
        }
        case 9:
        {
            Print(blankLine);
            Print(const_cast<char*>("b{+|-} <line#>         set/remove breakpt"));
            Print(const_cast<char*>("m [0 thru warrior #]   set current module (or list them)"));
            Print(const_cast<char*>("w[f|s]{+|-}{.} <variable> set/remove variable watch (fetch & store)"));
            Print(const_cast<char*>("p <variable>           display current value of variable"));
            Print(const_cast<char*>("s{+|-}                 start/stop step mode"));
            Print(const_cast<char*>("t{+|-}                 start/stop trace mode"));
            Print(const_cast<char*>("??                     current module info"));
            Print(const_cast<char*>("?                      help"));
            return;
        }
        case 10:
        {
            Print(blankLine);
            std::sprintf(Message, "CURRENT MODULE: %s", DebugModule->Name);
            Print(Message);
            int32_t numStatics;
            int32_t staticsSize;
            int32_t largestStatic = 0;
            DebugModule->GetInfo(numStatics, staticsSize, nullptr);

            if (numStatics <= 256)
            {
                int32_t sizeList[256];
                DebugModule->GetInfo(numStatics, staticsSize, sizeList);

                for (int32_t i = 0; i < numStatics; i++)
                {
                    largestStatic = std::max(largestStatic, sizeList[i]);
                }
            }

            std::sprintf(Message, "%d static vars, %d bytes, %d largest", numStatics, staticsSize, largestStatic);
            break;
        }

        default:
            return;
    }

    Print(Message);
}

auto MCDebugger::DebugMode() -> void
{
    Message[0] = '\0';
    DebugModule = Module;
    SprintStatement(Message);
    Print(Message);
    DebugCommand = 1;

    do
    {
        // The game's own loop (aSystem::run) while the debugger window takes the commands.
        PerfStartTime = MCPort::PerformanceCounter();

        if (!MCInput::PumpMessages())
        {
            DebugCommand = 0;
            Halt = 0;
            Trace = 0;
            Step = 0;
            TraceEntry = 0;
            TraceExit = 0;
        }

        if (ApplicationActive)
        {
            UpdateDisplay(TakeScreenShot, 0, 0, 0, 0);
            TakeScreenShot = 0;
            PerfStopTime = MCPort::PerformanceCounter();

            if (MPlayer)
            {
                MPlayer->ProcessReceiveList();
            }

            CheckMouse();
        }

        PrevStart = PerfStartTime;
        FrameRate = static_cast<float>(CountsPerSecond) / static_cast<float>(PerfStopTime - PerfStartTime);
    } while (DebugCommand);
}

auto AblGetDebugger() -> MCDebugger*
{
    return Debugger;
}

#if ABL_DEBUGGER_WINDOWS

MCScrollingTextWindow::~MCScrollingTextWindow()
{
    MCGuiObject::Destroy();
}

auto MCScrollingTextWindow::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    int32_t err = MCGuiObject::Init(xPos, yPos, width, height, name);

    if (err != 0)
    {
        return err;
    }

    NumColumns = width / 10;
    NumLines = height / 10;
    VfxPaneWipe(Port()->Frame(), BackColor());
    return 0;
}

auto MCScrollingTextWindow::Resize(int32_t width, int32_t height) -> void
{
    NumColumns = width / 10;
    NumLines = height / 10;
    MCGuiObject::Resize(width, height);
    int32_t color = BackColor();
    VfxPaneWipe(Port()->Frame(), color);
}

auto MCScrollingTextWindow::Draw() -> void
{
    MCGuiObject::Draw();
}

auto MCScrollingTextWindow::HandleEvent(MCGuiEvent* event) -> void
{
    MCGuiObject::HandleEvent(event);
}

auto MCScrollingTextWindow::Clear() -> void
{
    NumColumns = 0;
    NumLines = 0;
}

auto MCScrollingTextWindow::Print(char* s) -> void
{
    MCPane* pane = Port()->Frame();
    VfxPaneScroll(pane, 0, -10, 0, BackColor());
    SystemFont->WriteString(pane, 2, NumLines * 10 - 10, reinterpret_cast<uint8_t*>(s), -1);
}

MCDebuggerWindow::~MCDebuggerWindow()
{
    MCGuiTitleWindow::Destroy();
}

auto MCDebuggerWindow::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    int32_t err = MCGuiTitleWindow::Init(xPos, yPos, width, height, name);

    if (err != 0)
    {
        return err;
    }

    SetBackColor(10);

    AblDebuggerOut = new MCScrollingTextWindow;

    if (!AblDebuggerOut)
    {
        Fatal(0, "Not enough memory to make debugger windows");
    }

    err = AblDebuggerOut->Init(0, 0, width, height - 40, const_cast<char*>("ABL Out"));

    if (err != 0)
    {
        return err;
    }

    AddChild(AblDebuggerOut);

    AblDebuggerIn = new MCGuiTextObject;

    if (!AblDebuggerIn)
    {
        Fatal(0, "Not enough memory to make debugger windows");
    }

    err = AblDebuggerIn->Init(0, this->Height() - 36, 260, 36, nullptr);

    if (err != 0)
    {
        return err;
    }

    AblDebuggerIn->SetText(const_cast<char*>("\"?\" for help"));
    AddChild(AblDebuggerIn);
    Draw();
    return 0;
}

auto MCDebuggerWindow::Destroy() -> void
{
    if (AblDebuggerIn)
    {
        AblDebuggerIn->Destroy();
        delete AblDebuggerIn;
        AblDebuggerIn = nullptr;
    }

    if (AblDebuggerOut)
    {
        AblDebuggerOut->Destroy();
        delete AblDebuggerOut;
        AblDebuggerOut = nullptr;
    }

    MCGuiTitleWindow::Destroy();
}

auto MCDebuggerWindow::Resize(int32_t width, int32_t height) -> void
{
    AblDebuggerOut->Resize(width, height - 40);
    AblDebuggerIn->Resize(width, 36);
    AblDebuggerIn->MoveTo(0, height - 36);
    MCGuiTitleWindow::Resize(width, height);
    Draw();
}

#endif
