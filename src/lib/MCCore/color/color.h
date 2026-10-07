#pragma once

class MCFile;
class MCFitIniFile;
class MCPalette;

#pragma pack(push, 1)
/// <summary>
/// The header of a <c>.pal</c> file (0x304 bytes in all): the first colour and the number of colours, then
/// <c>numColors</c> RGB triples of 6-bit values.
/// </summary>
struct MCPaletteFileHeader
{
    uint16_t FirstColor;
    uint16_t NumColors;
};
#pragma pack(pop)
static_assert(sizeof(MCPaletteFileHeader) == 4);

/// <summary>The size of a whole <c>.pal</c> file (and of each palette of an extract palette file).</summary>
inline constexpr int32_t PALETTE_FILE_SIZE = 0x304;

/// <summary>A block of palette colours: a range of indices and their RGB values.</summary>
/// <remarks>Original source: <c>color\color.cpp</c>, 8 bytes.</remarks>
class MCPaletteBlock
{
public:
    /// <summary>
    /// Takes the range and colours from a <c>.pal</c> image (<see cref="MCPaletteFileHeader"/> then the triples),
    /// allocating <see cref="RgbData"/> the first time.
    /// </summary>
    void InitRgbData(uint8_t* data);

    /// <summary>Frees <see cref="RgbData"/>.</summary>
    void Destroy();

    /// <summary>The number of colours.</summary>
    int16_t NumColors = 0;
    /// <summary>The first palette index.</summary>
    int16_t FirstColor = 0;
    /// <summary>The colours (6 bits per channel); for a <see cref="MCPalette"/>, the palette the game shows.</summary>
    std::unique_ptr<uint8_t[]> RgbData;
};

#pragma pack(push, 1)
/// <summary>A colour range as the palette FIT's "Range%d" blocks give it (10 bytes, in reading order).</summary>
struct MCColorRangeData
{
    uint8_t BaseColorIndex;
    uint8_t MaxLitColor;
    uint8_t MaxHazedColor;
    uint8_t BaseRed;
    uint8_t BaseGreen;
    uint8_t BaseBlue;
    uint8_t LightSourceFlag;
    uint8_t DepthCueFlag;
    uint8_t CalcLightSource;
    uint8_t CalcDepthCue;
};
#pragma pack(pop)
static_assert(sizeof(MCColorRangeData) == 10);

/// <summary>
/// A run of palette indices shading one base colour, from lit to hazed: the unit the depth-haze and lighting
/// lookups work on.
/// </summary>
/// <remarks>Original source: <c>color\color.cpp</c>, 0x24 bytes.</remarks>
class MCColorRange
{
public:
    MCColorRange() = default;

    /// <summary>A range from its FIT data, belonging to <paramref name="palette"/>.</summary>
    MCColorRange(MCColorRangeData& data, MCPalette* palette);

    MCColorRange(int32_t baseColorIndex, uint8_t baseRed, uint8_t baseGreen, uint8_t baseBlue, int32_t maxLitColor,
                 int32_t maxHazedColor, MCPalette* palette, int lightSourceFlag, int depthCueFlag, int calcLightSource,
                 int calcDepthCue);

    /// <summary>The palette the range belongs to.</summary>
    MCPalette* Palette = nullptr;
    /// <summary>Nonzero when the range is lit by the light source.</summary>
    int LightSourceFlag = 0;
    /// <summary>Nonzero when the range is depth-cued (hazed).</summary>
    int DepthCueFlag = 0;
    int CalcLightSource = 0;
    int CalcDepthCue = 0;
    /// <summary>The range's first palette index.</summary>
    int32_t BaseColorIndex = 0;
    uint8_t BaseRed = 0;
    uint8_t BaseGreen = 0;
    uint8_t BaseBlue = 0;
    /// <summary>The number of lit shades (the range spans baseColorIndex..baseColorIndex + maxHazedColor).</summary>
    int32_t MaxLitColor = 0;
    /// <summary>The number of shades in the range.</summary>
    int32_t MaxHazedColor = 0;
};

