#include "stdafx.h"
#include "EditorMap.h"
#include "EditorObject.h"
#include "EditorPalette.h"
#include "EditorUtil.h"
#include "MapOverlay.h"
#include "ObjectLibrary.h"
#include "TileLibrary.h"
#include "X87Extended.h"

#include <bit>

// The map's pictures for the tactical map window and the map export (0x004572e0-0x00457e10, 0x0045d250).

namespace
{
    using X = X87Extended;

    /// <summary>A colour as the renderer builds it: blue, green, red.</summary>
    struct Bgr
    {
        uint8_t blue;
        uint8_t green;
        uint8_t red;
    };

    /// <summary>
    /// The terrain colours (0x00502648 for the planet set, 0x00502708 for GTiles): per texture, red, green and blue
    /// each as a base and how much the height (0 to the map's highest) adds.
    /// </summary>
    struct TextureShade
    {
        float redBase;
        float redSlope;
        float greenBase;
        float greenSlope;
        float blueBase;
        float blueSlope;
    };

    inline constexpr TextureShade PLANET_SHADES[TEXTURE_SLOTS] = {
        {68, 68, 48, 60, 16, 44}, {44, 52, 56, 64, 24, 44}, {44, 52, 56, 64, 24, 44}, {68, 68, 60, 56, 48, 40},
        {156, 0, 124, 0, 96, 0},  {24, 0, 40, 0, 48, 0},    {80, 0, 80, 0, 80, 0},    {44, 52, 56, 64, 24, 44}};
    inline constexpr TextureShade GTILES_SHADES[TEXTURE_SLOTS] = {
        {36, 120, 20, 104, 16, 80}, {40, 176, 20, 168, 0, 120}, {40, 176, 20, 168, 0, 120}, {48, 144, 28, 88, 12, 48},
        {252, 0, 220, 0, 176, 0},   {0, 0, 88, 0, 88, 0},       {100, 0, 100, 0, 100, 0},   {100, 0, 100, 0, 100, 0}};
    /// <summary>The water's colour where it meets another texture: brightened by these (planet set only).</summary>
    inline constexpr double WATER_RED_SCALE = 3.5;
    inline constexpr double WATER_GREEN_SCALE = 2.6;
    inline constexpr double WATER_BLUE_SCALE = 2.5;

    /// <summary>The overlay kinds' colours, red, green and blue (0x005027c8 for the planet set, 0x00502858 for GTiles).</summary>
    inline constexpr int32_t PLANET_OVERLAY_COLORS[OVERLAY_KINDS][3] = {
        {0, 64, 0},      {124, 32, 24}, {80, 80, 80},   {124, 32, 24}, {108, 64, 28}, {0, 0, 0},
        {188, 160, 100}, {0, 0, 0},     {108, 64, 128}, {0, 0, 0},     {124, 32, 24}, {0, 0, 0}};
    inline constexpr int32_t GTILES_OVERLAY_COLORS[OVERLAY_KINDS][3] = {
        {14, 63, 13}, {204, 0, 0},  {80, 80, 80},   {204, 0, 0},  {108, 64, 28}, {0, 0, 0},
        {0, 0, 0},    {32, 32, 32}, {108, 64, 128}, {56, 52, 36}, {204, 0, 0},   {0, 0, 0}};
    /// <summary>The colour of an object the tactical map shows.</summary>
    inline constexpr Bgr OBJECT_COLOR = {0x3c, 0x74, 0xc0};
    /// <summary>The colour the tactical map's bitmap starts as (and the map export's background).</summary>
    inline constexpr uint8_t BACKGROUND = 0x10;

