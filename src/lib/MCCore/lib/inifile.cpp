#include "stdafx.h"
#include "lib/inifile.h"
#include "lib/heap.h"

char fitIniHeader[] = "FITini";
char fitIniFooter[] = "FITend";

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

FitIniFile::FitIniFile() = default;

FitIniFile::~FitIniFile()
{
    close();
}

int32_t FitIniFile::findNextBlockStart(char* line, uint32_t lineLen)
{
    char buffer[256];
    char* read;

    do
    {
        if (line == nullptr)
        {
            readLine(reinterpret_cast<uint8_t*>(buffer), 0xfe);
            read = buffer;
        }
        else
        {
            readLine(reinterpret_cast<uint8_t*>(line), static_cast<int32_t>(lineLen));
            read = line;
        }
    } while (!eof() && *read != '[');

    return eof() ? NO_MORE_BLOCKS : 0;
}

int32_t FitIniFile::countBlocks()
{
    int32_t count = 0;
    const uint32_t start = logicalPosition;

    while (findNextBlockStart() != NO_MORE_BLOCKS)
    {
        ++count;
    }

    seek(static_cast<int32_t>(start), SEEK_SET);
    return count;
}

int32_t FitIniFile::getNextWord(char*& line, char* buffer, uint32_t bufLen)
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

int32_t FitIniFile::afterOpen()
{
    char line[256];

    if (fileMode == CREATE && parent == nullptr)
    {
        std::snprintf(line, sizeof(line), "%s \r\n", fitIniHeader);
        write(reinterpret_cast<uint8_t*>(line), static_cast<int32_t>(std::strlen(line)));
        totalBlocks = 0;
        return 0;
    }

    char header[12] = {};
    readLine(reinterpret_cast<uint8_t*>(header), 0xb);

    if (std::strstr(header, fitIniHeader) == nullptr)
    {
        return NOT_A_FITINIFILE;
    }

    totalBlocks = countBlocks();
    const uint32_t tableSize = static_cast<uint32_t>(totalBlocks) * sizeof(IniBlockNode);

    if (systemHeap != nullptr)
    {
        fileBlocks = static_cast<IniBlockNode*>(systemHeap->malloc(tableSize));
    }
    else
    {
        fileBlocks = static_cast<IniBlockNode*>(std::malloc(tableSize));
    }

    // Original behaviour: with systemHeap up, a file without blocks fails here (its malloc(0) returns null).
    if (fileBlocks == nullptr)
    {
        return NO_RAM_FOR_INI_BLOCKS;
    }

    std::memset(fileBlocks, 0, tableSize);

    int32_t blocksFound = 0;

    while (findNextBlockStart(line, 0xfe) != NO_MORE_BLOCKS)
    {
        if (blocksFound == totalBlocks)
        {
            return TOO_MANY_BLOCKS;
        }

        // Copy the name between '[' and ']'.
        char* blockId = fileBlocks[blocksFound].blockId;
        int32_t i = 1;

        while (line[i] != ']' && line[i] != '\n')
        {
            // Port fix: a line without ']' ran past its end, and a long name past the block table entry.
            if (line[i] == 0)
            {
                return SYNTAX_ERROR;
            }

            if (i - 1 < static_cast<int32_t>(sizeof(IniBlockNode::blockId)) - 1)
            {
                blockId[i - 1] = line[i];
            }

            ++i;
        }

        if (line[i] == '\n')
        {
            return SYNTAX_ERROR;
        }

        blockId[std::min<int32_t>(i - 1, static_cast<int32_t>(sizeof(IniBlockNode::blockId)) - 1)] = 0;
        fileBlocks[blocksFound].blockOffset = logicalPosition;
        ++blocksFound;
    }

    if (blocksFound != totalBlocks)
    {
        return NOT_ENOUGH_BLOCKS;
    }

    return 0;
}

