#include "stdafx.h"
#include "abl/MCAblDebugger.h"
#include "abl/MCAblRuntime.h"
#include "abl/MCAblSymbolTable.h"
#include "gui/MCGuiInput.h"
#include "gui/MCGuiSystem.h"
#include "gui/MCUpdateDisplay.h"
#include "network/MCMultiPlayer.h"
#include "platform/MCInput.h"

auto MCAblWatchManager::Add(MCAblSymbol& idPtr) -> std::shared_ptr<MCWatch>
{
    const MCAblSymbolKind idDefn = idPtr.Defn.Key;

    if (idDefn != MCAblSymbolKind::Const && idDefn != MCAblSymbolKind::Var && idDefn != MCAblSymbolKind::ValueParam &&
        idDefn != MCAblSymbolKind::RefParam)
    {
        return nullptr;
    }

    if (std::shared_ptr<MCWatch> watch = idPtr.Watch.lock())
    {
        return watch;
    }

    auto watch = std::make_shared<MCWatch>();
    watch->Symbol = idPtr.Id;
    idPtr.Watch = watch;
    _Watches.push_back(watch);
    return watch;
}

auto MCAblWatchManager::Remove(MCAblSymbol& idPtr) -> int32_t
{
    const std::shared_ptr<MCWatch> watch = idPtr.Watch.lock();

    if (!watch)
    {
        return 2;
    }

    std::erase(_Watches, watch);
    idPtr.Watch.reset();
    return 0;
}

auto MCAblWatchManager::RemoveAll() -> int32_t
{
    // The symbols' references expire with the watches.
    const auto removed = static_cast<int32_t>(_Watches.size());
    _Watches.clear();
    return removed;
}

