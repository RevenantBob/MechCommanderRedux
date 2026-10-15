#include "stdafx.h"
#include "EditorPalette.h"
#include "EditorUtil.h"
#include "MoverObject.h"

EditorPalette* editorPalette = nullptr;

namespace
{
    /// <summary>LOGPALETTE's palVersion for Windows 3.0 and later.</summary>
    inline constexpr WORD LOG_PALETTE_VERSION = 0x300;
    /// <summary>Entries of the game palette.</summary>
    inline constexpr int32_t PALETTE_ENTRIES = 0x100;
    /// <summary>The first entry the remap tables cover; 0-9 and 246-255 are the system's.</summary>
    inline constexpr int32_t FIRST_FREE_COLOR = 10;
    /// <summary>One past the last entry the remap tables cover.</summary>
    inline constexpr int32_t END_FREE_COLOR = 0xf6;
    /// <summary>One past the last entry <see cref="EditorPalette::setGamma"/> corrects (EB-044).</summary>
    inline constexpr int32_t END_GAMMA_COLOR = 0xe9;
    /// <summary>The highest 8-bit component; brighter results are clamped to it.</summary>
    inline constexpr double FULL_COMPONENT = 255.0;
    /// <summary>1 / 255, the scale gamma correction works in.</summary>
    inline constexpr double COMPONENT_SCALE = 1.0 / 255.0;
    /// <summary>Components under this make a colour "dark" for the highlight table.</summary>
    inline constexpr uint8_t DARK_COMPONENT = 0x14;
    /// <summary>The grey dark colours highlight to.</summary>
    inline constexpr uint8_t DARK_HIGHLIGHT = 0x7f;
    /// <summary>A component value that keeps a colour unchanged in the highlight table.</summary>
    inline constexpr uint8_t KEPT_COMPONENT = 0xb8;
    /// <summary>The sign's colours above this (red and blue) with green under <see cref="KEY_GREEN_MAX"/> are its magenta key.</summary>
    inline constexpr uint8_t KEY_RED_BLUE_MIN = 0xfa;
    /// <summary>The highest green of the sign's magenta key.</summary>
    inline constexpr uint8_t KEY_GREEN_MAX = 5;
    /// <summary>The sign's transparent index.</summary>
    inline constexpr uint8_t TRANSPARENT_INDEX = 0xff;

    /// <summary>The bits of a double's mantissa.</summary>
    inline constexpr int32_t DOUBLE_MANTISSA_BITS = 53;

    /// <summary>
    /// <paramref name="value"/> x <paramref name="factor"/> truncated toward zero (__ftol), as the editor's x87
    /// computes it.
    /// </summary>
    /// <remarks>
    /// The editor runs the x87 at 64-bit precision (checked against its tables in memory; at 53 bits, products like
    /// 10 x 0.3 would round up to a whole number). A byte (8 bits) times a double (53) fits the 64-bit mantissa, so the
    /// product is exact: computed here in integers.
    /// </remarks>
    uint64_t truncatedProduct(uint32_t value, double factor)
    {
        int exponent = 0;
        const double fraction = std::frexp(factor, &exponent);
        const auto mantissa = static_cast<uint64_t>(std::ldexp(fraction, DOUBLE_MANTISSA_BITS));
        const uint64_t product = mantissa * value;
        const int32_t shift = DOUBLE_MANTISSA_BITS - exponent;

        if (shift >= 64)
        {
            return 0;
        }

        return shift >= 0 ? product >> shift : product << -shift;
    }

    /// <summary>The low byte of <see cref="truncatedProduct"/>.</summary>
    uint8_t ftolByte(uint32_t value, double factor)
    {
        return static_cast<uint8_t>(truncatedProduct(value, factor));
    }

    /// <summary>The truncated product, or 0xff when the product is over 255.</summary>
    uint8_t clampedByte(uint32_t value, double factor)
    {
        const uint64_t whole = truncatedProduct(value, factor);
        return whole > 0xff ? 0xff : static_cast<uint8_t>(whole);
    }

    /// <summary>
    /// <paramref name="value"/> / 255 raised to <paramref name="exponent"/>, times 255, truncated: the product is exact
    /// at 64 bits, then rounded to a double for the CRT's pow, whose result times 255 is exact again.
    /// </summary>
    /// <remarks>
    /// Checked against the original at gamma 1.0, where pow returns its argument. For other gammas the CRT's x87 pow
    /// may differ from std::pow in the last bit.
    /// </remarks>
    uint8_t gammaComponent(uint8_t value, float exponent)
    {
        const double scaled = static_cast<double>(value) * COMPONENT_SCALE;
        return ftolByte(static_cast<uint32_t>(FULL_COMPONENT), std::pow(scaled, static_cast<double>(exponent)));
    }

    /// <summary>The 6-bit component <paramref name="value"/> as 8 bits (the byte shift drops nothing for 0-63).</summary>
    uint8_t expand(uint8_t value)
    {
        return static_cast<uint8_t>(value << 2);
    }
}

