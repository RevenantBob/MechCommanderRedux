#include "stdafx.h"
#include "SelectTool.h"
#include "EditorMap.h"
#include "EditorObject.h"
#include "PlaceObjectTool.h"
#include "X87Extended.h"

namespace
{
    /// <summary>The select cursor's hot spot.</summary>
    inline constexpr int32_t CURSOR_HOT_X = 10;
    inline constexpr int32_t CURSOR_HOT_Y = 4;
    /// <summary>The view directions of the map's projection (0x005025b8, 0x005025c0).</summary>
    inline constexpr double VIEW_ANGLE_ACROSS = -0.7853981633973;
    inline constexpr double VIEW_ANGLE_DOWN = -0.9599310885967;
    /// <summary>The outline's colour (0x0052736c, white).</summary>
    inline constexpr COLORREF OUTLINE_COLOR = RGB(0xff, 0xff, 0xff);
    /// <summary>The factor of the sides' slope product (0x004fad20).</summary>
    inline constexpr double SLOPE_FACTOR = -2.0;
    /// <summary>MK_SHIFT or MK_CONTROL: add to the selection rather than replace it.</summary>
    inline constexpr ToolKeys ADD_KEYS = MK_SHIFT | MK_CONTROL;

    /// <summary>Selects or unselects the tile's four vertices (<see cref="EditorMap::setVertexSelected"/>).</summary>
    void setTileVertices(EditorMap& map, int32_t col, int32_t row, bool keep)
    {
        map.setVertexSelected(col, row, keep);
        map.setVertexSelected(col + 1, row, keep);
        map.setVertexSelected(col, row + 1, keep);
        map.setVertexSelected(col + 1, row + 1, keep);
    }

    /// <summary>Whether <paramref name="object"/>'s cell lies on a selected tile.</summary>
    bool onSelectedTile(EditorMap& map, const EditorObject& object)
    {
        const EditorTile* tile = map.getTile(object.cellCol / MAP_CELLS_PER_TILE, object.cellRow / MAP_CELLS_PER_TILE);
        return tile != nullptr && tile->selected;
    }
}

bool SelectionMarker::draw(HDC dc, [[maybe_unused]] int32_t width, [[maybe_unused]] int32_t height, int32_t x,
                           int32_t y, [[maybe_unused]] int32_t zoom)
{
    if (this->height == 0 || this->width == 0)
    {
        return true;
    }

    // The tile's sides in the view, as unit vectors, as the editor works them out (in x87 extended precision).
    using X = X87Extended;
    const int32_t across = static_cast<int32_t>((X(std::cos(VIEW_ANGLE_ACROSS)) * X(tileWorldWidth)).truncate());
    const int32_t down = static_cast<int32_t>((X(std::cos(VIEW_ANGLE_DOWN)) * X(across)).truncate());
    const double length = std::sqrt(static_cast<double>(across * across + down * down));
    const double unitX = (X(across) / X(length)).toDouble();
    const X unitY = X(down) / X(length);
    const double slope = (unitY * X(unitX) * X(SLOPE_FACTOR)).toDouble();
    // How far along the first side the parallelogram's corner lies.
    const X along = (X(0) - X(this->height) * X(unitX) - X(this->width) * X(unitY.toDouble())) / X(slope);
    const double alongX = (along * X(unitX)).toDouble();
    const X alongY = along * X(unitY.toDouble());
    POINT points[5];
    points[0] = {x, y};
    points[1] = {static_cast<LONG>((X(x) + X(alongX)).truncate()), static_cast<LONG>((X(y) + alongY).truncate())};
    points[2] = {x + this->width, y + this->height};
    points[3] = {static_cast<LONG>((X(x + this->width) - X(alongX)).truncate()),
                 static_cast<LONG>((X(y + this->height) - alongY).truncate())};
    points[4] = {x, y};
    const HPEN pen = CreatePen(PS_DASH, 1, OUTLINE_COLOR);
    const HGDIOBJ oldPen = SelectObject(dc, pen);
    Polyline(dc, points, static_cast<int>(std::size(points)));
    // Not translated: the grid (never drawn, see grid).
    SelectObject(dc, oldPen);
    DeleteObject(pen);
    return true;
}

SelectTool::SelectTool(bool area)
    : CursorTool(IDB_SELECT_CURSOR, CURSOR_HOT_X, CURSOR_HOT_Y)
    , anchorX(MARK_ANYWHERE)
    , anchorY(MARK_ANYWHERE)
    , area(area)
{
}

SelectTool::~SelectTool()
{
    if (map != nullptr)
    {
        map->selectedVertices.clear();
        map->deselectAll();
    }

    if (marker != nullptr)
    {
        map->removeMark(marker, MARK_ANYWHERE, MARK_ANYWHERE);
        delete marker;
        marker = nullptr;
    }
}

bool SelectTool::begin(EditorMap& map)
{
    this->map = &map;

    if (marker != nullptr)
    {
        map.removeMark(marker, MARK_ANYWHERE, MARK_ANYWHERE);
        delete marker;
        marker = nullptr;
        anchorY = MARK_ANYWHERE;
        anchorX = MARK_ANYWHERE;
    }

    pressedObject = nullptr;
    return true;
}

EditorAction* SelectTool::end()
{
    EditorAction* result = nullptr;

    if (moveTool != nullptr)
    {
        result = moveTool->end();
        delete moveTool;
        moveTool = nullptr;
        pressedObject = nullptr;
    }

    anchorY = MARK_ANYWHERE;
    anchorX = MARK_ANYWHERE;
    return result;
}

