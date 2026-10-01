#include "stdafx.h"
#include "abl/ablstmt.h"
#include "abl/abldecl.h"
#include "abl/ablerr.h"
#include "abl/ablexec.h"
#include "abl/ablexpr.h"
#include "abl/ablrtn.h"
#include "abl/ablscan.h"
#include "abl/ablsymt.h"
#include "lib/aerror.h"
#include "lib/heap.h"

TokenCodeType statementStartList[] = {TKN_FOR, TKN_IF, TKN_REPEAT, TKN_WHILE, TKN_SWITCH, TKN_IDENTIFIER, TKN_NONE};
TokenCodeType statementEndList[] = {TKN_SEMICOLON,  TKN_END_IF,       TKN_END_WHILE, TKN_END_FOR,
                                    TKN_END_SWITCH, TKN_END_FUNCTION, TKN_ELSE,      TKN_ELSIF,
                                    TKN_UNTIL,      TKN_EOF,          TKN_NONE};
TokenCodeType FollowSwitchExpressionList[] = {TKN_CASE, TKN_SEMICOLON, TKN_NONE};
TokenCodeType FollowCaseLabelList[] = {TKN_COLON, TKN_SEMICOLON, TKN_NONE};
TokenCodeType CaseLabelStartList[] = {TKN_IDENTIFIER, TKN_NUMBER, TKN_PLUS, TKN_MINUS, TKN_STRING, TKN_NONE};

namespace
{
    /// <summary>
    /// Compiles statements (each followed by any number of semicolons) until <paramref name="endToken1"/> or
    /// <paramref name="endToken2"/>, or a token that can't start a statement.
    /// </summary>
    auto statementList(TokenCodeType endToken1, TokenCodeType endToken2) -> void
    {
        if (curToken == endToken1 || curToken == endToken2)
        {
            return;
        }

        do
        {
            statement();

            while (curToken == TKN_SEMICOLON)
            {
                getToken();
            }
        } while (curToken != endToken1 && curToken != endToken2 && tokenIn(statementStartList));
    }

    /// <summary>statementList with a single end token.</summary>
    auto statementList(TokenCodeType endToken) -> void
    {
        statementList(endToken, endToken);
    }
}

auto assignmentStatement(SymTableNodePtr varIdPtr) -> void
{
    TypePtr varType = variable(varIdPtr, USE_TARGET);
    ifTokenGetElseError(TKN_EQUAL, ABL_ERR_SYNTAX_MISSING_EQUAL);
    TypePtr exprType = expression();

    if (isAssignTypeCompatible(varType, exprType) == 0)
    {
        syntaxError(ABL_ERR_SYNTAX_INCOMPATIBLE_ASSIGNMENT);
    }
}

auto repeatStatement() -> void
{
    getToken();
    statementList(TKN_UNTIL);
    ifTokenGetElseError(TKN_UNTIL, ABL_ERR_SYNTAX_MISSING_UNTIL);

    if (expression() != BooleanTypePtr)
    {
        syntaxError(ABL_ERR_SYNTAX_INCOMPATIBLE_TYPES);
    }
}

auto whileStatement() -> void
{
    getToken();
    char* loopEndLocation = crunchAddressMarker(nullptr);

    if (expression() != BooleanTypePtr)
    {
        syntaxError(ABL_ERR_SYNTAX_INCOMPATIBLE_TYPES);
    }

    ifTokenGetElseError(TKN_DO, ABL_ERR_SYNTAX_MISSING_DO);
    statementList(TKN_END_WHILE);
    ifTokenGetElseError(TKN_END_WHILE, ABL_ERR_SYNTAX_MISSING_END_WHILE);
    fixupAddressMarker(loopEndLocation);
}

auto ifStatement() -> void
{
    getToken();
    char* falseLocation = crunchAddressMarker(nullptr);

    if (expression() != BooleanTypePtr)
    {
        syntaxError(ABL_ERR_SYNTAX_INCOMPATIBLE_TYPES);
    }

    ifTokenGetElseError(TKN_THEN, ABL_ERR_SYNTAX_MISSING_THEN);
    statementList(TKN_END_IF, TKN_ELSE);
    fixupAddressMarker(falseLocation);

    if (curToken == TKN_ELSE)
    {
        getToken();
        char* ifEndLocation = crunchAddressMarker(nullptr);
        statementList(TKN_END_IF);
        fixupAddressMarker(ifEndLocation);
    }

    ifTokenGetElseError(TKN_END_IF, ABL_ERR_SYNTAX_MISSING_END_IF);
}

