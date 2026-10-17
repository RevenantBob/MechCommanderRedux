#pragma once

#include <list>

#include "EditorAction.h"
#include "EditorTool.h"

struct EditorTile;

// The tools that paint the terrain tile by tile (virtual table 0x004fadbc and the classes derived from it): the
// textures, and through the vertex tool the elevations and overlays. Their source file name wasn't kept; the names
// are the port's.

/// <summary>The tiles a paint couldn't give a terrain tile, as they were before it.</summary>
using FailedTiles = std::list<TileAction::TileRecord>;

/// <summary>
/// A tool that paints tiles under its cursor, recording each changed tile's old state in a <see cref="TileAction"/>,
/// and lights up the tile under the cursor.
/// </summary>
/// <remarks>0x81c bytes, virtual table 0x004fadbc (14 slots).</remarks>
class BrushTool : public CursorTool
{
public:
    /// <summary>
    /// A brush whose cursor is bitmap <paramref name="bitmapId"/> with its hot spot at (<paramref name="hotX"/>,
    /// <paramref name="hotY"/>); with <paramref name="highlight"/> the tile under the cursor lights up.
    /// </summary>
    /// <remarks>0x004121d0.</remarks>
    BrushTool(bool highlight, UINT bitmapId, int32_t hotX, int32_t hotY);
    /// <summary>Takes the light off the last tile under the cursor.</summary>
    /// <remarks>0x00412230. The action of a use not ended isn't deleted.</remarks>
    ~BrushTool() override;

    /// <remarks>Pure again here (virtual table slot 1).</remarks>
    bool begin(EditorMap& map) override = 0;
    /// <summary>Takes the action of the use.</summary>
    /// <returns>The action, or null (deleted) when no tile changed.</returns>
    /// <remarks>0x00412400.</remarks>
    EditorAction* end() override;
    /// <returns>true.</returns>
    /// <remarks>0x004123f0.</remarks>
    bool apply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary>With the highlight on, lights up the tile under (<paramref name="x"/>, <paramref name="y"/>).</summary>
    /// <returns>true.</returns>
    /// <remarks>0x004132d0.</remarks>
    bool canApply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary><see cref="BrushTool::canApply"/>.</summary>
    /// <remarks>0x00413360.</remarks>
    bool canApplyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;

    /// <summary>Applies the tool to every selected tile of <paramref name="map"/> (the palette's buttons in select mode).</summary>
    /// <returns>Whether it applied everywhere.</returns>
    /// <remarks>Virtual table slot 11.</remarks>
    virtual bool applyToSelection(EditorMap& map, int32_t step) = 0;
    /// <summary>
    /// Gives the eight tiles around tile (<paramref name="col"/>, <paramref name="row"/>) its textures on the corners
    /// they share with it (water where a corner is at elevation 0), through <see cref="paintTerrain"/>.
    /// </summary>
    /// <returns>true.</returns>
    /// <remarks>Virtual table slot 12 (0x00412610). The tile must be on the map (the original reads a null one).</remarks>
    virtual bool matchNeighbours(EditorMap& map, int32_t col, int32_t row, FailedTiles& failed);
    /// <summary>
    /// Gives tile (<paramref name="col"/>, <paramref name="row"/>) <paramref name="textures"/> and
    /// <paramref name="elevations"/> (<see cref="EditorTile::setTerrain"/>), recording it when it changed. A tile no
    /// terrain tile shows is recorded anyway and added to <paramref name="failed"/>, as it was before.
    /// </summary>
    /// <remarks>Virtual table slot 13 (0x00412430). Nothing happens off the map.</remarks>
    virtual void paintTerrain(EditorMap& map, int32_t col, int32_t row, const uint8_t* textures,
                              const int32_t* elevations, FailedTiles& failed);

protected:
    /// <summary>
    /// Starts the action of a use on <paramref name="map"/>, named by string <paramref name="nameId"/> (empty when
    /// there is none), deleting one not ended.
    /// </summary>
    /// <returns>true.</returns>
    /// <remarks>0x00412250.</remarks>
    bool startAction(EditorMap& map, UINT nameId);

