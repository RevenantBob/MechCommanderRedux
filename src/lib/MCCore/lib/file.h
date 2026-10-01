#pragma once

class FastFile;

// Error codes of the file layer. The values are the original's (0xBADF00xx, returned as negative longs); the names
// are the port's, since the original's weren't kept. Open failures from the OS return small positive codes as
// GetLastError did (FILE_NOT_FOUND = 2).
inline constexpr int32_t NO_ERR = 0;
inline constexpr int32_t FILE_NOT_FOUND = 2;
inline constexpr int32_t READ_ERR = 30;
inline constexpr int32_t READ_PAST_EOF = static_cast<int32_t>(0xBADF0008);
inline constexpr int32_t INVALID_SEEK = static_cast<int32_t>(0xBADF0009);
inline constexpr int32_t WRITE_ERR = static_cast<int32_t>(0xBADF000A);
inline constexpr int32_t NO_RAM_FOR_FILENAME = static_cast<int32_t>(0xBADF000D);
inline constexpr int32_t PARENT_NULL = static_cast<int32_t>(0xBADF000E);
inline constexpr int32_t TOO_MANY_CHILDREN = static_cast<int32_t>(0xBADF000F);
inline constexpr int32_t FILE_NOT_OPEN = static_cast<int32_t>(0xBADF0010);
inline constexpr int32_t CANT_WRITE_TO_CHILD = static_cast<int32_t>(0xBADF0011);
inline constexpr int32_t NO_RAM_FOR_CHILD_LIST = static_cast<int32_t>(0xBADF0012);

/// <summary>How a <see cref="File"/> is opened.</summary>
enum FileMode
{
    NOMODE = 0,
    READ = 1,
    CREATE = 2,
    MC2_APPEND = 3,
    WRITE = 4,
    RDWRITE = 5
};

/// <summary>What kind of <see cref="File"/> an object is (<see cref="File::getFileClass"/>).</summary>
enum FileClass
{
    BASEFILE = 0,
    INIFILE = 1,
    PACKETFILE = 2
};

/// <summary>
/// The game's file: a file on disk, an entry of a FastFile (loaded whole into memory), or a "child" window into a
/// parent file (a packet of a <c>PacketFile</c>). Every read of game data goes through it.
/// </summary>
/// <remarks>
/// Original source: <c>lib\file.cpp</c>, 0x4c bytes. The original wrapped a Win32 handle; the port a
/// <c>std::FILE*</c>, with paths resolved against the game install by <c>MCFileSystem</c>. A loose file on disk wins
/// over a FastFile entry of the same name, as in the original.
/// </remarks>
class File
{
public:
    /// <remarks>MCX.EXE @ 0x00645400</remarks>
    File();
    /// <remarks>MCX.EXE @ 0x00645450</remarks>
    virtual ~File();
    File(const File&) = delete;
    File& operator=(const File&) = delete;

    /// <summary>Whether the read position is at or past the end.</summary>
    /// <remarks>MCX.EXE @ 0x00645460</remarks>
    int eof();

    /// <summary>
    /// Opens <paramref name="fName"/>: from disk, or else from the FastFiles (read whole into memory).
    /// <paramref name="numChildren"/> is how many child files it can have open at once.
    /// </summary>
    /// <returns>0, or an error code (FILE_NOT_FOUND when neither has it).</returns>
    /// <remarks>MCX.EXE @ 0x00645480</remarks>
    virtual int32_t open(const char* fName, FileMode _mode = READ, int32_t numChildren = 50);

    /// <summary>
    /// Opens a window of <paramref name="length"/> bytes into <paramref name="_parent"/> from its current position.
    /// With <paramref name="numChildren"/> -1 the window's bytes are read into memory at once.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00645780</remarks>
    virtual int32_t open(File* _parent, uint32_t length, int32_t numChildren = 50);

    /// <summary>Creates (truncates) <paramref name="fName"/> for writing.</summary>
    /// <remarks>MCX.EXE @ 0x00645940</remarks>
    virtual int32_t create(const char* fName);

    /// <summary>Closes the file and its children.</summary>
    /// <remarks>MCX.EXE @ 0x006459e0</remarks>
    virtual void close();

    /// <summary>What kind of file this is.</summary>
    /// <remarks>MCX.EXE @ 0x00645440</remarks>
    virtual FileClass getFileClass() { return BASEFILE; }

    /// <summary>Registers <paramref name="child"/> in the first free child slot.</summary>
    /// <remarks>MCX.EXE @ 0x00645960</remarks>
    int32_t addChild(File* child);

