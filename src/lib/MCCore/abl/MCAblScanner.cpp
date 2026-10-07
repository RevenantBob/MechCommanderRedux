#include "stdafx.h"
#include "abl/MCAblScanner.h"
#include "lib/MCFile.h"

namespace
{
    /// <summary>The character getChar gives at the end of the source.</summary>
    constexpr char EofChar = 0x7f;

    /// <summary>What kind of character starts a token (the original's charTable).</summary>
    enum class MCCharClass
    {
        Letter,
        Digit,
        DoubleQuote,
        Special,
        Eof
    };

    /// <summary>The class of <paramref name="ch"/>.</summary>
    /// <remarks>
    /// Port fix: the original indexed charTable with the signed char, so bytes 0x80..0xff read the 512 bytes before
    /// it. They are Special here (they scan as an error token).
    /// </remarks>
    constexpr auto Classify(char ch) -> MCCharClass
    {
        if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z'))
        {
            return MCCharClass::Letter;
        }

        if (ch >= '0' && ch <= '9')
        {
            return MCCharClass::Digit;
        }

        if (ch == '"')
        {
            return MCCharClass::DoubleQuote;
        }

        return ch == EofChar ? MCCharClass::Eof : MCCharClass::Special;
    }

    /// <summary>Whether <paramref name="ch"/> can continue an identifier.</summary>
    constexpr auto IsWordChar(char ch) -> bool
    {
        return Classify(ch) == MCCharClass::Letter || Classify(ch) == MCCharClass::Digit || ch == '_';
    }

    /// <summary>A reserved word and the token it scans as.</summary>
    struct MCReservedWord
    {
        std::string_view Word;
        MCAblToken Token;
    };

    /// <summary>
    /// The reserved words (the original's keywords2 .. keywords11 tables). Several tokens have no word: "div",
    /// "of", "elsif", "return", "orders", "endvar" and "endcode" scan as identifiers.
    /// </summary>
    constexpr MCReservedWord ReservedWords[] = {
        {"if", MCAblToken::If},
        {"or", MCAblToken::Or},
        {"do", MCAblToken::Do},
        {"to", MCAblToken::To},
        {"and", MCAblToken::And},
        {"for", MCAblToken::For},
        {"mod", MCAblToken::Mod},
        {"not", MCAblToken::Not},
        {"var", MCAblToken::Var},
        {"else", MCAblToken::Else},
        {"then", MCAblToken::Then},
        {"case", MCAblToken::Case},
        {"code", MCAblToken::Code},
        {"type", MCAblToken::Type},
        {"const", MCAblToken::Const},
        {"until", MCAblToken::Until},
        {"while", MCAblToken::While},
        {"endif", MCAblToken::EndIf},
        {"module", MCAblToken::Module},
        {"repeat", MCAblToken::Repeat},
        {"endfor", MCAblToken::EndFor},
        {"switch", MCAblToken::Switch},
        {"static", MCAblToken::Static},
        {"endcase", MCAblToken::EndCase},
        {"eternal", MCAblToken::Eternal},
        {"library", MCAblToken::Library},
        {"function", MCAblToken::Function},
        {"endwhile", MCAblToken::EndWhile},
        {"endswitch", MCAblToken::EndSwitch},
        {"endmodule", MCAblToken::EndModule},
        {"endlibrary", MCAblToken::EndLibrary},
        {"endfunction", MCAblToken::EndFunction},
    };

    /// <summary>Each token's text (the original's TokenStrings).</summary>
    constexpr std::array<std::string_view, static_cast<size_t>(MCAblToken::Count)> TokenTexts = {
        "{BAD TOKEN}",
        "{IDENTIFIER}",
        "{NUMBER}",
        "{TYPE}",
        "{STRING}",
        "*",
        "(",
        ")",
        "-",
        "+",
        "=",
        "[",
        "]",
        ":",
        ";",
        "<",
        ">",
        ",",
        ".",
        "/",
        "==",
        "<=",
        ">=",
        "<>",
        "{EOF}",
        "{ERROR}",
        "code",
        "orders",
        "and",
        "switch",
        "case",
        "const",
        "div",
        "do",
        "of",
        "else",
        "endif",
        "endwhile",
        "endfor",
        "endfunction",
        "endmodule",
        "endlibrary",
        "endvar",
        "endcode",
        "endcase",
        "endswitch",
        "for",
        "function",
        "if",
        "mod",
        "not",
        "or",
        "repeat",
        "then",
        "to",
        "until",
        "var",
        "@",
        "while",
        "elsif",
        "return",
        "module",
        "library",
        "eternal",
        "static",
        "#",
        "{UNEXPECTED TOKEN}",
        "{STATEMENT MARKER}",
        "{ADDRESS MARKER}",
    };

