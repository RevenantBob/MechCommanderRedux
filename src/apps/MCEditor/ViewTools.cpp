#include "stdafx.h"
#include "ViewTools.h"
#include "EditorMap.h"
#include "EditorTile.h"
#include "resource.h"

namespace
{
    /// <summary>The Drag View cursor's hot spot.</summary>
    inline constexpr int32_t DRAG_HOT_X = 0x10;
    inline constexpr int32_t DRAG_HOT_Y = 0x10;
    /// <summary>The zoom cursors' hot spot.</summary>
    inline constexpr int32_t ZOOM_HOT_X = 0x14;
    inline constexpr int32_t ZOOM_HOT_Y = 0xc;
    /// <summary>The zooms above which the full-size, the middle and the smallest tiles are used (0x004faf38-28).</summary>
    inline constexpr double FULL_ZOOM = 0.99;
    inline constexpr double MIDDLE_ZOOM = 0.49;
    inline constexpr double SMALL_ZOOM = 0.24;
    /// <summary>The smallest zoom the Zoom Out tool goes below (0x004fc160).</summary>
    inline constexpr double SMALLEST_ZOOM = 0.26;
    /// <summary>The tile widths and elevation scales of the three tile sizes.</summary>
    inline constexpr int32_t FULL_TILE = 0x9e;
    inline constexpr int32_t MIDDLE_TILE = 0x4f;
    inline constexpr int32_t SMALL_TILE = 0x27;
    inline constexpr int32_t FULL_ELEVATION_SCALE = 0x1c;
    inline constexpr int32_t MIDDLE_ELEVATION_SCALE = 0xe;
    inline constexpr int32_t SMALL_ELEVATION_SCALE = 7;

    /// <summary>The main window.</summary>
    HWND mainWindow()
    {
        CWnd* window = AfxGetMainWnd();
        return window != nullptr ? window->m_hWnd : nullptr;
    }
}

DragViewTool::DragViewTool()
    : CursorTool(IDB_DRAG_VIEW_CURSOR, DRAG_HOT_X, DRAG_HOT_Y), last{MARK_ANYWHERE, MARK_ANYWHERE}
{
}

bool DragViewTool::begin([[maybe_unused]] EditorMap& map)
{
    last = {MARK_ANYWHERE, MARK_ANYWHERE};
    return true;
}

EditorAction* DragViewTool::end()
{
    return nullptr;
}

bool DragViewTool::apply([[maybe_unused]] EditorMap& map, [[maybe_unused]] int32_t x, [[maybe_unused]] int32_t y,
                         [[maybe_unused]] ToolKeys keys)
{
    if (last.x == MARK_ANYWHERE)
    {
        GetCursorPos(&last);
        return true;
    }

    POINT cursor;
    GetCursorPos(&cursor);
    POINT delta = {last.x - cursor.x, last.y - cursor.y};

    // Original behaviour: it tests the cursor's position, not the movement.
    if (cursor.x != 0 || cursor.y != 0)
    {
        ::SendMessage(mainWindow(), WM_EDITOR_SCROLL_VIEW, 0, reinterpret_cast<LPARAM>(&delta));
    }

    last = cursor;
    return true;
}

bool DragViewTool::applyAgain([[maybe_unused]] EditorMap& map, [[maybe_unused]] int32_t x, [[maybe_unused]] int32_t y,
                              [[maybe_unused]] ToolKeys keys)
{
    return true;
}

bool DragViewTool::canApply([[maybe_unused]] EditorMap& map, [[maybe_unused]] int32_t x, [[maybe_unused]] int32_t y,
                            [[maybe_unused]] ToolKeys keys)
{
    return true;
}

bool DragViewTool::canApplyAgain([[maybe_unused]] EditorMap& map, [[maybe_unused]] int32_t x,
                                 [[maybe_unused]] int32_t y, [[maybe_unused]] ToolKeys keys)
{
    return false;
}

bool DragViewTool::hasTarget([[maybe_unused]] EditorMap& map)
{
    return false;
}

ZoomTool::ZoomTool(bool zoomIn, float zoom)
    : CursorTool(zoomIn ? IDB_ZOOM_IN_CURSOR : IDB_ZOOM_OUT_CURSOR, ZOOM_HOT_X, ZOOM_HOT_Y), zoom(zoom), zoomIn(zoomIn)
{
}

bool ZoomTool::begin([[maybe_unused]] EditorMap& map)
{
    zoomed = false;
    return true;
}

EditorAction* ZoomTool::end()
{
    zoomed = false;
    return nullptr;
}

void ZoomTool::setTileSize() const
{
    if (zoom > static_cast<float>(FULL_ZOOM))
    {
        elevationScale = FULL_ELEVATION_SCALE;
        tileWorldHeight = FULL_TILE;
        tileWorldWidth = FULL_TILE;
    }
    else if (zoom > static_cast<float>(MIDDLE_ZOOM))
    {
        elevationScale = MIDDLE_ELEVATION_SCALE;
        tileWorldHeight = MIDDLE_TILE;
        tileWorldWidth = MIDDLE_TILE;
    }
    else if (zoom > static_cast<float>(SMALL_ZOOM))
    {
        elevationScale = SMALL_ELEVATION_SCALE;
        tileWorldHeight = SMALL_TILE;
        tileWorldWidth = SMALL_TILE;
    }
}

bool ZoomTool::apply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys)
{
    if (zoomed)
    {
        return true;
    }

    if (!zoomIn)
    {
        return applyAgain(map, x, y, keys);
    }

    zoom = zoom + zoom;
    setTileSize();
    zoomed = true;
    POINT centre = {x * 2, y * 2};
    ::SendMessage(mainWindow(), WM_COMMAND, ID_ZOOM_IN, 0);
    ::SendMessage(mainWindow(), WM_EDITOR_SCROLL_VIEW, 1, reinterpret_cast<LPARAM>(&centre));
    return true;
}

bool ZoomTool::applyAgain([[maybe_unused]] EditorMap& map, int32_t x, int32_t y, [[maybe_unused]] ToolKeys keys)
{
    if (zoomed)
    {
        return true;
    }

    zoom = zoom * 0.5f;
    setTileSize();
    ::SendMessage(mainWindow(), WM_COMMAND, ID_ZOOM_OUT, 0);
    POINT centre = {x >> 1, y >> 1};
    ::SendMessage(mainWindow(), WM_EDITOR_SCROLL_VIEW, 1, reinterpret_cast<LPARAM>(&centre));
    zoomed = true;
    return true;
}

bool ZoomTool::canApply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys)
{
    if (!zoomIn)
    {
        return canApplyAgain(map, x, y, keys);
    }

    return zoom < static_cast<float>(FULL_ZOOM);
}

bool ZoomTool::canApplyAgain([[maybe_unused]] EditorMap& map, [[maybe_unused]] int32_t x, [[maybe_unused]] int32_t y,
                             [[maybe_unused]] ToolKeys keys)
{
    return zoom > static_cast<float>(SMALLEST_ZOOM);
}

bool ZoomTool::hasTarget([[maybe_unused]] EditorMap& map)
{
    return false;
}
