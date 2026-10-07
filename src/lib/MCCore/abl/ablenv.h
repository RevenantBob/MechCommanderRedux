#pragma once

// ABL environment: the registry of compiled modules and loaded libraries, and ABLModule, one running instance of a
// compiled module (a unit brain, a mission script) with its own static data. Also the execution profile log.

#include "abl/ablexec.h"

class MCFile;
class MCWatchManager;
class MCBreakPointManager;

/// <summary>Longest module instance name (ABLModule::name, with its terminator).</summary>
inline constexpr int32_t MAX_ABLMODULE_NAME = 26;
/// <summary>Lines the profile log buffers before writing them out.</summary>
inline constexpr int32_t MAX_PROFILE_LOG_LINES = 256;
/// <summary>Length of a profile log line.</summary>
inline constexpr int32_t MAX_PROFILE_LOG_LINE_LENGTH = 128;

/// <summary>What an ABLParam holds.</summary>
enum MCAblParamType : uint8_t
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
struct MCAblParam
{
    /// <summary>An ABLParamType.</summary>
    char Type = 0;
    int32_t Integer = 0;
    float Real = 0;
};

/// <summary>A compiled module (ModuleRegistry): its code and what its instances need.</summary>
/// <remarks>0x28 bytes in the original.</remarks>
struct MCModuleEntry
{
    /// <summary>The main source file's name.</summary>
    char* FileName = nullptr;
    /// <summary>The module symbol: its code, parameters and symbol tree.</summary>
    MCSymTableNodePtr ModuleIdPtr = nullptr;
    int32_t NumSourceFiles = 0;
    /// <summary>Every source file compiled into it (the index is a statement marker's file number).</summary>
    char** SourceFiles = nullptr;
    int32_t NumLibrariesUsed = 0;
    MCAblModule** LibrariesUsed = nullptr;
    int32_t NumStaticVars = 0;
    /// <summary>Per static variable, the bytes of its array block, or 0 for a scalar.</summary>
    int32_t* SizeStaticVars = nullptr;
    /// <summary>
    /// numStaticVars * 4 plus the array blocks (bookkeeping for the debugger; it counts the original's 4-byte slots).
    /// </summary>
    int32_t TotalSizeStaticVars = 0;
    int32_t NumInstances = 0;
};

/// <summary>
/// A running instance of a compiled module: a unit's brain, a mission script, or a library. Each instance has its
/// own static variables; the code and symbols are shared through ModuleRegistry.
/// </summary>
/// <remarks>
/// Original source: <c>abl\ablenv.cpp</c>, 0x48 bytes, allocated through its own operator new. Other
/// classes (MechWarrior, GeneralOrder, Scenario) read its fields directly.
/// </remarks>
class MCAblModule
{
public:
    /// <summary>An empty module: no id, no handle, no static data.</summary>
    /// <remarks>Inline in the original (GeneralOrder::init, MechWarrior::setBrain, ABLi_loadLibrary).</remarks>
    MCAblModule()
    {
        Id = -1;
        Name[0] = '\0';
        Handle = -1;
        StaticData = nullptr;
        InitCalled = 0;
        WatchManager = nullptr;
        BreakPointManager = nullptr;
        Trace = 0;
        Step = 0;
        TraceEntry = 0;
        TraceExit = 0;
    }

    /// <summary>
    /// Makes this an instance of registered module <paramref name="moduleHandle"/>: allocates its static data (and
    /// its static arrays), registers it in ModuleInstanceRegistry, and with the debugger on, its watch and break-point
    /// managers.
    /// </summary>
    /// <returns>0.</returns>
    int32_t Init(int32_t moduleHandle);

    /// <summary>Names the instance (at most 25 characters).</summary>
    void SetName(char* name);

    /// <summary>Runs the module's main code with <paramref name="paramList"/> for its parameters.</summary>
    /// <returns>The number of statements executed (0 if a parameter doesn't match).</returns>
    int32_t Execute(MCAblParam* paramList);

    /// <summary>Runs only function <paramref name="function"/> of the module, in the module's frame.</summary>
    /// <returns>The number of statements executed.</returns>
    int32_t Execute(MCAblParam* moduleParamList, MCSymTableNodePtr function, MCAblParam* functionParamList);

    /// <summary>
    /// Finds a symbol: in <paramref name="function"/>'s scope, then the module's, then (with
    /// <paramref name="searchLibraries"/>) the libraries it uses. Lower-cases <paramref name="symbolName"/> in place.
    /// </summary>
    MCSymTableNodePtr FindSymbol(char* symbolName, MCSymTableNodePtr function = nullptr, int searchLibraries = 0);

    /// <summary>Finds a function of the module (or, with <paramref name="searchLibraries"/>, of its libraries).</summary>
    MCSymTableNodePtr FindFunction(char* functionName, int searchLibraries = 0);

    /// <summary>Sets static integer <paramref name="staticName"/>.</summary>
    /// <returns>0, 1 (no such symbol), 2 (not an integer) or 3 (not static).</returns>
    int32_t SetStaticInteger(char* staticName, int32_t value);

    /// <summary>Sets static real <paramref name="staticName"/>.</summary>
    /// <returns>0, 1 (no such symbol), 2 (not a real) or 3 (not static).</returns>
    int32_t SetStaticReal(char* staticName, float value);

