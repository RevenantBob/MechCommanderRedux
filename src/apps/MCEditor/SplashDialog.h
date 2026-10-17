#pragma once

/// <summary>
/// The splash screen: IDB_SPLASH in a borderless popup. Shown at startup and as a "please wait" screen around long
/// operations; any key or click dismisses it.
/// </summary>
/// <remarks>Visual C++'s Component Gallery splash (CSplashWnd), built on the empty IDD_SPLASH_DIALOG template.</remarks>
class SplashDialog : public CDialog
{
public:
    /// <summary>The dialog template.</summary>
    enum
    {
        IDD = IDD_SPLASH_DIALOG
    };

    /// <summary>Builds the splash palette from IDB_SPLASH's colour table.</summary>
    SplashDialog();

    /// <summary>Frees the palette and bitmap; the splash is gone.</summary>
    ~SplashDialog() override;

    /// <summary>Allows or forbids the splash screen (the command line's /nologo turns it off).</summary>
    static void EnableSplashScreen(BOOL bEnable = TRUE);

    /// <summary>Shows the splash screen unless it's disabled or already up.</summary>
    static void ShowSplashScreen(CWnd* pParentWnd = nullptr);

    /// <summary>Closes the splash screen if it's up.</summary>
    static void HideSplashScreen();

    /// <summary>Closes the splash screen on any key or click; returns whether the message was used up.</summary>
    static BOOL PreTranslateAppMessage(MSG* pMsg);

protected:
    /// <summary>Creates the popup window.</summary>
    BOOL Create(CWnd* pParentWnd = nullptr);

    /// <summary>Deletes the object once its window is gone.</summary>
    void PostNcDestroy() override;

    /// <summary>Handles ON_WM_CREATE().</summary>
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);

    /// <summary>Handles ON_WM_PAINT().</summary>
    afx_msg void OnPaint();

    /// <summary>Handles ON_WM_TIMER().</summary>
    afx_msg void OnTimer(UINT_PTR nIDEvent);

    /// <summary>Handles ON_WM_QUERYNEWPALETTE().</summary>
    afx_msg BOOL OnQueryNewPalette();

    DECLARE_MESSAGE_MAP()

private:
    /// <summary>Whether the splash screen may be shown.</summary>
    static BOOL c_bShowSplashWnd;

    /// <summary>The splash screen while it's up.</summary>
    static SplashDialog* c_pSplashWnd;

    /// <summary>IDB_SPLASH, loaded on the first paint.</summary>
    HBITMAP _bitmap = nullptr;

    /// <summary>The bitmap's palette, created on the first paint.</summary>
    CPalette _palette;

    /// <summary>The palette's entries, copied from the bitmap's colour table.</summary>
    LOGPALETTE* _logPalette = nullptr;
};
