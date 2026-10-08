#include "stdafx.h"
#include "object/MCCollisionSystem.h"
#include "ai/MCMoveSystem.h"
#include "lib/MCFitIniFile.h"
#include "main/MCGameContext.h"
#include "object/MCGameObject.h"
#include "object/MCObjectSystem.h"
#include "object/MCObjectType.h"

// The original compares sums of squares on the x87 stack at extended precision; the port does those in double.

auto MCCollisionSystem::Create(MCFitIniFile& scenarioFile)
    -> std::expected<std::unique_ptr<MCCollisionSystem>, std::string>
{
    if (scenarioFile.SeekBlock("CollisionSystem") != 0)
    {
        return std::unexpected("no CollisionSystem block");
    }

    const MCFitResult<uint32_t> cellsAcross = scenarioFile.Read<uint32_t>("XGridSize");

    if (!cellsAcross)
    {
        return std::unexpected("no XGridSize in the CollisionSystem block");
    }

    const MCFitResult<uint32_t> gridRadius = scenarioFile.Read<uint32_t>("GridRadius");

    if (!gridRadius)
    {
        return std::unexpected("no GridRadius in the CollisionSystem block");
    }

    return std::make_unique<MCCollisionSystem>(*cellsAcross, *gridRadius);
}

auto MCCollisionSystem::CheckObjects() -> void
{
    Grid.Clear();
    const std::vector<std::unique_ptr<MCObjectList>>& lists = ObjectList()->Lists();

    // The first three lists (the loose objects and both sides' mechs) go into the grid.
    for (size_t listIndex = 0; listIndex < lists.size() && listIndex <= 2; listIndex++)
    {
        for (MCBaseObject* object : *lists[listIndex])
        {
            // Port fix: the original went back to the list's first object after one without a type, which never
            // ends. Every object of these lists has a type.
            if (object->GetObjectType() == nullptr)
            {
                continue;
            }

            Grid.Add(static_cast<MCGameObject*>(object));
            object->HandleStaticCollision();
        }
    }

    Grid.CheckPairs([this](MCGameObject* obj1, MCGameObject* obj2) { DetectCollision(obj1, obj2); });
}

auto MCCollisionSystem::DetectCollision(MCGameObject* obj1, MCGameObject* obj2) -> void
{
    if (obj1->ObjectClass < MCObjectClass::Explosion && obj2->ObjectClass < MCObjectClass::Explosion)
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
            CheckExtents(obj1, obj2);
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
        CheckExtents(obj1, obj2);
    }
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

    if (IsMoverClass(obj1->ObjectClass) && obj2->ObjectClass != MCObjectClass::Tree &&
        obj2->ObjectClass != MCObjectClass::MiscTerrainObject)
    {
        // A mover whose own cell is passable goes through.
        const MCObjectPosition* objPosition = obj1->GetObjPosition();
        MCMapTile& tile = GameMap()->Map[GameMap()->Width * objPosition->TileR + objPosition->TileC];

        if (tile.GetCellPassable(objPosition->CellR, objPosition->CellC) != 0)
        {
            return;
        }
    }

    CheckExtents(obj1, obj2);
}

auto MCCollisionSystem::CheckExtents(MCGameObject* obj1, MCGameObject* obj2) -> void
{
    MCObjectType* type1 = obj1->GetObjectType();
    MCObjectType* type2 = obj2->GetObjectType();
    const int collides1 = type1->HandleCollision(obj1, obj2);
    const int collides2 = type2->HandleCollision(obj2, obj1);

    if (collides1 != 0 && type1->HandleDestruction(obj1, obj2) != 0)
    {
        ObjectList()->Remove(obj1);
    }

    if (collides2 != 0 && type2->HandleDestruction(obj2, obj1) != 0)
    {
        ObjectList()->Remove(obj2);
    }
}

auto CollisionSystem() -> MCCollisionSystem*
{
    return MCGameContext::Current().CollisionSystem();
}
