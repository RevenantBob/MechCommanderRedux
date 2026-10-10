#include "stdafx.h"
#include "MCTest.h"
#include "TestGame.h"
#include "fakes/MCMemoryFileSource.h"
#include "lib/MCDice.h"
#include "lib/MCFastFileSet.h"
#include "lib/MCFile.h"
#include "lib/MCFitIniFile.h"
#include "lib/MCFrameOfRef.h"
#include "lib/MCIDString.h"
#include "lib/MCLz.h"
#include "lib/MCPacketFile.h"
#include "lib/MCPriorityQueue.h"
#include "main/MCGameContext.h"
#include "platform/MCFileSystem.h"

namespace
{
    /// <summary>A scratch folder of the test's own under the temp folder, deleted with it.</summary>
    class ScratchFolder
    {
    public:
        explicit ScratchFolder(std::string_view name)
            : _Path(std::filesystem::temp_directory_path() / "mc_lib_tests" / name)
        {
            std::error_code error;
            std::filesystem::remove_all(_Path, error);
            std::filesystem::create_directories(_Path);
        }

        ~ScratchFolder()
        {
            std::error_code error;
            std::filesystem::remove_all(_Path, error);
        }

        /// <summary>The path of <paramref name="name"/> in the folder, as a string the file layer takes.</summary>
        std::string operator/(std::string_view name) const { return (_Path / name).string(); }

    private:
        std::filesystem::path _Path;
    };

    std::vector<uint8_t> Bytes(std::string_view text)
    {
        return std::vector<uint8_t>(text.begin(), text.end());
    }

    void WriteFile(const std::string& path, std::span<const uint8_t> bytes)
    {
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }

    std::vector<uint8_t> ReadFile(const std::string& path)
    {
        std::ifstream in(path, std::ios::binary);
        return std::vector<uint8_t>(std::istreambuf_iterator<char>(in), {});
    }

    std::vector<uint8_t> RoundTrip(const std::vector<uint8_t>& data)
    {
        const std::vector<uint8_t> packed = LZCompress(data);
        std::vector<uint8_t> unpacked(data.size() + 16);
        unpacked.resize(static_cast<size_t>(LZDecomp(unpacked, packed)));
        return unpacked;
    }

    void Put32(std::vector<uint8_t>& out, int32_t value)
    {
        const auto bytes = std::bit_cast<std::array<uint8_t, 4>>(value);
        out.insert(out.end(), bytes.begin(), bytes.end());
    }

    int32_t Get32(const std::vector<uint8_t>& bytes, size_t at)
    {
        return std::bit_cast<int32_t>(std::array{bytes[at], bytes[at + 1], bytes[at + 2], bytes[at + 3]});
    }

    /// <summary>One entry of a FastFile built by <see cref="BuildFastFile"/>.</summary>
    struct FastEntry
    {
        std::string Name;
        std::vector<uint8_t> Data;
        bool Packed = false;
    };

    /// <summary>
    /// A FastFile as the format lays it out: the entry count, a 262-byte directory record per entry (offset, stored
    /// size, real size, name), then the data, each entry raw or LZ-packed.
    /// </summary>
    std::vector<uint8_t> BuildFastFile(const std::vector<FastEntry>& entries)
    {
        std::vector<uint8_t> out;
        Put32(out, static_cast<int32_t>(entries.size()));
        std::vector<uint8_t> data;
        const size_t dataStart = 4 + entries.size() * 262;

        for (const FastEntry& entry : entries)
        {
            const std::vector<uint8_t> stored = entry.Packed ? LZCompress(entry.Data) : entry.Data;
            Put32(out, static_cast<int32_t>(dataStart + data.size()));
            Put32(out, static_cast<int32_t>(stored.size()));
            Put32(out, static_cast<int32_t>(entry.Data.size()));
            std::array<char, 250> name{};
            std::ranges::copy(entry.Name, name.begin());
            out.insert(out.end(), name.begin(), name.end());
            data.insert(data.end(), stored.begin(), stored.end());
        }

        out.insert(out.end(), data.begin(), data.end());
        return out;
    }

