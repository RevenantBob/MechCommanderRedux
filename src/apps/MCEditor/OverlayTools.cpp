#include "stdafx.h"
#include "OverlayTools.h"
#include "EditorMap.h"
#include "EditorObject.h"
#include "MapOverlay.h"
#include "PlaceObjectTool.h"
#include "TileLibrary.h"
#include "X87Extended.h"

namespace
{
    /// <summary>The overlay tool's cursor and hot spot.</summary>
    inline constexpr UINT OVERLAY_CURSOR = IDB_PAINT_CURSOR;
    inline constexpr int32_t OVERLAY_HOT = 0x10;
    /// <summary>The first overlay kind name string (an overlay tool's action is named after its kind).</summary>
    inline constexpr UINT OVERLAY_NAME_STRINGS = IDS_FOREST;
    /// <summary>The longest string the tools load.</summary>
    inline constexpr int NAME_LENGTH = 0x100;
    /// <summary>The string offset of the GTiles set (<c>tileSetStringOffset</c>), whose forests blend into their neighbours.</summary>
    inline constexpr int32_t GTILES_STRING_OFFSET = 50;

    /// <summary>Whether the tile shows water on a corner (a corner at elevation 0 counts).</summary>
    bool showsWater(const EditorTile& tile)
    {
        for (int32_t i = 0; i < 4; i++)
        {
            if (tile.elevations[i] == 0 || tile.textures[i] == TEXTURE_WATER)
            {
                return true;
            }
        }

        return false;
    }

    /// <summary>The vertex at corner <paramref name="corner"/> of tile (<paramref name="col"/>, <paramref name="row"/>).</summary>
    POINT cornerVertex(int32_t col, int32_t row, int32_t corner)
    {
        return {col + (corner & 1), row + (corner >> 1)};
    }

    /// <summary>A string resource, empty when there is none.</summary>
    std::string loadName(UINT id)
    {
        char text[NAME_LENGTH];

        if (LoadStringA(GetModuleHandleA(nullptr), id, text, NAME_LENGTH) == 0)
        {
            text[0] = '\0';
        }

        return text;
    }

    /// <summary>
    /// The object on cell (<paramref name="col"/>, <paramref name="row"/>), or a mech or vehicle one cell down-right
    /// of it, as the tools test it inline.
    /// </summary>
    EditorObject* objectUnderCell(EditorMap& map, int32_t col, int32_t row)
    {
        return map.objectAt(col, row);
    }
}

EraseTool::EraseTool(UINT bitmapId, int32_t hotX, int32_t hotY, int32_t kind)
    : BrushTool(true, bitmapId, hotX, hotY), kind(kind)
{
}

bool EraseTool::begin(EditorMap& map)
{
    // Original behaviour: an action not taken by end is dropped without being deleted.
    objectsAction = new ObjectsAction("", &map, true);
    objectsAction->name += loadName(ID_ERASE);
    return true;
}

EditorAction* EraseTool::end()
{
    ObjectsAction* result = objectsAction;

    if (result->objects.empty() && result->records.empty())
    {
        delete result;
        objectsAction = nullptr;
        return nullptr;
    }

    return result;
}

bool EraseTool::apply(EditorMap& map, int32_t x, int32_t y, [[maybe_unused]] ToolKeys keys)
{
    int32_t mapX = 0;
    int32_t mapY = 0;
    int32_t mapZ = 0;
    map.viewToMap(x, y, &mapX, &mapY, &mapZ);
    const int32_t col = mapX / tileWorldWidth;
    const int32_t row = mapY / tileWorldHeight;
    EditorTile* tile = map.getTile(col, row);

    if (tile == nullptr)
    {
        return false;
    }

    const bool hadOverlay = tile->overlay != nullptr;

    if (kind != -1 && !hadOverlay)
    {
        return false;
    }

    int32_t cellCol = 0;
    int32_t cellRow = 0;
    map.viewToCell(x, y, &cellCol, &cellRow);

    if (EditorObject* object = objectUnderCell(map, cellCol, cellRow); object != nullptr)
    {
        objectsAction->recordObject(*object);

        if (!map.removeAnyObject(*object, true))
        {
            // The user kept it: the drag ends.
            if (auto* frame = static_cast<CFrameWnd*>(AfxGetMainWnd()); frame != nullptr)
            {
                if (CView* view = frame->GetActiveView(); view != nullptr)
                {
                    ::SendMessage(view->m_hWnd, WM_LBUTTONUP, 0, 0);
                }
            }
        }

        return true;
    }

    const TileAction::TileRecord before{col, row, *tile};

    if (hadOverlay)
    {
        delete tile->overlay;
        tile->overlay = nullptr;
        objectsAction->recordTile(col, row, before.tile);
    }

    return true;
}

bool EraseTool::applyAgain([[maybe_unused]] EditorMap& map, [[maybe_unused]] int32_t x, [[maybe_unused]] int32_t y,
                           [[maybe_unused]] ToolKeys keys)
{
    return true;
}

