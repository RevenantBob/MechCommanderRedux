#include "stdafx.h"
#include "lib/inifile.h"

char FitIniHeader[] = "FITini";
char FitIniFooter[] = "FITend";

namespace
{
    /// <summary>
    /// The MSVC 5/6 CRT's atol, which the original called: optional spaces and sign, then digits accumulated in a
    /// 32-bit long that wraps on overflow (no clamping as in the modern CRT).
    /// </summary>
    int32_t OriginalAtol(const char* text)
    {
        while (std::isspace(static_cast<uint8_t>(*text)))
        {
            ++text;
        }

        const char sign = *text;

        if (sign == '-' || sign == '+')
        {
            ++text;
        }

        uint32_t total = 0;

        while (*text >= '0' && *text <= '9')
        {
            total = total * 10 + static_cast<uint32_t>(*text - '0');
            ++text;
        }

        return sign == '-' ? static_cast<int32_t>(0u - total) : static_cast<int32_t>(total);
    }

    /// <summary>
    /// The hex parse the text-to-number methods share: after "0x", the text is cut at the first character that isn't
    /// a hex digit, then summed from the last digit up, each shifted 4 more bits (mod 32, as x86 shifts). A digit that
    /// isn't hex (only the 'x' of an empty "0x" can reach here) gives 0.
    /// </summary>
    /// <returns>Whether the text held "0x"; <paramref name="value"/> gets the sum.</returns>
    bool ParseHex(char* num, uint32_t& value)
    {
        char* hex = std::strstr(num, "0x");

        if (hex == nullptr)
        {
            return false;
        }

        hex += 2;

        const int32_t lastDigit = static_cast<int32_t>(std::strlen(hex)) - 1;

        for (int32_t i = 0; i <= lastDigit; ++i)
        {
            const int c = static_cast<uint8_t>(hex[i]);

            if (!std::isalnum(c) || (std::isalpha(c) && std::toupper(c) > 'F'))
            {
                hex[i] = 0;
                break;
            }
        }

        value = 0;
        uint32_t shift = 0;

        for (int32_t i = static_cast<int32_t>(std::strlen(hex)) - 1; i >= 0; --i)
        {
            const uint8_t c = static_cast<uint8_t>(std::toupper(static_cast<uint8_t>(hex[i])));
            uint32_t digit;

            if (c >= 'A' && c <= 'F')
            {
                digit = static_cast<uint32_t>(c - 'A' + 10);
            }
            else if (c >= '0' && c <= '9')
            {
                digit = static_cast<uint32_t>(c - '0');
            }
            else
            {
                value = 0;
                return true;
            }

            value += digit << (shift & 0x1f);
            shift += 4;
        }

        return true;
    }

    /// <summary>Formats into a stack buffer, then copies to <paramref name="result"/> when it fits.</summary>
    int32_t CopyIfFits(char* result, const char* text, uint32_t bufLen)
    {
        const uint32_t length = static_cast<uint32_t>(std::strlen(text));

        if (bufLen <= length)
        {
            return BUFFER_TOO_SMALL;
        }

        std::memcpy(result, text, length);
        result[length] = 0;
        return 0;
    }
}

MCFitIniFile::MCFitIniFile() = default;

MCFitIniFile::~MCFitIniFile()
{
    Close();
}

int32_t MCFitIniFile::FindNextBlockStart(char* line, uint32_t lineLen)
{
    char buffer[256];
    char* read;

    do
    {
        if (line == nullptr)
        {
            ReadLine(reinterpret_cast<uint8_t*>(buffer), 0xfe);
            read = buffer;
        }
        else
        {
            ReadLine(reinterpret_cast<uint8_t*>(line), static_cast<int32_t>(lineLen));
            read = line;
        }
    } while (!Eof() && *read != '[');

    return Eof() ? NO_MORE_BLOCKS : 0;
}

int32_t MCFitIniFile::CountBlocks()
{
    int32_t count = 0;
    const uint32_t start = _LogicalPosition;

    while (FindNextBlockStart() != NO_MORE_BLOCKS)
    {
        ++count;
    }

    Seek(static_cast<int32_t>(start), SEEK_SET);
    return count;
}