bool SelectTool::apply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys)
{
    this->map = &map;

    if (moveTool != nullptr)
    {
        return moveTool->apply(map, x, y, keys);
    }

    if (marker == nullptr)
    {
        marker = new SelectionMarker();
        map.addMark(marker, x, y);
    }

    const bool adding = (keys & ADD_KEYS) != 0;

    if (!adding)
    {
        map.deselectAll();
    }

    if (anchorX == MARK_ANYWHERE || !area)
    {
        anchorX = x;
        anchorY = y;

        if (!area)
        {
            // Original behaviour: the pressed object is only set with the move tool, which the start delegates to,
            // so the move tool is never applied here.
            int32_t col = 0;
            int32_t row = 0;

            if (pressedObject == nullptr && map.viewToCell(x, y, &col, &row))
            {
                if (EditorObject* object = map.objectAt(col, row); object != nullptr)
                {
                    if (!adding)
                    {
                        map.deselectAll();
                        object->selected = true;
                        moveTool = new MoveObjectsTool();
                        moveTool->begin(map);

                        if (moveTool->canApply(map, x, y, keys))
                        {
                            moveTool->apply(map, x, y, keys);
                        }

                        pressedObject = object;
                        return true;
                    }

                    object->selected = !object->selected;
                    return true;
                }
            }
        }

        int32_t col = 0;
        int32_t row = 0;

        if (map.viewToTile(x, y, &col, &row))
        {
            if (EditorTile* tile = map.getTile(col, row); tile != nullptr)
            {
                if (!adding || !tile->selected)
                {
                    tile->selected = true;
                    map.tilesSelected = true;
                    setTileVertices(map, col, row, true);
                }
                else
                {
                    tile->selected = false;
                    setTileVertices(map, col, row, false);
                }
            }
        }
    }

    if (!area)
    {
        return true;
    }

    // The tiles between the press and the cursor on the map.
    int32_t anchorMapX = 0;
    int32_t anchorMapY = 0;
    int32_t anchorMapZ = 0;
    map.viewToMap(anchorX, anchorY, &anchorMapX, &anchorMapY, &anchorMapZ);
    int32_t mapX = 0;
    int32_t mapY = 0;
    int32_t mapZ = 0;
    map.viewToMap(x, y, &mapX, &mapY, &mapZ);
    // The editor rounds the integer quotients down and up, which leaves them as they are.
    const int32_t firstCol = (std::min)(anchorMapX, mapX) / tileWorldWidth;
    const int32_t lastCol = (std::max)(anchorMapX, mapX) / tileWorldWidth;
    const int32_t firstRow = (std::min)(anchorMapY, mapY) / tileWorldHeight;
    const int32_t lastRow = (std::max)(anchorMapY, mapY) / tileWorldHeight;
    const int32_t vertexFirstRow = firstRow >= 0 ? firstRow + 1 : 0;
    const int32_t tileFirstRow = firstRow > 0 ? firstRow : 0;
    const int32_t vertexLastRow = lastRow < map.maxCol + 1 ? lastRow : map.maxCol + 1;
    const int32_t tileLastRow = lastRow >= map.maxCol ? map.maxCol - 1 : lastRow;
    const int32_t vertexFirstCol = firstCol >= 0 ? firstCol + 1 : 0;
    const int32_t tileFirstCol = firstCol > 0 ? firstCol : 0;
    const int32_t vertexLastCol = lastCol < map.maxRow + 1 ? lastCol : map.maxRow + 1;
    const int32_t tileLastCol = lastCol >= map.maxRow ? map.maxRow - 1 : lastCol;

    for (int32_t row = tileFirstRow; row <= tileLastRow; row++)
    {
        for (int32_t col = tileFirstCol; col <= tileLastCol; col++)
        {
            map.getTile(col, row)->selected = true;
            map.tilesSelected = true;
        }
    }

    for (int32_t row = vertexFirstRow; row <= vertexLastRow; row++)
    {
        for (int32_t col = vertexFirstCol; col <= vertexLastCol; col++)
        {
            map.setVertexSelected(col, row, true);
        }
    }

    marker->width = x - anchorX;
    marker->height = y - anchorY;

    for (const std::list<EditorObject*>* list : {&map.objects, &map.dropZones, &map.movers})
    {
        for (EditorObject* object : *list)
        {
            object->selected = onSelectedTile(map, *object);
        }
    }

    return true;
}

bool SelectTool::applyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys)
{
    return apply(map, x, y, keys);
}

bool SelectTool::canApply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys)
{
    if (moveTool != nullptr)
    {
        return moveTool->canApply(map, x, y, keys);
    }

    return true;
}

void SelectTool::selectAll()
{
    for (int32_t col = 0; col < map->maxRow; col++)
    {
        for (int32_t row = 0; row < map->maxCol; row++)
        {
            map->getTile(col, row)->selected = true;
        }
    }

    map->tilesSelected = true;
    map->selectedVertices.clear();

    for (int32_t row = 0; row < map->maxCol + 1; row++)
    {
        for (int32_t col = 0; col < map->maxRow + 1; col++)
        {
            map->selectedVertices.push_back({col, row});
        }
    }

    for (const std::list<EditorObject*>* list : {&map->objects, &map->dropZones})
    {
        for (EditorObject* object : *list)
        {
            object->selected = true;
        }
    }
}
