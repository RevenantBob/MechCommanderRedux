#include "stdafx.h"
#include "vfx/vfxint.h"
#include "color/color.h"
#include "lib/aerror.h"
#include "lib/file.h"

// The game's translucency tables (mcx\vfx\AlphaPalette.cpp): for every "alpha colour" defined in AlphaPal.ini, the
// palette index that colour blended over each background index gives.

char AlphaTable[ALPHA_COLORS * 256];
char SpecialColor[ALPHA_COLORS];
MCAlphaColor MCAlphaColors[ALPHA_COLORS];

void writeTGA(char* fileName, uint8_t* image, uint32_t width, uint32_t height)
{
    File file;

    if (file.create(fileName) != NO_ERR)
    {
        return;
    }

    // Uncompressed true-colour, 24 bits per pixel, origin at the top left (descriptor 0x20).
    uint8_t header[18] = {};
    header[2] = 2;
    header[12] = static_cast<uint8_t>(width);
    header[13] = static_cast<uint8_t>(width >> 8);
    header[14] = static_cast<uint8_t>(height);
    header[15] = static_cast<uint8_t>(height >> 8);
    header[16] = 0x18;
    header[17] = 0x20;
    file.write(header, 18);

    const uint8_t* rgb = gamePalette->rgbData.get();
    const int32_t count = static_cast<int32_t>(width * height);

    for (int32_t i = 0; i < count; ++i)
    {
        const uint8_t* entry = rgb + image[i] * 3;
        // 6-bit components, shifted up (as bytes: the top bits are lost for values over 63).
        file.writeByte(static_cast<uint8_t>(entry[2] << 2));
        file.writeByte(static_cast<uint8_t>(entry[1] << 2));
        file.writeByte(static_cast<uint8_t>(entry[0] << 2));
    }

    file.close();
}

namespace
{
    /// <summary>One AlphaPal.ini entry: the colour's (8-bit) components, its weight A and the background's weight B2.</summary>
    struct AlphaEntry
    {
        float r;
        float g;
        float b;
        float alpha;
        float backgroundWeight;
    };

    /// <summary>__ftol then the clamp to 0..255 InitAlphaLookup applies.</summary>
    int ClampComponent(double value)
    {
        const int component = static_cast<int>(value);
        return component < 0 ? 0 : (component > 0xff ? 0xff : component);
    }
}

void InitAlphaLookup(VFX_RGB* palette)
{
    AlphaEntry entries[ALPHA_COLORS] = {};
    std::memset(SpecialColor, 0, sizeof(SpecialColor));

    File* file = new File;
    const int32_t result = file->open("AlphaPal.ini", READ, 50);

    // Original: Assert(result == 0, result, "Could not open alphapal.ini"); Assert isn't ported yet.
    if (result != NO_ERR)
    {
        Fatal(result, "Could not open alphapal.ini");
    }

    int32_t lineNumber = 0;
    char message[256];

    while (!file->eof())
    {
        uint8_t line[256] = {};
        file->readLine(line, 0x100);
        ++lineNumber;
        const char first = static_cast<char>(line[0]);

        if (first == '#' || first == ';' || first == '\n' || first == '\0')
        {
            continue;
        }

        int index = 0;

        if (std::sscanf(reinterpret_cast<char*>(line), "%d", &index) == EOF)
        {
            break;
        }

        if (index < 0 || index >= ALPHA_COLORS)
        {
            std::snprintf(message, sizeof(message), "Invalid index field in AlphaPal.ini, line %d", lineNumber);
            Fatal(0, message);
        }

        SpecialColor[index] = 1;
        AlphaEntry& entry = entries[index];
        int ignored = 0;

        if (std::sscanf(reinterpret_cast<char*>(line), "%d %f %f %f %f %f", &ignored, &entry.r, &entry.g, &entry.b,
                        &entry.alpha, &entry.backgroundWeight) == EOF)
        {
            std::snprintf(message, sizeof(message), "Invalid data in AlphaPal.ini, line %d", lineNumber);
            Fatal(0, message);
        }

        // The dodge form divides by 255 - component.
        if (entry.alpha == 0.0f && entry.backgroundWeight == 0.0f &&
            (entry.r == 255.0f || entry.g == 255.0f || entry.b == 255.0f))
        {
            std::snprintf(message, sizeof(message), "Source color cannot be 255 in Alphapal.ini!, line %d", lineNumber);
            Fatal(0, message);
        }
    }

    file->close();
    delete file;

    for (int32_t color = 0; color < ALPHA_COLORS; ++color)
    {
        const AlphaEntry& entry = entries[color];
        MCAlphaColors[color] = MCAlphaColor{entry.r, entry.g, entry.b, entry.alpha, entry.backgroundWeight};
    }

    // The x87 ran at 53-bit precision (the MSVC CRT default), so double arithmetic gives the same results.
    uint8_t* out = reinterpret_cast<uint8_t*>(AlphaTable);

    for (int32_t color = 0; color < ALPHA_COLORS; ++color)
    {
        const AlphaEntry& entry = entries[color];

        for (int32_t background = 0; background < 256; ++background, ++out)
        {
            if (color == 0xff || color == 0)
            {
                *out = static_cast<uint8_t>(background);
            }
            else if (SpecialColor[color] == 0)
            {
                *out = static_cast<uint8_t>(color);
            }
            else if (background < 10 || background > 0xf5)
            {
                *out = 0xff;
            }
            else
            {
                const VFX_RGB& bg = palette[background];
                const double r8 = static_cast<double>(bg.r << 2);
                const double g8 = static_cast<double>(bg.g << 2);
                const double b8 = static_cast<double>(bg.b << 2);
                int r;
                int g;
                int b;

                if (entry.alpha == 0.0f && entry.backgroundWeight == 0.0f)
                {
                    r = ClampComponent(r8 * 255.0 / (255.0 - static_cast<double>(entry.r)));
                    g = ClampComponent(g8 * 255.0 / (255.0 - static_cast<double>(entry.g)));
                    b = ClampComponent(b8 * 255.0 / (255.0 - static_cast<double>(entry.b)));
                }
                else
                {
                    const double a = entry.alpha;
                    const double w = entry.backgroundWeight;
                    r = ClampComponent(r8 * w + static_cast<double>(entry.r) * a);
                    g = ClampComponent(g8 * w + static_cast<double>(entry.g) * a);
                    b = ClampComponent(b8 * w + static_cast<double>(entry.b) * a);
                }

                *out = FindClosest(palette, r >> 2, g >> 2, b >> 2);
            }
        }
    }

    // Its rows are tables too (the logistics darken table is one).
    MCRenderer::RegisterData(AlphaTable, sizeof(AlphaTable), MCDataKind::Tables);
    MCRenderer::AlphaTableChanged();
}

uint8_t FindClosest(VFX_RGB* palette, int r, int g, int b)
{
    uint8_t best = 10;
    int bestDistance = 0xfd02ff;

    // Only the free colours: 0..9 and 246..255 are the system's.
    for (int index = 10; index < 0xf6; ++index)
    {
        const VFX_RGB& entry = palette[index];
        const int dg = g - entry.g;
        const int db = b - entry.b;
        const int dr = r - entry.r;
        const int distance = dg * dg * 51 + db * db * 10 + dr * dr * 39;

        if (distance < bestDistance)
        {
            best = static_cast<uint8_t>(index);
            bestDistance = distance;

            if (distance == 0)
            {
                return best;
            }
        }
    }

    return best;
}
