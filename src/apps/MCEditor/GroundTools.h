#pragma once

#include "TileLibrary.h"
#include "VertexTool.h"

// The palette's water, cliff, concrete and crop tools (virtual tables 0x004fadf4, 0x004fae30 and 0x004faa7c): texture
// tools that also shape the ground through a vertex tool of their own. Their source file name wasn't kept; the names
// are the port's.

/// <summary>
/// Ends a use that stranded an object it can't remove: takes the use's action, undoes and deletes it, and with
/// <paramref name="release"/> sends the active view a button release so that the drag ends.
/// </summary>
void abandonUse(EditorTool& tool, bool release);

/// <summary>
/// The tool that floods tiles: their vertices (and the ring around) go to one water level, the lowest corners turn
/// to water, and objects that can't stand in water are removed.
/// </summary>
/// <remarks>0x1058 bytes, virtual table 0x004fadf4 (15 slots).</remarks>
class WaterTool : public TextureTool
{
public:
    /// <summary>A water tool.</summary>
    WaterTool();

    /// <summary>Forgets the water level and starts both actions.</summary>
    /// <returns>What the painting's start returns.</returns>
    /// <remarks>0x004171a0.</remarks>
    bool begin(EditorMap& map) override;
    /// <summary>Merges the painting's records into the height action and gives that (null when empty).</summary>
    /// <remarks>0x004173a0.</remarks>
    EditorAction* end() override;
    /// <summary>
    /// Takes the level from the first tile pressed (its corners' rounded average), sets the 4 x 4 vertices from up-left of the
    /// tile under (<paramref name="x"/>, <paramref name="y"/>) to it and paints the water.
    /// </summary>
    /// <returns>What the painting returns; false, the use undone, when a stranded object can't be removed.</returns>
    /// <remarks>0x004171d0.</remarks>
    bool apply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary>Lights the tile up; whether no object stands on any of its cells.</summary>
    /// <remarks>0x00417430.</remarks>
    bool canApply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary>Whether no object stands on any cell of a selected tile.</summary>
    /// <remarks>0x00417a50.</remarks>
    bool hasTarget(EditorMap& map) override;
    /// <summary>
    /// Floods the selection: every selected tile's 4 x 4 vertices to the selection's average elevation, then paints it.
    /// </summary>
    /// <remarks>0x00417760.</remarks>
    bool applyToSelection(EditorMap& map, int32_t step) override;
    /// <summary>
    /// <see cref="BrushTool::paintTerrain"/>, leaving water off the corners 1 up; a tile that changed loses an overlay
    /// that isn't all rough, and a failed one's record keeps its state after the change.
    /// </summary>
    /// <remarks>0x00417c60.</remarks>
    void paintTerrain(EditorMap& map, int32_t col, int32_t row, const uint8_t* textures, const int32_t* elevations,
                      FailedTiles& failed) override;
    /// <summary>Gives tile (<paramref name="col"/>, <paramref name="row"/>)'s lowest corners water and blends the tiles around.</summary>
    /// <remarks>0x004175f0.</remarks>
    bool paintTile(EditorMap& map, int32_t col, int32_t row) override;

private:
    /// <summary>Whether no object stands on a cell of tile (<paramref name="col"/>, <paramref name="row"/>).</summary>
    /// <remarks>0x00417fe0, also inlined.</remarks>
    static bool tileClear(EditorMap& map, int32_t col, int32_t row);

    /// <summary>Shapes the ground (+0x820).</summary>
    VertexTool vertex;
    /// <summary>The water level of the use; -1 until taken (+0x1054).</summary>
    int32_t waterLevel = -1;
};

/// <summary>
/// The tool that lays concrete or crops: a tile is flattened and shows one look of the texture (its textures stay as
/// they were); the alternate button takes the look off again.
/// </summary>
/// <remarks>0x1058 bytes, virtual table 0x004faa7c (14 slots).</remarks>
class ConcreteTool : public BrushTool
{
public:
    /// <summary>
    /// A tool laying <paramref name="texture"/> (concrete or crop) in look <paramref name="look"/> (the palette's
    /// variant, from 1).
    /// </summary>
    /// <remarks>0x004074a0.</remarks>
    ConcreteTool(uint8_t texture, int32_t look);

