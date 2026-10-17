#include "stdafx.h"
#include "vfx/alphapalette.h"
#include "lib/ffile.h"
#include "lib/lzdecomp.h"

#include <sstream>

char AlphaTable[ALPHA_TABLE_SIZE];
char SpecialColor[ALPHA_COLORS];

namespace
{
    /// <summary>The file the colours are defined in, beside the game or in its misc.fst.</summary>
    inline constexpr const char* ALPHA_FILE_NAME = "AlphaPal.ini";
    /// <summary>The FastFile searched when AlphaPal.ini isn't a loose file.</summary>
    inline constexpr const char* MISC_FAST_FILE = "misc.fst";
    /// <summary>A line of AlphaPal.ini.</summary>
    inline constexpr int32_t ALPHA_LINE_SIZE = 0x100;
    /// <summary>The first palette entry a blend can give; 0-9 and 246-255 are the system's.</summary>
    inline constexpr int32_t FIRST_FREE_COLOR = 10;
    /// <summary>The last palette entry a blend can give.</summary>
    inline constexpr int32_t LAST_FREE_COLOR = 0xf5;
    /// <summary>The distance the nearest-colour search starts from, beyond any colour's.</summary>
    inline constexpr int32_t COLOR_DISTANCE_START = 0xfd02ff;
    /// <summary>A full 8-bit component, as AlphaPal.ini's floats hold it.</summary>
    inline constexpr float FULL_COMPONENT = 255.0f;

    /// <summary>
    /// One AlphaPal.ini entry: the colour's (8-bit) components, its weight and the background's weight.
    /// </summary>
    struct AlphaEntry
    {
        /// <summary>Red, 0-255.</summary>
        float r;
        /// <summary>Green, 0-255.</summary>
        float g;
        /// <summary>Blue, 0-255.</summary>
        float b;
        /// <summary>The colour's weight; 0 with <see cref="backgroundWeight"/> 0 selects the dodge form.</summary>
        float alpha;
        /// <summary>The background's weight.</summary>
        float backgroundWeight;
    };

    /// <summary>__ftol, then the clamp to 0-255.</summary>
    int32_t clampComponent(double value)
    {
        const auto component = static_cast<int32_t>(value);
        return component < 0 ? 0 : (component > 0xff ? 0xff : component);
    }

    /// <summary>
    /// The entry of <paramref name="palette"/> (6-bit RGB triples) from 10 to 245 nearest to the 6-bit colour, by a
    /// distance that weighs green 51, red 39 and blue 10.
    /// </summary>
    /// <remarks>Inlined into InitAlphaLookup in the editor (the game has it as FindClosest).</remarks>
    uint8_t findClosest(const uint8_t* palette, int32_t r, int32_t g, int32_t b)
    {
        uint8_t best = FIRST_FREE_COLOR;
        int32_t bestDistance = COLOR_DISTANCE_START;

        for (int32_t index = FIRST_FREE_COLOR; index <= LAST_FREE_COLOR; index++)
        {
            const uint8_t* entry = palette + index * 3;
            const int32_t dr = r - entry[0];
            const int32_t dg = g - entry[1];
            const int32_t db = b - entry[2];
            const int32_t distance = dg * dg * 51 + db * db * 10 + dr * dr * 39;

            if (distance < bestDistance)
            {
                best = static_cast<uint8_t>(index);
                bestDistance = distance;

                if (distance == 0)
                {
                    break;
                }
            }
        }

        return best;
    }
}