int32_t MCFitIniFile::GetNextWord(char*& line, char* buffer, uint32_t bufLen)
{
    if (*line == 0 || *line == '/')
    {
        return GET_NEXT_LINE;
    }

    // Skip the separators before the word.
    do
    {
        const char c = *line;

        if (c != ' ' && c != '\t' && c != ',')
        {
            break;
        }

        ++line;
    } while (*line != 0);

    char* start = line;

    if (*start == 0 || *start == '/')
    {
        return GET_NEXT_LINE;
    }

    uint32_t count = 0;

    do
    {
        const char c = *line;

        if (c == ' ' || c == '\t' || c == ',')
        {
            break;
        }

        ++line;
        ++count;
    } while (*line != 0);

    if (bufLen < count)
    {
        return BUFFER_TOO_SMALL;
    }

    std::memcpy(buffer, start, count);
    buffer[count] = 0;
    return 0;
}

int32_t MCFitIniFile::AfterOpen()
{
    char line[256];

    if (_FileMode == CREATE && _Parent == nullptr)
    {
        std::snprintf(line, sizeof(line), "%s \r\n", FitIniHeader);
        Write(reinterpret_cast<uint8_t*>(line), static_cast<int32_t>(std::strlen(line)));
        _TotalBlocks = 0;
        return 0;
    }

    char header[12] = {};
    ReadLine(reinterpret_cast<uint8_t*>(header), 0xb);

    if (std::strstr(header, FitIniHeader) == nullptr)
    {
        return NOT_A_FITINIFILE;
    }

    _TotalBlocks = CountBlocks();

    // Original behaviour: a file without blocks fails here (systemHeap's malloc(0) returned null).
    if (_TotalBlocks <= 0)
    {
        return NO_RAM_FOR_INI_BLOCKS;
    }

    _FileBlocks.assign(static_cast<size_t>(_TotalBlocks), MCIniBlockNode{});

    int32_t blocksFound = 0;

    while (FindNextBlockStart(line, 0xfe) != NO_MORE_BLOCKS)
    {
        if (blocksFound == _TotalBlocks)
        {
            return TOO_MANY_BLOCKS;
        }

        // Copy the name between '[' and ']'.
        char* blockId = _FileBlocks[blocksFound].BlockId;
        int32_t i = 1;

        while (line[i] != ']' && line[i] != '\n')
        {
            // Port fix: a line without ']' ran past its end, and a long name past the block table entry.
            if (line[i] == 0)
            {
                return SYNTAX_ERROR;
            }

            if (i - 1 < static_cast<int32_t>(sizeof(MCIniBlockNode::BlockId)) - 1)
            {
                blockId[i - 1] = line[i];
            }

            ++i;
        }

        if (line[i] == '\n')
        {
            return SYNTAX_ERROR;
        }

        blockId[std::min<int32_t>(i - 1, static_cast<int32_t>(sizeof(MCIniBlockNode::BlockId)) - 1)] = 0;
        _FileBlocks[blocksFound].BlockOffset = _LogicalPosition;
        ++blocksFound;
    }

    if (blocksFound != _TotalBlocks)
    {
        return NOT_ENOUGH_BLOCKS;
    }

    return 0;
}

void MCFitIniFile::AtClose()
{
    if (_FileMode == CREATE)
    {
        Seek(0, SEEK_END);
        char line[200];
        std::snprintf(line, sizeof(line), "%s \r\n", FitIniFooter);
        Write(reinterpret_cast<uint8_t*>(line), static_cast<int32_t>(std::strlen(line)));
    }

    _FileBlocks.clear();
}

float MCFitIniFile::TextToFloat(char* num)
{
    return static_cast<float>(std::atof(num));
}

int32_t MCFitIniFile::TextToLong(char* num)
{
    uint32_t value;

    if (!ParseHex(num, value))
    {
        return OriginalAtol(num);
    }

    return static_cast<int32_t>(value);
}

int16_t MCFitIniFile::TextToShort(char* num)
{
    uint32_t value;

    if (!ParseHex(num, value))
    {
        return static_cast<int16_t>(OriginalAtol(num));
    }

    return static_cast<int16_t>(value);
}

char MCFitIniFile::TextToChar(char* num)
{
    uint32_t value;

    if (!ParseHex(num, value))
    {
        return static_cast<char>(OriginalAtol(num));
    }

    return static_cast<char>(value);
}

uint32_t MCFitIniFile::TextToULong(char* num)
{
    uint32_t value;

    if (!ParseHex(num, value))
    {
        return static_cast<uint32_t>(OriginalAtol(num));
    }

    return value;
}

uint16_t MCFitIniFile::TextToUShort(char* num)
{
    uint32_t value;

    if (!ParseHex(num, value))
    {
        return static_cast<uint16_t>(OriginalAtol(num));
    }

    return static_cast<uint16_t>(value);
}

uint8_t MCFitIniFile::TextToUChar(char* num)
{
    uint32_t value;

    if (!ParseHex(num, value))
    {
        return static_cast<uint8_t>(OriginalAtol(num));
    }

    return static_cast<uint8_t>(value);
}

