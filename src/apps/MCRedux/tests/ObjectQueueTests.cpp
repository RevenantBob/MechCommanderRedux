#include "stdafx.h"
#include "MCTest.h"
#include "object/objque.h"

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

    BaseObject mover;
    mover.partId = 0x200;
    BaseObject building;
    building.partId = buildingPartId;

    ObjectQueueNode moverList("TBlk15");
    ObjectQueueNode buildingList("RBlk15");
    moverList.addNode(&mover);
    buildingList.addNode(&building);

    ObjectQueue queue;
    queue.addList(&moverList);
    queue.addList(&buildingList);

    CHECK(queue.findObjectFromPart(buildingPartId) == &building);

    // With the block's TBlk list empty, it is found the same way.
    moverList.head = nullptr;
    moverList.tail = nullptr;
    CHECK(queue.findObjectFromPart(buildingPartId) == &building);

    // A part id that is in neither list is not found.
    CHECK(queue.findObjectFromPart(buildingPartId + 8) == nullptr);
}