    /// <summary>Text that packs well.</summary>
    std::vector<uint8_t> Repetitive(size_t size)
    {
        std::vector<uint8_t> data(size);

        for (size_t i = 0; i < size; ++i)
        {
            data[i] = static_cast<uint8_t>("MechCommander "[i % 14]);
        }

        return data;
    }

    /// <summary>Bytes that don't pack.</summary>
    std::vector<uint8_t> Noise(size_t size, uint32_t seed)
    {
        std::vector<uint8_t> data(size);

        for (uint8_t& b : data)
        {
            seed = seed * 1103515245 + 12345;
            b = static_cast<uint8_t>(seed >> 16);
        }

        return data;
    }
}

TEST_CASE("lz: packing then unpacking gives the input back")
{
    CHECK(RoundTrip(Repetitive(20000)) == Repetitive(20000));
    // Random bytes fill the dictionary and force clears at every code width.
    CHECK(RoundTrip(Noise(200000, 12345)) == Noise(200000, 12345));
    const std::vector<uint8_t> one{42};
    CHECK(RoundTrip(one) == one);
    const std::vector<uint8_t> runs(5000, 7);
    CHECK(RoundTrip(runs) == runs);
    CHECK(LZCompress(Repetitive(20000)).size() < 20000u / 4);
}

/// <summary>
/// An empty input is a clear code and an end code (9 bits each: three bytes), with no padding; the decoder stops
/// three bytes before the end, so it unpacks to nothing.
/// </summary>
TEST_CASE("lz: an empty input packs to a clear and an end code")
{
    const std::vector<uint8_t> packed = LZCompress({});
    REQUIRE_EQ(packed.size(), 3u);
    // 256 then 257, 9 bits each, LSB first.
    CHECK_EQ(packed[0], 0x00);
    CHECK_EQ(packed[1], 0x03);
    CHECK_EQ(packed[2], 0x02);
    std::array<uint8_t, 8> out{};
    CHECK_EQ(LZDecomp(out, packed), 0);
}

/// <summary>Damaged data never unpacks past the output (a port fix; the original trusted the stream).</summary>
TEST_CASE("lz: damaged data never writes past the output")
{
    for (uint32_t seed = 1; seed <= 50; ++seed)
    {
        MCTest::Scope scope(std::format("seed {}", seed));
        const std::vector<uint8_t> garbage = Noise(64 + seed * 7, seed);
        std::vector<uint8_t> guarded(48, 0xcd);
        const int32_t written = LZDecomp(std::span(guarded).first(40), garbage);
        CHECK(written >= 0 && written <= 40);
        CHECK(std::ranges::all_of(std::span(guarded).subspan(40), [](uint8_t b) { return b == 0xcd; }));
    }
}

