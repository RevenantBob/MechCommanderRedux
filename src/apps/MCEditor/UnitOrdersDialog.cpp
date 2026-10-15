#include "stdafx.h"
#include "UnitOrdersDialog.h"
#include "MapPickTool.h"
#include "Parameter.h"
#include "ParameterFields.h"
#include "UnitBrain.h"
#include "resource.h"

namespace
{
    /// <summary>The values every order has come first in its table; the fields are for the rest.</summary>
    inline constexpr int32_t FIRST_OWN_VALUE = 3;
    /// <summary>The engage radii the list offers, in its order.</summary>
    inline constexpr long ENGAGE_RADII[] = {0, 0x96, 0x15e, 0x28a};
    /// <summary>The gap below the engage radius list to the fields' row.</summary>
    inline constexpr int32_t ROW_GAP = 6;
    /// <summary>The fields' window style.</summary>
    inline constexpr DWORD FIELD_STYLE = WS_CHILD | WS_VISIBLE;
    /// <summary>The player's side.</summary>
    inline constexpr int32_t PLAYER_SIDE = 2;
    /// <summary>The answer that the orders are to be reset, and that the change was refused.</summary>
    inline constexpr int32_t RESET_ORDERS = -1;
    inline constexpr int32_t REFUSED = 0;
    inline constexpr int32_t UNCHANGED = 1;
    /// <summary>The caption of the dialogs' message boxes ("MCEditor").</summary>
    inline constexpr UINT MESSAGE_CAPTION = 2;
}

UnitOrdersDialog::UnitOrdersDialog(UnitBrain* brain, int32_t side, CWnd* pParent)
    : CDialog(IDD, pParent), brain(brain), side(side)
{
}

UnitOrdersDialog::~UnitOrdersDialog()
{
    deleteFields();
}

void UnitOrdersDialog::deleteFields()
{
    for (ParameterField* field : fields)
    {
        if (field != nullptr)
        {
            field->DestroyWindow();
            delete field;
        }
    }

    fields.clear();
}

BEGIN_MESSAGE_MAP(UnitOrdersDialog, CDialog)
ON_WM_CLOSE()
END_MESSAGE_MAP()

void UnitOrdersDialog::DoDataExchange(CDataExchange* pDX)
{
    DDX_Control(pDX, IDC_TACTIC_LABEL, tacticLabel);
    DDX_Control(pDX, IDC_MOVE_SPEED_LABEL, moveSpeedLabel);
    DDX_Control(pDX, IDC_TACTIC, tacticList);
    DDX_Control(pDX, IDC_MOVE_SPEED, moveSpeedList);
    DDX_Control(pDX, IDC_ENGAGE_RADIUS_LABEL, engageRadiusLabel);
    DDX_Control(pDX, IDC_ENGAGE_RADIUS, engageRadiusList);
}

BOOL UnitOrdersDialog::OnInitDialog()
{
    CDialog::OnInitDialog();
    UnitBrain* first = brain;
    brain = nullptr;
    setBrain(first);
    return TRUE;
}

void UnitOrdersDialog::OnOK()
{
}

void UnitOrdersDialog::OnCancel()
{
}

void UnitOrdersDialog::OnClose()
{
}

int32_t UnitOrdersDialog::targetFilter() const
{
    switch (brain->type)
    {
        case BRAIN_GUARD:
        case BRAIN_ESCORT:
        {
            // A friend's own units.
            return side == PLAYER_SIDE ? PICK_NOT_ENEMYS : PICK_NOT_PLAYERS;
        }

        case BRAIN_DESTROY:
        case BRAIN_KILL:
        {
            // Something of the other side that can be destroyed.
            return (side == PLAYER_SIDE ? PICK_NOT_PLAYERS : PICK_NOT_ENEMYS) | PICK_DESTROYABLE;
        }

        default:
        {
            return 0;
        }
    }
}

