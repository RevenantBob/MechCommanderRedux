#include "stdafx.h"
#include "HillTool.h"
#include "EditorAction.h"
#include "EditorMap.h"
#include "EditorTile.h"
#include "PlaceObjectTool.h"
#include "TileLibrary.h"

namespace
{
    /// <summary>The longest action name the tool loads (a 256-byte buffer).</summary>
    inline constexpr int ACTION_NAME_SIZE = 0x100;
}

HillTool::HillTool(bool flatten)
    : VertexTool(1, TEXTURE_GRASS, true, flatten ? IDB_FLATTEN_CURSOR : IDB_MAKE_HILLY_CURSOR), flatten(flatten)
{
}

bool HillTool::begin(EditorMap& map)
{
    const bool result = VertexTool::begin(map);
    char name[ACTION_NAME_SIZE];

    if (LoadStringA(GetModuleHandleA(nullptr), flatten ? IDS_FLATTEN : IDS_HILLS, name, ACTION_NAME_SIZE) == 0)
    {
        name[0] = '\0';
    }

    heightAction->name = name;
    return result;
}

bool HillTool::apply(EditorMap& map, int32_t x, int32_t y, [[maybe_unused]] ToolKeys keys)
{
    return flatten ? flattenAt(map, x, y) : makeHillyAt(map, x, y);
}

bool HillTool::applyAgain(EditorMap& map, int32_t x, int32_t y, [[maybe_unused]] ToolKeys keys)
{
    return flatten ? makeHillyAt(map, x, y) : flattenAt(map, x, y);
}

bool HillTool::flattenAt(EditorMap& map, int32_t x, int32_t y)
{
    int32_t col = 0;
    int32_t row = 0;

    if (!map.viewToTile(x, y, &col, &row))
    {
        return false;
    }

    const EditorTile* tile = map.getTile(col, row);
    const int32_t lowest = *std::min_element(std::begin(tile->elevations), std::end(tile->elevations));
    selectTileVertices(map, col, row);
    const bool result = setSelectedElevation(map, lowest);
    map.selectedVertices.clear();
    return result;
}

bool HillTool::makeHillyAt(EditorMap& map, int32_t x, int32_t y)
{
    int32_t col = 0;
    int32_t row = 0;

    if (!map.viewToTile(x, y, &col, &row))
    {
        return false;
    }

    selectTileVertices(map, col, row);
    const bool result = makeHilly(map);
    map.selectedVertices.clear();
    return result;
}

void HillTool::selectTileVertices(EditorMap& map, int32_t col, int32_t row)
{
    map.setVertexSelected(col, row, true);
    map.setVertexSelected(col + 1, row, true);
    map.setVertexSelected(col, row + 1, true);
    map.setVertexSelected(col + 1, row + 1, true);
}