    /// <summary>The single-character operators and punctuation that no second character can extend.</summary>
    constexpr auto SingleCharToken(char ch) -> MCAblToken
    {
        switch (ch)
        {
            case '#':
            {
                return MCAblToken::Pound;
            }
            case '(':
            {
                return MCAblToken::LParen;
            }
            case ')':
            {
                return MCAblToken::RParen;
            }
            case '*':
            {
                return MCAblToken::Star;
            }
            case '+':
            {
                return MCAblToken::Plus;
            }
            case ',':
            {
                return MCAblToken::Comma;
            }
            case '-':
            {
                return MCAblToken::Minus;
            }
            case '.':
            {
                return MCAblToken::Period;
            }
            case '/':
            {
                return MCAblToken::Slash;
            }
            case ':':
            {
                return MCAblToken::Colon;
            }
            case ';':
            {
                return MCAblToken::Semicolon;
            }
            case '@':
            {
                return MCAblToken::Ref;
            }
            case '[':
            {
                return MCAblToken::LBracket;
            }
            case ']':
            {
                return MCAblToken::RBracket;
            }
            default:
            {
                return MCAblToken::Error;
            }
        }
    }
}

auto MCAblTokenText(MCAblToken token) -> std::string_view
{
    return TokenTexts[static_cast<size_t>(token)];
}

MCAblScanner::MCAblScanner(MCAblDirectives& directives, bool inLibrary) : _Directives(directives), _InLibrary(inLibrary)
{
}

MCAblScanner::~MCAblScanner() = default;

auto MCAblScanner::Open(std::string_view fileName) -> bool
{
    return OpenFile(fileName);
}

auto MCAblScanner::Error(MCAblSyntaxError error) const -> void
{
    throw MCAblCompileError{error, _SourceFiles.empty() ? std::string() : _SourceFiles[_FileNumber], _LineNumber};
}

auto MCAblScanner::OpenFile(std::string_view fileName) -> bool
{
    if (_SourceFiles.size() == MaxAblSourceFiles)
    {
        return false;
    }

    MCFile file;

    if (file.Open(fileName) != 0)
    {
        return false;
    }

    std::string text(file.FileSize(), '\0');

    if (!text.empty())
    {
        file.Read(reinterpret_cast<uint8_t*>(text.data()), static_cast<int32_t>(text.size()));
    }

    file.Close();

    _FileNumber = static_cast<int32_t>(_SourceFiles.size());
    _SourceFiles.emplace_back(fileName);

    if (!_OpenFiles.empty())
    {
        _OpenFiles.back().LineNumber = _LineNumber;
    }

    _OpenFiles.push_back({std::move(text), 0, _FileNumber, 0});
    _LineNumber = 0;
    // The rest of the line that included the file is dropped.
    _Line.assign(1, '\0');
    _Position = 0;
    NextChar();
    return true;
}

auto MCAblScanner::CloseFile() -> void
{
    _OpenFiles.pop_back();

    if (!_OpenFiles.empty())
    {
        _FileNumber = _OpenFiles.back().FileNumber;
        _LineNumber = _OpenFiles.back().LineNumber;
    }
}

auto MCAblScanner::ReadLine() -> bool
{
    SourceFile& file = _OpenFiles.back();

    if (file.Position >= file.Text.size())
    {
        return false;
    }

    // A line runs up to and including its LF (the original's readLineEx; its 2048-byte buffer is gone).
    const size_t end = file.Text.find('\n', file.Position);
    const size_t length = end == std::string::npos ? file.Text.size() - file.Position : end + 1 - file.Position;
    _Line.assign(file.Text, file.Position, length);
    _Line.push_back('\0');
    file.Position += length;
    _LineNumber++;
    return true;
}

auto MCAblScanner::NextChar() -> void
{
    if (_Line[_Position] == '\0')
    {
        if (!ReadLine())
        {
            // The end of an include goes back to the including file without a new character.
            if (_OpenFiles.size() > 1)
            {
                CloseFile();
                return;
            }

            _CurChar = EofChar;
            return;
        }

        _Position = 0;
    }

    _CurChar = _Line[_Position++];

    if (_RawChars)
    {
        return;
    }

    switch (_CurChar)
    {
        case '\t':
        case '\n':
        case '\r':
        {
            _CurChar = ' ';
            break;
        }
        case '#':
        {
            LanguageDirective();
            break;
        }
        case '/':
        {
            if (_Line[_Position] == '/')
            {
                // A line comment: drop the rest of the line.
                _CurChar = ' ';
                _Line[_Position] = '\0';
            }
            else if (_Line[_Position] == '*')
            {
                SkipBlockComment();
            }

            break;
        }
        default:
        {
            break;
        }
    }
}

