#pragma once

// The Select Mission dialog's picture of the chosen map (0x00431fe0-0x00432540, its virtual table at 0x004fe110): an
// 8-bit colour-mapped .tga from the map's folder, stretched over the control. Its source file name wasn't kept; the
// names are the port's.

/// <summary>A static control that shows an 8-bit colour-mapped TGA image, stretched to its size.</summary>
/// <remarks>0x60 bytes in the original.</remarks>
class MissionPreview : public CStatic
{
public:
    /// <summary>An empty picture: an 8-bit top-down DIB header and a 256-entry palette, no pixels.</summary>
    /// <remarks>0x00431fe0.</remarks>
    MissionPreview();
    /// <summary>Frees the image, the DIB section, the memory DC and the palette.</summary>
    /// <remarks>0x004320e0.</remarks>
    ~MissionPreview() override;
    MissionPreview(const MissionPreview&) = delete;
    MissionPreview& operator=(const MissionPreview&) = delete;

    /// <summary>
    /// Reads the image <paramref name="path"/> for the next paint: its palette and pixels when it is colour-mapped
    /// with 256 colours; a missing file leaves an empty picture.
    /// </summary>
    /// <remarks>0x00432190. The caller redraws the control.</remarks>
    void load(const CString& path);

protected:
    /// <summary>Copies a newly loaded image into the DIB section, then stretches it over the control.</summary>
    /// <remarks>0x00432360.</remarks>
    afx_msg void OnPaint();

    DECLARE_MESSAGE_MAP()

private:
    /// <summary>The image's file (+0x3c).</summary>
    CString _path;
    /// <summary>An image was loaded since the last paint (+0x40).</summary>
    bool _dirty = false;
    /// <summary>The image's pixels, top row first, or null (+0x44).</summary>
    uint8_t* _pixels = nullptr;
    /// <summary>The DIB's header and colours (+0x48); the height is negative (top-down).</summary>
    BITMAPINFO* _bitmapInfo = nullptr;
    /// <summary>The DIB section the image is copied into, made on the first paint after a load (+0x4c).</summary>
    HBITMAP _dib = nullptr;
    /// <summary>The DIB section's pixels (+0x50).</summary>
    void* _dibBits = nullptr;
    /// <summary>The memory DC the DIB section is selected into to draw it (+0x54).</summary>
    CDC* _memoryDC = nullptr;
    /// <summary>The image's palette, made on the first paint after a load (+0x58).</summary>
    CPalette* _palette = nullptr;
    /// <summary>The image's colours as a LOGPALETTE (+0x5c).</summary>
    LOGPALETTE* _logPalette = nullptr;
};
