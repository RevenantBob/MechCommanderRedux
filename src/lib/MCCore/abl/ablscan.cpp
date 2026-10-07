#include "stdafx.h"
#include "abl/ablscan.h"
#include "abl/abldbug.h"
#include "abl/ablenv.h"
#include "abl/ablexec.h"
#include "abl/ablrtn.h"
#include "abl/ablsymt.h"
#include "lib/MCFatal.h"
#include "lib/MCFile.h"

MCReservedWord Keywords2[] = {
    {"if", TKN_IF}, {"or", TKN_OR}, {"do", TKN_DO}, {"to", TKN_TO}, {nullptr, TKN_NONE},
};

MCReservedWord Keywords3[] = {
    {"and", TKN_AND}, {"for", TKN_FOR}, {"mod", TKN_MOD}, {"not", TKN_NOT}, {"var", TKN_VAR}, {nullptr, TKN_NONE},
};

MCReservedWord Keywords4[] = {
    {"else", TKN_ELSE}, {"then", TKN_THEN}, {"case", TKN_CASE},
    {"code", TKN_CODE}, {"type", TKN_TYPE}, {nullptr, TKN_NONE},
};

MCReservedWord Keywords5[] = {
    {"const", TKN_CONST}, {"until", TKN_UNTIL}, {"while", TKN_WHILE}, {"endif", TKN_END_IF}, {nullptr, TKN_NONE},
};

MCReservedWord Keywords6[] = {
    {"module", TKN_MODULE}, {"repeat", TKN_REPEAT}, {"endfor", TKN_END_FOR},
    {"switch", TKN_SWITCH}, {"static", TKN_STATIC}, {nullptr, TKN_NONE},
};

MCReservedWord Keywords7[] = {
    {"endcase", TKN_END_CASE},
    {"eternal", TKN_ETERNAL},
    {"library", TKN_LIBRARY},
    {nullptr, TKN_NONE},
};

MCReservedWord Keywords8[] = {
    {"function", TKN_FUNCTION},
    {"endwhile", TKN_END_WHILE},
    {nullptr, TKN_NONE},
};

MCReservedWord Keywords9[] = {
    {"endswitch", TKN_END_SWITCH},
    {"endmodule", TKN_END_MODULE},
    {nullptr, TKN_NONE},
};

MCReservedWord Keywords10[] = {
    {"endlibrary", TKN_END_LIBRARY},
    {nullptr, TKN_NONE},
};

MCReservedWord Keywords11[] = {
    {"endfunction", TKN_END_FUNCTION},
    {nullptr, TKN_NONE},
};

MCReservedWord* KeywordTable[12] = {
    nullptr,   nullptr,   Keywords2, Keywords3, Keywords4,  Keywords5,
    Keywords6, Keywords7, Keywords8, Keywords9, Keywords10, Keywords11,
};

