#pragma once

#include "abl/MCAblCode.h"

class MCAblBreakPointManager;
class MCAblRuntime;
class MCAblWatchManager;
struct MCAblModuleEntry;
struct MCAblRuntimeFailure;

/// <summary>What an <see cref="MCAblParam"/> holds.</summary>
enum class MCAblParamType : uint8_t
{
    Void = 0,
    Integer = 1,
    Real = 2
};

/// <summary>
/// A parameter passed from C++ to an ABL module. Value parameters are copied onto the ABL stack; reference
/// parameters are passed as the address of <c>Integer</c> or <c>Real</c> in a copy of the list in ABL memory, which
/// the run copies back, so ABL code writes back into the list.
/// </summary>
struct MCAblParam
{
    MCAblParamType Type = MCAblParamType::Void;
    int32_t Integer = 0;
    float Real = 0;

    /// <summary>Makes it the integer <paramref name="value"/>.</summary>
    void SetInteger(int32_t value)
    {
        Type = MCAblParamType::Integer;
        Integer = value;
    }

    /// <summary>Makes it the real <paramref name="value"/>.</summary>
    void SetReal(float value)
    {
        Type = MCAblParamType::Real;
        Real = value;
    }
};

/// <summary>What setting a static variable from C++ did.</summary>
enum class MCAblStaticResult : int32_t
{
    Set = 0,
    /// <summary>No symbol of that name.</summary>
    NoSymbol = 1,
    /// <summary>The variable isn't of the value's type.</summary>
    WrongType = 2,
    /// <summary>The variable isn't static.</summary>
    NotStatic = 3
};

/// <summary>
/// A running instance of a compiled module: a unit's brain, a mission script, or a library. Each instance has its
/// own static variables (blocks of the runtime's memory); the code and symbols are shared through the runtime's
/// module registry.
/// </summary>
/// <remarks>
/// The instance registers itself with the current runtime (<see cref="AblRuntime"/>), which refers to it while it
/// lives and keeps it while it runs, and unregisters (freeing its statics) when destroyed; a module outliving its
/// runtime has nothing left to free.
/// </remarks>
class MCAblModule : public std::enable_shared_from_this<MCAblModule>
{
    /// <summary>The key only the module and the runtime can make (instances are made shared).</summary>
    struct Key
    {
    };

    friend class MCAblRuntime;

public:
    /// <summary>
    /// An instance of registered module <paramref name="moduleHandle"/> of the current runtime: its static data (and
    /// static arrays), and with the debugger on, its watch and break-point managers.
    /// </summary>
    static std::shared_ptr<MCAblModule> Create(int32_t moduleHandle);

    /// <summary>An instance not attached yet (Create; the runtime makes a library's before compiling it).</summary>
    explicit MCAblModule(Key) {}

    /// <summary>Unregisters the instance and frees its static data.</summary>
    ~MCAblModule();

    MCAblModule(const MCAblModule&) = delete;
    MCAblModule& operator=(const MCAblModule&) = delete;

    /// <summary>Names the instance (only reports show it).</summary>
    void SetName(std::string_view name) { _Name = name; }

    /// <summary>The instance number (the order instances were made in), or -1.</summary>
    int32_t Id() const { return _Id; }

    const std::string& Name() const { return _Name; }

    /// <summary>The compiled module's index in the registry, or -1.</summary>
    int32_t Handle() const { return _Handle; }

    /// <summary>The integer the last execution returned.</summary>
    int32_t ReturnValue() const { return _ReturnValue; }

    /// <summary>The compiled module (its registry entry).</summary>
    const MCAblModuleEntry& Entry() const;

    /// <summary>
    /// Runs the module's main code with <paramref name="params"/> for its parameters (after its <c>init</c> function on
    /// the first run). A runtime error is fatal, as in MCX.EXE.
    /// </summary>
    /// <returns>The number of statements executed (0 if a parameter doesn't match).</returns>
    int32_t Execute(std::span<MCAblParam> params = {});

    /// <summary>Runs only function <paramref name="function"/> of the module, in the module's frame.</summary>
    /// <returns>The number of statements executed.</returns>
    int32_t Execute(std::span<MCAblParam> moduleParams, MCAblSymbolId function);

