#include "stdafx.h"
#include "resource.h"
#include "SplashDialog.h"

namespace
{
    /// <summary>IDB_SPLASH's size.</summary>
    constexpr int SPLASH_WIDTH = 500;
    constexpr int SPLASH_HEIGHT = 330;

    /// <summary>The popup's size; ten pixels wider than the bitmap, as in the original.</summary>
    constexpr int WINDOW_WIDTH = 510;
    constexpr int WINDOW_HEIGHT = 330;

    /// <summary>IDB_SPLASH is an 8-bit bitmap: a 256-colour table follows its header.</summary>
    constexpr WORD PALETTE_SIZE = 256;

    /// <summary>LOGPALETTE's version for Windows 3.0 and later.</summary>
    constexpr WORD PALETTE_VERSION = 0x300;
}

BOOL SplashDialog::c_bShowSplashWnd = FALSE;
SplashDialog* SplashDialog::c_pSplashWnd = nullptr;

SplashDialog::SplashDialog() : CDialog(IDD, nullptr)
{
    HMODULE module = GetModuleHandle(nullptr);
    HRSRC resource = FindResource(module, MAKEINTRESOURCE(IDB_SPLASH), RT_BITMAP);
    auto header = static_cast<const BITMAPINFOHEADER*>(LockResource(LoadResource(module, resource)));
    auto colors = reinterpret_cast<const RGBQUAD*>(header + 1);

    _logPalette =
        static_cast<LOGPALETTE*>(operator new(sizeof(LOGPALETTE) + (PALETTE_SIZE - 1) * sizeof(PALETTEENTRY)));
    _logPalette->palVersion = PALETTE_VERSION;
    _logPalette->palNumEntries = PALETTE_SIZE;

    for (WORD i = 0; i < PALETTE_SIZE; ++i)
    {
        _logPalette->palPalEntry[i].peRed = colors[i].rgbRed;
        _logPalette->palPalEntry[i].peGreen = colors[i].rgbGreen;
        _logPalette->palPalEntry[i].peBlue = colors[i].rgbBlue;
        _logPalette->palPalEntry[i].peFlags = 0;
    }
}

SplashDialog::~SplashDialog()
{
    c_pSplashWnd = nullptr;
    operator delete(_logPalette);
    DeleteObject(_bitmap);
}

BEGIN_MESSAGE_MAP(SplashDialog, CDialog)
ON_WM_CREATE()
ON_WM_PAINT()
ON_WM_TIMER()
ON_WM_QUERYNEWPALETTE()
END_MESSAGE_MAP()

void SplashDialog::EnableSplashScreen(BOOL bEnable)
{
    c_bShowSplashWnd = bEnable;
}

void SplashDialog::ShowSplashScreen(CWnd* pParentWnd)
{
    if (!c_bShowSplashWnd || c_pSplashWnd != nullptr)
    {
        return;
    }

    c_pSplashWnd = new SplashDialog;

    if (!c_pSplashWnd->Create(pParentWnd))
    {
        delete c_pSplashWnd;
    }
    else
    {
        c_pSplashWnd->UpdateWindow();
    }
}

void SplashDialog::HideSplashScreen()
{
    if (c_pSplashWnd != nullptr && c_pSplashWnd->m_hWnd != nullptr)
    {
        c_pSplashWnd->DestroyWindow();
    }
}

BOOL SplashDialog::PreTranslateAppMessage(MSG* pMsg)
{
    if (c_pSplashWnd == nullptr)
    {
        return FALSE;
    }

    switch (pMsg->message)
    {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_NCLBUTTONDOWN:
        case WM_NCRBUTTONDOWN:
        case WM_NCMBUTTONDOWN:
        {
            HideSplashScreen();
            return TRUE;
        }

        default:
            return FALSE;
    }
}

BOOL SplashDialog::Create(CWnd* pParentWnd)
{
    // The parent goes unused: the original always created the splash without an owner.
    return CreateEx(0, AfxRegisterWndClass(0, ::LoadCursor(nullptr, IDC_ARROW)), nullptr, WS_POPUP | WS_VISIBLE, 0, 0,
                    WINDOW_WIDTH, WINDOW_HEIGHT, nullptr, nullptr);
}

void SplashDialog::PostNcDestroy()
{
    delete this;
}

int SplashDialog::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CDialog::OnCreate(lpCreateStruct) == -1)
    {
        return -1;
    }

    CenterWindow();
    return 0;
}

void SplashDialog::OnPaint()
{
    CPaintDC dc(this);

    if (_palette.GetSafeHandle() == nullptr)
    {
        _palette.Attach(CreatePalette(_logPalette));
    }

    dc.SelectPalette(&_palette, FALSE);
    dc.RealizePalette();

    CDC imageDC;

    if (imageDC.CreateCompatibleDC(&dc))
    {
        if (_bitmap == nullptr)
        {
            _bitmap = static_cast<HBITMAP>(LoadImage(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDB_SPLASH),
                                                     IMAGE_BITMAP, SPLASH_WIDTH, SPLASH_HEIGHT, LR_CREATEDIBSECTION));
        }

        imageDC.SelectObject(_bitmap);
        dc.BitBlt(0, 0, SPLASH_WIDTH, SPLASH_HEIGHT, &imageDC, 0, 0, SRCCOPY);
    }
}

void SplashDialog::OnTimer(UINT_PTR nIDEvent)
{
    // Empty in the original: the splash stays until a key, a click or HideSplashScreen.
}

BOOL SplashDialog::OnQueryNewPalette()
{
    // Original behaviour (EB-001): the DC is never released.
    CDC* pDC = CDC::FromHandle(::GetDC(m_hWnd));
    pDC->SelectPalette(&_palette, FALSE);
    pDC->RealizePalette();
    return TRUE;
}