int32_t MCFitIniFile::BooleanToLong(char* num)
{
    int32_t i = 0;

    while (num[i] != 0 && std::isspace(static_cast<uint8_t>(num[i])))
    {
        ++i;
    }

    if (std::toupper(static_cast<uint8_t>(num[i])) == 'T')
    {
        return 1;
    }

    return TextToLong(num);
}

int32_t MCFitIniFile::FloatToText(char* result, float num, uint32_t bufLen)
{
    char text[252];
    std::snprintf(text, sizeof(text), "%f4", static_cast<double>(num));
    return CopyIfFits(result, text, bufLen);
}

int32_t MCFitIniFile::LongToTextDec(char* result, int32_t num, uint32_t bufLen)
{
    char text[252];
    std::snprintf(text, sizeof(text), "%d", num);
    return CopyIfFits(result, text, bufLen);
}

int32_t MCFitIniFile::LongToTextHex(char* result, int32_t num, uint32_t bufLen)
{
    char text[252];
    std::snprintf(text, sizeof(text), "0x%x", static_cast<uint32_t>(num));
    return CopyIfFits(result, text, bufLen);
}

int32_t MCFitIniFile::ShortToTextDec(char* result, int16_t num, uint32_t bufLen)
{
    char text[252];
    std::snprintf(text, sizeof(text), "%d", static_cast<int>(num));
    return CopyIfFits(result, text, bufLen);
}

int32_t MCFitIniFile::ShortToTextHex(char* result, int16_t num, uint32_t bufLen)
{
    char text[252];
    std::snprintf(text, sizeof(text), "0x%x", static_cast<uint32_t>(static_cast<int>(num)));
    return CopyIfFits(result, text, bufLen);
}

int32_t MCFitIniFile::ByteToTextDec(char* result, uint8_t num, uint32_t bufLen)
{
    char text[252];
    std::snprintf(text, sizeof(text), "%d", static_cast<int>(num));
    return CopyIfFits(result, text, bufLen);
}

int32_t MCFitIniFile::ByteToTextHex(char* result, uint8_t num, uint32_t bufLen)
{
    char text[252];
    std::snprintf(text, sizeof(text), "0x%x", static_cast<uint32_t>(num));
    return CopyIfFits(result, text, bufLen);
}

int32_t MCFitIniFile::Open(const char* fName, MCFileMode mode, int32_t numChildren)
{
    const int32_t result = MCFile::Open(fName, mode, numChildren);

    if (result != 0)
    {
        return result;
    }

    Seek(0, SEEK_SET);
    return AfterOpen();
}

int32_t MCFitIniFile::Open(MCFile* parent, uint32_t fileSize, int32_t)
{
    // The child's bytes are always read into memory at once.
    const int32_t result = MCFile::Open(parent, fileSize, -1);

    if (result != 0)
    {
        return result;
    }

    return AfterOpen();
}

int32_t MCFitIniFile::Create(const char* fName)
{
    // File::create opens through the virtual open, which writes the header.
    return MCFile::Create(fName);
}

void MCFitIniFile::Close()
{
    AtClose();
    MCFile::Close();
}

int32_t MCFitIniFile::SeekBlock(const char* blockId)
{
    int32_t blockNum = 0;

    while (blockNum < _TotalBlocks && std::strcmp(_FileBlocks[blockNum].BlockId, blockId) != 0)
    {
        ++blockNum;
    }

    if (blockNum == _TotalBlocks)
    {
        return BLOCK_NOT_FOUND;
    }

    Seek(static_cast<int32_t>(_FileBlocks[blockNum].BlockOffset), SEEK_SET);
    _CurrentBlockId = _FileBlocks[blockNum].BlockId;
    _CurrentBlockOffset = _FileBlocks[blockNum].BlockOffset;

    if (blockNum + 1 != _TotalBlocks)
    {
        _CurrentBlockSize = _FileBlocks[blockNum + 1].BlockOffset - _CurrentBlockOffset;
    }
    else
    {
        _CurrentBlockSize = GetLength() - _CurrentBlockOffset;
    }

    return 0;
}

