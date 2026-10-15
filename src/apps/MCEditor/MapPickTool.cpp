#include "stdafx.h"
#include "MapPickTool.h"
#include "EditorMap.h"
#include "EditorObject.h"
#include "EditorPalette.h"
#include "EditorTile.h"
#include "EditorUtil.h"
#include "LinkTool.h"
#include "ObjectLibrary.h"
#include "TileLibrary.h"
#include "X87Extended.h"
#include "resource.h"

namespace
{
    /// <summary>The tool's cursor hot spot.</summary>
    inline constexpr int32_t PICK_HOT_X = 0x10;
    inline constexpr int32_t PICK_HOT_Y = 0x10;
    /// <summary>The low word of the mode: the kind.</summary>
    inline constexpr int32_t KIND_MASK = 0xffff;
    inline constexpr int32_t FILTER_SHIFT = 0x10;
    /// <summary>Where the flag's hot spot is in the cursor image.</summary>
    inline constexpr int32_t IMAGE_HOT_X = 0xa0;
    inline constexpr int32_t IMAGE_HOT_Y = 0x78;
    inline constexpr uint8_t TRANSPARENT_PIXEL = 0xff;
    /// <summary>The colours of the path's lines between points and of the line to the cursor.</summary>
    inline constexpr COLORREF PATH_LINE_COLOR = 0xff;
    inline constexpr COLORREF CURSOR_LINE_COLOR = 0;
    /// <summary>The command the active window gets when the pick is done (ID_PAINT: back to the select tool).</summary>
    inline constexpr WPARAM DONE_COMMAND = ID_PAINT;
    /// <summary>The sides an object can be on.</summary>
    inline constexpr int32_t ENEMY_SIDE = 1;
    inline constexpr int32_t PLAYER_SIDE = 2;
    /// <summary>No object id.</summary>
    inline constexpr uint32_t NO_ID = 0xffffffff;

    /// <summary>The object of <paramref name="list"/> whose footprint covers cell (<paramref name="col"/>, <paramref name="row"/>), or null.</summary>
    EditorObject* coveringObject(const std::list<EditorObject*>& list, int32_t col, int32_t row)
    {
        for (EditorObject* object : list)
        {
            if (object->coversCell(col, row))
            {
                return object;
            }
        }

        return nullptr;
    }
}

MapPickTool::MapPickTool(int32_t mode, CWnd* window)
    : CursorTool(IDB_PICK_CURSOR, PICK_HOT_X, PICK_HOT_Y), mode(mode), window(window)
{
}

int32_t MapPickTool::kind() const
{
    return mode & KIND_MASK;
}

bool MapPickTool::begin([[maybe_unused]] EditorMap& map)
{
    picked = false;
    return true;
}

EditorAction* MapPickTool::end()
{
    if (!picked)
    {
        return nullptr;
    }

    if (kind() == PICK_PATH)
    {
        if (!path.empty())
        {
            std::vector<POINT> points(path.begin(), path.end());
            ::SendMessageA(window->m_hWnd, WM_EDITOR_PICKED, points.size(), reinterpret_cast<LPARAM>(points.data()));
        }
    }
    else
    {
        POINT point = {x, y};
        ::SendMessageA(window->m_hWnd, WM_EDITOR_PICKED, pickedId, reinterpret_cast<LPARAM>(&point));
    }

    // The original asks the thread for its main window and drops the answer.
    if (CWnd* active = CWnd::FromHandle(::GetActiveWindow()); active != nullptr)
    {
        ::PostMessageA(active->m_hWnd, WM_COMMAND, DONE_COMMAND, 0);
    }

    return nullptr;
}

POINT MapPickTool::cellCentreView(EditorMap& map, int32_t col, int32_t row)
{
    using X = X87Extended;
    const float halfCell = (X(tileWorldWidth) / X(MAP_CELLS_PER_TILE * 2)).toFloat();
    const float cells = static_cast<float>(MAP_CELLS_PER_TILE);
    const auto cornerX = static_cast<int32_t>((X(tileWorldWidth) / X(cells) * X(col)).truncate());
    const auto centreX = static_cast<int32_t>((X(cornerX) + X(halfCell)).truncate());
    // Original behaviour: the half cell is the width's in both directions.
    const auto cornerY = static_cast<int32_t>((X(tileWorldHeight) / X(cells) * X(row)).truncate());
    const auto centreY = static_cast<int32_t>((X(cornerY) + X(halfCell)).truncate());
    int32_t viewX = 0;
    int32_t viewY = 0;
    map.mapToView(centreX, centreY, map.terrainHeight(centreX, centreY), &viewX, &viewY);
    return {viewX, viewY};
}

