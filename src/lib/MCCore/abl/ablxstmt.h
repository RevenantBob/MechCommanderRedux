#pragma once

// ABL statement interpreter: executes crunched statements, calls of declared functions (with the profile log) and
// the control structures.

#include "abl/MCAblSymbolTable.h"

/// <summary>Iterations a loop may make before it is an infinite-loop runtime error (<c>setmaxloops</c> changes it).</summary>
extern int32_t MaxLoopIterations;
/// <summary>Calls slower than this many milliseconds go to the profile log.</summary>
extern int32_t ProfileLogFunctionTimeLimit;
/// <summary>Set by <c>return</c> to leave the running routine.</summary>
extern int ExitWithReturn;

/// <summary>Executes one statement (after its statement marker: debugger tracing and the statement count).</summary>
void ExecStatement();

/// <summary>Executes <c>target = expression</c> (integers assigned to reals are converted; arrays are copied).</summary>
void ExecAssignmentStatement(MCAblSymbol* idPtr);

/// <summary>Executes a call of a declared or standard routine.</summary>
/// <returns>Its result type (the result is on the stack), or null.</returns>
MCAblType* ExecRoutineCall(MCAblSymbol* routineIdPtr);

/// <summary>
/// Calls a function written in ABL: pushes the frame and arguments and runs it, switching to its library's module
/// (static data, debugger) for a library function, and logging slow calls when profiling.
/// </summary>
MCAblType* ExecDeclaredRoutineCall(MCAblSymbol* routineIdPtr);

/// <summary>Sizes an open array parameter's type to <paramref name="size"/> bytes (the last dimension adapts).</summary>
void SetOpenArray(MCAblType* arrayTypePtr, int32_t size);

/// <summary>Evaluates the arguments of a declared routine call (copies of arrays passed by value).</summary>
void ExecActualParams(MCAblSymbol* routineIdPtr);

void ExecSwitchStatement();

void ExecForStatement();

void ExecIfStatement();

void ExecRepeatStatement();

void ExecWhileStatement();