char* MCFitIniFile::FindIdLine(const char* prefix, const char* varName, char* line, int32_t& error)
{
    Seek(static_cast<int32_t>(_CurrentBlockOffset), SEEK_SET);
    const uint32_t blockEnd = _CurrentBlockSize + _CurrentBlockOffset;

    char search[256];
    std::snprintf(search, sizeof(search), "%s %s", prefix, varName);
    const size_t searchLength = std::strlen(search);

    do
    {
        ReadLine(reinterpret_cast<uint8_t*>(line), 0xfe);

        if (MCPort::StrNICmp(line, search, searchLength) == 0)
        {
            const char* after = line + searchLength;

            while (std::isspace(static_cast<uint8_t>(*after)))
            {
                ++after;
            }

            if (*after == '=')
            {
                break;
            }
        }
    } while (_LogicalPosition < blockEnd);

    // Original behaviour: a match on the block's last line (the position then being at its end) is reported missing.
    if (blockEnd <= _LogicalPosition)
    {
        error = VARIABLE_NOT_FOUND;
        return nullptr;
    }

    char* equals = std::strstr(line, "=");

    if (equals == nullptr)
    {
        error = SYNTAX_ERROR;
        return nullptr;
    }

    error = 0;
    return equals + 1;
}

int32_t MCFitIniFile::ReadIdFloat(const char* varName, float& value)
{
    char line[256];
    int32_t error;
    char* text = FindIdLine("f", varName, line, error);

    if (text == nullptr)
    {
        if (error == VARIABLE_NOT_FOUND)
        {
            value = 0.0f;
        }

        return error;
    }

    value = TextToFloat(text);

    if (value == 0.0f)
    {
        value = MathToFloat(text);
    }

    return 0;
}

int32_t MCFitIniFile::ReadIdLong(const char* varName, int32_t& value)
{
    char line[256];
    int32_t error;
    char* text = FindIdLine("l", varName, line, error);

    if (text == nullptr)
    {
        if (error == VARIABLE_NOT_FOUND)
        {
            value = 0;
        }

        return error;
    }

    value = TextToLong(text);

    if (value == 0)
    {
        value = MathToLong(text);
    }

    return 0;
}

int32_t MCFitIniFile::ReadIdBoolean(const char* varName, int& value)
{
    char line[256];
    int32_t error;
    char* text = FindIdLine("b", varName, line, error);

    if (text == nullptr)
    {
        if (error == VARIABLE_NOT_FOUND)
        {
            value = 0;
        }

        return error;
    }

    value = BooleanToLong(text);
    return 0;
}

int32_t MCFitIniFile::ReadIdShort(const char* varName, int16_t& value)
{
    char line[256];
    int32_t error;
    char* text = FindIdLine("s", varName, line, error);

    if (text == nullptr)
    {
        if (error == VARIABLE_NOT_FOUND)
        {
            value = 0;
        }

        return error;
    }

    value = TextToShort(text);

    if (value == 0)
    {
        value = MathToShort(text);
    }

    return 0;
}

int32_t MCFitIniFile::ReadIdChar(const char* varName, char& value)
{
    char line[256];
    int32_t error;
    char* text = FindIdLine("c", varName, line, error);

    if (text == nullptr)
    {
        if (error == VARIABLE_NOT_FOUND)
        {
            value = 0;
        }

        return error;
    }

    value = TextToChar(text);

    if (value == 0)
    {
        value = MathToChar(text);
    }

    return 0;
}

int32_t MCFitIniFile::ReadIdULong(const char* varName, uint32_t& value)
{
    char line[256];
    int32_t error;
    char* text = FindIdLine("ul", varName, line, error);

    if (text == nullptr)
    {
        if (error == VARIABLE_NOT_FOUND)
        {
            value = 0;
        }

        return error;
    }

    value = TextToULong(text);

    if (value == 0)
    {
        value = MathToULong(text);
    }

    return 0;
}

int32_t MCFitIniFile::ReadIdUShort(const char* varName, uint16_t& value)
{
    char line[256];
    int32_t error;
    char* text = FindIdLine("us", varName, line, error);

    if (text == nullptr)
    {
        if (error == VARIABLE_NOT_FOUND)
        {
            value = 0;
        }

        return error;
    }

    value = TextToUShort(text);

    if (value == 0)
    {
        value = MathToUShort(text);
    }

    return 0;
}

int32_t MCFitIniFile::ReadIdUChar(const char* varName, uint8_t& value)
{
    char line[256];
    int32_t error;
    char* text = FindIdLine("uc", varName, line, error);

    if (text == nullptr)
    {
        if (error == VARIABLE_NOT_FOUND)
        {
            value = 0;
        }

        return error;
    }

    value = TextToUChar(text);

    if (value == 0)
    {
        value = MathToUChar(text);
    }

    return 0;
}