    /// <summary>The action of the use (+0x810).</summary>
    TileAction* action = nullptr;
    /// <summary>The tile under the cursor lights up (+0x814).</summary>
    bool highlight;
    /// <summary>The tile lit up last (+0x818).</summary>
    EditorTile* highlightedTile = nullptr;
};

/// <summary>
/// The tool that paints a texture on the tiles: their corners above elevation 0 take it, and the tiles around blend
/// into it. The palette's dirt, grass, rough and mountain buttons make one.
/// </summary>
/// <remarks>0x820 bytes, virtual table 0x004fad80 (15 slots).</remarks>
class TextureTool : public BrushTool
{
public:
    /// <summary>A tool painting <paramref name="texture"/>, with cursor <paramref name="bitmapId"/>.</summary>
    /// <remarks>0x00411600.</remarks>
    TextureTool(uint8_t texture, UINT bitmapId);

    /// <summary>Starts the action, named after the texture.</summary>
    /// <returns>true.</returns>
    /// <remarks>0x00411660.</remarks>
    bool begin(EditorMap& map) override;
    /// <summary>Takes the action of the use.</summary>
    /// <returns>The action, or null (deleted) when no tile changed.</returns>
    /// <remarks>0x00411800.</remarks>
    EditorAction* end() override;
    /// <summary>Paints the tile under (<paramref name="x"/>, <paramref name="y"/>) (<see cref="paintTile"/>).</summary>
    /// <returns>What the paint returns; true off the map.</returns>
    /// <remarks>0x004117a0.</remarks>
    bool apply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary><see cref="apply"/> with the default texture.</summary>
    /// <remarks>0x00412160.</remarks>
    bool applyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary>
    /// Whether the tile under (<paramref name="x"/>, <paramref name="y"/>) takes the texture: mountain only where
    /// <see cref="mountainFits"/> (and then the tile lights up), anything else where a corner is above elevation 0.
    /// </summary>
    /// <remarks>0x00411dd0.</remarks>
    bool canApply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary><see cref="canApply"/> with the default texture.</summary>
    /// <remarks>0x00411ec0.</remarks>
    bool canApplyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary>
    /// Whether the selection takes the texture: for mountain, every selected tile fits it; else some selected tile
    /// has a corner above elevation 0.
    /// </summary>
    /// <remarks>0x00411f10.</remarks>
    bool hasTarget(EditorMap& map) override;
    /// <summary>Paints every selected tile, row by row.</summary>
    /// <returns>Whether every paint succeeded.</returns>
    /// <remarks>0x00411830.</remarks>
    bool applyToSelection(EditorMap& map, int32_t step) override;

    /// <summary>
    /// Paints the texture on tile (<paramref name="col"/>, <paramref name="row"/>)'s corners above elevation 0 and
    /// blends the tiles around into it (<see cref="fixNeighbours"/>). A concrete or crop tile becomes a plain one
    /// first.
    /// </summary>
    /// <returns>false off the map or when a tile around can't be blended; true when nothing needed painting.</returns>
    /// <remarks>Virtual table slot 14 (0x004118e0).</remarks>
    virtual bool paintTile(EditorMap& map, int32_t col, int32_t row);

    /// <summary>
    /// Whether mountain fits tile (<paramref name="col"/>, <paramref name="row"/>): no corner at elevation 0, and with
    /// <paramref name="checkAround"/> and a corner at 1, the same for the eight tiles around (true off the map).
    /// </summary>
    /// <remarks>0x00412060.</remarks>
    static bool mountainFits(EditorMap& map, int32_t col, int32_t row, bool checkAround);

protected:
    /// <summary>
    /// Makes the tiles around tile (<paramref name="col"/>, <paramref name="row"/>) take its textures
    /// (<see cref="matchNeighbours"/>), then gives each one no terrain tile shows other textures
    /// (<see cref="TileLibrary::resolveTextures"/>) and matches its own neighbours to it in turn, or failing that paints
    /// it.
    /// </summary>
    /// <returns>Whether every tile found a terrain tile.</returns>
    /// <remarks>0x00411b00.</remarks>
    bool fixNeighbours(EditorMap& map, int32_t col, int32_t row);

    /// <summary>The texture painted (+0x81c).</summary>
    uint8_t texture;
};
