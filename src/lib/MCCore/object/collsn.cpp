#include "stdafx.h"
#include "object/collsn.h"
#include "ai/move.h"
#include "lib/MCFatal.h"
#include "lib/MCFitIniFile.h"
#include "main/main.h"
#include "mission/scenario.h"
#include "object/gameobj.h"
#include "object/mover.h"
#include "object/object.h"
#include "object/objque.h"
#include "object/objtype.h"

// The original compares sums of squares on the x87 stack at extended precision; the port does those in double.

uint32_t MCCollisionSystem::XGridSize = 0;
uint32_t MCCollisionSystem::YGridSize = 0;
uint32_t MCCollisionSystem::GridRadius = 0;
uint32_t MCCollisionSystem::MaxObjects = 0;
uint32_t MCCollisionSystem::MaxCollisions = 0;
uint32_t MCCollisionSystem::NumCollisions = 0;
float MCCollisionSystem::AlertTime = 0.0f;
float MCCollisionSystem::WarningDist = 0.0f;
MCGlobalCollisionAlert* GlobalCollisionAlert = nullptr;

namespace
{
    /// <summary>Takes <paramref name="object"/> off whichever object list holds it.</summary>
    void RemoveFromObjectList(MCGameObject* object)
    {
        for (MCObjectQueueNode* node = ObjectList->Head; node != nullptr; node = node->Next)
        {
            if (node->Remove(object) != 0)
            {
                break;
            }
        }
    }
}

//---------------------------------------------------------------------------
// GlobalCollisionAlert
//---------------------------------------------------------------------------

auto MCGlobalCollisionAlert::Init(uint32_t maxCollisionAlerts) -> int32_t
{
    MaxAlerts = maxCollisionAlerts;
    // Port fix: sized by the port's struct (0x10 bytes in the original).
    CollisionAlerts = std::make_unique<MCCollisionAlertRecord[]>(maxCollisionAlerts);
    PurgeRecords();
    return 0;
}

auto MCGlobalCollisionAlert::Destroy() -> void
{
    CollisionAlerts.reset();
    NextRecord = 0;
    MaxAlerts = 0;
}

auto MCGlobalCollisionAlert::AddRecord(MCGameObject* obj1, MCGameObject* obj2, float distance, float time) -> int32_t
{
    if (NextRecord >= MaxAlerts)
    {
        return static_cast<int32_t>(0xccef000b);
    }

    MCCollisionAlertRecord& record = CollisionAlerts[NextRecord];
    record.Object1 = obj1;
    record.Object2 = obj2;
    record.Distance = distance;
    record.Time = time;
    NextRecord++;
    return 0;
}

auto MCGlobalCollisionAlert::FindAlert(MCGameObject* object, MCCollisionAlertRecord* startRecord)
    -> MCCollisionAlertRecord*
{
    uint32_t start = 0;

    if (startRecord != nullptr)
    {
        int32_t index = 0;

        while (index < static_cast<int32_t>(NextRecord) && &CollisionAlerts[index] != startRecord)
        {
            index++;
        }

        start = static_cast<uint32_t>(index + 1);
    }

    for (uint32_t i = start; static_cast<int32_t>(i) < static_cast<int32_t>(NextRecord); i++)
    {
        if (CollisionAlerts[i].Object1 == object || CollisionAlerts[i].Object2 == object)
        {
            return &CollisionAlerts[i];
        }
    }

    return nullptr;
}

auto MCGlobalCollisionAlert::PurgeRecords() -> void
{
    NextRecord = 0;

    for (int32_t i = 0; i < static_cast<int32_t>(MaxAlerts); i++)
    {
        CollisionAlerts[i].Object1 = nullptr;
        CollisionAlerts[i].Object2 = nullptr;
        CollisionAlerts[i].Distance = 0.0f;
        CollisionAlerts[i].Time = 0.0f;
    }
}

//---------------------------------------------------------------------------
// CollisionGrid
//---------------------------------------------------------------------------

