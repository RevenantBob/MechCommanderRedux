#include "stdafx.h"
#include "object/collsn.h"
#include "ai/move.h"
#include "lib/aerror.h"
#include "lib/inifile.h"
#include "main/main.h"
#include "mission/scenario.h"
#include "object/gameobj.h"
#include "object/mover.h"
#include "object/object.h"
#include "object/objque.h"
#include "object/objtype.h"

// The original compares sums of squares on the x87 stack at extended precision; the port does those in double.

uint32_t CollisionSystem::xGridSize = 0;
uint32_t CollisionSystem::yGridSize = 0;
uint32_t CollisionSystem::gridRadius = 0;
uint32_t CollisionSystem::maxObjects = 0;
uint32_t CollisionSystem::maxCollisions = 0;
uint32_t CollisionSystem::numCollisions = 0;
float CollisionSystem::alertTime = 0.0f;
float CollisionSystem::warningDist = 0.0f;
GlobalCollisionAlert* globalCollisionAlert = nullptr;

namespace
{
    /// <summary>Takes <paramref name="object"/> off whichever object list holds it.</summary>
    void RemoveFromObjectList(GameObject* object)
    {
        for (ObjectQueueNode* node = objectList->head; node != nullptr; node = node->next)
        {
            if (node->remove(object) != 0)
            {
                break;
            }
        }
    }
}

//---------------------------------------------------------------------------
// GlobalCollisionAlert
//---------------------------------------------------------------------------

auto GlobalCollisionAlert::init(uint32_t maxCollisionAlerts) -> int32_t
{
    maxAlerts = maxCollisionAlerts;
    // Port fix: sized by the port's struct (0x10 bytes in the original).
    collisionAlerts = std::make_unique<CollisionAlertRecord[]>(maxCollisionAlerts);
    purgeRecords();
    return 0;
}

auto GlobalCollisionAlert::destroy() -> void
{
    collisionAlerts.reset();
    nextRecord = 0;
    maxAlerts = 0;
}

auto GlobalCollisionAlert::addRecord(GameObject* obj1, GameObject* obj2, float distance, float time) -> int32_t
{
    if (nextRecord >= maxAlerts)
    {
        return static_cast<int32_t>(0xccef000b);
    }

    CollisionAlertRecord& record = collisionAlerts[nextRecord];
    record.object1 = obj1;
    record.object2 = obj2;
    record.distance = distance;
    record.time = time;
    nextRecord++;
    return 0;
}

auto GlobalCollisionAlert::findAlert(GameObject* object, CollisionAlertRecord* startRecord) -> CollisionAlertRecord*
{
    uint32_t start = 0;

    if (startRecord != nullptr)
    {
        int32_t index = 0;

        while (index < static_cast<int32_t>(nextRecord) && &collisionAlerts[index] != startRecord)
        {
            index++;
        }

        start = static_cast<uint32_t>(index + 1);
    }

    for (uint32_t i = start; static_cast<int32_t>(i) < static_cast<int32_t>(nextRecord); i++)
    {
        if (collisionAlerts[i].object1 == object || collisionAlerts[i].object2 == object)
        {
            return &collisionAlerts[i];
        }
    }

    return nullptr;
}

auto GlobalCollisionAlert::purgeRecords() -> void
{
    nextRecord = 0;

    for (int32_t i = 0; i < static_cast<int32_t>(maxAlerts); i++)
    {
        collisionAlerts[i].object1 = nullptr;
        collisionAlerts[i].object2 = nullptr;
        collisionAlerts[i].distance = 0.0f;
        collisionAlerts[i].time = 0.0f;
    }
}

//---------------------------------------------------------------------------
// CollisionGrid
//---------------------------------------------------------------------------

