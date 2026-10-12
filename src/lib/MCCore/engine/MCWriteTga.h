#pragma once

#pragma pack(push, 1)
/// <summary>
/// The 18-byte header of a Targa file as <see cref="WriteTga8Bit"/> writes it: colour-mapped (type 1), 256 entries
/// of 24 bits, 8 bits per pixel, top-left origin (descriptor 0x20).
/// </summary>
struct MCTgaFileHeader
{
    /// <summary>No image id.</summary>
    uint8_t ImageIdLength = 0;
    /// <summary>A colour map follows the header.</summary>
    uint8_t ColorMapType = 1;
    /// <summary>1: colour-mapped.</summary>
    uint8_t ImageType = 1;
    /// <summary>The colour map's first entry and its number of entries.</summary>
    uint16_t ColorMapFirst = 0;
    uint16_t ColorMapLength = 0x100;
    /// <summary>Bits per colour map entry (BGR).</summary>
    uint8_t ColorMapEntrySize = 0x18;
    /// <summary>The image's origin and size.</summary>
    uint16_t XOrigin = 0;
    uint16_t YOrigin = 0;
    uint16_t Width = 0;
    uint16_t Height = 0;
    /// <summary>Bits per pixel.</summary>
    uint8_t PixelDepth = 8;
    /// <summary>0x20: the first row is the top one.</summary>
    uint8_t ImageDescriptor = 0x20;
};
#pragma pack(pop)
static_assert(sizeof(MCTgaFileHeader) == 18);

/// <summary>
/// Writes <paramref name="width"/> x <paramref name="height"/> 8-bit <paramref name="pixels"/> as a colour-mapped
/// TGA <paramref name="fileName"/>, with the game palette (6-bit values shifted up, stored BGR). A screenshot.
/// </summary>
/// <remarks>Original source: <c>engine\writegif.cpp</c>.</remarks>
void WriteTga8Bit(std::string_view fileName, std::span<const uint8_t> pixels, uint32_t width, uint32_t height);
