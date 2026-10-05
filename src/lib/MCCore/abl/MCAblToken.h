#pragma once

// The tokens of ABL, FASA's Pascal-like scripting language for unit brains, mission scripts and libraries.

/// <summary>
/// An ABL token. The values are the original's: crunched code stores each token as one byte (see ablexec.h), and
/// <see cref="MCAblTokenText"/> names them. Reserved words map to <c>Code</c> .. <c>Static</c> (MCAblScanner).
/// </summary>
enum class MCAblToken : uint8_t
{
    None = 0,
    Identifier = 1,
    Number = 2,
    Type = 3,
    String = 4,
    Star = 5,
    LParen = 6,
    RParen = 7,
    Minus = 8,
    Plus = 9,
    Equal = 10,
    LBracket = 11,
    RBracket = 12,
    Colon = 13,
    Semicolon = 14,
    Less = 15,
    Greater = 16,
    Comma = 17,
    Period = 18,
    Slash = 19,
    EqualEqual = 20,
    LessEqual = 21,
    GreaterEqual = 22,
    NotEqual = 23,
    Eof = 24,
    Error = 25,
    Code = 26,
    /// <summary>"orders"; no reserved word produces it in MCX.EXE.</summary>
    Orders = 27,
    And = 28,
    Switch = 29,
    Case = 30,
    Const = 31,
    /// <summary>"div"; not a reserved word in MCX.EXE (it scans as an identifier).</summary>
    Div = 32,
    Do = 33,
    Of = 34,
    Else = 35,
    EndIf = 36,
    EndWhile = 37,
    EndFor = 38,
    EndFunction = 39,
    EndModule = 40,
    EndLibrary = 41,
    EndVar = 42,
    EndCode = 43,
    EndCase = 44,
    EndSwitch = 45,
    For = 46,
    Function = 47,
    If = 48,
    Mod = 49,
    Not = 50,
    Or = 51,
    Repeat = 52,
    Then = 53,
    To = 54,
    Until = 55,
    Var = 56,
    /// <summary>'@': marks a reference parameter.</summary>
    Ref = 57,
    While = 58,
    Elsif = 59,
    Return = 60,
    Module = 61,
    Library = 62,
    Eternal = 63,
    Static = 64,
    /// <summary>'#' outside a language directive.</summary>
    Pound = 65,
    UnexpectedToken = 66,
    /// <summary>'C' in crunched code: a statement marker (ablexec.h).</summary>
    StatementMarker = 67,
    /// <summary>'D' in crunched code: an address marker (ablexec.h).</summary>
    AddressMarker = 68,
    Count
};

/// <summary>A set of tokens the parser expects (the original's zero-terminated token lists).</summary>
using MCAblTokenList = std::span<const MCAblToken>;

/// <summary>The text of <paramref name="token"/>: its spelling, or a <c>{NAME}</c> for the token classes.</summary>
std::string_view MCAblTokenText(MCAblToken token);
