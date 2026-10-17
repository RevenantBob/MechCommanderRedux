#include "stdafx.h"
#include "SprayTreesTool.h"
#include "EditorMap.h"
#include "EditorTile.h"
#include "ObjectLibrary.h"
#include "StaticObject.h"
#include "resource.h"

namespace
{
    /// <summary>The spray cursor's hot spot.</summary>
    inline constexpr int32_t SPRAY_HOT_X = 0x10;
    inline constexpr int32_t SPRAY_HOT_Y = 2;
    /// <summary>The longest action name the tool loads (a 256-byte buffer).</summary>
    inline constexpr int ACTION_NAME_SIZE = 0x100;
    /// <summary>How far from the cell under the cursor the spray reaches, and the core around it.</summary>
    inline constexpr int32_t SPRAY_REACH = 3;
    inline constexpr int32_t SPRAY_CORE = 1;
    /// <summary>The chances in 10 of a tree in the core, outside it, and on a selected tile's cell.</summary>
    inline constexpr int32_t CORE_CHANCE = 6;
    inline constexpr int32_t OUTER_CHANCE = 4;
    inline constexpr int32_t SELECTION_CHANCE = 5;
    inline constexpr int32_t CHANCE_SCALE = 10;

    /// <summary>The first spray-can object, the one the place tool holds.</summary>
    uint32_t firstTree()
    {
        const std::list<uint32_t>& trees = objectLibrary->sprayCanIds;
        return trees.empty() ? 0xffffffff : trees.front();
    }
}

SprayTreesTool::SprayTreesTool()
    : PlaceObjectTool(firstTree(), 0, false), cursor(IDB_SPRAY_TREES_CURSOR, SPRAY_HOT_X, SPRAY_HOT_Y)
{
}

bool SprayTreesTool::begin(EditorMap& map)
{
    char name[ACTION_NAME_SIZE];

    if (LoadStringA(GetModuleHandleA(nullptr), ID_SPRAY_TREES, name, ACTION_NAME_SIZE) == 0)
    {
        name[0] = '\0';
    }

    const bool result = PlaceObjectTool::begin(map);
    action->name = name;
    return result;
}

void SprayTreesTool::plantTree(EditorMap& map, uint32_t draw, int32_t col, int32_t row)
{
    const std::list<uint32_t>& trees = objectLibrary->sprayCanIds;
    auto tree = trees.begin();
    std::advance(tree, static_cast<ptrdiff_t>(draw % static_cast<uint32_t>(trees.size())));
    auto* object = new StaticObject(*tree, col, row, 0);

    if (!canPlaceAt(map, col, row))
    {
        delete object;
        return;
    }

    if (!map.addObject(object))
    {
        // EB-088: the selection's spray leaves a tree the map refused allocated.
        delete object;
        return;
    }

    action->recordObject(*object);
}

bool SprayTreesTool::apply(EditorMap& map, int32_t x, int32_t y, [[maybe_unused]] ToolKeys keys)
{
    int32_t col = 0;
    int32_t row = 0;

    if (!map.viewToCell(x, y, &col, &row))
    {
        return false;
    }

    for (int32_t dx = -SPRAY_REACH; dx <= SPRAY_REACH; dx++)
    {
        for (int32_t dy = -SPRAY_REACH; dy <= SPRAY_REACH; dy++)
        {
            const int32_t chance = std::abs(dx) > SPRAY_CORE || std::abs(dy) > SPRAY_CORE ? OUTER_CHANCE : CORE_CHANCE;
            const auto draw = static_cast<uint32_t>(std::rand());

            if (static_cast<int32_t>(draw) % CHANCE_SCALE < chance)
            {
                plantTree(map, draw, col + dx, row + dy);
            }
        }
    }

    return true;
}

bool SprayTreesTool::applyAgain([[maybe_unused]] EditorMap& map, [[maybe_unused]] int32_t x, [[maybe_unused]] int32_t y,
                                [[maybe_unused]] ToolKeys keys)
{
    return false;
}

bool SprayTreesTool::canApplyAgain([[maybe_unused]] EditorMap& map, [[maybe_unused]] int32_t x,
                                   [[maybe_unused]] int32_t y, [[maybe_unused]] ToolKeys keys)
{
    return true;
}

bool SprayTreesTool::hasTarget([[maybe_unused]] EditorMap& map)
{
    return true;
}

const uint8_t* SprayTreesTool::cursorImage(EditorMap& map, const POINT& position, POINT* topLeft, SIZE* size,
                                           int32_t zoom)
{
    return cursor.cursorImage(map, position, topLeft, size, zoom);
}

const uint8_t* SprayTreesTool::notAllowedImage(EditorMap& map, const POINT& position, POINT* topLeft, SIZE* size,
                                               int32_t zoom)
{
    return cursor.notAllowedImage(map, position, topLeft, size, zoom);
}

bool SprayTreesTool::spraySelection(EditorMap& map)
{
    for (int32_t col = 0; col < map.maxRow; col++)
    {
        for (int32_t row = 0; row < map.maxCol; row++)
        {
            const EditorTile* tile = map.getTile(col, row);

            if (tile == nullptr || !tile->selected)
            {
                continue;
            }

            for (int32_t i = 0; i < MAP_CELLS_PER_TILE; i++)
            {
                for (int32_t j = 0; j < MAP_CELLS_PER_TILE; j++)
                {
                    const auto draw = static_cast<uint32_t>(std::rand());

                    if (static_cast<int32_t>(draw) % CHANCE_SCALE < SELECTION_CHANCE)
                    {
                        plantTree(map, draw, MAP_CELLS_PER_TILE * col + i, MAP_CELLS_PER_TILE * row + j);
                    }
                }
            }
        }
    }

    return true;
}
