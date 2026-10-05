#include "stdafx.h"
#include "abl/MCAblCompiler.h"
#include "abl/MCAblTokenLists.h"

// The expressions: parsed, type-checked and written to the code (ablxexpr.cpp executes them). The original's
// ablexpr.cpp.

auto MCAblCompiler::LiteralSymbol() -> MCAblSymbol&
{
    // Every literal is a symbol of the module scope named by its text (a string's without the quotes), entered the
    // first time it appears.
    const MCAblSymbolId found = _Symbols.SearchSymTable(_Scanner.Text(), Scope(1));

    if (found == MCAblSymbolId::None)
    {
        return EnterSymbol(_Scanner.Text(), Scope(1));
    }

    MCAblSymbol& literal = Symbol(found);

    if (literal.Defn.Key != MCAblSymbolKind::Undefined)
    {
        _LiteralClashes.push_back(literal.Name);
    }

    return literal;
}

namespace
{
    /// <summary>Whether <paramref name="type"/> (of <paramref name="symbols"/>) is an array of char (a string).</summary>
    auto IsCharArray(const MCAblSymbolTable& symbols, MCAblTypeId type) -> bool
    {
        // A routine without a result has no type (the original read through its null pointer).
        return type != MCAblTypeId::None && symbols.Type(type).Form == MCAblTypeForm::Array &&
               symbols.Type(type).Array.ElementType == MCAblTypeId::Char;
    }
}

auto MCAblCompiler::ArithmeticResultType(MCAblTypeId type1, MCAblTypeId type2) const -> MCAblTypeId
{
    if (type1 == MCAblTypeId::Integer && type2 == MCAblTypeId::Integer)
    {
        return MCAblTypeId::Integer;
    }

    const bool numeric1 = type1 == MCAblTypeId::Integer || type1 == MCAblTypeId::Real;
    const bool numeric2 = type2 == MCAblTypeId::Integer || type2 == MCAblTypeId::Real;

    if (!numeric1 || !numeric2)
    {
        SyntaxError(MCAblSyntaxError::IncompatibleTypes);
    }

    return MCAblTypeId::Real;
}

auto MCAblCompiler::SameTypeResultType(MCAblTypeId type1, MCAblTypeId type2, MCAblTypeId required) const -> MCAblTypeId
{
    if (type1 != required || type2 != required)
    {
        SyntaxError(MCAblSyntaxError::IncompatibleTypes);
    }

    return required;
}

auto MCAblCompiler::CheckRelationalOpTypes(MCAblTypeId type1, MCAblTypeId type2) const -> void
{
    // The same scalar or enumeration type, integer with real, or two strings of the same length.
    if (type1 == type2 && type1 != MCAblTypeId::None &&
        (_Symbols.Type(type1).Form == MCAblTypeForm::Scalar || _Symbols.Type(type1).Form == MCAblTypeForm::Enum))
    {
        return;
    }

    if ((type1 == MCAblTypeId::Integer && type2 == MCAblTypeId::Real) ||
        (type2 == MCAblTypeId::Integer && type1 == MCAblTypeId::Real))
    {
        return;
    }

    if (IsCharArray(_Symbols, type1) && IsCharArray(_Symbols, type2) &&
        _Symbols.Type(type1).Array.ElementCount == _Symbols.Type(type2).Array.ElementCount)
    {
        return;
    }

    SyntaxError(MCAblSyntaxError::IncompatibleTypes);
}

auto MCAblCompiler::IsAssignTypeCompatible(MCAblTypeId targetType, MCAblTypeId valueType) const -> bool
{
    if (targetType == valueType)
    {
        return true;
    }

    if (targetType == MCAblTypeId::Real && valueType == MCAblTypeId::Integer)
    {
        return true;
    }

    // A string fits in a string target at least as long.
    return IsCharArray(_Symbols, targetType) && IsCharArray(_Symbols, valueType) &&
           _Symbols.Type(valueType).Array.ElementCount <= _Symbols.Type(targetType).Array.ElementCount;
}

