#include "stdafx.h"
#include "EditorMap.h"
#include "EditorObject.h"
#include "EditorPalette.h"
#include "EditorUtil.h"
#include "ObjectLibrary.h"
#include "Objective.h"
#include "Parameter.h"
#include "X87Extended.h"
#include "resource.h"

#include <bit>

// The mission's tactical map pictures, written with the save (0x00457e10, 0x004589d0, 0x0045d140, 0x00471d40,
// 0x00472100, 0x004723b0): the map a pixel a cell, turned 45 degrees and smoothed, as Targa files, with the drop zones'
// and objectives' marks for the logistics screen.

namespace
{
    using X = X87Extended;

    /// <summary>The highest elevation search's start.</summary>
    inline constexpr int32_t LOWEST_ELEVATION = -0xff;
    /// <summary>The colour of an object's footprint, blue, green, red.</summary>
    inline constexpr uint8_t OBJECT_BGR[3] = {0x3c, 0x74, 0xc0};
    /// <summary>The colour the turned picture starts as, which marks a pixel no cell reached.</summary>
    inline constexpr uint8_t UNSET = 0xf0;
    /// <summary>A footprint is 8 x 8 cells around the object's cell, from 4 up and left.</summary>
    inline constexpr int32_t FOOTPRINT_SIZE = 8;
    inline constexpr int32_t FOOTPRINT_OFFSET = 4;
    /// <summary>The smoothing's weights: the pixel, its corner neighbours, its side neighbours; and their sum.</summary>
    inline constexpr int32_t CENTRE_WEIGHT = 5;
    inline constexpr int32_t CORNER_WEIGHT = 4;
    inline constexpr int32_t SIDE_WEIGHT = 1;
    inline constexpr int32_t WEIGHT_SUM = 25;
    /// <summary>The Targa header: its size, colour-mapped and true-colour image types, top-left origin.</summary>
    inline constexpr size_t TGA_HEADER_SIZE = 0x12;
    inline constexpr uint8_t TGA_COLOR_MAPPED = 1;
    inline constexpr uint8_t TGA_TRUE_COLOR = 2;
    inline constexpr uint8_t TGA_TOP_LEFT = 0x20;
    inline constexpr int32_t PALETTE_ENTRIES = 0x100;
    /// <summary>
    /// The marks: the drop zones' bitmap (203: 140 x 48, three marks of 16 rows, each 4 marks of 35 wide, one per
    /// side of the point) and the objectives' (204: 84 x 128, marks of 21 x 21).
    /// </summary>
    inline constexpr int32_t ZONE_MARKS_WIDTH = 0x8c;
    inline constexpr int32_t ZONE_MARKS_HEIGHT = 0x30;
    inline constexpr int32_t ZONE_MARK_WIDTH = 0x23;
    inline constexpr int32_t ZONE_MARK_HEIGHT = 0x10;
    inline constexpr int32_t OBJECTIVE_MARKS_WIDTH = 0x54;
    inline constexpr int32_t OBJECTIVE_MARKS_HEIGHT = 0x80;
    inline constexpr int32_t OBJECTIVE_MARK_SIZE = 0x15;
    /// <summary>The objectives' bitmap pixels that are made see-through (six marks; the last two rows aren't).</summary>
    inline constexpr int32_t OBJECTIVE_MARKS_KEYED = 0x2958;
    /// <summary>A mark pixel left out of the picture.</summary>
    inline constexpr uint8_t SEE_THROUGH = 0xff;
    /// <summary>A mark placed over others goes this far down and right of its point.</summary>
    inline constexpr int32_t OVERLAP_OFFSET = 0x2a;

    /// <summary>The picture's pixel at <paramref name="index"/>.</summary>
    uint8_t* pixelAt(std::vector<uint8_t>& image, size_t index)
    {
        return image.data() + index * 3;
    }