/// <summary>
/// The game palette and its tables: the normal, black-and-white and "extract" palettes, fade tables (256-byte
/// index translations), the depth-haze tables and the colour ranges, all described by a palette FIT file.
/// </summary>
/// <remarks>
/// Original source: <c>color\color.cpp</c>, 0xb8 bytes. The <see cref="MCPaletteBlock"/> base holds the palette the
/// game shows; <see cref="OriginalPalette"/> the file as loaded, which fades return to.
/// </remarks>
class MCPalette : public MCPaletteBlock
{
public:
    /// <summary>The palette of haze level <paramref name="shade"/> (negative for the other half), null for 0.</summary>
    uint8_t* DepthHazedShadePalette(int32_t shade, float depth);

    /// <summary>
    /// The palette index of colour range <paramref name="colorRange"/> at <paramref name="shade"/> (0..256) and
    /// <paramref name="depth"/>; <paramref name="maxShade"/> 0 uses the range's lit shades.
    /// </summary>
    int32_t DepthHazedShade(int32_t colorRange, int32_t shade, float depth, int32_t maxShade);

    /// <summary>
    /// The fade table of haze level <paramref name="hazeLevel"/> (clamped; negative levels use the second set), null
    /// for 0.
    /// </summary>
    uint8_t* GetHazePalette(int32_t hazeLevel);

    /// <summary>The colour range containing <paramref name="colorIndex"/>, or -1.</summary>
    int32_t FindColorRange(int32_t colorIndex);

    /// <summary>The lit and depth-cued colour range containing <paramref name="colorIndex"/>, or -1.</summary>
    int32_t FindLightToDarkColorRange(int32_t colorIndex);

    /// <summary>The haze percentage (0..<see cref="MaxHazePercent"/>) at <paramref name="depth"/>, cached per span.</summary>
    int32_t HazePercentAtDepth(float depth);

    /// <summary>Nothing (colour cycling is always on).</summary>
    void FullCycleOn();

    /// <summary>Restores the loaded palette and shows it.</summary>
    void FullCycleOff();

    /// <summary>
    /// Moves the shown palette toward <paramref name="targetPalette"/> (a <c>.pal</c> image) by
    /// <paramref name="fadePercent"/> (clamped to 1).
    /// </summary>
    void FadeToPalette(float& fadePercent, uint8_t* targetPalette);

    /// <summary>Moves the shown palette toward one colour.</summary>
    void FadeToColor(float& fadePercent, char red, char green, char blue);

    /// <summary>Moves the shown palette back toward the loaded one.</summary>
    void FadeToOriginalPalette(float& fadePercent);

    /// <summary>Moves the shown palette toward the black-and-white one.</summary>
    void FadeToBlackAndWhite(float& fadePercent);

    /// <summary>Shows the palette through <c>aSystem::activatePalette</c> from <paramref name="start"/> for <paramref name="count"/> entries.</summary>
    void Animate(int start, int count);

    /// <summary>
    /// Shows a palette: <paramref name="which"/> 1 is extract palette <paramref name="extractIndex"/>, 2 the
    /// black-and-white palette, anything else the normal one.
    /// </summary>
    void Activate(int32_t which, int32_t extractIndex);

    /// <summary>
    /// Copies <paramref name="count"/> colours from <paramref name="colors"/> into the game palette from index
    /// <paramref name="start"/> (always <c>gamePalette</c>, whichever palette it's called on).
    /// </summary>
    void TweakPalette(int start, int count, MCVfxRgb* colors);

    /// <summary>Clears the pointers.</summary>
    void Init();

    /// <summary>Loads the palette FIT <paramref name="paletteFileName"/> from the palette path ("Palette" block).</summary>
    int32_t Init(char* paletteFileName);

    /// <summary>Loads the palette from an open FIT: its info, palettes, colour ranges and "Tables".</summary>
    int32_t Init(MCFitIniFile& paletteFile);

    /// <summary>Frees everything.</summary>
    void Destroy();

    /// <summary>Loads the depth table, fade table and all-fade table files named by the FIT.</summary>
    int32_t LoadTables();

    /// <summary>Reloads the normal palette (<see cref="PaletteFileName"/>).</summary>
    int32_t LoadPalette();

    /// <summary>Saves the normal palette (keeping a <c>.bak</c>).</summary>
    int32_t SavePalette();

