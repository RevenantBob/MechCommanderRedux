#include "stdafx.h"
#include "abl/ablscan.h"
#include "abl/abldbug.h"
#include "abl/ablenv.h"
#include "abl/ablexec.h"
#include "abl/ablrtn.h"
#include "abl/ablsymt.h"
#include "lib/aerror.h"
#include "lib/file.h"

ReservedWord keywords2[] = {
    {"if", TKN_IF}, {"or", TKN_OR}, {"do", TKN_DO}, {"to", TKN_TO}, {nullptr, TKN_NONE},
};

ReservedWord keywords3[] = {
    {"and", TKN_AND}, {"for", TKN_FOR}, {"mod", TKN_MOD}, {"not", TKN_NOT}, {"var", TKN_VAR}, {nullptr, TKN_NONE},
};

ReservedWord keywords4[] = {
    {"else", TKN_ELSE}, {"then", TKN_THEN}, {"case", TKN_CASE},
    {"code", TKN_CODE}, {"type", TKN_TYPE}, {nullptr, TKN_NONE},
};

ReservedWord keywords5[] = {
    {"const", TKN_CONST}, {"until", TKN_UNTIL}, {"while", TKN_WHILE}, {"endif", TKN_END_IF}, {nullptr, TKN_NONE},
};

ReservedWord keywords6[] = {
    {"module", TKN_MODULE}, {"repeat", TKN_REPEAT}, {"endfor", TKN_END_FOR},
    {"switch", TKN_SWITCH}, {"static", TKN_STATIC}, {nullptr, TKN_NONE},
};

ReservedWord keywords7[] = {
    {"endcase", TKN_END_CASE},
    {"eternal", TKN_ETERNAL},
    {"library", TKN_LIBRARY},
    {nullptr, TKN_NONE},
};

ReservedWord keywords8[] = {
    {"function", TKN_FUNCTION},
    {"endwhile", TKN_END_WHILE},
    {nullptr, TKN_NONE},
};

ReservedWord keywords9[] = {
    {"endswitch", TKN_END_SWITCH},
    {"endmodule", TKN_END_MODULE},
    {nullptr, TKN_NONE},
};

ReservedWord keywords10[] = {
    {"endlibrary", TKN_END_LIBRARY},
    {nullptr, TKN_NONE},
};

ReservedWord keywords11[] = {
    {"endfunction", TKN_END_FUNCTION},
    {nullptr, TKN_NONE},
};

