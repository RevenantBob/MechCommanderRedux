#pragma once

/// <summary>A VFX font file with its colour translation table.</summary>
/// <remarks>
/// Original source: <c>gui\afont.cpp</c>, 0x104 bytes (no vtable). The font data comes from the GUI heap. The fonts
/// the game uses are globals made by <c>aSystem::start</c> (<c>whiteFont</c>, <c>medRedFont</c>, ...,
/// gui/asystem.h).
/// </remarks>
class aFont
{
public:
    /// <remarks>MCX.EXE @ 0x0060a970</remarks>
    aFont();
    /// <summary>Unregisters the colour table (the data is freed by <see cref="destroy"/>).</summary>
    /// <remarks>MCX.EXE @ 0x0060a980</remarks>
    ~aFont();
    aFont(const aFont&) = delete;
    aFont& operator=(const aFont&) = delete;

    /// <summary>Loads font file <paramref name="fileName"/> under <c>fontPath</c>; the colour table is the identity.</summary>
    /// <returns>0, -1 when missing, -2 when empty, 3 when out of memory.</returns>
    /// <remarks>MCX.EXE @ 0x0060a990</remarks>
    int32_t init(char* fileName);
    /// <remarks>MCX.EXE @ 0x0060aaa0</remarks>
    void destroy();
    /// <summary><see cref="destroy"/> then <see cref="init"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0060aac0</remarks>
    int32_t load(char* fileName);
    /// <summary>The font's height (0 when not loaded).</summary>
    /// <remarks>MCX.EXE @ 0x0060aae0</remarks>
    int32_t height();
    /// <summary>The width of <paramref name="text"/> in pixels.</summary>
    /// <remarks>MCX.EXE @ 0x0060ab00</remarks>
    int32_t width(uint8_t* text);
    /// <summary>The width of one character.</summary>
    /// <remarks>MCX.EXE @ 0x0060ab70</remarks>
    int32_t width(uint8_t c);
    /// <summary>Draws one character at (<paramref name="xPos"/>, <paramref name="yPos"/>).</summary>
    /// <returns>0, or -3 when not loaded.</returns>
    /// <remarks>MCX.EXE @ 0x0060aba0 (unnamed in the symbols)</remarks>
    int32_t writeChar(_pane* pane, int32_t xPos, int32_t yPos, char c);
    /// <summary>
    /// Draws <paramref name="text"/>, cut to <paramref name="maxWidth"/> pixels unless it is -1 (the string is
    /// shortened in place while drawing and restored).
    /// </summary>
    /// <returns>0, or -3 when not loaded.</returns>
    /// <remarks>MCX.EXE @ 0x0060abf0</remarks>
    int32_t writeString(_pane* pane, int32_t xPos, int32_t yPos, uint8_t* text, int32_t maxWidth);
    /// <summary>Draws <paramref name="text"/> up to its first newline.</summary>
    /// <remarks>MCX.EXE @ 0x0060aca0</remarks>
    int32_t writeStringToNewline(_pane* pane, int32_t xPos, int32_t yPos, uint8_t* text);
    /// <summary>
    /// How many characters of <paramref name="text"/> fit in <paramref name="maxWidth"/> pixels; with
    /// <paramref name="wordWrap"/>, breaking at a space (-1 when no break fits).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0060ad20</remarks>
    int32_t charactersToWidth(uint8_t* text, int32_t maxWidth, int wordWrap);

    /// <summary>The font file's contents (registered with the renderers while loaded).</summary>
    std::unique_ptr<uint8_t[]> fontData; // +0x00
    /// <summary>The colour translation the characters are drawn through.</summary>
    uint8_t colorTable[256] = {}; // +0x04
};