bool EraseTool::canApply(EditorMap& map, int32_t x, int32_t y, [[maybe_unused]] ToolKeys keys)
{
    int32_t col = 0;
    int32_t row = 0;
    map.viewToTile(x, y, &col, &row);

    if (const EditorTile* tile = map.getTile(col, row); tile != nullptr && tile->overlay != nullptr)
    {
        return true;
    }

    int32_t cellCol = 0;
    int32_t cellRow = 0;
    return map.viewToCell(x, y, &cellCol, &cellRow) && objectUnderCell(map, cellCol, cellRow) != nullptr;
}

bool EraseTool::hasTarget(EditorMap& map)
{
    for (int32_t col = 0; col < map.maxRow; col++)
    {
        for (int32_t row = 0; row < map.maxCol; row++)
        {
            const EditorTile* tile = map.getTile(col, row);

            if (!tile->selected)
            {
                continue;
            }

            if (tile->overlay != nullptr)
            {
                return true;
            }

            for (int32_t i = 0; i < MAP_CELLS_PER_TILE; i++)
            {
                for (int32_t j = 0; j < MAP_CELLS_PER_TILE; j++)
                {
                    if (objectUnderCell(map, col * MAP_CELLS_PER_TILE + i, row * MAP_CELLS_PER_TILE + j) != nullptr)
                    {
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

bool EraseTool::applyToSelection(EditorMap& map, [[maybe_unused]] int32_t step)
{
    for (int32_t col = 0; col < map.maxRow; col++)
    {
        for (int32_t row = 0; row < map.maxCol; row++)
        {
            EditorTile* tile = map.getTile(col, row);

            if (tile->selected && tile->overlay != nullptr)
            {
                objectsAction->recordTile(col, row, *tile);
                delete tile->overlay;
                tile->overlay = nullptr;
            }
        }
    }

    std::list<EditorObject*> selected;

    for (std::list<EditorObject*>* list : {&map.objects, &map.dropZones, &map.movers})
    {
        for (EditorObject* object : *list)
        {
            if (object->selected)
            {
                selected.push_back(object);
            }
        }
    }

    for (EditorObject* object : selected)
    {
        objectsAction->recordObject(*object);
        map.removeAnyObject(*object, true);
    }

    return true;
}

OverlayTool::OverlayTool(int32_t kind) : VertexTool(1, TEXTURE_NONE, true, OVERLAY_CURSOR), kind(kind)
{
}

bool OverlayTool::begin(EditorMap& map)
{
    const bool result = VertexTool::begin(map);

    if (heightAction != nullptr)
    {
        heightAction->name = loadName(OVERLAY_NAME_STRINGS + kind);
    }

    firstCol = -1;
    firstRow = -1;
    lastCol = -1;
    lastRow = -1;
    return result;
}

bool OverlayTool::apply(EditorMap& map, int32_t x, int32_t y, [[maybe_unused]] ToolKeys keys)
{
    int32_t col = 0;
    int32_t row = 0;

    if (!map.viewToTile(x, y, &col, &row))
    {
        return false;
    }

    if (firstCol == -1)
    {
        firstRow = row;
        firstCol = col;
    }

    const bool result = layOverlay(map, col, row);

    if (lastCol != -1 && kind != OVERLAY_DOCK && kind != OVERLAY_FOREST && kind != OVERLAY_ROUGH)
    {
        // The line from the last tile to this one, as a Bresenham walk; tiles with water are passed over.
        int32_t atCol = lastCol;
        int32_t atRow = lastRow;
        const int32_t dx = col - atCol;
        const int32_t dy = row - atRow;
        const int32_t across = std::abs(dx) * 2;
        const int32_t down = std::abs(dy) * 2;
        const int32_t stepCol = dx < 0 ? -1 : (dx > 0 ? 1 : 0);
        const int32_t stepRow = dy < 0 ? -1 : (dy > 0 ? 1 : 0);
        auto layIfDry = [&](int32_t c, int32_t r)
        {
            const EditorTile* tile = map.getTile(c, r);

            if (tile == nullptr || !showsWater(*tile))
            {
                layOverlay(map, c, r);
            }
        };

        if (across <= down)
        {
            int32_t error = across - (down >> 1);

            while (true)
            {
                layIfDry(atCol, atRow);

                if (atRow == row)
                {
                    break;
                }

                if (error >= 0)
                {
                    atCol += stepCol;
                    error -= down;
                }

                atRow += stepRow;
                error += across;
            }
        }
        else
        {
            int32_t error = down - (across >> 1);

            while (true)
            {
                layIfDry(atCol, atRow);

                if (atCol == col)
                {
                    break;
                }

                if (error >= 0)
                {
                    atRow += stepRow;
                    error -= across;
                }

                atCol += stepCol;
                error += down;
            }
        }
    }

    lastCol = col;
    lastRow = row;
    return result;
}

bool OverlayTool::applyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys)
{
    EraseTool eraser(OVERLAY_CURSOR, OVERLAY_HOT, OVERLAY_HOT, kind);
    eraser.objectsAction = new ObjectsAction("", &map, true);
    eraser.objectsAction->name += loadName(ID_ERASE);
    const bool result = eraser.apply(map, x, y, keys);
    ObjectsAction* erased = eraser.objectsAction;
    eraser.objectsAction = nullptr;

    if (!erased->objects.empty() || !erased->records.empty())
    {
        for (const TileAction::TileRecord& record : erased->records)
        {
            if (heightAction != nullptr)
            {
                heightAction->recordTile(record.col, record.row, record.tile);
            }
        }
    }

    delete erased;
    return result;
}

bool OverlayTool::canApply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys)
{
    int32_t col = 0;
    int32_t row = 0;

    if (!map.viewToTile(x, y, &col, &row))
    {
        return false;
    }

    if (const EditorTile* tile = map.getTile(col, row); tile != nullptr && showsWater(*tile))
    {
        return false;
    }

    return BrushTool::canApply(map, x, y, keys);
}

bool OverlayTool::canApplyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys)
{
    // The original asks a temporary erase tool, whose highlight goes again when it does.
    EraseTool eraser(OVERLAY_CURSOR, OVERLAY_HOT, OVERLAY_HOT, kind);
    int32_t col = 0;
    int32_t row = 0;
    map.viewToTile(x, y, &col, &row);
    const EditorTile* tile = map.getTile(col, row);
    bool target = tile != nullptr && tile->overlay != nullptr;

    if (!target)
    {
        int32_t cellCol = 0;
        int32_t cellRow = 0;
        target = map.viewToCell(x, y, &cellCol, &cellRow) && objectUnderCell(map, cellCol, cellRow) != nullptr;
    }

    return target ? eraser.canApplyAgain(map, x, y, keys) : false;
}

bool OverlayTool::hasTarget(EditorMap& map)
{
    if (map.selectedVertices.size() == static_cast<size_t>((map.maxRow + 1) * (map.maxCol + 1)))
    {
        return false;
    }

    bool result = true;

    for (int32_t col = 0; col < map.maxRow; col++)
    {
        for (int32_t row = 0; row < map.maxCol; row++)
        {
            const EditorTile* tile = map.getTile(col, row);

            if (tile->selected && showsWater(*tile))
            {
                result = false;
                break;
            }
        }
    }

    return result;
}

bool OverlayTool::applyToSelection(EditorMap& map, [[maybe_unused]] int32_t step)
{
    bool result = true;

    for (int32_t row = 0; row < map.maxCol; row++)
    {
        for (int32_t col = 0; col < map.maxRow; col++)
        {
            if (!map.getTile(col, row)->selected)
            {
                continue;
            }

            if (firstCol == -1)
            {
                firstRow = row;
                firstCol = col;
            }

            const bool laid = layOverlay(map, col, row);
            result = laid && result;
        }
    }

    return result;
}

bool OverlayTool::layOverlay(EditorMap& map, int32_t col, int32_t row)
{
    EditorTile* tile = map.getTile(col, row);

    if (col == firstCol && row == firstRow && heightAction != nullptr && !heightAction->records.empty())
    {
        return true;
    }

    // The original copies the tile unchecked.
    if (tile == nullptr)
    {
        return false;
    }

    const TileAction::TileRecord before{col, row, *tile};
    // Original behaviour (EB-079): a dock leaves these unset; the port starts them empty.
    int32_t kinds[4] = {OVERLAY_NONE, OVERLAY_NONE, OVERLAY_NONE, OVERLAY_NONE};

    if (kind != OVERLAY_DOCK && kind != OVERLAY_FOREST && kind != OVERLAY_ROUGH)
    {
        // Next to the use's first tile: that one gets a straight piece if nothing around it joins it yet.
        const bool besideFirst = row == firstRow && (col - 1 == firstCol || col + 1 == firstCol);
        const bool belowFirst = row != firstRow && col == firstCol && (row == firstRow + 1 || row == firstRow - 1);

        if (besideFirst || belowFirst)
        {
            EditorTile* first = map.getTile(firstCol, firstRow);
            int32_t around[4];

            if (first != nullptr && neighbourKinds(map, firstCol, firstRow, around) &&
                std::all_of(std::begin(around), std::end(around), [](int32_t k) { return k == NO_JOIN; }))
            {
                int32_t straight[4];

                if (besideFirst)
                {
                    straight[0] = OVERLAY_NONE;
                    straight[1] = kind;
                    straight[2] = OVERLAY_NONE;
                    straight[3] = kind;
                }
                else
                {
                    straight[0] = kind;
                    straight[1] = OVERLAY_NONE;
                    straight[2] = kind;
                    straight[3] = OVERLAY_NONE;
                }

                if (first->hasOverlayFor(straight, false))
                {
                    if (heightAction != nullptr)
                    {
                        heightAction->recordTile(firstCol, firstRow, *first);
                    }

                    first->setOverlay(straight, false);
                }
            }
        }

        if (!neighbourKinds(map, col, row, kinds))
        {
            return false;
        }

        // Corners more than 1 up come down 1.
        uint8_t heights[4];
        tile->getHeights(heights);

        for (int32_t i = 0; i < 4; i++)
        {
            if (heights[i] > 1)
            {
                const POINT vertex = cornerVertex(col, row, i);

                if (heightAction != nullptr)
                {
                    heightAction->recordVertex(vertex.x, vertex.y);
                }

                moveVertex(map, vertex.x, vertex.y, -1);
            }
        }

        // A paved road meeting a dirt road keeps the other's corners.
        if (tile->overlay != nullptr)
        {
            for (int32_t i = 0; i < 4; i++)
            {
                const int32_t other = tile->overlay->kinds[i];

                if (other != OVERLAY_NONE && ((kind == OVERLAY_PAVED_ROAD && other == OVERLAY_DIRT_ROAD) ||
                                              (other == OVERLAY_PAVED_ROAD && kind == OVERLAY_DIRT_ROAD)))
                {
                    kinds[i] = other;
                }
            }
        }

        if (std::all_of(std::begin(kinds), std::end(kinds), [](int32_t k) { return k == NO_JOIN; }))
        {
            kinds[0] = OVERLAY_NONE;
            kinds[1] = OVERLAY_NONE;
            kinds[2] = kind;
            kinds[3] = OVERLAY_NONE;
        }
        else
        {
            // Each side that joins: the neighbour's facing corner takes the kind too.
            struct Side
            {
                int32_t col;
                int32_t row;
                int32_t corner;
            };

            const Side sides[4] = {{col, row - 1, 2}, {col + 1, row, 3}, {col, row + 1, 0}, {col - 1, row, 1}};

            for (int32_t i = 0; i < 4; i++)
            {
                if (kinds[i] == NO_JOIN || kinds[i] == OVERLAY_NONE)
                {
                    kinds[i] = OVERLAY_NONE;
                    continue;
                }

                EditorTile* neighbour = map.getTile(sides[i].col, sides[i].row);

                if (neighbour == nullptr)
                {
                    continue;
                }

                const EditorTile neighbourBefore = *neighbour;

                if (neighbour->canSetOverlayCorner(sides[i].corner, kinds[i]))
                {
                    neighbour->setOverlayCorner(sides[i].corner, kinds[i]);

                    if (heightAction != nullptr)
                    {
                        heightAction->recordTile(sides[i].col, sides[i].row, neighbourBefore);
                    }
                }
            }
        }
    }

    uint8_t heights[4];
    tile->getHeights(heights);

    if (tileLibrary->findOverlayTile(kinds, heights, false) == nullptr)
    {
        // A piece straight across.
        for (int32_t i = 0; i < 4; i++)
        {
            if (kinds[i] != OVERLAY_NONE)
            {
                kinds[(i + 2) & 3] = kinds[i];
                break;
            }
        }

        if (tileLibrary->findOverlayTile(kinds, heights, false) == nullptr)
        {
            flattenTile(map, col, row);
            std::fill(std::begin(heights), std::end(heights), static_cast<uint8_t>(0));

            if (tileLibrary->findOverlayTile(kinds, heights, false) == nullptr)
            {
                return kind == OVERLAY_RUNWAY;
            }
        }
    }

    if (heightAction != nullptr)
    {
        heightAction->recordTile(col, row, before.tile);
    }

    return tile->setOverlay(kinds, false);
}

bool OverlayTool::neighbourKinds(EditorMap& map, int32_t col, int32_t row, int32_t* kinds)
{
    std::fill(kinds, kinds + 4, NO_JOIN);
    return sideKind(map.getTile(col, row - 1), &kinds[0], 2) && sideKind(map.getTile(col + 1, row), &kinds[1], 3) &&
           sideKind(map.getTile(col, row + 1), &kinds[2], 0) && sideKind(map.getTile(col - 1, row), &kinds[3], 1);
}

bool OverlayTool::sideKind(const EditorTile* tile, int32_t* side, int32_t corner)
{
    *side = NO_JOIN;

    if (tile == nullptr || tile->overlay == nullptr)
    {
        return true;
    }

    const int32_t* kinds = tile->overlay->kinds;
    const int32_t facing = kinds[corner];

    if (facing == OVERLAY_NONE)
    {
        for (int32_t i = 0; i < 4; i++)
        {
            if (kinds[i] == kind && tile->canSetOverlayCorner(corner, kind))
            {
                *side = kind;
            }
        }

        return true;
    }

    if (facing == kind || (facing == OVERLAY_BRIDGE && kind == OVERLAY_PAVED_ROAD) ||
        (facing == OVERLAY_RAIL_BRIDGE && kind == OVERLAY_RAILROAD))
    {
        *side = kind;
        return true;
    }

    *side = NO_JOIN;
    return true;
}

BridgeTool::BridgeTool(int32_t kind, bool turned) : OverlayTool(kind), turned(turned)
{
}

bool BridgeTool::apply(EditorMap& map, int32_t x, int32_t y, [[maybe_unused]] ToolKeys keys)
{
    int32_t col = 0;
    int32_t row = 0;

    if (!map.viewToTile(x, y, &col, &row))
    {
        return false;
    }

    if (firstCol == -1)
    {
        firstRow = row;
        firstCol = col;
    }

    return layBridge(map, col, row);
}

bool BridgeTool::applyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys)
{
    return OverlayTool::applyAgain(map, x, y, keys);
}

bool BridgeTool::canApply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys)
{
    return BrushTool::canApply(map, x, y, keys);
}

bool BridgeTool::canApplyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys)
{
    return BrushTool::canApplyAgain(map, x, y, keys);
}

