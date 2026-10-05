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
struct IniBlockNode
{
    /// <summary>The block's name, between the brackets.</summary>
    char blockId[50]; // +0x00
    /// <summary>Offset of the line after the block's <c>[name]</c> line.</summary>
    uint32_t blockOffset; // +0x34
};

/// <summary>The first line of every FIT file.</summary>
extern char fitIniHeader[];
/// <summary>The last line of every FIT file.</summary>
extern char fitIniFooter[];

/// <summary>
/// A FIT file: a <see cref="File"/> read as blocks of typed <c>name = value</c> entries. Open it, pick a block with
/// <see cref="seekBlock"/>, then read entries with the readId* methods; or create one and write blocks and entries.
/// </summary>
/// <remarks>Original source: <c>lib\inifile.cpp</c>, 0x60 bytes.</remarks>
class FitIniFile : public File
{
public:
    /// <remarks>MCX.EXE @ 0x00649090</remarks>
    FitIniFile();
    /// <remarks>MCX.EXE @ 0x006490d0</remarks>
    ~FitIniFile() override;

    /// <summary>What kind of file this is (<see cref="INIFILE"/>).</summary>
    /// <remarks>MCX.EXE @ 0x006490c0</remarks>
    FileClass getFileClass() override { return INIFILE; }

    /// <summary>Opens a FIT file and reads its block table (writes the header when creating).</summary>
    /// <returns>0, a File error, or a FIT error (NOT_A_FITINIFILE, ...).</returns>
    /// <remarks>MCX.EXE @ 0x00649ed0</remarks>
    int32_t open(const char* fName, FileMode _mode = READ, int32_t numChildren = 50) override;

    /// <summary>Opens a FIT file stored inside <paramref name="_parent"/> (read into memory at once).</summary>
    /// <remarks>MCX.EXE @ 0x00649f10</remarks>
    int32_t open(File* _parent, uint32_t fileSize, int32_t numChildren = 50) override;

    /// <summary>Creates a FIT file for writing.</summary>
    /// <remarks>MCX.EXE @ 0x00649f40</remarks>
    int32_t create(const char* fName) override;

    /// <summary>Writes the footer when creating, frees the block table and closes the file.</summary>
    /// <remarks>MCX.EXE @ 0x00649f50</remarks>
    void close() override;

    /// <summary>Makes the block named <paramref name="blockId"/> (case matters) the current one.</summary>
    /// <returns>0, or <see cref="BLOCK_NOT_FOUND"/>.</returns>
    /// <remarks>MCX.EXE @ 0x00649f70</remarks>
    int32_t seekBlock(const char* blockId);

    /// <summary>Reads <c>f varName = value</c> from the current block (0 when missing).</summary>
    /// <remarks>MCX.EXE @ 0x0064a050</remarks>
    int32_t readIdFloat(const char* varName, float& value);
    /// <summary>Reads <c>l varName = value</c> (decimal, or hex with <c>0x</c>).</summary>
    /// <remarks>MCX.EXE @ 0x0064a190</remarks>
    int32_t readIdLong(const char* varName, int32_t& value);
    /// <summary>Reads <c>b varName = TRUE/FALSE</c> (a value starting with T is true, else a number).</summary>
    /// <remarks>MCX.EXE @ 0x0064a2c0</remarks>
    int32_t readIdBoolean(const char* varName, int& value);
    /// <summary>Reads <c>s varName = value</c>.</summary>
    /// <remarks>MCX.EXE @ 0x0064a3e0</remarks>
    int32_t readIdShort(const char* varName, int16_t& value);
    /// <summary>Reads <c>c varName = value</c>.</summary>
    /// <remarks>MCX.EXE @ 0x0064a510</remarks>
    int32_t readIdChar(const char* varName, char& value);
    /// <summary>Reads <c>ul varName = value</c>.</summary>
    /// <remarks>MCX.EXE @ 0x0064a640</remarks>
    int32_t readIdULong(const char* varName, uint32_t& value);
    /// <summary>Reads <c>us varName = value</c>.</summary>
    /// <remarks>MCX.EXE @ 0x0064a770</remarks>
    int32_t readIdUShort(const char* varName, uint16_t& value);
    /// <summary>Reads <c>uc varName = value</c>.</summary>
    /// <remarks>MCX.EXE @ 0x0064a8a0</remarks>
    int32_t readIdUChar(const char* varName, uint8_t& value);

