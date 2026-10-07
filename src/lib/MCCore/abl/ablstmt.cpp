#include "stdafx.h"
#include "abl/ablstmt.h"
#include "abl/abldecl.h"
#include "abl/ablerr.h"
#include "abl/ablexec.h"
#include "abl/ablexpr.h"
#include "abl/ablrtn.h"
#include "abl/ablscan.h"
#include "abl/ablsymt.h"
#include "lib/MCFatal.h"

MCTokenCodeType StatementStartList[] = {TKN_FOR, TKN_IF, TKN_REPEAT, TKN_WHILE, TKN_SWITCH, TKN_IDENTIFIER, TKN_NONE};
MCTokenCodeType StatementEndList[] = {TKN_SEMICOLON,  TKN_END_IF,       TKN_END_WHILE, TKN_END_FOR,
                                      TKN_END_SWITCH, TKN_END_FUNCTION, TKN_ELSE,      TKN_ELSIF,
                                      TKN_UNTIL,      TKN_EOF,          TKN_NONE};
MCTokenCodeType FollowSwitchExpressionList[] = {TKN_CASE, TKN_SEMICOLON, TKN_NONE};
MCTokenCodeType FollowCaseLabelList[] = {TKN_COLON, TKN_SEMICOLON, TKN_NONE};
MCTokenCodeType CaseLabelStartList[] = {TKN_IDENTIFIER, TKN_NUMBER, TKN_PLUS, TKN_MINUS, TKN_STRING, TKN_NONE};

namespace
{
    /// <summary>
    /// Compiles statements (each followed by any number of semicolons) until <paramref name="endToken1"/> or
    /// <paramref name="endToken2"/>, or a token that can't start a statement.
    /// </summary>
    auto StatementList(MCTokenCodeType endToken1, MCTokenCodeType endToken2) -> void
    {
        if (CurToken == endToken1 || CurToken == endToken2)
        {
            return;
        }

        do
        {
            Statement();

            while (CurToken == TKN_SEMICOLON)
            {
                GetToken();
            }
        } while (CurToken != endToken1 && CurToken != endToken2 && TokenIn(StatementStartList));
    }

    /// <summary>statementList with a single end token.</summary>
    auto StatementList(MCTokenCodeType endToken) -> void
    {
        StatementList(endToken, endToken);
    }
}

auto AssignmentStatement(MCSymTableNodePtr varIdPtr) -> void
{
    MCTypePtr varType = Variable(varIdPtr, USE_TARGET);
    IfTokenGetElseError(TKN_EQUAL, ABL_ERR_SYNTAX_MISSING_EQUAL);
    MCTypePtr exprType = Expression();

    if (IsAssignTypeCompatible(varType, exprType) == 0)
    {
        SyntaxError(ABL_ERR_SYNTAX_INCOMPATIBLE_ASSIGNMENT);
    }
}

auto RepeatStatement() -> void
{
    GetToken();
    StatementList(TKN_UNTIL);
    IfTokenGetElseError(TKN_UNTIL, ABL_ERR_SYNTAX_MISSING_UNTIL);

    if (Expression() != BooleanTypePtr)
    {
        SyntaxError(ABL_ERR_SYNTAX_INCOMPATIBLE_TYPES);
    }
}

auto WhileStatement() -> void
{
    GetToken();
    char* loopEndLocation = CrunchAddressMarker(nullptr);

    if (Expression() != BooleanTypePtr)
    {
        SyntaxError(ABL_ERR_SYNTAX_INCOMPATIBLE_TYPES);
    }

    IfTokenGetElseError(TKN_DO, ABL_ERR_SYNTAX_MISSING_DO);
    StatementList(TKN_END_WHILE);
    IfTokenGetElseError(TKN_END_WHILE, ABL_ERR_SYNTAX_MISSING_END_WHILE);
    FixupAddressMarker(loopEndLocation);
}