auto CollisionGrid::init(vector_3d& newOrigin) -> int32_t
{
    if (gridIsGo == 0)
    {
        // The original sizes both sides from XGridSize; YGridSize is read but never used.
        xGridWidth = CollisionSystem::xGridSize;
        yGridWidth = CollisionSystem::xGridSize;
        gridRadius = CollisionSystem::gridRadius;
        maxObjects = CollisionSystem::maxObjects;
        // Port fix: sized by the port's pointer and node (4 and 8 bytes in the original).
        gridSize = xGridWidth * yGridWidth * static_cast<uint32_t>(sizeof(CollisionGridNode*));
        nodeTableSize = maxObjects * static_cast<uint32_t>(sizeof(CollisionGridNode));

        grid = std::make_unique<CollisionGridNode*[]>(gridSize / sizeof(CollisionGridNode*));
        nodes = std::make_unique<CollisionGridNode[]>(maxObjects);

        gridIsGo = 1;
        gridXOffset = static_cast<float>(((xGridWidth + 1) * gridRadius) >> 1);
        gridYOffset = static_cast<float>(((yGridWidth + 1) * gridRadius) >> 1);
        gridXCheck = static_cast<float>(gridRadius * xGridWidth);
        gridYCheck = static_cast<float>(gridRadius * yGridWidth);
    }

    std::memset(grid.get(), 0, gridSize);
    std::memset(nodes.get(), 0, nodeTableSize);
    nextAvailableNode = 0;
    giantObjects = nullptr;
    gridOrigin = newOrigin;
    return 0;
}

auto CollisionGrid::destroy() -> void
{
    if (gridIsGo == 0)
    {
        return;
    }

    nodes.reset();
    grid.reset();

    yGridWidth = 0;
    xGridWidth = 0;
    gridRadius = 0;
    giantObjects = nullptr;
    grid = nullptr;
    nodes = nullptr;
    nextAvailableNode = 0;
    gridOrigin.z = 0.0f;
    gridOrigin.y = 0.0f;
    gridOrigin.x = 0.0f;
    gridIsGo = 0;
}

auto CollisionGrid::add(uint32_t gridIndex, GameObject* object) -> int32_t
{
    if (nextAvailableNode >= maxObjects)
    {
        return static_cast<int32_t>(0xccf00002);
    }

    if (gridIndex >= yGridWidth * xGridWidth)
    {
        return static_cast<int32_t>(0xccf00003);
    }

    CollisionGridNode* node = &nodes[nextAvailableNode];
    nextAvailableNode++;
    node->next = grid[gridIndex];
    grid[gridIndex] = node;
    node->object = object;
    return 0;
}

auto CollisionGrid::add(GameObject* object) -> int32_t
{
    if (object->collisionsOn == 0)
    {
        return 0;
    }

    if (nextAvailableNode >= maxObjects)
    {
        return static_cast<int32_t>(0xccf00002);
    }

    const uint32_t cellSize = gridRadius;

    if (object->getExtentRadius() <= static_cast<float>(cellSize))
    {
        // Positions are centred on 0: shift by half the grid, clamp to it, and divide by the cell size.
        float x = object->getPosition().x + gridXOffset;

        if (x < 0.0f)
        {
            x = 0.0f;
        }

        if (gridXCheck <= x)
        {
            x = gridXCheck - 1.0f;
        }

        x = x / static_cast<float>(cellSize);

        float y = object->getPosition().y + gridYOffset;

        if (y < 0.0f)
        {
            y = 0.0f;
        }

        if (gridYCheck <= y)
        {
            y = gridYCheck - 1.0f;
        }

        const int32_t row = static_cast<int32_t>(std::floor(static_cast<double>(y / static_cast<float>(cellSize))));
        const int32_t col = static_cast<int32_t>(std::floor(static_cast<double>(x)));
        return add(static_cast<uint32_t>(col + static_cast<int32_t>(xGridWidth) * row), object);
    }

    // Bigger than a cell: the giant list.
    CollisionGridNode* node = &nodes[nextAvailableNode];
    nextAvailableNode++;
    CollisionGridNode* oldFirst = giantObjects;
    giantObjects = node;
    node->object = object;
    node->next = oldFirst;
    return 0;
}

auto CollisionGrid::createGrid() -> void
{
    for (CollisionGridNode* node = giantObjects; node != nullptr; node = node->next)
    {
        if (node->next != nullptr)
        {
            checkGrid(node->object, node->next);
        }
    }

    const int32_t width = static_cast<int32_t>(xGridWidth);
    const int32_t height = static_cast<int32_t>(yGridWidth);

    for (int32_t row = 0; row < height; row++)
    {
        for (int32_t col = 0; col < width; col++)
        {
            const int32_t index = width * row + col;

            for (CollisionGridNode* node = grid[index]; node != nullptr; node = node->next)
            {
                GameObject* object = node->object;

                if (giantObjects != nullptr)
                {
                    checkGrid(object, giantObjects);
                }

                if (node->next != nullptr)
                {
                    checkGrid(object, node->next);
                }

                if (col < width - 1)
                {
                    if (grid[index + 1] != nullptr)
                    {
                        checkGrid(object, grid[index + 1]);
                    }

                    if (row < height - 1 && grid[index + width + 1] != nullptr)
                    {
                        checkGrid(object, grid[index + width + 1]);
                    }
                }

                if (row < height - 1 && grid[index + width] != nullptr)
                {
                    checkGrid(object, grid[index + width]);
                }
            }
        }
    }
}

