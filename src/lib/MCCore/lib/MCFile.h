#pragma once

// Result codes of the file layer. The values are the original's (0xBADF00xx, returned as negative longs); the names
// are the port's, since the original's weren't kept. Open failures from the OS return small positive codes as
// GetLastError did (FILE_NOT_FOUND = 2). They stay int32_t constants while the game's int32_t result chains carry them.
inline constexpr int32_t NO_ERR = 0;
inline constexpr int32_t FILE_NOT_FOUND = 2;
inline constexpr int32_t READ_ERR = 30;
inline constexpr int32_t READ_PAST_EOF = static_cast<int32_t>(0xBADF0008);
inline constexpr int32_t INVALID_SEEK = static_cast<int32_t>(0xBADF0009);
inline constexpr int32_t WRITE_ERR = static_cast<int32_t>(0xBADF000A);
inline constexpr int32_t PARENT_NULL = static_cast<int32_t>(0xBADF000E);
inline constexpr int32_t TOO_MANY_CHILDREN = static_cast<int32_t>(0xBADF000F);
inline constexpr int32_t CANT_WRITE_TO_CHILD = static_cast<int32_t>(0xBADF0011);

/// <summary>How a <see cref="MCFile"/> is opened.</summary>
/// <remarks>The original also had append, write and read-write modes; nothing in the game opens a file with them.</remarks>
enum class MCFileMode : uint8_t
{
    /// <summary>Read an existing file (from the disk, a test's memory source or the FastFiles).</summary>
    Read,
    /// <summary>Create (truncate) a file in the user folder and write it.</summary>
    Create
};

/// <summary>What kind of <see cref="MCFile"/> an object is (<see cref="MCFile::GetFileClass"/>).</summary>
enum class MCFileClass : uint8_t
{
    Base,
    Ini,
    Packet
};

/// <summary>
/// The game's file: a file on disk, a file read whole into memory (a FastFile entry, a test's in-memory file), or a
/// "child" window into a parent file (a packet of a <c>PacketFile</c>). Every read of game data goes through it.
/// </summary>
/// <remarks>
/// Original source: <c>lib\file.cpp</c>. The original wrapped a Win32 handle; the port a <c>std::FILE</c>, with paths
/// resolved against the game install by <c>MCFileSystem</c>. A loose file on disk wins over a FastFile entry of the
/// same name, as in the original. The file closes when it is destroyed.
/// </remarks>
class MCFile
{
public:
    MCFile();
    virtual ~MCFile();
    MCFile(const MCFile&) = delete;
    MCFile& operator=(const MCFile&) = delete;

    /// <summary>Whether the read position is at or past the end.</summary>
    bool Eof();

    /// <summary>Opens <paramref name="fileName"/>: from disk, or else from memory or the FastFiles (read whole).</summary>
    /// <returns>0, or an error code (FILE_NOT_FOUND when nothing has it).</returns>
    virtual int32_t Open(std::string_view fileName, MCFileMode mode = MCFileMode::Read);

    /// <summary>Opens a window of <paramref name="length"/> bytes into <paramref name="parent"/> from its read position.</summary>
    /// <returns>0, or PARENT_NULL, CANT_WRITE_TO_CHILD or TOO_MANY_CHILDREN (a parent read whole takes no children).</returns>
    virtual int32_t Open(MCFile* parent, uint32_t length);

    /// <summary>Creates (truncates) <paramref name="fileName"/> for writing.</summary>
    virtual int32_t Create(std::string_view fileName);

    /// <summary>Closes the file and its children.</summary>
    virtual void Close();

    /// <summary>What kind of file this is.</summary>
    virtual MCFileClass GetFileClass() const { return MCFileClass::Base; }

    /// <summary>Closes the file if it is open and not a child (despite the name, nothing is deleted from disk).</summary>
    void DeleteFile();

    /// <summary>Moves the read position (<paramref name="from"/> is SEEK_SET, SEEK_CUR or SEEK_END).</summary>
    int32_t Seek(int32_t pos, int32_t from = SEEK_SET);

    /// <summary>Reads one byte.</summary>
    uint8_t ReadByte();

    /// <summary>Reads a 16-bit value.</summary>
    int16_t ReadWord();

    /// <summary>Reads a 16-bit value (the same as <see cref="ReadWord"/>).</summary>
    int16_t ReadShort();

    /// <summary>Reads a 32-bit value.</summary>
    int32_t ReadLong();

    /// <summary>Reads a 32-bit float.</summary>
    float ReadFloat();

    /// <summary>Reads into <paramref name="buffer"/> from the read position.</summary>
    /// <returns>The number of bytes read.</returns>
    int32_t Read(std::span<uint8_t> buffer);

    /// <summary>Reads <paramref name="length"/> bytes into <paramref name="buffer"/> (see the span overload).</summary>
    int32_t Read(uint8_t* buffer, int32_t length)
    {
        return Read(std::span(buffer, static_cast<size_t>(std::max(length, 0))));
    }

