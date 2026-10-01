#pragma once

// ABL environment: the registry of compiled modules and loaded libraries, and ABLModule, one running instance of a
// compiled module (a unit brain, a mission script) with its own static data. Also the execution profile log.

#include "abl/ablexec.h"

class File;
class WatchManager;
class BreakPointManager;

/// <summary>Longest module instance name (ABLModule::name, with its terminator).</summary>
inline constexpr int32_t MAX_ABLMODULE_NAME = 26;
/// <summary>Lines the profile log buffers before writing them out.</summary>
inline constexpr int32_t MAX_PROFILE_LOG_LINES = 256;
/// <summary>Length of a profile log line.</summary>
inline constexpr int32_t MAX_PROFILE_LOG_LINE_LENGTH = 128;

/// <summary>What an ABLParam holds.</summary>
enum ABLParamType : uint8_t
{
    ABL_PARAM_VOID = 0,
    ABL_PARAM_INTEGER = 1,
    ABL_PARAM_REAL = 2
};

/// <summary>
/// A parameter passed from C++ to an ABL module or function (ABLi_createParamList). Value parameters are copied
/// onto the ABL stack; reference parameters are passed as the address of <c>integer</c> or <c>real</c>, so ABL code
/// can write back into the list.
/// </summary>
/// <remarks>0xc bytes in the original.</remarks>
struct ABLParam
{
    /// <summary>An ABLParamType.</summary>
    char type;       // +0x0
    int32_t integer; // +0x4
    float real;      // +0x8
};

/// <summary>A compiled module (ModuleRegistry): its code and what its instances need.</summary>
/// <remarks>0x28 bytes in the original.</remarks>
struct ModuleEntry
{
    /// <summary>The main source file's name.</summary>
    char* fileName; // +0x0
    /// <summary>The module symbol: its code, parameters and symbol tree.</summary>
    SymTableNodePtr moduleIdPtr; // +0x4
    int32_t numSourceFiles;      // +0x8
    /// <summary>Every source file compiled into it (the index is a statement marker's file number).</summary>
    char** sourceFiles;        // +0xc
    int32_t numLibrariesUsed;  // +0x10
    ABLModule** librariesUsed; // +0x14
    int32_t numStaticVars;     // +0x18
    /// <summary>Per static variable, the bytes of its array block, or 0 for a scalar.</summary>
    int32_t* sizeStaticVars; // +0x1c
    /// <summary>
    /// numStaticVars * 4 plus the array blocks (bookkeeping for the debugger; it counts the original's 4-byte slots).
    /// </summary>
    int32_t totalSizeStaticVars; // +0x20
    int32_t numInstances;        // +0x24
};

/// <summary>
/// A running instance of a compiled module: a unit's brain, a mission script, or a library. Each instance has its
/// own static variables; the code and symbols are shared through ModuleRegistry.
/// </summary>
/// <remarks>
/// Original source: <c>abl\ablenv.cpp</c>, 0x48 bytes, allocated through its own operator new (systemHeap). Other
/// classes (MechWarrior, GeneralOrder, Scenario) read its fields directly.
/// </remarks>
class ABLModule
{
public:
    /// <summary>An empty module: no id, no handle, no static data.</summary>
    /// <remarks>Inline in the original (GeneralOrder::init, MechWarrior::setBrain, ABLi_loadLibrary).</remarks>
    ABLModule()
    {
        id = -1;
        name[0] = '\0';
        handle = -1;
        staticData = nullptr;
        initCalled = 0;
        watchManager = nullptr;
        breakPointManager = nullptr;
        trace = 0;
        step = 0;
        traceEntry = 0;
        traceExit = 0;
    }

    /// <summary>Allocates from systemHeap (null if it is gone).</summary>
    /// <remarks>MCX.EXE @ 0x00622260 (unnamed in the symbols)</remarks>
    static void* operator new(size_t mySize) noexcept;
    /// <summary>Frees to systemHeap, or the C heap once systemHeap is gone.</summary>
    /// <remarks>MCX.EXE @ 0x00622290 (unnamed in the symbols)</remarks>
    static void operator delete(void* us);

