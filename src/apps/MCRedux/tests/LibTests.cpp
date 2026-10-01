#include "stdafx.h"
#include "MCTest.h"
#include "TestGame.h"
#include "lib/fastfile.h"
#include "lib/ffile.h"
#include "lib/file.h"
#include "lib/lzcomp.h"
#include "lib/lzdecomp.h"
#include "lib/packet.h"

namespace
{
    std::vector<uint8_t> RoundTrip(const std::vector<uint8_t>& data)
    {
        std::vector<uint8_t> packed(data.size() * 2 + 16);
        const int32_t packedSize = LZCompress(packed.data(), data.data(), static_cast<uint32_t>(data.size()));
        std::vector<uint8_t> unpacked(data.size() + 16);
        const int32_t size = LZDecomp(unpacked.data(), packed.data(), static_cast<uint32_t>(packedSize),
                                      static_cast<uint32_t>(unpacked.size()));
        unpacked.resize(static_cast<size_t>(size));
        return unpacked;
    }
}

TEST_CASE("lz: packing then unpacking gives the input back")
{
    std::vector<uint8_t> text;

    for (int i = 0; i < 20000; ++i)
    {
        text.push_back(static_cast<uint8_t>("MechCommander "[i % 14]));
    }

    CHECK(RoundTrip(text) == text);

    // Random bytes fill the dictionary and force clears at every code width.
    std::vector<uint8_t> noise(200000);
    uint32_t seed = 12345;

    for (uint8_t& b : noise)
    {
        seed = seed * 1103515245 + 12345;
        b = static_cast<uint8_t>(seed >> 16);
    }

    CHECK(RoundTrip(noise) == noise);

    const std::vector<uint8_t> one{42};
    CHECK(RoundTrip(one) == one);
    const std::vector<uint8_t> runs(5000, 7);
    CHECK(RoundTrip(runs) == runs);
}

TEST_CASE("game: every FastFile entry unpacks to its recorded size")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    int checked = 0;

    for (const char* name : {"ART.FST", "MISSION.FST", "MISC.FST", "SHAPES.FST", "TERRAIN.FST"})
    {
        MCTest::Scope scope(name);
        FastFile fst;
        REQUIRE_EQ(fst.open(name), 0);
        CHECK(fst.getNumFiles() > 0);
        std::vector<uint8_t> buffer;

        for (int32_t i = 0; i < fst.getNumFiles(); ++i)
        {
            const FILEENTRY* entry = fst.getEntry(i);
            MCTest::Scope entryScope(entry->name);
            const int32_t handle = fst.openFast(entry->name);
            REQUIRE(handle >= 0);
            buffer.assign(static_cast<size_t>(entry->realSize) + 1, 0);
            CHECK_EQ(fst.readFast(handle, buffer.data(), entry->realSize), entry->realSize);
            fst.closeFast(handle);
            ++checked;
        }
    }

    CHECK(checked > 1000);
}

TEST_CASE("game: File falls back to the FastFiles")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    MCTestGame::OpenFastFiles();
    File file;
    REQUIRE_EQ(file.open("data\\art\\access00.tga"), 0);
    CHECK(file.getLength() > 18);
    // A TGA header: no image id, no colour map or a colour map, type 1/2/9/10.
    const uint8_t idLength = file.readByte();
    const uint8_t mapType = file.readByte();
    const uint8_t imageType = file.readByte();
    CHECK(mapType <= 1);
    CHECK(imageType == 1 || imageType == 2 || imageType == 9 || imageType == 10);
    (void)idLength;
    file.close();
    CHECK(!file.isOpen());
}

TEST_CASE("game: packet files open and read")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    MCTestGame::OpenFastFiles();
    PacketFile pak;
    REQUIRE_EQ(pak.open("data\\art\\art.pak"), 0);
    CHECK(pak.getNumPackets() > 0);
    std::vector<uint8_t> buffer;

    for (int32_t i = 0; i < pak.getNumPackets(); ++i)
    {
        MCTest::Scope scope(std::format("packet {}", i));

        if (pak.seekPacket(i) != 0)
        {
            continue;
        }

        buffer.assign(static_cast<size_t>(pak.getPacketSize()) + 1, 0);
        CHECK_EQ(pak.readPacket(i, buffer.data()), pak.getPacketSize());
    }
}
