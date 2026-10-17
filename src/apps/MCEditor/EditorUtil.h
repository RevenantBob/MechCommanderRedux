#pragma once

// The editor's helper module (0x004790a0-0x00479da0): palette matching, Win32 error text, the TedIds.csv field reader
// and the shape drawer the object library renders its thumbnails and icons with. Its source file name wasn't kept;
// the names are the port's.

/// <summary>
/// String 0, "MechCommander Mission Editor": the title the editor's borderless dialogs give themselves (resource.h
/// calls it ID_SEPARATOR, which has the same value).
/// </summary>
inline constexpr UINT IDS_EDITOR_NAME = 0;

/// <summary>Longest field <see cref="getCsvField"/> looks through for its delimiter.</summary>
inline constexpr int32_t MAX_CSV_FIELD = 0x200;

/// <summary>
/// The palette index nearest to the colour <paramref name="bgr"/> (blue, green, red), by a distance that weighs green
/// 51, red 39 and blue 10; results are cached by colour at 5 bits per component.
/// </summary>
/// <param name="palette">
/// Read from byte 0x28 on as 4-byte entries (blue, green, red, unused), 246 of them; the index returned is the entry's
/// position plus 10, so entry 0 answers for palette index 10.
/// </param>
/// <remarks>
/// The cache answers for whatever palette filled it until <see cref="ColorCacheReady"/> is cleared (only
/// <c>DreditorDoc::loadPalette</c> does). With no entry closer than the start distance (0xfd02ff, more than any
/// colour can be) the answer would be 0.
/// </remarks>
uint8_t findNearestColor(const uint8_t* palette, const uint8_t* bgr);

/// <summary>Whether <see cref="findNearestColor"/>'s cache has been emptied; false empties it on the next call.</summary>
extern bool ColorCacheReady;

/// <summary>
/// Formats "<paramref name="function"/> failed with error code (0x<paramref name="error"/>) because
/// <c>message</c>\n" with the system's message for the Win32 error <paramref name="error"/> ("Unknown Error" when
/// it has none).
/// </summary>
/// <returns>A static buffer, overwritten by the next call.</returns>
const char* win32ErrorText(const char* function, DWORD error);

/// <summary>
/// Copies the field at <paramref name="source"/> (up to <paramref name="delimiter"/>, a newline or the end of the
/// string) into <paramref name="dest"/> and stores its length plus one, the step to the next field, in
/// <paramref name="length"/>.
/// </summary>
/// <returns>
/// 1; <see cref="MAX_CSV_FIELD"/> when no end was found within that many characters, and then nothing is copied and
/// <paramref name="length"/> keeps its value.
/// </returns>
/// <remarks><paramref name="dest"/> must hold <see cref="MAX_CSV_FIELD"/> characters.</remarks>
int32_t getCsvField(const char* source, char delimiter, char* dest, int32_t* length);

/// <summary>
/// Draws shape <paramref name="shapeNum"/> of the VFX shape table <paramref name="shapeTable"/> into the 8-bit
/// bitmap <paramref name="buffer"/> (<paramref name="width"/> x <paramref name="height"/>, rows of
/// <paramref name="width"/> bytes) with its hot spot at (<paramref name="hotX"/>, <paramref name="hotY"/>).
/// </summary>
/// <remarks>
/// The editor's copy of the game's AG_shape_draw (hand-written assembly), drawing through a window and pane built for
/// the bitmap. A shape whose data starts with the tokens 03 00 is translucent: each pixel becomes
/// <c>AlphaTable[shape &lt;&lt; 8 | bitmap]</c>. The clipping keeps the original's off-by-ones (EB-018).
/// </remarks>
void drawShape(uint8_t* buffer, int32_t width, int32_t height, const uint8_t* shapeTable, int32_t shapeNum,
               int32_t hotX, int32_t hotY);

/// <summary>
/// <see cref="drawShape"/> with every shape colour first looked up in <paramref name="remap"/> (256 entries: a row of
/// the fade table, the mechs' and vehicles' paint scheme).
/// </summary>
/// <remarks>
/// 0x004797a0, the editor's second copy of the drawer, with clipping of its own: it tests a shape's last column
/// against the right edge, and its translucent case neither shifts a pixel right nor takes an extra row. A
/// translucent shape cut at the bottom is drawn opaque (EB-059), and a run cut by an edge is remapped again for each
/// pixel it draws (EB-057).
/// </remarks>
void drawShapeRemapped(uint8_t* buffer, int32_t width, int32_t height, const uint8_t* shapeTable, int32_t shapeNum,
                       int32_t hotX, int32_t hotY, const uint8_t* remap);
