#pragma once

// ABL expression interpreter: evaluates crunched expressions onto the runtime stack (see ablexec.h for the stack
// and code layout).

#include "abl/MCAblSymbol.h"

/// <summary>How a variable reference is used (the original's <c>UseType</c>).</summary>
enum class MCAblUse : int32_t
{
    /// <summary>Its value, in an expression.</summary>
    Expression = 0,
    /// <summary>The target of an assignment (its address).</summary>
    Target = 1,
    /// <summary>A reference argument (its address).</summary>
    RefParam = 2
};

/// <summary>Adds a record field's offset to the address on top of the stack (ABL has no records; never reached).</summary>
/// <returns>The field's type.</returns>
MCAblType* ExecField();

/// <summary>Applies <c>[index, ...]</c> to the array address on top of the stack.</summary>
/// <returns>The element type.</returns>
MCAblType* ExecSubscripts(MCAblType* typePtr);

/// <summary>Pushes the value of constant <paramref name="idPtr"/> (a string constant as its address).</summary>
MCAblType* ExecConstant(MCAblSymbol* idPtr);

/// <summary>
/// Pushes the address of variable <paramref name="idPtr"/> (local through the static links, static, eternal, or
/// through a reference parameter), applies subscripts and, for MCAblUse::Expression, replaces it with the value.
/// </summary>
MCAblType* ExecVariable(MCAblSymbol* idPtr, MCAblUse use);

/// <summary>Evaluates a factor: a function call, constant, variable, number, literal, <c>( expression )</c> or <c>not</c>.</summary>
MCAblType* ExecFactor();

/// <summary>Evaluates factors joined by <c>* / div mod and</c> (division by zero gives 0).</summary>
MCAblType* ExecTerm();

/// <summary>Evaluates an optionally signed term followed by terms joined by <c>+ - or</c>.</summary>
MCAblType* ExecSimpleExpression();

/// <summary>Evaluates an expression, leaving its value on top of the stack.</summary>
/// <returns>Its type.</returns>
MCAblType* ExecExpression();