auto IfStatement() -> void
{
    GetToken();
    char* falseLocation = CrunchAddressMarker(nullptr);

    if (Expression() != BooleanTypePtr)
    {
        SyntaxError(ABL_ERR_SYNTAX_INCOMPATIBLE_TYPES);
    }

    IfTokenGetElseError(TKN_THEN, ABL_ERR_SYNTAX_MISSING_THEN);
    StatementList(TKN_END_IF, TKN_ELSE);
    FixupAddressMarker(falseLocation);

    if (CurToken == TKN_ELSE)
    {
        GetToken();
        char* ifEndLocation = CrunchAddressMarker(nullptr);
        StatementList(TKN_END_IF);
        FixupAddressMarker(ifEndLocation);
    }

    IfTokenGetElseError(TKN_END_IF, ABL_ERR_SYNTAX_MISSING_END_IF);
}

auto ForStatement() -> void
{
    GetToken();
    char* loopEndLocation = CrunchAddressMarker(nullptr);

    MCTypePtr controlType;

    if (CurToken == TKN_IDENTIFIER)
    {
        MCSymTableNodePtr controlIdPtr = nullptr;
        SearchAndFindAllSymTables(controlIdPtr);
        CrunchSymTableNodePtr(controlIdPtr);

        if (controlIdPtr->Defn.Key != DFN_VAR)
        {
            SyntaxError(ABL_ERR_SYNTAX_INVALID_FOR_CONTROL);
        }

        controlType = BaseType(controlIdPtr->TypePtr);
        GetToken();

        if (controlType != IntegerTypePtr && controlType->Form != FRM_ENUM)
        {
            SyntaxError(ABL_ERR_SYNTAX_INCOMPATIBLE_TYPES);
        }
    }
    else
    {
        SyntaxError(ABL_ERR_SYNTAX_MISSING_IDENTIFIER);
        controlType = &DummyType;
    }

    IfTokenGetElseError(TKN_EQUAL, ABL_ERR_SYNTAX_MISSING_EQUAL);

    if (IsAssignTypeCompatible(controlType, Expression()) == 0)
    {
        SyntaxError(ABL_ERR_SYNTAX_INCOMPATIBLE_TYPES);
    }

    if (CurToken == TKN_TO)
    {
        GetToken();
    }
    else
    {
        SyntaxError(ABL_ERR_SYNTAX_MISSING_TO);
    }

    if (IsAssignTypeCompatible(controlType, Expression()) == 0)
    {
        SyntaxError(ABL_ERR_SYNTAX_INCOMPATIBLE_TYPES);
    }

    IfTokenGetElseError(TKN_DO, ABL_ERR_SYNTAX_MISSING_DO);
    StatementList(TKN_END_FOR);
    IfTokenGetElseError(TKN_END_FOR, ABL_ERR_SYNTAX_MISSING_END_FOR);
    FixupAddressMarker(loopEndLocation);
}

