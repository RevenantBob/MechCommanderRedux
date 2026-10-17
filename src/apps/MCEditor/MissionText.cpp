#include "stdafx.h"
#include "MissionText.h"
#include "resource.h"

namespace
{
    /// <summary>The longest line read (CStdioFile::ReadString's limit).</summary>
    inline constexpr UINT LINE_SIZE = 0x100;
    /// <summary>The marks around the title.</summary>
    inline constexpr const char* TITLE_MARK = "***";
    /// <summary>The formatting codes cut from a section's start: "%fc6" and the like, or "%fm%fc6%c".</summary>
    inline constexpr size_t SHORT_CODE = 4;
    inline constexpr size_t LONG_CODE = 8;
    /// <summary>The characters cut from a section's end (its "%n" and line breaks).</summary>
    inline constexpr size_t SECTION_TAIL = 5;
    /// <summary>
    /// The string the "can't open" message is formatted from (0xe006); the editor has no such string, so the message
    /// box is empty.
    /// </summary>
    inline constexpr UINT IDS_DISPATCH_NOT_OPENED = 0xe006;

    /// <summary>
    /// Adds the lines of <paramref name="file"/> to <paramref name="section"/> from the one in
    /// <paramref name="line"/> up to (not including) the first holding <paramref name="marker"/>, or to the file's end.
    /// </summary>
    void readSection(CStdioFile& file, char* line, const char* marker, CString& section)
    {
        while (std::strstr(line, marker) == nullptr)
        {
            if (file.GetPosition() == file.GetLength())
            {
                break;
            }

            section += line;
            file.ReadString(line, LINE_SIZE);
        }
    }

    /// <summary>
    /// Cuts <paramref name="skip"/> characters from the start of <paramref name="section"/> and
    /// <see cref="SECTION_TAIL"/> from its end, through <paramref name="buffer"/> as the original does.
    /// </summary>
    /// <remarks>
    /// EB-070 fixed: a section shorter than <paramref name="skip"/> or <see cref="SECTION_TAIL"/> makes the original read
    /// past its text or write before the buffer; the port empties it.
    /// </remarks>
    void trimSection(CString& section, std::vector<char>& buffer, size_t skip)
    {
        const auto length = static_cast<size_t>(section.GetLength());

        if (length < skip || length < SECTION_TAIL)
        {
            section.Empty();
            return;
        }

        std::memcpy(buffer.data(), static_cast<LPCTSTR>(section), length + 1);
        buffer[length - SECTION_TAIL] = '\0';
        section = buffer.data() + skip;
    }
}

void MissionText::read(const CString& path)
{
    CStdioFile file;
    CFileException error;

    if (!file.Open(path, CFile::modeRead, &error))
    {
        CString format;
        format.LoadString(IDS_DISPATCH_NOT_OPENED);
        CString text;
        text.Format(format, static_cast<LPCTSTR>(path));
        CString caption;
        caption.LoadString(IDS_MCEDITOR);
        MessageBoxA(nullptr, text, caption, MB_OK);
        return;
    }

    // EB-071 fixed: the original's buffer is the file's length, though every read may take 256 bytes; the port's is
    // at least 256 (and room for the longest section it copies back).
    const auto length = static_cast<size_t>(file.GetLength());
    std::vector<char> buffer((std::max)(length + 1, static_cast<size_t>(LINE_SIZE)));
    char* line = buffer.data();
    std::strcpy(line, "");
    file.ReadString(line, LINE_SIZE);
    file.ReadString(line, LINE_SIZE);
    file.ReadString(line, LINE_SIZE);

    // The title lies between the third line's two "***" marks.
    const CString titleLine = line;
    CString afterMark = line + titleLine.Find(TITLE_MARK) + std::strlen(TITLE_MARK);
    afterMark = afterMark.Left(afterMark.Find(TITLE_MARK));
    title = afterMark;

    file.ReadString(line, LINE_SIZE);
    readSection(file, line, "%fc6FIRE SUPPORT:%n", dispatch);
    trimSection(dispatch, buffer, SHORT_CODE);

    for (int32_t i = 0; i < 5; i++)
    {
        file.ReadString(line, LINE_SIZE);
    }

    readSection(file, line, "%fc5%c----------%n", fireSupport);
    trimSection(fireSupport, buffer, SHORT_CODE);

    for (int32_t i = 0; i < 2; i++)
    {
        file.ReadString(line, LINE_SIZE);
    }

    readSection(file, line, "%fm%fc6%c**TACTICAL NOTICE**%n", tacticalNotice);
    trimSection(tacticalNotice, buffer, LONG_CODE);
    file.ReadString(line, LINE_SIZE);
    readSection(file, line, "%fc6<<<<END DISPATCH", closing);
    trimSection(closing, buffer, LONG_CODE);

    dispatch.Replace("%n", "\r\n");
    closing.Replace("%n", "\r\n");
    tacticalNotice.Replace("%n", "\r\n");
    fireSupport.Replace("%n", "\r\n");
}

