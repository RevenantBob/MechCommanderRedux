#include "stdafx.h"
#include "abl/ablexpr.h"
#include "abl/abldecl.h"
#include "abl/ablerr.h"
#include "abl/ablexec.h"
#include "abl/ablrtn.h"
#include "abl/ablscan.h"
#include "abl/ablstmt.h"
#include "abl/ablsymt.h"
#include "lib/MCFatal.h"

MCTokenCodeType RelationalOperatorList[] = {TKN_LT, TKN_LE, TKN_EQUALEQUAL, TKN_NE, TKN_GE, TKN_GT, TKN_NONE};
MCTokenCodeType AddOperatorList[] = {TKN_PLUS, TKN_MINUS, TKN_OR, TKN_NONE};
MCTokenCodeType MultiplyOperatorList[] = {TKN_STAR, TKN_FSLASH, TKN_DIV, TKN_MOD, TKN_AND, TKN_NONE};

namespace
{
    /// <summary>
    /// The result type of an arithmetic operator (+ - * /): integer for two integers, real for any mix of integer
    /// and real, otherwise an INCOMPATIBLE_TYPES error and DummyType.
    /// </summary>
    auto ArithmeticResultType(MCTypePtr type1, MCTypePtr type2) -> MCTypePtr
    {
        if (type1 == IntegerTypePtr && type2 == IntegerTypePtr)
        {
            return IntegerTypePtr;
        }

        bool numeric1 = type1 == IntegerTypePtr || type1 == RealTypePtr;
        bool numeric2 = type2 == IntegerTypePtr || type2 == RealTypePtr;

        if (numeric1 && numeric2)
        {
            return RealTypePtr;
        }

        SyntaxError(ABL_ERR_SYNTAX_INCOMPATIBLE_TYPES);
        return &DummyType;
    }

    /// <summary>
    /// The result type of an operator that takes and gives <paramref name="requiredType"/> (and, or, div, mod): an
    /// INCOMPATIBLE_TYPES error when either operand is anything else, with the result type all the same.
    /// </summary>
    auto SameTypeResultType(MCTypePtr type1, MCTypePtr type2, MCTypePtr requiredType) -> MCTypePtr
    {
        if (type1 != requiredType || type2 != requiredType)
        {
            SyntaxError(ABL_ERR_SYNTAX_INCOMPATIBLE_TYPES);
        }

        return requiredType;
    }

    /// <summary>Whether <paramref name="typePtr"/> is an array of char (a string).</summary>
    auto IsCharArray(MCTypePtr typePtr) -> bool
    {
        return typePtr->Form == FRM_ARRAY && typePtr->Info.Array.ElementTypePtr == CharTypePtr;
    }

    /// <summary>
    /// The symbol of a number or string literal: every literal is a symbol of the module scope (SymTableDisplay[1])
    /// named by its text, entered the first time it appears.
    /// </summary>
    auto LiteralSymbol() -> MCSymTableNodePtr
    {
        MCSymTableNodePtr literalIdPtr = SearchSymTable(TokenString, SymTableDisplay[1]);

        if (literalIdPtr == nullptr)
        {
            literalIdPtr = EnterSymTable(TokenString, &SymTableDisplay[1]);
        }

        return literalIdPtr;
    }
}

auto BaseType(MCTypePtr typePtr) -> MCTypePtr
{
    return typePtr;
}

auto CheckRelationalOpTypes(MCTypePtr type1, MCTypePtr type2) -> void
{
    // The same scalar or enumeration type, integer with real, or two strings of the same length.
    if (type1 == type2 && (type1->Form == FRM_SCALAR || type1->Form == FRM_ENUM))
    {
        return;
    }

    if ((type1 == IntegerTypePtr && type2 == RealTypePtr) || (type2 == IntegerTypePtr && type1 == RealTypePtr))
    {
        return;
    }

    if (IsCharArray(type1) && IsCharArray(type2) && type1->Info.Array.ElementCount == type2->Info.Array.ElementCount)
    {
        return;
    }

    SyntaxError(ABL_ERR_SYNTAX_INCOMPATIBLE_TYPES);
}

