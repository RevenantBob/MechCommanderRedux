#pragma once

#include "lib/file.h"

// Original source: mcx\lib\inifile.cpp (and lib\inifile.h). The FIT text-table format every game data file uses:
//
//   FITini
//   [Block]
//   l Name = 42              // typed entries: f l ul s us c uc b st, one per line
//   f[3] Values = 1.0, 2.5, 3
//   FITend
//
// Error codes: the values are the original's (0xFADA00xx, returned as negative longs).

/// <summary>seekBlock: no block of that name.</summary>
inline constexpr int32_t BLOCK_NOT_FOUND = static_cast<int32_t>(0xFADA0000);
/// <summary>Not used by MCX.EXE's reader.</summary>
inline constexpr int32_t ID_NOT_FOUND = static_cast<int32_t>(0xFADA0001);
/// <summary>Not used by MCX.EXE's reader.</summary>
inline constexpr int32_t DATA_NOT_CORRECT_TYPE = static_cast<int32_t>(0xFADA0002);
/// <summary>A value or word doesn't fit the caller's buffer.</summary>
inline constexpr int32_t BUFFER_TOO_SMALL = static_cast<int32_t>(0xFADA0003);
/// <summary>The file doesn't start with <c>FITini</c>.</summary>
inline constexpr int32_t NOT_A_FITINIFILE = static_cast<int32_t>(0xFADA0004);
/// <summary>The block table couldn't be allocated (also a file with no blocks).</summary>
inline constexpr int32_t NO_RAM_FOR_INI_BLOCKS = static_cast<int32_t>(0xFADA0005);
/// <summary>findNextBlockStart reached the end of the file.</summary>
inline constexpr int32_t NO_MORE_BLOCKS = static_cast<int32_t>(0xFADA0006);
/// <summary>More blocks were found than counted.</summary>
inline constexpr int32_t TOO_MANY_BLOCKS = static_cast<int32_t>(0xFADA0007);
/// <summary>Fewer blocks were found than counted.</summary>
inline constexpr int32_t NOT_ENOUGH_BLOCKS = static_cast<int32_t>(0xFADA0008);
/// <summary>The current block has no entry of that type and name.</summary>
inline constexpr int32_t VARIABLE_NOT_FOUND = static_cast<int32_t>(0xFADA0009);
/// <summary>A line is malformed (no <c>=</c>, no quotes, an unclosed block name).</summary>
inline constexpr int32_t SYNTAX_ERROR = static_cast<int32_t>(0xFADA000A);
/// <summary>An array ran out of lines before all its elements were read.</summary>
inline constexpr int32_t NOT_ENOUGH_ELEMENTS_FOR_ARRAY = static_cast<int32_t>(0xFADA000B);
/// <summary>getNextWord: nothing more on this line (end, or a <c>/</c> comment).</summary>
inline constexpr int32_t GET_NEXT_LINE = static_cast<int32_t>(0xFADA000C);
/// <summary>The array in the file is bigger than the caller's.</summary>
inline constexpr int32_t USER_ARRAY_TOO_SMALL = static_cast<int32_t>(0xFADA000D);
/// <summary>An array's element count is more than nine characters long.</summary>
inline constexpr int32_t TOO_MANY_ELEMENTS = static_cast<int32_t>(0xFADA000E);

/// <summary>A block of a FIT file: its name and where its first entry starts.</summary>
/// <remarks>0x38 bytes in the original.</remarks>
struct MCIniBlockNode
{
    /// <summary>The block's name, between the brackets.</summary>
    char BlockId[50];
    /// <summary>Offset of the line after the block's <c>[name]</c> line.</summary>
    uint32_t BlockOffset;
};

/// <summary>The first line of every FIT file.</summary>
extern char FitIniHeader[];
/// <summary>The last line of every FIT file.</summary>
extern char FitIniFooter[];

/// <summary>
/// A FIT file: a <see cref="MCFile"/> read as blocks of typed <c>name = value</c> entries. Open it, pick a block with
/// <see cref="SeekBlock"/>, then read entries with the readId* methods; or create one and write blocks and entries.
/// </summary>
/// <remarks>Original source: <c>lib\inifile.cpp</c>, 0x60 bytes.</remarks>
class MCFitIniFile : public MCFile
{
public:
    MCFitIniFile();
    ~MCFitIniFile() override;

