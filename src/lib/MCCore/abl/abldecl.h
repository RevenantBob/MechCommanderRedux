#pragma once

// ABL declaration compiler: <c>const</c>, <c>type</c> and <c>var</c> blocks (with <c>static</c> and <c>eternal</c>
// variables), and the types they build (enumerations, arrays, strings as char arrays).

#include "abl/ablerr.h"
#include "abl/ablscan.h"
#include "abl/ablsymt.h"

/// <summary>Token lists (zero-terminated) for synchronize and tokenIn.</summary>
extern TokenCodeType followRoutineList[];
extern TokenCodeType followDeclarationList[];
extern TokenCodeType followVariablesList[];
extern TokenCodeType followVarBlockList[];
extern TokenCodeType followDimensionList[];
extern TokenCodeType indexTypeStartList[];
/// <summary>Tokens that start a declaration block: const, var, function.</summary>
extern TokenCodeType declarationStartList[];

/// <summary>Skips curToken if it is <paramref name="tokenCode"/>.</summary>
/// <remarks>MCX.EXE @ 0x00621170</remarks>
void ifTokenGet(TokenCodeType tokenCode);

/// <summary>Skips curToken if it is <paramref name="tokenCode"/>, otherwise reports <paramref name="errorCode"/>.</summary>
/// <remarks>MCX.EXE @ 0x00621190</remarks>
void ifTokenGetElseError(TokenCodeType tokenCode, SyntaxErrorType errorCode);

/// <summary>
/// Compiles the declarations of a module or routine: constants, types, variables and (with
/// <paramref name="allowFunctions"/>, at module level) functions.
/// </summary>
/// <remarks>MCX.EXE @ 0x006211c0</remarks>
void declarations(SymTableNodePtr routineIdPtr, int allowFunctions);

/// <summary>Compiles a <c>const</c> block.</summary>
/// <remarks>MCX.EXE @ 0x00621290</remarks>
void constDefinitions();

/// <summary>A char-array type for a string of <paramref name="length"/> characters.</summary>
/// <remarks>MCX.EXE @ 0x00621340</remarks>
TypePtr makeStringType(int32_t length);

/// <summary>Compiles the value of constant <paramref name="constantIdPtr"/> (a number, string or other constant).</summary>
/// <remarks>MCX.EXE @ 0x00621390</remarks>
void doConst(SymTableNodePtr constantIdPtr);

/// <summary>Compiles a <c>type</c> block.</summary>
/// <remarks>MCX.EXE @ 0x006216e0</remarks>
void typeDefinitions();

/// <summary>Compiles a type: a type name or an enumeration, with array dimensions.</summary>
/// <remarks>MCX.EXE @ 0x006217a0</remarks>
TypePtr doType();

/// <summary>The type named by <paramref name="idPtr"/>.</summary>
/// <remarks>MCX.EXE @ 0x00621990</remarks>
TypePtr identifierType(SymTableNodePtr idPtr);

/// <summary>Compiles an enumeration <c>(a, b, c)</c>: a new type with its values as constants.</summary>
/// <remarks>MCX.EXE @ 0x006219a0 (unnamed in the symbols)</remarks>
TypePtr enumerationType();

/// <summary>Subrange types aren't supported: returns null.</summary>
/// <remarks>MCX.EXE @ 0x00621a70</remarks>
TypePtr subrangeType();

/// <summary>Empty in MCX.EXE.</summary>
/// <remarks>MCX.EXE @ 0x00621a80</remarks>
void getSubrangeLimit(SymTableNodePtr minIdPtr, int32_t* minLimit, TypePtr* typePtr);

/// <summary>Computes (and stores) the byte size of array type <paramref name="typePtr"/>.</summary>
/// <remarks>MCX.EXE @ 0x00621a90</remarks>
int32_t arraySize(TypePtr typePtr);

/// <summary>Compiles a <c>var</c> block of <paramref name="routineIdPtr"/>.</summary>
/// <remarks>MCX.EXE @ 0x00621ad0</remarks>
void varDeclarations(SymTableNodePtr routineIdPtr);

/// <summary>
/// Compiles variable lines: locals get stack slots from <paramref name="offset"/>, statics slots of the module's
/// static data (StaticVariablesSizes), eternals slots at the bottom of the stack (eternalOffset). Record fields
/// (no routine) aren't supported.
/// </summary>
/// <remarks>MCX.EXE @ 0x00621af0</remarks>
void varOrFieldDeclarations(SymTableNodePtr routineIdPtr, TypePtr recordTypePtr, int32_t offset);
