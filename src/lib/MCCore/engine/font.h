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
class MCFont
{
public:
    /// <summary>Loads <paramref name="fontName"/><c>.bin</c> from the font path.</summary>
    int32_t Init(char* fontName);

    /// <summary>The strokes of <paramref name="letter"/> (cached in <see cref="LetterCache"/>), or null.</summary>
    uint8_t* Find(uint8_t letter);

    /// <summary><see cref="Find"/>, falling back from lower to upper case.</summary>
    uint8_t* FindLetter(uint8_t letter);

    /// <summary>The width of <paramref name="letter"/> (scaled).</summary>
    int32_t PrintWidth(uint8_t letter);

    /// <summary>
    /// The width of <paramref name="text"/>; with <paramref name="multiLine"/>, the widest of its lines.
    /// </summary>
    int32_t PrintWidth(char* text, int multiLine);

    /// <summary>Draws <paramref name="letter"/> at the cursor into <paramref name="pane"/>; returns its width.</summary>
    int32_t Print(uint8_t letter, MCPane* pane);

    /// <summary>Draws <paramref name="text"/> at the cursor in <paramref name="color"/> (-1 keeps the colour).</summary>
    void Print(char* text, int32_t color, MCPane* pane);

    /// <summary>Moves the cursor to (<paramref name="x"/>, <paramref name="y"/>) and draws <paramref name="text"/>.</summary>
    void Print(int32_t x, int32_t y, char* text, int32_t color, MCPane* pane);

    /// <summary>Draws <paramref name="text"/> up to its first newline; returns the characters drawn.</summary>
    int32_t PrintToNewline(char* text, int32_t color, MCPane* pane);

    /// <summary>Moves the cursor and draws up to the first newline.</summary>
    int32_t PrintToNewline(int32_t x, int32_t y, char* text, int32_t color, MCPane* pane);

protected:
    /// <summary>The tallest letter in the font data.</summary>
    uint8_t GetHeight();

public:
    /// <summary>The cursor's x.</summary>
    int32_t CurX;
    /// <summary>The cursor's y.</summary>
    int32_t CurY;
    /// <summary>The colour letters are drawn in (15 after init).</summary>
    int32_t Color;
    /// <summary>The tallest letter's height.</summary>
    uint8_t FontHeight;
    /// <summary>The scale applied when <see cref="Scaled"/> (2.0 after init).</summary>
    float Scale;
    /// <summary>Nonzero to scale sizes by <see cref="Scale"/> (rounded down); 1 after init.</summary>
    int32_t Scaled;
    /// <summary>The font file.</summary>
    std::unique_ptr<uint8_t[]> FontData;
    /// <summary>Each letter's strokes once looked up; -1 (as a pointer) until then.</summary>
    uint8_t* LetterCache[256];
};