auto forStatement() -> void
{
    getToken();
    char* loopEndLocation = crunchAddressMarker(nullptr);

    TypePtr controlType;

    if (curToken == TKN_IDENTIFIER)
    {
        SymTableNodePtr controlIdPtr = nullptr;
        searchAndFindAllSymTables(controlIdPtr);
        crunchSymTableNodePtr(controlIdPtr);

        if (controlIdPtr->defn.key != DFN_VAR)
        {
            syntaxError(ABL_ERR_SYNTAX_INVALID_FOR_CONTROL);
        }

        controlType = baseType(controlIdPtr->typePtr);
        getToken();

        if (controlType != IntegerTypePtr && controlType->form != FRM_ENUM)
        {
            syntaxError(ABL_ERR_SYNTAX_INCOMPATIBLE_TYPES);
        }
    }
    else
    {
        syntaxError(ABL_ERR_SYNTAX_MISSING_IDENTIFIER);
        controlType = &DummyType;
    }

    ifTokenGetElseError(TKN_EQUAL, ABL_ERR_SYNTAX_MISSING_EQUAL);

    if (isAssignTypeCompatible(controlType, expression()) == 0)
    {
        syntaxError(ABL_ERR_SYNTAX_INCOMPATIBLE_TYPES);
    }

    if (curToken == TKN_TO)
    {
        getToken();
    }
    else
    {
        syntaxError(ABL_ERR_SYNTAX_MISSING_TO);
    }

    if (isAssignTypeCompatible(controlType, expression()) == 0)
    {
        syntaxError(ABL_ERR_SYNTAX_INCOMPATIBLE_TYPES);
    }

    ifTokenGetElseError(TKN_DO, ABL_ERR_SYNTAX_MISSING_DO);
    statementList(TKN_END_FOR);
    ifTokenGetElseError(TKN_END_FOR, ABL_ERR_SYNTAX_MISSING_END_FOR);
    fixupAddressMarker(loopEndLocation);
}

auto caseLabel(CaseItemPtr& caseItemHead, CaseItemPtr& caseItemTail, int32_t& caseLabelCount) -> TypePtr
{
    auto* newCaseItem = static_cast<CaseItemPtr>(AblStackHeap->malloc(sizeof(CaseItem)));

    if (newCaseItem == nullptr)
    {
        Fatal(0, " ABL: Unable to AblStackHeap->malloc case item ");
    }

    if (caseItemHead == nullptr)
    {
        caseItemHead = newCaseItem;
    }
    else
    {
        caseItemTail->next = newCaseItem;
    }

    caseItemTail = newCaseItem;
    newCaseItem->next = nullptr;
    // Port fix: the original left labelValue as the heap had it when the label sets none (see the end).
    newCaseItem->labelValue = 0;
    caseLabelCount++;

    TokenCodeType sign = TKN_PLUS;
    bool sawSign = false;

    if (curToken == TKN_PLUS || curToken == TKN_MINUS)
    {
        sign = curToken;
        sawSign = true;
        getToken();
    }

    if (curToken == TKN_NUMBER)
    {
        // Entered as a literal symbol like factor's, and crunched (the executor skips it).
        SymTableNodePtr literalIdPtr = searchSymTable(tokenString, SymTableDisplay[1]);

        if (literalIdPtr == nullptr)
        {
            literalIdPtr = enterSymTable(tokenString, &SymTableDisplay[1]);
        }

        crunchSymTableNodePtr(literalIdPtr);

        if (curLiteral.type == LIT_INTEGER)
        {
            newCaseItem->labelValue = sign == TKN_PLUS ? curLiteral.value.integer : -curLiteral.value.integer;
        }
        else
        {
            syntaxError(ABL_ERR_SYNTAX_INVALID_CONSTANT);
        }

        return IntegerTypePtr;
    }

    if (curToken == TKN_IDENTIFIER)
    {
        SymTableNodePtr labelIdPtr = nullptr;
        searchAllSymTables(labelIdPtr);
        crunchSymTableNodePtr(labelIdPtr);

        if (labelIdPtr == nullptr)
        {
            syntaxError(ABL_ERR_SYNTAX_UNDEFINED_IDENTIFIER);
            return &DummyType;
        }

        if (labelIdPtr->defn.key != DFN_CONST)
        {
            syntaxError(ABL_ERR_SYNTAX_NOT_A_CONSTANT_IDENTIFIER);
            return &DummyType;
        }

        const Value& value = labelIdPtr->defn.info.constant.value;

        if (labelIdPtr->typePtr == IntegerTypePtr)
        {
            newCaseItem->labelValue = sign == TKN_PLUS ? value.integer : -value.integer;
            return IntegerTypePtr;
        }

        if (labelIdPtr->typePtr == CharTypePtr)
        {
            if (sawSign)
            {
                syntaxError(ABL_ERR_SYNTAX_INVALID_CONSTANT);
            }

            newCaseItem->labelValue = value.character;
            return CharTypePtr;
        }

        if (labelIdPtr->typePtr->form == FRM_ENUM)
        {
            if (sawSign)
            {
                syntaxError(ABL_ERR_SYNTAX_INVALID_CONSTANT);
            }

            newCaseItem->labelValue = value.integer;
            return labelIdPtr->typePtr;
        }

        return &DummyType;
    }

    // A string label sets no value; anything else is an error.
    if (curToken != TKN_STRING)
    {
        syntaxError(ABL_ERR_SYNTAX_INVALID_CONSTANT);
    }

    return &DummyType;
}

