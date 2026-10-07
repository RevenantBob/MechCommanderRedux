#include "stdafx.h"
#include "color/color.h"
#include "gui/asystem.h"
#include "lib/cident.h"
#include "lib/file.h"
#include "lib/inifile.h"
#include "mission/scenario.h"
#include "platform/MCDisplay.h"
#include "platform/MCFileSystem.h"
#include "platform/MCRenderer.h"
#include "platform/MCInput.h"

MCPalette* GamePalette = nullptr;
uint8_t WaterMagicColors[8] = {0x5a, 0x59, 0x5a, 0x5b, 0x5d, 0x5c, 0x5b, 0x59};
uint8_t CurrentMagic = 0;
char PalettePath[80] = {};

int32_t MCPalette::_LastMinDepth = -1;
int32_t MCPalette::_LastMaxDepth = -1;
int32_t MCPalette::_LastHazePercent = 0;

namespace
{
    /// <summary>A 32-bit left shift as x86 does it: the count taken mod 32.</summary>
    int32_t Shl(int32_t value, int32_t count)
    {
        return static_cast<int32_t>(static_cast<uint32_t>(value) << (count & 31));
    }

    /// <summary>A 32-bit arithmetic right shift as x86 does it: the count taken mod 32.</summary>
    int32_t Sar(int32_t value, int32_t count)
    {
        return value >> (count & 31);
    }

    /// <summary>
    /// The absolute value of a byte delta as the fades compute it: in 8 bits, so -128 stays -128.
    /// </summary>
    int32_t ByteAbs(uint8_t delta)
    {
        return static_cast<int8_t>(std::abs(static_cast<int32_t>(static_cast<int8_t>(delta))));
    }

    /// <summary>
    /// What is left of a fade delta after moving <paramref name="step"/> toward 0 (0 once the step covers it).
    /// </summary>
    int8_t FadeRemainder(int8_t delta, int8_t step)
    {
        if (delta < 1)
        {
            if (delta < 0)
            {
                if (-static_cast<int32_t>(step) == delta || -static_cast<int32_t>(delta) < step)
                {
                    return 0;
                }

                return static_cast<int8_t>(delta + step);
            }

            return delta;
        }

        if (step < delta)
        {
            return static_cast<int8_t>(delta - step);
        }

        return 0;
    }

    /// <summary>The fade step: the largest delta times the percentage, at least 1 while any is left.</summary>
    int8_t FadeStep(int32_t maxFadeDelta, float fadePercent)
    {
        if (static_cast<float>(maxFadeDelta) * fadePercent > 0.0f)
        {
            const int8_t step = static_cast<int8_t>(
                static_cast<int32_t>(std::floor(static_cast<double>(static_cast<float>(maxFadeDelta) * fadePercent))));
            return step == 0 ? 1 : step;
        }

        return 0;
    }
}

void MCPaletteBlock::InitRgbData(uint8_t* data)
{
    const int16_t count = *reinterpret_cast<int16_t*>(data + 2);
    FirstColor = *reinterpret_cast<int16_t*>(data);
    NumColors = count;
    const uint32_t size = static_cast<uint32_t>(count * 3);

    if (RgbData == nullptr)
    {
        RgbData = std::make_unique<uint8_t[]>(size);
    }

    std::memcpy(RgbData.get(), data + 4, size);
}

void MCPaletteBlock::Destroy()
{
    RgbData.reset();
}

MCColorRange::MCColorRange(MCColorRangeData& data, MCPalette* palette)
{
    BaseColorIndex = data.BaseColorIndex;
    MaxLitColor = data.MaxLitColor;
    MaxHazedColor = data.MaxHazedColor;
    BaseRed = data.BaseRed;
    BaseGreen = data.BaseGreen;
    BaseBlue = data.BaseBlue;
    LightSourceFlag = data.LightSourceFlag;
    DepthCueFlag = data.DepthCueFlag;
    CalcLightSource = data.CalcLightSource;
    CalcDepthCue = data.CalcDepthCue;
    Palette = palette;
}

MCColorRange::MCColorRange(int32_t baseColorIndex, uint8_t baseRed, uint8_t baseGreen, uint8_t baseBlue,
                           int32_t maxLitColor, int32_t maxHazedColor, MCPalette* palette, int lightSourceFlag,
                           int depthCueFlag, int calcLightSource, int calcDepthCue)
{
    BaseColorIndex = baseColorIndex;
    BaseRed = baseRed;
    BaseGreen = baseGreen;
    BaseBlue = baseBlue;
    MaxLitColor = maxLitColor;
    MaxHazedColor = maxHazedColor;
    Palette = palette;
    LightSourceFlag = lightSourceFlag;
    DepthCueFlag = depthCueFlag;
    CalcLightSource = calcLightSource;
    CalcDepthCue = calcDepthCue;
}