    /// <summary>What kind of file this is (<see cref="INIFILE"/>).</summary>
    MCFileClass GetFileClass() override { return INIFILE; }

    /// <summary>Opens a FIT file and reads its block table (writes the header when creating).</summary>
    /// <returns>0, a File error, or a FIT error (NOT_A_FITINIFILE, ...).</returns>
    int32_t Open(const char* fName, MCFileMode mode = READ, int32_t numChildren = 50) override;

    /// <summary>Opens a FIT file stored inside <paramref name="parent"/> (read into memory at once).</summary>
    int32_t Open(MCFile* parent, uint32_t fileSize, int32_t numChildren = 50) override;

    /// <summary>Creates a FIT file for writing.</summary>
    int32_t Create(const char* fName) override;

    /// <summary>Writes the footer when creating, frees the block table and closes the file.</summary>
    void Close() override;

    /// <summary>Makes the block named <paramref name="blockId"/> (case matters) the current one.</summary>
    /// <returns>0, or <see cref="BLOCK_NOT_FOUND"/>.</returns>
    int32_t SeekBlock(const char* blockId);

    /// <summary>Reads <c>f varName = value</c> from the current block (0 when missing).</summary>
    int32_t ReadIdFloat(const char* varName, float& value);
    /// <summary>Reads <c>l varName = value</c> (decimal, or hex with <c>0x</c>).</summary>
    int32_t ReadIdLong(const char* varName, int32_t& value);
    /// <summary>Reads <c>b varName = TRUE/FALSE</c> (a value starting with T is true, else a number).</summary>
    int32_t ReadIdBoolean(const char* varName, int& value);
    /// <summary>Reads <c>s varName = value</c>.</summary>
    int32_t ReadIdShort(const char* varName, int16_t& value);
    /// <summary>Reads <c>c varName = value</c>.</summary>
    int32_t ReadIdChar(const char* varName, char& value);
    /// <summary>Reads <c>ul varName = value</c>.</summary>
    int32_t ReadIdULong(const char* varName, uint32_t& value);
    /// <summary>Reads <c>us varName = value</c>.</summary>
    int32_t ReadIdUShort(const char* varName, uint16_t& value);
    /// <summary>Reads <c>uc varName = value</c>.</summary>
    int32_t ReadIdUChar(const char* varName, uint8_t& value);

    /// <summary>Reads <c>st varName = "text"</c> into <paramref name="result"/> (at most maxLength - 1 characters).</summary>
    int32_t ReadIdString(const char* varName, char* result, uint32_t maxLength);

    /// <summary>The length of <c>st varName</c>'s text plus one, or an error.</summary>
    int32_t GetIdStringLength(const char* varName);

    /// <summary>Reads <c>f[n] varName = v1, v2, ...</c> (the elements may continue on the following lines).</summary>
    int32_t ReadIdFloatArray(const char* varName, float* result, uint32_t numElements);
    /// <summary>Reads <c>l[n] varName = ...</c>.</summary>
    int32_t ReadIdLongArray(const char* varName, int32_t* result, uint32_t numElements);
    /// <summary>Reads <c>ul[n] varName = ...</c>.</summary>
    int32_t ReadIdULongArray(const char* varName, uint32_t* result, uint32_t numElements);
    /// <summary>Reads <c>s[n] varName = ...</c>.</summary>
    int32_t ReadIdShortArray(const char* varName, int16_t* result, uint32_t numElements);
    /// <summary>Reads <c>us[n] varName = ...</c>.</summary>
    int32_t ReadIdUShortArray(const char* varName, uint16_t* result, uint32_t numElements);
    /// <summary>Reads <c>c[n] varName = ...</c>.</summary>
    int32_t ReadIdCharArray(const char* varName, char* result, uint32_t numElements);
    /// <summary>Reads <c>uc[n] varName = ...</c>.</summary>
    int32_t ReadIdUCharArray(const char* varName, uint8_t* result, uint32_t numElements);