    /// <summary>
    /// Writes the part of <paramref name="image"/> (<paramref name="width"/> x <paramref name="height"/>, 3 bytes a
    /// pixel) from (<paramref name="left"/>, <paramref name="top"/>), <paramref name="cropWidth"/> x
    /// <paramref name="cropHeight"/>, as a top-down Targa: with <paramref name="palette"/> (256 RGBQUADs) 8-bit, each
    /// pixel its nearest colour; without one, 24-bit.
    /// </summary>
    /// <remarks>
    /// 0x004589d0. EB-123 fixed: the original writes on when the file doesn't open (MFC asserts); the port writes
    /// nothing.
    /// </remarks>
    void writeTga(const char* name, const uint8_t* image, int32_t width, int32_t height, int32_t left, int32_t top,
                  int32_t cropWidth, int32_t cropHeight, const uint8_t* palette)
    {
        CFile file;

        if (!file.Open(name, CFile::modeCreate | CFile::modeWrite))
        {
            return;
        }

        // The colours are matched against this palette alone: findNearestColor's cache is emptied before and after.
        ColorCacheReady = false;
        uint8_t header[TGA_HEADER_SIZE] = {};
        header[1] = palette != nullptr ? 1 : 0;
        header[2] = palette != nullptr ? TGA_COLOR_MAPPED : TGA_TRUE_COLOR;
        const uint16_t mapLength = palette != nullptr ? PALETTE_ENTRIES : 0;
        std::memcpy(header + 5, &mapLength, sizeof(mapLength));
        header[7] = palette != nullptr ? 0x18 : 0;
        const auto imageWidth = static_cast<uint16_t>(cropWidth);
        const auto imageHeight = static_cast<uint16_t>(cropHeight);
        std::memcpy(header + 0xc, &imageWidth, sizeof(imageWidth));
        std::memcpy(header + 0xe, &imageHeight, sizeof(imageHeight));
        header[0x10] = palette != nullptr ? 8 : 0x18;
        header[0x11] = TGA_TOP_LEFT;
        file.Write(header, sizeof(header));

        if (palette != nullptr)
        {
            for (int32_t i = 0; i < PALETTE_ENTRIES; i++)
            {
                file.Write(palette + i * 4, 3);
            }
        }

        const uint8_t* pixel = image;

        for (int32_t y = 0; y < height; y++)
        {
            for (int32_t x = 0; x < width; x++, pixel += 3)
            {
                if (x < left || x >= left + cropWidth || y < top || y >= top + cropHeight)
                {
                    continue;
                }

                if (palette != nullptr)
                {
                    const uint8_t index = findNearestColor(palette, pixel);
                    file.Write(&index, 1);
                }
                else
                {
                    file.Write(pixel, 3);
                }
            }
        }

        file.Close();
        ColorCacheReady = false;
    }

    /// <summary>The area of <paramref name="rect"/>.</summary>
    int32_t area(const RECT& rect)
    {
        return (rect.right - rect.left) * (rect.bottom - rect.top);
    }

    /// <summary>
    /// Whether a mark from (<paramref name="x"/>, <paramref name="y"/>) to <paramref name="dx"/>,
    /// <paramref name="dy"/> on lies inside the picture, and in <paramref name="overlap"/> its biggest overlap with
    /// the marks placed (empty for none).
    /// </summary>
    /// <remarks>0x004723b0.</remarks>
    bool markFits(int32_t x, int32_t y, int32_t dx, int32_t dy, RECT& overlap, const std::list<RECT>& placed,
                  int32_t width, int32_t height)
    {
        if (x < 0 || x + dx < 0 || x >= width || x + dx >= width || y < 0 || y + dy < 0 || y >= height ||
            y + dy >= height)
        {
            return false;
        }

        const RECT mark = {(std::min)(x, x + dx), (std::min)(y, y + dy), (std::max)(x, x + dx), (std::max)(y, y + dy)};
        overlap = {};

        for (const RECT& other : placed)
        {
            RECT common;

            if (IntersectRect(&common, &other, &mark) && area(overlap) < area(common))
            {
                overlap = common;
            }
        }

        return true;
    }

