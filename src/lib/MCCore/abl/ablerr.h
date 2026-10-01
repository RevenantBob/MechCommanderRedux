#pragma once

// ABL error reporting: compile-time syntax errors (syntaxError) and execution-time runtime errors (runtimeError).
// Both are fatal in MCX.EXE: they build a message and call Fatal.

/// <summary>
/// ABL compile errors. The values index syntaxErrorMessages; the names are the port's (the MechCommander 2 source
/// used this style), the messages the original's. Values 62 .. 69 duplicate the runtime errors' messages.
/// </summary>
enum SyntaxErrorType
{
    ABL_NO_ERROR = 0,
    ABL_ERR_SYNTAX_GENERIC = 1,
    ABL_ERR_SYNTAX_TOO_MANY_ERRORS = 2,
    ABL_ERR_SYNTAX_SOURCE_FILE_OPEN = 3,
    ABL_ERR_SYNTAX_UNEXPECTED_EOF = 4,
    ABL_ERR_SYNTAX_INVALID_NUMBER = 5,
    ABL_ERR_SYNTAX_INVALID_FRACTION = 6,
    ABL_ERR_SYNTAX_INVALID_EXPONENT = 7,
    ABL_ERR_SYNTAX_TOO_MANY_DIGITS = 8,
    ABL_ERR_SYNTAX_REAL_OUT_OF_RANGE = 9,
    ABL_ERR_SYNTAX_INTEGER_OUT_OF_RANGE = 10,
    ABL_ERR_SYNTAX_MISSING_RPAREN = 11,
    ABL_ERR_SYNTAX_INVALID_EXPRESSION = 12,
    ABL_ERR_SYNTAX_UNDEFINED_IDENTIFIER = 13,
    ABL_ERR_SYNTAX_REDEFINED_IDENTIFIER = 14,
    ABL_ERR_SYNTAX_UNEXPECTED_TOKEN = 15,
    ABL_ERR_SYNTAX_INCOMPATIBLE_TYPES = 16,
    ABL_ERR_SYNTAX_NESTING_TOO_DEEP = 17,
    ABL_ERR_SYNTAX_CODE_SEGMENT_OVERFLOW = 18,
    ABL_ERR_SYNTAX_MISSING_EQUAL = 19,
    ABL_ERR_SYNTAX_MISSING_SEMICOLON = 20,
    ABL_ERR_SYNTAX_INVALID_CONSTANT = 21,
    ABL_ERR_SYNTAX_NOT_A_CONSTANT_IDENTIFIER = 22,
    ABL_ERR_SYNTAX_NO_RECORD_TYPES = 23,
    ABL_ERR_SYNTAX_MISSING_COLON = 24,
    ABL_ERR_SYNTAX_NOT_A_TYPE_IDENTIFIER = 25,
    ABL_ERR_SYNTAX_INVALID_TYPE = 26,
    ABL_ERR_SYNTAX_MISSING_END = 27,
    ABL_ERR_SYNTAX_INVALID_IDENTIFIER_USAGE = 28,
    ABL_ERR_SYNTAX_TOO_MANY_SUBSCRIPTS = 29,
    ABL_ERR_SYNTAX_MISSING_RBRACKET = 30,
    ABL_ERR_SYNTAX_INCOMPATIBLE_ASSIGNMENT = 31,
    ABL_ERR_SYNTAX_MISSING_UNTIL = 32,
    ABL_ERR_SYNTAX_MISSING_THEN = 33,
    ABL_ERR_SYNTAX_INVALID_FOR_CONTROL = 34,
    ABL_ERR_SYNTAX_MISSING_IDENTIFIER = 35,
    ABL_ERR_SYNTAX_MISSING_TO = 36,
    ABL_ERR_SYNTAX_MISSING_PERIOD = 37,
    ABL_ERR_SYNTAX_MISSING_MODULE = 38,
    ABL_ERR_SYNTAX_MISSING_LIBRARY = 39,
    ABL_ERR_SYNTAX_ALREADY_FORWARDED = 40,
    ABL_ERR_SYNTAX_INVALID_REF_PARAM = 41,
    ABL_ERR_SYNTAX_WRONG_NUMBER_OF_PARAMS = 42,
    ABL_ERR_SYNTAX_MISSING_BEGIN = 43,
    ABL_ERR_SYNTAX_MISSING_END_VAR = 44,
    ABL_ERR_SYNTAX_NO_FUNCTION_NESTING = 45,
    ABL_ERR_SYNTAX_MISSING_CODE = 46,
    ABL_ERR_SYNTAX_MISSING_END_IF = 47,
    ABL_ERR_SYNTAX_MISSING_END_WHILE = 48,
    ABL_ERR_SYNTAX_MISSING_END_FOR = 49,
    ABL_ERR_SYNTAX_MISSING_END_FUNCTION = 50,
    ABL_ERR_SYNTAX_MISSING_END_MODULE = 51,
    ABL_ERR_SYNTAX_MISSING_END_LIBRARY = 52,
    ABL_ERR_SYNTAX_MISSING_DO = 53,
    ABL_ERR_SYNTAX_INVALID_INDEX_TYPE = 54,
    ABL_ERR_SYNTAX_MISSING_COMMA = 55,
    ABL_ERR_SYNTAX_TOO_MANY_STATIC_VARS = 56,
    ABL_ERR_SYNTAX_MISSING_END_CASE = 57,
    ABL_ERR_SYNTAX_MISSING_END_SWITCH = 58,
    ABL_ERR_SYNTAX_MISSING_CONSTANT = 59,
    ABL_ERR_SYNTAX_BAD_LANGUAGE_DIRECTIVE_PARAM = 60,
    ABL_ERR_SYNTAX_UNKNOWN_LANGUAGE_DIRECTIVE = 61,
    ABL_ERR_SYNTAX_RUNTIME_STACK_OVERFLOW = 62,
    ABL_ERR_SYNTAX_INFINITE_LOOP = 63,
    ABL_ERR_SYNTAX_NESTED_FUNCTION_CALL = 64,
    ABL_ERR_SYNTAX_UNIMPLEMENTED_FEATURE = 65,
    /// <summary>Also what ABLi_preProcess reports for text after the module's closing period.</summary>
    ABL_ERR_SYNTAX_VALUE_OUT_OF_RANGE = 66,
    ABL_ERR_SYNTAX_DIVISION_BY_ZERO = 67,
    ABL_ERR_SYNTAX_INVALID_FUNCTION_ARGUMENT = 68,
    ABL_ERR_SYNTAX_INVALID_CASE_VALUE = 69,
    NUM_ABL_SYNTAX_ERRORS
};

