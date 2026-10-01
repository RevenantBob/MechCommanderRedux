#pragma once

#pragma pack(push, 1)
/// <summary>
/// The 18-byte header of a Targa file as <see cref="writeTGA8Bit"/> writes it: colour-mapped (type 1), 256 entries
/// of 24 bits, 8 bits per pixel, top-left origin (descriptor 0x20).
/// </summary>
struct TGAFileHeader
{
    uint8_t imageIdLength;     // +0x00 (0)
    uint8_t colorMapType;      // +0x01 (1)
    uint8_t imageType;         // +0x02 (1: colour-mapped)
    uint16_t colorMapFirst;    // +0x03 (0)
    uint16_t colorMapLength;   // +0x05 (256)
    uint8_t colorMapEntrySize; // +0x07 (24)
    uint16_t xOrigin;          // +0x08 (0)
    uint16_t yOrigin;          // +0x0a (0)
    uint16_t width;            // +0x0c
    uint16_t height;           // +0x0e
    uint8_t pixelDepth;        // +0x10 (8)
    uint8_t imageDescriptor;   // +0x11 (0x20)
};
#pragma pack(pop)
static_assert(sizeof(TGAFileHeader) == 18);

/// <summary>
/// Writes <paramref name="width"/> x <paramref name="height"/> 8-bit pixels as a colour-mapped TGA
/// <paramref name="fileName"/>, with the game palette (6-bit values shifted up, stored BGR).
/// </summary>
/// <remarks>MCX.EXE @ 0x006b4fe0 (<c>engine\writegif.cpp</c>, despite its name).</remarks>
void writeTGA8Bit(char* fileName, uint8_t* pixels, uint32_t width, uint32_t height);
