#include "stdafx.h"
#include "abl/MCAblCompiler.h"
#include "abl/MCAblTokenLists.h"
#include "abl/MCAblRuntime.h"
#include "lib/MCFatal.h"

// The declarations: const, type and var blocks (with static and eternal variables), and the types they build
// (enumerations, arrays, strings as char arrays). The original's abldecl.cpp.

auto MCAblCompiler::Declarations(MCAblSymbol& routine, bool allowFunctions) -> void
{
    using namespace MCAblTokens;

    if (Token() == MCAblToken::Const)
    {
        NextToken();
        ConstDefinitions();
    }

    if (Token() == MCAblToken::Type)
    {
        NextToken();
        TypeDefinitions();
    }

    if (Token() == MCAblToken::Var)
    {
        NextToken();
        VarDeclarations(routine);
    }

    if (!allowFunctions)
    {
        if (Token() == MCAblToken::Function)
        {
            SyntaxError(MCAblSyntaxError::NoFunctionNesting);
        }

        return;
    }

    while (Token() == MCAblToken::Function)
    {
        Routine();
        Synchronize(FollowRoutine, DeclarationStart, StatementStart);
        SkipSemicolon();
    }
}

auto MCAblCompiler::SkipSemicolon() -> void
{
    using namespace MCAblTokens;

    if (Token() == MCAblToken::Semicolon)
    {
        NextToken();
    }
    else if (TokenIn(DeclarationStart) || TokenIn(StatementStart))
    {
        SyntaxError(MCAblSyntaxError::MissingSemicolon);
    }
}

auto MCAblCompiler::ConstDefinitions() -> void
{
    using namespace MCAblTokens;

    while (Token() == MCAblToken::Identifier)
    {
        MCAblSymbol& constant = SearchAndEnterLocalSymTable();
        constant.Defn.Key = MCAblSymbolKind::Const;
        constant.Library = _Library;
        NextToken();
        IfTokenGetElseError(MCAblToken::Equal, MCAblSyntaxError::MissingEqual);
        DoConst(constant);
        Synchronize(FollowDeclaration, DeclarationStart, StatementStart);
        SkipSemicolon();
    }
}

auto MCAblCompiler::DoConst(MCAblSymbol& constant) -> void
{
    bool negative = false;
    bool sawSign = false;

    if (Token() == MCAblToken::Plus || Token() == MCAblToken::Minus)
    {
        negative = Token() == MCAblToken::Minus;
        sawSign = true;
        NextToken();
    }

    MCAblValue& value = constant.Defn.Info.Constant.Value;
    const MCAblLiteral& literal = _Scanner.Literal();

    switch (Token())
    {
        case MCAblToken::Number:
        {
            if (literal.Type == MCAblLiteralType::Integer)
            {
                value.Integer = negative ? -literal.Integer : literal.Integer;
                constant.Type = MCAblTypeId::Integer;
            }
            else
            {
                value.Real = negative ? -literal.Real : literal.Real;
                constant.Type = MCAblTypeId::Real;
            }

            break;
        }
        case MCAblToken::Identifier:
        {
            // Another constant's value.
            const MCAblSymbolId otherId = SearchSymTableDisplay(_Scanner.Word());

            if (otherId == MCAblSymbolId::None)
            {
                SyntaxError(MCAblSyntaxError::UndefinedIdentifier);
            }

            const MCAblSymbol& other = Symbol(otherId);

            if (other.Defn.Key != MCAblSymbolKind::Const)
            {
                SyntaxError(MCAblSyntaxError::NotAConstantIdentifier);
            }

            const MCAblValue& otherValue = other.Defn.Info.Constant.Value;
            const MCAblTypeId type = other.Type;

            if (type == MCAblTypeId::Integer)
            {
                value.Integer = negative ? -otherValue.Integer : otherValue.Integer;
                constant.Type = MCAblTypeId::Integer;
            }
            else if (type == MCAblTypeId::Char)
            {
                if (sawSign)
                {
                    SyntaxError(MCAblSyntaxError::InvalidConstant);
                }

                value.Character = otherValue.Character;
                constant.Type = MCAblTypeId::Char;
            }
            else if (type == MCAblTypeId::Real)
            {
                value.Real = negative ? -otherValue.Real : otherValue.Real;
                constant.Type = MCAblTypeId::Real;
            }
            else if (_Symbols.Type(type).Form == MCAblTypeForm::Enum ||
                     _Symbols.Type(type).Form == MCAblTypeForm::Array)
            {
                // An enumeration value, or a string constant (the text is shared).
                if (sawSign)
                {
                    SyntaxError(MCAblSyntaxError::InvalidConstant);
                }

                value = otherValue;
                constant.Type = type;
            }

            break;
        }
        case MCAblToken::String:
        {
            if (sawSign)
            {
                SyntaxError(MCAblSyntaxError::InvalidConstant);
            }

            const auto length = static_cast<int32_t>(literal.String.size());

            if (length == 1)
            {
                value.Character = literal.String[0];
                constant.Type = MCAblTypeId::Char;
            }
            else
            {
                // The symbol keeps the text.
                constant.LiteralText = literal.String;
                value.Text = constant.Id;
                constant.Type = _Symbols.MakeStringType(length);
            }

            break;
        }
        default:
        {
            SyntaxError(MCAblSyntaxError::InvalidConstant);
        }
    }

    NextToken();
}

