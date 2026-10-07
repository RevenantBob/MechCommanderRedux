#pragma once

/// <summary>A VFX font file with its colour translation table.</summary>
/// <remarks>
/// Original source: <c>gui\afont.cpp</c>, 0x104 bytes (no vtable). The font data comes from the GUI heap. The fonts
/// the game uses are globals made by <c>aSystem::start</c> (<c>whiteFont</c>, <c>medRedFont</c>, ...,
/// gui/asystem.h).
/// </remarks>
class MCGuiFont
{
public:
    MCGuiFont();
    /// <summary>Unregisters the colour table (the data is freed by <see cref="Destroy"/>).</summary>
    ~MCGuiFont();
    MCGuiFont(const MCGuiFont&) = delete;
    MCGuiFont& operator=(const MCGuiFont&) = delete;

    /// <summary>Loads font file <paramref name="fileName"/> under <c>fontPath</c>; the colour table is the identity.</summary>
    /// <returns>0, -1 when missing, -2 when empty, 3 when out of memory.</returns>
    int32_t Init(char* fileName);
    void Destroy();
    /// <summary><see cref="Destroy"/> then <see cref="Init"/>.</summary>
    int32_t Load(char* fileName);
    /// <summary>The font's height (0 when not loaded).</summary>
    int32_t Height();
    /// <summary>The width of <paramref name="text"/> in pixels.</summary>
    int32_t Width(uint8_t* text);
    /// <summary>The width of one character.</summary>
    int32_t Width(uint8_t c);
    /// <summary>Draws one character at (<paramref name="xPos"/>, <paramref name="yPos"/>).</summary>
    /// <returns>0, or -3 when not loaded.</returns>
    int32_t WriteChar(MCPane* pane, int32_t xPos, int32_t yPos, char c);
    /// <summary>
    /// Draws <paramref name="text"/>, cut to <paramref name="maxWidth"/> pixels unless it is -1 (the string is
    /// shortened in place while drawing and restored).
    /// </summary>
    /// <returns>0, or -3 when not loaded.</returns>
    int32_t WriteString(MCPane* pane, int32_t xPos, int32_t yPos, uint8_t* text, int32_t maxWidth);
    /// <summary>Draws <paramref name="text"/> up to its first newline.</summary>
    int32_t WriteStringToNewline(MCPane* pane, int32_t xPos, int32_t yPos, uint8_t* text);
    /// <summary>
    /// How many characters of <paramref name="text"/> fit in <paramref name="maxWidth"/> pixels; with
    /// <paramref name="wordWrap"/>, breaking at a space (-1 when no break fits).
    /// </summary>
    int32_t CharactersToWidth(uint8_t* text, int32_t maxWidth, int wordWrap);

    /// <summary>The font file's contents (registered with the renderers while loaded).</summary>
    std::unique_ptr<uint8_t[]> FontData;
    /// <summary>The colour translation the characters are drawn through.</summary>
    uint8_t ColorTable[256] = {};
};