/// <summary>ABL execution errors; the values index runtimeErrorMessages.</summary>
enum RuntimeErrorType
{
    ABL_ERR_RUNTIME_STACK_OVERFLOW = 0,
    ABL_ERR_RUNTIME_INFINITE_LOOP = 1,
    ABL_ERR_RUNTIME_NESTED_FUNCTION_CALL = 2,
    ABL_ERR_RUNTIME_UNIMPLEMENTED_FEATURE = 3,
    ABL_ERR_RUNTIME_VALUE_OUT_OF_RANGE = 4,
    ABL_ERR_RUNTIME_DIVISION_BY_ZERO = 5,
    ABL_ERR_RUNTIME_INVALID_FUNCTION_ARGUMENT = 6,
    ABL_ERR_RUNTIME_INVALID_CASE_VALUE = 7,
    ABL_ERR_RUNTIME_ABORT = 8,
    NUM_ABL_RUNTIME_ERRORS
};

/// <summary>Message of each SyntaxErrorType.</summary>
extern const char* syntaxErrorMessages[NUM_ABL_SYNTAX_ERRORS];
/// <summary>Message of each RuntimeErrorType.</summary>
extern const char* runtimeErrorMessages[NUM_ABL_RUNTIME_ERRORS];
/// <summary>Syntax errors of the current compile (runtime errors reset it too).</summary>
extern int32_t errorCount;

/// <summary>Reports a compile error with the file and line (fatal), and kills the current token.</summary>
/// <remarks>MCX.EXE @ 0x00622d50</remarks>
void syntaxError(int32_t errCode);

/// <summary>
/// Reports an execution error: through the debugger (and its break mode) when there is one, then fatally with the
/// module, file and line.
/// </summary>
/// <remarks>MCX.EXE @ 0x00622de0 (filed under ablerr.cpp by address; the line tables don't name it)</remarks>
void runtimeError(int32_t errCode);