    /// <summary>Reloads the black-and-white palette.</summary>
    int32_t LoadBWPalette();

    /// <summary>Saves the black-and-white palette (keeping a <c>.bak</c>).</summary>
    int32_t SaveBWPalette();

    /// <summary>Reloads the extract palettes.</summary>
    int32_t LoadExtractPalette();

    /// <summary>Saves the extract palettes (keeping a <c>.bak</c>).</summary>
    int32_t SaveExtractPalette();

    /// <summary>Appends a fade table (a copy of the last one).</summary>
    void AddFadePalette();

    /// <summary>Removes fade table <paramref name="index"/>.</summary>
    void RemoveFadePalette(int32_t index);

    /// <summary>Saves the fade tables.</summary>
    int32_t SaveFadePalettes();

    /// <summary>Appends an extract palette.</summary>
    void AddExtractPalette();

    /// <summary>Removes extract palette <paramref name="index"/>.</summary>
    void RemoveExtractPalette(int32_t index);

    /// <summary>Copies the loaded palette over extract palette <paramref name="index"/>.</summary>
    void CopyNormalToExtractPalette(int32_t index);

protected:
    /// <summary>Picks the depth-vs-haze table for <paramref name="altitude"/>.</summary>
    void RecalculateDepthVsHazeInfo(int32_t altitude);

    /// <summary>Reads the haze settings, the three palettes and the "Ranges" block.</summary>
    int32_t LoadPaletteInfo(MCFitIniFile& paletteFile);

    /// <summary>
    /// Reads "NumColorRanges" and each "Range%d" block into <see cref="ColorRanges"/>.
    /// </summary>
    int32_t LoadColorRanges(MCFitIniFile& paletteFile);

    /// <summary>Reads "PaletteFileName" and loads that <c>.pal</c>.</summary>
    int32_t LoadPalette(MCFitIniFile& paletteFile);

    /// <summary>Reads "BWPaletteFileName" and loads that <c>.pal</c>.</summary>
    int32_t LoadBWPalette(MCFitIniFile& paletteFile);

    /// <summary>Reads "ExPaletteFileName" and loads those palettes (0x304 bytes each).</summary>
    int32_t LoadExtractPalette(MCFitIniFile& paletteFile);

    /// <summary>
    /// Reads the depth table file: NumDepthAtHazeLevelTables tables of 2^NumDepthHazeEntriesShift int32 depths
    /// (its size must match).
    /// </summary>
    int32_t LoadDepthHazeTables(MCFile& tableFile);

    /// <summary>Reads the fade table file (256-byte tables).</summary>
    int32_t LoadFadePalettes(MCFile& tableFile);

