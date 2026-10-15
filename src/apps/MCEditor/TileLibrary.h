#pragma once

#include <istream>

// The editor's terrain tile library (0x004733b0-0x00477210): which tile of the tile file shows which textures, corner
// heights and overlay, read from Teditor\Tiles.csv, and the tiles' art, mapped from the tile files and unpacked on
// demand. Its source file name wasn't kept; the names are the port's.

/// <summary>A terrain texture: the third and fourth columns of a Tiles.csv "Tile" row.</summary>
enum TerrainTexture : uint8_t
{
    TEXTURE_DIRT = 0,
    TEXTURE_GRASS = 1,
    TEXTURE_RUFF = 2,
    TEXTURE_MOUN = 3,
    TEXTURE_CLIFF = 4,
    TEXTURE_WATER = 5,
    TEXTURE_CONC = 6,
    TEXTURE_CROP = 7,
    /// <summary>A name <see cref="TileLibrary::textureIndex"/> doesn't know.</summary>
    TEXTURE_NONE = 0xff
};

/// <summary>Textures a tile set can be made of (the pure and transition set tables are this square).</summary>
inline constexpr int32_t TEXTURE_SLOTS = 8;

/// <summary>An overlay kind: the third and fourth columns of a Tiles.csv "Overlay" row.</summary>
enum OverlayKind : int32_t
{
    /// <summary>"forest", "dforest" and "noforest".</summary>
    OVERLAY_FOREST = 0,
    /// <summary>"wall", "dwall".</summary>
    OVERLAY_WALL = 1,
    OVERLAY_RUNWAY = 2,
    /// <summary>"lwall", "dlwall".</summary>
    OVERLAY_LIGHT_WALL = 3,
    /// <summary>"rbridge", "drbridge".</summary>
    OVERLAY_RAIL_BRIDGE = 4,
    /// <summary>"proad".</summary>
    OVERLAY_PAVED_ROAD = 5,
    /// <summary>"droad".</summary>
    OVERLAY_DIRT_ROAD = 6,
    /// <summary>"bridge", "dbridge".</summary>
    OVERLAY_BRIDGE = 7,
    OVERLAY_RAILROAD = 8,
    OVERLAY_ROUGH = 9,
    /// <summary>"hwall", "dhwall".</summary>
    OVERLAY_HEAVY_WALL = 10,
    OVERLAY_DOCK = 11,
    /// <summary>No overlay on that corner.</summary>
    OVERLAY_NONE = -1
};

/// <summary>Overlay kinds the overlay set table has room for.</summary>
inline constexpr int32_t OVERLAY_KINDS = 12;

/// <summary>Corner height combinations of a tile: each of its four corners 0, 1 or 2 above the lowest.</summary>
inline constexpr int32_t TILE_SHAPES = 81;
/// <summary>The ways two textures share a tile (Tiles.csv transition letters 'a'-'n').</summary>
inline constexpr int32_t TRANSITION_TYPES = 14;
/// <summary><see cref="TileLibrary::transitionType"/> for a tile of one texture.</summary>
inline constexpr uint8_t TRANSITION_NONE = 15;
/// <summary>Corner height combinations of an overlay: each corner 0 or 1.</summary>
inline constexpr int32_t OVERLAY_SHAPES = 16;
/// <summary>Which corners an overlay covers (<see cref="TileLibrary::overlayShape"/>).</summary>
inline constexpr int32_t OVERLAY_LAYOUTS = 15;
/// <summary>Rows of Tiles.csv the library reads: one per tile of the tile file.</summary>
inline constexpr int32_t TILE_COUNT = 0x2f0a;
/// <summary>The id of no tile.</summary>
inline constexpr int32_t NO_TILE = -1;
/// <summary>The tiles per zoom level: a tile's entries at the next zooms are this many and twice this many on.</summary>
inline constexpr int32_t TILE_ZOOM_STRIDE = 0xfae;