    /// <summary>A tactical map pixel is a third of a tile: the tile's 3 x 3 cells, in the order the pixels take them.</summary>
    inline constexpr int32_t BLOCK = 3;
    inline constexpr POINT BLOCK_CELLS[BLOCK * BLOCK] = {{0, 1}, {0, 0}, {1, 0}, {0, 2}, {1, 1},
                                                         {2, 0}, {1, 2}, {2, 2}, {2, 1}};
    /// <summary>
    /// The pixels' map units (the game's tile, 158, over 3 pixels turned 45 degrees: 0x004feab8), the turn
    /// (0x004fc138), one over the tile (0x004feab0), the rounding (0x004fad18) and the tile in map units (0x004feac0).
    /// </summary>
    const double PIXEL_UNITS = std::bit_cast<double>(0x40529ed7af035febull);
    const double HALF_SQRT2 = std::bit_cast<double>(0x3fe6a09e667f3a21ull);
    const double ONE_OVER_TILE = std::bit_cast<double>(0x3f79ec8e951033d9ull);
    inline constexpr double HALF = 0.5;
    inline constexpr double TILE_UNITS = 158.0;
    /// <summary>Added to a double to leave its value, rounded to the nearest integer, in the low word.</summary>
    inline constexpr double ROUNDING_MAGIC = 6755399441055744.0;
    /// <summary>The tile set string offset of the GTiles set.</summary>
    inline constexpr int32_t GTILES_OFFSET = 0x32;
    /// <summary>The lowest elevation the highest search starts from.</summary>
    inline constexpr int32_t LOWEST_ELEVATION = -0xff;

    /// <summary>The low byte of <paramref name="value"/> rounded as the x87 code stores it (through a double).</summary>
    uint8_t roundByte(const X& value)
    {
        const double sum = (value + X(ROUNDING_MAGIC)).toDouble();
        uint64_t bits;
        std::memcpy(&bits, &sum, sizeof(bits));
        return static_cast<uint8_t>(bits);
    }

    /// <summary>A shade's channel at height fraction <paramref name="t"/>, times <paramref name="scale"/> when given.</summary>
    X shadeChannel(const X& t, float base, float slope)
    {
        return t * X(slope) + X(base);
    }

    /// <summary>Adds <paramref name="add"/> to <paramref name="value"/>, staying under 255 (255 when it would reach it).</summary>
    uint8_t addClamped(uint8_t value, uint8_t add)
    {
        return value + add < 0xff ? static_cast<uint8_t>(value + add) : 0xff;
    }

    /// <summary>
    /// The colour of terrain of textures <paramref name="first"/> and <paramref name="second"/> at height
    /// <paramref name="z"/> of the map's highest <paramref name="highest"/>: the first's shade, or with two textures
    /// half the sum of both (water brightened on the planet set); a cliff takes over.
    /// </summary>
    /// <remarks>0x00457920.</remarks>
    Bgr terrainColor(uint8_t first, uint8_t second, int32_t z, int32_t highest)
    {
        const X t = highest != 0 ? X(z) / X(highest) : X(0.0f);

        if (first == TEXTURE_CLIFF || second == TEXTURE_CLIFF)
        {
            first = TEXTURE_CLIFF;
            second = TEXTURE_CLIFF;
        }

        const bool gTiles = tileSetStringOffset == GTILES_OFFSET;
        const TextureShade* shades = gTiles ? GTILES_SHADES : PLANET_SHADES;
        const TextureShade& a = shades[first];
        Bgr color = {roundByte(shadeChannel(t, a.blueBase, a.blueSlope)),
                     roundByte(shadeChannel(t, a.greenBase, a.greenSlope)),
                     roundByte(shadeChannel(t, a.redBase, a.redSlope))};

        if (first == second)
        {
            return color;
        }

        if (first == TEXTURE_WATER || second == TEXTURE_WATER)
        {
            const TextureShade& water = shades[TEXTURE_WATER];

            if (gTiles)
            {
                color = {roundByte(shadeChannel(t, water.blueBase, water.blueSlope)),
                         roundByte(shadeChannel(t, water.greenBase, water.greenSlope)),
                         roundByte(shadeChannel(t, water.redBase, water.redSlope))};
            }
            else
            {
                color = {roundByte(shadeChannel(t, water.blueBase, water.blueSlope) * X(WATER_BLUE_SCALE)),
                         roundByte(shadeChannel(t, water.greenBase, water.greenSlope) * X(WATER_GREEN_SCALE)),
                         roundByte(shadeChannel(t, water.redBase, water.redSlope) * X(WATER_RED_SCALE))};
            }

            if (second == TEXTURE_WATER)
            {
                second = first;
            }
        }

        const TextureShade& b = shades[second];
        const uint8_t red = roundByte(shadeChannel(t, b.redBase, b.redSlope));
        const uint8_t green = roundByte(shadeChannel(t, b.greenBase, b.greenSlope));
        const uint8_t blue = roundByte(shadeChannel(t, b.blueBase, b.blueSlope));
        color.red = static_cast<uint8_t>(addClamped(color.red, red) >> 1);
        color.green = static_cast<uint8_t>(addClamped(color.green, green) >> 1);
        color.blue = static_cast<uint8_t>(addClamped(color.blue, blue) >> 1);
        return color;
    }

