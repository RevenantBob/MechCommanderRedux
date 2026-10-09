#include "stdafx.h"
#include "gameos/MCSoundResource.h"
#include "lib/MCFatal.h"
#include "lib/MCFile.h"
#include "platform/MCFileSystem.h"

namespace
{
    /// <summary>"RIFF".</summary>
    constexpr uint32_t RIFF_ID = 0x46464952;
    /// <summary>"WAVE".</summary>
    constexpr uint32_t WAVE_ID = 0x45564157;
    /// <summary>"fmt ".</summary>
    constexpr uint32_t FORMAT_ID = 0x20746d66;
    /// <summary>"data".</summary>
    constexpr uint32_t DATA_ID = 0x61746164;

    /// <summary>The little-endian 32-bit value at <paramref name="at"/>.</summary>
    uint32_t ReadU32(const uint8_t* at)
    {
        uint32_t value;
        std::memcpy(&value, at, 4);
        return value;
    }
}

MCSoundResource::MCSoundResource(const uint8_t* image) : Type(MCSoundResourceType::Memory)
{
    ReadWaveInfo(image);
}

MCSoundResource::MCSoundResource(MCSoundResourceType type, std::string_view fileName) : Type(type), _FileName(fileName)
{
    if (type == MCSoundResourceType::File)
    {
        LoadFile();
    }
    else if (type == MCSoundResourceType::Stream)
    {
        OpenStream();
    }
}

MCSoundResource::~MCSoundResource()
{
    if (_Stream != nullptr)
    {
        _Stream->Close();
    }
}

void MCSoundResource::LoadFile()
{
    std::ifstream file(MCFileSystem::Resolve(_FileName), std::ios::binary);

    // The original tested CreateFile's result against null, not INVALID_HANDLE_VALUE, so the Fatal never fired.
    if (!file)
    {
        Fatal(-1, "Cannot load sound resource!");
    }

    file.seekg(0, std::ios::end);
    _FileImage.resize(static_cast<size_t>(file.tellg()));
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(_FileImage.data()), static_cast<std::streamsize>(_FileImage.size()));
    ReadWaveInfo(_FileImage.data());
}

void MCSoundResource::ReadWaveInfo(const uint8_t* image)
{
    const uint32_t riffSize = ReadU32(image + 4);

    if (ReadU32(image) != RIFF_ID || ReadU32(image + 8) != WAVE_ID)
    {
        Fatal(-1, "Unknown Sound File Format!");
    }

    const uint8_t* chunk = image + 0xc;
    const uint8_t* end = image + 0xc + riffSize - 4;

    while (chunk < end)
    {
        const uint32_t chunkId = ReadU32(chunk);
        const uint32_t chunkSize = ReadU32(chunk + 4);
        const uint8_t* chunkData = chunk + 8;

        if (chunkId == FORMAT_ID)
        {
            if (chunkSize < 0xe)
            {
                Fatal(-1, "Cannot find FMT section!");
            }

            Format = reinterpret_cast<const tWAVEFORMATEX*>(chunkData);
        }
        else if (chunkId == DATA_ID)
        {
            WaveData = chunkData;
            WaveSize = chunkSize;
        }

        chunk = chunkData + ((chunkSize + 1) & ~1u);
    }
}

uint32_t MCSoundResource::Read(uint8_t* buffer, uint32_t bytes, bool loop, int32_t depth)
{
    if (depth > 3)
    {
        Fatal(static_cast<int32_t>(bytes), " Recursed more than 3 times into SoundResource::Read");
    }

    uint32_t bytesRead = 0;

    if (_Stream != nullptr)
    {
        bytesRead = static_cast<uint32_t>(_Stream->Read(buffer, static_cast<int32_t>(bytes)));
    }

    if (bytesRead == bytes)
    {
        return bytes;
    }

    if (loop)
    {
        Rewind();
        return Read(buffer + bytesRead, bytes - bytesRead, true, depth + 1) + bytesRead;
    }

    std::memset(buffer + bytesRead, Format->wBitsPerSample == 8 ? 0x80 : 0, bytes - bytesRead);
    return bytes;
}

