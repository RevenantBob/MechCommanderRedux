#include "stdafx.h"
#include "abl/MCAblRuntime.h"
#include "abl/MCAblCompiler.h"
#include "abl/MCAblDebugger.h"
#include "abl/MCAblSymbolTable.h"
#include "lib/MCFatal.h"
#include "lib/MCFile.h"
#include "main/MCGameContext.h"

namespace
{
    /// <summary><paramref name="text"/> lower-cased.</summary>
    auto LowerCase(std::string_view text) -> std::string
    {
        std::string lower(text);
        std::ranges::transform(lower, lower.begin(),
                               [](char ch) { return static_cast<char>(std::tolower(static_cast<uint8_t>(ch))); });
        return lower;
    }
}

auto MCAblModuleEntry::TotalStaticSize() const -> int32_t
{
    int32_t total = static_cast<int32_t>(StaticSizes.size()) * 4;

    for (const int32_t size : StaticSizes)
    {
        total += size;
    }

    return total;
}

auto MCAblRuntimeFailure::Message() const -> std::string
{
    return std::format("ABL RUNTIME ERROR {} [line {}] - (type {}) {}\n", FileName, LineNumber,
                       static_cast<int32_t>(Error), MCAblRuntimeErrorText(Error));
}

MCAblRuntime::MCAblRuntime(const MCAblOptions& options)
    : _DebugInfo(options.DebugInfo), _Stack(static_cast<size_t>(MaxStackItems))
{
    if (options.Debug)
    {
        _DebugInfo = true;
        _Debugger = std::make_unique<MCAblDebugger>(*this, options.DebuggerPrint);
    }

    if (options.Profile)
    {
        _ProfileLog = std::make_unique<MCFile>();

        if (_ProfileLog->Create("abl.log") != 0)
        {
            Fatal(0, " unable to create ABL ProfileLog ");
        }
    }
}

MCAblRuntime::~MCAblRuntime()
{
    _Libraries.clear();

    if (_ProfileLog)
    {
        _ProfileLog->WriteString(std::format("\nNum Total Lines = {}\n", _ProfileLogLines));
        _ProfileLog->Close();
    }
}

auto MCAblRuntime::CompileAndRegister(std::string_view fileName, MCAblModule* library) -> int32_t
{
    // The registry holds the names lower-cased (the original lower-cased the caller's string in place).
    const std::string lowerName = LowerCase(fileName);

    for (size_t i = 0; i < _Modules.size(); i++)
    {
        if (lowerName == _Modules[i].FileName)
        {
            return static_cast<int32_t>(i);
        }
    }

    auto compiled = MCAblCompiler::Compile(lowerName, {library, _Debugger != nullptr, _DebugInfo});

    if (!compiled)
    {
        // The module's own file won't open: the original returned openSourceFile's code, not a handle.
        if (compiled.error().Code == MCAblSyntaxError::SourceFileOpen && compiled.error().LineNumber == 0)
        {
            return -3;
        }

        // A syntax error ends the game, as in MCX.EXE.
        Fatal(0, compiled.error().Message());
    }

    // The original's registry held the scenario's AblMaxRegisteredModules; the port's grows.
    MCAblModuleEntry& entry = _Modules.emplace_back();
    entry.FileName = lowerName;
    entry.Module = compiled->Module;
    entry.SourceFiles = std::move(compiled->SourceFiles);
    entry.LibrariesUsed = std::move(compiled->LibrariesUsed);
    entry.StaticSizes = std::move(compiled->StaticSizes);
    return static_cast<int32_t>(_Modules.size()) - 1;
}

auto MCAblRuntime::PreProcess(std::string_view sourceFileName) -> int32_t
{
    return CompileAndRegister(sourceFileName, nullptr);
}

auto MCAblRuntime::LoadLibrary(std::string_view sourceFileName) -> int32_t
{
    auto library = std::make_unique<MCAblModule>(MCAblModule::LibraryTag{});
    const int32_t moduleHandle = CompileAndRegister(sourceFileName, library.get());

    // Anything but the module just registered (a library compiled before, or a file that won't open) fails.
    if (moduleHandle < ModuleCount() - 1)
    {
        return -1;
    }

    library->Attach(*this, moduleHandle);
    // The original named it with the caller's string, which the compile had lower-cased in place.
    library->SetName(LowerCase(sourceFileName));
    // The original's library registry held 10; the port's grows.
    _Libraries.push_back(std::move(library));
    return 0;
}

