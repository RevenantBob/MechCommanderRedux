#pragma once

// ABL's public interface (ABLi_*: start-up, compiling modules and libraries, running them, parameter lists) and the
// compiler for modules and functions (headers, parameter lists, calls).

#include "abl/ablenv.h"

/// <summary>What kind of code block is being compiled (blockType).</summary>
enum MCBlockType
{
    BLOCK_MODULE = 0,
    BLOCK_ROUTINE = 1
};

/// <summary>Debugger limits (set by ABLi_init, unused otherwise).</summary>
extern int32_t MaxBreaks;
extern int32_t MaxWatches;
/// <summary>Whether <c>print</c>, <c>assert</c> and the string functions are compiled (else their calls are dropped).</summary>
extern int PrintEnabled;
extern int AssertEnabled;
extern int StringFunctionsEnabled;
/// <summary>Nonzero to keep the execution profile log.</summary>
extern int ProfileAbl;
/// <summary>Nonzero between ABLi_init and ABLi_close.</summary>
extern int ABLenabled;
/// <summary>Sizes of the static variables of the module being compiled (0 for scalars, bytes for arrays).</summary>
extern int32_t* StaticVariablesSizes;
extern int32_t NumStaticVariables;
extern int32_t MaxStaticVariables;
/// <summary>Nonzero while compiling a code block (getToken crunches tokens), and which kind.</summary>
extern int BlockFlag;
extern MCBlockType BlockType;
/// <summary>The module being compiled or executed, and the routine.</summary>
extern MCSymTableNodePtr CurModuleIdPtr;
extern MCSymTableNodePtr CurRoutineIdPtr;
/// <summary>Nonzero while executing inside an orders block.</summary>
extern int InOrdersBlock;
/// <summary>Reset by ABLi_init / ABLi_preProcess; nothing else uses it.</summary>
extern int EofFlag;
/// <summary>Token lists (zero-terminated) for synchronize.</summary>
extern MCTokenCodeType FollowHeaderList[];
extern MCTokenCodeType FollowModuleIdList[];
extern MCTokenCodeType FollowFunctionIdList[];
extern MCTokenCodeType FollowParamsList[];
extern MCTokenCodeType FollowParamList[];
extern MCTokenCodeType FollowModuleDeclsList[];
extern MCTokenCodeType FollowRoutineDeclsList[];

/// <summary>
/// Starts ABL: creates the code buffer, the stack of <paramref name="stackSize"/> bytes (the port allocates at least
/// MAXSIZE_STACK items), the symbol table with the standard routines and the registries; with
/// <paramref name="debug"/>, the debugger printing through <paramref name="debuggerPrintCallback"/>; with
/// <paramref name="profile"/>, the profile log. The three heap sizes are ignored (ABL's heaps are gone).
/// </summary>
void AblInit(uint32_t symbolTableHeapSize, uint32_t stackHeapSize, uint32_t codeHeapSize, uint32_t stackSize,
             uint32_t maxCodeBufferSize, uint32_t maxModules, uint32_t maxStaticVariables,
             void (*debuggerPrintCallback)(char* s), int debugInfo, int debug, int profile);

/// <summary>
/// Compiles module <paramref name="sourceFileName"/> (or returns the handle of the one already compiled from that
/// file) and registers it. Reports the error, line and file counts through the pointers.
/// </summary>
/// <returns>The module handle.</returns>
int32_t AblPreProcess(char* sourceFileName, int32_t* numErrors = nullptr, int32_t* numLinesProcessed = nullptr,
                      int32_t* numFilesProcessed = nullptr, int printLines = 0);

/// <summary>
/// Runs compiled module <paramref name="moduleIdPtr"/> (without an instance: no static data) with
/// <paramref name="paramList"/>, storing its result in <paramref name="returnVal"/>.
/// </summary>
/// <returns>The number of statements executed.</returns>
int32_t AblExecute(MCSymTableNodePtr moduleIdPtr, MCSymTableNodePtr functionIdPtr = nullptr,
                   MCAblParam* paramList = nullptr, MCStackItemPtr returnVal = nullptr);

/// <summary>Frees everything ABLi_init made.</summary>
void AblClose();

/// <summary>Compiles library <paramref name="sourceFileName"/> and adds it to the loaded libraries.</summary>
/// <returns>0, -1 if it failed, or 0xFAAF000B (as a negative number) if out of memory.</returns>
int32_t AblLoadLibrary(char* sourceFileName, int32_t* numErrors = nullptr, int32_t* numLinesProcessed = nullptr,
                       int32_t* numFilesProcessed = nullptr, int printLines = 0);

/// <summary>A zeroed list of <paramref name="numParameters"/> parameters (null for none).</summary>
MCAblParam* AblCreateParamList(int32_t numParameters);

/// <summary>Makes parameter <paramref name="index"/> the integer <paramref name="value"/>.</summary>
void AblSetIntegerParam(MCAblParam* paramList, int32_t index, int32_t value);

/// <summary>Makes parameter <paramref name="index"/> the real <paramref name="value"/>.</summary>
void AblSetRealParam(MCAblParam* paramList, int32_t index, float value);

/// <summary>Frees a parameter list.</summary>
void AblDeleteParamList(MCAblParam* paramList);

/// <summary>Module instance <paramref name="id"/>, or null.</summary>
MCAblModule* AblGetModule(int32_t id);

/// <summary>Whether ABL is running.</summary>
int AblEnabled();

/// <summary>Compiles <c>module name(params)</c> (or <c>library name</c>) and opens its scope.</summary>
/// <returns>The module symbol.</returns>
MCSymTableNodePtr ModuleHeader();

/// <summary>Compiles a function: header, declarations and code (or a <c>forward</c> declaration).</summary>
void Routine();

/// <summary>Compiles <c>function name(params) : type</c> and opens its scope.</summary>
/// <returns>The function symbol.</returns>
MCSymTableNodePtr FunctionHeader();

/// <summary>Compiles a formal parameter list (<c>@</c> marks reference parameters).</summary>
/// <returns>The first parameter; the count and the stack items they take through the pointers.</returns>
MCSymTableNodePtr FormalParamList(int32_t* count, int32_t* totalSize);

/// <summary>Compiles a call of <paramref name="routineIdPtr"/> (standard or declared).</summary>
/// <returns>Its result type (null for none).</returns>
MCTypePtr RoutineCall(MCSymTableNodePtr routineIdPtr, int paramCheck);

/// <summary>Compiles a call of a function written in ABL.</summary>
MCTypePtr DeclaredRoutineCall(MCSymTableNodePtr routineIdPtr, int paramCheck);

/// <summary>Compiles an argument list, checking it against the routine's parameters when <paramref name="paramCheck"/>.</summary>
void ActualParamList(MCSymTableNodePtr routineIdPtr, int paramCheck);
