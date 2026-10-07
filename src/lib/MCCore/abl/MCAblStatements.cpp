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
            MCAblSymbol* symbol = SearchAndFindAllSymTables();

            if (symbol->Defn.Key != MCAblSymbolKind::Function)
            {
                AssignmentStatement(symbol);
                break;
            }

            // assert, print and concat calls compile to nothing while their directive is off.
            const MCAblRoutineKey key = symbol->Defn.Info.Routine.Key;

            if ((key == MCAblRoutineKey::Assert && !_Directives.Assert) ||
                (key == MCAblRoutineKey::Print && !_Directives.Print) ||
                (key == MCAblRoutineKey::Concat && !_Directives.StringFunctions))
            {
                _Code.RemoveStatementMarker();
                _Code.Enabled = false;
            }

            _Code.WriteSymbol(symbol);
            NextToken();
            MCAblSymbol* callingRoutine = _Routine;
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

auto MCAblCompiler::AssignmentStatement(MCAblSymbol* variable) -> void
{
    MCAblType* variableType = Variable(variable);
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

    if (Expression() != BooleanTypePtr)
    {
        SyntaxError(MCAblSyntaxError::IncompatibleTypes);
    }
}

auto MCAblCompiler::WhileStatement() -> void
{
    NextToken();
    const MCAblCodeMark loopEnd = _Code.InsertAddressMarker(NoCodeMark);

    if (Expression() != BooleanTypePtr)
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

    if (Expression() != BooleanTypePtr)
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

    MCAblSymbol* control = SearchAndFindAllSymTables();
    _Code.WriteSymbol(control);

    if (control->Defn.Key != MCAblSymbolKind::Var)
    {
        SyntaxError(MCAblSyntaxError::InvalidForControl);
    }

    MCAblType* controlType = control->TypePtr;
    NextToken();

    if (controlType != IntegerTypePtr && controlType->Form != MCAblTypeForm::Enum)
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

auto MCAblCompiler::CaseLabel(CaseItem& item) -> MCAblType*
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
        return IntegerTypePtr;
    }

    if (Token() == MCAblToken::Identifier)
    {
        MCAblSymbol* label = SearchSymTableDisplay(_Scanner.Word());
        // Written before it is checked, as in the original.
        _Code.WriteSymbol(label);

        if (label == nullptr)
        {
            SyntaxError(MCAblSyntaxError::UndefinedIdentifier);
        }

        if (label->Defn.Key != MCAblSymbolKind::Const)
        {
            SyntaxError(MCAblSyntaxError::NotAConstantIdentifier);
        }

        const MCAblValue& value = label->Defn.Info.Constant.Value;

        if (label->TypePtr == IntegerTypePtr)
        {
            item.LabelValue = negative ? -value.Integer : value.Integer;
            return IntegerTypePtr;
        }

        if (label->TypePtr == CharTypePtr || label->TypePtr->Form == MCAblTypeForm::Enum)
        {
            if (sawSign)
            {
                SyntaxError(MCAblSyntaxError::InvalidConstant);
            }

            item.LabelValue = label->TypePtr == CharTypePtr ? value.Character : value.Integer;
            return label->TypePtr;
        }

        // A real or string constant: no type, so never the switch's.
        return nullptr;
    }

    // A string label sets no value and has no type; anything else is an error.
    if (Token() != MCAblToken::String)
    {
        SyntaxError(MCAblSyntaxError::InvalidConstant);
    }

    return nullptr;
}

auto MCAblCompiler::CaseBranch(std::vector<CaseItem>& items, MCAblType* expressionType) -> void
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
    MCAblType* expressionType = Expression();

    if ((expressionType->Form != MCAblTypeForm::Scalar && expressionType->Form != MCAblTypeForm::Enum) ||
        expressionType == RealTypePtr)
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