void MapPickTool::placeEndMarker(EditorMap& map, int32_t markX, int32_t markY)
{
    endMarker = new LinkMarker();
    endMarker->color = CURSOR_LINE_COLOR;
    map.addMark(endMarker, markX, markY);
}

bool MapPickTool::apply(EditorMap& map, int32_t viewX, int32_t viewY, [[maybe_unused]] ToolKeys keys)
{
    int32_t col = 0;
    int32_t row = 0;

    if (!map.viewToCell(viewX, viewY, &col, &row))
    {
        return false;
    }

    switch (kind())
    {
        case PICK_LOCATION:
        {
            map.cellToWorld(col, row, &x, &y);
            pickedId = 0;
            picked = true;
            return true;
        }

        case PICK_STRUCTURE:
        {
            EditorObject* object = nullptr;

            if (map.objectAtCell(col, row) != nullptr)
            {
                object = coveringObject(map.objects, col, row);

                if (object == nullptr)
                {
                    object = coveringObject(map.dropZones, col, row);
                }
            }

            if (object != nullptr)
            {
                x = col;
            }

            y = row;
            // The original reads the id of no object as well; canApply lets the click through only onto one.
            pickedId = object != nullptr ? object->id : NO_ID;
            picked = true;
            return true;
        }

        case PICK_UNIT:
        {
            EditorObject* mover = map.moverAt(col, row);

            if (mover != nullptr)
            {
                x = col;
            }

            y = row;
            pickedId = mover != nullptr ? mover->id : NO_ID;
            picked = true;
            return true;
        }

        case PICK_PATH:
        {
            int32_t worldX = 0;
            int32_t worldY = 0;
            map.cellToWorld(col, row, &worldX, &worldY);
            const POINT point = {worldX, worldY};
            const POINT view = cellCentreView(map, col, row);

            if (!path.empty())
            {
                // A line from the new point back to the last; the cursor's line starts again from the new point.
                if (endMarker != nullptr)
                {
                    map.removeMark(endMarker, MARK_ANYWHERE, MARK_ANYWHERE);
                    delete endMarker;
                    endMarker = nullptr;
                }

                auto* line = new LinkMarker();
                line->dx = x - view.x;
                line->dy = y - view.y;
                line->color = PATH_LINE_COLOR;
                map.addMark(line, view.x, view.y);
            }

            if (endMarker == nullptr)
            {
                placeEndMarker(map, view.x, view.y);
            }

            path.push_back(point);
            x = view.x;
            y = view.y;

            if (path.size() > MAX_PATH_POINTS)
            {
                finish(map);
            }

            return true;
        }

        default:
        {
            return true;
        }
    }
}

bool MapPickTool::applyAgain([[maybe_unused]] EditorMap& map, [[maybe_unused]] int32_t viewX,
                             [[maybe_unused]] int32_t viewY, [[maybe_unused]] ToolKeys keys)
{
    return true;
}

bool MapPickTool::canApply(EditorMap& map, int32_t viewX, int32_t viewY, ToolKeys keys)
{
    track(map, viewX, viewY, keys);
    int32_t col = 0;
    int32_t row = 0;

    if (!map.viewToCell(viewX, viewY, &col, &row))
    {
        return false;
    }

    switch (kind())
    {
        case PICK_LOCATION:
        {
            return true;
        }

        case PICK_STRUCTURE:
        {
            if (map.objectAtCell(col, row) == nullptr)
            {
                return false;
            }

            EditorObject* object = coveringObject(map.objects, col, row);

            if (object == nullptr)
            {
                object = coveringObject(map.dropZones, col, row);
            }

            return object != nullptr && passesFilter(object);
        }

        case PICK_UNIT:
        {
            const EditorObject* mover = map.moverAt(col, row);
            return mover != nullptr && passesFilter(mover);
        }

        case PICK_PATH:
        {
            if (map.objectAt(col, row) != nullptr)
            {
                return false;
            }

            const EditorTile* tile = map.getTile(col / MAP_CELLS_PER_TILE, row / MAP_CELLS_PER_TILE);

            // The original inlines the water test (a concrete or crop tile is neither) and reads the tile without
            // testing for one; the cell is on the grid.
            return tile != nullptr && !tile->hasTexture(TEXTURE_WATER) && !tile->hasTexture(TEXTURE_CLIFF);
        }

        default:
        {
            return false;
        }
    }
}

