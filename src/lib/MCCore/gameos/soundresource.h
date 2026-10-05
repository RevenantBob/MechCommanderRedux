#pragma once

#include "platform/MCWin32Defs.h"

class File;
class SoundResource;

/// <summary>Where a sound resource's samples come from.</summary>
enum gosEnum_SoundResourceType : int32_t
{
    /// <summary>A wave file, read whole into memory. The enumerator names are the port's.</summary>
    SOUND_RESOURCE_FILE = 0,
    /// <summary>A wave file image the caller already has in memory (not copied).</summary>
    SOUND_RESOURCE_MEMORY = 1,
    /// <summary>A wave file streamed from disk as it plays (music).</summary>
    SOUND_RESOURCE_STREAM = 2,
};

/// <summary>A link of <see cref="SRLinkedList"/>.</summary>
/// <remarks>Original source: <c>game os\sound renderer\sound resource.cpp</c>; 0x10 bytes.</remarks>
class SRLink
{
public:
    /// <summary>Unlinks (clears next and previous).</summary>
    /// <remarks>MCX.EXE @ 0x007585f0 (vector deleting destructor)</remarks>
    virtual ~SRLink()
    {
        next = nullptr;
        prev = nullptr;
    }

    /// <summary>The next link.</summary>
    SRLink* next = nullptr; // +0x04
    /// <summary>The previous link.</summary>
    SRLink* prev = nullptr; // +0x08
    /// <summary>The resource.</summary>
    SoundResource* data = nullptr; // +0x0c
};

/// <summary>
/// The sound renderer's list of every live <see cref="SoundResource"/> (<c>SRLinkedList&lt;SoundResource*&gt;</c>
/// in the original, a template whose methods were all inlined). Resources add themselves when made and remove
/// themselves when destroyed.
/// </summary>
/// <remarks>Original source: <c>game os\sound renderer\sound resource.cpp</c>; 8 bytes. Its static constructor
/// (0x00758140, in <c>sound renderer.cpp</c>) only zeroes it.</remarks>
struct SRLinkedList
{
    /// <summary>The first link.</summary>
    SRLink* head = nullptr; // +0x00
    /// <summary>How many links there are.</summary>
    int32_t count = 0; // +0x04

    /// <summary>The static destructor (registered with atexit): frees every link, not the resources.</summary>
    /// <remarks>MCX.EXE @ 0x00758160 (unnamed in Ghidra)</remarks>
    ~SRLinkedList();
};

/// <summary>Every sound resource there is.</summary>
extern SRLinkedList m_soundResources;
/// <summary>Guards the resources against the streaming timer thread.</summary>
extern std::recursive_mutex SoundCritSec;
/// <summary>How deep SoundResource::Read has recursed (it wraps a looping stream); Fatal above 3.</summary>
extern int32_t readEntries;

/// <summary>Makes a sound resource of <paramref name="type"/> from a file name or a wave image.</summary>
/// <remarks>MCX.EXE @ 0x007583c0</remarks>
void gos_CreateSoundResource(void** resource, const char* source, gosEnum_SoundResourceType type, uint32_t flags);
/// <summary>Stops every channel playing the resource, then destroys it (if it is in the list).</summary>
/// <remarks>MCX.EXE @ 0x00758400</remarks>
void gos_DestroySoundResource(void* resource);

/// <summary>
/// A wave: a file loaded whole, a caller's memory image, or a file streamed from disk. Knows its format, where
/// its samples are, and for a stream how to read and rewind them.
/// </summary>
/// <remarks>Original source: <c>game os\sound renderer\sound resource.cpp</c>; 0x40 bytes.</remarks>
class SoundResource
{
public:
    /// <summary>Loads the file, parses the memory image, or opens the stream; then adds itself to
    /// <see cref="m_soundResources"/>.</summary>
    /// <remarks>MCX.EXE @ 0x00758490</remarks>
    SoundResource(const char* source, gosEnum_SoundResourceType type, uint32_t flags);
    /// <summary>Frees the name, closes the stream, and unlinks itself.</summary>
    /// <remarks>MCX.EXE @ 0x00758620</remarks>
    ~SoundResource();

    /// <summary>Reads the whole file into memory and finds its format and data. Fatal when it can't open it.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x007586f0</remarks>
    void LoadFile();
    /// <summary>Walks a RIFF WAVE image's chunks for "fmt " and "data". Fatal when it isn't one.</summary>
    /// <remarks>MCX.EXE @ 0x00758790</remarks>
    void GetWaveInfo(uint8_t* image, tWAVEFORMATEX** format, uint8_t** data, uint32_t* dataSize);
    /// <summary>
    /// Reads up to <paramref name="bytes"/> of stream; at the end, a looping stream rewinds and reads on, otherwise
    /// the rest is filled with silence.
    /// </summary>
    /// <returns><paramref name="bytes"/>.</returns>
    /// <remarks>MCX.EXE @ 0x00758830</remarks>
    uint32_t Read(uint8_t* buffer, uint32_t bytes, bool loop);
    /// <summary>Frees the format and closes the stream's file.</summary>
    /// <remarks>MCX.EXE @ 0x00758920</remarks>
    void CloseStream();
    /// <summary>Seeks the stream back to its data.</summary>
    /// <remarks>MCX.EXE @ 0x00758970 (unnamed in Ghidra; the name is the port's)</remarks>
    void Rewind();
    /// <summary>
    /// Opens the file as a stream: checks RIFF/WAVE and PCM, reads the format, finds the data chunk, and works out
    /// the duration. Fatal on a bad file.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00758990</remarks>
    void Open();
    /// <summary>Finds the format and data in the caller's image.</summary>
    /// <remarks>MCX.EXE @ 0x00758ca0</remarks>
    void OpenFromMemory();

    /// <summary>Where the samples come from.</summary>
    gosEnum_SoundResourceType type = SOUND_RESOURCE_FILE; // +0x00
    /// <summary>The file name (malloc) for a file or stream.</summary>
    char* fileName = nullptr; // +0x04
    /// <summary>The whole wave image: read by LoadFile (malloc), or the caller's.</summary>
    uint8_t* fileImage = nullptr; // +0x08
    /// <summary>The data chunk in the image.</summary>
    uint8_t* waveData = nullptr; // +0x0c
    /// <summary>The file's size (LoadFile).</summary>
    uint32_t fileSize = 0; // +0x10
    /// <summary>The data chunk's size.</summary>
    uint32_t waveSize = 0; // +0x14
    /// <summary>The flags given at creation; not read.</summary>
    uint32_t flags = 0; // +0x1c
    /// <summary>The format: in the image, or (a stream) a malloc'd copy.</summary>
    tWAVEFORMATEX* format = nullptr; // +0x20
    /// <summary>A stream's open file.</summary>
    File* stream = nullptr; // +0x24
    /// <summary>A stream's data chunk position in the file.</summary>
    int32_t dataStart = 0; // +0x28
    /// <summary>A stream's length in ms.</summary>
    uint32_t durationMs = 0; // +0x2c
    /// <summary>A stream's data size, rounded down to whole sample frames.</summary>
    uint32_t dataSize = 0; // +0x38
    /// <summary>Zeroed by the constructor and Rewind; not read.</summary>
    uint32_t streamPos = 0; // +0x3c
};