void FitIniFile::atClose()
{
    if (fileMode == CREATE)
    {
        seek(0, SEEK_END);
        char line[200];
        std::snprintf(line, sizeof(line), "%s \r\n", fitIniFooter);
        write(reinterpret_cast<uint8_t*>(line), static_cast<int32_t>(std::strlen(line)));
    }

    if (systemHeap != nullptr)
    {
        // Port fix: a table allocated before systemHeap existed came from malloc.
        if (fileBlocks != nullptr && !systemHeap->owns(fileBlocks))
        {
            std::free(fileBlocks);
        }
        else
        {
            systemHeap->free(fileBlocks);
        }
    }
    else
    {
        std::free(fileBlocks);
    }

    fileBlocks = nullptr;
}

float FitIniFile::textToFloat(char* num)
{
    return static_cast<float>(std::atof(num));
}

int32_t FitIniFile::textToLong(char* num)
{
    uint32_t value;

    if (!ParseHex(num, value))
    {
        return OriginalAtol(num);
    }

    return static_cast<int32_t>(value);
}

int16_t FitIniFile::textToShort(char* num)
{
    uint32_t value;

    if (!ParseHex(num, value))
    {
        return static_cast<int16_t>(OriginalAtol(num));
    }

    return static_cast<int16_t>(value);
}

char FitIniFile::textToChar(char* num)
{
    uint32_t value;

    if (!ParseHex(num, value))
    {
        return static_cast<char>(OriginalAtol(num));
    }

    return static_cast<char>(value);
}

uint32_t FitIniFile::textToULong(char* num)
{
    uint32_t value;

    if (!ParseHex(num, value))
    {
        return static_cast<uint32_t>(OriginalAtol(num));
    }

    return value;
}

uint16_t FitIniFile::textToUShort(char* num)
{
    uint32_t value;

    if (!ParseHex(num, value))
    {
        return static_cast<uint16_t>(OriginalAtol(num));
    }

    return static_cast<uint16_t>(value);
}

uint8_t FitIniFile::textToUChar(char* num)
{
    uint32_t value;

    if (!ParseHex(num, value))
    {
        return static_cast<uint8_t>(OriginalAtol(num));
    }

    return static_cast<uint8_t>(value);
}

int32_t FitIniFile::booleanToLong(char* num)
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

    return textToLong(num);
}

int32_t FitIniFile::floatToText(char* result, float num, uint32_t bufLen)
{
    char text[252];
    std::snprintf(text, sizeof(text), "%f4", static_cast<double>(num));
    return CopyIfFits(result, text, bufLen);
}

int32_t FitIniFile::longToTextDec(char* result, int32_t num, uint32_t bufLen)
{
    char text[252];
    std::snprintf(text, sizeof(text), "%d", num);
    return CopyIfFits(result, text, bufLen);
}

int32_t FitIniFile::longToTextHex(char* result, int32_t num, uint32_t bufLen)
{
    char text[252];
    std::snprintf(text, sizeof(text), "0x%x", static_cast<uint32_t>(num));
    return CopyIfFits(result, text, bufLen);
}

int32_t FitIniFile::shortToTextDec(char* result, int16_t num, uint32_t bufLen)
{
    char text[252];
    std::snprintf(text, sizeof(text), "%d", static_cast<int>(num));
    return CopyIfFits(result, text, bufLen);
}

int32_t FitIniFile::shortToTextHex(char* result, int16_t num, uint32_t bufLen)
{
    char text[252];
    std::snprintf(text, sizeof(text), "0x%x", static_cast<uint32_t>(static_cast<int>(num)));
    return CopyIfFits(result, text, bufLen);
}

int32_t FitIniFile::byteToTextDec(char* result, uint8_t num, uint32_t bufLen)
{
    char text[252];
    std::snprintf(text, sizeof(text), "%d", static_cast<int>(num));
    return CopyIfFits(result, text, bufLen);
}

int32_t FitIniFile::byteToTextHex(char* result, uint8_t num, uint32_t bufLen)
{
    char text[252];
    std::snprintf(text, sizeof(text), "0x%x", static_cast<uint32_t>(num));
    return CopyIfFits(result, text, bufLen);
}

