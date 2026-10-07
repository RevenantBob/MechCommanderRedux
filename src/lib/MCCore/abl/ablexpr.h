#pragma once

// ABL expression compiler: parses expressions, checks their types and crunches them into the code buffer
// (ablxexpr.cpp executes them).

#include "abl/ablsymt.h"
#include "abl/ablscan.h"

/// <summary>How a variable reference is used (variable, execVariable).</summary>
enum MCUseType
{
    /// <summary>Its value, in an expression.</summary>
    USE_EXPR = 0,
    /// <summary>The target of an assignment (its address).</summary>
    USE_TARGET = 1,
    /// <summary>A reference argument (its address).</summary>
    USE_REFPARAM = 2
};

/// <summary>Operator token lists (zero-terminated) for tokenIn.</summary>
extern MCTokenCodeType RelationalOperatorList[];
extern MCTokenCodeType AddOperatorList[];
extern MCTokenCodeType MultiplyOperatorList[];

/// <summary>The type an enumeration or subrange is based on (the type itself otherwise).</summary>
MCTypePtr BaseType(MCTypePtr typePtr);

/// <summary>Reports incompatible operands of a relational operator.</summary>
void CheckRelationalOpTypes(MCTypePtr type1, MCTypePtr type2);

/// <summary>Whether a <paramref name="valueType"/> value can be assigned to a <paramref name="targetType"/> target.</summary>
int IsAssignTypeCompatible(MCTypePtr targetType, MCTypePtr valueType);

/// <summary>Parses a variable reference (with subscripts).</summary>
/// <returns>Its type.</returns>
MCTypePtr Variable(MCSymTableNodePtr variableIdPtr, MCUseType use);

/// <summary>Parses <c>[index, ...]</c> after an array variable.</summary>
/// <returns>The element type.</returns>
MCTypePtr ArraySubscriptList(MCTypePtr typePtr);

/// <summary>Parses a factor: a constant, variable, function call, <c>not</c> or a parenthesised expression.</summary>
MCTypePtr Factor();

/// <summary>Parses factors joined by <c>* / div mod and</c>.</summary>
MCTypePtr Term();

/// <summary>Parses an optionally signed run of terms joined by <c>+ - or</c>.</summary>
MCTypePtr SimpleExpression();

/// <summary>Parses a full expression (with a relational operator).</summary>
/// <returns>Its type.</returns>
MCTypePtr Expression();