/// <summary>One tile of the tile file: where it is and its unpacked art, with a count of its users.</summary>
/// <remarks>0x14 bytes. The names are the port's.</remarks>
struct TileEntry
{
    /// <summary>Its packet table entry: the offset in the low 29 bits, the storage in the top 3.</summary>
    uint32_t offset;
    /// <summary>The packet's size in the file.</summary>
    uint32_t size;
    /// <summary>
    /// Counted up by <see cref="TileLibrary::getTileData"/> and down when a tile reference goes away; references are
    /// copied without counting, so it mostly runs negative.
    /// </summary>
    int32_t refCount;
    /// <summary>The size of the unpacked art, for a packed tile.</summary>
    uint32_t unpackedSize;
    /// <summary>The unpacked art; null until first used.</summary>
    uint8_t* data;
};

/// <summary>A tile set's base: a virtual destructor and the chance of drawing the first variant.</summary>
/// <remarks>The vtable at 0x00502644. The names are the port's.</remarks>
struct TileSet
{
    virtual ~TileSet() = default;
    /// <summary>
    /// How likely (in percent) <see cref="TileLibrary::pickTile"/> draws the first tile of a shape rather than one of
    /// the others: 80 for a pure set; a transition set never sets it (EB-035).
    /// </summary>
    int32_t probability = 0;
};

/// <summary>The tiles of one texture, by corner heights.</summary>
/// <remarks>0x618 bytes, the vtable at 0x00502640.</remarks>
struct PureTileSet : TileSet
{
    /// <summary>Releases its tiles.</summary>
    ~PureTileSet() override;
    /// <summary>The tile ids of each shape, indexed by <see cref="TileLibrary::shapeIndex"/>.</summary>
    std::vector<int32_t> shapes[TILE_SHAPES];
    /// <summary>The texture's name, string 8000 + its index (+50 for the GTiles set).</summary>
    char name[0x100] = {};
};

/// <summary>The tiles that blend one texture into another, by corner heights and transition.</summary>
/// <remarks>0x46e8 bytes, the vtable at 0x0050263c.</remarks>
struct TransitionTileSet : TileSet
{
    /// <summary>Releases its tiles.</summary>
    ~TransitionTileSet() override;
    /// <summary>The tile ids, indexed by shape * <see cref="TRANSITION_TYPES"/> + transition.</summary>
    std::vector<int32_t> shapes[TILE_SHAPES * TRANSITION_TYPES];
};

/// <summary>The overlays of one kind (intact or damaged), by corner heights and corners covered.</summary>
/// <remarks>0x1000 bytes. The names are the port's.</remarks>
struct OverlaySet
{
    /// <summary>The kind's name, string 9000 + its index (+50 for the GTiles set).</summary>
    char name[0x100] = {};
    /// <summary>The tile ids, indexed by heights * <see cref="OVERLAY_LAYOUTS"/> + layout.</summary>
    std::vector<int32_t> shapes[OVERLAY_SHAPES * OVERLAY_LAYOUTS];
};

/// <summary>An overlay that joins two kinds (a road meeting another): the kind on each corner.</summary>
/// <remarks>0x20 bytes. The names are the port's.</remarks>
struct MixedOverlay
{
    /// <summary>The kind on each corner, <see cref="OVERLAY_NONE"/> where it has none.</summary>
    int32_t kinds[4];
    /// <summary>The tile.</summary>
    int32_t tileId;
};

/// <summary>A range of tile ids that holds the tiles of one texture pair.</summary>
/// <remarks>0x10 bytes, in a static table (0x008ca5b4) filled by a static initializer (0x00477210).</remarks>
struct TileRange
{
    /// <summary>The first tile id; the range ends where the next one starts.</summary>
    int32_t start;
    /// <summary>The range's group number.</summary>
    int32_t group;
    /// <summary>The first and second texture.</summary>
    int32_t textureA;
    int32_t textureB;
};

/// <summary>The editor's terrain tile library.</summary>
/// <remarks>
/// 0x298 bytes. One instance, a static of the document; <see cref="tileLibrary"/> points to it. Every corner array is
/// in the order the library keeps heights: the CSV's first, second, fourth and third corner columns.
/// </remarks>
class TileLibrary
{
public:
    /// <summary>An empty library that needs loading; becomes <see cref="tileLibrary"/>.</summary>
    TileLibrary();
    /// <summary>Frees everything and clears <see cref="tileLibrary"/>.</summary>
    ~TileLibrary();
    TileLibrary(const TileLibrary&) = delete;
    TileLibrary& operator=(const TileLibrary&) = delete;

