#include "stdafx.h"
#include "PaletteBar.h"
#include "EditorPalette.h"

BITMAPINFO* paletteBitmapInfo = nullptr;

namespace
{
    /// <summary>The longest tool tip the bars give (TOOLTIPTEXT's szText).</summary>
    inline constexpr int TOOL_TIP_LENGTH = 0x50;
    /// <summary>The longest command string a tool tip is cut from.</summary>
    inline constexpr int COMMAND_STRING_LENGTH = 0x100;
    /// <summary>A key's "down" bit as GetAsyncKeyState's short, sign-extended, is tested at bit 28.</summary>
    inline constexpr int KEY_DOWN_BIT = 0x10000000;
}

PaletteBar::PaletteBar()
{
    paletteBitmapInfo = editorPalette->bitmapInfo;
}

PaletteBar::~PaletteBar() = default;

const char* PaletteBar::getButtonText([[maybe_unused]] UINT id)
{
    return nullptr;
}

BEGIN_MESSAGE_MAP(PaletteBar, CToolBar)
ON_WM_WINDOWPOSCHANGING()
ON_WM_DRAWITEM()
ON_WM_SETCURSOR()
ON_WM_CAPTURECHANGED()
ON_WM_SIZE()
ON_NOTIFY_EX_RANGE(TTN_NEEDTEXTA, 0, 0xffff, &PaletteBar::OnToolTipText)
ON_NOTIFY_REFLECT_EX(NM_CUSTOMDRAW, &PaletteBar::OnCustomDraw)
END_MESSAGE_MAP()

LRESULT PaletteBar::selectPalette(const NMCUSTOMDRAW& draw)
{
    if (palette.m_hObject == nullptr)
    {
        palette.Attach(CreatePalette(editorPalette->gammaPalette));
    }

    if (draw.dwDrawStage == CDDS_PREPAINT)
    {
        oldPalette = SelectPalette(draw.hdc, static_cast<HPALETTE>(palette.m_hObject), FALSE);
        RealizePalette(draw.hdc);
        return CDRF_NOTIFYPOSTPAINT;
    }

    if (draw.dwDrawStage == CDDS_POSTPAINT)
    {
        SelectPalette(draw.hdc, oldPalette, FALSE);
    }

    return CDRF_NOTIFYITEMDRAW;
}

BOOL PaletteBar::OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult)
{
    // Original behaviour (EB-076): the control id is compared with NM_CUSTOMDRAW, so this never happens.
    if (static_cast<int>(wParam) == NM_CUSTOMDRAW)
    {
        const auto& draw = *reinterpret_cast<const NMCUSTOMDRAW*>(lParam);

        if (draw.dwDrawStage == CDDS_PREPAINT)
        {
            *pResult = selectPalette(draw);
            return static_cast<BOOL>(*pResult);
        }

        if (draw.dwDrawStage == CDDS_POSTPAINT)
        {
            selectPalette(draw);
            return TRUE;
        }

        if (palette.m_hObject == nullptr)
        {
            palette.Attach(CreatePalette(editorPalette->gammaPalette));
        }
    }

    return CWnd::OnNotify(wParam, lParam, pResult);
}

void PaletteBar::OnWindowPosChanging(WINDOWPOS* lpwndpos)
{
    CToolBar::OnWindowPosChanging(lpwndpos);

    if ((lpwndpos->flags & SWP_HIDEWINDOW) != 0)
    {
        hiddenDocked = IsFloating() ? 0 : 1;
        GetWindowRect(&hiddenRect);
    }
}

void PaletteBar::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct)
{
    CToolBar::OnDrawItem(nIDCtl, lpDrawItemStruct);
}

BOOL PaletteBar::OnSetCursor([[maybe_unused]] CWnd* pWnd, UINT nHitTest, [[maybe_unused]] UINT message)
{
    const int keyState = GetAsyncKeyState(VK_LBUTTON);

    if ((keyState & KEY_DOWN_BIT) != 0 && m_pDockBar != nullptr)
    {
        bool moveFocus = false;

        if (nHitTest == HTCLIENT)
        {
            POINT cursor;
            GetCursorPos(&cursor);
            ::ScreenToClient(m_hWnd, &cursor);
            moveFocus = ::SendMessage(m_hWnd, TB_HITTEST, 0, reinterpret_cast<LPARAM>(&cursor)) < 1;
        }
        else if (nHitTest == HTCAPTION || nHitTest == HTBORDER)
        {
            moveFocus = true;
        }

        if (moveFocus)
        {
            m_pDockBar->SetFocus();
            // Original behaviour: no main window crashes here.
            AfxGetThread()->GetMainWnd()->SetFocus();
        }
    }

    return static_cast<BOOL>(Default());
}

void PaletteBar::OnCaptureChanged([[maybe_unused]] CWnd* pWnd)
{
    Default();

    if (m_pDockBar == nullptr)
    {
        return;
    }

    POINT cursor;
    GetCursorPos(&cursor);
    CRect caption;
    GetWindowRect(&caption);
    caption.bottom = caption.top;
    caption.top -= GetSystemMetrics(SM_CYSMCAPTION);

    if (caption.PtInRect(cursor))
    {
        m_pDockBar->SetFocus();
        // Original behaviour: no main window crashes here.
        AfxGetThread()->GetMainWnd()->SetFocus();
    }
}

void PaletteBar::OnSize([[maybe_unused]] UINT nType, [[maybe_unused]] int cx, [[maybe_unused]] int cy)
{
    Default();
}

BOOL PaletteBar::OnToolTipText([[maybe_unused]] UINT id, NMHDR* pNMHDR, LRESULT* pResult)
{
    auto& tip = *reinterpret_cast<TOOLTIPTEXTA*>(pNMHDR);
    tip.lpszText = const_cast<char*>(getButtonText(static_cast<UINT>(tip.hdr.idFrom)));

    if (tip.lpszText == nullptr)
    {
        char text[COMMAND_STRING_LENGTH];
        LoadStringA(GetModuleHandleA(nullptr), static_cast<UINT>(tip.hdr.idFrom), text, sizeof(text));
        CString tipText;
        AfxExtractSubString(tipText, text, 1, '\n');
        tip.lpszText = lstrcpynA(tip.szText, tipText, TOOL_TIP_LENGTH);
    }

    ::SetWindowPos(tip.hdr.hwndFrom, HWND_TOP, 0, 0, 0, 0,
                   SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    *pResult = 0;
    return FALSE;
}

BOOL PaletteBar::OnCustomDraw(NMHDR* pNMHDR, [[maybe_unused]] LRESULT* pResult)
{
    return static_cast<BOOL>(selectPalette(*reinterpret_cast<const NMCUSTOMDRAW*>(pNMHDR)));
}
