#include "stdafx.h"
#include "vfx/MCVfxClip.h"

// VFX's bitmap-font routines (vfxa.asm in MCX.EXE). The game's aFont (engine\font.cpp) loads data\fonts\*.fnt whole
// and draws through these with its own 256-byte colour table. Layout: see the fonts section of vfx/MCVfxFunctions.h.

namespace
{
    /// <summary>The glyph of <paramref name="character"/>: its width dword, then its pixels.</summary>
    std::span<const uint8_t> Glyph(std::span<const uint8_t> font, int32_t character)
    {
        return font.subspan(static_cast<size_t>(MCVfxRead32(font, 0x10 + static_cast<size_t>(character) * 4)));
    }
}

int32_t VfxFontHeight(std::span<const uint8_t> font)
{
    return MCVfxRead32(font, 8);
}

bool VfxFontFits(std::span<const uint8_t> font)
{
    if (font.size() < 0x10)
    {
        return false;
    }

    const int64_t count = MCVfxRead32(font, 4);
    const int64_t height = VfxFontHeight(font);

    if (count < 0 || height < 0 || 0x10 + count * 4 > static_cast<int64_t>(font.size()))
    {
        return false;
    }

    for (int64_t character = 0; character < count; character++)
    {
        const int64_t offset = static_cast<uint32_t>(MCVfxRead32(font, 0x10 + static_cast<size_t>(character) * 4));

        if (offset + 4 > static_cast<int64_t>(font.size()))
        {
            return false;
        }

        const int64_t width = MCVfxRead32(font, static_cast<size_t>(offset));

        if (width < 0 || offset + 4 + width * height > static_cast<int64_t>(font.size()))
        {
            return false;
        }
    }

    return true;
}

int32_t VfxCharacterWidth(std::span<const uint8_t> font, int32_t character)
{
    // A character past the font's table has no glyph (issue #25; DIM10.FNT lists 140, and the original read whatever
    // followed the table as offsets).
    if (character < 0 || character >= MCVfxRead32(font, 4))
    {
        return 0;
    }

    return MCVfxRead32(Glyph(font, character));
}

int32_t VfxCharacterDraw(const MCPane& pane, int32_t x, int32_t y, std::span<const uint8_t> font, int32_t character,
                         std::optional<MCColorTable> colorTranslate)
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
    const int32_t width = VfxCharacterWidth(font, character);

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
    MCRenderer::For(clip.Target()).Glyph(clip.Target(), command);
    return width;
}

void VfxStringDraw(const MCPane& pane, int32_t x, int32_t y, std::span<const uint8_t> font, std::string_view string,
                   std::optional<MCColorTable> colorTranslate)
{
    // OB-124: the asm drew the first character before looking for the terminator (an empty string drew character
    // 0), and moved x back by a negative (error) result.
    for (const char c : string)
    {
        const int32_t width = VfxCharacterDraw(pane, x, y, font, static_cast<uint8_t>(c), colorTranslate);

        if (width < 0)
        {
            return;
        }

        x += width;
    }
}