/// <summary>
/// A FastFile's entries are found by name with case ignored (the first of two equal names), read raw or unpacked,
/// and an MCFile opens a path the disk lacks from the first FastFile that has it.
/// </summary>
TEST_CASE("fastfile: entries are found by name and read raw or unpacked")
{
    ScratchFolder folder("fastfile");
    const std::vector<uint8_t> text = Bytes("FITini\r\n[Block]\r\nl Value = 7\r\n\r\nFITend\r\n");
    const std::vector<uint8_t> packed = Repetitive(5000);
    WriteFile(folder / "test.fst", BuildFastFile({{"data\\a.txt", Bytes("first"), false},
                                                  {"DATA\\B.BIN", packed, true},
                                                  {"data\\A.TXT", Bytes("second"), false},
                                                  {"data\\c.fit", text, true}}));

    auto fastFile = MCFastFile::Create(folder / "test.fst");
    REQUIRE(fastFile.has_value());
    MCFastFile& archive = **fastFile;
    CHECK_EQ(archive.GetNumFiles(), 4);
    CHECK_EQ(archive.Find("Data\\b.bin"), std::optional<int32_t>(1));
    CHECK_EQ(archive.Find("data\\a.txt"), std::optional<int32_t>(0));
    CHECK(!archive.Find("data\\d.txt").has_value());
    CHECK_EQ(archive.GetEntry(1)->GetName(), std::string_view("DATA\\B.BIN"));
    CHECK(archive.GetEntry(1)->Size < archive.GetEntry(1)->RealSize);
    CHECK(archive.GetEntry(4) == nullptr);

    std::vector<uint8_t> data(packed.size());
    CHECK_EQ(archive.ReadEntry(1, data), static_cast<int32_t>(packed.size()));
    CHECK(data == packed);
    CHECK(!MCFastFile::Create(folder / "missing.fst").has_value());

    // Through the file layer: nothing on disk or in memory has these, so they come from the FastFiles.
    MCTestContextScope scope;
    MCFastFileSet& fastFiles = scope.Context().SetFastFiles(std::make_unique<MCFastFileSet>());
    REQUIRE(fastFiles.Open(folder / "test.fst").has_value());
    CHECK(!fastFiles.Open(folder / "missing.fst").has_value());
    CHECK_EQ(fastFiles.Files().size(), 1u);
    scope.Context().SetFiles(std::make_unique<MCMemoryFileSource>());

    MCFile file;
    REQUIRE_EQ(file.Open("data\\b.bin"), NO_ERR);
    CHECK_EQ(file.GetLength(), static_cast<uint32_t>(packed.size()));
    CHECK_EQ(file.ReadByte(), packed[0]);
    REQUIRE_EQ(file.Seek(-1, SEEK_END), NO_ERR);
    CHECK_EQ(file.ReadByte(), packed.back());
    CHECK(file.Eof());
    file.Close();
    CHECK(!file.IsOpen());

    REQUIRE_EQ(file.Open("data\\a.txt"), NO_ERR);
    std::string first(5, '\0');
    CHECK_EQ(file.Read(reinterpret_cast<uint8_t*>(first.data()), 5), 5);
    CHECK_EQ(first, std::string("first"));
    file.Close();

    MCFitIniFile fit;
    REQUIRE_EQ(fit.Open("data\\c.fit"), NO_ERR);
    REQUIRE_EQ(fit.SeekBlock("Block"), 0);
    CHECK(fit.Read<int32_t>("Value") == MCFitResult<int32_t>(7));
    fit.Close();

    CHECK_EQ(file.Open("data\\nothere.bin"), FILE_NOT_FOUND);
    CHECK(file.GetFilename().empty());
}

/// <summary>Seeks are checked against the length; values are little-endian; lines end at CR, CR LF or LF.</summary>
TEST_CASE("file: seek, typed reads and lines")
{
    MCTestContextScope scope;
    MCMemoryFileSource& files = scope.Context().SetFiles(std::make_unique<MCMemoryFileSource>());
    files.AddFile("data\\values.bin", std::vector<uint8_t>{0x78, 0x56, 0x34, 0x12, 0xff, 0xfe, 0x00, 0x00, 0x80, 0x3f});
    files.AddFile("data\\lines.txt", std::string_view("one\r\ntwo\rthree\nfour"));

    MCFile file;
    REQUIRE_EQ(file.Open("data\\values.bin"), NO_ERR);
    CHECK_EQ(file.FileSize(), 10u);
    CHECK_EQ(file.ReadLong(), 0x12345678);
    CHECK_EQ(file.ReadWord(), static_cast<int16_t>(-257));
    CHECK_EQ(file.ReadShort(), static_cast<int16_t>(0));
    CHECK_EQ(file.GetLogicalPosition(), 8u);
    CHECK_EQ(file.Seek(6), NO_ERR);
    CHECK_EQ(file.ReadFloat(), 1.0f);
    CHECK(file.Eof());
    CHECK_EQ(file.Seek(11), READ_PAST_EOF);
    CHECK_EQ(file.Seek(10), NO_ERR);
    CHECK_EQ(file.Seek(1, SEEK_END), READ_PAST_EOF);
    CHECK_EQ(file.Seek(-11, SEEK_END), READ_PAST_EOF);
    CHECK_EQ(file.Seek(-4, SEEK_END), NO_ERR);
    CHECK_EQ(file.Seek(2, SEEK_CUR), NO_ERR);
    CHECK_EQ(file.GetLogicalPosition(), 8u);
    CHECK_EQ(file.Seek(3, SEEK_CUR), READ_PAST_EOF);
    file.Skip(-8);
    CHECK_EQ(file.GetLogicalPosition(), 0u);
    std::array<uint8_t, 16> buffer{};
    CHECK_EQ(file.Read(buffer), 10);
    CHECK_EQ(buffer[9], 0x3f);
    file.Close();

    REQUIRE_EQ(file.Open("data\\lines.txt"), NO_ERR);
    CHECK_EQ(file.ReadLine(100), std::string("one"));
    CHECK_EQ(file.ReadLine(100), std::string("two"));
    CHECK_EQ(file.ReadLine(100), std::string("three"));
    uint8_t line[8] = {};
    CHECK_EQ(file.ReadLine(line, 7), 5);
    CHECK_EQ(std::string(reinterpret_cast<char*>(line)), std::string("four"));
    CHECK(file.Eof());
}

