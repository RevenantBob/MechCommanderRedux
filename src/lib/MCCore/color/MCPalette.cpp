#include "stdafx.h"
#include "color/MCPalette.h"
#include "gui/asystem.h"
#include "lib/MCFile.h"
#include "lib/MCFitIniFile.h"
#include "platform/MCRenderer.h"
#include "vfx/MCVfx.h"

std::string PalettePath;

namespace
{
    /// <summary>A whole file of the palette path, or why it couldn't be read.</summary>
    std::expected<std::vector<uint8_t>, std::string> ReadPaletteFile(std::string_view name, std::string_view extension)
    {
        const std::string fileName = GamePath(PalettePath, name, extension);
        MCFile file;

        if (const int32_t result = file.Open(fileName); result != 0)
        {
            return std::unexpected(std::format("Could not open {} ({:#x})", fileName, static_cast<uint32_t>(result)));
        }

        std::vector<uint8_t> data(file.FileSize());
        file.Read(data);
        return data;
    }

    /// <summary>A FIT error as the message of a failed load.</summary>
    std::string FitMessage(std::string_view fileName, std::string_view what, MCFitError error)
    {
        return std::format("{}: could not read {} ({:#x})", fileName, what,
                           static_cast<uint32_t>(std::to_underlying(error)));
    }
}

MCPalette::MCPalette(std::span<const uint8_t> palFile, std::vector<uint8_t> fadeTables, int32_t numBitmapHazeLevels)
    : FadePalettes(std::move(fadeTables)), NumBitmapHazeLevels(numBitmapHazeLevels)
{
    // The .pal header's colour count; the colours fill the palette from entry 0 whatever its first colour says.
    uint16_t numColors = 0;

    if (palFile.size() >= 4)
    {
        std::memcpy(&numColors, palFile.data() + 2, sizeof(numColors));
    }

    RgbData.assign(static_cast<size_t>(ColorCount) * 3, 0);
    const size_t size = std::min(
        {static_cast<size_t>(numColors) * 3, RgbData.size(), palFile.size() >= 4 ? palFile.size() - 4 : size_t{0}});
    std::copy_n(palFile.begin() + (palFile.size() >= 4 ? 4 : 0), size, RgbData.begin());

    if (!FadePalettes.empty())
    {
        MCRenderer::RegisterData(FadePalettes.data(), FadePalettes.size(), MCDataKind::Tables);
    }
}

MCPalette::~MCPalette()
{
    if (!FadePalettes.empty())
    {
        MCRenderer::UnregisterData(FadePalettes.data());
    }
}

auto MCPalette::Create(std::string_view paletteFileName) -> std::expected<std::unique_ptr<MCPalette>, std::string>
{
    const std::string fileName = GamePath(PalettePath, paletteFileName, ".fit");
    MCFitIniFile paletteFile;

    if (const int32_t result = paletteFile.Open(fileName); result != 0)
    {
        return std::unexpected(std::format("Could not open {} ({:#x})", fileName, static_cast<uint32_t>(result)));
    }

    if (paletteFile.SeekBlock("Palette") != 0)
    {
        return std::unexpected(FitMessage(fileName, "[Palette]", MCFitError::BlockNotFound));
    }

    const MCFitResult<int32_t> hazeLevels = paletteFile.Read<int32_t>("NumBitmapHazeLevels");

    if (!hazeLevels)
    {
        return std::unexpected(FitMessage(fileName, "NumBitmapHazeLevels", hazeLevels.error()));
    }

    const MCFitResult<std::string> palName = paletteFile.Read<std::string>("PaletteFileName");

    if (!palName)
    {
        return std::unexpected(FitMessage(fileName, "PaletteFileName", palName.error()));
    }

    if (paletteFile.SeekBlock("Tables") != 0)
    {
        return std::unexpected(FitMessage(fileName, "[Tables]", MCFitError::BlockNotFound));
    }

    const MCFitResult<std::string> fadeName = paletteFile.Read<std::string>("FadeTableFile");

    if (!fadeName)
    {
        return std::unexpected(FitMessage(fileName, "FadeTableFile", fadeName.error()));
    }

    const std::expected<std::vector<uint8_t>, std::string> palFile = ReadPaletteFile(*palName, ".pal");

    if (!palFile)
    {
        return std::unexpected(palFile.error());
    }

    std::expected<std::vector<uint8_t>, std::string> fadeTables = ReadPaletteFile(*fadeName, ".tbl");

    if (!fadeTables)
    {
        return std::unexpected(fadeTables.error());
    }

    return std::make_unique<MCPalette>(*palFile, std::move(*fadeTables), *hazeLevels);
}

auto MCPalette::GetHazePalette(int32_t hazeLevel) -> uint8_t*
{
    if (hazeLevel == 0)
    {
        return nullptr;
    }

    // Levels 1..n are the first set, -1..-n the second.
    const int32_t level = std::min(hazeLevel > 0 ? hazeLevel : -hazeLevel, NumBitmapHazeLevels);
    const int32_t set = hazeLevel > 0 ? 0 : NumBitmapHazeLevels;
    return FadePalettes.data() + static_cast<ptrdiff_t>(set + level - 1) * FadeTableSize;
}

auto MCPalette::GetFadeTable(int32_t index) -> uint8_t*
{
    return FadePalettes.data() + static_cast<ptrdiff_t>(index + NumBitmapHazeLevels * 2) * FadeTableSize;
}

auto MCPalette::Activate() -> void
{
    Application->ActivatePalette(RgbData.data(), 0, ColorCount);
}

auto MCPalette::TweakPalette(int32_t start, std::span<const MCVfxRgb> colors) -> void
{
    for (const MCVfxRgb& color : colors)
    {
        std::memcpy(RgbData.data() + static_cast<size_t>(start & 0xff) * 3, &color, 3);
        start++;
    }
}