    /// <summary>Starts both actions, the painting's named after the texture's tile set.</summary>
    /// <remarks>0x004075b0.</remarks>
    bool begin(EditorMap& map) override;
    /// <summary>Merges the painting's records into the height action and gives that (null when empty).</summary>
    /// <remarks>0x00407710.</remarks>
    EditorAction* end() override;
    /// <summary>Lays the look on the tile under (<paramref name="x"/>, <paramref name="y"/>).</summary>
    /// <returns>Whether the library has the look; true off the map.</returns>
    /// <remarks>0x004077a0.</remarks>
    bool apply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary>Takes this texture's look off the tile under (<paramref name="x"/>, <paramref name="y"/>).</summary>
    /// <returns>true.</returns>
    /// <remarks>0x004077f0.</remarks>
    bool applyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary>Whether the tile under (<paramref name="x"/>, <paramref name="y"/>) can take the look; lights it up if so.</summary>
    /// <remarks>0x00407a00.</remarks>
    bool canApply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <returns>true.</returns>
    /// <remarks>0x00408010.</remarks>
    bool canApplyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary>Whether every selected tile can take the look.</summary>
    /// <remarks>0x00407a70.</remarks>
    bool hasTarget(EditorMap& map) override;
    /// <summary>
    /// Sets the selected vertices to their average elevation (truncated), then lays the look on every selected tile.
    /// </summary>
    /// <remarks>0x00407dc0.</remarks>
    bool applyToSelection(EditorMap& map, int32_t step) override;

private:
    /// <summary>Flattens tile (<paramref name="col"/>, <paramref name="row"/>) and gives it the look.</summary>
    /// <returns>Whether the library has the look.</returns>
    /// <remarks>0x00407af0.</remarks>
    bool layLook(EditorMap& map, int32_t col, int32_t row);
    /// <summary>
    /// Whether tile (<paramref name="col"/>, <paramref name="row"/>) can take a look: on the map, no corner at 0, no
    /// water, no spray-can object on its cells and no forest overlay.
    /// </summary>
    /// <remarks>0x00408020.</remarks>
    static bool canLay(EditorMap& map, int32_t col, int32_t row);

    /// <summary>Concrete or crop (+0x81c).</summary>
    uint8_t texture;
    /// <summary>Shapes the ground (+0x820).</summary>
    VertexTool vertex;
    /// <summary>The look, from 0 (+0x1054).</summary>
    int32_t look;
};

/// <summary>
/// The tool that makes cliffs: a tile is raised until it has corners 2 up and none 1 up, its high corners become cliff
/// and its low ones ground (dirt or grass, whichever the tile has more of), and the tiles around follow.
/// </summary>
/// <remarks>0x1060 bytes, virtual table 0x004fae30 (15 slots).</remarks>
class CliffTool : public TextureTool
{
public:
    /// <summary>A cliff tool.</summary>
    CliffTool();

