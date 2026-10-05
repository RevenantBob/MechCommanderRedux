#include "stdafx.h"
#include "MCTest.h"
#include "TestGame.h"
#include "lib/cident.h"
#include "lib/cvmath.h"
#include "lib/fastfile.h"
#include "lib/ffile.h"
#include "lib/hbtime.h"
#include "lib/inifile.h"
#include "lib/llist.h"
#include "lib/pqueue.h"
#include "lib/routines.h"

namespace
{
    /// <summary>A scratch path outside the game install.</summary>
    std::string ScratchPath(const char* name)
    {
        return (std::filesystem::temp_directory_path() / name).string();
    }

    /// <summary>Opens a FIT file and requires success.</summary>
    struct OpenFit
    {
        explicit OpenFit(const char* name) { Result = File.open(name); }

        FitIniFile File;
        int32_t Result = 0;
    };
}

TEST_CASE("pqueue: items come out smallest key first")
{
    PriorityQueue queue;
    REQUIRE_EQ(queue.init(16, -1000000), 0);
    const int32_t keys[] = {50, 10, 70, 30, 20, 60, 40};

    for (int32_t i = 0; i < 7; ++i)
    {
        PQNode node{keys[i], 100 + i, i, i};
        CHECK_EQ(queue.insert(node), 0);
    }

    CHECK(queue.find(102) > 0);
    CHECK_EQ(queue.find(999), 0);

    // Lower 70 to 5: it comes out first.
    queue.change(queue.find(102), 5);
    int32_t previous = INT32_MIN;
    std::vector<int32_t> ids;

    while (!queue.isEmpty())
    {
        PQNode node{};
        queue.remove(node);
        CHECK(node.key >= previous);
        previous = node.key;
        ids.push_back(node.id);
    }

    REQUIRE_EQ(ids.size(), 7u);
    CHECK_EQ(ids[0], 102);
    CHECK_EQ(ids[1], 101);
    queue.destroy();
}

TEST_CASE("llist: links are added, removed and counted")
{
    struct Counted : Link
    {
        explicit Counted(int* deaths) : Deaths(deaths) {}
        ~Counted() override { ++*Deaths; }
        int* Deaths;
    };

    int deaths = 0;
    {
        LinkedList list;
        Link* a = new Counted(&deaths);
        Link* b = new Counted(&deaths);
        Link* c = new Counted(&deaths);
        list.AddToTail(a);
        list.AddToTail(c);
        list.InsertAfter(a, b);
        list.AddToHead(new Counted(&deaths));
        CHECK_EQ(list.Count(), 4u);

        Link* walk = nullptr;
        int steps = 0;

        while (list.Traverse(walk))
        {
            ++steps;
        }

        CHECK_EQ(steps, 4);

        list.Destroy(b);
        CHECK_EQ(deaths, 1);
        CHECK_EQ(list.Count(), 3u);
        list.Remove(c);
        CHECK_EQ(list.Count(), 2u);
        delete c;
        CHECK_EQ(deaths, 2);
    }

    CHECK_EQ(deaths, 4);
}

TEST_CASE("hbtime: minutes convert to dates")
{
    HBDate date;
    CHECK_EQ(date.year, 3000);
    const HBDate start = date.LongToHBDate(0);
    CHECK_EQ(start.year, 3000);
    CHECK_EQ(start.month, 0);
    CHECK_EQ(start.day, 0);

    // 1 Feb + 2 days, 3:04.
    const int32_t minutes = 31 * 1440 + 2 * 1440 + 3 * 60 + 4;
    const HBDate feb = date.LongToHBDate(minutes);
    CHECK_EQ(feb.month, 1);
    CHECK_EQ(feb.day, 2);
    CHECK_EQ(feb.hour, 3);
    CHECK_EQ(feb.minute, 4);

    // Year 3000 is a leap year (divisible by 1000); 3100 isn't.
    CHECK(date.IsLeapYear(3000));
    CHECK(!date.IsLeapYear(3100));
    CHECK(date.IsLeapYear(3004));
    const HBDate nextYear = date.LongToHBDate(527040 + 60);
    CHECK_EQ(nextYear.year, 3001);
    CHECK_EQ(nextYear.hour, 1);
    CHECK(std::strncmp(date.GetShortMonth(2), "Mar", 3) == 0);
    CHECK(std::strncmp(date.GetLongMonth(8), "September", 9) == 0);
}

