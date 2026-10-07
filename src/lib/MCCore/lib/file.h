#pragma once

class MCFastFile;

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

/// <summary>How a <see cref="MCFile"/> is opened.</summary>
enum MCFileMode
{
    NOMODE = 0,
    READ = 1,
    CREATE = 2,
    MC2_APPEND = 3,
    WRITE = 4,
    RDWRITE = 5
};

/// <summary>What kind of <see cref="MCFile"/> an object is (<see cref="MCFile::GetFileClass"/>).</summary>
enum MCFileClass
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
class MCFile
{
public:
    MCFile();
    virtual ~MCFile();
    MCFile(const MCFile&) = delete;
    MCFile& operator=(const MCFile&) = delete;

    /// <summary>Whether the read position is at or past the end.</summary>
    int Eof();

    /// <summary>
    /// Opens <paramref name="fName"/>: from disk, or else from the FastFiles (read whole into memory).
    /// <paramref name="numChildren"/> is how many child files it can have open at once.
    /// </summary>
    /// <returns>0, or an error code (FILE_NOT_FOUND when neither has it).</returns>
    virtual int32_t Open(const char* fName, MCFileMode mode = READ, int32_t numChildren = 50);

    /// <summary>
    /// Opens a window of <paramref name="length"/> bytes into <paramref name="parent"/> from its current position.
    /// With <paramref name="numChildren"/> -1 the window's bytes are read into memory at once.
    /// </summary>
    virtual int32_t Open(MCFile* parent, uint32_t length, int32_t numChildren = 50);

    /// <summary>Creates (truncates) <paramref name="fName"/> for writing.</summary>
    virtual int32_t Create(const char* fName);

    /// <summary>Closes the file and its children.</summary>
    virtual void Close();

    /// <summary>What kind of file this is.</summary>
    virtual MCFileClass GetFileClass() { return BASEFILE; }

    /// <summary>Registers <paramref name="child"/> in the first free child slot.</summary>
    int32_t AddChild(MCFile* child);

    /// <summary>Frees <paramref name="child"/>'s slot.</summary>
    void RemoveChild(MCFile* child);

    /// <summary>Closes the file if it is open and not a child (despite the name, nothing is deleted from disk).</summary>
    void DeleteFile();

    /// <summary>Moves the read position (<paramref name="from"/> is SEEK_SET, SEEK_CUR or SEEK_END).</summary>
    int32_t Seek(int32_t pos, int32_t from = SEEK_SET);

    /// <summary>Reads <paramref name="length"/> bytes at <paramref name="pos"/> (the read position isn't advanced).</summary>
    int32_t Read(uint32_t pos, uint8_t* buffer, int32_t length);

    /// <summary>Reads one byte.</summary>
    uint8_t ReadByte();

    /// <summary>Reads a 16-bit value.</summary>
    int16_t ReadWord();

    /// <summary>Reads a 16-bit value (the same as <see cref="ReadWord"/>).</summary>
    int16_t ReadShort();

    /// <summary>Reads a 32-bit value.</summary>
    int32_t ReadLong();

    /// <summary>Reads a 32-bit float.</summary>
    /// <remarks>The linker merged it with <see cref="ReadLong"/>.</remarks>
    float ReadFloat();

    /// <summary>Reads a zero-terminated string into <paramref name="buffer"/>.</summary>
    /// <returns>Its length.</returns>
    int32_t ReadString(uint8_t* buffer);

    /// <summary>Reads <paramref name="length"/> bytes from the read position.</summary>
    /// <returns>The number of bytes read.</returns>
    int32_t Read(uint8_t* buffer, int32_t length);

    /// <summary>
    /// Reads a line ending in CR (or CR LF) of at most <paramref name="maxLength"/> bytes, without its ending.
    /// </summary>
    /// <returns>The line's length plus one.</returns>
    int32_t ReadLine(uint8_t* buffer, int32_t maxLength);

    /// <summary>Reads a line up to and including its LF.</summary>
    /// <returns>The line's length plus two.</returns>
    int32_t ReadLineEx(uint8_t* buffer, int32_t maxLength);

    /// <summary>Writes <paramref name="length"/> bytes at <paramref name="pos"/>.</summary>
    int32_t Write(uint32_t pos, const uint8_t* buffer, int32_t length);

    /// <summary>Writes one byte.</summary>
    int32_t WriteByte(uint8_t value);