auto caseBranch(CaseItemPtr& caseItemHead, CaseItemPtr& caseItemTail, int32_t& caseLabelCount, TypePtr expressionType)
    -> void
{
    CaseItemPtr oldCaseItemTail = caseItemTail;

    while (true)
    {
        if (caseLabel(caseItemHead, caseItemTail, caseLabelCount) != expressionType)
        {
            syntaxError(ABL_ERR_SYNTAX_INCOMPATIBLE_TYPES);
        }

        getToken();

        if (curToken != TKN_COMMA)
        {
            break;
        }

        getToken();

        if (tokenIn(CaseLabelStartList) == 0)
        {
            syntaxError(ABL_ERR_SYNTAX_MISSING_CONSTANT);
            break;
        }
    }

    synchronize(FollowCaseLabelList, statementStartList, nullptr);
    ifTokenGetElseError(TKN_COLON, ABL_ERR_SYNTAX_MISSING_COLON);

    // This branch's labels all jump here.
    char* branchLocation = codeBufferPtr;
    CaseItemPtr caseItem = oldCaseItemTail == nullptr ? caseItemHead : oldCaseItemTail->next;

    for (; caseItem != nullptr; caseItem = caseItem->next)
    {
        caseItem->branchLocation = branchLocation;
    }

    statementList(TKN_END_CASE);
    ifTokenGetElseError(TKN_END_CASE, ABL_ERR_SYNTAX_MISSING_END_CASE);
    ifTokenGetElseError(TKN_SEMICOLON, ABL_ERR_SYNTAX_MISSING_SEMICOLON);
}

auto switchStatement() -> void
{
    CaseItemPtr caseItemHead = nullptr;
    CaseItemPtr caseItemTail = nullptr;
    int32_t caseLabelCount = 0;

    getToken();
    char* caseTableLocation = crunchAddressMarker(nullptr);
    TypePtr expressionType = expression();

    if ((expressionType->form != FRM_SCALAR && expressionType->form != FRM_ENUM) || expressionType == RealTypePtr)
    {
        syntaxError(ABL_ERR_SYNTAX_INCOMPATIBLE_TYPES);
    }

    synchronize(FollowSwitchExpressionList, nullptr, nullptr);

    // Each branch ends with a marker jumping past the switch; the markers chain until fixed up at the end.
    char* branchEndChain = nullptr;

    while (curToken == TKN_CASE)
    {
        getToken();

        if (tokenIn(CaseLabelStartList))
        {
            caseBranch(caseItemHead, caseItemTail, caseLabelCount, expressionType);
        }

        branchEndChain = crunchAddressMarker(branchEndChain);
    }

    // The case table: the label count, then each label's value and branch offset.
    fixupAddressMarker(caseTableLocation);
    crunchInteger(caseLabelCount);
    CaseItemPtr caseItem = caseItemHead;

    while (caseItem != nullptr)
    {
        crunchInteger(caseItem->labelValue);
        crunchOffset(caseItem->branchLocation);
        CaseItemPtr nextCaseItem = caseItem->next;
        AblStackHeap->free(caseItem);
        caseItem = nextCaseItem;
    }

    ifTokenGetElseError(TKN_END_SWITCH, ABL_ERR_SYNTAX_MISSING_END_SWITCH);

    while (branchEndChain != nullptr)
    {
        branchEndChain = fixupAddressMarker(branchEndChain);
    }
}

auto statement() -> void
{
    if (curToken != TKN_CODE)
    {
        crunchStatementMarker();
    }

    switch (curToken)
    {
        case TKN_IDENTIFIER:
        {
            SymTableNodePtr idPtr = nullptr;
            searchAndFindAllSymTables(idPtr);

            if (idPtr->defn.key != DFN_FUNCTION)
            {
                assignmentStatement(idPtr);
                break;
            }

            // assert, print and concat calls compile to nothing while their directive is off.
            RoutineKey routineKey = idPtr->defn.info.routine.key;

            if ((routineKey == RTN_ASSERT && AssertEnabled == 0) || (routineKey == RTN_PRINT && PrintEnabled == 0) ||
                (routineKey == RTN_CONCAT && StringFunctionsEnabled == 0))
            {
                uncrunchStatementMarker();
                Crunch = 0;
            }

            crunchSymTableNodePtr(idPtr);
            getToken();
            SymTableNodePtr saveRoutineIdPtr = CurRoutineIdPtr;
            routineCall(idPtr, 1);
            Crunch = 1;
            CurRoutineIdPtr = saveRoutineIdPtr;
            break;
        }

        case TKN_SWITCH:
            switchStatement();
            break;
        case TKN_FOR:
            forStatement();
            break;
        case TKN_IF:
            ifStatement();
            break;
        case TKN_REPEAT:
            repeatStatement();
            break;
        case TKN_WHILE:
            whileStatement();
            break;
        default:
            break;
    }

    synchronize(statementEndList, nullptr, nullptr);

    if (tokenIn(statementStartList))
    {
        syntaxError(ABL_ERR_SYNTAX_MISSING_SEMICOLON);
    }
}