    /// <summary>
    /// Runs the module as <see cref="Execute(std::span{MCAblParam})"/> does (or only <paramref name="function"/>), but
    /// returns a runtime error instead of ending the game.
    /// </summary>
    std::expected<int32_t, MCAblRuntimeFailure> Run(std::span<MCAblParam> params = {},
                                                    MCAblSymbolId function = MCAblSymbolId::None);

    /// <summary>
    /// Finds a symbol: in <paramref name="function"/>'s scope, then the module's, then (with
    /// <paramref name="searchLibraries"/>) the libraries it uses. The name is looked up lower-cased.
    /// </summary>
    /// <returns>The symbol, or none.</returns>
    MCAblSymbolId FindSymbol(std::string_view symbolName, MCAblSymbolId function = MCAblSymbolId::None,
                             bool searchLibraries = false) const;

    /// <summary>
    /// Finds a function of the module (its own scope with the name as given), or with
    /// <paramref name="searchLibraries"/> of the libraries it uses (lower-cased).
    /// </summary>
    /// <returns>The function, or none.</returns>
    MCAblSymbolId FindFunction(std::string_view functionName, bool searchLibraries = false) const;

    /// <summary>Sets static integer <paramref name="staticName"/>.</summary>
    MCAblStaticResult SetStaticInteger(std::string_view staticName, int32_t value);

    /// <summary>Sets static real <paramref name="staticName"/>.</summary>
    MCAblStaticResult SetStaticReal(std::string_view staticName, float value);

    /// <summary>
    /// Copies <paramref name="values"/> into static array <paramref name="staticName"/> (as many as fit; the element
    /// type isn't checked).
    /// </summary>
    MCAblStaticResult SetStaticIntegerArray(std::string_view staticName, std::span<const int32_t> values);

    /// <summary>Copies <paramref name="values"/> into static array <paramref name="staticName"/> (as many as fit).</summary>
    MCAblStaticResult SetStaticRealArray(std::string_view staticName, std::span<const float> values);

    /// <summary>Name of source file <paramref name="fileNumber"/> of the module.</summary>
    const std::string& SourceFile(int32_t fileNumber) const;

    /// <summary>The instance's watches (null without the debugger).</summary>
    std::shared_ptr<MCAblWatchManager> Watches() const { return _Watches; }

    /// <summary>The instance's break points (null without the debugger).</summary>
    std::shared_ptr<MCAblBreakPointManager> BreakPoints() const { return _BreakPoints; }

    /// <summary>The library this instance is, or none for a module's instance.</summary>
    MCAblLibraryId Library() const { return _Library; }

    /// <summary>Debugger modes for this instance (the debugger takes them when the module runs).</summary>
    bool Trace = false;
    bool Step = false;
    bool TraceEntry = false;
    bool TraceExit = false;

private:
    /// <summary>
    /// Makes this an instance of <paramref name="moduleHandle"/> of <paramref name="runtime"/> (registers it,
    /// allocates its statics).
    /// </summary>
    void Attach(MCAblRuntime& runtime, int32_t moduleHandle);

    /// <summary>Where static variable <paramref name="symbol"/> is: its item in the static data.</summary>
    MCAblAddress StaticItem(const MCAblSymbol& symbol) const;

    /// <summary>The runtime it is registered with.</summary>
    std::weak_ptr<MCAblRuntime> _Runtime;
    int32_t _Id = -1;
    std::string _Name;
    int32_t _Handle = -1;
    MCAblLibraryId _Library = MCAblLibraryId::None;
    /// <summary>The instance's static variables, one item each (an array's holds its block's address), or none.</summary>
    MCAblAddress _StaticData{};
    /// <summary>The blocks it allocated (the static data and the static arrays), freed with it.</summary>
    std::vector<MCAblAddress> _Blocks;
    int32_t _ReturnValue = 0;
    /// <summary>Whether the module's <c>init</c> function has run.</summary>
    bool _InitCalled = false;
    std::shared_ptr<MCAblWatchManager> _Watches;
    std::shared_ptr<MCAblBreakPointManager> _BreakPoints;
};
