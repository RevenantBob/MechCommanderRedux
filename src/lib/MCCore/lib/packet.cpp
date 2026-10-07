#include "stdafx.h"
#include "lib/packet.h"
#include "lib/ffile.h"
#include "lib/lzcomp.h"
#include "lib/lzdecomp.h"
#include "lib/aerror.h"
#include "platform/MCFileSystem.h"

namespace
{
    constexpr uint32_t TypeShift = 29;
    constexpr uint32_t OffsetMask = 0x1fffffff;
    constexpr uint32_t PendingMark = 0xe0000000;
    constexpr int32_t PACKET_OUT_OF_RANGE = static_cast<int32_t>(0xBADF0004);
    constexpr int32_t BAD_PACKET_VERSION = static_cast<int32_t>(0xBADF0004);
    constexpr int32_t NO_RAM_FOR_SEEK_TABLE = static_cast<int32_t>(0xBADF000C);
    constexpr int32_t PACKET_WRONG_STORAGE = static_cast<int32_t>(0xBADF000B);
    constexpr int32_t PACKET_WRONG_SIZE = static_cast<int32_t>(0xBADF0005);
}

MCPacketFile::MCPacketFile()
{
    Clear();
}

MCPacketFile::~MCPacketFile()
{
    Close();
}

void MCPacketFile::Clear()
{
    _CurrentPacket = -1;
    _NumPackets = 0;
    _PacketBase = 0;
    _PacketSize = 0;
    delete[] _SeekTable;
    _SeekTable = nullptr;
}

void MCPacketFile::AtClose()
{
    if (IsOpen() && _FileMode != READ)
    {
        // Packets written as "pending" (never written) point at the next written packet, or at the end.
        uint32_t next = GetLength();
        _CurrentPacket = _NumPackets;

        if (_SeekTable == nullptr)
        {
            for (_CurrentPacket = _NumPackets - 1; _CurrentPacket >= 0; --_CurrentPacket)
            {
                Seek(_CurrentPacket * 4 + 8);
                const uint32_t entry = static_cast<uint32_t>(ReadLong());

                if ((entry & PendingMark) == PendingMark)
                {
                    Seek(_CurrentPacket * 4 + 8);
                    WriteLong(static_cast<int32_t>(next + PendingMark));
                }
                else
                {
                    next = entry & OffsetMask;
                }
            }
        }
        else
        {
            for (_CurrentPacket = _NumPackets - 1; _CurrentPacket >= 0; --_CurrentPacket)
            {
                uint32_t& entry = reinterpret_cast<uint32_t&>(_SeekTable[_CurrentPacket]);

                if ((entry & PendingMark) == PendingMark)
                {
                    entry = next + PendingMark;
                }
                else
                {
                    next = entry & OffsetMask;
                }
            }

            Seek(8);
            Write(reinterpret_cast<const uint8_t*>(_SeekTable), _NumPackets * 4);
        }

        if (_UsesCheckSum)
        {
            const int32_t sum = CheckSumFile();
            Seek(0);
            WriteLong(sum);
        }
    }

    Clear();
}

int32_t MCPacketFile::CheckSumFile()
{
    const uint32_t saved = _LogicalPosition;
    Seek(4);
    const uint32_t size = FileSize();
    std::vector<uint8_t> bytes(size);
    Read(bytes.data(), static_cast<int32_t>(size));
    int32_t sum = 0;

    for (uint32_t i = 0; i + 4 < size; ++i)
    {
        sum += bytes[i];
    }

    Seek(static_cast<int32_t>(saved));
    return sum;
}

