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

class aEvent;
class aTextObject;
class ScrollingTextWindow;
class DebuggerWindow;

/// <summary>Size of Debugger::message.</summary>
inline constexpr int32_t MAXLEN_DEBUGGER_MESSAGE = 512;

/// <summary>A watch on a variable (_SymTableNode::info points to it).</summary>
/// <remarks>0x14 bytes in the original.</remarks>
struct _Watch
{
    SymTableNodePtr idPtr; // +0x0
    /// <summary>Report stores to it.</summary>
    int32_t store; // +0x4
    /// <summary>Break into the debugger on a store.</summary>
    int32_t breakOnStore; // +0x8
    /// <summary>Report fetches of it.</summary>
    int32_t fetch; // +0xc
    /// <summary>Break into the debugger on a fetch.</summary>
    int32_t breakOnFetch; // +0x10
};

typedef _Watch Watch;
typedef _Watch* WatchPtr;

/// <summary>A module's watches.</summary>
/// <remarks>Original source: <c>abl\abldbug.cpp</c>, 0xc bytes, allocated from AblStackHeap.</remarks>
class WatchManager
{
public:
    WatchManager()
    {
        maxWatches = 0;
        watches = nullptr;
    }

    /// <remarks>MCX.EXE @ 0x0061f430</remarks>
    static void* operator new(size_t mySize) noexcept;
    /// <remarks>MCX.EXE @ 0x0061f450</remarks>
    static void operator delete(void* us);

    /// <summary>Allocates room for <paramref name="max"/> watches.</summary>
    /// <returns>0, or -1 if out of memory.</returns>
    /// <remarks>MCX.EXE @ 0x0061f470</remarks>
    int32_t init(int32_t max);

    /// <remarks>MCX.EXE @ 0x0061f4b0</remarks>
    void destroy();

    /// <summary>The watch on <paramref name="idPtr"/>, new if it has none (constants, variables and parameters only).</summary>
    /// <returns>The watch, or null if the symbol can't be watched or the table is full.</returns>
    /// <remarks>MCX.EXE @ 0x0061f4e0</remarks>
    WatchPtr add(SymTableNodePtr idPtr);

    /// <summary>Removes the watch on <paramref name="idPtr"/>.</summary>
    /// <returns>0, 1 (no symbol) or 2 (not watched).</returns>
    /// <remarks>MCX.EXE @ 0x0061f570</remarks>
    int32_t remove(SymTableNodePtr idPtr);

    /// <summary>Removes every watch.</summary>
    /// <returns>How many there were.</returns>
    /// <remarks>MCX.EXE @ 0x0061f610</remarks>
    int32_t removeAll();

    /// <summary>Turns reporting of stores to <paramref name="idPtr"/> on or off (<paramref name="breakOnStore"/> to also break).</summary>
    /// <returns>0, 1 (no symbol) or 2 (table full).</returns>
    /// <remarks>MCX.EXE @ 0x0061f650</remarks>
    int32_t setStore(SymTableNodePtr idPtr, int on, int breakOnStore = 0);

    /// <summary>Turns reporting of fetches of <paramref name="idPtr"/> on or off (<paramref name="breakOnFetch"/> to also break).</summary>
    /// <returns>0, 1 (no symbol) or 2 (table full).</returns>
    /// <remarks>MCX.EXE @ 0x0061f6d0</remarks>
    int32_t setFetch(SymTableNodePtr idPtr, int on, int breakOnFetch = 0);

    /// <summary>Whether stores to <paramref name="idPtr"/> are reported.</summary>
    /// <remarks>MCX.EXE @ 0x0061f750</remarks>
    int32_t getStore(SymTableNodePtr idPtr);

    /// <summary>Whether fetches of <paramref name="idPtr"/> are reported.</summary>
    /// <remarks>MCX.EXE @ 0x0061f770</remarks>
    int32_t getFetch(SymTableNodePtr idPtr);

    /// <summary>Lists the watches (empty in MCX.EXE).</summary>
    /// <remarks>MCX.EXE @ 0x0061f790</remarks>
    void print();

    int32_t maxWatches;     // +0x0
    int32_t numWatches = 0; // +0x4
    WatchPtr watches;       // +0x8
};

typedef WatchManager* WatchManagerPtr;

