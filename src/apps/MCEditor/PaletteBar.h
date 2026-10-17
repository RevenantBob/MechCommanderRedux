#pragma once

// The base of the editor's tool bars (0x00430e90-0x004313b0, virtual table 0x004fdd60): the main and standard tool
// bars and the Map Features palette. Its source file name wasn't kept; the names are the port's.

/// <summary>
/// The DIB header the palettes render their thumbnails with (0x007884b4): the editor palette's, set by every
/// <see cref="PaletteBar"/> constructor.
/// </summary>
extern BITMAPINFO* paletteBitmapInfo;

/// <summary>
/// A tool bar drawn in the editor's palette, whose tool tips come from the command's string (or
/// <see cref="getButtonText"/>), and which remembers where it was when hidden.
/// </summary>
/// <remarks>0x1c4 bytes with the CToolBar part (0x9c).</remarks>
class PaletteBar : public CToolBar
{
public:
    /// <summary>A bar not yet created.</summary>
    PaletteBar();
    /// <summary>Frees the palette.</summary>
    ~PaletteBar() override;

    /// <summary>The tool tip of button <paramref name="id"/>, or null for the command's string.</summary>
    /// <remarks>Virtual table slot 57; null here.</remarks>
    virtual const char* getButtonText(UINT id);

    /// <summary>The window rectangle when the bar was last hidden (+0x9c).</summary>
    CRect hiddenRect = CRect(0, 0, 0, 0);
    /// <summary>Whether the bar was docked when last hidden (+0xac); -1 until then.</summary>
    int32_t hiddenDocked = -1;

protected:
    /// <summary>
    /// A child's NM_CUSTOMDRAW (tested against the control id, not the code: EB-076) selects and realizes the palette
    /// before it paints and selects the old one back after; anything else goes to CWnd.
    /// </summary>
    BOOL OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult) override;

    /// <summary>Handles ON_WM_WINDOWPOSCHANGING(): records the rectangle and docking of a bar being hidden.</summary>
    afx_msg void OnWindowPosChanging(WINDOWPOS* lpwndpos);
    /// <summary>Handles ON_WM_DRAWITEM().</summary>
    afx_msg void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct);
    /// <summary>
    /// Handles ON_WM_SETCURSOR(): with the left button down on a docked bar's caption, border or first button,
    /// moves the focus to the dock bar and back to the main window.
    /// </summary>
    afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
    /// <summary>
    /// Handles ON_WM_CAPTURECHANGED(): when a docked bar loses the capture with the cursor on the caption above it,
    /// moves the focus to the dock bar and back to the main window.
    /// </summary>
    afx_msg void OnCaptureChanged(CWnd* pWnd);
    /// <summary>Handles ON_WM_SIZE().</summary>
    afx_msg void OnSize(UINT nType, int cx, int cy);
    /// <summary>
    /// Handles ON_NOTIFY_EX_RANGE(TTN_NEEDTEXTA, 0, 0xffff): the tip is <see cref="getButtonText"/> or the second part
    /// of the command's string, and the tip window goes on top.
    /// </summary>
    afx_msg BOOL OnToolTipText(UINT id, NMHDR* pNMHDR, LRESULT* pResult);
    /// <summary>Handles ON_NOTIFY_REFLECT_EX(NM_CUSTOMDRAW): the palette around the bar's painting.</summary>
    afx_msg BOOL OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult);

    DECLARE_MESSAGE_MAP()

    /// <summary>
    /// Selects and realizes the palette (made from the editor's on first use) for a custom draw's prepaint stage and
    /// selects the old one back for its postpaint stage.
    /// </summary>
    /// <returns>The value the original returns for the stage (CDRF_NOTIFYPOSTPAINT for the prepaint).</returns>
    LRESULT selectPalette(const NMCUSTOMDRAW& draw);

    /// <summary>The editor's palette as a GDI palette (+0xb0).</summary>
    CPalette palette;
    /// <summary>The palette a custom draw replaced (+0xb8).</summary>
    HPALETTE oldPalette = nullptr;
};
