#include "stdafx.h"
#include "MainFrame.h"
#include "DreditorDoc.h"
#include "DreditorView.h"
#include "MapFeaturesBar.h"
#include "TacticalMapDialog.h"
#include "ViewTools.h"
#include "resource.h"

namespace
{
    /// <summary>The window class registered for the frame; DreditorApp looks for it to find a running editor.</summary>
    constexpr char WINDOW_CLASS[] = "MCEDITORWND";

    /// <summary>The status bar's control ID: the seventh control-bar ID, not MFC's AFX_IDW_STATUS_BAR.</summary>
    constexpr UINT STATUS_BAR_ID = AFX_IDW_CONTROLBAR_FIRST + 6;

    /// <summary>The status bar's panes: the menu prompt, then the cursor's map coordinates and selection.</summary>
    constexpr UINT INDICATORS[] = {
        ID_SEPARATOR,        ID_INDICATOR_X,     ID_INDICATOR_Y,     ID_INDICATOR_Z,    ID_INDICATOR_BLOCK,
        ID_INDICATOR_VERTEX, ID_INDICATOR_DELTA, ID_INDICATOR_RANGE, ID_INDICATOR_AREA,
    };

    /// <summary>The tool bars' control ids: the main and standard bars and the Map Features palette.</summary>
    constexpr UINT MAIN_BAR_ID = 0x80;
    constexpr UINT STANDARD_BAR_ID = 0x81;
    constexpr UINT MAP_FEATURES_BAR_ID = IDS_MAP_FEATURES;

    /// <summary>The docked tool bars' style.</summary>
    constexpr DWORD DOCKED_BAR_STYLE = WS_CHILD | WS_VISIBLE | CBRS_TOP | CBRS_SIZE_DYNAMIC;
    /// <summary>The style a tool bar toggled on without a window is made with.</summary>
    constexpr DWORD TOGGLED_BAR_STYLE = WS_CHILD | WS_VISIBLE | CBRS_TOP;
    /// <summary>The Map Features palette's style.</summary>
    constexpr DWORD PALETTE_BAR_STYLE =
        WS_CHILD | WS_VISIBLE | CBRS_ALIGN_TOP | CBRS_BORDER_TOP | CBRS_SIZE_FIXED | CBRS_FLOATING;
    /// <summary>The palette floats this far left of the view's right edge.</summary>
    constexpr int PALETTE_OFFSET = 0x131;
    /// <summary>How far in from the desktop's edges a bar shown again must lie to stay where it was.</summary>
    constexpr int DESKTOP_MARGIN = 10;
    /// <summary>The registry section the bars' layout is kept under.</summary>
    constexpr char BAR_STATE_SECTION[] = "NewToolBars";
    /// <summary>The window title is string 0.</summary>
    constexpr UINT TITLE_STRING = 0;
}

IMPLEMENT_DYNCREATE(MainFrame, CFrameWnd)

MainFrame::MainFrame()
    : _standardBar(IDB_STANDARD_TOOLBAR, ID_VIEW_GRIDLINES, IDS_ASSIGN_A_SPECIFIC_HEIGHT)
    , _mainBar(IDB_MAIN_TOOLBAR, ID_TOOLS_SELECT, ID_TOOLS_REPAIR)
{
}

BOOL MainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
    // A copy of MFC's frame class under the editor's own name, so a second editor can find this one.
    if (cs.lpszClass != nullptr)
    {
        WNDCLASS windowClass;
        GetClassInfo(GetModuleHandle(nullptr), cs.lpszClass, &windowClass);
        windowClass.lpszClassName = WINDOW_CLASS;
        RegisterClass(&windowClass);
        cs.lpszClass = windowClass.lpszClassName;
    }

    if (!CFrameWnd::PreCreateWindow(cs))
    {
        return FALSE;
    }

    cs.style |= WS_MAXIMIZE;
    return TRUE;
}

