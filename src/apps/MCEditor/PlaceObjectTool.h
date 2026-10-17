#pragma once

#include <list>

#include "EditorAction.h"
#include "EditorTool.h"
#include "ErrorDialog.h"

class EditorObject;

// The tools that place, erase and move the map's objects, and their undo records (virtual tables 0x004fa944,
// 0x004fa9ec, 0x004fa99c and 0x004faa18). Their source file name wasn't kept; the names are the port's.

/// <summary>Objects placed on the map or erased from it, with the tiles the placing changed.</summary>
/// <remarks>0x1c5c bytes, virtual table 0x004fa99c.</remarks>
class ObjectsAction : public TileAction
{
public:
    /// <summary>An action named <paramref name="name"/> on <paramref name="map"/> with no objects yet.</summary>
    ObjectsAction(const std::string& name, EditorMap* map, bool erase);
    /// <summary>Deletes the recorded objects.</summary>
    /// <remarks>0x00403610.</remarks>
    ~ObjectsAction() override;

    /// <summary>Swaps the tiles back, then erases the objects again (or places them again).</summary>
    /// <remarks>0x00403520.</remarks>
    bool redo() override;
    /// <summary>Swaps the tiles back, then places the objects again (or erases them again).</summary>
    /// <remarks>0x00403550.</remarks>
    bool undo() override;

    /// <summary>Adds a copy of <paramref name="object"/> to <see cref="objects"/> unless one like it is there.</summary>
    /// <remarks>0x00403580.</remarks>
    void recordObject(EditorObject& object);
    /// <summary>Whether <see cref="objects"/> has one like <paramref name="object"/> (same cell and id).</summary>
    bool isRecorded(const EditorObject& object) const;

    /// <summary>The objects were erased rather than placed (+0x1c44).</summary>
    bool erase;
    /// <summary>The map (+0x1c48).</summary>
    EditorMap* objectMap;
    /// <summary>
    /// Each redo and undo swaps the object on each recorded one's cell with the recorded one (+0x1c4c); constructed
    /// false.
    /// </summary>
    bool replace = false;
    /// <summary>Copies of the objects as they were placed or erased (+0x1c50).</summary>
    std::list<EditorObject*> objects;

protected:
    /// <summary>
    /// Takes the recorded objects off the map (each like it there, asking about what refers to it), or in
    /// <see cref="replace"/> mode swaps them in.
    /// </summary>
    /// <returns>Whether every object was removed.</returns>
    /// <remarks>0x00403730.</remarks>
    bool removeRecorded();
    /// <summary>Places a copy of each recorded object, or in <see cref="replace"/> mode swaps them in.</summary>
    /// <returns>Whether every copy was placed.</returns>
    /// <remarks>0x00403870.</remarks>
    bool placeRecorded();
    /// <summary>
    /// Swaps each recorded object with the object on its cell: the one on the map is copied into the record and
    /// removed, the recorded one placed.
    /// </summary>
    void swapRecorded();
};

/// <summary>Objects moved on the map: the copies as they were, and the cells they went to.</summary>
/// <remarks>0x1c68 bytes, virtual table 0x004faa18.</remarks>
class MoveObjectsAction : public ObjectsAction
{
public:
    /// <summary>An action (with no name) on <paramref name="map"/> with no moves yet.</summary>
    explicit MoveObjectsAction(EditorMap* map);
    /// <summary>Frees the cells.</summary>
    /// <remarks>0x00404130.</remarks>
    ~MoveObjectsAction() override = default;

    /// <summary>Moves each object from its recorded cell to the cell it went to.</summary>
    /// <returns>False when a recorded cell is off the grid or empty.</returns>
    /// <remarks>0x00404b10.</remarks>
    bool redo() override;
    /// <summary>Moves the object on each cell it went to back to its recorded cell.</summary>
    /// <returns>true.</returns>
    /// <remarks>0x00404c40.</remarks>
    bool undo() override;