    /// <summary>The n of <c>f[n] varName</c>, or an error code (as an unsigned value).</summary>
    uint32_t GetIdFloatArrayElements(const char* varName);
    /// <summary>The n of <c>l[n] varName</c>, or an error code.</summary>
    uint32_t GetIdLongArrayElements(const char* varName);
    /// <summary>The n of <c>ul[n] varName</c>, or an error code.</summary>
    uint32_t GetIdULongArrayElements(const char* varName);
    /// <summary>The n of <c>s[n] varName</c>, or an error code.</summary>
    uint32_t GetIdShortArrayElements(const char* varName);
    /// <summary>The n of <c>us[n] varName</c>, or an error code.</summary>
    uint32_t GetIdUShortArrayElements(const char* varName);
    /// <summary>The n of <c>c[n] varName</c>, or an error code.</summary>
    uint32_t GetIdCharArrayElements(const char* varName);
    /// <summary>The n of <c>uc[n] varName</c>, or an error code.</summary>
    uint32_t GetIdUCharArrayElements(const char* varName);

    /// <summary>Writes a <c>[blockId]</c> line (after an empty line).</summary>
    /// <returns>The bytes written.</returns>
    int32_t WriteBlock(const char* blockId);
    /// <summary>Writes <c>f varName=value</c>.</summary>
    int32_t WriteIdFloat(const char* varName, float value);
    /// <summary>Writes <c>b varName=TRUE</c> or <c>FALSE</c>.</summary>
    int32_t WriteIdBoolean(const char* varName, int value);
    /// <summary>Writes <c>l varName=value</c>.</summary>
    int32_t WriteIdLong(const char* varName, int32_t value);
    /// <summary>Writes <c>s varName=value</c>.</summary>
    int32_t WriteIdShort(const char* varName, int16_t value);
    /// <summary>Writes <c>c varName=value</c>.</summary>
    int32_t WriteIdChar(const char* varName, char value);
    /// <summary>Writes <c>ul varName=value</c> (printed signed, as the original's %d).</summary>
    int32_t WriteIdULong(const char* varName, uint32_t value);
    /// <summary>Writes <c>us varName=value</c>.</summary>
    int32_t WriteIdUShort(const char* varName, uint16_t value);
    /// <summary>Writes <c>uc varName=value</c>.</summary>
    int32_t WriteIdUChar(const char* varName, uint8_t value);
    /// <summary>Writes <c>st varName="text"</c>.</summary>
    int32_t WriteIdString(const char* varName, const char* text);
    /// <summary>Writes <c>us[n] varName=v1,v2,...,</c>.</summary>
    int32_t WriteIdUShortArray(const char* varName, const uint16_t* array, uint32_t numElements);
    /// <summary>Writes <c>l[n] varName=v1,v2,...,</c>.</summary>
    int32_t WriteIdLongArray(const char* varName, const int32_t* array, uint32_t numElements);
    /// <summary>Writes <c>f[n] varName=v1, v2, ...,</c> with two decimals.</summary>
    int32_t WriteIdFloatArray(const char* varName, const float* array, uint32_t numElements);
    /// <summary>Writes <c>uc[n] varName=v1, v2, ...,</c>.</summary>
    int32_t WriteIdUCharArray(const char* varName, const uint8_t* array, uint32_t numElements);

    /// <summary>The number of blocks.</summary>
    int32_t GetNumBlocks() const { return _TotalBlocks; }

    /// <summary>The name of block <paramref name="index"/> (port helper, for tools and tests).</summary>
    const char* GetBlockName(int32_t index) const
    {
        return index >= 0 && index < _TotalBlocks ? _FileBlocks[index].BlockId : nullptr;
    }

protected:
    /// <summary>
    /// Reads lines until one starts with <c>[</c> (into <paramref name="line"/> when given).
    /// </summary>
    /// <returns>0, or <see cref="NO_MORE_BLOCKS"/> when the end of the file was reached.</returns>
    int32_t FindNextBlockStart(char* line = nullptr, uint32_t lineLen = 0);

    /// <summary>Counts the blocks from the read position on, then returns to it.</summary>
    int32_t CountBlocks();

    /// <summary>
    /// Copies the next word of <paramref name="line"/> (words are separated by spaces, tabs and commas) and moves
    /// <paramref name="line"/> past it.
    /// </summary>
    /// <returns>0, <see cref="GET_NEXT_LINE"/> at the end or a <c>/</c>, or <see cref="BUFFER_TOO_SMALL"/>.</returns>
    int32_t GetNextWord(char*& line, char* buffer, uint32_t bufLen);

    /// <summary>Checks the header and builds the block table (or writes the header when creating).</summary>
    int32_t AfterOpen();

    /// <summary>Writes the footer when creating and frees the block table.</summary>
    void AtClose();

