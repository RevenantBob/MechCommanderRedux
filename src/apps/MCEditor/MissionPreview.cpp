#include "stdafx.h"
#include "MissionPreview.h"

namespace
{
    /// <summary>The colours of the picture's palette.</summary>
    inline constexpr int32_t PREVIEW_COLORS = 256;
    /// <summary>Bytes allocated for the DIB header and its colours.</summary>
    inline constexpr size_t PREVIEW_BITMAP_INFO_SIZE = sizeof(BITMAPINFOHEADER) + PREVIEW_COLORS * sizeof(RGBQUAD);
    /// <summary>Bytes allocated for the LOGPALETTE (four more than 256 entries need).</summary>
    inline constexpr size_t PREVIEW_LOG_PALETTE_SIZE = 0x408;
    /// <summary>A LOGPALETTE's version.</summary>
    inline constexpr WORD LOG_PALETTE_VERSION = 0x300;

    /// <summary>The size of a TGA file's header.</summary>
    inline constexpr UINT TGA_HEADER_SIZE = 0x12;
    /// <summary>The header's image type byte, and the type of a colour-mapped image.</summary>
    inline constexpr size_t TGA_IMAGE_TYPE = 2;
    inline constexpr uint8_t TGA_COLOR_MAPPED = 1;
    /// <summary>The header's colour map length (16 bits).</summary>
    inline constexpr size_t TGA_COLOR_MAP_LENGTH = 5;
    /// <summary>The header's width and height (16 bits, signed as the editor reads them).</summary>
    inline constexpr size_t TGA_WIDTH = 12;
    inline constexpr size_t TGA_HEIGHT = 14;
    /// <summary>The colour map the picture takes: 256 blue, green, red triples.</summary>
    inline constexpr UINT TGA_COLOR_MAP_SIZE = PREVIEW_COLORS * 3;

    /// <summary>The 16-bit value at <paramref name="offset"/> of <paramref name="bytes"/>.</summary>
    uint16_t readWord(const uint8_t* bytes, size_t offset)
    {
        return static_cast<uint16_t>(bytes[offset] | bytes[offset + 1] << 8);
    }
}

MissionPreview::MissionPreview()
{
    _bitmapInfo = reinterpret_cast<BITMAPINFO*>(new uint8_t[PREVIEW_BITMAP_INFO_SIZE]());
    _bitmapInfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    _bitmapInfo->bmiHeader.biPlanes = 1;
    _bitmapInfo->bmiHeader.biBitCount = 8;
    _bitmapInfo->bmiHeader.biCompression = BI_RGB;
    _logPalette = reinterpret_cast<LOGPALETTE*>(new uint8_t[PREVIEW_LOG_PALETTE_SIZE]());
    _logPalette->palVersion = LOG_PALETTE_VERSION;
    _logPalette->palNumEntries = PREVIEW_COLORS;
}

MissionPreview::~MissionPreview()
{
    delete[] reinterpret_cast<uint8_t*>(_bitmapInfo);

    if (_dib != nullptr)
    {
        DeleteObject(_dib);
    }

    delete _palette;

    if (_pixels != nullptr)
    {
        delete[] _pixels;
        _pixels = nullptr;
    }

    delete _memoryDC;
    delete[] reinterpret_cast<uint8_t*>(_logPalette);
}

BEGIN_MESSAGE_MAP(MissionPreview, CStatic)
ON_WM_PAINT()
END_MESSAGE_MAP()

void MissionPreview::load(const CString& path)
{
    _path = path;
    _dirty = true;

    if (_pixels != nullptr)
    {
        delete[] _pixels;
        _pixels = nullptr;
    }

    CFile file;

    if (!file.Open(path, CFile::modeRead | CFile::shareDenyNone))
    {
        _bitmapInfo->bmiHeader.biHeight = 0;
        _bitmapInfo->bmiHeader.biWidth = 0;
        return;
    }

    if (_dib != nullptr)
    {
        DeleteObject(_dib);
        _dib = nullptr;
    }

    if (_palette != nullptr)
    {
        delete _palette;
        _palette = nullptr;
    }

    uint8_t header[TGA_HEADER_SIZE];
    file.Read(header, TGA_HEADER_SIZE);

    // Original behaviour (EB-054): any other image keeps the last one's size and no pixels.
    if (header[TGA_IMAGE_TYPE] != TGA_COLOR_MAPPED || readWord(header, TGA_COLOR_MAP_LENGTH) != PREVIEW_COLORS)
    {
        return;
    }

    uint8_t colorMap[TGA_COLOR_MAP_SIZE];
    file.Read(colorMap, TGA_COLOR_MAP_SIZE);
    const auto width = static_cast<int16_t>(readWord(header, TGA_WIDTH));
    const auto height = static_cast<int16_t>(readWord(header, TGA_HEIGHT));
    _pixels = new uint8_t[static_cast<size_t>(width * height)];
    file.Read(_pixels, static_cast<UINT>(width * height));

    for (int32_t i = 0; i < PREVIEW_COLORS; i++)
    {
        RGBQUAD& color = _bitmapInfo->bmiColors[i];
        color.rgbBlue = colorMap[i * 3];
        color.rgbGreen = colorMap[i * 3 + 1];
        color.rgbRed = colorMap[i * 3 + 2];
        PALETTEENTRY& entry = _logPalette->palPalEntry[i];
        entry.peRed = color.rgbRed;
        entry.peBlue = color.rgbBlue;
        entry.peGreen = color.rgbGreen;
        entry.peFlags = 0;
    }

    _bitmapInfo->bmiHeader.biHeight = -height;
    _bitmapInfo->bmiHeader.biWidth = width;
}

void MissionPreview::OnPaint()
{
    CPaintDC dc(this);

    if (_dirty)
    {
        if (_memoryDC == nullptr)
        {
            _memoryDC = new CDC;
            _memoryDC->Attach(CreateCompatibleDC(dc.GetSafeHdc()));
        }

        if (_dib == nullptr)
        {
            _dib = CreateDIBSection(dc.m_hDC, _bitmapInfo, DIB_RGB_COLORS, &_dibBits, nullptr, 0);
        }

        // Original behaviour (EB-054): the copy reads from a null pointer after an image without 256 colours.
        if (_pixels != nullptr)
        {
            std::memcpy(_dibBits, _pixels,
                        static_cast<size_t>(-_bitmapInfo->bmiHeader.biHeight * _bitmapInfo->bmiHeader.biWidth));
        }

        _dirty = false;
    }

    if (_palette == nullptr)
    {
        _palette = new CPalette;
        _palette->Attach(CreatePalette(_logPalette));
    }

    dc.SelectPalette(_palette, TRUE);
    RealizePalette(dc.m_hDC);

    if (_memoryDC == nullptr)
    {
        return;
    }

    HGDIOBJ oldBitmap = nullptr;

    if (_dib != nullptr)
    {
        oldBitmap = SelectObject(_memoryDC->m_hDC, _dib);
    }

    // The window's rectangle, not the client's: the static has no border, so they are the same size.
    RECT rect;
    ::GetWindowRect(m_hWnd, &rect);
    dc.SetStretchBltMode(COLORONCOLOR);
    StretchBlt(dc.m_hDC, 0, 0, rect.right - rect.left, rect.bottom - rect.top, _memoryDC->m_hDC, 0, 0,
               _bitmapInfo->bmiHeader.biWidth, -_bitmapInfo->bmiHeader.biHeight, SRCCOPY);

    if (oldBitmap != nullptr)
    {
        SelectObject(_memoryDC->m_hDC, oldBitmap);
    }
}
