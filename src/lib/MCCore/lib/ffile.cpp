#include "stdafx.h"
#include "lib/ffile.h"
#include "lib/file.h"
#include "lib/lzdecomp.h"
#include "platform/MCFileSystem.h"

uint8_t* LZPacketBuffer = nullptr;
uint32_t LZPacketBufferSize = 512000;

FastFile::FastFile() = default;

FastFile::~FastFile()
{
    close();
}

int32_t FastFile::open(const char* fName)
{
    const size_t nameLength = std::strlen(fName) + 1;
    fileName = new char[nameLength];
    std::memcpy(fileName, fName, nameLength);

    handle = std::fopen(MCFileSystem::Resolve(fileName).string().c_str(), "rb");

    if (handle == nullptr)
    {
        return FILE_NOT_FOUND;
    }

    logicalPosition = 0;

    if (length == 0)
    {
        std::fseek(handle, 0, SEEK_END);
        length = static_cast<int32_t>(std::ftell(handle));
    }

    std::fseek(handle, 0, SEEK_SET);
    logicalPosition = 0;

    int32_t count = 0;
    const bool readCount = std::fread(&count, 4, 1, handle) == 1;
    logicalPosition += 4;

    if (!readCount)
    {
        return READ_ERR;
    }

    numFiles = count;
    files = static_cast<FILE_HANDLE*>(std::malloc(sizeof(FILE_HANDLE) * static_cast<size_t>(numFiles)));

    for (int32_t i = 0; i < numFiles; ++i)
    {
        FILEENTRY* entry = static_cast<FILEENTRY*>(std::calloc(1, sizeof(FILEENTRY)));
        files[i].file = entry;
        // The original ignores the byte count; a short directory leaves the rest of the entry zeroed.
        (void)std::fread(entry, sizeof(FILEENTRY), 1, handle);
        entry->name[sizeof(entry->name) - 1] = 0;
        files[i].inuse = 0;
        files[i].pos = 0;
    }

    return NO_ERR;
}

void FastFile::close()
{
    delete[] fileName;
    fileName = nullptr;
    length = 0;

    if (handle != nullptr)
    {
        std::fclose(handle);
        handle = nullptr;
    }

    for (int32_t i = 0; i < numFiles; ++i)
    {
        std::free(files[i].file);
    }

    std::free(files);
    files = nullptr;
    numFiles = 0;
}

int32_t FastFile::openFast(const char* fName)
{
    for (int32_t i = 0; i < numFiles; ++i)
    {
        if (MCPort::StrICmp(files[i].file->name, fName) == 0)
        {
            files[i].inuse = 1;
            files[i].pos = 0;
            return i;
        }
    }

    return -1;
}

void FastFile::closeFast(int32_t fastFileHandle)
{
    if (fastFileHandle >= 0 && fastFileHandle < numFiles && files[fastFileHandle].inuse)
    {
        files[fastFileHandle].inuse = 0;
        files[fastFileHandle].pos = 0;
    }
}

int32_t FastFile::seekFast(int32_t fastFileHandle, int32_t off, int32_t from)
{
    if (fastFileHandle < 0 || fastFileHandle >= numFiles || !files[fastFileHandle].inuse)
    {
        return FILE_NOT_OPEN;
    }

    FILE_HANDLE& fh = files[fastFileHandle];

    // Original behaviour: the bounds checks use the stored (packed) size, and SEEK_END adds to it.
    switch (from)
    {
        case SEEK_SET:
        {
            if (off > fh.file->size)
            {
                return READ_PAST_EOF;
            }
            break;
        }
        case SEEK_CUR:
        {
            if (fh.pos + off > fh.file->size)
            {
                return READ_PAST_EOF;
            }

            off += fh.pos;
            break;
        }
        case SEEK_END:
        {
            if (std::abs(off) > fh.file->size || off > 0)
            {
                return READ_PAST_EOF;
            }

            off += fh.file->size;
            break;
        }
        default:
        {
            off = 0;
            fh.pos = off;
            logicalPosition = fh.file->offset + off;
            std::fseek(handle, logicalPosition, SEEK_SET);
            return off;
        }
    }

    if (off == -1)
    {
        return INVALID_SEEK;
    }

    fh.pos = off;
    logicalPosition = fh.file->offset + off;
    std::fseek(handle, logicalPosition, SEEK_SET);
    return off;
}

int32_t FastFile::readFast(int32_t fastFileHandle, void* bfr, int32_t size)
{
    if (fastFileHandle < 0 || fastFileHandle >= numFiles || !files[fastFileHandle].inuse)
    {
        return FILE_NOT_OPEN;
    }

    FILE_HANDLE& fh = files[fastFileHandle];
    FILEENTRY* entry = fh.file;

    logicalPosition = entry->offset + fh.pos;
    std::fseek(handle, logicalPosition, SEEK_SET);

    if (entry->size == entry->realSize)
    {
        logicalPosition += size;
        return static_cast<int32_t>(std::fread(bfr, 1, static_cast<size_t>(size), handle));
    }

    // Packed: read the whole stored entry and unpack it all into the caller's buffer.
    if (LZPacketBuffer == nullptr)
    {
        LZPacketBuffer = static_cast<uint8_t*>(std::malloc(LZPacketBufferSize));

        if (LZPacketBuffer == nullptr)
        {
            return 0;
        }
    }

    if (static_cast<int32_t>(LZPacketBufferSize) < entry->size)
    {
        LZPacketBufferSize = static_cast<uint32_t>(entry->size);
        std::free(LZPacketBuffer);
        LZPacketBuffer = static_cast<uint8_t*>(std::malloc(LZPacketBufferSize));

        if (LZPacketBuffer == nullptr)
        {
            return 0;
        }
    }

    logicalPosition += entry->size;
    const size_t got = std::fread(LZPacketBuffer, 1, static_cast<size_t>(entry->size), handle);
    // Port fix: unpack no further than the entry's real size, which is what every caller sizes its buffer to.
    const int32_t unpacked = LZDecomp(static_cast<uint8_t*>(bfr), LZPacketBuffer, static_cast<uint32_t>(got),
                                      static_cast<uint32_t>(entry->realSize));
    return unpacked == entry->realSize ? unpacked : 0;
}

int32_t FastFile::tellFast(int32_t fastFileHandle)
{
    if (fastFileHandle >= 0 && fastFileHandle < numFiles && files[fastFileHandle].inuse)
    {
        return files[fastFileHandle].pos;
    }

    return -1;
}

int32_t FastFile::sizeFast(int32_t fastFileHandle)
{
    if (fastFileHandle >= 0 && fastFileHandle < numFiles && files[fastFileHandle].inuse)
    {
        return files[fastFileHandle].file->realSize;
    }

    return -1;
}

int32_t FastFile::lzSizeFast(int32_t fastFileHandle)
{
    if (fastFileHandle >= 0 && fastFileHandle < numFiles && files[fastFileHandle].inuse)
    {
        return files[fastFileHandle].file->size;
    }

    return -1;
}