auto CollisionGrid::checkGrid(GameObject* object, CollisionGridNode* area) -> void
{
    while (area != nullptr && CollisionSystem::numCollisions < CollisionSystem::maxCollisions)
    {
        GameObject* other = area->object;
        area = area->next;

        if (object == nullptr || other == nullptr)
        {
            continue;
        }

        // Pairs that never collide: turrets, gates, train cars and explosions among themselves.
        const ObjectClass class1 = object->objectClass;
        const ObjectClass class2 = other->objectClass;

        if (class1 == TURRET && class2 == TURRET)
        {
            continue;
        }

        if (class1 == GATE && (class2 == GATE || class2 == TURRET))
        {
            continue;
        }

        if (class1 == TURRET && class2 == GATE)
        {
            continue;
        }

        if (class1 == TRAINCAR && class2 == TRAINCAR)
        {
            continue;
        }

        if (class1 == EXPLOSION && class2 == EXPLOSION)
        {
            continue;
        }

        collisionSystem->detectCollision(object, other);
    }
}

//---------------------------------------------------------------------------
// CollisionSystem
//---------------------------------------------------------------------------

auto CollisionSystem::init(FitIniFile* scenarioFile) -> int32_t
{
    int32_t result = scenarioFile->seekBlock("CollisionSystem");

    if (result != 0)
    {
        return result;
    }

    if ((result = scenarioFile->readIdULong("XGridSize", xGridSize)) != 0)
    {
        return result;
    }

    if ((result = scenarioFile->readIdULong("YGridSize", yGridSize)) != 0)
    {
        return result;
    }

    if ((result = scenarioFile->readIdULong("GridRadius", gridRadius)) != 0)
    {
        return result;
    }

    if ((result = scenarioFile->readIdULong("MaxObjects", maxObjects)) != 0)
    {
        return result;
    }

    maxObjects = 1200;

    if ((result = scenarioFile->readIdULong("MaxCollisions", maxCollisions)) != 0)
    {
        return result;
    }

    if ((result = scenarioFile->readIdULong("MaxPending", maxPending)) != 0)
    {
        return result;
    }

    if ((result = scenarioFile->readIdFloat("WarningDist", warningDist)) != 0)
    {
        return result;
    }

    if ((result = scenarioFile->readIdFloat("AlertTime", alertTime)) != 0)
    {
        return result;
    }

    uint32_t numAlerts = 0;

    if ((result = scenarioFile->readIdULong("NumAlerts", numAlerts)) != 0)
    {
        return result;
    }

    uint32_t heapSize = 0;

    if ((result = scenarioFile->readIdULong("CollisionHeapSize", heapSize)) != 0)
    {
        return result;
    }

    // Read, then ignored: the collision heap is gone.
    static_cast<void>(heapSize);
    collisionList = std::make_unique<CollisionRecord[]>(maxCollisions);

    collisionGrid = new CollisionGrid;

    if (collisionGrid == nullptr)
    {
        return static_cast<int32_t>(0xccef0009);
    }

    firstPending = nullptr;

    globalCollisionAlert = new GlobalCollisionAlert;
    return globalCollisionAlert->init(numAlerts);
}

auto CollisionSystem::checkObjects() -> void
{
    vector_3d origin;
    origin.x = 0.0f;
    origin.y = 0.0f;
    origin.z = 0.0f;
    collisionGrid->init(origin);
    globalCollisionAlert->purgeRecords();
    firstPending = nullptr;
    numCollisions = 0;
    std::memset(collisionList.get(), 0, maxCollisions * sizeof(CollisionRecord));

    // The first three object lists go into the grid.
    int32_t listCounts[3] = {};
    int32_t listIndex = 0;
    GameObject* object = nullptr;
    ObjectQueueNode* list = objectList->head;

    while (list != nullptr && listIndex <= 2)
    {
        if (object == nullptr || object->getObjectType() == nullptr)
        {
            object = static_cast<GameObject*>(list->head);
        }
        else
        {
            listCounts[listIndex]++;

            if (collisionGrid->add(object) != 0)
            {
                Fatal(-1, " No More Collision Nodes ");
            }

            object->handleStaticCollision();
            object = static_cast<GameObject*>(object->next);
        }

        if (object == nullptr)
        {
            list = list->next;
            listIndex++;
        }
    }

    collisionGrid->createGrid();
}