uint8_t* MCPalette::DepthHazedShadePalette(int32_t shade, float depth)
{
    int32_t level = (((NumBitmapHazeLevels + 1) * shade + 0x80) >> 8) - 1;
    const int32_t hazePercent = HazePercentAtDepth(depth);

    if (level < 0)
    {
        level = 0;
    }

    if (hazePercent == 0)
    {
        return GetHazePalette(level - NumBitmapHazeLevels);
    }

    return GetHazePalette(
        (Sar((NumBitmapHazeLevels * 2 - level) * hazePercent + HazeRound, HazeShift) - NumBitmapHazeLevels) + level);
}

int32_t MCPalette::DepthHazedShade(int32_t colorRange, int32_t shade, float depth, int32_t maxShade)
{
    if (colorRange < 0 || NumColorRanges <= colorRange)
    {
        return 0;
    }

    MCColorRange& range = ColorRanges[colorRange];

    if (maxShade == 0)
    {
        maxShade = range.MaxLitColor;
    }

    const int32_t hazePercent = HazePercentAtDepth(depth);
    const int32_t litShade = (shade * maxShade + 0x80) >> 8;

    if (hazePercent == 0)
    {
        return range.BaseColorIndex + litShade;
    }

    return (range.BaseColorIndex - Sar(hazePercent * litShade + range.Palette->HazeRound, range.Palette->HazeShift)) +
           litShade;
}

uint8_t* MCPalette::GetHazePalette(int32_t hazeLevel)
{
    if (hazeLevel == 0)
    {
        return nullptr;
    }

    if (hazeLevel > 0)
    {
        if (NumBitmapHazeLevels < hazeLevel)
        {
            hazeLevel = NumBitmapHazeLevels;
        }

        return FadePalettes.get() + (hazeLevel * 0x100 - 0x100);
    }

    int32_t level = static_cast<int32_t>(0u - static_cast<uint32_t>(hazeLevel));

    if (NumBitmapHazeLevels < level)
    {
        level = NumBitmapHazeLevels;
    }

    return FadePalettes.get() + (level * 0x100 - 0x100 + HazePaletteOffset);
}

int32_t MCPalette::FindColorRange(int32_t colorIndex)
{
    int32_t found = -1;

    for (int32_t i = 0; i < NumColorRanges; ++i)
    {
        const MCColorRange& range = ColorRanges[i];

        if (range.BaseColorIndex <= colorIndex && colorIndex <= range.MaxHazedColor + range.BaseColorIndex)
        {
            found = i;
            break;
        }
    }

    return found;
}

int32_t MCPalette::FindLightToDarkColorRange(int32_t colorIndex)
{
    int32_t found = -1;

    for (int32_t i = 0; i < NumColorRanges; ++i)
    {
        const MCColorRange& range = ColorRanges[i];

        if (range.BaseColorIndex <= colorIndex && colorIndex <= range.MaxHazedColor + range.BaseColorIndex &&
            range.LightSourceFlag != 0 && range.DepthCueFlag != 0)
        {
            found = i;
            break;
        }
    }

    return found;
}

int32_t MCPalette::HazePercentAtDepth(float depth)
{
    const int32_t iDepth = static_cast<int32_t>(std::floor(static_cast<double>(depth)));

    if (_LastMinDepth != -1 && _LastMinDepth <= iDepth && (MaxHazeDepth <= _LastMinDepth || iDepth < _LastMaxDepth))
    {
        return _LastHazePercent;
    }

    if (iDepth < MinHazeDepth)
    {
        _LastMaxDepth = MinHazeDepth;
        _LastMinDepth = 0;
        _LastHazePercent = 0;
        return 0;
    }

    _LastMinDepth = MaxHazeDepth;

    if (_LastMinDepth <= iDepth)
    {
        _LastMaxDepth = _LastMinDepth;
        _LastHazePercent = MaxHazePercent;
        return MaxHazePercent;
    }

    // A binary search of the current table for the span holding the depth.
    const int32_t* table = CurrentDepthTable;
    int32_t index = (NumDepthHazeEntries >> 1) - 1;
    int32_t step = NumDepthHazeEntriesShift - 1;

    while (true)
    {
        _LastMinDepth = table[index];
        _LastMaxDepth = table[index + 1];
        --step;

        if (_LastMinDepth <= iDepth && iDepth < _LastMaxDepth)
        {
            break;
        }

        if (iDepth == _LastMaxDepth)
        {
            ++index;
            _LastMinDepth = _LastMaxDepth;
            _LastMaxDepth = table[index + 1];
            break;
        }

        if (iDepth < _LastMinDepth)
        {
            index += Shl(-1, step);
        }
        else
        {
            index += Shl(1, step);
        }
    }

    _LastHazePercent = Shl(index + 1, HazeShift - NumDepthHazeEntriesShift);
    return _LastHazePercent;
}

