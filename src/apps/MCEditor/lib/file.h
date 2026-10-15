#pragma once

class FastFile;

// Error codes of the file layer. The values are the original's (0xBADF00xx, returned as negative longs); the names
// are the port's. Failures from Windows return the GetLastError code (FILE_NOT_FOUND = ERROR_FILE_NOT_FOUND).
inline constexpr int32_t NO_ERR = 0;
inline constexpr int32_t FILE_NOT_FOUND = 2;
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
/// The game's file: a file on disk, an entry of a FastFile (read whole into memory), or a "child" window into a
/// parent file (a packet of a <c>PacketFile</c>).
/// </summary>
/// <remarks>
/// Original source: <c>lib\file.cpp</c>, the editor's revision: a Win32 handle, and no virtual destructor. A new file's
/// handle is INVALID_HANDLE_VALUE, which <see cref="isOpen"/> counts as open (it only tests for null), and
/// <see cref="close"/> puts it back.
/// </remarks>
class File
{
public:
    /// <summary>A closed file.</summary>
    File() = default;
    /// <summary>Closes the file (File's own <see cref="close"/>, not an override).</summary>
    ~File();
    File(const File&) = delete;
    File& operator=(const File&) = delete;

    /// <summary>Whether the read position is at or past the end.</summary>
    int eof();

    /// <summary>
    /// Opens <paramref name="fName"/>: from disk, or else from the FastFiles (read whole into memory).
    /// <paramref name="numChildren"/> is how many child files it can have open at once.
    /// </summary>
    /// <returns>0, or an error code (FILE_NOT_FOUND when neither has it).</returns>
    virtual int32_t open(const char* fName, FileMode _mode = READ, int32_t numChildren = 50);

    /// <summary>
    /// Opens a window of <paramref name="length"/> bytes into <paramref name="_parent"/> from its current position.
    /// With <paramref name="numChildren"/> -1 the window's bytes are read into memory at once.
    /// </summary>
    virtual int32_t open(File* _parent, uint32_t length, int32_t numChildren = 50);

    /// <summary>Creates (truncates) <paramref name="fName"/> for writing.</summary>
    virtual int32_t create(const char* fName);

    /// <summary>Closes the file and its children.</summary>
    virtual void close();

    /// <summary>What kind of file this is.</summary>
    virtual FileClass getFileClass() { return BASEFILE; }

    /// <summary>Registers <paramref name="child"/> in the first free child slot.</summary>
    int32_t addChild(File* child);

    /// <summary>Frees <paramref name="child"/>'s slot.</summary>
    void removeChild(File* child);

    /// <summary>Closes the file if it is open and not a child (despite the name, nothing is deleted from disk).</summary>
    void deleteFile();

    /// <summary>Moves the read position (<paramref name="from"/> is SEEK_SET, SEEK_CUR or SEEK_END).</summary>
    int32_t seek(int32_t pos, int32_t from = SEEK_SET);

    /// <summary>Moves the read position to the end.</summary>
    void seekEnd();

    /// <summary>Moves the read position forward by <paramref name="bytesToSkip"/>.</summary>
    void skip(int32_t bytesToSkip);

    /// <summary>Reads <paramref name="length"/> bytes at <paramref name="pos"/> (the read position isn't advanced).</summary>
    int32_t read(uint32_t pos, uint8_t* buffer, int32_t length);

    /// <summary>Reads one byte.</summary>
    uint8_t readByte();

    /// <summary>Reads a 16-bit value.</summary>
    int16_t readWord();

    /// <summary>Reads a 16-bit value (the same as <see cref="readWord"/>; the linker merged them).</summary>
    int16_t readShort();

    /// <summary>Reads a 32-bit value.</summary>
    int32_t readLong();

    /// <summary>Reads a zero-terminated string into <paramref name="buffer"/>.</summary>
    /// <returns>Its length.</returns>
    int32_t readString(uint8_t* buffer);

    /// <summary>Reads <paramref name="length"/> bytes from the read position.</summary>
    /// <returns>The number of bytes read.</returns>
    int32_t read(uint8_t* buffer, int32_t length);