auto MCAblRuntime::InstanceAt(int32_t index) const -> MCAblModule*
{
    if (index > -1 && index < static_cast<int32_t>(_Instances.size()))
    {
        return _Instances[static_cast<size_t>(index)];
    }

    return nullptr;
}

auto MCAblRuntime::Register(MCAblModule* module) -> int32_t
{
    _Instances.push_back(module);
    return _InstanceCount++;
}

auto MCAblRuntime::Unregister(MCAblModule* module) -> void
{
    // The last instance takes the place of the one removed, as in the original.
    const auto found = std::ranges::find(_Instances, module);

    if (found != _Instances.end())
    {
        *found = _Instances.back();
        _Instances.pop_back();
    }
}

auto MCAblRuntime::DeclareEternal(MCAblType* type) -> int32_t
{
    if (_EternalCount >= MaxStackItems)
    {
        Fatal(0, MCAblRuntimeErrorText(MCAblRuntimeError::StackOverflow));
    }

    const int32_t offset = _EternalCount++;
    MCAblStackItem& slot = _Stack[static_cast<size_t>(offset)];
    slot = MCAblStackItem{};

    if (type->Form == MCAblTypeForm::Array)
    {
        slot.Address = AllocateArray(type->Size, "eternal array");
    }

    return offset;
}

auto MCAblRuntime::FileName() const -> std::string_view
{
    if (_FileNumber < 0 || _Module == nullptr)
    {
        return "unavailable";
    }

    return _Module->SourceFile(_FileNumber);
}

auto MCAblRuntime::RuntimeError(MCAblRuntimeError error) -> void
{
    if (_Debugger)
    {
        _Debugger->Print(
            std::format("RUNTIME ERROR:  [{}] {}", static_cast<int32_t>(error), MCAblRuntimeErrorText(error)));
        _Debugger->Print(std::format("MODULE {}", _Module->Name()));
        // Port fix: the original's "unavailable" form passed no argument for its %s.
        _Debugger->Print(_FileNumber < 0 ? std::string("FILE : unavailable") : std::format("FILE {}", FileName()));
        _Debugger->Print(std::format("LINE {}", _LineNumber));
        _Debugger->DebugMode();
    }

    throw MCAblRuntimeFailure{error, std::string(FileName()), _LineNumber};
}

auto MCAblRuntime::BeginModuleExecution(MCAblModule& module, std::span<MCAblParam> params) -> MCAblSymbol*
{
    _Module = &module;

    if (_Debugger)
    {
        _Debugger->SetModule(&module);
    }

    _StaticData = module._StaticData.empty() ? nullptr : module._StaticData.data();
    _Routine = nullptr;
    MCAblSymbol* moduleIdPtr = _Modules[static_cast<size_t>(module._Handle)].Module;
    _ExecutionCount++;
    _FileNumber = -1;
    // The item above the eternals stays unused: the frame starts one higher.
    _Tos = _Stack.data() + _EternalCount;
    _Frame = _Tos + 1;
    _StatementCount = 0;
    _Level = 1;
    _CallDepth = 0;

    PushInteger(0);
    PushAddress(nullptr);
    PushAddress(nullptr);
    PushAddress(nullptr);

    // A formal past the end of the list reads an empty parameter (the original's list had one zeroed spare).
    static MCAblParam missingParam;
    size_t index = 0;

    for (MCAblSymbol* formalIdPtr = moduleIdPtr->Defn.Info.Routine.Params; formalIdPtr;
         formalIdPtr = formalIdPtr->Next, index++)
    {
        if (params.empty())
        {
            break;
        }

        missingParam = MCAblParam{};
        MCAblParam& param = index < params.size() ? params[index] : missingParam;
        MCAblType* formalTypePtr = formalIdPtr->TypePtr;

        if (formalIdPtr->Defn.Key == MCAblSymbolKind::ValueParam)
        {
            if (formalTypePtr == RealTypePtr)
            {
                if (param.Type == MCAblParamType::Integer)
                {
                    PushReal(static_cast<float>(param.Integer));
                }
                else if (param.Type == MCAblParamType::Real)
                {
                    PushReal(param.Real);
                }
            }
            else if (formalTypePtr == IntegerTypePtr)
            {
                if (param.Type != MCAblParamType::Integer)
                {
                    return nullptr;
                }

                PushInteger(param.Integer);
            }

            // Faithful: nothing was pushed for an array parameter, so this copies the block the top item points to.
            if (formalTypePtr->Form == MCAblTypeForm::Array)
            {
                const MCAddress source = _Tos->Address;
                const MCAddress copy = AllocateArray(formalTypePtr->Size, "array parameter");
                _Tos->Address = copy;
                std::memcpy(copy, source, static_cast<size_t>(formalTypePtr->Size));
            }
        }
        else
        {
            // A reference parameter points into the list, so the module can write back.
            if (formalTypePtr == RealTypePtr)
            {
                PushAddress(reinterpret_cast<MCAddress>(&param.Real));
            }
            else if (formalTypePtr == IntegerTypePtr)
            {
                PushAddress(reinterpret_cast<MCAddress>(&param.Integer));
            }
            else
            {
                return nullptr;
            }
        }
    }

    return moduleIdPtr;
}