void MCPalette::FullCycleOn()
{
}

void MCPalette::FullCycleOff()
{
    InitRgbData(OriginalPalette.get());
    Activate(0, 0);
}

void MCPalette::FadeToPalette(float& fadePercent, uint8_t* targetPalette)
{
    if (fadePercent > 1.0f)
    {
        fadePercent = 1.0f;
    }

    uint8_t* shown = RgbData.get();
    const uint8_t* target = targetPalette + 4;

    if (FadeDeltasValid == 0)
    {
        const uint8_t* original = OriginalPalette.get() + 4;
        const int32_t count = static_cast<int32_t>(PaletteSize) - 4;
        MaxFadeDelta = 0;

        for (int32_t i = 0; i < count; ++i)
        {
            const uint8_t delta = static_cast<uint8_t>(target[i] - original[i]);
            FadeDeltas[i] = static_cast<int8_t>(delta);

            if (MaxFadeDelta < ByteAbs(delta))
            {
                MaxFadeDelta = ByteAbs(delta);
            }
        }

        FadeDeltasValid = 1;
    }

    if (fadePercent > 0.0f)
    {
        const int8_t step = FadeStep(MaxFadeDelta, fadePercent);

        // Original behaviour: colour c takes fadeDeltas[c] (the delta of byte c, not of its own channels) for all
        // three channels, and colour 255 is never faded.
        for (int32_t color = 0; color < 0xff; ++color)
        {
            const int8_t delta = FadeDeltas[color];
            uint8_t* entry = shown;

            for (int32_t channel = 0; channel < 3; ++channel)
            {
                *shown++ = static_cast<uint8_t>(*target++ - FadeRemainder(delta, step));
            }

            GamePalette->TweakPalette(color, 1, reinterpret_cast<MCVfxRgb*>(entry));
        }

        Activate(0, 0);
    }
}

void MCPalette::FadeToColor(float& fadePercent, char red, char green, char blue)
{
    if (fadePercent > 1.0f)
    {
        fadePercent = 1.0f;
    }

    // Original behaviour: a new colour restarts the fade only when all three components differ.
    if (FadeTarget != 2 || (red != static_cast<char>(FadeRed) && green != static_cast<char>(FadeGreen) &&
                            blue != static_cast<char>(FadeBlue)))
    {
        FadeRed = static_cast<uint8_t>(red);
        FadeTarget = 2;
        FadeDeltasValid = 0;
        FadeGreen = static_cast<uint8_t>(green);
        FadeBlue = static_cast<uint8_t>(blue);
    }

    const uint8_t fadeColor[3] = {FadeRed, FadeGreen, FadeBlue};

    if (FadeDeltasValid == 0)
    {
        MaxFadeDelta = 0;

        for (int32_t i = 0; i < 0x100 * 3; ++i)
        {
            const uint8_t delta = static_cast<uint8_t>(fadeColor[i % 3] - RgbData[i]);
            FadeDeltas[i] = static_cast<int8_t>(delta);

            if (MaxFadeDelta < ByteAbs(delta))
            {
                MaxFadeDelta = ByteAbs(delta);
            }
        }

        FadeDeltasValid = 1;
    }

    if (fadePercent > 0.0f)
    {
        const int8_t step = FadeStep(MaxFadeDelta, fadePercent);

        for (int32_t i = 0; i < 0x100 * 3; ++i)
        {
            RgbData[i] = static_cast<uint8_t>(fadeColor[i % 3] - FadeRemainder(FadeDeltas[i], step));
        }

        Activate(0, 0);
    }
}

void MCPalette::FadeToOriginalPalette(float& fadePercent)
{
    if (FadeTarget != 0)
    {
        FadeTarget = 0;
        FadeDeltasValid = 0;
    }

    FadeToPalette(fadePercent, OriginalPalette.get());
}