    /// <summary>
    /// Draws a mark at (<paramref name="x"/>, <paramref name="y"/>) on one of its four sides (up-left, up-right,
    /// down-right, down-left: the first inside the picture that overlaps no mark, else the one overlapping least),
    /// in the logistics colours, its <paramref name="marks"/> row holding a version for each side; its place is added
    /// to <paramref name="placed"/>.
    /// </summary>
    /// <remarks>
    /// 0x00472100. Original behaviour: a mark that overlaps others goes 42 pixels down and right of its point (the
    /// offset is worked out for a side past the four). EB-122 fixed: the original draws the mark without clipping it
    /// to the picture; the port clips.
    /// </remarks>
    void placeMark(std::vector<uint8_t>& image, int32_t width, int32_t height, const uint8_t* marks, int32_t markWidth,
                   int32_t markHeight, std::list<RECT>& placed, int32_t x, int32_t y)
    {
        bool usable[4] = {true, true, true, true};
        RECT overlaps[4] = {};
        int32_t side = 0;
        int32_t dx = 0;
        int32_t dy = 0;
        bool clear = false;

        for (int32_t i = 0; i < 4; i++)
        {
            dy = i == 0 || i == 1 ? -markHeight : markHeight;
            dx = i == 1 || i == 2 ? markWidth : -markWidth;

            if (!markFits(x, y, dx, dy, overlaps[i], placed, width, height))
            {
                usable[i] = false;
            }
            else if (overlaps[i].left == 0 && overlaps[i].right == 0 && overlaps[i].top == 0 && overlaps[i].bottom == 0)
            {
                side = i;
                clear = true;
                break;
            }
        }

        if (!clear)
        {
            int32_t least = INT32_MAX;

            for (int32_t i = 0; i < 4; i++)
            {
                if (usable[i] && area(overlaps[i]) < least)
                {
                    least = area(overlaps[i]);
                    side = i;
                }
            }

            dx = OVERLAP_OFFSET;
            dy = OVERLAP_OFFSET;
        }

        const RECT mark = {(std::min)(x, x + dx), (std::min)(y, y + dy), (std::max)(x, x + dx), (std::max)(y, y + dy)};
        placed.push_back(mark);
        const auto* colors = reinterpret_cast<const uint8_t*>(editorPalette->logisticColors);
        const uint8_t* source = marks + side * markWidth;

        for (int32_t row = 0; row < markHeight; row++, source += markWidth * 4)
        {
            const int32_t py = mark.top + row;

            for (int32_t col = 0; col < markWidth; col++)
            {
                const int32_t px = mark.left + col;

                if (source[col] == SEE_THROUGH || px < 0 || px >= width || py < 0 || py >= height)
                {
                    continue;
                }

                std::memcpy(pixelAt(image, static_cast<size_t>(py) * width + px), colors + source[col] * 4, 3);
            }
        }
    }

