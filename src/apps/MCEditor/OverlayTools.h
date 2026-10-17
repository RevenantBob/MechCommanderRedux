#pragma once

#include "VertexTool.h"

class ObjectsAction;

// The tools that put overlays on the tiles and take overlays and objects off (virtual tables 0x004fac2c and
// 0x004fab78). Their source file name wasn't kept; the names are the port's.

/// <summary>
/// The Erase tool: takes the object under the cursor off the map, or failing one the tile's overlay; recorded in an
/// objects action.
/// </summary>
/// <remarks>0x824 bytes, virtual table 0x004fac2c.</remarks>
class EraseTool : public BrushTool
{
public:
    /// <summary>
    /// An erase tool with cursor <paramref name="bitmapId"/> (hot spot <paramref name="hotX"/>,
    /// <paramref name="hotY"/>); with <paramref name="kind"/> other than -1 it erases nothing on a tile without an
    /// overlay.
    /// </summary>
    /// <remarks>Built inline where used (0x0041e0d0 for the Erase command).</remarks>
    EraseTool(UINT bitmapId, int32_t hotX, int32_t hotY, int32_t kind);

    /// <summary>Starts an objects action named "Erase".</summary>
    /// <returns>true.</returns>
    /// <remarks>0x0040e450.</remarks>
    bool begin(EditorMap& map) override;
    /// <summary>Gives the objects action; deletes it (null) when nothing was erased.</summary>
    /// <remarks>0x0040e6a0. The action given stays the tool's until the next use.</remarks>
    EditorAction* end() override;
    /// <summary>
    /// Erases the object (or the mech or vehicle one cell up-left) under (<paramref name="x"/>, <paramref name="y"/>),
    /// or failing one the overlay of the tile there.
    /// </summary>
    /// <returns>false off the map, or for a tile without an overlay when the tool has a kind; else true.</returns>
    /// <remarks>0x0040e6e0.</remarks>
    bool apply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <returns>true.</returns>
    /// <remarks>0x0040d3a0.</remarks>
    bool applyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary>Whether the tile under (<paramref name="x"/>, <paramref name="y"/>) has an overlay or an object is there.</summary>
    /// <remarks>0x0040e940.</remarks>
    bool canApply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary>Whether a selected tile has an overlay or an object on its cells.</summary>
    /// <remarks>0x0040ea60.</remarks>
    bool hasTarget(EditorMap& map) override;
    /// <summary>Takes the overlays off the selected tiles, then erases every selected object, drop zone and mover.</summary>
    /// <returns>true.</returns>
    /// <remarks>0x0040ec50.</remarks>
    bool applyToSelection(EditorMap& map, int32_t step) override;

    /// <summary>The use's action (+0x81c).</summary>
    ObjectsAction* objectsAction = nullptr;

private:
    /// <summary>The overlay kind it erases for an overlay tool, -1 for the Erase tool (+0x820).</summary>
    int32_t kind;
};

/// <summary>
/// The tool that lays a road, wall, runway or the like: each tile it passes (the line from the last one included)
/// gets the overlay, joined to the overlays of that kind around it and the ground under it lowered to fit.
/// </summary>
/// <remarks>0x848 bytes, virtual table 0x004fab78 (15 slots).</remarks>
class OverlayTool : public VertexTool
{
public:
    /// <summary>A tool laying overlay kind <paramref name="kind"/>.</summary>
    /// <remarks>0x0040c970.</remarks>
    explicit OverlayTool(int32_t kind);