bool BridgeTool::hasTarget([[maybe_unused]] EditorMap& map)
{
    return true;
}

bool BridgeTool::applyToSelection(EditorMap& map, [[maybe_unused]] int32_t step)
{
    bool result = true;

    for (int32_t col = 0; col < map.maxRow; col++)
    {
        for (int32_t row = 0; row < map.maxCol; row++)
        {
            if (map.getTile(col, row)->selected)
            {
                const bool laid = layBridge(map, col, row);
                result = laid && result;
            }
        }
    }

    return result;
}

bool BridgeTool::layBridge(EditorMap& map, int32_t col, int32_t row)
{
    EditorTile* tile = map.getTile(col, row);

    if (col == firstCol && row == firstRow && heightAction != nullptr && !heightAction->records.empty())
    {
        return true;
    }

    // The original copies the tile unchecked.
    if (tile == nullptr)
    {
        return false;
    }

    const TileAction::TileRecord before{col, row, *tile};
    int32_t sides[4];
    uint8_t ends[4];

    if (!bridgeEnds(map, col, row, sides, ends))
    {
        return false;
    }

    auto move = [&](const POINT& vertex, int32_t delta)
    {
        if (heightAction != nullptr)
        {
            heightAction->recordVertex(vertex.x, vertex.y);
        }

        moveVertex(map, vertex.x, vertex.y, delta);
    };

    // Each corner toward the height of the end it lies at.
    for (int32_t corner = 0; corner < 4; corner++)
    {
        uint8_t heights[4];
        tile->getHeights(heights);

        if (heights[corner] == 2)
        {
            if (ends[corner] == NO_JOIN)
            {
                move(cornerVertex(col, row, corner), -1);
            }
            else
            {
                for (int32_t i = 0; i < 4; i++)
                {
                    if (heights[i] == 0)
                    {
                        move(cornerVertex(col, row, i), 1);
                    }
                }
            }
        }
        else if (heights[corner] != ends[corner] && ends[corner] != NO_JOIN)
        {
            const int32_t level = ends[corner] != 1 ? 1 : -1;

            for (int32_t i = 0; i < 4; i++)
            {
                if (i != corner && ends[i] == NO_JOIN)
                {
                    move(cornerVertex(col, row, i), level - heights[i]);
                }
            }
        }
    }

    uint8_t heights[4];
    tile->getHeights(heights);

    // A saddle: the free corners go to 1.
    if (heights[0] == heights[3] && heights[1] == heights[2] && heights[0] != heights[1])
    {
        for (int32_t i = 0; i < 4; i++)
        {
            if (ends[i] == NO_JOIN)
            {
                move(cornerVertex(col, row, i), heights[i] != 1 ? 1 : -1);
            }
        }
    }

    if (std::any_of(std::begin(sides), std::end(sides), [](int32_t side) { return side != NO_JOIN; }))
    {
        const int32_t road = kind == OVERLAY_BRIDGE ? OVERLAY_PAVED_ROAD : OVERLAY_RAILROAD;

        struct Side
        {
            int32_t col;
            int32_t row;
            int32_t corner;
        };

        const Side around[4] = {{col, row - 1, 2}, {col + 1, row, 3}, {col, row + 1, 0}, {col - 1, row, 1}};

        for (int32_t i = 0; i < 4; i++)
        {
            if (sides[i] != road)
            {
                continue;
            }

            EditorTile* neighbour = map.getTile(around[i].col, around[i].row);

            if (neighbour == nullptr)
            {
                continue;
            }

            const EditorTile neighbourBefore = *neighbour;

            if (neighbour->canSetOverlayCorner(around[i].corner, road))
            {
                neighbour->setOverlayCorner(around[i].corner, road);

                if (heightAction != nullptr)
                {
                    heightAction->recordTile(around[i].col, around[i].row, neighbourBefore);
                }
            }
        }
    }

    tile->getHeights(heights);
    int32_t kinds[4];

    if (!turned)
    {
        kinds[0] = kind;
        kinds[1] = OVERLAY_NONE;
        kinds[2] = kind;
        kinds[3] = OVERLAY_NONE;
    }
    else
    {
        kinds[0] = OVERLAY_NONE;
        kinds[1] = kind;
        kinds[2] = OVERLAY_NONE;
        kinds[3] = kind;
    }

    if (tileLibrary->findOverlayTile(kinds, heights, false) == nullptr)
    {
        flattenTile(map, col, row);
        std::fill(std::begin(heights), std::end(heights), static_cast<uint8_t>(0));

        if (tileLibrary->findOverlayTile(kinds, heights, false) == nullptr)
        {
            return false;
        }
    }

    if (heightAction != nullptr)
    {
        heightAction->recordTile(col, row, before.tile);
    }

    return tile->setOverlay(kinds, false);
}

