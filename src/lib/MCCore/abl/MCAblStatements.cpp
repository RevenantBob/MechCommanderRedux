#include "stdafx.h"
#include "abl/MCAblCompiler.h"
#include "abl/MCAblTokenLists.h"

// The statements: assignments, calls, if / else, while, repeat / until, for, switch / case. The original's
// ablstmt.cpp.

auto MCAblCompiler::Statement() -> void
{
    if (Token() != MCAblToken::Code)
    {
        _Code.InsertStatementMarker(_Scanner.FileNumber(), _Scanner.LineNumber());
    }

    switch (Token())
    {
        case MCAblToken::Identifier:
        {
            const MCAblSymbol& symbol = SearchAndFindAllSymTables();

            if (symbol.Defn.Key != MCAblSymbolKind::Function)
            {
                AssignmentStatement(symbol);
                break;
            }

            // assert, print and concat calls compile to nothing while their directive is off.
            const MCAblRoutineKey key = symbol.Defn.Info.Routine.Key;

            if ((key == MCAblRoutineKey::Assert && !_Directives.Assert) ||
                (key == MCAblRoutineKey::Print && !_Directives.Print) ||
                (key == MCAblRoutineKey::Concat && !_Directives.StringFunctions))
            {
                _Code.RemoveStatementMarker();
                _Code.Enabled = false;
            }

            _Code.WriteSymbol(symbol);
            NextToken();
            const MCAblSymbolId callingRoutine = _Routine;
            RoutineCall(symbol);
            _Code.Enabled = true;
            _Routine = callingRoutine;
            break;
        }
        case MCAblToken::Switch:
        {
            SwitchStatement();
            break;
        }
        case MCAblToken::For:
        {
            ForStatement();
            break;
        }
        case MCAblToken::If:
        {
            IfStatement();
            break;
        }
        case MCAblToken::Repeat:
        {
            RepeatStatement();
            break;
        }
        case MCAblToken::While:
        {
            WhileStatement();
            break;
        }
        default:
        {
            break;
        }
    }

    Synchronize(MCAblTokens::StatementEnd);
}

auto MCAblCompiler::StatementList(MCAblToken endToken1, MCAblToken endToken2) -> void
{
    if (Token() == endToken1 || Token() == endToken2)
    {
        return;
    }

    do
    {
        Statement();

        while (Token() == MCAblToken::Semicolon)
        {
            NextToken();
        }
    } while (Token() != endToken1 && Token() != endToken2 && TokenIn(MCAblTokens::StatementStart));
}

auto MCAblCompiler::AssignmentStatement(const MCAblSymbol& variable) -> void
{
    const MCAblTypeId variableType = Variable(variable);
    IfTokenGetElseError(MCAblToken::Equal, MCAblSyntaxError::MissingEqual);

    if (!IsAssignTypeCompatible(variableType, Expression()))
    {
        SyntaxError(MCAblSyntaxError::IncompatibleAssignment);
    }
}

auto MCAblCompiler::RepeatStatement() -> void
{
    NextToken();
    StatementList(MCAblToken::Until, MCAblToken::Until);
    IfTokenGetElseError(MCAblToken::Until, MCAblSyntaxError::MissingUntil);

    if (Expression() != MCAblTypeId::Boolean)
    {
        SyntaxError(MCAblSyntaxError::IncompatibleTypes);
    }
}

auto MCAblCompiler::WhileStatement() -> void
{
    NextToken();
    const MCAblCodeMark loopEnd = _Code.InsertAddressMarker(NoCodeMark);

    if (Expression() != MCAblTypeId::Boolean)
    {
        SyntaxError(MCAblSyntaxError::IncompatibleTypes);
    }

    IfTokenGetElseError(MCAblToken::Do, MCAblSyntaxError::MissingDo);
    StatementList(MCAblToken::EndWhile, MCAblToken::EndWhile);
    IfTokenGetElseError(MCAblToken::EndWhile, MCAblSyntaxError::MissingEndWhile);
    _Code.FixupAddressMarker(loopEnd);
}