auto MCAblScanner::SkipBlockComment() -> void
{
    _RawChars = true;
    NextChar();
    NextChar();

    while (true)
    {
        if (_CurChar != '*')
        {
            if (_CurChar == EofChar)
            {
                Error(MCAblSyntaxError::UnexpectedEof);
            }

            NextChar();
            continue;
        }

        NextChar();

        if (_CurChar == '/')
        {
            _CurChar = ' ';
            _RawChars = false;
            return;
        }
    }
}

auto MCAblScanner::DirectiveFileName() -> std::string
{
    NextChar();

    if (_CurChar != '"')
    {
        Error(MCAblSyntaxError::BadLanguageDirectiveParam);
    }

    NextChar();
    std::string fileName;

    // The original read at most 127 characters; with no closing quote it then failed to open the cut name.
    while (_CurChar != '"')
    {
        if (_CurChar == EofChar)
        {
            Error(MCAblSyntaxError::SourceFileOpen);
        }

        fileName.push_back(_CurChar);
        NextChar();
    }

    return fileName;
}

auto MCAblScanner::LanguageDirective() -> void
{
    _RawChars = true;
    NextChar();
    std::string directive;

    // The original read at most 31 characters (no directive is longer); the end of the source ends the name too.
    while (_CurChar != ' ' && _CurChar != '\n' && _CurChar != '\r' && _CurChar != EofChar)
    {
        directive.push_back(_CurChar);
        NextChar();
    }

    if (directive == "include" || directive == "include_")
    {
        std::string fileName = DirectiveFileName();
        _RawChars = false;

        // #include_ "file" is relative to the folder of the module's own file.
        if (directive == "include_")
        {
            if (const size_t lastSlash = _SourceFiles[0].rfind('\\'); lastSlash != std::string::npos)
            {
                fileName = _SourceFiles[0].substr(0, lastSlash + 1) + fileName;
            }
        }

        if (!OpenFile(fileName))
        {
            Error(MCAblSyntaxError::SourceFileOpen);
        }

        return;
    }

    struct MCSwitch
    {
        std::string_view Directive;
        bool MCAblDirectives::* Flag;
        bool Value;
    };

    static constexpr MCSwitch Switches[] = {
        {"assert_on", &MCAblDirectives::Assert, true},
        {"assert_off", &MCAblDirectives::Assert, false},
        {"print_on", &MCAblDirectives::Print, true},
        {"print_off", &MCAblDirectives::Print, false},
        {"stringfuncs_on", &MCAblDirectives::StringFunctions, true},
        {"stringfuncs_off", &MCAblDirectives::StringFunctions, false},
    };

    const auto found = std::ranges::find(Switches, directive, &MCSwitch::Directive);

    if (found == std::end(Switches))
    {
        Error(MCAblSyntaxError::UndefinedLanguageDirective);
    }

    _Directives.*(found->Flag) = found->Value;
    _RawChars = false;
    _CurChar = ' ';
}

auto MCAblScanner::Next() -> void
{
    while (_CurChar == ' ')
    {
        NextChar();
    }

    switch (Classify(_CurChar))
    {
        case MCCharClass::Letter:
        {
            ScanWord();
            break;
        }
        case MCCharClass::Digit:
        {
            ScanNumber();
            break;
        }
        case MCCharClass::DoubleQuote:
        {
            ScanString();
            break;
        }
        case MCCharClass::Eof:
        {
            // The text is left as the last token's.
            _Token = MCAblToken::Eof;
            break;
        }
        default:
        {
            ScanSpecial();
            break;
        }
    }
}

auto MCAblScanner::ScanWord() -> void
{
    _Text.clear();

    while (IsWordChar(_CurChar))
    {
        _Text.push_back(_CurChar);
        NextChar();
    }

    // "library.name" is one qualified identifier, unless the word is the module's end keyword.
    const MCAblToken endToken = _InLibrary ? MCAblToken::EndLibrary : MCAblToken::EndModule;
    auto lowerCase = [](std::string text)
    {
        std::ranges::transform(text, text.begin(),
                               [](char ch) { return ch >= 'A' && ch <= 'Z' ? ch + 'a' - 'A' : ch; });
        return text;
    };

    _Word = lowerCase(_Text);

    if (_CurChar == '.' && _Word != MCAblTokenText(endToken))
    {
        _Text.push_back('.');
        NextChar();

        while (IsWordChar(_CurChar))
        {
            _Text.push_back(_CurChar);
            NextChar();
        }

        _Word = lowerCase(_Text);
    }

    const auto keyword = std::ranges::find(ReservedWords, std::string_view(_Word), &MCReservedWord::Word);
    _Token = keyword != std::end(ReservedWords) ? keyword->Token : MCAblToken::Identifier;
}

