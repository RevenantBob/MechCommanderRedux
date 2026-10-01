#include "stdafx.h"
#include "engine/cfont.h"
#include "camera/camera.h"
#include "gui/afont.h"

FontElement::FontElement(aFont* _font, vector_2d& pos, char* _text, int32_t _depth) : Element(_depth)
{
    position.x = pos.x;
    text = _text;
    position.y = pos.y;
    font = _font;
}

auto FontElement::draw() -> void
{
    if (font != nullptr && text != nullptr)
    {
        font->writeString(globalPane, static_cast<int32_t>(position.x), static_cast<int32_t>(position.y),
                          reinterpret_cast<uint8_t*>(text), -1);
    }
}
