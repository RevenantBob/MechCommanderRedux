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
        MCPort::StrLwr(lower.data());
        return lower;
    }

    /// <summary>Finds <paramref name="name"/> (lower-cased) among the globals of the libraries <paramref name="entry"/> uses.</summary>
    auto SearchLibrariesUsed(const MCAblModuleEntry& entry, std::string_view name) -> MCAblSymbol*
    {
        const std::string lower = LowerCaseName(name);
        const MCAblRuntime& runtime = *AblRuntime();

        for (const MCAblModule* library : entry.LibrariesUsed)
        {
            MCAblSymbol* libraryIdPtr = runtime.Module(library->Handle()).Module;

            if (MCAblSymbol* symbol = SearchSymTable(lower, libraryIdPtr->Defn.Info.Routine.LocalSymTable))
            {
                return symbol;
            }
        }

        return nullptr;
    }
}

MCAblModule::MCAblModule(int32_t moduleHandle)
{
    Attach(*AblRuntime(), moduleHandle);
}

MCAblModule::MCAblModule(LibraryTag)
{
}

MCAblModule::~MCAblModule()
{
    if (_Id > -1)
    {
        if (MCAblRuntime* runtime = AblRuntime())
        {
            runtime->Unregister(this);
        }
    }
}

auto MCAblModule::Attach(MCAblRuntime& runtime, int32_t moduleHandle) -> void
{
    _Handle = moduleHandle;
    _Id = runtime.Register(this);

    // One item per static; a static array's item points to its own block.
    const std::vector<int32_t>& sizes = runtime.Module(moduleHandle).StaticSizes;
    _StaticData.assign(sizes.size(), MCAblStackItem{});
    _StaticArrays.resize(sizes.size());

    for (size_t i = 0; i < sizes.size(); i++)
    {
        if (sizes[i] > 0)
        {
            _StaticArrays[i] = std::make_unique<uint8_t[]>(static_cast<size_t>(sizes[i]));
            _StaticData[i].Address = reinterpret_cast<MCAddress>(_StaticArrays[i].get());

            // The original's heap filled new blocks with 0xff, and nothing cleared a static array.
            if constexpr (!FixAblUninitializedStatics)
            {
                std::memset(_StaticArrays[i].get(), 0xff, static_cast<size_t>(sizes[i]));
            }
        }
    }

    _InitCalled = false;

    if (runtime.Debugger())
    {
        _Watches = std::make_unique<MCAblWatchManager>();
        _BreakPoints = std::make_unique<MCAblBreakPointManager>();
    }
}

auto MCAblModule::Entry() const -> const MCAblModuleEntry&
{
    return AblRuntime()->Module(_Handle);
}

auto MCAblModule::Run(std::span<MCAblParam> params, MCAblSymbol* function)
    -> std::expected<int32_t, MCAblRuntimeFailure>
{
    return AblRuntime()->Run(*this, params, function);
}

auto MCAblModule::Execute(std::span<MCAblParam> params) -> int32_t
{
    return Execute(params, nullptr);
}

auto MCAblModule::Execute(std::span<MCAblParam> moduleParams, MCAblSymbol* function) -> int32_t
{
    auto statements = Run(moduleParams, function);

    // A runtime error ends the game, as in MCX.EXE.
    if (!statements)
    {
        Fatal(-8, statements.error().Message());
    }

    return *statements;
}

auto MCAblModule::FindSymbol(std::string_view symbolName, MCAblSymbol* function, bool searchLibraries) const
    -> MCAblSymbol*
{
    const std::string lower = LowerCaseName(symbolName);

    if (function)
    {
        if (MCAblSymbol* symbol = SearchSymTable(lower, function->Defn.Info.Routine.LocalSymTable))
        {
            return symbol;
        }
    }

    const MCAblModuleEntry& entry = Entry();
    MCAblSymbol* symbol = SearchSymTable(lower, entry.Module->Defn.Info.Routine.LocalSymTable);

    if (!symbol && searchLibraries)
    {
        symbol = SearchLibrariesUsed(entry, symbolName);
    }

    return symbol;
}