    /// <summary>Frees the sets and the tiles' art, empties the lists and closes the tile files.</summary>
    void destroy();
    /// <summary>Forgets the sets and files without freeing them, as after <see cref="destroy"/>.</summary>
    void reset();

    /// <summary>
    /// When <see cref="reloadNeeded"/>, empties the library and loads it again: maps the tile files
    /// <paramref name="tile90File"/> (Tiles90.pak: tiles 0-4013, whose packet table it reads) and
    /// <paramref name="tileFile"/> (Tiles.pak, whose second half holds the next 4014), and reads one row of
    /// <paramref name="csvFile"/> per tile into the sets.
    /// </summary>
    /// <returns>Whether it loaded; true when nothing needed loading.</returns>
    bool init(const char* tile90File, const char* tileFile, const char* csvFile);

    /// <summary>The art of tile <paramref name="tileId"/> from Tiles90.pak, unpacked on first use; counts a use.</summary>
    /// <returns>Null for <see cref="NO_TILE"/> or when it can't be unpacked.</returns>
    uint8_t* getTileData(int32_t tileId);
    /// <summary>
    /// The art of tile <paramref name="tileId"/> at the middle zoom, from the second half of Tiles.pak (entry
    /// <c>tileId + TILE_ZOOM_STRIDE</c>), unpacked on first use; counts a use.
    /// </summary>
    /// <returns>Null when it can't be unpacked.</returns>
    /// <remarks>
    /// Original behaviour (EB-048): a tile stored unpacked is copied from the start of the file, not from its packet
    /// (every retail tile is packed).
    /// </remarks>
    uint8_t* getMediumTileData(int32_t tileId);
    /// <summary>
    /// The art of tile <paramref name="tileId"/> at the smallest zoom (entry <c>tileId + 2 * TILE_ZOOM_STRIDE</c>),
    /// made on first use: the middle zoom's art drawn into a 160 x 120 buffer, every other pixel of every other row
    /// kept, and the 80 x 60 result encoded again (<see cref="VFX_nTile_scan"/>, or for an
    /// <paramref name="overlay"/> <see cref="VFX_nShape_scan"/> in a one-shape table); counts a use.
    /// </summary>
    /// <returns>Null when the encoding fails.</returns>
    uint8_t* getSmallTileData(int32_t tileId, bool overlay);

    /// <summary>
    /// Reads the next row of Tiles.csv as tile <paramref name="tileId"/>'s and files it: a "Tile" row into a pure or
    /// transition set, an "Overlay" row into an overlay set or the mixed overlays.
    /// </summary>
    /// <param name="textureA">The row's first texture (or overlay kind).</param>
    /// <param name="textureB">Its second texture (or overlay kind).</param>
    /// <param name="corners">The four corner columns, in the library's order.</param>
    /// <param name="transition">The transition letter less 'a', 0xff for "NULL".</param>
    /// <returns>Whether the row was a "Tile" or "Overlay" row.</returns>
    bool parseRow(int32_t tileId, std::istream& stream, int32_t* textureA, int32_t* textureB, int32_t* corners,
                  int8_t* transition);

    /// <summary>
    /// Files tile <paramref name="tileId"/>: into the pure set of <paramref name="textureA"/> when both textures are the
    /// same, else into the transition set of the pair under <paramref name="transition"/> and into the reverse pair's
    /// set under the mirrored letter.
    /// </summary>
    /// <returns>false when the mirrored letter isn't one (the tile is filed one way only).</returns>
    bool addTile(int32_t tileId, int32_t textureA, int32_t textureB, int32_t height0, int32_t height1, int32_t height2,
                 int32_t height3, int8_t transition);

    /// <summary>
    /// Files overlay tile <paramref name="tileId"/>: into the overlay set of <paramref name="kindA"/> when both kinds
    /// are the same, else into the mixed overlays with the corner kinds the rest of the row
    /// (<paramref name="rest"/>) gives.
    /// </summary>
    bool addOverlay(const char* rest, int32_t tileId, int32_t kindA, int32_t kindB, int32_t height0, int32_t height1,
                    int32_t height2, int32_t height3, int8_t transition, bool damaged);

    /// <summary>
    /// A tile for <paramref name="textures"/> and <paramref name="heights"/> (corners above the lowest):
    /// variant <paramref name="variant"/> of the matching ones, or a random one for -1.
    /// </summary>
    /// <returns>The tile id; <see cref="NO_TILE"/> when there is none (more than two textures, or no such tile).</returns>
    int32_t pickTile(const uint8_t* textures, const uint8_t* heights, int32_t variant);

