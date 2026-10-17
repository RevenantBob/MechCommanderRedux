#pragma once

#include "EditorTool.h"

// The tools that move the view rather than the map: Drag View and Zoom In and Out (virtual tables 0x004fc1bc and
// 0x004fc1e8). Their source file name wasn't kept; the names are the port's. They reach the view through the main
// window, which passes the scroll on (WM_EDITOR_SCROLL_VIEW) and routes the zoom commands to the active view.

/// <summary>
/// The main window's and the view's message that scrolls the view: wParam 0 and lParam a step (POINT), or wParam 1
/// and lParam a view position (POINT) to centre the window on.
/// </summary>
inline constexpr UINT WM_EDITOR_SCROLL_VIEW = WM_USER + 260;

/// <summary>The Drag View tool: the view follows the cursor while the button is down.</summary>
/// <remarks>0x818 bytes, virtual table 0x004fc1bc.</remarks>
class DragViewTool : public CursorTool
{
public:
    /// <summary>A drag tool with the hand cursor.</summary>
    /// <remarks>Built inline by the view's command (0x0041fd80).</remarks>
    DragViewTool();

    /// <summary>Forgets the last cursor position.</summary>
    /// <returns>true.</returns>
    /// <remarks>0x0040a890.</remarks>
    bool begin(EditorMap& map) override;
    /// <returns>null.</returns>
    /// <remarks>0x0040a8b0.</remarks>
    EditorAction* end() override;
    /// <summary>Scrolls the view by how far the cursor moved since the last call (the first only notes where it is).</summary>
    /// <returns>true.</returns>
    /// <remarks>0x0040a8c0.</remarks>
    bool apply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <returns>true.</returns>
    /// <remarks>0x0040a960.</remarks>
    bool applyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <returns>true.</returns>
    /// <remarks>0x0041ff50.</remarks>
    bool canApply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <returns>false.</returns>
    /// <remarks>0x0041ff60.</remarks>
    bool canApplyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <returns>false.</returns>
    /// <remarks>0x0041ff70.</remarks>
    bool hasTarget(EditorMap& map) override;

private:
    /// <summary>The cursor's screen position at the last call; <c>MARK_ANYWHERE</c> before the first (+0x810).</summary>
    POINT last;
};

/// <summary>
/// The Zoom In and Zoom Out tools: a click zooms in (or for the Zoom Out tool out) once per press, centring the view on
/// the point clicked; the alternate apply always zooms out.
/// </summary>
/// <remarks>0x9ac bytes, virtual table 0x004fc1e8.</remarks>
class ZoomTool : public CursorTool
{
public:
    /// <summary>The Zoom In tool with <paramref name="zoomIn"/>, else the Zoom Out tool, starting at <paramref name="zoom"/>.</summary>
    /// <remarks>Built inline by the view's commands (0x004200d0, 0x00420390).</remarks>
    ZoomTool(bool zoomIn, float zoom);

    /// <summary>Allows the next zoom.</summary>
    /// <returns>true.</returns>
    /// <remarks>0x004202c0.</remarks>
    bool begin(EditorMap& map) override;
    /// <summary>Allows the next zoom.</summary>
    /// <returns>null.</returns>
    /// <remarks>0x004202d0.</remarks>
    EditorAction* end() override;
    /// <summary>
    /// The Zoom In tool doubles its zoom, sets the tile size for it and has the view zoom in and centre on
    /// (<paramref name="x"/>, <paramref name="y"/>) doubled; the Zoom Out tool does <see cref="applyAgain"/>. Once per press.
    /// </summary>
    /// <returns>true.</returns>
    /// <remarks>0x00418100.</remarks>
    bool apply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary>
    /// Halves its zoom, sets the tile size for it and has the view zoom out and centre on (<paramref name="x"/>,
    /// <paramref name="y"/>) halved. Once per press.
    /// </summary>
    /// <returns>true.</returns>
    /// <remarks>0x00418250.</remarks>
    bool applyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary>The Zoom In tool: whether its zoom is below the largest; the Zoom Out tool: <see cref="canApplyAgain"/>.</summary>
    /// <remarks>0x004202e0.</remarks>
    bool canApply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary>Whether its zoom is above the smallest.</summary>
    /// <remarks>0x00420330.</remarks>
    bool canApplyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <returns>false.</returns>
    /// <remarks>0x00420350.</remarks>
    bool hasTarget(EditorMap& map) override;

private:
    /// <summary>Sets the tile size and elevation scale for <see cref="zoom"/>.</summary>
    void setTileSize() const;

    /// <summary>The zoom it took from the view and changes as it goes (+0x810).</summary>
    float zoom;
    /// <summary>It zoomed during this press (+0x9a8).</summary>
    bool zoomed = false;
    /// <summary>The Zoom In tool (+0x9a9).</summary>
    bool zoomIn;
};
