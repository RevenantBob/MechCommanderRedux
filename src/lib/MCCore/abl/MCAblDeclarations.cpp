#include "stdafx.h"
#include "abl/MCAblCompiler.h"
#include "abl/MCAblTokenLists.h"
#include "abl/MCAblRuntime.h"
#include "lib/MCFatal.h"

// The declarations: const, type and var blocks (with static and eternal variables), and the types they build
// (enumerations, arrays, strings as char arrays). The original's abldecl.cpp.

auto MCAblCompiler::Declarations(MCAblSymbol* routine, bool allowFunctions) -> void
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
        MCAblSymbol* constant = SearchAndEnterLocalSymTable();
        constant->Defn.Key = MCAblSymbolKind::Const;
        constant->Library = _Library;
        NextToken();
        IfTokenGetElseError(MCAblToken::Equal, MCAblSyntaxError::MissingEqual);
        DoConst(constant);
        Synchronize(FollowDeclaration, DeclarationStart, StatementStart);
        SkipSemicolon();
    }
}

auto MCAblCompiler::DoConst(MCAblSymbol* constant) -> void
{
    bool negative = false;
    bool sawSign = false;

    if (Token() == MCAblToken::Plus || Token() == MCAblToken::Minus)
    {
        negative = Token() == MCAblToken::Minus;
        sawSign = true;
        NextToken();
    }

    MCAblValue& value = constant->Defn.Info.Constant.Value;
    const MCAblLiteral& literal = _Scanner.Literal();

    switch (Token())
    {
        case MCAblToken::Number:
        {
            if (literal.Type == MCAblLiteralType::Integer)
            {
                value.Integer = negative ? -literal.Integer : literal.Integer;
                constant->TypePtr = IntegerTypePtr;
            }
            else
            {
                value.Real = negative ? -literal.Real : literal.Real;
                constant->TypePtr = RealTypePtr;
            }

            break;
        }
        case MCAblToken::Identifier:
        {
            // Another constant's value.
            const MCAblSymbol* other = SearchSymTableDisplay(_Scanner.Word());

            if (other == nullptr)
            {
                SyntaxError(MCAblSyntaxError::UndefinedIdentifier);
            }

            if (other->Defn.Key != MCAblSymbolKind::Const)
            {
                SyntaxError(MCAblSyntaxError::NotAConstantIdentifier);
            }

            const MCAblValue& otherValue = other->Defn.Info.Constant.Value;
            MCAblType* type = other->TypePtr;

            if (type == IntegerTypePtr)
            {
                value.Integer = negative ? -otherValue.Integer : otherValue.Integer;
                constant->TypePtr = IntegerTypePtr;
            }
            else if (type == CharTypePtr)
            {
                if (sawSign)
                {
                    SyntaxError(MCAblSyntaxError::InvalidConstant);
                }

                value.Character = otherValue.Character;
                constant->TypePtr = CharTypePtr;
            }
            else if (type == RealTypePtr)
            {
                value.Real = negative ? -otherValue.Real : otherValue.Real;
                constant->TypePtr = RealTypePtr;
            }
            else if (type->Form == MCAblTypeForm::Enum || type->Form == MCAblTypeForm::Array)
            {
                // An enumeration value, or a string constant (the text is shared).
                if (sawSign)
                {
                    SyntaxError(MCAblSyntaxError::InvalidConstant);
                }

                value = otherValue;
                constant->TypePtr = type;
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
                constant->TypePtr = CharTypePtr;
            }
            else
            {
                // The symbol keeps the text; it never moves, so the pointer stays good.
                constant->LiteralText = literal.String;
                value.StringPtr = constant->LiteralText.data();
                constant->TypePtr = _Symbols.MakeStringType(length);
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
        MCAblSymbol* typeSymbol = SearchAndEnterLocalSymTable();
        typeSymbol->Defn.Key = MCAblSymbolKind::Type;
        typeSymbol->Library = _Library;
        NextToken();
        IfTokenGetElseError(MCAblToken::Equal, MCAblSyntaxError::MissingEqual);
        typeSymbol->TypePtr = DoType();
        Synchronize(FollowDeclaration, DeclarationStart, StatementStart);
        SkipSemicolon();
    }
}

auto MCAblCompiler::DoType() -> MCAblType*
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

    const MCAblSymbol* typeSymbol = SearchSymTableDisplay(_Scanner.Word());

    if (typeSymbol == nullptr)
    {
        SyntaxError(MCAblSyntaxError::UndefinedIdentifier);
    }

    if (typeSymbol->Defn.Key != MCAblSymbolKind::Type)
    {
        SyntaxError(MCAblSyntaxError::NotATypeIdentifier);
    }

    NextToken();
    MCAblType* elementType = typeSymbol->TypePtr;

    if (Token() != MCAblToken::LBracket)
    {
        return elementType;
    }

    // "type[d1, d2, ...]": one array type per dimension, each the element type of the one before.
    MCAblType* arrayType = _Symbols.MakeType();
    MCAblType* dimensionType = arrayType;

    while (true)
    {
        NextToken();
        bool validIndex = false;

        if (TokenIn(IndexTypeStart))
        {
            dimensionType->Form = MCAblTypeForm::Array;
            dimensionType->Size = 0;
            dimensionType->Array.IndexTypePtr = IntegerTypePtr;

            if (Token() == MCAblToken::Identifier)
            {
                const MCAblSymbol* countSymbol = SearchSymTableDisplay(_Scanner.Word());

                if (countSymbol == nullptr)
                {
                    SyntaxError(MCAblSyntaxError::UndefinedIdentifier);
                }

                if (countSymbol->Defn.Key == MCAblSymbolKind::Const && countSymbol->TypePtr == IntegerTypePtr)
                {
                    dimensionType->Array.ElementCount = countSymbol->Defn.Info.Constant.Value.Integer;
                    validIndex = true;
                }
            }
            else if (_Scanner.Literal().Type == MCAblLiteralType::Integer)
            {
                dimensionType->Array.ElementCount = _Scanner.Literal().Integer;
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

        MCAblType* nextDimensionType = _Symbols.MakeType();
        dimensionType->Array.ElementTypePtr = nextDimensionType;
        dimensionType = nextDimensionType;
    }

    IfTokenGetElseError(MCAblToken::RBracket, MCAblSyntaxError::MissingRBracket);
    dimensionType->Array.ElementTypePtr = elementType;
    arrayType->Size = ArraySize(arrayType);
    return arrayType;
}

auto MCAblCompiler::EnumerationType() -> MCAblType*
{
    MCAblType* type = _Symbols.MakeType();
    type->Form = MCAblTypeForm::Enum;
    type->Size = 4;
    int32_t value = -1;
    NextToken();

    while (Token() == MCAblToken::Identifier)
    {
        MCAblSymbol* constant = SearchAndEnterLocalSymTable();
        constant->Defn.Key = MCAblSymbolKind::Const;
        constant->Defn.Info.Constant.Value.Integer = ++value;
        constant->TypePtr = type;
        constant->Library = _Library;
        NextToken();
        IfTokenGet(MCAblToken::Comma);
    }

    IfTokenGetElseError(MCAblToken::RParen, MCAblSyntaxError::MissingRParen);
    return type;
}

auto MCAblCompiler::ArraySize(MCAblType* type) -> int32_t
{
    MCAblType* elementType = type->Array.ElementTypePtr;

    if (elementType->Size == 0)
    {
        elementType->Size = ArraySize(elementType);
    }

    type->Size = type->Array.ElementCount == -1 ? elementType->Size : elementType->Size * type->Array.ElementCount;
    return type->Size;
}

auto MCAblCompiler::VarDeclarations(MCAblSymbol* routine) -> void
{
    using namespace MCAblTokens;

    auto& routineInfo = routine->Defn.Info.Routine;
    // Locals follow the frame header (4 items) and the parameters.
    int32_t offset = routineInfo.TotalParamSize + 4;
    MCAblSymbol* previous = nullptr;
    // The last variable of the previous line (not eternal), linked to the first of the next.
    MCAblSymbol* previousLineLast = nullptr;

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
        MCAblSymbol* firstOfLine = nullptr;
        MCAblType* type = DoType();
        const int32_t size = type->Size;

        while (Token() == MCAblToken::Identifier)
        {
            MCAblSymbol* variable = nullptr;

            if (storage == MCAblStorage::Eternal)
            {
                // Eternals are global: entered at level 0.
                const int32_t savedLevel = _Level;
                _Level = 0;
                variable = SearchAndEnterLocalSymTable();
                _Level = savedLevel;
            }
            else
            {
                variable = SearchAndEnterLocalSymTable();
            }

            variable->Library = _Library;
            variable->Defn.Key = MCAblSymbolKind::Var;

            if (firstOfLine == nullptr)
            {
                firstOfLine = variable;

                if (storage != MCAblStorage::Eternal && routineInfo.Locals == nullptr)
                {
                    routineInfo.Locals = variable;
                }
            }
            else
            {
                previous->Next = variable;
            }

            NextToken();
            IfTokenGet(MCAblToken::Comma);
            previous = variable;
        }

        for (MCAblSymbol* variable = firstOfLine; variable != nullptr; variable = variable->Next)
        {
            variable->TypePtr = type;
            auto& data = variable->Defn.Info.Data;
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
                    // byte size (MCAblModule::Init allocates them); scalars 0.
                    data.Offset = static_cast<int32_t>(_StaticSizes.size());
                    _StaticSizes.push_back(type->Form == MCAblTypeForm::Array ? size : 0);
                    break;
                }
                case MCAblStorage::Eternal:
                {
                    // An item at the bottom of the runtime stack (an array's item points to its block).
                    data.Offset = AblRuntime()->DeclareEternal(type);
                    break;
                }
            }
        }

        if (storage != MCAblStorage::Eternal)
        {
            if (previousLineLast != nullptr)
            {
                previousLineLast->Next = firstOfLine;
            }

            previousLineLast = previous;
        }

        Synchronize(FollowVariables, DeclarationStart, StatementStart);
        SkipSemicolon();
    }

    Synchronize(FollowVarBlock);
}
