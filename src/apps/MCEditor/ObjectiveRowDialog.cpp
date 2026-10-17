#include "stdafx.h"
#include "ObjectiveRowDialog.h"
#include "EditorMap.h"
#include "MapPickTool.h"
#include "Objective.h"
#include "Parameter.h"
#include "ParameterFields.h"
#include "resource.h"

namespace
{
    /// <summary>The caption of the dialogs' message boxes ("MCEditor").</summary>
    inline constexpr UINT MESSAGE_CAPTION = IDS_MCEDITOR;
    /// <summary>The sides.</summary>
    inline constexpr int32_t PLAYER_SIDE = 2;
    /// <summary>The longest description.</summary>
    inline constexpr int MAX_DESCRIPTION = 0x28;
    /// <summary>The fields' gap.</summary>
    inline constexpr int32_t FIELD_GAP = 6;
    /// <summary>The number of fields the area is split for.</summary>
    inline constexpr int32_t FIELD_COLUMNS = 3;
    /// <summary>A field's style: shown, or hidden until the target before it is set.</summary>
    inline constexpr DWORD SHOWN_FIELD = WS_CHILD | WS_VISIBLE;
    inline constexpr DWORD HIDDEN_FIELD = WS_CHILD;

    /// <summary>Whether each kind can have a marker (0x00502248): all but "Destroy All Enemies".</summary>
    inline constexpr bool KIND_HAS_MARKER[OBJECTIVE_COUNT] = {false, true, true, true, true, true, true, true};

    /// <summary>The number of values <paramref name="objective"/> has.</summary>
    int32_t parameterCount(const Objective& objective)
    {
        int32_t count = 0;

        for (const Parameter* parameter : objective.parameters)
        {
            if (parameter != nullptr)
            {
                count++;
            }
        }

        return count;
    }

    /// <summary>The pick filter of a target of kind <paramref name="type"/> for an objective of <paramref name="side"/>.</summary>
    int32_t targetFilter(int32_t type, int32_t side)
    {
        const bool player = side == PLAYER_SIDE;

        switch (type)
        {
            case OBJECTIVE_DESTROY_UNITS:
            case OBJECTIVE_DESTROY_BUILDINGS:
            {
                return (player ? PICK_NOT_PLAYERS : PICK_NOT_ENEMYS) | PICK_DESTROYABLE;
            }

            case OBJECTIVE_GUARD_UNITS:
            case OBJECTIVE_GUARD_BUILDINGS:
            {
                return player ? PICK_NOT_ENEMYS : PICK_NOT_PLAYERS;
            }

            case OBJECTIVE_CAPTURE_UNITS:
            case OBJECTIVE_CAPTURE_BUILDINGS:
            {
                return (player ? PICK_NOT_PLAYERS : PICK_NOT_ENEMYS) | PICK_CAPTURABLE;
            }

            default:
            {
                return 0;
            }
        }
    }

    /// <summary>Whether the kind <paramref name="type"/> targets units.</summary>
    bool targetsUnits(int32_t type)
    {
        return type == OBJECTIVE_DESTROY_UNITS || type == OBJECTIVE_GUARD_UNITS || type == OBJECTIVE_CAPTURE_UNITS;
    }
}

ObjectiveRowDialog::ObjectiveRowDialog(Objective* objective, bool otherSide, CWnd* pParent, bool noUnitObjectives,
                                       bool first)
    : CDialog(IDD, pParent)
    , original(objective)
    , objective(objective != nullptr ? new Objective(*objective) : new Objective())
    , noUnitObjectives(noUnitObjectives)
    , first(first)
{
    marker = this->objective->hasMarker ? 1 : 0;
    completePrevious = this->objective->afterLastAction;
    description = this->objective->name;
    this->objective->side = otherSide ? PLAYER_SIDE - 1 : PLAYER_SIDE;
    primary = this->objective->primary ? 1 : 0;
}

ObjectiveRowDialog::~ObjectiveRowDialog()
{
    deleteObjective();
}

void ObjectiveRowDialog::deleteObjective()
{
    for (ParameterField* field : fields)
    {
        delete field;
    }

    fields.clear();
    delete objective;
    objective = nullptr;
}