    /// <summary>Reads the all-fade table file (256-byte tables).</summary>
    int32_t LoadAllFadePalettes(MCFile& tableFile);

public:
    /// <summary>The loaded palette file's size.</summary>
    uint32_t PaletteSize = 0;
    /// <summary>The black-and-white palette (a <c>.pal</c> image).</summary>
    std::unique_ptr<uint8_t[]> BwPalette;
    /// <summary>Per byte of the palette, the signed distance to the fade target.</summary>
    std::unique_ptr<int8_t[]> FadeDeltas;
    /// <summary>The extract palettes (0x304 bytes each).</summary>
    std::unique_ptr<uint8_t[]> ExtractPalettes;
    /// <summary>The largest of <see cref="FadeDeltas"/>.</summary>
    int32_t MaxFadeDelta = 0;
    /// <summary>Nonzero once <see cref="FadeDeltas"/> is computed for the current target.</summary>
    int32_t FadeDeltasValid = 0;
    /// <summary>The fade target: -1 none, 0 the loaded palette, 1 black and white, 2 a colour.</summary>
    int32_t FadeTarget = -1;
    /// <summary>The colour of the last <see cref="FadeToColor"/>.</summary>
    uint8_t FadeRed = 0;
    uint8_t FadeGreen = 0;
    uint8_t FadeBlue = 0;
    /// <summary>The depth-vs-haze tables (depths at which each haze step starts).</summary>
    std::unique_ptr<int32_t[]> DepthHazeTables;
    /// <summary>FIT "NumDepthAtHazeLevelTables": one table per altitude step.</summary>
    int32_t NumDepthAtHazeLevelTables = 0;
    /// <summary>The table for the camera's altitude.</summary>
    int32_t* CurrentDepthTable = nullptr;
    /// <summary>Entries per table (1 &lt;&lt; <see cref="NumDepthHazeEntriesShift"/>).</summary>
    int32_t NumDepthHazeEntries = 0;
    /// <summary>FIT "NumDepthHazeEntriesShift".</summary>
    int32_t NumDepthHazeEntriesShift = 0;
    /// <summary>The offset of the second set of haze fade tables (<see cref="NumBitmapHazeLevels"/> * 256).</summary>
    int32_t HazePaletteOffset = 0;
    /// <summary>FIT "PaletteFileName".</summary>
    char PaletteFileName[8] = {};
    /// <summary>FIT "ExPaletteFileName".</summary>
    char ExPaletteFileName[8] = {};
    /// <summary>FIT "BWPaletteFileName".</summary>
    char BwPaletteFileName[8] = {};
    /// <summary>FIT "DepthTableFile".</summary>
    char DepthTableFile[8] = {};
    /// <summary>FIT "FadeTableFile".</summary>
    char FadeTableFile[8] = {};
    /// <summary>FIT "AllFadeTableFile".</summary>
    char AllFadeTableFile[8] = {};
    /// <summary>The loaded <c>.pal</c> image.</summary>
    std::unique_ptr<uint8_t[]> OriginalPalette;
    /// <summary>FIT "NumBitmapHazeLevels": fade tables per haze set.</summary>
    int32_t NumBitmapHazeLevels = 0;
    /// <summary>FIT "NumColorRanges".</summary>
    int32_t NumColorRanges = 0;
    /// <summary>The number of extract palettes.</summary>
    int32_t NumExtractPalettes = 0;
    /// <summary>The colour ranges.</summary>
    std::vector<MCColorRange> ColorRanges;
    /// <summary>The number of all-fade tables.</summary>
    uint32_t NumAllFadePalettes = 0;
    /// <summary>The number of fade tables.</summary>
    uint32_t NumFadePalettes = 0;
    /// <summary>The fade tables (256 bytes each); the haze tables are among them.</summary>
    std::unique_ptr<uint8_t[]> FadePalettes;
    /// <summary>The all-fade tables (256 bytes each).</summary>
    std::unique_ptr<uint8_t[]> AllFadePalettes;
    /// <summary>FIT "HazeShift": haze percentages are fixed point with this many bits.</summary>
    int32_t HazeShift = 0;
    /// <summary>1 &lt;&lt; <see cref="HazeShift"/>: full haze.</summary>
    int32_t MaxHazePercent = 0;
    /// <summary>Half of <see cref="MaxHazePercent"/> (rounding).</summary>
    int32_t HazeRound = 0;
    /// <summary>The first depth of the current table (no haze before).</summary>
    int32_t MinHazeDepth = 0;
    /// <summary>The last depth of the current table (full haze after).</summary>
    int32_t MaxHazeDepth = 0;
    /// <summary>FIT "AltitudeShift": altitude to table index.</summary>
    int32_t AltitudeShift = 0;
    /// <summary>The altitude past which the last table is used.</summary>
    int32_t MaxAltitude = 0;

protected:
    /// <summary>The span <see cref="HazePercentAtDepth"/> last found (-1: none).</summary>
    static int32_t _LastMinDepth;
    static int32_t _LastMaxDepth;
    /// <summary>The haze percentage of that span.</summary>
    static int32_t _LastHazePercent;
};

/// <summary>
/// Cycles the water colours (every so many milliseconds, through <see cref="WaterMagicColors"/>) in the game
/// palette.
/// </summary>
void CycleColors();

/// <summary>The game's palette.</summary>
extern MCPalette* GamePalette;
/// <summary>The palette indices the water colour cycle steps through (8 entries).</summary>
extern uint8_t WaterMagicColors[8];
/// <summary>The current step of the water colour cycle (0..7).</summary>
extern uint8_t CurrentMagic;
/// <summary>Where palette files are found.</summary>
extern char PalettePath[80];
