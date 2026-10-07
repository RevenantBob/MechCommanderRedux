#pragma once

#include "lib/file.h"

/// <summary>How a packet is stored (the top 3 bits of its offset in the packet table).</summary>
enum MCPacketStorage : uint8_t
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
class MCPacketFile : public MCFile
{
public:
    /// <summary>The first word of a packet file whose checksum isn't checked.</summary>
    static constexpr int32_t PACKET_FILE_MAGIC = static_cast<int32_t>(0xFEEDFACE);

    MCPacketFile();
    ~MCPacketFile() override;

    int32_t Open(const char* fName, MCFileMode mode = READ, int32_t numChildren = 50) override;
    int32_t Open(MCFile* parent, uint32_t fileSize, int32_t numChildren = 50) override;
    int32_t Create(const char* fName) override;
    /// <summary>Finishes the packet table (when writing) and closes.</summary>
    void Close() override;
    MCFileClass GetFileClass() override { return PACKETFILE; }

    /// <summary>
    /// The offset of packet <paramref name="packet"/>, and its storage in <paramref name="packetType"/>; -1 past the
    /// end.
    /// </summary>
    int32_t ReadPacketOffset(int32_t packet, int32_t* packetType = nullptr);

    /// <summary>Reads (and unpacks) packet <paramref name="packet"/> into <paramref name="buffer"/>; -1 = the current one.</summary>
    /// <returns>Its unpacked size, or 0 on failure.</returns>
    int32_t ReadPacket(int32_t packet, uint8_t* buffer);

    /// <summary>Reads a packet as it is stored, without unpacking it.</summary>
    int32_t ReadPackedPacket(int32_t packet, uint8_t* buffer);

    /// <summary>Makes <paramref name="packet"/> current and moves to its data.</summary>
    int32_t SeekPacket(int32_t packet);

    /// <summary>The next packet (stays on the last).</summary>
    void operator++();

    /// <summary>The previous packet (stays on the first).</summary>
    void operator--();

    int32_t GetNumPackets() const { return _NumPackets; }
    int32_t GetCurrentPacket() const { return _CurrentPacket; }
    /// <summary>The current packet's unpacked size.</summary>
    int32_t GetPacketSize() const { return _PacketUnpackedSize; }
    /// <summary>The current packet's stored size.</summary>
    int32_t GetPackedPacketSize() const { return _PacketSize; }
    /// <summary>The current packet's storage (<see cref="MCPacketStorage"/>).</summary>
    int32_t GetStorageType() const { return _PacketType; }

    /// <summary>
    /// Starts a new file of <paramref name="count"/> packets: writes the header and an empty table.
    /// <paramref name="useCheckSum"/> makes <see cref="Close"/> write a checksum instead of the magic.
    /// </summary>
    void Reserve(int32_t count, int useCheckSum = 0);

    /// <summary>
    /// Appends packet <paramref name="packet"/> with <paramref name="storageType"/> (LZD packs it; NUL means raw).
    /// </summary>
    int32_t WritePacket(int32_t packet, uint8_t* buffer, int32_t nbytes, uint8_t storageType);

    /// <summary>Rewrites the file with a packet inserted (or replaced) at <paramref name="packet"/>.</summary>
    int32_t InsertPacket(int32_t packet, uint8_t* buffer, int32_t nbytes, uint8_t storageType);

    /// <summary>Overwrites a raw packet in place.</summary>
    int32_t WritePacket(int32_t packet, uint8_t* buffer);

protected:
    void Clear();
    /// <summary>Resolves the table's forward references and writes the checksum, if the file was written.</summary>
    void AtClose();
    /// <summary>Checks the header and reads the packet table.</summary>
    int32_t AfterOpen();
    /// <summary>The sum of every byte after the first word.</summary>
    int32_t CheckSumFile();

    int32_t _NumPackets = 0;
    int32_t _CurrentPacket = -1;
    /// <summary>The current packet's stored size.</summary>
    int32_t _PacketSize = 0;
    /// <summary>The current packet's offset.</summary>
    int32_t _PacketBase = 0;
    int32_t _PacketType = 0;
    int32_t _PacketUnpackedSize = 0;
    /// <summary>The packet table, read into memory.</summary>
    int32_t* _SeekTable = nullptr;
    /// <summary>Nonzero when the header holds a checksum.</summary>
    int32_t _UsesCheckSum = 0;
};