BEGIN_MESSAGE_MAP(ObjectiveRowDialog, CDialog)
ON_WM_CREATE()
ON_CBN_SELCHANGE(IDC_OBJECTIVE_KIND, &ObjectiveRowDialog::OnSelchangeObjectiveKind)
ON_WM_CLOSE()
END_MESSAGE_MAP()

void ObjectiveRowDialog::DoDataExchange(CDataExchange* pDX)
{
    DDX_Control(pDX, IDC_PRIMARY, primaryBox);
    DDX_Control(pDX, IDC_COMPLETE_PREVIOUS, completePreviousBox);
    DDX_Control(pDX, IDC_OBJECTIVE_FIELDS, fieldsArea);
    DDX_Control(pDX, IDC_OBJECTIVE_KIND, kindList);
    DDX_Control(pDX, IDC_MARKER, markerBox);
    DDX_Check(pDX, IDC_MARKER, marker);
    DDX_Text(pDX, IDC_OBJECTIVE_DESCRIPTION, description);
    DDV_MaxChars(pDX, description, MAX_DESCRIPTION);
    DDX_Check(pDX, IDC_COMPLETE_PREVIOUS, completePrevious);
    DDX_Check(pDX, IDC_PRIMARY, primary);
}

int ObjectiveRowDialog::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    return CDialog::OnCreate(lpCreateStruct) != -1 ? 0 : -1;
}

BOOL ObjectiveRowDialog::OnInitDialog()
{
    CDialog::OnInitDialog();

    // The first row has no "None".
    for (int32_t kind = first ? 0 : OBJECTIVE_NONE; kind < OBJECTIVE_COUNT; kind++)
    {
        if (noUnitObjectives && targetsUnits(kind))
        {
            continue;
        }

        CString text;
        text.LoadString(IDS_DESTROY_ALL_ENEMIES + kind);
        const int item = kindList.AddString(text);
        kindList.SetItemData(item, static_cast<DWORD_PTR>(kind));
    }

    if (first)
    {
        completePreviousBox.ModifyStyle(0, WS_DISABLED, 0);
    }

    UpdateData(FALSE);

    if (objective != nullptr)
    {
        int chosen = -1;

        for (int item = 0; item < kindList.GetCount(); item++)
        {
            if (static_cast<int32_t>(kindList.GetItemData(item)) == objective->type)
            {
                chosen = item;
            }
        }

        kindList.SetCurSel(chosen);
        showFields();
    }

    return TRUE;
}

void ObjectiveRowDialog::OnOK()
{
}

void ObjectiveRowDialog::OnCancel()
{
}

void ObjectiveRowDialog::OnClose()
{
    Default();
}

void ObjectiveRowDialog::OnSelchangeObjectiveKind()
{
    const auto kind = static_cast<int32_t>(kindList.GetItemData(kindList.GetCurSel()));
    const int32_t side = objective->side;
    deleteObjective();
    objective = new Objective(kind);
    objective->side = side;
    description = objective->name;
    marker = objective->hasMarker ? 1 : 0;
    completePrevious = objective->afterLastAction;
    primary = objective->primary ? 1 : 0;
    UpdateData(FALSE);

    if (!KIND_HAS_MARKER[objective->type])
    {
        marker = 0;
        markerBox.ModifyStyle(0, WS_DISABLED, 0);
    }
    else
    {
        markerBox.ModifyStyle(WS_DISABLED, 0, 0);
    }

    showFields();
    UpdateData(FALSE);
}

