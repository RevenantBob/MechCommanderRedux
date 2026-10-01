#pragma once

class aFont;
class aPort;

/// <summary>
/// The logistics text formatter: lays out a string with embedded <c>%</c> codes into a port, wrapping words at the
/// port's width. Codes: <c>%%</c> a percent sign, <c>%n</c> new line, <c>%c</c> centre the rest of the line,
/// <c>%tNN</c> move right NN pixels, <c>%fcN</c> font colour N, <c>%fs</c>/<c>%fm</c>/<c>%fl</c> small/medium/large
/// font (from the <c>fonts</c> table).
/// </summary>
/// <remarks>
/// Original source: <c>logistics\smuti.cpp</c>, 0x128 bytes. One instance is embedded in <c>aSystem</c> at +0x7d4
/// (<c>application</c>), which the inventory blocks and the chat window use. With no port, <see cref="process"/>
/// only measures.
/// </remarks>
class SMUTI
{
public:
    /// <summary>Reads the text file <paramref name="fileName"/> and formats it into <paramref name="port"/>.</summary>
    /// <returns>The height used, as <see cref="process"/>.</returns>
    /// <remarks>MCX.EXE @ 0x00728e10</remarks>
    int32_t init(char* fileName, aPort* port, int32_t width);

    /// <summary>
    /// Formats <paramref name="text"/> into <paramref name="port"/> starting at line <paramref name="startY"/>;
    /// <paramref name="width"/> is the wrap width when there is no port (otherwise the port's width).
    /// </summary>
    /// <returns>The y just below the last line written.</returns>
    /// <remarks>MCX.EXE @ 0x00728ed0</remarks>
    int32_t process(uint8_t* text, aPort* port, int32_t width, int32_t startY);

    /// <summary>
    /// Before <paramref name="nextChar"/> is added: when it would pass the wrap width, writes the line buffer up to
    /// its last space and carries the rest to the next line.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00729430</remarks>
    void checkWrap(uint8_t nextChar);

    /// <summary>The current font.</summary>
    aFont* font = nullptr; // +0x0
    /// <summary>The port written into; null while only measuring.</summary>
    aPort* port = nullptr; // +0x4
    /// <summary>The wrap width in pixels.</summary>
    int32_t width = 0; // +0x8
    /// <summary>The pixel width of the pending line buffer.</summary>
    int32_t curX = 0; // +0xc
    /// <summary>The y of the current line.</summary>
    int32_t curY = 0; // +0x10
    /// <summary>The x where the pending line buffer will be written.</summary>
    int32_t lineStartX = 0; // +0x14
    /// <summary>The font size: 0 small, 1 medium, 2 large (the column of <c>fonts</c>).</summary>
    int32_t fontSize = 0; // +0x18
    /// <summary>The font colour (the row of <c>fonts</c>); starts at 3.</summary>
    int32_t fontColor = 0; // +0x1c
    /// <summary>The text of the current line not yet written.</summary>
    char lineBuffer[256] = {}; // +0x20
    /// <summary>The number of characters in <see cref="lineBuffer"/>.</summary>
    int32_t lineLength = 0; // +0x120
    /// <summary>Set while a <c>%c</c> centred line was written directly (the buffer must not be flushed).</summary>
    int32_t centered = 0; // +0x124
};
