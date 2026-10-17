#pragma once

/// <summary>
/// A new map's settings: the locale (Port Arthur, or Cermak with the GTiles terrain set), the base terrain, and the
/// water, forest and hill coverage in percent (water and forest, and water and hills, add up to at most 100).
/// </summary>
class NewMapDialog : public CDialog
{
public:
    /// <summary>The dialog template.</summary>
    enum
    {
        IDD = IDD_NEW_MAP_DIALOG
    };

    /// <summary>Creates the dialog: Port Arthur, grass, no coverage.</summary>
    /// <remarks>0x00424990.</remarks>
    explicit NewMapDialog(CWnd* pParent = nullptr);

    /// <summary>The locale: 0 Port Arthur, 1 Cermak (GTiles) (+0x110).</summary>
    int locale = 0;
    /// <summary>The base terrain: 0 grass (sand in Cermak), 1 dirt (ash) (+0x114).</summary>
    int terrain = 0;
    /// <summary>The water coverage, 0-100 (+0x118).</summary>
    int waterCoverage = 0;
    /// <summary>The forest coverage, 0-100 (+0x11c).</summary>
    int forestCoverage = 0;
    /// <summary>The hill coverage, 0-100 (+0x120).</summary>
    int hillCoverage = 0;
    /// <summary>The map's size: 6 + 3 times this many blocks a side (+0x124); nothing sets it, so 0.</summary>
    int mapSize = 0;
    /// <summary>Not used (+0x128).</summary>
    CString name;

protected:
    /// <summary>Binds the sliders, the locale and the terrain.</summary>
    /// <remarks>0x00424ae0.</remarks>
    void DoDataExchange(CDataExchange* pDX) override;
    /// <summary>
    /// Titles the window with the editor's name (string 0), takes its caption bar away and gives the sliders 0-100.
    /// </summary>
    /// <remarks>0x00424c20.</remarks>
    BOOL OnInitDialog() override;
    /// <summary>
    /// Reads the settings and switches the tile library and the object library to the locale's terrain set (the
    /// object library reloads on its next load when that changed).
    /// </summary>
    /// <remarks>0x00424b60.</remarks>
    void OnOK() override;

    /// <summary>Keeps water and forest, and water and hills, at most 100 together, moving the other slider.</summary>
    afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);

    /// <summary>Names the terrain buttons for the locale chosen: grass and dirt, or sand and ash.</summary>
    afx_msg void OnPortArthur();

    /// <summary>The same as <see cref="OnPortArthur"/>.</summary>
    afx_msg void OnCermak();

    DECLARE_MESSAGE_MAP()

private:
    /// <summary>Reads the three sliders' positions.</summary>
    void readSliders();

    /// <summary>The hill coverage slider (+0x5c).</summary>
    CSliderCtrl _hillSlider;
    /// <summary>The forest coverage slider (+0x98).</summary>
    CSliderCtrl _forestSlider;
    /// <summary>The water coverage slider (+0xd4).</summary>
    CSliderCtrl _waterSlider;
};
