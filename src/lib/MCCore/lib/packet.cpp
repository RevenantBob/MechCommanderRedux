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

PacketFile::PacketFile()
{
    clear();
}

PacketFile::~PacketFile()
{
    close();
}

void PacketFile::clear()
{
    currentPacket = -1;
    numPackets = 0;
    packetBase = 0;
    packetSize = 0;
    delete[] seekTable;
    seekTable = nullptr;
}

void PacketFile::atClose()
{
    if (isOpen() && fileMode != READ)
    {
        // Packets written as "pending" (never written) point at the next written packet, or at the end.
        uint32_t next = getLength();
        currentPacket = numPackets;

        if (seekTable == nullptr)
        {
            for (currentPacket = numPackets - 1; currentPacket >= 0; --currentPacket)
            {
                seek(currentPacket * 4 + 8);
                const uint32_t entry = static_cast<uint32_t>(readLong());

                if ((entry & PendingMark) == PendingMark)
                {
                    seek(currentPacket * 4 + 8);
                    writeLong(static_cast<int32_t>(next + PendingMark));
                }
                else
                {
                    next = entry & OffsetMask;
                }
            }
        }
        else
        {
            for (currentPacket = numPackets - 1; currentPacket >= 0; --currentPacket)
            {
                uint32_t& entry = reinterpret_cast<uint32_t&>(seekTable[currentPacket]);

                if ((entry & PendingMark) == PendingMark)
                {
                    entry = next + PendingMark;
                }
                else
                {
                    next = entry & OffsetMask;
                }
            }

            seek(8);
            write(reinterpret_cast<const uint8_t*>(seekTable), numPackets * 4);
        }

        if (usesCheckSum)
        {
            const int32_t sum = checkSumFile();
            seek(0);
            writeLong(sum);
        }
    }

    clear();
}

int32_t PacketFile::checkSumFile()
{
    const uint32_t saved = logicalPosition;
    seek(4);
    const uint32_t size = fileSize();
    std::vector<uint8_t> bytes(size);
    read(bytes.data(), static_cast<int32_t>(size));
    int32_t sum = 0;

    for (uint32_t i = 0; i + 4 < size; ++i)
    {
        sum += bytes[i];
    }

    seek(static_cast<int32_t>(saved));
    return sum;
}

int32_t PacketFile::afterOpen()
{
    if (numPackets == 0 && getLength() >= 12)
    {
        const int32_t firstWord = readLong();

        if ((firstWord != PACKET_FILE_MAGIC || usesCheckSum) && checkSumFile() != firstWord)
        {
            return BAD_PACKET_VERSION;
        }

        const uint32_t firstPacket = static_cast<uint32_t>(readLong());
        numPackets = static_cast<int32_t>(firstPacket / 4) - 2;
    }

    currentPacket = -1;

    if ((fileMode == READ || fileMode == RDWRITE) && numPackets != 0 && seekTable == nullptr)
    {
        seekTable = new int32_t[static_cast<size_t>(numPackets)];
        seek(8);
        read(reinterpret_cast<uint8_t*>(seekTable), numPackets * 4);
    }

    return NO_ERR;
}

int32_t PacketFile::open(const char* fName, FileMode _mode, int32_t numChild)
{
    int32_t result = File::open(fName, _mode, numChild);

    if (result == NO_ERR)
    {
        result = afterOpen();
    }

    return result;
}

int32_t PacketFile::open(File* _parent, uint32_t fileSize, int32_t numChild)
{
    int32_t result = File::open(_parent, fileSize, numChild);

    if (result == NO_ERR)
    {
        result = afterOpen();
    }

    return result;
}

int32_t PacketFile::create(const char* fName)
{
    int32_t result = File::create(fName);

    if (result == NO_ERR)
    {
        result = afterOpen();
    }

    return result;
}

void PacketFile::close()
{
    atClose();
    File::close();
}

int32_t PacketFile::readPacketOffset(int32_t packet, int32_t* type)
{
    uint32_t offset = 0xffffffff;

    if (packet < numPackets)
    {
        if (seekTable != nullptr)
        {
            offset = static_cast<uint32_t>(seekTable[packet]);
        }

        if (type != nullptr)
        {
            *type = static_cast<int32_t>(offset >> TypeShift);
        }

        offset &= OffsetMask;
    }

    return static_cast<int32_t>(offset);
}