    /// <summary>Frees <paramref name="child"/>'s slot.</summary>
    /// <remarks>MCX.EXE @ 0x006459a0</remarks>
    void removeChild(File* child);

    /// <summary>Closes the file if it is open and not a child (despite the name, nothing is deleted from disk).</summary>
    /// <remarks>MCX.EXE @ 0x00645ae0</remarks>
    void deleteFile();

    /// <summary>Moves the read position (<paramref name="from"/> is SEEK_SET, SEEK_CUR or SEEK_END).</summary>
    /// <remarks>MCX.EXE @ 0x00645b00</remarks>
    int32_t seek(int32_t pos, int32_t from = SEEK_SET);

    /// <summary>Reads <paramref name="length"/> bytes at <paramref name="pos"/> (the read position isn't advanced).</summary>
    /// <remarks>MCX.EXE @ 0x00645cb0</remarks>
    int32_t read(uint32_t pos, uint8_t* buffer, int32_t length);

    /// <summary>Reads one byte.</summary>
    /// <remarks>MCX.EXE @ 0x00645da0</remarks>
    uint8_t readByte();

    /// <summary>Reads a 16-bit value.</summary>
    /// <remarks>MCX.EXE @ 0x00645e50</remarks>
    int16_t readWord();

    /// <summary>Reads a 16-bit value (the same as <see cref="readWord"/>).</summary>
    /// <remarks>MCX.EXE @ 0x00645f00</remarks>
    int16_t readShort();

    /// <summary>Reads a 32-bit value.</summary>
    /// <remarks>MCX.EXE @ 0x00645f10</remarks>
    int32_t readLong();

    /// <summary>Reads a 32-bit float.</summary>
    /// <remarks>The linker merged it with <see cref="readLong"/> (MCX.EXE @ 0x00645f10).</remarks>
    float readFloat();

    /// <summary>Reads a zero-terminated string into <paramref name="buffer"/>.</summary>
    /// <returns>Its length.</returns>
    /// <remarks>MCX.EXE @ 0x00645fc0</remarks>
    int32_t readString(uint8_t* buffer);

    /// <summary>Reads <paramref name="length"/> bytes from the read position.</summary>
    /// <returns>The number of bytes read.</returns>
    /// <remarks>MCX.EXE @ 0x00646010</remarks>
    int32_t read(uint8_t* buffer, int32_t length);

    /// <summary>
    /// Reads a line ending in CR (or CR LF) of at most <paramref name="maxLength"/> bytes, without its ending.
    /// </summary>
    /// <returns>The line's length plus one.</returns>
    /// <remarks>MCX.EXE @ 0x006460e0</remarks>
    int32_t readLine(uint8_t* buffer, int32_t maxLength);

    /// <summary>Reads a line up to and including its LF.</summary>
    /// <returns>The line's length plus two.</returns>
    /// <remarks>MCX.EXE @ 0x00646290</remarks>
    int32_t readLineEx(uint8_t* buffer, int32_t maxLength);

    /// <summary>Writes <paramref name="length"/> bytes at <paramref name="pos"/>.</summary>
    /// <remarks>MCX.EXE @ 0x00646410</remarks>
    int32_t write(uint32_t pos, const uint8_t* buffer, int32_t length);

    /// <summary>Writes one byte.</summary>
    /// <remarks>MCX.EXE @ 0x006464a0</remarks>
    int32_t writeByte(uint8_t value);

    /// <summary>Writes a 16-bit value.</summary>
    /// <remarks>MCX.EXE @ 0x00646520</remarks>
    int32_t writeWord(int16_t value);

    /// <summary>Writes a 16-bit value.</summary>
    /// <remarks>MCX.EXE @ 0x006465a0</remarks>
    int32_t writeShort(int16_t value);

    /// <summary>Writes a 32-bit value.</summary>
    /// <remarks>MCX.EXE @ 0x006465b0</remarks>
    int32_t writeLong(int32_t value);

    /// <summary>Writes a 32-bit float.</summary>
    /// <remarks>MCX.EXE @ 0x00646630</remarks>
    int32_t writeFloat(float value);

    /// <summary>Writes a string without its terminator.</summary>
    /// <remarks>MCX.EXE @ 0x006466b0</remarks>
    int32_t writeString(const char* text);

    /// <summary>Writes a string and CR LF.</summary>
    /// <remarks>MCX.EXE @ 0x00646720</remarks>
    int32_t writeLine(const char* text);

    /// <summary>Writes <paramref name="length"/> bytes at the write position.</summary>
    /// <remarks>MCX.EXE @ 0x006467a0</remarks>
    int32_t write(const uint8_t* buffer, int32_t length);

