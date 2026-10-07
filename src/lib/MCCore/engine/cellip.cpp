#include "stdafx.h"
#include "engine/cellip.h"
#include "camera/camera.h"
#include "vfx/vfxfuncs.h"

MCEllipseElement::MCEllipseElement(MCVector2D& center, MCVector2D& size, int32_t color, int32_t depth)
    : MCElement(depth)
{
    Center.X = center.X;
    Center.Y = center.Y;
    Size.X = size.X;
    Color = color;
    Size.Y = size.Y;
}

auto MCEllipseElement::Draw() -> void
{
    AGEllipseDraw(GlobalPane, static_cast<int32_t>(Center.X), static_cast<int32_t>(Center.Y),
                  static_cast<int32_t>(Size.X), static_cast<int32_t>(Size.Y), Color);
}
