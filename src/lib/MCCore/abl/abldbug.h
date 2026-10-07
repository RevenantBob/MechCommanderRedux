#pragma once

// The ABL debugger: watches (report or break when a variable is stored or fetched), break points on source lines,
// statement / routine tracing and single-stepping, and the in-game debugger window (honorb.cpp opens it and routes
// its commands to Debugger::processCommand).

#include "abl/ablenv.h"

// Port note: DebuggerWindow and ScrollingTextWindow derive from the gui classes. Until the gui/ headers exist the
// two window classes are left out; once gui/asystem.h and gui/awindow.h are written, drop this guard.
#if __has_include("gui/asystem.h") && __has_include("gui/awindow.h")
#define ABL_DEBUGGER_WINDOWS 1
#include "gui/asystem.h"
#include "gui/awindow.h"
#else
#define ABL_DEBUGGER_WINDOWS 0
#endif

class MCGuiEvent;
class MCGuiTextObject;
class MCScrollingTextWindow;
class MCDebuggerWindow;

/// <summary>Size of Debugger::message.</summary>
inline constexpr int32_t MAXLEN_DEBUGGER_MESSAGE = 512;

/// <summary>A watch on a variable (_SymTableNode::info points to it).</summary>
/// <remarks>0x14 bytes in the original.</remarks>
struct MCWatch
{
    MCSymTableNodePtr IdPtr = nullptr;
    /// <summary>Report stores to it.</summary>
    int32_t Store = 0;
    /// <summary>Break into the debugger on a store.</summary>
    int32_t BreakOnStore = 0;
    /// <summary>Report fetches of it.</summary>
    int32_t Fetch = 0;
    /// <summary>Break into the debugger on a fetch.</summary>
    int32_t BreakOnFetch = 0;
};

typedef MCWatch MCWatch;
typedef MCWatch* MCWatchPtr;

/// <summary>A module's watches.</summary>
/// <remarks>Original source: <c>abl\abldbug.cpp</c>, 0xc bytes.</remarks>
class MCWatchManager
{
public:
    MCWatchManager()
    {
        MaxWatches = 0;
        Watches = nullptr;
    }

    /// <summary>Allocates room for <paramref name="max"/> watches.</summary>
    /// <returns>0, or -1 if out of memory.</returns>
    int32_t Init(int32_t max);

    void Destroy();

    /// <summary>The watch on <paramref name="idPtr"/>, new if it has none (constants, variables and parameters only).</summary>
    /// <returns>The watch, or null if the symbol can't be watched or the table is full.</returns>
    MCWatchPtr Add(MCSymTableNodePtr idPtr);

    /// <summary>Removes the watch on <paramref name="idPtr"/>.</summary>
    /// <returns>0, 1 (no symbol) or 2 (not watched).</returns>
    int32_t Remove(MCSymTableNodePtr idPtr);

    /// <summary>Removes every watch.</summary>
    /// <returns>How many there were.</returns>
    int32_t RemoveAll();

    /// <summary>Turns reporting of stores to <paramref name="idPtr"/> on or off (<paramref name="breakOnStore"/> to also break).</summary>
    /// <returns>0, 1 (no symbol) or 2 (table full).</returns>
    int32_t SetStore(MCSymTableNodePtr idPtr, int on, int breakOnStore = 0);

    /// <summary>Turns reporting of fetches of <paramref name="idPtr"/> on or off (<paramref name="breakOnFetch"/> to also break).</summary>
    /// <returns>0, 1 (no symbol) or 2 (table full).</returns>
    int32_t SetFetch(MCSymTableNodePtr idPtr, int on, int breakOnFetch = 0);

    /// <summary>Whether stores to <paramref name="idPtr"/> are reported.</summary>
    int32_t GetStore(MCSymTableNodePtr idPtr);

    /// <summary>Whether fetches of <paramref name="idPtr"/> are reported.</summary>
    int32_t GetFetch(MCSymTableNodePtr idPtr);

    /// <summary>Lists the watches (empty in MCX.EXE).</summary>
    void Print();

    int32_t MaxWatches = 0;
    int32_t NumWatches = 0;
    MCWatchPtr Watches = nullptr;
};

typedef MCWatchManager* MCWatchManagerPtr;

/// <summary>A module's break points: source line numbers, kept sorted.</summary>
/// <remarks>Original source: <c>abl\abldbug.cpp</c>, 0xc bytes.</remarks>
class MCBreakPointManager
{
public:
    MCBreakPointManager()
    {
        MaxBreakPoints = 0;
        NumBreakPoints = 0;
        BreakPoints = nullptr;
    }