    /// <summary>atof.</summary>
    float TextToFloat(char* num);
    /// <summary>atol, or hex after <c>0x</c> (the text is cut at the first non-hex character).</summary>
    int32_t TextToLong(char* num);
    /// <summary>As <see cref="TextToLong"/>, as a short.</summary>
    int16_t TextToShort(char* num);
    /// <summary>As <see cref="TextToLong"/>, as a char.</summary>
    char TextToChar(char* num);
    /// <summary>As <see cref="TextToLong"/>, unsigned.</summary>
    uint32_t TextToULong(char* num);
    /// <summary>As <see cref="TextToLong"/>, as an unsigned short.</summary>
    uint16_t TextToUShort(char* num);
    /// <summary>As <see cref="TextToLong"/>, as an unsigned char.</summary>
    uint8_t TextToUChar(char* num);
    /// <summary>1 for text starting with T (after spaces), else <see cref="TextToLong"/>.</summary>
    int32_t BooleanToLong(char* num);

    /// <summary>Expression evaluation that was never written: always 0.</summary>
    float MathToFloat(char*) { return 0.0f; }
    /// <summary>Always 0.</summary>
    int32_t MathToLong(char*) { return 0; }
    /// <summary>Always 0.</summary>
    uint32_t MathToULong(char*) { return 0; }
    /// <summary>Always 0.</summary>
    int16_t MathToShort(char*) { return 0; }
    /// <summary>Always 0.</summary>
    uint16_t MathToUShort(char*) { return 0; }
    /// <summary>Always 0.</summary>
    char MathToChar(char*) { return 0; }
    /// <summary>Always 0.</summary>
    uint8_t MathToUChar(char*) { return 0; }

    /// <summary>Formats a float ("%f4") into <paramref name="result"/>.</summary>
    int32_t FloatToText(char* result, float num, uint32_t bufLen);
    /// <summary>Formats a long in decimal.</summary>
    int32_t LongToTextDec(char* result, int32_t num, uint32_t bufLen);
    /// <summary>Formats a long in hex ("0x%x").</summary>
    int32_t LongToTextHex(char* result, int32_t num, uint32_t bufLen);
    /// <summary>Formats a short in decimal.</summary>
    int32_t ShortToTextDec(char* result, int16_t num, uint32_t bufLen);
    /// <summary>Formats a short in hex.</summary>
    int32_t ShortToTextHex(char* result, int16_t num, uint32_t bufLen);
    /// <summary>Formats a byte in decimal.</summary>
    int32_t ByteToTextDec(char* result, uint8_t num, uint32_t bufLen);
    /// <summary>Formats a byte in hex.</summary>
    int32_t ByteToTextHex(char* result, uint8_t num, uint32_t bufLen);

    /// <summary>Copies the text between the first two double quotes of <paramref name="line"/>.</summary>
    int32_t CopyString(char* dest, char* line, uint32_t bufLen);

    /// <summary>Number of blocks.</summary>
    int32_t _TotalBlocks = 0;
    /// <summary>The block table.</summary>
    std::vector<MCIniBlockNode> _FileBlocks;
    /// <summary>The current block's name (points into <see cref="_FileBlocks"/>).</summary>
    char* _CurrentBlockId = nullptr;
    /// <summary>Where the current block's entries start.</summary>
    uint32_t _CurrentBlockOffset = 0;
    /// <summary>The current block's length in bytes (to the next block's first entry, or the end of the file).</summary>
    uint32_t _CurrentBlockSize = 0;

private:
    /// <summary>
    /// The search every scalar readId* method starts with: the line of the current block that starts with
    /// <paramref name="prefix"/>, a space, <paramref name="varName"/>, then optional spaces and <c>=</c>.
    /// </summary>
    /// <returns>The text after the <c>=</c>, or null with <paramref name="error"/> set.</returns>
    char* FindIdLine(const char* prefix, const char* varName, char* line, int32_t& error);

    /// <summary>
    /// The search every array reader starts with: the line holding both <paramref name="typePrefix"/> ("l[") and
    /// "] varName"; reads the element count between them.
    /// </summary>
    int32_t FindArrayLine(const char* typePrefix, const char* varName, char* line, uint32_t& count, char*& values);

    /// <summary>The element loop of the array readers; <paramref name="store"/> converts and stores element i.</summary>
    template <typename Store> int32_t ReadArrayElements(char* line, char* values, uint32_t count, Store store);
};