TEST_CASE("cvmath: vector products and random ranges")
{
    const vector_3d x(1.0f, 0.0f, 0.0f);
    const vector_3d y(0.0f, 1.0f, 0.0f);
    const vector_3d z = x & y;
    CHECK_EQ(z.x, 0.0f);
    CHECK_EQ(z.y, 0.0f);
    CHECK_EQ(z.z, 1.0f);
    CHECK_EQ(x | y, 0.0f);
    vector_3d v(3.0f, 4.0f, 0.0f);
    CHECK_EQ(v.magnitude(), 5.0f);
    v.normalize();
    CHECK_EQ(v.x, 0.6f);
    frame_of_ref frame;
    frame.reset_to_world_frame();
    CHECK_EQ(frame.k.z, 1.0f);
    CHECK_EQ(NULL_frame_of_ref.j.y, 1.0f);
    CHECK_EQ(frame.my_acos(2.0f), 0.0f);

    for (int i = 0; i < 1000; ++i)
    {
        const int32_t r = RandomNumber(10);
        CHECK(r >= 0 && r < 10);
        const int32_t s = SignedRandomNumber(5);
        CHECK(s >= -5 && s < 5);
    }

    CHECK_EQ(RollDice(0), 0);
    CHECK_EQ(RollDice(101), 1);
}

TEST_CASE("cident: ids and full paths")
{
    IDString id;
    id.init("ABCDEFGHIJ");
    CHECK(std::strcmp(id.id, "ABCDEFG") == 0);
    CHECK(id == "ABCDEFG");
    CHECK(!(id == "ABCDEFX"));
    CHECK(!(id == "ABC"));

    FullPathFileName path;
    path.init("data\\missions\\", "mcx0101", ".fit");
    CHECK(std::strcmp(path, "data\\missions\\mcx0101.fit") == 0);
    path.init("data\\", "x", nullptr);
    CHECK(std::strcmp(path, "data\\x") == 0);

    uint8_t bytes[13];
    memfill(bytes, 13);
    CHECK_EQ(bytes[12], 0xff);
    memclear(bytes + 1, 11);
    CHECK_EQ(bytes[0], 0xff);
    CHECK_EQ(bytes[5], 0);
    CHECK_EQ(bytes[12], 0xff);
}