bool MapPickTool::passesFilter(const EditorObject* object) const
{
    if (object == nullptr)
    {
        return false;
    }

    const int32_t filter = mode >> FILTER_SHIFT;
    bool passes = true;

    if ((filter & PICK_CAPTURABLE) != 0 && !objectLibrary->isCapturable(object->id))
    {
        passes = false;
    }

    if ((filter & PICK_DESTROYABLE) != 0 && !objectLibrary->isDestroyable(object->id) &&
        !isMoverClass(objectIdClass(object->id)))
    {
        passes = false;
    }

    if ((filter & PICK_PLAYERS) != 0 && object->side != PLAYER_SIDE)
    {
        passes = false;
    }

    if ((filter & PICK_ENEMYS) != 0 && object->side != ENEMY_SIDE)
    {
        passes = false;
    }

    if ((filter & PICK_NOT_PLAYERS) != 0 && object->side == PLAYER_SIDE)
    {
        passes = false;
    }

    if ((filter & PICK_NOT_ENEMYS) != 0 && object->side == ENEMY_SIDE)
    {
        return false;
    }

    return passes;
}

const uint8_t* MapPickTool::cursorImage(EditorMap& map, const POINT& position, POINT* topLeft, SIZE* size, int32_t zoom)
{
    if (kind() == PICK_LOCATION)
    {
        *topLeft = position;
        size->cx = IMAGE_WIDTH;
        size->cy = IMAGE_HEIGHT;
        const uint32_t flag = flagObjectId();

        // The original looks up the slot of no id too.
        if (ShapeSlot* slot = flag != NO_ID ? objectLibrary->getShapeSlot(flag) : nullptr; slot != nullptr)
        {
            std::memset(pickImage, TRANSPARENT_PIXEL, sizeof(pickImage));
            uint8_t** shapes[] = {&slot->shape, &slot->halfShape, &slot->icon};

            if (*shapes[zoom] == nullptr)
            {
                if (zoom == 0)
                {
                    slot->shape = objectLibrary->getShape(slot->id);
                }
                else if (zoom == 1)
                {
                    slot->halfShape = objectLibrary->getHalfShape(slot->id);
                }
                else if (zoom == 2)
                {
                    objectLibrary->makeIcon(slot->id);
                }
            }

            drawShape(pickImage, IMAGE_WIDTH, IMAGE_HEIGHT, *shapes[zoom], 0, IMAGE_HOT_X, IMAGE_HOT_Y);

            if (slot->delta && zoom != 2)
            {
                drawShape(pickImage, IMAGE_WIDTH, IMAGE_HEIGHT, *shapes[zoom], 1, IMAGE_HOT_X, IMAGE_HOT_Y);
            }

            for (uint8_t& pixel : pickImage)
            {
                if (pixel != TRANSPARENT_PIXEL)
                {
                    pixel = editorPalette->greenTint[pixel];
                }
            }

            return pickImage;
        }
    }

    return CursorTool::cursorImage(map, position, topLeft, size, zoom);
}

void MapPickTool::track([[maybe_unused]] EditorMap& map, int32_t viewX, int32_t viewY, [[maybe_unused]] ToolKeys keys)
{
    if (endMarker != nullptr)
    {
        endMarker->dx = viewX - x;
        endMarker->dy = viewY - y;
    }
}

void MapPickTool::finish(EditorMap& map)
{
    map.clearMarks();
    // The marks' markers are deleted with them; the original keeps the pointer (and moves the freed marker after).
    endMarker = nullptr;
    picked = true;
    end();
}

void MapPickTool::removeLastPoint(EditorMap& map)
{
    if (kind() != PICK_PATH)
    {
        return;
    }

    if (path.empty())
    {
        finish(map);
        return;
    }

    // Every mark at the last point goes; EB-092: its line marker isn't deleted.
    map.removeMark(nullptr, x, y);
    path.pop_back();

    if (!path.empty())
    {
        int32_t col = 0;
        int32_t row = 0;
        map.worldToCell(path.back().x, path.back().y, &col, &row);
        const POINT view = cellCentreView(map, col, row);
        x = view.x;
        y = view.y;
    }

    delete endMarker;
    placeEndMarker(map, x, y);
}
