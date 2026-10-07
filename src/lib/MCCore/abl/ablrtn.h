#pragma once

// ABL's public interface (ABLi_*): start-up, compiling modules and libraries (MCAblCompiler), running them, parameter
// lists.

#include "abl/ablenv.h"

/// <summary>Debugger limits (set by ABLi_init, unused otherwise).</summary>
extern int32_t MaxBreaks;
extern int32_t MaxWatches;
/// <summary>Nonzero to keep the execution profile log.</summary>
extern int ProfileAbl;
/// <summary>Nonzero between ABLi_init and ABLi_close.</summary>
extern int ABLenabled;
/// <summary>The routine being executed.</summary>
extern MCAblSymbol* CurRoutineIdPtr;
/// <summary>Nonzero while executing inside an orders block.</summary>
extern int InOrdersBlock;

/// <summary>
/// Starts ABL: the stack of <paramref name="stackSize"/> bytes (the port allocates at least MAXSIZE_STACK items), the
/// symbol table with the standard routines (<see cref="AblSymbols"/>) and the registries; with
/// <paramref name="debug"/>, the debugger printing through <paramref name="debuggerPrintCallback"/>; with
/// <paramref name="profile"/>, the profile log. The three heap sizes, the code buffer size and the static variable
/// limit are ignored (the heaps are gone, the code buffer and the statics grow).
/// </summary>
void AblInit(uint32_t symbolTableHeapSize, uint32_t stackHeapSize, uint32_t codeHeapSize, uint32_t stackSize,
             uint32_t maxCodeBufferSize, uint32_t maxModules, uint32_t maxStaticVariables,
             void (*debuggerPrintCallback)(char* s), int debugInfo, int debug, int profile);

/// <summary>
/// Compiles module <paramref name="sourceFileName"/> (or returns the handle of the one already compiled from that
/// file, compared lower-cased) and registers it. A syntax error is fatal, as in MCX.EXE.
/// </summary>
/// <returns>The module handle, or -3 when the file won't open.</returns>
int32_t AblPreProcess(std::string_view sourceFileName);

/// <summary>
/// Runs compiled module <paramref name="moduleIdPtr"/> (without an instance: no static data) with
/// <paramref name="paramList"/>, storing its result in <paramref name="returnVal"/>.
/// </summary>
/// <returns>The number of statements executed.</returns>
int32_t AblExecute(MCAblSymbol* moduleIdPtr, MCAblSymbol* functionIdPtr = nullptr, MCAblParam* paramList = nullptr,
                   MCStackItemPtr returnVal = nullptr);

/// <summary>Frees everything ABLi_init made.</summary>
void AblClose();

/// <summary>Compiles library <paramref name="sourceFileName"/> and adds it to the loaded libraries.</summary>
/// <returns>0, or -1 if it won't open or was compiled before.</returns>
int32_t AblLoadLibrary(std::string_view sourceFileName);

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