    /// <summary>
    /// Makes this an instance of registered module <paramref name="moduleHandle"/>: allocates its static data (and
    /// its static arrays), registers it in ModuleInstanceRegistry, and with the debugger on, its watch and break-point
    /// managers.
    /// </summary>
    /// <returns>0.</returns>
    /// <remarks>MCX.EXE @ 0x006222c0</remarks>
    int32_t init(int32_t moduleHandle);

    /// <summary>Names the instance (at most 25 characters).</summary>
    /// <remarks>MCX.EXE @ 0x006224a0</remarks>
    void setName(char* _name);

    /// <summary>Runs the module's main code with <paramref name="paramList"/> for its parameters.</summary>
    /// <returns>The number of statements executed (0 if a parameter doesn't match).</returns>
    /// <remarks>MCX.EXE @ 0x006224d0</remarks>
    int32_t execute(ABLParam* paramList);

    /// <summary>Runs only function <paramref name="function"/> of the module, in the module's frame.</summary>
    /// <returns>The number of statements executed.</returns>
    /// <remarks>MCX.EXE @ 0x006226f0</remarks>
    int32_t execute(ABLParam* moduleParamList, SymTableNodePtr function, ABLParam* functionParamList);

    /// <summary>
    /// Finds a symbol: in <paramref name="function"/>'s scope, then the module's, then (with
    /// <paramref name="searchLibraries"/>) the libraries it uses. Lower-cases <paramref name="symbolName"/> in place.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00622910</remarks>
    SymTableNodePtr findSymbol(char* symbolName, SymTableNodePtr function = nullptr, int searchLibraries = 0);

    /// <summary>Finds a function of the module (or, with <paramref name="searchLibraries"/>, of its libraries).</summary>
    /// <remarks>MCX.EXE @ 0x006229c0</remarks>
    SymTableNodePtr findFunction(char* functionName, int searchLibraries = 0);

    /// <summary>Sets static integer <paramref name="staticName"/>.</summary>
    /// <returns>0, 1 (no such symbol), 2 (not an integer) or 3 (not static).</returns>
    /// <remarks>MCX.EXE @ 0x00622a50</remarks>
    int32_t setStaticInteger(char* staticName, int32_t value);

    /// <summary>Sets static real <paramref name="staticName"/>.</summary>
    /// <returns>0, 1 (no such symbol), 2 (not a real) or 3 (not static).</returns>
    /// <remarks>MCX.EXE @ 0x00622ac0</remarks>
    int32_t setStaticReal(char* staticName, float value);

    /// <summary>Copies <paramref name="size"/> integers into static array <paramref name="staticName"/>.</summary>
    /// <returns>0, 1 (no such symbol) or 3 (not static).</returns>
    /// <remarks>MCX.EXE @ 0x00622b30</remarks>
    int32_t setStaticIntegerArray(char* staticName, int32_t size, int32_t* values);

    /// <summary>Copies <paramref name="size"/> reals into static array <paramref name="staticName"/>.</summary>
    /// <returns>0, 1 (no such symbol) or 3 (not static).</returns>
    /// <remarks>MCX.EXE @ 0x00622b80</remarks>
    int32_t setStaticRealArray(char* staticName, int32_t size, float* values);

    /// <summary>Name of source file <paramref name="fileNumber"/> of the module.</summary>
    /// <remarks>MCX.EXE @ 0x00622bd0</remarks>
    char* getSourceFile(int32_t fileNumber);

    /// <summary>Copies the folder of source file <paramref name="fileNumber"/> (with its backslash) to <paramref name="directory"/>.</summary>
    /// <returns><paramref name="directory"/>, or null if the name has no folder.</returns>
    /// <remarks>MCX.EXE @ 0x00622bf0</remarks>
    char* getSourceDirectory(int32_t fileNumber, char* directory);

    /// <summary>The module's static variable count and total size, and optionally each one's array size.</summary>
    /// <remarks>MCX.EXE @ 0x00622c70</remarks>
    void getInfo(int32_t& numStatics, int32_t& staticsSize, int32_t* sizeList);

    /// <summary>Unregisters the instance and frees its static data and debugger managers.</summary>
    /// <remarks>MCX.EXE @ 0x00622cb0</remarks>
    void destroy();

    int32_t getId() const { return id; }
    int32_t getHandle() const { return handle; }
    char* getName() { return name; }
    int32_t getReturnValue() const { return returnVal; }

