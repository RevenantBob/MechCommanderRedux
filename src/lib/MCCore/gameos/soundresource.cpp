#include "stdafx.h"
#include "gameos/soundresource.h"
#include "gameos/soundchannel.h"
#include "gameos/soundrenderer.h"
#include "lib/aerror.h"
#include "lib/file.h"
#include "platform/MCFileSystem.h"

MCSRLinkedList MSoundResources;
std::recursive_mutex SoundCritSec;
int32_t ReadEntries = 0;

namespace
{
    /// <summary>A malloc'd copy of <paramref name="text"/>.</summary>
    char* CopyName(const char* text)
    {
        size_t size = std::strlen(text) + 1;
        char* copy = static_cast<char*>(std::malloc(size));
        std::memcpy(copy, text, size);
        return copy;
    }

    /// <summary>Unlinks and frees the link holding <paramref name="resource"/> (the inlined
    /// <c>SRLinkedList::Remove</c>).</summary>
    void RemoveLink(MCSoundResource* resource)
    {
        MCSRLink* link = MSoundResources.Head;

        if (link == nullptr)
        {
            return;
        }

        if (link->Data == resource)
        {
            MSoundResources.Count--;
            MSoundResources.Head = link->Next;

            if (MSoundResources.Head != nullptr)
            {
                MSoundResources.Head->Prev = nullptr;
            }

            delete link;
            return;
        }

        MCSRLink* previous = link;

        for (link = link->Next; link != nullptr; link = link->Next)
        {
            if (link->Data == resource)
            {
                previous->Next = link->Next;

                if (link->Next != nullptr)
                {
                    link->Next->Prev = previous;
                }

                MSoundResources.Count--;
                delete link;
                return;
            }

            previous = link;
        }
    }
}

MCSRLinkedList::~MCSRLinkedList()
{
    while (Head != nullptr)
    {
        MCSRLink* link = Head;
        Head = link->Next;
        Count--;
        delete link;
    }
}

void GosCreateSoundResource(void** resource, const char* source, MCSoundResourceType type, uint32_t flags)
{
    *resource = new MCSoundResource(source, type, flags);
}

void GosDestroySoundResource(void* resource)
{
    std::lock_guard<std::recursive_mutex> lock(SoundCritSec);

    for (int i = 0; i < SRData.NumChannels; i++)
    {
        if (SRData.Channels[i]->Resource == resource)
        {
            GosStopChannel(i);
            SRData.Channels[i]->Resource = nullptr;
        }
    }

    for (MCSRLink* link = MSoundResources.Head; link != nullptr && link->Data != nullptr; link = link->Next)
    {
        if (link->Data == resource)
        {
            delete link->Data;
            break;
        }
    }
}

MCSoundResource::MCSoundResource(const char* source, MCSoundResourceType type, uint32_t flags)
    : Type(type), Flags(flags)
{
    if (type == SOUND_RESOURCE_FILE)
    {
        FileName = CopyName(source);
        LoadFile();
    }
    else if (type == SOUND_RESOURCE_MEMORY)
    {
        FileImage = reinterpret_cast<uint8_t*>(const_cast<char*>(source));
        OpenFromMemory();
    }
    else if (type == SOUND_RESOURCE_STREAM)
    {
        FileName = CopyName(source);
        Open();
    }

    MCSRLink* link = new MCSRLink();
    link->Data = this;

    if (MSoundResources.Head == nullptr)
    {
        MSoundResources.Count++;
        MSoundResources.Head = link;
        return;
    }

    MCSRLink* last = MSoundResources.Head;

    while (last->Next != nullptr)
    {
        last = last->Next;
    }

    last->Next = link;
    link->Prev = last;
    MSoundResources.Count++;
}

MCSoundResource::~MCSoundResource()
{
    if (FileName != nullptr)
    {
        std::free(FileName);
    }

    FileName = nullptr;

    if (Stream != nullptr)
    {
        if (Format != nullptr)
        {
            std::free(Format);
            Format = nullptr;
        }

        Stream->Close();
        delete Stream;
        Stream = nullptr;
    }

    // A file resource's image is never freed (it leaks in the original too).
    RemoveLink(this);
}

void MCSoundResource::LoadFile()
{
    std::ifstream file(MCFileSystem::Resolve(FileName), std::ios::binary);

    // The original tested CreateFile's result against null, not INVALID_HANDLE_VALUE, so the Fatal never fired.
    if (!file)
    {
        Fatal(-1, "Cannot load sound resource!");
    }

    file.seekg(0, std::ios::end);
    FileSize = static_cast<uint32_t>(file.tellg());
    file.seekg(0, std::ios::beg);
    FileImage = static_cast<uint8_t*>(std::malloc(FileSize));

    if (FileImage != nullptr)
    {
        file.read(reinterpret_cast<char*>(FileImage), FileSize);
    }

    GetWaveInfo(FileImage, &Format, &WaveData, &WaveSize);
}

void MCSoundResource::GetWaveInfo(uint8_t* image, tWAVEFORMATEX** format, uint8_t** data, uint32_t* dataSize)
{
    uint32_t riffSize;
    std::memcpy(&riffSize, image + 4, 4);
    uint32_t riffId;
    uint32_t waveId;
    std::memcpy(&riffId, image, 4);
    std::memcpy(&waveId, image + 8, 4);

    if (riffId != 0x46464952 || waveId != 0x45564157)
    {
        Fatal(-1, "Unknown Sound File Format!");
    }

    uint8_t* chunk = image + 0xc;
    uint8_t* end = image + 0xc + riffSize - 4;

    while (chunk < end)
    {
        uint32_t chunkId;
        uint32_t chunkSize;
        std::memcpy(&chunkId, chunk, 4);
        std::memcpy(&chunkSize, chunk + 4, 4);
        uint8_t* chunkData = chunk + 8;

        if (chunkId == 0x20746d66)
        {
            if (chunkSize < 0xe)
            {
                Fatal(-1, "Cannot find FMT section!");
            }

            *format = reinterpret_cast<tWAVEFORMATEX*>(chunkData);
        }
        else if (chunkId == 0x61746164)
        {
            *data = chunkData;
            *dataSize = chunkSize;
        }

        chunk = chunkData + ((chunkSize + 1) & ~1u);
    }
}