    /// <summary>Reads <c>st varName = "text"</c> into <paramref name="result"/> (at most maxLength - 1 characters).</summary>
    /// <remarks>MCX.EXE @ 0x0064aa30</remarks>
    int32_t readIdString(const char* varName, char* result, uint32_t maxLength);

    /// <summary>The length of <c>st varName</c>'s text plus one, or an error.</summary>
    /// <remarks>MCX.EXE @ 0x0064ab50</remarks>
    int32_t getIdStringLength(const char* varName);

    /// <summary>Reads <c>f[n] varName = v1, v2, ...</c> (the elements may continue on the following lines).</summary>
    /// <remarks>MCX.EXE @ 0x0064ac70</remarks>
    int32_t readIdFloatArray(const char* varName, float* result, uint32_t numElements);
    /// <summary>Reads <c>l[n] varName = ...</c>.</summary>
    /// <remarks>MCX.EXE @ 0x0064ae40</remarks>
    int32_t readIdLongArray(const char* varName, int32_t* result, uint32_t numElements);
    /// <summary>Reads <c>ul[n] varName = ...</c>.</summary>
    /// <remarks>MCX.EXE @ 0x0064b010</remarks>
    int32_t readIdULongArray(const char* varName, uint32_t* result, uint32_t numElements);
    /// <summary>Reads <c>s[n] varName = ...</c>.</summary>
    /// <remarks>MCX.EXE @ 0x0064b1e0</remarks>
    int32_t readIdShortArray(const char* varName, int16_t* result, uint32_t numElements);
    /// <summary>Reads <c>us[n] varName = ...</c>.</summary>
    /// <remarks>MCX.EXE @ 0x0064b3c0</remarks>
    int32_t readIdUShortArray(const char* varName, uint16_t* result, uint32_t numElements);
    /// <summary>Reads <c>c[n] varName = ...</c>.</summary>
    /// <remarks>MCX.EXE @ 0x0064b590</remarks>
    int32_t readIdCharArray(const char* varName, char* result, uint32_t numElements);
    /// <summary>Reads <c>uc[n] varName = ...</c>.</summary>
    /// <remarks>MCX.EXE @ 0x0064b760</remarks>
    int32_t readIdUCharArray(const char* varName, uint8_t* result, uint32_t numElements);

    /// <summary>The n of <c>f[n] varName</c>, or an error code (as an unsigned value).</summary>
    /// <remarks>MCX.EXE @ 0x0064b930</remarks>
    uint32_t getIdFloatArrayElements(const char* varName);
    /// <summary>The n of <c>l[n] varName</c>, or an error code.</summary>
    /// <remarks>MCX.EXE @ 0x0064ba20</remarks>
    uint32_t getIdLongArrayElements(const char* varName);
    /// <summary>The n of <c>ul[n] varName</c>, or an error code.</summary>
    /// <remarks>MCX.EXE @ 0x0064bb10</remarks>
    uint32_t getIdULongArrayElements(const char* varName);
    /// <summary>The n of <c>s[n] varName</c>, or an error code.</summary>
    /// <remarks>MCX.EXE @ 0x0064bc00</remarks>
    uint32_t getIdShortArrayElements(const char* varName);
    /// <summary>The n of <c>us[n] varName</c>, or an error code.</summary>
    /// <remarks>MCX.EXE @ 0x0064bcf0</remarks>
    uint32_t getIdUShortArrayElements(const char* varName);
    /// <summary>The n of <c>c[n] varName</c>, or an error code.</summary>
    /// <remarks>MCX.EXE @ 0x0064bde0</remarks>
    uint32_t getIdCharArrayElements(const char* varName);
    /// <summary>The n of <c>uc[n] varName</c>, or an error code.</summary>
    /// <remarks>MCX.EXE @ 0x0064bed0</remarks>
    uint32_t getIdUCharArrayElements(const char* varName);