    /// <summary>Forgets the ground texture and starts both actions.</summary>
    /// <returns>Whether both started.</returns>
    /// <remarks>0x00405c30.</remarks>
    bool begin(EditorMap& map) override;
    /// <summary>Merges the painting's records into the height action and gives that (null when empty).</summary>
    /// <remarks>0x00405d50.</remarks>
    EditorAction* end() override;
    /// <summary>Raises and paints the tile under (<paramref name="x"/>, <paramref name="y"/>).</summary>
    /// <returns>false off the map, or (the use undone) when a stranded object can't be removed.</returns>
    /// <remarks>0x00405c80.</remarks>
    bool apply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary>Lights the tile up; whether it has no water.</summary>
    /// <remarks>0x00405de0.</remarks>
    bool canApply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary>Whether no selected tile has water.</summary>
    /// <remarks>0x00406230.</remarks>
    bool hasTarget(EditorMap& map) override;
    /// <summary>Raises and paints every selected tile, row by row.</summary>
    /// <returns>false at the first tile that can't be raised.</returns>
    /// <remarks>0x00406150.</remarks>
    bool applyToSelection(EditorMap& map, int32_t step) override;
    /// <summary>
    /// Raises and paints the four tiles beside tile (<paramref name="col"/>, <paramref name="row"/>) that its cliff
    /// edges face (all four for a tile without one).
    /// </summary>
    /// <returns>true.</returns>
    /// <remarks>0x00406710.</remarks>
    bool matchNeighbours(EditorMap& map, int32_t col, int32_t row, FailedTiles& failed) override;
    /// <summary>
    /// <see cref="BrushTool::paintTerrain"/>, but a tile that takes the change is recorded even unchanged, and a changed
    /// one matches its own neighbours in turn; failed tiles aren't recorded.
    /// </summary>
    /// <remarks>0x00406430.</remarks>
    void paintTerrain(EditorMap& map, int32_t col, int32_t row, const uint8_t* textures, const int32_t* elevations,
                      FailedTiles& failed) override;
    /// <summary>
    /// Makes tile (<paramref name="col"/>, <paramref name="row"/>)'s corners 2 up cliff and its lowest ground (the
    /// ground spreading to the tiles sharing them), then fixes the tiles around.
    /// </summary>
    /// <returns>false left or above the map; true right or below it or when nothing changed.</returns>
    /// <remarks>0x00405ed0.</remarks>
    bool paintTile(EditorMap& map, int32_t col, int32_t row) override;

private:
    /// <summary>
    /// Raises tile (<paramref name="col"/>, <paramref name="row"/>) to a cliff shape unless it and the cliff tiles it
    /// leads to have one: corners 1 up go up 1 (or with no corner 2 up, corner 0 goes up 2), then a flat or saddle
    /// tile has its first lowest corner raised 2.
    /// </summary>
    /// <returns>false off the map.</returns>
    /// <remarks>0x004069a0.</remarks>
    bool raiseForCliff(EditorMap& map, int32_t col, int32_t row);
    /// <summary>
    /// Whether tile (<paramref name="col"/>, <paramref name="row"/>) and the cliff tiles its edges lead to all have a
    /// cliff shape.
    /// </summary>
    /// <remarks>0x00406c00.</remarks>
    static bool hasCliffShape(EditorMap& map, int32_t col, int32_t row);
    /// <summary>
    /// Whether tile (<paramref name="col"/>, <paramref name="row"/>) has corners only 0 or 2 up and isn't flat or a
    /// saddle; for <paramref name="first"/> or a corner shape, adds the tiles its edges face to
    /// <paramref name="pending"/> (false when the shape leads everywhere).
    /// </summary>
    /// <remarks>0x004065b0; true off the map.</remarks>
    static bool checkCliffShape(EditorMap& map, int32_t col, int32_t row, std::list<POINT>& pending, bool first);
    /// <summary>
    /// Gives corner <paramref name="corner"/> of tile (<paramref name="col"/>, <paramref name="row"/>) the ground
    /// texture and spreads it to the tiles sharing that vertex while that changes something.
    /// </summary>
    /// <returns>true.</returns>
    /// <remarks>0x00407060.</remarks>
    bool spreadGround(EditorMap& map, int32_t col, int32_t row, int32_t corner);
    /// <summary>
    /// Matches the tiles around (<paramref name="col"/>, <paramref name="row"/>), then gives each one no terrain tile
    /// shows cliff on its corners 2 up and ground on the rest, spreading the ground, and matches its neighbours.
    /// </summary>
    /// <returns>Whether every match succeeded.</returns>
    /// <remarks>0x00406db0.</remarks>
    bool fixCliffNeighbours(EditorMap& map, int32_t col, int32_t row);

    /// <summary>Shapes the ground (+0x820).</summary>
    VertexTool vertex;
    /// <summary>A corner this far up becomes cliff (+0x1054).</summary>
    int32_t highHeight = 2;
    /// <summary>A corner this far up becomes ground (+0x1058).</summary>
    int32_t lowHeight = 0;
    /// <summary>The ground texture of the use; <c>TEXTURE_NONE</c> until the first tile chooses it (+0x105c).</summary>
    uint8_t ground = TEXTURE_NONE;
};