    /// <summary>Allocates room for <paramref name="max"/> break points.</summary>
    /// <returns>0, or -1 if out of memory.</returns>
    int32_t Init(int32_t max);

    void Destroy();

    /// <summary>Adds a break point at <paramref name="lineNumber"/>.</summary>
    /// <returns>0, 1 (full) or 2 (bad line).</returns>
    int32_t Add(int32_t lineNumber);

    /// <summary>Removes the break point at <paramref name="lineNumber"/>.</summary>
    int32_t Remove(int32_t lineNumber);

    /// <summary>Removes every break point.</summary>
    /// <returns>How many there were.</returns>
    int32_t RemoveAll();

    /// <summary>Whether there is a break point at <paramref name="lineNumber"/>.</summary>
    int IsBreakPoint(int32_t lineNumber);

    /// <summary>Lists the break points (empty in MCX.EXE).</summary>
    void Print();

    int32_t MaxBreakPoints = 0;
    int32_t NumBreakPoints = 0;
    int32_t* BreakPoints = nullptr;
};

typedef MCBreakPointManager* MCBreakPointManagerPtr;

/// <summary>
/// The ABL debugger: traces execution of the current module, reports watched variables and breaks into debugMode,
/// which runs the game's message loop until a command resumes.
/// </summary>
/// <remarks>Original source: <c>abl\abldbug.cpp</c>, 0x30 bytes.</remarks>
class MCDebugger
{
public:
    /// <summary>A zeroed debugger.</summary>
    /// <remarks>Inline in the original (ABLi_init).</remarks>
    MCDebugger()
    {
        Module = nullptr;
        WatchManager = nullptr;
        BreakPointManager = nullptr;
        DebugModule = nullptr;
        Enabled = 0;
        DebugCommand = 0;
        Halt = 0;
        Trace = 0;
        Step = 0;
        TraceEntry = 0;
        TraceExit = 0;
        PrintCallback = nullptr;
    }

    /// <summary>Sets the output routine and the module to debug.</summary>
    /// <returns>0.</returns>
    int32_t Init(void (*callback)(char* s), MCAblModule* ablModule);

    /// <summary>Does nothing in MCX.EXE.</summary>
    void Destroy();

    /// <summary>Writes a line through the print callback.</summary>
    /// <returns>0.</returns>
    int32_t Print(char* s);

    /// <summary>Makes <paramref name="ablModule"/> the module being executed: its managers and debug modes.</summary>
    void SetModule(MCAblModule* ablModule);

    /// <summary>Parses a variable from the command line and sets its watch by <paramref name="states"/> (bit flags).</summary>
    int32_t SetWatch(int32_t states);

    /// <summary>Parses a line number and adds a break point.</summary>
    int32_t AddBreakPoint();

    /// <summary>Parses a line number and removes its break point (or all of them).</summary>
    int32_t RemoveBreakPoint();

    /// <summary>Decompiles the statement at statementStartPtr into <paramref name="dest"/>.</summary>
    void SprintStatement(char* dest);

    /// <summary>Writes the current line label.</summary>
    void SprintLineNumber(char* dest);

    /// <summary>Writes the value at <paramref name="data"/> as type <paramref name="dataType"/>.</summary>
    void SprintDataValue(char* dest, MCStackItemPtr data, MCTypePtr dataType);

    /// <summary>Writes the value of scalar symbol <paramref name="symbol"/>.</summary>
    int32_t SprintSimpleValue(char* dest, MCSymTableNodePtr symbol);

    /// <summary>Writes an element of array <paramref name="symbol"/>; <paramref name="subscriptString"/> is <c>[i][j]...</c>.</summary>
    /// <returns>0, or 1 for a subscript out of range.</returns>
    int32_t SprintArrayValue(char* dest, MCSymTableNodePtr symbol, char* subscriptString);

    /// <summary>Writes the value of variable expression <paramref name="exprString"/> (a name, maybe subscripted).</summary>
    /// <returns>0, or 1 when the name isn't in the symbol table.</returns>
    int32_t SprintValue(char* dest, char* exprString);

    /// <summary>Called before each statement: breaks on a break point or when stepping.</summary>
    int32_t TraceStatementExecution();

    /// <summary>Called on entering a routine: reports it when tracing entries.</summary>
    int32_t TraceRoutineEntry(MCSymTableNodePtr idPtr);

    /// <summary>Called on leaving a routine: reports it when tracing exits.</summary>
    int32_t TraceRoutineExit(MCSymTableNodePtr idPtr);

    /// <summary>Called after a store to a variable: reports it if watched (and breaks if asked).</summary>
    int32_t TraceDataStore(MCSymTableNodePtr id, MCTypePtr idType, MCStackItemPtr target, MCTypePtr targetType);

