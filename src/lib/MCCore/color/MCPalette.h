#pragma once

#include "main/MCGameContext.h"
#include "vfx/MCVfx.h"

/// <summary>
/// The game palette and its fade tables (256-byte index translations: the haze levels and the actors' fades), as a
/// palette FIT file describes them.
/// </summary>
/// <remarks>
/// <para>Original source: <c>color\color.cpp</c>. The FIT's "Palette" block names the <c>.pal</c> file
/// ("PaletteFileName": <c>uint16 firstColor, uint16 numColors</c>, then the colours as 6-bit RGB triples) and gives
/// "NumBitmapHazeLevels"; its "Tables" block names the fade table file ("FadeTableFile", a <c>.tbl</c> of 256-byte
/// tables).</para>
/// <para>The original class also loaded a black-and-white palette, "extract" palettes, colour ranges, depth-haze
/// tables and an all-fade table, and could fade the shown palette toward a colour or another palette, look up shades
/// by depth and edit and save its files. Nothing in MCX.EXE called any of that, so the port doesn't have it.</para>
/// </remarks>
class MCPalette
{
public:
    /// <summary>The number of colours.</summary>
    static constexpr int32_t ColorCount = 256;
    /// <summary>The size of a fade table.</summary>
    static constexpr int32_t FadeTableSize = 256;

    /// <summary>
    /// The colours of <paramref name="palFile"/> (a <c>.pal</c> image) and the fade tables of
    /// <paramref name="fadeTables"/> (a <c>.tbl</c> image), <paramref name="numBitmapHazeLevels"/> to a haze set.
    /// </summary>
    MCPalette(std::span<const uint8_t> palFile, std::vector<uint8_t> fadeTables, int32_t numBitmapHazeLevels);

    ~MCPalette();
    MCPalette(const MCPalette&) = delete;
    MCPalette& operator=(const MCPalette&) = delete;

    /// <summary>Loads the palette FIT <paramref name="paletteFileName"/> from the palette path.</summary>
    static std::expected<std::unique_ptr<MCPalette>, std::string> Create(std::string_view paletteFileName);

    /// <summary>
    /// The fade table of haze level <paramref name="hazeLevel"/>, clamped to <see cref="NumBitmapHazeLevels"/>
    /// (negative levels use the second set), none for 0.
    /// </summary>
    std::optional<MCColorTable> GetHazePalette(int32_t hazeLevel) const;

    /// <summary>
    /// The fade table <paramref name="index"/> of the actors' set (after the two haze sets); none for an index past the
    /// palette's tables.
    /// </summary>
    std::optional<MCColorTable> GetFadeTable(int32_t index) const;

    /// <summary>Shows the palette (<c>MCGuiSystem::ActivatePalette</c>, all 256 entries).</summary>
    void Activate();

    /// <summary>
    /// Copies <paramref name="colors"/> into the palette from index <paramref name="start"/> (wrapping at 256); the
    /// display isn't told.
    /// </summary>
    void TweakPalette(int32_t start, std::span<const MCVfxRgb> colors);

    /// <summary>The colours as VFX keeps them.</summary>
    std::span<MCVfxRgb, ColorCount> Colors()
    {
        return std::span<MCVfxRgb, ColorCount>(reinterpret_cast<MCVfxRgb*>(RgbData.data()), ColorCount);
    }

    /// <summary>The colours (6 bits per channel), 3 bytes each.</summary>
    std::vector<uint8_t> RgbData;
    /// <summary>The fade tables (256 bytes each); the haze tables are among them.</summary>
    std::vector<uint8_t> FadePalettes;
    /// <summary>FIT "NumBitmapHazeLevels": fade tables per haze set.</summary>
    int32_t NumBitmapHazeLevels = 0;
};

/// <summary>The palette the game shows (set from the interface's start to the end of the game).</summary>
MCPalette& GamePalette();

/// <summary>Where palette files are found (SYSTEM.CFG's "palettePath").</summary>
extern std::string PalettePath;
