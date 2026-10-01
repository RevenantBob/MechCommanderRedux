#pragma once

// ABL statement interpreter: executes crunched statements, calls of declared functions (with the profile log) and
// the control structures.

#include "abl/ablsymt.h"

/// <summary>Iterations a loop may make before it is an infinite-loop runtime error (<c>setmaxloops</c> changes it).</summary>
extern int32_t MaxLoopIterations;
/// <summary>Calls slower than this many milliseconds go to the profile log.</summary>
extern int32_t ProfileLogFunctionTimeLimit;
/// <summary>Set by <c>return</c> to leave the running routine.</summary>
extern int ExitWithReturn;

/// <summary>Executes one statement (after its statement marker: debugger tracing and the statement count).</summary>
/// <remarks>MCX.EXE @ 0x006374c0</remarks>
void execStatement();

/// <summary>Executes <c>target = expression</c> (integers assigned to reals are converted; arrays are copied).</summary>
/// <remarks>MCX.EXE @ 0x00637650</remarks>
void execAssignmentStatement(SymTableNodePtr idPtr);

/// <summary>Executes a call of a declared or standard routine.</summary>
/// <returns>Its result type (the result is on the stack), or null.</returns>
/// <remarks>MCX.EXE @ 0x00637740</remarks>
TypePtr execRoutineCall(SymTableNodePtr routineIdPtr);

/// <summary>
/// Calls a function written in ABL: pushes the frame and arguments and runs it, switching to its library's module
/// (static data, debugger) for a library function, and logging slow calls when profiling.
/// </summary>
/// <remarks>MCX.EXE @ 0x00637770</remarks>
TypePtr execDeclaredRoutineCall(SymTableNodePtr routineIdPtr);

/// <summary>Sizes an open array parameter's type to <paramref name="size"/> bytes (the last dimension adapts).</summary>
/// <remarks>MCX.EXE @ 0x006379c0</remarks>
void setOpenArray(TypePtr arrayTypePtr, int32_t size);

/// <summary>Evaluates the arguments of a declared routine call (copies of arrays passed by value).</summary>
/// <remarks>MCX.EXE @ 0x00637a00</remarks>
void execActualParams(SymTableNodePtr routineIdPtr);

/// <remarks>MCX.EXE @ 0x00637af0</remarks>
void execSwitchStatement();

/// <remarks>MCX.EXE @ 0x00637bd0</remarks>
void execForStatement();

/// <remarks>MCX.EXE @ 0x00637d90</remarks>
void execIfStatement();

/// <remarks>MCX.EXE @ 0x00637e80</remarks>
void execRepeatStatement();

/// <remarks>MCX.EXE @ 0x00637f00</remarks>
void execWhileStatement();
