#include "stdafx.h"
#include "gui/mchwcursor.h"
#include "gui/asystem.h"
#include "gui/updisp.h"
#include "platform/MCInput.h"
#include "vfx/MCVfxFunctions.h"

namespace
{
    /// <summary>The cursor shapes as pictures, made by MCHardwareCursorPreload.</summary>
    std::vector<MCCursorImage> ShapeImages;

    /// <summary>What this frame's cursor carries (MCHardwareCursorCarry).</summary>
    bool Carrying = false;
    MCCursorImage Carried;

    /// <summary>Draws a shape onto a pane of <paramref name="fill"/>, hot spot at (hotX, hotY).</summary>
    std::vector<uint8_t> DrawShape(void* shapeTable, int32_t shapeNum, int32_t width, int32_t height, int32_t hotX,
                                   int32_t hotY, uint8_t fill)
    {
        std::vector<uint8_t> pixels(static_cast<size_t>(width) * height, fill);
        MCWindow window{};
        window.Buffer = pixels.data();
        window.XMax = width - 1;
        window.YMax = height - 1;
        MCPane pane{&window, 0, 0, width - 1, height - 1};
        AGShapeDraw(&pane, shapeTable, shapeNum, hotX, hotY);
        return pixels;
    }
}

MCCursorImage MCCursorImageFromShape(void* shapeTable, int32_t shapeNum)
{
    const int32_t resolution = VfxShapeResolution(shapeTable, shapeNum);
    const int32_t minXY = VfxShapeMinxy(shapeTable, shapeNum);
    const int32_t width = resolution >> 16;
    const int32_t height = resolution & 0xffff;

    if (width <= 0 || height <= 0)
    {
        return {};
    }

    const int32_t hotX = -(minXY >> 16);
    const int32_t hotY = -static_cast<int16_t>(minXY);
    // Drawn twice, over 0 and over 255: a pixel the shape skips keeps the background, so it differs between the two.
    const std::vector<uint8_t> overBlack = DrawShape(shapeTable, shapeNum, width, height, hotX, hotY, 0);
    const std::vector<uint8_t> overWhite = DrawShape(shapeTable, shapeNum, width, height, hotX, hotY, 0xff);
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

MCCursorImage MCCursorImageFromPane(MCPane* pane, int32_t hotX, int32_t hotY)
{
    if (pane == nullptr || pane->Window == nullptr || pane->Window->Buffer == nullptr)
    {
        return {};
    }

    const int32_t width = pane->X1 - pane->X0 + 1;
    const int32_t height = pane->Y1 - pane->Y0 + 1;

    if (width <= 0 || height <= 0)
    {
        return {};
    }

    MCCursorImage image = MCCursorImage::Blank(width, height, hotX, hotY);
    const int32_t pitch = pane->Window->XMax + 1;

    for (int32_t y = 0; y < height; ++y)
    {
        const uint8_t* row = pane->Window->Buffer + static_cast<ptrdiff_t>(pane->Y0 + y) * pitch + pane->X0;
        std::memcpy(image.Pixels.data() + static_cast<size_t>(y) * width, row, static_cast<size_t>(width));
    }

    std::ranges::fill(image.Opaque, 1);
    return image;
}

void MCHardwareCursorPreload()
{
    ShapeImages.assign(128, {});

    for (size_t shape = 0; shape < ShapeImages.size() && CursorShapes != nullptr; ++shape)
    {
        if (CursorShapes[shape] != nullptr)
        {
            ShapeImages[shape] = MCCursorImageFromShape(CursorShapes[shape], 0);
        }
    }

    MCCursor::Preload(ShapeImages);
}

void MCHardwareCursorNewFrame()
{
    Carrying = false;
}

bool MCHardwareCursorCarry(MCGuiObject* object, MCPane* pixels)
{
    if (GSoftwareCursor != 0 || object == nullptr || MCInput::Display() == nullptr)
    {
        return false;
    }

    // The object was placed from the mouse position of the message the game handled last; the same offset under
    // the live mouse keeps it where the game put it relative to the cursor.
    const MCPoint mouse = MCInput::GetMessagePos();
    const int32_t offsetX = mouse.x - object->GlobalX();
    const int32_t offsetY = mouse.y - object->GlobalY();

    if (offsetX < 0 || offsetY < 0 || offsetX >= object->Width() || offsetY >= object->Height())
    {
        return false;
    }

    MCCursorImage image = MCCursorImageFromPane(pixels, offsetX, offsetY);

    if (image.Width == 0)
    {
        return false;
    }

    Carried = std::move(image);
    Carrying = true;
    return true;
}

void MCHardwareCursorUpdate()
{
    const int32_t shape = Application->CursorShape;
    const bool hasShape = shape >= 0 && static_cast<size_t>(shape) < ShapeImages.size() &&
                          ShapeImages[static_cast<size_t>(shape)].Width > 0;

    if (Carrying)
    {
        Carrying = false;
        MCCursor::Show(hasShape ? MCCursorImage::Overlay(Carried, ShapeImages[static_cast<size_t>(shape)]) : Carried);
    }
    else if (hasShape)
    {
        MCCursor::ShowShape(static_cast<size_t>(shape));
    }
    else
    {
        MCCursor::Hide();
    }
}
