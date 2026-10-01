#include "stdafx.h"
#include "abl/ablexpr.h"
#include "abl/abldecl.h"
#include "abl/ablerr.h"
#include "abl/ablexec.h"
#include "abl/ablrtn.h"
#include "abl/ablscan.h"
#include "abl/ablstmt.h"
#include "abl/ablsymt.h"
#include "lib/aerror.h"
#include "lib/heap.h"

TokenCodeType relationalOperatorList[] = {TKN_LT, TKN_LE, TKN_EQUALEQUAL, TKN_NE, TKN_GE, TKN_GT, TKN_NONE};
TokenCodeType addOperatorList[] = {TKN_PLUS, TKN_MINUS, TKN_OR, TKN_NONE};
TokenCodeType multiplyOperatorList[] = {TKN_STAR, TKN_FSLASH, TKN_DIV, TKN_MOD, TKN_AND, TKN_NONE};

namespace
{
    /// <summary>
    /// The result type of an arithmetic operator (+ - * /): integer for two integers, real for any mix of integer
    /// and real, otherwise an INCOMPATIBLE_TYPES error and DummyType.
    /// </summary>
    auto arithmeticResultType(TypePtr type1, TypePtr type2) -> TypePtr
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

        syntaxError(ABL_ERR_SYNTAX_INCOMPATIBLE_TYPES);
        return &DummyType;
    }

    /// <summary>
    /// The result type of an operator that takes and gives <paramref name="requiredType"/> (and, or, div, mod): an
    /// INCOMPATIBLE_TYPES error when either operand is anything else, with the result type all the same.
    /// </summary>
    auto sameTypeResultType(TypePtr type1, TypePtr type2, TypePtr requiredType) -> TypePtr
    {
        if (type1 != requiredType || type2 != requiredType)
        {
            syntaxError(ABL_ERR_SYNTAX_INCOMPATIBLE_TYPES);
        }

        return requiredType;
    }

    /// <summary>Whether <paramref name="typePtr"/> is an array of char (a string).</summary>
    auto isCharArray(TypePtr typePtr) -> bool
    {
        return typePtr->form == FRM_ARRAY && typePtr->info.array.elementTypePtr == CharTypePtr;
    }

    /// <summary>
    /// The symbol of a number or string literal: every literal is a symbol of the module scope (SymTableDisplay[1])
    /// named by its text, entered the first time it appears.
    /// </summary>
    auto literalSymbol() -> SymTableNodePtr
    {
        SymTableNodePtr literalIdPtr = searchSymTable(tokenString, SymTableDisplay[1]);

        if (literalIdPtr == nullptr)
        {
            literalIdPtr = enterSymTable(tokenString, &SymTableDisplay[1]);
        }

        return literalIdPtr;
    }
}

auto baseType(TypePtr typePtr) -> TypePtr
{
    return typePtr;
}

auto checkRelationalOpTypes(TypePtr type1, TypePtr type2) -> void
{
    // The same scalar or enumeration type, integer with real, or two strings of the same length.
    if (type1 == type2 && (type1->form == FRM_SCALAR || type1->form == FRM_ENUM))
    {
        return;
    }

    if ((type1 == IntegerTypePtr && type2 == RealTypePtr) || (type2 == IntegerTypePtr && type1 == RealTypePtr))
    {
        return;
    }

    if (isCharArray(type1) && isCharArray(type2) && type1->info.array.elementCount == type2->info.array.elementCount)
    {
        return;
    }

    syntaxError(ABL_ERR_SYNTAX_INCOMPATIBLE_TYPES);
}

auto isAssignTypeCompatible(TypePtr targetType, TypePtr valueType) -> int
{
    targetType = baseType(targetType);
    valueType = baseType(valueType);

    if (targetType == valueType)
    {
        return 1;
    }

    if (targetType == RealTypePtr && valueType == IntegerTypePtr)
    {
        return 1;
    }

    // A string fits in a string target at least as long.
    if (isCharArray(targetType) && isCharArray(valueType) &&
        valueType->info.array.elementCount <= targetType->info.array.elementCount)
    {
        return 1;
    }

    return 0;
}

auto variable(SymTableNodePtr variableIdPtr, UseType) -> TypePtr
{
    TypePtr typePtr = variableIdPtr->typePtr;
    DefinitionType defnKey = variableIdPtr->defn.key;
    crunchSymTableNodePtr(variableIdPtr);

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
            syntaxError(ABL_ERR_SYNTAX_INVALID_IDENTIFIER_USAGE);
            break;
        }
    }

    getToken();

    if (curToken == TKN_LPAREN)
    {
        syntaxError(ABL_ERR_SYNTAX_UNEXPECTED_TOKEN);
        actualParamList(variableIdPtr, 0);
        return typePtr;
    }
    while (curToken == TKN_LBRACKET)
    {
        typePtr = arraySubscriptList(typePtr);
    }

    if (curToken == TKN_PERIOD)
    {
        // Record fields: never supported.
        exit(666);
    }

    return typePtr;
}

