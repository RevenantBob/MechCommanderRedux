#pragma once

// The ABL debugger: watches (report or break when a variable is stored or fetched), break points on source lines,
// statement / routine tracing and single-stepping. The in-game window that takes its commands is
// MCAblDebuggerWindow (honorb.cpp opens it and routes its commands to MCAblDebugger::ProcessCommand).

#include "abl/MCAblCode.h"

class MCAblModule;
class MCAblRuntime;

/// <summary>A watch on a variable (the symbol's <c>Watch</c> points to it).</summary>
struct MCWatch
{
    MCAblSymbol* IdPtr = nullptr;
    /// <summary>Report stores to it.</summary>
    bool Store = false;
    /// <summary>Break into the debugger on a store.</summary>
    bool BreakOnStore = false;
    /// <summary>Report fetches of it.</summary>
    bool Fetch = false;
    /// <summary>Break into the debugger on a fetch.</summary>
    bool BreakOnFetch = false;
};

/// <summary>A module's watches.</summary>
/// <remarks>
/// The original's table held MaxWatchesPerModule (20) watches; the port's has no limit, and removing a watch keeps
/// the others (OB-040 fixed).
/// </remarks>
class MCAblWatchManager
{
public:
    MCAblWatchManager() = default;

    /// <summary>Clears the watched symbols' links.</summary>
    ~MCAblWatchManager();

    MCAblWatchManager(const MCAblWatchManager&) = delete;
    MCAblWatchManager& operator=(const MCAblWatchManager&) = delete;

    /// <summary>The watch on <paramref name="idPtr"/>, new if it has none (constants, variables and parameters only).</summary>
    /// <returns>The watch, or null if the symbol can't be watched.</returns>
    MCWatch* Add(MCAblSymbol* idPtr);

    /// <summary>Removes the watch on <paramref name="idPtr"/>.</summary>
    /// <returns>0, 1 (no symbol) or 2 (not watched).</returns>
    int32_t Remove(MCAblSymbol* idPtr);

    /// <summary>Removes every watch.</summary>
    /// <returns>How many there were.</returns>
    int32_t RemoveAll();

    /// <summary>Turns reporting of stores to <paramref name="idPtr"/> on or off (<paramref name="breakOnStore"/> to also break).</summary>
    /// <returns>0, 1 (no symbol) or 2 (the symbol can't be watched).</returns>
    int32_t SetStore(MCAblSymbol* idPtr, bool on, bool breakOnStore = false);

    /// <summary>Turns reporting of fetches of <paramref name="idPtr"/> on or off (<paramref name="breakOnFetch"/> to also break).</summary>
    /// <returns>0, 1 (no symbol) or 2 (the symbol can't be watched).</returns>
    int32_t SetFetch(MCAblSymbol* idPtr, bool on, bool breakOnFetch = false);

    /// <summary>Whether stores to <paramref name="idPtr"/> are reported.</summary>
    static bool GetStore(const MCAblSymbol* idPtr);

    /// <summary>Whether fetches of <paramref name="idPtr"/> are reported.</summary>
    static bool GetFetch(const MCAblSymbol* idPtr);

    /// <summary>How many watches there are.</summary>
    size_t Count() const { return _Watches.size(); }

private:
    std::vector<std::unique_ptr<MCWatch>> _Watches;
};

/// <summary>A module's break points: source line numbers.</summary>
/// <remarks>
/// The original's sorted table held MaxBreakPointsPerModule (20) lines; the port's has no limit. Adding a line keeps
/// the others (OB-041 fixed), and removing a line without one keeps them all (OB-042 fixed).
/// </remarks>
class MCAblBreakPointManager
{
public:
    /// <summary>Adds a break point at <paramref name="lineNumber"/>.</summary>
    /// <returns>0, or 2 (bad line).</returns>
    int32_t Add(int32_t lineNumber);

    /// <summary>Removes the break point at <paramref name="lineNumber"/>, if there is one.</summary>
    void Remove(int32_t lineNumber) { _Lines.erase(lineNumber); }

    /// <summary>Removes every break point.</summary>
    /// <returns>How many there were.</returns>
    int32_t RemoveAll();

    /// <summary>Whether there is a break point at <paramref name="lineNumber"/>.</summary>
    bool IsBreakPoint(int32_t lineNumber) const { return _Lines.contains(lineNumber); }

    /// <summary>The lines with a break point, in order.</summary>
    const std::set<int32_t>& Lines() const { return _Lines; }

private:
    std::set<int32_t> _Lines;
};

