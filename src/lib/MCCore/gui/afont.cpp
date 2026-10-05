#include "stdafx.h"
#include "gui/afont.h"
#include "gui/asystem.h"
#include "lib/aerror.h"
#include "lib/file.h"
#include "logistics/logmain.h"
#include "platform/MCRenderer.h"
#include "vfx/vfxfuncs.h"

aFont::aFont()
{
}

aFont::~aFont()
{
    destroy();
    MCRenderer::UnregisterData(colorTable, sizeof(colorTable));
}

auto aFont::init(char* fileName) -> int32_t
{
    File file;

    if (fontData != nullptr)
    {
        destroy();
    }

    char path[128];
    std::snprintf(path, sizeof(path), "%s%s", fontPath, fileName);

    if (file.open(path, READ, 0x32) != 0)
    {
        char message[256];
        std::snprintf(message, sizeof(message), "Unable to find '%s'", path);
        GeneralMsg(message);
        return -1;
    }

    const uint32_t size = file.fileSize();

    if (size == 0)
    {
        return -2;
    }

    fontData = std::make_unique<uint8_t[]>(size);
    file.read(fontData.get(), static_cast<int32_t>(size));
    file.close();
    MCRenderer::RegisterData(fontData.get(), size, MCDataKind::Shapes);

    for (int32_t i = 0; i < 0x100; i++)
    {
        colorTable[i] = static_cast<uint8_t>(i);
    }

    MCRenderer::RegisterData(colorTable, sizeof(colorTable), MCDataKind::Tables);
    return 0;
}

auto aFont::destroy() -> void
{
    if (fontData != nullptr)
    {
        MCRenderer::UnregisterData(fontData.get());
        fontData.reset();
    }
}

auto aFont::load(char* fileName) -> int32_t
{
    destroy();
    return init(fileName);
}

auto aFont::height() -> int32_t
{
    if (fontData == nullptr)
    {
        return 0;
    }

    return VFX_font_height(fontData.get());
}

auto aFont::width(uint8_t* text) -> int32_t
{
    if (text == nullptr || *text == 0 || fontData == nullptr)
    {
        return 0;
    }

    const int32_t length = static_cast<int32_t>(std::strlen(reinterpret_cast<char*>(text)));
    int32_t total = 0;

    for (int32_t i = 0; i < length; i++)
    {
        total += VFX_character_width(fontData.get(), text[i]);
    }

    return total;
}

auto aFont::width(uint8_t c) -> int32_t
{
    if (c == 0)
    {
        return 0;
    }

    if (fontData == nullptr)
    {
        return 0;
    }

    return VFX_character_width(fontData.get(), c);
}

auto aFont::writeChar(_pane* pane, int32_t xPos, int32_t yPos, char c) -> int32_t
{
    if (this == nullptr || fontData == nullptr)
    {
        return -3;
    }

    if (c != 0)
    {
        VFX_character_draw(pane, xPos, yPos, fontData.get(), static_cast<uint8_t>(c), colorTable);
    }

    return 0;
}

auto aFont::writeString(_pane* pane, int32_t xPos, int32_t yPos, uint8_t* text, int32_t maxWidth) -> int32_t
{
    if (this == nullptr || fontData == nullptr)
    {
        return -3;
    }

    if (text == nullptr || *text == 0)
    {
        return 0;
    }

    // Too wide: cut the last character off until it fits, putting back the one cut before; the cut character is
    // restored after drawing.
    uint8_t saved = 0;
    int32_t end = -1;

    if (maxWidth != -1)
    {
        end = static_cast<int32_t>(std::strlen(reinterpret_cast<char*>(text)));
        int32_t textWidth = width(text);

        while (maxWidth < textWidth && end > 0)
        {
            text[end] = saved;
            saved = text[end - 1];
            end--;
            text[end] = 0;
            textWidth = width(text);
        }
    }

    VFX_string_draw(pane, xPos, yPos, fontData.get(), reinterpret_cast<char*>(text), colorTable);

    if (maxWidth != -1)
    {
        text[end] = saved;
    }

    return 0;
}

auto aFont::writeStringToNewline(_pane* pane, int32_t xPos, int32_t yPos, uint8_t* text) -> int32_t
{
    if (this == nullptr || fontData == nullptr)
    {
        return -3;
    }

    if (text != nullptr && *text != 0)
    {
        auto* newline = reinterpret_cast<uint8_t*>(std::strchr(reinterpret_cast<char*>(text), '\n'));

        if (newline != text)
        {
            if (newline != nullptr)
            {
                *newline = 0;
            }

            VFX_string_draw(pane, xPos, yPos, fontData.get(), reinterpret_cast<char*>(text), colorTable);

            if (newline != nullptr)
            {
                *newline = '\n';
            }
        }
    }

    return 0;
}

auto aFont::charactersToWidth(uint8_t* text, int32_t maxWidth, int wordWrap) -> int32_t
{
    char* string = reinterpret_cast<char*>(text);

    if (wordWrap == 0)
    {
        // Cut characters off the end (as writeString does) until the rest fits.
        uint8_t* end = text + std::strlen(string);
        uint8_t saved = 0;

        while (text < end)
        {
            if (width(text) <= maxWidth)
            {
                break;
            }

            *end = saved;
            saved = end[-1];
            end--;
            *end = 0;
        }

        *end = saved;
        return static_cast<int32_t>(end - text);
    }

    if (width(text) <= maxWidth)
    {
        return static_cast<int32_t>(std::strlen(string));
    }

    // Cut at the last space, then the one before it, until the rest fits.
    char* space = std::strrchr(string, ' ');

    if (space == nullptr)
    {
        return -1;
    }

    *space = 0;
    while (maxWidth < width(text))
    {
        char* previous = std::strrchr(string, ' ');

        *space = ' ';
        if (previous == nullptr)
        {
            return -1;
        }

        *previous = 0;
        space = previous;
    }

    const int32_t length = static_cast<int32_t>(std::strlen(string));
    *space = ' ';
    return length;
}
