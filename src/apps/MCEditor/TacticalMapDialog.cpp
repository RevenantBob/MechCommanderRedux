#include "stdafx.h"
#include "TacticalMapDialog.h"
#include "DreditorDoc.h"
#include "EditorMap.h"
#include "EditorPalette.h"
#include "resource.h"

namespace
{
    /// <summary>The colour the bitmap is cleared to.</summary>
    inline constexpr int BACKGROUND_COLOR = 0x10;
    /// <summary>The outline's colour, cyan, and its width.</summary>
    inline constexpr COLORREF OUTLINE_COLOR = RGB(0, 0xff, 0xff);
    inline constexpr SIZE OUTLINE_SIZE = {1, 1};
    /// <summary>A tile is 3 pixels; the map turned 45 degrees takes half as many each way.</summary>
    inline constexpr int32_t PIXELS_PER_TILE = 3;
    /// <summary>A click centres the outline, a little off (the original's adjustment).</summary>
    inline constexpr int32_t CLICK_OFFSET_X = 6;
    inline constexpr int32_t CLICK_OFFSET_Y = 8;
}

TacticalMapDialog::TacticalMapDialog(CWnd* pParent) : CDialog(IDD, pParent)
{
    bitmapInfo = static_cast<BITMAPINFO*>(operator new(BITMAP_INFO_ALLOC_SIZE));
    std::memset(bitmapInfo, 0, BITMAP_INFO_ALLOC_SIZE);
    bitmapInfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo->bmiHeader.biPlanes = 1;
    bitmapInfo->bmiHeader.biBitCount = 8;
    bitmapInfo->bmiHeader.biCompression = BI_RGB;
}

TacticalMapDialog::~TacticalMapDialog()
{
    if (dib != nullptr)
    {
        DeleteObject(dib);
    }

    delete memoryDC;
    operator delete(bitmapInfo);
}

void TacticalMapDialog::setViewRect(const RECT& rect)
{
    // The redraw's flags: RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW.
    constexpr UINT REDRAW_FLAGS = 0x121;
    viewRectChanged = true;
    previousViewRect = viewRect;
    viewRect = rect;
    viewRect.top -= 2;
    viewRect.left -= 2;

    if (m_hWnd != nullptr)
    {
        ::RedrawWindow(m_hWnd, nullptr, nullptr, REDRAW_FLAGS);
    }
}

void TacticalMapDialog::setParentView(CWnd* view)
{
    m_pParentWnd = view;
}

BEGIN_MESSAGE_MAP(TacticalMapDialog, CDialog)
ON_WM_PAINT()
ON_WM_LBUTTONDOWN()
ON_WM_ERASEBKGND()
ON_WM_WINDOWPOSCHANGED()
ON_WM_MOVE()
ON_WM_QUERYNEWPALETTE()
END_MESSAGE_MAP()

BOOL TacticalMapDialog::OnInitDialog()
{
    CDialog::OnInitDialog();
    std::memcpy(bitmapInfo, editorPalette->bitmapInfo, BITMAP_INFO_ALLOC_SIZE);
    ShowWindow(AfxGetApp()->GetProfileInt("TacMapLoc", "Visible", 1) != 0 ? SW_SHOW : SW_HIDE);
    return TRUE;
}