auto MCAblCompiler::TypeDefinitions() -> void
{
    using namespace MCAblTokens;

    while (Token() == MCAblToken::Identifier)
    {
        MCAblSymbol& typeSymbol = SearchAndEnterLocalSymTable();
        typeSymbol.Defn.Key = MCAblSymbolKind::Type;
        typeSymbol.Library = _Library;
        NextToken();
        IfTokenGetElseError(MCAblToken::Equal, MCAblSyntaxError::MissingEqual);
        typeSymbol.Type = DoType();
        Synchronize(FollowDeclaration, DeclarationStart, StatementStart);
        SkipSemicolon();
    }
}

auto MCAblCompiler::DoType() -> MCAblTypeId
{
    using namespace MCAblTokens;

    switch (Token())
    {
        case MCAblToken::Identifier:
        {
            break;
        }
        case MCAblToken::Number:
        case MCAblToken::String:
        case MCAblToken::Minus:
        case MCAblToken::Plus:
        {
            // Subrange types were never supported: the original quit the game on the spot (exit 666, OB-141).
            SyntaxError(MCAblSyntaxError::UnimplementedFeature);
        }
        case MCAblToken::LParen:
        {
            return EnumerationType();
        }
        default:
        {
            SyntaxError(MCAblSyntaxError::InvalidType);
        }
    }

    const MCAblSymbolId typeSymbol = SearchSymTableDisplay(_Scanner.Word());

    if (typeSymbol == MCAblSymbolId::None)
    {
        SyntaxError(MCAblSyntaxError::UndefinedIdentifier);
    }

    if (Symbol(typeSymbol).Defn.Key != MCAblSymbolKind::Type)
    {
        SyntaxError(MCAblSyntaxError::NotATypeIdentifier);
    }

    NextToken();
    const MCAblTypeId elementType = Symbol(typeSymbol).Type;

    if (Token() != MCAblToken::LBracket)
    {
        return elementType;
    }

    // "type[d1, d2, ...]": one array type per dimension, each the element type of the one before.
    const MCAblTypeId arrayType = _Symbols.MakeType().Id;
    MCAblTypeId dimensionType = arrayType;

    while (true)
    {
        NextToken();
        bool validIndex = false;
        MCAblType& dimension = _Symbols.Type(dimensionType);

        if (TokenIn(IndexTypeStart))
        {
            dimension.Form = MCAblTypeForm::Array;
            dimension.Size = 0;
            dimension.Array.IndexType = MCAblTypeId::Integer;

            if (Token() == MCAblToken::Identifier)
            {
                const MCAblSymbolId countSymbol = SearchSymTableDisplay(_Scanner.Word());

                if (countSymbol == MCAblSymbolId::None)
                {
                    SyntaxError(MCAblSyntaxError::UndefinedIdentifier);
                }

                if (const MCAblSymbol& count = Symbol(countSymbol);
                    count.Defn.Key == MCAblSymbolKind::Const && count.Type == MCAblTypeId::Integer)
                {
                    dimension.Array.ElementCount = count.Defn.Info.Constant.Value.Integer;
                    validIndex = true;
                }
            }
            else if (_Scanner.Literal().Type == MCAblLiteralType::Integer)
            {
                dimension.Array.ElementCount = _Scanner.Literal().Integer;
                validIndex = true;
            }
        }

        if (!validIndex)
        {
            SyntaxError(MCAblSyntaxError::InvalidIndexType);
        }

        NextToken();
        Synchronize(FollowDimension);

        if (Token() != MCAblToken::Comma)
        {
            break;
        }

        const MCAblTypeId nextDimensionType = _Symbols.MakeType().Id;
        _Symbols.Type(dimensionType).Array.ElementType = nextDimensionType;
        dimensionType = nextDimensionType;
    }

    IfTokenGetElseError(MCAblToken::RBracket, MCAblSyntaxError::MissingRBracket);
    _Symbols.Type(dimensionType).Array.ElementType = elementType;
    MCAblType& array = _Symbols.Type(arrayType);
    array.Size = ArraySize(array);
    return arrayType;
}