/// <summary>
/// Original behaviour (OB-135): a line longer than the reader looks at is cut there, and the byte after the cut is
/// lost: the next read starts one byte later.
/// </summary>
TEST_CASE("file: a line longer than the read is cut and loses the next byte")
{
    MCTestContextScope scope;
    MCMemoryFileSource& files = scope.Context().SetFiles(std::make_unique<MCMemoryFileSource>());
    files.AddFile("data\\long.txt", std::string_view("abcdefghij\r\nnext"));
    MCFile file;
    REQUIRE_EQ(file.Open("data\\long.txt"), NO_ERR);
    CHECK_EQ(file.ReadLine(4), std::string("abcd"));
    CHECK_EQ(file.ReadLine(100), std::string("fghij"));
    CHECK_EQ(file.ReadLine(100), std::string("next"));
}

/// <summary>
/// A child file is a window into its parent from the parent's read position. A parent read whole into memory (a
/// FastFile entry, a test's memory file) takes no window children (TOO_MANY_CHILDREN, as the original), and closing
/// a parent closes its children.
/// </summary>
TEST_CASE("file: a child window reads its part of the parent")
{
    ScratchFolder folder("child");
    WriteFile(folder / "parent.bin", Bytes("0123456789abcdef"));

    MCFile parent;
    REQUIRE_EQ(parent.Open(folder / "parent.bin"), NO_ERR);
    REQUIRE_EQ(parent.Seek(4), NO_ERR);
    MCFile child;
    CHECK_EQ(child.Open(nullptr, 4), PARENT_NULL);
    REQUIRE_EQ(child.Open(&parent, 6), NO_ERR);
    CHECK_EQ(child.GetLength(), 6u);
    CHECK(child.GetParent() == &parent);
    CHECK_EQ(child.GetFilename(), parent.GetFilename());
    std::string text(8, '\0');
    CHECK_EQ(child.Read(reinterpret_cast<uint8_t*>(text.data()), 8), 8);
    CHECK_EQ(text.substr(0, 6), std::string("456789"));
    CHECK_EQ(child.Seek(7), READ_PAST_EOF);
    CHECK_EQ(child.WriteByte(1), 0);

    // A window takes windows of its own, and closes them with it.
    REQUIRE_EQ(child.Seek(2), NO_ERR);
    MCFile grandchild;
    REQUIRE_EQ(grandchild.Open(&child, 3), NO_ERR);
    CHECK_EQ(grandchild.GetLength(), 3u);

    parent.Close();
    CHECK(!child.IsOpen());
    CHECK(!grandchild.IsOpen());

    MCTestContextScope scope;
    MCMemoryFileSource& files = scope.Context().SetFiles(std::make_unique<MCMemoryFileSource>());
    files.AddFile("data\\memory.bin", Bytes("0123456789"));
    MCFile memoryParent;
    REQUIRE_EQ(memoryParent.Open("data\\memory.bin"), NO_ERR);
    MCFile memoryChild;
    CHECK_EQ(memoryChild.Open(&memoryParent, 4), TOO_MANY_CHILDREN);
}

