#pragma once

#include "abl/MCAblErrors.h"
#include "abl/MCAblToken.h"

/// <summary>What the literal of a number or string token is.</summary>
enum class MCAblLiteralType : int32_t
{
    Integer = 0,
    Real = 1,
    String = 2
};

/// <summary>The value of the number or string token just scanned.</summary>
struct MCAblLiteral
{
    MCAblLiteralType Type = MCAblLiteralType::Integer;
    int32_t Integer = 0;
    float Real = 0;
    std::string String;
};

/// <summary>What the language directives switch: whether <c>assert</c>, <c>print</c> and <c>concat</c> calls compile.</summary>
struct MCAblDirectives
{
    bool Assert = false;
    bool Print = false;
    bool StringFunctions = true;
};

/// <summary>
/// How many source files one module can be built from. Kept: a statement marker stores the file number in one byte
/// (ablexec.h).
/// </summary>
inline constexpr size_t MaxAblSourceFiles = 256;

/// <summary>
/// The ABL scanner (the original's ablscan.cpp): turns a module's source into tokens for MCAblCompiler. It reads the
/// module's file and the files it includes line by line, handles comments and language directives, and leaves the
/// current token in <see cref="Token"/>, <see cref="Text"/>, <see cref="Word"/> and <see cref="Literal"/>. A syntax
/// error throws <see cref="MCAblCompileError"/> with the file and line being read.
/// </summary>
class MCAblScanner
{
public:
    /// <param name="directives">The switches the directives set.</param>
    /// <param name="inLibrary">Whether a library is compiled (its end keyword is <c>endlibrary</c>).</param>
    MCAblScanner(MCAblDirectives& directives, bool inLibrary);
    ~MCAblScanner();

    MCAblScanner(const MCAblScanner&) = delete;
    MCAblScanner& operator=(const MCAblScanner&) = delete;

    /// <summary>Opens the module's own file (source file 0) and reads its first character.</summary>
    /// <returns>Whether it opened.</returns>
    bool Open(std::string_view fileName);

    /// <summary>Scans the next token.</summary>
    void Next();

    /// <summary>The token just scanned.</summary>
    MCAblToken Token() const { return _Token; }

    /// <summary>The token as written (a string literal without its quotes).</summary>
    const std::string& Text() const { return _Text; }

    /// <summary>The word token just scanned, lower-cased (identifiers are case-insensitive).</summary>
    const std::string& Word() const { return _Word; }

    /// <summary>The value of the number or string token just scanned.</summary>
    const MCAblLiteral& Literal() const { return _Literal; }

    /// <summary>The line being read in the file being read.</summary>
    int32_t LineNumber() const { return _LineNumber; }

    /// <summary>The file being read: its index in <see cref="SourceFiles"/>.</summary>
    int32_t FileNumber() const { return _FileNumber; }

    /// <summary>Every file opened so far, in the order they were opened (an include opened twice is listed twice).</summary>
    const std::vector<std::string>& SourceFiles() const { return _SourceFiles; }

    /// <summary>Throws <paramref name="error"/> at the file and line being read.</summary>
    [[noreturn]] void Error(MCAblSyntaxError error) const;

private:
    /// <summary>A file being read: the module's own or an included one.</summary>
    struct SourceFile
    {
        /// <summary>The whole file.</summary>
        std::string Text;
        /// <summary>Where the next line starts.</summary>
        size_t Position = 0;
        int32_t FileNumber = 0;
        /// <summary>Its line number, kept while a file it includes is read.</summary>
        int32_t LineNumber = 0;
    };

    /// <summary>
    /// Opens a file, makes it the one read and reads its first character (the original's openSourceFile).
    /// </summary>
    /// <returns>Whether it opened.</returns>
    bool OpenFile(std::string_view fileName);

    /// <summary>Goes back to the file that included the current one (the original's closeSourceFile).</summary>
    void CloseFile();

    /// <summary>Reads the next line of the current file.</summary>
    /// <returns>False at the end of the file.</returns>
    bool ReadLine();

    /// <summary>
    /// Reads the next character into <c>_CurChar</c>, reading the next line (or leaving an include) at the end of the
    /// line. Turns tabs and line ends into spaces and handles comments and directives (the original's getChar).
    /// </summary>
    void NextChar();

    /// <summary>Skips a <c>/* ... */</c> comment.</summary>
    void SkipBlockComment();

    /// <summary>
    /// Handles a <c>#</c> directive: <c>#include "file"</c> (the name as written), <c>#include_ "file"</c> (relative to
    /// the folder of the module's own file), <c>#assert_on</c>, <c>#assert_off</c>, <c>#print_on</c>,
    /// <c>#print_off</c>, <c>#stringfuncs_on</c>, <c>#stringfuncs_off</c>.
    /// </summary>
    void LanguageDirective();

    /// <summary>Reads a double-quoted <c>#include</c> file name.</summary>
    std::string DirectiveFileName();

    /// <summary>Scans an identifier (possibly <c>library.name</c>) or reserved word.</summary>
    void ScanWord();

    /// <summary>Adds a run of digits to <paramref name="value"/>; <paramref name="error"/> if there are none.</summary>
    void AccumulateValue(float& value, MCAblSyntaxError error);

    /// <summary>Scans an integer or real number.</summary>
    void ScanNumber();

    /// <summary>Scans a double-quoted string.</summary>
    void ScanString();

    /// <summary>Scans an operator or punctuation token.</summary>
    void ScanSpecial();

    MCAblDirectives& _Directives;
    bool _InLibrary = false;
    /// <summary>The include stack: the module's own file, then each file included from the one before.</summary>
    std::vector<SourceFile> _OpenFiles;
    std::vector<std::string> _SourceFiles;
    /// <summary>The current line, always ending in a zero (the original's sourceBuffer).</summary>
    std::string _Line = std::string(1, '\0');
    /// <summary>The next character to read in <c>_Line</c>.</summary>
    size_t _Position = 0;
    char _CurChar = 0;
    /// <summary>Set while comments and directives are read, so their characters aren't interpreted.</summary>
    bool _RawChars = false;
    MCAblToken _Token = MCAblToken::None;
    std::string _Text;
    std::string _Word;
    MCAblLiteral _Literal;
    int32_t _LineNumber = 0;
    int32_t _FileNumber = 0;
    /// <summary>Digits of the number being scanned, and whether there were too many.</summary>
    int32_t _DigitCount = 0;
    bool _CountError = false;
};
