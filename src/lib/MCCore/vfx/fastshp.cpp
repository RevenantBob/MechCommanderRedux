#include "stdafx.h"
#include "vfx/vfxint.h"

// The game's "fast shape" drawer (mcx\vfx\fastshp.cpp, inline assembly), used for the terrain overlay tiles. The
// format is its own, not VFX's:
//
//   table:  +0 "DNAH", then at +8 + 4n the offset of shape n from the table
//   shape:  +4 int16 hot spot x, +6 int16 hot spot y,
//           +8 uint16 height (rows), +10 uint16 width (pixels per row, less one),
//           +12 uint16 row offsets from the shape
//   row:    packets until width + 1 pixels are covered: a byte c < 0x80 is a run of c pixels of the colour in the
//           next byte (255: transparent); c >= 0x80 is c - 0x80 literal pixels following.
//
// A shape whose first row starts with the 16-bit word 1 is translucent: every pixel is blended through AlphaTable.

namespace
{
    uint16_t Read16(const uint8_t* p)
    {
        uint16_t value;
        std::memcpy(&value, p, 2);
        return value;
    }
}

int32_t FastShapeDraw(MCPane* pane, void* shapeTable, int32_t shapeNum, int32_t hotX, int32_t hotY, uint8_t* xlat,
                      int unused)
{
    (void)unused;
    uint8_t* table = static_cast<uint8_t*>(shapeTable);
    const uint8_t* shape = table + MCVfxRead32(table + 8 + static_cast<intptr_t>(shapeNum) * 4);
    const uint32_t dims = static_cast<uint32_t>(MCVfxRead32(shape + 8));
    const int32_t height = static_cast<int32_t>(dims & 0xffff);
    const int32_t width = static_cast<int32_t>(dims >> 16);

    const MCWindow* window = pane->Window;
    int32_t clipX1 = std::min(pane->X1, window->XMax);
    int32_t clipY1 = std::min(pane->Y1, window->YMax);
    int32_t clipX0 = std::max(pane->X0, 0);
    int32_t clipY0 = std::max(pane->Y0, 0);
    MCClipToView(window, clipX0, clipY0, clipX1, clipY1);

    // OB-115: the asm offset by the pane's origin clipped to the window. OB-116: and counted a shape starting on the
    // last column or row as outside.
    const int32_t sx = pane->X0 + hotX - static_cast<int16_t>(Read16(shape + 4));
    const int32_t sy = pane->Y0 + hotY - static_cast<int16_t>(Read16(shape + 6));

    if (clipX1 < clipX0 || clipY1 < clipY0 || sx > clipX1 || sy > clipY1 || sx + width < clipX0 ||
        sy + height <= clipY0)
    {
        return 0;
    }

    int32_t leftSkip = 0;
    int32_t span = width;

    if (sx < clipX0)
    {
        leftSkip = clipX0 - sx;
        span = width - leftSkip;
        // Port fix: the original clipped only the left of a shape wider than the pane on both sides, running the
        // rows on past the right edge (and, on the window's last row, past its buffer).
        span = std::min(span, clipX1 - clipX0);
    }
    else if (sx + width > clipX1)
    {
        span = width - (sx + width - clipX1);
    }

    const int32_t limit = span + 1;

    const int32_t firstRow = sy < clipY0 ? clipY0 - sy : 0;
    const int32_t endRow = height + sy > clipY1 ? clipY1 - sy + 1 : height;

    if (firstRow >= endRow)
    {
        return 0;
    }

    MCFastShapeCommand command;
    command.Shape = shape;
    command.Top = std::max(sy, clipY0);
    command.FirstRow = firstRow;
    command.EndRow = endRow;
    command.StartX = sx;
    command.ClipX0 = clipX0;
    command.LeftSkip = leftSkip;
    command.Limit = limit;
    command.Alpha = Read16(shape + Read16(shape + 0xc)) == 1;
    command.Table = xlat;
    MCRenderer::For(pane->Window).FastShape(pane->Window, command);
    return 0;
}