void UnitOrdersDialog::setBrain(UnitBrain* newBrain)
{
    if (brain != nullptr)
    {
        deleteFields();
    }

    brain = newBrain;
    const int32_t count = brain->parameterCount();
    CRect row;
    GetWindowRect(&row);
    ScreenToClient(&row);
    CWnd* common[] = {&tacticLabel,   &moveSpeedLabel,    &tacticList,
                      &moveSpeedList, &engageRadiusLabel, &engageRadiusList};

    if (count == 0)
    {
        for (CWnd* control : common)
        {
            control->ShowWindow(SW_HIDE);
        }

        return;
    }

    fields.assign(static_cast<size_t>(count), nullptr);
    CRect engage;
    engageRadiusList.GetWindowRect(&engage);
    CRect tactic;
    tacticList.GetWindowRect(&tactic);
    // The fields are as wide as the engage radius list and as far apart as it is from the tactic list.
    const int32_t width = engage.Width();
    const int32_t gap = tactic.left - engage.right;
    engage.bottom = engage.top + ROW_GAP + static_cast<int32_t>(engageRadiusList.SendMessage(CB_GETITEMHEIGHT, 0, 0));
    ScreenToClient(&engage);
    row.top = engage.Height() + ROW_GAP + engage.bottom;
    row.bottom = row.top + engage.Height();
    row.left = engage.left;
    row.right = row.left + width;

    for (int32_t i = FIRST_OWN_VALUE; i < count; i++)
    {
        Parameter* value = brain->parameter(i);

        if (value == nullptr)
        {
            continue;
        }

        ParameterField* field = nullptr;

        switch (value->type)
        {
            case PARAMETER_STRUCTURE:
            case PARAMETER_UNIT:
            {
                field = new TargetField(static_cast<TargetParameter&>(*value), targetFilter());
                break;
            }

            case PARAMETER_AREA:
            {
                field = new AreaField(static_cast<AreaParameter&>(*value));
                break;
            }

            case PARAMETER_NUMBER:
            {
                field = new NumberField(static_cast<NumberParameter&>(*value), false);
                break;
            }

            case PARAMETER_PATH:
            {
                field = new PathField(static_cast<PathParameter&>(*value));
                break;
            }

            case PARAMETER_BOOL:
            {
                field = new BoolField(static_cast<BoolParameter&>(*value));
                break;
            }

            case PARAMETER_DIRECTION:
            {
                field = new DirectionField(static_cast<DirectionParameter&>(*value));
                break;
            }

            default:
            {
                // EB-094: the original makes a field of whatever its array holds for the other kinds.
                break;
            }
        }

        if (field != nullptr)
        {
            fields[static_cast<size_t>(i)] = field;
            field->createField(FIELD_STYLE, row, this, static_cast<UINT>(i));
        }

        row.left = row.right + gap;
        row.right = width + row.left;
    }

    engageRadiusLabel.ShowWindow(SW_SHOW);
    engageRadiusList.ShowWindow(SW_SHOW);

    for (int32_t i = 0; i < static_cast<int32_t>(std::size(ENGAGE_RADII)); i++)
    {
        if (brain->engageRadius.value == ENGAGE_RADII[i])
        {
            engageRadiusList.SetCurSel(i);
            break;
        }
    }

    tacticList.SetCurSel(static_cast<int>(brain->tactic.value));
    moveSpeedList.SetCurSel(brain->run.value ? 1 : 0);

    for (CWnd* control : common)
    {
        control->ShowWindow(SW_SHOW);
    }
}

bool UnitOrdersDialog::save(EditorMap& map)
{
    UpdateData(TRUE);
    bool saved = true;

    if (engageRadiusList.IsWindowVisible())
    {
        const int selection = engageRadiusList.GetCurSel();
        // Original behaviour: no choice keeps the list's -1 as the radius.
        brain->engageRadius.value = selection >= 0 && selection < static_cast<int>(std::size(ENGAGE_RADII))
                                        ? ENGAGE_RADII[selection]
                                        : selection;
        brain->tactic.value = tacticList.GetCurSel();
        brain->run.value = moveSpeedList.GetCurSel() != 0;
    }

    const int32_t count = brain->parameterCount();

    for (int32_t i = FIRST_OWN_VALUE; i < count; i++)
    {
        ParameterField* field = static_cast<size_t>(i) < fields.size() ? fields[static_cast<size_t>(i)] : nullptr;
        // EB-094: the original saves whatever its array holds where no field was made.
        const bool held = field != nullptr && field->save(map);
        saved = held && saved;
        Parameter* value = brain->parameter(i);

        if (value != nullptr && !value->isSet())
        {
            CString message;
            message.Format(IDS_THE_S_PARAMETER_IS, static_cast<LPCTSTR>(value->name));
            CString caption;
            caption.LoadString(MESSAGE_CAPTION);
            MessageBox(message, caption, MB_OK);
            saved = false;

            if (field != nullptr)
            {
                field->SetFocus();
            }
        }
    }

    return saved;
}

int32_t UnitOrdersDialog::setSide(int32_t newSide)
{
    int32_t answer = UNCHANGED;

    switch (brain->type)
    {
        case BRAIN_GUARD:
        case BRAIN_DESTROY:
        case BRAIN_KILL:
        case BRAIN_ESCORT:
        {
            CString message;
            message.LoadString(IDS_CHANGING_THIS_UNITS_ALIGNMENT);
            CString caption;
            caption.LoadString(MESSAGE_CAPTION);
            answer = MessageBox(message, caption, MB_YESNO) == IDYES ? RESET_ORDERS : REFUSED;
            break;
        }

        default:
        {
            break;
        }
    }

    side = newSide;
    return answer;
}
