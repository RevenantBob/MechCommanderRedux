#include "stdafx.h"
#include "NewMapDialog.h"
#include "EditorUtil.h"
#include "ObjectLibrary.h"
#include "TileLibrary.h"
#include "resource.h"

namespace
{
    /// <summary>The coverage sliders' range, and the most two coverages may add up to.</summary>
    inline constexpr int MAX_COVERAGE = 100;
    /// <summary>What the Cermak names' string ids add to Port Arthur's (IDS_ASH for IDS_DIRT, IDS_SAND for IDS_GRASS).</summary>
    inline constexpr UINT CERMAK_STRING_OFFSET = IDS_ASH - IDS_DIRT;
    /// <summary>The buffer a terrain button's name is loaded into.</summary>
    inline constexpr int TERRAIN_NAME_SIZE = 0x100;
}

NewMapDialog::NewMapDialog(CWnd* pParent) : CDialog(IDD, pParent)
{
}

BEGIN_MESSAGE_MAP(NewMapDialog, CDialog)
ON_WM_HSCROLL()
ON_BN_CLICKED(IDC_PORT_ARTHUR, &NewMapDialog::OnPortArthur)
ON_BN_CLICKED(IDC_CERMAK, &NewMapDialog::OnCermak)
END_MESSAGE_MAP()

void NewMapDialog::DoDataExchange(CDataExchange* pDX)
{
    DDX_Control(pDX, IDC_HILL_COVERAGE, _hillSlider);
    DDX_Control(pDX, IDC_FOREST_COVERAGE, _forestSlider);
    DDX_Control(pDX, IDC_WATER_COVERAGE, _waterSlider);
    DDX_Radio(pDX, IDC_PORT_ARTHUR, locale);
    DDX_Radio(pDX, IDC_GRASS, terrain);
}

BOOL NewMapDialog::OnInitDialog()
{
    CDialog::OnInitDialog();
    CString title;
    title.LoadString(IDS_EDITOR_NAME);
    SetWindowText(title);
    ModifyStyle(WS_CAPTION, 0, 0);
    _waterSlider.SetRange(0, MAX_COVERAGE, FALSE);
    _forestSlider.SetRange(0, MAX_COVERAGE, FALSE);
    _hillSlider.SetRange(0, MAX_COVERAGE, FALSE);
    return TRUE;
}

void NewMapDialog::readSliders()
{
    waterCoverage = static_cast<int>(::SendMessage(_waterSlider.m_hWnd, TBM_GETPOS, 0, 0));
    forestCoverage = static_cast<int>(::SendMessage(_forestSlider.m_hWnd, TBM_GETPOS, 0, 0));
    hillCoverage = static_cast<int>(::SendMessage(_hillSlider.m_hWnd, TBM_GETPOS, 0, 0));
}

void NewMapDialog::OnOK()
{
    UpdateData(TRUE);
    readSliders();
    UpdateData(TRUE);
    const bool gTiles = locale != 0;
    tileLibrary->setGTiles(gTiles);

    if (objectLibrary->gTiles != gTiles)
    {
        objectLibrary->reloadNeeded = true;
    }

    objectLibrary->gTiles = gTiles;
    CDialog::OnOK();
}

void NewMapDialog::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
    readSliders();
    const HWND moved = pScrollBar->m_hWnd;

    if (waterCoverage + forestCoverage > MAX_COVERAGE)
    {
        if (moved == _waterSlider.m_hWnd)
        {
            forestCoverage = MAX_COVERAGE - waterCoverage;
            ::SendMessage(_forestSlider.m_hWnd, TBM_SETPOS, TRUE, forestCoverage);
        }
        else if (moved == _forestSlider.m_hWnd)
        {
            waterCoverage = MAX_COVERAGE - forestCoverage;
            ::SendMessage(_waterSlider.m_hWnd, TBM_SETPOS, TRUE, waterCoverage);
        }
    }

    if (hillCoverage + waterCoverage > MAX_COVERAGE)
    {
        if (moved == _waterSlider.m_hWnd)
        {
            hillCoverage = MAX_COVERAGE - waterCoverage;
            ::SendMessage(_hillSlider.m_hWnd, TBM_SETPOS, TRUE, hillCoverage);
        }
        else
        {
            // Also when the forest slider moved: the hills stay and the water gives way.
            waterCoverage = MAX_COVERAGE - hillCoverage;
            ::SendMessage(_waterSlider.m_hWnd, TBM_SETPOS, TRUE, waterCoverage);
        }
    }

    CDialog::OnHScroll(nSBCode, nPos, pScrollBar);
}

void NewMapDialog::OnPortArthur()
{
    UpdateData(TRUE);
    const UINT offset = locale != 0 ? CERMAK_STRING_OFFSET : 0;
    const HINSTANCE instance = GetModuleHandle(nullptr);
    char text[TERRAIN_NAME_SIZE];
    LoadString(instance, IDS_DIRT + offset, text, TERRAIN_NAME_SIZE);
    CString label("&");
    label += text;
    GetDlgItem(IDC_DIRT)->SetWindowText(label);
    LoadString(instance, IDS_GRASS + offset, text, TERRAIN_NAME_SIZE);
    label = "&";
    label += text;
    GetDlgItem(IDC_GRASS)->SetWindowText(label);
}

void NewMapDialog::OnCermak()
{
    OnPortArthur();
}