ReservedWord* keywordTable[12] = {
    nullptr,   nullptr,   keywords2, keywords3, keywords4,  keywords5,
    keywords6, keywords7, keywords8, keywords9, keywords10, keywords11,
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

CharCodeType charTable[256];
char sourceBuffer[MAXLEN_SOURCELINE];
char* bufferp = sourceBuffer;
char* tokenp = tokenString;
char tokenString[MAXLEN_TOKENSTRING];
char wordString[MAXLEN_TOKENSTRING];
char curChar;
TokenCodeType curToken;
Literal curLiteral;
char SourceFiles[MAX_SOURCE_FILES][MAXLEN_FILENAME];
int32_t NumSourceFiles;
SourceFile openFiles[MAX_INCLUDE_DEPTH];
int32_t NumOpenFiles;
File* sourceFile;
int32_t lineNumber;
int32_t FileNumber;
int32_t bufferOffset;
int DumbGetCharOn;
int32_t digitCount;
int countError;
int printFlag = 1;
int32_t lineCount = MAX_LINES_PER_PAGE;
int32_t pageNumber;
char sourceName[256];
char date[26];

namespace
{
    /// <summary>The character class of <paramref name="ch"/>.</summary>
    /// <remarks>
    /// Port fix: the original indexed charTable with the signed char, so bytes 0x80..0xff read the 512 bytes before
    /// it. The port indexes with the byte value (those bytes are CHR_SPECIAL, which scans as TKN_ERROR).
    /// </remarks>
    auto charCode(char ch) -> CharCodeType
    {
        return charTable[static_cast<uint8_t>(ch)];
    }

    /// <summary>Whether <paramref name="ch"/> can continue an identifier.</summary>
    auto isWordChar(char ch) -> bool
    {
        return charCode(ch) == CHR_LETTER || charCode(ch) == CHR_DIGIT || ch == '_';
    }

    /// <summary>Reads a double-quoted #include file name into <paramref name="fileName"/> (up to 127 characters).
    /// </summary>
    /// <returns>false if curChar is not the opening quote.</returns>
    auto readDirectiveFileName(char (&fileName)[128]) -> bool
    {
        getChar();

        if (curChar != '"')
        {
            return false;
        }

        getChar();
        int32_t i = 0;

        for (; curChar != '"' && i < 127; i++)
        {
            fileName[i] = curChar;
            getChar();
        }

        fileName[i] = '\0';
        return true;
    }
}

auto isReservedWord() -> int
{
    auto wordLength = static_cast<int32_t>(strlen(wordString));

    if (wordLength < 2 || wordLength > 11)
    {
        return 0;
    }

    ReservedWord* rwp = keywordTable[wordLength];

    if (rwp == nullptr)
    {
        return 0;
    }

    for (; rwp->string != nullptr; rwp++)
    {
        if (strcmp(wordString, rwp->string) == 0)
        {
            curToken = rwp->tokenCode;
            return 1;
        }
    }

    return 0;
}

auto initScanner(char* fileName) -> void
{
    for (auto& code : charTable)
    {
        code = CHR_SPECIAL;
    }

    for (int32_t ch = '0'; ch <= '9'; ch++)
    {
        charTable[ch] = CHR_DIGIT;
    }

    for (int32_t ch = 'A'; ch <= 'Z'; ch++)
    {
        charTable[ch] = CHR_LETTER;
    }

    for (int32_t ch = 'a'; ch <= 'z'; ch++)
    {
        charTable[ch] = CHR_LETTER;
    }

    charTable['"'] = CHR_DQUOTE;
    charTable[0x7f] = CHR_EOF;

    if (fileName != nullptr)
    {
        sourceFile = new File;

        if (sourceFile->open(fileName) != 0)
        {
            syntaxError(ABL_ERR_SYNTAX_SOURCE_FILE_OPEN);
            curToken = TKN_ERROR;
        }

        sourceBuffer[0] = '\0';
        bufferp = sourceBuffer;
        getChar();
    }
}

auto quitScanner() -> void
{
    sourceFile->close();
    delete sourceFile;
    sourceFile = nullptr;
}

auto skipBlockComment() -> void
{
    DumbGetCharOn = 1;
    getChar();
    getChar();

    while (true)
    {
        if (curChar != '*')
        {
            if (curChar != 0x7f)
            {
                getChar();
                continue;
            }

            syntaxError(ABL_ERR_SYNTAX_UNEXPECTED_EOF);
            curToken = TKN_ERROR;
        }

        getChar();

        if (curChar == '/')
        {
            curChar = ' ';
            DumbGetCharOn = 0;
            return;
        }
    }
}

auto skipBlanks() -> void
{
    while (curChar == ' ')
    {
        getChar();
    }
}

auto languageDirective() -> void
{
    char directive[32];
    char fileName[128];
    char fullPath[256];

    DumbGetCharOn = 1;
    getChar();
    int32_t i = 0;

    for (; curChar != ' ' && curChar != '\n' && curChar != '\r' && i < 31; i++)
    {
        directive[i] = curChar;
        getChar();
    }

    directive[i] = '\0';

    // Every error path leaves DumbGetCharOn set, as the original does.
    const char* openName = nullptr;

    if (strcmp(directive, "include") == 0)
    {
        // #include "file": the name as written.
        if (!readDirectiveFileName(fileName))
        {
            syntaxError(ABL_ERR_SYNTAX_BAD_LANGUAGE_DIRECTIVE_PARAM);
            curToken = TKN_ERROR;
            return;
        }

        DumbGetCharOn = 0;
        openName = fileName;
    }
    else if (strcmp(directive, "include_") == 0)
    {
        // #include_ "file": relative to the folder of the module's main file (SourceFiles[0]).
        if (!readDirectiveFileName(fileName))
        {
            syntaxError(ABL_ERR_SYNTAX_BAD_LANGUAGE_DIRECTIVE_PARAM);
            curToken = TKN_ERROR;
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
            syntaxError(ABL_ERR_SYNTAX_UNKNOWN_LANGUAGE_DIRECTIVE);
            curToken = TKN_ERROR;
            return;
        }

        *flag = value;
        DumbGetCharOn = 0;
        curChar = ' ';
        return;
    }

    if (openSourceFile(const_cast<char*>(openName)) != 0)
    {
        syntaxError(ABL_ERR_SYNTAX_SOURCE_FILE_OPEN);
        curToken = TKN_ERROR;
    }
}

auto getChar() -> void
{
    if (*bufferp == '\0')
    {
        if (getSourceLine() == 0)
        {
            // The end of an #include returns to the including file without setting curChar.
            if (NumOpenFiles > 1)
            {
                closeSourceFile();
                return;
            }

            curChar = 0x7f;
            return;
        }

        bufferOffset = 0;
        bufferp = sourceBuffer;
    }

    curChar = *bufferp++;

    if (DumbGetCharOn == 0)
    {
        switch (curChar)
        {
            case '\t':
            {
                bufferOffset += 4 - bufferOffset % 4;
                curChar = ' ';
                return;
            }
            case '\n':
            case '\r':
            {
                bufferOffset++;
                curChar = ' ';
                return;
            }
            case '#':
            {
                languageDirective();
                return;
            }
            case '/':
            {
                if (*bufferp == '/')
                {
                    // A line comment: drop the rest of the line.
                    curChar = ' ';
                    *bufferp = '\0';
                    return;
                }

                if (*bufferp == '*')
                {
                    skipBlockComment();
                    return;
                }
                break;
            }
            default:
                break;
        }
    }

    bufferOffset++;
}

auto downShiftWord() -> void
{
    if (strlen(wordString) >= MAXLEN_TOKENSTRING || strlen(tokenString) >= MAXLEN_TOKENSTRING)
    {
        Fatal(-1, " Boy did Glenn screw the pooch here!! ");
    }

    char* wp = wordString;
    const char* tp = tokenString;

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

auto getToken() -> void
{
    skipBlanks();
    tokenp = tokenString;

    switch (charCode(curChar))
    {
        case CHR_LETTER:
            getWord();
            break;
        case CHR_DIGIT:
            getNumber();
            break;
        case CHR_DQUOTE:
            getString();
            break;
        case CHR_EOF:
            curToken = TKN_EOF;
            break;
        default:
            getSpecial();
            break;
    }

    if (blockFlag != 0)
    {
        crunchToken();
    }
}

auto getWord() -> void
{
    while (isWordChar(curChar))
    {
        *tokenp++ = curChar;
        getChar();
    }

    *tokenp = '\0';
    downShiftWord();

    // "library.name": a qualified identifier, unless the word is this module's end keyword.
    if (curChar == '.' && strcmp(wordString, TokenStrings[TKN_END_MODULE + (CurLibrary != nullptr ? 1 : 0)]) != 0)
    {
        *tokenp = '.';
        while (true)
        {
            tokenp++;
            getChar();

            if (!isWordChar(curChar))
            {
                break;
            }

            *tokenp = curChar;
        }

        *tokenp = '\0';
        downShiftWord();
    }

    if (isReservedWord() == 0)
    {
        curToken = TKN_IDENTIFIER;
    }
}

auto accumulateValue(float* valuePtr, SyntaxErrorType errCode) -> void
{
    float value = *valuePtr;

    if (charCode(curChar) != CHR_DIGIT)
    {
        syntaxError(errCode);
        curToken = TKN_ERROR;
        return;
    }

    do
    {
        *tokenp++ = curChar;
        if (++digitCount <= 20)
        {
            value = value * 10.0f + static_cast<float>(curChar - '0');
        }
        else
        {
            countError = 1;
        }

        getChar();
    } while (charCode(curChar) == CHR_DIGIT);

    *valuePtr = value;
}

auto getNumber() -> void
{
    int32_t decimalOffset = 0;
    char exponentSign = '+';
    float numberValue = 0.0f;
    float exponentValue = 0.0f;
    digitCount = 0;
    countError = 0;
    curToken = TKN_NONE;
    curLiteral.type = LIT_INTEGER;

    accumulateValue(&numberValue, ABL_ERR_SYNTAX_INVALID_NUMBER);

    if (curToken == TKN_ERROR)
    {
        return;
    }

    int32_t wholeCount = digitCount;

    if (curChar == '.')
    {
        getChar();
        *tokenp++ = '.';
        curLiteral.type = LIT_REAL;
        accumulateValue(&numberValue, ABL_ERR_SYNTAX_INVALID_FRACTION);

        if (curToken == TKN_ERROR)
        {
            return;
        }

        decimalOffset = wholeCount - digitCount;
    }

    if (curChar == 'E' || curChar == 'e')
    {
        curLiteral.type = LIT_REAL;

        // Original behaviour (OB-037): the 'E' is not consumed (no getChar), so the sign test and accumulateValue
        // see the 'E' again and every exponent is ABL_ERR_SYNTAX_INVALID_EXPONENT.
        *tokenp++ = curChar;
        if (curChar == '+' || curChar == '-')
        {
            exponentSign = curChar;
            *tokenp++ = curChar;
            getChar();
        }

        accumulateValue(&exponentValue, ABL_ERR_SYNTAX_INVALID_EXPONENT);

        if (curToken == TKN_ERROR)
        {
            return;
        }

        if (exponentSign == '-')
        {
            exponentValue = -exponentValue;
        }
    }

    if (countError != 0)
    {
        syntaxError(ABL_ERR_SYNTAX_TOO_MANY_DIGITS);
        curToken = TKN_ERROR;
        return;
    }

    auto exponent = static_cast<int32_t>(static_cast<float>(decimalOffset) + exponentValue);

    if (exponent + wholeCount < -20 || exponent + wholeCount > 20)
    {
        syntaxError(ABL_ERR_SYNTAX_REAL_OUT_OF_RANGE);
        curToken = TKN_ERROR;
        return;
    }

    if (exponent != 0)
    {
        numberValue = static_cast<float>(pow(10.0, exponent) * numberValue);
    }

    if (curLiteral.type == LIT_INTEGER)
    {
        // __ftol gives 0x80000000 for anything out of range, which the original rejects.
        if (numberValue >= 2147483648.0f)
        {
            syntaxError(ABL_ERR_SYNTAX_INTEGER_OUT_OF_RANGE);
            curToken = TKN_ERROR;
            return;
        }

        curLiteral.value.integer = static_cast<int32_t>(numberValue);
    }
    else
    {
        curLiteral.value.real = numberValue;
    }

    curToken = TKN_NUMBER;
    *tokenp = '\0';
}

auto getString() -> void
{
    char* sp = curLiteral.value.string;
    // The opening quote is written but then overwritten by the first character (tokenp is not advanced).
    *tokenp = '"';
    getChar();

    while (curChar != 0x7f && curChar != '"')
    {
        // Port fix: an unterminated string ran past the literal and token buffers; the port stops storing.
        if (sp < curLiteral.value.string + MAXLEN_TOKENSTRING - 1)
        {
            *sp++ = curChar;
            *tokenp++ = curChar;
        }

        getChar();
    }

    *sp = '\0';
    // The closing quote is dropped by replacing it with a blank.
    curChar = ' ';
    curToken = TKN_STRING;
    *tokenp = '\0';
    curLiteral.type = LIT_STRING;
}

auto getSpecial() -> void
{
    char firstChar = curChar;

    *tokenp++ = curChar;
    switch (firstChar)
    {
        case '#':
            curToken = TKN_POUND;
            break;
        case '(':
            curToken = TKN_LPAREN;
            break;
        case ')':
            curToken = TKN_RPAREN;
            break;
        case '*':
            curToken = TKN_STAR;
            break;
        case '+':
            curToken = TKN_PLUS;
            break;
        case ',':
            curToken = TKN_COMMA;
            break;
        case '-':
            curToken = TKN_MINUS;
            break;
        case '.':
            curToken = TKN_PERIOD;
            break;
        case '/':
            curToken = TKN_FSLASH;
            break;
        case ':':
            curToken = TKN_COLON;
            break;
        case ';':
            curToken = TKN_SEMICOLON;
            break;
        case '@':
            curToken = TKN_REF;
            break;
        case '[':
            curToken = TKN_LBRACKET;
            break;
        case ']':
            curToken = TKN_RBRACKET;
            break;
        case '<':
        {
            getChar();

            if (curChar == '=')
            {
                curToken = TKN_LE;
                *tokenp++ = '=';
                getChar();
            }
            else if (curChar == '>')
            {
                curToken = TKN_NE;
                *tokenp++ = '>';
                getChar();
            }
            else
            {
                curToken = TKN_LT;
            }

            *tokenp = '\0';
            return;
        }
        case '=':
        {
            getChar();

            if (curChar == '=')
            {
                curToken = TKN_EQUALEQUAL;
                *tokenp++ = '=';
                getChar();
            }
            else
            {
                curToken = TKN_EQUAL;
            }

            *tokenp = '\0';
            return;
        }
        case '>':
        {
            getChar();

            if (curChar == '=')
            {
                curToken = TKN_GE;
                *tokenp++ = '=';
                getChar();
            }
            else
            {
                curToken = TKN_GT;
            }

            *tokenp = '\0';
            return;
        }
        default:
            curToken = TKN_ERROR;
            break;
    }

    getChar();
    *tokenp = '\0';
}

auto tokenIn(TokenCodeType* tokenList) -> int
{
    if (tokenList == nullptr)
    {
        return 0;
    }

    for (; *tokenList != TKN_NONE; tokenList++)
    {
        if (curToken == *tokenList)
        {
            return 1;
        }
    }

    return 0;
}

auto synchronize(TokenCodeType* tokenList1, TokenCodeType* tokenList2, TokenCodeType* tokenList3) -> void
{
    auto inAny = [&] { return tokenIn(tokenList1) || tokenIn(tokenList2) || tokenIn(tokenList3); };

    if (inAny())
    {
        return;
    }

    syntaxError(curToken == TKN_EOF ? ABL_ERR_SYNTAX_UNEXPECTED_EOF : ABL_ERR_SYNTAX_UNEXPECTED_TOKEN);

    while (!inAny() && curToken != TKN_EOF)
    {
        getToken();
    }
}

auto getSourceLine() -> int
{
    if (sourceFile->eof())
    {
        return 0;
    }

    sourceFile->readLineEx(reinterpret_cast<uint8_t*>(sourceBuffer), MAXLEN_SOURCELINE);
    lineNumber++;

    if (printFlag != 0)
    {
        char printBuffer[MAXLEN_SOURCELINE + 16];
        snprintf(printBuffer, sizeof(printBuffer), "%4d %d: %s", lineNumber, level, sourceBuffer);
        printLine(printBuffer);
    }

    return 1;
}

auto openSourceFile(char* sourceFileName) -> int32_t
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

    auto* newFile = new File;

    if (newFile->open(sourceFileName) != 0)
    {
        // Port fix: the original leaked the File.
        delete newFile;
        return -3;
    }

    sourceFile = newFile;
    strcpy(SourceFiles[NumSourceFiles], sourceFileName);
    FileNumber = NumSourceFiles;
    NumSourceFiles++;

    SourceFile& openFile = openFiles[NumOpenFiles];
    strcpy(openFile.fileName, sourceFileName);
    openFile.fileNumber = static_cast<uint8_t>(FileNumber);
    openFile.filePtr = newFile;
    openFile.lineNumber = 0;

    if (NumOpenFiles > 0)
    {
        openFiles[NumOpenFiles - 1].lineNumber = lineNumber;
    }

    NumOpenFiles++;

    lineNumber = 0;
    sourceBuffer[0] = '\0';
    bufferp = sourceBuffer;
    getChar();
    return 0;
}

auto closeSourceFile() -> int32_t
{
    if (NumOpenFiles == 0)
    {
        return -1;
    }

    sourceFile->close();
    delete sourceFile;
    sourceFile = nullptr;
    // Port fix: the original cleared openFiles[NumOpenFiles].filePtr, one past the closing file (out of bounds when
    // six files are open); the port clears the closing file's entry.
    NumOpenFiles--;
    openFiles[NumOpenFiles].filePtr = nullptr;

    if (NumOpenFiles > 0)
    {
        const SourceFile& openFile = openFiles[NumOpenFiles - 1];
        sourceFile = openFile.filePtr;
        FileNumber = openFile.fileNumber;
        lineNumber = openFile.lineNumber;
    }

    return 0;
}

auto printLine(char* line) -> void
{
    if (++lineCount > MAX_LINES_PER_PAGE)
    {
        printPageHeader();
        lineCount = 1;
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

auto initPageHeader(char* fileName) -> void
{
    strncpy(sourceName, fileName, 255);
    time_t timer = time(nullptr);
    strcpy(date, asctime(localtime(&timer)));
}

auto printPageHeader() -> void
{
    pageNumber++;
    printf("Page %d   %s   %s\n\n", pageNumber, sourceName, date);
}
