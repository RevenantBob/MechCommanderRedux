#pragma once

#include <list>

#include "EditorTool.h"

class EditorObject;
class LinkMarker;

// The tool the orders' and objectives' value fields put on the view (WM_EDITOR_PICK) to pick their value on the map
// (virtual table 0x00502468, 0x0046f700-0x00470600). Its source file name wasn't kept; the names are the port's.

/// <summary>The view message that makes a pick tool the tool: wParam its mode (<see cref="MapPickTool"/>), lParam the window told.</summary>
inline constexpr UINT WM_EDITOR_PICK = WM_USER + 261;
/// <summary>
/// The message a pick tool sends the window that asked: for a path, wParam the number of points and lParam an array of
/// their map positions (POINT); else wParam the object id (0 for a location) and lParam the POINT picked (a location's
/// map position, an object's cell).
/// </summary>
inline constexpr UINT WM_EDITOR_PICKED = WM_USER + 262;

/// <summary>What a pick tool picks: the low word of its mode.</summary>
enum MapPickKind : int32_t
{
    /// <summary>A map position.</summary>
    PICK_LOCATION = 0,
    /// <summary>A structure (or drop zone).</summary>
    PICK_STRUCTURE = 1,
    /// <summary>A mech or vehicle.</summary>
    PICK_UNIT = 2,
    /// <summary>A path of up to 16 points, ended by Enter or Escape.</summary>
    PICK_PATH = 3,
};

/// <summary>The conditions a picked object must meet: the high word of a pick tool's mode.</summary>
enum MapPickFilter : int32_t
{
    /// <summary>Its type is capturable.</summary>
    PICK_CAPTURABLE = 1,
    /// <summary>Its type can be destroyed, or it is a mech or vehicle.</summary>
    PICK_DESTROYABLE = 2,
    /// <summary>It is the player's.</summary>
    PICK_PLAYERS = 4,
    /// <summary>It is the enemy's.</summary>
    PICK_ENEMYS = 8,
    /// <summary>It isn't the player's.</summary>
    PICK_NOT_PLAYERS = 0x10,
    /// <summary>It isn't the enemy's.</summary>
    PICK_NOT_ENEMYS = 0x20,
};

/// <summary>
/// The tool that picks a value on the map for the window that asked: a click picks a location, structure or unit; for a
/// path each click adds a point (a line follows the cursor from the last) until Enter, Escape or the 16th point. The
/// value goes to the window (<see cref="WM_EDITOR_PICKED"/>), and the view goes back to the select tool.
/// </summary>
/// <remarks>0x1343c bytes, virtual table 0x00502468 (11 slots).</remarks>
class MapPickTool : public CursorTool
{
public:
    /// <summary>The cursor image's size, as the place tool's.</summary>
    static constexpr int32_t IMAGE_WIDTH = 0x140;
    static constexpr int32_t IMAGE_HEIGHT = 0xf0;
    /// <summary>The most points a path takes: the one past it ends the pick.</summary>
    static constexpr size_t MAX_PATH_POINTS = 0xf;

    /// <summary>A tool picking by <paramref name="mode"/> (kind and filter) for <paramref name="window"/>.</summary>
    /// <remarks>0x0046f700.</remarks>
    MapPickTool(int32_t mode, CWnd* window);
    /// <remarks>0x0046f7d0. Original behaviour: the line marker stays on the map.</remarks>
    ~MapPickTool() override = default;

    /// <summary>Starts a click: nothing picked yet.</summary>
    /// <returns>true.</returns>
    /// <remarks>0x0046f840.</remarks>
    bool begin(EditorMap& map) override;
    /// <summary>
    /// When the pick is done, sends the value to the window and has the active window go back to the select tool
    /// (ID_PAINT).
    /// </summary>
    /// <returns>null.</returns>
    /// <remarks>0x0046f860.</remarks>
    EditorAction* end() override;
    /// <summary>Picks at the cell under (<paramref name="x"/>, <paramref name="y"/>), or adds it to the path.</summary>
    /// <returns>false off the grid.</returns>
    /// <remarks>0x0046f960.</remarks>
    bool apply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <returns>true.</returns>
    /// <remarks>0x0046fda0.</remarks>
    bool applyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary>
    /// Follows the cursor, then whether the cell under (<paramref name="x"/>, <paramref name="y"/>) can be picked: a
    /// location anywhere; a structure or unit there that meets the filter; a path point on a free cell (no mech or
    /// vehicle one cell down-right) with no water or cliff on its tile.
    /// </summary>
    /// <remarks>0x0046fdb0.</remarks>
    bool canApply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary>A location's cursor is the flag object tinted green; the others the tool's cursor.</summary>
    /// <remarks>0x00470130.</remarks>
    const uint8_t* cursorImage(EditorMap& map, const POINT& position, POINT* topLeft, SIZE* size,
                               int32_t zoom) override;
    /// <summary>Makes the path's line follow the cursor from its last point.</summary>
    /// <remarks>0x00470600.</remarks>
    void track(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;

    /// <summary>Ends the pick (Enter, Escape): takes the marks off the map and sends what was picked.</summary>
    /// <remarks>0x004703e0.</remarks>
    void finish(EditorMap& map);
    /// <summary>The Erase key: takes the path's last point off, or with none left ends the pick.</summary>
    /// <remarks>0x00470400.</remarks>
    void removeLastPoint(EditorMap& map);

private:
    /// <summary>The low word of <see cref="mode"/>.</summary>
    int32_t kind() const;
    /// <summary>Whether <paramref name="object"/> meets the filter.</summary>
    /// <remarks>0x00470070.</remarks>
    bool passesFilter(const EditorObject* object) const;
    /// <summary>
    /// The view position of cell (<paramref name="col"/>, <paramref name="row"/>)'s centre on the terrain, computed as
    /// the original does.
    /// </summary>
    static POINT cellCentreView(EditorMap& map, int32_t col, int32_t row);
    /// <summary>Puts a new line marker that follows the cursor at view position (<paramref name="x"/>, <paramref name="y"/>).</summary>
    void placeEndMarker(EditorMap& map, int32_t x, int32_t y);

    /// <summary>
    /// The value picked: a location's map position, an object's cell; for a path, the view position of its last
    /// point (+0x810, +0x814).
    /// </summary>
    int32_t x = -1;
    int32_t y = -1;
    /// <summary>The object id picked; 0 for a location (+0x818).</summary>
    uint32_t pickedId = 0xffffffff;
    /// <summary>The pick is done: <see cref="end"/> sends it (+0x81c).</summary>
    bool picked = false;
    /// <summary>The kind (low word) and filter (high word) (+0x820).</summary>
    int32_t mode;
    /// <summary>The path's points in map units (+0x828).</summary>
    std::list<POINT> path;
    /// <summary>The window the value goes to (+0x830).</summary>
    CWnd* window;
    /// <summary>The location cursor's image (+0x834), 0xff transparent.</summary>
    uint8_t pickImage[IMAGE_WIDTH * IMAGE_HEIGHT] = {};
    /// <summary>The line from the path's last point to the cursor (+0x13434); the map's marks own it.</summary>
    LinkMarker* endMarker = nullptr;
};
