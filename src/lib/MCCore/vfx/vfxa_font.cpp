#include "stdafx.h"
#include "vfx/vfxint.h"

// VFX's bitmap-font routines (vfxa.asm in MCX.EXE). The game's aFont (engine\font.cpp) loads data\fonts\*.fnt whole
// and draws through these with its own 256-byte colour table. Layout: see the fonts section of vfx/vfxfuncs.h.

namespace
{
    /// <summary>The glyph of <paramref name="character"/>: its width dword, then its pixels.</summary>
    uint8_t* Glyph(void* font, int32_t character)
    {
        uint8_t* base = static_cast<uint8_t*>(font);
        return base + MCVfxRead32(base + 0x10 + static_cast<intptr_t>(character) * 4);
    }
}

int32_t VFX_font_height(void* font)
{
    return MCVfxRead32(static_cast<uint8_t*>(font) + 8);
}

int32_t VFX_character_width(void* font, int32_t character)
{
    return MCVfxRead32(Glyph(font, character));
}

int32_t VFX_character_draw(PANE* pane, int32_t x, int32_t y, void* font, int32_t character, uint8_t* colorTranslate)
{
    MCVfxClip clip;
    const int32_t status = MCVfxClipPane(pane, clip);

    if (status != 0)
    {
        return status;
    }

    x += clip.PaneX;
    y += clip.PaneY;
    int32_t rows = VFX_font_height(font);
    const uint8_t* glyph = Glyph(font, character);
    const int32_t width = MCVfxRead32(glyph);

    if (width == 0)
    {
        return 0;
    }

    const uint8_t* source = glyph + 4;

    // Clip right, left, bottom, top in that order, as the asm does.
    int32_t columns = width;
    int32_t over = clip.X1 + 1 - columns - x;

    if (over < 0)
    {
        columns += over;

        if (columns <= 0)
        {
            return width;
        }
    }

    over = x - clip.X0;

    if (over < 0)
    {
        columns += over;

        if (columns <= 0)
        {
            return width;
        }

        source -= over;
        x -= over;
    }

    over = clip.Y1 + 1 - rows - y;

    if (over < 0)
    {
        rows += over;

        if (rows <= 0)
        {
            return width;
        }
    }

    over = y - clip.Y0;

    if (over < 0)
    {
        rows += over;

        if (rows <= 0)
        {
            return width;
        }

        y -= over;
        source -= static_cast<intptr_t>(over) * width;
    }

    uint8_t* dest = clip.At(x, y);

    for (; rows > 0; --rows)
    {
        if (colorTranslate == nullptr)
        {
            std::memcpy(dest, source, static_cast<size_t>(columns));
        }
        else
        {
            for (int32_t i = 0; i < columns; ++i)
            {
                const uint8_t pixel = colorTranslate[source[i]];

                if (pixel != 0xff)
                {
                    dest[i] = pixel;
                }
            }
        }

        source += width;
        dest += clip.Stride;
    }

    return width;
}

void VFX_string_draw(PANE* pane, int32_t x, int32_t y, void* font, const char* string, uint8_t* colorTranslate)
{
    // Original behaviour: the first character is drawn before the terminator is looked at, and a negative (error)
    // result moves x back.
    do
    {
        x += VFX_character_draw(pane, x, y, font, static_cast<uint8_t>(*string), colorTranslate);
        ++string;
    } while (*string != 0);
}
