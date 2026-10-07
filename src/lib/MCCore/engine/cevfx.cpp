#include "stdafx.h"
#include "engine/cevfx.h"
#include "camera/camera.h"
#include "engine/scale.h"
#include "vfx/vfxfuncs.h"

char CurrentVfx[8] = {};
char CurrentVfx2[8] = {};
std::array<uint8_t, TEMP_BUFFER_SIZE> TempBuffer{};

MCVfxElement::MCVfxElement(uint8_t* shape, int32_t x, int32_t y, int32_t frame, int reverse, uint8_t* fadeTbl,
                           int noScaleDraw, int scaleUp)
    : MCElement(-y)
{
    X = x;
    Y = y;
    Reverse = reverse;
    FadeTable = fadeTbl;
    ShapeTable = shape;
    FrameNum = frame;
    NoScaleDraw = noScaleDraw;
    ScaleUp = scaleUp;
    const int32_t count = VfxShapeCount(shape);

    if (count <= frame)
    {
        FrameNum = count - 1;
    }
}

MCVfxElement::MCVfxElement(uint8_t* shape, float x, float y, int32_t frame, int reverse, uint8_t* fadeTbl,
                           int noScaleDraw, int scaleUp)
    : MCElement(-y)
{
    ShapeTable = shape;
    X = static_cast<int16_t>(static_cast<int32_t>(std::floor(static_cast<double>(x))));
    Y = static_cast<int16_t>(static_cast<int32_t>(std::floor(static_cast<double>(y))));
    Reverse = reverse;
    FrameNum = frame;
    FadeTable = fadeTbl;
    NoScaleDraw = noScaleDraw;
    ScaleUp = scaleUp;
    const int32_t count = VfxShapeCount(shape);

    if (count <= frame)
    {
        FrameNum = count - 1;
    }
}

auto MCVfxElement::Draw() -> void
{
    // Port fix: bounded copies. Nothing sets the names, so the original strcpy'd whatever the element pool held.
    std::memcpy(CurrentVfx, Name, sizeof(CurrentVfx));
    CurrentVfx[sizeof(CurrentVfx) - 1] = 0;
    std::memcpy(CurrentVfx2, Name2, sizeof(CurrentVfx2));
    CurrentVfx2[sizeof(CurrentVfx2) - 1] = 0;

    if (NoScaleDraw == 0)
    {
        ScaleDraw(ShapeTable, static_cast<uint32_t>(FrameNum), X, Y, Reverse, FadeTable, ScaleUp);
        return;
    }

    if (std::memcmp(ShapeTable, "1.10", 4) != 0)
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
        AGShapeTranslateTransform(GlobalPane, ShapeTable, FrameNum, X, Y, TempBuffer.data(), Reverse, 1);
        return;
    }

    AGShapeTransform(GlobalPane, ShapeTable, FrameNum, X, Y, TempBuffer.data(), Reverse, 1);
}
