#include "stdafx.h"
#include "TileLibrary.h"
#include "EditorUtil.h"
#include "lib/lzdecomp.h"
#include "vfx/vfxnshape.h"
#include "vfx/vfxtile.h"

#include <sstream>

TileLibrary* tileLibrary = nullptr;
int32_t tileSetStringOffset = 0;
uint8_t defaultTexture = TEXTURE_GRASS;

// Original behaviour: the editor fills this table in a static initializer (0x00477210); the last row closes the
// table, its range running to the (unset) start of the next.
const TileRange TILE_RANGES[] = {{0, 0, TEXTURE_DIRT, TEXTURE_DIRT},
                                 {79, 1, TEXTURE_GRASS, TEXTURE_GRASS},
                                 {158, 2, TEXTURE_RUFF, TEXTURE_RUFF},
                                 {237, 3, TEXTURE_MOUN, TEXTURE_MOUN},
                                 {316, 4, TEXTURE_GRASS, TEXTURE_DIRT},
                                 {395, 5, TEXTURE_GRASS, TEXTURE_DIRT},
                                 {474, 6, TEXTURE_GRASS, TEXTURE_DIRT},
                                 {553, 7, TEXTURE_GRASS, TEXTURE_DIRT},
                                 {632, 8, TEXTURE_GRASS, TEXTURE_DIRT},
                                 {711, 9, TEXTURE_GRASS, TEXTURE_DIRT},
                                 {790, 10, TEXTURE_GRASS, TEXTURE_DIRT},
                                 {869, 11, TEXTURE_GRASS, TEXTURE_DIRT},
                                 {948, 12, TEXTURE_GRASS, TEXTURE_DIRT},
                                 {1027, 13, TEXTURE_GRASS, TEXTURE_DIRT},
                                 {1106, 14, TEXTURE_GRASS, TEXTURE_DIRT},
                                 {1185, 15, TEXTURE_GRASS, TEXTURE_DIRT},
                                 {1264, 16, TEXTURE_GRASS, TEXTURE_MOUN},
                                 {1343, 17, TEXTURE_GRASS, TEXTURE_MOUN},
                                 {1422, 18, TEXTURE_GRASS, TEXTURE_MOUN},
                                 {1501, 19, TEXTURE_GRASS, TEXTURE_MOUN},
                                 {1580, 20, TEXTURE_GRASS, TEXTURE_MOUN},
                                 {1659, 21, TEXTURE_GRASS, TEXTURE_MOUN},
                                 {1738, 22, TEXTURE_GRASS, TEXTURE_MOUN},
                                 {1817, 23, TEXTURE_GRASS, TEXTURE_MOUN},
                                 {1896, 24, TEXTURE_GRASS, TEXTURE_MOUN},
                                 {1975, 25, TEXTURE_GRASS, TEXTURE_MOUN},
                                 {2054, 26, TEXTURE_GRASS, TEXTURE_MOUN},
                                 {2133, 27, TEXTURE_GRASS, TEXTURE_MOUN},
                                 {2212, 28, TEXTURE_DIRT, TEXTURE_MOUN},
                                 {2291, 29, TEXTURE_DIRT, TEXTURE_MOUN},
                                 {2370, 30, TEXTURE_DIRT, TEXTURE_MOUN},
                                 {2449, 31, TEXTURE_DIRT, TEXTURE_MOUN},
                                 {2528, 32, TEXTURE_DIRT, TEXTURE_MOUN},
                                 {2607, 33, TEXTURE_DIRT, TEXTURE_MOUN},
                                 {2686, 34, TEXTURE_DIRT, TEXTURE_MOUN},
                                 {2765, 35, TEXTURE_DIRT, TEXTURE_MOUN},
                                 {2844, 36, TEXTURE_DIRT, TEXTURE_MOUN},
                                 {2923, 37, TEXTURE_DIRT, TEXTURE_MOUN},
                                 {3002, 38, TEXTURE_DIRT, TEXTURE_MOUN},
                                 {3081, 39, TEXTURE_DIRT, TEXTURE_MOUN},
                                 {3160, 40, TEXTURE_CLIFF, TEXTURE_GRASS},
                                 {3182, 41, TEXTURE_CLIFF, TEXTURE_DIRT},
                                 {3204, 42, TEXTURE_WATER, TEXTURE_WATER},
                                 {3208, 43, TEXTURE_GRASS, TEXTURE_WATER},
                                 {3222, 44, TEXTURE_DIRT, TEXTURE_WATER},
                                 {3236, 45, TEXTURE_GRASS, TEXTURE_WATER},
                                 {3248, 46, TEXTURE_DIRT, TEXTURE_WATER},
                                 {3272, 46, TEXTURE_DIRT, TEXTURE_WATER},
                                 {3296, 45, TEXTURE_GRASS, TEXTURE_WATER},
                                 {3301, 53, TEXTURE_GRASS, TEXTURE_WATER},
                                 {3316, 54, TEXTURE_DIRT, TEXTURE_WATER},
                                 {3328, 62, TEXTURE_CONC, TEXTURE_CONC},
                                 {3346, 62, TEXTURE_CONC, TEXTURE_CONC},
                                 {3358, 53, TEXTURE_GRASS, TEXTURE_WATER},
                                 {3360, 44, TEXTURE_DIRT, TEXTURE_WATER},
                                 {3376, 43, TEXTURE_GRASS, TEXTURE_WATER},
                                 {3428, 63, TEXTURE_CROP, TEXTURE_CROP},
                                 {3430, 63, -1, -1},
                                 {0, 0, 0, 0}};

namespace
{
    /// <summary>The bytes the editor copies of a BITMAPINFO with 256 colours.</summary>
    inline constexpr size_t BITMAPINFO_256_SIZE = 0x428;

    /// <summary>A little-endian 32-bit value at <paramref name="p"/>.</summary>
    int32_t read32(const uint8_t* p)
    {
        int32_t value;
        std::memcpy(&value, p, sizeof(value));
        return value;
    }