    /// <summary>Called after a fetch of a variable: reports it if watched (and breaks if asked).</summary>
    int32_t TraceDataFetch(MCSymTableNodePtr id, MCTypePtr idType, MCStackItemPtr data);

    /// <summary>Compiles and runs an expression from the command line and prints its value.</summary>
    void ShowValue();

    /// <summary>Unfinished in MCX.EXE: reads a token and stops.</summary>
    void AssignVariable();

    /// <summary>Prints the module instances, two per line.</summary>
    void DisplayModuleInstanceRegistry(int32_t numCols);

    /// <summary>
    /// Runs a debugger window command: 0 select module, 1 trace, 2 step, 3/4 add/remove break point, 5 watch, 6 clear
    /// watches, 7 print a value, 8 resume, 9 help, 10 module info.
    /// </summary>
    void ProcessCommand(int32_t commandId, char* strParam1, int32_t numParam1, MCAblModule* moduleParam1);

    /// <summary>
    /// Shows the current statement and runs the game's message loop and display until a command resumes (or the
    /// window is closed).
    /// </summary>
    void DebugMode();

    /// <summary>The module being executed.</summary>
    MCAblModule* Module = nullptr;
    MCWatchManagerPtr WatchManager = nullptr;
    MCBreakPointManagerPtr BreakPointManager = nullptr;
    /// <summary>The module the debugger's commands apply to.</summary>
    MCAblModule* DebugModule = nullptr;
    /// <summary>Never set in MCX.EXE.</summary>
    int32_t Enabled = 0;
    /// <summary>Nonzero while debugMode waits for a command.</summary>
    int32_t DebugCommand = 0;
    int32_t Halt = 0;
    int32_t Trace = 0;
    int32_t Step = 0;
    int32_t TraceEntry = 0;
    int32_t TraceExit = 0;
    void (*PrintCallback)(char* s) = nullptr;

    /// <summary>The line being built for print.</summary>
    static char Message[MAXLEN_DEBUGGER_MESSAGE];
};

typedef MCDebugger* MCDebuggerPtr;

#if ABL_DEBUGGER_WINDOWS

/// <summary>The debugger's output pane: lines of text scrolling up in a VFX pane.</summary>
/// <remarks>Original source: <c>abl\abldbug.cpp</c>, 0x4b4 bytes.</remarks>
class MCScrollingTextWindow : public MCGuiObject
{
public:
    /// <summary>Inline in the original (DebuggerWindow::init).</summary>
    MCScrollingTextWindow() { Clear(); }

    ~MCScrollingTextWindow() override;

    /// <summary>Creates the pane and sizes the text grid (10-pixel cells).</summary>
    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) override;

    void Resize(int32_t width, int32_t height) override;

    void Draw() override;

    void HandleEvent(MCGuiEvent* event) override;

    /// <summary>Zeroes the text grid size.</summary>
    virtual void Clear();

    /// <summary>Scrolls up a line and writes <paramref name="s"/> at the bottom.</summary>
    virtual void Print(char* s);

    /// <summary>Columns and lines of 10-pixel cells.</summary>
    int32_t NumColumns = 0;
    int32_t NumLines = 0;
};

/// <summary>The ABL debugger window: the output pane over a one-line command box.</summary>
/// <remarks>Original source: <c>abl\abldbug.cpp</c> (created by honorb.cpp), 0x4c0 bytes (no fields of its own).</remarks>
class MCDebuggerWindow : public MCGuiTitleWindow
{
public:
    /// <summary>Port: its output window scrolls its picture, so it keeps one.</summary>
    bool DrawsLive() override { return false; }
    ~MCDebuggerWindow() override;

    /// <summary>Creates the window, ABLDebuggerOut and ABLDebuggerIn.</summary>
    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) override;

    /// <summary>Destroys ABLDebuggerIn and ABLDebuggerOut, then the window.</summary>
    void Destroy() override;

    /// <summary>Resizes the output pane and moves the command box to the bottom.</summary>
    void Resize(int32_t width, int32_t height) override;
};

#endif

/// <summary>The debugger, or null when ABL runs without one.</summary>
extern MCDebugger* Debugger;
/// <summary>The debugger window's output pane and command box.</summary>
extern MCScrollingTextWindow* AblDebuggerOut;
extern MCGuiTextObject* AblDebuggerIn;
/// <summary>Text of each token, for decompiling statements.</summary>
extern const char* TokenStrings[NUM_TOKENS];

/// <summary>The debugger.</summary>
MCDebugger* AblGetDebugger();
