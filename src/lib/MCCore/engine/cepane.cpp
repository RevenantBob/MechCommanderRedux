#include "stdafx.h"
#include "engine/cepane.h"
#include "camera/camera.h"
#include "vfx/vfx.h"
#include "vfx/vfxfuncs.h"

MCPaneElement::MCPaneElement(MCPane* pane, int32_t x, int32_t y, int32_t offsetX, int32_t offsetY, int32_t width,
                             int32_t height)
    : MCElement(-y)
{
    OffsetX = offsetX;
    ShapePane = pane;
    X = x;
    Y = y;
    Height = height;
    OffsetY = offsetY;
    Width = width;
}

auto MCPaneElement::Draw() -> void
{
    DrawTransparent(GlobalPane, ShapePane->Window, X - OffsetX, Y - OffsetY, Width, Height);
}

MCDeltaElement::MCDeltaElement(uint8_t* shape, int32_t x, int32_t y, int32_t frame, int reverse, uint8_t* fadeTbl,
                               int noScaleDraw, int scaleUp)
    : MCElement(-y)
{
    ShapeTable = shape;
    FrameNum = frame;
    X = x;
    Reverse = reverse;
    Y = y;
    NoScaleDraw = noScaleDraw;
    FadeTable = fadeTbl;
    ScaleUp = scaleUp;
}

auto MCDeltaElement::Draw() -> void
{
    if (std::memcmp(ShapeTable, "1.10", 4) != 0)
    {
        return;
    }

    const int32_t count = VfxShapeCount(ShapeTable);

    if (count <= FrameNum + 1)
    {
        FrameNum = count - 2;
    }

    if (FadeTable == nullptr)
    {
        AGShapeDraw(GlobalPane, ShapeTable, 0, X, Y);
        AGShapeDraw(GlobalPane, ShapeTable, FrameNum + 1, X, Y);
        return;
    }

    AGShapeLookaside(FadeTable);
    AGShapeTranslateDraw(GlobalPane, ShapeTable, 0, X, Y);
    AGShapeTranslateDraw(GlobalPane, ShapeTable, FrameNum + 1, X, Y);
}
