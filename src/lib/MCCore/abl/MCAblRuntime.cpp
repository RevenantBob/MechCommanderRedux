#include "stdafx.h"
#include "abl/MCAblRuntime.h"
#include "abl/MCAblCompiler.h"
#include "abl/MCAblDebugger.h"
#include "abl/MCAblSymbolTable.h"
#include "lib/MCFatal.h"
#include "lib/MCFile.h"
#include "main/MCGameContext.h"
#include "object/MCBaseObject.h"
#include "object/MCGameObject.h"
#include "object/MCMechWarrior.h"
#include "object/MCMoverGroup.h"

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

    /// <summary>Bytes of a stack item.</summary>
    constexpr auto ItemSize = static_cast<int32_t>(sizeof(MCAblStackItem));

    /// <summary>Where module parameter <paramref name="index"/> is in a run's parameter block: Integer, then Real.</summary>
    constexpr int32_t ParamOffset(size_t index)
    {
        return static_cast<int32_t>(index) * 8;
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

auto MCAblBrainContext::Group() const -> std::shared_ptr<MCMoverGroup>
{
    return _Group.lock();
}

auto MCAblBrainContext::Object() const -> std::shared_ptr<MCGameObject>
{
    return LockObject(_Object);
}

auto MCAblBrainContext::Warrior() const -> std::shared_ptr<MCMechWarrior>
{
    return _Warrior.lock();
}

auto MCAblBrainContext::Contact() const -> std::shared_ptr<MCGameObject>
{
    return LockObject(_Contact);
}

auto MCAblRuntimeFailure::Message() const -> std::string
{
    return std::format("ABL RUNTIME ERROR {} [line {}] - (type {}) {}\n", FileName, LineNumber,
                       static_cast<int32_t>(Error), MCAblRuntimeErrorText(Error));
}

auto MCAblRuntime::Create(std::shared_ptr<MCAblSymbolTable> symbols, const MCAblOptions& options)
    -> std::shared_ptr<MCAblRuntime>
{
    auto runtime = std::make_shared<MCAblRuntime>(Key{}, std::move(symbols), options);

    // The debugger refers back to its runtime.
    if (options.Debug)
    {
        runtime->_Debugger = std::make_shared<MCAblDebugger>(runtime->weak_from_this(), options.DebuggerPrint);
    }

    return runtime;
}

MCAblRuntime::MCAblRuntime(Key, std::shared_ptr<MCAblSymbolTable> symbols, const MCAblOptions& options)
    : _Symbols(std::move(symbols))
    , _DebugInfo(options.DebugInfo || options.Debug)
    , _Stack(static_cast<size_t>(MaxStackItems))
{
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

auto MCAblRuntime::CompileAndRegister(std::string_view fileName, MCAblLibraryId library) -> int32_t
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
    return CompileAndRegister(sourceFileName, MCAblLibraryId::None);
}

auto MCAblRuntime::LoadLibrary(std::string_view sourceFileName) -> int32_t
{
    // The library's symbols record the place it takes among the libraries.
    const auto libraryId = static_cast<MCAblLibraryId>(_Libraries.size() + 1);
    const int32_t moduleHandle = CompileAndRegister(sourceFileName, libraryId);

    // Anything but the module just registered (a library compiled before, or a file that won't open) fails.
    if (moduleHandle < ModuleCount() - 1)
    {
        return -1;
    }

    auto library = std::make_shared<MCAblModule>(MCAblModule::Key{});
    library->_Library = libraryId;
    library->Attach(*this, moduleHandle);
    // The original named it with the caller's string, which the compile had lower-cased in place.
    library->SetName(LowerCase(sourceFileName));
    // The original's library registry held 10; the port's grows.
    _Libraries.push_back(std::move(library));
    return 0;
}

auto MCAblRuntime::Instances() const -> std::vector<std::shared_ptr<MCAblModule>>
{
    std::vector<std::shared_ptr<MCAblModule>> instances;
    instances.reserve(_Instances.size());

    for (const InstanceSlot& slot : _Instances)
    {
        if (std::shared_ptr<MCAblModule> module = slot.Module.lock())
        {
            instances.push_back(std::move(module));
        }
    }

    return instances;
}

auto MCAblRuntime::InstanceAt(int32_t index) const -> std::shared_ptr<MCAblModule>
{
    if (index > -1 && index < static_cast<int32_t>(_Instances.size()))
    {
        return _Instances[static_cast<size_t>(index)].Module.lock();
    }

    return nullptr;
}

auto MCAblRuntime::Register(MCAblModule& module) -> int32_t
{
    _Instances.push_back({_InstanceCount, module.weak_from_this()});
    return _InstanceCount++;
}

auto MCAblRuntime::Unregister(int32_t id) -> void
{
    // The last instance takes the place of the one removed, as in the original.
    const auto found = std::ranges::find(_Instances, id, &InstanceSlot::Id);

    if (found != _Instances.end())
    {
        *found = std::move(_Instances.back());
        _Instances.pop_back();
    }
}

auto MCAblRuntime::DeclareEternal(const MCAblType& type) -> int32_t
{
    if (_EternalCount >= MaxStackItems)
    {
        Fatal(0, MCAblRuntimeErrorText(MCAblRuntimeError::StackOverflow));
    }

    const int32_t offset = _EternalCount++;
    MCAblStackItem& slot = _Stack[static_cast<size_t>(offset)];
    slot = MCAblStackItem{};

    if (type.Form == MCAblTypeForm::Array)
    {
        slot.Address = AllocateArray(type.Size, "eternal array");
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

// ---- ABL memory ----------------------------------------------------------------------------------------------------

auto MCAblRuntime::AllocateBlock(int32_t size, std::string_view what, bool array) -> MCAblAddress
{
    // An empty block came back null from the original's heap, which was fatal. So is one past the script heap's 16 MB
    // (issue #25: the size comes from the script's declarations).
    if (size <= 0 || size > MaxBlockBytes)
    {
        Fatal(0, std::format(" ABL: Unable to AblStackHeap->malloc {} ", what));
    }

    int32_t block = 0;

    if (!_FreeBlocks.empty())
    {
        block = _FreeBlocks.back();
        _FreeBlocks.pop_back();
    }
    else
    {
        block = static_cast<int32_t>(_Blocks.size());

        if (block >= MCAblAddress::MaxBlocks)
        {
            Fatal(0, std::format(" ABL: Unable to AblStackHeap->malloc {} ", what));
        }

        _Blocks.emplace_back();
    }

    MemoryBlock& memory = _Blocks[static_cast<size_t>(block)];
    memory.Bytes.assign(static_cast<size_t>(size), 0);
    memory.Live = true;
    memory.Array = array;

    if (array)
    {
        _ArrayBlockCount++;
    }

    return MCAblAddress::Of(MCAblSpace::Block, block, 0);
}

auto MCAblRuntime::FreeBlock(MCAblAddress address) -> void
{
    if (address.Space() != MCAblSpace::Block || static_cast<size_t>(address.Block()) >= _Blocks.size() ||
        !_Blocks[static_cast<size_t>(address.Block())].Live)
    {
        Fatal(0, " ABL: freeing memory it doesn't own ");
    }

    MemoryBlock& memory = _Blocks[static_cast<size_t>(address.Block())];

    if (memory.Array)
    {
        _ArrayBlockCount--;
    }

    memory = MemoryBlock{};
    _FreeBlocks.push_back(address.Block());
}

auto MCAblRuntime::Bytes(MCAblAddress address) -> std::span<uint8_t>
{
    std::span<uint8_t> memory;

    switch (address.Space())
    {
        case MCAblSpace::Stack:
        {
            memory = {reinterpret_cast<uint8_t*>(_Stack.data()), _Stack.size() * sizeof(MCAblStackItem)};
            break;
        }

        case MCAblSpace::Block:
        {
            if (static_cast<size_t>(address.Block()) < _Blocks.size())
            {
                memory = _Blocks[static_cast<size_t>(address.Block())].Bytes;
            }
            break;
        }

        case MCAblSpace::Literal:
        {
            // The text and the 0 after it.
            std::string& text = Symbol(static_cast<MCAblSymbolId>(address.Block())).LiteralText;
            memory = {reinterpret_cast<uint8_t*>(text.data()), text.size() + 1};
            break;
        }

        default:
        {
            break;
        }
    }

    if (address.Offset < 0 || static_cast<size_t>(address.Offset) >= memory.size())
    {
        Fatal(0, std::format(" ABL: address {:#x}+{} is outside ABL memory ", address.SpaceAndBlock, address.Offset));
    }

    return memory.subspan(static_cast<size_t>(address.Offset));
}

auto MCAblRuntime::Checked(MCAblAddress address, size_t size) -> std::span<uint8_t>
{
    const std::span<uint8_t> bytes = Bytes(address);

    if (bytes.size() < size)
    {
        Fatal(0, std::format(" ABL: address {:#x}+{} is outside ABL memory ", address.SpaceAndBlock, address.Offset));
    }

    return bytes.first(size);
}

auto MCAblRuntime::Text(MCAblAddress address) -> std::string_view
{
    const std::span<uint8_t> bytes = Bytes(address);
    const auto end = std::ranges::find(bytes, uint8_t{0});
    return {reinterpret_cast<const char*>(bytes.data()), static_cast<size_t>(end - bytes.begin())};
}

auto MCAblRuntime::CopyBytes(MCAblAddress target, MCAblAddress source, int32_t size) -> void
{
    const auto count = static_cast<size_t>(size);
    const std::span<uint8_t> to = Checked(target, count);
    // A string may be shorter than the char array it goes into (a literal, a constant, a smaller array): its bytes
    // are copied and the rest zeroed (OB-180).
    const std::span<uint8_t> from = Bytes(source);
    const size_t copied = std::min(count, from.size());
    // memmove: an array may be copied over itself.
    std::memmove(to.data(), from.data(), copied);
    std::fill(to.begin() + static_cast<ptrdiff_t>(copied), to.end(), uint8_t{0});
}

auto MCAblRuntime::StackIndex(MCAblAddress address) -> int32_t
{
    if (address.Space() != MCAblSpace::Stack)
    {
        Fatal(0, " ABL: a frame link outside the stack ");
    }

    return address.Offset / ItemSize;
}

auto MCAblRuntime::StaticLink(int32_t frame) -> int32_t
{
    return StackIndex(StackItem(frame + std::to_underlying(MCAblFrameItem::StaticLink)).Address);
}

auto MCAblRuntime::StaticItem(int32_t offset) const -> MCAblAddress
{
    return _StaticData + offset * ItemSize;
}

// ---- Execution -----------------------------------------------------------------------------------------------------

auto MCAblRuntime::SetModule(std::shared_ptr<MCAblModule> module) -> void
{
    _Module = std::move(module);
    _ModuleHandle = _Module->_Handle;

    if (_Debugger)
    {
        _Debugger->SetModule(*_Module);
    }

    _StaticData = _Module->_StaticData;
}

auto MCAblRuntime::BeginModuleExecution(MCAblModule& module, std::span<MCAblParam> params, MCAblAddress& paramBlock)
    -> MCAblSymbolId
{
    SetModule(module.shared_from_this());
    _Routine = MCAblSymbolId::None;
    const MCAblSymbol& moduleIdPtr = Symbol(_Modules[static_cast<size_t>(module._Handle)].Module);
    _ExecutionCount++;
    _FileNumber = -1;
    // The item above the eternals stays unused: the frame starts one higher.
    _Tos = _EternalCount;
    _Frame = _Tos + 1;
    _StatementCount = 0;
    _Level = 1;
    _CallDepth = 0;

    PushInteger(0);
    PushAddress({});
    PushAddress({});
    PushAddress({});

    if (params.empty())
    {
        return moduleIdPtr.Id;
    }

    // The list in ABL memory, with one more zeroed parameter after it: a formal past the end of the list reads that
    // one (the original's list had it, cleared for each formal).
    paramBlock = AllocateBlock(ParamOffset(params.size() + 1), "module parameters", false);

    for (size_t i = 0; i < params.size(); i++)
    {
        Store(paramBlock + ParamOffset(i), params[i].Integer);
        Store(paramBlock + ParamOffset(i) + 4, params[i].Real);
    }

    size_t index = 0;

    for (MCAblSymbolId formal = moduleIdPtr.Defn.Info.Routine.Params; formal != MCAblSymbolId::None;
         formal = Symbol(formal).Next, index++)
    {
        const MCAblSymbol& formalIdPtr = Symbol(formal);
        const MCAblAddress pastTheList = paramBlock + ParamOffset(params.size());
        Store(pastTheList, int32_t{0});
        Store(pastTheList + 4, 0.0f);

        const MCAblParam missingParam{};
        const MCAblParam& param = index < params.size() ? params[index] : missingParam;
        const MCAblAddress paramAddress = index < params.size() ? paramBlock + ParamOffset(index) : pastTheList;
        const MCAblTypeId formalTypePtr = formalIdPtr.Type;

        if (formalIdPtr.Defn.Key == MCAblSymbolKind::ValueParam)
        {
            if (formalTypePtr == MCAblTypeId::Real)
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
            else if (formalTypePtr == MCAblTypeId::Integer)
            {
                if (param.Type != MCAblParamType::Integer)
                {
                    return MCAblSymbolId::None;
                }

                PushInteger(param.Integer);
            }

            // Faithful: nothing was pushed for an array parameter, so this copies the block the top item points to.
            if (const MCAblType& formalType = Type(formalTypePtr); formalType.Form == MCAblTypeForm::Array)
            {
                const MCAblAddress source = Top().Address;
                const MCAblAddress copy = AllocateArray(formalType.Size, "array parameter");
                Top().Address = copy;
                CopyBytes(copy, source, formalType.Size);
            }
        }
        else
        {
            // A reference parameter points into the list, so the module can write back.
            if (formalTypePtr == MCAblTypeId::Real)
            {
                PushAddress(paramAddress + 4);
            }
            else if (formalTypePtr == MCAblTypeId::Integer)
            {
                PushAddress(paramAddress);
            }
            else
            {
                return MCAblSymbolId::None;
            }
        }
    }

    return moduleIdPtr.Id;
}

auto MCAblRuntime::Run(MCAblModule& module, std::span<MCAblParam> params, MCAblSymbolId function)
    -> std::expected<int32_t, MCAblRuntimeFailure>
{
    MCAblAddress paramBlock{};
    // The instance is kept while it runs, and let go after (its owner may destroy it then).
    const std::shared_ptr<MCAblModule> callerModule = _Module;

    // Whatever ended the run: let the instance go, and copy back the list the reference parameters wrote into.
    auto finishRun = [&]
    {
        _Module = callerModule;

        if (paramBlock.IsNull())
        {
            return;
        }

        for (size_t i = 0; i < params.size(); i++)
        {
            params[i].Integer = Load<int32_t>(paramBlock + ParamOffset(i));
            params[i].Real = Load<float>(paramBlock + ParamOffset(i) + 4);
        }

        FreeBlock(paramBlock);
    };

    try
    {
        const MCAblSymbolId moduleIdPtr = BeginModuleExecution(module, params, paramBlock);

        if (moduleIdPtr == MCAblSymbolId::None)
        {
            finishRun();
            return 0;
        }

        _CallModuleInit = !module._InitCalled;
        module._InitCalled = true;

        if (function != MCAblSymbolId::None)
        {
            ExecuteChild(Symbol(moduleIdPtr), Symbol(function));
        }
        else
        {
            Execute(Symbol(moduleIdPtr));
        }

        module._ReturnValue = _ReturnValue.Integer;
        finishRun();
        return _StatementCount;
    }
    catch (const MCAblRuntimeFailure& failure)
    {
        finishRun();
        return std::unexpected(failure);
    }
}

auto MCAblRuntime::PushItem() -> MCAblStackItem&
{
    if (++_Tos >= MaxStackItems)
    {
        RuntimeError(MCAblRuntimeError::StackOverflow);
    }

    // The original stored only the value's own bytes; the whole 8-byte slot is cleared first.
    MCAblStackItem& item = Top();
    item = MCAblStackItem{};
    return item;
}

auto MCAblRuntime::PushInteger(int32_t value) -> void
{
    PushItem().Integer = value;
}

auto MCAblRuntime::PushReal(float value) -> void
{
    PushItem().Real = value;
}

auto MCAblRuntime::PushByte(char value) -> void
{
    PushItem().Byte = static_cast<uint8_t>(value);
}

auto MCAblRuntime::PushAddress(MCAblAddress address) -> void
{
    PushItem().Address = address;
}

auto MCAblRuntime::JumpTo(MCAblSegmentId segment, int32_t position) -> void
{
    if (segment != _SegmentId)
    {
        _SegmentId = segment;
        _Segment = segment == MCAblSegmentId::None ? nullptr : _Symbols->CodeSegment(segment);
    }

    _CodePosition = position;
}

auto MCAblRuntime::GetCodeSymbol() -> MCAblSymbol&
{
    MCAblSymbolId id = MCAblSymbolId::None;
    std::memcpy(&id, &(*_Segment)[static_cast<size_t>(_CodePosition)], AblCodeSymbolSize);
    _CodePosition += AblCodeSymbolSize;
    return Symbol(id);
}

auto MCAblRuntime::GetCodeStatementMarker() -> int32_t
{
    int32_t line = -1;

    if (_Token == MCAblToken::StatementMarker && _DebugInfo)
    {
        const std::vector<char>& code = *_Segment;
        _FileNumber = static_cast<uint8_t>(code[static_cast<size_t>(_CodePosition)]);
        std::memcpy(&line, &code[static_cast<size_t>(_CodePosition) + 1], sizeof(line));
        _CodePosition += AblCodeStatementMarkerSize;
    }

    return line;
}

auto MCAblRuntime::GetCodeAddressMarker() -> int32_t
{
    int32_t position = -1;

    if (_Token == MCAblToken::AddressMarker)
    {
        int32_t offset = 0;
        std::memcpy(&offset, &(*_Segment)[static_cast<size_t>(_CodePosition)], AblCodeAddressSize);
        position = _CodePosition + offset - 1;
        _CodePosition += AblCodeAddressSize;
    }

    return position;
}

auto MCAblRuntime::GetCodeInteger() -> int32_t
{
    int32_t value = 0;
    std::memcpy(&value, &(*_Segment)[static_cast<size_t>(_CodePosition)], AblCodeIntegerSize);
    _CodePosition += AblCodeIntegerSize;
    return value;
}

auto MCAblRuntime::GetCodeAddress() -> int32_t
{
    int32_t offset = 0;
    std::memcpy(&offset, &(*_Segment)[static_cast<size_t>(_CodePosition)], AblCodeIntegerSize);
    const int32_t position = _CodePosition + offset - 1;
    _CodePosition += AblCodeIntegerSize;
    return position;
}

auto MCAblRuntime::NextInteger() -> int32_t
{
    GetCodeToken();
    ExecExpression();
    const int32_t value = Top().Integer;
    Pop();
    return value;
}

auto MCAblRuntime::NextReal() -> float
{
    GetCodeToken();
    ExecExpression();
    const float value = Top().Real;
    Pop();
    return value;
}

auto MCAblRuntime::NextAddress() -> MCAblAddress
{
    GetCodeToken();
    ExecExpression();
    const MCAblAddress value = Top().Address;
    Pop();
    return value;
}

auto MCAblRuntime::NextReference() -> MCAblAddress
{
    const MCAblSymbol& idPtr = GetCodeSymbol();
    ExecVariable(idPtr, MCAblUse::RefParam);
    return Top().Address;
}

auto MCAblRuntime::PushStackFrameHeader(int32_t oldLevel, int32_t newLevel) -> void
{
    const int32_t header = _Frame;
    // Function value.
    PushInteger(0);
    // Static link: none for a routine of another module; the caller's frame for a routine nested in it; the
    // caller's own static link for a routine at the caller's level.
    MCAblAddress staticLink{};

    if (newLevel == -1)
    {
        staticLink = {};
    }
    else if (newLevel == oldLevel + 1)
    {
        staticLink = MCAblAddress::StackItem(_Frame);
    }
    else if (newLevel == oldLevel)
    {
        staticLink = StackItem(header + std::to_underlying(MCAblFrameItem::StaticLink)).Address;
    }
    else
    {
        RuntimeError(MCAblRuntimeError::NestedFunctionCall);
    }

    PushAddress(staticLink);
    // Dynamic link.
    PushAddress(MCAblAddress::StackItem(_Frame));
    // Return address (set by the caller).
    PushAddress({});
}

auto MCAblRuntime::CurrentRoutineFrame() -> int32_t
{
    int32_t frame = _Frame;

    for (int32_t delta = _Level - Symbol(_Routine).Level - 1; delta > 0; delta--)
    {
        frame = StaticLink(frame);
    }

    return frame;
}

auto MCAblRuntime::AllocateArray(int32_t size, std::string_view what) -> MCAblAddress
{
    return AllocateBlock(size, what, true);
}

auto MCAblRuntime::AllocLocal(MCAblTypeId typePtr) -> void
{
    if (typePtr == MCAblTypeId::Integer)
    {
        PushInteger(0);
    }
    else if (typePtr == MCAblTypeId::Real)
    {
        PushReal(0.0f);
    }
    else if (typePtr == MCAblTypeId::Boolean || typePtr == MCAblTypeId::Char)
    {
        PushByte(0);
    }
    else if (Type(typePtr).Form == MCAblTypeForm::Enum)
    {
        PushInteger(0);
    }
    else if (Type(typePtr).Form == MCAblTypeForm::Array)
    {
        PushAddress(AllocateArray(Type(typePtr).Size, "local array"));
    }
}

auto MCAblRuntime::FreeLocal(const MCAblSymbol& idPtr) -> void
{
    // Only local arrays own memory; a reference parameter's array belongs to the caller.
    if (Type(idPtr.Type).Form == MCAblTypeForm::Array && idPtr.Defn.Key != MCAblSymbolKind::RefParam)
    {
        if (idPtr.Defn.Info.Data.VarType != MCAblStorage::Normal)
        {
            RuntimeError(MCAblRuntimeError::StackOverflow);
        }

        const MCAblAddress block = StackItem(_Frame + idPtr.Defn.Info.Data.Offset).Address;

        if (!block.IsNull())
        {
            FreeBlock(block);
        }
    }
}

auto MCAblRuntime::RoutineEntry(const MCAblSymbol& routineIdPtr) -> void
{
    if (_Debugger)
    {
        _Debugger->TraceRoutineEntry(routineIdPtr);
    }

    JumpTo(routineIdPtr.Defn.Info.Routine.CodeSegment, 0);
    _ReturnValue = MCAblStackItem{};

    // Static and eternal locals live elsewhere.
    for (MCAblSymbolId local = routineIdPtr.Defn.Info.Routine.Locals; local != MCAblSymbolId::None;
         local = Symbol(local).Next)
    {
        const MCAblSymbol& varIdPtr = Symbol(local);

        if (varIdPtr.Defn.Info.Data.VarType == MCAblStorage::Normal)
        {
            AllocLocal(varIdPtr.Type);
        }
    }
}

auto MCAblRuntime::RoutineExit(const MCAblSymbol& routineIdPtr) -> void
{
    if (_Debugger)
    {
        _Debugger->TraceRoutineExit(routineIdPtr);
    }

    for (MCAblSymbolId param = routineIdPtr.Defn.Info.Routine.Params; param != MCAblSymbolId::None;
         param = Symbol(param).Next)
    {
        FreeLocal(Symbol(param));
    }

    for (MCAblSymbolId local = routineIdPtr.Defn.Info.Routine.Locals; local != MCAblSymbolId::None;
         local = Symbol(local).Next)
    {
        if (Symbol(local).Defn.Info.Data.VarType == MCAblStorage::Normal)
        {
            FreeLocal(Symbol(local));
        }
    }

    // A module's frame has no return address: there is no more code to run.
    const MCAblAddress returnAddress = StackItem(_Frame + std::to_underlying(MCAblFrameItem::ReturnAddress)).Address;
    JumpTo(static_cast<MCAblSegmentId>(returnAddress.IsNull() ? 0 : returnAddress.Block()), returnAddress.Offset);

    // A function leaves its value (the frame's first item) on the stack.
    const MCAblAddress dynamicLink = StackItem(_Frame + std::to_underlying(MCAblFrameItem::DynamicLink)).Address;
    _Tos = routineIdPtr.Type != MCAblTypeId::None ? _Frame : _Frame - 1;
    _Frame = dynamicLink.IsNull() ? 0 : StackIndex(dynamicLink);
}

auto MCAblRuntime::CallModuleInit(const MCAblSymbol& moduleIdPtr) -> MCAblSymbolId
{
    if (!_CallModuleInit)
    {
        return MCAblSymbolId::None;
    }

    _CallModuleInit = false;
    const MCAblSymbolId initIdPtr = _Symbols->SearchSymTable("init", moduleIdPtr.Defn.Info.Routine.LocalSymTable);

    if (initIdPtr != MCAblSymbolId::None)
    {
        ExecRoutineCall(Symbol(initIdPtr));
        // The call reads the token after it; back up to it.
        _CodePosition--;
    }

    return initIdPtr;
}

auto MCAblRuntime::Execute(const MCAblSymbol& routineIdPtr) -> void
{
    const MCAblSymbolId thisRoutineIdPtr = _Routine;
    _Routine = routineIdPtr.Id;
    RoutineEntry(routineIdPtr);
    CallModuleInit(Symbol(_Modules[static_cast<size_t>(_Module->_Handle)].Module));
    GetCodeToken();
    ExecStatement();
    _ExitWithReturn = false;
    RoutineExit(routineIdPtr);
    _Routine = thisRoutineIdPtr;
}

auto MCAblRuntime::ExecuteChild(const MCAblSymbol& moduleIdPtr, const MCAblSymbol& childRoutineIdPtr) -> void
{
    const MCAblSymbolId thisRoutineIdPtr = _Routine;
    _Routine = moduleIdPtr.Id;
    RoutineEntry(moduleIdPtr);

    // When the child is init itself, it has just run.
    if (CallModuleInit(moduleIdPtr) != childRoutineIdPtr.Id)
    {
        ExecRoutineCall(childRoutineIdPtr);
        _CodePosition--;
    }

    _ExitWithReturn = false;
    RoutineExit(moduleIdPtr);
    _Routine = thisRoutineIdPtr;
}

auto MCAblRuntime::LogCall(const MCAblSymbol& routineIdPtr, int32_t runTime) -> void
{
    // The original buffered 256 lines and cut each to 127 characters; the port writes each line whole.
    _ProfileLog->WriteString(std::format("[{:08}] {}{} ({})\n", _ExecutionCount,
                                         std::string(static_cast<size_t>(std::max(_CallDepth, 0)), ' '),
                                         routineIdPtr.Name, runTime));
    _ProfileLogLines++;
}

MCAblBrainScope::MCAblBrainScope(std::weak_ptr<MCMoverGroup> group, std::weak_ptr<MCGameObject> object,
                                 int32_t objectClass, std::weak_ptr<MCMechWarrior> warrior)
    : _Runtime(AblRuntime())
{
    if (_Runtime)
    {
        MCAblBrainContext& brain = _Runtime->Brain;
        brain.IsUnitOrder = false;
        brain.SetGroup(std::move(group));
        brain.SetObject(std::move(object));
        brain.ObjectClass = objectClass;
        brain.SetContact({});
        brain.SetWarrior(std::move(warrior));
    }
}

MCAblBrainScope::~MCAblBrainScope()
{
    if (_Runtime)
    {
        MCAblBrainContext& brain = _Runtime->Brain;
        brain.IsUnitOrder = false;
        brain.SetGroup({});
        brain.SetObject({});
        brain.ObjectClass = 0;
        brain.SetWarrior({});
        brain.SetContact({});
    }
}

auto AblRuntime() -> std::shared_ptr<MCAblRuntime>
{
    return MCGameContext::Current().AblRuntime();
}

auto AblInit(const MCAblOptions& options) -> void
{
    // The original's heap sizes, stack size, code buffer size, module and static variable limits are gone.
    MCGameContext& context = MCGameContext::Current();
    auto symbols = std::make_shared<MCAblSymbolTable>();
    context.SetAblSymbols(symbols);
    context.SetAblRuntime(MCAblRuntime::Create(std::move(symbols), options));
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