    /// <summary>Writes a <c>[blockId]</c> line (after an empty line).</summary>
    /// <returns>The bytes written.</returns>
    /// <remarks>MCX.EXE @ 0x0064bfc0</remarks>
    int32_t writeBlock(const char* blockId);
    /// <summary>Writes <c>f varName=value</c>.</summary>
    /// <remarks>MCX.EXE @ 0x0064c010</remarks>
    int32_t writeIdFloat(const char* varName, float value);
    /// <summary>Writes <c>b varName=TRUE</c> or <c>FALSE</c>.</summary>
    /// <remarks>MCX.EXE @ 0x0064c070</remarks>
    int32_t writeIdBoolean(const char* varName, int value);
    /// <summary>Writes <c>l varName=value</c>.</summary>
    /// <remarks>MCX.EXE @ 0x0064c0e0</remarks>
    int32_t writeIdLong(const char* varName, int32_t value);
    /// <summary>Writes <c>s varName=value</c>.</summary>
    /// <remarks>MCX.EXE @ 0x0064c130</remarks>
    int32_t writeIdShort(const char* varName, int16_t value);
    /// <summary>Writes <c>c varName=value</c>.</summary>
    /// <remarks>MCX.EXE @ 0x0064c190</remarks>
    int32_t writeIdChar(const char* varName, char value);
    /// <summary>Writes <c>ul varName=value</c> (printed signed, as the original's %d).</summary>
    /// <remarks>MCX.EXE @ 0x0064c1f0</remarks>
    int32_t writeIdULong(const char* varName, uint32_t value);
    /// <summary>Writes <c>us varName=value</c>.</summary>
    /// <remarks>MCX.EXE @ 0x0064c240</remarks>
    int32_t writeIdUShort(const char* varName, uint16_t value);
    /// <summary>Writes <c>uc varName=value</c>.</summary>
    /// <remarks>MCX.EXE @ 0x0064c2a0</remarks>
    int32_t writeIdUChar(const char* varName, uint8_t value);
    /// <summary>Writes <c>st varName="text"</c>.</summary>
    /// <remarks>MCX.EXE @ 0x0064c300</remarks>
    int32_t writeIdString(const char* varName, const char* text);
    /// <summary>Writes <c>us[n] varName=v1,v2,...,</c>.</summary>
    /// <remarks>MCX.EXE @ 0x0064c350</remarks>
    int32_t writeIdUShortArray(const char* varName, const uint16_t* array, uint32_t numElements);
    /// <summary>Writes <c>l[n] varName=v1,v2,...,</c>.</summary>
    /// <remarks>MCX.EXE @ 0x0064c450</remarks>
    int32_t writeIdLongArray(const char* varName, const int32_t* array, uint32_t numElements);
    /// <summary>Writes <c>f[n] varName=v1, v2, ...,</c> with two decimals.</summary>
    /// <remarks>MCX.EXE @ 0x0064c540</remarks>
    int32_t writeIdFloatArray(const char* varName, const float* array, uint32_t numElements);
    /// <summary>Writes <c>uc[n] varName=v1, v2, ...,</c>.</summary>
    /// <remarks>MCX.EXE @ 0x0064c640</remarks>
    int32_t writeIdUCharArray(const char* varName, const uint8_t* array, uint32_t numElements);

    /// <summary>The number of blocks.</summary>
    int32_t getNumBlocks() const { return totalBlocks; }