    /// <summary>
    /// An overlay tile for <paramref name="kinds"/> (per corner) and <paramref name="heights"/>, random among the
    /// matches; for two kinds, the mixed overlay with exactly those corners.
    /// </summary>
    /// <returns>Where its id is kept, or null.</returns>
    const int32_t* findOverlayTile(const int32_t* kinds, const uint8_t* heights, bool damaged);

    /// <summary>
    /// Draws a flat tile of <paramref name="texture"/> (variant <paramref name="variant"/> of its pure set's flat
    /// tiles; for cliff, which has no pure set, the first cliff-to-grass tile of corners 2, 2, 0, 0 and transition 1)
    /// into a new <paramref name="info"/>-shaped DIB section on <paramref name="dc"/>, centred down by half its height.
    /// </summary>
    /// <returns>The bitmap, or null when it can't be made.</returns>
    /// <remarks>0x00473dd0.</remarks>
    HBITMAP drawTextureThumbnail(HDC dc, uint8_t texture, BITMAPINFO* info, int32_t variant);
    /// <summary>
    /// Draws the first overlay of kind <paramref name="kind"/> with the layout of <paramref name="corners"/> (when
    /// null: four corners for forest and rough, else layout 10) on flat ground at 128 x 128, shrunk into a new
    /// <paramref name="info"/>-shaped DIB section on <paramref name="dc"/>.
    /// </summary>
    /// <returns>The bitmap, or null when the kind has no such overlay.</returns>
    /// <remarks>0x00473ea0.</remarks>
    HBITMAP drawOverlayThumbnail(HDC dc, int32_t kind, BITMAPINFO* info, const int32_t* corners);

    /// <summary>Which overlay kinds tile <paramref name="tileId"/> puts on its corners.</summary>
    /// <returns>Whether the tile is an overlay.</returns>
    bool findOverlay(int32_t tileId, int32_t* kinds, bool* damaged);

    /// <summary>The textures and corner heights of tile <paramref name="tileId"/>.</summary>
    /// <returns>Whether the tile is in its range's set.</returns>
    bool getTileShape(int32_t tileId, uint8_t* textures, uint8_t* heights);

    /// <summary>
    /// Switches between the planet (Tiles) and GTiles tile sets; a change makes <see cref="init"/> load again.
    /// </summary>
    void setGTiles(bool gTiles);

    /// <summary>Counts a reference to tile <paramref name="tileId"/> gone (<see cref="TileEntry::refCount"/>).</summary>
    void releaseTile(int32_t tileId);

    /// <summary>
    /// Textures for a tile next to one painted <paramref name="texture"/> that no terrain tile shows: the corners of
    /// <paramref name="texture"/> keep it, the others all take one texture the library blends with it (the
    /// default texture first, else the lowest one; dirt when none qualifies). A cliff or water corner, or painting
    /// either, goes to <see cref="resolveCliff"/> or <see cref="resolveWater"/>.
    /// </summary>
    /// <param name="textures">The tile's textures; the corners not of <paramref name="texture"/> are changed too.</param>
    /// <param name="out">The textures to give the tile.</param>
    /// <returns>false when the result has two textures on opposite corners (no tile shows that).</returns>
    /// <remarks>0x004746b0; the original takes a fifth argument it doesn't use.</remarks>
    bool resolveTextures(uint8_t texture, uint8_t* textures, const uint8_t* heights, uint8_t* out);
    /// <summary>
    /// <see cref="resolveTextures"/> where cliff is involved: painting cliff, the corners 2 above the lowest become
    /// cliff and the lowest ones a ground texture (all of it ground when a corner is 1 up); painting anything else,
    /// all ground. The ground is the tile's one other texture, or of several the one the library blends with
    /// <paramref name="texture"/> (the default first, else the lowest), and only dirt or grass, else the default.
    /// </summary>
    /// <returns>true.</returns>
    /// <remarks>0x004751f0.</remarks>
    bool resolveCliff(uint8_t texture, const uint8_t* textures, const uint8_t* heights, uint8_t* out);
    /// <summary>
    /// <see cref="resolveTextures"/> where water is involved: the lowest corners water, the ones 1 up a shore
    /// texture (<paramref name="texture"/> when it is dirt or grass, else the default), all shore when a corner is
    /// higher, and all shore too when the library has a tile for that water.
    /// </summary>
    /// <returns>true.</returns>
    /// <remarks>0x00475350.</remarks>
    bool resolveWater(uint8_t texture, const uint8_t* heights, uint8_t* out);

