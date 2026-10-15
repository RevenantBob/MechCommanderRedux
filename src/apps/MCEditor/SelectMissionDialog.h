#pragma once

#include "MissionPreview.h"

class DaoDatabase;

/// <summary>
/// Picks a mission from the database (solo or multiplayer ones) or a map from Data\Terrain, with a picture of the map;
/// Delete removes the chosen mission's files and records.
/// </summary>
class SelectMissionDialog : public CDialog
{
public:
    /// <summary>The dialog template.</summary>
    enum
    {
        IDD = IDD_SELECT_MISSION_DIALOG
    };

    /// <summary>What the list shows.</summary>
    enum Mode
    {
        /// <summary>The solo missions.</summary>
        SOLO_MISSIONS = 0,
        /// <summary>The multiplayer missions.</summary>
        MULTIPLAYER_MISSIONS = 1,
        /// <summary>The maps (Data\Terrain\*.fit).</summary>
        MAPS = 2,
    };

    /// <summary>
    /// Creates the dialog over <paramref name="database"/>, listing what <paramref name="mode"/> says; loads the
    /// list's icons (a solo and a multiplayer mission).
    /// </summary>
    /// <remarks>0x00426ea0.</remarks>
    SelectMissionDialog(DaoDatabase* database, int mode, CWnd* pParent = nullptr);

    /// <summary>The chosen mission's or map's name (+0x134); empty before one is chosen.</summary>
    CString selectedName;

protected:
    /// <summary>Binds OK, the picture and the list.</summary>
    /// <remarks>0x004270a0.</remarks>
    void DoDataExchange(CDataExchange* pDX) override;
    /// <summary>
    /// Fills the list: the maps, lower case without ".fit", or the missions of the mode (with their icon); ends the
    /// dialog with IDCANCEL, after a message, when there is nothing to list or the database fails.
    /// </summary>
    /// <remarks>0x004271d0.</remarks>
    BOOL OnInitDialog() override;
    /// <summary>Takes the selected item's name.</summary>
    /// <remarks>0x004270f0.</remarks>
    void OnOK() override;
    /// <summary>Reads the controls and cancels.</summary>
    /// <remarks>0x004271b0.</remarks>
    void OnCancel() override;

    /// <summary>The Delete key deletes the selected mission, as the Delete button does.</summary>
    afx_msg void OnKeydownMissionList(NMHDR* pNMHDR, LRESULT* pResult);

    /// <summary>Shows the item's map picture (Data\Terrain\name.tga) and enables OK.</summary>
    afx_msg void OnItemchangedMissionList(NMHDR* pNMHDR, LRESULT* pResult);

    /// <summary>Deletes the selected mission.</summary>
    afx_msg void OnDelete();

    /// <summary>Double-clicking an item shows it and chooses it.</summary>
    afx_msg void OnDblclkMissionList(NMHDR* pNMHDR, LRESULT* pResult);

    DECLARE_MESSAGE_MAP()

private:
    /// <summary>Takes the selected item's name into <see cref="selectedName"/>, if one is selected.</summary>
    void takeSelectedName();
    /// <summary>
    /// After asking, deletes the mission <see cref="selectedName"/>: its list item, its map, mission, campaign,
    /// briefing and save files, its warriors' brain scripts, and its records (the rest of its rows go with them).
    /// </summary>
    /// <remarks>0x00427930.</remarks>
    void deleteMission();

    /// <summary>The OK button (+0x5c).</summary>
    CButton _okButton;
    /// <summary>The map's picture (+0x98).</summary>
    MissionPreview _preview;
    /// <summary>The missions or maps (+0xf8).</summary>
    CListCtrl _list;
    /// <summary>The mission database (+0x138).</summary>
    DaoDatabase* _database = nullptr;
    /// <summary>What the list shows (+0x13c).</summary>
    int _mode = SOLO_MISSIONS;
    /// <summary>The list's icons: a solo mission (0) and a multiplayer one (1) (+0x140).</summary>
    CImageList _icons;
};
