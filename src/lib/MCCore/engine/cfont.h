#pragma once

#include "engine/celement.h"
#include "lib/MCFrameOfRef.h"
#include "lib/MCVector2D.h"
#include "lib/MCDice.h"

class MCGuiFont;

/// <summary>A string written with an interface font at a screen point.</summary>
/// <remarks>Original source: <c>engine\cfont.cpp</c>, 0x24 bytes (the callers allocate 0x24, one 0x3c).</remarks>
class MCFontElement : public MCElement
{
public:
    /// <summary><paramref name="text"/> in <paramref name="font"/> at <paramref name="pos"/> and <paramref name="depth"/>.</summary>
    MCFontElement(MCGuiFont* font, MCVector2D& pos, char* text, int32_t depth);

    /// <summary>Writes the string into <c>globalPane</c> (nothing without a font or text).</summary>
    void Draw() override;

    /// <summary>The font.</summary>
    MCGuiFont* Font;
    /// <summary>Where the string goes.</summary>
    MCVector2D Position;
    /// <summary>The string (not copied: it must live until the frame is drawn).</summary>
    char* Text;
};