void MainFrame::ActivateFrame(int nCmdShow)
{
    CFrameWnd::ActivateFrame(nCmdShow);
    CRect rect;
    ::GetWindowRect(m_hWnd, &rect);
    m_strTitle.LoadString(TITLE_STRING);
    CString title;
    title.LoadString(IDS_MAIN);

    if ((nCmdShow & SW_SHOW) != 0)
    {
        if (_mainBar.m_hWnd == nullptr)
        {
            _mainBar.Create(this, DOCKED_BAR_STYLE, MAIN_BAR_ID);
            _mainBar.SetBarStyle(_mainBar.GetBarStyle() | CBRS_TOOLTIPS | CBRS_FLYBY);
            _mainBar.EnableDocking(CBRS_ALIGN_ANY);
            _mainBar.SetWindowText(title);
            DockControlBar(&_mainBar, static_cast<UINT>(0), nullptr);
        }

        if (_standardBar.m_hWnd == nullptr)
        {
            title.LoadString(IDS_STANDARD);
            _standardBar.Create(this, DOCKED_BAR_STYLE, STANDARD_BAR_ID);
            RecalcLayout(TRUE);
            // Docked to the right of the main bar, on its dock bar.
            ::GetWindowRect(_mainBar.m_hWnd, &rect);
            rect.OffsetRect(1, 0);
            const DWORD style = _mainBar.GetBarStyle();
            UINT dockBar = 0;

            if ((style & CBRS_ALIGN_TOP) != 0)
            {
                dockBar = AFX_IDW_DOCKBAR_TOP;
            }

            if ((style & CBRS_ALIGN_BOTTOM) != 0 && dockBar == 0)
            {
                dockBar = AFX_IDW_DOCKBAR_BOTTOM;
            }

            if ((style & CBRS_ALIGN_LEFT) != 0 && dockBar == 0)
            {
                dockBar = AFX_IDW_DOCKBAR_LEFT;
            }

            if ((style & CBRS_ALIGN_RIGHT) != 0 && dockBar == 0)
            {
                dockBar = AFX_IDW_DOCKBAR_RIGHT;
            }

            _standardBar.SetBarStyle(_standardBar.GetBarStyle() | CBRS_TOOLTIPS | CBRS_FLYBY);
            _standardBar.EnableDocking(CBRS_ALIGN_ANY);
            _standardBar.SetWindowText(title);
            DockControlBar(&_standardBar, dockBar, &rect);
        }

        // The palette is made again each time, for the view's kind of map.
        mapFeaturesBar->multiplayer = multiplayer;
        mapFeaturesBar->DestroyWindow();

        if (mapFeaturesBar->m_hWnd == nullptr)
        {
            mapFeaturesBar->Create(this, PALETTE_BAR_STYLE, MAP_FEATURES_BAR_ID);
            mapFeaturesBar->EnableDocking(0);
            CView* view = GetActiveView();
            CRect viewRect;
            ::GetWindowRect(tacticalMap->m_hWnd, &viewRect);
            ::GetWindowRect(view->m_hWnd, &viewRect);
            CString name;
            name.LoadString(IDS_MAP_FEATURES);
            mapFeaturesBar->SetWindowText(name);
            FloatControlBar(mapFeaturesBar, CPoint(viewRect.right - PALETTE_OFFSET, viewRect.top), CBRS_ALIGN_TOP);
            mapFeaturesBar->SetBarStyle(mapFeaturesBar->GetBarStyle() | CBRS_TOOLTIPS | CBRS_FLYBY);
        }

        static_cast<DreditorView*>(GetActiveView())->statusBar = &_statusBar;
    }

    static_cast<DreditorView*>(GetActiveView())->mapFeaturesBar = mapFeaturesBar;
    LoadBarState(BAR_STATE_SECTION);
    ::InvalidateRect(m_hWnd, nullptr, TRUE);
}