void MCPalette::FadeToBlackAndWhite(float& fadePercent)
{
    if (FadeTarget != 1)
    {
        FadeTarget = 1;
        FadeDeltasValid = 0;
    }

    FadeToPalette(fadePercent, BwPalette.get());
}

void MCPalette::RecalculateDepthVsHazeInfo(int32_t altitude)
{
    int32_t table;

    if (altitude < MaxAltitude)
    {
        table = Sar(altitude, AltitudeShift);
    }
    else
    {
        table = NumDepthAtHazeLevelTables - 1;
    }

    CurrentDepthTable = DepthHazeTables.get() + Shl(table, NumDepthHazeEntriesShift);
    MinHazeDepth = CurrentDepthTable[0];
    _LastMinDepth = -1;
    MaxHazeDepth = CurrentDepthTable[NumDepthHazeEntries - 1];
}

void MCPalette::Animate(int start, int count)
{
    Application->ActivatePalette(RgbData.get(), start, count);
}

void MCPalette::Activate(int32_t which, int32_t extractIndex)
{
    if (which == 1)
    {
        uint8_t* colors = nullptr;

        if (extractIndex < NumExtractPalettes && extractIndex >= 0)
        {
            colors = ExtractPalettes.get() + 4 + extractIndex * PALETTE_FILE_SIZE;
        }

        Application->ActivatePalette(colors, 0, 0x100);
        return;
    }

    if (which != 2)
    {
        Application->ActivatePalette(RgbData.get(), 0, 0x100);
        return;
    }

    // Original behaviour: the black-and-white palette is handed over with its 4-byte .pal header.
    Application->ActivatePalette(BwPalette.get(), 0, 0x100);
}

void MCPalette::TweakPalette(int start, int count, MCVfxRgb* colors)
{
    const uint8_t* source = reinterpret_cast<const uint8_t*>(colors);

    for (int index = start; index < start + count; ++index)
    {
        std::memcpy(GamePalette->RgbData.get() + (index & 0xff) * 3, source, 3);
        source += 3;
    }
}

void MCPalette::Init()
{
    OriginalPalette.reset();
    FadeDeltas.reset();
    BwPalette.reset();
    ExtractPalettes.reset();
    FadePalettes.reset();
    DepthHazeTables.reset();
    ColorRanges.clear();
    RgbData.reset();
}

int32_t MCPalette::Init(char* paletteFileName)
{
    MCFullPathFileName fileName;
    fileName.Init(PalettePath, paletteFileName, ".fit");
    MCFitIniFile paletteFile;
    int32_t result = paletteFile.Open(fileName, READ, 50);

    if (result == 0 && (result = paletteFile.SeekBlock("Palette")) == 0)
    {
        result = Init(paletteFile);

        if (result == 0)
        {
            paletteFile.Close();
            return 0;
        }
    }

    return result;
}

int32_t MCPalette::Init(MCFitIniFile& paletteFile)
{
    int32_t result = LoadPaletteInfo(paletteFile);

    if (result != 0)
    {
        return result;
    }

    result = paletteFile.SeekBlock("Tables");

    if (result != 0)
    {
        return result;
    }

    result = paletteFile.ReadIdString("FadeTableFile", FadeTableFile, 8);

    if (result != 0)
    {
        return result;
    }

    result = paletteFile.ReadIdString("DepthTableFile", DepthTableFile, 8);

    if (result != 0)
    {
        return result;
    }

    result = paletteFile.ReadIdString("AllFadeTableFile", AllFadeTableFile, 8);

    if (result != 0)
    {
        return result;
    }

    result = LoadTables();

    if (result != 0)
    {
        return result;
    }

    RecalculateDepthVsHazeInfo(0);
    return 0;
}

void MCPalette::Destroy()
{
    MCPaletteBlock::Destroy();
    OriginalPalette.reset();
    FadeDeltas.reset();
    BwPalette.reset();

    if (FadePalettes != nullptr)
    {
        MCRenderer::UnregisterData(FadePalettes.get());
        FadePalettes.reset();
    }

    if (AllFadePalettes != nullptr)
    {
        MCRenderer::UnregisterData(AllFadePalettes.get());
        AllFadePalettes.reset();
    }

    ExtractPalettes.reset();
    DepthHazeTables.reset();
    ColorRanges.clear();
}

