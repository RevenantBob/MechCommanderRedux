#include "stdafx.h"
#include "engine/MCFont.h"
#include "lib/MCFile.h"
#include "logistics/logmain.h"
#include "vfx/MCVfxFunctions.h"

namespace
{
    /// <summary>The byte after a letter's strokes: the next letter.</summary>
    const uint8_t* NextLetter(const uint8_t* letter)
    {
        const uint8_t* stroke = letter + 3;

        while (*stroke != 0)
        {
            stroke += 5;
        }

        return stroke + 1;
    }
}

MCFont::MCFont(std::vector<uint8_t> data) : _Data(std::move(data))
{
    if (_Data.empty())
    {
        return;
    }

    // The tallest letter, 0xFF not counted.
    for (const uint8_t* letter = _Data.data(); *letter != 0; letter = NextLetter(letter))
    {
        if (*letter != 0xff && FontHeight <= letter[2])
        {
            FontHeight = letter[2];
        }
    }
}

auto MCFont::Create(std::string_view fontName) -> std::expected<std::unique_ptr<MCFont>, std::string>
{
    const std::string fileName = GamePath(FontPath, fontName, ".bin");
    MCFile file;

    if (const int32_t result = file.Open(fileName); result != 0)
    {
        return std::unexpected(std::format("Could not open font {} ({:#x})", fileName, static_cast<uint32_t>(result)));
    }

    std::vector<uint8_t> data(file.FileSize());
    file.Read(data);
    return std::make_unique<MCFont>(std::move(data));
}

auto MCFont::Find(uint8_t letter) -> const uint8_t*
{
    std::optional<const uint8_t*>& cached = _Letters[letter];

    if (!cached.has_value())
    {
        const uint8_t* found = nullptr;

        if (!_Data.empty())
        {
            for (const uint8_t* data = _Data.data(); *data != 0; data = NextLetter(data))
            {
                if (*data == letter)
                {
                    found = data;
                    break;
                }
            }
        }

        // A letter the font lacks is looked up again next time, as the original didn't cache misses.
        if (found == nullptr)
        {
            return nullptr;
        }

        cached = found;
    }

    return *cached;
}

auto MCFont::FindLetter(uint8_t letter) -> const uint8_t*
{
    const uint8_t* data = Find(letter);

    if (data == nullptr && letter > 0x60 && letter < 0x7b)
    {
        data = Find(letter ^ 0x20);
    }

    return data;
}

auto MCFont::ScaledSize(uint32_t size) const -> int32_t
{
    if (!Scaled)
    {
        return static_cast<int32_t>(size);
    }

    // Rounded down and cut to 16 bits, as the original's __ftol into a short.
    return static_cast<int16_t>(
        static_cast<int32_t>(std::floor(static_cast<double>(static_cast<float>(size) * Scale))));
}

auto MCFont::PrintWidth(uint8_t letter) -> int32_t
{
    if (letter == 0)
    {
        return 0;
    }

    const uint8_t* data = FindLetter(letter);
    return data != nullptr ? ScaledSize(data[1]) : 0;
}

auto MCFont::PrintWidth(std::string_view text, bool multiLine) -> int32_t
{
    int32_t width = 0;

    if (!multiLine)
    {
        for (const char c : text)
        {
            width += PrintWidth(static_cast<uint8_t>(c));
        }

        return width;
    }

    int32_t widest = 0;

    for (const char c : text)
    {
        if (c == '\n')
        {
            widest = std::max(widest, width);
            width = 0;
        }
        else
        {
            width += PrintWidth(static_cast<uint8_t>(c));
        }
    }

    return std::max(widest, width);
}

auto MCFont::Print(uint8_t letter, MCPane* pane) -> int32_t
{
    const uint8_t* data = FindLetter(letter);

    if (data == nullptr)
    {
        return 0;
    }

    const int32_t width = ScaledSize(data[1]);
    const int32_t height = ScaledSize(data[2]);

    for (const uint8_t* stroke = data + 3; *stroke != 0; stroke += 5)
    {
        if (*stroke == 1)
        {
            // Pixel coordinates, scaled like the letter's size.
            VfxLineDraw(pane, CurX + ScaledSize(stroke[1]), CurY + ScaledSize(stroke[2]), CurX + ScaledSize(stroke[3]),
                        CurY + ScaledSize(stroke[4]), Color);
        }
        else if (*stroke == 2)
        {
            // 1/256ths of the letter's cell.
            VfxLineDraw(pane, ((stroke[1] * width) >> 8) + CurX, ((stroke[2] * height) >> 8) + CurY,
                        ((stroke[3] * width) >> 8) + CurX, ((stroke[4] * height) >> 8) + CurY, Color);
        }
    }

    return width;
}

auto MCFont::Print(std::string_view text, int32_t newColor, MCPane* pane) -> void
{
    if (newColor != -1)
    {
        Color = newColor;
    }

    for (const char c : text)
    {
        // A NUL ends the text, as in the original's C string.
        if (c == 0)
        {
            break;
        }

        CurX += Print(static_cast<uint8_t>(c), pane);
    }
}

auto MCFont::Print(int32_t x, int32_t y, std::string_view text, int32_t newColor, MCPane* pane) -> void
{
    CurX = x;
    CurY = y;

    if (pane != nullptr)
    {
        Print(text, newColor, pane);
    }
}

auto MCFont::PrintToNewline(std::string_view text, int32_t newColor, MCPane* pane) -> int32_t
{
    int32_t count = 0;

    if (newColor != -1)
    {
        Color = newColor;
    }

    for (const char c : text)
    {
        if (c == 0 || c == '\n')
        {
            break;
        }

        CurX += Print(static_cast<uint8_t>(c), pane);
        count++;
    }

    return count;
}

auto MCFont::PrintToNewline(int32_t x, int32_t y, std::string_view text, int32_t newColor, MCPane* pane) -> int32_t
{
    CurX = x;
    CurY = y;

    if (pane != nullptr)
    {
        return PrintToNewline(text, newColor, pane);
    }

    return 0;
}
