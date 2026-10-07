#pragma once

// ABL errors: compile-time syntax errors (MCAblCompiler) and execution-time runtime errors (MCAblRuntime::RuntimeError). Both end
// the game in MCX.EXE: they build a message and call Fatal.

/// <summary>
/// An ABL compile error. The values are the original's (shown as the error's "type"); the names are the port's (the
/// MechCommander 2 source used this style), the messages the original's. Values 62 .. 69 repeat the runtime errors'
/// messages.
/// </summary>
enum class MCAblSyntaxError : int32_t
{
    None = 0,
    Generic = 1,
    TooManyErrors = 2,
    SourceFileOpen = 3,
    UnexpectedEof = 4,
    InvalidNumber = 5,
    InvalidFraction = 6,
    InvalidExponent = 7,
    TooManyDigits = 8,
    RealOutOfRange = 9,
    IntegerOutOfRange = 10,
    MissingRParen = 11,
    InvalidExpression = 12,
    UndefinedIdentifier = 13,
    RedefinedIdentifier = 14,
    UnexpectedToken = 15,
    IncompatibleTypes = 16,
    NestingTooDeep = 17,
    CodeSegmentOverflow = 18,
    MissingEqual = 19,
    MissingSemicolon = 20,
    InvalidConstant = 21,
    NotAConstantIdentifier = 22,
    NoRecordTypes = 23,
    MissingColon = 24,
    NotATypeIdentifier = 25,
    InvalidType = 26,
    MissingEnd = 27,
    InvalidIdentifierUsage = 28,
    TooManySubscripts = 29,
    MissingRBracket = 30,
    IncompatibleAssignment = 31,
    MissingUntil = 32,
    MissingThen = 33,
    InvalidForControl = 34,
    MissingIdentifier = 35,
    MissingTo = 36,
    MissingPeriod = 37,
    MissingModule = 38,
    MissingLibrary = 39,
    AlreadyForwarded = 40,
    InvalidRefParam = 41,
    WrongNumberOfParams = 42,
    MissingBegin = 43,
    MissingEndVar = 44,
    NoFunctionNesting = 45,
    MissingCode = 46,
    MissingEndIf = 47,
    MissingEndWhile = 48,
    MissingEndFor = 49,
    MissingEndFunction = 50,
    MissingEndModule = 51,
    MissingEndLibrary = 52,
    MissingDo = 53,
    InvalidIndexType = 54,
    MissingComma = 55,
    TooManyStaticVars = 56,
    MissingEndCase = 57,
    MissingEndSwitch = 58,
    MissingConstant = 59,
    BadLanguageDirectiveParam = 60,
    UndefinedLanguageDirective = 61,
    RuntimeStackOverflow = 62,
    InfiniteLoop = 63,
    NestedFunctionCall = 64,
    /// <summary>Also what the port reports for subrange types and record fields (OB-141).</summary>
    UnimplementedFeature = 65,
    /// <summary>Also what AblPreProcess reports for text after the module's closing period.</summary>
    ValueOutOfRange = 66,
    DivisionByZero = 67,
    InvalidFunctionArgument = 68,
    InvalidCaseValue = 69,
    Count
};

/// <summary>An ABL execution error.</summary>
enum class MCAblRuntimeError : int32_t
{
    StackOverflow = 0,
    InfiniteLoop = 1,
    NestedFunctionCall = 2,
    UnimplementedFeature = 3,
    ValueOutOfRange = 4,
    DivisionByZero = 5,
    InvalidFunctionArgument = 6,
    InvalidCaseValue = 7,
    Abort = 8,
    Count
};

/// <summary>The original's message of <paramref name="error"/>.</summary>
std::string_view MCAblSyntaxErrorText(MCAblSyntaxError error);

/// <summary>The original's message of <paramref name="error"/>.</summary>
std::string_view MCAblRuntimeErrorText(MCAblRuntimeError error);

/// <summary>
/// The syntax error that ended a compile: what it was and where. The first syntax error ends a compile (in MCX.EXE it
/// was fatal at once); MCAblCompiler throws this to leave the parser and returns it.
/// </summary>
struct MCAblCompileError
{
    MCAblSyntaxError Code = MCAblSyntaxError::None;
    /// <summary>The source file being read when it happened.</summary>
    std::string FileName;
    /// <summary>The line in that file (0 when the module's own file couldn't be opened).</summary>
    int32_t LineNumber = 0;

    /// <summary>The original's report: <c>SYNTAX ERROR file [line n] - (type n) message</c>, with a line break.</summary>
    std::string Message() const;
};
