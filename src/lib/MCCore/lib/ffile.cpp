#include "stdafx.h"
#include "lib/ffile.h"
#include "lib/file.h"
#include "lib/lzdecomp.h"
#include "platform/MCFileSystem.h"

uint8_t* LZPacketBuffer = nullptr;
uint32_t LZPacketBufferSize = 512000;

MCFastFile::MCFastFile() = default;

MCFastFile::~MCFastFile()
{
    Close();
}

int32_t MCFastFile::Open(const char* fName)
{
    const size_t nameLength = std::strlen(fName) + 1;
    _FileName = new char[nameLength];
    std::memcpy(_FileName, fName, nameLength);

    _Handle = std::fopen(MCFileSystem::Resolve(_FileName).string().c_str(), "rb");

    if (_Handle == nullptr)
    {
        return FILE_NOT_FOUND;
    }

    _LogicalPosition = 0;

    if (_Length == 0)
    {
        std::fseek(_Handle, 0, SEEK_END);
        _Length = static_cast<int32_t>(std::ftell(_Handle));
    }

    std::fseek(_Handle, 0, SEEK_SET);
    _LogicalPosition = 0;

    int32_t count = 0;
    const bool readCount = std::fread(&count, 4, 1, _Handle) == 1;
    _LogicalPosition += 4;

    if (!readCount)
    {
        return READ_ERR;
    }

    _NumFiles = count;
    _Files = static_cast<MCFileHandle*>(std::malloc(sizeof(MCFileHandle) * static_cast<size_t>(_NumFiles)));

    for (int32_t i = 0; i < _NumFiles; ++i)
    {
        MCFileEntry* entry = static_cast<MCFileEntry*>(std::calloc(1, sizeof(MCFileEntry)));
        _Files[i].File = entry;
        // The original ignores the byte count; a short directory leaves the rest of the entry zeroed.
        (void)std::fread(entry, sizeof(MCFileEntry), 1, _Handle);
        entry->Name[sizeof(entry->Name) - 1] = 0;
        _Files[i].Inuse = 0;
        _Files[i].Pos = 0;
    }

    return NO_ERR;
}

void MCFastFile::Close()
{
    delete[] _FileName;
    _FileName = nullptr;
    _Length = 0;

    if (_Handle != nullptr)
    {
        std::fclose(_Handle);
        _Handle = nullptr;
    }

    for (int32_t i = 0; i < _NumFiles; ++i)
    {
        std::free(_Files[i].File);
    }

    std::free(_Files);
    _Files = nullptr;
    _NumFiles = 0;
}

int32_t MCFastFile::OpenFast(const char* fName)
{
    for (int32_t i = 0; i < _NumFiles; ++i)
    {
        if (MCPort::StrICmp(_Files[i].File->Name, fName) == 0)
        {
            _Files[i].Inuse = 1;
            _Files[i].Pos = 0;
            return i;
        }
    }

    return -1;
}

void MCFastFile::CloseFast(int32_t fastFileHandle)
{
    if (fastFileHandle >= 0 && fastFileHandle < _NumFiles && _Files[fastFileHandle].Inuse)
    {
        _Files[fastFileHandle].Inuse = 0;
        _Files[fastFileHandle].Pos = 0;
    }
}

int32_t MCFastFile::SeekFast(int32_t fastFileHandle, int32_t off, int32_t from)
{
    if (fastFileHandle < 0 || fastFileHandle >= _NumFiles || !_Files[fastFileHandle].Inuse)
    {
        return FILE_NOT_OPEN;
    }

    MCFileHandle& fh = _Files[fastFileHandle];

    // Original behaviour: the bounds checks use the stored (packed) size, and SEEK_END adds to it.
    switch (from)
    {
        case SEEK_SET:
        {
            if (off > fh.File->Size)
            {
                return READ_PAST_EOF;
            }
            break;
        }
        case SEEK_CUR:
        {
            if (fh.Pos + off > fh.File->Size)
            {
                return READ_PAST_EOF;
            }

            off += fh.Pos;
            break;
        }
        case SEEK_END:
        {
            if (std::abs(off) > fh.File->Size || off > 0)
            {
                return READ_PAST_EOF;
            }

            off += fh.File->Size;
            break;
        }
        default:
        {
            off = 0;
            fh.Pos = off;
            _LogicalPosition = fh.File->Offset + off;
            std::fseek(_Handle, _LogicalPosition, SEEK_SET);
            return off;
        }
    }

    if (off == -1)
    {
        return INVALID_SEEK;
    }

    fh.Pos = off;
    _LogicalPosition = fh.File->Offset + off;
    std::fseek(_Handle, _LogicalPosition, SEEK_SET);
    return off;
}

int32_t MCFastFile::ReadFast(int32_t fastFileHandle, void* bfr, int32_t size)
{
    if (fastFileHandle < 0 || fastFileHandle >= _NumFiles || !_Files[fastFileHandle].Inuse)
    {
        return FILE_NOT_OPEN;
    }

    MCFileHandle& fh = _Files[fastFileHandle];
    MCFileEntry* entry = fh.File;

    _LogicalPosition = entry->Offset + fh.Pos;
    std::fseek(_Handle, _LogicalPosition, SEEK_SET);

    if (entry->Size == entry->RealSize)
    {
        _LogicalPosition += size;
        return static_cast<int32_t>(std::fread(bfr, 1, static_cast<size_t>(size), _Handle));
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

    if (static_cast<int32_t>(LZPacketBufferSize) < entry->Size)
    {
        LZPacketBufferSize = static_cast<uint32_t>(entry->Size);
        std::free(LZPacketBuffer);
        LZPacketBuffer = static_cast<uint8_t*>(std::malloc(LZPacketBufferSize));

        if (LZPacketBuffer == nullptr)
        {
            return 0;
        }
    }

    _LogicalPosition += entry->Size;
    const size_t got = std::fread(LZPacketBuffer, 1, static_cast<size_t>(entry->Size), _Handle);
    // Port fix: unpack no further than the entry's real size, which is what every caller sizes its buffer to.
    const int32_t unpacked = LZDecomp(static_cast<uint8_t*>(bfr), LZPacketBuffer, static_cast<uint32_t>(got),
                                      static_cast<uint32_t>(entry->RealSize));
    return unpacked == entry->RealSize ? unpacked : 0;
}

int32_t MCFastFile::TellFast(int32_t fastFileHandle)
{
    if (fastFileHandle >= 0 && fastFileHandle < _NumFiles && _Files[fastFileHandle].Inuse)
    {
        return _Files[fastFileHandle].Pos;
    }

    return -1;
}

int32_t MCFastFile::SizeFast(int32_t fastFileHandle)
{
    if (fastFileHandle >= 0 && fastFileHandle < _NumFiles && _Files[fastFileHandle].Inuse)
    {
        return _Files[fastFileHandle].File->RealSize;
    }

    return -1;
}

int32_t MCFastFile::LzSizeFast(int32_t fastFileHandle)
{
    if (fastFileHandle >= 0 && fastFileHandle < _NumFiles && _Files[fastFileHandle].Inuse)
    {
        return _Files[fastFileHandle].File->Size;
    }

    return -1;
}