auto CollisionSystem::checkAlarums() -> void
{
    // The object at position <index> of a list.
    auto objectAt = [](ObjectQueueNode* list, int32_t index)
    {
        GameObject* object = nullptr;

        do
        {
            object = static_cast<GameObject*>(object == nullptr ? list->head : object->next);
        } while (object != nullptr && index-- > 0);

        return object;
    };

    for (ObjectQueueNode* list : {innerSphereMechList, clanMechList})
    {
        if (list == nullptr)
        {
            continue;
        }

        int32_t count = 0;

        for (BaseObject* object = list->head; object != nullptr; object = object->next)
        {
            count++;
        }

        for (int32_t i = 0; i < count - 1; i++)
        {
            for (int32_t j = i + 1; j < count; j++)
            {
                checkCollisionAlerts(objectAt(list, i), objectAt(list, j), 1);
            }
        }
    }
}

auto CollisionSystem::detectCollision(GameObject* obj1, GameObject* obj2) -> void
{
    if (obj1->objectClass < EXPLOSION && obj2->objectClass < EXPLOSION)
    {
        // Two movers touch when they share a terrain vertex and cell.
        int32_t block1;
        int32_t vertex1;
        int32_t block2;
        int32_t vertex2;
        obj1->getBlockAndVertexNumber(block1, vertex1);
        obj2->getBlockAndVertexNumber(block2, vertex2);

        if (block1 == block2 && vertex1 == vertex2 && obj1->getObjPosition()->cellR == obj2->getObjPosition()->cellR &&
            obj1->getObjPosition()->cellC == obj2->getObjPosition()->cellC)
        {
            checkExtents(obj1, obj2, 0.0f);
        }

        return;
    }

    const vector_3d position1 = obj1->getPosition();
    const vector_3d position2 = obj2->getPosition();
    const float dx = position2.x - position1.x;
    const float dy = position2.y - position1.y;
    const float radius1 = obj1->getExtentRadius();
    const double reach = static_cast<double>(obj2->getExtentRadius()) + radius1;

    if (static_cast<double>(dx) * dx + static_cast<double>(dy) * dy < reach * reach)
    {
        checkExtents(obj1, obj2, 0.0f);
    }

    checkCollisionAlerts(obj1, obj2, 1);
}

auto CollisionSystem::detectStaticCollision(GameObject* obj1, GameObject* obj2) -> void
{
    const vector_3d position1 = obj1->getPosition();
    const vector_3d position2 = obj2->getPosition();
    const float dx = position2.x - position1.x;
    const float dy = position2.y - position1.y;
    const float radius1 = obj1->getExtentRadius();
    const double reach = static_cast<double>(obj2->getExtentRadius()) + radius1;

    if (!(static_cast<double>(dx) * dx + static_cast<double>(dy) * dy < reach * reach))
    {
        return;
    }

    const ObjectClass class1 = obj1->objectClass;
    const bool isMover = class1 == BATTLEMECH || class1 == GROUNDVEHICLE || class1 == ELEMENTAL || class1 == MOVER;

    if (isMover && obj2->objectClass != TREE && obj2->objectClass != MISCTERRAINOBJECT)
    {
        // A mover whose own cell is passable goes through.
        const _ObjectPosition* objPosition = obj1->getObjPosition();
        MapTile& tile = GameMap->map[GameMap->width * objPosition->tileR + objPosition->tileC];

        if (tile.getCellPassable(objPosition->cellR, objPosition->cellC) != 0)
        {
            return;
        }
    }

    checkExtents(obj1, obj2, 0.0f);
}