bool InitAlphaLookup(const uint8_t* palette, const char* dataPath)
{
    AlphaEntry entries[ALPHA_COLORS] = {};
    std::memset(SpecialColor, 0, sizeof(SpecialColor));
    auto out = reinterpret_cast<uint8_t*>(AlphaTable);
    std::vector<char> text;
    CFile file;
    const CString path = CString(dataPath) + ALPHA_FILE_NAME;
    WIN32_FIND_DATA findData;
    const HANDLE find = FindFirstFile(path, &findData);

    if (find == INVALID_HANDLE_VALUE)
    {
        const CString fastFileName = CString(dataPath) + MISC_FAST_FILE;

        if (!file.Open(fastFileName, CFile::modeRead))
        {
            CString message;
            message.LoadString(IDS_YOU_CANNOT_OPEN_MECHCOMMANDER);
            CString title;
            title.LoadString(IDS_MCEDITOR);
            MessageBoxA(nullptr, message, title, MB_OK);
            return false;
        }

        int32_t count = 0;
        file.Read(&count, sizeof(count));

        // Original behaviour: without the entry the text stays empty, so every colour but 0 and 0xff is undefined.
        for (int32_t i = 0; i < count; i++)
        {
            FILEENTRY entry;
            file.Read(&entry, sizeof(entry));

            if (_stricmp(entry.name, ALPHA_FILE_NAME) != 0)
            {
                continue;
            }

            file.Seek(entry.offset, CFile::begin);
            text.resize(entry.realSize);

            if (entry.size != entry.realSize)
            {
                std::vector<uint8_t> packed(entry.size);
                file.Read(packed.data(), entry.size);
                LZDecomp(reinterpret_cast<uint8_t*>(text.data()), packed.data(), entry.size);
            }
            else
            {
                file.Read(text.data(), entry.realSize);
            }

            break;
        }
    }
    else
    {
        text.resize(findData.nFileSizeLow);
        // Original behaviour (EB-042): the editor opens "AlphaPal.ini" in the current folder, which is the data path
        // once a document has been made (DreditorDoc's OnNewDocument changes to it), and ignores the result.
        file.Open(path, CFile::modeRead);
        file.Read(text.data(), findData.nFileSizeLow);
    }

    std::istringstream stream(std::string(text.data(), text.size()));
    int32_t lineNumber = 0;
    // Original behaviour (EB-043): a line that doesn't start with a number keeps the last index (at first an
    // uninitialised one; the port starts from 0).
    int32_t index = 0;

    while (!stream.fail())
    {
        char line[ALPHA_LINE_SIZE] = {};
        stream.getline(line, ALPHA_LINE_SIZE);
        lineNumber++;
        const char first = line[0];

        if (first == '#' || first == ';' || first == '\n' || first == '\0' || first == '\r')
        {
            continue;
        }

        if (std::sscanf(line, "%d", &index) == EOF)
        {
            break;
        }

        // Original behaviour (EB-043): the editor formats "Invalid index field in AlphaPal.ini, line %d" and returns
        // false without showing it, leaking the text and the find handle.
        if (index < 0 || index >= ALPHA_COLORS)
        {
            return false;
        }

        SpecialColor[index] = 1;
        AlphaEntry& entry = entries[index];
        int32_t ignored = 0;

        // Original behaviour (EB-043): "Invalid data in AlphaPal.ini, line %d", formatted and dropped.
        if (std::sscanf(line, "%d %f %f %f %f %f", &ignored, &entry.r, &entry.g, &entry.b, &entry.alpha,
                        &entry.backgroundWeight) == EOF)
        {
            return false;
        }

        // The dodge form divides by 255 - component. Original behaviour (EB-043): "Source color cannot be 255 in
        // Alphapal.ini!, line %d", formatted and dropped.
        if (entry.alpha == 0.0f && entry.backgroundWeight == 0.0f &&
            (entry.r == FULL_COMPONENT || entry.g == FULL_COMPONENT || entry.b == FULL_COMPONENT))
        {
            return false;
        }
    }

    // The editor's x87 runs at 64-bit precision. The products are exact either way; the sums and quotients round at 53
    // bits here instead of 64, which changes no truncated component for the retail AlphaPal.ini with either palette.
    for (int32_t color = 0; color < ALPHA_COLORS; color++)
    {
        const AlphaEntry& entry = entries[color];

        for (int32_t background = 0; background < 256; background++, out++)
        {
            if (color == 0xff || color == 0)
            {
                *out = static_cast<uint8_t>(background);
            }
            else if (SpecialColor[color] == 0)
            {
                *out = static_cast<uint8_t>(color);
            }
            else if (background < FIRST_FREE_COLOR || background > LAST_FREE_COLOR)
            {
                *out = 0xff;
            }
            else
            {
                const uint8_t* rgb = palette + background * 3;
                const auto r8 = static_cast<double>(rgb[0] << 2);
                const auto g8 = static_cast<double>(rgb[1] << 2);
                const auto b8 = static_cast<double>(rgb[2] << 2);
                int32_t r;
                int32_t g;
                int32_t b;

                if (entry.alpha == 0.0f && entry.backgroundWeight == 0.0f)
                {
                    r = clampComponent(r8 * FULL_COMPONENT / (FULL_COMPONENT - static_cast<double>(entry.r)));
                    g = clampComponent(g8 * FULL_COMPONENT / (FULL_COMPONENT - static_cast<double>(entry.g)));
                    b = clampComponent(b8 * FULL_COMPONENT / (FULL_COMPONENT - static_cast<double>(entry.b)));
                }
                else
                {
                    const double alpha = entry.alpha;
                    const double weight = entry.backgroundWeight;
                    r = clampComponent(r8 * weight + static_cast<double>(entry.r) * alpha);
                    g = clampComponent(g8 * weight + static_cast<double>(entry.g) * alpha);
                    b = clampComponent(b8 * weight + static_cast<double>(entry.b) * alpha);
                }

                *out = findClosest(palette, r >> 2, g >> 2, b >> 2);
            }
        }
    }

    // The original also passes the invalid handle when the text came from misc.fst.
    if (find != INVALID_HANDLE_VALUE)
    {
        FindClose(find);
    }

    return true;
}