int32_t PacketFile::readPacket(int32_t packet, uint8_t* buffer)
{
    if (packet != -1 && packet != currentPacket && seekPacket(packet) != NO_ERR)
    {
        return 0;
    }

    const int32_t storage = getStorageType();

    if (storage == STORAGE_TYPE_RAW || storage == STORAGE_TYPE_FWF)
    {
        seek(packetBase);
        return read(buffer, packetSize);
    }

    if (storage == STORAGE_TYPE_LZD)
    {
        seek(packetBase + 4);

        if (LZPacketBuffer == nullptr)
        {
            LZPacketBuffer = static_cast<uint8_t*>(std::malloc(LZPacketBufferSize));

            if (LZPacketBuffer == nullptr)
            {
                Fatal(-1, " No RAM to Unpack Files ");
            }
        }

        if (static_cast<int32_t>(LZPacketBufferSize) < packetSize)
        {
            LZPacketBufferSize = static_cast<uint32_t>(packetSize);
            std::free(LZPacketBuffer);
            LZPacketBuffer = static_cast<uint8_t*>(std::malloc(LZPacketBufferSize));

            if (LZPacketBuffer == nullptr)
            {
                Fatal(-1, " No RAM to Unpack Files ");
            }
        }

        const int32_t got = read(LZPacketBuffer, packetSize - 4);
        const int32_t unpacked = LZDecomp(buffer, LZPacketBuffer, static_cast<uint32_t>(std::max(got, 0)),
                                          static_cast<uint32_t>(packetUnpackedSize));
        return unpacked == packetUnpackedSize ? unpacked : 0;
    }

    return 0;
}

int32_t PacketFile::readPackedPacket(int32_t packet, uint8_t* buffer)
{
    if (packet != -1 && packet != currentPacket && seekPacket(packet) != NO_ERR)
    {
        return 0;
    }

    const int32_t storage = getStorageType();

    if (storage == STORAGE_TYPE_RAW || storage == STORAGE_TYPE_FWF)
    {
        seek(packetBase);
        return read(buffer, packetSize);
    }

    if (storage == STORAGE_TYPE_LZD)
    {
        seek(packetBase + 4);
        read(buffer, packetSize);
    }

    return 0;
}

int32_t PacketFile::seekPacket(int32_t packet)
{
    if (packet < 0)
    {
        return PACKET_OUT_OF_RANGE;
    }

    const int32_t offset = readPacketOffset(packet, &packetType);
    currentPacket = packet;
    const int32_t next = packet + 1 == numPackets ? static_cast<int32_t>(getLength()) : readPacketOffset(packet + 1);
    packetSize = next - offset;
    packetBase = offset;
    seek(offset);

    switch (getStorageType())
    {
        case STORAGE_TYPE_RAW:
            packetUnpackedSize = packetSize;
            break;
        case STORAGE_TYPE_LZD:
            packetUnpackedSize = readLong();
            break;
        case STORAGE_TYPE_NUL:
            packetUnpackedSize = 0;
            break;
        default:
            return PACKET_WRONG_STORAGE;
    }

    return offset > 0 ? NO_ERR : PACKET_OUT_OF_RANGE;
}

void PacketFile::operator++()
{
    if (++currentPacket >= numPackets)
    {
        currentPacket = numPackets - 1;
    }

    seekPacket(currentPacket);
}

void PacketFile::operator--()
{
    if (currentPacket-- <= 0)
    {
        currentPacket = 0;
    }

    seekPacket(currentPacket);
}

void PacketFile::reserve(int32_t count, int useCheckSum)
{
    if (numPackets != 0)
    {
        return;
    }

    usesCheckSum = useCheckSum;
    numPackets = count;
    const int32_t firstPacket = count * 4 + 8;
    writeLong(PACKET_FILE_MAGIC);
    writeLong(firstPacket);

    for (int32_t i = 0; i < count; ++i)
    {
        writeLong(static_cast<int32_t>(static_cast<uint32_t>(firstPacket) + PendingMark));
    }

    if (seekTable == nullptr)
    {
        seekTable = new int32_t[static_cast<size_t>(std::max(numPackets, 1))];
        seek(8);
        read(reinterpret_cast<uint8_t*>(seekTable), numPackets * 4);
    }
}