/// <summary>A module's break points: source line numbers, kept sorted.</summary>
/// <remarks>Original source: <c>abl\abldbug.cpp</c>, 0xc bytes, allocated from AblStackHeap.</remarks>
class BreakPointManager
{
public:
    BreakPointManager()
    {
        maxBreakPoints = 0;
        numBreakPoints = 0;
        breakPoints = nullptr;
    }

    /// <remarks>MCX.EXE @ 0x0061f7a0</remarks>
    static void* operator new(size_t mySize) noexcept;
    /// <remarks>MCX.EXE @ 0x0061f7c0</remarks>
    static void operator delete(void* us);

    /// <summary>Allocates room for <paramref name="max"/> break points.</summary>
    /// <returns>0, or -1 if out of memory.</returns>
    /// <remarks>MCX.EXE @ 0x0061f7e0</remarks>
    int32_t init(int32_t max);

    /// <remarks>MCX.EXE @ 0x0061f810</remarks>
    void destroy();

    /// <summary>Adds a break point at <paramref name="lineNumber"/>.</summary>
    /// <returns>0, 1 (full) or 2 (bad line).</returns>
    /// <remarks>MCX.EXE @ 0x0061f840</remarks>
    int32_t add(int32_t lineNumber);

    /// <summary>Removes the break point at <paramref name="lineNumber"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0061f8c0</remarks>
    int32_t remove(int32_t lineNumber);

    /// <summary>Removes every break point.</summary>
    /// <returns>How many there were.</returns>
    /// <remarks>MCX.EXE @ 0x0061f910</remarks>
    int32_t removeAll();

    /// <summary>Whether there is a break point at <paramref name="lineNumber"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0061f920</remarks>
    int isBreakPoint(int32_t lineNumber);

    /// <summary>Lists the break points (empty in MCX.EXE).</summary>
    /// <remarks>MCX.EXE @ 0x0061f960 (unnamed in the symbols; called where the watch list is printed)</remarks>
    void print();

    int32_t maxBreakPoints; // +0x0
    int32_t numBreakPoints; // +0x4
    int32_t* breakPoints;   // +0x8
};

typedef BreakPointManager* BreakPointManagerPtr;

/// <summary>
/// The ABL debugger: traces execution of the current module, reports watched variables and breaks into debugMode,
/// which runs the game's message loop until a command resumes.
/// </summary>
/// <remarks>Original source: <c>abl\abldbug.cpp</c>, 0x30 bytes, allocated from AblStackHeap (ABLi_init).</remarks>
class Debugger
{
public:
    /// <summary>A zeroed debugger.</summary>
    /// <remarks>Inline in the original (ABLi_init).</remarks>
    Debugger()
    {
        module = nullptr;
        watchManager = nullptr;
        breakPointManager = nullptr;
        debugModule = nullptr;
        enabled = 0;
        debugCommand = 0;
        halt = 0;
        trace = 0;
        step = 0;
        traceEntry = 0;
        traceExit = 0;
        printCallback = nullptr;
    }

    /// <remarks>MCX.EXE @ 0x0061f970</remarks>
    static void* operator new(size_t mySize) noexcept;
    /// <remarks>MCX.EXE @ 0x0061f990</remarks>
    static void operator delete(void* us);

    /// <summary>Sets the output routine and the module to debug.</summary>
    /// <returns>0.</returns>
    /// <remarks>MCX.EXE @ 0x0061f9b0</remarks>
    int32_t init(void (*callback)(char* s), ABLModule* _module);

    /// <summary>Does nothing in MCX.EXE.</summary>
    /// <remarks>MCX.EXE @ 0x0061f9e0</remarks>
    void destroy();

    /// <summary>Writes a line through the print callback.</summary>
    /// <returns>0.</returns>
    /// <remarks>MCX.EXE @ 0x0061f9f0</remarks>
    int32_t print(char* s);

    /// <summary>Makes <paramref name="_module"/> the module being executed: its managers and debug modes.</summary>
    /// <remarks>MCX.EXE @ 0x0061fa10</remarks>
    void setModule(ABLModule* _module);

    /// <summary>Parses a variable from the command line and sets its watch by <paramref name="states"/> (bit flags).</summary>
    /// <remarks>MCX.EXE @ 0x0061fa40</remarks>
    int32_t setWatch(int32_t states);

