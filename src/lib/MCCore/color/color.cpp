#include "stdafx.h"
#include "color/color.h"
#include "gui/asystem.h"
#include "lib/cident.h"
#include "lib/file.h"
#include "lib/heap.h"
#include "lib/inifile.h"
#include "mission/scenario.h"
#include "platform/MCDisplay.h"
#include "platform/MCFileSystem.h"
#include "platform/MCRenderer.h"
#include "platform/MCInput.h"

Palette* gamePalette = nullptr;
uint8_t WaterMagicColors[8] = {0x5a, 0x59, 0x5a, 0x5b, 0x5d, 0x5c, 0x5b, 0x59};
uint8_t currentMagic = 0;
char palettePath[80] = {};

int32_t Palette::lastMinDepth = -1;
int32_t Palette::lastMaxDepth = -1;
int32_t Palette::lastHazePercent = 0;

namespace
{
    /// <summary>Whether systemHeap is up (the palette's allocations test it first).</summary>
    bool SystemHeapReady()
    {
        return systemHeap != nullptr && systemHeap->heapSize != 0;
    }

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

void* PaletteBlock::operator new(size_t size) noexcept
{
    if (!SystemHeapReady())
    {
        return nullptr;
    }

    return systemHeap->malloc(static_cast<uint32_t>(size));
}

void PaletteBlock::operator delete(void* block)
{
    if (SystemHeapReady())
    {
        systemHeap->free(block);
    }
}

void PaletteBlock::initRgbData(uint8_t* data)
{
    const int16_t count = *reinterpret_cast<int16_t*>(data + 2);
    firstColor = *reinterpret_cast<int16_t*>(data);
    numColors = count;
    const uint32_t size = static_cast<uint32_t>(count * 3);

    if (rgbData == nullptr && SystemHeapReady())
    {
        rgbData = static_cast<uint8_t*>(systemHeap->malloc(size));
    }

    if (rgbData != nullptr)
    {
        std::memcpy(rgbData, data + 4, size);
    }
}

void PaletteBlock::destroy()
{
    if (rgbData != nullptr)
    {
        if (SystemHeapReady())
        {
            systemHeap->free(rgbData);
        }

        rgbData = nullptr;
    }
}

void* ColorRange::operator new(size_t size) noexcept
{
    return systemHeap->malloc(static_cast<uint32_t>(size));
}

void ColorRange::operator delete(void* block)
{
    systemHeap->free(block);
}

ColorRange::ColorRange(ColorRangeData& data, Palette* _palette)
{
    baseColorIndex = data.baseColorIndex;
    maxLitColor = data.maxLitColor;
    maxHazedColor = data.maxHazedColor;
    baseRed = data.baseRed;
    baseGreen = data.baseGreen;
    baseBlue = data.baseBlue;
    lightSourceFlag = data.lightSourceFlag;
    depthCueFlag = data.depthCueFlag;
    calcLightSource = data.calcLightSource;
    calcDepthCue = data.calcDepthCue;
    palette = _palette;
}

ColorRange::ColorRange(int32_t _baseColorIndex, uint8_t _baseRed, uint8_t _baseGreen, uint8_t _baseBlue,
                       int32_t _maxLitColor, int32_t _maxHazedColor, Palette* _palette, int _lightSourceFlag,
                       int _depthCueFlag, int _calcLightSource, int _calcDepthCue)
{
    baseColorIndex = _baseColorIndex;
    baseRed = _baseRed;
    baseGreen = _baseGreen;
    baseBlue = _baseBlue;
    maxLitColor = _maxLitColor;
    maxHazedColor = _maxHazedColor;
    palette = _palette;
    lightSourceFlag = _lightSourceFlag;
    depthCueFlag = _depthCueFlag;
    calcLightSource = _calcLightSource;
    calcDepthCue = _calcDepthCue;
}

uint8_t* Palette::depthHazedShadePalette(int32_t shade, float depth)
{
    int32_t level = (((numBitmapHazeLevels + 1) * shade + 0x80) >> 8) - 1;
    const int32_t hazePercent = hazePercentAtDepth(depth);

    if (level < 0)
    {
        level = 0;
    }

    if (hazePercent == 0)
    {
        return getHazePalette(level - numBitmapHazeLevels);
    }

    return getHazePalette(
        (Sar((numBitmapHazeLevels * 2 - level) * hazePercent + hazeRound, hazeShift) - numBitmapHazeLevels) + level);
}

int32_t Palette::depthHazedShade(int32_t colorRange, int32_t shade, float depth, int32_t maxShade)
{
    if (colorRange < 0 || numColorRanges <= colorRange)
    {
        return 0;
    }

    ColorRange& range = colorRanges[colorRange];

    if (maxShade == 0)
    {
        maxShade = range.maxLitColor;
    }

    const int32_t hazePercent = hazePercentAtDepth(depth);
    const int32_t litShade = (shade * maxShade + 0x80) >> 8;

    if (hazePercent == 0)
    {
        return range.baseColorIndex + litShade;
    }

    return (range.baseColorIndex - Sar(hazePercent * litShade + range.palette->hazeRound, range.palette->hazeShift)) +
           litShade;
}

uint8_t* Palette::getHazePalette(int32_t hazeLevel)
{
    if (hazeLevel == 0)
    {
        return nullptr;
    }

    if (hazeLevel > 0)
    {
        if (numBitmapHazeLevels < hazeLevel)
        {
            hazeLevel = numBitmapHazeLevels;
        }

        return fadePalettes + (hazeLevel * 0x100 - 0x100);
    }

    int32_t level = static_cast<int32_t>(0u - static_cast<uint32_t>(hazeLevel));

    if (numBitmapHazeLevels < level)
    {
        level = numBitmapHazeLevels;
    }

    return fadePalettes + (level * 0x100 - 0x100 + hazePaletteOffset);
}

int32_t Palette::findColorRange(int32_t colorIndex)
{
    int32_t found = -1;

    for (int32_t i = 0; i < numColorRanges; ++i)
    {
        const ColorRange& range = colorRanges[i];

        if (range.baseColorIndex <= colorIndex && colorIndex <= range.maxHazedColor + range.baseColorIndex)
        {
            found = i;
            break;
        }
    }

    return found;
}

int32_t Palette::findLightToDarkColorRange(int32_t colorIndex)
{
    int32_t found = -1;

    for (int32_t i = 0; i < numColorRanges; ++i)
    {
        const ColorRange& range = colorRanges[i];

        if (range.baseColorIndex <= colorIndex && colorIndex <= range.maxHazedColor + range.baseColorIndex &&
            range.lightSourceFlag != 0 && range.depthCueFlag != 0)
        {
            found = i;
            break;
        }
    }

    return found;
}

int32_t Palette::hazePercentAtDepth(float depth)
{
    const int32_t iDepth = static_cast<int32_t>(std::floor(static_cast<double>(depth)));

    if (lastMinDepth != -1 && lastMinDepth <= iDepth && (maxHazeDepth <= lastMinDepth || iDepth < lastMaxDepth))
    {
        return lastHazePercent;
    }

    if (iDepth < minHazeDepth)
    {
        lastMaxDepth = minHazeDepth;
        lastMinDepth = 0;
        lastHazePercent = 0;
        return 0;
    }

    lastMinDepth = maxHazeDepth;

    if (lastMinDepth <= iDepth)
    {
        lastMaxDepth = lastMinDepth;
        lastHazePercent = maxHazePercent;
        return maxHazePercent;
    }

    // A binary search of the current table for the span holding the depth.
    const int32_t* table = currentDepthTable;
    int32_t index = (numDepthHazeEntries >> 1) - 1;
    int32_t step = numDepthHazeEntriesShift - 1;

    while (true)
    {
        lastMinDepth = table[index];
        lastMaxDepth = table[index + 1];
        --step;

        if (lastMinDepth <= iDepth && iDepth < lastMaxDepth)
        {
            break;
        }

        if (iDepth == lastMaxDepth)
        {
            ++index;
            lastMinDepth = lastMaxDepth;
            lastMaxDepth = table[index + 1];
            break;
        }

        if (iDepth < lastMinDepth)
        {
            index += Shl(-1, step);
        }
        else
        {
            index += Shl(1, step);
        }
    }

    lastHazePercent = Shl(index + 1, hazeShift - numDepthHazeEntriesShift);
    return lastHazePercent;
}

void Palette::fullCycleOn()
{
}

void Palette::fullCycleOff()
{
    initRgbData(originalPalette);
    activate(0, 0);
}

void Palette::fadeToPalette(float& fadePercent, uint8_t* targetPalette)
{
    if (fadePercent > 1.0f)
    {
        fadePercent = 1.0f;
    }

    uint8_t* shown = rgbData;
    const uint8_t* target = targetPalette + 4;

    if (fadeDeltasValid == 0)
    {
        const uint8_t* original = originalPalette + 4;
        const int32_t count = static_cast<int32_t>(paletteSize) - 4;
        maxFadeDelta = 0;

        for (int32_t i = 0; i < count; ++i)
        {
            const uint8_t delta = static_cast<uint8_t>(target[i] - original[i]);
            fadeDeltas[i] = static_cast<int8_t>(delta);

            if (maxFadeDelta < ByteAbs(delta))
            {
                maxFadeDelta = ByteAbs(delta);
            }
        }

        fadeDeltasValid = 1;
    }

    if (fadePercent > 0.0f)
    {
        const int8_t step = FadeStep(maxFadeDelta, fadePercent);

        // Original behaviour: colour c takes fadeDeltas[c] (the delta of byte c, not of its own channels) for all
        // three channels, and colour 255 is never faded.
        for (int32_t color = 0; color < 0xff; ++color)
        {
            const int8_t delta = fadeDeltas[color];
            uint8_t* entry = shown;

            for (int32_t channel = 0; channel < 3; ++channel)
            {
                *shown++ = static_cast<uint8_t>(*target++ - FadeRemainder(delta, step));
            }

            gamePalette->tweakPalette(color, 1, reinterpret_cast<VFX_RGB*>(entry));
        }

        activate(0, 0);
    }
}

void Palette::fadeToColor(float& fadePercent, char red, char green, char blue)
{
    if (fadePercent > 1.0f)
    {
        fadePercent = 1.0f;
    }

    // Original behaviour: a new colour restarts the fade only when all three components differ.
    if (fadeTarget != 2 || (red != static_cast<char>(fadeRed) && green != static_cast<char>(fadeGreen) &&
                            blue != static_cast<char>(fadeBlue)))
    {
        fadeRed = static_cast<uint8_t>(red);
        fadeTarget = 2;
        fadeDeltasValid = 0;
        fadeGreen = static_cast<uint8_t>(green);
        fadeBlue = static_cast<uint8_t>(blue);
    }

    const uint8_t fadeColor[3] = {fadeRed, fadeGreen, fadeBlue};

    if (fadeDeltasValid == 0)
    {
        maxFadeDelta = 0;

        for (int32_t i = 0; i < 0x100 * 3; ++i)
        {
            const uint8_t delta = static_cast<uint8_t>(fadeColor[i % 3] - rgbData[i]);
            fadeDeltas[i] = static_cast<int8_t>(delta);

            if (maxFadeDelta < ByteAbs(delta))
            {
                maxFadeDelta = ByteAbs(delta);
            }
        }

        fadeDeltasValid = 1;
    }

    if (fadePercent > 0.0f)
    {
        const int8_t step = FadeStep(maxFadeDelta, fadePercent);

        for (int32_t i = 0; i < 0x100 * 3; ++i)
        {
            rgbData[i] = static_cast<uint8_t>(fadeColor[i % 3] - FadeRemainder(fadeDeltas[i], step));
        }

        activate(0, 0);
    }
}

void Palette::fadeToOriginalPalette(float& fadePercent)
{
    if (fadeTarget != 0)
    {
        fadeTarget = 0;
        fadeDeltasValid = 0;
    }

    fadeToPalette(fadePercent, originalPalette);
}

void Palette::fadeToBlackAndWhite(float& fadePercent)
{
    if (fadeTarget != 1)
    {
        fadeTarget = 1;
        fadeDeltasValid = 0;
    }

    fadeToPalette(fadePercent, bwPalette);
}

void Palette::recalculateDepthVsHazeInfo(int32_t altitude)
{
    int32_t table;

    if (altitude < maxAltitude)
    {
        table = Sar(altitude, altitudeShift);
    }
    else
    {
        table = numDepthAtHazeLevelTables - 1;
    }

    currentDepthTable = depthHazeTables + Shl(table, numDepthHazeEntriesShift);
    minHazeDepth = currentDepthTable[0];
    lastMinDepth = -1;
    maxHazeDepth = currentDepthTable[numDepthHazeEntries - 1];
}

void Palette::animate(int start, int count)
{
    application->activatePalette(rgbData, start, count);
}

void Palette::activate(int32_t which, int32_t extractIndex)
{
    if (which == 1)
    {
        uint8_t* colors = nullptr;

        if (extractIndex < numExtractPalettes && extractIndex >= 0)
        {
            colors = extractPalettes + 4 + extractIndex * PALETTE_FILE_SIZE;
        }

        application->activatePalette(colors, 0, 0x100);
        return;
    }

    if (which != 2)
    {
        application->activatePalette(rgbData, 0, 0x100);
        return;
    }

    // Original behaviour: the black-and-white palette is handed over with its 4-byte .pal header.
    application->activatePalette(bwPalette, 0, 0x100);
}

void Palette::tweakPalette(int start, int count, VFX_RGB* colors)
{
    const uint8_t* source = reinterpret_cast<const uint8_t*>(colors);

    for (int index = start; index < start + count; ++index)
    {
        std::memcpy(gamePalette->rgbData + (index & 0xff) * 3, source, 3);
        source += 3;
    }
}

void Palette::init()
{
    originalPalette = nullptr;
    fadeDeltas = nullptr;
    bwPalette = nullptr;
    extractPalettes = nullptr;
    fadePalettes = nullptr;
    depthHazeTables = nullptr;
    colorRanges = nullptr;
    rgbData = nullptr;
}

int32_t Palette::init(char* paletteFileName)
{
    FullPathFileName fileName;
    fileName.init(palettePath, paletteFileName, ".fit");
    FitIniFile paletteFile;
    int32_t result = paletteFile.open(fileName, READ, 50);

    if (result == 0 && (result = paletteFile.seekBlock("Palette")) == 0)
    {
        result = init(paletteFile);

        if (result == 0)
        {
            paletteFile.close();
            return 0;
        }
    }

    return result;
}

int32_t Palette::init(FitIniFile& paletteFile)
{
    int32_t result = loadPaletteInfo(paletteFile);

    if (result != 0)
    {
        return result;
    }

    result = paletteFile.seekBlock("Tables");

    if (result != 0)
    {
        return result;
    }

    result = paletteFile.readIdString("FadeTableFile", fadeTableFile, 8);

    if (result != 0)
    {
        return result;
    }

    result = paletteFile.readIdString("DepthTableFile", depthTableFile, 8);

    if (result != 0)
    {
        return result;
    }

    result = paletteFile.readIdString("AllFadeTableFile", allFadeTableFile, 8);

    if (result != 0)
    {
        return result;
    }

    result = loadTables();

    if (result != 0)
    {
        return result;
    }

    recalculateDepthVsHazeInfo(0);
    return 0;
}

void Palette::destroy()
{
    PaletteBlock::destroy();

    if (originalPalette != nullptr)
    {
        systemHeap->free(originalPalette);
        originalPalette = nullptr;
    }

    if (fadeDeltas != nullptr)
    {
        systemHeap->free(fadeDeltas);
        fadeDeltas = nullptr;
    }

    if (bwPalette != nullptr)
    {
        systemHeap->free(bwPalette);
        bwPalette = nullptr;
    }

    if (fadePalettes != nullptr)
    {
        systemHeap->free(fadePalettes);
        fadePalettes = nullptr;
    }

    if (allFadePalettes != nullptr)
    {
        systemHeap->free(allFadePalettes);
        allFadePalettes = nullptr;
    }

    if (extractPalettes != nullptr)
    {
        systemHeap->free(extractPalettes);
        extractPalettes = nullptr;
    }

    systemHeap->free(depthHazeTables);
    depthHazeTables = nullptr;
    ::operator delete(colorRanges);
    colorRanges = nullptr;
}

int32_t Palette::loadPaletteInfo(FitIniFile& paletteFile)
{
    int32_t result = paletteFile.readIdLong("HazeShift", hazeShift);

    if (result != 0)
    {
        return result;
    }

    result = paletteFile.readIdLong("NumBitmapHazeLevels", numBitmapHazeLevels);

    if (result != 0)
    {
        return result;
    }

    result = paletteFile.readIdLong("NumDepthHazeEntriesShift", numDepthHazeEntriesShift);

    if (result != 0)
    {
        return result;
    }

    result = paletteFile.readIdLong("NumDepthAtHazeLevelTables", numDepthAtHazeLevelTables);

    if (result != 0)
    {
        return result;
    }

    result = paletteFile.readIdLong("AltitudeShift", altitudeShift);

    if (result != 0)
    {
        return result;
    }

    const int32_t fullHaze = Shl(1, hazeShift);
    unknown74 = 0;
    maxHazePercent = fullHaze;
    hazeRound = fullHaze >> 1;
    unknown78 = numBitmapHazeLevels;
    hazePaletteOffset = numBitmapHazeLevels << 8;
    numDepthHazeEntries = Shl(1, numDepthHazeEntriesShift);
    maxAltitude = Shl(numDepthAtHazeLevelTables, altitudeShift);

    result = loadPalette(paletteFile);

    if (result != 0)
    {
        return result;
    }

    loadBWPalette(paletteFile);
    loadExtractPalette(paletteFile);
    result = paletteFile.seekBlock("Ranges");

    if (result != 0)
    {
        return result;
    }

    return loadColorRanges(paletteFile);
}

int32_t Palette::loadColorRanges(FitIniFile& paletteFile)
{
    int32_t result = paletteFile.readIdLong("NumColorRanges", numColorRanges);

    if (result != 0)
    {
        return result;
    }

    const int32_t count = numColorRanges;
    // The original sized this with the global operator new (count * 0x24), which destroy frees with the global
    // operator delete.
    colorRanges = static_cast<ColorRange*>(::operator new(static_cast<size_t>(count) * sizeof(ColorRange)));

    for (int32_t i = 0; i < count; ++i)
    {
        char blockId[12];
        std::snprintf(blockId, sizeof(blockId), "Range%d", i);
        result = paletteFile.seekBlock(blockId);

        if (result != 0)
        {
            return result;
        }

        ColorRangeData data{};

        if ((result = paletteFile.readIdUChar("BaseColorIndex", data.baseColorIndex)) != 0)
        {
            return result;
        }

        if ((result = paletteFile.readIdUChar("MaxLitColor", data.maxLitColor)) != 0)
        {
            return result;
        }

        if ((result = paletteFile.readIdUChar("MaxHazedColor", data.maxHazedColor)) != 0)
        {
            return result;
        }

        if ((result = paletteFile.readIdUChar("BaseRed", data.baseRed)) != 0)
        {
            return result;
        }

        if ((result = paletteFile.readIdUChar("BaseGreen", data.baseGreen)) != 0)
        {
            return result;
        }

        if ((result = paletteFile.readIdUChar("BaseBlue", data.baseBlue)) != 0)
        {
            return result;
        }

        if ((result = paletteFile.readIdUChar("LightSourceFlag", data.lightSourceFlag)) != 0)
        {
            return result;
        }

        if ((result = paletteFile.readIdUChar("DepthCueFlag", data.depthCueFlag)) != 0)
        {
            return result;
        }

        if ((result = paletteFile.readIdUChar("CalcLightSource", data.calcLightSource)) != 0)
        {
            return result;
        }

        if ((result = paletteFile.readIdUChar("CalcDepthCue", data.calcDepthCue)) != 0)
        {
            return result;
        }

        ::new (&colorRanges[i]) ColorRange(data, this);
    }

    return 0;
}

int32_t Palette::loadTables()
{
    FullPathFileName depthName;
    depthName.init(palettePath, depthTableFile, ".tbl");
    FullPathFileName fadeName;
    fadeName.init(palettePath, fadeTableFile, ".tbl");
    FullPathFileName allFadeName;
    allFadeName.init(palettePath, allFadeTableFile, ".tbl");

    File depthFile;
    File fadeFile;
    File allFadeFile;
    int32_t result = depthFile.open(depthName, READ, 50);

    if (result != 0)
    {
        return result;
    }

    loadDepthHazeTables(depthFile);
    depthFile.close();
    result = fadeFile.open(fadeName, READ, 50);

    if (result != 0)
    {
        return result;
    }

    loadFadePalettes(fadeFile);
    fadeFile.close();
    result = allFadeFile.open(allFadeName, READ, 50);

    if (result != 0)
    {
        return result;
    }

    loadAllFadePalettes(allFadeFile);
    allFadeFile.close();
    return 0;
}

int32_t Palette::loadPalette(FitIniFile& paletteFile)
{
    int32_t result = paletteFile.readIdString("PaletteFileName", paletteFileName, 8);

    if (result != 0)
    {
        return result;
    }

    FullPathFileName fileName;
    fileName.init(palettePath, paletteFileName, ".pal");
    File file;
    result = file.open(fileName, READ, 50);

    if (result != 0)
    {
        return result;
    }

    const uint32_t size = file.fileSize();
    originalPalette = static_cast<uint8_t*>(systemHeap->malloc(size));

    if (originalPalette == nullptr)
    {
        return static_cast<int32_t>(0xabda0001);
    }

    file.read(originalPalette, static_cast<int32_t>(size));
    initRgbData(originalPalette);
    fadeDeltasValid = 0;
    fadeTarget = -1;
    fadeBlue = 0;
    fadeGreen = 0;
    fadeRed = 0;
    paletteSize = size;
    fadeDeltas = static_cast<int8_t*>(systemHeap->malloc(size));

    if (fadeDeltas == nullptr)
    {
        return static_cast<int32_t>(0xabda0002);
    }

    file.close();
    return 0;
}

int32_t Palette::loadPalette()
{
    FullPathFileName fileName;
    fileName.init(palettePath, paletteFileName, ".pal");
    File file;
    int32_t result = file.open(fileName, READ, 50);

    if (result != 0)
    {
        return result;
    }

    const uint32_t size = file.fileSize();

    if (originalPalette == nullptr)
    {
        originalPalette = static_cast<uint8_t*>(systemHeap->malloc(size));

        if (originalPalette == nullptr)
        {
            return static_cast<int32_t>(0xabda0001);
        }
    }

    file.read(originalPalette, static_cast<int32_t>(size));
    initRgbData(originalPalette);
    fadeDeltasValid = 0;
    fadeTarget = -1;
    fadeBlue = 0;
    fadeGreen = 0;
    fadeRed = 0;
    paletteSize = size;

    if (fadeDeltas == nullptr)
    {
        fadeDeltas = static_cast<int8_t*>(systemHeap->malloc(size));

        if (fadeDeltas == nullptr)
        {
            return static_cast<int32_t>(0xabda0002);
        }
    }

    file.close();
    return 0;
}

int32_t Palette::savePalette()
{
    FullPathFileName backupName;
    backupName.init(palettePath, paletteFileName, ".bak");
    FullPathFileName fileName;
    fileName.init(palettePath, paletteFileName, ".pal");
    MCFileSystem::CopyFile(fileName.fullName, backupName.fullName);

    File file;
    const int32_t result = file.create(fileName);

    if (result != 0)
    {
        return result;
    }

    file.writeShort(0);
    file.writeShort(0x100);
    file.write(rgbData, 0x300);
    file.close();
    return 0;
}

int32_t Palette::loadBWPalette(FitIniFile& paletteFile)
{
    int32_t result = paletteFile.readIdString("BWPaletteFileName", bwPaletteFileName, 8);

    if (result != 0)
    {
        return result;
    }

    FullPathFileName fileName;
    fileName.init(palettePath, bwPaletteFileName, ".pal");
    File file;
    result = file.open(fileName, READ, 50);

    if (result != 0)
    {
        return result;
    }

    const uint32_t size = file.fileSize();
    bwPalette = static_cast<uint8_t*>(systemHeap->malloc(size));

    if (bwPalette == nullptr)
    {
        return static_cast<int32_t>(0xabda0003);
    }

    file.read(bwPalette, static_cast<int32_t>(size));
    file.close();
    return 0;
}

int32_t Palette::loadBWPalette()
{
    FullPathFileName fileName;
    fileName.init(palettePath, bwPaletteFileName, ".pal");
    File file;
    const int32_t result = file.open(fileName, READ, 50);

    if (result != 0)
    {
        return result;
    }

    const uint32_t size = file.fileSize();

    if (bwPalette == nullptr)
    {
        bwPalette = static_cast<uint8_t*>(systemHeap->malloc(size));

        if (bwPalette == nullptr)
        {
            return static_cast<int32_t>(0xabda0003);
        }
    }

    file.read(bwPalette, static_cast<int32_t>(size));
    file.close();
    return 0;
}

int32_t Palette::saveBWPalette()
{
    FullPathFileName backupName;
    backupName.init(palettePath, bwPaletteFileName, ".bak");
    FullPathFileName fileName;
    fileName.init(palettePath, bwPaletteFileName, ".pal");
    MCFileSystem::CopyFile(fileName.fullName, backupName.fullName);

    File file;
    const int32_t result = file.create(fileName);

    if (result != 0)
    {
        return result;
    }

    file.write(bwPalette, PALETTE_FILE_SIZE);
    file.close();
    return 0;
}

int32_t Palette::loadExtractPalette(FitIniFile& paletteFile)
{
    int32_t result = paletteFile.readIdString("ExPaletteFileName", exPaletteFileName, 8);

    if (result != 0)
    {
        return result;
    }

    FullPathFileName fileName;
    fileName.init(palettePath, exPaletteFileName, ".pal");
    File file;
    result = file.open(fileName, READ, 50);

    if (result != 0)
    {
        return result;
    }

    const uint32_t size = file.fileSize();
    extractPalettes = static_cast<uint8_t*>(systemHeap->malloc(size));

    if (extractPalettes == nullptr)
    {
        return static_cast<int32_t>(0xabda0007);
    }

    numExtractPalettes = static_cast<int32_t>(size / PALETTE_FILE_SIZE);
    file.read(extractPalettes, static_cast<int32_t>(size));
    file.close();
    return 0;
}

int32_t Palette::loadExtractPalette()
{
    FullPathFileName fileName;
    fileName.init(palettePath, exPaletteFileName, ".pal");
    File file;
    const int32_t result = file.open(fileName, READ, 50);

    if (result != 0)
    {
        return result;
    }

    const uint32_t size = file.fileSize();

    if (extractPalettes == nullptr)
    {
        extractPalettes = static_cast<uint8_t*>(systemHeap->malloc(size));

        if (extractPalettes == nullptr)
        {
            return static_cast<int32_t>(0xabda0007);
        }
    }

    numExtractPalettes = static_cast<int32_t>(size / PALETTE_FILE_SIZE);
    file.read(extractPalettes, static_cast<int32_t>(size));
    file.close();
    return 0;
}

int32_t Palette::saveExtractPalette()
{
    FullPathFileName backupName;
    backupName.init(palettePath, exPaletteFileName, ".bak");
    FullPathFileName fileName;
    fileName.init(palettePath, exPaletteFileName, ".pal");
    MCFileSystem::CopyFile(fileName.fullName, backupName.fullName);

    File file;
    const int32_t result = file.create(fileName);

    if (result != 0)
    {
        return result;
    }

    file.write(extractPalettes, numExtractPalettes * PALETTE_FILE_SIZE);
    file.close();
    return 0;
}

int32_t Palette::loadDepthHazeTables(File& tableFile)
{
    if (tableFile.fileSize() != static_cast<uint32_t>(Shl(numDepthAtHazeLevelTables, numDepthHazeEntriesShift) * 4))
    {
        return static_cast<int32_t>(0xabda0004);
    }

    const uint32_t size = tableFile.fileSize();
    uint8_t* tables = static_cast<uint8_t*>(systemHeap->malloc(size));
    depthHazeTables = reinterpret_cast<int32_t*>(tables);

    if (tables == nullptr)
    {
        return static_cast<int32_t>(0xabda0005);
    }

    tableFile.read(tables, static_cast<int32_t>(tableFile.fileSize()));
    return 0;
}

int32_t Palette::loadFadePalettes(File& tableFile)
{
    const uint32_t size = tableFile.fileSize();
    numFadePalettes = size;
    fadePalettes = static_cast<uint8_t*>(systemHeap->malloc(size));

    if (fadePalettes == nullptr)
    {
        return static_cast<int32_t>(0xabda0006);
    }

    tableFile.read(fadePalettes, static_cast<int32_t>(size));
    MCRenderer::RegisterData(fadePalettes, size, MCDataKind::Tables);
    numFadePalettes = size >> 8;
    return 0;
}

int32_t Palette::loadAllFadePalettes(File& tableFile)
{
    const uint32_t size = tableFile.fileSize();
    numAllFadePalettes = size;
    allFadePalettes = static_cast<uint8_t*>(systemHeap->malloc(size));

    if (allFadePalettes == nullptr)
    {
        return static_cast<int32_t>(0xabda0006);
    }

    tableFile.read(allFadePalettes, static_cast<int32_t>(size));
    MCRenderer::RegisterData(allFadePalettes, size, MCDataKind::Tables);
    numAllFadePalettes = size >> 8;
    return 0;
}

void Palette::addFadePalette()
{
    ++numFadePalettes;
    const uint32_t size = numFadePalettes * 0x100;
    uint8_t* tables = static_cast<uint8_t*>(systemHeap->malloc(size));
    uint8_t* old = fadePalettes;
    // Original behaviour: the old tables are copied and the new last one is left as the heap gave it (the
    // declaration's "a copy of the last one" was never done).
    std::memcpy(tables, old, size - 0x100);
    systemHeap->free(old);
    fadePalettes = tables;
    MCRenderer::RegisterData(fadePalettes, size, MCDataKind::Tables);
}

void Palette::removeFadePalette(int32_t index)
{
    uint8_t* tables = static_cast<uint8_t*>(systemHeap->malloc((numFadePalettes - 1) * 0x100));
    uint8_t* old = fadePalettes;
    // Original behaviour: both copies start at the beginning of the old tables, so the tables before the removed one
    // are overwritten by the first ones again and the tables after it are lost (only the first count - 1 survive).
    std::memcpy(tables, old, static_cast<size_t>(static_cast<uint32_t>(index) & 0xffffff) << 8);
    std::memcpy(tables, old, static_cast<size_t>((numFadePalettes - static_cast<uint32_t>(index) - 1) & 0xffffff) << 8);
    --numFadePalettes;
    systemHeap->free(old);
    fadePalettes = tables;
    MCRenderer::RegisterData(fadePalettes, static_cast<size_t>(numFadePalettes) * 0x100, MCDataKind::Tables);
}

int32_t Palette::saveFadePalettes()
{
    FullPathFileName fileName;
    fileName.init(palettePath, fadeTableFile, ".tbl");
    File file;
    const int32_t result = file.create(fileName);

    if (result != 0)
    {
        return result;
    }

    file.write(fadePalettes, static_cast<int32_t>(numFadePalettes << 8));
    file.close();
    return 0;
}

void Palette::addExtractPalette()
{
    ++numExtractPalettes;
    const uint32_t size = static_cast<uint32_t>(numExtractPalettes * PALETTE_FILE_SIZE);
    uint8_t* palettes = static_cast<uint8_t*>(systemHeap->malloc(size));
    uint8_t* old = extractPalettes;
    std::memcpy(palettes, old, size - PALETTE_FILE_SIZE);
    systemHeap->free(old);
    extractPalettes = palettes;
}

void Palette::removeExtractPalette(int32_t index)
{
    const int32_t count = numExtractPalettes;
    uint8_t* palettes =
        static_cast<uint8_t*>(systemHeap->malloc(static_cast<uint32_t>(count * PALETTE_FILE_SIZE - PALETTE_FILE_SIZE)));
    uint8_t* old = extractPalettes;
    // Original behaviour: as removeFadePalette, both copies start at the beginning of the old palettes.
    std::memcpy(palettes, old, static_cast<size_t>(static_cast<uint32_t>(index * 0xc1) & 0x3fffffff) * 4);
    std::memcpy(palettes, old, static_cast<size_t>(static_cast<uint32_t>((count - index - 1) * 0xc1) & 0x3fffffff) * 4);
    numExtractPalettes = count - 1;
    systemHeap->free(old);
    extractPalettes = palettes;
}

void Palette::copyNormalToExtractPalette(int32_t index)
{
    if (index < numExtractPalettes && index > -1)
    {
        std::memcpy(extractPalettes + index * PALETTE_FILE_SIZE, originalPalette, PALETTE_FILE_SIZE);
    }
}

void cycleColors()
{
    // Set on the first call; the original kept a start time it never read.
    static uint32_t lastCycleTime = MCPort::Milliseconds();

    if (scenario == nullptr)
    {
        return;
    }

    if (scenario->cycleLength * 1000.0f < static_cast<float>(MCPort::Milliseconds() - lastCycleTime))
    {
        lastCycleTime = MCPort::Milliseconds();

        if (application->paletteCycle != 0)
        {
            uint32_t magic = currentMagic;

            for (int32_t i = 0; i < 8; ++i)
            {
                uint8_t color[3];
                std::memcpy(color, gamePalette->rgbData + WaterMagicColors[magic] * 3, 3);
                gamePalette->tweakPalette(i + 0xd8, 1, reinterpret_cast<VFX_RGB*>(color));
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
                cycle.Step = currentMagic;
                display->SetColorCycle(cycle);
            }

            ++currentMagic;

            if (currentMagic > 7)
            {
                currentMagic = 0;
            }
        }
    }
}