void ObjectiveRowDialog::showFields()
{
    markerBox.ShowWindow(SW_SHOW);
    completePreviousBox.ShowWindow(SW_SHOW);
    markerBox.EnableWindow(TRUE);
    completePreviousBox.EnableWindow(TRUE);
    primaryBox.EnableWindow(TRUE);

    if (first)
    {
        completePreviousBox.EnableWindow(FALSE);
    }

    if (objective->type == OBJECTIVE_DESTROY_ALL)
    {
        completePreviousBox.EnableWindow(FALSE);
        markerBox.EnableWindow(FALSE);
    }

    if (objective->type == OBJECTIVE_NONE)
    {
        completePreviousBox.EnableWindow(FALSE);
        markerBox.EnableWindow(FALSE);
        primaryBox.EnableWindow(FALSE);
        primaryBox.SetCheck(BST_UNCHECKED);
    }

    const int32_t count = parameterCount(*objective);
    fields.assign(static_cast<size_t>(count), nullptr);
    CRect area;
    fieldsArea.GetWindowRect(&area);
    ScreenToClient(&area);
    const int32_t width = (area.Width() - 2 * FIELD_GAP) / FIELD_COLUMNS;
    CRect rect(area.left, area.top, area.left + width, area.bottom);
    bool previousSet = false;

    for (int32_t i = 0; i < count; i++)
    {
        Parameter* parameter = objective->parameters[i];
        DWORD style = SHOWN_FIELD;

        if (parameter == nullptr)
        {
            continue;
        }

        switch (parameter->type)
        {
            case PARAMETER_STRUCTURE:
            case PARAMETER_UNIT:
            {
                fields[i] = new TargetField(static_cast<TargetParameter&>(*parameter),
                                            targetFilter(objective->type, objective->side));

                if (i > 0 && !previousSet)
                {
                    style = HIDDEN_FIELD;
                }

                previousSet = parameter->isSet();
                break;
            }

            case PARAMETER_AREA:
            {
                fields[i] = new AreaField(static_cast<AreaParameter&>(*parameter));
                break;
            }

            case PARAMETER_NUMBER:
            {
                fields[i] = new NumberField(static_cast<NumberParameter&>(*parameter), true);
                break;
            }

            default:
            {
                break;
            }
        }

        if (fields[i] != nullptr)
        {
            fields[i]->createField(style, rect, this, static_cast<UINT>(i));
        }

        rect.left = rect.right + FIELD_GAP;
        rect.right = rect.left + width;
    }

    markerBox.RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
}

int32_t ObjectiveRowDialog::save(Objective*& result, bool& added, bool& removed, EditorMap& map)
{
    removed = false;
    added = false;
    UpdateData(TRUE);
    const int32_t count = parameterCount(*objective);
    CString caption;
    caption.LoadString(MESSAGE_CAPTION);

    for (int32_t i = 0; i < count; i++)
    {
        if (fields[i] == nullptr || !fields[i]->save(map))
        {
            CString name = objective->parameters[i]->name;
            CString message;

            if (!name.IsEmpty())
            {
                message.Format(IDS_THE_S_PARAMETER_IS, static_cast<LPCTSTR>(name));
            }
            else
            {
                // An unnamed value is named after the first one, with its number.
                name = objective->parameters[0]->name;
                message.Format(IDS_S_PARAMETE_LD_IS, static_cast<LPCTSTR>(name), static_cast<long>(i + 1));
            }

            MessageBox(message, caption, MB_OK);
            return 0;
        }
    }

    if (count > 0 && !objective->parameters[0]->isSet())
    {
        CString message;
        message.Format(IDS_THE_S_PARAMETER_IS, static_cast<LPCTSTR>(objective->parameters[0]->name));
        MessageBox(message, caption, MB_OK);
        return 0;
    }

    for (int32_t i = 0; i < count; i++)
    {
        for (int32_t j = i + 1; j < count; j++)
        {
            Parameter& one = *objective->parameters[i];
            Parameter& other = *objective->parameters[j];

            if (one.isSet() && other.isSet() && one.equals(other))
            {
                CString message;
                message.Format(IDS_NO_TWO_PARAMETERS_CAN, i + 1, j + 1, static_cast<LPCTSTR>(objective->name));
                MessageBox(message, caption, MB_OK);
                return 0;
            }
        }
    }

    objective->afterLastAction = completePrevious;
    objective->hasMarker = marker != 0;
    objective->name = description;
    objective->primary = primary != 0;

    if (original != nullptr)
    {
        if (objective->type == OBJECTIVE_NONE)
        {
            removed = true;
            result = original;
        }
        else
        {
            // The edited copy takes the original's place; adding it first keeps the marks both refer to.
            Objective* old = original;
            original = new Objective(*objective);
            map.addObjective(original);
            map.removeObjective(old);
        }

        return 2;
    }

    if (objective->type == OBJECTIVE_NONE)
    {
        result = nullptr;
        removed = false;
        added = false;
        return 1;
    }

    result = new Objective(*objective);
    added = true;
    return 2;
}
