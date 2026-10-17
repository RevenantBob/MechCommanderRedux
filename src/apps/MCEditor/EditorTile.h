#pragma once

struct MapOverlay;

// A tile of the editor's map (0x00472970-0x00473380): its corner textures and elevations, the terrain tile that
// shows them and its overlay. Its source file name wasn't kept; the names are the port's.

/// <summary>The width of a tile in the map's coordinates: 79 in the views, 158 while objects are placed.</summary>
extern int32_t tileWorldWidth;
/// <summary>The height of a tile in the map's coordinates (set from <see cref="tileWorldWidth"/>).</summary>
extern int32_t tileWorldHeight;
/// <summary>Map coordinates per elevation step.</summary>
extern int32_t elevationScale;
/// <summary>The corner elevation of a new map's tiles (0 makes them water).</summary>
extern int32_t defaultElevation;

/// <summary>One tile of the map.</summary>
/// <remarks>
/// 0x34 bytes, no virtual table. The map keeps one per tile but the last row and column, so a tile's right and
/// bottom corners are its neighbours' left and top ones.
/// </remarks>
struct EditorTile
{
    /// <summary>
    /// A tile of the new-map texture and elevation (<see cref="defaultTexture"/>, <see cref="defaultElevation"/>) and a
    /// random terrain tile for them.
    /// </summary>
    EditorTile();
    /// <summary>Frees the overlay and counts the terrain tile's reference gone.</summary>
    ~EditorTile();
    /// <summary>A copy of <paramref name="other"/>, as <see cref="assign"/>.</summary>
    EditorTile(const EditorTile& other);
    EditorTile& operator=(const EditorTile& other);

    /// <summary>
    /// Copies <paramref name="other"/>'s textures, elevations, terrain tile (uncounted), overlay (a new copy) and
    /// <see cref="concIndex"/>/<see cref="cropIndex"/>; the flags stay.
    /// </summary>
    void assign(const EditorTile& other);
    /// <summary>The corners' heights above the lowest one, into <paramref name="heights"/>.</summary>
    /// <returns>Whether every corner is at most 2 above the lowest (a tile can show that).</returns>
    bool getHeights(uint8_t* heights) const;
    /// <summary>
    /// Draws the terrain tile's art at zoom level <paramref name="zoom"/> (0 full size, 1 middle, 2 smallest; fetched
    /// from the library on first use) into <paramref name="buffer"/> at (<paramref name="x"/>, <paramref name="y"/>),
    /// as <see cref="VFX_nTile_draw"/> with <paramref name="xlat"/> and <paramref name="outline"/>.
    /// </summary>
    /// <returns>What <see cref="VFX_nTile_draw"/> returns.</returns>
    /// <remarks>0x00451fd0, a method of the original's tile reference.</remarks>
    uint32_t drawArt(uint8_t* buffer, int32_t width, int32_t height, int32_t x, int32_t y, const uint8_t* xlat,
                     int32_t outline, int32_t zoom);