/// <summary>
/// What the game writes goes to the user folder: an install file of the same name stays as it was, and reads find
/// the user's copy first. The same holds for a memory source's scratch folder.
/// </summary>
TEST_CASE("file: writes go to the user folder and never touch the install")
{
    ScratchFolder folder("overlay");
    std::filesystem::create_directories(folder / "install\\data");
    const std::vector<uint8_t> original = Bytes("FITini\r\n[Prefs]\r\nl Volume = 3\r\n\r\nFITend\r\n");
    WriteFile(folder / "install\\data\\prefs.cfg", original);

    {
        MCTestContextScope scope;
        scope.Context().SetFiles(std::make_unique<MCDiskFileSource>());
        MCFileSystem::SetGameRoot(folder / "install");
        MCFileSystem::SetUserRoot(folder / "user");

        MCFitIniFile fit;
        REQUIRE_EQ(fit.Open("DATA\\PREFS.CFG"), 0);
        REQUIRE_EQ(fit.SeekBlock("Prefs"), 0);
        CHECK(fit.Read<int32_t>("Volume") == MCFitResult<int32_t>(3));
        fit.Close();

        MCFitIniFile out;
        REQUIRE_EQ(out.Create("data\\prefs.cfg"), 0);
        out.WriteBlock("Prefs");
        out.WriteIdLong("Volume", 9);
        out.Close();

        CHECK(std::filesystem::exists(folder / "user\\data\\prefs.cfg"));
        REQUIRE_EQ(fit.Open("data\\prefs.cfg"), 0);
        REQUIRE_EQ(fit.SeekBlock("Prefs"), 0);
        CHECK(fit.Read<int32_t>("Volume") == MCFitResult<int32_t>(9));
    }

    CHECK(ReadFile(folder / "install\\data\\prefs.cfg") == original);

    MCTestContextScope scope;
    MCMemoryFileSource& files = scope.Context().SetFiles(std::make_unique<MCMemoryFileSource>());
    files.AddFile("data\\prefs.cfg", original);
    {
        MCFitIniFile out;
        REQUIRE_EQ(out.Create("data\\prefs.cfg"), 0);
        out.WriteBlock("Prefs");
        out.WriteIdLong("Volume", 5);
    }

    MCFitIniFile fit;
    REQUIRE_EQ(fit.Open("data\\prefs.cfg"), 0);
    REQUIRE_EQ(fit.SeekBlock("Prefs"), 0);
    CHECK(fit.Read<int32_t>("Volume") == MCFitResult<int32_t>(5));
    fit.Close();
    REQUIRE(MCFileSystem::RemoveFile("data\\prefs.cfg"));
    const auto image = MCFileSystem::FindImage("data\\prefs.cfg");
    REQUIRE(image.has_value());
    CHECK(std::ranges::equal(*image, original));
}