int32_t MCFitIniFile::CopyString(char* dest, char* line, uint32_t bufLen)
{
    // Port fix: the original scanned for the quotes without stopping at the end of the line.
    while (*line != '"')
    {
        if (*line == 0)
        {
            return SYNTAX_ERROR;
        }

        ++line;
    }

    ++line;
    uint32_t count = 0;

    while (line[count] != '"' && line[count] != 0 && count < bufLen)
    {
        dest[count] = line[count];
        ++count;
    }

    if (count == bufLen)
    {
        return BUFFER_TOO_SMALL;
    }

    dest[count] = 0;
    return 0;
}

int32_t MCFitIniFile::ReadIdString(const char* varName, char* result, uint32_t maxLength)
{
    char line[256];
    int32_t error;
    char* text = FindIdLine("st", varName, line, error);

    if (text == nullptr)
    {
        return error;
    }

    return CopyString(result, text, maxLength);
}

int32_t MCFitIniFile::GetIdStringLength(const char* varName)
{
    char line[256];
    int32_t error;
    char* text = FindIdLine("st", varName, line, error);

    // The quotes are looked for from the start of the line.
    if (text == nullptr)
    {
        return error;
    }

    const char* open = std::strchr(line, '"');

    if (open != nullptr)
    {
        const char* close = open + 1;

        while (*close != '"' && *close != 0)
        {
            ++close;
        }

        if (*close != 0)
        {
            return static_cast<int32_t>(close - (open + 1)) + 1;
        }
    }

    return SYNTAX_ERROR;
}

int32_t MCFitIniFile::FindArrayLine(const char* typePrefix, const char* varName, char* line, uint32_t& count,
                                    char*& values)
{
    Seek(static_cast<int32_t>(_CurrentBlockOffset), SEEK_SET);
    const uint32_t blockEnd = _CurrentBlockSize + _CurrentBlockOffset;

    char nameText[256];
    std::snprintf(nameText, sizeof(nameText), "] %s", varName);
    char* typeAt = nullptr;
    char* nameAt = nullptr;

    do
    {
        ReadLine(reinterpret_cast<uint8_t*>(line), 0xfe);
        typeAt = std::strstr(line, typePrefix);
        nameAt = std::strstr(line, nameText);

        if (typeAt != nullptr && nameAt != nullptr)
        {
            break;
        }
    } while (_LogicalPosition < blockEnd);

    if (blockEnd <= _LogicalPosition)
    {
        return VARIABLE_NOT_FOUND;
    }

    // The element count between "l[" and "] name".
    const char* countText = typeAt + std::strlen(typePrefix);
    const ptrdiff_t countLength = nameAt - countText;

    if (countLength > 9)
    {
        return TOO_MANY_ELEMENTS;
    }

    // Port fix: "] name" before the type prefix gave a negative length the original passed to strncpy.
    if (countLength < 0)
    {
        return SYNTAX_ERROR;
    }

    char number[12];
    std::memcpy(number, countText, static_cast<size_t>(countLength));
    number[countLength] = 0;
    count = TextToULong(number);
    values = nullptr;
    return 0;
}

template <typename Store> int32_t MCFitIniFile::ReadArrayElements(char* line, char* values, uint32_t count, Store store)
{
    const uint32_t blockEnd = _CurrentBlockSize + _CurrentBlockOffset;
    char word[12];
    uint32_t i = 0;

    while (_LogicalPosition < blockEnd && i < count)
    {
        const int32_t result = GetNextWord(values, word, 9);

        if (result == GET_NEXT_LINE)
        {
            ReadLine(reinterpret_cast<uint8_t*>(line), 0xfe);
            values = line;
        }
        else
        {
            if (result != 0)
            {
                return result;
            }

            store(i, word);
            ++i;
        }
    }

    // Original behaviour: elements on the block's last line aren't read (the loop tests the position first).
    if (blockEnd <= _LogicalPosition && i < count)
    {
        return NOT_ENOUGH_ELEMENTS_FOR_ARRAY;
    }

    return 0;
}

int32_t MCFitIniFile::ReadIdFloatArray(const char* varName, float* result, uint32_t numElements)
{
    char line[256];
    uint32_t count;
    char* values;
    const int32_t error = FindArrayLine("f[", varName, line, count, values);

    if (error != 0)
    {
        return error;
    }

    if (numElements < count)
    {
        return USER_ARRAY_TOO_SMALL;
    }

    values = std::strstr(line, "=");

    if (values == nullptr)
    {
        return SYNTAX_ERROR;
    }

    return ReadArrayElements(line, values + 1, count,
                             [&](uint32_t i, char* word)
                             {
                                 result[i] = TextToFloat(word);

                                 if (result[i] == 0.0f)
                                 {
                                     result[i] = MathToFloat(word);
                                 }
                             });
}

