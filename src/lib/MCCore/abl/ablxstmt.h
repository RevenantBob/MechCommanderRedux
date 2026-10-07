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
void ExecStatement();

/// <summary>Executes <c>target = expression</c> (integers assigned to reals are converted; arrays are copied).</summary>
void ExecAssignmentStatement(MCSymTableNodePtr idPtr);

/// <summary>Executes a call of a declared or standard routine.</summary>
/// <returns>Its result type (the result is on the stack), or null.</returns>
MCTypePtr ExecRoutineCall(MCSymTableNodePtr routineIdPtr);

/// <summary>
/// Calls a function written in ABL: pushes the frame and arguments and runs it, switching to its library's module
/// (static data, debugger) for a library function, and logging slow calls when profiling.
/// </summary>
MCTypePtr ExecDeclaredRoutineCall(MCSymTableNodePtr routineIdPtr);

/// <summary>Sizes an open array parameter's type to <paramref name="size"/> bytes (the last dimension adapts).</summary>
void SetOpenArray(MCTypePtr arrayTypePtr, int32_t size);

/// <summary>Evaluates the arguments of a declared routine call (copies of arrays passed by value).</summary>
void ExecActualParams(MCSymTableNodePtr routineIdPtr);

void ExecSwitchStatement();

void ExecForStatement();

void ExecIfStatement();

void ExecRepeatStatement();

void ExecWhileStatement();