/// <summary>
/// The packet file format: a magic (or checksum) word, the offset of the first packet (so the packet count is
/// offset / 4 - 2), a table of offsets whose top 3 bits are the storage, then the packets; an LZ packet starts with
/// its unpacked size. A packet never written reads as an empty NUL packet.
/// </summary>
TEST_CASE("packet file: written packets read back raw and packed")
{
    ScratchFolder folder("packet");
    const std::vector<uint8_t> repetitive = Repetitive(3000);
    const std::vector<uint8_t> noise = Noise(500, 77);
    const std::vector<uint8_t> small = Bytes("abc");
    {
        MCPacketFile out;
        REQUIRE_EQ(out.Create(folder / "test.pak"), NO_ERR);
        out.Reserve(4);
        CHECK_EQ(out.WritePacket(0, noise, MCPacketStorage::Nul), 500);
        CHECK(out.GetStorageType() == MCPacketStorage::Raw);
        out.WritePacket(1, repetitive, MCPacketStorage::Nul);
        CHECK(out.GetStorageType() == MCPacketStorage::Lzd);
        out.WritePacket(2, small, MCPacketStorage::Lzd);
        CHECK(out.GetStorageType() == MCPacketStorage::Lzd);
        CHECK_EQ(out.WritePacket(4, small, MCPacketStorage::Raw), 0);
    }

    const std::vector<uint8_t> raw = ReadFile(folder / "test.pak");
    REQUIRE(raw.size() > 24);
    CHECK_EQ(Get32(raw, 0), MCPacketFile::PacketFileMagic);
    CHECK_EQ(Get32(raw, 4), 4 * 4 + 8);
    // Packet 0 is stored raw right after the table; packet 1 is LZD (storage 2 in the top bits).
    CHECK_EQ(static_cast<uint32_t>(Get32(raw, 8)), 24u);
    CHECK_EQ(static_cast<uint32_t>(Get32(raw, 12)) >> 29, 2u);
    CHECK_EQ(static_cast<uint32_t>(Get32(raw, 12)) & 0x1fffffff, 524u);
    CHECK_EQ(Get32(raw, 524), 3000);

    MCPacketFile pak;
    REQUIRE_EQ(pak.Open(folder / "test.pak"), NO_ERR);
    REQUIRE_EQ(pak.GetNumPackets(), 4);

    REQUIRE_EQ(pak.SeekPacket(0), NO_ERR);
    CHECK(pak.GetStorageType() == MCPacketStorage::Raw);
    CHECK_EQ(pak.GetPacketSize(), 500);
    std::vector<uint8_t> buffer(500);
    CHECK_EQ(pak.ReadPacket(0, buffer), 500);
    CHECK(buffer == noise);

    REQUIRE_EQ(pak.SeekPacket(1), NO_ERR);
    CHECK(pak.GetStorageType() == MCPacketStorage::Lzd);
    CHECK_EQ(pak.GetPacketSize(), 3000);
    CHECK(pak.GetPackedPacketSize() < 3000);
    buffer.assign(3000, 0);
    CHECK_EQ(pak.ReadPacket(1, buffer.data()), 3000);
    CHECK(buffer == repetitive);

    buffer.assign(3, 0);
    CHECK_EQ(pak.ReadPacket(2, buffer), 3);
    CHECK(buffer == small);

    REQUIRE_EQ(pak.SeekPacket(3), NO_ERR);
    CHECK(pak.GetStorageType() == MCPacketStorage::Nul);
    CHECK_EQ(pak.GetPacketSize(), 0);
    CHECK_EQ(pak.ReadPacket(3, buffer), 0);
    CHECK(pak.SeekPacket(-1) != NO_ERR);
}

/// <summary>A checksummed packet file holds the sum of every byte after the first word, and fails to open when one changes.</summary>
TEST_CASE("packet file: a checksum guards the contents")
{
    ScratchFolder folder("checksum");
    {
        MCPacketFile out;
        REQUIRE_EQ(out.Create(folder / "sum.pak"), NO_ERR);
        out.Reserve(2, true);
        out.WritePacket(0, Bytes("hello"), MCPacketStorage::Raw);
        out.WritePacket(1, Bytes("world!"), MCPacketStorage::Raw);
    }

    std::vector<uint8_t> raw = ReadFile(folder / "sum.pak");
    REQUIRE_EQ(raw.size(), 8u + 2 * 4 + 11);
    CHECK_EQ(Get32(raw, 0), std::accumulate(raw.begin() + 4, raw.end(), 0));

    {
        MCPacketFile pak;
        REQUIRE_EQ(pak.Open(folder / "sum.pak"), NO_ERR);
        CHECK_EQ(pak.GetNumPackets(), 2);
        std::vector<uint8_t> buffer(6);
        REQUIRE_EQ(pak.SeekPacket(1), NO_ERR);
        CHECK_EQ(pak.ReadPacket(-1, buffer), 6);
        CHECK(buffer == Bytes("world!"));
    }

    raw.back() ^= 1;
    WriteFile(folder / "sum.pak", raw);
    MCPacketFile bad;
    CHECK_EQ(bad.Open(folder / "sum.pak"), static_cast<int32_t>(0xBADF0004));
}