int32_t FitIniFile::open(const char* fName, FileMode _mode, int32_t numChildren)
{
    const int32_t result = File::open(fName, _mode, numChildren);

    if (result != 0)
    {
        return result;
    }

    seek(0, SEEK_SET);
    return afterOpen();
}

int32_t FitIniFile::open(File* _parent, uint32_t fileSize, int32_t)
{
    // The child's bytes are always read into memory at once.
    const int32_t result = File::open(_parent, fileSize, -1);

    if (result != 0)
    {
        return result;
    }

    return afterOpen();
}

int32_t FitIniFile::create(const char* fName)
{
    // File::create opens through the virtual open, which writes the header.
    return File::create(fName);
}

void FitIniFile::close()
{
    atClose();
    File::close();
}

int32_t FitIniFile::seekBlock(const char* blockId)
{
    int32_t blockNum = 0;

    while (blockNum < totalBlocks && std::strcmp(fileBlocks[blockNum].blockId, blockId) != 0)
    {
        ++blockNum;
    }

    if (blockNum == totalBlocks)
    {
        return BLOCK_NOT_FOUND;
    }

    seek(static_cast<int32_t>(fileBlocks[blockNum].blockOffset), SEEK_SET);
    currentBlockId = fileBlocks[blockNum].blockId;
    currentBlockOffset = fileBlocks[blockNum].blockOffset;

    if (blockNum + 1 != totalBlocks)
    {
        currentBlockSize = fileBlocks[blockNum + 1].blockOffset - currentBlockOffset;
    }
    else
    {
        currentBlockSize = getLength() - currentBlockOffset;
    }

    return 0;
}