auto MCCollisionGrid::Init(MCVector3D& newOrigin) -> int32_t
{
    if (GridIsGo == 0)
    {
        // The original sizes both sides from XGridSize; YGridSize is read but never used.
        XGridWidth = MCCollisionSystem::XGridSize;
        YGridWidth = MCCollisionSystem::XGridSize;
        GridRadius = MCCollisionSystem::GridRadius;
        MaxObjects = MCCollisionSystem::MaxObjects;
        // Port fix: sized by the port's pointer and node (4 and 8 bytes in the original).
        GridSize = XGridWidth * YGridWidth * static_cast<uint32_t>(sizeof(MCCollisionGridNode*));
        NodeTableSize = MaxObjects * static_cast<uint32_t>(sizeof(MCCollisionGridNode));

        Grid = std::make_unique<MCCollisionGridNode*[]>(GridSize / sizeof(MCCollisionGridNode*));
        Nodes = std::make_unique<MCCollisionGridNode[]>(MaxObjects);

        GridIsGo = 1;
        GridXOffset = static_cast<float>(((XGridWidth + 1) * GridRadius) >> 1);
        GridYOffset = static_cast<float>(((YGridWidth + 1) * GridRadius) >> 1);
        GridXCheck = static_cast<float>(GridRadius * XGridWidth);
        GridYCheck = static_cast<float>(GridRadius * YGridWidth);
    }

    std::memset(Grid.get(), 0, GridSize);
    std::memset(Nodes.get(), 0, NodeTableSize);
    NextAvailableNode = 0;
    GiantObjects = nullptr;
    GridOrigin = newOrigin;
    return 0;
}

auto MCCollisionGrid::Destroy() -> void
{
    if (GridIsGo == 0)
    {
        return;
    }

    Nodes.reset();
    Grid.reset();

    YGridWidth = 0;
    XGridWidth = 0;
    GridRadius = 0;
    GiantObjects = nullptr;
    Grid = nullptr;
    Nodes = nullptr;
    NextAvailableNode = 0;
    GridOrigin.Z = 0.0f;
    GridOrigin.Y = 0.0f;
    GridOrigin.X = 0.0f;
    GridIsGo = 0;
}

auto MCCollisionGrid::Add(uint32_t gridIndex, MCGameObject* object) -> int32_t
{
    if (NextAvailableNode >= MaxObjects)
    {
        return static_cast<int32_t>(0xccf00002);
    }

    if (gridIndex >= YGridWidth * XGridWidth)
    {
        return static_cast<int32_t>(0xccf00003);
    }

    MCCollisionGridNode* node = &Nodes[NextAvailableNode];
    NextAvailableNode++;
    node->Next = Grid[gridIndex];
    Grid[gridIndex] = node;
    node->Object = object;
    return 0;
}

auto MCCollisionGrid::Add(MCGameObject* object) -> int32_t
{
    if (object->CollisionsOn == 0)
    {
        return 0;
    }

    if (NextAvailableNode >= MaxObjects)
    {
        return static_cast<int32_t>(0xccf00002);
    }

    const uint32_t cellSize = GridRadius;

    if (object->GetExtentRadius() <= static_cast<float>(cellSize))
    {
        // Positions are centred on 0: shift by half the grid, clamp to it, and divide by the cell size.
        float x = object->GetPosition().X + GridXOffset;

        if (x < 0.0f)
        {
            x = 0.0f;
        }

        if (GridXCheck <= x)
        {
            x = GridXCheck - 1.0f;
        }

        x = x / static_cast<float>(cellSize);

        float y = object->GetPosition().Y + GridYOffset;

        if (y < 0.0f)
        {
            y = 0.0f;
        }

        if (GridYCheck <= y)
        {
            y = GridYCheck - 1.0f;
        }

        const int32_t row = static_cast<int32_t>(std::floor(static_cast<double>(y / static_cast<float>(cellSize))));
        const int32_t col = static_cast<int32_t>(std::floor(static_cast<double>(x)));
        return Add(static_cast<uint32_t>(col + static_cast<int32_t>(XGridWidth) * row), object);
    }

    // Bigger than a cell: the giant list.
    MCCollisionGridNode* node = &Nodes[NextAvailableNode];
    NextAvailableNode++;
    MCCollisionGridNode* oldFirst = GiantObjects;
    GiantObjects = node;
    node->Object = object;
    node->Next = oldFirst;
    return 0;
}