bool BridgeTool::bridgeEnds(EditorMap& map, int32_t col, int32_t row, int32_t* sides, uint8_t* heights)
{
    std::fill(sides, sides + 4, NO_JOIN);
    std::fill(heights, heights + 4, static_cast<uint8_t>(NO_JOIN));

    if (!turned)
    {
        const EditorTile* above = map.getTile(col, row - 1);
        const EditorTile* below = map.getTile(col, row + 1);
        bridgeSide(above, 2, &sides[0], &heights[0]);
        bridgeSide(below, 0, &sides[2], &heights[2]);

        if (heights[0] != NO_JOIN && above != nullptr)
        {
            heights[1] = heights[0];
        }

        if (heights[2] != NO_JOIN && below != nullptr)
        {
            heights[3] = heights[2];
        }
    }
    else
    {
        const EditorTile* left = map.getTile(col - 1, row);
        const EditorTile* right = map.getTile(col + 1, row);
        bridgeSide(left, 1, &sides[3], &heights[0]);
        bridgeSide(right, 3, &sides[1], &heights[1]);

        if (heights[0] != NO_JOIN && left != nullptr)
        {
            heights[2] = heights[0];
        }

        if (heights[1] != NO_JOIN && right != nullptr)
        {
            heights[3] = heights[1];
        }
    }

    return true;
}

