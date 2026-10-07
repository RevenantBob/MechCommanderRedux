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
/// parameters are passed as the address of <c>Integer</c> or <c>Real</c>, so ABL code can write back into the list.
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
/// own static variables; the code and symbols are shared through the runtime's module registry.
/// </summary>
/// <remarks>
/// The instance registers itself with the current runtime (<see cref="AblRuntime"/>) and unregisters when destroyed;
/// a module outliving its runtime only frees its own data.
/// </remarks>
class MCAblModule
{
public:
    /// <summary>
    /// An instance of registered module <paramref name="moduleHandle"/>: its static data (and static arrays), and
    /// with the debugger on, its watch and break-point managers.
    /// </summary>
    explicit MCAblModule(int32_t moduleHandle);

    /// <summary>The key only the runtime can make, for the library constructor.</summary>
    class LibraryTag
    {
        friend class MCAblRuntime;
        LibraryTag() = default;
    };

    /// <summary>A library instance before its module is compiled (the compile records it in the symbols it makes).</summary>
    explicit MCAblModule(LibraryTag);

    /// <summary>Unregisters the instance.</summary>
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
    int32_t Execute(std::span<MCAblParam> moduleParams, MCAblSymbol* function);

    /// <summary>
    /// Runs the module as <see cref="Execute(std::span{MCAblParam})"/> does (or only <paramref name="function"/>), but
    /// returns a runtime error instead of ending the game.
    /// </summary>
    std::expected<int32_t, MCAblRuntimeFailure> Run(std::span<MCAblParam> params = {}, MCAblSymbol* function = nullptr);

    /// <summary>
    /// Finds a symbol: in <paramref name="function"/>'s scope, then the module's, then (with
    /// <paramref name="searchLibraries"/>) the libraries it uses. The name is looked up lower-cased.
    /// </summary>
    MCAblSymbol* FindSymbol(std::string_view symbolName, MCAblSymbol* function = nullptr,
                            bool searchLibraries = false) const;

    /// <summary>
    /// Finds a function of the module (its own scope with the name as given), or with
    /// <paramref name="searchLibraries"/> of the libraries it uses (lower-cased).
    /// </summary>
    MCAblSymbol* FindFunction(std::string_view functionName, bool searchLibraries = false) const;

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
    MCAblWatchManager* Watches() const { return _Watches.get(); }

    /// <summary>The instance's break points (null without the debugger).</summary>
    MCAblBreakPointManager* BreakPoints() const { return _BreakPoints.get(); }

    /// <summary>Debugger modes for this instance (the debugger takes them when the module runs).</summary>
    bool Trace = false;
    bool Step = false;
    bool TraceEntry = false;
    bool TraceExit = false;

private:
    friend class MCAblRuntime;

    /// <summary>Makes this an instance of <paramref name="moduleHandle"/> (see the public constructor).</summary>
    void Attach(MCAblRuntime& runtime, int32_t moduleHandle);

    /// <summary>The bytes of static array <paramref name="idPtr"/>'s block.</summary>
    std::span<uint8_t> StaticArray(const MCAblSymbol* idPtr);

    int32_t _Id = -1;
    std::string _Name;
    int32_t _Handle = -1;
    /// <summary>The instance's static variables, one item each (arrays as pointers to their blocks).</summary>
    std::vector<MCAblStackItem> _StaticData;
    /// <summary>The static arrays' blocks, by static variable (null for a scalar).</summary>
    std::vector<std::unique_ptr<uint8_t[]>> _StaticArrays;
    int32_t _ReturnValue = 0;
    /// <summary>Whether the module's <c>init</c> function has run.</summary>
    bool _InitCalled = false;
    std::unique_ptr<MCAblWatchManager> _Watches;
    std::unique_ptr<MCAblBreakPointManager> _BreakPoints;
};
