#include "stdafx.h"
#include "engine/MCWriteTga.h"
#include "color/MCPalette.h"
#include "lib/MCFile.h"

auto WriteTga8Bit(std::string_view fileName, std::span<const uint8_t> pixels, uint32_t width, uint32_t height) -> void
{
    MCFile file;

    if (file.Create(fileName) != 0)
    {
        return;
    }

    MCTgaFileHeader header{};
    header.Width = static_cast<uint16_t>(width);
    header.Height = static_cast<uint16_t>(height);
    file.Write(std::span(reinterpret_cast<const uint8_t*>(&header), sizeof(header)));

    std::array<uint8_t, 0x300> colorMap{};
    const uint8_t* rgb = GamePalette()->RgbData.data();

    for (size_t i = 0; i < 0x100; i++)
    {
        colorMap[i * 3] = static_cast<uint8_t>(rgb[i * 3 + 2] << 2);
        colorMap[i * 3 + 1] = static_cast<uint8_t>(rgb[i * 3 + 1] << 2);
        colorMap[i * 3 + 2] = static_cast<uint8_t>(rgb[i * 3] << 2);
    }

    file.Write(colorMap);
    file.Write(pixels.first(static_cast<size_t>(width) * height));
}
