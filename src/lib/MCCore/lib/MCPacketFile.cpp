#include "stdafx.h"
#include "lib/MCPacketFile.h"
#include "lib/MCFatal.h"
#include "lib/MCLz.h"

namespace
{
    constexpr uint32_t TypeShift = 29;
    constexpr uint32_t OffsetMask = 0x1fffffff;
    /// <summary>The storage bits of a table entry whose packet hasn't been written yet.</summary>
    constexpr uint32_t PendingMark = 0xe0000000;
    constexpr int32_t PacketOutOfRange = static_cast<int32_t>(0xBADF0004);
    constexpr int32_t BadPacketVersion = static_cast<int32_t>(0xBADF0004);
    constexpr int32_t PacketWrongStorage = static_cast<int32_t>(0xBADF000B);

    /// <summary>The table as the bytes a file holds.</summary>
    std::span<uint8_t> TableBytes(std::vector<uint32_t>& table)
    {
        return std::span(reinterpret_cast<uint8_t*>(table.data()), table.size() * sizeof(uint32_t));
    }
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
    _SeekTable.clear();
}

void MCPacketFile::AtClose()
{
    if (IsOpen() && _FileMode != MCFileMode::Read)
    {
        // Packets left "pending" (never written) point at the next written packet, or at the end.
        // (A written file always has its table in memory: Reserve made it.)
        uint32_t next = GetLength();

        for (_CurrentPacket = _NumPackets - 1; _CurrentPacket >= 0; --_CurrentPacket)
        {
            uint32_t& entry = _SeekTable[static_cast<size_t>(_CurrentPacket)];

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
        Write(TableBytes(_SeekTable));

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
    Read(bytes);
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

        if ((firstWord != PacketFileMagic || _UsesCheckSum) && CheckSumFile() != firstWord)
        {
            return BadPacketVersion;
        }

        const uint32_t firstPacket = static_cast<uint32_t>(ReadLong());
        _NumPackets = static_cast<int32_t>(firstPacket / 4) - 2;
    }

    _CurrentPacket = -1;

    if (_FileMode == MCFileMode::Read && _NumPackets > 0 && _SeekTable.empty())
    {
        _SeekTable.assign(static_cast<size_t>(_NumPackets), 0);
        Seek(8);
        Read(TableBytes(_SeekTable));
    }

    return NO_ERR;
}

int32_t MCPacketFile::Open(std::string_view fileName, MCFileMode mode)
{
    int32_t result = MCFile::Open(fileName, mode);

    if (result == NO_ERR)
    {
        result = AfterOpen();
    }

    return result;
}

int32_t MCPacketFile::Open(MCFile* parent, uint32_t length)
{
    int32_t result = MCFile::Open(parent, length);

    if (result == NO_ERR)
    {
        result = AfterOpen();
    }

    return result;
}

int32_t MCPacketFile::Create(std::string_view fileName)
{
    int32_t result = MCFile::Create(fileName);

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

int32_t MCPacketFile::ReadPacketOffset(int32_t packet, MCPacketStorage* storage) const
{
    uint32_t offset = 0xffffffff;

    if (packet < _NumPackets)
    {
        if (!_SeekTable.empty())
        {
            offset = _SeekTable[static_cast<size_t>(packet)];
        }

        if (storage != nullptr)
        {
            *storage = static_cast<MCPacketStorage>(offset >> TypeShift);
        }

        offset &= OffsetMask;
    }

    return static_cast<int32_t>(offset);
}

int32_t MCPacketFile::ReadPacket(int32_t packet, std::span<uint8_t> buffer)
{
    if (packet != -1 && packet != _CurrentPacket && SeekPacket(packet) != NO_ERR)
    {
        return 0;
    }

    switch (_PacketType)
    {
        case MCPacketStorage::Raw:
        case MCPacketStorage::Fwf:
        {
            Seek(_PacketBase);
            return Read(buffer.first(std::min(buffer.size(), static_cast<size_t>(std::max(_PacketSize, 0)))));
        }
        case MCPacketStorage::Lzd:
        {
            Seek(_PacketBase + 4);
            std::vector<uint8_t> packed(static_cast<size_t>(std::max(_PacketSize - 4, 0)));
            packed.resize(static_cast<size_t>(std::max(Read(packed), 0)));
            const int32_t unpacked = LZDecomp(
                buffer.first(std::min(buffer.size(), static_cast<size_t>(std::max(_PacketUnpackedSize, 0)))), packed);
            return unpacked == _PacketUnpackedSize ? unpacked : 0;
        }
        default:
        {
            return 0;
        }
    }
}

int32_t MCPacketFile::ReadPacket(int32_t packet, uint8_t* buffer)
{
    if (packet != -1 && packet != _CurrentPacket && SeekPacket(packet) != NO_ERR)
    {
        return 0;
    }

    const int32_t size = _PacketType == MCPacketStorage::Lzd ? _PacketUnpackedSize : _PacketSize;
    return ReadPacket(-1, std::span(buffer, static_cast<size_t>(std::max(size, 0))));
}

int32_t MCPacketFile::SeekPacket(int32_t packet)
{
    if (packet < 0)
    {
        return PacketOutOfRange;
    }

    const int32_t offset = ReadPacketOffset(packet, &_PacketType);
    _CurrentPacket = packet;
    const int32_t next = packet + 1 == _NumPackets ? static_cast<int32_t>(GetLength()) : ReadPacketOffset(packet + 1);
    _PacketSize = next - offset;
    _PacketBase = offset;
    Seek(offset);

    switch (_PacketType)
    {
        case MCPacketStorage::Raw:
            _PacketUnpackedSize = _PacketSize;
            break;
        case MCPacketStorage::Lzd:
            _PacketUnpackedSize = ReadLong();
            break;
        case MCPacketStorage::Nul:
            _PacketUnpackedSize = 0;
            break;
        default:
            return PacketWrongStorage;
    }

    return offset > 0 ? NO_ERR : PacketOutOfRange;
}

void MCPacketFile::Reserve(int32_t count, bool useCheckSum)
{
    if (_NumPackets != 0)
    {
        return;
    }

    _UsesCheckSum = useCheckSum;
    _NumPackets = count;
    const int32_t firstPacket = count * 4 + 8;
    WriteLong(PacketFileMagic);
    WriteLong(firstPacket);

    for (int32_t i = 0; i < count; ++i)
    {
        WriteLong(static_cast<int32_t>(static_cast<uint32_t>(firstPacket) + PendingMark));
    }

    _SeekTable.assign(static_cast<size_t>(std::max(_NumPackets, 0)), 0);
    Seek(8);
    Read(TableBytes(_SeekTable));
}

int32_t MCPacketFile::WritePacket(int32_t packet, std::span<const uint8_t> data, MCPacketStorage storage)
{
    if (packet < 0 || packet >= _NumPackets)
    {
        return 0;
    }

    const int32_t size = static_cast<int32_t>(data.size());
    const uint32_t end = GetLength();
    _PacketBase = static_cast<int32_t>(end);
    _CurrentPacket = packet;
    _PacketUnpackedSize = size;
    _PacketSize = size;
    _PacketType = MCPacketStorage::Raw;
    std::vector<uint8_t> packed;

    if (storage == MCPacketStorage::Nul || storage == MCPacketStorage::Lzd)
    {
        packed = LZCompress(data);
        std::vector<uint8_t> check(data.size() + 16);

        if (LZDecomp(check, packed) != size)
        {
            Fatal(-1, " Holy Dogshit, Private Pyle ");
        }

        if (storage == MCPacketStorage::Lzd || static_cast<int32_t>(packed.size()) < size)
        {
            _PacketType = MCPacketStorage::Lzd;
            _PacketSize = static_cast<int32_t>(packed.size());
        }
    }
    else
    {
        _PacketType = storage;
    }

    Seek(static_cast<int32_t>(end));
    int32_t written;

    if (_PacketType == MCPacketStorage::Lzd)
    {
        WriteLong(_PacketUnpackedSize);
        written = Write(packed);
    }
    else
    {
        written = Write(data);
    }

    const uint32_t entry = static_cast<uint32_t>(_PacketType) << TypeShift | static_cast<uint32_t>(_PacketBase);

    _SeekTable[static_cast<size_t>(packet)] = entry;

    // The packets after it point at the new end until they are written (the last one isn't moved; Close points
    // every packet still pending at the next written one).
    const uint32_t newEnd = GetLength();

    for (int32_t i = packet + 1; i < _NumPackets - 1; ++i)
    {
        _SeekTable[static_cast<size_t>(i)] = newEnd + PendingMark;
    }

    return written;
}