    /// <summary>Starts the height action, named after the overlay kind, and forgets the use's tiles.</summary>
    /// <returns>What the vertex tool's start returns.</returns>
    /// <remarks>0x0040cc60.</remarks>
    bool begin(EditorMap& map) override;
    /// <summary>
    /// Lays the overlay on the tile under (<paramref name="x"/>, <paramref name="y"/>) and (but for docks) on each dry
    /// tile of the line from the last one.
    /// </summary>
    /// <returns>What laying it on the tile returns; false off the map.</returns>
    /// <remarks>0x0040cd80.</remarks>
    bool apply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary>Erases the overlay or object under (<paramref name="x"/>, <paramref name="y"/>) through an erase tool.</summary>
    /// <returns>What the erase returns.</returns>
    /// <remarks>
    /// 0x0040d050. Original behaviour (EB-084): only the erased tiles join the height action; erased objects can't be
    /// undone.
    /// </remarks>
    bool applyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary>Lights the tile up; whether it has no water.</summary>
    /// <remarks>0x0040c9b0.</remarks>
    bool canApply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary>Whether there is an overlay or object to erase there; lights the tile up if so.</summary>
    /// <remarks>0x0040d3d0.</remarks>
    bool canApplyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary>Whether no selected tile has water, and not every vertex is selected.</summary>
    /// <remarks>0x0040d530.</remarks>
    bool hasTarget(EditorMap& map) override;
    /// <summary>Lays the overlay on every selected tile, row by row.</summary>
    /// <remarks>0x0040d710.</remarks>
    bool applyToSelection(EditorMap& map, int32_t step) override;

protected:
    /// <summary>
    /// Lays the overlay on tile (<paramref name="col"/>, <paramref name="row"/>): next to the use's first tile, gives
    /// that one a straight piece toward it; joins the corners to the overlays of the kind beside it; lowers corners
    /// more than 1 up; and if no overlay tile fits, tries the piece straight, then flattens the tile.
    /// </summary>
    /// <returns>Whether the tile got the overlay (true for a runway that didn't).</returns>
    /// <remarks>0x0040d7d0.</remarks>
    bool layOverlay(EditorMap& map, int32_t col, int32_t row);
    /// <summary>
    /// Which kind each side of tile (<paramref name="col"/>, <paramref name="row"/>) should take from its neighbour
    /// (above, right, below, left): this kind where the neighbour's facing corner has it (or a bridge it continues),
    /// or could; else <see cref="NO_JOIN"/>.
    /// </summary>
    /// <returns>true.</returns>
    /// <remarks>0x0040cac0.</remarks>
    bool neighbourKinds(EditorMap& map, int32_t col, int32_t row, int32_t* kinds);
    /// <summary>Whether this kind joins corner <paramref name="corner"/> of <paramref name="tile"/>, into <paramref name="kind"/>.</summary>
    /// <returns>true.</returns>
    /// <remarks>0x0040e370.</remarks>
    bool sideKind(const EditorTile* tile, int32_t* kind, int32_t corner);

    /// <summary>A side <see cref="neighbourKinds"/> found nothing to join.</summary>
    static constexpr int32_t NO_JOIN = 0x20;

    /// <summary>The overlay kind (+0x834).</summary>
    int32_t kind;
    /// <summary>The use's first tile; -1 for none yet (+0x838, +0x83c).</summary>
    int32_t firstCol = -1;
    int32_t firstRow = -1;
    /// <summary>The use's last tile; -1 for none yet (+0x840, +0x844).</summary>
    int32_t lastCol = -1;
    int32_t lastRow = -1;
};

/// <summary>
/// The tool that lays a road or rail bridge piece on each tile pressed: the ground under it shaped to the roads it
/// meets, the roads' ends joined to it.
/// </summary>
/// <remarks>0x84c bytes, virtual table 0x004fae6c (15 slots).</remarks>
class BridgeTool : public OverlayTool
{
public:
    /// <summary>
    /// A tool laying bridge kind <paramref name="kind"/> (road or rail), across the tile's first and third corners or
    /// with <paramref name="turned"/> its second and fourth.
    /// </summary>
    /// <remarks>Built inline by the factory (0x00412ea1).</remarks>
    BridgeTool(int32_t kind, bool turned);

