#pragma once

#include "engine/celement.h"
#include "lib/cvmath.h"

class aFont;

/// <summary>A string written with an interface font at a screen point.</summary>
/// <remarks>Original source: <c>engine\cfont.cpp</c>, 0x24 bytes (the callers allocate 0x24, one 0x3c).</remarks>
class FontElement : public Element
{
public:
    /// <summary><paramref name="_text"/> in <paramref name="_font"/> at <paramref name="pos"/> and <paramref name="_depth"/>.</summary>
    /// <remarks>MCX.EXE @ 0x00644870</remarks>
    FontElement(aFont* _font, vector_2d& pos, char* _text, int32_t _depth);

    /// <summary>Writes the string into <c>globalPane</c> (nothing without a font or text).</summary>
    /// <remarks>MCX.EXE @ 0x006448b0; slot 0</remarks>
    void draw() override;

    /// <summary>The font.</summary>
    aFont* font; // +0x0c
    /// <summary>Where the string goes.</summary>
    vector_2d position; // +0x10
    /// <summary>The string (not copied: it must live until the frame is drawn).</summary>
    char* text;        // +0x18
    int32_t unknown1C; // +0x1c (never accessed)
    int32_t unknown20; // +0x20 (never accessed)
};
