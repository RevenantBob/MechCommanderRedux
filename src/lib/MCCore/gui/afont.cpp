#include "stdafx.h"
#include "gui/afont.h"
#include "gui/asystem.h"
#include "lib/MCFatal.h"
#include "lib/MCFile.h"
#include "logistics/logmain.h"
#include "platform/MCRenderer.h"
#include "vfx/MCVfxFunctions.h"

MCGuiFont::MCGuiFont()
{
}

MCGuiFont::~MCGuiFont()
{
    Destroy();
    MCRenderer::UnregisterData(ColorTable, sizeof(ColorTable));
}

auto MCGuiFont::Init(char* fileName) -> int32_t
{
    MCFile file;

    if (FontData != nullptr)
    {
        Destroy();
    }

    char path[128];
    std::snprintf(path, sizeof(path), "%s%s", FontPath, fileName);

    if (file.Open(path) != 0)
    {
        char message[256];
        std::snprintf(message, sizeof(message), "Unable to find '%s'", path);
        GeneralMsg(message);
        return -1;
    }

    const uint32_t size = file.FileSize();

    if (size == 0)
    {
        return -2;
    }

    FontData = std::make_unique<uint8_t[]>(size);
    file.Read(FontData.get(), static_cast<int32_t>(size));
    file.Close();
    MCRenderer::RegisterData(FontData.get(), size, MCDataKind::Shapes);

    for (int32_t i = 0; i < 0x100; i++)
    {
        ColorTable[i] = static_cast<uint8_t>(i);
    }

    MCRenderer::RegisterData(ColorTable, sizeof(ColorTable), MCDataKind::Tables);
    return 0;
}

auto MCGuiFont::Destroy() -> void
{
    if (FontData != nullptr)
    {
        MCRenderer::UnregisterData(FontData.get());
        FontData.reset();
    }
}

auto MCGuiFont::Load(char* fileName) -> int32_t
{
    Destroy();
    return Init(fileName);
}

auto MCGuiFont::Height() -> int32_t
{
    if (FontData == nullptr)
    {
        return 0;
    }

    return VfxFontHeight(FontData.get());
}

auto MCGuiFont::Width(uint8_t* text) -> int32_t
{
    if (text == nullptr || *text == 0 || FontData == nullptr)
    {
        return 0;
    }

    const int32_t length = static_cast<int32_t>(std::strlen(reinterpret_cast<char*>(text)));
    int32_t total = 0;

    for (int32_t i = 0; i < length; i++)
    {
        total += VfxCharacterWidth(FontData.get(), text[i]);
    }

    return total;
}

auto MCGuiFont::Width(uint8_t c) -> int32_t
{
    if (c == 0)
    {
        return 0;
    }

    if (FontData == nullptr)
    {
        return 0;
    }

    return VfxCharacterWidth(FontData.get(), c);
}

auto MCGuiFont::WriteChar(MCPane* pane, int32_t xPos, int32_t yPos, char c) -> int32_t
{
    if (this == nullptr || FontData == nullptr)
    {
        return -3;
    }

    if (c != 0)
    {
        VfxCharacterDraw(pane, xPos, yPos, FontData.get(), static_cast<uint8_t>(c), ColorTable);
    }

    return 0;
}

auto MCGuiFont::WriteString(MCPane* pane, int32_t xPos, int32_t yPos, uint8_t* text, int32_t maxWidth) -> int32_t
{
    if (this == nullptr || FontData == nullptr)
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
        int32_t textWidth = Width(text);

        while (maxWidth < textWidth && end > 0)
        {
            text[end] = saved;
            saved = text[end - 1];
            end--;
            text[end] = 0;
            textWidth = Width(text);
        }
    }

    VfxStringDraw(pane, xPos, yPos, FontData.get(), reinterpret_cast<char*>(text), ColorTable);

    if (maxWidth != -1)
    {
        text[end] = saved;
    }

    return 0;
}

auto MCGuiFont::WriteStringToNewline(MCPane* pane, int32_t xPos, int32_t yPos, uint8_t* text) -> int32_t
{
    if (this == nullptr || FontData == nullptr)
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

            VfxStringDraw(pane, xPos, yPos, FontData.get(), reinterpret_cast<char*>(text), ColorTable);

            if (newline != nullptr)
            {
                *newline = '\n';
            }
        }
    }

    return 0;
}

auto MCGuiFont::CharactersToWidth(uint8_t* text, int32_t maxWidth, int wordWrap) -> int32_t
{
    char* string = reinterpret_cast<char*>(text);

    if (wordWrap == 0)
    {
        // Cut characters off the end (as writeString does) until the rest fits.
        uint8_t* end = text + std::strlen(string);
        uint8_t saved = 0;

        while (text < end)
        {
            if (Width(text) <= maxWidth)
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

    if (Width(text) <= maxWidth)
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
    while (maxWidth < Width(text))
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
