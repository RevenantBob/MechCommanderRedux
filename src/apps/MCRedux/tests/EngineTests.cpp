#include "stdafx.h"
#include "MCTest.h"
#include "camera/camera.h"
#include "engine/bitflag.h"
#include "engine/ceglist.h"
#include "engine/celement.h"
#include "object/objque.h"

namespace
{
    /// <summary>A draw element that records its draws and its destruction.</summary>
    class MCRecordingElement : public MCElement
    {
    public:
        MCRecordingElement(int32_t depth, std::vector<int32_t>* draws, int32_t* deaths)
            : MCElement(depth), _Draws(draws), _Deaths(deaths)
        {
        }

        ~MCRecordingElement() override { ++*_Deaths; }

        void Draw() override { _Draws->push_back(static_cast<int32_t>(Depth)); }

        /// <summary>A field no constructor sets: the pool's storage starts zeroed.</summary>
        int32_t Untouched;

    private:
        std::vector<int32_t>* _Draws;
        int32_t* _Deaths;
    };
}

TEST_CASE("elements: the pool makes zeroed elements and frees them all on reset")
{
    MCElementPool::Reset();
    std::vector<int32_t> draws;
    int32_t deaths = 0;

    // More elements than any frame's ElementHeapSize allowed: the pool grows instead of failing.
    for (int32_t i = 0; i < 5000; ++i)
    {
        auto* element = MCElementPool::Make<MCRecordingElement>(i, &draws, &deaths);
        REQUIRE(element != nullptr);
        MCTest::Scope scope(std::format("element {}", i));
        CHECK_EQ(element->Untouched, 0);
    }

    CHECK_EQ(MCElementPool::ElementCount, 5000);
    MCElementPool::Reset();
    CHECK_EQ(deaths, 5000);
    CHECK_EQ(MCElementPool::ElementCount, 0);
    MCElementPool::Free();
}

TEST_CASE("elements: groups draw deepest first, their elements sorted when asked")
{
    MCElementPool::Reset();
    std::vector<int32_t> draws;
    int32_t deaths = 0;
    MCElementBuffer buffer;
    REQUIRE_EQ(buffer.Init(8, 0, 3), 0);
    const int savedMaxObjectsDrawn = MaxObjectsDrawn;
    MaxObjectsDrawn = 0;

    // Group 1 (opened by reset, depth 0, sorted): elements 5, 9, 7.
    buffer.Add(MCElementPool::Make<MCRecordingElement>(5, &draws, &deaths));
    buffer.Add(MCElementPool::Make<MCRecordingElement>(9, &draws, &deaths));
    buffer.Add(MCElementPool::Make<MCRecordingElement>(7, &draws, &deaths));
    // Group 2 at depth 100, unsorted: 1, 3.
    buffer.OpenGroup(100, 0);
    buffer.Add(MCElementPool::Make<MCRecordingElement>(1, &draws, &deaths));
    buffer.Add(MCElementPool::Make<MCRecordingElement>(3, &draws, &deaths));
    // Group 3 at depth 50, sorted: 2, 4.
    buffer.OpenGroup(50, 1);
    buffer.Add(MCElementPool::Make<MCRecordingElement>(2, &draws, &deaths));
    buffer.Add(MCElementPool::Make<MCRecordingElement>(4, &draws, &deaths));
    // No group is left: the next open marks the frame as having too many objects and keeps adding to group 3.
    buffer.OpenGroup(10, 1);
    CHECK_EQ(MaxObjectsDrawn, 1);
    CHECK_EQ(buffer.NumGroups, 3);
    buffer.Add(MCElementPool::Make<MCRecordingElement>(6, &draws, &deaths));
    // The buffer holds 8 elements: the ninth is dropped.
    buffer.Add(MCElementPool::Make<MCRecordingElement>(8, &draws, &deaths));
    CHECK_EQ(buffer.NumElements, 8);

    buffer.Sort();
    buffer.Draw();
    // Groups by depth, deepest first (100, 50, 0); inside a sorted group deepest first too.
    const std::vector<int32_t> expected = {1, 3, 6, 4, 2, 9, 7, 5};
    CHECK(draws == expected);

    buffer.Free();
    MCElementPool::Reset();
    CHECK_EQ(deaths, 9);
    MaxObjectsDrawn = savedMaxObjectsDrawn;
}

TEST_CASE("bitflag: flags address rows of whole bytes, column == columns reaching the next row")
{
    MCBitFlag flags{};
    REQUIRE_EQ(flags.Init(4, 16, 0), 0);
    CHECK_EQ(flags.TotalRam, 8u);
    CHECK_EQ(flags.GetFlag(1, 0), 0);

    // Column 16 of row 0 is bit 0 of row 1's first byte.
    flags.SetFlag(0, 16);
    CHECK_EQ(flags.GetFlag(1, 0), 1);
    CHECK_EQ(flags.GetFlag(0, 16), 1);

    // On the last row it is the byte after the grid, which the original's heap slack held: set and read back.
    flags.SetFlag(3, 16);
    CHECK_EQ(flags.GetFlag(3, 16), 1);

    // Rows past the grid read 0 and aren't written.
    flags.SetFlag(4, 0);
    CHECK_EQ(flags.GetFlag(4, 0), 0);

    flags.SetFlag(2, 13);
    CHECK_EQ(flags.GetFlag(2, 13), 1);
    CHECK_EQ(flags.GetFlag(2, 12), 0);
    flags.ResetAll(1);
    CHECK_EQ(flags.GetFlag(0, 3), 1);
    flags.ResetAll(0);
    CHECK_EQ(flags.GetFlag(2, 13), 0);
    flags.Destroy();
    CHECK(flags.FlagData.empty());
}
