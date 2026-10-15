#include "stdafx.h"
#include "SetHeightDialog.h"
#include "EditorAction.h"
#include "EditorMap.h"
#include "UndoList.h"
#include "VertexTool.h"
#include "resource.h"

namespace
{
    /// <summary>The highest height the box takes.</summary>
    inline constexpr int MAX_HEIGHT = 0x80;
    /// <summary>RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW.</summary>
    inline constexpr UINT REDRAW_NOW = 0x121;
}

SetHeightDialog::SetHeightDialog(EditorMap& map, VertexTool& tool, CWnd* pParent)
    : CDialog(IDD, pParent), tool(tool), map(map)
{
    tool.begin(map);
    int32_t sum = 0;
    int32_t count = 0;

    for (const POINT& vertex : map.selectedVertices)
    {
        sum += map.vertexElevation(vertex.x, vertex.y);
        count++;
    }

    height = sum / count;
}

void SetHeightDialog::DoDataExchange(CDataExchange* pDX)
{
    DDX_Text(pDX, IDC_HEIGHT, height);
    DDV_MinMaxInt(pDX, height, 0, MAX_HEIGHT);
}

BEGIN_MESSAGE_MAP(SetHeightDialog, CDialog)
ON_BN_CLICKED(IDC_APPLY, &SetHeightDialog::OnApply)
END_MESSAGE_MAP()

void SetHeightDialog::applyHeight()
{
    tool.setSelectedElevation(map, height);
    ::RedrawWindow(::GetParent(m_hWnd), nullptr, nullptr, REDRAW_NOW);
}

void SetHeightDialog::OnApply()
{
    UpdateData(TRUE);
    CWaitCursor wait;
    applyHeight();
}

void SetHeightDialog::OnOK()
{
    if (!UpdateData(TRUE))
    {
        return;
    }

    {
        CWaitCursor wait;
        applyHeight();

        if (EditorAction* action = tool.end(); action != nullptr)
        {
            undoList->push(action);
        }
    }

    CDialog::OnOK();
}

void SetHeightDialog::OnCancel()
{
    if (EditorAction* action = tool.end(); action != nullptr)
    {
        action->undo();
        delete action;
    }

    ::RedrawWindow(::GetParent(m_hWnd), nullptr, nullptr, REDRAW_NOW);
    CDialog::OnCancel();
}