    /// <summary>
    /// The 8-bit pixels of bitmap <paramref name="id"/> (<paramref name="width"/> x <paramref name="height"/>), the
    /// first <paramref name="keyed"/> of them that are magenta made see-through.
    /// </summary>
    std::vector<uint8_t> loadMarks(HDC dc, HINSTANCE instance, UINT id, int32_t width, int32_t height, int32_t keyed)
    {
        // The editor's DIB header and palette, the bitmap's size, top-down; GetDIBits puts the bitmap's colours in.
        std::vector<uint8_t> info(sizeof(BITMAPINFOHEADER) + PALETTE_ENTRIES * sizeof(RGBQUAD));
        std::memcpy(info.data(), editorPalette->bitmapInfo, info.size());
        auto& header = *reinterpret_cast<BITMAPINFOHEADER*>(info.data());
        header.biWidth = width;
        header.biHeight = -height;
        const auto bitmap =
            static_cast<HBITMAP>(LoadImageA(instance, MAKEINTRESOURCEA(id), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
        std::vector<uint8_t> pixels(static_cast<size_t>(width) * height);
        // EB-124 fixed: the original asks for as many lines as the bitmap has bytes, which Windows refuses, leaving
        // the marks as stack garbage; the port asks for its height.
        GetDIBits(dc, bitmap, 0, static_cast<UINT>(height), pixels.data(), reinterpret_cast<BITMAPINFO*>(info.data()),
                  DIB_RGB_COLORS);
        const auto* colors = reinterpret_cast<const RGBQUAD*>(info.data() + sizeof(BITMAPINFOHEADER));

        for (int32_t i = 0; i < keyed; i++)
        {
            const RGBQUAD& color = colors[pixels[static_cast<size_t>(i)]];

            if (color.rgbRed > 0xfa && color.rgbBlue > 0xfa && color.rgbGreen < 5)
            {
                pixels[static_cast<size_t>(i)] = SEE_THROUGH;
            }
        }

        DeleteObject(bitmap);
        return pixels;
    }
}

void EditorMap::cellToTactical(int32_t col, int32_t row, int32_t* x, int32_t* y) const
{
    const X third = X(std::bit_cast<double>(0x3fd5555555555555ull));
    const X turn = X(std::bit_cast<double>(0x3fe6a09e667f3a21ull));
    const X backTurn = X(std::bit_cast<double>(0xbfe6a09e667f3a21ull));
    const X scale = X(std::bit_cast<double>(0x4010f876ccdf6b99ull));
    constexpr double ROW_SHIFT = 6.0;
    // The cell's map position (a third of a tile a cell), y up, then turned and scaled to the picture's pixels.
    const X across = X(tileWorldWidth * col) * third;
    const float down = (X(-row * tileWorldHeight) * third).toFloat();
    const X acrossTurned = across * turn;
    const float py = ((X(down) * turn - acrossTurned) * (scale / X(tileWorldHeight))).toFloat();
    const X px = (acrossTurned - X(down) * backTurn) * (scale / X(tileWorldWidth)) + X(maxRow >> 1) * X(ROW_SHIFT);
    *x = static_cast<int32_t>(px.truncate());
    *y = static_cast<int32_t>((X(0.0f) - X(py)).truncate());
}

bool EditorMap::writeTacticalImages(const uint8_t* palette, const char* tgaName, const char* trueColorName,
                                    const char* logisticName)
{
    int32_t highest = LOWEST_ELEVATION;

    for (int32_t row = 0; row < maxCol; row++)
    {
        for (int32_t col = 0; col < maxRow; col++)
        {
            highest = (std::max)(highest, vertexElevation(col, row));
        }
    }

    // A pixel a cell, the vertices past the last tiles showing the tiles before them.
    const int32_t width = (maxRow + 1) * MAP_CELLS_PER_TILE;
    const int32_t height = (maxCol + 1) * MAP_CELLS_PER_TILE;
    std::vector<uint8_t> cells(static_cast<size_t>(width) * height * 3);
    uint8_t* pixel = cells.data();

    for (int32_t y = 0; y < height; y++)
    {
        for (int32_t x = 0; x < width; x++, pixel += 3)
        {
            const int32_t tileCol = x / MAP_CELLS_PER_TILE;
            const int32_t tileRow = y / MAP_CELLS_PER_TILE;
            const int32_t col = tileCol < maxRow ? tileCol : tileCol - 1;
            const int32_t row = tileRow < maxCol ? tileRow : tileRow - 1;
            tacticalCellColor(col, row, x % MAP_CELLS_PER_TILE, y % MAP_CELLS_PER_TILE, highest, pixel);
        }
    }

    // The objects' and drop zones' footprints.
    for (const std::list<EditorObject*>* list : {&objects, &dropZones})
    {
        for (const EditorObject* object : *list)
        {
            const uint32_t id = object->id;
            const ObjectFrame* frame;

            if (objectIdClass(id) == 1)
            {
                frame = &objectLibrary->dropZones[objectIdType(id)];
            }
            else
            {
                const std::vector<ObjectGroup>& groups =
                    isMoverClass(objectIdClass(id)) ? objectLibrary->moverGroups : objectLibrary->objectGroups;
                frame = &groups[objectIdGroup(id)].types[objectIdType(id)].frames[objectIdFrame(id)];
            }

            const uint64_t footprint = static_cast<uint64_t>(frame->footprintHigh) << 32 | frame->footprintLow;

            for (int32_t r = 0; r < FOOTPRINT_SIZE; r++)
            {
                for (int32_t c = 0; c < FOOTPRINT_SIZE; c++)
                {
                    const int32_t x = object->cellCol - FOOTPRINT_OFFSET + c;
                    const int32_t y = object->cellRow - FOOTPRINT_OFFSET + r;

                    // EB-120 fixed: the original writes the footprint of an object near the map's edge outside the
                    // picture; the port leaves those pixels out.
                    if ((footprint >> (r * FOOTPRINT_SIZE + c) & 1) == 0 || x < 0 || x >= width || y < 0 || y >= height)
                    {
                        continue;
                    }

                    std::memcpy(pixelAt(cells, static_cast<size_t>(y) * width + x), OBJECT_BGR, 3);
                }
            }
        }
    }

    // Turned 45 degrees into a picture twice the width each way: the cells' diagonals become its rows, every other
    // pixel. Original behaviour: the picture is made square from the width (maps are square).
    const int32_t size = width * 2;
    std::vector<uint8_t> image(static_cast<size_t>(size) * size * 3, UNSET);
    size_t dest = 0;
    size_t source = 0;

    for (int32_t y = 0; y < width; y++)
    {
        for (int32_t i = 0, s = static_cast<int32_t>(source); i < y + 1; i++, s += 1 - width)
        {
            std::memcpy(pixelAt(image, dest + static_cast<size_t>(i) * 2), pixelAt(cells, static_cast<size_t>(s)), 3);
        }

        source += static_cast<size_t>(width);
        dest = static_cast<size_t>((size - 1) * y + width - 1);

        if (y == width - 1)
        {
            dest += 2;
        }
    }

    source = static_cast<size_t>((width - 1) * width + 1);

    for (int32_t j = width; j < size; j++, source++)
    {
        const int32_t start = j - width + 1;

        if (start < width)
        {
            for (int32_t i = 0, s = static_cast<int32_t>(source); i < width - start; i++, s += 1 - width)
            {
                std::memcpy(pixelAt(image, dest + static_cast<size_t>(i) * 2), pixelAt(cells, static_cast<size_t>(s)),
                            3);
            }
        }

        dest = static_cast<size_t>((size + 1) * j - width + 3);
    }

    // The pixels between the cells: the average of their four neighbours.
    const auto at = [&](int32_t x, int32_t y) { return pixelAt(image, static_cast<size_t>(y) * size + x); };

    for (int32_t y = 1; y < size - 1; y++)
    {
        for (int32_t x = 1; x < size - 1; x++)
        {
            uint8_t* p = at(x, y);

            if (p[0] != UNSET || p[1] != UNSET || p[2] != UNSET)
            {
                continue;
            }

            for (int32_t c = 0; c < 3; c++)
            {
                p[c] =
                    static_cast<uint8_t>((at(x, y + 1)[c] + at(x, y - 1)[c] + at(x - 1, y)[c] + at(x + 1, y)[c]) >> 2);
            }
        }
    }

    // Smoothed in place (each pixel from its neighbours as they are by then).
    for (int32_t y = 1; y < size - 1; y++)
    {
        for (int32_t x = 1; x < size - 1; x++)
        {
            uint8_t around[3][3][3];

            for (int32_t dy = 0; dy < 3; dy++)
            {
                for (int32_t dx = 0; dx < 3; dx++)
                {
                    std::memcpy(around[dy][dx], at(x + dx - 1, y + dy - 1), 3);
                }
            }

            uint8_t* p = at(x, y);

            for (int32_t c = 0; c < 3; c++)
            {
                const int32_t corners = around[0][0][c] + around[0][2][c] + around[2][0][c] + around[2][2][c];
                const int32_t sides = around[0][1][c] + around[1][0][c] + around[1][2][c] + around[2][1][c];
                p[c] = static_cast<uint8_t>(
                    (around[1][1][c] * CENTRE_WEIGHT + corners * CORNER_WEIGHT + sides * SIDE_WEIGHT) / WEIGHT_SUM);
            }
        }
    }

    // The middle of the picture, where the map is.
    const int32_t left = (size >> 2) + 4;
    const int32_t cropWidth = (size >> 1) - 8;
    const int32_t cropHeight = cropWidth - 4;

    if (tgaName != nullptr)
    {
        writeTga(tgaName, image.data(), size, size, left, left, cropWidth, cropHeight, palette);
    }

    drawMissionMarks(image.data(), size, size);

    if (logisticName != nullptr)
    {
        writeTga(logisticName, image.data(), size, size, left, left, cropWidth, cropHeight,
                 reinterpret_cast<const uint8_t*>(editorPalette->logisticColors));
    }

    if (trueColorName != nullptr)
    {
        writeTga(trueColorName, image.data(), size, size, left, left, cropWidth, cropHeight, nullptr);
    }

    return true;
}

void SingleMap::drawMissionMarks(uint8_t* image, int32_t width, int32_t height)
{
    const HDC dc = GetDC(nullptr);
    const HINSTANCE instance = GetModuleHandleA(nullptr);
    const std::vector<uint8_t> zoneMarks = loadMarks(dc, instance, IDB_DROP_ZONE_MARKS, ZONE_MARKS_WIDTH,
                                                     ZONE_MARKS_HEIGHT, ZONE_MARKS_WIDTH * ZONE_MARKS_HEIGHT);
    const std::vector<uint8_t> objectiveMarks = loadMarks(dc, instance, IDB_OBJECTIVE_MARKS, OBJECTIVE_MARKS_WIDTH,
                                                          OBJECTIVE_MARKS_HEIGHT, OBJECTIVE_MARKS_KEYED);
    ReleaseDC(nullptr, dc);
    std::vector<uint8_t> picture(image, image + static_cast<size_t>(width) * height * 3);
    std::list<RECT> placed;

    // EB-121 fixed: the original takes the marks on past its bitmaps for more than three drop zones or six
    // objectives; the port draws no more.
    const size_t zoneMarkBytes = static_cast<size_t>(ZONE_MARKS_WIDTH) * ZONE_MARK_HEIGHT;
    size_t zoneMark = 0;

    for (const EditorObject* zone : dropZones)
    {
        if ((zoneMark + 1) * zoneMarkBytes > zoneMarks.size())
        {
            break;
        }

        int32_t x = 0;
        int32_t y = 0;
        cellToTactical(zone->cellCol, zone->cellRow, &x, &y);
        placeMark(picture, width, height, zoneMarks.data() + zoneMark * zoneMarkBytes, ZONE_MARK_WIDTH,
                  ZONE_MARK_HEIGHT, placed, x, y);
        zoneMark++;
    }

    // Each objective has a mark (used or not); one with a marker shows it at its first value with a position.
    const size_t objectiveMarkBytes = static_cast<size_t>(OBJECTIVE_MARKS_WIDTH) * OBJECTIVE_MARK_SIZE;
    size_t objectiveMark = 0;

    for (Objective* objective : objectives)
    {
        if ((objectiveMark + 1) * objectiveMarkBytes > objectiveMarks.size())
        {
            break;
        }

        if (objective->hasMarker)
        {
            const auto values = static_cast<int32_t>(
                std::ranges::count_if(objective->parameters, [](const Parameter* value) { return value != nullptr; }));

            for (int32_t i = 0; i < values; i++)
            {
                int32_t worldX = 0;
                int32_t worldY = 0;

                if (objective->parameters[i] == nullptr ||
                    !objective->parameters[i]->getPosition(&worldX, &worldY, *this))
                {
                    continue;
                }

                int32_t col = 0;
                int32_t row = 0;
                worldToCell(worldX, worldY, &col, &row);
                int32_t x = 0;
                int32_t y = 0;
                cellToTactical(col, row, &x, &y);
                placeMark(picture, width, height, objectiveMarks.data() + objectiveMark * objectiveMarkBytes,
                          OBJECTIVE_MARK_SIZE, OBJECTIVE_MARK_SIZE, placed, x, y);
                break;
            }
        }

        objectiveMark++;
    }

    std::memcpy(image, picture.data(), picture.size());
}
