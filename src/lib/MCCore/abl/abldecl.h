#pragma once

// ABL declaration compiler: <c>const</c>, <c>type</c> and <c>var</c> blocks (with <c>static</c> and <c>eternal</c>
// variables), and the types they build (enumerations, arrays, strings as char arrays).

#include "abl/ablerr.h"
#include "abl/ablscan.h"
#include "abl/ablsymt.h"

/// <summary>Token lists (zero-terminated) for synchronize and tokenIn.</summary>
extern MCTokenCodeType FollowRoutineList[];
extern MCTokenCodeType FollowDeclarationList[];
extern MCTokenCodeType FollowVariablesList[];
extern MCTokenCodeType FollowVarBlockList[];
extern MCTokenCodeType FollowDimensionList[];
extern MCTokenCodeType IndexTypeStartList[];
/// <summary>Tokens that start a declaration block: const, var, function.</summary>
extern MCTokenCodeType DeclarationStartList[];

/// <summary>Skips curToken if it is <paramref name="tokenCode"/>.</summary>
void IfTokenGet(MCTokenCodeType tokenCode);

/// <summary>Skips curToken if it is <paramref name="tokenCode"/>, otherwise reports <paramref name="errorCode"/>.</summary>
void IfTokenGetElseError(MCTokenCodeType tokenCode, MCSyntaxErrorType errorCode);

/// <summary>
/// Compiles the declarations of a module or routine: constants, types, variables and (with
/// <paramref name="allowFunctions"/>, at module level) functions.
/// </summary>
void Declarations(MCSymTableNodePtr routineIdPtr, int allowFunctions);

/// <summary>Compiles a <c>const</c> block.</summary>
void ConstDefinitions();

/// <summary>A char-array type for a string of <paramref name="length"/> characters.</summary>
MCTypePtr MakeStringType(int32_t length);

/// <summary>Compiles the value of constant <paramref name="constantIdPtr"/> (a number, string or other constant).</summary>
void DoConst(MCSymTableNodePtr constantIdPtr);

/// <summary>Compiles a <c>type</c> block.</summary>
void TypeDefinitions();

/// <summary>Compiles a type: a type name or an enumeration, with array dimensions.</summary>
MCTypePtr DoType();

/// <summary>The type named by <paramref name="idPtr"/>.</summary>
MCTypePtr IdentifierType(MCSymTableNodePtr idPtr);

/// <summary>Compiles an enumeration <c>(a, b, c)</c>: a new type with its values as constants.</summary>
MCTypePtr EnumerationType();

/// <summary>Subrange types aren't supported: returns null.</summary>
MCTypePtr SubrangeType();

/// <summary>Empty in MCX.EXE.</summary>
void GetSubrangeLimit(MCSymTableNodePtr minIdPtr, int32_t* minLimit, MCTypePtr* typePtr);

/// <summary>Computes (and stores) the byte size of array type <paramref name="typePtr"/>.</summary>
int32_t ArraySize(MCTypePtr typePtr);

/// <summary>Compiles a <c>var</c> block of <paramref name="routineIdPtr"/>.</summary>
void VarDeclarations(MCSymTableNodePtr routineIdPtr);

/// <summary>
/// Compiles variable lines: locals get stack slots from <paramref name="offset"/>, statics slots of the module's
/// static data (StaticVariablesSizes), eternals slots at the bottom of the stack (eternalOffset). Record fields
/// (no routine) aren't supported.
/// </summary>
void VarOrFieldDeclarations(MCSymTableNodePtr routineIdPtr, MCTypePtr recordTypePtr, int32_t offset);
