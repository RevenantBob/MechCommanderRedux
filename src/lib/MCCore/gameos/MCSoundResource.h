#pragma once

#include "platform/MCWin32Defs.h"

class MCFile;

/// <summary>Where a sound resource's samples come from.</summary>
enum class MCSoundResourceType : int32_t
{
    /// <summary>A wave file, read whole into memory.</summary>
    File = 0,
    /// <summary>A wave file image the caller already has in memory (not copied).</summary>
    Memory = 1,
    /// <summary>A wave file streamed from disk as it plays (music).</summary>
    Stream = 2,
};

/// <summary>
/// A wave: a file loaded whole, a caller's memory image, or a file streamed from disk. Knows its format, where its
/// samples are, and for a stream how to read and rewind them. The <see cref="MCSoundRenderer"/> owns every resource.
/// </summary>
/// <remarks>Original source: <c>game os\sound renderer\sound resource.cpp</c>.</remarks>
class MCSoundResource
{
public:
    /// <summary>A memory resource over the caller's RIFF WAVE image, which must outlive it. Fatal when it isn't
    /// one.</summary>
    explicit MCSoundResource(const uint8_t* image);
    /// <summary>A file resource (read whole) or a stream resource (opened for reading as it plays). Fatal on a file
    /// that can't be opened or isn't a PCM wave.</summary>
    MCSoundResource(MCSoundResourceType type, std::string_view fileName);
    /// <summary>Closes the stream.</summary>
    ~MCSoundResource();

    MCSoundResource(const MCSoundResource&) = delete;
    MCSoundResource& operator=(const MCSoundResource&) = delete;

    /// <summary>
    /// Reads up to <paramref name="bytes"/> of stream; at the end, a looping stream rewinds and reads on, otherwise
    /// the rest is filled with silence.
    /// </summary>
    /// <param name="depth">How deep the looping read has recursed; Fatal above 3.</param>
    /// <returns><paramref name="bytes"/>.</returns>
    uint32_t Read(uint8_t* buffer, uint32_t bytes, bool loop, int32_t depth = 1);
    /// <summary>Seeks the stream back to its data.</summary>
    void Rewind();

    /// <summary>Where the samples come from.</summary>
    MCSoundResourceType Type = MCSoundResourceType::File;
    /// <summary>The data chunk in the image.</summary>
    const uint8_t* WaveData = nullptr;
    /// <summary>The data chunk's size.</summary>
    uint32_t WaveSize = 0;
    /// <summary>The format: in the image, or a stream's own copy.</summary>
    const tWAVEFORMATEX* Format = nullptr;
    /// <summary>A stream's length in ms.</summary>
    uint32_t DurationMs = 0;

private:
    /// <summary>Reads the whole file into memory and finds its format and data. Fatal when it can't open it.
    /// </summary>
    void LoadFile();
    /// <summary>Walks a RIFF WAVE image's chunks for "fmt " and "data". Fatal when it isn't one.</summary>
    void ReadWaveInfo(const uint8_t* image);
    /// <summary>
    /// Opens the file as a stream: checks RIFF/WAVE and PCM, reads the format, finds the data chunk, and works out
    /// the duration. Fatal on a bad file.
    /// </summary>
    void OpenStream();

    /// <summary>The file name, for a file or stream.</summary>
    std::string _FileName;
    /// <summary>A file resource's whole wave image.</summary>
    std::vector<uint8_t> _FileImage;
    /// <summary>A stream's format, read from its file.</summary>
    tWAVEFORMATEX _StreamFormat{};
    /// <summary>A stream's open file.</summary>
    std::unique_ptr<MCFile> _Stream;
    /// <summary>A stream's data chunk position in the file.</summary>
    int32_t _DataStart = 0;
};
