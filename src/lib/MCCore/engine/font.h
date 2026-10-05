#pragma once

/// <summary>
/// A stroke (vector) font loaded from a <c>.bin</c> file of the font path, drawn with lines and scalable.
/// </summary>
/// <remarks>
/// Original source: <c>engine\font.cpp</c>, 0x420 bytes. File format (read whole): a list of letters, each
/// <c>uint8 letter, uint8 width, uint8 height</c> then strokes of 5 bytes <c>uint8 type, x0, y0, x1, y1</c> ended
/// by a type 0; the list ends with letter 0. Type 1 is a line in pixels (scaled with the letter), type 2 a line in
/// 1/256ths of the letter cell. A letter of 0xFF doesn't count toward the height.
/// </remarks>
class Font
{
public:
    /// <summary>Loads <paramref name="fontName"/><c>.bin</c> from the font path.</summary>
    /// <remarks>MCX.EXE @ 0x00646960</remarks>
    int32_t init(char* fontName);

    /// <summary>The strokes of <paramref name="letter"/> (cached in <see cref="letterCache"/>), or null.</summary>
    /// <remarks>MCX.EXE @ 0x00646a70</remarks>
    uint8_t* find(uint8_t letter);

    /// <summary><see cref="find"/>, falling back from lower to upper case.</summary>
    /// <remarks>MCX.EXE @ 0x00646ad0</remarks>
    uint8_t* findLetter(uint8_t letter);

    /// <summary>The width of <paramref name="letter"/> (scaled).</summary>
    /// <remarks>MCX.EXE @ 0x00646b00</remarks>
    int32_t printWidth(uint8_t letter);

    /// <summary>
    /// The width of <paramref name="text"/>; with <paramref name="multiLine"/>, the widest of its lines.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00646b60</remarks>
    int32_t printWidth(char* text, int multiLine);

    /// <summary>Draws <paramref name="letter"/> at the cursor into <paramref name="pane"/>; returns its width.</summary>
    /// <remarks>MCX.EXE @ 0x00646c30</remarks>
    int32_t print(uint8_t letter, _pane* pane);

    /// <summary>Draws <paramref name="text"/> at the cursor in <paramref name="color"/> (-1 keeps the colour).</summary>
    /// <remarks>MCX.EXE @ 0x00646e30</remarks>
    void print(char* text, int32_t color, _pane* pane);

    /// <summary>Moves the cursor to (<paramref name="x"/>, <paramref name="y"/>) and draws <paramref name="text"/>.</summary>
    /// <remarks>MCX.EXE @ 0x00646e70</remarks>
    void print(int32_t x, int32_t y, char* text, int32_t color, _pane* pane);

    /// <summary>Draws <paramref name="text"/> up to its first newline; returns the characters drawn.</summary>
    /// <remarks>MCX.EXE @ 0x00646ea0</remarks>
    int32_t printToNewline(char* text, int32_t color, _pane* pane);

    /// <summary>Moves the cursor and draws up to the first newline.</summary>
    /// <remarks>MCX.EXE @ 0x00646ef0</remarks>
    int32_t printToNewline(int32_t x, int32_t y, char* text, int32_t color, _pane* pane);

protected:
    /// <summary>The tallest letter in the font data.</summary>
    /// <remarks>MCX.EXE @ 0x00646bf0</remarks>
    uint8_t getHeight();

public:
    /// <summary>The cursor's x.</summary>
    int32_t curX; // +0x00
    /// <summary>The cursor's y.</summary>
    int32_t curY; // +0x04
    /// <summary>The colour letters are drawn in (15 after init).</summary>
    int32_t color; // +0x08
    /// <summary>The tallest letter's height.</summary>
    uint8_t fontHeight; // +0x0c
    /// <summary>The scale applied when <see cref="scaled"/> (2.0 after init).</summary>
    float scale; // +0x10
    /// <summary>Nonzero to scale sizes by <see cref="scale"/> (rounded down); 1 after init.</summary>
    int32_t scaled; // +0x18
    /// <summary>The font file.</summary>
    std::unique_ptr<uint8_t[]> fontData; // +0x1c
    /// <summary>Each letter's strokes once looked up; -1 (as a pointer) until then.</summary>
    uint8_t* letterCache[256]; // +0x20
};