int32_t MCPacketFile::AfterOpen()
{
    if (_NumPackets == 0 && GetLength() >= 12)
    {
        const int32_t firstWord = ReadLong();

        if ((firstWord != PACKET_FILE_MAGIC || _UsesCheckSum) && CheckSumFile() != firstWord)
        {
            return BAD_PACKET_VERSION;
        }

        const uint32_t firstPacket = static_cast<uint32_t>(ReadLong());
        _NumPackets = static_cast<int32_t>(firstPacket / 4) - 2;
    }

    _CurrentPacket = -1;

    if ((_FileMode == READ || _FileMode == RDWRITE) && _NumPackets != 0 && _SeekTable == nullptr)
    {
        _SeekTable = new int32_t[static_cast<size_t>(_NumPackets)];
        Seek(8);
        Read(reinterpret_cast<uint8_t*>(_SeekTable), _NumPackets * 4);
    }

    return NO_ERR;
}

int32_t MCPacketFile::Open(const char* fName, MCFileMode mode, int32_t numChild)
{
    int32_t result = MCFile::Open(fName, mode, numChild);

    if (result == NO_ERR)
    {
        result = AfterOpen();
    }

    return result;
}

int32_t MCPacketFile::Open(MCFile* parent, uint32_t fileSize, int32_t numChild)
{
    int32_t result = MCFile::Open(parent, fileSize, numChild);

    if (result == NO_ERR)
    {
        result = AfterOpen();
    }

    return result;
}

int32_t MCPacketFile::Create(const char* fName)
{
    int32_t result = MCFile::Create(fName);

    if (result == NO_ERR)
    {
        result = AfterOpen();
    }

    return result;
}

void MCPacketFile::Close()
{
    AtClose();
    MCFile::Close();
}

int32_t MCPacketFile::ReadPacketOffset(int32_t packet, int32_t* type)
{
    uint32_t offset = 0xffffffff;

    if (packet < _NumPackets)
    {
        if (_SeekTable != nullptr)
        {
            offset = static_cast<uint32_t>(_SeekTable[packet]);
        }

        if (type != nullptr)
        {
            *type = static_cast<int32_t>(offset >> TypeShift);
        }

        offset &= OffsetMask;
    }

    return static_cast<int32_t>(offset);
}

int32_t MCPacketFile::ReadPacket(int32_t packet, uint8_t* buffer)
{
    if (packet != -1 && packet != _CurrentPacket && SeekPacket(packet) != NO_ERR)
    {
        return 0;
    }

    const int32_t storage = GetStorageType();

    if (storage == STORAGE_TYPE_RAW || storage == STORAGE_TYPE_FWF)
    {
        Seek(_PacketBase);
        return Read(buffer, _PacketSize);
    }

    if (storage == STORAGE_TYPE_LZD)
    {
        Seek(_PacketBase + 4);

        if (LZPacketBuffer == nullptr)
        {
            LZPacketBuffer = static_cast<uint8_t*>(std::malloc(LZPacketBufferSize));

            if (LZPacketBuffer == nullptr)
            {
                Fatal(-1, " No RAM to Unpack Files ");
            }
        }

        if (static_cast<int32_t>(LZPacketBufferSize) < _PacketSize)
        {
            LZPacketBufferSize = static_cast<uint32_t>(_PacketSize);
            std::free(LZPacketBuffer);
            LZPacketBuffer = static_cast<uint8_t*>(std::malloc(LZPacketBufferSize));

            if (LZPacketBuffer == nullptr)
            {
                Fatal(-1, " No RAM to Unpack Files ");
            }
        }

        const int32_t got = Read(LZPacketBuffer, _PacketSize - 4);
        const int32_t unpacked = LZDecomp(buffer, LZPacketBuffer, static_cast<uint32_t>(std::max(got, 0)),
                                          static_cast<uint32_t>(_PacketUnpackedSize));
        return unpacked == _PacketUnpackedSize ? unpacked : 0;
    }

    return 0;
}