auto CollisionSystem::checkCollisionAlerts(GameObject* obj1, GameObject* obj2, int setAlert) -> float
{
    if (obj1 == nullptr || obj2 == nullptr)
    {
        return -1.0f;
    }

    const vector_3d position1 = obj1->getPosition();
    const vector_3d position2 = obj2->getPosition();
    vector_3d delta;
    delta.x = position2.x - position1.x;
    delta.y = position2.y - position1.y;
    delta.z = position2.z - position1.z;
    const float distanceSq =
        static_cast<float>(static_cast<double>(delta.y) * delta.y + static_cast<double>(delta.x) * delta.x);
    obj1->getExtentRadius();
    obj2->getExtentRadius();

    // Only mechs and vehicles; two mechs only of one side.
    const ObjectClass class1 = obj1->objectClass;
    const ObjectClass class2 = obj2->objectClass;

    if (class1 != BATTLEMECH && class1 != GROUNDVEHICLE)
    {
        return -1.0f;
    }

    if (class2 != BATTLEMECH && class2 != GROUNDVEHICLE)
    {
        return -1.0f;
    }

    if (obj1->getAlignment() != obj2->getAlignment() && class1 != GROUNDVEHICLE && class2 != GROUNDVEHICLE)
    {
        return -1.0f;
    }

    if (!(distanceSq < warningDist))
    {
        return -1.0f;
    }

    // The time of closest approach, from the relative velocity in world units.
    const vector_3d velocity1 = obj1->getVelocity();
    const vector_3d velocity2 = obj2->getVelocity();
    vector_3d relVelocity;
    relVelocity.x = velocity2.x * worldUnitsPerMeter - velocity1.x * worldUnitsPerMeter;
    relVelocity.y = velocity2.y * worldUnitsPerMeter - velocity1.y * worldUnitsPerMeter;
    relVelocity.z = velocity2.z * worldUnitsPerMeter - velocity1.z * worldUnitsPerMeter;
    float time = 10000000.0f;
    const double relSpeedSq = static_cast<double>(relVelocity.z) * relVelocity.z +
                              static_cast<double>(relVelocity.y) * relVelocity.y +
                              static_cast<double>(relVelocity.x) * relVelocity.x;

    if (relSpeedSq != 0.0)
    {
        const double closing = static_cast<double>(delta.z) * relVelocity.z +
                               static_cast<double>(relVelocity.y) * delta.y +
                               static_cast<double>(relVelocity.x) * delta.x;
        time = static_cast<float>(-(closing / relSpeedSq));

        if (time < 0.0f)
        {
            return time;
        }
    }

    // Only if one of them is still moving then.
    float stopTime1;
    float stopOther1;
    float stopTime2;
    float stopOther2;
    static_cast<Mover*>(obj1)->getStopInfo(stopTime1, stopOther1);
    static_cast<Mover*>(obj2)->getStopInfo(stopTime2, stopOther2);

    if (!(time <= stopTime1) && !(time <= stopTime2))
    {
        return -1.0f;
    }

    vector_3d newPosition1;
    newPosition1.x = velocity1.x * time * worldUnitsPerMeter + position1.x;
    newPosition1.y = velocity1.y * time * worldUnitsPerMeter + position1.y;
    newPosition1.z = velocity1.z * time * worldUnitsPerMeter + position1.z;
    vector_3d newPosition2;
    newPosition2.x = position2.x + velocity2.x * time * worldUnitsPerMeter;
    newPosition2.y = position2.y + velocity2.y * time * worldUnitsPerMeter;
    newPosition2.z = position2.z + velocity2.z * time * worldUnitsPerMeter;
    const float gapX = newPosition2.x - newPosition1.x;
    const float gapY = newPosition2.y - newPosition1.y;
    const float gapSq = static_cast<float>(static_cast<double>(gapY) * gapY + static_cast<double>(gapX) * gapX);

    const float radius1 = obj1->getExtentRadius();
    const double reach = static_cast<double>(obj2->getExtentRadius()) + radius1;

    if (!(gapSq < reach * reach))
    {
        return -1.0f;
    }

    if (setAlert != 0)
    {
        globalCollisionAlert->addRecord(obj1, obj2, gapSq, time);
    }

    return time;
}

auto CollisionSystem::checkExtents(GameObject* obj1, GameObject* obj2, float) -> void
{
    ObjectType* type1 = obj1->getObjectType();
    ObjectType* type2 = obj2->getObjectType();
    const int collides1 = type1->handleCollision(obj1, obj2);
    const int collides2 = type2->handleCollision(obj2, obj1);

    if (collides1 != 0 && type1->handleDestruction(obj1, obj2) != 0)
    {
        RemoveFromObjectList(obj1);
    }

    if (collides2 != 0 && type2->handleDestruction(obj2, obj1) != 0)
    {
        RemoveFromObjectList(obj2);
    }
}

