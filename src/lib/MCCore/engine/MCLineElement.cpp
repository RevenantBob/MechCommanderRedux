#include "stdafx.h"
#include "engine/MCLineElement.h"
#include "camera/MCCamera.h"
#include "vfx/MCVfxFunctions.h"

MCLineElement::MCLineElement(const MCVector2D& start, const MCVector2D& end, int32_t color, const uint8_t* fadeTable,
                             int32_t depth, int32_t endColor)
    : MCElement(depth), StartPos(start), EndPos(end), Color(color), EndColor(endColor), FadeTable(fadeTable)
{
}

auto MCLineElement::Draw() -> void
{
    // A two-colour line (endColor set) draws nothing.
    if (EndColor != -1)
    {
        return;
    }

    const auto x0 = static_cast<int32_t>(StartPos.X);
    const auto y0 = static_cast<int32_t>(StartPos.Y);
    const auto x1 = static_cast<int32_t>(EndPos.X);
    const auto y1 = static_cast<int32_t>(EndPos.Y);

    if (FadeTable != nullptr)
    {
        VfxLineTranslate(GlobalPane, x0, y0, x1, y1, FadeTable);
        return;
    }

    VfxLineDraw(GlobalPane, x0, y0, x1, y1, Color);
}