int32_t MCPalette::LoadPaletteInfo(MCFitIniFile& paletteFile)
{
    int32_t result = paletteFile.ReadIdLong("HazeShift", HazeShift);

    if (result != 0)
    {
        return result;
    }

    result = paletteFile.ReadIdLong("NumBitmapHazeLevels", NumBitmapHazeLevels);

    if (result != 0)
    {
        return result;
    }

    result = paletteFile.ReadIdLong("NumDepthHazeEntriesShift", NumDepthHazeEntriesShift);

    if (result != 0)
    {
        return result;
    }

    result = paletteFile.ReadIdLong("NumDepthAtHazeLevelTables", NumDepthAtHazeLevelTables);

    if (result != 0)
    {
        return result;
    }

    result = paletteFile.ReadIdLong("AltitudeShift", AltitudeShift);

    if (result != 0)
    {
        return result;
    }

    const int32_t fullHaze = Shl(1, HazeShift);
    MaxHazePercent = fullHaze;
    HazeRound = fullHaze >> 1;
    HazePaletteOffset = NumBitmapHazeLevels << 8;
    NumDepthHazeEntries = Shl(1, NumDepthHazeEntriesShift);
    MaxAltitude = Shl(NumDepthAtHazeLevelTables, AltitudeShift);

    result = LoadPalette(paletteFile);

    if (result != 0)
    {
        return result;
    }

    LoadBWPalette(paletteFile);
    LoadExtractPalette(paletteFile);
    result = paletteFile.SeekBlock("Ranges");

    if (result != 0)
    {
        return result;
    }

    return LoadColorRanges(paletteFile);
}

int32_t MCPalette::LoadColorRanges(MCFitIniFile& paletteFile)
{
    int32_t result = paletteFile.ReadIdLong("NumColorRanges", NumColorRanges);

    if (result != 0)
    {
        return result;
    }

    const int32_t count = NumColorRanges;
    ColorRanges.assign(static_cast<size_t>(std::max(count, 0)), MCColorRange{});

    for (int32_t i = 0; i < count; ++i)
    {
        char blockId[12];
        std::snprintf(blockId, sizeof(blockId), "Range%d", i);
        result = paletteFile.SeekBlock(blockId);

        if (result != 0)
        {
            return result;
        }

        MCColorRangeData data{};

        if ((result = paletteFile.ReadIdUChar("BaseColorIndex", data.BaseColorIndex)) != 0)
        {
            return result;
        }

        if ((result = paletteFile.ReadIdUChar("MaxLitColor", data.MaxLitColor)) != 0)
        {
            return result;
        }

        if ((result = paletteFile.ReadIdUChar("MaxHazedColor", data.MaxHazedColor)) != 0)
        {
            return result;
        }

        if ((result = paletteFile.ReadIdUChar("BaseRed", data.BaseRed)) != 0)
        {
            return result;
        }

        if ((result = paletteFile.ReadIdUChar("BaseGreen", data.BaseGreen)) != 0)
        {
            return result;
        }

        if ((result = paletteFile.ReadIdUChar("BaseBlue", data.BaseBlue)) != 0)
        {
            return result;
        }

        if ((result = paletteFile.ReadIdUChar("LightSourceFlag", data.LightSourceFlag)) != 0)
        {
            return result;
        }

        if ((result = paletteFile.ReadIdUChar("DepthCueFlag", data.DepthCueFlag)) != 0)
        {
            return result;
        }

        if ((result = paletteFile.ReadIdUChar("CalcLightSource", data.CalcLightSource)) != 0)
        {
            return result;
        }

        if ((result = paletteFile.ReadIdUChar("CalcDepthCue", data.CalcDepthCue)) != 0)
        {
            return result;
        }

        ColorRanges[i] = MCColorRange(data, this);
    }

    return 0;
}

int32_t MCPalette::LoadTables()
{
    MCFullPathFileName depthName;
    depthName.Init(PalettePath, DepthTableFile, ".tbl");
    MCFullPathFileName fadeName;
    fadeName.Init(PalettePath, FadeTableFile, ".tbl");
    MCFullPathFileName allFadeName;
    allFadeName.Init(PalettePath, AllFadeTableFile, ".tbl");

    MCFile depthFile;
    MCFile fadeFile;
    MCFile allFadeFile;
    int32_t result = depthFile.Open(depthName, READ, 50);

    if (result != 0)
    {
        return result;
    }

    LoadDepthHazeTables(depthFile);
    depthFile.Close();
    result = fadeFile.Open(fadeName, READ, 50);

    if (result != 0)
    {
        return result;
    }

    LoadFadePalettes(fadeFile);
    fadeFile.Close();
    result = allFadeFile.Open(allFadeName, READ, 50);

    if (result != 0)
    {
        return result;
    }

    LoadAllFadePalettes(allFadeFile);
    allFadeFile.Close();
    return 0;
}

