#pragma once

class File;
class FitIniFile;
class Palette;

#pragma pack(push, 1)
/// <summary>
/// The header of a <c>.pal</c> file (0x304 bytes in all): the first colour and the number of colours, then
/// <c>numColors</c> RGB triples of 6-bit values.
/// </summary>
struct PaletteFileHeader
{
    uint16_t firstColor; // +0x00
    uint16_t numColors;  // +0x02
};
#pragma pack(pop)
static_assert(sizeof(PaletteFileHeader) == 4);

/// <summary>The size of a whole <c>.pal</c> file (and of each palette of an extract palette file).</summary>
inline constexpr int32_t PALETTE_FILE_SIZE = 0x304;

/// <summary>A block of palette colours: a range of indices and their RGB values.</summary>
/// <remarks>Original source: <c>color\color.cpp</c>, 8 bytes.</remarks>
class PaletteBlock
{
public:
    /// <summary>
    /// Takes the range and colours from a <c>.pal</c> image (<see cref="PaletteFileHeader"/> then the triples),
    /// allocating <see cref="rgbData"/> the first time.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006b2180</remarks>
    void initRgbData(uint8_t* data);

    /// <summary>Frees <see cref="rgbData"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006b21f0</remarks>
    void destroy();

    /// <summary>The number of colours.</summary>
    int16_t numColors = 0; // +0x00
    /// <summary>The first palette index.</summary>
    int16_t firstColor = 0; // +0x02
    /// <summary>The colours (6 bits per channel); for a <see cref="Palette"/>, the palette the game shows.</summary>
    std::unique_ptr<uint8_t[]> rgbData; // +0x04
};

#pragma pack(push, 1)
/// <summary>A colour range as the palette FIT's "Range%d" blocks give it (10 bytes, in reading order).</summary>
struct ColorRangeData
{
    uint8_t baseColorIndex;  // +0x00
    uint8_t maxLitColor;     // +0x01
    uint8_t maxHazedColor;   // +0x02
    uint8_t baseRed;         // +0x03
    uint8_t baseGreen;       // +0x04
    uint8_t baseBlue;        // +0x05
    uint8_t lightSourceFlag; // +0x06
    uint8_t depthCueFlag;    // +0x07
    uint8_t calcLightSource; // +0x08
    uint8_t calcDepthCue;    // +0x09
};
#pragma pack(pop)
static_assert(sizeof(ColorRangeData) == 10);

/// <summary>
/// A run of palette indices shading one base colour, from lit to hazed: the unit the depth-haze and lighting
/// lookups work on.
/// </summary>
/// <remarks>Original source: <c>color\color.cpp</c>, 0x24 bytes.</remarks>
class ColorRange
{
public:
    ColorRange() = default;

    /// <summary>A range from its FIT data, belonging to <paramref name="_palette"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006b2260</remarks>
    ColorRange(ColorRangeData& data, Palette* _palette);

    /// <remarks>MCX.EXE @ 0x006b22c0</remarks>
    ColorRange(int32_t _baseColorIndex, uint8_t _baseRed, uint8_t _baseGreen, uint8_t _baseBlue, int32_t _maxLitColor,
               int32_t _maxHazedColor, Palette* _palette, int _lightSourceFlag, int _depthCueFlag, int _calcLightSource,
               int _calcDepthCue);

    /// <summary>The palette the range belongs to.</summary>
    Palette* palette = nullptr; // +0x00
    /// <summary>Nonzero when the range is lit by the light source.</summary>
    int lightSourceFlag = 0; // +0x04
    /// <summary>Nonzero when the range is depth-cued (hazed).</summary>
    int depthCueFlag = 0;    // +0x08
    int calcLightSource = 0; // +0x0c
    int calcDepthCue = 0;    // +0x10
    /// <summary>The range's first palette index.</summary>
    int32_t baseColorIndex = 0; // +0x14
    uint8_t baseRed = 0;        // +0x18
    uint8_t baseGreen = 0;      // +0x19
    uint8_t baseBlue = 0;       // +0x1a
    /// <summary>The number of lit shades (the range spans baseColorIndex..baseColorIndex + maxHazedColor).</summary>
    int32_t maxLitColor = 0; // +0x1c
    /// <summary>The number of shades in the range.</summary>
    int32_t maxHazedColor = 0; // +0x20
};