bool BridgeTool::bridgeSide(const EditorTile* tile, int32_t corner, int32_t* side, uint8_t* height)
{
    *height = static_cast<uint8_t>(NO_JOIN);
    *side = NO_JOIN;
    const int32_t road = kind == OVERLAY_RAIL_BRIDGE ? OVERLAY_RAILROAD : OVERLAY_PAVED_ROAD;

    if (tile == nullptr || tile->overlay == nullptr)
    {
        return true;
    }

    const int32_t* kinds = tile->overlay->kinds;

    if (kinds[corner] != OVERLAY_NONE && kinds[corner] == kind)
    {
        uint8_t heights[4];
        tile->getHeights(heights);
        *height = heights[corner];
    }

    for (int32_t i = 0; i < 4; i++)
    {
        if (kinds[i] == road)
        {
            *height = 1;
            *side = road;
        }
    }

    return true;
}

ForestTool::ForestTool() : BrushTool(true, OVERLAY_CURSOR, OVERLAY_HOT, OVERLAY_HOT)
{
}

bool ForestTool::begin(EditorMap& map)
{
    return startAction(map, OVERLAY_NAME_STRINGS + OVERLAY_FOREST);
}

bool ForestTool::apply(EditorMap& map, int32_t x, int32_t y, [[maybe_unused]] ToolKeys keys)
{
    int32_t col = 0;
    int32_t row = 0;

    if (!map.viewToTile(x, y, &col, &row))
    {
        return false;
    }

    return plant(map, col, row, true);
}

