#pragma once

#include "ToolButtonBar.h"

class DreditorView;
class MapFeaturesBar;

/// <summary>The editor's main window.</summary>
/// <remarks>0x510 bytes.</remarks>
class MainFrame : public CFrameWnd
{
protected:
    /// <summary>
    /// Creates the object from serialization or a document template: the standard and main tool bars (not yet
    /// created) and no Map Features palette.
    /// </summary>
    /// <remarks>0x00423e20.</remarks>
    MainFrame();
    DECLARE_DYNCREATE(MainFrame)

public:
    /// <summary>Registers the MCEDITORWND window class and opens maximized.</summary>
    BOOL PreCreateWindow(CREATESTRUCT& cs) override;

    /// <summary>
    /// Shows the frame; when shown, titles it from string 0, builds and docks the main and standard tool bars the
    /// first time, and makes the Map Features palette again (floating at the view's top right), then restores the
    /// bars' layout ("NewToolBars").
    /// </summary>
    /// <remarks>Virtual table slot 51 (0x00424230).</remarks>
    void ActivateFrame(int nCmdShow = -1) override;

    /// <summary>
    /// Makes <paramref name="view"/> the one the frame's bars work for, the multiplayer one or not: it gets the status
    /// bar and the Map Features palette.
    /// </summary>
    /// <remarks>0x00424500.</remarks>
    void attachView(DreditorView* view, bool multiplayer);
    /// <summary>
    /// Shows or hides <paramref name="bar"/> (creating and docking it when it has no window); a bar shown again whose
    /// last place is off the desktop floats in the desktop's centre.
    /// </summary>
    /// <remarks>0x00424540.</remarks>
    void toggleBar(PaletteBar& bar);

    /// <summary>The Map Features palette (+0x138), made by <see cref="OnCreate"/>.</summary>
    MapFeaturesBar* mapFeaturesBar = nullptr;
    /// <summary>The multiplayer view is shown (+0x50c).</summary>
    bool multiplayer = true;

protected:
    /// <summary>
    /// Handles ON_WM_CREATE(): the status bar, the Map Features palette's object, and docking on every side.
    /// </summary>
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);

    /// <summary>Handles ON_COMMAND(ID_VIEW_MAP_FEATURES_PALETTE).</summary>
    afx_msg void OnViewMapFeaturesPalette();

    /// <summary>Handles ON_UPDATE_COMMAND_UI(ID_VIEW_MAP_FEATURES_PALETTE).</summary>
    afx_msg void OnUpdateViewMapFeaturesPalette(CCmdUI* pCmdUI);

    /// <summary>
    /// Handles ON_WM_CLOSE(): saves the bars' layout and whether the tactical map is shown, then closes.
    /// </summary>
    afx_msg void OnClose();

    /// <summary>Handles ON_WM_QUERYNEWPALETTE(): passes it to every child window first.</summary>
    afx_msg BOOL OnQueryNewPalette();

    /// <summary>Handles ON_COMMAND(ID_FILE_OPEN).</summary>
    afx_msg void OnFileOpen();

    /// <summary>Handles ON_COMMAND(ID_VIEW_STANDARD_TOOLBAR).</summary>
    afx_msg void OnViewStandardToolbar();

    /// <summary>Handles ON_UPDATE_COMMAND_UI(ID_VIEW_STANDARD_TOOLBAR).</summary>
    afx_msg void OnUpdateViewStandardToolbar(CCmdUI* pCmdUI);

    /// <summary>Handles ON_COMMAND(ID_VIEW_MAIN_TOOLBAR).</summary>
    afx_msg void OnViewMainToolbar();

    /// <summary>Handles ON_UPDATE_COMMAND_UI(ID_VIEW_MAIN_TOOLBAR).</summary>
    afx_msg void OnUpdateViewMainToolbar(CCmdUI* pCmdUI);

    /// <summary>
    /// Handles ON_WM_SETTINGCHANGE(): after a change of the non-client metrics, makes the Map Features palette and the
    /// tactical map again where they were.
    /// </summary>
    afx_msg void OnSettingChange(UINT uFlags, LPCTSTR lpszSection);

    /// <summary>Handles ON_MESSAGE(WM_EDITOR_SELECT_OBJECT): passes it to the view, lays the bars out and activates the frame.</summary>
    afx_msg LRESULT OnSelectObject(WPARAM wParam, LPARAM lParam);

    /// <summary>Handles ON_MESSAGE(WM_IDLEUPDATECMDUI): passes it to every child window first.</summary>
    afx_msg LRESULT OnIdleupdatecmdui(WPARAM wParam, LPARAM lParam);

    /// <summary>Handles ON_MESSAGE(WM_EDITOR_SCROLL_VIEW): passes it to the view.</summary>
    afx_msg LRESULT OnScrollView(WPARAM wParam, LPARAM lParam);

    DECLARE_MESSAGE_MAP()

private:
    /// <summary>The status bar: menu prompts and the cursor's map coordinates (+0xbc).</summary>
    CStatusBar _statusBar;
    /// <summary>The standard tool bar: gridlines, undo, redo, set height (+0x13c).</summary>
    ToolButtonBar _standardBar;
    /// <summary>The main tool bar: the tools (+0x324).</summary>
    ToolButtonBar _mainBar;
};