int32_t MCPalette::LoadPalette(MCFitIniFile& paletteFile)
{
    int32_t result = paletteFile.ReadIdString("PaletteFileName", PaletteFileName, 8);

    if (result != 0)
    {
        return result;
    }

    MCFullPathFileName fileName;
    fileName.Init(PalettePath, PaletteFileName, ".pal");
    MCFile file;
    result = file.Open(fileName, READ, 50);

    if (result != 0)
    {
        return result;
    }

    const uint32_t size = file.FileSize();
    OriginalPalette = std::make_unique<uint8_t[]>(size);

    file.Read(OriginalPalette.get(), static_cast<int32_t>(size));
    InitRgbData(OriginalPalette.get());
    FadeDeltasValid = 0;
    FadeTarget = -1;
    FadeBlue = 0;
    FadeGreen = 0;
    FadeRed = 0;
    PaletteSize = size;
    FadeDeltas = std::make_unique<int8_t[]>(size);

    file.Close();
    return 0;
}

int32_t MCPalette::LoadPalette()
{
    MCFullPathFileName fileName;
    fileName.Init(PalettePath, PaletteFileName, ".pal");
    MCFile file;
    int32_t result = file.Open(fileName, READ, 50);

    if (result != 0)
    {
        return result;
    }

    const uint32_t size = file.FileSize();

    if (OriginalPalette == nullptr)
    {
        OriginalPalette = std::make_unique<uint8_t[]>(size);
    }

    file.Read(OriginalPalette.get(), static_cast<int32_t>(size));
    InitRgbData(OriginalPalette.get());
    FadeDeltasValid = 0;
    FadeTarget = -1;
    FadeBlue = 0;
    FadeGreen = 0;
    FadeRed = 0;
    PaletteSize = size;

    if (FadeDeltas == nullptr)
    {
        FadeDeltas = std::make_unique<int8_t[]>(size);
    }

    file.Close();
    return 0;
}

int32_t MCPalette::SavePalette()
{
    MCFullPathFileName backupName;
    backupName.Init(PalettePath, PaletteFileName, ".bak");
    MCFullPathFileName fileName;
    fileName.Init(PalettePath, PaletteFileName, ".pal");
    MCFileSystem::CopyFile(fileName.FullName, backupName.FullName);

    MCFile file;
    const int32_t result = file.Create(fileName);

    if (result != 0)
    {
        return result;
    }

    file.WriteShort(0);
    file.WriteShort(0x100);
    file.Write(RgbData.get(), 0x300);
    file.Close();
    return 0;
}

int32_t MCPalette::LoadBWPalette(MCFitIniFile& paletteFile)
{
    int32_t result = paletteFile.ReadIdString("BWPaletteFileName", BwPaletteFileName, 8);

    if (result != 0)
    {
        return result;
    }

    MCFullPathFileName fileName;
    fileName.Init(PalettePath, BwPaletteFileName, ".pal");
    MCFile file;
    result = file.Open(fileName, READ, 50);

    if (result != 0)
    {
        return result;
    }

    const uint32_t size = file.FileSize();
    BwPalette = std::make_unique<uint8_t[]>(size);

    file.Read(BwPalette.get(), static_cast<int32_t>(size));
    file.Close();
    return 0;
}

int32_t MCPalette::LoadBWPalette()
{
    MCFullPathFileName fileName;
    fileName.Init(PalettePath, BwPaletteFileName, ".pal");
    MCFile file;
    const int32_t result = file.Open(fileName, READ, 50);

    if (result != 0)
    {
        return result;
    }

    const uint32_t size = file.FileSize();

    if (BwPalette == nullptr)
    {
        BwPalette = std::make_unique<uint8_t[]>(size);
    }

    file.Read(BwPalette.get(), static_cast<int32_t>(size));
    file.Close();
    return 0;
}

int32_t MCPalette::SaveBWPalette()
{
    MCFullPathFileName backupName;
    backupName.Init(PalettePath, BwPaletteFileName, ".bak");
    MCFullPathFileName fileName;
    fileName.Init(PalettePath, BwPaletteFileName, ".pal");
    MCFileSystem::CopyFile(fileName.FullName, backupName.FullName);

    MCFile file;
    const int32_t result = file.Create(fileName);

    if (result != 0)
    {
        return result;
    }

    file.Write(BwPalette.get(), PALETTE_FILE_SIZE);
    file.Close();
    return 0;
}