    /// <summary>
    /// Reads a line ending in CR (or CR LF, or LF), looking at no more than <paramref name="maxLength"/> bytes, and
    /// moves past its ending.
    /// </summary>
    /// <remarks>
    /// Original behaviour (OB-135): a line longer than <paramref name="maxLength"/> is cut there, and the byte after
    /// the cut is skipped.
    /// </remarks>
    std::string ReadLine(int32_t maxLength);

    /// <summary>
    /// <see cref="ReadLine(int32_t)"/> into <paramref name="buffer"/> (<paramref name="maxLength"/> + 1 bytes), zero-
    /// terminated.
    /// </summary>
    /// <returns>The line's length plus one.</returns>
    int32_t ReadLine(uint8_t* buffer, int32_t maxLength);

    /// <summary>Reads a line up to and including its LF.</summary>
    /// <returns>The line's length plus two.</returns>
    int32_t ReadLineEx(uint8_t* buffer, int32_t maxLength);

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

    /// <summary>Writes a string without a terminator.</summary>
    /// <returns>Its length, or -1 when the file can't be written.</returns>
    int32_t WriteString(std::string_view text);

    /// <summary>Writes a string and CR LF.</summary>
    /// <returns>The string's length, or -1 when the file can't be written.</returns>
    int32_t WriteLine(std::string_view text);

    /// <summary>Writes <paramref name="data"/> at the write position.</summary>
    /// <returns>The number of bytes written.</returns>
    int32_t Write(std::span<const uint8_t> data);

    /// <summary>Writes <paramref name="length"/> bytes of <paramref name="buffer"/> (see the span overload).</summary>
    int32_t Write(const uint8_t* buffer, int32_t length)
    {
        return Write(std::span(buffer, static_cast<size_t>(std::max(length, 0))));
    }

    /// <summary>Whether the file is open (on disk, through its parent's disk file, or in memory).</summary>
    bool IsOpen() const;

    /// <summary>The path the file was opened with (a child has its parent's).</summary>
    const std::string& GetFilename() const { return _FileName; }

    /// <summary>The file's length in bytes.</summary>
    uint32_t GetLength();

    /// <summary>The file's length in bytes (the same as <see cref="GetLength"/>).</summary>
    uint32_t FileSize() { return GetLength(); }

    /// <summary>Moves the read position forward by <paramref name="bytesToSkip"/>.</summary>
    void Skip(int32_t bytesToSkip);

    /// <summary>The read position.</summary>
    uint32_t GetLogicalPosition() const { return _LogicalPosition; }

    /// <summary>The parent file of a child file, or null.</summary>
    MCFile* GetParent() const { return _Parent; }

protected:
    /// <summary>
    /// Opens a child window (see the public overload). With <paramref name="readWhole"/> its bytes are read into
    /// memory at once, and it takes no children of its own.
    /// </summary>
    int32_t OpenChild(MCFile* parent, uint32_t length, bool readWhole);

    /// <summary>
    /// Reads up to <paramref name="buffer"/>'s size at <paramref name="pos"/> from wherever the file lives (its memory
    /// image, the disk, or its parent's image for a child) without moving the read position.
    /// </summary>
    int32_t ReadRawAt(uint32_t pos, std::span<uint8_t> buffer);

    /// <summary>Writes at the read/write position, advancing it.</summary>
    int32_t WriteRaw(std::span<const uint8_t> data);

    /// <summary>The disk file this file reads: its own, or its parent's for a child; null for a file in memory.</summary>
    std::FILE* DiskHandle() const;

    /// <summary>Closes a disk file.</summary>
    struct FileCloser
    {
        void operator()(std::FILE* file) const { std::fclose(file); }
    };

    /// <summary>The path the file was opened with.</summary>
    std::string _FileName;
    /// <summary>How it was opened.</summary>
    MCFileMode _FileMode = MCFileMode::Read;
    /// <summary>The open file on disk; a child reads its parent's.</summary>
    std::unique_ptr<std::FILE, FileCloser> _Disk;
    /// <summary>Cached length.</summary>
    uint32_t _Length = 0;
    /// <summary>The read/write position.</summary>
    uint32_t _LogicalPosition = 0;
    /// <summary>The open child files.</summary>
    std::vector<MCFile*> _Children;
    /// <summary>
    /// Whether children can be opened on this file: a file opened from disk or a child window. A file read whole
    /// into memory takes none.
    /// </summary>
    bool _TakesChildren = false;
    /// <summary>The parent of a child file.</summary>
    MCFile* _Parent = nullptr;
    /// <summary>Where a child starts within its parent.</summary>
    uint32_t _ParentOffset = 0;
    /// <summary>A child's length within its parent.</summary>
    uint32_t _PhysicalLength = 0;
    /// <summary>Whether the file's contents are in <see cref="_FileImage"/>.</summary>
    bool _InRam = false;
    /// <summary>The file's contents, for a file read whole into memory.</summary>
    std::vector<uint8_t> _FileImage;
};

/// <summary>Whether a loose file exists on disk (FastFiles aren't searched).</summary>
bool FileExists(std::string_view fileName);

/// <summary>
/// A game path built from a directory (with its trailing <c>\</c>), a name and an extension: <c>data\art\</c> +
/// <c>access00</c> + <c>.tga</c>.
/// </summary>
std::string GamePath(std::string_view directory, std::string_view name, std::string_view extension = {});