/// <summary>A debugger window command (<see cref="MCAblDebugger::ProcessCommand"/>).</summary>
enum class MCAblDebugCommand : int32_t
{
    /// <summary>Select the module the commands apply to (or list them).</summary>
    SelectModule = 0,
    Trace = 1,
    Step = 2,
    AddBreakPoint = 3,
    RemoveBreakPoint = 4,
    /// <summary>Set or clear a watch: the number holds the bits (1 store off, 2 store on, 4 fetch off, 8 fetch on, 16 break).</summary>
    Watch = 5,
    ClearWatches = 6,
    PrintValue = 7,
    Resume = 8,
    Help = 9,
    ModuleInfo = 10
};

/// <summary>
/// The ABL debugger: traces execution of the current module, reports watched variables and breaks into
/// <see cref="DebugMode"/>, which runs the game's message loop until a command resumes.
/// </summary>
class MCAblDebugger
{
public:
    /// <param name="runtime">The runtime it debugs.</param>
    /// <param name="print">Where its lines go.</param>
    MCAblDebugger(MCAblRuntime& runtime, std::function<void(std::string_view)> print);

    /// <summary>Writes a line through the print callback.</summary>
    void Print(std::string_view text) const;

    /// <summary>Makes <paramref name="ablModule"/> the module being executed: its managers and debug modes.</summary>
    void SetModule(MCAblModule* ablModule);

    /// <summary>The module being executed.</summary>
    MCAblModule* Module() const { return _Module; }

    /// <summary>The module the debugger's commands apply to.</summary>
    MCAblModule* DebugModule() const { return _DebugModule; }

    /// <summary>Decompiles the statement being executed (its tokens and symbol names, each after a space).</summary>
    std::string StatementText() const;

    /// <summary>The value at <paramref name="data"/> as type <paramref name="dataType"/>.</summary>
    static std::string DataValueText(const MCAblStackItem* data, MCAblType* dataType);

    /// <summary>
    /// The value of variable expression <paramref name="exprString"/> (a name, maybe subscripted <c>[i][j]</c> or
    /// <c>[i,j]</c>) in the debug module.
    /// </summary>
    /// <returns>The text, or null when the name isn't in the symbol table.</returns>
    std::optional<std::string> ValueText(std::string_view exprString) const;

    /// <summary>Called before each statement: breaks on a break point or when stepping.</summary>
    void TraceStatementExecution();

    /// <summary>Called on entering a routine: reports it when tracing entries.</summary>
    void TraceRoutineEntry(MCAblSymbol* idPtr) const;

    /// <summary>Called on leaving a routine: reports it when tracing exits.</summary>
    void TraceRoutineExit(MCAblSymbol* idPtr) const;

    /// <summary>Called after a store to a variable: reports it if watched (and breaks if asked).</summary>
    void TraceDataStore(MCAblSymbol* id, MCAblType* idType, const MCAblStackItem* target, MCAblType* targetType);

    /// <summary>Called after a fetch of a variable: reports it if watched (and breaks if asked).</summary>
    void TraceDataFetch(MCAblSymbol* id, MCAblType* idType, const MCAblStackItem* data);

    /// <summary>Runs a debugger window command.</summary>
    void ProcessCommand(MCAblDebugCommand command, std::string_view text, int32_t number, MCAblModule* module);

    /// <summary>
    /// Shows the current statement and runs the game's message loop and display until a command resumes (or the
    /// window is closed).
    /// </summary>
    void DebugMode();

private:
    /// <summary>Prints the module instances, two per line.</summary>
    void DisplayModuleInstanceRegistry() const;

    /// <summary>The value of scalar symbol <paramref name="symbol"/>.</summary>
    std::string SimpleValueText(MCAblSymbol* symbol) const;

    /// <summary>An element of array <paramref name="symbol"/>; <paramref name="subscripts"/> is <c>[i][j]...</c>.</summary>
    /// <returns>The text, or null for a subscript out of range.</returns>
    std::optional<std::string> ArrayValueText(MCAblSymbol* symbol, std::string_view subscripts) const;

    /// <summary>Where variable <paramref name="idPtr"/> lives in the running frame, the statics or the eternals.</summary>
    MCAblStackItem* VariableItem(const MCAblSymbol* idPtr) const;

    MCAblRuntime& _Runtime;
    std::function<void(std::string_view)> _Print;
    MCAblModule* _Module = nullptr;
    MCAblModule* _DebugModule = nullptr;
    bool _DebugCommand = false;
    bool _Trace = false;
    bool _Step = false;
    bool _TraceEntry = false;
    bool _TraceExit = false;
};

/// <summary>The debugger of the current runtime, or null when ABL runs without one.</summary>
MCAblDebugger* AblGetDebugger();
