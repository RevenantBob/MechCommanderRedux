#pragma once

// ABL statement compiler: assignments, calls, if / else, while, repeat / until, for, switch / case.

#include "abl/ablscan.h"
#include "abl/ablsymt.h"

/// <summary>A case label of the switch being compiled, until switchStatement writes the jump table.</summary>
/// <remarks>0xc bytes in the original.</remarks>
struct MCCaseItem
{
    int32_t LabelValue = 0;
    /// <summary>Where the branch's code starts in the code buffer.</summary>
    char* BranchLocation = nullptr;
    MCCaseItem* Next = nullptr;
};

typedef MCCaseItem* MCCaseItemPtr;

/// <summary>Tokens that start a statement.</summary>
extern MCTokenCodeType StatementStartList[];
/// <summary>
/// Tokens that can follow a statement (the port's name: the original's list @ 0x0078c144 is unnamed in the symbols).
/// </summary>
extern MCTokenCodeType StatementEndList[];
extern MCTokenCodeType FollowSwitchExpressionList[];
extern MCTokenCodeType FollowCaseLabelList[];
extern MCTokenCodeType CaseLabelStartList[];

/// <summary>Compiles <c>target = expression</c>.</summary>
void AssignmentStatement(MCSymTableNodePtr varIdPtr);

/// <summary>Compiles <c>repeat ... until condition</c>.</summary>
void RepeatStatement();

/// <summary>Compiles <c>while condition do ... endwhile</c>.</summary>
void WhileStatement();

/// <summary>Compiles <c>if condition then ... [else ...] endif</c> (there is no elsif branch).</summary>
void IfStatement();

/// <summary>Compiles <c>for var = a to b do ... endfor</c>.</summary>
void ForStatement();

/// <summary>Compiles one case label (a number, constant or char) and adds it to the list.</summary>
/// <returns>The label's type.</returns>
MCTypePtr CaseLabel(MCCaseItemPtr& caseItemHead, MCCaseItemPtr& caseItemTail, int32_t& caseLabelCount);

/// <summary>Compiles <c>case labels : statements endcase;</c>.</summary>
void CaseBranch(MCCaseItemPtr& caseItemHead, MCCaseItemPtr& caseItemTail, int32_t& caseLabelCount,
                MCTypePtr expressionType);

/// <summary>Compiles <c>switch expression case ... endswitch</c> and its jump table.</summary>
void SwitchStatement();

/// <summary>Compiles one statement (with its statement marker).</summary>
void Statement();