char* FitIniFile::findIdLine(const char* prefix, const char* varName, char* line, int32_t& error)
{
    seek(static_cast<int32_t>(currentBlockOffset), SEEK_SET);
    const uint32_t blockEnd = currentBlockSize + currentBlockOffset;

    char search[256];
    std::snprintf(search, sizeof(search), "%s %s", prefix, varName);
    const size_t searchLength = std::strlen(search);

    do
    {
        readLine(reinterpret_cast<uint8_t*>(line), 0xfe);

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
    } while (logicalPosition < blockEnd);

    // Original behaviour: a match on the block's last line (the position then being at its end) is reported missing.
    if (blockEnd <= logicalPosition)
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

int32_t FitIniFile::readIdFloat(const char* varName, float& value)
{
    char line[256];
    int32_t error;
    char* text = findIdLine("f", varName, line, error);

    if (text == nullptr)
    {
        if (error == VARIABLE_NOT_FOUND)
        {
            value = 0.0f;
        }

        return error;
    }

    value = textToFloat(text);

    if (value == 0.0f)
    {
        value = mathToFloat(text);
    }

    return 0;
}

int32_t FitIniFile::readIdLong(const char* varName, int32_t& value)
{
    char line[256];
    int32_t error;
    char* text = findIdLine("l", varName, line, error);

    if (text == nullptr)
    {
        if (error == VARIABLE_NOT_FOUND)
        {
            value = 0;
        }

        return error;
    }

    value = textToLong(text);

    if (value == 0)
    {
        value = mathToLong(text);
    }

    return 0;
}

int32_t FitIniFile::readIdBoolean(const char* varName, int& value)
{
    char line[256];
    int32_t error;
    char* text = findIdLine("b", varName, line, error);

    if (text == nullptr)
    {
        if (error == VARIABLE_NOT_FOUND)
        {
            value = 0;
        }

        return error;
    }

    value = booleanToLong(text);
    return 0;
}

int32_t FitIniFile::readIdShort(const char* varName, int16_t& value)
{
    char line[256];
    int32_t error;
    char* text = findIdLine("s", varName, line, error);

    if (text == nullptr)
    {
        if (error == VARIABLE_NOT_FOUND)
        {
            value = 0;
        }

        return error;
    }

    value = textToShort(text);

    if (value == 0)
    {
        value = mathToShort(text);
    }

    return 0;
}

int32_t FitIniFile::readIdChar(const char* varName, char& value)
{
    char line[256];
    int32_t error;
    char* text = findIdLine("c", varName, line, error);

    if (text == nullptr)
    {
        if (error == VARIABLE_NOT_FOUND)
        {
            value = 0;
        }

        return error;
    }

    value = textToChar(text);

    if (value == 0)
    {
        value = mathToChar(text);
    }

    return 0;
}

int32_t FitIniFile::readIdULong(const char* varName, uint32_t& value)
{
    char line[256];
    int32_t error;
    char* text = findIdLine("ul", varName, line, error);

    if (text == nullptr)
    {
        if (error == VARIABLE_NOT_FOUND)
        {
            value = 0;
        }

        return error;
    }

    value = textToULong(text);

    if (value == 0)
    {
        value = mathToULong(text);
    }

    return 0;
}

int32_t FitIniFile::readIdUShort(const char* varName, uint16_t& value)
{
    char line[256];
    int32_t error;
    char* text = findIdLine("us", varName, line, error);

    if (text == nullptr)
    {
        if (error == VARIABLE_NOT_FOUND)
        {
            value = 0;
        }

        return error;
    }

    value = textToUShort(text);

    if (value == 0)
    {
        value = mathToUShort(text);
    }

    return 0;
}

int32_t FitIniFile::readIdUChar(const char* varName, uint8_t& value)
{
    char line[256];
    int32_t error;
    char* text = findIdLine("uc", varName, line, error);

    if (text == nullptr)
    {
        if (error == VARIABLE_NOT_FOUND)
        {
            value = 0;
        }

        return error;
    }

    value = textToUChar(text);

    if (value == 0)
    {
        value = mathToUChar(text);
    }

    return 0;
}

int32_t FitIniFile::copyString(char* dest, char* line, uint32_t bufLen)
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

int32_t FitIniFile::readIdString(const char* varName, char* result, uint32_t maxLength)
{
    char line[256];
    int32_t error;
    char* text = findIdLine("st", varName, line, error);

    if (text == nullptr)
    {
        return error;
    }

    return copyString(result, text, maxLength);
}

int32_t FitIniFile::getIdStringLength(const char* varName)
{
    char line[256];
    int32_t error;
    char* text = findIdLine("st", varName, line, error);

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

int32_t FitIniFile::findArrayLine(const char* typePrefix, const char* varName, char* line, uint32_t& count,
                                  char*& values)
{
    seek(static_cast<int32_t>(currentBlockOffset), SEEK_SET);
    const uint32_t blockEnd = currentBlockSize + currentBlockOffset;

    char nameText[256];
    std::snprintf(nameText, sizeof(nameText), "] %s", varName);
    char* typeAt = nullptr;
    char* nameAt = nullptr;

    do
    {
        readLine(reinterpret_cast<uint8_t*>(line), 0xfe);
        typeAt = std::strstr(line, typePrefix);
        nameAt = std::strstr(line, nameText);

        if (typeAt != nullptr && nameAt != nullptr)
        {
            break;
        }
    } while (logicalPosition < blockEnd);

    if (blockEnd <= logicalPosition)
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
    count = textToULong(number);
    values = nullptr;
    return 0;
}

template <typename Store> int32_t FitIniFile::readArrayElements(char* line, char* values, uint32_t count, Store store)
{
    const uint32_t blockEnd = currentBlockSize + currentBlockOffset;
    char word[12];
    uint32_t i = 0;

    while (logicalPosition < blockEnd && i < count)
    {
        const int32_t result = getNextWord(values, word, 9);

        if (result == GET_NEXT_LINE)
        {
            readLine(reinterpret_cast<uint8_t*>(line), 0xfe);
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
    if (blockEnd <= logicalPosition && i < count)
    {
        return NOT_ENOUGH_ELEMENTS_FOR_ARRAY;
    }

    return 0;
}

int32_t FitIniFile::readIdFloatArray(const char* varName, float* result, uint32_t numElements)
{
    char line[256];
    uint32_t count;
    char* values;
    const int32_t error = findArrayLine("f[", varName, line, count, values);

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

    return readArrayElements(line, values + 1, count,
                             [&](uint32_t i, char* word)
                             {
                                 result[i] = textToFloat(word);

                                 if (result[i] == 0.0f)
                                 {
                                     result[i] = mathToFloat(word);
                                 }
                             });
}

int32_t FitIniFile::readIdLongArray(const char* varName, int32_t* result, uint32_t numElements)
{
    char line[256];
    uint32_t count;
    char* values;
    const int32_t error = findArrayLine("l[", varName, line, count, values);

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

    return readArrayElements(line, values + 1, count,
                             [&](uint32_t i, char* word)
                             {
                                 result[i] = textToLong(word);

                                 if (result[i] == 0)
                                 {
                                     result[i] = mathToLong(word);
                                 }
                             });
}

int32_t FitIniFile::readIdULongArray(const char* varName, uint32_t* result, uint32_t numElements)
{
    char line[256];
    uint32_t count;
    char* values;
    const int32_t error = findArrayLine("ul[", varName, line, count, values);

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

    return readArrayElements(line, values + 1, count,
                             [&](uint32_t i, char* word)
                             {
                                 result[i] = textToULong(word);

                                 if (result[i] == 0)
                                 {
                                     result[i] = mathToULong(word);
                                 }
                             });
}

int32_t FitIniFile::readIdShortArray(const char* varName, int16_t* result, uint32_t numElements)
{
    char line[256];
    uint32_t count;
    char* values;
    const int32_t error = findArrayLine("s[", varName, line, count, values);

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

    return readArrayElements(line, values + 1, count,
                             [&](uint32_t i, char* word)
                             {
                                 result[i] = textToShort(word);

                                 if (result[i] == 0)
                                 {
                                     result[i] = mathToShort(word);
                                 }
                             });
}

int32_t FitIniFile::readIdUShortArray(const char* varName, uint16_t* result, uint32_t numElements)
{
    char line[256];
    uint32_t count;
    char* values;
    const int32_t error = findArrayLine("us[", varName, line, count, values);

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

    return readArrayElements(line, values + 1, count,
                             [&](uint32_t i, char* word)
                             {
                                 result[i] = textToUShort(word);

                                 if (result[i] == 0)
                                 {
                                     result[i] = mathToUShort(word);
                                 }
                             });
}

int32_t FitIniFile::readIdCharArray(const char* varName, char* result, uint32_t numElements)
{
    char line[256];
    uint32_t count;
    char* values;
    const int32_t error = findArrayLine("c[", varName, line, count, values);

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

    return readArrayElements(line, values + 1, count,
                             [&](uint32_t i, char* word)
                             {
                                 result[i] = textToChar(word);

                                 if (result[i] == 0)
                                 {
                                     result[i] = mathToChar(word);
                                 }
                             });
}

int32_t FitIniFile::readIdUCharArray(const char* varName, uint8_t* result, uint32_t numElements)
{
    char line[256];
    uint32_t count;
    char* values;
    const int32_t error = findArrayLine("uc[", varName, line, count, values);

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

    return readArrayElements(line, values + 1, count,
                             [&](uint32_t i, char* word)
                             {
                                 result[i] = textToUChar(word);

                                 if (result[i] == 0)
                                 {
                                     result[i] = mathToUChar(word);
                                 }
                             });
}

uint32_t FitIniFile::getIdFloatArrayElements(const char* varName)
{
    char line[256];
    uint32_t count;
    char* values;
    const int32_t error = findArrayLine("f[", varName, line, count, values);
    return error != 0 ? static_cast<uint32_t>(error) : count;
}

uint32_t FitIniFile::getIdLongArrayElements(const char* varName)
{
    char line[256];
    uint32_t count;
    char* values;
    const int32_t error = findArrayLine("l[", varName, line, count, values);
    return error != 0 ? static_cast<uint32_t>(error) : count;
}

uint32_t FitIniFile::getIdULongArrayElements(const char* varName)
{
    char line[256];
    uint32_t count;
    char* values;
    const int32_t error = findArrayLine("ul[", varName, line, count, values);
    return error != 0 ? static_cast<uint32_t>(error) : count;
}

uint32_t FitIniFile::getIdShortArrayElements(const char* varName)
{
    char line[256];
    uint32_t count;
    char* values;
    const int32_t error = findArrayLine("s[", varName, line, count, values);
    return error != 0 ? static_cast<uint32_t>(error) : count;
}

uint32_t FitIniFile::getIdUShortArrayElements(const char* varName)
{
    char line[256];
    uint32_t count;
    char* values;
    const int32_t error = findArrayLine("us[", varName, line, count, values);
    return error != 0 ? static_cast<uint32_t>(error) : count;
}

uint32_t FitIniFile::getIdCharArrayElements(const char* varName)
{
    char line[256];
    uint32_t count;
    char* values;
    const int32_t error = findArrayLine("c[", varName, line, count, values);
    return error != 0 ? static_cast<uint32_t>(error) : count;
}

uint32_t FitIniFile::getIdUCharArrayElements(const char* varName)
{
    char line[256];
    uint32_t count;
    char* values;
    const int32_t error = findArrayLine("uc[", varName, line, count, values);
    return error != 0 ? static_cast<uint32_t>(error) : count;
}

int32_t FitIniFile::writeBlock(const char* blockId)
{
    char line[256];
    std::snprintf(line, sizeof(line), "\r\n[%s]\r\n", blockId);
    return write(reinterpret_cast<uint8_t*>(line), static_cast<int32_t>(std::strlen(line)));
}

int32_t FitIniFile::writeIdFloat(const char* varName, float value)
{
    char line[256];
    std::snprintf(line, sizeof(line), "f %s=%f\r\n", varName, static_cast<double>(value));
    return write(reinterpret_cast<uint8_t*>(line), static_cast<int32_t>(std::strlen(line)));
}

int32_t FitIniFile::writeIdBoolean(const char* varName, int value)
{
    char line[256];
    std::snprintf(line, sizeof(line), "b %s=%s\r\n", varName, value ? "TRUE" : "FALSE");
    return write(reinterpret_cast<uint8_t*>(line), static_cast<int32_t>(std::strlen(line)));
}

int32_t FitIniFile::writeIdLong(const char* varName, int32_t value)
{
    char line[256];
    std::snprintf(line, sizeof(line), "l %s=%d\r\n", varName, value);
    return write(reinterpret_cast<uint8_t*>(line), static_cast<int32_t>(std::strlen(line)));
}

int32_t FitIniFile::writeIdShort(const char* varName, int16_t value)
{
    char line[256];
    std::snprintf(line, sizeof(line), "s %s=%d\r\n", varName, static_cast<int>(value));
    return write(reinterpret_cast<uint8_t*>(line), static_cast<int32_t>(std::strlen(line)));
}

int32_t FitIniFile::writeIdChar(const char* varName, char value)
{
    char line[256];
    std::snprintf(line, sizeof(line), "c %s=%d\r\n", varName, static_cast<int>(value));
    return write(reinterpret_cast<uint8_t*>(line), static_cast<int32_t>(std::strlen(line)));
}

int32_t FitIniFile::writeIdULong(const char* varName, uint32_t value)
{
    char line[256];
    std::snprintf(line, sizeof(line), "ul %s=%d\r\n", varName, static_cast<int32_t>(value));
    return write(reinterpret_cast<uint8_t*>(line), static_cast<int32_t>(std::strlen(line)));
}

int32_t FitIniFile::writeIdUShort(const char* varName, uint16_t value)
{
    char line[256];
    std::snprintf(line, sizeof(line), "us %s=%d\r\n", varName, static_cast<int>(value));
    return write(reinterpret_cast<uint8_t*>(line), static_cast<int32_t>(std::strlen(line)));
}

int32_t FitIniFile::writeIdUChar(const char* varName, uint8_t value)
{
    char line[256];
    std::snprintf(line, sizeof(line), "uc %s=%d\r\n", varName, static_cast<int>(value));
    return write(reinterpret_cast<uint8_t*>(line), static_cast<int32_t>(std::strlen(line)));
}

int32_t FitIniFile::writeIdString(const char* varName, const char* text)
{
    char line[256];
    std::snprintf(line, sizeof(line), "st %s=\"%s\"\r\n", varName, text);
    return write(reinterpret_cast<uint8_t*>(line), static_cast<int32_t>(std::strlen(line)));
}

int32_t FitIniFile::writeIdUShortArray(const char* varName, const uint16_t* array, uint32_t numElements)
{
    char line[256];
    std::snprintf(line, sizeof(line), "us[%d] %s=%d,", static_cast<int32_t>(numElements), varName,
                  static_cast<int>(array[0]));
    int32_t written = write(reinterpret_cast<uint8_t*>(line), static_cast<int32_t>(std::strlen(line)));

    for (int32_t i = 1; i < static_cast<int32_t>(numElements); ++i)
    {
        std::snprintf(line, sizeof(line), "%d,", static_cast<int>(array[i]));
        written += write(reinterpret_cast<uint8_t*>(line), static_cast<int32_t>(std::strlen(line)));
    }

    std::snprintf(line, sizeof(line), "\r\n");
    return written + write(reinterpret_cast<uint8_t*>(line), static_cast<int32_t>(std::strlen(line)));
}

int32_t FitIniFile::writeIdLongArray(const char* varName, const int32_t* array, uint32_t numElements)
{
    char line[256];
    std::snprintf(line, sizeof(line), "l[%d] %s=%d,", static_cast<int32_t>(numElements), varName, array[0]);
    int32_t written = write(reinterpret_cast<uint8_t*>(line), static_cast<int32_t>(std::strlen(line)));

    for (int32_t i = 1; i < static_cast<int32_t>(numElements); ++i)
    {
        std::snprintf(line, sizeof(line), "%d,", array[i]);
        written += write(reinterpret_cast<uint8_t*>(line), static_cast<int32_t>(std::strlen(line)));
    }

    std::snprintf(line, sizeof(line), "\r\n");
    return written + write(reinterpret_cast<uint8_t*>(line), static_cast<int32_t>(std::strlen(line)));
}

int32_t FitIniFile::writeIdFloatArray(const char* varName, const float* array, uint32_t numElements)
{
    char line[256];
    std::snprintf(line, sizeof(line), "f[%d] %s=%.2f,", static_cast<int32_t>(numElements), varName,
                  static_cast<double>(array[0]));
    int32_t written = write(reinterpret_cast<uint8_t*>(line), static_cast<int32_t>(std::strlen(line)));

    for (int32_t i = 1; i < static_cast<int32_t>(numElements); ++i)
    {
        std::snprintf(line, sizeof(line), " %.2f,", static_cast<double>(array[i]));
        written += write(reinterpret_cast<uint8_t*>(line), static_cast<int32_t>(std::strlen(line)));
    }

    std::snprintf(line, sizeof(line), "\r\n");
    return written + write(reinterpret_cast<uint8_t*>(line), static_cast<int32_t>(std::strlen(line)));
}

int32_t FitIniFile::writeIdUCharArray(const char* varName, const uint8_t* array, uint32_t numElements)
{
    char line[256];
    std::snprintf(line, sizeof(line), "uc[%d] %s=%d,", static_cast<int32_t>(numElements), varName,
                  static_cast<int>(array[0]));
    int32_t written = write(reinterpret_cast<uint8_t*>(line), static_cast<int32_t>(std::strlen(line)));

    for (int32_t i = 1; i < static_cast<int32_t>(numElements); ++i)
    {
        std::snprintf(line, sizeof(line), " %d,", static_cast<int>(array[i]));
        written += write(reinterpret_cast<uint8_t*>(line), static_cast<int32_t>(std::strlen(line)));
    }

    std::snprintf(line, sizeof(line), "\r\n");
    return written + write(reinterpret_cast<uint8_t*>(line), static_cast<int32_t>(std::strlen(line)));
}