auto CaseLabel(MCCaseItemPtr& caseItemHead, MCCaseItemPtr& caseItemTail, int32_t& caseLabelCount) -> MCTypePtr
{
    MCCaseItemPtr newCaseItem = AblMemory.Make<MCCaseItem>();

    if (caseItemHead == nullptr)
    {
        caseItemHead = newCaseItem;
    }
    else
    {
        caseItemTail->Next = newCaseItem;
    }

    caseItemTail = newCaseItem;
    newCaseItem->Next = nullptr;
    // Port fix: the original left labelValue as the heap had it when the label sets none (see the end).
    newCaseItem->LabelValue = 0;
    caseLabelCount++;

    MCTokenCodeType sign = TKN_PLUS;
    bool sawSign = false;

    if (CurToken == TKN_PLUS || CurToken == TKN_MINUS)
    {
        sign = CurToken;
        sawSign = true;
        GetToken();
    }

    if (CurToken == TKN_NUMBER)
    {
        // Entered as a literal symbol like factor's, and crunched (the executor skips it).
        MCSymTableNodePtr literalIdPtr = SearchSymTable(TokenString, SymTableDisplay[1]);

        if (literalIdPtr == nullptr)
        {
            literalIdPtr = EnterSymTable(TokenString, &SymTableDisplay[1]);
        }

        CrunchSymTableNodePtr(literalIdPtr);

        if (CurLiteral.Type == LIT_INTEGER)
        {
            newCaseItem->LabelValue = sign == TKN_PLUS ? CurLiteral.Value.Integer : -CurLiteral.Value.Integer;
        }
        else
        {
            SyntaxError(ABL_ERR_SYNTAX_INVALID_CONSTANT);
        }

        return IntegerTypePtr;
    }

    if (CurToken == TKN_IDENTIFIER)
    {
        MCSymTableNodePtr labelIdPtr = nullptr;
        SearchAllSymTables(labelIdPtr);
        CrunchSymTableNodePtr(labelIdPtr);

        if (labelIdPtr == nullptr)
        {
            SyntaxError(ABL_ERR_SYNTAX_UNDEFINED_IDENTIFIER);
            return &DummyType;
        }

        if (labelIdPtr->Defn.Key != DFN_CONST)
        {
            SyntaxError(ABL_ERR_SYNTAX_NOT_A_CONSTANT_IDENTIFIER);
            return &DummyType;
        }

        const MCValue& value = labelIdPtr->Defn.Info.Constant.Value;

        if (labelIdPtr->TypePtr == IntegerTypePtr)
        {
            newCaseItem->LabelValue = sign == TKN_PLUS ? value.Integer : -value.Integer;
            return IntegerTypePtr;
        }

        if (labelIdPtr->TypePtr == CharTypePtr)
        {
            if (sawSign)
            {
                SyntaxError(ABL_ERR_SYNTAX_INVALID_CONSTANT);
            }

            newCaseItem->LabelValue = value.Character;
            return CharTypePtr;
        }

        if (labelIdPtr->TypePtr->Form == FRM_ENUM)
        {
            if (sawSign)
            {
                SyntaxError(ABL_ERR_SYNTAX_INVALID_CONSTANT);
            }

            newCaseItem->LabelValue = value.Integer;
            return labelIdPtr->TypePtr;
        }

        return &DummyType;
    }

    // A string label sets no value; anything else is an error.
    if (CurToken != TKN_STRING)
    {
        SyntaxError(ABL_ERR_SYNTAX_INVALID_CONSTANT);
    }

    return &DummyType;
}

auto CaseBranch(MCCaseItemPtr& caseItemHead, MCCaseItemPtr& caseItemTail, int32_t& caseLabelCount,
                MCTypePtr expressionType) -> void
{
    MCCaseItemPtr oldCaseItemTail = caseItemTail;

    while (true)
    {
        if (CaseLabel(caseItemHead, caseItemTail, caseLabelCount) != expressionType)
        {
            SyntaxError(ABL_ERR_SYNTAX_INCOMPATIBLE_TYPES);
        }

        GetToken();

        if (CurToken != TKN_COMMA)
        {
            break;
        }

        GetToken();

        if (TokenIn(CaseLabelStartList) == 0)
        {
            SyntaxError(ABL_ERR_SYNTAX_MISSING_CONSTANT);
            break;
        }
    }

    Synchronize(FollowCaseLabelList, StatementStartList, nullptr);
    IfTokenGetElseError(TKN_COLON, ABL_ERR_SYNTAX_MISSING_COLON);

    // This branch's labels all jump here.
    char* branchLocation = CodeBufferPtr;
    MCCaseItemPtr caseItem = oldCaseItemTail == nullptr ? caseItemHead : oldCaseItemTail->Next;

    for (; caseItem != nullptr; caseItem = caseItem->Next)
    {
        caseItem->BranchLocation = branchLocation;
    }

    StatementList(TKN_END_CASE);
    IfTokenGetElseError(TKN_END_CASE, ABL_ERR_SYNTAX_MISSING_END_CASE);
    IfTokenGetElseError(TKN_SEMICOLON, ABL_ERR_SYNTAX_MISSING_SEMICOLON);
}

