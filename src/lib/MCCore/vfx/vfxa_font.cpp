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

int32_t VfxFontHeight(void* font)
{
    return MCVfxRead32(static_cast<uint8_t*>(font) + 8);
}

int32_t VfxCharacterWidth(void* font, int32_t character)
{
    return MCVfxRead32(Glyph(font, character));
}

int32_t VfxCharacterDraw(MCPane* pane, int32_t x, int32_t y, void* font, int32_t character, uint8_t* colorTranslate)
{
    MCVfxClip clip;
    const int32_t status = MCVfxClipPane(pane, clip);

    if (status != 0)
    {
        return status;
    }

    x += clip.PaneX;
    y += clip.PaneY;
    int32_t rows = VfxFontHeight(font);
    const uint8_t* glyph = Glyph(font, character);
    const int32_t width = MCVfxRead32(glyph);

    if (width == 0)
    {
        return 0;
    }

    // Where in the glyph the drawn part starts.
    int32_t sourceX = 0;
    int32_t sourceY = 0;

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

        sourceX -= over;
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
        sourceY -= over;
    }

    MCGlyphCommand command;
    command.Font = font;
    command.Character = character;
    command.X = x;
    command.Y = y;
    command.SourceX = sourceX;
    command.SourceY = sourceY;
    command.Columns = columns;
    command.Rows = rows;
    command.Table = colorTranslate;
    MCRenderer::For(pane->Window).Glyph(pane->Window, command);
    return width;
}

void VfxStringDraw(MCPane* pane, int32_t x, int32_t y, void* font, const char* string, uint8_t* colorTranslate)
{
    // OB-124: the asm drew the first character before looking for the terminator (an empty string drew character
    // 0), and moved x back by a negative (error) result.
    for (; *string != 0; ++string)
    {
        const int32_t width = VfxCharacterDraw(pane, x, y, font, static_cast<uint8_t>(*string), colorTranslate);

        if (width < 0)
        {
            return;
        }

        x += width;
    }
}
