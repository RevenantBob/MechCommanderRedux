#pragma once

#include <list>

#include "BrushTool.h"
#include "PlaceObjectTool.h"

// The tool that raises and lowers the map's vertices (virtual table 0x004faedc), the base of the overlay tools and
// the height part of the water, cliff and concrete tools. Its source file name wasn't kept; the names are the port's.

/// <summary>A vertex and a value: an elevation change, or a concrete or crop tile's look.</summary>
/// <remarks>0x14-byte list nodes in the original.</remarks>
struct VertexChange
{
    int32_t col;
    int32_t row;
    int32_t value;
};

/// <summary>
/// A vertex tool's "Change Height" action: the vertices' tiles and the objects removed with them, recorded as erased.
/// </summary>
/// <remarks>
/// 0x1c5c bytes, virtual table 0x004faf18: <see cref="ObjectsAction"/>'s slots but its own deleting destructor
/// (0x00413d50). Made inline in <see cref="VertexTool::begin"/>.
/// </remarks>
class HeightAction : public ObjectsAction
{
public:
    /// <summary>An action named <paramref name="name"/> on <paramref name="map"/> with nothing recorded yet.</summary>
    HeightAction(const std::string& name, EditorMap* map);
};

/// <summary>
/// The tool that moves the selected vertices up or down by its step (or to one elevation, or at random), keeping
/// neighbouring corners within a tile's reach of each other, then fixes the tiles the change spoiled: cliff and water
/// edges, overlays, mountain, concrete and crop, and objects left where they can't stand.
/// </summary>
/// <remarks>0x834 bytes, virtual table 0x004faedc (15 slots).</remarks>
class VertexTool : public TextureTool
{
public:
    /// <summary>
    /// A tool moving vertices by <paramref name="step"/>, painting <paramref name="texture"/> where its repairs
    /// paint; with <paramref name="ownAction"/> it records an undoable action of its own. Its cursor is
    /// <paramref name="bitmapId"/>, or for 0 the raise cursor for a step of 1 and the lower one otherwise.
    /// </summary>
    /// <remarks>0x004138f0.</remarks>
    VertexTool(int32_t step, uint8_t texture, bool ownAction, UINT bitmapId);

    /// <summary>Forgets the last point; with its own action, starts a "Change Height" action.</summary>
    /// <returns>true.</returns>
    /// <remarks>0x004139a0.</remarks>
    bool begin(EditorMap& map) override;
    /// <summary>
    /// With its own action: moves the painting's records into the height action and gives that (null when empty).
    /// </summary>
    /// <returns>The action, or null.</returns>
    /// <remarks>0x00413d80.</remarks>
    EditorAction* end() override;
    /// <summary>
    /// Selects the vertex nearest (<paramref name="x"/>, <paramref name="y"/>) when within reach, moves the selected
    /// vertices by the step and unselects them all.
    /// </summary>
    /// <remarks>0x00413e00.</remarks>
    bool apply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary><see cref="apply"/> lowering by 1 whatever the step.</summary>
    /// <remarks>0x00413f60.</remarks>
    bool applyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary>
    /// False only at the nearest vertex's lowest elevation with <paramref name="keys"/> negative (never) or at its
    /// highest with <paramref name="keys"/> nonzero.
    /// </summary>
    /// <remarks>0x00415860. The original tests the message keys as it would a step.</remarks>
    bool canApply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary>
    /// Whether the vertex nearest (<paramref name="x"/>, <paramref name="y"/>) is within reach and above 0 for a step
    /// of 1, or above 128 for a step of -1.
    /// </summary>
    /// <remarks>0x00413ec0.</remarks>
    bool canApplyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary>
    /// Moves every selected vertex by <paramref name="step"/> (lowering a vertex at 3 by 2 when a selected neighbour
    /// is at 1), the neighbours following as far as needed, then repairs the tiles and removes stranded objects.
    /// </summary>
    /// <returns>Whether every follow-up move was possible.</returns>
    /// <remarks>Virtual table slot 11 (0x00414530).</remarks>
    bool applyToSelection(EditorMap& map, int32_t step) override;

    /// <summary>
    /// Sets the selected vertices to <paramref name="elevation"/> (the neighbours following), repairs the tiles and
    /// removes stranded objects; with every vertex selected, sets every tile flat at it instead.
    /// </summary>
    /// <returns>Whether every follow-up move was possible.</returns>
    /// <remarks>0x00414f30.</remarks>
    bool setSelectedElevation(EditorMap& map, int32_t elevation);
    /// <summary>
    /// Moves each selected vertex up, down or not at all at random (the neighbours following) and repairs the tiles.
    /// </summary>
    /// <returns>true.</returns>
    /// <remarks>0x00415cd0.</remarks>
    bool makeHilly(EditorMap& map);
    /// <summary>
    /// Moves tile (<paramref name="col"/>, <paramref name="row"/>)'s four vertices to their rounded average (the
    /// neighbours following) and repairs the tiles.
    /// </summary>
    /// <returns>Whether every move was possible.</returns>
    /// <remarks>0x004165c0.</remarks>
    bool flattenTile(EditorMap& map, int32_t col, int32_t row);
    /// <summary>
    /// Moves vertex (<paramref name="col"/>, <paramref name="row"/>) by <paramref name="delta"/> (the neighbours
    /// following) and repairs the tiles.
    /// </summary>
    /// <returns>Whether the vertex's own move was possible.</returns>
    /// <remarks>0x004169c0.</remarks>
    bool moveVertex(EditorMap& map, int32_t col, int32_t row, int32_t delta);

