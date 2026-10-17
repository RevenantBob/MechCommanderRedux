#include "stdafx.h"
#include "DamageTool.h"
#include "EditorMap.h"
#include "EditorTile.h"
#include "MapOverlay.h"
#include "ObjectLibrary.h"
#include "TileLibrary.h"
#include "resource.h"

namespace
{
    /// <summary>The tools' cursor hot spot.</summary>
    inline constexpr int32_t DAMAGE_HOT_X = 3;
    inline constexpr int32_t DAMAGE_HOT_Y = 0x13;
    /// <summary>The longest action name the tool loads (a 256-byte buffer).</summary>
    inline constexpr int ACTION_NAME_SIZE = 0x100;
    /// <summary>The id bit of the destroyed look.</summary>
    inline constexpr uint32_t DESTROYED_BIT = 1;

    /// <summary>Whether <paramref name="object"/> has the destroyed look.</summary>
    bool isDestroyed(const EditorObject& object)
    {
        return (object.id & DESTROYED_BIT) != 0;
    }
}

DamageAction::DamageAction(const std::string& name, EditorMap* map) : TileAction(name, map)
{
}

bool DamageAction::redo()
{
    for (const std::list<EditorObject*>* list : {&map->objects, &map->dropZones})
    {
        for (EditorObject* object : *list)
        {
            for (StaticObject& record : objects)
            {
                if (object->cellCol == record.cellCol && object->cellRow == record.cellRow)
                {
                    const bool destroyed = isDestroyed(record);
                    object->setDestroyed(destroyed);
                    record.setDestroyed(!destroyed);
                }
            }
        }
    }

    return TileAction::redo();
}

bool DamageAction::undo()
{
    return redo();
}

void DamageAction::recordObject(const EditorObject& object)
{
    for (const StaticObject& record : objects)
    {
        if (record.isSameAs(object))
        {
            return;
        }
    }

    objects.emplace_back(object);
}

DamageTool::DamageTool(bool damage)
    : BrushTool(false, damage ? IDB_DAMAGE_CURSOR : IDB_REPAIR_CURSOR, DAMAGE_HOT_X, DAMAGE_HOT_Y), damage(damage)
{
}

DamageAction* DamageTool::damageAction() const
{
    return static_cast<DamageAction*>(action);
}

std::list<EditorObject*> DamageTool::placedObjects(const EditorMap& map)
{
    std::list<EditorObject*> placed = map.objects;
    placed.insert(placed.end(), map.dropZones.begin(), map.dropZones.end());
    return placed;
}

bool DamageTool::begin(EditorMap& map)
{
    char name[ACTION_NAME_SIZE];

    if (LoadStringA(GetModuleHandleA(nullptr), damage ? ID_DAMAGE : ID_REPAIR, name, ACTION_NAME_SIZE) == 0)
    {
        name[0] = '\0';
    }

    // Original behaviour: an action not taken by end is dropped without being deleted.
    action = new DamageAction(name, &map);
    return true;
}

EditorAction* DamageTool::end()
{
    DamageAction* result = damageAction();
    action = nullptr;

    if (result->objects.empty() && result->records.empty())
    {
        delete result;
        return nullptr;
    }

    return result;
}

void DamageTool::damageObject(EditorObject& object)
{
    damageAction()->recordObject(object);
    object.setDestroyed(damage);
}

bool DamageTool::apply(EditorMap& map, int32_t x, int32_t y, [[maybe_unused]] ToolKeys keys)
{
    int32_t col = 0;
    int32_t row = 0;

    if (map.viewToCell(x, y, &col, &row))
    {
        for (EditorObject* object : placedObjects(map))
        {
            if (object->coversCell(col, row))
            {
                damageObject(*object);
            }
        }
    }

    if (!map.viewToTile(x, y, &col, &row))
    {
        return true;
    }

    return damageTile(map, col, row);
}

bool DamageTool::applyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys)
{
    damage = !damage;
    const bool result = apply(map, x, y, keys);
    damage = !damage;
    return result;
}