auto IsAssignTypeCompatible(MCTypePtr targetType, MCTypePtr valueType) -> int
{
    targetType = BaseType(targetType);
    valueType = BaseType(valueType);

    if (targetType == valueType)
    {
        return 1;
    }

    if (targetType == RealTypePtr && valueType == IntegerTypePtr)
    {
        return 1;
    }

    // A string fits in a string target at least as long.
    if (IsCharArray(targetType) && IsCharArray(valueType) &&
        valueType->Info.Array.ElementCount <= targetType->Info.Array.ElementCount)
    {
        return 1;
    }

    return 0;
}

auto Variable(MCSymTableNodePtr variableIdPtr, MCUseType) -> MCTypePtr
{
    MCTypePtr typePtr = variableIdPtr->TypePtr;
    MCDefinitionType defnKey = variableIdPtr->Defn.Key;
    CrunchSymTableNodePtr(variableIdPtr);

    switch (defnKey)
    {
        case DFN_UNDEFINED:
        case DFN_VAR:
        case DFN_VALPARAM:
        case DFN_REFPARAM:
        case DFN_FUNCTION:
            break;
        default:
        {
            typePtr = &DummyType;
            SyntaxError(ABL_ERR_SYNTAX_INVALID_IDENTIFIER_USAGE);
            break;
        }
    }

    GetToken();

    if (CurToken == TKN_LPAREN)
    {
        SyntaxError(ABL_ERR_SYNTAX_UNEXPECTED_TOKEN);
        ActualParamList(variableIdPtr, 0);
        return typePtr;
    }
    while (CurToken == TKN_LBRACKET)
    {
        typePtr = ArraySubscriptList(typePtr);
    }

    if (CurToken == TKN_PERIOD)
    {
        // Record fields: never supported.
        exit(666);
    }

    return typePtr;
}

auto ArraySubscriptList(MCTypePtr typePtr) -> MCTypePtr
{
    do
    {
        if (typePtr->Form == FRM_ARRAY)
        {
            MCTypePtr elementTypePtr = typePtr->Info.Array.ElementTypePtr;
            GetToken();
            MCTypePtr indexTypePtr = Expression();

            if (IsAssignTypeCompatible(typePtr->Info.Array.IndexTypePtr, indexTypePtr) == 0)
            {
                SyntaxError(ABL_ERR_SYNTAX_INCOMPATIBLE_TYPES);
            }

            typePtr = elementTypePtr;
        }
        else
        {
            SyntaxError(ABL_ERR_SYNTAX_TOO_MANY_SUBSCRIPTS);

            while (CurToken != TKN_RBRACKET && TokenIn(StatementEndList) == 0)
            {
                GetToken();
            }
        }
    } while (CurToken == TKN_COMMA);

    IfTokenGetElseError(TKN_RBRACKET, ABL_ERR_SYNTAX_MISSING_RBRACKET);
    return typePtr;
}