auto MCCollisionGrid::CreateGrid() -> void
{
    for (MCCollisionGridNode* node = GiantObjects; node != nullptr; node = node->Next)
    {
        if (node->Next != nullptr)
        {
            CheckGrid(node->Object, node->Next);
        }
    }

    const int32_t width = static_cast<int32_t>(XGridWidth);
    const int32_t height = static_cast<int32_t>(YGridWidth);

    for (int32_t row = 0; row < height; row++)
    {
        for (int32_t col = 0; col < width; col++)
        {
            const int32_t index = width * row + col;

            for (MCCollisionGridNode* node = Grid[index]; node != nullptr; node = node->Next)
            {
                MCGameObject* object = node->Object;

                if (GiantObjects != nullptr)
                {
                    CheckGrid(object, GiantObjects);
                }

                if (node->Next != nullptr)
                {
                    CheckGrid(object, node->Next);
                }

                if (col < width - 1)
                {
                    if (Grid[index + 1] != nullptr)
                    {
                        CheckGrid(object, Grid[index + 1]);
                    }

                    if (row < height - 1 && Grid[index + width + 1] != nullptr)
                    {
                        CheckGrid(object, Grid[index + width + 1]);
                    }
                }

                if (row < height - 1 && Grid[index + width] != nullptr)
                {
                    CheckGrid(object, Grid[index + width]);
                }
            }
        }
    }
}

auto MCCollisionGrid::CheckGrid(MCGameObject* object, MCCollisionGridNode* area) -> void
{
    while (area != nullptr && MCCollisionSystem::NumCollisions < MCCollisionSystem::MaxCollisions)
    {
        MCGameObject* other = area->Object;
        area = area->Next;

        if (object == nullptr || other == nullptr)
        {
            continue;
        }

        // Pairs that never collide: turrets, gates, train cars and explosions among themselves.
        const MCObjectClass class1 = object->ObjectClass;
        const MCObjectClass class2 = other->ObjectClass;

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

        CollisionSystem->DetectCollision(object, other);
    }
}

//---------------------------------------------------------------------------
// CollisionSystem
//---------------------------------------------------------------------------

auto MCCollisionSystem::Init(MCFitIniFile* scenarioFile) -> int32_t
{
    int32_t result = scenarioFile->SeekBlock("CollisionSystem");

    if (result != 0)
    {
        return result;
    }

    if ((result = scenarioFile->ReadIdULong("XGridSize", XGridSize)) != 0)
    {
        return result;
    }

    if ((result = scenarioFile->ReadIdULong("YGridSize", YGridSize)) != 0)
    {
        return result;
    }

    if ((result = scenarioFile->ReadIdULong("GridRadius", GridRadius)) != 0)
    {
        return result;
    }

    if ((result = scenarioFile->ReadIdULong("MaxObjects", MaxObjects)) != 0)
    {
        return result;
    }

    MaxObjects = 1200;

    if ((result = scenarioFile->ReadIdULong("MaxCollisions", MaxCollisions)) != 0)
    {
        return result;
    }

    if ((result = scenarioFile->ReadIdULong("MaxPending", MaxPending)) != 0)
    {
        return result;
    }

    if ((result = scenarioFile->ReadIdFloat("WarningDist", WarningDist)) != 0)
    {
        return result;
    }

    if ((result = scenarioFile->ReadIdFloat("AlertTime", AlertTime)) != 0)
    {
        return result;
    }

    uint32_t numAlerts = 0;

    if ((result = scenarioFile->ReadIdULong("NumAlerts", numAlerts)) != 0)
    {
        return result;
    }

    uint32_t heapSize = 0;

    if ((result = scenarioFile->ReadIdULong("CollisionHeapSize", heapSize)) != 0)
    {
        return result;
    }

    // Read, then ignored: the collision heap is gone.
    static_cast<void>(heapSize);
    CollisionList = std::make_unique<MCCollisionRecord[]>(MaxCollisions);

    CollisionGrid = new MCCollisionGrid;

    if (CollisionGrid == nullptr)
    {
        return static_cast<int32_t>(0xccef0009);
    }

    FirstPending = nullptr;

    GlobalCollisionAlert = new MCGlobalCollisionAlert;
    return GlobalCollisionAlert->Init(numAlerts);
}