    /// <summary>
    /// Removes the objects, drop zones and mechs and vehicles left on water (unless their type allows it) or cliff,
    /// and the objects and drop zones on tiles steeper than their type's slope, recording them in the height action.
    /// </summary>
    /// <returns>How many objects and drop zones were removed; -1 when one couldn't be.</returns>
    /// <remarks>0x00416020.</remarks>
    int32_t removeStrandedObjects(EditorMap& map);

    /// <summary>How far a vertex may be from the cursor to be picked (0x00527dec, doubled where used).</summary>
    static constexpr int32_t VERTEX_REACH = 48;

protected:
    /// <summary>The tiles and vertices a move touched, for the repairs after it.</summary>
    struct Work
    {
        /// <summary>The neighbouring vertices still to move, and by how much; taken from the back, added at the front.</summary>
        std::list<VertexChange> changes;
        /// <summary>The tiles that had cliff or water.</summary>
        std::list<POINT> cliffWater;
        /// <summary>The tiles that had an overlay.</summary>
        std::list<POINT> overlays;
        /// <summary>The tiles that had mountain.</summary>
        std::list<POINT> mountains;
        /// <summary>The concrete and crop tiles: <c>texture &lt;&lt; 16 | index</c>.</summary>
        std::list<VertexChange> concCrop;
    };

    /// <summary>
    /// Moves vertex (<paramref name="col"/>, <paramref name="row"/>) by <paramref name="delta"/> on its four tiles,
    /// noting each tile in <paramref name="work"/> first and each corner then too far from it as a change.
    /// </summary>
    /// <returns>false, moving nothing, when the new elevation is below 0 or above 128.</returns>
    /// <remarks>0x00414020.</remarks>
    bool adjustVertex(EditorMap& map, int32_t col, int32_t row, int32_t delta, Work& work);
    /// <summary>Moves the vertices of <paramref name="work"/>'s changes until none is left.</summary>
    /// <returns>Whether every move was possible.</returns>
    bool processChanges(EditorMap& map, Work& work, bool result);
    /// <summary>Runs the repairs of <paramref name="work"/>: cliff and water, overlays, mountain, concrete and crop.</summary>
    void repair(EditorMap& map, Work& work);
    /// <summary>
    /// Sets every tile flat at <paramref name="elevation"/> (recording them all), repainting the ones no terrain tile
    /// shows then.
    /// </summary>
    /// <remarks>0x00415a20.</remarks>
    void setAllElevations(EditorMap& map, int32_t elevation);

    /// <summary>Notes what tile (<paramref name="col"/>, <paramref name="row"/>) has, for the repairs.</summary>
    /// <remarks>0x00416df0.</remarks>
    static void noteTile(const EditorTile& tile, int32_t col, int32_t row, Work& work);
    /// <summary>Adds change (<paramref name="col"/>, <paramref name="row"/>, <paramref name="value"/>), or keeps the larger one.</summary>
    /// <remarks>0x004157c0.</remarks>
    static void pushChange(std::list<VertexChange>& changes, int32_t col, int32_t row, int32_t value);

public:
    /// <summary>Adds tile (<paramref name="col"/>, <paramref name="row"/>) at the front unless it is there.</summary>
    /// <remarks>0x00413210.</remarks>
    static void addTile(std::list<POINT>& tiles, int32_t col, int32_t row);

protected:
    /// <summary>
    /// Whether no corner of the four tiles around vertex (<paramref name="col"/>, <paramref name="row"/>) is more
    /// than 2 above it while <paramref name="delta"/> raises, or more than 2 below while it lowers.
    /// </summary>
    /// <remarks>0x004158d0.</remarks>
    static bool vertexFits(EditorMap& map, int32_t col, int32_t row, int32_t delta);

    /// <summary>
    /// Repaints the tiles of <paramref name="tiles"/> no terrain tile shows with their one other texture than cliff
    /// and water (the default when there are several or none).
    /// </summary>
    /// <remarks>0x00415380. The tool's texture keeps the last one.</remarks>
    void repairCliffWater(EditorMap& map, std::list<POINT>& tiles);
    /// <summary>
    /// Takes the overlays of <paramref name="tiles"/> off where water came (but all-rough ones) or no overlay tile fits
    /// any more, and refits the rest.
    /// </summary>
    /// <remarks>0x00415500.</remarks>
    static void repairOverlays(EditorMap& map, std::list<POINT>& tiles);
    /// <summary>Repaints the tiles of <paramref name="tiles"/> with a corner at 0 with their first texture but mountain.</summary>
    /// <remarks>0x004156a0. The tool's texture keeps the last one.</remarks>
    void repairMountain(EditorMap& map, std::list<POINT>& tiles);
    /// <summary>Gives the concrete and crop tiles of <paramref name="tiles"/> that are flat their look again.</summary>
    /// <remarks>0x00416c50.</remarks>
    static void repairConcCrop(EditorMap& map, std::list<VertexChange>& tiles);

    /// <summary>The last view position applied at; <c>MARK_ANYWHERE</c> for none (+0x820, +0x824).</summary>
    int32_t lastX;
    int32_t lastY;
    /// <summary>The tool records its own height action (+0x828); off while it changes the whole map.</summary>
    bool ownAction;
    /// <summary>The elevation step of <see cref="apply"/> (+0x82c).</summary>
    int32_t step;
    /// <summary>The height action of the use (+0x830).</summary>
    HeightAction* heightAction = nullptr;
};
