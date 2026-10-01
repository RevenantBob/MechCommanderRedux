#include "stdafx.h"
#include "gui/mchwcursor.h"
#include "gui/asystem.h"
#include "gui/updisp.h"
#include "platform/MCInput.h"
#include "vfx/vfxfuncs.h"

namespace
{
    /// <summary>The cursor shapes as pictures, made the first time each is shown.</summary>
    struct MCShapeCache
    {
        const void* Shape = nullptr;
        MCCursorImage Image;
    };

    std::array<MCShapeCache, 128> shapeCache;

    /// <summary>What this frame's cursor carries (MCHardwareCursorCarry).</summary>
    bool carrying = false;
    MCCursorImage carried;

    /// <summary>Draws a shape onto a pane of <paramref name="fill"/>, hot spot at (hotX, hotY).</summary>
    std::vector<uint8_t> drawShape(void* shapeTable, int32_t shapeNum, int32_t width, int32_t height, int32_t hotX,
                                   int32_t hotY, uint8_t fill)
    {
        std::vector<uint8_t> pixels(static_cast<size_t>(width) * height, fill);
        WINDOW window{};
        window.buffer = pixels.data();
        window.x_max = width - 1;
        window.y_max = height - 1;
        PANE pane{&window, 0, 0, width - 1, height - 1};
        AG_shape_draw(&pane, shapeTable, shapeNum, hotX, hotY);
        return pixels;
    }
}

MCCursorImage MCCursorImageFromShape(void* shapeTable, int32_t shapeNum)
{
    const int32_t resolution = VFX_shape_resolution(shapeTable, shapeNum);
    const int32_t minXY = VFX_shape_minxy(shapeTable, shapeNum);
    const int32_t width = resolution >> 16;
    const int32_t height = resolution & 0xffff;

    if (width <= 0 || height <= 0)
    {
        return {};
    }

    const int32_t hotX = -(minXY >> 16);
    const int32_t hotY = -static_cast<int16_t>(minXY);
    // Drawn twice, over 0 and over 255: a pixel the shape skips keeps the background, so it differs between the two.
    const std::vector<uint8_t> overBlack = drawShape(shapeTable, shapeNum, width, height, hotX, hotY, 0);
    const std::vector<uint8_t> overWhite = drawShape(shapeTable, shapeNum, width, height, hotX, hotY, 0xff);
    MCCursorImage image = MCCursorImage::Blank(width, height, hotX, hotY);

    for (size_t i = 0; i < image.Pixels.size(); ++i)
    {
        if (overBlack[i] != overWhite[i])
        {
            continue;
        }

        image.Pixels[i] = overBlack[i];
        image.Opaque[i] = 1;
    }

    return image;
}

MCCursorImage MCCursorImageFromPane(PANE* pane, int32_t hotX, int32_t hotY)
{
    if (pane == nullptr || pane->window == nullptr || pane->window->buffer == nullptr)
    {
        return {};
    }

    const int32_t width = pane->x1 - pane->x0 + 1;
    const int32_t height = pane->y1 - pane->y0 + 1;

    if (width <= 0 || height <= 0)
    {
        return {};
    }

    MCCursorImage image = MCCursorImage::Blank(width, height, hotX, hotY);
    const int32_t pitch = pane->window->x_max + 1;

    for (int32_t y = 0; y < height; ++y)
    {
        const uint8_t* row = pane->window->buffer + static_cast<ptrdiff_t>(pane->y0 + y) * pitch + pane->x0;
        std::memcpy(image.Pixels.data() + static_cast<size_t>(y) * width, row, static_cast<size_t>(width));
    }

    std::ranges::fill(image.Opaque, 1);
    return image;
}

void MCHardwareCursorNewFrame()
{
    carrying = false;
}

bool MCHardwareCursorCarry(aObject* object, PANE* pixels)
{
    if (gSoftwareCursor != 0 || object == nullptr || MCInput::Display() == nullptr)
    {
        return false;
    }

    // The object was placed from the mouse position of the message the game handled last; the same offset under
    // the live mouse keeps it where the game put it relative to the cursor.
    const MCPoint mouse = MCInput::GetMessagePos();
    const int32_t offsetX = mouse.x - object->globalX();
    const int32_t offsetY = mouse.y - object->globalY();

    if (offsetX < 0 || offsetY < 0 || offsetX >= object->width() || offsetY >= object->height())
    {
        return false;
    }

    MCCursorImage image = MCCursorImageFromPane(pixels, offsetX, offsetY);

    if (image.Width == 0)
    {
        return false;
    }

    carried = std::move(image);
    carrying = true;
    return true;
}

void MCHardwareCursorUpdate()
{
    MCCursorImage image;
    const int32_t shape = application->cursorShape;

    if (cursorShapes != nullptr && shape >= 0 && shape < 128 && cursorShapes[shape] != nullptr)
    {
        MCShapeCache& cached = shapeCache[static_cast<size_t>(shape)];

        if (cached.Shape != cursorShapes[shape])
        {
            cached.Image = MCCursorImageFromShape(cursorShapes[shape], 0);
            cached.Shape = cursorShapes[shape];
        }

        image = cached.Image;
    }

    if (carrying)
    {
        image = image.Width > 0 ? MCCursorImage::Overlay(carried, image) : carried;
    }

    carrying = false;

    if (image.Width > 0)
    {
        MCCursor::Show(image);
    }
    else
    {
        MCCursor::Hide();
    }
}