    /// <summary>
    /// Gives the corners <paramref name="newTextures"/> (<see cref="KEEP_TEXTURE"/> keeps a corner's) and takes a
    /// random terrain tile for them: rough shows as grass unless all four corners are rough, a corner at elevation 0
    /// is water.
    /// </summary>
    /// <returns>false, the tile keeping its terrain tile, when the library has none for the corners.</returns>
    /// <remarks>0x00472be0.</remarks>
    bool setTextures(const uint8_t* newTextures);
    /// <summary>
    /// <see cref="setTextures"/>, telling in <paramref name="changed"/> whether a corner's texture changed (or the
    /// tile stopped being a concrete or crop tile).
    /// </summary>
    /// <remarks>0x00472d60.</remarks>
    bool setTextures(const uint8_t* newTextures, bool* changed);
    /// <summary>
    /// Gives the corners <paramref name="newElevations"/> (<see cref="KEEP_ELEVATION"/> keeps a corner's) and takes a
    /// random terrain tile for them, water where a corner is at 0 (only for picking: the corner keeps its texture).
    /// </summary>
    /// <returns>false, the tile keeping its terrain tile, when a corner is more than 2 above the lowest or the library
    /// has no tile.</returns>
    /// <remarks>0x00472dc0.</remarks>
    bool setElevations(const int32_t* newElevations);
    /// <summary>
    /// <see cref="setTextures"/> and <see cref="setElevations"/>, telling in <paramref name="changed"/> whether either
    /// changed anything.
    /// </summary>
    /// <returns>Whether both found a terrain tile.</returns>
    /// <remarks>0x00472f20.</remarks>
    bool setTerrain(const uint8_t* newTextures, const int32_t* newElevations, bool* changed);
    /// <summary>
    /// Gives the tile an overlay of <paramref name="kinds"/> (per corner) and <paramref name="damaged"/>, or takes
    /// it off when every corner is <c>OVERLAY_NONE</c>.
    /// </summary>
    /// <returns>Whether the tile has the overlay (false after taking it off).</returns>
    /// <remarks>0x00472fd0.</remarks>
    bool setOverlay(const int32_t* kinds, bool damaged);
    /// <summary>Puts overlay kind <paramref name="kind"/> on corner <paramref name="corner"/>, making an overlay if needed.</summary>
    /// <remarks>0x004730d0.</remarks>
    void setOverlayCorner(int32_t corner, int32_t kind);
    /// <summary>
    /// Whether the library has an overlay for the tile's kinds with <paramref name="kind"/> on corner
    /// <paramref name="corner"/>; never on a tile with a corner more than 1 above the lowest.
    /// </summary>
    /// <remarks>0x00473190.</remarks>
    bool canSetOverlayCorner(int32_t corner, int32_t kind) const;
    /// <summary>Whether the library has an overlay of <paramref name="kinds"/> for the tile's corner heights.</summary>
    /// <remarks>0x00473260.</remarks>
    bool hasOverlayFor(const int32_t* kinds, bool damaged) const;

    /// <summary>
    /// Whether the tile shows <paramref name="texture"/>: a concrete or crop tile only that; else on a corner, water
    /// also on a corner at elevation 0.
    /// </summary>
    /// <remarks>0x00416430 (a tool function the editor also inlines).</remarks>
    bool hasTexture(uint8_t texture) const;

    /// <summary>A texture <see cref="setTextures"/> leaves as it is.</summary>
    static constexpr uint8_t KEEP_TEXTURE = 0xff;
    /// <summary>An elevation <see cref="setElevations"/> leaves as it is.</summary>
    static constexpr int32_t KEEP_ELEVATION = 0x7fffffff;

    /// <summary>
    /// The texture of each corner: the tile's own vertex (column, row), then (column + 1, row), (column, row + 1) and
    /// (column + 1, row + 1).
    /// </summary>
    uint8_t textures[4];
    /// <summary>The elevation of each corner, in the same order.</summary>
    int32_t elevations[4];
    /// <summary>The terrain tile that shows the textures and heights.</summary>
    int32_t tileId = -1;
    /// <summary>
    /// The terrain tile's art at each zoom level (the library's, not owned), fetched on first draw; a copied tile
    /// starts without.
    /// </summary>
    uint8_t* zoomArt[3] = {};
    /// <summary>The tile is selected (+0x24): drawn outlined again over its neighbours.</summary>
    bool selected = false;
    /// <summary>The brush tools' cursor is on the tile (+0x25): it is drawn brighter.</summary>
    bool highlighted = false;
    /// <summary>Within the cut-off corners of the map (set by the map's constructor).</summary>
    bool inCorner = false;
    /// <summary>The overlay, or null.</summary>
    MapOverlay* overlay = nullptr;
    /// <summary>
    /// The look of the concrete the tile shows (the tile library picks the concrete tile by it), -1 for none. A map
    /// read gives a concrete tile look 0.
    /// </summary>
    int32_t concIndex = -1;
    /// <summary>The look of the crops the tile shows, as <see cref="concIndex"/>; -1 for none.</summary>
    int32_t cropIndex = -1;
};