int32_t MCPalette::LoadExtractPalette(MCFitIniFile& paletteFile)
{
    int32_t result = paletteFile.ReadIdString("ExPaletteFileName", ExPaletteFileName, 8);

    if (result != 0)
    {
        return result;
    }

    MCFullPathFileName fileName;
    fileName.Init(PalettePath, ExPaletteFileName, ".pal");
    MCFile file;
    result = file.Open(fileName, READ, 50);

    if (result != 0)
    {
        return result;
    }

    const uint32_t size = file.FileSize();
    ExtractPalettes = std::make_unique<uint8_t[]>(size);

    NumExtractPalettes = static_cast<int32_t>(size / PALETTE_FILE_SIZE);
    file.Read(ExtractPalettes.get(), static_cast<int32_t>(size));
    file.Close();
    return 0;
}

int32_t MCPalette::LoadExtractPalette()
{
    MCFullPathFileName fileName;
    fileName.Init(PalettePath, ExPaletteFileName, ".pal");
    MCFile file;
    const int32_t result = file.Open(fileName, READ, 50);

    if (result != 0)
    {
        return result;
    }

    const uint32_t size = file.FileSize();

    if (ExtractPalettes == nullptr)
    {
        ExtractPalettes = std::make_unique<uint8_t[]>(size);
    }

    NumExtractPalettes = static_cast<int32_t>(size / PALETTE_FILE_SIZE);
    file.Read(ExtractPalettes.get(), static_cast<int32_t>(size));
    file.Close();
    return 0;
}

int32_t MCPalette::SaveExtractPalette()
{
    MCFullPathFileName backupName;
    backupName.Init(PalettePath, ExPaletteFileName, ".bak");
    MCFullPathFileName fileName;
    fileName.Init(PalettePath, ExPaletteFileName, ".pal");
    MCFileSystem::CopyFile(fileName.FullName, backupName.FullName);

    MCFile file;
    const int32_t result = file.Create(fileName);

    if (result != 0)
    {
        return result;
    }

    file.Write(ExtractPalettes.get(), NumExtractPalettes * PALETTE_FILE_SIZE);
    file.Close();
    return 0;
}

int32_t MCPalette::LoadDepthHazeTables(MCFile& tableFile)
{
    if (tableFile.FileSize() != static_cast<uint32_t>(Shl(NumDepthAtHazeLevelTables, NumDepthHazeEntriesShift) * 4))
    {
        return static_cast<int32_t>(0xabda0004);
    }

    const uint32_t size = tableFile.FileSize();
    DepthHazeTables = std::make_unique<int32_t[]>(size / sizeof(int32_t));
    tableFile.Read(reinterpret_cast<uint8_t*>(DepthHazeTables.get()), static_cast<int32_t>(size));
    return 0;
}

int32_t MCPalette::LoadFadePalettes(MCFile& tableFile)
{
    const uint32_t size = tableFile.FileSize();
    NumFadePalettes = size;
    FadePalettes = std::make_unique<uint8_t[]>(size);
    tableFile.Read(FadePalettes.get(), static_cast<int32_t>(size));
    MCRenderer::RegisterData(FadePalettes.get(), size, MCDataKind::Tables);
    NumFadePalettes = size >> 8;
    return 0;
}

int32_t MCPalette::LoadAllFadePalettes(MCFile& tableFile)
{
    const uint32_t size = tableFile.FileSize();
    NumAllFadePalettes = size;
    AllFadePalettes = std::make_unique<uint8_t[]>(size);
    tableFile.Read(AllFadePalettes.get(), static_cast<int32_t>(size));
    MCRenderer::RegisterData(AllFadePalettes.get(), size, MCDataKind::Tables);
    NumAllFadePalettes = size >> 8;
    return 0;
}

void MCPalette::AddFadePalette()
{
    ++NumFadePalettes;
    const uint32_t size = NumFadePalettes * 0x100;
    auto tables = std::make_unique<uint8_t[]>(size);
    // Original behaviour: the old tables are copied and the new last one is left as the heap gave it (the
    // declaration's "a copy of the last one" was never done).
    std::memcpy(tables.get(), FadePalettes.get(), size - 0x100);
    MCRenderer::UnregisterData(FadePalettes.get());
    FadePalettes = std::move(tables);
    MCRenderer::RegisterData(FadePalettes.get(), size, MCDataKind::Tables);
}

