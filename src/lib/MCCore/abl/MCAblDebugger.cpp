#include "stdafx.h"
#include "abl/MCAblDebugger.h"
#include "abl/MCAblRuntime.h"
#include "abl/MCAblSymbolTable.h"
#include "gui/MCGuiInput.h"
#include "gui/MCGuiSystem.h"
#include "gui/MCUpdateDisplay.h"
#include "network/MCMultiPlayer.h"
#include "platform/MCInput.h"

MCAblWatchManager::~MCAblWatchManager()
{
    RemoveAll();
}

auto MCAblWatchManager::Add(MCAblSymbol* idPtr) -> MCWatch*
{
    const MCAblSymbolKind idDefn = idPtr->Defn.Key;

    if (idDefn != MCAblSymbolKind::Const && idDefn != MCAblSymbolKind::Var && idDefn != MCAblSymbolKind::ValueParam &&
        idDefn != MCAblSymbolKind::RefParam)
    {
        return nullptr;
    }

    if (idPtr->Watch)
    {
        return idPtr->Watch;
    }

    MCWatch* watch = _Watches.emplace_back(std::make_unique<MCWatch>()).get();
    watch->IdPtr = idPtr;
    idPtr->Watch = watch;
    return watch;
}

auto MCAblWatchManager::Remove(MCAblSymbol* idPtr) -> int32_t
{
    if (!idPtr)
    {
        return 1;
    }

    if (!idPtr->Watch)
    {
        return 2;
    }

    std::erase_if(_Watches, [&](const std::unique_ptr<MCWatch>& watch) { return watch.get() == idPtr->Watch; });
    idPtr->Watch = nullptr;
    return 0;
}

auto MCAblWatchManager::RemoveAll() -> int32_t
{
    const auto removed = static_cast<int32_t>(_Watches.size());

    for (const std::unique_ptr<MCWatch>& watch : _Watches)
    {
        watch->IdPtr->Watch = nullptr;
    }

    _Watches.clear();
    return removed;
}

auto MCAblWatchManager::SetStore(MCAblSymbol* idPtr, bool on, bool breakOnStore) -> int32_t
{
    if (!idPtr)
    {
        return 1;
    }

    MCWatch* watch = idPtr->Watch;

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

        watch->Store = true;
        watch->BreakOnStore = breakOnStore;
        return 0;
    }

    if (watch)
    {
        // Keep the watch while it still watches fetches.
        if (watch->Fetch)
        {
            watch->Store = false;
            watch->BreakOnStore = false;
            return 0;
        }

        Remove(idPtr);
    }

    return 0;
}

auto MCAblWatchManager::SetFetch(MCAblSymbol* idPtr, bool on, bool breakOnFetch) -> int32_t
{
    if (!idPtr)
    {
        return 1;
    }

    MCWatch* watch = idPtr->Watch;

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

        watch->Fetch = true;
        watch->BreakOnFetch = breakOnFetch;
        return 0;
    }

    if (watch)
    {
        if (watch->Store)
        {
            watch->Fetch = false;
            watch->BreakOnFetch = false;
            return 0;
        }

        Remove(idPtr);
    }

    return 0;
}

auto MCAblWatchManager::GetStore(const MCAblSymbol* idPtr) -> bool
{
    return idPtr->Watch && idPtr->Watch->Store;
}

auto MCAblWatchManager::GetFetch(const MCAblSymbol* idPtr) -> bool
{
    return idPtr->Watch && idPtr->Watch->Fetch;
}

auto MCAblBreakPointManager::Add(int32_t lineNumber) -> int32_t
{
    if (lineNumber < 1)
    {
        return 2;
    }

    _Lines.insert(lineNumber);
    return 0;
}

auto MCAblBreakPointManager::RemoveAll() -> int32_t
{
    const auto removed = static_cast<int32_t>(_Lines.size());
    _Lines.clear();
    return removed;
}

MCAblDebugger::MCAblDebugger(MCAblRuntime& runtime, std::function<void(std::string_view)> print)
    : _Runtime(runtime), _Print(std::move(print))
{
}

auto MCAblDebugger::Print(std::string_view text) const -> void
{
    if (_Print)
    {
        _Print(text);
    }
}

