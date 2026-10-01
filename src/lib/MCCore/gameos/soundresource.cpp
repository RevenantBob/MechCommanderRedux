#include "stdafx.h"
#include "gameos/soundresource.h"
#include "gameos/soundchannel.h"
#include "gameos/soundrenderer.h"
#include "lib/aerror.h"
#include "lib/file.h"
#include "platform/MCFileSystem.h"

SRLinkedList m_soundResources;
std::recursive_mutex SoundCritSec;
int32_t readEntries = 0;

namespace
{
    /// <summary>A malloc'd copy of <paramref name="text"/>.</summary>
    char* copyName(const char* text)
    {
        size_t size = std::strlen(text) + 1;
        char* copy = static_cast<char*>(std::malloc(size));
        std::memcpy(copy, text, size);
        return copy;
    }

    /// <summary>Unlinks and frees the link holding <paramref name="resource"/> (the inlined
    /// <c>SRLinkedList::Remove</c>).</summary>
    void removeLink(SoundResource* resource)
    {
        SRLink* link = m_soundResources.head;

        if (link == nullptr)
        {
            return;
        }

        if (link->data == resource)
        {
            m_soundResources.count--;
            m_soundResources.head = link->next;

            if (m_soundResources.head != nullptr)
            {
                m_soundResources.head->prev = nullptr;
            }

            delete link;
            return;
        }

        SRLink* previous = link;

        for (link = link->next; link != nullptr; link = link->next)
        {
            if (link->data == resource)
            {
                previous->next = link->next;

                if (link->next != nullptr)
                {
                    link->next->prev = previous;
                }

                m_soundResources.count--;
                delete link;
                return;
            }

            previous = link;
        }
    }
}

SRLinkedList::~SRLinkedList()
{
    while (head != nullptr)
    {
        SRLink* link = head;
        head = link->next;
        count--;
        delete link;
    }
}

void gos_CreateSoundResource(void** resource, const char* source, gosEnum_SoundResourceType type, uint32_t flags)
{
    *resource = new SoundResource(source, type, flags);
}

void gos_DestroySoundResource(void* resource)
{
    std::lock_guard<std::recursive_mutex> lock(SoundCritSec);

    for (int i = 0; i < g_SRData.numChannels; i++)
    {
        if (g_SRData.channels[i]->resource == resource)
        {
            gos_StopChannel(i);
            g_SRData.channels[i]->resource = nullptr;
        }
    }

    for (SRLink* link = m_soundResources.head; link != nullptr && link->data != nullptr; link = link->next)
    {
        if (link->data == resource)
        {
            delete link->data;
            break;
        }
    }
}

SoundResource::SoundResource(const char* source, gosEnum_SoundResourceType type, uint32_t flags)
    : type(type), flags(flags)
{
    if (type == SOUND_RESOURCE_FILE)
    {
        fileName = copyName(source);
        LoadFile();
    }
    else if (type == SOUND_RESOURCE_MEMORY)
    {
        fileImage = reinterpret_cast<uint8_t*>(const_cast<char*>(source));
        OpenFromMemory();
    }
    else if (type == SOUND_RESOURCE_STREAM)
    {
        fileName = copyName(source);
        Open();
    }

    SRLink* link = new SRLink();
    link->data = this;

    if (m_soundResources.head == nullptr)
    {
        m_soundResources.count++;
        m_soundResources.head = link;
        return;
    }

    SRLink* last = m_soundResources.head;

    while (last->next != nullptr)
    {
        last = last->next;
    }

    last->next = link;
    link->prev = last;
    m_soundResources.count++;
}

SoundResource::~SoundResource()
{
    if (fileName != nullptr)
    {
        std::free(fileName);
    }

    fileName = nullptr;

    if (stream != nullptr)
    {
        if (format != nullptr)
        {
            std::free(format);
            format = nullptr;
        }

        stream->close();
        delete stream;
        stream = nullptr;
    }

    // A file resource's image is never freed (it leaks in the original too).
    removeLink(this);
}

void SoundResource::LoadFile()
{
    std::ifstream file(MCFileSystem::Resolve(fileName), std::ios::binary);

    // The original tested CreateFile's result against null, not INVALID_HANDLE_VALUE, so the Fatal never fired.
    if (!file)
    {
        Fatal(-1, "Cannot load sound resource!");
    }

    file.seekg(0, std::ios::end);
    fileSize = static_cast<uint32_t>(file.tellg());
    file.seekg(0, std::ios::beg);
    fileImage = static_cast<uint8_t*>(std::malloc(fileSize));

    if (fileImage != nullptr)
    {
        file.read(reinterpret_cast<char*>(fileImage), fileSize);
    }

    GetWaveInfo(fileImage, &format, &waveData, &waveSize);
}

void SoundResource::GetWaveInfo(uint8_t* image, tWAVEFORMATEX** format, uint8_t** data, uint32_t* dataSize)
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

