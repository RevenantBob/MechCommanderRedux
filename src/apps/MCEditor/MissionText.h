#pragma once

// The mission's dispatch, the briefing text the game shows (Data\Missions\<mission>.txt): its title and sections, as
// the map keeps them (+0x70ec; 0x0045faf0-0x004607c0, virtual table 0x004febc0). Its source file name wasn't kept;
// the names are the port's, after the file's markers.

/// <summary>A mission's dispatch text, in sections, with the editor's line breaks.</summary>
/// <remarks>0x18 bytes with the virtual table.</remarks>
class MissionText
{
public:
    /// <summary>
    /// Reads the dispatch from <paramref name="path"/>: the title between the third line's "***" marks, then each
    /// section up to the next marker ("FIRE SUPPORT:", the rule, "**TACTICAL NOTICE**", "END DISPATCH"), without its
    /// formatting codes and with "%n" as line breaks. A file that doesn't open is reported in a message box.
    /// </summary>
    /// <remarks>0x00460220. The sections are added to, not replaced.</remarks>
    void read(const CString& path);
    /// <summary>
    /// Writes the dispatch to <paramref name="path"/> with its formatting codes: the CLASSIFIED head, the title, the
    /// body, FIRE SUPPORT with the artillery (<paramref name="artillerySmall"/>, <paramref name="artilleryLarge"/>),
    /// sensor probes and camera drones the mission gives, the fire support text, the TACTICAL WARNING and TACTICAL
    /// NOTICE sections and END DISPATCH. An empty section is written as a space (the last three are set to one).
    /// </summary>
    /// <returns>False when the file can't be made.</returns>
    /// <remarks>0x0045fbd0.</remarks>
    bool write(const CString& path, long artillerySmall, long artilleryLarge, long sensorProbes, long cameraDrones);

    /// <summary>The title.</summary>
    CString title;
    /// <summary>The dispatch's body.</summary>
    CString dispatch;
    /// <summary>The fire support section.</summary>
    CString fireSupport;
    /// <summary>The tactical notice.</summary>
    CString tacticalNotice;
    /// <summary>The closing section, up to "END DISPATCH".</summary>
    CString closing;
};

/// <summary>
/// Writes <paramref name="text"/> to <paramref name="file"/> as it is, each doubled carriage return made one (what
/// reading a multiplayer save's description adds).
/// </summary>
/// <remarks>0x00460180; the original takes two more arguments it doesn't use (10 and 40).</remarks>
void writeDescription(CFile& file, CString text);