/// <summary>
/// The game palette and its tables: the normal, black-and-white and "extract" palettes, fade tables (256-byte
/// index translations), the depth-haze tables and the colour ranges, all described by a palette FIT file.
/// </summary>
/// <remarks>
/// Original source: <c>color\color.cpp</c>, 0xb8 bytes. The <see cref="PaletteBlock"/> base holds the palette the
/// game shows; <see cref="originalPalette"/> the file as loaded, which fades return to.
/// </remarks>
class Palette : public PaletteBlock
{
public:
    /// <summary>The palette of haze level <paramref name="shade"/> (negative for the other half), null for 0.</summary>
    /// <remarks>MCX.EXE @ 0x006b2310</remarks>
    uint8_t* depthHazedShadePalette(int32_t shade, float depth);

    /// <summary>
    /// The palette index of colour range <paramref name="colorRange"/> at <paramref name="shade"/> (0..256) and
    /// <paramref name="depth"/>; <paramref name="maxShade"/> 0 uses the range's lit shades.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006b2380</remarks>
    int32_t depthHazedShade(int32_t colorRange, int32_t shade, float depth, int32_t maxShade);

    /// <summary>
    /// The fade table of haze level <paramref name="hazeLevel"/> (clamped; negative levels use the second set), null
    /// for 0.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006b2400</remarks>
    uint8_t* getHazePalette(int32_t hazeLevel);

    /// <summary>The colour range containing <paramref name="colorIndex"/>, or -1.</summary>
    /// <remarks>MCX.EXE @ 0x006b2460</remarks>
    int32_t findColorRange(int32_t colorIndex);

    /// <summary>The lit and depth-cued colour range containing <paramref name="colorIndex"/>, or -1.</summary>
    /// <remarks>MCX.EXE @ 0x006b24d0</remarks>
    int32_t findLightToDarkColorRange(int32_t colorIndex);

    /// <summary>The haze percentage (0..<see cref="maxHazePercent"/>) at <paramref name="depth"/>, cached per span.</summary>
    /// <remarks>MCX.EXE @ 0x006b2550</remarks>
    int32_t hazePercentAtDepth(float depth);

    /// <summary>Nothing (colour cycling is always on).</summary>
    /// <remarks>MCX.EXE @ 0x006b2670</remarks>
    void fullCycleOn();

    /// <summary>Restores the loaded palette and shows it.</summary>
    /// <remarks>MCX.EXE @ 0x006b2680</remarks>
    void fullCycleOff();

    /// <summary>
    /// Moves the shown palette toward <paramref name="targetPalette"/> (a <c>.pal</c> image) by
    /// <paramref name="fadePercent"/> (clamped to 1).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006b26a0</remarks>
    void fadeToPalette(float& fadePercent, uint8_t* targetPalette);

    /// <summary>Moves the shown palette toward one colour.</summary>
    /// <remarks>MCX.EXE @ 0x006b2830</remarks>
    void fadeToColor(float& fadePercent, char red, char green, char blue);

    /// <summary>Moves the shown palette back toward the loaded one.</summary>
    /// <remarks>MCX.EXE @ 0x006b29e0</remarks>
    void fadeToOriginalPalette(float& fadePercent);

    /// <summary>Moves the shown palette toward the black-and-white one.</summary>
    /// <remarks>MCX.EXE @ 0x006b2a10</remarks>
    void fadeToBlackAndWhite(float& fadePercent);

    /// <summary>Shows the palette through <c>aSystem::activatePalette</c> from <paramref name="start"/> for <paramref name="count"/> entries.</summary>
    /// <remarks>MCX.EXE @ 0x006b2aa0</remarks>
    void animate(int start, int count);

