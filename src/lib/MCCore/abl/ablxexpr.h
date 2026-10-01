#pragma once

// ABL expression interpreter: evaluates crunched expressions onto the runtime stack (see ablexec.h for the stack
// and code layout).

#include "abl/ablexpr.h"

/// <summary>Adds a record field's offset to the address on top of the stack (ABL has no records; never reached).</summary>
/// <returns>The field's type.</returns>
/// <remarks>MCX.EXE @ 0x0062e4e0</remarks>
TypePtr execField();

/// <summary>Applies <c>[index, ...]</c> to the array address on top of the stack.</summary>
/// <returns>The element type.</returns>
/// <remarks>MCX.EXE @ 0x0062e510</remarks>
TypePtr execSubscripts(TypePtr typePtr);

/// <summary>Pushes the value of constant <paramref name="idPtr"/> (a string constant as its address).</summary>
/// <remarks>MCX.EXE @ 0x0062e5a0</remarks>
TypePtr execConstant(SymTableNodePtr idPtr);

/// <summary>
/// Pushes the address of variable <paramref name="idPtr"/> (local through the static links, static, eternal, or
/// through a reference parameter), applies subscripts and, for USE_EXPR, replaces it with the value.
/// </summary>
/// <remarks>MCX.EXE @ 0x0062e630</remarks>
TypePtr execVariable(SymTableNodePtr idPtr, UseType use);

/// <summary>Evaluates a factor: a function call, constant, variable, number, literal, <c>( expression )</c> or <c>not</c>.</summary>
/// <remarks>MCX.EXE @ 0x0062e780</remarks>
TypePtr execFactor();

/// <summary>Evaluates factors joined by <c>* / div mod and</c> (division by zero gives 0).</summary>
/// <remarks>MCX.EXE @ 0x0062e930</remarks>
TypePtr execTerm();

/// <summary>Evaluates an optionally signed term followed by terms joined by <c>+ - or</c>.</summary>
/// <remarks>MCX.EXE @ 0x0062eb40</remarks>
TypePtr execSimpleExpression();

/// <summary>Evaluates an expression, leaving its value on top of the stack.</summary>
/// <returns>Its type.</returns>
/// <remarks>MCX.EXE @ 0x0062ec80</remarks>
TypePtr execExpression();
