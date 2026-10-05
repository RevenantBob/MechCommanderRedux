#include "stdafx.h"
#include "engine/cevfx.h"
#include "camera/camera.h"
#include "engine/scale.h"
#include "vfx/vfxfuncs.h"

char CurrentVFX[8] = {};
char CurrentVFX2[8] = {};
std::array<uint8_t, TEMP_BUFFER_SIZE> tempBuffer{};

VFXElement::VFXElement(uint8_t* _shape, int32_t _x, int32_t _y, int32_t frame, int _reverse, uint8_t* fadeTbl,
                       int _noScaleDraw, int _scaleUp)
    : Element(-_y)
{
    x = _x;
    y = _y;
    reverse = _reverse;
    fadeTable = fadeTbl;
    shapeTable = _shape;
    frameNum = frame;
    noScaleDraw = _noScaleDraw;
    scaleUp = _scaleUp;
    const int32_t count = VFX_shape_count(_shape);

    if (count <= frame)
    {
        frameNum = count - 1;
    }
}

VFXElement::VFXElement(uint8_t* _shape, float _x, float _y, int32_t frame, int _reverse, uint8_t* fadeTbl,
                       int _noScaleDraw, int _scaleUp)
    : Element(-_y)
{
    shapeTable = _shape;
    x = static_cast<int16_t>(static_cast<int32_t>(std::floor(static_cast<double>(_x))));
    y = static_cast<int16_t>(static_cast<int32_t>(std::floor(static_cast<double>(_y))));
    reverse = _reverse;
    frameNum = frame;
    fadeTable = fadeTbl;
    noScaleDraw = _noScaleDraw;
    scaleUp = _scaleUp;
    const int32_t count = VFX_shape_count(_shape);

    if (count <= frame)
    {
        frameNum = count - 1;
    }
}

auto VFXElement::draw() -> void
{
    // Port fix: bounded copies. Nothing sets the names, so the original strcpy'd whatever the element pool held.
    std::memcpy(CurrentVFX, name, sizeof(CurrentVFX));
    CurrentVFX[sizeof(CurrentVFX) - 1] = 0;
    std::memcpy(CurrentVFX2, name2, sizeof(CurrentVFX2));
    CurrentVFX2[sizeof(CurrentVFX2) - 1] = 0;

    if (noScaleDraw == 0)
    {
        scaleDraw(shapeTable, static_cast<uint32_t>(frameNum), x, y, reverse, fadeTable, scaleUp);
        return;
    }

    if (std::memcmp(shapeTable, "1.10", 4) != 0)
    {
        return;
    }

    if (reverse == 0)
    {
        if (fadeTable == nullptr)
        {
            AG_shape_draw(globalPane, shapeTable, frameNum, x, y);
            return;
        }

        AG_shape_lookaside(fadeTable);
        AG_shape_translate_draw(globalPane, shapeTable, frameNum, x, y);
        return;
    }

    if (fadeTable != nullptr)
    {
        AG_shape_lookaside(fadeTable);
        AG_shape_translate_transform(globalPane, shapeTable, frameNum, x, y, tempBuffer.data(), reverse, 1);
        return;
    }

    AG_shape_transform(globalPane, shapeTable, frameNum, x, y, tempBuffer.data(), reverse, 1);
}