    /// <summary>Parses a line number and adds a break point.</summary>
    /// <remarks>MCX.EXE @ 0x0061fb10</remarks>
    int32_t addBreakPoint();

    /// <summary>Parses a line number and removes its break point (or all of them).</summary>
    /// <remarks>MCX.EXE @ 0x0061fb60</remarks>
    int32_t removeBreakPoint();

    /// <summary>Decompiles the statement at statementStartPtr into <paramref name="dest"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0061fbb0</remarks>
    void sprintStatement(char* dest);

    /// <summary>Writes the current line label.</summary>
    /// <remarks>MCX.EXE @ 0x0061fd80</remarks>
    void sprintLineNumber(char* dest);

    /// <summary>Writes the value at <paramref name="data"/> as type <paramref name="dataType"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0061fda0</remarks>
    void sprintDataValue(char* dest, StackItemPtr data, TypePtr dataType);

    /// <summary>Writes the value of scalar symbol <paramref name="symbol"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0061fea0</remarks>
    int32_t sprintSimpleValue(char* dest, SymTableNodePtr symbol);

    /// <summary>Writes an element of array <paramref name="symbol"/>; <paramref name="subscriptString"/> is <c>[i][j]...</c>.</summary>
    /// <returns>0, or 1 for a subscript out of range.</returns>
    /// <remarks>MCX.EXE @ 0x00620030</remarks>
    int32_t sprintArrayValue(char* dest, SymTableNodePtr symbol, char* subscriptString);

    /// <summary>Writes the value of variable expression <paramref name="exprString"/> (a name, maybe subscripted).</summary>
    /// <returns>0, or 1 for an unknown name.</returns>
    /// <remarks>MCX.EXE @ 0x006201d0</remarks>
    int32_t sprintValue(char* dest, char* exprString);

    /// <summary>Called before each statement: breaks on a break point or when stepping.</summary>
    /// <remarks>MCX.EXE @ 0x006202f0</remarks>
    int32_t traceStatementExecution();

    /// <summary>Called on entering a routine: reports it when tracing entries.</summary>
    /// <remarks>MCX.EXE @ 0x00620360</remarks>
    int32_t traceRoutineEntry(SymTableNodePtr idPtr);

    /// <summary>Called on leaving a routine: reports it when tracing exits.</summary>
    /// <remarks>MCX.EXE @ 0x006203b0</remarks>
    int32_t traceRoutineExit(SymTableNodePtr idPtr);

    /// <summary>Called after a store to a variable: reports it if watched (and breaks if asked).</summary>
    /// <remarks>MCX.EXE @ 0x00620400 (unnamed in the symbols)</remarks>
    int32_t traceDataStore(SymTableNodePtr id, TypePtr idType, StackItemPtr target, TypePtr targetType);

    /// <summary>Called after a fetch of a variable: reports it if watched (and breaks if asked).</summary>
    /// <remarks>MCX.EXE @ 0x006204a0 (unnamed in the symbols)</remarks>
    int32_t traceDataFetch(SymTableNodePtr id, TypePtr idType, StackItemPtr data);

    /// <summary>Compiles and runs an expression from the command line and prints its value.</summary>
    /// <remarks>MCX.EXE @ 0x00620540</remarks>
    void showValue();

    /// <summary>Unfinished in MCX.EXE: reads a token and stops.</summary>
    /// <remarks>MCX.EXE @ 0x00620620</remarks>
    void assignVariable();

    /// <summary>Prints the module instances, two per line.</summary>
    /// <remarks>MCX.EXE @ 0x00620630</remarks>
    void displayModuleInstanceRegistry(int32_t numCols);

    /// <summary>
    /// Runs a debugger window command: 0 select module, 1 trace, 2 step, 3/4 add/remove break point, 5 watch, 6 clear
    /// watches, 7 print a value, 8 resume, 9 help, 10 module info.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00620720</remarks>
    void processCommand(int32_t commandId, char* strParam1, int32_t numParam1, ABLModule* moduleParam1);

    /// <summary>
    /// Shows the current statement and runs the game's message loop and display until a command resumes (or the
    /// window is closed).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00620c50</remarks>
    void debugMode();