auto Factor() -> MCTypePtr
{
    switch (CurToken)
    {
        case TKN_IDENTIFIER:
        {
            MCSymTableNodePtr idPtr = nullptr;
            SearchAndFindAllSymTables(idPtr);

            if (idPtr->Defn.Key == DFN_CONST)
            {
                CrunchSymTableNodePtr(idPtr);
                GetToken();
                return idPtr->TypePtr;
            }

            if (idPtr->Defn.Key != DFN_FUNCTION)
            {
                return Variable(idPtr, USE_EXPR);
            }

            CrunchSymTableNodePtr(idPtr);
            GetToken();
            return RoutineCall(idPtr, 1);
        }

        case TKN_NUMBER:
        {
            MCSymTableNodePtr literalIdPtr = LiteralSymbol();
            MCTypePtr typePtr;

            if (CurLiteral.Type == LIT_INTEGER)
            {
                typePtr = IntegerTypePtr;
                literalIdPtr->Defn.Info.Constant.Value.Integer = CurLiteral.Value.Integer;
            }
            else
            {
                typePtr = RealTypePtr;
                literalIdPtr->Defn.Info.Constant.Value.Real = CurLiteral.Value.Real;
            }

            literalIdPtr->TypePtr = typePtr;
            CrunchSymTableNodePtr(literalIdPtr);
            GetToken();
            return typePtr;
        }

        case TKN_STRING:
        {
            auto length = static_cast<int32_t>(strlen(CurLiteral.Value.String));
            MCSymTableNodePtr literalIdPtr = LiteralSymbol();
            MCTypePtr typePtr = CharTypePtr;

            if (length == 1)
            {
                // A one-character literal is a char; its typePtr is left as it was (execFactor goes by the name).
                literalIdPtr->Defn.Info.Constant.Value.Character = CurLiteral.Value.String[0];
            }
            else
            {
                typePtr = MakeStringType(length);
                literalIdPtr->TypePtr = typePtr;
                literalIdPtr->LiteralString = AblMemory.CopyString(CurLiteral.Value.String);
            }

            CrunchSymTableNodePtr(literalIdPtr);
            GetToken();
            return typePtr;
        }

        case TKN_NOT:
        {
            GetToken();
            return Factor();
        }
        case TKN_LPAREN:
        {
            GetToken();
            MCTypePtr typePtr = Expression();
            IfTokenGetElseError(TKN_RPAREN, ABL_ERR_SYNTAX_MISSING_RPAREN);
            return typePtr;
        }

        default:
        {
            SyntaxError(ABL_ERR_SYNTAX_INVALID_EXPRESSION);
            return &DummyType;
        }
    }
}

auto Term() -> MCTypePtr
{
    MCTypePtr resultType = Factor();

    while (TokenIn(MultiplyOperatorList))
    {
        MCTokenCodeType op = CurToken;
        MCTypePtr operandType1 = BaseType(resultType);
        GetToken();
        MCTypePtr operandType2 = BaseType(Factor());

        switch (op)
        {
            case TKN_STAR:
            case TKN_FSLASH:
                resultType = ArithmeticResultType(operandType1, operandType2);
                break;
            case TKN_AND:
                resultType = SameTypeResultType(operandType1, operandType2, BooleanTypePtr);
                break;
            case TKN_DIV:
            case TKN_MOD:
                resultType = SameTypeResultType(operandType1, operandType2, IntegerTypePtr);
                break;
            default:
                resultType = operandType1;
                break;
        }
    }

    return resultType;
}

auto SimpleExpression() -> MCTypePtr
{
    bool sawSign = false;

    if (CurToken == TKN_PLUS || CurToken == TKN_MINUS)
    {
        sawSign = true;
        GetToken();
    }

    MCTypePtr resultType = Term();

    if (sawSign && BaseType(resultType) != IntegerTypePtr && resultType != RealTypePtr)
    {
        SyntaxError(ABL_ERR_SYNTAX_INCOMPATIBLE_TYPES);
    }

    while (TokenIn(AddOperatorList))
    {
        MCTokenCodeType op = CurToken;
        MCTypePtr operandType1 = BaseType(resultType);
        GetToken();
        MCTypePtr operandType2 = BaseType(Term());

        if (op == TKN_PLUS || op == TKN_MINUS)
        {
            resultType = ArithmeticResultType(operandType1, operandType2);
        }
        else if (op == TKN_OR)
        {
            resultType = SameTypeResultType(operandType1, operandType2, BooleanTypePtr);
        }
        else
        {
            resultType = operandType1;
        }
    }

    return resultType;
}

auto Expression() -> MCTypePtr
{
    MCTypePtr resultType = SimpleExpression();

    if (TokenIn(RelationalOperatorList) == 0)
    {
        return resultType;
    }

    MCTypePtr operandType1 = BaseType(resultType);
    GetToken();
    MCTypePtr operandType2 = BaseType(SimpleExpression());
    CheckRelationalOpTypes(operandType1, operandType2);
    return BooleanTypePtr;
}
