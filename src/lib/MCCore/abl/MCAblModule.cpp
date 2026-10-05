#include "stdafx.h"
#include "abl/MCAblModule.h"
#include "abl/MCAblDebugger.h"
#include "abl/MCAblRuntime.h"
#include "abl/MCAblSymbolTable.h"
#include "lib/MCFatal.h"
#include "main/MCOriginalBugFixes.h"

namespace
{
    /// <summary><paramref name="name"/> lower-cased, as the symbol tables hold it.</summary>
    auto LowerCaseName(std::string_view name) -> std::string
    {
        std::string lower(name);
        MCPort::StrLwr(lower);
        return lower;
    }

    /// <summary>Finds <paramref name="name"/> (lower-cased) among the globals of the libraries <paramref name="entry"/> uses.</summary>
    auto SearchLibrariesUsed(const MCAblRuntime& runtime, const MCAblModuleEntry& entry, std::string_view name)
        -> MCAblSymbolId
    {
        const std::string lower = LowerCaseName(name);

        for (const MCAblLibraryId library : entry.LibrariesUsed)
        {
            const MCAblSymbol& libraryIdPtr = runtime.Symbol(runtime.Module(runtime.Library(library).Handle()).Module);

            if (const MCAblSymbolId symbol =
                    runtime.Symbols().SearchSymTable(lower, libraryIdPtr.Defn.Info.Routine.LocalSymTable);
                symbol != MCAblSymbolId::None)
            {
                return symbol;
            }
        }

        return MCAblSymbolId::None;
    }
}

auto MCAblModule::Create(int32_t moduleHandle) -> std::shared_ptr<MCAblModule>
{
    auto module = std::make_shared<MCAblModule>(Key{});
    module->Attach(*AblRuntime(), moduleHandle);
    return module;
}

MCAblModule::~MCAblModule()
{
    if (const std::shared_ptr<MCAblRuntime> runtime = _Runtime.lock())
    {
        if (_Id > -1)
        {
            runtime->Unregister(_Id);
        }

        for (const MCAblAddress block : _Blocks)
        {
            runtime->FreeBlock(block);
        }
    }
}

auto MCAblModule::Attach(MCAblRuntime& runtime, int32_t moduleHandle) -> void
{
    _Runtime = runtime.weak_from_this();
    _Handle = moduleHandle;
    _Id = runtime.Register(*this);

    // One item per static; a static array's item holds its own block's address.
    const std::vector<int32_t>& sizes = runtime.Module(moduleHandle).StaticSizes;

    if (!sizes.empty())
    {
        _StaticData =
            runtime.AllocateBlock(static_cast<int32_t>(sizes.size() * sizeof(MCAblStackItem)), "static data", false);
        _Blocks.push_back(_StaticData);
    }

    for (size_t i = 0; i < sizes.size(); i++)
    {
        if (sizes[i] > 0)
        {
            const MCAblAddress array = runtime.AllocateBlock(sizes[i], "static array", false);
            _Blocks.push_back(array);
            MCAblStackItem item{};
            item.Address = array;
            runtime.Store(_StaticData + static_cast<int32_t>(i * sizeof(MCAblStackItem)), item);

            // The original's heap filled new blocks with 0xff, and nothing cleared a static array.
            if constexpr (!FixAblUninitializedStatics)
            {
                std::ranges::fill(runtime.Bytes(array), uint8_t{0xff});
            }
        }
    }

    _InitCalled = false;

    if (runtime.Debugger())
    {
        _Watches = std::make_shared<MCAblWatchManager>();
        _BreakPoints = std::make_shared<MCAblBreakPointManager>();
    }
}

auto MCAblModule::Entry() const -> const MCAblModuleEntry&
{
    return AblRuntime()->Module(_Handle);
}

auto MCAblModule::Run(std::span<MCAblParam> params, MCAblSymbolId function)
    -> std::expected<int32_t, MCAblRuntimeFailure>
{
    return AblRuntime()->Run(*this, params, function);
}

auto MCAblModule::Execute(std::span<MCAblParam> params) -> int32_t
{
    return Execute(params, MCAblSymbolId::None);
}

auto MCAblModule::Execute(std::span<MCAblParam> moduleParams, MCAblSymbolId function) -> int32_t
{
    auto statements = Run(moduleParams, function);

    // A runtime error ends the game, as in MCX.EXE.
    if (!statements)
    {
        Fatal(-8, statements.error().Message());
    }

    return *statements;
}

auto MCAblModule::FindSymbol(std::string_view symbolName, MCAblSymbolId function, bool searchLibraries) const
    -> MCAblSymbolId
{
    const std::string lower = LowerCaseName(symbolName);
    const std::shared_ptr<MCAblRuntime> runtime = AblRuntime();
    const MCAblSymbolTable& symbols = runtime->Symbols();

    if (function != MCAblSymbolId::None)
    {
        if (const MCAblSymbolId symbol =
                symbols.SearchSymTable(lower, symbols.Symbol(function).Defn.Info.Routine.LocalSymTable);
            symbol != MCAblSymbolId::None)
        {
            return symbol;
        }
    }

    const MCAblModuleEntry& entry = Entry();
    MCAblSymbolId symbol = symbols.SearchSymTable(lower, symbols.Symbol(entry.Module).Defn.Info.Routine.LocalSymTable);

    if (symbol == MCAblSymbolId::None && searchLibraries)
    {
        symbol = SearchLibrariesUsed(*runtime, entry, symbolName);
    }

    return symbol;
}