/// <summary>A FIT file stored as a packet opens straight from the packet file (read into memory).</summary>
TEST_CASE("packet file: a FIT file inside a packet opens as a child")
{
    ScratchFolder folder("fitpacket");
    const std::vector<uint8_t> fit =
        Bytes("FITini\r\n[Mech]\r\nl Tonnage = 35\r\nst Name = \"Uller\"\r\n\r\nFITend\r\n");
    {
        MCPacketFile out;
        REQUIRE_EQ(out.Create(folder / "fits.pak"), NO_ERR);
        out.Reserve(2);
        out.WritePacket(0, Bytes("not a fit"), MCPacketStorage::Raw);
        out.WritePacket(1, fit, MCPacketStorage::Lzd);
    }

    MCPacketFile pak;
    REQUIRE_EQ(pak.Open(folder / "fits.pak"), NO_ERR);
    REQUIRE_EQ(pak.SeekPacket(1), NO_ERR);
    MCFitIniFile profile;
    REQUIRE_EQ(profile.Open(&pak, static_cast<uint32_t>(pak.GetPacketSize())), 0);
    REQUIRE_EQ(profile.SeekBlock("Mech"), 0);
    CHECK(profile.Read<int32_t>("Tonnage") == MCFitResult<int32_t>(35));
    CHECK(profile.Read<std::string>("Name") == MCFitResult<std::string>("Uller"));

    REQUIRE_EQ(pak.SeekPacket(0), NO_ERR);
    MCFitIniFile notFit;
    CHECK_EQ(notFit.Open(&pak, static_cast<uint32_t>(pak.GetPacketSize())), NOT_A_FITINIFILE);
}

/// <summary>
/// The open list pops the smallest key first. Equal keys come out last in, first out (an insert climbs past equal
/// parents), which decides between equally good path steps (rule R5).
/// </summary>
TEST_CASE("pqueue: smallest key first, ties last in first out")
{
    MCPriorityQueue queue(16, -1000000);
    const int32_t keys[] = {50, 10, 70, 30, 20, 60, 40};

    for (int32_t i = 0; i < 7; ++i)
    {
        CHECK(queue.Insert({keys[i], 100 + i, i, i}));
    }

    CHECK(queue.Find(102) > 0);
    CHECK_EQ(queue.GetItem(queue.Find(102)).Key, 70);
    CHECK_EQ(queue.Find(999), 0);

    // Lower 70 to 5: it comes out first; raise 10 to 65: it comes out after 60.
    queue.Change(queue.Find(102), 5);
    queue.Change(queue.Find(101), 65);
    std::vector<int32_t> ids;

    while (!queue.IsEmpty())
    {
        ids.push_back(queue.Pop().Id);
    }

    CHECK((ids == std::vector<int32_t>{102, 104, 103, 106, 100, 105, 101}));

    for (int32_t id = 1; id <= 3; ++id)
    {
        CHECK(queue.Insert({5, id, 0, 0}));
    }

    CHECK_EQ(queue.Pop().Id, 3);
    CHECK_EQ(queue.Pop().Id, 2);
    CHECK_EQ(queue.Pop().Id, 1);
}

/// <summary>The queue holds two items more than its size (the original counted its spare slots), and refuses more.</summary>
TEST_CASE("pqueue: a full queue refuses an insert")
{
    MCPriorityQueue queue(2, -1000);

    for (int32_t i = 0; i < 4; ++i)
    {
        CHECK(queue.Insert({10 - i, i, 0, 0}));
    }

    CHECK(!queue.Insert({0, 9, 0, 0}));
    CHECK_EQ(queue.Size(), 4);
    queue.Clear();
    CHECK(queue.IsEmpty());
    CHECK(queue.Insert({0, 9, 0, 0}));
}

TEST_CASE("cident: ids keep seven characters, and game paths join")
{
    MCIDString id;
    id.Init("ABCDEFGHIJ");
    CHECK_EQ(id.Id, std::string("ABCDEFG"));
    CHECK(id == "ABCDEFG");
    CHECK(!(id == "ABCDEFGH"));
    CHECK(!(id == "ABCDEFX"));
    CHECK(!(id == "ABC"));
    id.Init("TBlk12");
    CHECK(id == "TBlk12");

    CHECK_EQ(GamePath("data\\missions\\", "mcx0101", ".fit"), std::string("data\\missions\\mcx0101.fit"));
    CHECK_EQ(GamePath("data\\", "x"), std::string("data\\x"));
}