uint32_t MCSoundResource::Read(uint8_t* buffer, uint32_t bytes, bool loop)
{
    ReadEntries++;

    if (ReadEntries > 3)
    {
        Fatal(static_cast<int32_t>(bytes), " Recursed more than 3 times into SoundResource::Read");
    }

    uint32_t bytesRead = 0;

    if (Stream != nullptr)
    {
        bytesRead = static_cast<uint32_t>(Stream->Read(buffer, static_cast<int32_t>(bytes)));
    }

    if (bytesRead != bytes)
    {
        if (loop)
        {
            Rewind();
            uint32_t rest = Read(buffer + bytesRead, bytes - bytesRead, true);
            ReadEntries--;
            return rest + bytesRead;
        }

        uint32_t fill = bytes;

        if (bytesRead != 0)
        {
            buffer += bytesRead;
            fill = bytes - bytesRead;
        }

        std::memset(buffer, Format->wBitsPerSample == 8 ? 0x80 : 0, fill);
    }

    ReadEntries--;
    return bytes;
}

void MCSoundResource::CloseStream()
{
    if (Format != nullptr)
    {
        std::free(Format);
        Format = nullptr;
    }

    if (Stream != nullptr)
    {
        Stream->Close();
        delete Stream;
        Stream = nullptr;
    }
}

void MCSoundResource::Rewind()
{
    if (Stream != nullptr)
    {
        Stream->Seek(DataStart, SEEK_SET);
    }

    StreamPos = 0;
}

void MCSoundResource::Open()
{
    char message[1024];
    MCFile* file = new MCFile();
    Stream = file;
    int32_t result = file->Open(FileName, READ, 50);

    if (result != 0)
    {
        std::snprintf(message, sizeof(message), "Could not open Music File %s", FileName);
        Fatal(result, message);
    }

    if (file->ReadLong() != 0x46464952)
    {
        std::snprintf(message, sizeof(message), "Music File %s Not a RIFF file", FileName);
        Fatal(-1, message);
    }

    file->ReadLong();

    if (file->ReadLong() != 0x45564157)
    {
        std::snprintf(message, sizeof(message), "Music File %s Not a WAVE file", FileName);
        Fatal(-1, message);
    }

    int32_t chunkId = file->ReadLong();

    while (chunkId != 0x20746d66 && file->Eof() == 0)
    {
        int32_t chunkSize = file->ReadLong();
        file->Seek((chunkSize + 1) & ~1, SEEK_CUR);
        chunkId = file->ReadLong();
    }

    if (file->Eof() != 0)
    {
        std::snprintf(message, sizeof(message), "Music File %s Has No FMT Chunk!", FileName);
        Fatal(-1, message);
    }

    tWAVEFORMATEX* waveFormat = static_cast<tWAVEFORMATEX*>(std::malloc(sizeof(tWAVEFORMATEX)));
    Format = waveFormat;
    int32_t formatEnd = file->ReadLong();
    formatEnd += static_cast<int32_t>(file->GetLogicalPosition());
    uint32_t value = static_cast<uint32_t>(file->ReadLong());
    waveFormat->wFormatTag = static_cast<uint16_t>(value);

    if (static_cast<int16_t>(value) != 1)
    {
        std::snprintf(message, sizeof(message), "Music File %s Not Microsoft Format (PCM)", FileName);
        Fatal(-1, message);
    }

    waveFormat->nChannels = static_cast<uint16_t>(value >> 16);
    waveFormat->nSamplesPerSec = static_cast<uint32_t>(file->ReadLong());
    waveFormat->nAvgBytesPerSec = static_cast<uint32_t>(file->ReadLong());
    value = static_cast<uint32_t>(file->ReadLong());
    waveFormat->nBlockAlign = static_cast<uint16_t>(value);
    waveFormat->wBitsPerSample = static_cast<uint16_t>(value >> 16);
    waveFormat->cbSize = 0;
    file->Seek(formatEnd, SEEK_SET);
    chunkId = file->ReadLong();

    while (chunkId != 0x61746164)
    {
        if (file->Eof() != 0)
        {
            break;
        }

        int32_t chunkSize = file->ReadLong();
        file->Seek((chunkSize + 1) & ~1, SEEK_CUR);
        chunkId = file->ReadLong();
    }

    if (file->Eof() != 0)
    {
        std::snprintf(message, sizeof(message), "Music File %s Has No DATA Chunk!", FileName);
        Fatal(-1, message);
    }

    uint32_t size = static_cast<uint32_t>(file->ReadLong());
    int32_t frameBits = static_cast<int32_t>(waveFormat->wBitsPerSample * waveFormat->nChannels);
    int32_t frameBytes = (frameBits + ((frameBits >> 31) & 7)) >> 3;
    DataSize = size & ~static_cast<uint32_t>(frameBytes - 1);
    DataStart = static_cast<int32_t>(file->GetLogicalPosition());
    Rewind();
    DurationMs = static_cast<uint32_t>(static_cast<int64_t>(DataSize) * 1000 / waveFormat->nAvgBytesPerSec);
}

void MCSoundResource::OpenFromMemory()
{
    GetWaveInfo(FileImage, &Format, &WaveData, &WaveSize);
}
