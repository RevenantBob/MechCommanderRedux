#include "stdafx.h"
#include "ObjectivesTeamDialog.h"
#include "EditorMap.h"
#include "Objective.h"
#include "ObjectiveRowDialog.h"
#include "resource.h"

namespace
{
    /// <summary>The sides.</summary>
    inline constexpr int32_t ENEMY_SIDE = 1;
    inline constexpr int32_t PLAYER_SIDE = 2;
    /// <summary>The gap above each row, and how far a row's rectangle reaches below the one above it.</summary>
    inline constexpr int32_t ROW_GAP = 6;
    inline constexpr int32_t ROW_REACH = 0xc;
}

ObjectivesTeamDialog::ObjectivesTeamDialog(EditorMap& map, CWnd* pParent, bool otherSide, bool noUnitObjectives)
    : CDialog(IDD, pParent), map(map), otherSide(otherSide), noUnitObjectives(noUnitObjectives)
{
}

ObjectivesTeamDialog::~ObjectivesTeamDialog()
{
    for (ObjectiveRowDialog* row : rows)
    {
        delete row;
    }
}

BEGIN_MESSAGE_MAP(ObjectivesTeamDialog, CDialog)
END_MESSAGE_MAP()

void ObjectivesTeamDialog::DoDataExchange(CDataExchange* pDX)
{
    DDX_Control(pDX, IDC_OBJECTIVE, headingLine);
}

BOOL ObjectivesTeamDialog::OnInitDialog()
{
    CDialog::OnInitDialog();
    CString title;
    GetWindowText(title);
    title += otherSide ? " 2" : " 1";
    SetWindowText(title);
    const int32_t side = otherSide ? ENEMY_SIDE : PLAYER_SIDE;
    size_t count = 0;

    for (Objective* objective : map.objectives)
    {
        if (objective->side == side && count < rows.size())
        {
            rows[count] = new ObjectiveRowDialog(objective, otherSide, this, noUnitObjectives, count == 0);
            count++;
        }
    }

    for (; count < rows.size(); count++)
    {
        rows[count] = new ObjectiveRowDialog(nullptr, otherSide, this, noUnitObjectives, false);
    }

    CRect rect;
    headingLine.GetWindowRect(&rect);
    ScreenToClient(&rect);
    rect.top += ROW_GAP;
    rect.bottom += ROW_REACH;

    for (ObjectiveRowDialog* row : rows)
    {
        row->Create(ObjectiveRowDialog::IDD, this);
        row->SetWindowPos(nullptr, rect.left, rect.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);
        row->GetWindowRect(&rect);
        rect.top = rect.bottom + ROW_GAP;
        rect.bottom += ROW_REACH;
        ScreenToClient(&rect);
    }

    ShowWindow(SW_SHOW);
    CenterWindow();
    return TRUE;
}

void ObjectivesTeamDialog::OnOK()
{
    for (ObjectiveRowDialog* row : rows)
    {
        Objective* objective = nullptr;
        bool added = false;
        bool removed = false;
        const int32_t result = row->save(objective, added, removed, map);

        if (result == 0)
        {
            return;
        }

        if (result == 2)
        {
            if (added)
            {
                objective->side = otherSide ? ENEMY_SIDE : PLAYER_SIDE;
                map.addObjective(objective);
            }
            else if (removed)
            {
                map.removeObjective(objective);
            }
        }
    }

    CDialog::OnOK();
}