auto MCAblModule::FindFunction(std::string_view functionName, bool searchLibraries) const -> MCAblSymbol*
{
    // The module's own table is searched with the name as given (not lower-cased).
    const MCAblModuleEntry& entry = Entry();
    MCAblSymbol* symbol = SearchSymTable(functionName, entry.Module->Defn.Info.Routine.LocalSymTable);

    if (!symbol && searchLibraries)
    {
        symbol = SearchLibrariesUsed(entry, functionName);
    }

    return symbol;
}

auto MCAblModule::SetStaticInteger(std::string_view staticName, int32_t value) -> MCAblStaticResult
{
    MCAblSymbol* idPtr = FindSymbol(staticName);

    if (!idPtr)
    {
        return MCAblStaticResult::NoSymbol;
    }

    if (idPtr->TypePtr != IntegerTypePtr)
    {
        return MCAblStaticResult::WrongType;
    }

    if (idPtr->Defn.Info.Data.VarType != MCAblStorage::Static)
    {
        return MCAblStaticResult::NotStatic;
    }

    _StaticData[static_cast<size_t>(idPtr->Defn.Info.Data.Offset)].Integer = value;
    return MCAblStaticResult::Set;
}

auto MCAblModule::SetStaticReal(std::string_view staticName, float value) -> MCAblStaticResult
{
    MCAblSymbol* idPtr = FindSymbol(staticName);

    if (!idPtr)
    {
        return MCAblStaticResult::NoSymbol;
    }

    if (idPtr->TypePtr != RealTypePtr)
    {
        return MCAblStaticResult::WrongType;
    }

    if (idPtr->Defn.Info.Data.VarType != MCAblStorage::Static)
    {
        return MCAblStaticResult::NotStatic;
    }

    _StaticData[static_cast<size_t>(idPtr->Defn.Info.Data.Offset)].Real = value;
    return MCAblStaticResult::Set;
}

auto MCAblModule::StaticArray(const MCAblSymbol* idPtr) -> std::span<uint8_t>
{
    const auto offset = static_cast<size_t>(idPtr->Defn.Info.Data.Offset);
    const int32_t size = Entry().StaticSizes[offset];
    return {_StaticArrays[offset].get(), static_cast<size_t>(std::max(size, 0))};
}

auto MCAblModule::SetStaticIntegerArray(std::string_view staticName, std::span<const int32_t> values)
    -> MCAblStaticResult
{
    MCAblSymbol* idPtr = FindSymbol(staticName);

    if (!idPtr)
    {
        return MCAblStaticResult::NoSymbol;
    }

    if (idPtr->Defn.Info.Data.VarType != MCAblStorage::Static)
    {
        return MCAblStaticResult::NotStatic;
    }

    // Port fix: the original copied every value, past the end of a shorter array.
    const std::span<uint8_t> block = StaticArray(idPtr);
    std::memcpy(block.data(), values.data(), std::min(block.size(), values.size_bytes()));
    return MCAblStaticResult::Set;
}

auto MCAblModule::SetStaticRealArray(std::string_view staticName, std::span<const float> values) -> MCAblStaticResult
{
    MCAblSymbol* idPtr = FindSymbol(staticName);

    if (!idPtr)
    {
        return MCAblStaticResult::NoSymbol;
    }

    if (idPtr->Defn.Info.Data.VarType != MCAblStorage::Static)
    {
        return MCAblStaticResult::NotStatic;
    }

    // Port fix: the original copied every value, past the end of a shorter array.
    const std::span<uint8_t> block = StaticArray(idPtr);
    std::memcpy(block.data(), values.data(), std::min(block.size(), values.size_bytes()));
    return MCAblStaticResult::Set;
}

auto MCAblModule::SourceFile(int32_t fileNumber) const -> const std::string&
{
    return Entry().SourceFiles[static_cast<size_t>(fileNumber)];
}