    /// <summary>Lays the bridge on the tile under (<paramref name="x"/>, <paramref name="y"/>).</summary>
    /// <returns>Whether it was laid; false off the map.</returns>
    /// <remarks>0x00401030.</remarks>
    bool apply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary>The overlay tool's erase.</summary>
    /// <remarks>0x00401090.</remarks>
    bool applyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary>Lights the tile up.</summary>
    /// <returns>true.</returns>
    /// <remarks>0x004010b0.</remarks>
    bool canApply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary>Lights the tile up.</summary>
    /// <returns>true.</returns>
    /// <remarks>0x004010d0.</remarks>
    bool canApplyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <returns>true.</returns>
    /// <remarks>0x004010f0.</remarks>
    bool hasTarget(EditorMap& map) override;
    /// <summary>Lays the bridge on every selected tile, column by column.</summary>
    /// <remarks>0x00401100.</remarks>
    bool applyToSelection(EditorMap& map, int32_t step) override;

private:
    /// <summary>
    /// Lays the bridge on tile (<paramref name="col"/>, <paramref name="row"/>): its corners raised or lowered to the
    /// roads at its ends, the roads' facing corners joined, flattened when no bridge tile fits.
    /// </summary>
    /// <remarks>0x00401190.</remarks>
    bool layBridge(EditorMap& map, int32_t col, int32_t row);
    /// <summary>
    /// What lies at the bridge's two ends of tile (<paramref name="col"/>, <paramref name="row"/>): per side
    /// (above, right, below, left) the road kind when the neighbour has the road, and per corner the height the bridge
    /// should have there (1 at a road, the neighbour's own where it continues the bridge); <see cref="NO_JOIN"/> else.
    /// </summary>
    /// <returns>true.</returns>
    /// <remarks>0x00401870.</remarks>
    bool bridgeEnds(EditorMap& map, int32_t col, int32_t row, int32_t* sides, uint8_t* heights);
    /// <summary>What neighbour <paramref name="tile"/> offers the bridge end at its corner <paramref name="corner"/>.</summary>
    /// <returns>true.</returns>
    /// <remarks>0x00401ad0.</remarks>
    bool bridgeSide(const EditorTile* tile, int32_t corner, int32_t* side, uint8_t* height);

    /// <summary>Across the tile's second and fourth corners rather than its first and third (+0x848).</summary>
    bool turned;
};

/// <summary>
/// The tool behind the Map Features palette's overlay layout buttons: each tile pressed gets an overlay of the
/// button's kind on the corners its layout names, flat (or as near as the library has), whatever lies around it.
/// </summary>
/// <remarks>0x85c bytes, virtual table 0x004fab3c (15 slots).</remarks>
class OverlayLayoutTool : public OverlayTool
{
public:
    /// <summary>
    /// A tool laying overlay kind <paramref name="kind"/> on the corners whose bits are set in
    /// <paramref name="layout"/>.
    /// </summary>
    /// <remarks>0x00409980.</remarks>
    OverlayLayoutTool(int32_t kind, int32_t layout);

    /// <summary>Lays the overlay on the tile under (<paramref name="x"/>, <paramref name="y"/>).</summary>
    /// <returns>Whether it was laid; false off the map.</returns>
    /// <remarks>0x00409a50.</remarks>
    bool apply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary>Lights the tile up (the brush's).</summary>
    /// <returns>true.</returns>
    /// <remarks>0x00409c80.</remarks>
    bool canApply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <returns>true.</returns>
    /// <remarks>0x00409ca0.</remarks>
    bool hasTarget(EditorMap& map) override;
    /// <summary>Lays the overlay on every selected tile, column by column.</summary>
    /// <returns>Whether every one was laid.</returns>
    /// <remarks>0x00409cb0.</remarks>
    bool applyToSelection(EditorMap& map, int32_t step) override;

private:
    /// <summary>
    /// Lays the overlay on tile (<paramref name="col"/>, <paramref name="row"/>) at the tile's heights (2 taken as
    /// 1), or flat when the library has no such overlay, recording the tile.
    /// </summary>
    /// <returns>Whether the tile is on the map.</returns>
    /// <remarks>0x00409aa0.</remarks>
    bool layLayout(EditorMap& map, int32_t col, int32_t row);

    /// <summary>The corners the overlay goes on: bit i for corner i (+0x848).</summary>
    int32_t layout;
    /// <summary>The overlay kind on each corner, <c>OVERLAY_NONE</c> off the layout (+0x84c).</summary>
    int32_t layoutKinds[4];
};

/// <summary>
/// The tool that plants forest: a tile gets a full forest overlay, and (with the GTiles set) the tiles around blend
/// into it; the alternate button thins one ninth of a forest tile at a time.
/// </summary>
/// <remarks>0x81c bytes, virtual table 0x004fabb4.</remarks>
class ForestTool : public BrushTool
{
public:
    /// <summary>A forest tool.</summary>
    /// <remarks>0x00409d60.</remarks>
    ForestTool();