    /// <summary>Whether the file is open (on disk or in memory).</summary>
    /// <remarks>MCX.EXE @ 0x00646830</remarks>
    int isOpen();

    /// <summary>The path the file was opened with.</summary>
    /// <remarks>MCX.EXE @ 0x00646850</remarks>
    char* getFilename() { return fileName; }

    /// <summary>The file's length in bytes.</summary>
    /// <remarks>MCX.EXE @ 0x00646860</remarks>
    uint32_t getLength();

    /// <summary>The file's length in bytes (the same as <see cref="getLength"/>).</summary>
    /// <remarks>MCX.EXE @ 0x006468c0</remarks>
    uint32_t fileSize();

    /// <summary>The number of LF characters in the file.</summary>
    /// <remarks>MCX.EXE @ 0x006468d0</remarks>
    uint32_t getNumLines();

    /// <summary>Moves the read position forward by <paramref name="bytesToSkip"/>.</summary>
    /// <remarks>MCX.EXE @ 0x00646940</remarks>
    void skip(int32_t bytesToSkip);

    /// <summary>The read position.</summary>
    uint32_t getLogicalPosition() const { return logicalPosition; }

    /// <summary>The parent file of a child file, or null.</summary>
    File* getParent() const { return parent; }

    /// <summary>The in-memory image of a file read whole (a FastFile entry, a child read at once), or null.</summary>
    uint8_t* getFileImage() const { return inRAM ? fileImage : nullptr; }

    /// <summary>When nonzero, every open is logged to <c>filetraffic.log</c>.</summary>
    static int logFileTraffic;

protected:
    /// <summary>
    /// Reads up to <paramref name="count"/> bytes at <paramref name="pos"/> from wherever the file lives (its memory
    /// image, a FastFile entry, the disk, or its parent's bytes for a child) without moving the read position. The
    /// port's single read path; the original repeated it in every read method.
    /// </summary>
    int32_t readRawAt(uint32_t pos, void* buffer, int32_t count);

    /// <summary>Writes at the read/write position, advancing it.</summary>
    int32_t writeRaw(const void* buffer, int32_t count);

    /// <summary>The path the file was opened with.</summary>
    char* fileName = nullptr; // +0x04
    /// <summary>How it was opened.</summary>
    FileMode fileMode = NOMODE; // +0x08
    /// <summary>The open file on disk (shared with the parent for a child); a Win32 handle in the original.</summary>
    std::FILE* handle = nullptr; // +0x0c
    /// <summary>The FastFile the file came from while it is being read in.</summary>
    FastFile* fastFile = nullptr; // +0x10
    /// <summary>The entry's handle in <see cref="fastFile"/>.</summary>
    int32_t fastFileHandle = -1; // +0x14
    /// <summary>Cached length.</summary>
    uint32_t length = 0; // +0x18
    /// <summary>The read/write position.</summary>
    uint32_t logicalPosition = 0; // +0x1c
    /// <summary>Unused in MCX.EXE (always 0).</summary>
    int32_t unknown20 = 0; // +0x20
    /// <summary>The open child files (slots, null when free).</summary>
    File** childList = nullptr; // +0x24
    /// <summary>Number of children open.</summary>
    uint32_t numChildren = 0; // +0x28
    /// <summary>Number of child slots.</summary>
    uint32_t maxChildren = 0; // +0x2c
    /// <summary>The parent of a child file.</summary>
    File* parent = nullptr; // +0x30
    /// <summary>Where a child starts within its parent.</summary>
    uint32_t parentOffset = 0; // +0x34
    /// <summary>A child's length within its parent.</summary>
    uint32_t physicalLength = 0; // +0x38
    /// <summary>Nonzero when the file's contents are in <see cref="fileImage"/>.</summary>
    int32_t inRAM = 0; // +0x44
    /// <summary>The file's contents, for a file read whole into memory.</summary>
    uint8_t* fileImage = nullptr; // +0x48
};

/// <summary>Whether a loose file exists on disk (FastFiles aren't searched).</summary>
/// <remarks>MCX.EXE @ 0x00645340</remarks>
int fileExists(const char* fName);

/// <summary>Opens <c>filetraffic.log</c> for the traffic log, if it isn't open.</summary>
/// <remarks>MCX.EXE @ 0x006452f0</remarks>
void createTrafficLog();

/// <summary>The traffic log, when <see cref="File::logFileTraffic"/> is on.</summary>
extern File* fileTrafficLog;
