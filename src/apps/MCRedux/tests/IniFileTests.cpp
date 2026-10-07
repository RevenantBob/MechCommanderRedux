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
        explicit OpenFit(const char* name) { Result = File.Open(name); }

        MCFitIniFile File;
        int32_t Result = 0;
    };
}

TEST_CASE("pqueue: items come out smallest key first")
{
    MCPriorityQueue queue;
    REQUIRE_EQ(queue.Init(16, -1000000), 0);
    const int32_t keys[] = {50, 10, 70, 30, 20, 60, 40};

    for (int32_t i = 0; i < 7; ++i)
    {
        MCPQNode node{keys[i], 100 + i, i, i};
        CHECK_EQ(queue.Insert(node), 0);
    }

    CHECK(queue.Find(102) > 0);
    CHECK_EQ(queue.Find(999), 0);

    // Lower 70 to 5: it comes out first.
    queue.Change(queue.Find(102), 5);
    int32_t previous = INT32_MIN;
    std::vector<int32_t> ids;

    while (!queue.IsEmpty())
    {
        MCPQNode node{};
        queue.Remove(node);
        CHECK(node.Key >= previous);
        previous = node.Key;
        ids.push_back(node.Id);
    }

    REQUIRE_EQ(ids.size(), 7u);
    CHECK_EQ(ids[0], 102);
    CHECK_EQ(ids[1], 101);
    queue.Destroy();
}

