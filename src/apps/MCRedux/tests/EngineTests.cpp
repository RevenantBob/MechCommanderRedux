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
    class MCRecordingElement : public Element
    {
    public:
        MCRecordingElement(int32_t depth, std::vector<int32_t>* draws, int32_t* deaths)
            : Element(depth), _Draws(draws), _Deaths(deaths)
        {
        }

        ~MCRecordingElement() override { ++*_Deaths; }

        void draw() override { _Draws->push_back(static_cast<int32_t>(depth)); }

        /// <summary>A field no constructor sets: the pool's storage starts zeroed.</summary>
        int32_t Untouched;

    private:
        std::vector<int32_t>* _Draws;
        int32_t* _Deaths;
    };
}

TEST_CASE("elements: the pool makes zeroed elements and frees them all on reset")
{
    ElementPool::reset();
    std::vector<int32_t> draws;
    int32_t deaths = 0;

    // More elements than any frame's ElementHeapSize allowed: the pool grows instead of failing.
    for (int32_t i = 0; i < 5000; ++i)
    {
        auto* element = ElementPool::Make<MCRecordingElement>(i, &draws, &deaths);
        REQUIRE(element != nullptr);
        MCTest::Scope scope(std::format("element {}", i));
        CHECK_EQ(element->Untouched, 0);
    }

    CHECK_EQ(ElementPool::elementCount, 5000);
    ElementPool::reset();
    CHECK_EQ(deaths, 5000);
    CHECK_EQ(ElementPool::elementCount, 0);
    ElementPool::free();
}

TEST_CASE("elements: groups draw deepest first, their elements sorted when asked")
{
    ElementPool::reset();
    std::vector<int32_t> draws;
    int32_t deaths = 0;
    ElementBuffer buffer;
    REQUIRE_EQ(buffer.init(8, 0, 3), 0);
    const int savedMaxObjectsDrawn = MaxObjectsDrawn;
    MaxObjectsDrawn = 0;

    // Group 1 (opened by reset, depth 0, sorted): elements 5, 9, 7.
    buffer.add(ElementPool::Make<MCRecordingElement>(5, &draws, &deaths));
    buffer.add(ElementPool::Make<MCRecordingElement>(9, &draws, &deaths));
    buffer.add(ElementPool::Make<MCRecordingElement>(7, &draws, &deaths));
    // Group 2 at depth 100, unsorted: 1, 3.
    buffer.openGroup(100, 0);
    buffer.add(ElementPool::Make<MCRecordingElement>(1, &draws, &deaths));
    buffer.add(ElementPool::Make<MCRecordingElement>(3, &draws, &deaths));
    // Group 3 at depth 50, sorted: 2, 4.
    buffer.openGroup(50, 1);
    buffer.add(ElementPool::Make<MCRecordingElement>(2, &draws, &deaths));
    buffer.add(ElementPool::Make<MCRecordingElement>(4, &draws, &deaths));
    // No group is left: the next open marks the frame as having too many objects and keeps adding to group 3.
    buffer.openGroup(10, 1);
    CHECK_EQ(MaxObjectsDrawn, 1);
    CHECK_EQ(buffer.numGroups, 3);
    buffer.add(ElementPool::Make<MCRecordingElement>(6, &draws, &deaths));
    // The buffer holds 8 elements: the ninth is dropped.
    buffer.add(ElementPool::Make<MCRecordingElement>(8, &draws, &deaths));
    CHECK_EQ(buffer.numElements, 8);

    buffer.sort();
    buffer.draw();
    // Groups by depth, deepest first (100, 50, 0); inside a sorted group deepest first too.
    const std::vector<int32_t> expected = {1, 3, 6, 4, 2, 9, 7, 5};
    CHECK(draws == expected);

    buffer.free();
    ElementPool::reset();
    CHECK_EQ(deaths, 9);
    MaxObjectsDrawn = savedMaxObjectsDrawn;
}

TEST_CASE("bitflag: flags address rows of whole bytes, column == columns reaching the next row")
{
    BitFlag flags{};
    REQUIRE_EQ(flags.init(4, 16, 0), 0);
    CHECK_EQ(flags.totalRAM, 8u);
    CHECK_EQ(flags.getFlag(1, 0), 0);

    // Column 16 of row 0 is bit 0 of row 1's first byte.
    flags.setFlag(0, 16);
    CHECK_EQ(flags.getFlag(1, 0), 1);
    CHECK_EQ(flags.getFlag(0, 16), 1);

    // On the last row it is the byte after the grid, which the original's heap slack held: set and read back.
    flags.setFlag(3, 16);
    CHECK_EQ(flags.getFlag(3, 16), 1);

    // Rows past the grid read 0 and aren't written.
    flags.setFlag(4, 0);
    CHECK_EQ(flags.getFlag(4, 0), 0);

    flags.setFlag(2, 13);
    CHECK_EQ(flags.getFlag(2, 13), 1);
    CHECK_EQ(flags.getFlag(2, 12), 0);
    flags.resetAll(1);
    CHECK_EQ(flags.getFlag(0, 3), 1);
    flags.resetAll(0);
    CHECK_EQ(flags.getFlag(2, 13), 0);
    flags.destroy();
    CHECK(flags.flagData.empty());
}
