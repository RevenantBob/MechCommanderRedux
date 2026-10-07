#pragma once

// ABL scanner: turns ABL source text into tokens for the compiler (abldecl, ablexpr, ablstmt, ablrtn). ABL is FASA's
// Pascal-like scripting language for unit brains, mission scripts and libraries; the scanner reads a source file line
// by line (with #include nesting) and leaves the current token in curToken / tokenString / curLiteral.

#include "abl/ablerr.h"

class MCFile;

/// <summary>Longest source line the scanner reads (sourceBuffer).</summary>
inline constexpr int32_t MAXLEN_SOURCELINE = 2048;
/// <summary>Longest token (tokenString, wordString, Literal::value.string).</summary>
inline constexpr int32_t MAXLEN_TOKENSTRING = 2048;
/// <summary>Longest source file name (SourceFiles, SourceFile::fileName).</summary>
inline constexpr int32_t MAXLEN_FILENAME = 256;
/// <summary>How deep #include files nest (openFiles).</summary>
inline constexpr int32_t MAX_INCLUDE_DEPTH = 6;
/// <summary>How many source files one module can be built from (SourceFiles).</summary>
inline constexpr int32_t MAX_SOURCE_FILES = 256;
/// <summary>Columns of a listing line (printLine truncates there).</summary>
inline constexpr int32_t MAX_PRINT_LINE_LENGTH = 80;
/// <summary>Listing lines per page (printLine).</summary>
inline constexpr int32_t MAX_LINES_PER_PAGE = 50;

/// <summary>
/// The tokens of ABL. The values are the original's: they are stored as single bytes in crunched code (see
/// ablexec.h) and index TokenStrings. Reserved words map to TKN_CODE .. TKN_STATIC through keywordTable.
/// </summary>
enum MCTokenCodeType
{
    TKN_NONE = 0,
    TKN_IDENTIFIER = 1,
    TKN_NUMBER = 2,
    TKN_TYPE = 3,
    TKN_STRING = 4,
    TKN_STAR = 5,
    TKN_LPAREN = 6,
    TKN_RPAREN = 7,
    TKN_MINUS = 8,
    TKN_PLUS = 9,
    TKN_EQUAL = 10,
    TKN_LBRACKET = 11,
    TKN_RBRACKET = 12,
    TKN_COLON = 13,
    TKN_SEMICOLON = 14,
    TKN_LT = 15,
    TKN_GT = 16,
    TKN_COMMA = 17,
    TKN_PERIOD = 18,
    TKN_FSLASH = 19,
    TKN_EQUALEQUAL = 20,
    TKN_LE = 21,
    TKN_GE = 22,
    TKN_NE = 23,
    TKN_EOF = 24,
    TKN_ERROR = 25,
    TKN_CODE = 26,
    /// <summary>"orders" (TokenStrings); no reserved word produces it in MCX.EXE.</summary>
    TKN_ORDERS = 27,
    TKN_AND = 28,
    TKN_SWITCH = 29,
    TKN_CASE = 30,
    TKN_CONST = 31,
    TKN_DIV = 32,
    TKN_DO = 33,
    TKN_OF = 34,
    TKN_ELSE = 35,
    TKN_END_IF = 36,
    TKN_END_WHILE = 37,
    TKN_END_FOR = 38,
    TKN_END_FUNCTION = 39,
    TKN_END_MODULE = 40,
    TKN_END_LIBRARY = 41,
    TKN_END_VAR = 42,
    TKN_END_CODE = 43,
    TKN_END_CASE = 44,
    TKN_END_SWITCH = 45,
    TKN_FOR = 46,
    TKN_FUNCTION = 47,
    TKN_IF = 48,
    TKN_MOD = 49,
    TKN_NOT = 50,
    TKN_OR = 51,
    TKN_REPEAT = 52,
    TKN_THEN = 53,
    TKN_TO = 54,
    TKN_UNTIL = 55,
    TKN_VAR = 56,
    /// <summary>'@': marks a reference parameter.</summary>
    TKN_REF = 57,
    TKN_WHILE = 58,
    TKN_ELSIF = 59,
    TKN_RETURN = 60,
    TKN_MODULE = 61,
    TKN_LIBRARY = 62,
    TKN_ETERNAL = 63,
    TKN_STATIC = 64,
    /// <summary>'#' outside a language directive.</summary>
    TKN_POUND = 65,
    TKN_UNEXPECTED_TOKEN = 66,
    /// <summary>'C' in crunched code: a statement marker (ablexec.h).</summary>
    TKN_STATEMENT_MARKER = 67,
    /// <summary>'D' in crunched code: an address marker (ablexec.h).</summary>
    TKN_ADDRESS_MARKER = 68,
    NUM_TOKENS
};

