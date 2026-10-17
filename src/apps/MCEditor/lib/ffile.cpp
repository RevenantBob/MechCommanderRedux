#include "stdafx.h"
#include "lib/ffile.h"
#include "lib/file.h"
#include "lib/lzdecomp.h"

uint8_t* LZPacketBuffer = nullptr;
uint32_t LZPacketBufferSize = 512000;

FastFile::~FastFile()
{
    close();
    std::free(LZPacketBuffer);
    LZPacketBuffer = nullptr;
}

int32_t FastFile::open(const char* fName)
{
    const size_t nameLength = std::strlen(fName) + 1;
    fileName = new (std::nothrow) char[nameLength];

    if (fileName == nullptr)
    {
        return NO_RAM_FOR_FILENAME;
    }

    std::strncpy(fileName, fName, nameLength);
    handle =
        CreateFileA(fileName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (handle != INVALID_HANDLE_VALUE)
    {
        logicalPosition = 0;

        if (length == 0)
        {
            length = static_cast<int32_t>(GetFileSize(handle, nullptr));
        }
    }

    // Original behaviour: a missing archive isn't reported here; the read below fails on the invalid handle instead.
    logicalPosition = static_cast<int32_t>(SetFilePointer(handle, 0, nullptr, FILE_BEGIN));
    DWORD bytesRead = 0;
    const BOOL readCount = ReadFile(handle, &numFiles, sizeof(numFiles), &bytesRead, nullptr);
    logicalPosition += sizeof(numFiles);

    if (!readCount)
    {
        return static_cast<int32_t>(GetLastError());
    }

    files = static_cast<FILE_HANDLE*>(std::malloc(sizeof(FILE_HANDLE) * static_cast<size_t>(numFiles)));

    for (int32_t i = 0; i < numFiles; ++i)
    {
        files[i].file = static_cast<FILEENTRY*>(std::malloc(sizeof(FILEENTRY)));
        std::memset(files[i].file, 0, sizeof(FILEENTRY));
        bytesRead = 0;
        ReadFile(handle, files[i].file, sizeof(FILEENTRY), &bytesRead, nullptr);
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

    if (handle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(handle);
        handle = INVALID_HANDLE_VALUE;
    }

    for (int32_t i = 0; i < numFiles; ++i)
    {
        std::free(files[i].file);
    }

    std::free(files);
    files = nullptr;
    numFiles = 0;
}

int32_t FastFile::openFastByIndex(int32_t fileIndex)
{
    if (fileIndex >= 0 && fileIndex < numFiles)
    {
        files[fileIndex].inuse = 1;
        files[fileIndex].pos = 0;
        return fileIndex;
    }

    return -1;
}

int32_t FastFile::openFast(const char* fName)
{
    for (int32_t i = 0; i < numFiles; ++i)
    {
        if (_stricmp(files[i].file->name, fName) == 0)
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
            // An unknown origin goes to the start without the -1 check.
            off = 0;
            fh.pos = off;
            logicalPosition =
                static_cast<int32_t>(SetFilePointer(handle, fh.file->offset + fh.pos, nullptr, FILE_BEGIN));
            return fh.pos;
        }
    }

    if (off == -1)
    {
        return INVALID_SEEK;
    }

    fh.pos = off;
    logicalPosition = static_cast<int32_t>(SetFilePointer(handle, fh.file->offset + fh.pos, nullptr, FILE_BEGIN));
    return fh.pos;
}

int32_t FastFile::readFast(int32_t fastFileHandle, void* bfr, int32_t size)
{
    if (fastFileHandle < 0 || fastFileHandle >= numFiles || !files[fastFileHandle].inuse)
    {
        return FILE_NOT_OPEN;
    }

    FILE_HANDLE& fh = files[fastFileHandle];
    logicalPosition = static_cast<int32_t>(SetFilePointer(handle, fh.file->offset + fh.pos, nullptr, FILE_BEGIN));
    DWORD bytesRead = 0;

    if (fh.file->size == fh.file->realSize)
    {
        const BOOL ok = ReadFile(handle, bfr, static_cast<DWORD>(size), &bytesRead, nullptr);
        logicalPosition += size;

        if (!ok)
        {
            return static_cast<int32_t>(GetLastError());
        }

        return static_cast<int32_t>(bytesRead);
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

    if (static_cast<int32_t>(LZPacketBufferSize) < fh.file->size)
    {
        LZPacketBufferSize = static_cast<uint32_t>(fh.file->size);
        std::free(LZPacketBuffer);
        LZPacketBuffer = static_cast<uint8_t*>(std::malloc(LZPacketBufferSize));

        if (LZPacketBuffer == nullptr)
        {
            return 0;
        }
    }

    const BOOL ok = ReadFile(handle, LZPacketBuffer, static_cast<DWORD>(fh.file->size), &bytesRead, nullptr);
    logicalPosition += fh.file->size;

    if (!ok)
    {
        return static_cast<int32_t>(GetLastError());
    }

    const int32_t unpacked = LZDecomp(static_cast<uint8_t*>(bfr), LZPacketBuffer, static_cast<uint32_t>(fh.file->size));
    return unpacked == fh.file->realSize ? unpacked : 0;
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

const char* FastFile::getEntryName(int32_t fastFileHandle)
{
    if (fastFileHandle >= 0 && fastFileHandle < numFiles && files[fastFileHandle].inuse)
    {
        return files[fastFileHandle].file->name;
    }

    return nullptr;
}