BEGIN_MESSAGE_MAP(MainFrame, CFrameWnd)
ON_WM_CREATE()
ON_COMMAND(ID_VIEW_MAP_FEATURES_PALETTE, &MainFrame::OnViewMapFeaturesPalette)
ON_UPDATE_COMMAND_UI(ID_VIEW_MAP_FEATURES_PALETTE, &MainFrame::OnUpdateViewMapFeaturesPalette)
ON_WM_CLOSE()
ON_WM_QUERYNEWPALETTE()
ON_COMMAND(ID_FILE_OPEN, &MainFrame::OnFileOpen)
ON_COMMAND(ID_VIEW_STANDARD_TOOLBAR, &MainFrame::OnViewStandardToolbar)
ON_UPDATE_COMMAND_UI(ID_VIEW_STANDARD_TOOLBAR, &MainFrame::OnUpdateViewStandardToolbar)
ON_COMMAND(ID_VIEW_MAIN_TOOLBAR, &MainFrame::OnViewMainToolbar)
ON_UPDATE_COMMAND_UI(ID_VIEW_MAIN_TOOLBAR, &MainFrame::OnUpdateViewMainToolbar)
ON_WM_SETTINGCHANGE()
ON_MESSAGE(WM_EDITOR_SELECT_OBJECT, &MainFrame::OnSelectObject)
ON_MESSAGE(WM_IDLEUPDATECMDUI, &MainFrame::OnIdleupdatecmdui)
ON_MESSAGE(WM_EDITOR_SCROLL_VIEW, &MainFrame::OnScrollView)
END_MESSAGE_MAP()

int MainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
    {
        return -1;
    }

    if (!_statusBar.Create(this, WS_CHILD | WS_VISIBLE | CBRS_BOTTOM, STATUS_BAR_ID) ||
        !_statusBar.SetIndicators(INDICATORS, static_cast<int>(std::size(INDICATORS))))
    {
        return -1;
    }

    if (mapFeaturesBar == nullptr)
    {
        mapFeaturesBar = new MapFeaturesBar(false);
    }

    EnableDocking(CBRS_ALIGN_ANY | CBRS_FLOAT_MULTI);
    return 0;
}

void MainFrame::toggleBar(PaletteBar& bar)
{
    if (bar.m_hWnd == nullptr)
    {
        bar.Create(this, TOGGLED_BAR_STYLE, AFX_IDW_TOOLBAR);
        DockControlBar(&bar, static_cast<UINT>(0), nullptr);
        return;
    }

    if (bar.IsVisible())
    {
        ShowControlBar(&bar, FALSE, FALSE);
        RecalcLayout(TRUE);
        return;
    }

    ShowControlBar(&bar, TRUE, FALSE);
    const POINT lastPlace = bar.hiddenRect.TopLeft();
    CRect desktop;
    ::GetWindowRect(::GetDesktopWindow(), &desktop);
    desktop.DeflateRect(DESKTOP_MARGIN, DESKTOP_MARGIN, DESKTOP_MARGIN, DESKTOP_MARGIN);

    if (!desktop.PtInRect(lastPlace))
    {
        FloatControlBar(&bar, CPoint((desktop.left + desktop.right) / 2, (desktop.top + desktop.bottom) / 2), 0);
        bar.ShowWindow(SW_SHOW);
    }

    RecalcLayout(TRUE);
}

void MainFrame::OnViewMapFeaturesPalette()
{
    toggleBar(*mapFeaturesBar);
}

void MainFrame::OnUpdateViewMapFeaturesPalette(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(mapFeaturesBar->IsVisible());
}

void MainFrame::OnClose()
{
    SaveBarState(BAR_STATE_SECTION);
    GetActiveView();
    const BOOL visible = ::IsWindowVisible(tacticalMap->m_hWnd);
    AfxGetApp()->WriteProfileInt("TacMapLoc", "Visible", visible);
    CFrameWnd::OnClose();
}