auto MCAblDebugger::SetModule(MCAblModule* ablModule) -> void
{
    _Module = ablModule;
    // Faithful: the module's trace flag sets all three trace modes (its TraceEntry/TraceExit are not read).
    _Step = ablModule->Step;
    _TraceExit = ablModule->Trace;
    _TraceEntry = ablModule->Trace;
    _Trace = ablModule->Trace;
}

auto MCAblDebugger::StatementText() const -> std::string
{
    std::string text;
    const char* code = _Runtime.StatementStart();
    bool done = false;

    do
    {
        const auto token = static_cast<MCAblToken>(*code);
        const char* next = code + 1;

        switch (token)
        {
            case MCAblToken::Semicolon:
            case MCAblToken::EndIf:
            case MCAblToken::EndWhile:
            case MCAblToken::EndFor:
            case MCAblToken::EndFunction:
            case MCAblToken::EndModule:
            case MCAblToken::EndLibrary:
            case MCAblToken::EndCase:
            case MCAblToken::EndSwitch:
            case MCAblToken::Then:
            {
                done = true;
                break;
            }

            case MCAblToken::StatementMarker:
            {
                // The next statement.
                return text;
            }

            default:
            {
                break;
            }
        }

        switch (token)
        {
            case MCAblToken::Identifier:
            case MCAblToken::Number:
            case MCAblToken::String:
            {
                MCAblSymbol* idPtr = nullptr;
                std::memcpy(&idPtr, next, sizeof(idPtr));
                text += ' ';
                text += idPtr->Name;
                next += AblCodeSymbolSize;
                break;
            }

            case MCAblToken::AddressMarker:
            {
                next += AblCodeAddressSize;
                break;
            }

            default:
            {
                text += ' ';
                text += MCAblTokenText(token);
                break;
            }
        }

        code = next;
    } while (!done);

    return text;
}

auto MCAblDebugger::DataValueText(const MCAblStackItem* data, MCAblType* dataType) -> std::string
{
    if (dataType->Form == MCAblTypeForm::Enum && dataType != BooleanTypePtr)
    {
        dataType = IntegerTypePtr;
    }

    if (dataType == IntegerTypePtr)
    {
        return std::format("{}", data->Integer);
    }

    if (dataType == RealTypePtr)
    {
        return std::format("{:.6f}", data->Real);
    }

    if (dataType == BooleanTypePtr)
    {
        return data->Integer == 1 ? "true" : "false";
    }

    if (dataType == CharTypePtr)
    {
        return std::string(1, static_cast<char>(data->Byte));
    }

    if (dataType->Form == MCAblTypeForm::Array)
    {
        return dataType->Array.ElementTypePtr == CharTypePtr ? "CHAR ARRAY" : "ARRAY";
    }

    return {};
}

auto MCAblDebugger::VariableItem(const MCAblSymbol* idPtr) const -> MCAblStackItem*
{
    switch (idPtr->Defn.Info.Data.VarType)
    {
        case MCAblStorage::Normal:
        {
            MCAblStackItem* frame = _Runtime.Frame();

            for (int32_t delta = _Runtime.Level() - idPtr->Level; delta > 0; delta--)
            {
                frame = reinterpret_cast<MCAblStackItem*>(
                    reinterpret_cast<MCAblStackFrameHeader*>(frame)->StaticLink.Address);
            }

            return frame + idPtr->Defn.Info.Data.Offset;
        }

        case MCAblStorage::Static:
        {
            return _Runtime.StaticData() + idPtr->Defn.Info.Data.Offset;
        }

        case MCAblStorage::Eternal:
        {
            return _Runtime.StackItem(idPtr->Defn.Info.Data.Offset);
        }
    }

    return nullptr;
}