int32_t MCFitIniFile::ReadIdLongArray(const char* varName, int32_t* result, uint32_t numElements)
{
    char line[256];
    uint32_t count;
    char* values;
    const int32_t error = FindArrayLine("l[", varName, line, count, values);

    if (error != 0)
    {
        return error;
    }

    if (numElements < count)
    {
        return USER_ARRAY_TOO_SMALL;
    }

    values = std::strstr(line, "=");

    if (values == nullptr)
    {
        return SYNTAX_ERROR;
    }

    return ReadArrayElements(line, values + 1, count,
                             [&](uint32_t i, char* word)
                             {
                                 result[i] = TextToLong(word);

                                 if (result[i] == 0)
                                 {
                                     result[i] = MathToLong(word);
                                 }
                             });
}

int32_t MCFitIniFile::ReadIdULongArray(const char* varName, uint32_t* result, uint32_t numElements)
{
    char line[256];
    uint32_t count;
    char* values;
    const int32_t error = FindArrayLine("ul[", varName, line, count, values);

    if (error != 0)
    {
        return error;
    }

    if (numElements < count)
    {
        return USER_ARRAY_TOO_SMALL;
    }

    values = std::strstr(line, "=");

    if (values == nullptr)
    {
        return SYNTAX_ERROR;
    }

    return ReadArrayElements(line, values + 1, count,
                             [&](uint32_t i, char* word)
                             {
                                 result[i] = TextToULong(word);

                                 if (result[i] == 0)
                                 {
                                     result[i] = MathToULong(word);
                                 }
                             });
}

int32_t MCFitIniFile::ReadIdShortArray(const char* varName, int16_t* result, uint32_t numElements)
{
    char line[256];
    uint32_t count;
    char* values;
    const int32_t error = FindArrayLine("s[", varName, line, count, values);

    if (error != 0)
    {
        return error;
    }

    if (numElements < count)
    {
        return USER_ARRAY_TOO_SMALL;
    }

    values = std::strstr(line, "=");

    if (values == nullptr)
    {
        return SYNTAX_ERROR;
    }

    return ReadArrayElements(line, values + 1, count,
                             [&](uint32_t i, char* word)
                             {
                                 result[i] = TextToShort(word);

                                 if (result[i] == 0)
                                 {
                                     result[i] = MathToShort(word);
                                 }
                             });
}

int32_t MCFitIniFile::ReadIdUShortArray(const char* varName, uint16_t* result, uint32_t numElements)
{
    char line[256];
    uint32_t count;
    char* values;
    const int32_t error = FindArrayLine("us[", varName, line, count, values);

    if (error != 0)
    {
        return error;
    }

    if (numElements < count)
    {
        return USER_ARRAY_TOO_SMALL;
    }

    values = std::strstr(line, "=");

    if (values == nullptr)
    {
        return SYNTAX_ERROR;
    }

    return ReadArrayElements(line, values + 1, count,
                             [&](uint32_t i, char* word)
                             {
                                 result[i] = TextToUShort(word);

                                 if (result[i] == 0)
                                 {
                                     result[i] = MathToUShort(word);
                                 }
                             });
}

int32_t MCFitIniFile::ReadIdCharArray(const char* varName, char* result, uint32_t numElements)
{
    char line[256];
    uint32_t count;
    char* values;
    const int32_t error = FindArrayLine("c[", varName, line, count, values);

    if (error != 0)
    {
        return error;
    }

    if (numElements < count)
    {
        return USER_ARRAY_TOO_SMALL;
    }

    values = std::strstr(line, "=");

    if (values == nullptr)
    {
        return SYNTAX_ERROR;
    }

    return ReadArrayElements(line, values + 1, count,
                             [&](uint32_t i, char* word)
                             {
                                 result[i] = TextToChar(word);

                                 if (result[i] == 0)
                                 {
                                     result[i] = MathToChar(word);
                                 }
                             });
}

int32_t MCFitIniFile::ReadIdUCharArray(const char* varName, uint8_t* result, uint32_t numElements)
{
    char line[256];
    uint32_t count;
    char* values;
    const int32_t error = FindArrayLine("uc[", varName, line, count, values);

    if (error != 0)
    {
        return error;
    }

    if (numElements < count)
    {
        return USER_ARRAY_TOO_SMALL;
    }

    values = std::strstr(line, "=");

    if (values == nullptr)
    {
        return SYNTAX_ERROR;
    }

    return ReadArrayElements(line, values + 1, count,
                             [&](uint32_t i, char* word)
                             {
                                 result[i] = TextToUChar(word);

                                 if (result[i] == 0)
                                 {
                                     result[i] = MathToUChar(word);
                                 }
                             });
}