TEST_CASE("llist: links are added, removed and counted")
{
    struct Counted : MCLink
    {
        explicit Counted(int* deaths) : Deaths(deaths) {}
        ~Counted() override { ++*Deaths; }
        int* Deaths;
    };

    int deaths = 0;
    {
        MCLinkedList list;
        MCLink* a = new Counted(&deaths);
        MCLink* b = new Counted(&deaths);
        MCLink* c = new Counted(&deaths);
        list.AddToTail(a);
        list.AddToTail(c);
        list.InsertAfter(a, b);
        list.AddToHead(new Counted(&deaths));
        CHECK_EQ(list.Count(), 4u);

        MCLink* walk = nullptr;
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
    MCHBDate date;
    CHECK_EQ(date.Year, 3000);
    const MCHBDate start = date.LongToHBDate(0);
    CHECK_EQ(start.Year, 3000);
    CHECK_EQ(start.Month, 0);
    CHECK_EQ(start.Day, 0);

    // 1 Feb + 2 days, 3:04.
    const int32_t minutes = 31 * 1440 + 2 * 1440 + 3 * 60 + 4;
    const MCHBDate feb = date.LongToHBDate(minutes);
    CHECK_EQ(feb.Month, 1);
    CHECK_EQ(feb.Day, 2);
    CHECK_EQ(feb.Hour, 3);
    CHECK_EQ(feb.Minute, 4);

    // Year 3000 is a leap year (divisible by 1000); 3100 isn't.
    CHECK(date.IsLeapYear(3000));
    CHECK(!date.IsLeapYear(3100));
    CHECK(date.IsLeapYear(3004));
    const MCHBDate nextYear = date.LongToHBDate(527040 + 60);
    CHECK_EQ(nextYear.Year, 3001);
    CHECK_EQ(nextYear.Hour, 1);
    CHECK(std::strncmp(date.GetShortMonth(2), "Mar", 3) == 0);
    CHECK(std::strncmp(date.GetLongMonth(8), "September", 9) == 0);
}

TEST_CASE("cvmath: vector products and random ranges")
{
    const MCVector3D x(1.0f, 0.0f, 0.0f);
    const MCVector3D y(0.0f, 1.0f, 0.0f);
    const MCVector3D z = x & y;
    CHECK_EQ(z.X, 0.0f);
    CHECK_EQ(z.Y, 0.0f);
    CHECK_EQ(z.Z, 1.0f);
    CHECK_EQ(x | y, 0.0f);
    MCVector3D v(3.0f, 4.0f, 0.0f);
    CHECK_EQ(v.Magnitude(), 5.0f);
    v.Normalize();
    CHECK_EQ(v.X, 0.6f);
    MCFrameOfRef frame;
    frame.ResetToWorldFrame();
    CHECK_EQ(frame.K.Z, 1.0f);
    CHECK_EQ(NullFrameOfRef.J.Y, 1.0f);
    CHECK_EQ(frame.MyAcos(2.0f), 0.0f);

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
    MCIDString id;
    id.Init("ABCDEFGHIJ");
    CHECK(std::strcmp(id.Id, "ABCDEFG") == 0);
    CHECK(id == "ABCDEFG");
    CHECK(!(id == "ABCDEFX"));
    CHECK(!(id == "ABC"));

    MCFullPathFileName path;
    path.Init("data\\missions\\", "mcx0101", ".fit");
    CHECK(std::strcmp(path, "data\\missions\\mcx0101.fit") == 0);
    path.Init("data\\", "x", nullptr);
    CHECK(std::strcmp(path, "data\\x") == 0);

    uint8_t bytes[13];
    Memfill(bytes, 13);
    CHECK_EQ(bytes[12], 0xff);
    Memclear(bytes + 1, 11);
    CHECK_EQ(bytes[0], 0xff);
    CHECK_EQ(bytes[5], 0);
    CHECK_EQ(bytes[12], 0xff);
}

TEST_CASE("inifile: a written FIT file reads back")
{
    const std::string path = ScratchPath("mc_inifile_test.fit");
    {
        MCFitIniFile out;
        REQUIRE_EQ(out.Create(path.c_str()), 0);
        out.WriteBlock("First");
        out.WriteIdLong("Count", -42);
        out.WriteIdULong("Big", 4000000000u);
        out.WriteIdFloat("Scale", 2.5f);
        out.WriteIdBoolean("On", 1);
        out.WriteIdBoolean("Off", 0);
        out.WriteIdString("Name", "Jade Falcon");
        out.WriteIdShort("Short", -7);
        out.WriteIdUShort("UShort", 65000);
        out.WriteIdChar("Char", -3);
        out.WriteIdUChar("UChar", 200);
        out.WriteBlock("Arrays");
        const int32_t longs[] = {1, -2, 3, 40000};
        out.WriteIdLongArray("Longs", longs, 4);
        const float floats[] = {0.25f, -1.5f, 100.0f};
        out.WriteIdFloatArray("Floats", floats, 3);
        const uint8_t bytes[] = {0, 7, 255};
        out.WriteIdUCharArray("Bytes", bytes, 3);
        const uint16_t shorts[] = {9, 65535};
        out.WriteIdUShortArray("Shorts", shorts, 2);
        out.Close();
    }

    MCFitIniFile in;
    REQUIRE_EQ(in.Open(path.c_str()), 0);
    CHECK_EQ(in.GetNumBlocks(), 2);
    CHECK_EQ(in.SeekBlock("Missing"), BLOCK_NOT_FOUND);
    CHECK_EQ(in.SeekBlock("first"), BLOCK_NOT_FOUND);
    REQUIRE_EQ(in.SeekBlock("First"), 0);

    int32_t count = 0;
    CHECK_EQ(in.ReadIdLong("Count", count), 0);
    CHECK_EQ(count, -42);
    CHECK_EQ(in.ReadIdLong("count", count), 0); // names ignore case
    uint32_t big = 0;
    CHECK_EQ(in.ReadIdULong("Big", big), 0);
    CHECK_EQ(big, 4000000000u);
    float scale = 0.0f;
    CHECK_EQ(in.ReadIdFloat("Scale", scale), 0);
    CHECK_EQ(scale, 2.5f);
    int on = 0;
    int off = 1;
    CHECK_EQ(in.ReadIdBoolean("On", on), 0);
    CHECK_EQ(in.ReadIdBoolean("Off", off), 0);
    CHECK_EQ(on, 1);
    CHECK_EQ(off, 0);
    char name[32] = {};
    CHECK_EQ(in.ReadIdString("Name", name, sizeof(name)), 0);
    CHECK(std::strcmp(name, "Jade Falcon") == 0);
    CHECK_EQ(in.GetIdStringLength("Name"), 12);
    char tiny[4];
    CHECK_EQ(in.ReadIdString("Name", tiny, sizeof(tiny)), BUFFER_TOO_SMALL);
    int16_t s = 0;
    CHECK_EQ(in.ReadIdShort("Short", s), 0);
    CHECK_EQ(s, -7);
    uint16_t us = 0;
    CHECK_EQ(in.ReadIdUShort("UShort", us), 0);
    CHECK_EQ(us, 65000);
    char c = 0;
    CHECK_EQ(in.ReadIdChar("Char", c), 0);
    CHECK_EQ(c, -3);
    uint8_t uc = 0;
    CHECK_EQ(in.ReadIdUChar("UChar", uc), 0);
    CHECK_EQ(uc, 200);
    count = 5;
    CHECK_EQ(in.ReadIdLong("Scale", count), VARIABLE_NOT_FOUND); // "f Scale" isn't an "l" entry
    CHECK_EQ(count, 0);

    REQUIRE_EQ(in.SeekBlock("Arrays"), 0);
    CHECK_EQ(in.GetIdLongArrayElements("Longs"), 4u);
    int32_t readLongs[4] = {};
    CHECK_EQ(in.ReadIdLongArray("Longs", readLongs, 4), 0);
    CHECK_EQ(readLongs[1], -2);
    CHECK_EQ(readLongs[3], 40000);
    int32_t small[2];
    CHECK_EQ(in.ReadIdLongArray("Longs", small, 2), USER_ARRAY_TOO_SMALL);
    float readFloats[3] = {};
    CHECK_EQ(in.ReadIdFloatArray("Floats", readFloats, 3), 0);
    CHECK_EQ(readFloats[0], 0.25f);
    CHECK_EQ(readFloats[1], -1.5f);
    CHECK_EQ(readFloats[2], 100.0f);
    uint8_t readBytes[3] = {};
    CHECK_EQ(in.ReadIdUCharArray("Bytes", readBytes, 3), 0);
    CHECK_EQ(readBytes[2], 255);
    uint16_t readShorts[2] = {};
    CHECK_EQ(in.ReadIdUShortArray("Shorts", readShorts, 2), 0);
    CHECK_EQ(readShorts[1], 65535);
    CHECK_EQ(in.GetIdFloatArrayElements("Nothing"), static_cast<uint32_t>(VARIABLE_NOT_FOUND));
    in.Close();

    // The file is laid out as the game writes it.
    MCFile raw;
    REQUIRE_EQ(raw.Open(path.c_str()), 0);
    std::string text(raw.GetLength(), '\0');
    raw.Read(reinterpret_cast<uint8_t*>(text.data()), static_cast<int32_t>(text.size()));
    raw.Close();
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

    MCFitIniFile in;
    REQUIRE_EQ(in.Open(path.c_str()), 0);
    REQUIRE_EQ(in.SeekBlock("Hex"), 0);
    int32_t mask = 0;
    CHECK_EQ(in.ReadIdLong("Mask", mask), 0);
    CHECK_EQ(mask, 0x1f);
    uint32_t color = 0;
    CHECK_EQ(in.ReadIdULong("Color", color), 0);
    CHECK_EQ(color, 0xffaa00u);
    int32_t word = 7;
    CHECK_EQ(in.ReadIdLong("Word", word), 0);
    CHECK_EQ(word, 0);
    int32_t split[3] = {};
    CHECK_EQ(in.ReadIdLongArray("Split", split, 3), 0);
    CHECK_EQ(split[2], 3);
    int32_t noEquals = 0;
    CHECK_EQ(in.ReadIdLong("NoEquals", noEquals), VARIABLE_NOT_FOUND);
    in.Close();

    {
        std::ofstream out(path, std::ios::binary);
        out << "Not a fit file\r\n[Block]\r\n";
    }

    MCFitIniFile bad;
    CHECK_EQ(bad.Open(path.c_str()), NOT_A_FITINIFILE);
    bad.Close();
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
    REQUIRE_EQ(system.File.SeekBlock("systemHeap"), 0);
    uint32_t size = 0;
    CHECK_EQ(system.File.ReadIdULong("systemHeapSize", size), 0);
    CHECK_EQ(size, 16383999u);
    CHECK_EQ(system.File.ReadIdULong("logisticsHeapSize", size), 0);
    CHECK_EQ(size, 33554430u);

    REQUIRE_EQ(system.File.SeekBlock("ABL"), 0);
    CHECK_EQ(system.File.ReadIdULong("SymbolTableHeapSize", size), 0);
    CHECK_EQ(size, 4095999u);
    int32_t watches = 0;
    CHECK_EQ(system.File.ReadIdLong("MaxWatchesPerModule", watches), 0);
    CHECK_EQ(watches, 20);

    REQUIRE_EQ(system.File.SeekBlock("FastFiles"), 0);
    int32_t numFastFiles = 0;
    CHECK_EQ(system.File.ReadIdLong("NumFastFiles", numFastFiles), 0);
    CHECK_EQ(numFastFiles, 5);
    char name[80] = {};
    CHECK_EQ(system.File.ReadIdString("File0", name, sizeof(name)), 0);
    CHECK(std::strcmp(name, "art.fst") == 0);
    CHECK_EQ(system.File.ReadIdString("File4", name, sizeof(name)), 0); // spelled "FIle4" in the file
    CHECK(std::strcmp(name, "shapes.fst") == 0);

    REQUIRE_EQ(system.File.SeekBlock("systemPaths"), 0);
    CHECK_EQ(system.File.ReadIdString("missionPath", name, sizeof(name)), 0);
    CHECK(std::strcmp(name, "data\\missions\\") == 0);
    CHECK_EQ(system.File.SeekBlock("UseSound"), 0);

    OpenFit prefs("prefs.cfg");
    REQUIRE_EQ(prefs.Result, 0);
    REQUIRE_EQ(prefs.File.SeekBlock("MechCommander"), 0);
    int flag = 0;
    CHECK_EQ(prefs.File.ReadIdBoolean("DirectDraw", flag), 0);
    CHECK_EQ(flag, 1);
    CHECK_EQ(prefs.File.ReadIdBoolean("Force16Mb", flag), 0);
    CHECK_EQ(flag, 0);
    int32_t difficulty = -1;
    CHECK_EQ(prefs.File.ReadIdLong("Difficulty", difficulty), 0);
    CHECK(difficulty >= 0 && difficulty <= 3);
    int32_t volume = -1;
    CHECK_EQ(prefs.File.ReadIdLong("SFXVolume", volume), 0);
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
        MCFitIniFile& fit = mission.File;
        REQUIRE_EQ(fit.SeekBlock("GameScale"), 0);
        float unitsPerMeter = 0.0f;
        CHECK_EQ(fit.ReadIdFloat("WorldUnitsPerMeter", unitsPerMeter), 0);
        CHECK_EQ(unitsPerMeter, 5.01f);
        uint32_t duration = 0;
        CHECK_EQ(fit.ReadIdULong("Duration", duration), 0);
        CHECK_EQ(duration, 60u);

        REQUIRE_EQ(fit.SeekBlock("Artillery"), 0);
        int32_t strikes = -1;
        CHECK_EQ(fit.ReadIdLong("NumLargeStrikes", strikes), 0); // "Null"
        CHECK_EQ(strikes, 0);
        CHECK_EQ(fit.ReadIdLong("NumSmallStrikes", strikes), 0);
        CHECK_EQ(strikes, 4);

        REQUIRE_EQ(fit.SeekBlock("Part1"), 0);
        uint32_t objectNumber = 0;
        CHECK_EQ(fit.ReadIdULong("ObjectNumber", objectNumber), 0);
        CHECK_EQ(objectNumber, 15u);
        int playerPart = 1;
        CHECK_EQ(fit.ReadIdBoolean("PlayerPart", playerPart), 0);
        CHECK_EQ(playerPart, 0);
        char team = 0;
        CHECK_EQ(fit.ReadIdChar("TeamID", team), 0);
        CHECK_EQ(team, 1);
        char profile[16] = {};
        CHECK_EQ(fit.ReadIdString("ObjectProfile", profile, sizeof(profile)), 0);
        CHECK(std::strcmp(profile, "PM200300") == 0);
        float x = 0.0f;
        float y = 0.0f;
        CHECK_EQ(fit.ReadIdFloat("PositionX", x), 0);
        CHECK_EQ(fit.ReadIdFloat("PositionY", y), 0);
        CHECK_EQ(x, 1530.0f);
        CHECK_EQ(y, -1233.0f);

        REQUIRE_EQ(fit.SeekBlock("Commander1Group:0"), 0);
        CHECK_EQ(fit.GetIdLongArrayElements("Mates"), 12u);
        int32_t mates[12] = {};
        CHECK_EQ(fit.ReadIdLongArray("Mates", mates, 12), 0);
        CHECK_EQ(mates[0], 1);
        CHECK_EQ(mates[4], 5);
        CHECK_EQ(mates[5], 0);
    }

    {
        OpenFit clan("data\\missions\\E3_0101.FIT");
        REQUIRE_EQ(clan.Result, 0);
        REQUIRE_EQ(clan.File.SeekBlock("ClanStar:0"), 0);
        float withdraw[7] = {};
        CHECK_EQ(clan.File.ReadIdFloatArray("WithdrawThreshold", withdraw, 7), 0);
        CHECK_EQ(withdraw[0], 0.25f);
        CHECK_EQ(withdraw[4], 500.0f);
        CHECK_EQ(withdraw[5], -2000.0f);
        CHECK_EQ(withdraw[6], -1.0f);
        float zone[4] = {};
        CHECK_EQ(clan.File.ReadIdFloatArray("Zone1", zone, 4), 0); // ends in a // comment
        CHECK_EQ(zone[0], -135.0f);
        CHECK_EQ(zone[3], 90.0f);
        int32_t roles[5] = {};
        CHECK_EQ(clan.File.ReadIdLongArray("Roles", roles, 5), 0);
        CHECK_EQ(roles[0], 0);
        CHECK_EQ(roles[4], 1);
    }

    {
        OpenFit screen("data\\art\\lanscreen.fit");
        REQUIRE_EQ(screen.Result, 0);
        REQUIRE_EQ(screen.File.SeekBlock("Element1"), 0);
        int32_t left = 0;
        CHECK_EQ(screen.File.ReadIdLong("Left", left), 0);
        CHECK_EQ(left, 549);
        char art[32] = {};
        CHECK_EQ(screen.File.ReadIdString("GreyArt", art, sizeof(art)), 0);
        CHECK(std::strcmp(art, "bn_cancl.tga") == 0);
        REQUIRE_EQ(screen.File.SeekBlock("Element0"), 0);
        int back = 0;
        CHECK_EQ(screen.File.ReadIdBoolean("UseBackPalette", back), 0); // "T"
        CHECK_EQ(back, 1);
        REQUIRE_EQ(screen.File.SeekBlock("Blocks"), 0);
        uint8_t block1[12] = {};
        CHECK_EQ(screen.File.GetIdUCharArrayElements("Block1"), 12u);
        CHECK_EQ(screen.File.ReadIdUCharArray("Block1", block1, 12), 0); // "8, 9,10,11"
        CHECK_EQ(block1[7], 7);
        CHECK_EQ(block1[9], 9);
        CHECK_EQ(block1[11], 11);
    }

    {
        OpenFit palette("data\\palette\\palette.fit");
        REQUIRE_EQ(palette.Result, 0);
        REQUIRE_EQ(palette.File.SeekBlock("Range2"), 0);
        uint8_t base = 0;
        CHECK_EQ(palette.File.ReadIdUChar("BaseColorIndex", base), 0);
        CHECK_EQ(base, 32);
        REQUIRE_EQ(palette.File.SeekBlock("Ranges"), 0);
        int32_t ranges = 0;
        CHECK_EQ(palette.File.ReadIdLong("NumColorRanges", ranges), 0);
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

    for (int32_t f = 0; f < NumFastFiles; ++f)
    {
        for (int32_t i = 0; i < FastFiles[f]->GetNumFiles(); ++i)
        {
            const MCFileEntry* entry = FastFiles[f]->GetEntry(i);
            const std::string_view name(entry->Name);

            if (name.size() < 4 || MCPort::StrICmp(entry->Name + name.size() - 4, ".fit") != 0)
            {
                continue;
            }

            MCTest::Scope scope(entry->Name);
            MCFitIniFile fit;
            CHECK_EQ(fit.Open(entry->Name), 0);

            for (int32_t b = 0; b < fit.GetNumBlocks(); ++b)
            {
                MCTest::Scope blockScope(fit.GetBlockName(b));
                // A block named twice is found at its first copy; seeking still succeeds.
                CHECK_EQ(fit.SeekBlock(fit.GetBlockName(b)), 0);
                ++blocks;
            }

            fit.Close();
            ++opened;
        }
    }

    CHECK(opened > 700);
    CHECK(blocks > 10000);
}