auto MCAblRuntime::Run(MCAblModule& module, std::span<MCAblParam> params, MCAblSymbol* function)
    -> std::expected<int32_t, MCAblRuntimeFailure>
{
    try
    {
        MCAblSymbol* moduleIdPtr = BeginModuleExecution(module, params);

        if (!moduleIdPtr)
        {
            return 0;
        }

        _ModuleHandle = module._Handle;
        _CallModuleInit = !module._InitCalled;
        module._InitCalled = true;

        if (function)
        {
            ExecuteChild(moduleIdPtr, function);
        }
        else
        {
            Execute(moduleIdPtr);
        }

        module._ReturnValue = _ReturnValue.Integer;
        return _StatementCount;
    }
    catch (const MCAblRuntimeFailure& failure)
    {
        return std::unexpected(failure);
    }
}

auto MCAblRuntime::PushItem() -> MCAblStackItem*
{
    MCAblStackItem* item = ++_Tos;

    if (item >= _Stack.data() + MaxStackItems)
    {
        RuntimeError(MCAblRuntimeError::StackOverflow);
    }

    // The original stored only the value's own bytes; the whole 8-byte slot is cleared first.
    *item = MCAblStackItem{};
    return item;
}

auto MCAblRuntime::PushInteger(int32_t value) -> void
{
    PushItem()->Integer = value;
}

auto MCAblRuntime::PushReal(float value) -> void
{
    PushItem()->Real = value;
}

auto MCAblRuntime::PushByte(char value) -> void
{
    PushItem()->Byte = static_cast<uint8_t>(value);
}

auto MCAblRuntime::PushAddress(MCAddress address) -> void
{
    PushItem()->Address = address;
}

auto MCAblRuntime::GetCodeSymbol() -> MCAblSymbol*
{
    MCAblSymbol* nodePtr;
    std::memcpy(static_cast<void*>(&nodePtr), _Code, AblCodeSymbolSize);
    _Code += AblCodeSymbolSize;
    return nodePtr;
}

auto MCAblRuntime::GetCodeStatementMarker() -> int32_t
{
    int32_t line = -1;

    if (_Token == MCAblToken::StatementMarker && _DebugInfo)
    {
        _FileNumber = static_cast<uint8_t>(*_Code);
        std::memcpy(&line, _Code + 1, sizeof(line));
        _Code += AblCodeStatementMarkerSize;
    }

    return line;
}

auto MCAblRuntime::GetCodeAddressMarker() -> char*
{
    char* address = nullptr;

    if (_Token == MCAblToken::AddressMarker)
    {
        int32_t offset;
        std::memcpy(&offset, _Code, AblCodeAddressSize);
        address = _Code + offset - 1;
        _Code += AblCodeAddressSize;
    }

    return address;
}

auto MCAblRuntime::GetCodeInteger() -> int32_t
{
    int32_t value;
    std::memcpy(&value, _Code, AblCodeIntegerSize);
    _Code += AblCodeIntegerSize;
    return value;
}

auto MCAblRuntime::GetCodeAddress() -> char*
{
    int32_t offset;
    std::memcpy(&offset, _Code, AblCodeIntegerSize);
    char* address = _Code + offset - 1;
    _Code += AblCodeIntegerSize;
    return address;
}

auto MCAblRuntime::NextInteger() -> int32_t
{
    GetCodeToken();
    ExecExpression();
    const int32_t value = _Tos->Integer;
    Pop();
    return value;
}

auto MCAblRuntime::NextReal() -> float
{
    GetCodeToken();
    ExecExpression();
    const float value = _Tos->Real;
    Pop();
    return value;
}

auto MCAblRuntime::NextAddress() -> MCAddress
{
    GetCodeToken();
    ExecExpression();
    const MCAddress value = _Tos->Address;
    Pop();
    return value;
}

