#pragma once

class MCGuiFont;
class MCGuiPort;

/// <summary>
/// The logistics text formatter: lays out a string with embedded <c>%</c> codes into a port, wrapping words at the
/// port's width. Codes: <c>%%</c> a percent sign, <c>%n</c> new line, <c>%c</c> centre the rest of the line,
/// <c>%tNN</c> move right NN pixels, <c>%fcN</c> font colour N, <c>%fs</c>/<c>%fm</c>/<c>%fl</c> small/medium/large
/// font (from the <c>fonts</c> table).
/// </summary>
/// <remarks>
/// Original source: <c>logistics\smuti.cpp</c>, 0x128 bytes. One instance is embedded in <c>aSystem</c> at +0x7d4
/// (<c>application</c>), which the inventory blocks and the chat window use. With no port, <see cref="Process"/>
/// only measures.
/// </remarks>
class MCSmuti
{
public:
    /// <summary>Reads the text file <paramref name="fileName"/> and formats it into <paramref name="port"/>.</summary>
    /// <returns>The height used, as <see cref="Process"/>.</returns>
    int32_t Init(char* fileName, MCGuiPort* port, int32_t width);

    /// <summary>
    /// Formats <paramref name="text"/> into <paramref name="port"/> starting at line <paramref name="startY"/>;
    /// <paramref name="width"/> is the wrap width when there is no port (otherwise the port's width).
    /// </summary>
    /// <returns>The y just below the last line written.</returns>
    int32_t Process(uint8_t* text, MCGuiPort* port, int32_t width, int32_t startY);

    /// <summary>
    /// Before <paramref name="nextChar"/> is added: when it would pass the wrap width, writes the line buffer up to
    /// its last space and carries the rest to the next line.
    /// </summary>
    void CheckWrap(uint8_t nextChar);

    /// <summary>The current font.</summary>
    MCGuiFont* Font = nullptr;
    /// <summary>The port written into; null while only measuring.</summary>
    MCGuiPort* Port = nullptr;
    /// <summary>The wrap width in pixels.</summary>
    int32_t Width = 0;
    /// <summary>The pixel width of the pending line buffer.</summary>
    int32_t CurX = 0;
    /// <summary>The y of the current line.</summary>
    int32_t CurY = 0;
    /// <summary>The x where the pending line buffer will be written.</summary>
    int32_t LineStartX = 0;
    /// <summary>The font size: 0 small, 1 medium, 2 large (the column of <c>fonts</c>).</summary>
    int32_t FontSize = 0;
    /// <summary>The font colour (the row of <c>fonts</c>); starts at 3.</summary>
    int32_t FontColor = 0;
    /// <summary>The text of the current line not yet written.</summary>
    char LineBuffer[256] = {};
    /// <summary>The number of characters in <see cref="LineBuffer"/>.</summary>
    int32_t LineLength = 0;
    /// <summary>Set while a <c>%c</c> centred line was written directly (the buffer must not be flushed).</summary>
    int32_t Centered = 0;
};