auto MCCollisionSystem::CheckObjects() -> void
{
    MCVector3D origin;
    origin.X = 0.0f;
    origin.Y = 0.0f;
    origin.Z = 0.0f;
    CollisionGrid->Init(origin);
    GlobalCollisionAlert->PurgeRecords();
    FirstPending = nullptr;
    NumCollisions = 0;
    std::memset(CollisionList.get(), 0, MaxCollisions * sizeof(MCCollisionRecord));

    // The first three object lists go into the grid.
    int32_t listCounts[3] = {};
    int32_t listIndex = 0;
    MCGameObject* object = nullptr;
    MCObjectQueueNode* list = ObjectList->Head;

    while (list != nullptr && listIndex <= 2)
    {
        if (object == nullptr || object->GetObjectType() == nullptr)
        {
            object = static_cast<MCGameObject*>(list->Head);
        }
        else
        {
            listCounts[listIndex]++;

            if (CollisionGrid->Add(object) != 0)
            {
                Fatal(-1, " No More Collision Nodes ");
            }

            object->HandleStaticCollision();
            object = static_cast<MCGameObject*>(object->Next);
        }

        if (object == nullptr)
        {
            list = list->Next;
            listIndex++;
        }
    }

    CollisionGrid->CreateGrid();
}

auto MCCollisionSystem::CheckAlarums() -> void
{
    // The object at position <index> of a list.
    auto objectAt = [](MCObjectQueueNode* list, int32_t index)
    {
        MCGameObject* object = nullptr;

        do
        {
            object = static_cast<MCGameObject*>(object == nullptr ? list->Head : object->Next);
        } while (object != nullptr && index-- > 0);

        return object;
    };

    for (MCObjectQueueNode* list : {InnerSphereMechList, ClanMechList})
    {
        if (list == nullptr)
        {
            continue;
        }

        int32_t count = 0;

        for (MCBaseObject* object = list->Head; object != nullptr; object = object->Next)
        {
            count++;
        }

        for (int32_t i = 0; i < count - 1; i++)
        {
            for (int32_t j = i + 1; j < count; j++)
            {
                CheckCollisionAlerts(objectAt(list, i), objectAt(list, j), 1);
            }
        }
    }
}

auto MCCollisionSystem::DetectCollision(MCGameObject* obj1, MCGameObject* obj2) -> void
{
    if (obj1->ObjectClass < EXPLOSION && obj2->ObjectClass < EXPLOSION)
    {
        // Two movers touch when they share a terrain vertex and cell.
        int32_t block1;
        int32_t vertex1;
        int32_t block2;
        int32_t vertex2;
        obj1->GetBlockAndVertexNumber(block1, vertex1);
        obj2->GetBlockAndVertexNumber(block2, vertex2);

        if (block1 == block2 && vertex1 == vertex2 && obj1->GetObjPosition()->CellR == obj2->GetObjPosition()->CellR &&
            obj1->GetObjPosition()->CellC == obj2->GetObjPosition()->CellC)
        {
            CheckExtents(obj1, obj2, 0.0f);
        }

        return;
    }

    const MCVector3D position1 = obj1->GetPosition();
    const MCVector3D position2 = obj2->GetPosition();
    const float dx = position2.X - position1.X;
    const float dy = position2.Y - position1.Y;
    const float radius1 = obj1->GetExtentRadius();
    const double reach = static_cast<double>(obj2->GetExtentRadius()) + radius1;

    if (static_cast<double>(dx) * dx + static_cast<double>(dy) * dy < reach * reach)
    {
        CheckExtents(obj1, obj2, 0.0f);
    }

    CheckCollisionAlerts(obj1, obj2, 1);
}

