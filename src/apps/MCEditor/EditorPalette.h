#pragma once

class DaoDatabase;

/// <summary>Bytes the editor allocates for a LOGPALETTE of 256 entries (four more than it needs).</summary>
inline constexpr size_t LOG_PALETTE_ALLOC_SIZE = 0x408;
/// <summary>Bytes the editor allocates for a BITMAPINFO with 256 colours (four more than it needs).</summary>
inline constexpr size_t BITMAP_INFO_ALLOC_SIZE = 0x42c;
/// <summary>Width and height of the "not allowed" sign.</summary>
inline constexpr int32_t NOT_ALLOWED_SIZE = 32;

/// <summary>
/// The editor's palette: the game palette as a LOGPALETTE (gamma-corrected and as loaded) and as the colour table of
/// its 8-bit DIBs, the logistics palette, the colour remap tables the views tint with, the fade table, and the
/// "not allowed" sign in palette indices.
/// </summary>
/// <remarks>
/// The editor's own code (0x0046e220-0x0046ebc0, after MapOverlay; the source file name wasn't kept, the names are the
/// port's). 0x918 bytes. One instance, <c>editorPaletteInstance</c> in DreditorDoc.cpp; <see cref="editorPalette"/>
/// points to it. Filled by <c>DreditorDoc::loadPalette</c>. The remap tables cover entries 10-245 (the brighter one
/// all 256); their names describe the colours they give, the views that use them aren't translated yet.
/// </remarks>
class EditorPalette
{
public:
    /// <summary>Allocates the DIB header (8 bits, no colours yet) and becomes <see cref="editorPalette"/>.</summary>
    EditorPalette();
    /// <summary>Frees the palettes and tables and the sign's bitmap; clears <see cref="editorPalette"/>.</summary>
    ~EditorPalette();
    EditorPalette(const EditorPalette&) = delete;
    EditorPalette& operator=(const EditorPalette&) = delete;

    /// <summary>
    /// Takes the game palette <paramref name="rgb"/> (256 RGB triples, 6 bits per component), builds the remap
    /// tables, applies the gamma saved in the settings (Gamma\LastGamma, "1.0" by default) and loads the "not allowed"
    /// sign.
    /// </summary>
    /// <returns>True.</returns>
    bool init(const uint8_t* rgb);
    /// <summary>
    /// Stores the logistics palette <paramref name="rgb"/> (256 RGB triples, 6 bits per component) as
    /// <see cref="logisticColors"/>.
    /// </summary>
    /// <returns>True.</returns>
    bool setLogisticPalette(const uint8_t* rgb);
    /// <summary>
    /// Applies <paramref name="gamma"/> to entries 10-232 of the loaded palette (<see cref="gammaPalette"/> and the
    /// DIB colours) and saves it as the settings' Gamma\LastGamma.
    /// </summary>
    /// <returns>True.</returns>
    bool setGamma(float gamma);
    /// <summary>Rebuilds <see cref="brighter"/> against <paramref name="colors"/> (256 RGBQUADs).</summary>
    /// <remarks>Nothing in the editor calls it.</remarks>
    void rebuildBrighter(const uint8_t* colors);
    /// <summary>
    /// Takes <paramref name="table"/> (allocated with new[]) as the fade table, freeing the last one, and reads the
    /// mechs' and vehicles' paint schemes and tonnages from <paramref name="database"/> (<c>loadMoverTables</c>).
    /// </summary>
    /// <returns>True.</returns>
    /// <remarks><paramref name="size"/> it ignores.</remarks>
    bool setFadeTable(DaoDatabase& database, uint8_t* table, int32_t size);
    /// <summary>The "not allowed" sign: 32 x 32 palette indices, bottom row first, 0xff where it is transparent.</summary>
    /// <param name="size">Gets the sign's width and height.</param>
    uint8_t* getNotAllowedBits(CSize& size);

    /// <summary>The loaded palette with the gamma applied.</summary>
    LOGPALETTE* gammaPalette = nullptr;
    /// <summary>The header and colours of the editor's 8-bit DIBs: the gamma-corrected palette.</summary>
    BITMAPINFO* bitmapInfo = nullptr;
    /// <summary>The palette as loaded.</summary>
    LOGPALETTE* basePalette = nullptr;
    /// <summary>The logistics palette (Teditor\logistic.pal), 256 RGBQUADs.</summary>
    RGBQUAD* logisticColors = nullptr;
    /// <summary>Each colour with green at 1.2 and red and blue at 0.3.</summary>
    uint8_t greenTint[256] = {};
    /// <summary>Each colour with red at 1.5 and green and blue at 0.3.</summary>
    uint8_t redTint[256] = {};
    /// <summary>
    /// Each colour at 1.5, dark ones (every component under 20) as grey 127, and ones with a component of 0xb8 kept
    /// (with red for blue, EB-045).
    /// </summary>
    uint8_t highlight[256] = {};
    /// <summary>Each colour at 1.3, unclamped (EB-046).</summary>
    uint8_t brighter[256] = {};
    /// <summary>Each colour as red 0.7 x green, green 0.3 x green, blue 0.3 x blue.</summary>
    uint8_t amberTint[256] = {};
    /// <summary>The fade table (fade.tbl, gfade.tbl for GTiles).</summary>
    uint8_t* fadeTable = nullptr;
    /// <summary>The sign's DIB section (IDB_NOT_ALLOWED).</summary>
    HBITMAP notAllowedBitmap = nullptr;
    /// <summary>The sign in palette indices (<see cref="getNotAllowedBits"/>).</summary>
    uint8_t notAllowedBits[NOT_ALLOWED_SIZE * NOT_ALLOWED_SIZE] = {};
};

/// <summary>The editor's palette, set by its constructor.</summary>
extern EditorPalette* editorPalette;