auto MCAblCompiler::EnumerationType() -> MCAblTypeId
{
    MCAblType& type = _Symbols.MakeType();
    type.Form = MCAblTypeForm::Enum;
    type.Size = 4;
    int32_t value = -1;
    NextToken();

    while (Token() == MCAblToken::Identifier)
    {
        MCAblSymbol& constant = SearchAndEnterLocalSymTable();
        constant.Defn.Key = MCAblSymbolKind::Const;
        constant.Defn.Info.Constant.Value.Integer = ++value;
        constant.Type = type.Id;
        constant.Library = _Library;
        NextToken();
        IfTokenGet(MCAblToken::Comma);
    }

    IfTokenGetElseError(MCAblToken::RParen, MCAblSyntaxError::MissingRParen);
    return type.Id;
}

auto MCAblCompiler::ArraySize(MCAblType& type) -> int32_t
{
    MCAblType& elementType = _Symbols.Type(type.Array.ElementType);

    if (elementType.Size == 0)
    {
        elementType.Size = ArraySize(elementType);
    }

    type.Size = type.Array.ElementCount == -1 ? elementType.Size : elementType.Size * type.Array.ElementCount;
    return type.Size;
}

auto MCAblCompiler::VarDeclarations(MCAblSymbol& routine) -> void
{
    using namespace MCAblTokens;

    auto& routineInfo = routine.Defn.Info.Routine;
    // Locals follow the frame header and the parameters.
    int32_t offset = routineInfo.TotalParamSize + AblFrameHeaderItems;
    MCAblSymbolId previous = MCAblSymbolId::None;
    // The last variable of the previous line (not eternal), linked to the first of the next.
    MCAblSymbolId previousLineLast = MCAblSymbolId::None;

    while (Token() == MCAblToken::Identifier || Token() == MCAblToken::Eternal || Token() == MCAblToken::Static)
    {
        MCAblStorage storage = MCAblStorage::Normal;

        if (Token() == MCAblToken::Eternal || Token() == MCAblToken::Static)
        {
            storage = Token() == MCAblToken::Eternal ? MCAblStorage::Eternal : MCAblStorage::Static;
            NextToken();

            if (Token() != MCAblToken::Identifier)
            {
                SyntaxError(MCAblSyntaxError::MissingIdentifier);
            }
        }

        // "type name, name, ...;"
        MCAblSymbolId firstOfLine = MCAblSymbolId::None;
        const MCAblTypeId type = DoType();
        const MCAblType& typeInfo = _Symbols.Type(type);
        const int32_t size = typeInfo.Size;

        while (Token() == MCAblToken::Identifier)
        {
            MCAblSymbolId variableId = MCAblSymbolId::None;

            if (storage == MCAblStorage::Eternal)
            {
                // Eternals are global: entered at level 0.
                const int32_t savedLevel = _Level;
                _Level = 0;
                variableId = SearchAndEnterLocalSymTable().Id;
                _Level = savedLevel;
            }
            else
            {
                variableId = SearchAndEnterLocalSymTable().Id;
            }

            MCAblSymbol& variable = Symbol(variableId);
            variable.Library = _Library;
            variable.Defn.Key = MCAblSymbolKind::Var;

            if (firstOfLine == MCAblSymbolId::None)
            {
                firstOfLine = variableId;

                if (storage != MCAblStorage::Eternal && routineInfo.Locals == MCAblSymbolId::None)
                {
                    routineInfo.Locals = variableId;
                }
            }
            else
            {
                Symbol(previous).Next = variableId;
            }

            NextToken();
            IfTokenGet(MCAblToken::Comma);
            previous = variableId;
        }

        for (MCAblSymbolId variableId = firstOfLine; variableId != MCAblSymbolId::None;
             variableId = Symbol(variableId).Next)
        {
            MCAblSymbol& variable = Symbol(variableId);
            variable.Type = type;
            auto& data = variable.Defn.Info.Data;
            data.VarType = storage;

            switch (storage)
            {
                case MCAblStorage::Normal:
                {
                    data.Offset = offset++;
                    break;
                }
                case MCAblStorage::Static:
                {
                    // The original's limit (the scenario's AblMaxStaticVariables) is gone. Arrays record their
                    // byte size (MCAblModule allocates them); scalars 0.
                    data.Offset = static_cast<int32_t>(_StaticSizes.size());
                    _StaticSizes.push_back(typeInfo.Form == MCAblTypeForm::Array ? size : 0);
                    break;
                }
                case MCAblStorage::Eternal:
                {
                    // An item at the bottom of the runtime stack (an array's item holds its block's address).
                    data.Offset = AblRuntime()->DeclareEternal(typeInfo);
                    break;
                }
            }
        }

        if (storage != MCAblStorage::Eternal)
        {
            if (previousLineLast != MCAblSymbolId::None)
            {
                Symbol(previousLineLast).Next = firstOfLine;
            }

            previousLineLast = previous;
        }

        Synchronize(FollowVariables, DeclarationStart, StatementStart);
        SkipSemicolon();
    }

    Synchronize(FollowVarBlock);
}