/// <summary>What kind of character starts a token (charTable).</summary>
enum MCCharCodeType
{
    CHR_LETTER = 0,
    CHR_DIGIT = 1,
    CHR_DQUOTE = 2,
    CHR_SPECIAL = 3,
    /// <summary>Character 0x7f, which getChar returns at the end of the source.</summary>
    CHR_EOF = 4
};

/// <summary>What curLiteral holds.</summary>
enum MCLiteralType
{
    LIT_INTEGER = 0,
    LIT_REAL = 1,
    LIT_STRING = 2
};

/// <summary>The value of the number or string token just scanned.</summary>
/// <remarks>0x808 bytes in the original (curLiteral @ 0x007c4528).</remarks>
struct MCLiteral
{
    MCLiteralType Type{};
    struct
    {
        int32_t Integer = 0;
        float Real = 0;
        char String[MAXLEN_TOKENSTRING]{};
    } Value{};
};

/// <summary>An entry of a reserved-word table: the word and the token it scans as.</summary>
/// <remarks>8 bytes in the original. The tables (keywords2 .. keywords11) are grouped by word length.</remarks>
struct MCReservedWord
{
    const char* String = nullptr;
    MCTokenCodeType TokenCode{};
};

/// <summary>A source file open for scanning; openFiles stacks them for #include.</summary>
/// <remarks>0x10c bytes in the original (openFiles @ 0x007c3ee0, an unnamed global).</remarks>
struct MCSourceFile
{
    char FileName[MAXLEN_FILENAME]{};
    /// <summary>Its index in SourceFiles (FileNumber while it is scanned).</summary>
    uint8_t FileNumber = 0;
    MCFile* FilePtr = nullptr;
    /// <summary>The line the including file had reached, restored when this one closes.</summary>
    int32_t LineNumber = 0;
};

/// <summary>The reserved words (keywords) of 2 .. 11 letters, each table ending with a null entry.</summary>
extern MCReservedWord Keywords2[];
extern MCReservedWord Keywords3[];
extern MCReservedWord Keywords4[];
extern MCReservedWord Keywords5[];
extern MCReservedWord Keywords6[];
extern MCReservedWord Keywords7[];
extern MCReservedWord Keywords8[];
extern MCReservedWord Keywords9[];
extern MCReservedWord Keywords10[];
extern MCReservedWord Keywords11[];
/// <summary>The reserved-word tables by word length 0 .. 11 (entries 0 and 1 are null).</summary>
extern MCReservedWord* KeywordTable[12];

/// <summary>Character class of every byte value.</summary>
extern MCCharCodeType CharTable[256];
/// <summary>The current source line.</summary>
extern char SourceBuffer[MAXLEN_SOURCELINE];
/// <summary>The next character to read in sourceBuffer.</summary>
extern char* Bufferp;
/// <summary>Where the next character of the token goes in tokenString.</summary>
extern char* Tokenp;
/// <summary>The current token as written.</summary>
extern char TokenString[MAXLEN_TOKENSTRING];
/// <summary>The current word token, lower-cased (identifiers are case-insensitive).</summary>
extern char WordString[MAXLEN_TOKENSTRING];
/// <summary>The character being scanned.</summary>
extern char CurChar;
/// <summary>The token just scanned.</summary>
extern MCTokenCodeType CurToken;
/// <summary>The value of the number or string token just scanned.</summary>
extern MCLiteral CurLiteral;
/// <summary>Names of every source file of the module being compiled; FileNumber indexes it.</summary>
extern char SourceFiles[MAX_SOURCE_FILES][MAXLEN_FILENAME];
extern int32_t NumSourceFiles;
/// <summary>The #include stack (an unnamed global of the original, @ 0x007c3ee0).</summary>
extern MCSourceFile OpenFiles[MAX_INCLUDE_DEPTH];
extern int32_t NumOpenFiles;
/// <summary>The file being scanned.</summary>
extern MCFile* SourceFile;
/// <summary>Line number in the file being scanned.</summary>
extern int32_t LineNumber;
/// <summary>SourceFiles index of the file being scanned (or executed, from statement markers).</summary>
extern int32_t FileNumber;
/// <summary>Column of bufferp, with tabs expanded to 4.</summary>
extern int32_t BufferOffset;
/// <summary>Nonzero while getChar must not interpret comments and directives (inside them).</summary>
extern int DumbGetCharOn;
/// <summary>Digits read by getNumber, and whether there were too many.</summary>
extern int32_t DigitCount;
extern int CountError;
/// <summary>Nonzero to print each source line as it is read (a compile listing).</summary>
extern int PrintFlag;
/// <summary>Listing state: lines on the page, page number, header text.</summary>
extern int32_t LineCount;
extern int32_t PageNumber;
extern char SourceName[256];
extern char Date[26];

