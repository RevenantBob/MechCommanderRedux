#include "stdafx.h"
#include "engine/cellip.h"
#include "camera/camera.h"
#include "vfx/vfxfuncs.h"

EllipseElement::EllipseElement(vector_2d& _center, vector_2d& _size, int32_t _color, int32_t _depth) : Element(_depth)
{
    center.x = _center.x;
    center.y = _center.y;
    size.x = _size.x;
    color = _color;
    size.y = _size.y;
}

auto EllipseElement::draw() -> void
{
    AG_ellipse_draw(globalPane, static_cast<int32_t>(center.x), static_cast<int32_t>(center.y),
                    static_cast<int32_t>(size.x), static_cast<int32_t>(size.y), color);
}
