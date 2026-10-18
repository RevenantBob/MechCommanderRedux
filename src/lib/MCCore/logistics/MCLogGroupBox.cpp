#include "stdafx.h"
#include "logistics/MCLogGroupBox.h"

MCLogGroupBox::MCLogGroupBox(uint8_t outlineColor, uint8_t fillColor)
    : _OutlineColor(outlineColor), _FillColor(fillColor)
{
}

auto MCLogGroupBox::Draw() -> void
{
    const auto right = static_cast<int16_t>(Width() - 1);
    const auto bottom = static_cast<int16_t>(Height() - 1);
    FillBox(0, 0, right, bottom, _FillColor);
    FillBox(0, 0, right, 0, _OutlineColor);
    FillBox(0, bottom, right, bottom, _OutlineColor);
    FillBox(0, 0, 0, bottom, _OutlineColor);
    FillBox(right, 0, right, bottom, _OutlineColor);
}

auto MCLogGroupBox::FindObject(int32_t xPos, int32_t yPos) -> std::shared_ptr<MCGuiObject>
{
    std::shared_ptr<MCGuiObject> found = MCLogObject::FindObject(xPos, yPos);
    return found.get() == this ? nullptr : found;
}