bool DamageTool::canApply(EditorMap& map, int32_t x, int32_t y, [[maybe_unused]] ToolKeys keys)
{
    bool result = false;
    int32_t col = 0;
    int32_t row = 0;

    if (map.viewToCell(x, y, &col, &row))
    {
        for (const EditorObject* object : placedObjects(map))
        {
            if (object->coversCell(col, row) && isDestroyed(*object) != damage &&
                objectLibrary->isDestroyable(object->id))
            {
                result = true;
            }
        }
    }

    if (!map.viewToTile(x, y, &col, &row))
    {
        return result;
    }

    const MapOverlay* overlay = map.getTile(col, row)->overlay;

    if (overlay == nullptr)
    {
        return result;
    }

    if (overlay->damaged)
    {
        return true;
    }

    // The original asks with flat heights.
    const uint8_t heights[4] = {};
    return tileLibrary->findOverlayTile(overlay->kinds, heights, true) != nullptr || result;
}

bool DamageTool::canApplyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys)
{
    damage = !damage;
    const bool result = canApply(map, x, y, keys);
    damage = !damage;
    return result;
}

bool DamageTool::hasTarget(EditorMap& map)
{
    if (!map.tilesSelected)
    {
        return true;
    }

    const uint8_t flatHeights[4] = {};

    for (int32_t col = 0; col < map.maxRow; col++)
    {
        for (int32_t row = 0; row < map.maxCol; row++)
        {
            const EditorTile* tile = map.getTile(col, row);

            if (!tile->selected)
            {
                continue;
            }

            if (const MapOverlay* overlay = tile->overlay; overlay != nullptr)
            {
                if (damage &&
                    (overlay->damaged || tileLibrary->findOverlayTile(overlay->kinds, flatHeights, true) != nullptr))
                {
                    return true;
                }

                if (!damage && overlay->damaged)
                {
                    return true;
                }
            }

            for (int32_t i = 0; i < MAP_CELLS_PER_TILE; i++)
            {
                for (int32_t j = 0; j < MAP_CELLS_PER_TILE; j++)
                {
                    const EditorObject* object =
                        map.objectAt(col * MAP_CELLS_PER_TILE + i, row * MAP_CELLS_PER_TILE + j);

                    if (object == nullptr)
                    {
                        continue;
                    }

                    if (damage ? objectLibrary->isDestroyable(object->id) : isDestroyed(*object))
                    {
                        return true;
                    }
                }
            }
        }
    }

    for (const EditorObject* object : placedObjects(map))
    {
        if (object == nullptr || !object->selected)
        {
            continue;
        }

        if (damage ? objectLibrary->isDestroyable(object->id) : isDestroyed(*object))
        {
            return true;
        }
    }

    return false;
}

bool DamageTool::applyToSelection(EditorMap& map, [[maybe_unused]] int32_t step)
{
    for (int32_t col = 0; col < map.maxRow; col++)
    {
        for (int32_t row = 0; row < map.maxCol; row++)
        {
            if (map.getTile(col, row)->selected)
            {
                damageTile(map, col, row);
            }
        }
    }

    for (EditorObject* object : placedObjects(map))
    {
        if (object->selected)
        {
            damageObject(*object);
        }
    }

    return true;
}

bool DamageTool::damageTile(EditorMap& map, int32_t col, int32_t row)
{
    EditorTile* tile = map.getTile(col, row);
    MapOverlay* overlay = tile->overlay;

    if (overlay == nullptr)
    {
        return false;
    }

    if (overlay->damaged == damage)
    {
        return true;
    }

    uint8_t heights[4];
    tile->getHeights(heights);
    int32_t kinds[4];
    std::copy(std::begin(overlay->kinds), std::end(overlay->kinds), kinds);

    if (tileLibrary->findOverlayTile(kinds, heights, damage) == nullptr)
    {
        return false;
    }

    action->recordTile(col, row, *tile);
    return overlay->set(kinds, heights, damage);
}