const char* TokenStrings[NUM_TOKENS] = {
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

MCCharCodeType CharTable[256];
char SourceBuffer[MAXLEN_SOURCELINE];
char* Bufferp = SourceBuffer;
char* Tokenp = TokenString;
char TokenString[MAXLEN_TOKENSTRING];
char WordString[MAXLEN_TOKENSTRING];
char CurChar;
MCTokenCodeType CurToken;
MCLiteral CurLiteral;
char SourceFiles[MAX_SOURCE_FILES][MAXLEN_FILENAME];
int32_t NumSourceFiles;
MCSourceFile OpenFiles[MAX_INCLUDE_DEPTH];
int32_t NumOpenFiles;
MCFile* SourceFile;
int32_t LineNumber;
int32_t FileNumber;
int32_t BufferOffset;
int DumbGetCharOn;
int32_t DigitCount;
int CountError;
int PrintFlag = 1;
int32_t LineCount = MAX_LINES_PER_PAGE;
int32_t PageNumber;
char SourceName[256];
char Date[26];

namespace
{
    /// <summary>The character class of <paramref name="ch"/>.</summary>
    /// <remarks>
    /// Port fix: the original indexed charTable with the signed char, so bytes 0x80..0xff read the 512 bytes before
    /// it. The port indexes with the byte value (those bytes are CHR_SPECIAL, which scans as TKN_ERROR).
    /// </remarks>
    auto CharCode(char ch) -> MCCharCodeType
    {
        return CharTable[static_cast<uint8_t>(ch)];
    }

    /// <summary>Whether <paramref name="ch"/> can continue an identifier.</summary>
    auto IsWordChar(char ch) -> bool
    {
        return CharCode(ch) == CHR_LETTER || CharCode(ch) == CHR_DIGIT || ch == '_';
    }

    /// <summary>Reads a double-quoted #include file name into <paramref name="fileName"/> (up to 127 characters).
    /// </summary>
    /// <returns>false if curChar is not the opening quote.</returns>
    auto ReadDirectiveFileName(char (&fileName)[128]) -> bool
    {
        GetChar();

        if (CurChar != '"')
        {
            return false;
        }

        GetChar();
        int32_t i = 0;

        for (; CurChar != '"' && i < 127; i++)
        {
            fileName[i] = CurChar;
            GetChar();
        }

        fileName[i] = '\0';
        return true;
    }
}

auto IsReservedWord() -> int
{
    auto wordLength = static_cast<int32_t>(strlen(WordString));

    if (wordLength < 2 || wordLength > 11)
    {
        return 0;
    }

    MCReservedWord* rwp = KeywordTable[wordLength];

    if (rwp == nullptr)
    {
        return 0;
    }

    for (; rwp->String != nullptr; rwp++)
    {
        if (strcmp(WordString, rwp->String) == 0)
        {
            CurToken = rwp->TokenCode;
            return 1;
        }
    }

    return 0;
}

auto InitScanner(char* fileName) -> void
{
    for (auto& code : CharTable)
    {
        code = CHR_SPECIAL;
    }

    for (int32_t ch = '0'; ch <= '9'; ch++)
    {
        CharTable[ch] = CHR_DIGIT;
    }

    for (int32_t ch = 'A'; ch <= 'Z'; ch++)
    {
        CharTable[ch] = CHR_LETTER;
    }

    for (int32_t ch = 'a'; ch <= 'z'; ch++)
    {
        CharTable[ch] = CHR_LETTER;
    }

    CharTable['"'] = CHR_DQUOTE;
    CharTable[0x7f] = CHR_EOF;

    if (fileName != nullptr)
    {
        SourceFile = new MCFile;

        if (SourceFile->Open(fileName) != 0)
        {
            SyntaxError(ABL_ERR_SYNTAX_SOURCE_FILE_OPEN);
            CurToken = TKN_ERROR;
        }

        SourceBuffer[0] = '\0';
        Bufferp = SourceBuffer;
        GetChar();
    }
}

auto QuitScanner() -> void
{
    SourceFile->Close();
    delete SourceFile;
    SourceFile = nullptr;
}

auto SkipBlockComment() -> void
{
    DumbGetCharOn = 1;
    GetChar();
    GetChar();

    while (true)
    {
        if (CurChar != '*')
        {
            if (CurChar != 0x7f)
            {
                GetChar();
                continue;
            }

            SyntaxError(ABL_ERR_SYNTAX_UNEXPECTED_EOF);
            CurToken = TKN_ERROR;
        }

        GetChar();

        if (CurChar == '/')
        {
            CurChar = ' ';
            DumbGetCharOn = 0;
            return;
        }
    }
}

auto SkipBlanks() -> void
{
    while (CurChar == ' ')
    {
        GetChar();
    }
}

auto LanguageDirective() -> void
{
    char directive[32];
    char fileName[128];
    char fullPath[256];

    DumbGetCharOn = 1;
    GetChar();
    int32_t i = 0;

    for (; CurChar != ' ' && CurChar != '\n' && CurChar != '\r' && i < 31; i++)
    {
        directive[i] = CurChar;
        GetChar();
    }

    directive[i] = '\0';

    // Every error path leaves DumbGetCharOn set, as the original does.
    const char* openName = nullptr;

    if (strcmp(directive, "include") == 0)
    {
        // #include "file": the name as written.
        if (!ReadDirectiveFileName(fileName))
        {
            SyntaxError(ABL_ERR_SYNTAX_BAD_LANGUAGE_DIRECTIVE_PARAM);
            CurToken = TKN_ERROR;
            return;
        }

        DumbGetCharOn = 0;
        openName = fileName;
    }
    else if (strcmp(directive, "include_") == 0)
    {
        // #include_ "file": relative to the folder of the module's main file (SourceFiles[0]).
        if (!ReadDirectiveFileName(fileName))
        {
            SyntaxError(ABL_ERR_SYNTAX_BAD_LANGUAGE_DIRECTIVE_PARAM);
            CurToken = TKN_ERROR;
            return;
        }

        DumbGetCharOn = 0;
        auto lastSlash = static_cast<int32_t>(strlen(SourceFiles[0]));

        while (lastSlash >= 0 && SourceFiles[0][lastSlash] != '\\')
        {
            lastSlash--;
        }

        if (lastSlash == -1)
        {
            strcpy(fullPath, fileName);
        }
        else
        {
            strcpy(fullPath, SourceFiles[0]);
            fullPath[lastSlash + 1] = '\0';
            strcat(fullPath, fileName);
        }

        openName = fullPath;
    }
    else
    {
        int* flag = nullptr;
        int value = 0;

        if (strcmp(directive, "assert_on") == 0)
        {
            flag = &AssertEnabled, value = 1;
        }
        else if (strcmp(directive, "assert_off") == 0)
        {
            flag = &AssertEnabled, value = 0;
        }
        else if (strcmp(directive, "print_on") == 0)
        {
            flag = &PrintEnabled, value = 1;
        }
        else if (strcmp(directive, "print_off") == 0)
        {
            flag = &PrintEnabled, value = 0;
        }
        else if (strcmp(directive, "stringfuncs_on") == 0)
        {
            flag = &StringFunctionsEnabled, value = 1;
        }
        else if (strcmp(directive, "stringfuncs_off") == 0)
        {
            flag = &StringFunctionsEnabled, value = 0;
        }

        if (flag == nullptr)
        {
            SyntaxError(ABL_ERR_SYNTAX_UNKNOWN_LANGUAGE_DIRECTIVE);
            CurToken = TKN_ERROR;
            return;
        }

        *flag = value;
        DumbGetCharOn = 0;
        CurChar = ' ';
        return;
    }

    if (OpenSourceFile(const_cast<char*>(openName)) != 0)
    {
        SyntaxError(ABL_ERR_SYNTAX_SOURCE_FILE_OPEN);
        CurToken = TKN_ERROR;
    }
}

auto GetChar() -> void
{
    if (*Bufferp == '\0')
    {
        if (GetSourceLine() == 0)
        {
            // The end of an #include returns to the including file without setting curChar.
            if (NumOpenFiles > 1)
            {
                CloseSourceFile();
                return;
            }

            CurChar = 0x7f;
            return;
        }

        BufferOffset = 0;
        Bufferp = SourceBuffer;
    }

    CurChar = *Bufferp++;

    if (DumbGetCharOn == 0)
    {
        switch (CurChar)
        {
            case '\t':
            {
                BufferOffset += 4 - BufferOffset % 4;
                CurChar = ' ';
                return;
            }
            case '\n':
            case '\r':
            {
                BufferOffset++;
                CurChar = ' ';
                return;
            }
            case '#':
            {
                LanguageDirective();
                return;
            }
            case '/':
            {
                if (*Bufferp == '/')
                {
                    // A line comment: drop the rest of the line.
                    CurChar = ' ';
                    *Bufferp = '\0';
                    return;
                }

                if (*Bufferp == '*')
                {
                    SkipBlockComment();
                    return;
                }
                break;
            }
            default:
                break;
        }
    }

    BufferOffset++;
}

auto DownShiftWord() -> void
{
    if (strlen(WordString) >= MAXLEN_TOKENSTRING || strlen(TokenString) >= MAXLEN_TOKENSTRING)
    {
        Fatal(-1, " Boy did Glenn screw the pooch here!! ");
    }

    char* wp = WordString;
    const char* tp = TokenString;

    do
    {
        char ch = *tp;

        if (ch >= 'A' && ch <= 'Z')
        {
            ch += 'a' - 'A';
        }

        *wp++ = ch;
        tp++;
    } while (*tp != '\0');

    *wp = '\0';
}

auto GetToken() -> void
{
    SkipBlanks();
    Tokenp = TokenString;

    switch (CharCode(CurChar))
    {
        case CHR_LETTER:
            GetWord();
            break;
        case CHR_DIGIT:
            ScanNumber();
            break;
        case CHR_DQUOTE:
            GetString();
            break;
        case CHR_EOF:
            CurToken = TKN_EOF;
            break;
        default:
            GetSpecial();
            break;
    }

    if (BlockFlag != 0)
    {
        CrunchToken();
    }
}

auto GetWord() -> void
{
    while (IsWordChar(CurChar))
    {
        *Tokenp++ = CurChar;
        GetChar();
    }

    *Tokenp = '\0';
    DownShiftWord();

    // "library.name": a qualified identifier, unless the word is this module's end keyword.
    if (CurChar == '.' && strcmp(WordString, TokenStrings[TKN_END_MODULE + (CurLibrary != nullptr ? 1 : 0)]) != 0)
    {
        *Tokenp = '.';
        while (true)
        {
            Tokenp++;
            GetChar();

            if (!IsWordChar(CurChar))
            {
                break;
            }

            *Tokenp = CurChar;
        }

        *Tokenp = '\0';
        DownShiftWord();
    }

    if (IsReservedWord() == 0)
    {
        CurToken = TKN_IDENTIFIER;
    }
}

auto AccumulateValue(float* valuePtr, MCSyntaxErrorType errCode) -> void
{
    float value = *valuePtr;

    if (CharCode(CurChar) != CHR_DIGIT)
    {
        SyntaxError(errCode);
        CurToken = TKN_ERROR;
        return;
    }

    do
    {
        *Tokenp++ = CurChar;
        if (++DigitCount <= 20)
        {
            value = value * 10.0f + static_cast<float>(CurChar - '0');
        }
        else
        {
            CountError = 1;
        }

        GetChar();
    } while (CharCode(CurChar) == CHR_DIGIT);

    *valuePtr = value;
}

auto ScanNumber() -> void
{
    int32_t decimalOffset = 0;
    char exponentSign = '+';
    float numberValue = 0.0f;
    float exponentValue = 0.0f;
    DigitCount = 0;
    CountError = 0;
    CurToken = TKN_NONE;
    CurLiteral.Type = LIT_INTEGER;

    AccumulateValue(&numberValue, ABL_ERR_SYNTAX_INVALID_NUMBER);

    if (CurToken == TKN_ERROR)
    {
        return;
    }

    int32_t wholeCount = DigitCount;

    if (CurChar == '.')
    {
        GetChar();
        *Tokenp++ = '.';
        CurLiteral.Type = LIT_REAL;
        AccumulateValue(&numberValue, ABL_ERR_SYNTAX_INVALID_FRACTION);

        if (CurToken == TKN_ERROR)
        {
            return;
        }

        decimalOffset = wholeCount - DigitCount;
    }

    if (CurChar == 'E' || CurChar == 'e')
    {
        CurLiteral.Type = LIT_REAL;

        // Original behaviour (OB-037): the 'E' is not consumed (no getChar), so the sign test and accumulateValue
        // see the 'E' again and every exponent is ABL_ERR_SYNTAX_INVALID_EXPONENT.
        *Tokenp++ = CurChar;
        if (CurChar == '+' || CurChar == '-')
        {
            exponentSign = CurChar;
            *Tokenp++ = CurChar;
            GetChar();
        }

        AccumulateValue(&exponentValue, ABL_ERR_SYNTAX_INVALID_EXPONENT);

        if (CurToken == TKN_ERROR)
        {
            return;
        }

        if (exponentSign == '-')
        {
            exponentValue = -exponentValue;
        }
    }

    if (CountError != 0)
    {
        SyntaxError(ABL_ERR_SYNTAX_TOO_MANY_DIGITS);
        CurToken = TKN_ERROR;
        return;
    }

    auto exponent = static_cast<int32_t>(static_cast<float>(decimalOffset) + exponentValue);

    if (exponent + wholeCount < -20 || exponent + wholeCount > 20)
    {
        SyntaxError(ABL_ERR_SYNTAX_REAL_OUT_OF_RANGE);
        CurToken = TKN_ERROR;
        return;
    }

    if (exponent != 0)
    {
        numberValue = static_cast<float>(pow(10.0, exponent) * numberValue);
    }

    if (CurLiteral.Type == LIT_INTEGER)
    {
        // __ftol gives 0x80000000 for anything out of range, which the original rejects.
        if (numberValue >= 2147483648.0f)
        {
            SyntaxError(ABL_ERR_SYNTAX_INTEGER_OUT_OF_RANGE);
            CurToken = TKN_ERROR;
            return;
        }

        CurLiteral.Value.Integer = static_cast<int32_t>(numberValue);
    }
    else
    {
        CurLiteral.Value.Real = numberValue;
    }

    CurToken = TKN_NUMBER;
    *Tokenp = '\0';
}

auto GetString() -> void
{
    char* sp = CurLiteral.Value.String;
    // The opening quote is written but then overwritten by the first character (tokenp is not advanced).
    *Tokenp = '"';
    GetChar();

    while (CurChar != 0x7f && CurChar != '"')
    {
        // Port fix: an unterminated string ran past the literal and token buffers; the port stops storing.
        if (sp < CurLiteral.Value.String + MAXLEN_TOKENSTRING - 1)
        {
            *sp++ = CurChar;
            *Tokenp++ = CurChar;
        }

        GetChar();
    }

    *sp = '\0';
    // The closing quote is dropped by replacing it with a blank.
    CurChar = ' ';
    CurToken = TKN_STRING;
    *Tokenp = '\0';
    CurLiteral.Type = LIT_STRING;
}

auto GetSpecial() -> void
{
    char firstChar = CurChar;

    *Tokenp++ = CurChar;
    switch (firstChar)
    {
        case '#':
            CurToken = TKN_POUND;
            break;
        case '(':
            CurToken = TKN_LPAREN;
            break;
        case ')':
            CurToken = TKN_RPAREN;
            break;
        case '*':
            CurToken = TKN_STAR;
            break;
        case '+':
            CurToken = TKN_PLUS;
            break;
        case ',':
            CurToken = TKN_COMMA;
            break;
        case '-':
            CurToken = TKN_MINUS;
            break;
        case '.':
            CurToken = TKN_PERIOD;
            break;
        case '/':
            CurToken = TKN_FSLASH;
            break;
        case ':':
            CurToken = TKN_COLON;
            break;
        case ';':
            CurToken = TKN_SEMICOLON;
            break;
        case '@':
            CurToken = TKN_REF;
            break;
        case '[':
            CurToken = TKN_LBRACKET;
            break;
        case ']':
            CurToken = TKN_RBRACKET;
            break;
        case '<':
        {
            GetChar();

            if (CurChar == '=')
            {
                CurToken = TKN_LE;
                *Tokenp++ = '=';
                GetChar();
            }
            else if (CurChar == '>')
            {
                CurToken = TKN_NE;
                *Tokenp++ = '>';
                GetChar();
            }
            else
            {
                CurToken = TKN_LT;
            }

            *Tokenp = '\0';
            return;
        }
        case '=':
        {
            GetChar();

            if (CurChar == '=')
            {
                CurToken = TKN_EQUALEQUAL;
                *Tokenp++ = '=';
                GetChar();
            }
            else
            {
                CurToken = TKN_EQUAL;
            }

            *Tokenp = '\0';
            return;
        }
        case '>':
        {
            GetChar();

            if (CurChar == '=')
            {
                CurToken = TKN_GE;
                *Tokenp++ = '=';
                GetChar();
            }
            else
            {
                CurToken = TKN_GT;
            }

            *Tokenp = '\0';
            return;
        }
        default:
            CurToken = TKN_ERROR;
            break;
    }

    GetChar();
    *Tokenp = '\0';
}

auto TokenIn(MCTokenCodeType* tokenList) -> int
{
    if (tokenList == nullptr)
    {
        return 0;
    }

    for (; *tokenList != TKN_NONE; tokenList++)
    {
        if (CurToken == *tokenList)
        {
            return 1;
        }
    }

    return 0;
}

auto Synchronize(MCTokenCodeType* tokenList1, MCTokenCodeType* tokenList2, MCTokenCodeType* tokenList3) -> void
{
    auto inAny = [&] { return TokenIn(tokenList1) || TokenIn(tokenList2) || TokenIn(tokenList3); };

    if (inAny())
    {
        return;
    }

    SyntaxError(CurToken == TKN_EOF ? ABL_ERR_SYNTAX_UNEXPECTED_EOF : ABL_ERR_SYNTAX_UNEXPECTED_TOKEN);

    while (!inAny() && CurToken != TKN_EOF)
    {
        GetToken();
    }
}

auto GetSourceLine() -> int
{
    if (SourceFile->Eof())
    {
        return 0;
    }

    SourceFile->ReadLineEx(reinterpret_cast<uint8_t*>(SourceBuffer), MAXLEN_SOURCELINE);
    LineNumber++;

    if (PrintFlag != 0)
    {
        char printBuffer[MAXLEN_SOURCELINE + 16];
        snprintf(printBuffer, sizeof(printBuffer), "%4d %d: %s", LineNumber, Level, SourceBuffer);
        PrintLine(printBuffer);
    }

    return 1;
}

auto OpenSourceFile(char* sourceFileName) -> int32_t
{
    if (sourceFileName == nullptr)
    {
        return -1;
    }

    if (NumOpenFiles == MAX_INCLUDE_DEPTH)
    {
        return -2;
    }

    if (NumSourceFiles == MAX_SOURCE_FILES)
    {
        return -3;
    }

    auto* newFile = new MCFile;

    if (newFile->Open(sourceFileName) != 0)
    {
        // Port fix: the original leaked the File.
        delete newFile;
        return -3;
    }

    SourceFile = newFile;
    strcpy(SourceFiles[NumSourceFiles], sourceFileName);
    FileNumber = NumSourceFiles;
    NumSourceFiles++;

    MCSourceFile& openFile = OpenFiles[NumOpenFiles];
    strcpy(openFile.FileName, sourceFileName);
    openFile.FileNumber = static_cast<uint8_t>(FileNumber);
    openFile.FilePtr = newFile;
    openFile.LineNumber = 0;

    if (NumOpenFiles > 0)
    {
        OpenFiles[NumOpenFiles - 1].LineNumber = LineNumber;
    }

    NumOpenFiles++;

    LineNumber = 0;
    SourceBuffer[0] = '\0';
    Bufferp = SourceBuffer;
    GetChar();
    return 0;
}

auto CloseSourceFile() -> int32_t
{
    if (NumOpenFiles == 0)
    {
        return -1;
    }

    SourceFile->Close();
    delete SourceFile;
    SourceFile = nullptr;
    // Port fix: the original cleared openFiles[NumOpenFiles].filePtr, one past the closing file (out of bounds when
    // six files are open); the port clears the closing file's entry.
    NumOpenFiles--;
    OpenFiles[NumOpenFiles].FilePtr = nullptr;

    if (NumOpenFiles > 0)
    {
        const MCSourceFile& openFile = OpenFiles[NumOpenFiles - 1];
        SourceFile = openFile.FilePtr;
        FileNumber = openFile.FileNumber;
        LineNumber = openFile.LineNumber;
    }

    return 0;
}

auto PrintLine(char* line) -> void
{
    if (++LineCount > MAX_LINES_PER_PAGE)
    {
        PrintPageHeader();
        LineCount = 1;
    }

    char* truncatedAt = nullptr;
    char savedChar = '\0';

    if (strlen(line) > MAX_PRINT_LINE_LENGTH)
    {
        truncatedAt = line + MAX_PRINT_LINE_LENGTH;
        savedChar = *truncatedAt;
        *truncatedAt = '\0';
    }

    printf("%s", line);

    if (truncatedAt != nullptr)
    {
        *truncatedAt = savedChar;
    }
}

auto InitPageHeader(char* fileName) -> void
{
    strncpy(SourceName, fileName, 255);
    time_t timer = time(nullptr);
    strcpy(Date, asctime(localtime(&timer)));
}

auto PrintPageHeader() -> void
{
    PageNumber++;
    printf("Page %d   %s   %s\n\n", PageNumber, SourceName, Date);
}
