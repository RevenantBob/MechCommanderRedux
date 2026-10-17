#pragma once

/// <summary>
/// File > Open's first question: the map with its mission data or the map only. It ends with the button's id
/// (IDC_LOAD_MAP_WITH_MISSION_DATA, IDC_LOAD_MAP_ONLY) or IDCANCEL.
/// </summary>
/// <remarks>Constructor 0x00423d90.</remarks>
class OpenDialog : public CDialog
{
public:
    /// <summary>The dialog template.</summary>
    enum
    {
        IDD = IDD_OPEN_DIALOG
    };

    /// <summary>Creates the dialog.</summary>
    explicit OpenDialog(CWnd* pParent = nullptr);

protected:
    /// <summary>Handles ON_BN_CLICKED(IDC_LOAD_MAP_ONLY).</summary>
    afx_msg void OnLoadMapOnly();

    /// <summary>Handles ON_BN_CLICKED(IDC_LOAD_MAP_WITH_MISSION_DATA).</summary>
    afx_msg void OnLoadMapWithMissionData();

    DECLARE_MESSAGE_MAP()
};
