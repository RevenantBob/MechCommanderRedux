#pragma once

/// <summary>
/// One entry of a FastFile's directory, as stored in the file: where the data is, its stored and unpacked sizes and
/// its game path (<c>data\art\ACCESS00.tga</c>). <c>size == realSize</c> means stored raw, otherwise LZ-packed.
/// </summary>
/// <remarks>262 bytes on disk.</remarks>
#pragma pack(push, 1)
struct FILEENTRY
{
    /// <summary>Offset of the data from the start of the FastFile.</summary>
    int32_t offset; // +0x00
    /// <summary>Stored size in bytes.</summary>
    int32_t size; // +0x04
    /// <summary>Unpacked size in bytes.</summary>
    int32_t realSize; // +0x08
    /// <summary>The entry's game path, zero-terminated.</summary>
    char name[250]; // +0x0c
};
#pragma pack(pop)
static_assert(sizeof(FILEENTRY) == 0x106);

/// <summary>An open entry of a FastFile: whether it is open, the read position within it, and its directory entry.</summary>
struct FILE_HANDLE
{
    /// <summary>Nonzero while the entry is open.</summary>
    int32_t inuse; // +0x00
    /// <summary>Read position within the entry's unpacked data.</summary>
    int32_t pos; // +0x04
    /// <summary>The entry's directory record.</summary>
    FILEENTRY* file; // +0x08
};

/// <summary>
/// A FastFile (<c>.FST</c>): an archive of game files, each stored raw or LZ-packed, that <c>File::open</c> falls back
/// to when a path isn't found on disk. The game opens the ones listed in SYSTEM.CFG's [FastFiles] at startup.
/// </summary>
/// <remarks>Original source: <c>lib\ffile.cpp</c>. 0x18 bytes in the original.</remarks>
class FastFile
{
public:
    /// <remarks>MCX.EXE @ 0x00644d50</remarks>
    FastFile();
    /// <remarks>MCX.EXE @ 0x00644d70</remarks>
    ~FastFile();
    FastFile(const FastFile&) = delete;
    FastFile& operator=(const FastFile&) = delete;

    /// <summary>Opens the archive at <paramref name="fName"/> and reads its directory.</summary>
    /// <returns>0, or the reason it failed.</returns>
    /// <remarks>MCX.EXE @ 0x00644d80</remarks>
    int32_t open(const char* fName);

    /// <summary>Closes the archive and frees its directory.</summary>
    /// <remarks>MCX.EXE @ 0x00644ed0</remarks>
    void close();

    /// <summary>Opens the entry named <paramref name="fName"/> (case ignored).</summary>
    /// <returns>The entry's handle, or -1 when the archive doesn't hold it.</returns>
    /// <remarks>MCX.EXE @ 0x00644f40</remarks>
    int32_t openFast(const char* fName);

    /// <summary>Closes an entry opened with <see cref="openFast"/>.</summary>
    /// <remarks>MCX.EXE @ 0x00644fb0</remarks>
    void closeFast(int32_t fastFileHandle);

    /// <summary>Moves the read position within an entry (<paramref name="from"/> is SEEK_SET, SEEK_CUR or SEEK_END).</summary>
    /// <returns>The new position, or an error code.</returns>
    /// <remarks>MCX.EXE @ 0x00644fe0</remarks>
    int32_t seekFast(int32_t fastFileHandle, int32_t off, int32_t from);

    /// <summary>
    /// Reads an entry into <paramref name="bfr"/>. A packed entry is always unpacked whole, from its start, whatever
    /// the read position and <paramref name="size"/>.
    /// </summary>
    /// <returns>The number of bytes read, or 0 when a packed entry didn't unpack to its full size.</returns>
    /// <remarks>MCX.EXE @ 0x006450f0</remarks>
    int32_t readFast(int32_t fastFileHandle, void* bfr, int32_t size);

    /// <summary>The read position within an entry, or -1.</summary>
    /// <remarks>MCX.EXE @ 0x00645260</remarks>
    int32_t tellFast(int32_t fastFileHandle);

    /// <summary>An entry's unpacked size, or -1.</summary>
    /// <remarks>MCX.EXE @ 0x00645290</remarks>
    int32_t sizeFast(int32_t fastFileHandle);

    /// <summary>An entry's stored (packed) size, or -1.</summary>
    /// <remarks>MCX.EXE @ 0x006452c0</remarks>
    int32_t lzSizeFast(int32_t fastFileHandle);

    /// <summary>The number of entries in the archive.</summary>
    int32_t getNumFiles() const { return numFiles; }

    /// <summary>The directory record of entry <paramref name="index"/> (for the tools; the game goes by name).</summary>
    const FILEENTRY* getEntry(int32_t index) const
    {
        return index >= 0 && index < numFiles ? files[index].file : nullptr;
    }

protected:
    /// <summary>Number of entries.</summary>
    int32_t numFiles = 0; // +0x00
    /// <summary>One handle per entry.</summary>
    FILE_HANDLE* files = nullptr; // +0x04
    /// <summary>The archive's path.</summary>
    char* fileName = nullptr; // +0x08
    /// <summary>The open archive (a Win32 handle in the original).</summary>
    std::FILE* handle = nullptr; // +0x0c
    /// <summary>The archive's length in bytes.</summary>
    int32_t length = 0; // +0x10
    /// <summary>The archive file's current position.</summary>
    int32_t logicalPosition = 0; // +0x14
};

/// <summary>The scratch buffer packed entries are read into before unpacking; grows to the largest one.</summary>
extern uint8_t* LZPacketBuffer;
/// <summary>The size of <see cref="LZPacketBuffer"/>.</summary>
extern uint32_t LZPacketBufferSize;