auto MCAblRuntime::NextReference() -> MCAddress
{
    MCAblSymbol* idPtr = GetCodeSymbol();
    ExecVariable(idPtr, MCAblUse::RefParam);
    return _Tos->Address;
}

auto MCAblRuntime::PushStackFrameHeader(int32_t oldLevel, int32_t newLevel) -> void
{
    auto* headerPtr = reinterpret_cast<MCAblStackFrameHeader*>(_Frame);
    // Function value.
    PushInteger(0);
    // Static link: none for a routine of another module; the caller's frame for a routine nested in it; the
    // caller's own static link for a routine at the caller's level.
    MCAblStackItem* staticLink = nullptr;

    if (newLevel == -1)
    {
        staticLink = nullptr;
    }
    else if (newLevel == oldLevel + 1)
    {
        staticLink = _Frame;
    }
    else if (newLevel == oldLevel)
    {
        staticLink = reinterpret_cast<MCAblStackItem*>(headerPtr->StaticLink.Address);
    }
    else
    {
        RuntimeError(MCAblRuntimeError::NestedFunctionCall);
    }

    PushAddress(reinterpret_cast<MCAddress>(staticLink));
    // Dynamic link.
    PushAddress(reinterpret_cast<MCAddress>(_Frame));
    // Return address (set by the caller).
    PushAddress(nullptr);
}

auto MCAblRuntime::CurrentRoutineFrame() const -> MCAblStackItem*
{
    MCAblStackItem* framePtr = _Frame;

    for (int32_t delta = _Level - _Routine->Level - 1; delta > 0; delta--)
    {
        framePtr =
            reinterpret_cast<MCAblStackItem*>(reinterpret_cast<MCAblStackFrameHeader*>(framePtr)->StaticLink.Address);
    }

    return framePtr;
}

auto MCAblRuntime::AllocateArray(int32_t size, std::string_view what) -> MCAddress
{
    auto* block = static_cast<MCAddress>(_ArrayBlocks.Allocate(static_cast<size_t>(size)));

    // An empty array got no block from the original's heap, which was fatal.
    if (!block)
    {
        Fatal(0, std::format(" ABL: Unable to AblStackHeap->malloc {} ", what));
    }

    return block;
}

auto MCAblRuntime::AllocLocal(MCAblType* typePtr) -> void
{
    if (typePtr == IntegerTypePtr)
    {
        PushInteger(0);
    }
    else if (typePtr == RealTypePtr)
    {
        PushReal(0.0f);
    }
    else if (typePtr == BooleanTypePtr || typePtr == CharTypePtr)
    {
        PushByte(0);
    }
    else if (typePtr->Form == MCAblTypeForm::Enum)
    {
        PushInteger(0);
    }
    else if (typePtr->Form == MCAblTypeForm::Array)
    {
        PushAddress(AllocateArray(typePtr->Size, "local array"));
    }
}

auto MCAblRuntime::FreeLocal(MCAblSymbol* idPtr) -> void
{
    // Only local arrays own memory; a reference parameter's array belongs to the caller.
    if (idPtr->TypePtr->Form == MCAblTypeForm::Array && idPtr->Defn.Key != MCAblSymbolKind::RefParam)
    {
        MCAblStackItem* dataPtr = _Frame + idPtr->Defn.Info.Data.Offset;

        if (idPtr->Defn.Info.Data.VarType != MCAblStorage::Normal || !dataPtr)
        {
            RuntimeError(MCAblRuntimeError::StackOverflow);
        }

        _ArrayBlocks.Free(dataPtr->Address);
    }
}

auto MCAblRuntime::RoutineEntry(MCAblSymbol* routineIdPtr) -> void
{
    if (_Debugger)
    {
        _Debugger->TraceRoutineEntry(routineIdPtr);
    }

    _Code = routineIdPtr->Defn.Info.Routine.CodeSegment;
    _ReturnValue = MCAblStackItem{};

    // Static and eternal locals live elsewhere.
    for (MCAblSymbol* varIdPtr = routineIdPtr->Defn.Info.Routine.Locals; varIdPtr; varIdPtr = varIdPtr->Next)
    {
        if (varIdPtr->Defn.Info.Data.VarType == MCAblStorage::Normal)
        {
            AllocLocal(varIdPtr->TypePtr);
        }
    }
}