    /// <summary>Copies <paramref name="size"/> integers into static array <paramref name="staticName"/>.</summary>
    /// <returns>0, 1 (no such symbol) or 3 (not static).</returns>
    int32_t SetStaticIntegerArray(char* staticName, int32_t size, int32_t* values);

    /// <summary>Copies <paramref name="size"/> reals into static array <paramref name="staticName"/>.</summary>
    /// <returns>0, 1 (no such symbol) or 3 (not static).</returns>
    int32_t SetStaticRealArray(char* staticName, int32_t size, float* values);

    /// <summary>Name of source file <paramref name="fileNumber"/> of the module.</summary>
    char* GetSourceFile(int32_t fileNumber);

    /// <summary>Copies the folder of source file <paramref name="fileNumber"/> (with its backslash) to <paramref name="directory"/>.</summary>
    /// <returns><paramref name="directory"/>, or null if the name has no folder.</returns>
    char* GetSourceDirectory(int32_t fileNumber, char* directory);

    /// <summary>The module's static variable count and total size, and optionally each one's array size.</summary>
    void GetInfo(int32_t& numStatics, int32_t& staticsSize, int32_t* sizeList);

    /// <summary>Unregisters the instance and frees its static data and debugger managers.</summary>
    void Destroy();

    int32_t GetId() const { return Id; }
    int32_t GetHandle() const { return Handle; }
    char* GetName() { return Name; }
    int32_t GetReturnValue() const { return ReturnVal; }

    /// <summary>Instance number (the order of init calls), or -1.</summary>
    int32_t Id = 0;
    char Name[MAX_ABLMODULE_NAME]{};
    /// <summary>Index of its compiled module in ModuleRegistry, or -1.</summary>
    int32_t Handle = 0;
    /// <summary>The instance's static variables, one StackItem each (arrays as pointers to their blocks).</summary>
    MCStackItemPtr StaticData = nullptr;
    /// <summary>The integer the last execution returned.</summary>
    int32_t ReturnVal = 0;
    /// <summary>Nonzero once the module's <c>init</c> function has run.</summary>
    int32_t InitCalled = 0;
    MCWatchManager* WatchManager = nullptr;
    MCBreakPointManager* BreakPointManager = nullptr;
    /// <summary>Debugger modes for this instance (copied into the Debugger by setModule).</summary>
    int32_t Trace = 0;
    int32_t Step = 0;
    int32_t TraceEntry = 0;
    int32_t TraceExit = 0;
};

typedef MCAblModule* MCAblModulePtr;

/// <summary>Per-module watch and break-point limits (from ABLi_init).</summary>
extern int32_t MaxWatchesPerModule;
extern int32_t MaxBreakPointsPerModule;
/// <summary>The compiled modules; a module's handle indexes it.</summary>
extern MCModuleEntry* ModuleRegistry;
extern int32_t MaxModules;
extern int32_t NumModulesRegistered;
/// <summary>Instances made so far (the next instance id).</summary>
extern int32_t NumModules;
/// <summary>The live instances.</summary>
extern MCAblModule** ModuleInstanceRegistry;
extern int32_t NumModuleInstances;
/// <summary>The loaded libraries.</summary>
extern MCAblModule** LibraryInstanceRegistry;
extern int32_t MaxLibraries;
extern int32_t NumLibrariesLoaded;
/// <summary>The instance executing, and its handle.</summary>
extern MCAblModule* CurModule;
extern int32_t CurModuleHandle;
/// <summary>The library being compiled (ABLi_loadLibrary), or null for a module.</summary>
extern MCAblModule* CurLibrary;
/// <summary>Nesting depth of declared-routine calls.</summary>
extern int32_t CallStackLevel;
/// <summary>Nonzero when the executing module's <c>init</c> must run first.</summary>
extern int CallModuleInit;
/// <summary>Stack items taken by eternal variables (the bottom of the stack).</summary>
extern int32_t EternalOffset;
/// <summary>Executions so far (for the profile log).</summary>
extern int32_t NumExecutions;
/// <summary>The profile log file (<c>abl.log</c>) and its line buffer.</summary>
extern MCFile* ProfileLog;
extern char ProfileLogBuffer[MAX_PROFILE_LOG_LINES][MAX_PROFILE_LOG_LINE_LENGTH];
extern int32_t NumProfileLogLines;
extern int32_t TotalProfileLogLines;

/// <summary>Writes the buffered profile lines to the log.</summary>
void DumpProfileLog();

/// <summary>Flushes and closes the profile log.</summary>
void AblCloseProfileLog();

/// <summary>Creates the profile log <c>abl.log</c>.</summary>
void AblOpenProfileLog();

/// <summary>Buffers a profile line (at most 127 characters).</summary>
void AblAddToProfileLog(char* profileEntry);

/// <summary>Allocates the module and instance registries for <paramref name="maxModules"/> modules.</summary>
void InitModuleRegistry(int32_t maxModules);

/// <summary>Frees the registries and each module's file names.</summary>
void DestroyModuleRegistry();

/// <summary>Allocates the library registry.</summary>
void InitLibraryRegistry(int32_t maxLibraries);

/// <summary>Destroys the loaded libraries and the registry.</summary>
void DestroyLibraryRegistry();