    /// <summary>The colour of overlay kind <paramref name="kind"/>.</summary>
    /// <remarks>0x00457d80, also inlined.</remarks>
    Bgr overlayColor(int32_t kind)
    {
        const int32_t (&rgb)[3] =
            (tileSetStringOffset == GTILES_OFFSET ? GTILES_OVERLAY_COLORS : PLANET_OVERLAY_COLORS)[kind];
        return {static_cast<uint8_t>(rgb[2]), static_cast<uint8_t>(rgb[1]), static_cast<uint8_t>(rgb[0])};
    }

    /// <summary>Whether a corner's overlay kind leaves the terrain showing: none, or rough.</summary>
    bool showsTerrain(int32_t kind)
    {
        return kind == OVERLAY_NONE || kind == OVERLAY_ROUGH;
    }
}

void EditorMap::tacticalCellColor(int32_t col, int32_t row, int32_t subX, int32_t subY, int32_t highest, uint8_t* bgr)
{
    const EditorTile* tile = getTile(col, row);
    const EditorObject* object =
        cellGrid[static_cast<size_t>(((row * MAP_CELLS_PER_TILE + subY) * maxRow + col) * MAP_CELLS_PER_TILE + subX)];
    bool terrain = true;

    if (object != nullptr && objectLibrary->isShownOnTacMap(object->id))
    {
        *reinterpret_cast<Bgr*>(bgr) = OBJECT_COLOR;
        terrain = false;
    }
    else if (tile != nullptr && tile->overlay != nullptr)
    {
        // The edge cells show their side's corner; the middle the first corner with an overlay
        // that hides the terrain; the corner cells a forest on any corner.
        const int32_t* kinds = tile->overlay->kinds;
        int32_t corner = -1;

        if (subX == 1 && subY == 0)
        {
            corner = 0;
        }
        else if (subX == 1 && subY == 2)
        {
            corner = 2;
        }
        else if (subX == 0 && subY == 1)
        {
            corner = 3;
        }
        else if (subX == 2 && subY == 1)
        {
            corner = 1;
        }

        if (corner >= 0)
        {
            if (!showsTerrain(kinds[corner]))
            {
                *reinterpret_cast<Bgr*>(bgr) = overlayColor(kinds[corner]);
                terrain = false;
            }
        }
        else if (subX == 1 && subY == 1)
        {
            for (int32_t i = 0; i < 4; i++)
            {
                if (!showsTerrain(kinds[i]))
                {
                    *reinterpret_cast<Bgr*>(bgr) = overlayColor(kinds[i]);
                    terrain = false;
                    break;
                }
            }
        }
        else
        {
            for (int32_t i = 0; i < 4; i++)
            {
                if (kinds[i] == OVERLAY_FOREST)
                {
                    *reinterpret_cast<Bgr*>(bgr) = overlayColor(kinds[i]);
                    terrain = false;
                    break;
                }
            }
        }
    }

    if (terrain && tile != nullptr)
    {
        // The corners' textures, water where a corner is at 0; the first and the first other.
        uint8_t textures[4];

        for (int32_t i = 0; i < 4; i++)
        {
            textures[i] = tile->elevations[i] == 0 ? TEXTURE_WATER : tile->textures[i];
        }

        const int32_t z = vertexElevation(col, row);
        uint8_t first = textures[0];
        uint8_t second = textures[0];

        for (int32_t i = 1; i < 4; i++)
        {
            if (textures[i] != textures[0])
            {
                second = textures[i];
                break;
            }
        }

        if (tile->concIndex != -1)
        {
            first = TEXTURE_CONC;
            second = TEXTURE_CONC;
        }
        else if (tile->cropIndex != -1)
        {
            first = TEXTURE_CROP;
            second = TEXTURE_CROP;
        }

        *reinterpret_cast<Bgr*>(bgr) = terrainColor(first, second, z, highest);
    }
}

