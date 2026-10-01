#include "stdafx.h"
#include "engine/celine.h"
#include "camera/camera.h"
#include "vfx/vfxfuncs.h"

LineElement::LineElement(vector_2d& start, vector_2d& end, int32_t _color, uint8_t* _fadeTable, int32_t _depth,
                         int32_t _endColor)
    : Element(_depth)
{
    startPos.x = start.x;
    startPos.y = start.y;
    endPos.x = end.x;
    endPos.y = end.y;
    color = _color;
    fadeTable = _fadeTable;
    endColor = _endColor;
}

auto LineElement::draw() -> void
{
    if (fadeTable != nullptr)
    {
        if (endColor == -1)
        {
            VFX_line_draw(globalPane, static_cast<int32_t>(startPos.x), static_cast<int32_t>(startPos.y),
                          static_cast<int32_t>(endPos.x), static_cast<int32_t>(endPos.y), LD_TRANSLATE,
                          reinterpret_cast<intptr_t>(fadeTable));
        }

        return;
    }

    if (endColor == -1)
    {
        VFX_line_draw(globalPane, static_cast<int32_t>(startPos.x), static_cast<int32_t>(startPos.y),
                      static_cast<int32_t>(endPos.x), static_cast<int32_t>(endPos.y), LD_DRAW, color);
    }
}