TEST_CASE("inifile: a written FIT file reads back")
{
    const std::string path = ScratchPath("mc_inifile_test.fit");
    {
        FitIniFile out;
        REQUIRE_EQ(out.create(path.c_str()), 0);
        out.writeBlock("First");
        out.writeIdLong("Count", -42);
        out.writeIdULong("Big", 4000000000u);
        out.writeIdFloat("Scale", 2.5f);
        out.writeIdBoolean("On", 1);
        out.writeIdBoolean("Off", 0);
        out.writeIdString("Name", "Jade Falcon");
        out.writeIdShort("Short", -7);
        out.writeIdUShort("UShort", 65000);
        out.writeIdChar("Char", -3);
        out.writeIdUChar("UChar", 200);
        out.writeBlock("Arrays");
        const int32_t longs[] = {1, -2, 3, 40000};
        out.writeIdLongArray("Longs", longs, 4);
        const float floats[] = {0.25f, -1.5f, 100.0f};
        out.writeIdFloatArray("Floats", floats, 3);
        const uint8_t bytes[] = {0, 7, 255};
        out.writeIdUCharArray("Bytes", bytes, 3);
        const uint16_t shorts[] = {9, 65535};
        out.writeIdUShortArray("Shorts", shorts, 2);
        out.close();
    }

    FitIniFile in;
    REQUIRE_EQ(in.open(path.c_str()), 0);
    CHECK_EQ(in.getNumBlocks(), 2);
    CHECK_EQ(in.seekBlock("Missing"), BLOCK_NOT_FOUND);
    CHECK_EQ(in.seekBlock("first"), BLOCK_NOT_FOUND);
    REQUIRE_EQ(in.seekBlock("First"), 0);

    int32_t count = 0;
    CHECK_EQ(in.readIdLong("Count", count), 0);
    CHECK_EQ(count, -42);
    CHECK_EQ(in.readIdLong("count", count), 0); // names ignore case
    uint32_t big = 0;
    CHECK_EQ(in.readIdULong("Big", big), 0);
    CHECK_EQ(big, 4000000000u);
    float scale = 0.0f;
    CHECK_EQ(in.readIdFloat("Scale", scale), 0);
    CHECK_EQ(scale, 2.5f);
    int on = 0;
    int off = 1;
    CHECK_EQ(in.readIdBoolean("On", on), 0);
    CHECK_EQ(in.readIdBoolean("Off", off), 0);
    CHECK_EQ(on, 1);
    CHECK_EQ(off, 0);
    char name[32] = {};
    CHECK_EQ(in.readIdString("Name", name, sizeof(name)), 0);
    CHECK(std::strcmp(name, "Jade Falcon") == 0);
    CHECK_EQ(in.getIdStringLength("Name"), 12);
    char tiny[4];
    CHECK_EQ(in.readIdString("Name", tiny, sizeof(tiny)), BUFFER_TOO_SMALL);
    int16_t s = 0;
    CHECK_EQ(in.readIdShort("Short", s), 0);
    CHECK_EQ(s, -7);
    uint16_t us = 0;
    CHECK_EQ(in.readIdUShort("UShort", us), 0);
    CHECK_EQ(us, 65000);
    char c = 0;
    CHECK_EQ(in.readIdChar("Char", c), 0);
    CHECK_EQ(c, -3);
    uint8_t uc = 0;
    CHECK_EQ(in.readIdUChar("UChar", uc), 0);
    CHECK_EQ(uc, 200);
    count = 5;
    CHECK_EQ(in.readIdLong("Scale", count), VARIABLE_NOT_FOUND); // "f Scale" isn't an "l" entry
    CHECK_EQ(count, 0);

    REQUIRE_EQ(in.seekBlock("Arrays"), 0);
    CHECK_EQ(in.getIdLongArrayElements("Longs"), 4u);
    int32_t readLongs[4] = {};
    CHECK_EQ(in.readIdLongArray("Longs", readLongs, 4), 0);
    CHECK_EQ(readLongs[1], -2);
    CHECK_EQ(readLongs[3], 40000);
    int32_t small[2];
    CHECK_EQ(in.readIdLongArray("Longs", small, 2), USER_ARRAY_TOO_SMALL);
    float readFloats[3] = {};
    CHECK_EQ(in.readIdFloatArray("Floats", readFloats, 3), 0);
    CHECK_EQ(readFloats[0], 0.25f);
    CHECK_EQ(readFloats[1], -1.5f);
    CHECK_EQ(readFloats[2], 100.0f);
    uint8_t readBytes[3] = {};
    CHECK_EQ(in.readIdUCharArray("Bytes", readBytes, 3), 0);
    CHECK_EQ(readBytes[2], 255);
    uint16_t readShorts[2] = {};
    CHECK_EQ(in.readIdUShortArray("Shorts", readShorts, 2), 0);
    CHECK_EQ(readShorts[1], 65535);
    CHECK_EQ(in.getIdFloatArrayElements("Nothing"), static_cast<uint32_t>(VARIABLE_NOT_FOUND));
    in.close();

    // The file is laid out as the game writes it.
    File raw;
    REQUIRE_EQ(raw.open(path.c_str()), 0);
    std::string text(raw.getLength(), '\0');
    raw.read(reinterpret_cast<uint8_t*>(text.data()), static_cast<int32_t>(text.size()));
    raw.close();
    CHECK(text.starts_with("FITini \r\n\r\n[First]\r\nl Count=-42\r\n"));
    CHECK(text.find("ul Big=-294967296\r\n") != std::string::npos);
    CHECK(text.find("l[4] Longs=1,-2,3,40000,\r\n") != std::string::npos);
    CHECK(text.find("f[3] Floats=0.25, -1.50, 100.00,\r\n") != std::string::npos);
    CHECK(text.find("uc[3] Bytes=0, 7, 255,\r\n") != std::string::npos);
    CHECK(text.ends_with("FITend \r\n"));
    std::filesystem::remove(path);
}