bool EditorMap::drawTacticalMap(uint8_t* buffer, int32_t width, int32_t height, const uint8_t* colors)
{
    int32_t highest = LOWEST_ELEVATION;

    for (int32_t row = 0; row < maxCol; row++)
    {
        for (int32_t col = 0; col < maxRow; col++)
        {
            highest = (std::max)(highest, vertexElevation(col, row));
        }
    }

    const float top = (X(static_cast<int32_t>(static_cast<uint32_t>(maxCol + 1) >> 1)) * X(TILE_UNITS)).toFloat();

    if (buffer == nullptr || height - 1 <= 1)
    {
        return true;
    }

    // The colour a pixel off the map takes: the last one worked out (the original's starts as whatever is on the
    // stack).
    Bgr color = {};
    uint8_t* rowStart = buffer - 1;

    for (int32_t blockRow = 0, k = -1; blockRow < height / BLOCK; blockRow++, k -= BLOCK, rowStart += width * BLOCK)
    {
        // Each 3 x 3 block of pixels is one tile: its map position from the block's, turned 45 degrees.
        const float along = (X(k) * X(PIXEL_UNITS) * X(HALF_SQRT2)).toFloat();

        for (int32_t j = 1; j < width - 1; j += BLOCK)
        {
            const double across = (X(j) * X(PIXEL_UNITS) * X(HALF_SQRT2)).toDouble();
            const auto tileCol =
                static_cast<int32_t>(std::floor(((X(across) - X(along)) * X(ONE_OVER_TILE) + X(HALF)).toDouble()));
            const auto tileRow = static_cast<int32_t>(
                std::floor(((X(top) - (X(along) + X(across))) * X(ONE_OVER_TILE) + X(HALF)).toDouble()));
            uint8_t* pixel = rowStart + j;
            int32_t cell = 0;

            for (int32_t dy = 0; dy < BLOCK; dy++, pixel += width - BLOCK)
            {
                for (int32_t dx = 0; dx < BLOCK; dx++, pixel++)
                {
                    if (tileCol >= 0 && tileRow >= 0 && tileCol <= maxRow && tileRow <= maxCol)
                    {
                        // The last row and column of vertices show the tiles before them.
                        const int32_t col = tileCol < maxRow ? tileCol : tileCol - 1;
                        const int32_t row = tileRow < maxCol ? tileRow : tileRow - 1;
                        const POINT& sub = BLOCK_CELLS[cell];
                        tacticalCellColor(col, row, sub.x, sub.y, highest, reinterpret_cast<uint8_t*>(&color));

                        cell++;
                    }

                    const uint8_t bgr[3] = {color.blue, color.green, color.red};
                    *pixel = findNearestColor(colors, bgr);
                }
            }
        }
    }

    return true;
}

bool EditorMap::saveImage(const CString& fileName, const POINT& origin, uint8_t* buffer, int32_t width, int32_t height,
                          int32_t zoom)
{
    drawTerrain(origin, buffer, width, height, false, zoom);
    CFile file;

    if (!file.Open(fileName, CFile::modeCreate | CFile::modeWrite))
    {
        return false;
    }

    // An uncompressed 8-bit colour-mapped Targa, top row first.
    constexpr uint8_t TGA_COLOR_MAPPED = 1;
    constexpr uint16_t TGA_COLORS = 0x100;
    constexpr uint8_t TGA_ENTRY_BITS = 0x18;
    constexpr uint8_t TGA_PIXEL_BITS = 8;
    constexpr uint8_t TGA_TOP_FIRST = 0x20;
#pragma pack(push, 1)
    struct
    {
        uint8_t idLength;
        uint8_t colorMapType;
        uint8_t imageType;
        uint16_t firstEntry;
        uint16_t entries;
        uint8_t entryBits;
        uint16_t x;
        uint16_t y;
        uint16_t width;
        uint16_t height;
        uint8_t pixelBits;
        uint8_t descriptor;
    } header = {0,
                TGA_COLOR_MAPPED,
                TGA_COLOR_MAPPED,
                0,
                TGA_COLORS,
                TGA_ENTRY_BITS,
                0,
                0,
                static_cast<uint16_t>(width),
                static_cast<uint16_t>(height),
                TGA_PIXEL_BITS,
                TGA_TOP_FIRST};
#pragma pack(pop)
    file.Write(&header, sizeof(header));
    const RGBQUAD* colors = editorPalette->bitmapInfo->bmiColors;

    for (int32_t i = 0; i < TGA_COLORS; i++)
    {
        file.Write(&colors[i].rgbBlue, 1);
        file.Write(&colors[i].rgbGreen, 1);
        file.Write(&colors[i].rgbRed, 1);
    }

    file.Write(buffer, static_cast<UINT>(width * height));
    return true;
}
