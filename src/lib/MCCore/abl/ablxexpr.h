#pragma once

// ABL expression interpreter: evaluates crunched expressions onto the runtime stack (see ablexec.h for the stack
// and code layout).

#include "abl/ablexpr.h"

/// <summary>Adds a record field's offset to the address on top of the stack (ABL has no records; never reached).</summary>
/// <returns>The field's type.</returns>
MCTypePtr ExecField();

/// <summary>Applies <c>[index, ...]</c> to the array address on top of the stack.</summary>
/// <returns>The element type.</returns>
MCTypePtr ExecSubscripts(MCTypePtr typePtr);

/// <summary>Pushes the value of constant <paramref name="idPtr"/> (a string constant as its address).</summary>
MCTypePtr ExecConstant(MCSymTableNodePtr idPtr);

/// <summary>
/// Pushes the address of variable <paramref name="idPtr"/> (local through the static links, static, eternal, or
/// through a reference parameter), applies subscripts and, for USE_EXPR, replaces it with the value.
/// </summary>
MCTypePtr ExecVariable(MCSymTableNodePtr idPtr, MCUseType use);

/// <summary>Evaluates a factor: a function call, constant, variable, number, literal, <c>( expression )</c> or <c>not</c>.</summary>
MCTypePtr ExecFactor();

/// <summary>Evaluates factors joined by <c>* / div mod and</c> (division by zero gives 0).</summary>
MCTypePtr ExecTerm();

/// <summary>Evaluates an optionally signed term followed by terms joined by <c>+ - or</c>.</summary>
MCTypePtr ExecSimpleExpression();

/// <summary>Evaluates an expression, leaving its value on top of the stack.</summary>
/// <returns>Its type.</returns>
MCTypePtr ExecExpression();