bool ForestTool::applyAgain(EditorMap& map, int32_t x, int32_t y, [[maybe_unused]] ToolKeys keys)
{
    int32_t mapX = 0;
    int32_t mapY = 0;
    int32_t mapZ = 0;
    map.viewToMap(x, y, &mapX, &mapY, &mapZ);
    const int32_t col = mapX / tileWorldWidth;
    const int32_t row = mapY / tileWorldHeight;
    EditorTile* tile = map.getTile(col, row);

    if (tile == nullptr || tile->overlay == nullptr)
    {
        return false;
    }

    uint8_t heights[4];
    tile->getHeights(heights);
    // Which ninth of the tile: the map position over a third of the tile width, in x87 extended precision.
    using X = X87Extended;
    const X cell = X(tileWorldWidth) / X(MAP_CELLS_PER_TILE);
    const auto across = static_cast<int32_t>((X(mapX) / cell).truncate()) % MAP_CELLS_PER_TILE;
    const auto down = static_cast<int32_t>((X(mapY) / cell).truncate()) % MAP_CELLS_PER_TILE;
    const TileAction::TileRecord before{col, row, *tile};

    // Clears corner corner of the forest; false when it isn't forest or what is left has no tile.
    auto clearCorner = [&](MapOverlay& overlay, int32_t corner)
    {
        // Original behaviour (EB-081): the middle row's first corner is -1, which the editor reads from outside its
        // array (no forest there in practice).
        if (corner < 0)
        {
            return false;
        }

        int32_t kinds[4];
        std::copy(std::begin(overlay.kinds), std::end(overlay.kinds), kinds);

        if (kinds[corner] != OVERLAY_FOREST)
        {
            return false;
        }

        kinds[corner] = OVERLAY_NONE;

        if (tileLibrary->findOverlayTile(kinds, heights, overlay.damaged) == nullptr)
        {
            return false;
        }

        return overlay.set(kinds, heights, overlay.damaged);
    };

    auto dropOverlay = [tile]()
    {
        delete tile->overlay;
        tile->overlay = nullptr;
    };

    if (across == 1 && down == 1)
    {
        dropOverlay();
    }
    else
    {
        int32_t corner = down == 0 ? 0 : (down == 2 ? 2 : -1);

        if (!clearCorner(*tile->overlay, corner))
        {
            dropOverlay();
        }

        if (across == 0)
        {
            corner = 3;
        }

        if (down == 1)
        {
            corner = 1;
        }

        // The original goes on with the overlay it may have just freed; on the map that changes nothing.
        if (tile->overlay != nullptr && !clearCorner(*tile->overlay, corner))
        {
            dropOverlay();
        }
    }

    if (action != nullptr)
    {
        action->recordTile(col, row, before.tile);
    }

    return true;
}

