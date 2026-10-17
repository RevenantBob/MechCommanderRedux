#include "stdafx.h"
#include "lib/packet.h"
#include "lib/aerror.h"
#include "lib/ffile.h"
#include "lib/lzcomp.h"
#include "lib/lzdecomp.h"

namespace
{
    constexpr uint32_t TYPE_SHIFT = 29;
    constexpr uint32_t OFFSET_MASK = 0x1fffffff;

    /// <summary>The table entry of a packet not written yet: storage 7 over the offset of the next written one.</summary>
    constexpr uint32_t PENDING_MARK = 0xe0000000;

    /// <summary>One table entry's storage step: the storage type times this, plus the offset.</summary>
    constexpr int32_t STORAGE_UNIT = 0x20000000;

    /// <summary>The header before the table: the checksum or magic, and the first packet's offset.</summary>
    constexpr int32_t HEADER_SIZE = 8;

    /// <summary>The smallest file that has a header and one table entry.</summary>
    constexpr uint32_t MIN_PACKET_FILE_SIZE = 12;

    /// <summary>The temporary file <c>insertPacket</c> rebuilds the file in.</summary>
    constexpr const char* INSERT_TEMP_NAME = "AF3456AF.788";

    /// <summary><c>insertPacket</c>'s scratch buffer, grown for bigger packets.</summary>
    constexpr int32_t INSERT_SCRATCH_SIZE = 0xffff;
}

PacketFile::PacketFile()
{
    clear();
}

PacketFile::~PacketFile()
{
    if (isOpen())
    {
        PacketFile::close();
    }

    std::free(LZPacketBuffer);
    LZPacketBuffer = nullptr;
}

void PacketFile::clear()
{
    currentPacket = -1;
    numPackets = 0;
    packetBase = 0;
    packetSize = 0;
    std::free(seekTable);
    seekTable = nullptr;
}

void PacketFile::atClose()
{
    if (fileMode != READ && fileMode != NOMODE)
    {
        // Packets never written point at the next written packet, or at the end.
        uint32_t next = getLength();
        currentPacket = numPackets;

        if (seekTable == nullptr)
        {
            for (currentPacket = numPackets - 1; currentPacket >= 0; --currentPacket)
            {
                seek(currentPacket * 4 + HEADER_SIZE);
                const uint32_t entry = static_cast<uint32_t>(readLong());

                if ((entry & PENDING_MARK) == PENDING_MARK)
                {
                    seek(currentPacket * 4 + HEADER_SIZE);
                    writeLong(static_cast<int32_t>(next + PENDING_MARK));
                }
                else
                {
                    next = entry & OFFSET_MASK;
                }
            }
        }
        else
        {
            for (currentPacket = numPackets - 1; currentPacket >= 0; --currentPacket)
            {
                const uint32_t entry = static_cast<uint32_t>(seekTable[currentPacket]);

                if ((entry & PENDING_MARK) == PENDING_MARK)
                {
                    seekTable[currentPacket] = static_cast<int32_t>(next + PENDING_MARK);
                }
                else
                {
                    next = entry & OFFSET_MASK;
                }
            }
        }

        if (seekTable != nullptr)
        {
            seek(HEADER_SIZE);
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
    auto bytes = static_cast<uint8_t*>(std::malloc(fileSize()));
    read(bytes, static_cast<int32_t>(fileSize()));
    int32_t sum = 0;

    for (uint32_t i = 0; i + 4 < fileSize(); ++i)
    {
        sum += bytes[i];
    }

    std::free(bytes);
    seek(static_cast<int32_t>(saved));
    return sum;
}

int32_t PacketFile::afterOpen()
{
    usesCheckSum = 0;

    if (numPackets == 0 && getLength() >= MIN_PACKET_FILE_SIZE)
    {
        const int32_t firstWord = readLong();

        if (firstWord != PACKET_FILE_MAGIC)
        {
            usesCheckSum = 1;

            if (checkSumFile() != firstWord)
            {
                return BAD_PACKET_VERSION;
            }
        }

        const uint32_t firstPacket = static_cast<uint32_t>(readLong());
        numPackets = static_cast<int32_t>(firstPacket / 4) - 2;
    }

    currentPacket = -1;

    if ((fileMode == READ || fileMode == RDWRITE) && numPackets != 0 && seekTable == nullptr)
    {
        seekTable = static_cast<int32_t*>(std::malloc(sizeof(int32_t) * static_cast<size_t>(numPackets)));

        if (seekTable == nullptr)
        {
            return NO_RAM_FOR_SEEK_TABLE;
        }

        seek(HEADER_SIZE);
        read(reinterpret_cast<uint8_t*>(seekTable), numPackets * 4);
    }

    return NO_ERR;
}

int32_t PacketFile::open(const char* fName, FileMode _mode, int32_t numChild)
{
    const int32_t result = File::open(fName, _mode, numChild);

    if (result != NO_ERR)
    {
        return result;
    }

    return afterOpen();
}

int32_t PacketFile::open(File* _parent, uint32_t fileSize, int32_t numChild)
{
    const int32_t result = File::open(_parent, fileSize, numChild);

    if (result != NO_ERR)
    {
        return result;
    }

    return afterOpen();
}

int32_t PacketFile::create(const char* fName)
{
    // File::create opens through the virtual open, so afterOpen runs twice; the second time finds nothing to do.
    const int32_t result = File::create(fName);

    if (result != NO_ERR)
    {
        return result;
    }

    return afterOpen();
}

void PacketFile::close()
{
    atClose();
    File::close();
    usesCheckSum = 0;
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
            *type = static_cast<int32_t>(offset >> TYPE_SHIFT);
        }

        offset &= OFFSET_MASK;
    }

    return static_cast<int32_t>(offset);
}