auto MCCollisionSystem::DetectStaticCollision(MCGameObject* obj1, MCGameObject* obj2) -> void
{
    const MCVector3D position1 = obj1->GetPosition();
    const MCVector3D position2 = obj2->GetPosition();
    const float dx = position2.X - position1.X;
    const float dy = position2.Y - position1.Y;
    const float radius1 = obj1->GetExtentRadius();
    const double reach = static_cast<double>(obj2->GetExtentRadius()) + radius1;

    if (!(static_cast<double>(dx) * dx + static_cast<double>(dy) * dy < reach * reach))
    {
        return;
    }

    const MCObjectClass class1 = obj1->ObjectClass;
    const bool isMover = class1 == BATTLEMECH || class1 == GROUNDVEHICLE || class1 == ELEMENTAL || class1 == MOVER;

    if (isMover && obj2->ObjectClass != TREE && obj2->ObjectClass != MISCTERRAINOBJECT)
    {
        // A mover whose own cell is passable goes through.
        const MCObjectPosition* objPosition = obj1->GetObjPosition();
        MCMapTile& tile = GameMap->Map[GameMap->Width * objPosition->TileR + objPosition->TileC];

        if (tile.GetCellPassable(objPosition->CellR, objPosition->CellC) != 0)
        {
            return;
        }
    }

    CheckExtents(obj1, obj2, 0.0f);
}