auto MCAblDebugger::SimpleValueText(MCAblSymbol* symbol) const -> std::string
{
    MCAblType* typePtr = symbol->TypePtr;

    if (symbol->Defn.Key == MCAblSymbolKind::Const)
    {
        if (typePtr == IntegerTypePtr)
        {
            return std::format("{}", symbol->Defn.Info.Constant.Value.Integer);
        }

        if (typePtr == CharTypePtr)
        {
            return std::string(1, symbol->Defn.Info.Constant.Value.Character);
        }

        return std::format("{:.4f}", symbol->Defn.Info.Constant.Value.Real);
    }

    const MCAblStackItem* valuePtr = VariableItem(symbol);

    if (symbol->Defn.Key == MCAblSymbolKind::RefParam && typePtr->Form != MCAblTypeForm::Array)
    {
        valuePtr = reinterpret_cast<const MCAblStackItem*>(valuePtr->Address);
    }

    if (typePtr->Form == MCAblTypeForm::Array)
    {
        return "ARRAY";
    }

    if (typePtr == IntegerTypePtr || typePtr->Form == MCAblTypeForm::Enum)
    {
        return std::format("{}", valuePtr->Integer);
    }

    if (typePtr == CharTypePtr)
    {
        return std::format("\"{}\"", static_cast<char>(valuePtr->Byte));
    }

    return std::format("{:.4f}", valuePtr->Real);
}

auto MCAblDebugger::ArrayValueText(MCAblSymbol* symbol, std::string_view subscripts) const -> std::optional<std::string>
{
    if (symbol->Defn.Key == MCAblSymbolKind::Const)
    {
        return std::format("\"{}\"", symbol->Defn.Info.Constant.Value.StringPtr);
    }

    MCAblType* typePtr = symbol->TypePtr;
    const char* element = VariableItem(symbol)->Address;

    if (!subscripts.empty())
    {
        // "[i][j]..." (or "[i,j]"): skip the first bracket, then split on commas and closing brackets.
        std::string_view rest = subscripts.substr(1);

        while (!rest.empty())
        {
            const size_t end = rest.find_first_of(",]");
            const std::string_view subscript = rest.substr(0, end);
            rest = end == std::string_view::npos ? std::string_view{} : rest.substr(end + 1);

            // strtok skipped empty fields ("][" between subscripts).
            if (subscript.empty())
            {
                continue;
            }

            const int32_t index = std::atoi(std::string(subscript).c_str());

            if (index < 0 || index >= typePtr->Array.ElementCount)
            {
                return std::nullopt;
            }

            typePtr = typePtr->Array.ElementTypePtr;
            element += typePtr->Size * index;
        }
    }

    if (typePtr->Form == MCAblTypeForm::Array)
    {
        if (typePtr->Array.ElementTypePtr == CharTypePtr)
        {
            return std::format("\"{}\"", element);
        }

        return "Could you be more specific?";
    }

    if (typePtr == IntegerTypePtr || typePtr->Form == MCAblTypeForm::Enum)
    {
        return std::format("{}", *reinterpret_cast<const int32_t*>(element));
    }

    if (typePtr == CharTypePtr)
    {
        return std::format("\"{}\"", *element);
    }

    return std::format("{:.4f}", *reinterpret_cast<const float*>(element));
}

auto MCAblDebugger::ValueText(std::string_view exprString) const -> std::optional<std::string>
{
    const size_t subscripts = exprString.find('[');
    MCAblSymbol* symbol = _DebugModule->FindSymbol(exprString.substr(0, subscripts), _Runtime.CurrentRoutine());

    if (!symbol)
    {
        return std::nullopt;
    }

    if (subscripts == std::string_view::npos && symbol->TypePtr->Form != MCAblTypeForm::Array)
    {
        return SimpleValueText(symbol);
    }

    // A subscript out of range prints nothing.
    return ArrayValueText(symbol,
                          subscripts == std::string_view::npos ? std::string_view{} : exprString.substr(subscripts))
        .value_or(std::string{});
}

auto MCAblDebugger::TraceStatementExecution() -> void
{
    MCAblBreakPointManager* breakPoints = _Module->BreakPoints();

    if (breakPoints && breakPoints->IsBreakPoint(_Runtime.LineNumber()))
    {
        Print(std::format("HIT BP: ({}) {} [{}]", _Module->Id(), _Module->Name(), _Runtime.LineNumber()));
        DebugMode();
        return;
    }

    if (_Step)
    {
        DebugMode();
    }
}

auto MCAblDebugger::TraceRoutineEntry(MCAblSymbol* idPtr) const -> void
{
    if (_TraceEntry)
    {
        Print(std::format("ENTER ({}) {}:{}", _Module->Id(), _Module->Name(), idPtr->Name));
    }
}