auto MCAblScanner::AccumulateValue(float& value, MCAblSyntaxError error) -> void
{
    if (Classify(_CurChar) != MCCharClass::Digit)
    {
        Error(error);
    }

    float accumulated = value;

    do
    {
        _Text.push_back(_CurChar);

        // A number's digits are a language rule: past 20, too many digits.
        if (++_DigitCount <= 20)
        {
            accumulated = accumulated * 10.0f + static_cast<float>(_CurChar - '0');
        }
        else
        {
            _CountError = true;
        }

        NextChar();
    } while (Classify(_CurChar) == MCCharClass::Digit);

    value = accumulated;
}

auto MCAblScanner::ScanNumber() -> void
{
    _Text.clear();
    float numberValue = 0.0f;
    int32_t decimalOffset = 0;
    _DigitCount = 0;
    _CountError = false;
    _Literal.Type = MCAblLiteralType::Integer;

    AccumulateValue(numberValue, MCAblSyntaxError::InvalidNumber);
    const int32_t wholeCount = _DigitCount;

    if (_CurChar == '.')
    {
        NextChar();
        _Text.push_back('.');
        _Literal.Type = MCAblLiteralType::Real;
        AccumulateValue(numberValue, MCAblSyntaxError::InvalidFraction);
        decimalOffset = wholeCount - _DigitCount;
    }

    if (_CurChar == 'E' || _CurChar == 'e')
    {
        // Original behaviour (OB-037): the 'E' isn't consumed, so the exponent's sign and digits are looked for at
        // the 'E' itself, and every exponent is an invalid exponent.
        Error(MCAblSyntaxError::InvalidExponent);
    }

    if (_CountError)
    {
        Error(MCAblSyntaxError::TooManyDigits);
    }

    const int32_t exponent = decimalOffset;

    if (exponent + wholeCount < -20 || exponent + wholeCount > 20)
    {
        Error(MCAblSyntaxError::RealOutOfRange);
    }

    if (exponent != 0)
    {
        numberValue = static_cast<float>(std::pow(10.0, exponent) * numberValue);
    }

    if (_Literal.Type == MCAblLiteralType::Integer)
    {
        // __ftol gives 0x80000000 for anything out of range, which the original rejects.
        if (numberValue >= 2147483648.0f)
        {
            Error(MCAblSyntaxError::IntegerOutOfRange);
        }

        _Literal.Integer = static_cast<int32_t>(numberValue);
    }
    else
    {
        _Literal.Real = numberValue;
    }

    _Token = MCAblToken::Number;
}

auto MCAblScanner::ScanString() -> void
{
    // The token's text is the string without its quotes.
    _Text.clear();
    _Literal.String.clear();
    NextChar();

    while (_CurChar != EofChar && _CurChar != '"')
    {
        _Literal.String.push_back(_CurChar);
        _Text.push_back(_CurChar);
        NextChar();
    }

    // The closing quote is dropped by replacing it with a blank.
    _CurChar = ' ';
    _Token = MCAblToken::String;
    _Literal.Type = MCAblLiteralType::String;
}

auto MCAblScanner::ScanSpecial() -> void
{
    _Text.assign(1, _CurChar);

    // '<', '=' and '>' may take a second character.
    auto pair = [this](char second, MCAblToken paired, MCAblToken single)
    {
        NextChar();

        if (_CurChar != second)
        {
            return single;
        }

        _Text.push_back(second);
        NextChar();
        return paired;
    };

    switch (_CurChar)
    {
        case '<':
        {
            NextChar();

            if (_CurChar == '=' || _CurChar == '>')
            {
                _Token = _CurChar == '=' ? MCAblToken::LessEqual : MCAblToken::NotEqual;
                _Text.push_back(_CurChar);
                NextChar();
            }
            else
            {
                _Token = MCAblToken::Less;
            }

            return;
        }
        case '=':
        {
            _Token = pair('=', MCAblToken::EqualEqual, MCAblToken::Equal);
            return;
        }
        case '>':
        {
            _Token = pair('=', MCAblToken::GreaterEqual, MCAblToken::Greater);
            return;
        }
        default:
        {
            _Token = SingleCharToken(_CurChar);
            NextChar();
            return;
        }
    }
}