bool ForestTool::canApply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys)
{
    int32_t col = 0;
    int32_t row = 0;

    if (!map.viewToTile(x, y, &col, &row) || !canPlant(map, col, row))
    {
        return false;
    }

    return BrushTool::canApply(map, x, y, keys);
}

bool ForestTool::canApplyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys)
{
    return BrushTool::canApplyAgain(map, x, y, keys);
}

bool ForestTool::hasTarget(EditorMap& map)
{
    for (int32_t col = 0; col < map.maxRow; col++)
    {
        for (int32_t row = 0; row < map.maxCol; row++)
        {
            if (map.getTile(col, row)->selected && !canPlant(map, col, row))
            {
                return false;
            }
        }
    }

    return true;
}

bool ForestTool::applyToSelection(EditorMap& map, [[maybe_unused]] int32_t step)
{
    bool result = true;

    for (int32_t col = 0; col < map.maxRow; col++)
    {
        for (int32_t row = 0; row < map.maxCol; row++)
        {
            if (map.getTile(col, row)->selected)
            {
                const bool planted = plant(map, col, row, true);
                result = planted && result;
            }
        }
    }

    return result;
}

bool ForestTool::plant(EditorMap& map, int32_t col, int32_t row, bool spread)
{
    EditorTile* tile = map.getTile(col, row);

    // The original copies the tile unchecked.
    if (tile == nullptr)
    {
        return false;
    }

    const int32_t forest[4] = {OVERLAY_FOREST, OVERLAY_FOREST, OVERLAY_FOREST, OVERLAY_FOREST};
    const TileAction::TileRecord before{col, row, *tile};

    if (!tile->setOverlay(forest, false))
    {
        return false;
    }

    if (action != nullptr)
    {
        action->recordTile(col, row, before.tile);
    }

    if (tileSetStringOffset != GTILES_STRING_OFFSET)
    {
        return true;
    }

    // Each neighbour's facing corner, and the corner of its own overlay that tells whether it is forest there.
    struct Side
    {
        int32_t col;
        int32_t row;
        int32_t corner;
        int32_t forestCorner;
    };

    const Side sides[4] = {{col, row - 1, 2, 0}, {col + 1, row, 3, 1}, {col, row + 1, 0, 2}, {col - 1, row, 1, 3}};

    for (const Side& side : sides)
    {
        EditorTile* neighbour = map.getTile(side.col, side.row);

        if (neighbour == nullptr)
        {
            continue;
        }

        const EditorTile neighbourBefore = *neighbour;

        if (neighbour->canSetOverlayCorner(side.corner, OVERLAY_FOREST))
        {
            neighbour->setOverlayCorner(side.corner, OVERLAY_FOREST);

            if (action != nullptr)
            {
                action->recordTile(side.col, side.row, neighbourBefore);
            }

            continue;
        }

        const int32_t there =
            neighbour->overlay != nullptr ? neighbour->overlay->kinds[side.forestCorner] : OVERLAY_NONE;

        if (there == OVERLAY_FOREST)
        {
            neighbour->setOverlay(forest, false);

            if (action != nullptr)
            {
                action->recordTile(side.col, side.row, neighbourBefore);
            }

            if (spread)
            {
                plant(map, side.col, side.row, false);
            }
        }
    }

    return true;
}

bool ForestTool::canPlant(EditorMap& map, int32_t col, int32_t row)
{
    const EditorTile* tile = map.getTile(col, row);

    if (tile == nullptr || tile->hasTexture(TEXTURE_WATER) || tile->concIndex != -1 || tile->cropIndex != -1)
    {
        return false;
    }

    if (tile->overlay != nullptr)
    {
        for (const int32_t kind : tile->overlay->kinds)
        {
            if (kind != OVERLAY_NONE && kind != OVERLAY_FOREST)
            {
                return false;
            }
        }
    }

    for (const std::list<EditorObject*>* list : {&map.objects, &map.dropZones})
    {
        for (const EditorObject* object : *list)
        {
            if (object->cellCol / MAP_CELLS_PER_TILE == col && object->cellRow / MAP_CELLS_PER_TILE == row)
            {
                return false;
            }
        }
    }

    return true;
}

RoughTool::RoughTool() : BrushTool(true, OVERLAY_CURSOR, OVERLAY_HOT, OVERLAY_HOT)
{
}