    /// <summary>
    /// Shows a palette: <paramref name="which"/> 1 is extract palette <paramref name="extractIndex"/>, 2 the
    /// black-and-white palette, anything else the normal one.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006b2ac0</remarks>
    void activate(int32_t which, int32_t extractIndex);

    /// <summary>
    /// Copies <paramref name="count"/> colours from <paramref name="colors"/> into the game palette from index
    /// <paramref name="start"/> (always <c>gamePalette</c>, whichever palette it's called on).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006b2b40</remarks>
    void tweakPalette(int start, int count, VFX_RGB* colors);

    /// <summary>Clears the pointers.</summary>
    /// <remarks>MCX.EXE @ 0x006b2b90</remarks>
    void init();

    /// <summary>Loads the palette FIT <paramref name="paletteFileName"/> from the palette path ("Palette" block).</summary>
    /// <remarks>MCX.EXE @ 0x006b2bc0</remarks>
    int32_t init(char* paletteFileName);

    /// <summary>Loads the palette from an open FIT: its info, palettes, colour ranges and "Tables".</summary>
    /// <remarks>MCX.EXE @ 0x006b2c70</remarks>
    int32_t init(FitIniFile& paletteFile);

    /// <summary>Frees everything.</summary>
    /// <remarks>MCX.EXE @ 0x006b2d00</remarks>
    void destroy();

    /// <summary>Loads the depth table, fade table and all-fade table files named by the FIT.</summary>
    /// <remarks>MCX.EXE @ 0x006b30b0</remarks>
    int32_t loadTables();

    /// <summary>Reloads the normal palette (<see cref="paletteFileName"/>).</summary>
    /// <remarks>MCX.EXE @ 0x006b3370</remarks>
    int32_t loadPalette();

    /// <summary>Saves the normal palette (keeping a <c>.bak</c>).</summary>
    /// <remarks>MCX.EXE @ 0x006b34a0</remarks>
    int32_t savePalette();

    /// <summary>Reloads the black-and-white palette.</summary>
    /// <remarks>MCX.EXE @ 0x006b3660</remarks>
    int32_t loadBWPalette();

    /// <summary>Saves the black-and-white palette (keeping a <c>.bak</c>).</summary>
    /// <remarks>MCX.EXE @ 0x006b3730</remarks>
    int32_t saveBWPalette();

    /// <summary>Reloads the extract palettes.</summary>
    /// <remarks>MCX.EXE @ 0x006b38f0</remarks>
    int32_t loadExtractPalette();

    /// <summary>Saves the extract palettes (keeping a <c>.bak</c>).</summary>
    /// <remarks>MCX.EXE @ 0x006b39d0</remarks>
    int32_t saveExtractPalette();

    /// <summary>Appends a fade table (a copy of the last one).</summary>
    /// <remarks>MCX.EXE @ 0x006b3be0</remarks>
    void addFadePalette();

    /// <summary>Removes fade table <paramref name="index"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006b3c50</remarks>
    void removeFadePalette(int32_t index);

    /// <summary>Saves the fade tables.</summary>
    /// <remarks>MCX.EXE @ 0x006b3cf0</remarks>
    int32_t saveFadePalettes();

    /// <summary>Appends an extract palette.</summary>
    /// <remarks>MCX.EXE @ 0x006b3d90</remarks>
    void addExtractPalette();

    /// <summary>Removes extract palette <paramref name="index"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006b3e00</remarks>
    void removeExtractPalette(int32_t index);

    /// <summary>Copies the loaded palette over extract palette <paramref name="index"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006b3ea0</remarks>
    void copyNormalToExtractPalette(int32_t index);

protected:
    /// <summary>Picks the depth-vs-haze table for <paramref name="altitude"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006b2a40</remarks>
    void recalculateDepthVsHazeInfo(int32_t altitude);

    /// <summary>Reads the haze settings, the three palettes and the "Ranges" block.</summary>
    /// <remarks>MCX.EXE @ 0x006b2dd0</remarks>
    int32_t loadPaletteInfo(FitIniFile& paletteFile);

