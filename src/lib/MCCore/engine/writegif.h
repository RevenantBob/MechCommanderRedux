#pragma once

#pragma pack(push, 1)
/// <summary>
/// The 18-byte header of a Targa file as <see cref="WriteTga8Bit"/> writes it: colour-mapped (type 1), 256 entries
/// of 24 bits, 8 bits per pixel, top-left origin (descriptor 0x20).
/// </summary>
struct MCTgaFileHeader
{
    uint8_t ImageIdLength;     // (0)
    uint8_t ColorMapType;      // (1)
    uint8_t ImageType;         // (1: colour-mapped)
    uint16_t ColorMapFirst;    // (0)
    uint16_t ColorMapLength;   // (256)
    uint8_t ColorMapEntrySize; // (24)
    uint16_t XOrigin;          // (0)
    uint16_t YOrigin;          // (0)
    uint16_t Width;
    uint16_t Height;
    uint8_t PixelDepth;      // (8)
    uint8_t ImageDescriptor; // (0x20)
};
#pragma pack(pop)
static_assert(sizeof(MCTgaFileHeader) == 18);

/// <summary>
/// Writes <paramref name="width"/> x <paramref name="height"/> 8-bit pixels as a colour-mapped TGA
/// <paramref name="fileName"/>, with the game palette (6-bit values shifted up, stored BGR).
/// </summary>
void WriteTga8Bit(char* fileName, uint8_t* pixels, uint32_t width, uint32_t height);