    /// <summary>The name of block <paramref name="index"/> (port helper, for tools and tests).</summary>
    const char* getBlockName(int32_t index) const
    {
        return index >= 0 && index < totalBlocks ? fileBlocks[index].blockId : nullptr;
    }

protected:
    /// <summary>
    /// Reads lines until one starts with <c>[</c> (into <paramref name="line"/> when given).
    /// </summary>
    /// <returns>0, or <see cref="NO_MORE_BLOCKS"/> when the end of the file was reached.</returns>
    /// <remarks>MCX.EXE @ 0x006490f0</remarks>
    int32_t findNextBlockStart(char* line = nullptr, uint32_t lineLen = 0);

    /// <summary>Counts the blocks from the read position on, then returns to it.</summary>
    /// <remarks>MCX.EXE @ 0x00649160</remarks>
    int32_t countBlocks();

    /// <summary>
    /// Copies the next word of <paramref name="line"/> (words are separated by spaces, tabs and commas) and moves
    /// <paramref name="line"/> past it.
    /// </summary>
    /// <returns>0, <see cref="GET_NEXT_LINE"/> at the end or a <c>/</c>, or <see cref="BUFFER_TOO_SMALL"/>.</returns>
    /// <remarks>MCX.EXE @ 0x006491a0</remarks>
    int32_t getNextWord(char*& line, char* buffer, uint32_t bufLen);

    /// <summary>Checks the header and builds the block table (or writes the header when creating).</summary>
    /// <remarks>MCX.EXE @ 0x00649250</remarks>
    int32_t afterOpen();

    /// <summary>Writes the footer when creating and frees the block table.</summary>
    /// <remarks>MCX.EXE @ 0x00649400</remarks>
    void atClose();

    /// <summary>atof.</summary>
    /// <remarks>MCX.EXE @ 0x00649490</remarks>
    float textToFloat(char* num);
    /// <summary>atol, or hex after <c>0x</c> (the text is cut at the first non-hex character).</summary>
    /// <remarks>MCX.EXE @ 0x006494b0</remarks>
    int32_t textToLong(char* num);
    /// <summary>As <see cref="textToLong"/>, as a short.</summary>
    /// <remarks>MCX.EXE @ 0x006495d0</remarks>
    int16_t textToShort(char* num);
    /// <summary>As <see cref="textToLong"/>, as a char.</summary>
    /// <remarks>MCX.EXE @ 0x006496e0</remarks>
    char textToChar(char* num);
    /// <summary>As <see cref="textToLong"/>, unsigned.</summary>
    /// <remarks>MCX.EXE @ 0x006497d0</remarks>
    uint32_t textToULong(char* num);
    /// <summary>As <see cref="textToLong"/>, as an unsigned short.</summary>
    /// <remarks>MCX.EXE @ 0x006498f0</remarks>
    uint16_t textToUShort(char* num);
    /// <summary>As <see cref="textToLong"/>, as an unsigned char.</summary>
    /// <remarks>MCX.EXE @ 0x00649a00</remarks>
    uint8_t textToUChar(char* num);
    /// <summary>1 for text starting with T (after spaces), else <see cref="textToLong"/>.</summary>
    /// <remarks>MCX.EXE @ 0x00649af0</remarks>
    int32_t booleanToLong(char* num);

    /// <summary>Expression evaluation that was never written: always 0.</summary>
    /// <remarks>MCX.EXE @ 0x00649b50</remarks>
    float mathToFloat(char*) { return 0.0f; }
    /// <summary>Always 0.</summary>
    /// <remarks>MCX.EXE @ 0x00649b60</remarks>
    int32_t mathToLong(char*) { return 0; }
    /// <summary>Always 0.</summary>
    /// <remarks>MCX.EXE @ 0x00649b70</remarks>
    uint32_t mathToULong(char*) { return 0; }
    /// <summary>Always 0.</summary>
    /// <remarks>MCX.EXE @ 0x00649b80</remarks>
    int16_t mathToShort(char*) { return 0; }
    /// <summary>Always 0.</summary>
    /// <remarks>MCX.EXE @ 0x00649b90</remarks>
    uint16_t mathToUShort(char*) { return 0; }
    /// <summary>Always 0.</summary>
    /// <remarks>MCX.EXE @ 0x00649ba0</remarks>
    char mathToChar(char*) { return 0; }
    /// <summary>Always 0.</summary>
    /// <remarks>MCX.EXE @ 0x00649bb0</remarks>
    uint8_t mathToUChar(char*) { return 0; }