namespace
{
    /// <summary>The section separator line.</summary>
    const char* const RULE_LINE = "%fc5%c----------%n\n";
    /// <summary>What an empty section is written as.</summary>
    const char* const EMPTY_SECTION = " ";
    /// <summary>What ends a section's text.</summary>
    const char* const SECTION_END = "%n%n";

    /// <summary><paramref name="format"/> with the string <paramref name="id"/> in it.</summary>
    CString formatWithString(const char* format, UINT id)
    {
        CString text;
        text.LoadString(id);
        CString line;
        line.Format(format, static_cast<const char*>(text));
        return line;
    }

    /// <summary>
    /// Writes a section: <paramref name="prefix"/>, <paramref name="text"/> with its line breaks as "%n",
    /// <paramref name="suffix"/>, a line break.
    /// </summary>
    /// <remarks>0x00460090; the original takes two more arguments it doesn't use.</remarks>
    void writeSection(CStdioFile& file, const CString& prefix, CString text, const CString& suffix)
    {
        text.Replace("\r\n", "%n");
        file.WriteString(prefix + text + suffix + "\n");
    }
}

void writeDescription(CFile& file, CString text)
{
    text.Replace("\r\r", "\r");
    file.Write(static_cast<LPCTSTR>(text), static_cast<UINT>(text.GetLength()));
}

bool MissionText::write(const CString& path, long artillerySmall, long artilleryLarge, long sensorProbes,
                        long cameraDrones)
{
    CStdioFile file;

    if (!file.Open(path, CFile::modeCreate | CFile::modeWrite | CFile::typeText))
    {
        return false;
    }

    file.WriteString(formatWithString("%%fm%%fc1%%c****%s****%%n\n", IDS_CLASSIFIED));
    file.WriteString("%fs%fc5%c----------%n\n");
    file.WriteString("%fc6%c***" + title + "***%n\n");
    writeSection(file, "%fc2", dispatch.GetLength() > 0 ? dispatch : CString(EMPTY_SECTION), SECTION_END);
    file.WriteString(formatWithString("%%fc6%s:%%n\n", IDS_FIRE_SUPPORT));

    CString format;
    format.LoadString(IDS_ARTILLERY_LD_SMALL_LD);
    CString text;
    text.Format(format, artillerySmall, artilleryLarge);
    CString line;
    line.Format("%%fc2%%t12%s%%n\n", static_cast<const char*>(text));
    file.WriteString(line);
    format.LoadString(IDS_SENSOR_PROBE_LD);
    text.Format(format, sensorProbes);
    line.Format("%%t12%s%%n\n", static_cast<const char*>(text));
    file.WriteString(line);
    format.LoadString(IDS_CAMERA_DRONE_LD);
    text.Format(format, cameraDrones);
    line.Format("%%t12%s%%n%%n\n", static_cast<const char*>(text));
    file.WriteString(line);

    // The last three sections become a space when empty.
    if (fireSupport.IsEmpty())
    {
        fireSupport = EMPTY_SECTION;
    }

    file.WriteString(RULE_LINE);
    writeSection(file, "%fc4", fireSupport, SECTION_END);
    file.WriteString(RULE_LINE);

    if (tacticalNotice.IsEmpty())
    {
        tacticalNotice = EMPTY_SECTION;
    }

    file.WriteString(formatWithString("%%fc1%%fm%%c**%s**%%n\n", IDS_TACTICAL_WARNING));
    writeSection(file, "%fc2%fs*", tacticalNotice, SECTION_END);

    if (closing.IsEmpty())
    {
        closing = EMPTY_SECTION;
    }

    file.WriteString(formatWithString("%%fm%%fc6%%c**%s**%%n\n", IDS_TACTICAL_NOTICE));
    writeSection(file, "%fs%fc2*", closing, SECTION_END);
    file.WriteString(formatWithString("%%fc6<<<<%s\n", IDS_END_DISPATCH));
    return true;
}
