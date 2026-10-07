#include "stdafx.h"
#include "engine/writegif.h"
#include "color/color.h"
#include "lib/MCFile.h"

auto WriteTga8Bit(char* fileName, uint8_t* pixels, uint32_t width, uint32_t height) -> void
{
    MCFile file;

    if (file.Create(fileName) != 0)
    {
        return;
    }

    MCTgaFileHeader header{};
    header.ImageIdLength = 0;
    header.ColorMapType = 1;
    header.ImageType = 1;
    header.ColorMapFirst = 0;
    header.ColorMapLength = 0x100;
    header.ColorMapEntrySize = 0x18;
    header.XOrigin = 0;
    header.YOrigin = 0;
    header.Width = static_cast<uint16_t>(width);
    header.Height = static_cast<uint16_t>(height);
    header.PixelDepth = 8;
    header.ImageDescriptor = 0x20;
    file.Write(reinterpret_cast<uint8_t*>(&header), 0x12);
    const uint8_t* rgb = GamePalette->RgbData.get();

    for (int32_t i = 0; i < 0x100; i++, rgb += 3)
    {
        file.WriteByte(static_cast<uint8_t>(rgb[2] << 2));
        file.WriteByte(static_cast<uint8_t>(rgb[1] << 2));
        file.WriteByte(static_cast<uint8_t>(rgb[0] << 2));
    }

    for (int32_t row = static_cast<int32_t>(height); row > 0; row--)
    {
        for (int32_t column = static_cast<int32_t>(width); column > 0; column--)
        {
            file.WriteByte(*pixels++);
        }
    }

    file.Close();
}