auto MCAblCompiler::IfStatement() -> void
{
    NextToken();
    const MCAblCodeMark falseBranch = _Code.InsertAddressMarker(NoCodeMark);

    if (Expression() != MCAblTypeId::Boolean)
    {
        SyntaxError(MCAblSyntaxError::IncompatibleTypes);
    }

    IfTokenGetElseError(MCAblToken::Then, MCAblSyntaxError::MissingThen);
    StatementList(MCAblToken::EndIf, MCAblToken::Else);
    _Code.FixupAddressMarker(falseBranch);

    if (Token() == MCAblToken::Else)
    {
        NextToken();
        const MCAblCodeMark ifEnd = _Code.InsertAddressMarker(NoCodeMark);
        StatementList(MCAblToken::EndIf, MCAblToken::EndIf);
        _Code.FixupAddressMarker(ifEnd);
    }

    IfTokenGetElseError(MCAblToken::EndIf, MCAblSyntaxError::MissingEndIf);
}

auto MCAblCompiler::ForStatement() -> void
{
    NextToken();
    const MCAblCodeMark loopEnd = _Code.InsertAddressMarker(NoCodeMark);

    if (Token() != MCAblToken::Identifier)
    {
        SyntaxError(MCAblSyntaxError::MissingIdentifier);
    }

    const MCAblSymbol& control = SearchAndFindAllSymTables();
    _Code.WriteSymbol(control);

    if (control.Defn.Key != MCAblSymbolKind::Var)
    {
        SyntaxError(MCAblSyntaxError::InvalidForControl);
    }

    const MCAblTypeId controlType = control.Type;
    NextToken();

    if (controlType != MCAblTypeId::Integer && _Symbols.Type(controlType).Form != MCAblTypeForm::Enum)
    {
        SyntaxError(MCAblSyntaxError::IncompatibleTypes);
    }

    IfTokenGetElseError(MCAblToken::Equal, MCAblSyntaxError::MissingEqual);

    if (!IsAssignTypeCompatible(controlType, Expression()))
    {
        SyntaxError(MCAblSyntaxError::IncompatibleTypes);
    }

    IfTokenGetElseError(MCAblToken::To, MCAblSyntaxError::MissingTo);

    if (!IsAssignTypeCompatible(controlType, Expression()))
    {
        SyntaxError(MCAblSyntaxError::IncompatibleTypes);
    }

    IfTokenGetElseError(MCAblToken::Do, MCAblSyntaxError::MissingDo);
    StatementList(MCAblToken::EndFor, MCAblToken::EndFor);
    IfTokenGetElseError(MCAblToken::EndFor, MCAblSyntaxError::MissingEndFor);
    _Code.FixupAddressMarker(loopEnd);
}

auto MCAblCompiler::CaseLabel(CaseItem& item) -> MCAblTypeId
{
    bool negative = false;
    bool sawSign = false;

    if (Token() == MCAblToken::Plus || Token() == MCAblToken::Minus)
    {
        negative = Token() == MCAblToken::Minus;
        sawSign = true;
        NextToken();
    }

    if (Token() == MCAblToken::Number)
    {
        // Entered as a literal symbol, as in an expression, and written to the code (the executor skips it).
        _Code.WriteSymbol(LiteralSymbol());
        const MCAblLiteral& literal = _Scanner.Literal();

        if (literal.Type != MCAblLiteralType::Integer)
        {
            SyntaxError(MCAblSyntaxError::InvalidConstant);
        }

        item.LabelValue = negative ? -literal.Integer : literal.Integer;
        return MCAblTypeId::Integer;
    }

    if (Token() == MCAblToken::Identifier)
    {
        const MCAblSymbolId labelId = SearchSymTableDisplay(_Scanner.Word());

        // The original wrote the symbol before checking it; an error drops the code either way.
        if (labelId == MCAblSymbolId::None)
        {
            SyntaxError(MCAblSyntaxError::UndefinedIdentifier);
        }

        const MCAblSymbol& label = Symbol(labelId);
        _Code.WriteSymbol(label);

        if (label.Defn.Key != MCAblSymbolKind::Const)
        {
            SyntaxError(MCAblSyntaxError::NotAConstantIdentifier);
        }

        const MCAblValue& value = label.Defn.Info.Constant.Value;

        if (label.Type == MCAblTypeId::Integer)
        {
            item.LabelValue = negative ? -value.Integer : value.Integer;
            return MCAblTypeId::Integer;
        }

        if (label.Type == MCAblTypeId::Char || _Symbols.Type(label.Type).Form == MCAblTypeForm::Enum)
        {
            if (sawSign)
            {
                SyntaxError(MCAblSyntaxError::InvalidConstant);
            }

            item.LabelValue = label.Type == MCAblTypeId::Char ? value.Character : value.Integer;
            return label.Type;
        }

        // A real or string constant: no type, so never the switch's.
        return MCAblTypeId::None;
    }

    // A string label sets no value and has no type; anything else is an error.
    if (Token() != MCAblToken::String)
    {
        SyntaxError(MCAblSyntaxError::InvalidConstant);
    }

    return MCAblTypeId::None;
}