    /// <summary>Formats a float ("%f4") into <paramref name="result"/>.</summary>
    /// <remarks>MCX.EXE @ 0x00649bc0</remarks>
    int32_t floatToText(char* result, float num, uint32_t bufLen);
    /// <summary>Formats a long in decimal.</summary>
    /// <remarks>MCX.EXE @ 0x00649c30</remarks>
    int32_t longToTextDec(char* result, int32_t num, uint32_t bufLen);
    /// <summary>Formats a long in hex ("0x%x").</summary>
    /// <remarks>MCX.EXE @ 0x00649ca0</remarks>
    int32_t longToTextHex(char* result, int32_t num, uint32_t bufLen);
    /// <summary>Formats a short in decimal.</summary>
    /// <remarks>MCX.EXE @ 0x00649d10</remarks>
    int32_t shortToTextDec(char* result, int16_t num, uint32_t bufLen);
    /// <summary>Formats a short in hex.</summary>
    /// <remarks>MCX.EXE @ 0x00649d80</remarks>
    int32_t shortToTextHex(char* result, int16_t num, uint32_t bufLen);
    /// <summary>Formats a byte in decimal.</summary>
    /// <remarks>MCX.EXE @ 0x00649df0</remarks>
    int32_t byteToTextDec(char* result, uint8_t num, uint32_t bufLen);
    /// <summary>Formats a byte in hex.</summary>
    /// <remarks>MCX.EXE @ 0x00649e60</remarks>
    int32_t byteToTextHex(char* result, uint8_t num, uint32_t bufLen);

    /// <summary>Copies the text between the first two double quotes of <paramref name="line"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0064a9d0</remarks>
    int32_t copyString(char* dest, char* line, uint32_t bufLen);

    /// <summary>Number of blocks.</summary>
    int32_t totalBlocks = 0; // +0x4c
    /// <summary>The block table.</summary>
    std::vector<IniBlockNode> fileBlocks; // +0x50
    /// <summary>The current block's name (points into <see cref="fileBlocks"/>).</summary>
    char* currentBlockId = nullptr; // +0x54
    /// <summary>Where the current block's entries start.</summary>
    uint32_t currentBlockOffset = 0; // +0x58
    /// <summary>The current block's length in bytes (to the next block's first entry, or the end of the file).</summary>
    uint32_t currentBlockSize = 0; // +0x5c

private:
    /// <summary>
    /// The search every scalar readId* method starts with: the line of the current block that starts with
    /// <paramref name="prefix"/>, a space, <paramref name="varName"/>, then optional spaces and <c>=</c>.
    /// </summary>
    /// <returns>The text after the <c>=</c>, or null with <paramref name="error"/> set.</returns>
    char* findIdLine(const char* prefix, const char* varName, char* line, int32_t& error);

    /// <summary>
    /// The search every array reader starts with: the line holding both <paramref name="typePrefix"/> ("l[") and
    /// "] varName"; reads the element count between them.
    /// </summary>
    int32_t findArrayLine(const char* typePrefix, const char* varName, char* line, uint32_t& count, char*& values);

    /// <summary>The element loop of the array readers; <paramref name="store"/> converts and stores element i.</summary>
    template <typename Store> int32_t readArrayElements(char* line, char* values, uint32_t count, Store store);
};
