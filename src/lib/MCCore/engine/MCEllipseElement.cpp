#include "stdafx.h"
#include "engine/MCEllipseElement.h"
#include "camera/camera.h"
#include "vfx/MCVfxFunctions.h"

MCEllipseElement::MCEllipseElement(const MCVector2D& center, const MCVector2D& size, int32_t color, int32_t depth)
    : MCElement(depth), Center(center), Size(size), Color(color)
{
}

auto MCEllipseElement::Draw() -> void
{
    AGEllipseDraw(GlobalPane, static_cast<int32_t>(Center.X), static_cast<int32_t>(Center.Y),
                  static_cast<int32_t>(Size.X), static_cast<int32_t>(Size.Y), Color);
}