auto MCAblCompiler::CaseBranch(std::vector<CaseItem>& items, MCAblTypeId expressionType) -> void
{
    const size_t firstItem = items.size();

    while (true)
    {
        CaseItem item;

        if (CaseLabel(item) != expressionType)
        {
            SyntaxError(MCAblSyntaxError::IncompatibleTypes);
        }

        items.push_back(item);
        NextToken();

        if (Token() != MCAblToken::Comma)
        {
            break;
        }

        NextToken();

        if (!TokenIn(MCAblTokens::CaseLabelStart))
        {
            SyntaxError(MCAblSyntaxError::MissingConstant);
        }
    }

    Synchronize(MCAblTokens::FollowCaseLabel, MCAblTokens::StatementStart);
    IfTokenGetElseError(MCAblToken::Colon, MCAblSyntaxError::MissingColon);

    // This branch's labels all jump here.
    const MCAblCodeMark branchLocation = _Code.Position();

    for (size_t i = firstItem; i < items.size(); i++)
    {
        items[i].BranchLocation = branchLocation;
    }

    StatementList(MCAblToken::EndCase, MCAblToken::EndCase);
    IfTokenGetElseError(MCAblToken::EndCase, MCAblSyntaxError::MissingEndCase);
    IfTokenGetElseError(MCAblToken::Semicolon, MCAblSyntaxError::MissingSemicolon);
}

auto MCAblCompiler::SwitchStatement() -> void
{
    NextToken();
    const MCAblCodeMark caseTable = _Code.InsertAddressMarker(NoCodeMark);
    const MCAblTypeId expressionType = Expression();

    // A routine without a result has no type (the original read through its null pointer).
    if (expressionType == MCAblTypeId::None ||
        (_Symbols.Type(expressionType).Form != MCAblTypeForm::Scalar &&
         _Symbols.Type(expressionType).Form != MCAblTypeForm::Enum) ||
        expressionType == MCAblTypeId::Real)
    {
        SyntaxError(MCAblSyntaxError::IncompatibleTypes);
    }

    Synchronize(MCAblTokens::FollowSwitchExpression);

    // Each branch ends with a marker jumping past the switch; the markers chain until they are fixed up at the end.
    std::vector<CaseItem> items;
    MCAblCodeMark branchEndChain = NoCodeMark;

    while (Token() == MCAblToken::Case)
    {
        NextToken();

        if (TokenIn(MCAblTokens::CaseLabelStart))
        {
            CaseBranch(items, expressionType);
        }

        branchEndChain = _Code.InsertAddressMarker(branchEndChain);
    }

    // The case table: the label count, then each label's value and branch offset.
    _Code.FixupAddressMarker(caseTable);
    _Code.WriteInteger(static_cast<int32_t>(items.size()));

    for (const CaseItem& item : items)
    {
        _Code.WriteInteger(item.LabelValue);
        _Code.WriteOffset(item.BranchLocation);
    }

    IfTokenGetElseError(MCAblToken::EndSwitch, MCAblSyntaxError::MissingEndSwitch);

    while (branchEndChain != NoCodeMark)
    {
        branchEndChain = _Code.FixupAddressMarker(branchEndChain);
    }
}
