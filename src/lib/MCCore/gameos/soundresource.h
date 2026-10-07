#pragma once

#include "platform/MCWin32Defs.h"

class MCFile;
class MCSoundResource;

/// <summary>Where a sound resource's samples come from.</summary>
enum MCSoundResourceType : int32_t
{
    /// <summary>A wave file, read whole into memory. The enumerator names are the port's.</summary>
    SOUND_RESOURCE_FILE = 0,
    /// <summary>A wave file image the caller already has in memory (not copied).</summary>
    SOUND_RESOURCE_MEMORY = 1,
    /// <summary>A wave file streamed from disk as it plays (music).</summary>
    SOUND_RESOURCE_STREAM = 2,
};

/// <summary>A link of <see cref="MCSRLinkedList"/>.</summary>
/// <remarks>Original source: <c>game os\sound renderer\sound resource.cpp</c>; 0x10 bytes.</remarks>
class MCSRLink
{
public:
    /// <summary>Unlinks (clears next and previous).</summary>
    virtual ~MCSRLink()
    {
        Next = nullptr;
        Prev = nullptr;
    }

    /// <summary>The next link.</summary>
    MCSRLink* Next = nullptr;
    /// <summary>The previous link.</summary>
    MCSRLink* Prev = nullptr;
    /// <summary>The resource.</summary>
    MCSoundResource* Data = nullptr;
};

/// <summary>
/// The sound renderer's list of every live <see cref="MCSoundResource"/> (<c>SRLinkedList&lt;SoundResource*&gt;</c>
/// in the original, a template whose methods were all inlined). Resources add themselves when made and remove
/// themselves when destroyed.
/// </summary>
/// <remarks>Original source: <c>game os\sound renderer\sound resource.cpp</c>; 8 bytes. Its static constructor
/// (0x00758140, in <c>sound renderer.cpp</c>) only zeroes it.</remarks>
struct MCSRLinkedList
{
    /// <summary>The first link.</summary>
    MCSRLink* Head = nullptr;
    /// <summary>How many links there are.</summary>
    int32_t Count = 0;

    /// <summary>The static destructor (registered with atexit): frees every link, not the resources.</summary>
    ~MCSRLinkedList();
};

/// <summary>Every sound resource there is.</summary>
extern MCSRLinkedList MSoundResources;
/// <summary>Guards the resources against the streaming timer thread.</summary>
extern std::recursive_mutex SoundCritSec;
/// <summary>How deep SoundResource::Read has recursed (it wraps a looping stream); Fatal above 3.</summary>
extern int32_t ReadEntries;

/// <summary>Makes a sound resource of <paramref name="type"/> from a file name or a wave image.</summary>
void GosCreateSoundResource(void** resource, const char* source, MCSoundResourceType type, uint32_t flags);
/// <summary>Stops every channel playing the resource, then destroys it (if it is in the list).</summary>
void GosDestroySoundResource(void* resource);

/// <summary>
/// A wave: a file loaded whole, a caller's memory image, or a file streamed from disk. Knows its format, where
/// its samples are, and for a stream how to read and rewind them.
/// </summary>
/// <remarks>Original source: <c>game os\sound renderer\sound resource.cpp</c>; 0x40 bytes.</remarks>
class MCSoundResource
{
public:
    /// <summary>Loads the file, parses the memory image, or opens the stream; then adds itself to
    /// <see cref="MSoundResources"/>.</summary>
    MCSoundResource(const char* source, MCSoundResourceType type, uint32_t flags);
    /// <summary>Frees the name, closes the stream, and unlinks itself.</summary>
    ~MCSoundResource();

    /// <summary>Reads the whole file into memory and finds its format and data. Fatal when it can't open it.
    /// </summary>
    void LoadFile();
    /// <summary>Walks a RIFF WAVE image's chunks for "fmt " and "data". Fatal when it isn't one.</summary>
    void GetWaveInfo(uint8_t* image, tWAVEFORMATEX** format, uint8_t** data, uint32_t* dataSize);
    /// <summary>
    /// Reads up to <paramref name="bytes"/> of stream; at the end, a looping stream rewinds and reads on, otherwise
    /// the rest is filled with silence.
    /// </summary>
    /// <returns><paramref name="bytes"/>.</returns>
    uint32_t Read(uint8_t* buffer, uint32_t bytes, bool loop);
    /// <summary>Frees the format and closes the stream's file.</summary>
    void CloseStream();
    /// <summary>Seeks the stream back to its data.</summary>
    void Rewind();
    /// <summary>
    /// Opens the file as a stream: checks RIFF/WAVE and PCM, reads the format, finds the data chunk, and works out
    /// the duration. Fatal on a bad file.
    /// </summary>
    void Open();
    /// <summary>Finds the format and data in the caller's image.</summary>
    void OpenFromMemory();

    /// <summary>Where the samples come from.</summary>
    MCSoundResourceType Type = SOUND_RESOURCE_FILE;
    /// <summary>The file name (malloc) for a file or stream.</summary>
    char* FileName = nullptr;
    /// <summary>The whole wave image: read by LoadFile (malloc), or the caller's.</summary>
    uint8_t* FileImage = nullptr;
    /// <summary>The data chunk in the image.</summary>
    uint8_t* WaveData = nullptr;
    /// <summary>The file's size (LoadFile).</summary>
    uint32_t FileSize = 0;
    /// <summary>The data chunk's size.</summary>
    uint32_t WaveSize = 0;
    /// <summary>The flags given at creation; not read.</summary>
    uint32_t Flags = 0;
    /// <summary>The format: in the image, or (a stream) a malloc'd copy.</summary>
    tWAVEFORMATEX* Format = nullptr;
    /// <summary>A stream's open file.</summary>
    MCFile* Stream = nullptr;
    /// <summary>A stream's data chunk position in the file.</summary>
    int32_t DataStart = 0;
    /// <summary>A stream's length in ms.</summary>
    uint32_t DurationMs = 0;
    /// <summary>A stream's data size, rounded down to whole sample frames.</summary>
    uint32_t DataSize = 0;
    /// <summary>Zeroed by the constructor and Rewind; not read.</summary>
    uint32_t StreamPos = 0;
};
