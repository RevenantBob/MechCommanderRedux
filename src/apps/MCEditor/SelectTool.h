#pragma once

#include "EditorTool.h"
#include "MapMarker.h"

class EditorObject;
class MoveObjectsTool;

// The select tool (virtual table 0x004facdc) and the outline it draws (0x004fad08). Their source file name wasn't
// kept; the names are the port's.

/// <summary>The select tool's outline: a dashed white parallelogram from the press to the cursor.</summary>
/// <remarks>0x20 bytes, virtual table 0x004fad08.</remarks>
class SelectionMarker : public MapMarker
{
public:
    /// <summary>An outline of no size.</summary>
    /// <remarks>0x00410880.</remarks>
    SelectionMarker() = default;

    /// <summary>
    /// Draws the outline from (<paramref name="x"/>, <paramref name="y"/>) to (x, y) +
    /// (<see cref="width"/>, <see cref="height"/>), its sides along the map's axes; nothing while either is 0.
    /// </summary>
    /// <returns>true.</returns>
    /// <remarks>0x004108a0.</remarks>
    bool draw(HDC dc, int32_t width, int32_t height, int32_t x, int32_t y, int32_t zoom) override;

    /// <summary>
    /// Draws a 4 x 4 grid in the outline, filling the cells of <see cref="cells"/> (+4); constructed false and never
    /// set, so the grid isn't drawn (not translated).
    /// </summary>
    bool grid = false;
    /// <summary>The grid's filled cells (+5).</summary>
    uint8_t cells[8] = {};
    /// <summary>The cursor's offset from the press (+0x18, +0x1c).</summary>
    int32_t width = 0;
    int32_t height = 0;
};

/// <summary>
/// The tool that selects tiles (with their vertices) and objects, by click or (in area mode) by dragging a
/// parallelogram, and drags an object it is pressed on.
/// </summary>
/// <remarks>0x82c bytes, virtual table 0x004facdc.</remarks>
class SelectTool : public CursorTool
{
public:
    /// <summary>A select tool, in area mode with <paramref name="area"/>; its cursor is IDB_SELECT_CURSOR.</summary>
    /// <remarks>0x0040fe20.</remarks>
    explicit SelectTool(bool area);
    /// <summary>Unselects everything on the map it was used on, and takes its outline off.</summary>
    /// <remarks>0x0040fea0.</remarks>
    ~SelectTool() override;

    /// <summary>Takes the last outline off the map.</summary>
    /// <returns>true.</returns>
    /// <remarks>0x0040ff80.</remarks>
    bool begin(EditorMap& map) override;
    /// <summary>Ends the move of a pressed object, if any.</summary>
    /// <returns>The move's action, or null.</returns>
    /// <remarks>0x00410820.</remarks>
    EditorAction* end() override;
    /// <summary>
    /// Without Shift or Ctrl, unselects everything first. Pressed on an object (not in area mode): selects it and
    /// starts moving it, or with Shift or Ctrl toggles it. Else toggles the tile under the cursor (with its four
    /// vertices); in area mode also selects the tiles, inner vertices and objects within the outline from the press.
    /// </summary>
    /// <returns>true.</returns>
    /// <remarks>0x0040fff0.</remarks>
    bool apply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary><see cref="apply"/>.</summary>
    /// <remarks>0x00410800.</remarks>
    bool applyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary>While an object moves, whether it can go there; else true.</summary>
    /// <remarks>0x00410e80.</remarks>
    bool canApply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;

    /// <summary>
    /// Selects every tile of the map, every vertex, every object and drop zone (not the mechs and vehicles).
    /// </summary>
    /// <remarks>0x00410d50.</remarks>
    void selectAll();

private:
    /// <summary>The outline, on the map's marks while shown (+0x810).</summary>
    SelectionMarker* marker = nullptr;
    /// <summary>The move of a pressed object (+0x814).</summary>
    MoveObjectsTool* moveTool = nullptr;
    /// <summary>The map last used on (+0x818).</summary>
    EditorMap* map = nullptr;
    /// <summary>The view position of the press; <c>MARK_ANYWHERE</c> for none (+0x81c, +0x820).</summary>
    int32_t anchorX;
    int32_t anchorY;
    /// <summary>Area mode (+0x825).</summary>
    bool area;
    /// <summary>The object the press moves (+0x828).</summary>
    EditorObject* pressedObject = nullptr;
};