int32_t PacketFile::writePacket(int32_t packet, uint8_t* buffer, int32_t nbytes, uint8_t storageType)
{
    std::vector<uint8_t> packed;

    if (storageType == STORAGE_TYPE_NUL || storageType == STORAGE_TYPE_LZD)
    {
        packed.resize(static_cast<size_t>(nbytes) * 2 + 16);
    }

    if (packet < 0 || packet >= numPackets)
    {
        return 0;
    }

    const uint32_t end = getLength();
    packetBase = static_cast<int32_t>(end);
    currentPacket = packet;
    packetUnpackedSize = nbytes;
    packetSize = nbytes;

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
            packetSize = packedSize;
        }
    }

    packetType = type;

    seek(static_cast<int32_t>(end));
    int32_t written;

    if (packetType == STORAGE_TYPE_LZD)
    {
        writeLong(packetUnpackedSize);
        written = write(packed.data(), packetSize);
    }
    else
    {
        written = write(buffer, packetSize);
    }

    const int32_t entry = packetType * 0x20000000 + packetBase;

    if (seekTable == nullptr)
    {
        seek(packet * 4 + 8);
        writeLong(entry);
    }
    else
    {
        seekTable[packet] = entry;
    }

    // Later packets not written yet point at the new end. Original behaviour: on disk this rewrites entries
    // packet+1 .. numPackets-1, in the table packet+1 .. numPackets-2.
    const uint32_t newEnd = getLength();

    if (seekTable == nullptr)
    {
        for (int32_t i = packet; i < numPackets - 1; ++i)
        {
            writeLong(static_cast<int32_t>(newEnd + PendingMark));
        }
    }
    else
    {
        for (int32_t i = packet + 1; i < numPackets - 1; ++i)
        {
            seekTable[i] = static_cast<int32_t>(newEnd + PendingMark);
        }
    }

    return written;
}

int32_t PacketFile::insertPacket(int32_t packet, uint8_t* buffer, int32_t nbytes, uint8_t storageType)
{
    if (packet < 0)
    {
        return 0;
    }

    static const char* tempName = "AF3456AF.788";

    std::vector<uint8_t> scratch(0xffff);
    PacketFile temp;
    const int32_t result = temp.create(tempName);

    if (packet >= numPackets)
    {
        ++numPackets;
    }

    temp.reserve(numPackets);

    for (int32_t i = 0; i < numPackets; ++i)
    {
        if (i == packet)
        {
            temp.writePacket(i, buffer, nbytes, storageType);
            continue;
        }

        seekPacket(i);
        const int32_t type = getStorageType();
        const int32_t size = getPacketSize();

        if (static_cast<size_t>(size) > scratch.size())
        {
            scratch.resize(static_cast<size_t>(size));
        }

        readPacket(i, scratch.data());
        temp.writePacket(i, scratch.data(), size, static_cast<uint8_t>(type));
    }

    const std::string name = getFilename();
    const FileMode mode = fileMode;
    temp.close();
    close();
    std::error_code error;
    std::filesystem::copy_file(MCFileSystem::Resolve(tempName), MCFileSystem::ResolveWrite(name),
                               std::filesystem::copy_options::overwrite_existing, error);
    std::filesystem::remove(MCFileSystem::ResolveWrite(tempName), error);
    open(name.c_str(), mode, 50);
    seekPacket(packet);
    return result;
}

int32_t PacketFile::writePacket(int32_t packet, uint8_t* buffer)
{
    if (packet < 0 || packet >= numPackets)
    {
        return 0;
    }

    seekPacket(packet);

    if (packetType == STORAGE_TYPE_LZD || packetType == STORAGE_TYPE_HF)
    {
        return PACKET_WRONG_STORAGE;
    }

    const int32_t written = write(buffer, packetSize);
    return written != packetUnpackedSize ? WRITE_ERR : NO_ERR;
}
