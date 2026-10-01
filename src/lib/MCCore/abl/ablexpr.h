#pragma once

// ABL expression compiler: parses expressions, checks their types and crunches them into the code buffer
// (ablxexpr.cpp executes them).

#include "abl/ablsymt.h"
#include "abl/ablscan.h"

/// <summary>How a variable reference is used (variable, execVariable).</summary>
enum UseType
{
    /// <summary>Its value, in an expression.</summary>
    USE_EXPR = 0,
    /// <summary>The target of an assignment (its address).</summary>
    USE_TARGET = 1,
    /// <summary>A reference argument (its address).</summary>
    USE_REFPARAM = 2
};

/// <summary>Operator token lists (zero-terminated) for tokenIn.</summary>
extern TokenCodeType relationalOperatorList[];
extern TokenCodeType addOperatorList[];
extern TokenCodeType multiplyOperatorList[];

/// <summary>The type an enumeration or subrange is based on (the type itself otherwise).</summary>
/// <remarks>MCX.EXE @ 0x006237b0</remarks>
TypePtr baseType(TypePtr typePtr);

/// <summary>Reports incompatible operands of a relational operator.</summary>
/// <remarks>MCX.EXE @ 0x006237c0</remarks>
void checkRelationalOpTypes(TypePtr type1, TypePtr type2);

/// <summary>Whether a <paramref name="valueType"/> value can be assigned to a <paramref name="targetType"/> target.</summary>
/// <remarks>MCX.EXE @ 0x00623840</remarks>
int isAssignTypeCompatible(TypePtr targetType, TypePtr valueType);

/// <summary>Parses a variable reference (with subscripts).</summary>
/// <returns>Its type.</returns>
/// <remarks>MCX.EXE @ 0x006238c0</remarks>
TypePtr variable(SymTableNodePtr variableIdPtr, UseType use);

/// <summary>Parses <c>[index, ...]</c> after an array variable.</summary>
/// <returns>The element type.</returns>
/// <remarks>MCX.EXE @ 0x00623960</remarks>
TypePtr arraySubscriptList(TypePtr typePtr);

/// <summary>Parses a factor: a constant, variable, function call, <c>not</c> or a parenthesised expression.</summary>
/// <remarks>MCX.EXE @ 0x006239f0</remarks>
TypePtr factor();

/// <summary>Parses factors joined by <c>* / div mod and</c>.</summary>
/// <remarks>MCX.EXE @ 0x00623c50</remarks>
TypePtr term();

/// <summary>Parses an optionally signed run of terms joined by <c>+ - or</c>.</summary>
/// <remarks>MCX.EXE @ 0x00623dd0</remarks>
TypePtr simpleExpression();

/// <summary>Parses a full expression (with a relational operator).</summary>
/// <returns>Its type.</returns>
/// <remarks>MCX.EXE @ 0x00623ef0</remarks>
TypePtr expression();
