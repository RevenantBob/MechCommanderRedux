#include "stdafx.h"
#include "engine/font.h"
#include "lib/MCIDString.h"
#include "lib/MCFile.h"
#include "logistics/logmain.h"
#include "vfx/vfxfuncs.h"

namespace
{
    /// <summary>A font size scaled by <paramref name="scale"/>, rounded down and cut to 16 bits as the original.</summary>
    int32_t ScaleSize(uint32_t size, float scale)
    {
        return static_cast<int16_t>(
            static_cast<int32_t>(std::floor(static_cast<double>(static_cast<float>(size) * scale))));
    }
}

auto MCFont::Init(char* fontName) -> int32_t
{
    CurY = 0;
    CurX = 0;
    Color = 0xf;
    Scale = 2.0f;
    Scaled = 1;
    FontData.reset();

    for (int32_t i = 0; i < 0x100; i++)
    {
        LetterCache[i] = reinterpret_cast<uint8_t*>(static_cast<intptr_t>(-1));
    }

    std::string fileName;
    fileName = GamePath(FontPath, fontName, ".bin");
    MCFile file;
    int32_t result = file.Open(fileName);

    if (result != 0)
    {
        return result;
    }

    FontData = std::make_unique<uint8_t[]>(file.FileSize());
    file.Read(FontData.get(), static_cast<int32_t>(file.FileSize()));
    file.Close();
    FontHeight = GetHeight();
    return 0;
}

auto MCFont::Find(uint8_t letter) -> uint8_t*
{
    uint8_t* data = LetterCache[letter];

    if (data == reinterpret_cast<uint8_t*>(static_cast<intptr_t>(-1)) && (data = FontData.get()) != nullptr)
    {
        while (true)
        {
            if (*data == 0)
            {
                return nullptr;
            }

            if (*data == letter)
            {
                break;
            }

            uint8_t* stroke = data + 3;

            while (*stroke != 0)
            {
                stroke += 5;
            }

            data = stroke + 1;
        }

        LetterCache[letter] = data;
    }

    return data;
}

auto MCFont::FindLetter(uint8_t letter) -> uint8_t*
{
    uint8_t* data = Find(letter);

    if (data == nullptr && letter > 0x60 && letter < 0x7b)
    {
        data = Find(letter ^ 0x20);
    }

    return data;
}

auto MCFont::PrintWidth(uint8_t letter) -> int32_t
{
    if (letter != 0)
    {
        uint8_t* data = FindLetter(letter);

        if (data != nullptr)
        {
            if (Scaled == 0)
            {
                return data[1];
            }

            return ScaleSize(data[1], Scale);
        }
    }

    return 0;
}

auto MCFont::PrintWidth(char* text, int multiLine) -> int32_t
{
    int32_t width = 0;

    if (text == nullptr)
    {
        return 0;
    }

    if (multiLine == 0)
    {
        for (; *text != 0; text++)
        {
            width += PrintWidth(static_cast<uint8_t>(*text));
        }

        return width;
    }

    int32_t widest = 0;

    if (*text != 0)
    {
        for (; *text != 0; text++)
        {
            if (*text == '\n')
            {
                if (widest < width)
                {
                    widest = width;
                }

                width = 0;
            }
            else
            {
                width += PrintWidth(static_cast<uint8_t>(*text));
            }
        }

        if (widest < width)
        {
            widest = width;
        }
    }

    return widest;
}

auto MCFont::GetHeight() -> uint8_t
{
    uint8_t* data = FontData.get();
    uint8_t height = 0;

    while (*data != 0)
    {
        if (*data != 0xff && height <= data[2])
        {
            height = data[2];
        }

        uint8_t* stroke = data + 3;

        while (*stroke != 0)
        {
            stroke += 5;
        }

        data = stroke + 1;
    }

    return height;
}

auto MCFont::Print(uint8_t letter, MCPane* pane) -> int32_t
{
    uint8_t* data = FindLetter(letter);

    if (data == nullptr)
    {
        return 0;
    }

    int32_t width = data[1];

    if (Scaled != 0)
    {
        width = ScaleSize(static_cast<uint32_t>(width), Scale);
    }

    int32_t height = data[2];

    if (Scaled != 0)
    {
        height = ScaleSize(static_cast<uint32_t>(height), Scale);
    }

    for (uint8_t* stroke = data + 3; *stroke != 0; stroke += 5)
    {
        if (*stroke == 1)
        {
            // Pixel coordinates, scaled like the letter's size.
            int32_t x0 = stroke[1];
            int32_t y0 = stroke[2];
            int32_t x1 = stroke[3];
            int32_t y1 = stroke[4];

            if (Scaled != 0)
            {
                x0 = ScaleSize(static_cast<uint32_t>(x0), Scale);
                y0 = ScaleSize(static_cast<uint32_t>(y0), Scale);
                x1 = ScaleSize(static_cast<uint32_t>(x1), Scale);
                y1 = ScaleSize(static_cast<uint32_t>(y1), Scale);
            }

            VfxLineDraw(pane, CurX + x0, CurY + y0, CurX + x1, CurY + y1, LD_DRAW, Color);
        }
        else if (*stroke == 2)
        {
            // 1/256ths of the letter's cell.
            VfxLineDraw(pane, ((stroke[1] * width) >> 8) + CurX, ((stroke[2] * height) >> 8) + CurY,
                        ((stroke[3] * width) >> 8) + CurX, ((stroke[4] * height) >> 8) + CurY, LD_DRAW, Color);
        }
    }

    return width;
}

auto MCFont::Print(char* text, int32_t newColor, MCPane* pane) -> void
{
    if (newColor != -1)
    {
        Color = newColor;
    }

    if (text != nullptr)
    {
        for (; *text != 0; text++)
        {
            CurX += Print(static_cast<uint8_t>(*text), pane);
        }
    }
}

auto MCFont::Print(int32_t x, int32_t y, char* text, int32_t newColor, MCPane* pane) -> void
{
    CurX = x;
    CurY = y;

    if (pane != nullptr)
    {
        Print(text, newColor, pane);
    }
}

auto MCFont::PrintToNewline(char* text, int32_t newColor, MCPane* pane) -> int32_t
{
    int32_t count = 0;

    if (newColor != -1)
    {
        Color = newColor;
    }

    if (text != nullptr)
    {
        for (; *text != 0 && *text != '\n'; text++)
        {
            CurX += Print(static_cast<uint8_t>(*text), pane);
            count++;
        }
    }

    return count;
}

auto MCFont::PrintToNewline(int32_t x, int32_t y, char* text, int32_t newColor, MCPane* pane) -> int32_t
{
    CurX = x;
    CurY = y;

    if (pane != nullptr)
    {
        return PrintToNewline(text, newColor, pane);
    }

    return 0;
}