TEST_CASE("inifile: hex values and malformed files")
{
    const std::string path = ScratchPath("mc_inifile_hex.fit");
    {
        std::ofstream out(path, std::ios::binary);
        out << "FITini\r\n[Hex]\r\nl Mask = 0x1F  // comment\r\nul Color=0xffAA00\r\nl Word = Null\r\n"
               "l[3] Split = 1, 2\r\n  3\r\nl NoEquals 5\r\n\r\n[Next]\r\nFITend\r\n";
    }

    FitIniFile in;
    REQUIRE_EQ(in.open(path.c_str()), 0);
    REQUIRE_EQ(in.seekBlock("Hex"), 0);
    int32_t mask = 0;
    CHECK_EQ(in.readIdLong("Mask", mask), 0);
    CHECK_EQ(mask, 0x1f);
    uint32_t color = 0;
    CHECK_EQ(in.readIdULong("Color", color), 0);
    CHECK_EQ(color, 0xffaa00u);
    int32_t word = 7;
    CHECK_EQ(in.readIdLong("Word", word), 0);
    CHECK_EQ(word, 0);
    int32_t split[3] = {};
    CHECK_EQ(in.readIdLongArray("Split", split, 3), 0);
    CHECK_EQ(split[2], 3);
    int32_t noEquals = 0;
    CHECK_EQ(in.readIdLong("NoEquals", noEquals), VARIABLE_NOT_FOUND);
    in.close();

    {
        std::ofstream out(path, std::ios::binary);
        out << "Not a fit file\r\n[Block]\r\n";
    }

    FitIniFile bad;
    CHECK_EQ(bad.open(path.c_str()), NOT_A_FITINIFILE);
    bad.close();
    std::filesystem::remove(path);
}

TEST_CASE("game: SYSTEM.CFG and PREFS.CFG read as the game reads them")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    OpenFit system("system.cfg");
    REQUIRE_EQ(system.Result, 0);
    REQUIRE_EQ(system.File.seekBlock("systemHeap"), 0);
    uint32_t size = 0;
    CHECK_EQ(system.File.readIdULong("systemHeapSize", size), 0);
    CHECK_EQ(size, 16383999u);
    CHECK_EQ(system.File.readIdULong("logisticsHeapSize", size), 0);
    CHECK_EQ(size, 33554430u);

    REQUIRE_EQ(system.File.seekBlock("ABL"), 0);
    CHECK_EQ(system.File.readIdULong("SymbolTableHeapSize", size), 0);
    CHECK_EQ(size, 4095999u);
    int32_t watches = 0;
    CHECK_EQ(system.File.readIdLong("MaxWatchesPerModule", watches), 0);
    CHECK_EQ(watches, 20);

    REQUIRE_EQ(system.File.seekBlock("FastFiles"), 0);
    int32_t numFastFiles = 0;
    CHECK_EQ(system.File.readIdLong("NumFastFiles", numFastFiles), 0);
    CHECK_EQ(numFastFiles, 5);
    char name[80] = {};
    CHECK_EQ(system.File.readIdString("File0", name, sizeof(name)), 0);
    CHECK(std::strcmp(name, "art.fst") == 0);
    CHECK_EQ(system.File.readIdString("File4", name, sizeof(name)), 0); // spelled "FIle4" in the file
    CHECK(std::strcmp(name, "shapes.fst") == 0);

    REQUIRE_EQ(system.File.seekBlock("systemPaths"), 0);
    CHECK_EQ(system.File.readIdString("missionPath", name, sizeof(name)), 0);
    CHECK(std::strcmp(name, "data\\missions\\") == 0);
    CHECK_EQ(system.File.seekBlock("UseSound"), 0);

    OpenFit prefs("prefs.cfg");
    REQUIRE_EQ(prefs.Result, 0);
    REQUIRE_EQ(prefs.File.seekBlock("MechCommander"), 0);
    int flag = 0;
    CHECK_EQ(prefs.File.readIdBoolean("DirectDraw", flag), 0);
    CHECK_EQ(flag, 1);
    CHECK_EQ(prefs.File.readIdBoolean("Force16Mb", flag), 0);
    CHECK_EQ(flag, 0);
    int32_t difficulty = -1;
    CHECK_EQ(prefs.File.readIdLong("Difficulty", difficulty), 0);
    CHECK(difficulty >= 0 && difficulty <= 3);
    int32_t volume = -1;
    CHECK_EQ(prefs.File.readIdLong("SFXVolume", volume), 0);
    CHECK(volume >= 0);
}