uint32_t MCFitIniFile::GetIdFloatArrayElements(const char* varName)
{
    char line[256];
    uint32_t count;
    char* values;
    const int32_t error = FindArrayLine("f[", varName, line, count, values);
    return error != 0 ? static_cast<uint32_t>(error) : count;
}

uint32_t MCFitIniFile::GetIdLongArrayElements(const char* varName)
{
    char line[256];
    uint32_t count;
    char* values;
    const int32_t error = FindArrayLine("l[", varName, line, count, values);
    return error != 0 ? static_cast<uint32_t>(error) : count;
}

uint32_t MCFitIniFile::GetIdULongArrayElements(const char* varName)
{
    char line[256];
    uint32_t count;
    char* values;
    const int32_t error = FindArrayLine("ul[", varName, line, count, values);
    return error != 0 ? static_cast<uint32_t>(error) : count;
}

uint32_t MCFitIniFile::GetIdShortArrayElements(const char* varName)
{
    char line[256];
    uint32_t count;
    char* values;
    const int32_t error = FindArrayLine("s[", varName, line, count, values);
    return error != 0 ? static_cast<uint32_t>(error) : count;
}

uint32_t MCFitIniFile::GetIdUShortArrayElements(const char* varName)
{
    char line[256];
    uint32_t count;
    char* values;
    const int32_t error = FindArrayLine("us[", varName, line, count, values);
    return error != 0 ? static_cast<uint32_t>(error) : count;
}

uint32_t MCFitIniFile::GetIdCharArrayElements(const char* varName)
{
    char line[256];
    uint32_t count;
    char* values;
    const int32_t error = FindArrayLine("c[", varName, line, count, values);
    return error != 0 ? static_cast<uint32_t>(error) : count;
}

uint32_t MCFitIniFile::GetIdUCharArrayElements(const char* varName)
{
    char line[256];
    uint32_t count;
    char* values;
    const int32_t error = FindArrayLine("uc[", varName, line, count, values);
    return error != 0 ? static_cast<uint32_t>(error) : count;
}

int32_t MCFitIniFile::WriteBlock(const char* blockId)
{
    char line[256];
    std::snprintf(line, sizeof(line), "\r\n[%s]\r\n", blockId);
    return Write(reinterpret_cast<uint8_t*>(line), static_cast<int32_t>(std::strlen(line)));
}

int32_t MCFitIniFile::WriteIdFloat(const char* varName, float value)
{
    char line[256];
    std::snprintf(line, sizeof(line), "f %s=%f\r\n", varName, static_cast<double>(value));
    return Write(reinterpret_cast<uint8_t*>(line), static_cast<int32_t>(std::strlen(line)));
}

int32_t MCFitIniFile::WriteIdBoolean(const char* varName, int value)
{
    char line[256];
    std::snprintf(line, sizeof(line), "b %s=%s\r\n", varName, value ? "TRUE" : "FALSE");
    return Write(reinterpret_cast<uint8_t*>(line), static_cast<int32_t>(std::strlen(line)));
}

int32_t MCFitIniFile::WriteIdLong(const char* varName, int32_t value)
{
    char line[256];
    std::snprintf(line, sizeof(line), "l %s=%d\r\n", varName, value);
    return Write(reinterpret_cast<uint8_t*>(line), static_cast<int32_t>(std::strlen(line)));
}

int32_t MCFitIniFile::WriteIdShort(const char* varName, int16_t value)
{
    char line[256];
    std::snprintf(line, sizeof(line), "s %s=%d\r\n", varName, static_cast<int>(value));
    return Write(reinterpret_cast<uint8_t*>(line), static_cast<int32_t>(std::strlen(line)));
}

int32_t MCFitIniFile::WriteIdChar(const char* varName, char value)
{
    char line[256];
    std::snprintf(line, sizeof(line), "c %s=%d\r\n", varName, static_cast<int>(value));
    return Write(reinterpret_cast<uint8_t*>(line), static_cast<int32_t>(std::strlen(line)));
}

int32_t MCFitIniFile::WriteIdULong(const char* varName, uint32_t value)
{
    char line[256];
    std::snprintf(line, sizeof(line), "ul %s=%d\r\n", varName, static_cast<int32_t>(value));
    return Write(reinterpret_cast<uint8_t*>(line), static_cast<int32_t>(std::strlen(line)));
}

