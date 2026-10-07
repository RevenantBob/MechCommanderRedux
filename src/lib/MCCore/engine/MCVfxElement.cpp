#include "stdafx.h"
#include "engine/MCVfxElement.h"
#include "camera/camera.h"
#include "engine/MCScaleDraw.h"
#include "vfx/MCVfxFunctions.h"

MCVfxElement::MCVfxElement(uint8_t* shape, int32_t x, int32_t y, int32_t frame, int32_t reverse, uint8_t* fadeTable,
                           bool noScaleDraw)
    : MCElement(-y)
    , ShapeTable(shape)
    , FrameNum(frame)
    , X(x)
    , Y(y)
    , Reverse(reverse)
    , FadeTable(fadeTable)
    , NoScaleDraw(noScaleDraw)
{
    ClampFrame();
}

MCVfxElement::MCVfxElement(uint8_t* shape, float x, float y, int32_t frame, int32_t reverse, uint8_t* fadeTable,
                           bool noScaleDraw)
    : MCElement(-y)
    , ShapeTable(shape)
    , FrameNum(frame)
    , Reverse(reverse)
    , FadeTable(fadeTable)
    , NoScaleDraw(noScaleDraw)
{
    // Rounded down and cut to 16 bits, as the original's __ftol into a short.
    X = static_cast<int16_t>(static_cast<int32_t>(std::floor(static_cast<double>(x))));
    Y = static_cast<int16_t>(static_cast<int32_t>(std::floor(static_cast<double>(y))));
    ClampFrame();
}

auto MCVfxElement::ClampFrame() -> void
{
    const int32_t count = VfxShapeCount(ShapeTable);

    if (count <= FrameNum)
    {
        FrameNum = count - 1;
    }
}

auto MCVfxElement::Draw() -> void
{
    if (!NoScaleDraw)
    {
        ScaleDraw(ShapeTable, static_cast<uint32_t>(FrameNum), X, Y, Reverse, FadeTable);
        return;
    }

    if (!VfxIsShapeTable(ShapeTable))
    {
        return;
    }

    if (Reverse == 0)
    {
        if (FadeTable == nullptr)
        {
            AGShapeDraw(GlobalPane, ShapeTable, FrameNum, X, Y);
            return;
        }

        AGShapeLookaside(FadeTable);
        AGShapeTranslateDraw(GlobalPane, ShapeTable, FrameNum, X, Y);
        return;
    }

    if (FadeTable != nullptr)
    {
        AGShapeLookaside(FadeTable);
        AGShapeTranslateTransform(GlobalPane, ShapeTable, FrameNum, X, Y, Reverse, 1);
        return;
    }

    AGShapeTransform(GlobalPane, ShapeTable, FrameNum, X, Y, Reverse, 1);
}
