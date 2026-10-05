#include "stdafx.h"
#include "engine/font.h"
#include "lib/cident.h"
#include "lib/file.h"
#include "logistics/logmain.h"
#include "vfx/vfxfuncs.h"

namespace
{
    /// <summary>A font size scaled by <paramref name="scale"/>, rounded down and cut to 16 bits as the original.</summary>
    int32_t scaleSize(uint32_t size, float scale)
    {
        return static_cast<int16_t>(
            static_cast<int32_t>(std::floor(static_cast<double>(static_cast<float>(size) * scale))));
    }
}

auto Font::init(char* fontName) -> int32_t
{
    curY = 0;
    curX = 0;
    color = 0xf;
    scale = 2.0f;
    unknown14 = 0;
    scaled = 1;
    fontData.reset();

    for (int32_t i = 0; i < 0x100; i++)
    {
        letterCache[i] = reinterpret_cast<uint8_t*>(static_cast<intptr_t>(-1));
    }

    FullPathFileName fileName;
    fileName.init(fontPath, fontName, ".bin");
    File file;
    int32_t result = file.open(fileName, READ, 0x32);

    if (result != 0)
    {
        return result;
    }

    fontData = std::make_unique<uint8_t[]>(file.fileSize());
    file.read(fontData.get(), static_cast<int32_t>(file.fileSize()));
    file.close();
    fontHeight = getHeight();
    return 0;
}

auto Font::find(uint8_t letter) -> uint8_t*
{
    uint8_t* data = letterCache[letter];

    if (data == reinterpret_cast<uint8_t*>(static_cast<intptr_t>(-1)) && (data = fontData.get()) != nullptr)
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

        letterCache[letter] = data;
    }

    return data;
}

auto Font::findLetter(uint8_t letter) -> uint8_t*
{
    uint8_t* data = find(letter);

    if (data == nullptr && letter > 0x60 && letter < 0x7b)
    {
        data = find(letter ^ 0x20);
    }

    return data;
}

auto Font::printWidth(uint8_t letter) -> int32_t
{
    if (letter != 0)
    {
        uint8_t* data = findLetter(letter);

        if (data != nullptr)
        {
            if (scaled == 0)
            {
                return data[1];
            }

            return scaleSize(data[1], scale);
        }
    }

    return 0;
}

auto Font::printWidth(char* text, int multiLine) -> int32_t
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
            width += printWidth(static_cast<uint8_t>(*text));
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
                width += printWidth(static_cast<uint8_t>(*text));
            }
        }

        if (widest < width)
        {
            widest = width;
        }
    }

    return widest;
}

auto Font::getHeight() -> uint8_t
{
    uint8_t* data = fontData.get();
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

auto Font::print(uint8_t letter, _pane* pane) -> int32_t
{
    uint8_t* data = findLetter(letter);

    if (data == nullptr)
    {
        return 0;
    }

    int32_t width = data[1];

    if (scaled != 0)
    {
        width = scaleSize(static_cast<uint32_t>(width), scale);
    }

    int32_t height = data[2];

    if (scaled != 0)
    {
        height = scaleSize(static_cast<uint32_t>(height), scale);
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

            if (scaled != 0)
            {
                x0 = scaleSize(static_cast<uint32_t>(x0), scale);
                y0 = scaleSize(static_cast<uint32_t>(y0), scale);
                x1 = scaleSize(static_cast<uint32_t>(x1), scale);
                y1 = scaleSize(static_cast<uint32_t>(y1), scale);
            }

            VFX_line_draw(pane, curX + x0, curY + y0, curX + x1, curY + y1, LD_DRAW, color);
        }
        else if (*stroke == 2)
        {
            // 1/256ths of the letter's cell.
            VFX_line_draw(pane, ((stroke[1] * width) >> 8) + curX, ((stroke[2] * height) >> 8) + curY,
                          ((stroke[3] * width) >> 8) + curX, ((stroke[4] * height) >> 8) + curY, LD_DRAW, color);
        }
    }

    return width;
}

auto Font::print(char* text, int32_t newColor, _pane* pane) -> void
{
    if (newColor != -1)
    {
        color = newColor;
    }

    if (text != nullptr)
    {
        for (; *text != 0; text++)
        {
            curX += print(static_cast<uint8_t>(*text), pane);
        }
    }
}

auto Font::print(int32_t x, int32_t y, char* text, int32_t newColor, _pane* pane) -> void
{
    curX = x;
    curY = y;

    if (pane != nullptr)
    {
        print(text, newColor, pane);
    }
}

auto Font::printToNewline(char* text, int32_t newColor, _pane* pane) -> int32_t
{
    int32_t count = 0;

    if (newColor != -1)
    {
        color = newColor;
    }

    if (text != nullptr)
    {
        for (; *text != 0 && *text != '\n'; text++)
        {
            curX += print(static_cast<uint8_t>(*text), pane);
            count++;
        }
    }

    return count;
}

auto Font::printToNewline(int32_t x, int32_t y, char* text, int32_t newColor, _pane* pane) -> int32_t
{
    curX = x;
    curY = y;

    if (pane != nullptr)
    {
        return printToNewline(text, newColor, pane);
    }

    return 0;
}