int32_t MCPacketFile::ReadPackedPacket(int32_t packet, uint8_t* buffer)
{
    if (packet != -1 && packet != _CurrentPacket && SeekPacket(packet) != NO_ERR)
    {
        return 0;
    }

    const int32_t storage = GetStorageType();

    if (storage == STORAGE_TYPE_RAW || storage == STORAGE_TYPE_FWF)
    {
        Seek(_PacketBase);
        return Read(buffer, _PacketSize);
    }

    if (storage == STORAGE_TYPE_LZD)
    {
        Seek(_PacketBase + 4);
        Read(buffer, _PacketSize);
    }

    return 0;
}

int32_t MCPacketFile::SeekPacket(int32_t packet)
{
    if (packet < 0)
    {
        return PACKET_OUT_OF_RANGE;
    }

    const int32_t offset = ReadPacketOffset(packet, &_PacketType);
    _CurrentPacket = packet;
    const int32_t next = packet + 1 == _NumPackets ? static_cast<int32_t>(GetLength()) : ReadPacketOffset(packet + 1);
    _PacketSize = next - offset;
    _PacketBase = offset;
    Seek(offset);

    switch (GetStorageType())
    {
        case STORAGE_TYPE_RAW:
            _PacketUnpackedSize = _PacketSize;
            break;
        case STORAGE_TYPE_LZD:
            _PacketUnpackedSize = ReadLong();
            break;
        case STORAGE_TYPE_NUL:
            _PacketUnpackedSize = 0;
            break;
        default:
            return PACKET_WRONG_STORAGE;
    }

    return offset > 0 ? NO_ERR : PACKET_OUT_OF_RANGE;
}

void MCPacketFile::operator++()
{
    if (++_CurrentPacket >= _NumPackets)
    {
        _CurrentPacket = _NumPackets - 1;
    }

    SeekPacket(_CurrentPacket);
}

void MCPacketFile::operator--()
{
    if (_CurrentPacket-- <= 0)
    {
        _CurrentPacket = 0;
    }

    SeekPacket(_CurrentPacket);
}

void MCPacketFile::Reserve(int32_t count, int useCheckSum)
{
    if (_NumPackets != 0)
    {
        return;
    }

    _UsesCheckSum = useCheckSum;
    _NumPackets = count;
    const int32_t firstPacket = count * 4 + 8;
    WriteLong(PACKET_FILE_MAGIC);
    WriteLong(firstPacket);

    for (int32_t i = 0; i < count; ++i)
    {
        WriteLong(static_cast<int32_t>(static_cast<uint32_t>(firstPacket) + PendingMark));
    }

    if (_SeekTable == nullptr)
    {
        _SeekTable = new int32_t[static_cast<size_t>(std::max(_NumPackets, 1))];
        Seek(8);
        Read(reinterpret_cast<uint8_t*>(_SeekTable), _NumPackets * 4);
    }
}

