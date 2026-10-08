#include "stdafx.h"
#include "MCTest.h"
#include "object/MCBaseObject.h"
#include "object/MCObjectQueue.h"

namespace
{
    /// <summary>A plain object with part id <paramref name="partId"/>.</summary>
    std::unique_ptr<MCBaseObject> NewObject(int32_t partId)
    {
        auto object = std::make_unique<MCBaseObject>();
        object->PartId = partId;
        return object;
    }

    /// <summary>The part ids of a list's objects, in walk order.</summary>
    std::vector<int32_t> PartIds(const MCObjectList& list)
    {
        std::vector<int32_t> ids;

        for (MCBaseObject* object : list)
        {
            ids.push_back(object->PartId);
        }

        return ids;
    }
}

/// <summary>
/// A mission script finds a building by its terrain part id (getterrainobjectpartid) and asks whether it is dead.
/// Buildings sit in their block's RBlk list; movers walking through the block go in its TBlk list. A mover in the
/// TBlk list must not hide the building: otherwise the lookup fails, the building counts as dead, and mission 1's
/// "destroy" objectives succeed on the first frame.
/// </summary>
TEST_CASE("objque: findObjectFromPart finds a building while a mover shares its block")
{
    // Mission 1's Camp Alpha HQ: block 15, cell 183.
    const int32_t buildingPartId = (15 * 400 + 183) * 8 + 0x1000;

    MCObjectQueue queue;
    MCObjectList* moverList = queue.AddList(std::make_unique<MCObjectList>("TBlk15"));
    MCObjectList* buildingList = queue.AddList(std::make_unique<MCObjectList>("RBlk15"));
    MCBaseObject* mover = moverList->Add(NewObject(0x200));
    MCBaseObject* building = buildingList->Add(NewObject(buildingPartId));

    CHECK(queue.FindObjectFromPart(buildingPartId) == building);

    // With the block's TBlk list empty, it is found the same way.
    CHECK(moverList->Remove(mover));
    CHECK(moverList->Empty());
    CHECK(queue.FindObjectFromPart(buildingPartId) == building);

    // A part id that is in neither list is not found.
    CHECK(queue.FindObjectFromPart(buildingPartId + 8) == nullptr);
}

TEST_CASE("objque: a queue starts with its DEFAULT list and finds lists by name")
{
    MCObjectQueue queue;
    REQUIRE_EQ(queue.Lists().size(), 1u);
    CHECK_EQ(queue.DefaultList().Name(), std::string(MCObjectQueue::DefaultListName));
    CHECK(queue.FindList("DEFAULT") == &queue.DefaultList());
    CHECK(queue.FindList("TBlk3") == nullptr);

    MCObjectList* block = queue.FindOrAddList("TBlk3");
    CHECK(block->IsTerrainList());
    CHECK(!queue.DefaultList().IsTerrainList());
    CHECK(queue.FindOrAddList("TBlk3") == block);
    CHECK_EQ(queue.Lists().size(), 2u);

    queue.DeleteList(block);
    CHECK(queue.FindList("TBlk3") == nullptr);
}

/// <summary>
/// An update or a collision deletes objects while their list is walked. MCX.EXE's walk went on from the deleted
/// object's stale next pointer; the port's leaves the deleted object's slot empty, so the walk goes on from it, passes
/// the empty slots and still reaches objects added at the end.
/// </summary>
TEST_CASE("objque: a walk goes on past objects removed under it and reaches ones added at the end")
{
    MCObjectList list("DEFAULT");

    for (int32_t id = 1; id <= 4; id++)
    {
        list.Add(NewObject(id));
    }

    std::vector<int32_t> walked;

    for (MCBaseObject* object : list)
    {
        walked.push_back(object->PartId);

        if (object->PartId == 2)
        {
            // The object being walked removes itself and the one after it, and adds one at the end.
            MCBaseObject* next = list.After(object);
            CHECK(list.Remove(object));
            CHECK(list.Remove(next));
            list.Add(NewObject(5));
        }
    }

    const std::vector<int32_t> expectedWalk = {1, 2, 4, 5};
    const std::vector<int32_t> expectedLeft = {1, 4, 5};
    CHECK(walked == expectedWalk);
    CHECK_EQ(list.Size(), 3u);
    CHECK(PartIds(list) == expectedLeft);

    // Compacting drops the empty slots and keeps the order.
    list.Compact();
    CHECK_EQ(list.Size(), 3u);
    CHECK(PartIds(list) == expectedLeft);
    CHECK_EQ(list.First()->PartId, 1);
}

TEST_CASE("objque: release hands an object over without deleting it")
{
    MCObjectList from("ISMECH");
    MCObjectList to("CLANMEC");
    MCBaseObject* object = from.Add(NewObject(7));

    std::unique_ptr<MCBaseObject> taken = from.Release(object);
    REQUIRE(taken.get() == object);
    CHECK(from.Empty());
    CHECK(from.Release(object) == nullptr);
    CHECK(!from.Remove(object));

    to.Add(std::move(taken));
    CHECK(to.FindPart(7) == object);
    CHECK(to.After(object) == nullptr);
    CHECK(to.After(nullptr) == object);
}

TEST_CASE("objque: the queue removes an object from whichever list holds it")
{
    MCObjectQueue queue;
    MCObjectList* block = queue.FindOrAddList("RBlk2");
    MCBaseObject* first = queue.DefaultList().Add(NewObject(1));
    MCBaseObject* second = block->Add(NewObject(2));

    CHECK(queue.FindIf([](MCBaseObject* object) { return object->PartId == 2; }) == second);
    CHECK(queue.Remove(second));
    CHECK(!queue.Remove(second));
    CHECK(block->Empty());
    CHECK(queue.FindIf([](MCBaseObject* object) { return object->PartId == 2; }) == nullptr);
    CHECK(queue.DefaultList().First() == first);
}