auto CollisionSystem::processCollisions() -> void
{
    for (CollisionRecord* record = firstPending; record != nullptr; record = record->next)
    {
        GameObject* obj1 = record->obj1;
        GameObject* obj2 = record->obj2;

        if (obj1 == nullptr || obj2 == nullptr)
        {
            continue;
        }

        ObjectType* type1 = obj1->getObjectType();
        ObjectType* type2 = obj2->getObjectType();
        const int collides1 = type1->handleCollision(obj1, obj2);
        const int collides2 = type2->handleCollision(obj2, obj1);

        if (collides1 != 0 && type1->handleDestruction(obj1, obj2) != 0)
        {
            removeCollisions(obj1, record);
            RemoveFromObjectList(obj1);
        }

        if (collides2 != 0 && type2->handleDestruction(obj2, obj1) != 0)
        {
            removeCollisions(obj2, record);
            RemoveFromObjectList(obj2);
        }

        record->obj1 = nullptr;
        record->obj2 = nullptr;
    }
}

auto CollisionSystem::removeCollisions(GameObject* object, CollisionRecord* record) -> void
{
    for (CollisionRecord* next = record->next; next != nullptr; next = next->next)
    {
        if (next->obj1 == object)
        {
            next->obj1 = nullptr;
        }

        if (next->obj2 == object)
        {
            next->obj2 = nullptr;
        }
    }
}

auto CollisionSystem::findNextPending() -> CollisionRecord*
{
    return nullptr;
}

auto CollisionSystem::addPendingCollision(GameObject*, GameObject*, float) -> int32_t
{
    return 0;
}

auto CollisionSystem::timeToImpact(GameObject* obj1, GameObject* obj2) -> float
{
    const vector_3d position1 = obj1->getPosition();
    const vector_3d position2 = obj2->getPosition();
    vector_3d delta;
    delta.x = position2.x - position1.x;
    delta.y = position2.y - position1.y;
    delta.z = position2.z - position1.z;
    const float distanceSq =
        static_cast<float>(static_cast<double>(delta.y) * delta.y + static_cast<double>(delta.x) * delta.x);
    const float radius1 = obj1->getExtentRadius();
    const double reach = static_cast<double>(obj2->getExtentRadius()) + radius1;
    const float reachSq = static_cast<float>(reach * reach);

    if (distanceSq < reachSq)
    {
        return 0.0f;
    }

    // Where each will be at the end of the frame, and how far each moves.
    const vector_3d velocity1 = obj1->getVelocity();
    const vector_3d velocity2 = obj2->getVelocity();
    vector_3d end1;
    end1.x = velocity1.x * frameLength + position1.x;
    end1.y = velocity1.y * frameLength + position1.y;
    end1.z = velocity1.z * frameLength + position1.z;
    vector_3d end2;
    end2.x = velocity2.x * frameLength + position2.x;
    end2.y = velocity2.y * frameLength + position2.y;
    end2.z = velocity2.z * frameLength + position2.z;
    vector_3d move1;
    move1.x = end1.x - position1.x;
    move1.y = end1.y - position1.y;
    move1.z = end1.z - position1.z;
    vector_3d move2;
    move2.x = end2.x - position2.x;
    move2.y = end2.y - position2.y;
    move2.z = end2.z - position2.z;

    // operator| (MCX.EXE @ 0x00658270): z, y, then x.
    auto dot = [](const vector_3d& a, const vector_3d& b)
    { return static_cast<double>(a.z) * b.z + static_cast<double>(a.y) * b.y + static_cast<double>(a.x) * b.x; };

    if (!(dot(move1, move2) < 0.0))
    {
        return -1.0f;
    }

    vector_3d relVelocity;
    relVelocity.x = velocity2.x - velocity1.x;
    relVelocity.y = velocity2.y - velocity1.y;
    relVelocity.z = velocity2.z - velocity1.z;
    const float closing = static_cast<float>(dot(delta, relVelocity));
    const float time = static_cast<float>(-(closing / dot(relVelocity, relVelocity)));

    if (!(time <= frameLength))
    {
        return -1.0f;
    }

    const float closestX = delta.x + relVelocity.x * time;
    const float closestY = delta.y + relVelocity.y * time;

    if (static_cast<double>(closestY) * closestY + static_cast<double>(closestX) * closestX < reachSq)
    {
        return time;
    }

    return -1.0f;
}

auto CollisionSystem::destroy() -> void
{
    if (collisionGrid != nullptr)
    {
        collisionGrid->destroy();
        delete collisionGrid;
    }

    collisionGrid = nullptr;

    collisionList.reset();

    if (globalCollisionAlert != nullptr)
    {
        globalCollisionAlert->destroy();
        delete globalCollisionAlert;
    }

    globalCollisionAlert = nullptr;
}
