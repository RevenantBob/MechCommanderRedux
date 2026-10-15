#pragma once

#include "DreditorView.h"

/// <summary>The multiplayer mission view: the map with two teams' objectives, settings and drop zones.</summary>
/// <remarks>Virtual table 0x004fd108.</remarks>
class MultiView : public DreditorView
{
protected:
    /// <summary>Creates the object from serialization or a document template.</summary>
    /// <remarks>0x00428f20.</remarks>
    MultiView();
    DECLARE_DYNCREATE(MultiView)

private:
    /// <summary>
    /// Opens a new objectives dialog (replacing the last one) for team 2 (side 1) when <paramref name="team2"/>, else
    /// team 1 (side 2), without the kinds that target units, and marks the document changed.
    /// </summary>
    /// <remarks>The body of 0x00429250 and 0x004293c0. Unlike the solo view's, the dialog keeps its template's title.</remarks>
    void openObjectives(bool team2);
    /// <summary>
    /// Asks for two values (<paramref name="first"/> and <paramref name="second"/>, each within its range) in a
    /// <see cref="ValueDialog"/> titled as menu command <paramref name="command"/>.
    /// </summary>
    /// <returns>Whether the user pressed OK, which alone stores the values.</returns>
    /// <remarks>0x00429000. Original behaviour: nothing calls it.</remarks>
    bool askValues(UINT& first, UINT& second, UINT command, UINT firstMin, UINT firstMax, UINT secondMin,
                   UINT secondMax);

protected:
    /// <summary>The multiplayer popup menus.</summary>
    /// <remarks>0x0042bcc0.</remarks>
    CMenu* popupMenu(EditorObject* object, CMenu& menu) override;

    /// <summary>Opens team 1's objectives (side 2).</summary>
    /// <remarks>0x00429250.</remarks>
    afx_msg void OnMissionObjectivesTeam1();

    /// <summary>Turns Drop Zones (team 2) off once side 1 has more than two drop zones.</summary>
    /// <remarks>0x00429370.</remarks>
    afx_msg void OnUpdateMissionDropZonesTeam2(CCmdUI* pCmdUI);

    /// <summary>Opens team 2's objectives (side 1).</summary>
    /// <remarks>0x004293c0.</remarks>
    afx_msg void OnMissionObjectivesTeam2();

    /// <summary>Opens a multiplayer mission.</summary>
    /// <remarks>0x0042b740.</remarks>
    afx_msg void OnFileOpen();

    /// <summary>Edits team 1's settings and dispatch, with the mission's time limit.</summary>
    /// <remarks>0x0042b790.</remarks>
    afx_msg void OnMissionSettingsTeam1();

    /// <summary>Edits team 2's settings and dispatch.</summary>
    /// <remarks>0x0042b9d0.</remarks>
    afx_msg void OnMissionSettingsTeam2();

    /// <summary>Picks the tool placing team 2's drop zones (frame 1 of the drop zone, for side 1).</summary>
    /// <remarks>0x004292f0.</remarks>
    afx_msg void OnMissionDropZonesTeam2();

    /// <summary>Edits the mission's description (<see cref="EditorMap::description"/>).</summary>
    /// <remarks>0x0042bb50.</remarks>
    afx_msg void OnMissionDescription();

    /// <summary>
    /// Writes a .dpk: a FastFile holding a fresh save of the mission (Data\Savegame\&lt;name&gt;.MPK, written under a
    /// GUID and swapped in for the mission's own while the .dpk is made) and a FastFile of the mission's files, copied
    /// under the GUID with the script and FIT file rewritten to name the copies.
    /// </summary>
    /// <remarks>
    /// 0x00429910. Unlike the solo view's, it doesn't look the mission up in the database and packs both teams'
    /// dispatches but no campaign or warrior file.
    /// </remarks>
    afx_msg void OnFileWriteDistributionFileDpk();

    DECLARE_MESSAGE_MAP()
};