void MCSoundResource::Rewind()
{
    if (_Stream != nullptr)
    {
        _Stream->Seek(_DataStart, SEEK_SET);
    }
}

void MCSoundResource::OpenStream()
{
    _Stream = std::make_unique<MCFile>();
    MCFile* file = _Stream.get();
    const int32_t result = file->Open(_FileName);

    if (result != 0)
    {
        Fatal(result, std::format("Could not open Music File {}", _FileName).c_str());
    }

    if (static_cast<uint32_t>(file->ReadLong()) != RIFF_ID)
    {
        Fatal(-1, std::format("Music File {} Not a RIFF file", _FileName).c_str());
    }

    file->ReadLong();

    if (static_cast<uint32_t>(file->ReadLong()) != WAVE_ID)
    {
        Fatal(-1, std::format("Music File {} Not a WAVE file", _FileName).c_str());
    }

    uint32_t chunkId = static_cast<uint32_t>(file->ReadLong());

    while (chunkId != FORMAT_ID && !file->Eof())
    {
        const int32_t chunkSize = file->ReadLong();
        file->Seek((chunkSize + 1) & ~1, SEEK_CUR);
        chunkId = static_cast<uint32_t>(file->ReadLong());
    }

    if (file->Eof())
    {
        Fatal(-1, std::format("Music File {} Has No FMT Chunk!", _FileName).c_str());
    }

    Format = &_StreamFormat;
    int32_t formatEnd = file->ReadLong();
    formatEnd += static_cast<int32_t>(file->GetLogicalPosition());
    uint32_t value = static_cast<uint32_t>(file->ReadLong());
    _StreamFormat.wFormatTag = static_cast<uint16_t>(value);

    if (static_cast<int16_t>(value) != 1)
    {
        Fatal(-1, std::format("Music File {} Not Microsoft Format (PCM)", _FileName).c_str());
    }

    _StreamFormat.nChannels = static_cast<uint16_t>(value >> 16);
    _StreamFormat.nSamplesPerSec = static_cast<uint32_t>(file->ReadLong());
    _StreamFormat.nAvgBytesPerSec = static_cast<uint32_t>(file->ReadLong());
    value = static_cast<uint32_t>(file->ReadLong());
    _StreamFormat.nBlockAlign = static_cast<uint16_t>(value);
    _StreamFormat.wBitsPerSample = static_cast<uint16_t>(value >> 16);
    _StreamFormat.cbSize = 0;
    file->Seek(formatEnd, SEEK_SET);
    chunkId = static_cast<uint32_t>(file->ReadLong());

    while (chunkId != DATA_ID && !file->Eof())
    {
        const int32_t chunkSize = file->ReadLong();
        file->Seek((chunkSize + 1) & ~1, SEEK_CUR);
        chunkId = static_cast<uint32_t>(file->ReadLong());
    }

    if (file->Eof())
    {
        Fatal(-1, std::format("Music File {} Has No DATA Chunk!", _FileName).c_str());
    }

    const uint32_t size = static_cast<uint32_t>(file->ReadLong());
    const int32_t frameBits = static_cast<int32_t>(_StreamFormat.wBitsPerSample * _StreamFormat.nChannels);
    const int32_t frameBytes = (frameBits + ((frameBits >> 31) & 7)) >> 3;
    const uint32_t dataSize = size & ~static_cast<uint32_t>(frameBytes - 1);
    _DataStart = static_cast<int32_t>(file->GetLogicalPosition());
    Rewind();
    DurationMs = static_cast<uint32_t>(static_cast<int64_t>(dataSize) * 1000 / _StreamFormat.nAvgBytesPerSec);
}