TEST_CASE("game: FIT files from the FastFiles read")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    MCTestGame::OpenFastFiles();

    {
        OpenFit mission("data\\missions\\MCX0101.FIT");
        REQUIRE_EQ(mission.Result, 0);
        FitIniFile& fit = mission.File;
        REQUIRE_EQ(fit.seekBlock("GameScale"), 0);
        float unitsPerMeter = 0.0f;
        CHECK_EQ(fit.readIdFloat("WorldUnitsPerMeter", unitsPerMeter), 0);
        CHECK_EQ(unitsPerMeter, 5.01f);
        uint32_t duration = 0;
        CHECK_EQ(fit.readIdULong("Duration", duration), 0);
        CHECK_EQ(duration, 60u);

        REQUIRE_EQ(fit.seekBlock("Artillery"), 0);
        int32_t strikes = -1;
        CHECK_EQ(fit.readIdLong("NumLargeStrikes", strikes), 0); // "Null"
        CHECK_EQ(strikes, 0);
        CHECK_EQ(fit.readIdLong("NumSmallStrikes", strikes), 0);
        CHECK_EQ(strikes, 4);

        REQUIRE_EQ(fit.seekBlock("Part1"), 0);
        uint32_t objectNumber = 0;
        CHECK_EQ(fit.readIdULong("ObjectNumber", objectNumber), 0);
        CHECK_EQ(objectNumber, 15u);
        int playerPart = 1;
        CHECK_EQ(fit.readIdBoolean("PlayerPart", playerPart), 0);
        CHECK_EQ(playerPart, 0);
        char team = 0;
        CHECK_EQ(fit.readIdChar("TeamID", team), 0);
        CHECK_EQ(team, 1);
        char profile[16] = {};
        CHECK_EQ(fit.readIdString("ObjectProfile", profile, sizeof(profile)), 0);
        CHECK(std::strcmp(profile, "PM200300") == 0);
        float x = 0.0f;
        float y = 0.0f;
        CHECK_EQ(fit.readIdFloat("PositionX", x), 0);
        CHECK_EQ(fit.readIdFloat("PositionY", y), 0);
        CHECK_EQ(x, 1530.0f);
        CHECK_EQ(y, -1233.0f);

        REQUIRE_EQ(fit.seekBlock("Commander1Group:0"), 0);
        CHECK_EQ(fit.getIdLongArrayElements("Mates"), 12u);
        int32_t mates[12] = {};
        CHECK_EQ(fit.readIdLongArray("Mates", mates, 12), 0);
        CHECK_EQ(mates[0], 1);
        CHECK_EQ(mates[4], 5);
        CHECK_EQ(mates[5], 0);
    }

    {
        OpenFit clan("data\\missions\\E3_0101.FIT");
        REQUIRE_EQ(clan.Result, 0);
        REQUIRE_EQ(clan.File.seekBlock("ClanStar:0"), 0);
        float withdraw[7] = {};
        CHECK_EQ(clan.File.readIdFloatArray("WithdrawThreshold", withdraw, 7), 0);
        CHECK_EQ(withdraw[0], 0.25f);
        CHECK_EQ(withdraw[4], 500.0f);
        CHECK_EQ(withdraw[5], -2000.0f);
        CHECK_EQ(withdraw[6], -1.0f);
        float zone[4] = {};
        CHECK_EQ(clan.File.readIdFloatArray("Zone1", zone, 4), 0); // ends in a // comment
        CHECK_EQ(zone[0], -135.0f);
        CHECK_EQ(zone[3], 90.0f);
        int32_t roles[5] = {};
        CHECK_EQ(clan.File.readIdLongArray("Roles", roles, 5), 0);
        CHECK_EQ(roles[0], 0);
        CHECK_EQ(roles[4], 1);
    }

    {
        OpenFit screen("data\\art\\lanscreen.fit");
        REQUIRE_EQ(screen.Result, 0);
        REQUIRE_EQ(screen.File.seekBlock("Element1"), 0);
        int32_t left = 0;
        CHECK_EQ(screen.File.readIdLong("Left", left), 0);
        CHECK_EQ(left, 549);
        char art[32] = {};
        CHECK_EQ(screen.File.readIdString("GreyArt", art, sizeof(art)), 0);
        CHECK(std::strcmp(art, "bn_cancl.tga") == 0);
        REQUIRE_EQ(screen.File.seekBlock("Element0"), 0);
        int back = 0;
        CHECK_EQ(screen.File.readIdBoolean("UseBackPalette", back), 0); // "T"
        CHECK_EQ(back, 1);
        REQUIRE_EQ(screen.File.seekBlock("Blocks"), 0);
        uint8_t block1[12] = {};
        CHECK_EQ(screen.File.getIdUCharArrayElements("Block1"), 12u);
        CHECK_EQ(screen.File.readIdUCharArray("Block1", block1, 12), 0); // "8, 9,10,11"
        CHECK_EQ(block1[7], 7);
        CHECK_EQ(block1[9], 9);
        CHECK_EQ(block1[11], 11);
    }

    {
        OpenFit palette("data\\palette\\palette.fit");
        REQUIRE_EQ(palette.Result, 0);
        REQUIRE_EQ(palette.File.seekBlock("Range2"), 0);
        uint8_t base = 0;
        CHECK_EQ(palette.File.readIdUChar("BaseColorIndex", base), 0);
        CHECK_EQ(base, 32);
        REQUIRE_EQ(palette.File.seekBlock("Ranges"), 0);
        int32_t ranges = 0;
        CHECK_EQ(palette.File.readIdLong("NumColorRanges", ranges), 0);
        CHECK_EQ(ranges, 9);
    }
}

TEST_CASE("game: every FIT file in the FastFiles opens and every block seeks")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    MCTestGame::OpenFastFiles();

    int opened = 0;
    int blocks = 0;

    for (int32_t f = 0; f < numFastFiles; ++f)
    {
        for (int32_t i = 0; i < fastFiles[f]->getNumFiles(); ++i)
        {
            const FILEENTRY* entry = fastFiles[f]->getEntry(i);
            const std::string_view name(entry->name);

            if (name.size() < 4 || MCPort::StrICmp(entry->name + name.size() - 4, ".fit") != 0)
            {
                continue;
            }

            MCTest::Scope scope(entry->name);
            FitIniFile fit;
            CHECK_EQ(fit.open(entry->name), 0);

            for (int32_t b = 0; b < fit.getNumBlocks(); ++b)
            {
                MCTest::Scope blockScope(fit.getBlockName(b));
                // A block named twice is found at its first copy; seeking still succeeds.
                CHECK_EQ(fit.seekBlock(fit.getBlockName(b)), 0);
                ++blocks;
            }

            fit.close();
            ++opened;
        }
    }

    CHECK(opened > 700);
    CHECK(blocks > 10000);
}