/// <summary>Looks wordString up in the reserved words; on a hit sets curToken.</summary>
int IsReservedWord();

/// <summary>Sets up charTable and, with a file name, opens it and reads its first character.</summary>
void InitScanner(char* fileName);

/// <summary>Closes the source file.</summary>
void QuitScanner();

/// <summary>Skips a <c>/* ... */</c> comment.</summary>
void SkipBlockComment();

/// <summary>Skips spaces (getChar turns tabs and line ends into spaces).</summary>
void SkipBlanks();

/// <summary>
/// Handles a <c>#</c> directive: <c>#include "file"</c> (the name as written), <c>#include_ "file"</c> (relative to
/// the folder of the module's main file), <c>#assert_on</c>, <c>#assert_off</c>, <c>#print_on</c>, <c>#print_off</c>,
/// <c>#stringfuncs_on</c>, <c>#stringfuncs_off</c>.
/// </summary>
void LanguageDirective();

/// <summary>
/// Reads the next character into curChar, reading the next line (or closing an include) at the end of the buffer.
/// Turns tabs and line ends into spaces, and handles comments and directives.
/// </summary>
void GetChar();

/// <summary>Copies tokenString to wordString in lower case.</summary>
void DownShiftWord();

/// <summary>Scans the next token; inside a code block it is also crunched into the code buffer.</summary>
void GetToken();

/// <summary>Scans an identifier (possibly <c>library.name</c>) or reserved word.</summary>
void GetWord();

/// <summary>Accumulates a run of digits into <paramref name="valuePtr"/>; <paramref name="errCode"/> if none.</summary>
void AccumulateValue(float* valuePtr, MCSyntaxErrorType errCode);

/// <summary>Scans an integer or real number into curLiteral.</summary>
void ScanNumber();

/// <summary>Scans a double-quoted string into curLiteral.</summary>
void GetString();

/// <summary>Scans an operator or punctuation token.</summary>
void GetSpecial();

/// <summary>Whether curToken is in the zero-terminated <paramref name="tokenList"/>.</summary>
int TokenIn(MCTokenCodeType* tokenList);

/// <summary>
/// Error recovery: if curToken is in none of the lists, reports it and skips tokens until one is (or EOF).
/// </summary>
void Synchronize(MCTokenCodeType* tokenList1, MCTokenCodeType* tokenList2, MCTokenCodeType* tokenList3);

/// <summary>Reads the next line of the source file into sourceBuffer.</summary>
/// <returns>0 at the end of the file.</returns>
int GetSourceLine();

/// <summary>Opens a source (or #include) file, pushes it on openFiles and adds it to SourceFiles.</summary>
/// <returns>0, or -1 (no name), -2 (includes too deep), -3 (too many files, or it can't be opened).</returns>
int32_t OpenSourceFile(char* sourceFileName);

/// <summary>Closes the current source file and returns to the one that included it.</summary>
int32_t CloseSourceFile();

/// <summary>Prints a listing line (truncated to 80 columns), with page headers.</summary>
void PrintLine(char* line);

/// <summary>Sets the listing's file name and date.</summary>
void InitPageHeader(char* fileName);

/// <summary>Prints the listing's page header.</summary>
void PrintPageHeader();