auto MCAblModule::FindFunction(std::string_view functionName, bool searchLibraries) const -> MCAblSymbolId
{
    // The module's own table is searched with the name as given (not lower-cased).
    const std::shared_ptr<MCAblRuntime> runtime = AblRuntime();
    const MCAblSymbolTable& symbols = runtime->Symbols();
    const MCAblModuleEntry& entry = Entry();
    MCAblSymbolId symbol =
        symbols.SearchSymTable(functionName, symbols.Symbol(entry.Module).Defn.Info.Routine.LocalSymTable);

    if (symbol == MCAblSymbolId::None && searchLibraries)
    {
        symbol = SearchLibrariesUsed(*runtime, entry, functionName);
    }

    return symbol;
}

auto MCAblModule::StaticItem(const MCAblSymbol& symbol) const -> MCAblAddress
{
    return _StaticData + symbol.Defn.Info.Data.Offset * static_cast<int32_t>(sizeof(MCAblStackItem));
}

auto MCAblModule::SetStaticInteger(std::string_view staticName, int32_t value) -> MCAblStaticResult
{
    const MCAblSymbolId id = FindSymbol(staticName);

    if (id == MCAblSymbolId::None)
    {
        return MCAblStaticResult::NoSymbol;
    }

    const std::shared_ptr<MCAblRuntime> runtime = AblRuntime();
    const MCAblSymbol& idPtr = runtime->Symbol(id);

    if (idPtr.Type != MCAblTypeId::Integer)
    {
        return MCAblStaticResult::WrongType;
    }

    if (idPtr.Defn.Info.Data.VarType != MCAblStorage::Static)
    {
        return MCAblStaticResult::NotStatic;
    }

    runtime->Store(StaticItem(idPtr), value);
    return MCAblStaticResult::Set;
}

auto MCAblModule::SetStaticReal(std::string_view staticName, float value) -> MCAblStaticResult
{
    const MCAblSymbolId id = FindSymbol(staticName);

    if (id == MCAblSymbolId::None)
    {
        return MCAblStaticResult::NoSymbol;
    }

    const std::shared_ptr<MCAblRuntime> runtime = AblRuntime();
    const MCAblSymbol& idPtr = runtime->Symbol(id);

    if (idPtr.Type != MCAblTypeId::Real)
    {
        return MCAblStaticResult::WrongType;
    }

    if (idPtr.Defn.Info.Data.VarType != MCAblStorage::Static)
    {
        return MCAblStaticResult::NotStatic;
    }

    runtime->Store(StaticItem(idPtr), value);
    return MCAblStaticResult::Set;
}

auto MCAblModule::SetStaticIntegerArray(std::string_view staticName, std::span<const int32_t> values)
    -> MCAblStaticResult
{
    const MCAblSymbolId id = FindSymbol(staticName);

    if (id == MCAblSymbolId::None)
    {
        return MCAblStaticResult::NoSymbol;
    }

    const std::shared_ptr<MCAblRuntime> runtime = AblRuntime();
    const MCAblSymbol& idPtr = runtime->Symbol(id);

    if (idPtr.Defn.Info.Data.VarType != MCAblStorage::Static)
    {
        return MCAblStaticResult::NotStatic;
    }

    // A scalar has no block: nothing is copied. Port fix: the original copied every value, past the end of a
    // shorter array.
    if (Entry().StaticSizes[static_cast<size_t>(idPtr.Defn.Info.Data.Offset)] > 0)
    {
        const std::span<uint8_t> block = runtime->Bytes(runtime->Load<MCAblAddress>(StaticItem(idPtr)));
        std::memcpy(block.data(), values.data(), std::min(block.size(), values.size_bytes()));
    }

    return MCAblStaticResult::Set;
}

auto MCAblModule::SetStaticRealArray(std::string_view staticName, std::span<const float> values) -> MCAblStaticResult
{
    const MCAblSymbolId id = FindSymbol(staticName);

    if (id == MCAblSymbolId::None)
    {
        return MCAblStaticResult::NoSymbol;
    }

    const std::shared_ptr<MCAblRuntime> runtime = AblRuntime();
    const MCAblSymbol& idPtr = runtime->Symbol(id);

    if (idPtr.Defn.Info.Data.VarType != MCAblStorage::Static)
    {
        return MCAblStaticResult::NotStatic;
    }

    // A scalar has no block: nothing is copied. Port fix: the original copied every value, past the end of a
    // shorter array.
    if (Entry().StaticSizes[static_cast<size_t>(idPtr.Defn.Info.Data.Offset)] > 0)
    {
        const std::span<uint8_t> block = runtime->Bytes(runtime->Load<MCAblAddress>(StaticItem(idPtr)));
        std::memcpy(block.data(), values.data(), std::min(block.size(), values.size_bytes()));
    }

    return MCAblStaticResult::Set;
}

auto MCAblModule::SourceFile(int32_t fileNumber) const -> const std::string&
{
    return Entry().SourceFiles[static_cast<size_t>(fileNumber)];
}