int32_t MCFitIniFile::WriteIdUShort(const char* varName, uint16_t value)
{
    char line[256];
    std::snprintf(line, sizeof(line), "us %s=%d\r\n", varName, static_cast<int>(value));
    return Write(reinterpret_cast<uint8_t*>(line), static_cast<int32_t>(std::strlen(line)));
}

int32_t MCFitIniFile::WriteIdUChar(const char* varName, uint8_t value)
{
    char line[256];
    std::snprintf(line, sizeof(line), "uc %s=%d\r\n", varName, static_cast<int>(value));
    return Write(reinterpret_cast<uint8_t*>(line), static_cast<int32_t>(std::strlen(line)));
}

int32_t MCFitIniFile::WriteIdString(const char* varName, const char* text)
{
    char line[256];
    std::snprintf(line, sizeof(line), "st %s=\"%s\"\r\n", varName, text);
    return Write(reinterpret_cast<uint8_t*>(line), static_cast<int32_t>(std::strlen(line)));
}

int32_t MCFitIniFile::WriteIdUShortArray(const char* varName, const uint16_t* array, uint32_t numElements)
{
    char line[256];
    std::snprintf(line, sizeof(line), "us[%d] %s=%d,", static_cast<int32_t>(numElements), varName,
                  static_cast<int>(array[0]));
    int32_t written = Write(reinterpret_cast<uint8_t*>(line), static_cast<int32_t>(std::strlen(line)));

    for (int32_t i = 1; i < static_cast<int32_t>(numElements); ++i)
    {
        std::snprintf(line, sizeof(line), "%d,", static_cast<int>(array[i]));
        written += Write(reinterpret_cast<uint8_t*>(line), static_cast<int32_t>(std::strlen(line)));
    }

    std::snprintf(line, sizeof(line), "\r\n");
    return written + Write(reinterpret_cast<uint8_t*>(line), static_cast<int32_t>(std::strlen(line)));
}

int32_t MCFitIniFile::WriteIdLongArray(const char* varName, const int32_t* array, uint32_t numElements)
{
    char line[256];
    std::snprintf(line, sizeof(line), "l[%d] %s=%d,", static_cast<int32_t>(numElements), varName, array[0]);
    int32_t written = Write(reinterpret_cast<uint8_t*>(line), static_cast<int32_t>(std::strlen(line)));

    for (int32_t i = 1; i < static_cast<int32_t>(numElements); ++i)
    {
        std::snprintf(line, sizeof(line), "%d,", array[i]);
        written += Write(reinterpret_cast<uint8_t*>(line), static_cast<int32_t>(std::strlen(line)));
    }

    std::snprintf(line, sizeof(line), "\r\n");
    return written + Write(reinterpret_cast<uint8_t*>(line), static_cast<int32_t>(std::strlen(line)));
}

int32_t MCFitIniFile::WriteIdFloatArray(const char* varName, const float* array, uint32_t numElements)
{
    char line[256];
    std::snprintf(line, sizeof(line), "f[%d] %s=%.2f,", static_cast<int32_t>(numElements), varName,
                  static_cast<double>(array[0]));
    int32_t written = Write(reinterpret_cast<uint8_t*>(line), static_cast<int32_t>(std::strlen(line)));

    for (int32_t i = 1; i < static_cast<int32_t>(numElements); ++i)
    {
        std::snprintf(line, sizeof(line), " %.2f,", static_cast<double>(array[i]));
        written += Write(reinterpret_cast<uint8_t*>(line), static_cast<int32_t>(std::strlen(line)));
    }

    std::snprintf(line, sizeof(line), "\r\n");
    return written + Write(reinterpret_cast<uint8_t*>(line), static_cast<int32_t>(std::strlen(line)));
}

int32_t MCFitIniFile::WriteIdUCharArray(const char* varName, const uint8_t* array, uint32_t numElements)
{
    char line[256];
    std::snprintf(line, sizeof(line), "uc[%d] %s=%d,", static_cast<int32_t>(numElements), varName,
                  static_cast<int>(array[0]));
    int32_t written = Write(reinterpret_cast<uint8_t*>(line), static_cast<int32_t>(std::strlen(line)));

    for (int32_t i = 1; i < static_cast<int32_t>(numElements); ++i)
    {
        std::snprintf(line, sizeof(line), " %d,", static_cast<int>(array[i]));
        written += Write(reinterpret_cast<uint8_t*>(line), static_cast<int32_t>(std::strlen(line)));
    }

    std::snprintf(line, sizeof(line), "\r\n");
    return written + Write(reinterpret_cast<uint8_t*>(line), static_cast<int32_t>(std::strlen(line)));
}
