#pragma once

/// <summary>
/// One entry of a FastFile's directory, as stored in the file: where the data is, its stored and unpacked sizes and
/// its game path. <c>size == realSize</c> means stored raw, otherwise LZ-packed.
/// </summary>
#pragma pack(push, 1)
struct FILEENTRY
{
    /// <summary>Offset of the data from the start of the FastFile.</summary>
    int32_t offset;
    /// <summary>Stored size in bytes.</summary>
    int32_t size;
    /// <summary>Unpacked size in bytes.</summary>
    int32_t realSize;
    /// <summary>The entry's game path, zero-terminated.</summary>
    char name[250];
};
#pragma pack(pop)
static_assert(sizeof(FILEENTRY) == 0x106);

/// <summary>An open entry of a FastFile: whether it is open, the read position within it, and its directory entry.</summary>
struct FILE_HANDLE
{
    /// <summary>Nonzero while the entry is open.</summary>
    int32_t inuse;
    /// <summary>Read position within the entry.</summary>
    int32_t pos;
    /// <summary>The entry's directory record.</summary>
    FILEENTRY* file;
};

/// <summary>
/// A FastFile (<c>.FST</c>): an archive of game files, each stored raw or LZ-packed, that <c>File::open</c> falls back
/// to when a path isn't found on disk.
/// </summary>
/// <remarks>
/// Original source: <c>lib\ffile.cpp</c>, the editor's revision (a Win32 file handle, entries opened by index or name,
/// and a destructor that frees the shared unpack buffer).
/// </remarks>
class FastFile
{
public:
    /// <summary>An empty archive.</summary>
    FastFile() = default;
    /// <summary>Closes the archive and frees <see cref="LZPacketBuffer"/>.</summary>
    ~FastFile();
    FastFile(const FastFile&) = delete;
    FastFile& operator=(const FastFile&) = delete;

    /// <summary>Opens the archive at <paramref name="fName"/> and reads its directory.</summary>
    /// <returns>0, or the reason it failed (a Win32 error code or NO_RAM_FOR_FILENAME).</returns>
    int32_t open(const char* fName);

    /// <summary>Closes the archive and frees its directory.</summary>
    void close();

    /// <summary>Opens entry number <paramref name="fileIndex"/>.</summary>
    /// <returns>The entry's handle (its index), or -1 when there is no such entry.</returns>
    int32_t openFastByIndex(int32_t fileIndex);

    /// <summary>Opens the entry named <paramref name="fName"/> (case ignored).</summary>
    /// <returns>The entry's handle, or -1 when the archive doesn't hold it.</returns>
    int32_t openFast(const char* fName);

    /// <summary>Closes an entry opened with <see cref="openFast"/>.</summary>
    void closeFast(int32_t fastFileHandle);

    /// <summary>Moves the read position within an entry (<paramref name="from"/> is SEEK_SET, SEEK_CUR or SEEK_END).</summary>
    /// <returns>The new position, or an error code.</returns>
    int32_t seekFast(int32_t fastFileHandle, int32_t off, int32_t from);

    /// <summary>
    /// Reads an entry into <paramref name="bfr"/>. A packed entry is always unpacked whole, from its start, whatever
    /// the read position and <paramref name="size"/>.
    /// </summary>
    /// <returns>The number of bytes read, 0 when a packed entry didn't unpack to its full size, or an error code.</returns>
    int32_t readFast(int32_t fastFileHandle, void* bfr, int32_t size);

    /// <summary>The read position within an entry, or -1.</summary>
    int32_t tellFast(int32_t fastFileHandle);

    /// <summary>An entry's unpacked size, or -1.</summary>
    int32_t sizeFast(int32_t fastFileHandle);

    /// <summary>An entry's stored (packed) size, or -1.</summary>
    int32_t lzSizeFast(int32_t fastFileHandle);

    /// <summary>An open entry's game path, or null.</summary>
    const char* getEntryName(int32_t fastFileHandle);

protected:
    /// <summary>Number of entries.</summary>
    int32_t numFiles = 0;
    /// <summary>One handle per entry.</summary>
    FILE_HANDLE* files = nullptr;
    /// <summary>The archive's path.</summary>
    char* fileName = nullptr;
    /// <summary>The open archive.</summary>
    HANDLE handle = INVALID_HANDLE_VALUE;
    /// <summary>The archive's length in bytes.</summary>
    int32_t length = 0;
    /// <summary>The archive file's current position.</summary>
    int32_t logicalPosition = 0;
};

/// <summary>The scratch buffer packed entries and packets are read into before unpacking; grows to the largest one.</summary>
extern uint8_t* LZPacketBuffer;
/// <summary>The size of <see cref="LZPacketBuffer"/>.</summary>
extern uint32_t LZPacketBufferSize;