auto SwitchStatement() -> void
{
    MCCaseItemPtr caseItemHead = nullptr;
    MCCaseItemPtr caseItemTail = nullptr;
    int32_t caseLabelCount = 0;

    GetToken();
    char* caseTableLocation = CrunchAddressMarker(nullptr);
    MCTypePtr expressionType = Expression();

    if ((expressionType->Form != FRM_SCALAR && expressionType->Form != FRM_ENUM) || expressionType == RealTypePtr)
    {
        SyntaxError(ABL_ERR_SYNTAX_INCOMPATIBLE_TYPES);
    }

    Synchronize(FollowSwitchExpressionList, nullptr, nullptr);

    // Each branch ends with a marker jumping past the switch; the markers chain until fixed up at the end.
    char* branchEndChain = nullptr;

    while (CurToken == TKN_CASE)
    {
        GetToken();

        if (TokenIn(CaseLabelStartList))
        {
            CaseBranch(caseItemHead, caseItemTail, caseLabelCount, expressionType);
        }

        branchEndChain = CrunchAddressMarker(branchEndChain);
    }

    // The case table: the label count, then each label's value and branch offset.
    FixupAddressMarker(caseTableLocation);
    CrunchInteger(caseLabelCount);
    MCCaseItemPtr caseItem = caseItemHead;

    while (caseItem != nullptr)
    {
        CrunchInteger(caseItem->LabelValue);
        CrunchOffset(caseItem->BranchLocation);
        MCCaseItemPtr nextCaseItem = caseItem->Next;
        AblMemory.Free(caseItem);
        caseItem = nextCaseItem;
    }

    IfTokenGetElseError(TKN_END_SWITCH, ABL_ERR_SYNTAX_MISSING_END_SWITCH);

    while (branchEndChain != nullptr)
    {
        branchEndChain = FixupAddressMarker(branchEndChain);
    }
}

auto Statement() -> void
{
    if (CurToken != TKN_CODE)
    {
        CrunchStatementMarker();
    }

    switch (CurToken)
    {
        case TKN_IDENTIFIER:
        {
            MCSymTableNodePtr idPtr = nullptr;
            SearchAndFindAllSymTables(idPtr);

            if (idPtr->Defn.Key != DFN_FUNCTION)
            {
                AssignmentStatement(idPtr);
                break;
            }

            // assert, print and concat calls compile to nothing while their directive is off.
            MCRoutineKey routineKey = idPtr->Defn.Info.Routine.Key;

            if ((routineKey == RTN_ASSERT && AssertEnabled == 0) || (routineKey == RTN_PRINT && PrintEnabled == 0) ||
                (routineKey == RTN_CONCAT && StringFunctionsEnabled == 0))
            {
                UncrunchStatementMarker();
                Crunch = 0;
            }

            CrunchSymTableNodePtr(idPtr);
            GetToken();
            MCSymTableNodePtr saveRoutineIdPtr = CurRoutineIdPtr;
            RoutineCall(idPtr, 1);
            Crunch = 1;
            CurRoutineIdPtr = saveRoutineIdPtr;
            break;
        }

        case TKN_SWITCH:
            SwitchStatement();
            break;
        case TKN_FOR:
            ForStatement();
            break;
        case TKN_IF:
            IfStatement();
            break;
        case TKN_REPEAT:
            RepeatStatement();
            break;
        case TKN_WHILE:
            WhileStatement();
            break;
        default:
            break;
    }

    Synchronize(StatementEndList, nullptr, nullptr);

    if (TokenIn(StatementStartList))
    {
        SyntaxError(ABL_ERR_SYNTAX_MISSING_SEMICOLON);
    }
}