int32_t PacketFile::readPacket(int32_t packet, uint8_t* buffer)
{
    if (packet != -1 && packet != currentPacket && seekPacket(packet) != NO_ERR)
    {
        return 0;
    }

    if (packetType == STORAGE_TYPE_RAW || packetType == STORAGE_TYPE_FWF)
    {
        seek(packetBase);
        return read(buffer, packetSize);
    }

    if (packetType != STORAGE_TYPE_LZD)
    {
        return 0;
    }

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

    read(LZPacketBuffer, packetSize - 4);
    const int32_t unpacked = LZDecomp(buffer, LZPacketBuffer, static_cast<uint32_t>(packetSize - 4));
    return unpacked == packetUnpackedSize ? unpacked : 0;
}

int32_t PacketFile::readPackedPacket(int32_t packet, uint8_t* buffer)
{
    if (packet != -1 && packet != currentPacket && seekPacket(packet) != NO_ERR)
    {
        return 0;
    }

    if (packetType == STORAGE_TYPE_RAW || packetType == STORAGE_TYPE_FWF)
    {
        seek(packetBase);
        return read(buffer, packetSize);
    }

    if (packetType == STORAGE_TYPE_LZD)
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

    switch (packetType)
    {
        case STORAGE_TYPE_RAW:
        {
            packetUnpackedSize = packetSize;
            break;
        }

        case STORAGE_TYPE_LZD:
        {
            packetUnpackedSize = readLong();
            break;
        }

        case STORAGE_TYPE_NUL:
        {
            packetUnpackedSize = 0;
            break;
        }

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
    const int32_t firstPacket = count * 4 + HEADER_SIZE;
    writeLong(PACKET_FILE_MAGIC);
    writeLong(firstPacket);

    for (int32_t i = 0; i < count; ++i)
    {
        writeLong(static_cast<int32_t>(static_cast<uint32_t>(firstPacket) + PENDING_MARK));
    }

    if (seekTable == nullptr)
    {
        seekTable = static_cast<int32_t*>(std::malloc(sizeof(int32_t) * static_cast<size_t>(numPackets)));

        if (seekTable != nullptr)
        {
            seek(HEADER_SIZE);
            read(reinterpret_cast<uint8_t*>(seekTable), numPackets * 4);
        }
    }
}

int32_t PacketFile::writePacket(int32_t packet, uint8_t* buffer, int32_t nbytes, uint8_t storageType)
{
    uint8_t* packed = nullptr;

    if (storageType == STORAGE_TYPE_NUL || storageType == STORAGE_TYPE_LZD)
    {
        // Original behaviour (EB-006): twice nbytes, too small for packets of a few bytes.
        packed = static_cast<uint8_t*>(
            std::malloc((std::max<size_t>)(static_cast<size_t>(nbytes) * 2, LZCompressBound(nbytes))));

        if (packed == nullptr)
        {
            return 0;
        }
    }

    if (packet < 0 || packet >= numPackets)
    {
        std::free(packed);
        return 0;
    }

    packetBase = static_cast<int32_t>(getLength());
    currentPacket = packet;
    packetUnpackedSize = nbytes;
    packetSize = nbytes;
    uint8_t type = storageType;

    if (type == STORAGE_TYPE_NUL || type == STORAGE_TYPE_LZD)
    {
        // NUL asks for "packed if it helps"; LZD always packs.
        const bool forcePack = type == STORAGE_TYPE_LZD;
        type = STORAGE_TYPE_RAW;
        const int32_t packedSize = LZCompress(packed, buffer, nbytes);

        if (packedSize == -1)
        {
            std::free(packed);
            return 0;
        }

        if (LZDecomp(buffer, packed, static_cast<uint32_t>(packedSize)) != nbytes)
        {
            Fatal(-1, " Holy Dogshit, Private Pyle ");
        }

        if (forcePack || packedSize < nbytes)
        {
            type = STORAGE_TYPE_LZD;
            packetSize = packedSize;
        }
    }

    packetType = type;
    seek(packetBase);
    int32_t written = 0;

    if (packetType == STORAGE_TYPE_LZD)
    {
        writeLong(packetUnpackedSize);
        written = write(packed, packetSize);
    }
    else
    {
        written = write(buffer, packetSize);
    }

    const int32_t entry = packetType * STORAGE_UNIT + packetBase;

    if (seekTable == nullptr)
    {
        seek(packet * 4 + HEADER_SIZE);
        writeLong(entry);
    }
    else
    {
        seekTable[packet] = entry;
    }

    // Later packets not written yet point at the new end. Original behaviour: on disk this rewrites entries
    // packet+1 .. numPackets-1 (following the entry just written), in the table packet+1 .. numPackets-2.
    if (seekTable != nullptr)
    {
        ++packet;
    }

    const uint32_t end = getLength();

    for (; packet < numPackets - 1; ++packet)
    {
        if (seekTable == nullptr)
        {
            writeLong(static_cast<int32_t>(end + PENDING_MARK));
        }
        else
        {
            seekTable[packet] = static_cast<int32_t>(end + PENDING_MARK);
        }
    }

    std::free(packed);
    return written;
}

int32_t PacketFile::insertPacket(int32_t packet, uint8_t* buffer, int32_t nbytes, uint8_t storageType)
{
    if (packet < 0)
    {
        return 0;
    }

    auto scratch = static_cast<uint8_t*>(std::malloc(INSERT_SCRATCH_SIZE));
    PacketFile temp;
    const int32_t result = temp.create(INSERT_TEMP_NAME);

    if (packet >= numPackets)
    {
        ++numPackets;
    }

    temp.reserve(numPackets);

    for (int32_t i = 0; i < numPackets; ++i)
    {
        if (i == packet)
        {
            // Original behaviour (EB-007): a new packet over 0xfffe bytes reallocated the scratch buffer to the
            // current packet's stored size, which the next packets were then read into.
            temp.writePacket(i, buffer, nbytes, storageType);
            continue;
        }

        seekPacket(i);
        const int32_t size = packetUnpackedSize;
        const int32_t type = packetType;

        if (size >= INSERT_SCRATCH_SIZE)
        {
            std::free(scratch);
            scratch = static_cast<uint8_t*>(std::malloc(static_cast<size_t>(size)));
        }

        readPacket(i, scratch);
        temp.writePacket(i, scratch, size, static_cast<uint8_t>(type));
    }

    std::free(scratch);
    const std::string name = getFilename();
    const FileMode mode = fileMode;
    temp.PacketFile::close();
    close();
    CopyFileA(INSERT_TEMP_NAME, name.c_str(), FALSE);
    DeleteFileA(INSERT_TEMP_NAME);
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
        return PACKET_WRONG_SIZE;
    }

    const int32_t written = write(buffer, packetSize);
    return written != packetUnpackedSize ? WRITE_ERR : NO_ERR;
}
