#include "stdafx.h"
#include "abl/MCAblCompiler.h"
#include "abl/MCAblTokenLists.h"

// The expressions: parsed, type-checked and written to the code (ablxexpr.cpp executes them). The original's
// ablexpr.cpp.

namespace
{
    /// <summary>Whether <paramref name="type"/> is an array of char (a string).</summary>
    auto IsCharArray(const MCAblType* type) -> bool
    {
        return type->Form == MCAblTypeForm::Array && type->Array.ElementTypePtr == CharTypePtr;
    }
}

auto MCAblCompiler::LiteralSymbol() -> MCAblSymbol*
{
    // Every literal is a symbol of the module scope named by its text (a string's without the quotes), entered the
    // first time it appears.
    MCAblSymbol* literal = SearchSymTable(_Scanner.Text(), Scope(1));

    if (literal == nullptr)
    {
        literal = EnterSymbol(_Scanner.Text(), Scope(1));
    }
    else if (literal->Defn.Key != MCAblSymbolKind::Undefined)
    {
        _LiteralClashes.push_back(literal->Name);
    }

    return literal;
}

auto MCAblCompiler::ArithmeticResultType(MCAblType* type1, MCAblType* type2) const -> MCAblType*
{
    if (type1 == IntegerTypePtr && type2 == IntegerTypePtr)
    {
        return IntegerTypePtr;
    }

    const bool numeric1 = type1 == IntegerTypePtr || type1 == RealTypePtr;
    const bool numeric2 = type2 == IntegerTypePtr || type2 == RealTypePtr;

    if (!numeric1 || !numeric2)
    {
        SyntaxError(MCAblSyntaxError::IncompatibleTypes);
    }

    return RealTypePtr;
}

auto MCAblCompiler::SameTypeResultType(MCAblType* type1, MCAblType* type2, MCAblType* required) const -> MCAblType*
{
    if (type1 != required || type2 != required)
    {
        SyntaxError(MCAblSyntaxError::IncompatibleTypes);
    }

    return required;
}

auto MCAblCompiler::CheckRelationalOpTypes(MCAblType* type1, MCAblType* type2) const -> void
{
    // The same scalar or enumeration type, integer with real, or two strings of the same length.
    if (type1 == type2 && (type1->Form == MCAblTypeForm::Scalar || type1->Form == MCAblTypeForm::Enum))
    {
        return;
    }

    if ((type1 == IntegerTypePtr && type2 == RealTypePtr) || (type2 == IntegerTypePtr && type1 == RealTypePtr))
    {
        return;
    }

    if (IsCharArray(type1) && IsCharArray(type2) && type1->Array.ElementCount == type2->Array.ElementCount)
    {
        return;
    }

    SyntaxError(MCAblSyntaxError::IncompatibleTypes);
}

auto MCAblCompiler::IsAssignTypeCompatible(MCAblType* targetType, MCAblType* valueType) -> bool
{
    if (targetType == valueType)
    {
        return true;
    }

    if (targetType == RealTypePtr && valueType == IntegerTypePtr)
    {
        return true;
    }

    // A string fits in a string target at least as long.
    return IsCharArray(targetType) && IsCharArray(valueType) &&
           valueType->Array.ElementCount <= targetType->Array.ElementCount;
}

auto MCAblCompiler::Variable(MCAblSymbol* variable) -> MCAblType*
{
    MCAblType* type = variable->TypePtr;
    _Code.WriteSymbol(variable);

    switch (variable->Defn.Key)
    {
        case MCAblSymbolKind::Undefined:
        case MCAblSymbolKind::Var:
        case MCAblSymbolKind::ValueParam:
        case MCAblSymbolKind::RefParam:
        case MCAblSymbolKind::Function:
        {
            break;
        }
        default:
        {
            SyntaxError(MCAblSyntaxError::InvalidIdentifierUsage);
        }
    }

    NextToken();

    if (Token() == MCAblToken::LParen)
    {
        SyntaxError(MCAblSyntaxError::UnexpectedToken);
    }

    while (Token() == MCAblToken::LBracket)
    {
        type = ArraySubscriptList(type);
    }

    if (Token() == MCAblToken::Period)
    {
        // Record fields were never supported: the original quit the game on the spot (exit 666, OB-141).
        SyntaxError(MCAblSyntaxError::UnimplementedFeature);
    }

    return type;
}