uint32_t SoundResource::Read(uint8_t* buffer, uint32_t bytes, bool loop)
{
    readEntries++;

    if (readEntries > 3)
    {
        Fatal(static_cast<int32_t>(bytes), " Recursed more than 3 times into SoundResource::Read");
    }

    uint32_t bytesRead = 0;

    if (stream != nullptr)
    {
        bytesRead = static_cast<uint32_t>(stream->read(buffer, static_cast<int32_t>(bytes)));
    }

    if (bytesRead != bytes)
    {
        if (loop)
        {
            Rewind();
            uint32_t rest = Read(buffer + bytesRead, bytes - bytesRead, true);
            readEntries--;
            return rest + bytesRead;
        }

        uint32_t fill = bytes;

        if (bytesRead != 0)
        {
            buffer += bytesRead;
            fill = bytes - bytesRead;
        }

        std::memset(buffer, format->wBitsPerSample == 8 ? 0x80 : 0, fill);
    }

    readEntries--;
    return bytes;
}

void SoundResource::CloseStream()
{
    if (format != nullptr)
    {
        std::free(format);
        format = nullptr;
    }

    if (stream != nullptr)
    {
        stream->close();
        delete stream;
        stream = nullptr;
    }
}

void SoundResource::Rewind()
{
    if (stream != nullptr)
    {
        stream->seek(dataStart, SEEK_SET);
    }

    streamPos = 0;
}

void SoundResource::Open()
{
    char message[1024];
    File* file = new File();
    stream = file;
    int32_t result = file->open(fileName, READ, 50);

    if (result != 0)
    {
        std::snprintf(message, sizeof(message), "Could not open Music File %s", fileName);
        Fatal(result, message);
    }

    if (file->readLong() != 0x46464952)
    {
        std::snprintf(message, sizeof(message), "Music File %s Not a RIFF file", fileName);
        Fatal(-1, message);
    }

    file->readLong();

    if (file->readLong() != 0x45564157)
    {
        std::snprintf(message, sizeof(message), "Music File %s Not a WAVE file", fileName);
        Fatal(-1, message);
    }

    int32_t chunkId = file->readLong();

    while (chunkId != 0x20746d66 && file->eof() == 0)
    {
        int32_t chunkSize = file->readLong();
        file->seek((chunkSize + 1) & ~1, SEEK_CUR);
        chunkId = file->readLong();
    }

    if (file->eof() != 0)
    {
        std::snprintf(message, sizeof(message), "Music File %s Has No FMT Chunk!", fileName);
        Fatal(-1, message);
    }

    tWAVEFORMATEX* waveFormat = static_cast<tWAVEFORMATEX*>(std::malloc(sizeof(tWAVEFORMATEX)));
    format = waveFormat;
    int32_t formatEnd = file->readLong();
    formatEnd += static_cast<int32_t>(file->getLogicalPosition());
    uint32_t value = static_cast<uint32_t>(file->readLong());
    waveFormat->wFormatTag = static_cast<uint16_t>(value);

    if (static_cast<int16_t>(value) != 1)
    {
        std::snprintf(message, sizeof(message), "Music File %s Not Microsoft Format (PCM)", fileName);
        Fatal(-1, message);
    }

    waveFormat->nChannels = static_cast<uint16_t>(value >> 16);
    waveFormat->nSamplesPerSec = static_cast<uint32_t>(file->readLong());
    waveFormat->nAvgBytesPerSec = static_cast<uint32_t>(file->readLong());
    value = static_cast<uint32_t>(file->readLong());
    waveFormat->nBlockAlign = static_cast<uint16_t>(value);
    waveFormat->wBitsPerSample = static_cast<uint16_t>(value >> 16);
    waveFormat->cbSize = 0;
    file->seek(formatEnd, SEEK_SET);
    chunkId = file->readLong();

    while (chunkId != 0x61746164)
    {
        if (file->eof() != 0)
        {
            break;
        }

        int32_t chunkSize = file->readLong();
        file->seek((chunkSize + 1) & ~1, SEEK_CUR);
        chunkId = file->readLong();
    }

    if (file->eof() != 0)
    {
        std::snprintf(message, sizeof(message), "Music File %s Has No DATA Chunk!", fileName);
        Fatal(-1, message);
    }

    uint32_t size = static_cast<uint32_t>(file->readLong());
    int32_t frameBits = static_cast<int32_t>(waveFormat->wBitsPerSample * waveFormat->nChannels);
    int32_t frameBytes = (frameBits + ((frameBits >> 31) & 7)) >> 3;
    dataSize = size & ~static_cast<uint32_t>(frameBytes - 1);
    dataStart = static_cast<int32_t>(file->getLogicalPosition());
    Rewind();
    durationMs = static_cast<uint32_t>(static_cast<int64_t>(dataSize) * 1000 / waveFormat->nAvgBytesPerSec);
}

void SoundResource::OpenFromMemory()
{
    GetWaveInfo(fileImage, &format, &waveData, &waveSize);
}