bool RoughTool::begin(EditorMap& map)
{
    return startAction(map, OVERLAY_NAME_STRINGS + OVERLAY_ROUGH);
}

bool RoughTool::apply(EditorMap& map, int32_t x, int32_t y, [[maybe_unused]] ToolKeys keys)
{
    int32_t col = 0;
    int32_t row = 0;

    if (!map.viewToTile(x, y, &col, &row))
    {
        return false;
    }

    return putRough(map, col, row);
}

bool RoughTool::applyAgain(EditorMap& map, int32_t x, int32_t y, [[maybe_unused]] ToolKeys keys)
{
    int32_t col = 0;
    int32_t row = 0;

    if (!map.viewToTile(x, y, &col, &row))
    {
        return false;
    }

    EditorTile* tile = map.getTile(col, row);

    if (tile == nullptr || tile->overlay == nullptr || tile->overlay->kinds[0] != OVERLAY_ROUGH)
    {
        return false;
    }

    delete tile->overlay;
    tile->overlay = nullptr;
    return true;
}

bool RoughTool::canApply([[maybe_unused]] EditorMap& map, [[maybe_unused]] int32_t x, [[maybe_unused]] int32_t y,
                         [[maybe_unused]] ToolKeys keys)
{
    return true;
}

bool RoughTool::hasTarget([[maybe_unused]] EditorMap& map)
{
    return true;
}

bool RoughTool::applyToSelection(EditorMap& map, [[maybe_unused]] int32_t step)
{
    bool result = true;

    for (int32_t col = 0; col < map.maxRow; col++)
    {
        for (int32_t row = 0; row < map.maxCol; row++)
        {
            if (map.getTile(col, row)->selected)
            {
                const bool put = putRough(map, col, row);
                result = put && result;
            }
        }
    }

    return result;
}

bool RoughTool::putRough(EditorMap& map, int32_t col, int32_t row)
{
    EditorTile* tile = map.getTile(col, row);

    if (tile == nullptr)
    {
        return false;
    }

    if (action != nullptr)
    {
        action->recordTile(col, row, *tile);
    }

    const int32_t rough[4] = {OVERLAY_ROUGH, OVERLAY_ROUGH, OVERLAY_ROUGH, OVERLAY_ROUGH};
    return tile->setOverlay(rough, false);
}

OverlayLayoutTool::OverlayLayoutTool(int32_t kind, int32_t layout) : OverlayTool(kind), layout(layout)
{
    for (int32_t corner = 0; corner < 4; corner++)
    {
        layoutKinds[corner] = ((layout >> corner) & 1) != 0 ? kind : OVERLAY_NONE;
    }
}

bool OverlayLayoutTool::apply(EditorMap& map, int32_t x, int32_t y, [[maybe_unused]] ToolKeys keys)
{
    int32_t col = 0;
    int32_t row = 0;

    if (!map.viewToTile(x, y, &col, &row))
    {
        return false;
    }

    return layLayout(map, col, row);
}

bool OverlayLayoutTool::canApply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys)
{
    return BrushTool::canApply(map, x, y, keys);
}

bool OverlayLayoutTool::hasTarget([[maybe_unused]] EditorMap& map)
{
    return true;
}

bool OverlayLayoutTool::applyToSelection(EditorMap& map, [[maybe_unused]] int32_t step)
{
    bool result = true;

    for (int32_t col = 0; col < map.maxRow; col++)
    {
        for (int32_t row = 0; row < map.maxCol; row++)
        {
            if (map.getTile(col, row)->selected)
            {
                const bool laid = layLayout(map, col, row);
                result = laid && result;
            }
        }
    }

    return result;
}

bool OverlayLayoutTool::layLayout(EditorMap& map, int32_t col, int32_t row)
{
    EditorTile* tile = map.getTile(col, row);

    if (tile == nullptr)
    {
        return false;
    }

    uint8_t heights[4];
    tile->getHeights(heights);

    for (uint8_t& height : heights)
    {
        if (height == 2)
        {
            height = 1;
        }
    }

    if (tileLibrary->findOverlayTile(layoutKinds, heights, false) == nullptr)
    {
        // Original behaviour: a corner kind is the tool's kind or none, so this test (made twice, the second time to
        // copy the other way) never holds, and the overlay is tried flat.
        if (layoutKinds[0] == 1 && layoutKinds[2] == 1 && layoutKinds[1] == 0 && layoutKinds[3] == 0)
        {
            heights[1] = heights[0];
            heights[3] = heights[2];
        }
        else
        {
            std::fill(std::begin(heights), std::end(heights), static_cast<uint8_t>(0));
        }

        if (tileLibrary->findOverlayTile(layoutKinds, heights, false) == nullptr)
        {
            std::fill(std::begin(heights), std::end(heights), static_cast<uint8_t>(0));
            tileLibrary->findOverlayTile(layoutKinds, heights, false);
        }
    }

    action->recordTile(col, row, *tile);

    if (tile->overlay == nullptr)
    {
        tile->overlay = new MapOverlay();
    }

    tile->overlay->set(layoutKinds, heights, false);
    return true;
}