auto MCAblCompiler::ArraySubscriptList(MCAblType* type) -> MCAblType*
{
    do
    {
        if (type->Form != MCAblTypeForm::Array)
        {
            SyntaxError(MCAblSyntaxError::TooManySubscripts);
        }

        MCAblType* elementType = type->Array.ElementTypePtr;
        NextToken();

        if (!IsAssignTypeCompatible(type->Array.IndexTypePtr, Expression()))
        {
            SyntaxError(MCAblSyntaxError::IncompatibleTypes);
        }

        type = elementType;
    } while (Token() == MCAblToken::Comma);

    IfTokenGetElseError(MCAblToken::RBracket, MCAblSyntaxError::MissingRBracket);
    return type;
}

auto MCAblCompiler::Factor() -> MCAblType*
{
    switch (Token())
    {
        case MCAblToken::Identifier:
        {
            MCAblSymbol* symbol = SearchAndFindAllSymTables();

            if (symbol->Defn.Key == MCAblSymbolKind::Const)
            {
                _Code.WriteSymbol(symbol);
                NextToken();
                return symbol->TypePtr;
            }

            if (symbol->Defn.Key != MCAblSymbolKind::Function)
            {
                return Variable(symbol);
            }

            _Code.WriteSymbol(symbol);
            NextToken();
            return RoutineCall(symbol);
        }
        case MCAblToken::Number:
        {
            MCAblSymbol* literal = LiteralSymbol();
            const MCAblLiteral& value = _Scanner.Literal();
            MCAblType* type = nullptr;

            if (value.Type == MCAblLiteralType::Integer)
            {
                type = IntegerTypePtr;
                literal->Defn.Info.Constant.Value.Integer = value.Integer;
            }
            else
            {
                type = RealTypePtr;
                literal->Defn.Info.Constant.Value.Real = value.Real;
            }

            literal->TypePtr = type;
            _Code.WriteSymbol(literal);
            NextToken();
            return type;
        }
        case MCAblToken::String:
        {
            const std::string& text = _Scanner.Literal().String;
            MCAblSymbol* literal = LiteralSymbol();
            MCAblType* type = CharTypePtr;

            if (text.size() == 1)
            {
                // A one-character literal is a char; its type is left as it was (the executor goes by the name).
                literal->Defn.Info.Constant.Value.Character = text[0];
            }
            else
            {
                type = _Symbols.MakeStringType(static_cast<int32_t>(text.size()));
                literal->TypePtr = type;
                literal->LiteralText = text;
            }

            _Code.WriteSymbol(literal);
            NextToken();
            return type;
        }
        case MCAblToken::Not:
        {
            NextToken();
            return Factor();
        }
        case MCAblToken::LParen:
        {
            NextToken();
            MCAblType* type = Expression();
            IfTokenGetElseError(MCAblToken::RParen, MCAblSyntaxError::MissingRParen);
            return type;
        }
        default:
        {
            SyntaxError(MCAblSyntaxError::InvalidExpression);
        }
    }
}

auto MCAblCompiler::Term() -> MCAblType*
{
    MCAblType* resultType = Factor();

    while (TokenIn(MCAblTokens::MultiplyOperators))
    {
        const MCAblToken op = Token();
        MCAblType* operandType1 = resultType;
        NextToken();
        MCAblType* operandType2 = Factor();

        if (op == MCAblToken::And)
        {
            resultType = SameTypeResultType(operandType1, operandType2, BooleanTypePtr);
        }
        else if (op == MCAblToken::Div || op == MCAblToken::Mod)
        {
            resultType = SameTypeResultType(operandType1, operandType2, IntegerTypePtr);
        }
        else
        {
            resultType = ArithmeticResultType(operandType1, operandType2);
        }
    }

    return resultType;
}

auto MCAblCompiler::SimpleExpression() -> MCAblType*
{
    bool sawSign = false;

    if (Token() == MCAblToken::Plus || Token() == MCAblToken::Minus)
    {
        sawSign = true;
        NextToken();
    }

    MCAblType* resultType = Term();

    if (sawSign && resultType != IntegerTypePtr && resultType != RealTypePtr)
    {
        SyntaxError(MCAblSyntaxError::IncompatibleTypes);
    }

    while (TokenIn(MCAblTokens::AddOperators))
    {
        const MCAblToken op = Token();
        MCAblType* operandType1 = resultType;
        NextToken();
        MCAblType* operandType2 = Term();

        resultType = op == MCAblToken::Or ? SameTypeResultType(operandType1, operandType2, BooleanTypePtr)
                                          : ArithmeticResultType(operandType1, operandType2);
    }

    return resultType;
}

auto MCAblCompiler::Expression() -> MCAblType*
{
    MCAblType* resultType = SimpleExpression();

    if (!TokenIn(MCAblTokens::RelationalOperators))
    {
        return resultType;
    }

    NextToken();
    CheckRelationalOpTypes(resultType, SimpleExpression());
    return BooleanTypePtr;
}
