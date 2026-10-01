#include "stdafx.h"
#include "engine/cepane.h"
#include "camera/camera.h"
#include "vfx/vfx.h"
#include "vfx/vfxfuncs.h"

PaneElement::PaneElement(_pane* pane, int32_t _x, int32_t _y, int32_t _offsetX, int32_t _offsetY, int32_t _width,
                         int32_t _height)
    : Element(-_y)
{
    offsetX = _offsetX;
    shapePane = pane;
    x = _x;
    y = _y;
    height = _height;
    offsetY = _offsetY;
    width = _width;
}

auto PaneElement::draw() -> void
{
    DrawTransparent(globalPane, shapePane->window, x - offsetX, y - offsetY, width, height);
}

DeltaElement::DeltaElement(uint8_t* _shape, int32_t _x, int32_t _y, int32_t frame, int _reverse, uint8_t* fadeTbl,
                           int _noScaleDraw, int _scaleUp)
    : Element(-_y)
{
    shapeTable = _shape;
    frameNum = frame;
    x = _x;
    reverse = _reverse;
    y = _y;
    noScaleDraw = _noScaleDraw;
    fadeTable = fadeTbl;
    scaleUp = _scaleUp;
}

auto DeltaElement::draw() -> void
{
    if (std::memcmp(shapeTable, "1.10", 4) != 0)
    {
        return;
    }

    const int32_t count = VFX_shape_count(shapeTable);

    if (count <= frameNum + 1)
    {
        frameNum = count - 2;
    }

    if (fadeTable == nullptr)
    {
        AG_shape_draw(globalPane, shapeTable, 0, x, y);
        AG_shape_draw(globalPane, shapeTable, frameNum + 1, x, y);
        return;
    }

    AG_shape_lookaside(fadeTable);
    AG_shape_translate_draw(globalPane, shapeTable, 0, x, y);
    AG_shape_translate_draw(globalPane, shapeTable, frameNum + 1, x, y);
}
