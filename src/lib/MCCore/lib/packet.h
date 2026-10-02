#pragma once

#include "lib/file.h"

/// <summary>How a packet is stored (the top 3 bits of its offset in the packet table).</summary>
enum PacketStorage : uint8_t
{
    STORAGE_TYPE_RAW = 0,
    STORAGE_TYPE_FWF = 1,
    STORAGE_TYPE_LZD = 2,
    STORAGE_TYPE_HF = 3,
    STORAGE_TYPE_ZLIB = 4,
    STORAGE_TYPE_NUL = 7
};

/// <summary>
/// A file of numbered packets (PAK, MPK, ...): a header, a table of packet offsets whose top 3 bits give each
/// packet's storage, then the packets, each raw or LZ-packed (preceded by its unpacked size).
/// </summary>
/// <remarks>
/// Original source: <c>lib\packet.cpp</c>, 0x6c bytes. Layout: <c>int32 checksumOrMagic</c> (0xFEEDFACE when
/// unchecked), <c>int32 firstPacketOffset</c> (so the table has <c>firstPacketOffset / 4 - 2</c> entries), the table,
/// then the data.
/// </remarks>
class PacketFile : public File
{
public:
    /// <summary>The first word of a packet file whose checksum isn't checked.</summary>
    static constexpr int32_t PACKET_FILE_MAGIC = static_cast<int32_t>(0xFEEDFACE);

    /// <remarks>MCX.EXE @ 0x0064d110</remarks>
    PacketFile();
    /// <remarks>MCX.EXE @ 0x0064d150</remarks>
    ~PacketFile() override;

    /// <remarks>MCX.EXE @ 0x0064d170</remarks>
    int32_t open(const char* fName, FileMode _mode = READ, int32_t numChildren = 50) override;
    /// <remarks>MCX.EXE @ 0x0064d1a0</remarks>
    int32_t open(File* _parent, uint32_t fileSize, int32_t numChildren = 50) override;
    /// <remarks>MCX.EXE @ 0x0064d1d0</remarks>
    int32_t create(const char* fName) override;
    /// <summary>Finishes the packet table (when writing) and closes.</summary>
    /// <remarks>MCX.EXE @ 0x0064d1f0</remarks>
    void close() override;
    /// <remarks>MCX.EXE @ 0x0064d140</remarks>
    FileClass getFileClass() override { return PACKETFILE; }

    /// <summary>
    /// The offset of packet <paramref name="packet"/>, and its storage in <paramref name="packetType"/>; -1 past the
    /// end.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0064d210</remarks>
    int32_t readPacketOffset(int32_t packet, int32_t* packetType = nullptr);

    /// <summary>Reads (and unpacks) packet <paramref name="packet"/> into <paramref name="buffer"/>; -1 = the current one.</summary>
    /// <returns>Its unpacked size, or 0 on failure.</returns>
    /// <remarks>MCX.EXE @ 0x0064d250</remarks>
    int32_t readPacket(int32_t packet, uint8_t* buffer);

    /// <summary>Reads a packet as it is stored, without unpacking it.</summary>
    /// <remarks>MCX.EXE @ 0x0064d3a0</remarks>
    int32_t readPackedPacket(int32_t packet, uint8_t* buffer);

    /// <summary>Makes <paramref name="packet"/> current and moves to its data.</summary>
    /// <remarks>MCX.EXE @ 0x0064d430</remarks>
    int32_t seekPacket(int32_t packet);

    /// <summary>The next packet (stays on the last).</summary>
    /// <remarks>MCX.EXE @ 0x0064d4e0</remarks>
    void operator++();

    /// <summary>The previous packet (stays on the first).</summary>
    /// <remarks>MCX.EXE @ 0x0064d500</remarks>
    void operator--();

    /// <remarks>MCX.EXE @ 0x0064d520</remarks>
    int32_t getNumPackets() const { return numPackets; }
    /// <remarks>MCX.EXE @ 0x0064d530</remarks>
    int32_t getCurrentPacket() const { return currentPacket; }
    /// <summary>The current packet's unpacked size.</summary>
    /// <remarks>MCX.EXE @ 0x0064d540</remarks>
    int32_t getPacketSize() const { return packetUnpackedSize; }
    /// <summary>The current packet's stored size.</summary>
    /// <remarks>MCX.EXE @ 0x0064d550</remarks>
    int32_t getPackedPacketSize() const { return packetSize; }
    /// <summary>The current packet's storage (<see cref="PacketStorage"/>).</summary>
    /// <remarks>MCX.EXE @ 0x0064d560</remarks>
    int32_t getStorageType() const { return packetType; }

    /// <summary>
    /// Starts a new file of <paramref name="count"/> packets: writes the header and an empty table.
    /// <paramref name="useCheckSum"/> makes <see cref="close"/> write a checksum instead of the magic.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0064d570</remarks>
    void reserve(int32_t count, int useCheckSum = 0);

    /// <summary>
    /// Appends packet <paramref name="packet"/> with <paramref name="storageType"/> (LZD packs it; NUL means raw).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0064d620</remarks>
    int32_t writePacket(int32_t packet, uint8_t* buffer, int32_t nbytes, uint8_t storageType);

    /// <summary>Rewrites the file with a packet inserted (or replaced) at <paramref name="packet"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0064d7f0</remarks>
    int32_t insertPacket(int32_t packet, uint8_t* buffer, int32_t nbytes, uint8_t storageType);

    /// <summary>Overwrites a raw packet in place.</summary>
    /// <remarks>MCX.EXE @ 0x0064d980</remarks>
    int32_t writePacket(int32_t packet, uint8_t* buffer);

protected:
    /// <remarks>MCX.EXE @ 0x0064ce50</remarks>
    void clear();
    /// <summary>Resolves the table's forward references and writes the checksum, if the file was written.</summary>
    /// <remarks>MCX.EXE @ 0x0064ce80</remarks>
    void atClose();
    /// <summary>Checks the header and reads the packet table.</summary>
    /// <remarks>MCX.EXE @ 0x0064d050</remarks>
    int32_t afterOpen();
    /// <summary>The sum of every byte after the first word.</summary>
    /// <remarks>MCX.EXE @ 0x0064cfb0</remarks>
    int32_t checkSumFile();

    int32_t numPackets = 0;     // +0x4c
    int32_t currentPacket = -1; // +0x50
    /// <summary>The current packet's stored size.</summary>
    int32_t packetSize = 0; // +0x54
    /// <summary>The current packet's offset.</summary>
    int32_t packetBase = 0;         // +0x58
    int32_t packetType = 0;         // +0x5c
    int32_t packetUnpackedSize = 0; // +0x60
    /// <summary>The packet table, read into memory.</summary>
    int32_t* seekTable = nullptr; // +0x64
    /// <summary>Nonzero when the header holds a checksum.</summary>
    int32_t usesCheckSum = 0; // +0x68
};