auto arraySubscriptList(TypePtr typePtr) -> TypePtr
{
    do
    {
        if (typePtr->form == FRM_ARRAY)
        {
            TypePtr elementTypePtr = typePtr->info.array.elementTypePtr;
            getToken();
            TypePtr indexTypePtr = expression();

            if (isAssignTypeCompatible(typePtr->info.array.indexTypePtr, indexTypePtr) == 0)
            {
                syntaxError(ABL_ERR_SYNTAX_INCOMPATIBLE_TYPES);
            }

            typePtr = elementTypePtr;
        }
        else
        {
            syntaxError(ABL_ERR_SYNTAX_TOO_MANY_SUBSCRIPTS);

            while (curToken != TKN_RBRACKET && tokenIn(statementEndList) == 0)
            {
                getToken();
            }
        }
    } while (curToken == TKN_COMMA);

    ifTokenGetElseError(TKN_RBRACKET, ABL_ERR_SYNTAX_MISSING_RBRACKET);
    return typePtr;
}

auto factor() -> TypePtr
{
    switch (curToken)
    {
        case TKN_IDENTIFIER:
        {
            SymTableNodePtr idPtr = nullptr;
            searchAndFindAllSymTables(idPtr);

            if (idPtr->defn.key == DFN_CONST)
            {
                crunchSymTableNodePtr(idPtr);
                getToken();
                return idPtr->typePtr;
            }

            if (idPtr->defn.key != DFN_FUNCTION)
            {
                return variable(idPtr, USE_EXPR);
            }

            crunchSymTableNodePtr(idPtr);
            getToken();
            return routineCall(idPtr, 1);
        }

        case TKN_NUMBER:
        {
            SymTableNodePtr literalIdPtr = literalSymbol();
            TypePtr typePtr;

            if (curLiteral.type == LIT_INTEGER)
            {
                typePtr = IntegerTypePtr;
                literalIdPtr->defn.info.constant.value.integer = curLiteral.value.integer;
            }
            else
            {
                typePtr = RealTypePtr;
                literalIdPtr->defn.info.constant.value.real = curLiteral.value.real;
            }

            literalIdPtr->typePtr = typePtr;
            crunchSymTableNodePtr(literalIdPtr);
            getToken();
            return typePtr;
        }

        case TKN_STRING:
        {
            auto length = static_cast<int32_t>(strlen(curLiteral.value.string));
            SymTableNodePtr literalIdPtr = literalSymbol();
            TypePtr typePtr = CharTypePtr;

            if (length == 1)
            {
                // A one-character literal is a char; its typePtr is left as it was (execFactor goes by the name).
                literalIdPtr->defn.info.constant.value.character = curLiteral.value.string[0];
            }
            else
            {
                typePtr = makeStringType(length);
                literalIdPtr->typePtr = typePtr;
                literalIdPtr->literalString =
                    static_cast<char*>(AblSymTableHeap->malloc(static_cast<uint32_t>(length + 1)));

                if (literalIdPtr->literalString == nullptr)
                {
                    Fatal(0, " ABL: Unable to AblSymTableHeap->malloc string literal ");
                }

                strcpy(literalIdPtr->literalString, curLiteral.value.string);
            }

            crunchSymTableNodePtr(literalIdPtr);
            getToken();
            return typePtr;
        }

        case TKN_NOT:
        {
            getToken();
            return factor();
        }
        case TKN_LPAREN:
        {
            getToken();
            TypePtr typePtr = expression();
            ifTokenGetElseError(TKN_RPAREN, ABL_ERR_SYNTAX_MISSING_RPAREN);
            return typePtr;
        }

        default:
        {
            syntaxError(ABL_ERR_SYNTAX_INVALID_EXPRESSION);
            return &DummyType;
        }
    }
}

auto term() -> TypePtr
{
    TypePtr resultType = factor();

    while (tokenIn(multiplyOperatorList))
    {
        TokenCodeType op = curToken;
        TypePtr operandType1 = baseType(resultType);
        getToken();
        TypePtr operandType2 = baseType(factor());

        switch (op)
        {
            case TKN_STAR:
            case TKN_FSLASH:
                resultType = arithmeticResultType(operandType1, operandType2);
                break;
            case TKN_AND:
                resultType = sameTypeResultType(operandType1, operandType2, BooleanTypePtr);
                break;
            case TKN_DIV:
            case TKN_MOD:
                resultType = sameTypeResultType(operandType1, operandType2, IntegerTypePtr);
                break;
            default:
                resultType = operandType1;
                break;
        }
    }

    return resultType;
}

auto simpleExpression() -> TypePtr
{
    bool sawSign = false;

    if (curToken == TKN_PLUS || curToken == TKN_MINUS)
    {
        sawSign = true;
        getToken();
    }

    TypePtr resultType = term();

    if (sawSign && baseType(resultType) != IntegerTypePtr && resultType != RealTypePtr)
    {
        syntaxError(ABL_ERR_SYNTAX_INCOMPATIBLE_TYPES);
    }

    while (tokenIn(addOperatorList))
    {
        TokenCodeType op = curToken;
        TypePtr operandType1 = baseType(resultType);
        getToken();
        TypePtr operandType2 = baseType(term());

        if (op == TKN_PLUS || op == TKN_MINUS)
        {
            resultType = arithmeticResultType(operandType1, operandType2);
        }
        else if (op == TKN_OR)
        {
            resultType = sameTypeResultType(operandType1, operandType2, BooleanTypePtr);
        }
        else
        {
            resultType = operandType1;
        }
    }

    return resultType;
}

auto expression() -> TypePtr
{
    TypePtr resultType = simpleExpression();

    if (tokenIn(relationalOperatorList) == 0)
    {
        return resultType;
    }

    TypePtr operandType1 = baseType(resultType);
    getToken();
    TypePtr operandType2 = baseType(simpleExpression());
    checkRelationalOpTypes(operandType1, operandType2);
    return BooleanTypePtr;
}