    /// <summary>The cell (column, row) each recorded object went to, in the same order (+0x1c60).</summary>
    std::list<POINT> destinations;
};

/// <summary>The tool that places objects of one id (or erases objects) where it is applied.</summary>
/// <remarks>0x258b4 bytes, virtual table 0x004fa944.</remarks>
class PlaceObjectTool : public EditorTool
{
public:
    /// <summary>The cursor image's size.</summary>
    static constexpr int32_t IMAGE_WIDTH = 0x140;
    static constexpr int32_t IMAGE_HEIGHT = 0xf0;

    /// <summary>
    /// A tool placing <paramref name="id"/> for <paramref name="side"/>, or with <paramref name="erase"/> erasing:
    /// the cursor image is the object at full size, tinted green.
    /// </summary>
    /// <remarks>0x00401e50.</remarks>
    PlaceObjectTool(uint32_t id, int32_t side, bool erase);
    /// <summary>Deletes the error box.</summary>
    /// <remarks>0x00401f80.</remarks>
    ~PlaceObjectTool() override = default;

    /// <summary>
    /// Starts an <see cref="ObjectsAction"/> named after the object's type (for erasing, the Erase Overlays command's
    /// text).
    /// </summary>
    /// <returns>true.</returns>
    /// <remarks>0x00402000.</remarks>
    bool begin(EditorMap& map) override;
    /// <summary>Ends the action: deleted when no object was placed or erased.</summary>
    /// <remarks>0x004024c0.</remarks>
    EditorAction* end() override;
    /// <summary>
    /// Places a new object on the cell under (<paramref name="x"/>, <paramref name="y"/>) (a gate in its tile's
    /// middle cell, taking the tile's walls off), or erases the object there.
    /// </summary>
    /// <returns>false off the grid, or when there is nothing to erase.</returns>
    /// <remarks>0x00402500.</remarks>
    bool apply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary>Applies the tool in erase mode.</summary>
    /// <remarks>0x00405040.</remarks>
    bool applyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary>Whether the object can be placed on the cell under (<paramref name="x"/>, <paramref name="y"/>).</summary>
    /// <remarks>0x00402910.</remarks>
    bool canApply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary><see cref="canApply"/> in erase mode.</summary>
    /// <remarks>0x004028d0.</remarks>
    bool canApplyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary>In erase mode, whether any object is selected; else false.</summary>
    /// <remarks>0x00403430.</remarks>
    bool hasTarget(EditorMap& map) override;
    /// <summary>The object tinted green on the cell under <paramref name="position"/>.</summary>
    /// <remarks>0x00403e20.</remarks>
    const uint8_t* cursorImage(EditorMap& map, const POINT& position, POINT* topLeft, SIZE* size,
                               int32_t zoom) override;
    /// <summary>The object tinted red on the cell under <paramref name="position"/>.</summary>
    /// <remarks>0x00403e60.</remarks>
    const uint8_t* notAllowedImage(EditorMap& map, const POINT& position, POINT* topLeft, SIZE* size,
                                   int32_t zoom) override;