auto MCAblDebugger::TraceRoutineExit(MCAblSymbol* idPtr) const -> void
{
    if (_TraceExit)
    {
        Print(std::format("EXIT ({}) {}:{}", _Module->Id(), _Module->Name(), idPtr->Name));
    }
}

auto MCAblDebugger::TraceDataStore(MCAblSymbol* id, MCAblType* idType, const MCAblStackItem* target,
                                   MCAblType* targetType) -> void
{
    MCWatch* watch = id->Watch;

    if (watch && watch->Store)
    {
        Print(std::format("STORE: ({}) {} [{}] -> {}{} = {}\n", _Module->Id(), _Module->Name(), _Runtime.LineNumber(),
                          id->Name, idType->Form == MCAblTypeForm::Array ? "[#]" : "",
                          DataValueText(target, targetType)));

        if (watch->BreakOnStore)
        {
            DebugMode();
        }
    }
}

auto MCAblDebugger::TraceDataFetch(MCAblSymbol* id, MCAblType* idType, const MCAblStackItem* data) -> void
{
    MCWatch* watch = id->Watch;

    if (watch && watch->Fetch)
    {
        Print(std::format("FETCH: ({}) {} [{}] - {}{} = {}\n", _Module->Id(), _Module->Name(), _Runtime.LineNumber(),
                          id->Name, id->TypePtr->Form == MCAblTypeForm::Array ? "[#]" : "",
                          DataValueText(data, idType)));

        if (watch->BreakOnFetch)
        {
            DebugMode();
        }
    }
}

auto MCAblDebugger::DisplayModuleInstanceRegistry() const -> void
{
    const std::span<MCAblModule* const> instances = _Runtime.Instances();

    for (size_t row = 0; row < (instances.size() + 1) / 2; row++)
    {
        const MCAblModule* left = instances[row * 2];
        std::string line = std::format("({:02}) {:<20} ", left->Id(), left->Name());

        if (row * 2 + 1 < instances.size())
        {
            const MCAblModule* right = instances[row * 2 + 1];
            line += std::format("({:02}) {:<20} ", right->Id(), right->Name());
        }

        Print(line);
    }
}

