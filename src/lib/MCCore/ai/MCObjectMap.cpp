#include "stdafx.h"
#include "ai/MCObjectMap.h"
#include "ai/MCMoveSystem.h"
#include "lib/MCFatal.h"
#include "object/MCBigGameObject.h"
#include "object/MCObjectType.h"

MCObjectMap::MCObjectMap(const MCScenarioMap& map)
    : _Height(map.Height), _Width(map.Width), _Rows(static_cast<size_t>(map.Height))
{
}

MCObjectMap::~MCObjectMap()
{
    for (const auto& [address, position] : _Positions)
    {
        if (position->Object != nullptr)
        {
            position->Object->SetObjPosition(nullptr);
        }
    }
}

auto MCObjectMap::Link(MCObjectPosition* position) -> void
{
    // The original linked a record into rows[tileR] whatever the row; one off the map is kept but on no row.
    if (position->TileR >= 0 && position->TileR < _Height)
    {
        _Rows[static_cast<size_t>(position->TileR)].push_back(position);
    }
}

auto MCObjectMap::Unlink(MCObjectPosition* position) -> void
{
    if (position->TileR >= 0 && position->TileR < _Height)
    {
        std::erase(_Rows[static_cast<size_t>(position->TileR)], position);
    }
}

auto MCObjectMap::AddObject(MCGameObject* object) -> void
{
    auto position = std::make_unique<MCObjectPosition>();
    GameMap()->WorldToMapPos(object->GetPosition(), position->TileR, position->TileC, position->CellR, position->CellC);
    position->Object = object;
    MCObjectPosition* record = position.get();
    _Positions.emplace(record, std::move(position));
    object->SetObjPosition(record);
    Link(record);
}

auto MCObjectMap::UpdateObject(MCGameObject* object) -> bool
{
    const MCScenarioMap* map = GameMap();
    int32_t tileR = 0;
    int32_t tileC = 0;
    int32_t cellR = 0;
    int32_t cellC = 0;
    map->WorldToMapPos(object->GetPosition(), tileR, tileC, cellR, cellC);

    if (cellR < 0 || cellR > 2 || cellC < 0 || cellC > 2)
    {
        const std::string message = std::format(
            "Bad Cell - Object: {}   Positionx: {:f}   Positiony: {:f}\t CRow: {}   CCol: {}",
            object->GetObjectType()->ObjTypeNum, object->GetPosition().X, object->GetPosition().Y, cellR, cellC);
        Assert(cellR >= 0 && cellR <= 2, static_cast<uint32_t>(cellR), message);
        Assert(cellC >= 0 && cellC <= 2, static_cast<uint32_t>(cellC), message);
    }

    if (!map->OnMap(tileR, tileC))
    {
        const MCObjectClass objectClass = object->ObjectClass;
        const bool isMover = objectClass == MCObjectClass::BattleMech || objectClass == MCObjectClass::GroundVehicle ||
                             objectClass == MCObjectClass::Elemental || objectClass == MCObjectClass::Mover;
        Assert(!isMover, 0,
               std::format("Object: {}   Positionx: {:f}   Positiony: {:f}\t TRow: {}   TCol: {}",
                           object->GetObjectType()->ObjTypeNum, object->GetPosition().X, object->GetPosition().Y, tileR,
                           tileC));
        RemoveObject(object);
        return false;
    }

    Assert(tileR >= 0 && tileR < _Height, 0, " Object moved off map ");
    Assert(tileC >= 0 && tileC < _Width, 0, " Object moved off map ");

    MCObjectPosition* position = object->GetObjPosition();

    if (tileR != position->TileR || tileC != position->TileC)
    {
        Unlink(position);
        position->TileR = tileR;
        position->TileC = tileC;
        Link(position);
    }

    position->CellR = cellR;
    position->CellC = cellC;
    position->MapCellR = tileR * MapCellDim + cellR;
    position->MapCellC = tileC * MapCellDim + cellC;
    return true;
}

auto MCObjectMap::RemoveObject(MCGameObject* object) -> void
{
    MCObjectPosition* position = object->GetObjPosition();

    if (position != nullptr)
    {
        Unlink(position);
        _Positions.erase(position);
    }

    object->SetObjPosition(nullptr);
}

auto MCObjectMap::CountObjects(int32_t tileR, int32_t tileC, bool sensorBlockingOnly) const -> int32_t
{
    int32_t count = 0;

    for (const MCObjectPosition* position : _Rows[static_cast<size_t>(tileR)])
    {
        if (position->TileC == tileC && position->Object != nullptr &&
            (!sensorBlockingOnly || position->Object->ObjectClass != MCObjectClass::Tree))
        {
            count++;
        }
    }

    return count;
}

auto MCObjectMap::GetNumObjects(int32_t tileR, int32_t tileC) const -> int32_t
{
    return CountObjects(tileR, tileC, false);
}

auto MCObjectMap::GetNumSensorBlockingObjects(int32_t tileR, int32_t tileC) const -> int32_t
{
    return CountObjects(tileR, tileC, true);
}