auto MCAblCompiler::Variable(const MCAblSymbol& variable) -> MCAblTypeId
{
    MCAblTypeId type = variable.Type;
    _Code.WriteSymbol(variable);

    switch (variable.Defn.Key)
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

auto MCAblCompiler::ArraySubscriptList(MCAblTypeId type) -> MCAblTypeId
{
    do
    {
        if (type == MCAblTypeId::None || _Symbols.Type(type).Form != MCAblTypeForm::Array)
        {
            SyntaxError(MCAblSyntaxError::TooManySubscripts);
        }

        const MCAblTypeId elementType = _Symbols.Type(type).Array.ElementType;
        const MCAblTypeId indexType = _Symbols.Type(type).Array.IndexType;
        NextToken();

        if (!IsAssignTypeCompatible(indexType, Expression()))
        {
            SyntaxError(MCAblSyntaxError::IncompatibleTypes);
        }

        type = elementType;
    } while (Token() == MCAblToken::Comma);

    IfTokenGetElseError(MCAblToken::RBracket, MCAblSyntaxError::MissingRBracket);
    return type;
}

auto MCAblCompiler::Factor() -> MCAblTypeId
{
    switch (Token())
    {
        case MCAblToken::Identifier:
        {
            const MCAblSymbol& symbol = SearchAndFindAllSymTables();

            if (symbol.Defn.Key == MCAblSymbolKind::Const)
            {
                _Code.WriteSymbol(symbol);
                NextToken();
                return symbol.Type;
            }

            if (symbol.Defn.Key != MCAblSymbolKind::Function)
            {
                return Variable(symbol);
            }

            _Code.WriteSymbol(symbol);
            NextToken();
            return RoutineCall(symbol);
        }
        case MCAblToken::Number:
        {
            MCAblSymbol& literal = LiteralSymbol();
            const MCAblLiteral& value = _Scanner.Literal();
            MCAblTypeId type = MCAblTypeId::None;

            if (value.Type == MCAblLiteralType::Integer)
            {
                type = MCAblTypeId::Integer;
                literal.Defn.Info.Constant.Value.Integer = value.Integer;
            }
            else
            {
                type = MCAblTypeId::Real;
                literal.Defn.Info.Constant.Value.Real = value.Real;
            }

            literal.Type = type;
            _Code.WriteSymbol(literal);
            NextToken();
            return type;
        }
        case MCAblToken::String:
        {
            const std::string& text = _Scanner.Literal().String;
            MCAblSymbol& literal = LiteralSymbol();
            MCAblTypeId type = MCAblTypeId::Char;

            if (text.size() == 1)
            {
                // A one-character literal is a char; its type is left as it was (the executor goes by the name).
                literal.Defn.Info.Constant.Value.Character = text[0];
            }
            else
            {
                type = _Symbols.MakeStringType(static_cast<int32_t>(text.size()));
                literal.Type = type;
                literal.LiteralText = text;
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
            const MCAblTypeId type = Expression();
            IfTokenGetElseError(MCAblToken::RParen, MCAblSyntaxError::MissingRParen);
            return type;
        }
        default:
        {
            SyntaxError(MCAblSyntaxError::InvalidExpression);
        }
    }
}

auto MCAblCompiler::Term() -> MCAblTypeId
{
    MCAblTypeId resultType = Factor();

    while (TokenIn(MCAblTokens::MultiplyOperators))
    {
        const MCAblToken op = Token();
        const MCAblTypeId operandType1 = resultType;
        NextToken();
        const MCAblTypeId operandType2 = Factor();

        if (op == MCAblToken::And)
        {
            resultType = SameTypeResultType(operandType1, operandType2, MCAblTypeId::Boolean);
        }
        else if (op == MCAblToken::Div || op == MCAblToken::Mod)
        {
            resultType = SameTypeResultType(operandType1, operandType2, MCAblTypeId::Integer);
        }
        else
        {
            resultType = ArithmeticResultType(operandType1, operandType2);
        }
    }

    return resultType;
}

auto MCAblCompiler::SimpleExpression() -> MCAblTypeId
{
    bool sawSign = false;

    if (Token() == MCAblToken::Plus || Token() == MCAblToken::Minus)
    {
        sawSign = true;
        NextToken();
    }

    MCAblTypeId resultType = Term();

    if (sawSign && resultType != MCAblTypeId::Integer && resultType != MCAblTypeId::Real)
    {
        SyntaxError(MCAblSyntaxError::IncompatibleTypes);
    }

    while (TokenIn(MCAblTokens::AddOperators))
    {
        const MCAblToken op = Token();
        const MCAblTypeId operandType1 = resultType;
        NextToken();
        const MCAblTypeId operandType2 = Term();

        resultType = op == MCAblToken::Or ? SameTypeResultType(operandType1, operandType2, MCAblTypeId::Boolean)
                                          : ArithmeticResultType(operandType1, operandType2);
    }

    return resultType;
}

auto MCAblCompiler::Expression() -> MCAblTypeId
{
    const MCAblTypeId resultType = SimpleExpression();

    if (!TokenIn(MCAblTokens::RelationalOperators))
    {
        return resultType;
    }

    NextToken();
    CheckRelationalOpTypes(resultType, SimpleExpression());
    return MCAblTypeId::Boolean;
}