    /// <summary>
    /// Whether the object can stand on cell (<paramref name="col"/>, <paramref name="row"/>): the tile's terrain and
    /// overlay allow it, its corners are no steeper than the object's slope, the cells of its footprint are free, the
    /// map takes it (<see cref="EditorMap::isPlaceable"/>, which may tell why not in the error box), and the tile
    /// holds at most 7 objects then. Always in erase mode.
    /// </summary>
    /// <remarks>0x004029a0.</remarks>
    bool canPlaceAt(EditorMap& map, int32_t col, int32_t row);
    /// <summary>In erase mode, erases the selected objects; else nothing.</summary>
    /// <returns>true.</returns>
    /// <remarks>0x004039c0.</remarks>
    bool eraseSelected(EditorMap& map);
    /// <summary>
    /// Steps the object to the frame <paramref name="delta"/> away in its type (wrapping around) when it has more
    /// than one look, and draws the cursor image of it again (untinted).
    /// </summary>
    /// <returns>Whether the frame changed.</returns>
    /// <remarks>0x00404d50.</remarks>
    bool stepFrame(int32_t delta);

protected:
    /// <summary>
    /// The object drawn at zoom level <paramref name="zoom"/> into <see cref="image"/>, mapped through
    /// <paramref name="tint"/>, with its cell centre's view position in <paramref name="topLeft"/>.
    /// </summary>
    /// <returns>The image; null when the cell under <paramref name="position"/> is off the grid.</returns>
    /// <remarks>0x00403b80.</remarks>
    const uint8_t* drawCursor(EditorMap& map, const POINT& position, POINT* topLeft, SIZE* size, int32_t zoom,
                              const uint8_t* tint);
    /// <summary>Shows <paramref name="text"/> in the error box (made the first time) and ends the use of the tool.</summary>
    void showError(const CString& text);

    /// <summary>The action of the current use (+4).</summary>
    ObjectsAction* action = nullptr;

public:
    /// <summary>The object id placed; -1 for none (+8).</summary>
    uint32_t id;

protected:
    /// <summary>The tool erases (+0xc); also set while the erase-mode slots run.</summary>
    bool erase;
    /// <summary>The side of the placed objects (+0x10).</summary>
    int32_t side;
    /// <summary>The cursor image (+0x14), 0xff transparent.</summary>
    uint8_t image[IMAGE_WIDTH * IMAGE_HEIGHT];
    /// <summary>A use is going on (+0x25814).</summary>
    bool active = false;
    /// <summary>The box that says why an object can't be placed (+0x25818).</summary>
    ErrorDialog errorDialog;
};

/// <summary>The select tool's helper that drags the object it was pressed on to other cells.</summary>
/// <remarks>0x258bc bytes, virtual table 0x004fa9ec.</remarks>
class MoveObjectsTool : public PlaceObjectTool
{
public:
    /// <summary>A tool with no object.</summary>
    /// <remarks>0x00403ea0.</remarks>
    MoveObjectsTool();
    /// <remarks>0x00403f00.</remarks>
    ~MoveObjectsTool() override = default;

    /// <summary>Starts a <see cref="MoveObjectsAction"/>.</summary>
    /// <returns>true.</returns>
    /// <remarks>0x00403f80.</remarks>
    bool begin(EditorMap& map) override;
    /// <summary>Records the cell the object ended on, then ends the action as the place tool does.</summary>
    /// <remarks>0x00404270.</remarks>
    EditorAction* end() override;
    /// <summary>Moves the object to the cell under (<paramref name="x"/>, <paramref name="y"/>).</summary>
    /// <returns>true.</returns>
    /// <remarks>0x00404330.</remarks>
    bool apply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary>
    /// Before the drag, takes the object on the cell under (<paramref name="x"/>, <paramref name="y"/>) (recording
    /// it); during it, whether the object can stand there: as <see cref="canPlaceAt"/> without the overlays and the
    /// map's limits, the object itself not counting.
    /// </summary>
    /// <remarks>0x004043a0.</remarks>
    bool canApply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <returns>false.</returns>
    /// <remarks>0x00403ed0.</remarks>
    bool canApplyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <returns>false.</returns>
    /// <remarks>0x00404ac0.</remarks>
    bool hasTarget(EditorMap& map) override;
    /// <summary>The object tinted green on the cell under <paramref name="position"/>.</summary>
    /// <remarks>0x00404ad0.</remarks>
    const uint8_t* cursorImage(EditorMap& map, const POINT& position, POINT* topLeft, SIZE* size,
                               int32_t zoom) override;

private:
    /// <summary>The object being moved (+0x258b4).</summary>
    EditorObject* object = nullptr;
    /// <summary>The drag is going on (+0x258b8).</summary>
    bool dragging = false;
};
