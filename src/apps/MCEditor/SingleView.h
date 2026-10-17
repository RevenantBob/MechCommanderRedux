#pragma once

#include "DreditorView.h"

/// <summary>The solo mission view: the map with one player's objectives, settings and drop zones.</summary>
class SingleView : public DreditorView
{
    DECLARE_DYNCREATE(SingleView)

public:
    /// <summary>Creates the view; DreditorApp creates it beside the document template's multiplayer view.</summary>
    SingleView();

private:
    /// <summary>
    /// Asks for <paramref name="value"/> (from <paramref name="low"/> to <paramref name="high"/>) in a
    /// <see cref="ValueDialog"/> titled as menu command <paramref name="command"/>.
    /// </summary>
    /// <returns>Whether the user pressed OK, which alone stores the value.</returns>
    /// <remarks>0x0042ec70. Original behaviour: nothing calls it.</remarks>
    bool askValue(UINT& value, UINT command, UINT low, UINT high);

protected:
    /// <summary>The solo popup menus.</summary>
    /// <remarks>0x00430ca0.</remarks>
    CMenu* popupMenu(EditorObject* object, CMenu& menu) override;

    /// <summary>Handles ON_COMMAND(ID_MISSION_OBJECTIVES_TEAM_1).</summary>
    afx_msg void OnMissionObjectivesTeam1();

    /// <summary>Handles ON_COMMAND(ID_FILE_OPEN).</summary>
    afx_msg void OnFileOpen();

    /// <summary>Handles ON_COMMAND(ID_MISSION_SETTINGS_TEAM_1).</summary>
    afx_msg void OnMissionSettingsTeam1();

    /// <summary>Handles ON_COMMAND(ID_FILE_WRITE_DISTRIBUTION_FILE_DPK).</summary>
    afx_msg void OnFileWriteDistributionFileDpk();

    DECLARE_MESSAGE_MAP()
};