int32_t MCPacketFile::WritePacket(int32_t packet, uint8_t* buffer, int32_t nbytes, uint8_t storageType)
{
    std::vector<uint8_t> packed;

    if (storageType == STORAGE_TYPE_NUL || storageType == STORAGE_TYPE_LZD)
    {
        packed.resize(static_cast<size_t>(nbytes) * 2 + 16);
    }

    if (packet < 0 || packet >= _NumPackets)
    {
        return 0;
    }

    const uint32_t end = GetLength();
    _PacketBase = static_cast<int32_t>(end);
    _CurrentPacket = packet;
    _PacketUnpackedSize = nbytes;
    _PacketSize = nbytes;

    uint8_t type = storageType;

    if (type == STORAGE_TYPE_NUL || type == STORAGE_TYPE_LZD)
    {
        // NUL asks for "packed if it helps"; LZD always packs.
        if (type == STORAGE_TYPE_NUL)
        {
            type = STORAGE_TYPE_RAW;
        }

        const int32_t packedSize = LZCompress(packed.data(), buffer, static_cast<uint32_t>(nbytes));

        if (packedSize == -1)
        {
            return 0;
        }

        std::vector<uint8_t> check(static_cast<size_t>(nbytes) + 16);

        if (LZDecomp(check.data(), packed.data(), static_cast<uint32_t>(packedSize),
                     static_cast<uint32_t>(check.size())) != nbytes)
        {
            Fatal(-1, " Holy Dogshit, Private Pyle ");
        }

        if (storageType == STORAGE_TYPE_LZD || packedSize < nbytes)
        {
            type = STORAGE_TYPE_LZD;
            _PacketSize = packedSize;
        }
    }

    _PacketType = type;

    Seek(static_cast<int32_t>(end));
    int32_t written;

    if (_PacketType == STORAGE_TYPE_LZD)
    {
        WriteLong(_PacketUnpackedSize);
        written = Write(packed.data(), _PacketSize);
    }
    else
    {
        written = Write(buffer, _PacketSize);
    }

    const int32_t entry = _PacketType * 0x20000000 + _PacketBase;

    if (_SeekTable == nullptr)
    {
        Seek(packet * 4 + 8);
        WriteLong(entry);
    }
    else
    {
        _SeekTable[packet] = entry;
    }

    // Later packets not written yet point at the new end. Original behaviour: on disk this rewrites entries
    // packet+1 .. numPackets-1, in the table packet+1 .. numPackets-2.
    const uint32_t newEnd = GetLength();

    if (_SeekTable == nullptr)
    {
        for (int32_t i = packet; i < _NumPackets - 1; ++i)
        {
            WriteLong(static_cast<int32_t>(newEnd + PendingMark));
        }
    }
    else
    {
        for (int32_t i = packet + 1; i < _NumPackets - 1; ++i)
        {
            _SeekTable[i] = static_cast<int32_t>(newEnd + PendingMark);
        }
    }

    return written;
}

int32_t MCPacketFile::InsertPacket(int32_t packet, uint8_t* buffer, int32_t nbytes, uint8_t storageType)
{
    if (packet < 0)
    {
        return 0;
    }

    static const char* tempName = "AF3456AF.788";

    std::vector<uint8_t> scratch(0xffff);
    MCPacketFile temp;
    const int32_t result = temp.Create(tempName);

    if (packet >= _NumPackets)
    {
        ++_NumPackets;
    }

    temp.Reserve(_NumPackets);

    for (int32_t i = 0; i < _NumPackets; ++i)
    {
        if (i == packet)
        {
            temp.WritePacket(i, buffer, nbytes, storageType);
            continue;
        }

        SeekPacket(i);
        const int32_t type = GetStorageType();
        const int32_t size = GetPacketSize();

        if (static_cast<size_t>(size) > scratch.size())
        {
            scratch.resize(static_cast<size_t>(size));
        }

        ReadPacket(i, scratch.data());
        temp.WritePacket(i, scratch.data(), size, static_cast<uint8_t>(type));
    }

    const std::string name = GetFilename();
    const MCFileMode mode = _FileMode;
    temp.Close();
    Close();
    std::error_code error;
    std::filesystem::copy_file(MCFileSystem::Resolve(tempName), MCFileSystem::ResolveWrite(name),
                               std::filesystem::copy_options::overwrite_existing, error);
    std::filesystem::remove(MCFileSystem::ResolveWrite(tempName), error);
    Open(name.c_str(), mode, 50);
    SeekPacket(packet);
    return result;
}

int32_t MCPacketFile::WritePacket(int32_t packet, uint8_t* buffer)
{
    if (packet < 0 || packet >= _NumPackets)
    {
        return 0;
    }

    SeekPacket(packet);

    if (_PacketType == STORAGE_TYPE_LZD || _PacketType == STORAGE_TYPE_HF)
    {
        return PACKET_WRONG_STORAGE;
    }

    const int32_t written = Write(buffer, _PacketSize);
    return written != _PacketUnpackedSize ? WRITE_ERR : NO_ERR;
}
