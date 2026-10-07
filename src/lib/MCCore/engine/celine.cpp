#include "stdafx.h"
#include "engine/celine.h"
#include "camera/camera.h"
#include "vfx/vfxfuncs.h"

MCLineElement::MCLineElement(MCVector2D& start, MCVector2D& end, int32_t color, uint8_t* fadeTable, int32_t depth,
                             int32_t endColor)
    : MCElement(depth)
{
    StartPos.X = start.X;
    StartPos.Y = start.Y;
    EndPos.X = end.X;
    EndPos.Y = end.Y;
    Color = color;
    FadeTable = fadeTable;
    EndColor = endColor;
}

auto MCLineElement::Draw() -> void
{
    if (FadeTable != nullptr)
    {
        if (EndColor == -1)
        {
            VfxLineDraw(GlobalPane, static_cast<int32_t>(StartPos.X), static_cast<int32_t>(StartPos.Y),
                        static_cast<int32_t>(EndPos.X), static_cast<int32_t>(EndPos.Y), LD_TRANSLATE,
                        reinterpret_cast<intptr_t>(FadeTable));
        }

        return;
    }

    if (EndColor == -1)
    {
        VfxLineDraw(GlobalPane, static_cast<int32_t>(StartPos.X), static_cast<int32_t>(StartPos.Y),
                    static_cast<int32_t>(EndPos.X), static_cast<int32_t>(EndPos.Y), LD_DRAW, Color);
    }
}