    /// <summary>
    /// Reads "NumColorRanges" and each "Range%d" block into <see cref="colorRanges"/>.
    /// </summary>
    /// <remarks>
    /// MCX.EXE @ 0x006b2ee0 (a Palette method without a symbol; the name is the port's).
    /// </remarks>
    int32_t loadColorRanges(FitIniFile& paletteFile);

    /// <summary>Reads "PaletteFileName" and loads that <c>.pal</c>.</summary>
    /// <remarks>MCX.EXE @ 0x006b3240</remarks>
    int32_t loadPalette(FitIniFile& paletteFile);

    /// <summary>Reads "BWPaletteFileName" and loads that <c>.pal</c>.</summary>
    /// <remarks>MCX.EXE @ 0x006b3580</remarks>
    int32_t loadBWPalette(FitIniFile& paletteFile);

    /// <summary>Reads "ExPaletteFileName" and loads those palettes (0x304 bytes each).</summary>
    /// <remarks>MCX.EXE @ 0x006b3800</remarks>
    int32_t loadExtractPalette(FitIniFile& paletteFile);

    /// <summary>
    /// Reads the depth table file: NumDepthAtHazeLevelTables tables of 2^NumDepthHazeEntriesShift int32 depths
    /// (its size must match).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006b3ab0</remarks>
    int32_t loadDepthHazeTables(File& tableFile);

    /// <summary>Reads the fade table file (256-byte tables).</summary>
    /// <remarks>MCX.EXE @ 0x006b3b20</remarks>
    int32_t loadFadePalettes(File& tableFile);