    /// <summary>The module being executed.</summary>
    ABLModule* module;                      // +0x0
    WatchManagerPtr watchManager;           // +0x4
    BreakPointManagerPtr breakPointManager; // +0x8
    /// <summary>The module the debugger's commands apply to.</summary>
    ABLModule* debugModule; // +0xc
    /// <summary>Never set in MCX.EXE.</summary>
    int32_t enabled; // +0x10
    /// <summary>Nonzero while debugMode waits for a command.</summary>
    int32_t debugCommand;           // +0x14
    int32_t halt;                   // +0x18
    int32_t trace;                  // +0x1c
    int32_t step;                   // +0x20
    int32_t traceEntry;             // +0x24
    int32_t traceExit;              // +0x28
    void (*printCallback)(char* s); // +0x2c

    /// <summary>The line being built for print.</summary>
    static char message[MAXLEN_DEBUGGER_MESSAGE];
};

typedef Debugger* DebuggerPtr;

#if ABL_DEBUGGER_WINDOWS

/// <summary>The debugger's output pane: lines of text scrolling up in a VFX pane.</summary>
/// <remarks>Original source: <c>abl\abldbug.cpp</c>, 0x4b4 bytes.</remarks>
class ScrollingTextWindow : public aObject
{
public:
    /// <summary>Inline in the original (DebuggerWindow::init).</summary>
    ScrollingTextWindow() { clear(); }

    /// <remarks>MCX.EXE @ 0x00620ef0 (the deleting destructor; the body is aObject::destroy)</remarks>
    ~ScrollingTextWindow() override;

    /// <summary>Creates the pane and sizes the text grid (10-pixel cells).</summary>
    /// <remarks>MCX.EXE @ 0x00620ff0</remarks>
    int32_t init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) override;

    /// <remarks>MCX.EXE @ 0x00621090</remarks>
    void resize(int32_t width, int32_t height) override;

    /// <remarks>MCX.EXE @ 0x00621160</remarks>
    void draw() override;

    /// <remarks>MCX.EXE @ 0x00621080</remarks>
    void handleEvent(aEvent* event) override;

    /// <summary>Zeroes the text grid size.</summary>
    /// <remarks>MCX.EXE @ 0x00620fe0 (vtable slot 77; unnamed in the symbols)</remarks>
    virtual void clear();

    /// <summary>Scrolls up a line and writes <paramref name="s"/> at the bottom.</summary>
    /// <remarks>MCX.EXE @ 0x00621100 (vtable slot 78)</remarks>
    virtual void print(char* s);

    /// <summary>Columns and lines of 10-pixel cells.</summary>
    int32_t numColumns; // +0x4ac
    int32_t numLines;   // +0x4b0
};

/// <summary>The ABL debugger window: the output pane over a one-line command box.</summary>
/// <remarks>Original source: <c>abl\abldbug.cpp</c> (created by honorb.cpp), 0x4c0 bytes (no fields of its own).</remarks>
class DebuggerWindow : public aTitleWindow
{
public:
    /// <summary>Port: its output window scrolls its picture, so it keeps one.</summary>
    bool DrawsLive() override { return false; }
    /// <remarks>MCX.EXE @ 0x0075a850 (the deleting destructor)</remarks>
    ~DebuggerWindow() override;

    /// <summary>Creates the window, ABLDebuggerOut and ABLDebuggerIn.</summary>
    /// <remarks>MCX.EXE @ 0x00620da0</remarks>
    int32_t init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) override;

    /// <summary>Destroys ABLDebuggerIn and ABLDebuggerOut, then the window.</summary>
    /// <remarks>MCX.EXE @ 0x00620f20</remarks>
    void destroy() override;

    /// <summary>Resizes the output pane and moves the command box to the bottom.</summary>
    /// <remarks>MCX.EXE @ 0x00620f80</remarks>
    void resize(int32_t width, int32_t height) override;
};

#endif

/// <summary>The debugger, or null when ABL runs without one.</summary>
extern Debugger* debugger;
/// <summary>The debugger window's output pane and command box.</summary>
extern ScrollingTextWindow* ABLDebuggerOut;
extern aTextObject* ABLDebuggerIn;
/// <summary>Text of each token, for decompiling statements.</summary>
extern const char* TokenStrings[NUM_TOKENS];

/// <summary>The debugger.</summary>
/// <remarks>MCX.EXE @ 0x00620d90</remarks>
Debugger* ABLi_getDebugger();
