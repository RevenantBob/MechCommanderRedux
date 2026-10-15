#pragma once

/// <summary>The shape colours that can be translucent: the rows of <see cref="AlphaTable"/>.</summary>
inline constexpr int32_t ALPHA_COLORS = 0x118;
/// <summary>Size of <see cref="AlphaTable"/>: a row of 256 results for every shape colour.</summary>
inline constexpr int32_t ALPHA_TABLE_SIZE = ALPHA_COLORS * 256;

/// <summary>
/// The translucency table: <c>AlphaTable[shapeColor &lt;&lt; 8 | background]</c> is the palette index a translucent
/// shape's pixel of <c>shapeColor</c> gives over <c>background</c>.
/// </summary>
/// <remarks>Original source: the editor's copy of <c>vfx\AlphaPalette.cpp</c>. Filled by <see cref="InitAlphaLookup"/>.</remarks>
extern char AlphaTable[ALPHA_TABLE_SIZE];
/// <summary>Nonzero for the colours AlphaPal.ini defines.</summary>
extern char SpecialColor[ALPHA_COLORS];

/// <summary>
/// Reads AlphaPal.ini (from <paramref name="dataPath"/>, else from its misc.fst) and fills <see cref="AlphaTable"/>
/// for <paramref name="palette"/> (256 RGB triples, 6 bits per component): colours 0 and 0xff leave the background,
/// colours the file doesn't define replace it with themselves, and a defined colour blends with backgrounds 10-245
/// (others give 0xff) by its weights, or by the dodge form when both weights are 0.
/// </summary>
/// <param name="dataPath">The game folder, ending with a backslash.</param>
/// <returns>
/// False when misc.fst can't be opened (after a message) or the file has an error (no message, EB-043); a misc.fst
/// without the file gives a table of undefined colours.
/// </returns>
/// <remarks>
/// The editor's revision of the game's InitAlphaLookup: 0x118 colours, the file read through MFC, the errors returned
/// instead of fatal.
/// </remarks>
bool InitAlphaLookup(const uint8_t* palette, const char* dataPath);