    /// <summary>
    /// Reads a line ending in CR (or CR LF) of at most <paramref name="maxLength"/> bytes, without its ending.
    /// </summary>
    /// <returns>The line's length plus one.</returns>
    int32_t readLine(uint8_t* buffer, int32_t maxLength);

    /// <summary>Reads a line up to and including its LF.</summary>
    /// <returns>The line's length plus two.</returns>
    int32_t readLineEx(uint8_t* buffer, int32_t maxLength);

    /// <summary>Writes <paramref name="length"/> bytes at <paramref name="pos"/> (the read position isn't advanced).</summary>
    int32_t write(uint32_t pos, const uint8_t* buffer, int32_t length);

    /// <summary>Writes one byte.</summary>
    int32_t writeByte(uint8_t value);

    /// <summary>Writes a 16-bit value.</summary>
    int32_t writeWord(int16_t value);

    /// <summary>Writes a 16-bit value.</summary>
    int32_t writeShort(int16_t value);

    /// <summary>Writes a 32-bit value.</summary>
    int32_t writeLong(int32_t value);

    /// <summary>Writes a 32-bit float.</summary>
    int32_t writeFloat(float value);

    /// <summary>Writes a string without its terminator.</summary>
    /// <returns>Its length, or -1.</returns>
    int32_t writeString(const char* text);

    /// <summary>Writes a string and CR LF.</summary>
    /// <returns>The string's length, or -1.</returns>
    int32_t writeLine(const char* text);

    /// <summary>Writes <paramref name="length"/> bytes at the write position.</summary>
    int32_t write(const uint8_t* buffer, int32_t length);

    /// <summary>Whether the file has a handle (see the class remarks).</summary>
    int isOpen();

    /// <summary>The path the file was opened with.</summary>
    char* getFilename() { return fileName; }

    /// <summary>The file's length in bytes.</summary>
    uint32_t getLength();

    /// <summary>The file's length in bytes (the same as <see cref="getLength"/>).</summary>
    uint32_t fileSize();

    /// <summary>The number of LF characters in the file.</summary>
    uint32_t getNumLines();

    /// <summary>The read position.</summary>
    uint32_t getLogicalPosition() const { return logicalPosition; }

    /// <summary>When nonzero, every open is logged to <c>filetraffic.log</c>. Never set in the editor.</summary>
    static int logFileTraffic;

    /// <summary>The last error a read or write ran into (they return a count, not the error).</summary>
    static int32_t lastError;

protected:
    /// <summary>The path the file was opened with (the parent's, for a child).</summary>
    char* fileName = nullptr;
    /// <summary>How it was opened.</summary>
    FileMode fileMode = NOMODE;
    /// <summary>The open file on disk (shared with the parent for a child).</summary>
    HANDLE handle = INVALID_HANDLE_VALUE;
    /// <summary>The FastFile the file came from while it is being read in.</summary>
    FastFile* fastFile = nullptr;
    /// <summary>The entry's handle in <see cref="fastFile"/>.</summary>
    int32_t fastFileHandle = -1;
    /// <summary>Cached length.</summary>
    uint32_t length = 0;
    /// <summary>The read/write position.</summary>
    uint32_t logicalPosition = 0;
    /// <summary>The open child files (slots, null when free).</summary>
    File** childList = nullptr;
    /// <summary>Number of children open.</summary>
    uint32_t numChildren = 0;
    /// <summary>Number of child slots.</summary>
    uint32_t maxChildren = 0;
    /// <summary>The parent of a child file.</summary>
    File* parent = nullptr;
    /// <summary>Where a child starts within its parent.</summary>
    uint32_t parentOffset = 0;
    /// <summary>A child's length within its parent.</summary>
    uint32_t physicalLength = 0;
    /// <summary>Nonzero when the file's contents are in <see cref="fileImage"/>.</summary>
    int32_t inRAM = 0;
    /// <summary>The file's contents, for a file read whole into memory.</summary>
    uint8_t* fileImage = nullptr;
};

/// <summary>Whether a file or folder exists on disk (FastFiles aren't searched).</summary>
int fileExists(const char* fName);

/// <summary>Creates <c>filetraffic.log</c> for the traffic log, if it isn't open.</summary>
void createTrafficLog();

/// <summary>The traffic log, when <see cref="File::logFileTraffic"/> is on.</summary>
extern File* fileTrafficLog;
