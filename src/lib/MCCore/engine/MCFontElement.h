#pragma once

#include "engine/MCElement.h"
#include "lib/MCVector2D.h"
#include "platform/MCRenderer.h"

class MCGuiFont;

/// <summary>A string written with an interface font at a screen point.</summary>
/// <remarks>Original source: <c>engine\cfont.cpp</c>.</remarks>
class MCFontElement : public MCElement
{
public:
    /// <summary><paramref name="text"/> in <paramref name="font"/> at <paramref name="pos"/> and <paramref name="depth"/>.</summary>
    MCFontElement(MCOptionalRef<MCGuiFont> font, const MCVector2D& pos, std::string_view text, int32_t depth);

    /// <summary>Writes the string into <c>GlobalPane</c> (nothing without a font).</summary>
    void Draw() override;

    /// <summary>The font (borrowed for the frame), if any.</summary>
    MCOptionalRef<MCGuiFont> Font;
    /// <summary>Where the string goes.</summary>
    MCVector2D Position;
    /// <summary>The string.</summary>
    std::string Text;
};
