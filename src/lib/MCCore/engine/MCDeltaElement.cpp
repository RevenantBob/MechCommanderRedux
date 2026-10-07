#include "stdafx.h"
#include "engine/MCDeltaElement.h"
#include "camera/MCCamera.h"
#include "vfx/MCVfxFunctions.h"

MCDeltaElement::MCDeltaElement(uint8_t* shape, int32_t x, int32_t y, int32_t frame, uint8_t* fadeTable)
    : MCElement(-y), ShapeTable(shape), FrameNum(frame), X(x), Y(y), FadeTable(fadeTable)
{
}

auto MCDeltaElement::Draw() -> void
{
    if (!VfxIsShapeTable(ShapeTable))
    {
        return;
    }

    const int32_t count = VfxShapeCount(ShapeTable);

    if (count <= FrameNum + 1)
    {
        FrameNum = count - 2;
    }

    if (FadeTable == nullptr)
    {
        AGShapeDraw(GlobalPane, ShapeTable, 0, X, Y);
        AGShapeDraw(GlobalPane, ShapeTable, FrameNum + 1, X, Y);
        return;
    }

    AGShapeLookaside(FadeTable);
    AGShapeTranslateDraw(GlobalPane, ShapeTable, 0, X, Y);
    AGShapeTranslateDraw(GlobalPane, ShapeTable, FrameNum + 1, X, Y);
}