    /// <summary>Reads the all-fade table file (256-byte tables).</summary>
    /// <remarks>MCX.EXE @ 0x006b3b80</remarks>
    int32_t loadAllFadePalettes(File& tableFile);

public:
    /// <summary>The loaded palette file's size.</summary>
    uint32_t paletteSize = 0; // +0x08
    /// <summary>The black-and-white palette (a <c>.pal</c> image).</summary>
    std::unique_ptr<uint8_t[]> bwPalette; // +0x0c
    /// <summary>Per byte of the palette, the signed distance to the fade target.</summary>
    std::unique_ptr<int8_t[]> fadeDeltas; // +0x10
    /// <summary>The extract palettes (0x304 bytes each).</summary>
    std::unique_ptr<uint8_t[]> extractPalettes; // +0x14
    /// <summary>The largest of <see cref="fadeDeltas"/>.</summary>
    int32_t maxFadeDelta = 0; // +0x18
    /// <summary>Nonzero once <see cref="fadeDeltas"/> is computed for the current target.</summary>
    int32_t fadeDeltasValid = 0; // +0x1c
    /// <summary>The fade target: -1 none, 0 the loaded palette, 1 black and white, 2 a colour.</summary>
    int32_t fadeTarget = -1; // +0x20
    /// <summary>The colour of the last <see cref="fadeToColor"/>.</summary>
    uint8_t fadeRed = 0;   // +0x24
    uint8_t fadeGreen = 0; // +0x25
    uint8_t fadeBlue = 0;  // +0x26
    /// <summary>The depth-vs-haze tables (depths at which each haze step starts).</summary>
    std::unique_ptr<int32_t[]> depthHazeTables; // +0x28
    /// <summary>FIT "NumDepthAtHazeLevelTables": one table per altitude step.</summary>
    int32_t numDepthAtHazeLevelTables = 0; // +0x2c
    /// <summary>The table for the camera's altitude.</summary>
    int32_t* currentDepthTable = nullptr; // +0x30
    /// <summary>Entries per table (1 &lt;&lt; <see cref="numDepthHazeEntriesShift"/>).</summary>
    int32_t numDepthHazeEntries = 0; // +0x34
    /// <summary>FIT "NumDepthHazeEntriesShift".</summary>
    int32_t numDepthHazeEntriesShift = 0; // +0x38
    /// <summary>The offset of the second set of haze fade tables (<see cref="numBitmapHazeLevels"/> * 256).</summary>
    int32_t hazePaletteOffset = 0; // +0x3c
    /// <summary>FIT "PaletteFileName".</summary>
    char paletteFileName[8] = {}; // +0x40
    /// <summary>FIT "ExPaletteFileName".</summary>
    char exPaletteFileName[8] = {}; // +0x48
    /// <summary>FIT "BWPaletteFileName".</summary>
    char bwPaletteFileName[8] = {}; // +0x50
    /// <summary>FIT "DepthTableFile".</summary>
    char depthTableFile[8] = {}; // +0x58
    /// <summary>FIT "FadeTableFile".</summary>
    char fadeTableFile[8] = {}; // +0x60
    /// <summary>FIT "AllFadeTableFile".</summary>
    char allFadeTableFile[8] = {}; // +0x68
    /// <summary>The loaded <c>.pal</c> image.</summary>
    std::unique_ptr<uint8_t[]> originalPalette; // +0x70
    /// <summary>FIT "NumBitmapHazeLevels": fade tables per haze set.</summary>
    int32_t numBitmapHazeLevels = 0; // +0x7c
    /// <summary>FIT "NumColorRanges".</summary>
    int32_t numColorRanges = 0; // +0x80
    /// <summary>The number of extract palettes.</summary>
    int32_t numExtractPalettes = 0; // +0x84
    /// <summary>The colour ranges.</summary>
    std::vector<ColorRange> colorRanges; // +0x88
    /// <summary>The number of all-fade tables.</summary>
    uint32_t numAllFadePalettes = 0; // +0x8c
    /// <summary>The number of fade tables.</summary>
    uint32_t numFadePalettes = 0; // +0x90
    /// <summary>The fade tables (256 bytes each); the haze tables are among them.</summary>
    std::unique_ptr<uint8_t[]> fadePalettes; // +0x94
    /// <summary>The all-fade tables (256 bytes each).</summary>
    std::unique_ptr<uint8_t[]> allFadePalettes; // +0x98
    /// <summary>FIT "HazeShift": haze percentages are fixed point with this many bits.</summary>
    int32_t hazeShift = 0; // +0x9c
    /// <summary>1 &lt;&lt; <see cref="hazeShift"/>: full haze.</summary>
    int32_t maxHazePercent = 0; // +0xa0
    /// <summary>Half of <see cref="maxHazePercent"/> (rounding).</summary>
    int32_t hazeRound = 0; // +0xa4
    /// <summary>The first depth of the current table (no haze before).</summary>
    int32_t minHazeDepth = 0; // +0xa8
    /// <summary>The last depth of the current table (full haze after).</summary>
    int32_t maxHazeDepth = 0; // +0xac
    /// <summary>FIT "AltitudeShift": altitude to table index.</summary>
    int32_t altitudeShift = 0; // +0xb0
    /// <summary>The altitude past which the last table is used.</summary>
    int32_t maxAltitude = 0; // +0xb4

protected:
    /// <summary>The span <see cref="hazePercentAtDepth"/> last found (-1: none).</summary>
    static int32_t lastMinDepth;
    static int32_t lastMaxDepth;
    /// <summary>The haze percentage of that span.</summary>
    static int32_t lastHazePercent;
};

/// <summary>
/// Cycles the water colours (every so many milliseconds, through <see cref="WaterMagicColors"/>) in the game
/// palette.
/// </summary>
/// <remarks>MCX.EXE @ 0x006b3ee0</remarks>
void cycleColors();

/// <summary>The game's palette.</summary>
extern Palette* gamePalette;
/// <summary>The palette indices the water colour cycle steps through (8 entries).</summary>
extern uint8_t WaterMagicColors[8];
/// <summary>The current step of the water colour cycle (0..7).</summary>
extern uint8_t currentMagic;
/// <summary>Where palette files are found.</summary>
extern char palettePath[80];