auto MCAblDebugger::ProcessCommand(MCAblDebugCommand command, std::string_view text, int32_t number,
                                   MCAblModule* module) -> void
{
    std::string message;

    switch (command)
    {
        case MCAblDebugCommand::SelectModule:
        {
            Print(" ");

            if (!module)
            {
                DisplayModuleInstanceRegistry();
                message = std::format("CURRENT MODULE: {}", _DebugModule->Name());
            }
            else
            {
                _DebugModule = module;
                message = std::format("SET MODULE: {}", _DebugModule->Name());
            }
            break;
        }

        case MCAblDebugCommand::Trace:
        {
            // Tracing turns stepping off.
            const bool on = number != 0;
            _DebugModule->Trace = on;
            _DebugModule->TraceEntry = on;
            _DebugModule->TraceExit = on;

            if (on)
            {
                _DebugModule->Step = false;
            }

            if (_Module == _DebugModule)
            {
                _Trace = on;
                _TraceEntry = on;
                _TraceExit = on;

                if (on)
                {
                    _Step = false;
                }
            }

            return;
        }

        case MCAblDebugCommand::Step:
        {
            // Stepping turns tracing off.
            const bool on = number != 0;
            _DebugModule->Step = on;

            if (on)
            {
                _DebugModule->Trace = false;
                _DebugModule->TraceEntry = false;
                _DebugModule->TraceExit = false;
            }

            if (_Module == _DebugModule)
            {
                _Step = on;

                if (on)
                {
                    _Trace = false;
                    _TraceEntry = false;
                    _TraceExit = false;
                }
            }

            return;
        }

        case MCAblDebugCommand::AddBreakPoint:
        {
            Print(" ");
            _DebugModule->BreakPoints()->Add(number);
            message = std::format("SET BP: {} ({})", _DebugModule->Name(), number);
            break;
        }

        case MCAblDebugCommand::RemoveBreakPoint:
        {
            Print(" ");
            _DebugModule->BreakPoints()->Remove(number);
            message = std::format("REMOVE BP: {} ({})", _DebugModule->Name(), number);
            break;
        }

        case MCAblDebugCommand::Watch:
        {
            Print(" ");
            MCAblSymbol* idPtr = _DebugModule->FindSymbol(text);

            if (!idPtr)
            {
                Print("Unknown identifier in current scope.\n");
                return;
            }

            MCAblWatchManager* watches = _DebugModule->Watches();
            const bool breakFlag = ((number >> 4) & 1) != 0;

            if ((number & 2) && watches->SetStore(idPtr, true, breakFlag) == 2)
            {
                Print("Reached max watch limit--unable to set watch.\n");
                return;
            }

            if ((number & 8) && watches->SetFetch(idPtr, true, breakFlag) == 2)
            {
                Print("Reached max watch limit--unable to set watch.\n");
                return;
            }

            if (number & 1)
            {
                watches->SetStore(idPtr, false, breakFlag);
            }

            if (number & 4)
            {
                watches->SetFetch(idPtr, false, breakFlag);
            }

            const bool watchesStores = MCAblWatchManager::GetStore(idPtr);
            const bool watchesFetches = MCAblWatchManager::GetFetch(idPtr);

            if (!watchesStores && !watchesFetches)
            {
                message = std::format("REMOVE WATCH: {}.{}", _DebugModule->Name(), text);
            }
            else
            {
                message = std::format("SET WATCH: {}.{} ({}{})", _DebugModule->Name(), text, watchesStores ? "s" : "",
                                      watchesFetches ? "f" : "");
            }
            break;
        }

        case MCAblDebugCommand::ClearWatches:
        {
            _DebugModule->Watches()->RemoveAll();
            return;
        }

        case MCAblDebugCommand::PrintValue:
        {
            Print(" ");
            std::optional<std::string> value = ValueText(text);

            if (!value)
            {
                Print("Unknown identifier in current scope.");
                return;
            }

            message = std::move(*value);
            break;
        }

        case MCAblDebugCommand::Resume:
        {
            _DebugCommand = false;
            return;
        }

        case MCAblDebugCommand::Help:
        {
            Print(" ");
            Print("b{+|-} <line#>         set/remove breakpt");
            Print("m [0 thru warrior #]   set current module (or list them)");
            Print("w[f|s]{+|-}{.} <variable> set/remove variable watch (fetch & store)");
            Print("p <variable>           display current value of variable");
            Print("s{+|-}                 start/stop step mode");
            Print("t{+|-}                 start/stop trace mode");
            Print("??                     current module info");
            Print("?                      help");
            return;
        }

        case MCAblDebugCommand::ModuleInfo:
        {
            Print(" ");
            Print(std::format("CURRENT MODULE: {}", _DebugModule->Name()));
            const MCAblModuleEntry& entry = _DebugModule->Entry();
            const int32_t largestStatic =
                entry.StaticSizes.empty() ? 0 : std::max(0, std::ranges::max(entry.StaticSizes));
            message = std::format("{} static vars, {} bytes, {} largest", entry.StaticSizes.size(),
                                  entry.TotalStaticSize(), largestStatic);
            break;
        }
    }

    Print(message);
}

auto MCAblDebugger::DebugMode() -> void
{
    _DebugModule = _Module;
    Print(StatementText());
    _DebugCommand = true;

    do
    {
        // The game's own loop (aSystem::run) while the debugger window takes the commands.
        PerfStartTime = MCPort::PerformanceCounter();

        if (!MCInput::PumpMessages())
        {
            _DebugCommand = false;
            _Trace = false;
            _Step = false;
            _TraceEntry = false;
            _TraceExit = false;
        }

        if (ApplicationActive)
        {
            UpdateDisplay(TakeScreenShot, 0, 0, 0, 0);
            TakeScreenShot = 0;
            PerfStopTime = MCPort::PerformanceCounter();

            if (MultiPlayer())
            {
                MultiPlayer()->ProcessReceiveList();
            }

            CheckMouse();
        }

        PrevStart = PerfStartTime;
        FrameRate = static_cast<float>(CountsPerSecond) / static_cast<float>(PerfStopTime - PerfStartTime);
    } while (_DebugCommand);
}

auto AblGetDebugger() -> MCAblDebugger*
{
    MCAblRuntime* runtime = AblRuntime();
    return runtime ? runtime->Debugger() : nullptr;
}
