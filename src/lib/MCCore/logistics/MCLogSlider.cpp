#include "stdafx.h"
#include "logistics/MCLogSlider.h"
#include "gui/MCGuiEvent.h"
#include "main/MCGamePaths.h"
#include "vfx/MCVfxFunctions.h"

MCLogSlider::MCLogSlider() : ThumbPort(std::make_unique<MCLogPort>())
{
    ThumbPort->Load(std::format("{}prefs_02.tga", ArtPath));
}

MCLogSlider::~MCLogSlider()
{
    MCLogSlider::Destroy();
}

auto MCLogSlider::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, [[maybe_unused]] const char* name)
    -> int32_t
{
    const int32_t result = MCLogObject::Init(xPos, yPos, width, height);
    SetTransparent(true);
    return result;
}

auto MCLogSlider::Destroy() -> void
{
    ThumbPort.reset();
    MCLogObject::Destroy();
}

auto MCLogSlider::ThumbX() -> int32_t
{
    // The value's share of the travel (x87).
    const int32_t travel = Width() - ThumbPort->Width();
    return static_cast<int32_t>(static_cast<double>(travel) *
                                (static_cast<double>(CurrentValue - MinValue) / (MaxValue - MinValue)));
}

auto MCLogSlider::Draw() -> void
{
    VfxPaneWipe(Lport()->Frame(), 0xff);
    ThumbPort->CopyTo(Lport()->Frame(), ThumbX(), 0, false);
}

auto MCLogSlider::SetCurrentValue(int32_t value) -> void
{
    // The minimum wins over the maximum (std::clamp needs them in order).
    CurrentValue = value < MinValue ? MinValue : std::min(value, MaxValue);
}

auto MCLogSlider::ValueAt(int32_t mouseX) -> int32_t
{
    // The offset (as a float) over the travel, times the range.
    const auto offset = static_cast<float>(mouseX - X());
    const int32_t travel = Width() - ThumbPort->Width();
    return static_cast<int32_t>(static_cast<double>(offset) / travel * (MaxValue - MinValue)) + MinValue;
}

auto MCLogSlider::HandleEvent(MCGuiEvent* event) -> void
{
    switch (event->Type)
    {
        case MCGuiEventType::LeftButtonDown:
        {
            GuiSystem()->Grab(this);

            if (GuiSystem()->GrabbedObject() != nullptr)
            {
                SetCurrentValue(ValueAt(event->X));
            }
            break;
        }
        case MCGuiEventType::LeftButtonUp:
        {
            GuiSystem()->Release();
            SetCurrentValue(ValueAt(event->X));
            break;
        }
        case MCGuiEventType::MouseMove:
        {
            if (GuiSystem()->GrabbedObject() != nullptr)
            {
                SetCurrentValue(ValueAt(event->X));
            }
            break;
        }
        default:
            break;
    }

    if (EventRoutine != nullptr)
    {
        EventRoutine(this, event);
    }
}
