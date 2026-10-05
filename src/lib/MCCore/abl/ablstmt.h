#pragma once

// ABL statement compiler: assignments, calls, if / else, while, repeat / until, for, switch / case.

#include "abl/ablscan.h"
#include "abl/ablsymt.h"

/// <summary>A case label of the switch being compiled, until switchStatement writes the jump table.</summary>
/// <remarks>0xc bytes in the original.</remarks>
struct CaseItem
{
    int32_t labelValue = 0; // +0x0
    /// <summary>Where the branch's code starts in the code buffer.</summary>
    char* branchLocation = nullptr; // +0x4
    CaseItem* next = nullptr;       // +0x8
};

typedef CaseItem* CaseItemPtr;

/// <summary>Tokens that start a statement.</summary>
extern TokenCodeType statementStartList[];
/// <summary>
/// Tokens that can follow a statement (the port's name: the original's list @ 0x0078c144 is unnamed in the symbols).
/// </summary>
extern TokenCodeType statementEndList[];
extern TokenCodeType FollowSwitchExpressionList[];
extern TokenCodeType FollowCaseLabelList[];
extern TokenCodeType CaseLabelStartList[];

/// <summary>Compiles <c>target = expression</c>.</summary>
/// <remarks>MCX.EXE @ 0x0062c570</remarks>
void assignmentStatement(SymTableNodePtr varIdPtr);

/// <summary>Compiles <c>repeat ... until condition</c>.</summary>
/// <remarks>MCX.EXE @ 0x0062c5b0</remarks>
void repeatStatement();

/// <summary>Compiles <c>while condition do ... endwhile</c>.</summary>
/// <remarks>MCX.EXE @ 0x0062c620</remarks>
void whileStatement();

/// <summary>Compiles <c>if condition then ... [else ...] endif</c> (there is no elsif branch).</summary>
/// <remarks>MCX.EXE @ 0x0062c6b0</remarks>
void ifStatement();

/// <summary>Compiles <c>for var = a to b do ... endfor</c>.</summary>
/// <remarks>MCX.EXE @ 0x0062c7b0</remarks>
void forStatement();

/// <summary>Compiles one case label (a number, constant or char) and adds it to the list.</summary>
/// <returns>The label's type.</returns>
/// <remarks>MCX.EXE @ 0x0062c900</remarks>
TypePtr caseLabel(CaseItemPtr& caseItemHead, CaseItemPtr& caseItemTail, int32_t& caseLabelCount);

/// <summary>Compiles <c>case labels : statements endcase;</c>.</summary>
/// <remarks>MCX.EXE @ 0x0062cac0</remarks>
void caseBranch(CaseItemPtr& caseItemHead, CaseItemPtr& caseItemTail, int32_t& caseLabelCount, TypePtr expressionType);

/// <summary>Compiles <c>switch expression case ... endswitch</c> and its jump table.</summary>
/// <remarks>MCX.EXE @ 0x0062cbc0</remarks>
void switchStatement();

/// <summary>Compiles one statement (with its statement marker).</summary>
/// <remarks>MCX.EXE @ 0x0062cce0</remarks>
void statement();
