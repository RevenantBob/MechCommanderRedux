#pragma once

class DreditorDoc;

/// <summary>The view message that centres the view on a tactical map point: wParam 1, lParam the point.</summary>
inline constexpr UINT WM_EDITOR_CENTRE_ON_TACTICAL = WM_USER + 259;

/// <summary>
/// The IDD_TACTICAL_MAP_DIALOG dialog: the whole map in small (<see cref="EditorMap::drawTacticalMap"/>) with the
/// part the view shows outlined; a click there scrolls the view to it. The view keeps it in <c>tacticalMap</c>.
/// </summary>
/// <remarks>0xac bytes, virtual table 0x004fc9e0.</remarks>
class TacticalMapDialog : public CDialog
{
public:
    /// <summary>The dialog template.</summary>
    enum
    {
        IDD = IDD_TACTICAL_MAP_DIALOG
    };

    /// <summary>Creates the object, with the 8-bit header of its bitmap; the window is made later.</summary>
    /// <remarks>0x004265a0.</remarks>
    explicit TacticalMapDialog(CWnd* pParent = nullptr);
    /// <summary>Frees the bitmap, its DC and header.</summary>
    /// <remarks>0x00426730.</remarks>
    ~TacticalMapDialog() override;

    /// <summary>
    /// Takes <paramref name="rect"/> (tactical map pixels, its top-left moved 2 up and left) as the part of the map the
    /// view shows, keeping the last one, and redraws the window if there is one.
    /// </summary>
    /// <remarks>0x0041b130.</remarks>
    void setViewRect(const RECT& rect);
    /// <summary>Makes <paramref name="view"/> the window the tactical map reports to (its parent, +0x50).</summary>
    /// <remarks>Inlined in <c>DreditorApp::SwitchView</c>.</remarks>
    void setParentView(CWnd* view);

    /// <summary>The document whose map it shows (+0x5c).</summary>
    DreditorDoc* document = nullptr;
    /// <summary>The part of the map the view shows (+0x60).</summary>
    RECT viewRect = {};
    /// <summary>The one before (+0x70).</summary>
    RECT previousViewRect = {};
    /// <summary>The view rectangle changed since the last paint (+0x98): the paint moves its outline only.</summary>
    bool viewRectChanged = false;

protected:
    /// <summary>Copies the editor's palette into the bitmap's header and shows the window if the settings say so.</summary>
    /// <remarks>Virtual table slot 47 (0x00426bf0).</remarks>
    BOOL OnInitDialog() override;

    /// <summary>
    /// Handles ON_WM_PAINT(): after a scroll only moves the outline; else sizes the window to the map (a third of a
    /// tile a pixel, a multiple of 4 across and down), draws the map into its bitmap and the outline over it.
    /// </summary>
    /// <remarks>0x00426810.</remarks>
    afx_msg void OnPaint();

    /// <summary>Handles ON_WM_LBUTTONDOWN(): scrolls the view so that the outline is centred on the click.</summary>
    /// <remarks>0x00426b60.</remarks>
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);

    /// <summary>Handles ON_WM_ERASEBKGND(): nothing to erase; the next paint draws everything.</summary>
    /// <remarks>0x00426c60.</remarks>
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);

    /// <summary>Handles ON_WM_WINDOWPOSCHANGED(): gives the view the focus back.</summary>
    /// <remarks>0x00426c70.</remarks>
    afx_msg void OnWindowPosChanged(WINDOWPOS* lpwndpos);

    /// <summary>Handles ON_WM_MOVE(): keeps where it is in the settings (TacMapLoc) while it is shown.</summary>
    /// <remarks>0x00426c90.</remarks>
    afx_msg void OnMove(int x, int y);

    /// <summary>Handles ON_WM_QUERYNEWPALETTE(): realizes the editor's palette.</summary>
    /// <remarks>0x00426d00.</remarks>
    afx_msg BOOL OnQueryNewPalette();

    /// <summary>The bitmap's header and colours (+0x80).</summary>
    BITMAPINFO* bitmapInfo;
    /// <summary>The bitmap's size, the window's client size (+0x84, +0x88); 0 until the first paint.</summary>
    int32_t bitsWidth = 0;
    int32_t bitsHeight = 0;
    /// <summary>The DC the bitmap is drawn from (+0x8c).</summary>
    CDC* memoryDC = nullptr;
    /// <summary>The bitmap (+0x90) and its pixels (+0x94).</summary>
    HBITMAP dib = nullptr;
    uint8_t* bits = nullptr;
    /// <summary>The editor's palette (+0x9c).</summary>
    CPalette palette;
    /// <summary>The outline's brush, cyan (+0xa4).</summary>
    CBrush outlineBrush;

    DECLARE_MESSAGE_MAP()
};