void MCPalette::RemoveFadePalette(int32_t index)
{
    auto tables = std::make_unique<uint8_t[]>((NumFadePalettes - 1) * 0x100);
    const uint8_t* old = FadePalettes.get();
    // Original behaviour: both copies start at the beginning of the old tables, so the tables before the removed one
    // are overwritten by the first ones again and the tables after it are lost (only the first count - 1 survive).
    std::memcpy(tables.get(), old, static_cast<size_t>(static_cast<uint32_t>(index) & 0xffffff) << 8);
    std::memcpy(tables.get(), old,
                static_cast<size_t>((NumFadePalettes - static_cast<uint32_t>(index) - 1) & 0xffffff) << 8);
    --NumFadePalettes;
    MCRenderer::UnregisterData(old);
    FadePalettes = std::move(tables);
    MCRenderer::RegisterData(FadePalettes.get(), static_cast<size_t>(NumFadePalettes) * 0x100, MCDataKind::Tables);
}

int32_t MCPalette::SaveFadePalettes()
{
    MCFullPathFileName fileName;
    fileName.Init(PalettePath, FadeTableFile, ".tbl");
    MCFile file;
    const int32_t result = file.Create(fileName);

    if (result != 0)
    {
        return result;
    }

    file.Write(FadePalettes.get(), static_cast<int32_t>(NumFadePalettes << 8));
    file.Close();
    return 0;
}

void MCPalette::AddExtractPalette()
{
    ++NumExtractPalettes;
    const uint32_t size = static_cast<uint32_t>(NumExtractPalettes * PALETTE_FILE_SIZE);
    auto palettes = std::make_unique<uint8_t[]>(size);
    std::memcpy(palettes.get(), ExtractPalettes.get(), size - PALETTE_FILE_SIZE);
    ExtractPalettes = std::move(palettes);
}

void MCPalette::RemoveExtractPalette(int32_t index)
{
    const int32_t count = NumExtractPalettes;
    auto palettes = std::make_unique<uint8_t[]>(static_cast<size_t>(count * PALETTE_FILE_SIZE - PALETTE_FILE_SIZE));
    const uint8_t* old = ExtractPalettes.get();
    // Original behaviour: as removeFadePalette, both copies start at the beginning of the old palettes.
    std::memcpy(palettes.get(), old, static_cast<size_t>(static_cast<uint32_t>(index * 0xc1) & 0x3fffffff) * 4);
    std::memcpy(palettes.get(), old,
                static_cast<size_t>(static_cast<uint32_t>((count - index - 1) * 0xc1) & 0x3fffffff) * 4);
    NumExtractPalettes = count - 1;
    ExtractPalettes = std::move(palettes);
}

void MCPalette::CopyNormalToExtractPalette(int32_t index)
{
    if (index < NumExtractPalettes && index > -1)
    {
        std::memcpy(ExtractPalettes.get() + index * PALETTE_FILE_SIZE, OriginalPalette.get(), PALETTE_FILE_SIZE);
    }
}

void CycleColors()
{
    // Set on the first call; the original kept a start time it never read.
    static uint32_t lastCycleTime = MCPort::Milliseconds();

    if (Scenario == nullptr)
    {
        return;
    }

    if (Scenario->CycleLength * 1000.0f < static_cast<float>(MCPort::Milliseconds() - lastCycleTime))
    {
        lastCycleTime = MCPort::Milliseconds();

        if (Application->PaletteCycle != 0)
        {
            uint32_t magic = CurrentMagic;

            for (int32_t i = 0; i < 8; ++i)
            {
                uint8_t color[3];
                std::memcpy(color, GamePalette->RgbData.get() + WaterMagicColors[magic] * 3, 3);
                GamePalette->TweakPalette(i + 0xd8, 1, reinterpret_cast<MCVfxRgb*>(color));
                ++magic;

                if (static_cast<int32_t>(magic) > 7)
                {
                    magic = 0;
                }
            }

            // The original handed the eight entries to the display (gamePalette->animate(0xd8, 8)). The display shows
            // them as an index remap instead, so the palette the GPU holds doesn't change every cycle; the next palette
            // set over them ends the remap, as it overwrote the animated entries.
            if (MCDisplay* display = MCInput::Display())
            {
                MCColorCycle cycle;
                cycle.First = 0xd8;
                std::copy_n(WaterMagicColors, 8, cycle.Sources.begin());
                cycle.Step = CurrentMagic;
                display->SetColorCycle(cycle);
            }

            ++CurrentMagic;

            if (CurrentMagic > 7)
            {
                CurrentMagic = 0;
            }
        }
    }
}