auto MCAblRuntime::RoutineExit(MCAblSymbol* routineIdPtr) -> void
{
    if (_Debugger)
    {
        _Debugger->TraceRoutineExit(routineIdPtr);
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

    auto* headerPtr = reinterpret_cast<MCAblStackFrameHeader*>(_Frame);
    _Code = headerPtr->ReturnAddress.Address;

    // A function leaves its value (the frame's first item) on the stack.
    _Tos = routineIdPtr->TypePtr ? _Frame : _Frame - 1;
    _Frame = reinterpret_cast<MCAblStackItem*>(headerPtr->DynamicLink.Address);
}

auto MCAblRuntime::CallModuleInit(MCAblSymbol* moduleIdPtr) -> MCAblSymbol*
{
    if (!_CallModuleInit)
    {
        return nullptr;
    }

    _CallModuleInit = false;
    MCAblSymbol* initIdPtr = SearchSymTable("init", moduleIdPtr->Defn.Info.Routine.LocalSymTable);

    if (initIdPtr)
    {
        ExecRoutineCall(initIdPtr);
        // The call reads the token after it; back up to it.
        _Code--;
    }

    return initIdPtr;
}

auto MCAblRuntime::Execute(MCAblSymbol* routineIdPtr) -> void
{
    MCAblSymbol* thisRoutineIdPtr = _Routine;
    _Routine = routineIdPtr;
    RoutineEntry(routineIdPtr);
    CallModuleInit(_Modules[static_cast<size_t>(_Module->_Handle)].Module);
    GetCodeToken();
    ExecStatement();
    _ExitWithReturn = false;
    RoutineExit(routineIdPtr);
    _Routine = thisRoutineIdPtr;
}

auto MCAblRuntime::ExecuteChild(MCAblSymbol* moduleIdPtr, MCAblSymbol* childRoutineIdPtr) -> void
{
    MCAblSymbol* thisRoutineIdPtr = _Routine;
    _Routine = moduleIdPtr;
    RoutineEntry(moduleIdPtr);

    // When the child is init itself, it has just run.
    if (CallModuleInit(moduleIdPtr) != childRoutineIdPtr)
    {
        ExecRoutineCall(childRoutineIdPtr);
        _Code--;
    }

    _ExitWithReturn = false;
    RoutineExit(moduleIdPtr);
    _Routine = thisRoutineIdPtr;
}

auto MCAblRuntime::LogCall(MCAblSymbol* routineIdPtr, int32_t runTime) -> void
{
    // The original buffered 256 lines and cut each to 127 characters; the port writes each line whole.
    _ProfileLog->WriteString(std::format("[{:08}] {}{} ({})\n", _ExecutionCount,
                                         std::string(static_cast<size_t>(std::max(_CallDepth, 0)), ' '),
                                         routineIdPtr->Name, runTime));
    _ProfileLogLines++;
}

MCAblBrainScope::MCAblBrainScope(MCMoverGroup* group, MCGameObject* object, int32_t objectClass, MCMechWarrior* warrior)
    : _Runtime(AblRuntime())
{
    if (_Runtime)
    {
        MCAblBrainContext& brain = _Runtime->Brain;
        brain.IsUnitOrder = false;
        brain.Group = group;
        brain.Object = object;
        brain.ObjectClass = objectClass;
        brain.Contact = nullptr;
        brain.Warrior = warrior;
    }
}

MCAblBrainScope::~MCAblBrainScope()
{
    if (_Runtime)
    {
        MCAblBrainContext& brain = _Runtime->Brain;
        brain.IsUnitOrder = false;
        brain.Group = nullptr;
        brain.Object = nullptr;
        brain.ObjectClass = 0;
        brain.Warrior = nullptr;
        brain.Contact = nullptr;
    }
}

auto AblRuntime() -> MCAblRuntime*
{
    return MCGameContext::Current().AblRuntime();
}

auto AblInit(const MCAblOptions& options) -> void
{
    // The original's heap sizes, stack size, code buffer size, module and static variable limits are gone.
    MCGameContext& context = MCGameContext::Current();
    context.SetAblSymbols(std::make_unique<MCAblSymbolTable>());
    context.SetAblRuntime(std::make_unique<MCAblRuntime>(options));
}

auto AblClose() -> void
{
    MCGameContext& context = MCGameContext::Current();
    context.SetAblRuntime(nullptr);
    context.SetAblSymbols(nullptr);
}

auto AblEnabled() -> bool
{
    return AblRuntime() != nullptr;
}

auto AblPreProcess(std::string_view sourceFileName) -> int32_t
{
    return AblRuntime()->PreProcess(sourceFileName);
}

auto AblLoadLibrary(std::string_view sourceFileName) -> int32_t
{
    return AblRuntime()->LoadLibrary(sourceFileName);
}
