#include "stdafx.h"
#include "engine/MCFontElement.h"
#include "camera/MCCamera.h"
#include "gui/afont.h"

MCFontElement::MCFontElement(MCGuiFont* font, const MCVector2D& pos, std::string_view text, int32_t depth)
    : MCElement(depth), Font(font), Position(pos), Text(text)
{
}

auto MCFontElement::Draw() -> void
{
    if (Font != nullptr)
    {
        Font->WriteString(GlobalPane, static_cast<int32_t>(Position.X), static_cast<int32_t>(Position.Y),
                          reinterpret_cast<uint8_t*>(Text.data()), -1);
    }
}
