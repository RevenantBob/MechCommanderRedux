#include "stdafx.h"
#include "engine/cfont.h"
#include "camera/camera.h"
#include "gui/afont.h"

MCFontElement::MCFontElement(MCGuiFont* font, MCVector2D& pos, char* text, int32_t depth) : MCElement(depth)
{
    Position.X = pos.X;
    Text = text;
    Position.Y = pos.Y;
    Font = font;
}

auto MCFontElement::Draw() -> void
{
    if (Font != nullptr && Text != nullptr)
    {
        Font->WriteString(GlobalPane, static_cast<int32_t>(Position.X), static_cast<int32_t>(Position.Y),
                          reinterpret_cast<uint8_t*>(Text), -1);
    }
}