    /// <summary>Writes a 16-bit value.</summary>
    int32_t WriteWord(int16_t value);

    /// <summary>Writes a 16-bit value.</summary>
    int32_t WriteShort(int16_t value);

    /// <summary>Writes a 32-bit value.</summary>
    int32_t WriteLong(int32_t value);

    /// <summary>Writes a 32-bit float.</summary>
    int32_t WriteFloat(float value);

    /// <summary>Writes a string without its terminator.</summary>
    int32_t WriteString(const char* text);

    /// <summary>Writes a string and CR LF.</summary>
    int32_t WriteLine(const char* text);

    /// <summary>Writes <paramref name="length"/> bytes at the write position.</summary>
    int32_t Write(const uint8_t* buffer, int32_t length);

    /// <summary>Whether the file is open (on disk or in memory).</summary>
    int IsOpen();

    /// <summary>The path the file was opened with.</summary>
    char* GetFilename() { return _FileName; }

    /// <summary>The file's length in bytes.</summary>
    uint32_t GetLength();

    /// <summary>The file's length in bytes (the same as <see cref="GetLength"/>).</summary>
    uint32_t FileSize();

    /// <summary>The number of LF characters in the file.</summary>
    uint32_t GetNumLines();

    /// <summary>Moves the read position forward by <paramref name="bytesToSkip"/>.</summary>
    void Skip(int32_t bytesToSkip);

    /// <summary>The read position.</summary>
    uint32_t GetLogicalPosition() const { return _LogicalPosition; }

    /// <summary>The parent file of a child file, or null.</summary>
    MCFile* GetParent() const { return _Parent; }

    /// <summary>The in-memory image of a file read whole (a FastFile entry, a child read at once), or null.</summary>
    uint8_t* GetFileImage() const { return _InRam ? _FileImage : nullptr; }

    /// <summary>When nonzero, every open is logged to <c>filetraffic.log</c>.</summary>
    static int LogFileTraffic;

protected:
    /// <summary>
    /// Reads up to <paramref name="count"/> bytes at <paramref name="pos"/> from wherever the file lives (its memory
    /// image, a FastFile entry, the disk, or its parent's bytes for a child) without moving the read position. The
    /// port's single read path; the original repeated it in every read method.
    /// </summary>
    int32_t ReadRawAt(uint32_t pos, void* buffer, int32_t count);

    /// <summary>Writes at the read/write position, advancing it.</summary>
    int32_t WriteRaw(const void* buffer, int32_t count);

    /// <summary>The path the file was opened with.</summary>
    char* _FileName = nullptr;
    /// <summary>How it was opened.</summary>
    MCFileMode _FileMode = NOMODE;
    /// <summary>The open file on disk (shared with the parent for a child); a Win32 handle in the original.</summary>
    std::FILE* _Handle = nullptr;
    /// <summary>The FastFile the file came from while it is being read in.</summary>
    MCFastFile* _FastFile = nullptr;
    /// <summary>The entry's handle in <see cref="_FastFile"/>.</summary>
    int32_t _FastFileHandle = -1;
    /// <summary>Cached length.</summary>
    uint32_t _Length = 0;
    /// <summary>The read/write position.</summary>
    uint32_t _LogicalPosition = 0;
    /// <summary>The open child files (slots, null when free).</summary>
    MCFile** _ChildList = nullptr;
    /// <summary>Number of children open.</summary>
    uint32_t _NumChildren = 0;
    /// <summary>Number of child slots.</summary>
    uint32_t _MaxChildren = 0;
    /// <summary>The parent of a child file.</summary>
    MCFile* _Parent = nullptr;
    /// <summary>Where a child starts within its parent.</summary>
    uint32_t _ParentOffset = 0;
    /// <summary>A child's length within its parent.</summary>
    uint32_t _PhysicalLength = 0;
    /// <summary>Nonzero when the file's contents are in <see cref="_FileImage"/>.</summary>
    int32_t _InRam = 0;
    /// <summary>The file's contents, for a file read whole into memory.</summary>
    uint8_t* _FileImage = nullptr;
};

/// <summary>Whether a loose file exists on disk (FastFiles aren't searched).</summary>
int FileExists(const char* fName);

/// <summary>Opens <c>filetraffic.log</c> for the traffic log, if it isn't open.</summary>
void CreateTrafficLog();

/// <summary>The traffic log, when <see cref="MCFile::LogFileTraffic"/> is on.</summary>
extern MCFile* FileTrafficLog;