auto MCAblWatchManager::SetStore(MCAblSymbol& idPtr, bool on, bool breakOnStore) -> int32_t
{
    std::shared_ptr<MCWatch> watch = idPtr.Watch.lock();

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

auto MCAblWatchManager::SetFetch(MCAblSymbol& idPtr, bool on, bool breakOnFetch) -> int32_t
{
    std::shared_ptr<MCWatch> watch = idPtr.Watch.lock();

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

auto MCAblWatchManager::GetStore(const MCAblSymbol& idPtr) -> bool
{
    const std::shared_ptr<MCWatch> watch = idPtr.Watch.lock();
    return watch && watch->Store;
}

auto MCAblWatchManager::GetFetch(const MCAblSymbol& idPtr) -> bool
{
    const std::shared_ptr<MCWatch> watch = idPtr.Watch.lock();
    return watch && watch->Fetch;
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

MCAblDebugger::MCAblDebugger(std::weak_ptr<MCAblRuntime> runtime, std::function<void(std::string_view)> print)
    : _Runtime(std::move(runtime)), _Print(std::move(print))
{
}

auto MCAblDebugger::Runtime() const -> MCAblRuntime&
{
    // The runtime owns the debugger, so it is there whenever the debugger runs.
    return *_Runtime.lock();
}

auto MCAblDebugger::Print(std::string_view text) const -> void
{
    if (_Print)
    {
        _Print(text);
    }
}

auto MCAblDebugger::SetModule(MCAblModule& ablModule) -> void
{
    _Module = ablModule.weak_from_this();
    // Faithful: the module's trace flag sets all three trace modes (its TraceEntry/TraceExit are not read).
    _Step = ablModule.Step;
    _TraceExit = ablModule.Trace;
    _TraceEntry = ablModule.Trace;
    _Trace = ablModule.Trace;
}

auto MCAblDebugger::StatementText() const -> std::string
{
    MCAblRuntime& runtime = Runtime();
    const std::shared_ptr<const std::vector<char>> segment = runtime.CodeSegment();
    std::string text;

    if (!segment)
    {
        return text;
    }

    const std::vector<char>& code = *segment;
    auto position = static_cast<size_t>(runtime.StatementStart());
    bool done = false;

    do
    {
        const auto token = static_cast<MCAblToken>(code[position]);
        size_t next = position + 1;

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
                MCAblSymbolId id = MCAblSymbolId::None;
                std::memcpy(&id, &code[next], AblCodeSymbolSize);
                text += ' ';
                text += runtime.Symbol(id).Name;
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

        position = next;
    } while (!done);

    return text;
}

auto MCAblDebugger::DataValueText(MCAblAddress data, MCAblTypeId dataType) const -> std::string
{
    MCAblRuntime& runtime = Runtime();

    if (dataType != MCAblTypeId::Boolean && runtime.Type(dataType).Form == MCAblTypeForm::Enum)
    {
        dataType = MCAblTypeId::Integer;
    }

    if (dataType == MCAblTypeId::Integer)
    {
        return std::format("{}", runtime.Load<int32_t>(data));
    }

    if (dataType == MCAblTypeId::Real)
    {
        return std::format("{:.6f}", runtime.Load<float>(data));
    }

    if (dataType == MCAblTypeId::Boolean)
    {
        return runtime.Load<int32_t>(data) == 1 ? "true" : "false";
    }

    if (dataType == MCAblTypeId::Char)
    {
        return std::string(1, static_cast<char>(runtime.Load<uint8_t>(data)));
    }

    if (const MCAblType& type = runtime.Type(dataType); type.Form == MCAblTypeForm::Array)
    {
        return type.Array.ElementType == MCAblTypeId::Char ? "CHAR ARRAY" : "ARRAY";
    }

    return {};
}

auto MCAblDebugger::VariableItem(const MCAblSymbol& idPtr) const -> MCAblAddress
{
    MCAblRuntime& runtime = Runtime();

    switch (idPtr.Defn.Info.Data.VarType)
    {
        case MCAblStorage::Normal:
        {
            int32_t frame = runtime.Frame();

            for (int32_t delta = runtime.Level() - idPtr.Level; delta > 0; delta--)
            {
                frame = runtime.StaticLink(frame);
            }

            return MCAblAddress::StackItem(frame + idPtr.Defn.Info.Data.Offset);
        }

        case MCAblStorage::Static:
        {
            return runtime.StaticItem(idPtr.Defn.Info.Data.Offset);
        }

        case MCAblStorage::Eternal:
        {
            return MCAblAddress::StackItem(idPtr.Defn.Info.Data.Offset);
        }
    }

    return {};
}

auto MCAblDebugger::SimpleValueText(const MCAblSymbol& symbol) const -> std::string
{
    MCAblRuntime& runtime = Runtime();
    const MCAblTypeId typePtr = symbol.Type;

    if (symbol.Defn.Key == MCAblSymbolKind::Const)
    {
        if (typePtr == MCAblTypeId::Integer)
        {
            return std::format("{}", symbol.Defn.Info.Constant.Value.Integer);
        }

        if (typePtr == MCAblTypeId::Char)
        {
            return std::string(1, symbol.Defn.Info.Constant.Value.Character);
        }

        return std::format("{:.4f}", symbol.Defn.Info.Constant.Value.Real);
    }

    MCAblAddress valuePtr = VariableItem(symbol);
    const MCAblType& type = runtime.Type(typePtr);

    if (symbol.Defn.Key == MCAblSymbolKind::RefParam && type.Form != MCAblTypeForm::Array)
    {
        valuePtr = runtime.Load<MCAblAddress>(valuePtr);
    }

    if (type.Form == MCAblTypeForm::Array)
    {
        return "ARRAY";
    }

    if (typePtr == MCAblTypeId::Integer || type.Form == MCAblTypeForm::Enum)
    {
        return std::format("{}", runtime.Load<int32_t>(valuePtr));
    }

    if (typePtr == MCAblTypeId::Char)
    {
        return std::format("\"{}\"", static_cast<char>(runtime.Load<uint8_t>(valuePtr)));
    }

    return std::format("{:.4f}", runtime.Load<float>(valuePtr));
}

auto MCAblDebugger::ArrayValueText(const MCAblSymbol& symbol, std::string_view subscripts) const
    -> std::optional<std::string>
{
    MCAblRuntime& runtime = Runtime();

    if (symbol.Defn.Key == MCAblSymbolKind::Const)
    {
        return std::format("\"{}\"", runtime.Symbol(symbol.Defn.Info.Constant.Value.Text).LiteralText);
    }

    MCAblTypeId typePtr = symbol.Type;
    MCAblAddress element = runtime.Load<MCAblAddress>(VariableItem(symbol));

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

            if (index < 0 || index >= runtime.Type(typePtr).Array.ElementCount)
            {
                return std::nullopt;
            }

            typePtr = runtime.Type(typePtr).Array.ElementType;
            element = element + runtime.Type(typePtr).Size * index;
        }
    }

    const MCAblType& type = runtime.Type(typePtr);

    if (type.Form == MCAblTypeForm::Array)
    {
        if (type.Array.ElementType == MCAblTypeId::Char)
        {
            return std::format("\"{}\"", runtime.Text(element));
        }

        return "Could you be more specific?";
    }

    if (typePtr == MCAblTypeId::Integer || type.Form == MCAblTypeForm::Enum)
    {
        return std::format("{}", runtime.Load<int32_t>(element));
    }

    if (typePtr == MCAblTypeId::Char)
    {
        return std::format("\"{}\"", static_cast<char>(runtime.Load<uint8_t>(element)));
    }

    return std::format("{:.4f}", runtime.Load<float>(element));
}

auto MCAblDebugger::ValueText(std::string_view exprString) const -> std::optional<std::string>
{
    MCAblRuntime& runtime = Runtime();
    const size_t subscripts = exprString.find('[');
    const MCAblSymbolId found = DebugModule()->FindSymbol(exprString.substr(0, subscripts), runtime.CurrentRoutine());

    if (found == MCAblSymbolId::None)
    {
        return std::nullopt;
    }

    const MCAblSymbol& symbol = runtime.Symbol(found);

    if (subscripts == std::string_view::npos && runtime.Type(symbol.Type).Form != MCAblTypeForm::Array)
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
    const std::shared_ptr<MCAblModule> module = Module();
    const std::shared_ptr<MCAblBreakPointManager> breakPoints = module->BreakPoints();
    MCAblRuntime& runtime = Runtime();

    if (breakPoints && breakPoints->IsBreakPoint(runtime.LineNumber()))
    {
        Print(std::format("HIT BP: ({}) {} [{}]", module->Id(), module->Name(), runtime.LineNumber()));
        DebugMode();
        return;
    }

    if (_Step)
    {
        DebugMode();
    }
}

auto MCAblDebugger::TraceRoutineEntry(const MCAblSymbol& idPtr) const -> void
{
    if (_TraceEntry)
    {
        const std::shared_ptr<MCAblModule> module = Module();
        Print(std::format("ENTER ({}) {}:{}", module->Id(), module->Name(), idPtr.Name));
    }
}

auto MCAblDebugger::TraceRoutineExit(const MCAblSymbol& idPtr) const -> void
{
    if (_TraceExit)
    {
        const std::shared_ptr<MCAblModule> module = Module();
        Print(std::format("EXIT ({}) {}:{}", module->Id(), module->Name(), idPtr.Name));
    }
}

auto MCAblDebugger::TraceDataStore(const MCAblSymbol& id, MCAblTypeId idType, MCAblAddress target,
                                   MCAblTypeId targetType) -> void
{
    const std::shared_ptr<MCWatch> watch = id.Watch.lock();

    if (watch && watch->Store)
    {
        const std::shared_ptr<MCAblModule> module = Module();
        MCAblRuntime& runtime = Runtime();
        Print(std::format("STORE: ({}) {} [{}] -> {}{} = {}\n", module->Id(), module->Name(), runtime.LineNumber(),
                          id.Name, runtime.Type(idType).Form == MCAblTypeForm::Array ? "[#]" : "",
                          DataValueText(target, targetType)));

        if (watch->BreakOnStore)
        {
            DebugMode();
        }
    }
}

auto MCAblDebugger::TraceDataFetch(const MCAblSymbol& id, MCAblTypeId idType, MCAblAddress data) -> void
{
    const std::shared_ptr<MCWatch> watch = id.Watch.lock();

    if (watch && watch->Fetch)
    {
        const std::shared_ptr<MCAblModule> module = Module();
        MCAblRuntime& runtime = Runtime();
        Print(std::format("FETCH: ({}) {} [{}] - {}{} = {}\n", module->Id(), module->Name(), runtime.LineNumber(),
                          id.Name, runtime.Type(id.Type).Form == MCAblTypeForm::Array ? "[#]" : "",
                          DataValueText(data, idType)));

        if (watch->BreakOnFetch)
        {
            DebugMode();
        }
    }
}

auto MCAblDebugger::DisplayModuleInstanceRegistry() const -> void
{
    const std::vector<std::shared_ptr<MCAblModule>> instances = Runtime().Instances();

    for (size_t row = 0; row < (instances.size() + 1) / 2; row++)
    {
        const MCAblModule& left = *instances[row * 2];
        std::string line = std::format("({:02}) {:<20} ", left.Id(), left.Name());

        if (row * 2 + 1 < instances.size())
        {
            const MCAblModule& right = *instances[row * 2 + 1];
            line += std::format("({:02}) {:<20} ", right.Id(), right.Name());
        }

        Print(line);
    }
}

auto MCAblDebugger::ProcessCommand(MCAblDebugCommand command, std::string_view text, int32_t number,
                                   std::shared_ptr<MCAblModule> module) -> void
{
    std::string message;
    std::shared_ptr<MCAblModule> debugModule = DebugModule();

    switch (command)
    {
        case MCAblDebugCommand::SelectModule:
        {
            Print(" ");

            if (!module)
            {
                DisplayModuleInstanceRegistry();
                message = std::format("CURRENT MODULE: {}", debugModule->Name());
            }
            else
            {
                _DebugModule = module;
                message = std::format("SET MODULE: {}", module->Name());
            }
            break;
        }

        case MCAblDebugCommand::Trace:
        {
            // Tracing turns stepping off.
            const bool on = number != 0;
            debugModule->Trace = on;
            debugModule->TraceEntry = on;
            debugModule->TraceExit = on;

            if (on)
            {
                debugModule->Step = false;
            }

            if (Module() == debugModule)
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
            debugModule->Step = on;

            if (on)
            {
                debugModule->Trace = false;
                debugModule->TraceEntry = false;
                debugModule->TraceExit = false;
            }

            if (Module() == debugModule)
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
            debugModule->BreakPoints()->Add(number);
            message = std::format("SET BP: {} ({})", debugModule->Name(), number);
            break;
        }

        case MCAblDebugCommand::RemoveBreakPoint:
        {
            Print(" ");
            debugModule->BreakPoints()->Remove(number);
            message = std::format("REMOVE BP: {} ({})", debugModule->Name(), number);
            break;
        }

        case MCAblDebugCommand::Watch:
        {
            Print(" ");
            const MCAblSymbolId found = debugModule->FindSymbol(text);

            if (found == MCAblSymbolId::None)
            {
                Print("Unknown identifier in current scope.\n");
                return;
            }

            MCAblSymbol& idPtr = Runtime().Symbol(found);
            const std::shared_ptr<MCAblWatchManager> watches = debugModule->Watches();
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
                message = std::format("REMOVE WATCH: {}.{}", debugModule->Name(), text);
            }
            else
            {
                message = std::format("SET WATCH: {}.{} ({}{})", debugModule->Name(), text, watchesStores ? "s" : "",
                                      watchesFetches ? "f" : "");
            }
            break;
        }

        case MCAblDebugCommand::ClearWatches:
        {
            debugModule->Watches()->RemoveAll();
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
            Print(std::format("CURRENT MODULE: {}", debugModule->Name()));
            const MCAblModuleEntry& entry = debugModule->Entry();
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
            UpdateDisplay(TakeScreenShot, false, 0, false, 0);
            TakeScreenShot = false;
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

auto AblGetDebugger() -> std::shared_ptr<MCAblDebugger>
{
    const std::shared_ptr<MCAblRuntime> runtime = AblRuntime();
    return runtime ? runtime->Debugger() : nullptr;
}