    /// <summary>Starts an action named after the forest.</summary>
    /// <returns>true.</returns>
    /// <remarks>0x00409db0.</remarks>
    bool begin(EditorMap& map) override;
    /// <summary>Plants the tile under (<paramref name="x"/>, <paramref name="y"/>).</summary>
    /// <returns>Whether the forest fit; false off the map.</returns>
    /// <remarks>0x00409de0.</remarks>
    bool apply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary>
    /// Thins the forest under (<paramref name="x"/>, <paramref name="y"/>): the middle ninth of a tile clears it, the
    /// others clear the corners they lie toward (the overlay goes when what is left has no tile).
    /// </summary>
    /// <returns>false off the map or on a tile without an overlay.</returns>
    /// <remarks>0x0040a670.</remarks>
    bool applyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary>Whether the tile under (<paramref name="x"/>, <paramref name="y"/>) can take forest; lights it up if so.</summary>
    /// <remarks>0x00409e30.</remarks>
    bool canApply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary>Lights the tile up.</summary>
    /// <returns>true.</returns>
    /// <remarks>0x00409e90.</remarks>
    bool canApplyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary>Whether every selected tile can take forest.</summary>
    /// <remarks>0x00409eb0.</remarks>
    bool hasTarget(EditorMap& map) override;
    /// <summary>Plants every selected tile, column by column.</summary>
    /// <remarks>0x0040a5d0.</remarks>
    bool applyToSelection(EditorMap& map, int32_t step) override;

    /// <summary>
    /// Plants tile (<paramref name="col"/>, <paramref name="row"/>); with the GTiles set the neighbours' facing
    /// corners take forest too, a neighbour that can't but already has forest there is filled (and with
    /// <paramref name="spread"/> planted in turn).
    /// </summary>
    /// <returns>Whether the tile took the forest.</returns>
    /// <remarks>0x0040a0b0.</remarks>
    bool plant(EditorMap& map, int32_t col, int32_t row, bool spread);
    /// <summary>
    /// Whether tile (<paramref name="col"/>, <paramref name="row"/>) can take forest: no water, concrete or crop, no
    /// overlay but forest, no object or drop zone on its cells.
    /// </summary>
    /// <remarks>0x00409f30.</remarks>
    static bool canPlant(EditorMap& map, int32_t col, int32_t row);
};

/// <summary>The tool that puts rough ground (an all-rough overlay) on the tiles; the alternate button takes it off.</summary>
/// <remarks>0x81c bytes, virtual table 0x004fac64.</remarks>
class RoughTool : public BrushTool
{
public:
    /// <summary>A rough tool.</summary>
    /// <remarks>0x0040f060.</remarks>
    RoughTool();

    /// <summary>Starts an action named after the rough ground.</summary>
    /// <returns>true.</returns>
    /// <remarks>0x0040f0b0.</remarks>
    bool begin(EditorMap& map) override;
    /// <summary>Puts rough on the tile under (<paramref name="x"/>, <paramref name="y"/>).</summary>
    /// <remarks>0x0040f0e0.</remarks>
    bool apply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary>Takes a rough overlay off the tile under (<paramref name="x"/>, <paramref name="y"/>).</summary>
    /// <returns>Whether one was taken off.</returns>
    /// <remarks>0x0040f130. Original behaviour (EB-080): the removal isn't recorded, so Undo doesn't bring it back.</remarks>
    bool applyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <returns>true (the tile isn't lit up).</returns>
    /// <remarks>0x0040f200.</remarks>
    bool canApply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <returns>true.</returns>
    /// <remarks>0x0040f210.</remarks>
    bool hasTarget(EditorMap& map) override;
    /// <summary>Puts rough on every selected tile, column by column.</summary>
    /// <remarks>0x0040f3e0.</remarks>
    bool applyToSelection(EditorMap& map, int32_t step) override;

private:
    /// <summary>Puts an all-rough overlay on tile (<paramref name="col"/>, <paramref name="row"/>).</summary>
    /// <returns>Whether it fit; false off the map.</returns>
    /// <remarks>0x0040f2e0.</remarks>
    bool putRough(EditorMap& map, int32_t col, int32_t row);
};
