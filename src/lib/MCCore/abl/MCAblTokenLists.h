#pragma once

#include "abl/MCAblToken.h"

// The token sets the ABL compiler expects at each point (the original's zero-terminated token lists, by their names).

/// <summary>The sets of tokens MCAblCompiler synchronizes on and tests for.</summary>
namespace MCAblTokens
{
    using enum MCAblToken;

    inline constexpr std::array FollowHeader{Semicolon, Eof};
    inline constexpr std::array FollowModuleId{LParen, Colon, Semicolon, Eof};
    inline constexpr std::array FollowFunctionId{LParen, Colon, Semicolon, Eof};
    inline constexpr std::array FollowParams{RParen, Comma, Eof};
    inline constexpr std::array FollowParam{Comma, RParen};
    inline constexpr std::array FollowModuleDecls{Semicolon, Code, Eof};
    inline constexpr std::array FollowRoutineDecls{Semicolon, Code, Eof};
    inline constexpr std::array FollowRoutine{Semicolon, Eof};
    inline constexpr std::array FollowDeclaration{Semicolon, Identifier, Eof};
    inline constexpr std::array FollowVariables{Semicolon, Identifier, Eof};
    inline constexpr std::array FollowVarBlock{Function, Code, Eof};
    inline constexpr std::array FollowDimension{Comma, RBracket, Eof};
    inline constexpr std::array IndexTypeStart{Identifier, Number};
    /// <summary>Tokens that start a declaration block.</summary>
    inline constexpr std::array DeclarationStart{Const, Var, Function};
    inline constexpr std::array RelationalOperators{Less, LessEqual, EqualEqual, NotEqual, GreaterEqual, Greater};
    inline constexpr std::array AddOperators{Plus, Minus, Or};
    inline constexpr std::array MultiplyOperators{Star, Slash, Div, Mod, And};
    /// <summary>Tokens that start a statement.</summary>
    inline constexpr std::array StatementStart{For, If, Repeat, While, Switch, Identifier};
    /// <summary>Tokens that can follow a statement (the original's list is unnamed in the symbols).</summary>
    inline constexpr std::array StatementEnd{Semicolon,   EndIf, EndWhile, EndFor, EndSwitch,
                                             EndFunction, Else,  Elsif,    Until,  Eof};
    inline constexpr std::array FollowSwitchExpression{Case, Semicolon};
    inline constexpr std::array FollowCaseLabel{Colon, Semicolon};
    inline constexpr std::array CaseLabelStart{Identifier, Number, Plus, Minus, String};
}