    /// <summary>The rows of <see cref="TILE_RANGES"/> the shape lookup searches (the last closes the table).</summary>
    inline constexpr int32_t SEARCHED_RANGES = 58;
    /// <summary>The top 3 bits of a packet table entry give the packet's storage; the rest is its offset.</summary>
    inline constexpr uint32_t PACKET_OFFSET_MASK = 0x1fffffff;
    /// <summary>Packet storage bit: LZ-packed, after a word giving its unpacked size.</summary>
    inline constexpr uint32_t PACKET_PACKED = 0x40000000;
    /// <summary>The first word of a packet file.</summary>
    inline constexpr uint32_t PACKET_FILE_MAGIC = 0xfeedface;
    /// <summary>Tile n's packet entry is word n + 2 of the tile file (packet 0 isn't a tile).</summary>
    inline constexpr int32_t FIRST_TILE_ENTRY = 2;
    /// <summary>A line of Tiles.csv.</summary>
    inline constexpr int32_t CSV_LINE_SIZE = 0x200;
    /// <summary>A field of Tiles.csv.</summary>
    inline constexpr int32_t CSV_FIELD_SIZE = 0x100;
    /// <summary>The string ids of the texture and overlay names (plus <see cref="tileSetStringOffset"/>).</summary>
    inline constexpr int32_t TEXTURE_NAME_STRINGS = IDS_DIRT;
    inline constexpr int32_t OVERLAY_NAME_STRINGS = IDS_FOREST;
    /// <summary>The chance (percent) a pure set gives its first tile.</summary>
    inline constexpr int32_t PURE_SET_PROBABILITY = 0x50;
    /// <summary><see cref="tileSetStringOffset"/> for the GTiles set.</summary>
    inline constexpr int32_t GTILES_STRING_OFFSET = 0x32;
    /// <summary>An overlay row's transition for "NULL": all four corners.</summary>
    inline constexpr int32_t OVERLAY_LAYOUT_ALL = 4;
    /// <summary>Overlay rows give letters 'p'-'z' to layouts 4-14.</summary>
    inline constexpr int8_t OVERLAY_LETTER_P = 15;
    inline constexpr int8_t OVERLAY_LETTER_Z = 25;
    inline constexpr int8_t OVERLAY_LETTER_OFFSET = 11;
    /// <summary>A layout <see cref="TileLibrary::addOverlay"/> doesn't know.</summary>
    inline constexpr int32_t OVERLAY_LAYOUT_NONE = 0xff;

    /// <summary>Reads the next comma-separated field of <paramref name="p"/> into <paramref name="field"/>.</summary>
    const char* nextField(const char* p, char* field)
    {
        int32_t length = CSV_FIELD_SIZE;
        getCsvField(p, ',', field, &length);
        return p + length;
    }

    /// <summary>The letter a transition is seen as from the other texture ('a'-'n'), or 0 for none.</summary>
    char mirrorTransition(char letter)
    {
        switch (letter)
        {
            case 'A':
                return 'I';
            case 'B':
                return 'F';
            case 'C':
                return 'J';
            case 'D':
                return 'H';
            case 'E':
                return 'K';
            case 'F':
                return 'B';
            case 'G':
                return 'L';
            case 'H':
                return 'D';
            case 'I':
                return 'A';
            case 'J':
                return 'C';
            case 'K':
                return 'E';
            case 'L':
                return 'G';
            case 'M':
                return 'N';
            case 'N':
                return 'M';
            default:
                return 0;
        }
    }

    /// <summary>A Tiles.csv row's overlay layout from its transition letter.</summary>
    int32_t overlayLayout(int8_t transition)
    {
        if (transition == -1)
        {
            return OVERLAY_LAYOUT_ALL;
        }

        if (transition >= 0 && transition <= 3)
        {
            return transition;
        }

        if (transition >= OVERLAY_LETTER_P && transition <= OVERLAY_LETTER_Z)
        {
            return transition - OVERLAY_LETTER_OFFSET;
        }

        return OVERLAY_LAYOUT_NONE;
    }

    /// <summary>Releases the tiles of a set's lists (the lists' destructor, 0x00476490).</summary>
    void releaseShapes(std::vector<int32_t>* shapes, int32_t count)
    {
        for (int32_t i = 0; i < count; i++)
        {
            for (const int32_t tileId : shapes[i])
            {
                if (tileId != NO_TILE && tileLibrary != nullptr)
                {
                    tileLibrary->releaseTile(tileId);
                }
            }
        }
    }

    /// <summary>
    /// Adds <paramref name="tileId"/> to a set's list as the original's vector of tile references did (0x00476890).
    /// </summary>
    /// <remarks>
    /// Original behaviour (EB-125): VC6's vector grows from n to 2n elements, copying the references without counting them and
    /// destroying the old ones, which releases them. So a list of 1, 2, 4, 8... tiles releases all of its tiles when
    /// one more is added, and the reference counts (the .pre file's tiles) depend on it.
    /// </remarks>
    void addTileReference(std::vector<int32_t>& list, int32_t tileId)
    {
        const size_t size = list.size();

        if (size != 0 && (size & (size - 1)) == 0)
        {
            for (const int32_t oldId : list)
            {
                tileLibrary->releaseTile(oldId);
            }
        }

        list.push_back(tileId);
    }

    /// <summary>A variant of <paramref name="shape"/>'s tiles for <see cref="TileLibrary::pickTile"/>.</summary>
    /// <returns>The variant; -1 when there is no tile.</returns>
    int32_t chooseVariant(const std::vector<int32_t>& shape, int32_t probability, int32_t variant)
    {
        const auto count = static_cast<int32_t>(shape.size());

        if (variant != -1)
        {
            return variant;
        }

        if (count < 2)
        {
            return count < 1 ? -1 : 0;
        }

        const int32_t roll = std::rand();

        if (probability <= roll % 100)
        {
            return roll % (count - 1) + 1;
        }

        return 0;
    }
}

PureTileSet::~PureTileSet()
{
    releaseShapes(shapes, TILE_SHAPES);
}

TransitionTileSet::~TransitionTileSet()
{
    releaseShapes(shapes, TILE_SHAPES * TRANSITION_TYPES);
}

TileLibrary::TileLibrary()
{
    tileLibrary = this;
}

TileLibrary::~TileLibrary()
{
    destroy();
    mixedOverlays.clear();
    tiles.clear();
    tileLibrary = nullptr;
}

void TileLibrary::destroy()
{
    for (auto& row : sets)
    {
        for (TileSet*& set : row)
        {
            delete set;
            set = nullptr;
        }
    }

    for (auto& row : overlays)
    {
        for (OverlaySet*& set : row)
        {
            delete set;
            set = nullptr;
        }
    }

    for (TileEntry& entry : tiles)
    {
        delete[] entry.data;
        entry.data = nullptr;
    }

    tiles.clear();
    mixedOverlays.clear();

    for (HANDLE* handle : {&tile90Mapping, &tile90FileHandle, &tileFileHandle, &tileMapping})
    {
        if (*handle != nullptr)
        {
            CloseHandle(*handle);
        }
    }
}

void TileLibrary::reset()
{
    tile90Mapping = nullptr;
    tile90FileHandle = nullptr;
    loaded = false;
    std::fill(&sets[0][0], &sets[0][0] + TEXTURE_SLOTS * TEXTURE_SLOTS, nullptr);
    std::fill(&overlays[0][0], &overlays[0][0] + OVERLAY_KINDS * 2, nullptr);
    name[0] = 0;
    initialized = false;
    reloadNeeded = true;
}