TEST_CASE("cvmath: vector products and random ranges")
{
    const MCVector3D x(1.0f, 0.0f, 0.0f);
    const MCVector3D y(0.0f, 1.0f, 0.0f);
    const MCVector3D z = x & y;
    CHECK_EQ(z.X, 0.0f);
    CHECK_EQ(z.Y, 0.0f);
    CHECK_EQ(z.Z, 1.0f);
    CHECK_EQ(x | y, 0.0);
    MCVector3D v(3.0f, 4.0f, 0.0f);
    CHECK_EQ(v.Magnitude(), 5.0);
    v.Normalize();
    CHECK_EQ(v.X, 0.6f);
    MCFrameOfRef frame;
    frame.ResetToWorldFrame();
    CHECK_EQ(frame.K.Z, 1.0f);
    CHECK_EQ(NullFrameOfRef.J.Y, 1.0f);
    CHECK_EQ(frame.MyAcos(2.0f), 0.0);
    CHECK_EQ(AcosMatherr(-2.0), 3.14159265359);

    for (int i = 0; i < 1000; ++i)
    {
        const int32_t r = RandomNumber(10);
        CHECK(r >= 0 && r < 10);
        const int32_t s = SignedRandomNumber(5);
        CHECK(s >= -5 && s < 5);
    }

    CHECK(!RollDice(0));
    CHECK(RollDice(101));
}

TEST_CASE("game: every FastFile entry unpacks to its recorded size")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    int checked = 0;
    int packed = 0;

    for (const char* name : {"ART.FST", "MISSION.FST", "MISC.FST", "SHAPES.FST", "TERRAIN.FST"})
    {
        MCTest::Scope scope(name);
        auto fastFile = MCFastFile::Create(name);
        REQUIRE(fastFile.has_value());
        MCFastFile& archive = **fastFile;
        CHECK(archive.GetNumFiles() > 0);
        std::vector<uint8_t> buffer;

        for (int32_t i = 0; i < archive.GetNumFiles(); ++i)
        {
            const MCFileEntry* entry = archive.GetEntry(i);
            MCTest::Scope entryScope(std::string(entry->GetName()));
            REQUIRE(archive.Find(entry->GetName()).has_value());
            buffer.assign(static_cast<size_t>(entry->RealSize), 0);
            CHECK_EQ(archive.ReadEntry(i, buffer), entry->RealSize);
            packed += entry->Size != entry->RealSize ? 1 : 0;
            ++checked;
        }
    }

    CHECK(checked > 1000);
    CHECK(packed > 100);
}

TEST_CASE("game: File falls back to the FastFiles")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    MCTestGame::OpenFastFiles();
    MCFile file;
    REQUIRE_EQ(file.Open("data\\art\\access00.tga"), 0);
    CHECK(file.GetLength() > 18);
    // A TGA header: no image id, no colour map or a colour map, type 1/2/9/10.
    (void)file.ReadByte();
    const uint8_t mapType = file.ReadByte();
    const uint8_t imageType = file.ReadByte();
    CHECK(mapType <= 1);
    CHECK(imageType == 1 || imageType == 2 || imageType == 9 || imageType == 10);
    file.Close();
    CHECK(!file.IsOpen());
}

TEST_CASE("game: packet files open and read")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    MCTestGame::OpenFastFiles();
    MCPacketFile pak;
    REQUIRE_EQ(pak.Open("data\\art\\art.pak"), 0);
    CHECK(pak.GetNumPackets() > 0);
    std::vector<uint8_t> buffer;

    for (int32_t i = 0; i < pak.GetNumPackets(); ++i)
    {
        MCTest::Scope scope(std::format("packet {}", i));

        if (pak.SeekPacket(i) != 0)
        {
            continue;
        }

        buffer.assign(static_cast<size_t>(pak.GetPacketSize()) + 1, 0);
        CHECK_EQ(pak.ReadPacket(i, buffer.data()), pak.GetPacketSize());
    }
}