    /// <summary>The texture of a corner of a new map (<see cref="defaultTexture"/>).</summary>
    static uint8_t getDefaultTexture();

    /// <summary>Index of a texture name; <see cref="TEXTURE_NONE"/> when unknown.</summary>
    static uint8_t textureIndex(const char* name);
    /// <summary>Index of an overlay name, and whether it's the damaged look; 0xff when unknown.</summary>
    static uint8_t overlayIndex(const char* name, bool* damaged);
    /// <summary>
    /// How <paramref name="textures"/> (per corner) split between two textures, as a transition index;
    /// <see cref="TRANSITION_NONE"/> when all four are the same.
    /// </summary>
    static uint8_t transitionType(const uint8_t* textures);
    /// <summary>Which corners of <paramref name="kinds"/> have an overlay, as a layout index; -1 for none.</summary>
    static int8_t overlayShape(const int32_t* kinds);
    /// <summary>Sets <paramref name="kinds"/> to <paramref name="kind"/> on the corners of layout <paramref name="shape"/>.</summary>
    static bool expandOverlayShape(int32_t kind, int32_t shape, int32_t* kinds);
    /// <summary>
    /// Sets <paramref name="textures"/> to <paramref name="textureA"/>, with <paramref name="textureB"/> on the
    /// corners transition <paramref name="transition"/> gives it.
    /// </summary>
    /// <returns>false for a value that isn't a transition (the textures are then all the first).</returns>
    static bool expandTransition(uint8_t textureA, uint8_t textureB, uint8_t transition, uint8_t* textures);
    /// <summary>The group number of the range holding <paramref name="tileId"/>; -1 when none does.</summary>
    static int32_t tileGroup(int32_t tileId);
    /// <summary>The index of a shape: the corners as base-3 digits, the first most significant.</summary>
    static int32_t shapeIndex(const uint8_t* heights);

    /// <summary>The tile set path or name the views show (empty here).</summary>
    char name[0x100] = {};
    /// <summary>The pure sets on the diagonal, the transition sets elsewhere: [first texture][second texture].</summary>
    TileSet* sets[TEXTURE_SLOTS][TEXTURE_SLOTS] = {};
    /// <summary>The overlay sets: [kind][damaged].</summary>
    OverlaySet* overlays[OVERLAY_KINDS][2] = {};
    /// <summary>The overlays that join two kinds.</summary>
    std::vector<MixedOverlay> mixedOverlays;
    /// <summary>Every tile of the tile file, by id.</summary>
    std::vector<TileEntry> tiles;
    /// <summary>The last <see cref="init"/> read every row.</summary>
    bool loaded = false;
    /// <summary>A load has finished.</summary>
    bool initialized = false;
    /// <summary>The tile set changed since the last <see cref="init"/>.</summary>
    bool reloadNeeded = true;

private:
    /// <summary>The mapping of Tiles90.pak, which the art is read from.</summary>
    HANDLE tile90Mapping = nullptr;
    /// <summary>Tiles90.pak.</summary>
    HANDLE tile90FileHandle = nullptr;
    /// <summary>The mapping of Tiles.pak.</summary>
    HANDLE tileMapping = nullptr;
    /// <summary>Tiles.pak.</summary>
    HANDLE tileFileHandle = nullptr;
};

/// <summary>The editor's tile library (set by its constructor).</summary>
extern TileLibrary* tileLibrary;
/// <summary>
/// Added to the string ids of the set names: 0 for the planet (Tiles) set, 50 for the GTiles set
/// (<see cref="TileLibrary::setGTiles"/>).
/// </summary>
extern int32_t tileSetStringOffset;
/// <summary>The texture of the corners of a new map's tiles (grass, unless the new map is set to dirt).</summary>
extern uint8_t defaultTexture;
/// <summary>The id ranges of the texture pairs.</summary>
extern const TileRange TILE_RANGES[];
