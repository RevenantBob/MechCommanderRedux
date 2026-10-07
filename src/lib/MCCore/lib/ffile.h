#pragma once

/// <summary>
/// One entry of a FastFile's directory, as stored in the file: where the data is, its stored and unpacked sizes and
/// its game path (<c>data\art\ACCESS00.tga</c>). <c>size == realSize</c> means stored raw, otherwise LZ-packed.
/// </summary>
/// <remarks>262 bytes on disk.</remarks>
#pragma pack(push, 1)
struct MCFileEntry
{
    /// <summary>Offset of the data from the start of the FastFile.</summary>
    int32_t Offset;
    /// <summary>Stored size in bytes.</summary>
    int32_t Size;
    /// <summary>Unpacked size in bytes.</summary>
    int32_t RealSize;
    /// <summary>The entry's game path, zero-terminated.</summary>
    char Name[250];
};
#pragma pack(pop)
static_assert(sizeof(MCFileEntry) == 0x106);

/// <summary>An open entry of a FastFile: whether it is open, the read position within it, and its directory entry.</summary>
struct MCFileHandle
{
    /// <summary>Nonzero while the entry is open.</summary>
    int32_t Inuse;
    /// <summary>Read position within the entry's unpacked data.</summary>
    int32_t Pos;
    /// <summary>The entry's directory record.</summary>
    MCFileEntry* File;
};

/// <summary>
/// A FastFile (<c>.FST</c>): an archive of game files, each stored raw or LZ-packed, that <c>File::open</c> falls back
/// to when a path isn't found on disk. The game opens the ones listed in SYSTEM.CFG's [FastFiles] at startup.
/// </summary>
/// <remarks>Original source: <c>lib\ffile.cpp</c>. 0x18 bytes in the original.</remarks>
class MCFastFile
{
public:
    MCFastFile();
    ~MCFastFile();
    MCFastFile(const MCFastFile&) = delete;
    MCFastFile& operator=(const MCFastFile&) = delete;

    /// <summary>Opens the archive at <paramref name="fName"/> and reads its directory.</summary>
    /// <returns>0, or the reason it failed.</returns>
    int32_t Open(const char* fName);

    /// <summary>Closes the archive and frees its directory.</summary>
    void Close();

    /// <summary>Opens the entry named <paramref name="fName"/> (case ignored).</summary>
    /// <returns>The entry's handle, or -1 when the archive doesn't hold it.</returns>
    int32_t OpenFast(const char* fName);

    /// <summary>Closes an entry opened with <see cref="OpenFast"/>.</summary>
    void CloseFast(int32_t fastFileHandle);

    /// <summary>Moves the read position within an entry (<paramref name="from"/> is SEEK_SET, SEEK_CUR or SEEK_END).</summary>
    /// <returns>The new position, or an error code.</returns>
    int32_t SeekFast(int32_t fastFileHandle, int32_t off, int32_t from);

    /// <summary>
    /// Reads an entry into <paramref name="bfr"/>. A packed entry is always unpacked whole, from its start, whatever
    /// the read position and <paramref name="size"/>.
    /// </summary>
    /// <returns>The number of bytes read, or 0 when a packed entry didn't unpack to its full size.</returns>
    int32_t ReadFast(int32_t fastFileHandle, void* bfr, int32_t size);

    /// <summary>The read position within an entry, or -1.</summary>
    int32_t TellFast(int32_t fastFileHandle);

    /// <summary>An entry's unpacked size, or -1.</summary>
    int32_t SizeFast(int32_t fastFileHandle);

    /// <summary>An entry's stored (packed) size, or -1.</summary>
    int32_t LzSizeFast(int32_t fastFileHandle);

    /// <summary>The number of entries in the archive.</summary>
    int32_t GetNumFiles() const { return _NumFiles; }

    /// <summary>The directory record of entry <paramref name="index"/> (for the tools; the game goes by name).</summary>
    const MCFileEntry* GetEntry(int32_t index) const
    {
        return index >= 0 && index < _NumFiles ? _Files[index].File : nullptr;
    }

protected:
    /// <summary>Number of entries.</summary>
    int32_t _NumFiles = 0;
    /// <summary>One handle per entry.</summary>
    MCFileHandle* _Files = nullptr;
    /// <summary>The archive's path.</summary>
    char* _FileName = nullptr;
    /// <summary>The open archive (a Win32 handle in the original).</summary>
    std::FILE* _Handle = nullptr;
    /// <summary>The archive's length in bytes.</summary>
    int32_t _Length = 0;
    /// <summary>The archive file's current position.</summary>
    int32_t _LogicalPosition = 0;
};

/// <summary>The scratch buffer packed entries are read into before unpacking; grows to the largest one.</summary>
extern uint8_t* LZPacketBuffer;
/// <summary>The size of <see cref="LZPacketBuffer"/>.</summary>
extern uint32_t LZPacketBufferSize;