    /// <summary>Instance number (the order of init calls), or -1.</summary>
    int32_t id;                    // +0x0
    char name[MAX_ABLMODULE_NAME]; // +0x4
    /// <summary>Index of its compiled module in ModuleRegistry, or -1.</summary>
    int32_t handle; // +0x20
    /// <summary>The instance's static variables, one StackItem each (arrays as pointers to their blocks).</summary>
    StackItemPtr staticData; // +0x24
    /// <summary>The integer the last execution returned.</summary>
    int32_t returnVal; // +0x28
    /// <summary>Nonzero once the module's <c>init</c> function has run.</summary>
    int32_t initCalled;                   // +0x2c
    WatchManager* watchManager;           // +0x30
    BreakPointManager* breakPointManager; // +0x34
    /// <summary>Debugger modes for this instance (copied into the Debugger by setModule).</summary>
    int32_t trace;      // +0x38
    int32_t step;       // +0x3c
    int32_t traceEntry; // +0x40
    int32_t traceExit;  // +0x44
};

typedef ABLModule* ABLModulePtr;

/// <summary>Per-module watch and break-point limits (from ABLi_init).</summary>
extern int32_t MaxWatchesPerModule;
extern int32_t MaxBreakPointsPerModule;
/// <summary>The compiled modules; a module's handle indexes it.</summary>
extern ModuleEntry* ModuleRegistry;
extern int32_t MaxModules;
extern int32_t NumModulesRegistered;
/// <summary>Instances made so far (the next instance id).</summary>
extern int32_t NumModules;
/// <summary>The live instances.</summary>
extern ABLModule** ModuleInstanceRegistry;
extern int32_t NumModuleInstances;
/// <summary>The loaded libraries.</summary>
extern ABLModule** LibraryInstanceRegistry;
extern int32_t MaxLibraries;
extern int32_t numLibrariesLoaded;
/// <summary>The instance executing, and its handle.</summary>
extern ABLModule* CurModule;
extern int32_t CurModuleHandle;
/// <summary>The library being compiled (ABLi_loadLibrary), or null for a module.</summary>
extern ABLModule* CurLibrary;
/// <summary>Nesting depth of declared-routine calls.</summary>
extern int32_t CallStackLevel;
/// <summary>Nonzero when the executing module's <c>init</c> must run first.</summary>
extern int CallModuleInit;
/// <summary>Stack items taken by eternal variables (the bottom of the stack).</summary>
extern int32_t eternalOffset;
/// <summary>Executions so far (for the profile log).</summary>
extern int32_t NumExecutions;
/// <summary>The profile log file (<c>abl.log</c>) and its line buffer.</summary>
extern File* ProfileLog;
extern char ProfileLogBuffer[MAX_PROFILE_LOG_LINES][MAX_PROFILE_LOG_LINE_LENGTH];
extern int32_t NumProfileLogLines;
extern int32_t TotalProfileLogLines;

/// <summary>Writes the buffered profile lines to the log.</summary>
/// <remarks>MCX.EXE @ 0x00621e70</remarks>
void DumpProfileLog();

/// <summary>Flushes and closes the profile log.</summary>
/// <remarks>MCX.EXE @ 0x00621ec0</remarks>
void ABL_CloseProfileLog();

/// <summary>Creates the profile log <c>abl.log</c>.</summary>
/// <remarks>MCX.EXE @ 0x00621f50</remarks>
void ABL_OpenProfileLog();

/// <summary>Buffers a profile line (at most 127 characters).</summary>
/// <remarks>MCX.EXE @ 0x00621fd0</remarks>
void ABL_AddToProfileLog(char* profileEntry);

/// <summary>Allocates the module and instance registries for <paramref name="maxModules"/> modules.</summary>
/// <remarks>MCX.EXE @ 0x00622030</remarks>
void initModuleRegistry(int32_t maxModules);

/// <summary>Frees the registries and each module's file names.</summary>
/// <remarks>MCX.EXE @ 0x006220d0</remarks>
void destroyModuleRegistry();

/// <summary>Allocates the library registry.</summary>
/// <remarks>MCX.EXE @ 0x006221a0</remarks>
void initLibraryRegistry(int32_t maxLibraries);

/// <summary>Destroys the loaded libraries and the registry.</summary>
/// <remarks>MCX.EXE @ 0x006221f0</remarks>
void destroyLibraryRegistry();