EditorPalette::EditorPalette()
{
    editorPalette = this;
    bitmapInfo = reinterpret_cast<BITMAPINFO*>(new uint8_t[BITMAP_INFO_ALLOC_SIZE]());
    bitmapInfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo->bmiHeader.biPlanes = 1;
    bitmapInfo->bmiHeader.biBitCount = 8;
    bitmapInfo->bmiHeader.biCompression = BI_RGB;
    bitmapInfo->bmiHeader.biXPelsPerMeter = 0;
    bitmapInfo->bmiHeader.biYPelsPerMeter = 0;
    bitmapInfo->bmiHeader.biClrUsed = 0;
    bitmapInfo->bmiHeader.biClrImportant = 0;
}

EditorPalette::~EditorPalette()
{
    delete[] reinterpret_cast<uint8_t*>(gammaPalette);
    delete[] reinterpret_cast<uint8_t*>(bitmapInfo);
    delete[] reinterpret_cast<uint8_t*>(basePalette);
    delete[] reinterpret_cast<uint8_t*>(logisticColors);
    delete[] fadeTable;
    DeleteObject(notAllowedBitmap);
    editorPalette = nullptr;
}

bool EditorPalette::init(const uint8_t* rgb)
{
    if (gammaPalette == nullptr)
    {
        gammaPalette = reinterpret_cast<LOGPALETTE*>(new uint8_t[LOG_PALETTE_ALLOC_SIZE]());
    }

    if (basePalette == nullptr)
    {
        basePalette = reinterpret_cast<LOGPALETTE*>(new uint8_t[LOG_PALETTE_ALLOC_SIZE]());
    }

    basePalette->palNumEntries = PALETTE_ENTRIES;
    gammaPalette->palNumEntries = PALETTE_ENTRIES;
    basePalette->palVersion = LOG_PALETTE_VERSION;
    gammaPalette->palVersion = LOG_PALETTE_VERSION;
    RGBQUAD* colors = bitmapInfo->bmiColors;

    for (int32_t i = 0; i < PALETTE_ENTRIES; i++, rgb += 3)
    {
        colors[i].rgbRed = expand(rgb[0]);
        colors[i].rgbGreen = expand(rgb[1]);
        colors[i].rgbBlue = expand(rgb[2]);
        colors[i].rgbReserved = 0;
        gammaPalette->palPalEntry[i] = {colors[i].rgbRed, colors[i].rgbGreen, colors[i].rgbBlue, 0};
        basePalette->palPalEntry[i] = {colors[i].rgbRed, colors[i].rgbGreen, colors[i].rgbBlue, 0};
    }

    // The remap tables, matched against the palette (findNearestColor reads entry 10 on).
    const auto palette = reinterpret_cast<const uint8_t*>(colors);

    // Original behaviour (EB-046): the brighter components aren't clamped.
    for (int32_t i = 0; i < PALETTE_ENTRIES; i++)
    {
        const uint8_t bgr[3] = {ftolByte(colors[i].rgbBlue, 1.3), ftolByte(colors[i].rgbGreen, 1.3),
                                ftolByte(colors[i].rgbRed, 1.3)};
        brighter[i] = findNearestColor(palette, bgr);
    }

    for (int32_t i = FIRST_FREE_COLOR; i < END_FREE_COLOR; i++)
    {
        const RGBQUAD& color = colors[i];
        const uint8_t green[3] = {ftolByte(color.rgbBlue, 0.3), clampedByte(color.rgbGreen, 1.2),
                                  ftolByte(color.rgbRed, 0.3)};
        greenTint[i] = findNearestColor(palette, green);
        uint8_t bgr[3];

        if (color.rgbRed < DARK_COMPONENT && color.rgbGreen < DARK_COMPONENT && color.rgbBlue < DARK_COMPONENT)
        {
            bgr[0] = DARK_HIGHLIGHT;
            bgr[1] = DARK_HIGHLIGHT;
            bgr[2] = DARK_HIGHLIGHT;
        }
        else if (color.rgbRed == KEPT_COMPONENT || color.rgbGreen == KEPT_COMPONENT || color.rgbBlue == KEPT_COMPONENT)
        {
            // Original behaviour (EB-045): blue is taken from red.
            bgr[0] = color.rgbRed;
            bgr[1] = color.rgbGreen;
            bgr[2] = color.rgbRed;
        }
        else
        {
            bgr[2] = clampedByte(color.rgbRed, 1.5);
            bgr[1] = clampedByte(color.rgbGreen, 1.5);
            bgr[0] = clampedByte(color.rgbBlue, 1.5);
        }

        highlight[i] = findNearestColor(palette, bgr);
    }

    for (int32_t i = FIRST_FREE_COLOR; i < END_FREE_COLOR; i++)
    {
        const RGBQUAD& color = colors[i];
        const uint8_t bgr[3] = {ftolByte(color.rgbBlue, 0.3), ftolByte(color.rgbGreen, 0.3),
                                clampedByte(color.rgbRed, 1.5)};
        redTint[i] = findNearestColor(palette, bgr);
    }

    for (int32_t i = FIRST_FREE_COLOR; i < END_FREE_COLOR; i++)
    {
        const RGBQUAD& color = colors[i];
        const uint8_t bgr[3] = {ftolByte(color.rgbBlue, 0.3), ftolByte(color.rgbGreen, 0.3),
                                ftolByte(color.rgbGreen, 0.7)};
        amberTint[i] = findNearestColor(palette, bgr);
    }

    const CString gamma = AfxGetApp()->GetProfileString("Gamma", "LastGamma", "1.0");
    setGamma(static_cast<float>(std::atof(gamma)));

    HDC dc = GetDC(nullptr);
    HINSTANCE module = GetModuleHandle(nullptr);

    if (notAllowedBitmap != nullptr)
    {
        DeleteObject(notAllowedBitmap);
    }

    // A copy of the header with the palette's colours, for the sign's size; GetDIBits puts the bitmap's own colours in
    // it.
    auto info = reinterpret_cast<BITMAPINFO*>(new uint8_t[BITMAP_INFO_ALLOC_SIZE]());
    std::memcpy(info, editorPalette->bitmapInfo, sizeof(BITMAPINFOHEADER) + PALETTE_ENTRIES * sizeof(RGBQUAD));
    info->bmiHeader.biWidth = NOT_ALLOWED_SIZE;
    info->bmiHeader.biHeight = NOT_ALLOWED_SIZE;
    notAllowedBitmap = static_cast<HBITMAP>(
        LoadImage(module, MAKEINTRESOURCE(IDB_NOT_ALLOWED), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
    GetDIBits(dc, notAllowedBitmap, 0, NOT_ALLOWED_SIZE, notAllowedBits, info, DIB_RGB_COLORS);

    for (uint8_t& pixel : notAllowedBits)
    {
        const RGBQUAD& color = info->bmiColors[pixel];

        if (color.rgbRed > KEY_RED_BLUE_MIN && color.rgbBlue > KEY_RED_BLUE_MIN && color.rgbGreen < KEY_GREEN_MAX)
        {
            pixel = TRANSPARENT_INDEX;
        }
    }

    ReleaseDC(nullptr, dc);
    delete[] reinterpret_cast<uint8_t*>(info);
    return true;
}

bool EditorPalette::setLogisticPalette(const uint8_t* rgb)
{
    if (logisticColors == nullptr)
    {
        logisticColors = reinterpret_cast<RGBQUAD*>(new uint8_t[PALETTE_ENTRIES * sizeof(RGBQUAD)]());
    }

    for (int32_t i = 0; i < PALETTE_ENTRIES; i++, rgb += 3)
    {
        logisticColors[i].rgbRed = expand(rgb[0]);
        logisticColors[i].rgbGreen = expand(rgb[1]);
        logisticColors[i].rgbBlue = expand(rgb[2]);
        logisticColors[i].rgbReserved = 0;
    }

    return true;
}

bool EditorPalette::setGamma(float gamma)
{
    const float exponent = 1.0f / gamma;

    // Original behaviour (EB-044): only entries 10-232 are corrected.
    for (int32_t i = FIRST_FREE_COLOR; i < END_GAMMA_COLOR; i++)
    {
        const PALETTEENTRY& base = basePalette->palPalEntry[i];
        PALETTEENTRY& corrected = gammaPalette->palPalEntry[i];
        corrected.peRed = gammaComponent(base.peRed, exponent);
        corrected.peGreen = gammaComponent(base.peGreen, exponent);
        corrected.peBlue = gammaComponent(base.peBlue, exponent);
        RGBQUAD& color = bitmapInfo->bmiColors[i];
        color.rgbRed = corrected.peRed;
        color.rgbGreen = corrected.peGreen;
        color.rgbBlue = corrected.peBlue;
        color.rgbReserved = 0;
    }

    CString text;
    text.Format("%lf", static_cast<double>(gamma));
    AfxGetApp()->WriteProfileString("Gamma", "LastGamma", text);
    return true;
}

void EditorPalette::rebuildBrighter(const uint8_t* colors)
{
    const uint8_t* color = colors;

    for (int32_t i = 0; i < PALETTE_ENTRIES; i++, color += sizeof(RGBQUAD))
    {
        const uint8_t bgr[3] = {ftolByte(color[0], 1.3), ftolByte(color[1], 1.3), ftolByte(color[2], 1.3)};
        brighter[i] = findNearestColor(colors, bgr);
    }
}

bool EditorPalette::setFadeTable(DaoDatabase& database, uint8_t* table, int32_t size)
{
    delete[] fadeTable;
    fadeTable = table;
    loadMoverTables(database);
    return true;
}

uint8_t* EditorPalette::getNotAllowedBits(CSize& size)
{
    size.cx = NOT_ALLOWED_SIZE;
    size.cy = NOT_ALLOWED_SIZE;
    return notAllowedBits;
}