BOOL MainFrame::OnQueryNewPalette()
{
    SendMessageToDescendants(WM_QUERYNEWPALETTE, 0, 0, TRUE, FALSE);
    return CFrameWnd::OnQueryNewPalette();
}

void MainFrame::OnFileOpen()
{
    // Both views take ID_FILE_OPEN themselves, so this runs only when neither is active (and has no view to ask).
    static_cast<DreditorDoc*>(GetActiveView()->GetDocument())->openMissionByMap();
}

void MainFrame::attachView(DreditorView* view, bool multiplayer)
{
    view->statusBar = &_statusBar;
    view->mapFeaturesBar = mapFeaturesBar;
    this->multiplayer = multiplayer;
}

void MainFrame::OnViewStandardToolbar()
{
    toggleBar(_standardBar);
}

void MainFrame::OnUpdateViewStandardToolbar(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(_standardBar.IsVisible());
}

void MainFrame::OnViewMainToolbar()
{
    toggleBar(_mainBar);
}

void MainFrame::OnUpdateViewMainToolbar(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(_mainBar.IsVisible());
}

void MainFrame::OnSettingChange(UINT uFlags, LPCTSTR lpszSection)
{
    CFrameWnd::OnSettingChange(uFlags, lpszSection);

    if (uFlags != SPI_SETNONCLIENTMETRICS)
    {
        return;
    }

    // The palette again where it was.
    CRect rect;
    ::GetWindowRect(mapFeaturesBar->m_hWnd, &rect);
    mapFeaturesBar->DestroyWindow();
    mapFeaturesBar->Create(this, PALETTE_BAR_STYLE, MAP_FEATURES_BAR_ID);
    mapFeaturesBar->EnableDocking(0);
    CString name;
    name.LoadString(IDS_MAP_FEATURES);
    mapFeaturesBar->SetWindowText(name);
    FloatControlBar(mapFeaturesBar, CPoint(rect.left, rect.top), CBRS_ALIGN_TOP);
    mapFeaturesBar->SetBarStyle(mapFeaturesBar->GetBarStyle() | CBRS_TOOLTIPS | CBRS_FLYBY);
    auto* view = static_cast<DreditorView*>(GetActiveView());
    view->mapFeaturesBar = mapFeaturesBar;
    // The tactical map again where it was, for this view and its document.
    ::GetWindowRect(tacticalMap->m_hWnd, &rect);
    tacticalMap->DestroyWindow();
    delete tacticalMap;
    tacticalMap = new TacticalMapDialog(nullptr);
    tacticalMap->setParentView(view);
    tacticalMap->document = static_cast<DreditorDoc*>(view->GetDocument());
    tacticalMap->Create(TacticalMapDialog::IDD, nullptr);
    tacticalMap->SetWindowPos(nullptr, rect.left, rect.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    ::InvalidateRect(tacticalMap->m_hWnd, nullptr, TRUE);
}

LRESULT MainFrame::OnSelectObject(WPARAM wParam, LPARAM lParam)
{
    if (CView* view = GetActiveView(); view != nullptr)
    {
        ::SendMessage(view->m_hWnd, WM_EDITOR_SELECT_OBJECT, wParam, lParam);
    }

    RecalcLayout(TRUE);
    CWnd::FromHandle(::SetActiveWindow(m_hWnd));
    return 0;
}

LRESULT MainFrame::OnIdleupdatecmdui(WPARAM wParam, LPARAM lParam)
{
    SendMessageToDescendants(WM_IDLEUPDATECMDUI, TRUE, 0, TRUE, FALSE);
    CFrameWnd::OnIdleUpdateCmdUI();
    return 0;
}

LRESULT MainFrame::OnScrollView(WPARAM wParam, LPARAM lParam)
{
    if (CView* view = GetActiveView(); view != nullptr)
    {
        ::SendMessage(view->m_hWnd, WM_EDITOR_SCROLL_VIEW, wParam, lParam);
    }

    return 0;
}