auto MCCollisionSystem::CheckCollisionAlerts(MCGameObject* obj1, MCGameObject* obj2, int setAlert) -> float
{
    if (obj1 == nullptr || obj2 == nullptr)
    {
        return -1.0f;
    }

    const MCVector3D position1 = obj1->GetPosition();
    const MCVector3D position2 = obj2->GetPosition();
    MCVector3D delta;
    delta.X = position2.X - position1.X;
    delta.Y = position2.Y - position1.Y;
    delta.Z = position2.Z - position1.Z;
    const float distanceSq =
        static_cast<float>(static_cast<double>(delta.Y) * delta.Y + static_cast<double>(delta.X) * delta.X);
    obj1->GetExtentRadius();
    obj2->GetExtentRadius();

    // Only mechs and vehicles; two mechs only of one side.
    const MCObjectClass class1 = obj1->ObjectClass;
    const MCObjectClass class2 = obj2->ObjectClass;

    if (class1 != BATTLEMECH && class1 != GROUNDVEHICLE)
    {
        return -1.0f;
    }

    if (class2 != BATTLEMECH && class2 != GROUNDVEHICLE)
    {
        return -1.0f;
    }

    if (obj1->GetAlignment() != obj2->GetAlignment() && class1 != GROUNDVEHICLE && class2 != GROUNDVEHICLE)
    {
        return -1.0f;
    }

    if (!(distanceSq < WarningDist))
    {
        return -1.0f;
    }

    // The time of closest approach, from the relative velocity in world units.
    const MCVector3D velocity1 = obj1->GetVelocity();
    const MCVector3D velocity2 = obj2->GetVelocity();
    MCVector3D relVelocity;
    relVelocity.X = velocity2.X * WorldUnitsPerMeter - velocity1.X * WorldUnitsPerMeter;
    relVelocity.Y = velocity2.Y * WorldUnitsPerMeter - velocity1.Y * WorldUnitsPerMeter;
    relVelocity.Z = velocity2.Z * WorldUnitsPerMeter - velocity1.Z * WorldUnitsPerMeter;
    float time = 10000000.0f;
    const double relSpeedSq = static_cast<double>(relVelocity.Z) * relVelocity.Z +
                              static_cast<double>(relVelocity.Y) * relVelocity.Y +
                              static_cast<double>(relVelocity.X) * relVelocity.X;

    if (relSpeedSq != 0.0)
    {
        const double closing = static_cast<double>(delta.Z) * relVelocity.Z +
                               static_cast<double>(relVelocity.Y) * delta.Y +
                               static_cast<double>(relVelocity.X) * delta.X;
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
    static_cast<MCMover*>(obj1)->GetStopInfo(stopTime1, stopOther1);
    static_cast<MCMover*>(obj2)->GetStopInfo(stopTime2, stopOther2);

    if (!(time <= stopTime1) && !(time <= stopTime2))
    {
        return -1.0f;
    }

    MCVector3D newPosition1;
    newPosition1.X = velocity1.X * time * WorldUnitsPerMeter + position1.X;
    newPosition1.Y = velocity1.Y * time * WorldUnitsPerMeter + position1.Y;
    newPosition1.Z = velocity1.Z * time * WorldUnitsPerMeter + position1.Z;
    MCVector3D newPosition2;
    newPosition2.X = position2.X + velocity2.X * time * WorldUnitsPerMeter;
    newPosition2.Y = position2.Y + velocity2.Y * time * WorldUnitsPerMeter;
    newPosition2.Z = position2.Z + velocity2.Z * time * WorldUnitsPerMeter;
    const float gapX = newPosition2.X - newPosition1.X;
    const float gapY = newPosition2.Y - newPosition1.Y;
    const float gapSq = static_cast<float>(static_cast<double>(gapY) * gapY + static_cast<double>(gapX) * gapX);

    const float radius1 = obj1->GetExtentRadius();
    const double reach = static_cast<double>(obj2->GetExtentRadius()) + radius1;

    if (!(gapSq < reach * reach))
    {
        return -1.0f;
    }

    if (setAlert != 0)
    {
        GlobalCollisionAlert->AddRecord(obj1, obj2, gapSq, time);
    }

    return time;
}

auto MCCollisionSystem::CheckExtents(MCGameObject* obj1, MCGameObject* obj2, float) -> void
{
    MCObjectType* type1 = obj1->GetObjectType();
    MCObjectType* type2 = obj2->GetObjectType();
    const int collides1 = type1->HandleCollision(obj1, obj2);
    const int collides2 = type2->HandleCollision(obj2, obj1);

    if (collides1 != 0 && type1->HandleDestruction(obj1, obj2) != 0)
    {
        RemoveFromObjectList(obj1);
    }

    if (collides2 != 0 && type2->HandleDestruction(obj2, obj1) != 0)
    {
        RemoveFromObjectList(obj2);
    }
}

auto MCCollisionSystem::ProcessCollisions() -> void
{
    for (MCCollisionRecord* record = FirstPending; record != nullptr; record = record->Next)
    {
        MCGameObject* obj1 = record->Obj1;
        MCGameObject* obj2 = record->Obj2;

        if (obj1 == nullptr || obj2 == nullptr)
        {
            continue;
        }

        MCObjectType* type1 = obj1->GetObjectType();
        MCObjectType* type2 = obj2->GetObjectType();
        const int collides1 = type1->HandleCollision(obj1, obj2);
        const int collides2 = type2->HandleCollision(obj2, obj1);

        if (collides1 != 0 && type1->HandleDestruction(obj1, obj2) != 0)
        {
            RemoveCollisions(obj1, record);
            RemoveFromObjectList(obj1);
        }

        if (collides2 != 0 && type2->HandleDestruction(obj2, obj1) != 0)
        {
            RemoveCollisions(obj2, record);
            RemoveFromObjectList(obj2);
        }

        record->Obj1 = nullptr;
        record->Obj2 = nullptr;
    }
}

auto MCCollisionSystem::RemoveCollisions(MCGameObject* object, MCCollisionRecord* record) -> void
{
    for (MCCollisionRecord* next = record->Next; next != nullptr; next = next->Next)
    {
        if (next->Obj1 == object)
        {
            next->Obj1 = nullptr;
        }

        if (next->Obj2 == object)
        {
            next->Obj2 = nullptr;
        }
    }
}

auto MCCollisionSystem::FindNextPending() -> MCCollisionRecord*
{
    return nullptr;
}

auto MCCollisionSystem::AddPendingCollision(MCGameObject*, MCGameObject*, float) -> int32_t
{
    return 0;
}

auto MCCollisionSystem::TimeToImpact(MCGameObject* obj1, MCGameObject* obj2) -> float
{
    const MCVector3D position1 = obj1->GetPosition();
    const MCVector3D position2 = obj2->GetPosition();
    MCVector3D delta;
    delta.X = position2.X - position1.X;
    delta.Y = position2.Y - position1.Y;
    delta.Z = position2.Z - position1.Z;
    const float distanceSq =
        static_cast<float>(static_cast<double>(delta.Y) * delta.Y + static_cast<double>(delta.X) * delta.X);
    const float radius1 = obj1->GetExtentRadius();
    const double reach = static_cast<double>(obj2->GetExtentRadius()) + radius1;
    const float reachSq = static_cast<float>(reach * reach);

    if (distanceSq < reachSq)
    {
        return 0.0f;
    }

    // Where each will be at the end of the frame, and how far each moves.
    const MCVector3D velocity1 = obj1->GetVelocity();
    const MCVector3D velocity2 = obj2->GetVelocity();
    MCVector3D end1;
    end1.X = velocity1.X * FrameLength + position1.X;
    end1.Y = velocity1.Y * FrameLength + position1.Y;
    end1.Z = velocity1.Z * FrameLength + position1.Z;
    MCVector3D end2;
    end2.X = velocity2.X * FrameLength + position2.X;
    end2.Y = velocity2.Y * FrameLength + position2.Y;
    end2.Z = velocity2.Z * FrameLength + position2.Z;
    MCVector3D move1;
    move1.X = end1.X - position1.X;
    move1.Y = end1.Y - position1.Y;
    move1.Z = end1.Z - position1.Z;
    MCVector3D move2;
    move2.X = end2.X - position2.X;
    move2.Y = end2.Y - position2.Y;
    move2.Z = end2.Z - position2.Z;

    // operator|: z, y, then x.
    auto dot = [](const MCVector3D& a, const MCVector3D& b)
    { return static_cast<double>(a.Z) * b.Z + static_cast<double>(a.Y) * b.Y + static_cast<double>(a.X) * b.X; };

    if (!(dot(move1, move2) < 0.0))
    {
        return -1.0f;
    }

    MCVector3D relVelocity;
    relVelocity.X = velocity2.X - velocity1.X;
    relVelocity.Y = velocity2.Y - velocity1.Y;
    relVelocity.Z = velocity2.Z - velocity1.Z;
    const float closing = static_cast<float>(dot(delta, relVelocity));
    const float time = static_cast<float>(-(closing / dot(relVelocity, relVelocity)));

    if (!(time <= FrameLength))
    {
        return -1.0f;
    }

    const float closestX = delta.X + relVelocity.X * time;
    const float closestY = delta.Y + relVelocity.Y * time;

    if (static_cast<double>(closestY) * closestY + static_cast<double>(closestX) * closestX < reachSq)
    {
        return time;
    }

    return -1.0f;
}

auto MCCollisionSystem::Destroy() -> void
{
    if (CollisionGrid != nullptr)
    {
        CollisionGrid->Destroy();
        delete CollisionGrid;
    }

    CollisionGrid = nullptr;

    CollisionList.reset();

    if (GlobalCollisionAlert != nullptr)
    {
        GlobalCollisionAlert->Destroy();
        delete GlobalCollisionAlert;
    }

    GlobalCollisionAlert = nullptr;
}
