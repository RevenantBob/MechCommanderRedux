#include "stdafx.h"
#include "engine/writegif.h"
#include "color/color.h"
#include "lib/file.h"

auto writeTGA8Bit(char* fileName, uint8_t* pixels, uint32_t width, uint32_t height) -> void
{
    File file;

    if (file.create(fileName) != 0)
    {
        return;
    }

    TGAFileHeader header{};
    header.imageIdLength = 0;
    header.colorMapType = 1;
    header.imageType = 1;
    header.colorMapFirst = 0;
    header.colorMapLength = 0x100;
    header.colorMapEntrySize = 0x18;
    header.xOrigin = 0;
    header.yOrigin = 0;
    header.width = static_cast<uint16_t>(width);
    header.height = static_cast<uint16_t>(height);
    header.pixelDepth = 8;
    header.imageDescriptor = 0x20;
    file.write(reinterpret_cast<uint8_t*>(&header), 0x12);
    const uint8_t* rgb = gamePalette->rgbData.get();

    for (int32_t i = 0; i < 0x100; i++, rgb += 3)
    {
        file.writeByte(static_cast<uint8_t>(rgb[2] << 2));
        file.writeByte(static_cast<uint8_t>(rgb[1] << 2));
        file.writeByte(static_cast<uint8_t>(rgb[0] << 2));
    }

    for (int32_t row = static_cast<int32_t>(height); row > 0; row--)
    {
        for (int32_t column = static_cast<int32_t>(width); column > 0; column--)
        {
            file.writeByte(*pixels++);
        }
    }

    file.close();
}