void TacticalMapDialog::OnPaint()
{
    CPaintDC dc(this);

    if (palette.m_hObject == nullptr)
    {
        palette.Attach(CreatePalette(editorPalette->gammaPalette));
    }

    dc.SelectPalette(&palette, TRUE);
    ::RealizePalette(dc.m_hDC);

    if (viewRectChanged)
    {
        // After a scroll only the outline moves, once the map is drawn.
        if (bitsWidth != 0)
        {
            dc.DrawDragRect(&viewRect, OUTLINE_SIZE, &previousViewRect, OUTLINE_SIZE, &outlineBrush, &outlineBrush);
            viewRectChanged = false;
            return;
        }

        dc.DrawDragRect(&viewRect, OUTLINE_SIZE, nullptr, OUTLINE_SIZE, &outlineBrush, &outlineBrush);
    }

    // The original draws without testing for a map.
    if (document == nullptr || document->map == nullptr)
    {
        return;
    }

    const EditorMap& map = *document->map;
    const auto roundUp = [](int32_t value) { return value % 4 != 0 ? (value + 4) / 4 * 4 : value; };
    const int32_t width = roundUp((map.maxRow + 1) * PIXELS_PER_TILE >> 1);
    const int32_t height = roundUp((map.maxCol + 1) * PIXELS_PER_TILE >> 1);

    if (width != bitsWidth)
    {
        // The window takes the bitmap's size. Original behaviour: it adds a thin border (SM_CXBORDER) where the
        // dialog's frame is thicker, so the client ends 4 pixels short of the bitmap each way. The original (unaware
        // of DPI) gets the classic frame; the rebuild's is padded, so the client the original ends with is asked for.
        bitsWidth = width;
        bitsHeight = height;
        DeleteObject(dib);
        dib = nullptr;
        CRect frame(0, 0, width + (GetSystemMetrics(SM_CXBORDER) - GetSystemMetrics(SM_CXFIXEDFRAME)) * 2,
                    height + (GetSystemMetrics(SM_CYBORDER) - GetSystemMetrics(SM_CYFIXEDFRAME)) * 2);
        AdjustWindowRectEx(&frame, GetStyle(), FALSE, GetExStyle());
        SetWindowPos(nullptr, 0, 0, frame.Width(), frame.Height(), SWP_NOMOVE | SWP_NOZORDER);
    }

    bitmapInfo->bmiHeader.biWidth = bitsWidth;
    bitmapInfo->bmiHeader.biHeight = -bitsHeight;
    // Original behaviour: the size is given in bits.
    bitmapInfo->bmiHeader.biSizeImage = bitsHeight * bitsWidth * 8;

    if (memoryDC == nullptr)
    {
        memoryDC = new CDC;
        memoryDC->Attach(CreateCompatibleDC(dc.m_hDC));
    }

    if (dib == nullptr)
    {
        dib =
            CreateDIBSection(memoryDC->m_hDC, bitmapInfo, DIB_RGB_COLORS, reinterpret_cast<void**>(&bits), nullptr, 0);
    }

    std::memset(bits, BACKGROUND_COLOR, static_cast<size_t>(width) * height);
    document->map->drawTacticalMap(bits, width, height, reinterpret_cast<const uint8_t*>(bitmapInfo->bmiColors));
    HGDIOBJ old = SelectObject(memoryDC->m_hDC, dib);

    if (outlineBrush.m_hObject == nullptr)
    {
        outlineBrush.Attach(CreateSolidBrush(OUTLINE_COLOR));
    }

    BitBlt(dc.m_hDC, 0, 0, width, height, memoryDC->m_hDC, 0, 0, SRCCOPY);
    dc.DrawDragRect(&viewRect, OUTLINE_SIZE, nullptr, OUTLINE_SIZE, &outlineBrush, &outlineBrush);

    if (old != nullptr)
    {
        SelectObject(memoryDC->m_hDC, old);
    }
}

void TacticalMapDialog::OnLButtonDown([[maybe_unused]] UINT nFlags, CPoint point)
{
    POINT target = {point.x + CLICK_OFFSET_X - ((viewRect.right - viewRect.left) >> 1),
                    point.y + CLICK_OFFSET_Y - ((viewRect.bottom - viewRect.top) >> 1)};
    ::SendMessageA(m_pParentWnd->m_hWnd, WM_EDITOR_CENTRE_ON_TACTICAL, 1, reinterpret_cast<LPARAM>(&target));
    Default();
    // The original asks the thread for its main window and drops the answer.
    m_pParentWnd->SetFocus();
    viewRectChanged = false;
    InvalidateRect(nullptr, TRUE);
}

BOOL TacticalMapDialog::OnEraseBkgnd([[maybe_unused]] CDC* pDC)
{
    viewRectChanged = false;
    return TRUE;
}

void TacticalMapDialog::OnWindowPosChanged([[maybe_unused]] WINDOWPOS* lpwndpos)
{
    Default();

    if (m_pParentWnd != nullptr)
    {
        m_pParentWnd->SetFocus();
    }
}

void TacticalMapDialog::OnMove([[maybe_unused]] int x, [[maybe_unused]] int y)
{
    Default();

    if (IsWindowVisible())
    {
        CRect rect;
        GetWindowRect(&rect);
        AfxGetApp()->WriteProfileInt("TacMapLoc", "XPos", rect.left);
        AfxGetApp()->WriteProfileInt("TacMapLoc", "YPos", rect.top);
    }
}

BOOL TacticalMapDialog::OnQueryNewPalette()
{
    CDC* dc = CDC::FromHandle(::GetDC(m_hWnd));

    if (palette.m_hObject == nullptr)
    {
        palette.Attach(CreatePalette(editorPalette->gammaPalette));
    }

    dc->SelectPalette(&palette, FALSE);
    ::RealizePalette(dc->m_hDC);
    ::ReleaseDC(m_hWnd, dc->m_hDC);
    return TRUE;
}
