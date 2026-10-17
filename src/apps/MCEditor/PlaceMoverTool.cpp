#include "stdafx.h"
#include "PlaceMoverTool.h"
#include "EditorMap.h"
#include "EditorTile.h"
#include "MoverObject.h"
#include "ObjectLibrary.h"
#include "resource.h"

PlaceMoverTool::PlaceMoverTool(uint32_t id, int32_t side, int32_t variant)
    : PlaceObjectTool(id, side, false), variant(variant)
{
}

PlaceMoverTool::~PlaceMoverTool()
{
    if (highlightedTile != nullptr)
    {
        highlightedTile->highlighted = false;
    }
}

bool PlaceMoverTool::begin(EditorMap& map)
{
    active = true;
    return PlaceObjectTool::begin(map);
}

bool PlaceMoverTool::apply(EditorMap& map, int32_t x, int32_t y, [[maybe_unused]] ToolKeys keys)
{
    int32_t col = 0;
    int32_t row = 0;

    if (!map.viewToCell(x, y, &col, &row))
    {
        return false;
    }

    if (erase)
    {
        EditorObject* mover = map.moverAt(col, row);

        if (mover == nullptr)
        {
            return false;
        }

        action->recordObject(*mover);
        map.removeMover(*mover, true);
        return true;
    }

    MoverObject* mover = nullptr;

    if (objectIdClass(id) == OBJECT_CLASS_MECH)
    {
        mover = new MechObject(id, col, row, side, variant);
    }
    else
    {
        mover = new VehicleObject(id, col, row, side);
    }

    if (!map.addMover(mover))
    {
        delete mover;
        return true;
    }

    action->recordObject(*mover);
    return true;
}

bool PlaceMoverTool::canApply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys)
{
    int32_t col = 0;
    int32_t row = 0;

    if (!map.viewToCell(x, y, &col, &row))
    {
        return false;
    }

    // The original takes the tile without testing for one; the cell is on the grid.
    EditorTile* tile = map.getTile(col / MAP_CELLS_PER_TILE, row / MAP_CELLS_PER_TILE);
    map.clearHighlight();

    if (tile != nullptr)
    {
        tile->highlighted = true;
    }

    highlightedTile = tile;

    if (static_cast<int32_t>(map.movers.size()) < MAX_MOVERS)
    {
        return PlaceObjectTool::canApply(map, x, y, keys);
    }

    if (active)
    {
        CString text;
        text.Format(IDS_A_MAP_MAY_ONLY, MAX_MOVERS);
        showError(text);
    }

    return false;
}
