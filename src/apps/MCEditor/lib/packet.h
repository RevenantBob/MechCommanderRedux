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

inline constexpr int32_t PACKET_OUT_OF_RANGE = static_cast<int32_t>(0xBADF0004);
inline constexpr int32_t BAD_PACKET_VERSION = static_cast<int32_t>(0xBADF0004);
inline constexpr int32_t PACKET_WRONG_SIZE = static_cast<int32_t>(0xBADF0005);
inline constexpr int32_t PACKET_WRONG_STORAGE = static_cast<int32_t>(0xBADF000B);
inline constexpr int32_t NO_RAM_FOR_SEEK_TABLE = static_cast<int32_t>(0xBADF000C);

/// <summary>
/// A file of numbered packets: a header, a table of packet offsets whose top 3 bits give each packet's storage, then
/// the packets, each raw or LZ-packed (preceded by its unpacked size).
/// </summary>
/// <remarks>
/// Original source: <c>lib\packet.cpp</c>, the editor's revision. Layout: <c>int32 checksumOrMagic</c> (0xFEEDFACE
/// when unchecked), <c>int32 firstPacketOffset</c> (so the table has <c>firstPacketOffset / 4 - 2</c> entries), the
/// table, then the data.
/// </remarks>
class PacketFile : public File
{
public:
    /// <summary>The first word of a packet file whose checksum isn't checked.</summary>
    static constexpr int32_t PACKET_FILE_MAGIC = static_cast<int32_t>(0xFEEDFACE);

    /// <summary>A closed packet file.</summary>
    PacketFile();
    /// <summary>Closes the file if it is open and frees <c>LZPacketBuffer</c>.</summary>
    ~PacketFile();

    /// <summary>Opens the file and reads its packet table.</summary>
    int32_t open(const char* fName, FileMode _mode = READ, int32_t numChildren = 50) override;
    /// <summary>Opens a child window as a packet file and reads its packet table.</summary>
    int32_t open(File* _parent, uint32_t fileSize, int32_t numChildren = 50) override;
    /// <summary>Creates the file; <see cref="reserve"/> starts its table.</summary>
    int32_t create(const char* fName) override;
    /// <summary>Finishes the packet table (when writing) and closes.</summary>
    void close() override;
    /// <summary>A packet file.</summary>
    FileClass getFileClass() override { return PACKETFILE; }

    /// <summary>
    /// The offset of packet <paramref name="packet"/>, and its storage in <paramref name="packetType"/>; -1 past the
    /// end.
    /// </summary>
    int32_t readPacketOffset(int32_t packet, int32_t* packetType = nullptr);

    /// <summary>Reads (and unpacks) packet <paramref name="packet"/> into <paramref name="buffer"/>; -1 = the current one.</summary>
    /// <returns>Its unpacked size, or 0 on failure.</returns>
    int32_t readPacket(int32_t packet, uint8_t* buffer);

    /// <summary>Reads a packet as it is stored, without unpacking it.</summary>
    /// <returns>Its size for a raw packet, 0 for a packed one.</returns>
    int32_t readPackedPacket(int32_t packet, uint8_t* buffer);

    /// <summary>Makes <paramref name="packet"/> current and moves to its data.</summary>
    int32_t seekPacket(int32_t packet);

    /// <summary>The next packet (stays on the last).</summary>
    void operator++();

    /// <summary>The previous packet (stays on the first).</summary>
    void operator--();

    /// <summary>The number of packets.</summary>
    int32_t getNumPackets() const { return numPackets; }
    /// <summary>The current packet, or -1.</summary>
    int32_t getCurrentPacket() const { return currentPacket; }
    /// <summary>The current packet's unpacked size.</summary>
    int32_t getPacketSize() const { return packetUnpackedSize; }
    /// <summary>The current packet's stored size.</summary>
    int32_t getPackedPacketSize() const { return packetSize; }
    /// <summary>The current packet's storage (<see cref="PacketStorage"/>).</summary>
    int32_t getStorageType() const { return packetType; }

    /// <summary>
    /// Starts a new file of <paramref name="count"/> packets: writes the header and an empty table.
    /// <paramref name="useCheckSum"/> makes <see cref="close"/> write a checksum instead of the magic.
    /// </summary>
    void reserve(int32_t count, int useCheckSum = 0);

    /// <summary>
    /// Appends packet <paramref name="packet"/> with <paramref name="storageType"/>: LZD packs it, NUL packs it if
    /// that's smaller, anything else stores it raw.
    /// </summary>
    /// <returns>The bytes written (not counting a packed packet's size word), or 0.</returns>
    /// <remarks>Packing checks itself by unpacking into <paramref name="buffer"/>, over the caller's data.</remarks>
    int32_t writePacket(int32_t packet, uint8_t* buffer, int32_t nbytes, uint8_t storageType);

    /// <summary>Rewrites the file with a packet inserted (or replaced) at <paramref name="packet"/>.</summary>
    /// <returns>The result of creating the temporary file.</returns>
    int32_t insertPacket(int32_t packet, uint8_t* buffer, int32_t nbytes, uint8_t storageType);

    /// <summary>Overwrites a raw packet in place.</summary>
    int32_t writePacket(int32_t packet, uint8_t* buffer);

protected:
    /// <summary>Forgets the packets and frees the table.</summary>
    void clear();
    /// <summary>Resolves the table's forward references and writes the checksum, if the file was written.</summary>
    void atClose();
    /// <summary>Checks the header and reads the packet table.</summary>
    int32_t afterOpen();
    /// <summary>The sum of every byte after the first word.</summary>
    int32_t checkSumFile();

    /// <summary>The number of packets.</summary>
    int32_t numPackets = 0;
    /// <summary>The current packet, or -1.</summary>
    int32_t currentPacket = -1;
    /// <summary>The current packet's stored size.</summary>
    int32_t packetSize = 0;
    /// <summary>The current packet's offset.</summary>
    int32_t packetBase = 0;
    /// <summary>The current packet's storage.</summary>
    int32_t packetType = 0;
    /// <summary>The current packet's unpacked size.</summary>
    int32_t packetUnpackedSize = 0;
    /// <summary>The packet table, read into memory.</summary>
    int32_t* seekTable = nullptr;
    /// <summary>Nonzero when the header holds a checksum.</summary>
    int32_t usesCheckSum = 0;
};