bool TileLibrary::init(const char* tile90File, const char* tileFile, const char* csvFile)
{
    bool ok = true;

    if (!reloadNeeded)
    {
        return ok;
    }

    if (loaded)
    {
        destroy();
        // Original behaviour: the Tiles.pak handles are closed but not cleared.
        reset();
    }

    // Original behaviour: CreateFile's failure value is INVALID_HANDLE_VALUE, not the null these test for, so a
    // missing file is only noticed by CreateFileMapping (which then closes the wrong handle).
    tileFileHandle =
        CreateFileA(tileFile, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (tileFileHandle == nullptr)
    {
        return false;
    }

    tileMapping = CreateFileMappingA(tileFileHandle, nullptr, PAGE_READONLY, 0, 0, nullptr);

    if (tileMapping == nullptr)
    {
        return false;
    }

    tile90FileHandle =
        CreateFileA(tile90File, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (tile90FileHandle == nullptr)
    {
        return false;
    }

    tile90Mapping = CreateFileMappingA(tile90FileHandle, nullptr, PAGE_READONLY, 0, 0, nullptr);

    if (tile90Mapping == nullptr)
    {
        return false;
    }

    const auto* table = static_cast<const uint32_t*>(MapViewOfFile(tile90Mapping, FILE_MAP_READ, 0, 0, 0));

    if (table == nullptr || table[0] != PACKET_FILE_MAGIC)
    {
        return false;
    }

    CFile csv;

    if (!csv.Open(csvFile, CFile::modeRead))
    {
        return false;
    }

    std::string text(static_cast<size_t>(csv.GetLength()), '\0');
    csv.Read(text.data(), static_cast<UINT>(text.size()));
    std::istringstream stream(text);
    tiles.resize(TILE_COUNT, TileEntry{});

    for (int32_t i = 0; i < TILE_COUNT; i++)
    {
        TileEntry& entry = tiles[static_cast<size_t>(i)];
        const uint32_t packetEntry = table[FIRST_TILE_ENTRY + i];
        entry.size = (table[FIRST_TILE_ENTRY + i + 1] & PACKET_OFFSET_MASK) - (packetEntry & PACKET_OFFSET_MASK);
        entry.data = nullptr;
        entry.offset = packetEntry;
        int32_t textureA = 0;
        int32_t textureB = 0;
        int32_t corners[4] = {};
        int8_t transition = 0;
        parseRow(i, stream, &textureA, &textureB, corners, &transition);
    }

    loaded = ok;
    UnmapViewOfFile(table);
    initialized = true;
    reloadNeeded = false;
    return ok;
}

bool TileLibrary::parseRow(int32_t tileId, std::istream& stream, int32_t* textureA, int32_t* textureB, int32_t* corners,
                           int8_t* transition)
{
    char line[CSV_LINE_SIZE];
    char field[CSV_FIELD_SIZE];
    char kind[CSV_FIELD_SIZE];
    stream.getline(line, CSV_LINE_SIZE);
    bool damaged = false;
    bool isTile = false;
    const char* p = nextField(line, kind);
    // The tile number column; the row's position is what counts.
    p = nextField(p, field);
    std::atoi(field);

    if (_stricmp(kind, "Tile") == 0)
    {
        p = nextField(p, field);
        *textureA = textureIndex(field);
        p = nextField(p, field);
        *textureB = textureIndex(field);
        isTile = true;
    }
    else if (_stricmp(kind, "Overlay") == 0)
    {
        p = nextField(p, field);
        *textureA = static_cast<int8_t>(overlayIndex(field, &damaged));
        p = nextField(p, field);
        *textureB = static_cast<int8_t>(overlayIndex(field, &damaged));
    }
    else
    {
        return false;
    }

    p = nextField(p, field);

    if (_stricmp(field, "NULL") == 0)
    {
        *transition = -1;
    }
    else
    {
        *transition = static_cast<int8_t>(std::tolower(static_cast<unsigned char>(field[0])) - 'a');
    }

    p = nextField(p, field);
    corners[0] = std::atoi(field);
    p = nextField(p, field);
    corners[1] = std::atoi(field);
    p = nextField(p, field);
    corners[3] = std::atoi(field);
    p = nextField(p, field);
    corners[2] = std::atoi(field);

    if (isTile)
    {
        addTile(tileId, *textureA, *textureB, corners[0], corners[1], corners[2], corners[3], *transition);
        return true;
    }

    addOverlay(p, tileId, *textureA, *textureB, corners[0], corners[1], corners[2], corners[3], *transition, damaged);
    return true;
}

bool TileLibrary::addTile(int32_t tileId, int32_t textureA, int32_t textureB, int32_t height0, int32_t height1,
                          int32_t height2, int32_t height3, int8_t transition)
{
    const int32_t shape = ((height0 * 3 + height1) * 3 + height2) * 3 + height3;

    if (textureA == textureB)
    {
        TileSet*& slot = sets[textureA][textureA];

        if (slot == nullptr)
        {
            auto* set = new PureTileSet();
            slot = set;
            set->name[0] = 0;
            LoadStringA(GetModuleHandleA(nullptr), TEXTURE_NAME_STRINGS + tileSetStringOffset + textureA, set->name,
                        sizeof(set->name));
            set->probability = PURE_SET_PROBABILITY;
        }

        addTileReference(static_cast<PureTileSet*>(slot)->shapes[shape], tileId);
        releaseTile(tileId);
        return true;
    }

    if (transition == -1)
    {
        return true;
    }

    TileSet*& forward = sets[textureA][textureB];

    if (forward == nullptr)
    {
        forward = new TransitionTileSet();
    }

    addTileReference(
        static_cast<TransitionTileSet*>(forward)->shapes[shape * TRANSITION_TYPES + static_cast<uint8_t>(transition)],
        tileId);
    releaseTile(tileId);
    const auto mirrored = static_cast<int8_t>(mirrorTransition(static_cast<char>(transition + 'A')) - 'A');

    if (mirrored < 0 || mirrored > TRANSITION_TYPES)
    {
        return false;
    }

    TileSet*& reverse = sets[textureB][textureA];

    if (reverse == nullptr)
    {
        reverse = new TransitionTileSet();
    }

    addTileReference(static_cast<TransitionTileSet*>(reverse)->shapes[shape * TRANSITION_TYPES + mirrored], tileId);
    releaseTile(tileId);
    return true;
}

bool TileLibrary::addOverlay(const char* rest, int32_t tileId, int32_t kindA, int32_t kindB, int32_t height0,
                             int32_t height1, int32_t height2, int32_t height3, int8_t transition, bool damaged)
{
    const int32_t layout = overlayLayout(transition);

    if (kindA == kindB)
    {
        OverlaySet*& slot = overlays[kindA][damaged ? 1 : 0];

        if (slot == nullptr)
        {
            slot = new OverlaySet();
            LoadStringA(GetModuleHandleA(nullptr), OVERLAY_NAME_STRINGS + tileSetStringOffset + kindA, slot->name,
                        sizeof(slot->name));
        }

        const int32_t heights = ((height0 * 2 + height1) * 2 + height2) * 2 + height3;
        // Original behaviour: an unknown layout (0xff) files the tile past the end of its heights' row.
        slot->shapes[heights * OVERLAY_LAYOUTS + layout].push_back(tileId);
        return true;
    }

    // Original behaviour: a corner field other than "null", "b..." or "t..." leaves the corner uninitialized.
    MixedOverlay overlay{{0, 0, 0, 0}, tileId};
    char field[CSV_FIELD_SIZE];

    for (int32_t& cornerKind : overlay.kinds)
    {
        rest = nextField(rest, field);

        if (_stricmp(field, "null") == 0)
        {
            cornerKind = OVERLAY_NONE;
        }
        else if (field[0] == 'b')
        {
            cornerKind = kindA;
        }
        else if (field[0] == 't')
        {
            cornerKind = kindB;
        }
    }

    mixedOverlays.push_back(overlay);
    return true;
}

int32_t TileLibrary::pickTile(const uint8_t* textures, const uint8_t* heights, int32_t variant)
{
    const uint8_t first = textures[0];
    int32_t second = -1;

    for (int32_t i = 1; i < 4; i++)
    {
        if (textures[i] != first && textures[i] != second)
        {
            if (second != -1)
            {
                return NO_TILE;
            }

            second = textures[i];
        }
    }

    const uint8_t transition = transitionType(textures);
    const int32_t shape = shapeIndex(heights);

    if (transition == TRANSITION_NONE && sets[first][first] != nullptr)
    {
        const auto* set = static_cast<PureTileSet*>(sets[first][first]);
        const std::vector<int32_t>& tilesOfShape = set->shapes[shape];
        const auto count = static_cast<int32_t>(tilesOfShape.size());

        if (variant != -1 && variant >= count)
        {
            variant = count - 1;
        }

        variant = chooseVariant(tilesOfShape, set->probability, variant);

        if (variant < 0)
        {
            return NO_TILE;
        }

        return tilesOfShape[static_cast<size_t>(variant)];
    }

    // Original behaviour: second is -1 here for a single texture with no pure set, which indexes the row before.
    if (second == -1)
    {
        return NO_TILE;
    }

    const auto* set = static_cast<TransitionTileSet*>(sets[first][second]);

    if (set == nullptr)
    {
        return NO_TILE;
    }

    const std::vector<int32_t>& tilesOfShape = set->shapes[shape * TRANSITION_TYPES + transition];
    variant = chooseVariant(tilesOfShape, set->probability, variant);

    // Original behaviour (EB-036): a variant past the last tile reads beyond the list.
    if (variant < 0 || variant >= static_cast<int32_t>(tilesOfShape.size()))
    {
        return NO_TILE;
    }

    const int32_t tileId = tilesOfShape[static_cast<size_t>(variant)];
    releaseTile(tileId);
    return tileId;
}

const int32_t* TileLibrary::findOverlayTile(const int32_t* kinds, const uint8_t* heights, bool damaged)
{
    int32_t kind = OVERLAY_NONE;
    int32_t otherKind = OVERLAY_NONE;

    for (int32_t i = 0; i < 4; i++)
    {
        if (kinds[i] != OVERLAY_NONE)
        {
            if (kind == OVERLAY_NONE)
            {
                kind = kinds[i];
            }
            else if (kinds[i] != kind)
            {
                otherKind = kinds[i];
            }
        }
    }

    if (kind == OVERLAY_NONE)
    {
        return nullptr;
    }

    uint8_t corners[4] = {heights[0], heights[1], heights[2], heights[3]};

    if (kind == OVERLAY_ROUGH || kind == OVERLAY_FOREST)
    {
        std::fill(std::begin(corners), std::end(corners), static_cast<uint8_t>(0));
    }
    else if (corners[0] == 2 || corners[1] == 2 || corners[2] == 2 || corners[3] == 2)
    {
        return nullptr;
    }

    if (otherKind != OVERLAY_NONE)
    {
        for (const MixedOverlay& overlay : mixedOverlays)
        {
            if (std::equal(std::begin(overlay.kinds), std::end(overlay.kinds), kinds))
            {
                return &overlay.tileId;
            }
        }

        return nullptr;
    }

    const int8_t layout = overlayShape(kinds);
    const OverlaySet* set = overlays[kind][damaged ? 1 : 0];

    if (layout == -1 || set == nullptr)
    {
        return nullptr;
    }

    const int32_t index =
        layout + (corners[3] + (corners[2] + (corners[1] + corners[0] * 2) * 2) * 2) * OVERLAY_LAYOUTS;
    const std::vector<int32_t>& tilesOfShape = set->shapes[index];
    const auto count = static_cast<int32_t>(tilesOfShape.size());

    if (count > 1)
    {
        return &tilesOfShape[static_cast<size_t>(std::rand() % count)];
    }

    if (count == 1)
    {
        return tilesOfShape.data();
    }

    return nullptr;
}

HBITMAP TileLibrary::drawTextureThumbnail(HDC dc, uint8_t texture, BITMAPINFO* info, int32_t variant)
{
    // Cliff's stand-in: the cliff-to-grass tiles of corners 2, 2, 0, 0 (shape 72) and transition 1.
    constexpr int32_t CLIFF_THUMBNAIL_SHAPE = 72;
    constexpr int32_t CLIFF_THUMBNAIL_TRANSITION = 1;
    // The flat shape: every corner at the lowest height.
    constexpr int32_t FLAT_SHAPE = 0;
    int32_t tileId = NO_TILE;

    if (const TileSet* set = sets[texture][texture]; set != nullptr)
    {
        const std::vector<int32_t>& flat = static_cast<const PureTileSet*>(set)->shapes[FLAT_SHAPE];

        // Original behaviour: a variant past the set's flat tiles is read past the end of their list.
        if (variant >= 0 && static_cast<size_t>(variant) < flat.size())
        {
            tileId = flat[static_cast<size_t>(variant)];
        }
    }
    else if (texture == TEXTURE_CLIFF)
    {
        const TileSet* set = sets[TEXTURE_CLIFF][TEXTURE_GRASS];

        if (set != nullptr)
        {
            const std::vector<int32_t>& tilesOfShape =
                static_cast<const TransitionTileSet*>(set)
                    ->shapes[CLIFF_THUMBNAIL_SHAPE * TRANSITION_TYPES + CLIFF_THUMBNAIL_TRANSITION];

            if (!tilesOfShape.empty())
            {
                tileId = tilesOfShape[0];
            }
        }
    }

    const uint8_t* tile = getTileData(tileId);

    // Original behaviour: a texture with no tile is drawn from a null pointer (a crash); the port draws nothing.
    if (tile == nullptr)
    {
        return nullptr;
    }

    const uint8_t tileHeight = tile[2];
    const LONG height = info->bmiHeader.biHeight;
    void* bits = nullptr;
    const HBITMAP bitmap = CreateDIBSection(dc, info, DIB_RGB_COLORS, &bits, nullptr, 0);

    if (bitmap == nullptr)
    {
        return nullptr;
    }

    VFX_nTile_draw(static_cast<uint8_t*>(bits), info->bmiHeader.biWidth, -info->bmiHeader.biHeight, tile, 0,
                   -static_cast<int32_t>(tileHeight >> 1) - height / 2, nullptr, VFX_NO_OUTLINE);
    return bitmap;
}

HBITMAP TileLibrary::drawOverlayThumbnail(HDC dc, int32_t kind, BITMAPINFO* info, const int32_t* corners)
{
    // The layouts drawn without corners: all four corners for forest and rough, else layout 10.
    constexpr int32_t FULL_LAYOUT = 4;
    constexpr int32_t DEFAULT_LAYOUT = 10;
    // The overlay is drawn at this size, then shrunk.
    constexpr LONG CANVAS_SIZE = 0x80;
    // The colour both bitmaps are cleared to.
    constexpr uint8_t BACKGROUND = 0xe7;
    const OverlaySet* set = overlays[kind][0];

    if (set == nullptr)
    {
        return nullptr;
    }

    int32_t layout = kind == OVERLAY_FOREST || kind == OVERLAY_ROUGH ? FULL_LAYOUT : DEFAULT_LAYOUT;

    if (corners != nullptr)
    {
        layout = overlayShape(corners);

        if (layout == -1 || set->shapes[layout].empty())
        {
            return nullptr;
        }
    }

    // Original behaviour: without corners, an empty layout's first tile is read past the end of its list.
    if (set->shapes[layout].empty())
    {
        return nullptr;
    }

    const uint8_t* table = getTileData(set->shapes[layout][0]);

    if (table == nullptr)
    {
        return nullptr;
    }

    auto* canvasInfo = reinterpret_cast<BITMAPINFO*>(new uint8_t[BITMAPINFO_256_SIZE]);
    std::memcpy(canvasInfo, info, BITMAPINFO_256_SIZE);
    canvasInfo->bmiHeader.biHeight = -CANVAS_SIZE;
    canvasInfo->bmiHeader.biWidth = CANVAS_SIZE;
    canvasInfo->bmiHeader.biSizeImage = 0;
    void* thumbnailBits = nullptr;
    void* canvasBits = nullptr;
    const HBITMAP thumbnail = CreateDIBSection(dc, info, DIB_RGB_COLORS, &thumbnailBits, nullptr, 0);
    const HBITMAP canvas = CreateDIBSection(dc, canvasInfo, DIB_RGB_COLORS, &canvasBits, nullptr, 0);

    // Original behaviour: a failure from here on leaks the bitmaps, the header and the DC; the port frees them.
    const auto fail = [&]()
    {
        if (thumbnail != nullptr)
        {
            DeleteObject(thumbnail);
        }

        if (canvas != nullptr)
        {
            DeleteObject(canvas);
        }

        delete[] reinterpret_cast<uint8_t*>(canvasInfo);
        return static_cast<HBITMAP>(nullptr);
    };

    if (thumbnail == nullptr || canvas == nullptr)
    {
        return fail();
    }

    std::memset(canvasBits, BACKGROUND,
                static_cast<size_t>(-canvasInfo->bmiHeader.biHeight * canvasInfo->bmiHeader.biWidth));
    std::memset(thumbnailBits, BACKGROUND, static_cast<size_t>(-info->bmiHeader.biWidth * info->bmiHeader.biHeight));
    const uint8_t* shape = table + read32(table + 8);
    int16_t hotX;
    int16_t hotY;
    std::memcpy(&hotX, shape + 4, sizeof(hotX));
    std::memcpy(&hotY, shape + 6, sizeof(hotY));
    VFX_nShape_draw(static_cast<uint8_t*>(canvasBits), canvasInfo->bmiHeader.biWidth, -canvasInfo->bmiHeader.biHeight,
                    table, 0, hotX, hotY, nullptr);
    const HGDIOBJ oldThumbnail = SelectObject(dc, thumbnail);

    if (oldThumbnail == nullptr)
    {
        return fail();
    }

    const HDC canvasDc = CreateCompatibleDC(dc);
    const HGDIOBJ oldCanvas = SelectObject(canvasDc, canvas);

    if (oldCanvas == nullptr)
    {
        DeleteDC(canvasDc);
        return fail();
    }

    SetStretchBltMode(dc, COLORONCOLOR);

    if (!StretchBlt(dc, 0, 0, info->bmiHeader.biWidth, -info->bmiHeader.biHeight, canvasDc, 0, 0,
                    canvasInfo->bmiHeader.biWidth, -canvasInfo->bmiHeader.biHeight, SRCCOPY))
    {
        SelectObject(canvasDc, oldCanvas);
        DeleteDC(canvasDc);
        return fail();
    }

    SelectObject(dc, oldThumbnail);
    SelectObject(canvasDc, oldCanvas);
    DeleteObject(canvas);
    DeleteDC(canvasDc);
    delete[] reinterpret_cast<uint8_t*>(canvasInfo);
    return thumbnail;
}

bool TileLibrary::findOverlay(int32_t tileId, int32_t* kinds, bool* damaged)
{
    for (int32_t kind = 0; kind < OVERLAY_KINDS; kind++)
    {
        for (int32_t heights = 0; heights < OVERLAY_SHAPES; heights++)
        {
            for (int32_t layout = 0; layout < OVERLAY_LAYOUTS; layout++)
            {
                for (int32_t look = 0; look < 2; look++)
                {
                    const OverlaySet* set = overlays[kind][look];

                    if (set == nullptr)
                    {
                        continue;
                    }

                    const std::vector<int32_t>& tilesOfShape = set->shapes[heights * OVERLAY_LAYOUTS + layout];

                    if (std::find(tilesOfShape.begin(), tilesOfShape.end(), tileId) != tilesOfShape.end())
                    {
                        *damaged = look != 0;
                        return expandOverlayShape(kind, layout, kinds);
                    }
                }
            }
        }
    }

    for (const MixedOverlay& overlay : mixedOverlays)
    {
        if (overlay.tileId == tileId)
        {
            std::copy(std::begin(overlay.kinds), std::end(overlay.kinds), kinds);
            return true;
        }
    }

    return false;
}

bool TileLibrary::getTileShape(int32_t tileId, uint8_t* textures, uint8_t* heights)
{
    int32_t range = 0;

    while (tileId < TILE_RANGES[range].start || TILE_RANGES[range + 1].start <= tileId)
    {
        range++;

        // Original behaviour (EB-034): a tile in no range reads the pure set pointer from before the set table.
        if (range >= SEARCHED_RANGES)
        {
            return false;
        }
    }

    const int32_t textureA = TILE_RANGES[range].textureA;
    const int32_t textureB = TILE_RANGES[range].textureB;

    if (textureA != textureB)
    {
        const auto* set = static_cast<const TransitionTileSet*>(sets[textureA][textureB]);

        // Original behaviour (EB-034): a missing set is read through a null pointer.
        if (set == nullptr)
        {
            return false;
        }

        for (uint8_t h0 = 0; h0 < 3; h0++)
        {
            for (uint8_t h1 = 0; h1 < 3; h1++)
            {
                for (uint8_t h2 = 0; h2 < 3; h2++)
                {
                    for (uint8_t h3 = 0; h3 < 3; h3++)
                    {
                        for (uint8_t transition = 0; transition < TRANSITION_TYPES; transition++)
                        {
                            const std::vector<int32_t>& tilesOfShape =
                                set->shapes[(((h0 * 3 + h1) * 3 + h2) * 3 + h3) * TRANSITION_TYPES + transition];

                            if (std::find(tilesOfShape.begin(), tilesOfShape.end(), tileId) != tilesOfShape.end())
                            {
                                heights[0] = h0;
                                heights[1] = h1;
                                heights[2] = h2;
                                heights[3] = h3;
                                return expandTransition(static_cast<uint8_t>(textureA), static_cast<uint8_t>(textureB),
                                                        transition, textures);
                            }
                        }
                    }
                }
            }
        }

        return false;
    }

    const auto* set = static_cast<const PureTileSet*>(sets[textureA][textureA]);

    if (set == nullptr)
    {
        return false;
    }

    for (uint8_t h0 = 0; h0 < 3; h0++)
    {
        for (uint8_t h1 = 0; h1 < 3; h1++)
        {
            for (uint8_t h2 = 0; h2 < 3; h2++)
            {
                for (uint8_t h3 = 0; h3 < 3; h3++)
                {
                    const std::vector<int32_t>& tilesOfShape = set->shapes[((h0 * 3 + h1) * 3 + h2) * 3 + h3];

                    if (std::find(tilesOfShape.begin(), tilesOfShape.end(), tileId) != tilesOfShape.end())
                    {
                        std::fill(textures, textures + 4, static_cast<uint8_t>(textureA));
                        heights[0] = h0;
                        heights[1] = h1;
                        heights[2] = h2;
                        heights[3] = h3;
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

void TileLibrary::setGTiles(bool gTiles)
{
    const int32_t offset = gTiles ? GTILES_STRING_OFFSET : 0;

    if (tileSetStringOffset != offset)
    {
        reloadNeeded = true;
    }

    tileSetStringOffset = offset;
}

void TileLibrary::releaseTile(int32_t tileId)
{
    if (tileId != NO_TILE && static_cast<size_t>(tileId) < tiles.size())
    {
        tiles[static_cast<size_t>(tileId)].refCount--;
    }
}

uint8_t* TileLibrary::getTileData(int32_t tileId)
{
    if (tileId == NO_TILE)
    {
        return nullptr;
    }

    TileEntry& entry = tiles[static_cast<size_t>(tileId)];
    entry.refCount++;

    if (entry.data != nullptr)
    {
        return entry.data;
    }

    auto* view = static_cast<const uint8_t*>(MapViewOfFile(tile90Mapping, FILE_MAP_READ, 0, 0, 0));

    if (view == nullptr)
    {
        return nullptr;
    }

    const uint8_t* packet = view + (entry.offset & PACKET_OFFSET_MASK);
    uint8_t* data = nullptr;

    if ((entry.offset & PACKET_PACKED) == 0)
    {
        data = new uint8_t[entry.size];
        std::memcpy(data, packet, entry.size);
    }
    else
    {
        std::memcpy(&entry.unpackedSize, packet, sizeof(entry.unpackedSize));
        data = new uint8_t[entry.unpackedSize + 1];

        if (LZDecomp(data, packet + sizeof(uint32_t), entry.size) == 0)
        {
            delete[] data;
            // Original behaviour: the failed paths leave the view mapped.
            UnmapViewOfFile(view);
            return nullptr;
        }
    }

    entry.data = data;
    // The original passes the tile's address in the view, which Windows takes for the view's.
    UnmapViewOfFile(view);
    return data;
}

uint8_t* TileLibrary::getMediumTileData(int32_t tileId)
{
    const int32_t index = tileId + TILE_ZOOM_STRIDE;

    if (index == NO_TILE)
    {
        return nullptr;
    }

    TileEntry& entry = tiles[static_cast<size_t>(index)];
    entry.refCount++;

    if (entry.data != nullptr)
    {
        return entry.data;
    }

    // Original behaviour: the view isn't checked.
    auto* view = static_cast<const uint8_t*>(MapViewOfFile(tileMapping, FILE_MAP_READ, 0, 0, 0));
    uint32_t packetEntry;
    uint32_t nextEntry;
    std::memcpy(&packetEntry, view + (FIRST_TILE_ENTRY + static_cast<size_t>(index)) * 4, sizeof(packetEntry));
    std::memcpy(&nextEntry, view + (FIRST_TILE_ENTRY + static_cast<size_t>(index) + 1) * 4, sizeof(nextEntry));
    entry.offset = packetEntry;
    entry.size = (nextEntry - packetEntry) & PACKET_OFFSET_MASK;
    const uint8_t* packet = view + (packetEntry & PACKET_OFFSET_MASK);
    uint8_t* data = nullptr;

    if ((packetEntry & PACKET_PACKED) == 0)
    {
        data = new uint8_t[entry.size];
        // Original behaviour (EB-048): copied from the start of the file.
        std::memcpy(data, view, entry.size);
    }
    else
    {
        std::memcpy(&entry.unpackedSize, packet, sizeof(entry.unpackedSize));
        data = new (std::nothrow) uint8_t[entry.unpackedSize + 1];

        // Original behaviour: the failed paths leave the view mapped.
        if (data == nullptr)
        {
            return nullptr;
        }

        if (LZDecomp(data, packet + sizeof(uint32_t), entry.size) == 0)
        {
            delete[] data;
            return nullptr;
        }
    }

    entry.data = data;
    UnmapViewOfFile(view);
    return data;
}

uint8_t* TileLibrary::getSmallTileData(int32_t tileId, bool overlay)
{
    // The sizes the smallest zoom's art is made at.
    constexpr int32_t DRAW_WIDTH = 160;
    constexpr int32_t DRAW_HEIGHT = 120;
    constexpr int32_t SMALL_WIDTH = DRAW_WIDTH / 2;
    constexpr int32_t SMALL_HEIGHT = DRAW_HEIGHT / 2;
    // Where the art's hot spot goes, in the drawing and in the result.
    constexpr int32_t DRAW_HOT_X = SMALL_WIDTH;
    constexpr uint8_t SMALL_HOT_X = SMALL_WIDTH / 2;
    // The window the encoders get: its last column and row become a tile's width and rows bytes. Original behaviour
    // (EB-051): 70 and 59, for 80 x 60 pixels.
    constexpr int32_t SMALL_X_MAX = 0x46;
    constexpr int32_t SMALL_Y_MAX = SMALL_HEIGHT - 1;
    // The one-shape table around an overlay shape: magic, count, the shape's offset, a pad word.
    constexpr uint32_t TABLE_MAGIC = 0x48414e44;
    constexpr size_t TABLE_HEADER = 0x10;
    constexpr size_t TABLE_SLACK = 0x20;

    TileEntry& entry = tiles[static_cast<size_t>(tileId + 2 * TILE_ZOOM_STRIDE)];
    entry.refCount++;

    if (entry.data != nullptr)
    {
        return entry.data;
    }

    const uint8_t* medium = getMediumTileData(tileId);
    // Original behaviour (EB-049): meant to give back the use just counted, it counts down the middle entry's packet
    // offset (read again before it is used).
    tiles[static_cast<size_t>(tileId + TILE_ZOOM_STRIDE)].offset--;

    if (medium == nullptr)
    {
        // The original draws from the null pointer.
        return nullptr;
    }

    // The result, then the drawing, one after the other as on the original's stack: the overlay encoder reads a row
    // past the result.
    std::vector<uint8_t> scratch(static_cast<size_t>(SMALL_WIDTH * SMALL_HEIGHT + DRAW_WIDTH * DRAW_HEIGHT));
    uint8_t* reduced = scratch.data();
    uint8_t* drawing = reduced + SMALL_WIDTH * SMALL_HEIGHT;
    std::fill(drawing, drawing + DRAW_WIDTH * DRAW_HEIGHT, VFX_TRANSPARENT);
    int32_t hotY = 0;

    if (overlay)
    {
        int16_t shapeHotY;
        std::memcpy(&shapeHotY, medium + TABLE_HEADER + 6, sizeof(shapeHotY));
        hotY = (std::max)(static_cast<int32_t>(shapeHotY), 0);
        VFX_nShape_draw(drawing, DRAW_WIDTH, DRAW_HEIGHT, medium, 0, DRAW_HOT_X, hotY, nullptr);
    }
    else
    {
        VFX_nTile_draw(drawing, DRAW_WIDTH, DRAW_HEIGHT, medium, DRAW_HOT_X, 0, nullptr, VFX_NO_OUTLINE);
    }

    for (int32_t y = 0; y < SMALL_HEIGHT; y++)
    {
        for (int32_t x = 0; x < SMALL_WIDTH; x++)
        {
            reduced[y * SMALL_WIDTH + x] = drawing[y * 2 * DRAW_WIDTH + x * 2];
        }
    }

    WINDOW window = {reduced, SMALL_X_MAX, SMALL_Y_MAX, nullptr, nullptr};
    const PANE pane = {&window, 0, 0, SMALL_WIDTH, SMALL_HEIGHT};
    uint8_t* result = nullptr;
    intptr_t made = 0;

    if (overlay)
    {
        uint8_t* shape = nullptr;
        const int32_t size = VFX_nShape_scan(pane, SMALL_HOT_X, static_cast<uint8_t>(hotY / 2), &shape);
        result = new uint8_t[static_cast<size_t>(size) + TABLE_SLACK]();
        const uint32_t header[4] = {TABLE_MAGIC, 1, TABLE_HEADER, 0};
        std::memcpy(result, header, sizeof(header));
        std::memcpy(result + TABLE_HEADER, shape, static_cast<size_t>(size));
        delete[] shape;
        // The original tests the new block's address for success.
        made = reinterpret_cast<intptr_t>(result);
    }
    else
    {
        made = VFX_nTile_scan(pane, SMALL_HOT_X, 0, &result);
    }

    // Original behaviour: a failed encoding keeps its block.
    if (made <= 0)
    {
        return nullptr;
    }

    entry.data = result;
    return result;
}

uint8_t TileLibrary::getDefaultTexture()
{
    return defaultTexture;
}

namespace
{
    /// <summary>
    /// Whether texture <paramref name="candidate"/> beats <paramref name="best"/> as the one to blend with: the default
    /// texture first, else the lower one.
    /// </summary>
    /// <remarks>0x004748d0, inlined where the original uses it.</remarks>
    bool preferTexture(uint8_t candidate, uint8_t best)
    {
        if (candidate == defaultTexture)
        {
            return true;
        }

        if (best == defaultTexture)
        {
            return false;
        }

        return candidate < best;
    }

    /// <summary>The textures of <paramref name="textures"/> that aren't <paramref name="texture"/>, each once, in corner order.</summary>
    std::vector<uint8_t> otherTextures(const uint8_t* textures, uint8_t texture)
    {
        std::vector<uint8_t> others;

        for (int32_t i = 0; i < 4; i++)
        {
            if (textures[i] != texture && std::find(others.begin(), others.end(), textures[i]) == others.end())
            {
                others.push_back(textures[i]);
            }
        }

        return others;
    }
}

bool TileLibrary::resolveTextures(uint8_t texture, uint8_t* textures, const uint8_t* heights, uint8_t* out)
{
    if (texture == TEXTURE_CLIFF)
    {
        return resolveCliff(TEXTURE_CLIFF, textures, heights, out);
    }

    if (texture == TEXTURE_WATER)
    {
        return resolveWater(TEXTURE_WATER, heights, out);
    }

    std::vector<uint8_t> others;

    for (int32_t i = 0; i < 4; i++)
    {
        if (textures[i] != texture && std::find(others.begin(), others.end(), textures[i]) == others.end())
        {
            others.push_back(textures[i]);
        }

        if (textures[i] == TEXTURE_CLIFF)
        {
            return resolveCliff(texture, textures, heights, out);
        }

        if (textures[i] == TEXTURE_WATER)
        {
            return resolveWater(texture, heights, out);
        }
    }

    // Original behaviour: the search starts from dirt, so dirt wins when no other texture qualifies.
    uint8_t best = TEXTURE_DIRT;

    for (const uint8_t other : others)
    {
        if (preferTexture(other, best) && sets[other][texture] != nullptr)
        {
            best = other;
        }

        if (other == texture)
        {
            best = texture;
            break;
        }
    }

    for (int32_t i = 0; i < 4; i++)
    {
        if (textures[i] != texture)
        {
            textures[i] = best;
        }

        out[i] = textures[i];
    }

    // Two textures on opposite corners.
    return out[0] != out[3] || out[1] != out[2] || out[1] == out[0];
}

bool TileLibrary::resolveCliff(uint8_t texture, const uint8_t* textures, const uint8_t* heights, uint8_t* out)
{
    const std::vector<uint8_t> others = otherTextures(textures, TEXTURE_CLIFF);
    uint8_t ground = defaultTexture;

    if (others.size() == 1)
    {
        ground = others[0];
    }
    else if (others.size() > 1)
    {
        ground = TEXTURE_WATER;

        for (const uint8_t other : others)
        {
            if (preferTexture(other, ground) && sets[other][texture] != nullptr)
            {
                ground = other;
            }
        }
    }

    if (ground != TEXTURE_DIRT && ground != TEXTURE_GRASS)
    {
        ground = defaultTexture;
    }

    if (texture != TEXTURE_CLIFF)
    {
        std::fill(out, out + 4, ground);
        return true;
    }

    for (int32_t i = 0; i < 4; i++)
    {
        if (heights[i] == 2)
        {
            out[i] = TEXTURE_CLIFF;
        }
        else if (heights[i] == 0)
        {
            out[i] = ground;
        }
        else
        {
            std::fill(out, out + 4, ground);
            break;
        }
    }

    return true;
}

bool TileLibrary::resolveWater(uint8_t texture, const uint8_t* heights, uint8_t* out)
{
    uint8_t shore = defaultTexture;

    if (texture != defaultTexture && texture != TEXTURE_WATER && (texture == TEXTURE_DIRT || texture == TEXTURE_GRASS))
    {
        shore = texture;
    }

    for (int32_t i = 0; i < 4; i++)
    {
        if (heights[i] == 1)
        {
            out[i] = shore;
        }
        else if (heights[i] == 0)
        {
            out[i] = TEXTURE_WATER;
        }
        else
        {
            std::fill(out, out + 4, shore);
            break;
        }
    }

    const int32_t id = pickTile(out, heights, -1);
    releaseTile(id);

    // Original behaviour: the water is given up exactly when the library has a tile for it.
    if (id != NO_TILE)
    {
        std::fill(out, out + 4, shore);
    }

    return true;
}

uint8_t TileLibrary::textureIndex(const char* name)
{
    static constexpr struct
    {
        const char* name;
        uint8_t texture;
    } NAMES[] = {{"grass", TEXTURE_GRASS}, {"dirt", TEXTURE_DIRT},   {"moun", TEXTURE_MOUN}, {"ruff", TEXTURE_RUFF},
                 {"cliff", TEXTURE_CLIFF}, {"water", TEXTURE_WATER}, {"conc", TEXTURE_CONC}, {"crop", TEXTURE_CROP}};

    for (const auto& entry : NAMES)
    {
        if (_stricmp(entry.name, name) == 0)
        {
            return entry.texture;
        }
    }

    return TEXTURE_NONE;
}

uint8_t TileLibrary::overlayIndex(const char* name, bool* damaged)
{
    static constexpr struct
    {
        const char* name;
        bool damaged;
        uint8_t kind;
    } NAMES[] = {{"bridge", false, OVERLAY_BRIDGE},       {"dbridge", true, OVERLAY_BRIDGE},
                 {"droad", false, OVERLAY_DIRT_ROAD},     {"proad", false, OVERLAY_PAVED_ROAD},
                 {"railroad", false, OVERLAY_RAILROAD},   {"rbridge", false, OVERLAY_RAIL_BRIDGE},
                 {"drbridge", true, OVERLAY_RAIL_BRIDGE}, {"lwall", false, OVERLAY_LIGHT_WALL},
                 {"dlwall", true, OVERLAY_LIGHT_WALL},    {"runway", false, OVERLAY_RUNWAY},
                 {"wall", false, OVERLAY_WALL},           {"dwall", true, OVERLAY_WALL},
                 {"forest", false, OVERLAY_FOREST},       {"dforest", true, OVERLAY_FOREST},
                 {"rough", false, OVERLAY_ROUGH},         {"dock", false, OVERLAY_DOCK},
                 {"noforest", false, OVERLAY_FOREST},     {"hwall", false, OVERLAY_HEAVY_WALL},
                 {"dhwall", true, OVERLAY_HEAVY_WALL}};

    for (const auto& entry : NAMES)
    {
        if (_stricmp(entry.name, name) == 0)
        {
            *damaged = entry.damaged;
            return entry.kind;
        }
    }

    return 0xff;
}

uint8_t TileLibrary::transitionType(const uint8_t* textures)
{
    if (textures[0] != textures[1])
    {
        if (textures[1] == textures[2])
        {
            return textures[2] != textures[3] ? 13 : 0;
        }

        return textures[0] != textures[3] ? 7 : 9;
    }

    if (textures[1] == textures[2])
    {
        return textures[2] != textures[3] ? 10 : TRANSITION_NONE;
    }

    return textures[0] != textures[3] ? 1 : 11;
}

int8_t TileLibrary::overlayShape(const int32_t* kinds)
{
    const bool has0 = kinds[0] != OVERLAY_NONE;
    const bool has1 = kinds[1] != OVERLAY_NONE;
    const bool has2 = kinds[2] != OVERLAY_NONE;
    const bool has3 = kinds[3] != OVERLAY_NONE;

    if (has0)
    {
        if (has1)
        {
            if (!has2)
            {
                return has3 ? 0 : 6;
            }

            return has3 ? 4 : 1;
        }

        if (!has2)
        {
            return has3 ? 5 : 12;
        }

        return has3 ? 3 : 10;
    }

    if (has1)
    {
        if (!has2)
        {
            return has3 ? 9 : 13;
        }

        return has3 ? 2 : 7;
    }

    if (!has2)
    {
        return has3 ? 11 : -1;
    }

    return has3 ? 8 : 14;
}

bool TileLibrary::expandOverlayShape(int32_t kind, int32_t shape, int32_t* kinds)
{
    // The corners each layout covers, as bits 0-3 for corners 0-3.
    static constexpr uint8_t CORNERS[OVERLAY_LAYOUTS] = {0xb, 0x7, 0xe, 0xd, 0xf, 0x9, 0x3, 0x6,
                                                         0xc, 0xa, 0x5, 0x8, 0x1, 0x2, 0x4};
    std::fill(kinds, kinds + 4, static_cast<int32_t>(OVERLAY_NONE));

    if (shape < 0 || shape >= OVERLAY_LAYOUTS)
    {
        return true;
    }

    for (int32_t i = 0; i < 4; i++)
    {
        if ((CORNERS[shape] >> i & 1) != 0)
        {
            kinds[i] = kind;
        }
    }

    return true;
}

bool TileLibrary::expandTransition(uint8_t textureA, uint8_t textureB, uint8_t transition, uint8_t* textures)
{
    // The corners each transition gives the second texture, as bits 0-3 for corners 0-3.
    static constexpr uint8_t CORNERS[TRANSITION_TYPES] = {0xe, 0xc, 0xd, 0x5, 0x7, 0x3, 0xb,
                                                          0xa, 0x1, 0x2, 0x8, 0x4, 0x9, 0x6};
    std::fill(textures, textures + 4, textureA);

    if (transition >= TRANSITION_TYPES)
    {
        return false;
    }

    for (int32_t i = 0; i < 4; i++)
    {
        if ((CORNERS[transition] >> i & 1) != 0)
        {
            textures[i] = textureB;
        }
    }

    return true;
}

int32_t TileLibrary::tileGroup(int32_t tileId)
{
    // Original behaviour: the search also takes the closing row, whose range ends at whatever follows the table.
    for (int32_t range = 0; range < SEARCHED_RANGES; range++)
    {
        if (TILE_RANGES[range].start <= tileId && tileId < TILE_RANGES[range + 1].start)
        {
            return TILE_RANGES[range].group;
        }
    }

    return -1;
}

int32_t TileLibrary::shapeIndex(const uint8_t* heights)
{
    return ((heights[0] * 3 + heights[1]) * 3 + heights[2]) * 3 + heights[3];
}
