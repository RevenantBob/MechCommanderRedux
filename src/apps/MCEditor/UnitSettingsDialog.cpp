#include "stdafx.h"
#include "UnitSettingsDialog.h"
#include "EditorMap.h"
#include "MoverObject.h"
#include "ObjectLibrary.h"
#include "Objective.h"
#include "UnitBrain.h"
#include "UnitOrdersDialog.h"
#include "resource.h"

namespace
{
    /// <summary>The pilot ranks a mech's list offers (Vehicle Crew, Green and on).</summary>
    inline constexpr int32_t RANK_COUNT = 5;
    /// <summary>The paint schemes' names start at Red/White.</summary>
    inline constexpr UINT FIRST_PAINT_SCHEME_NAME = IDS_RED_WHITE;
    /// <summary>The variant buttons, which a vehicle doesn't use.</summary>
    inline constexpr UINT FIRST_VARIANT = IDC_VARIANT_A;
    inline constexpr UINT VARIANT_END = IDC_VARIANT_J + 1;
    /// <summary>The orders' descriptions start with the attack's.</summary>
    inline constexpr UINT FIRST_ORDER_DESCRIPTION = IDS_ORDERS_THE_UNIT_TO;
    /// <summary>A variant read from no mech.</summary>
    inline constexpr int NO_VARIANT = -1;
    /// <summary>The sides.</summary>
    inline constexpr int32_t ENEMY_SIDE = 1;
    inline constexpr int32_t PLAYER_SIDE = 2;
    /// <summary>Where the orders dialog sits in the Parameters group.</summary>
    inline constexpr int32_t ORDERS_OFFSET_X = 2;
    inline constexpr int32_t ORDERS_OFFSET_Y = 0xc;
    /// <summary>The answers of <see cref="UnitOrdersDialog::setSide"/>.</summary>
    inline constexpr int32_t RESET_ORDERS = -1;
    inline constexpr int32_t REFUSED = 0;
    /// <summary>The caption of the dialogs' message boxes ("MCEditor").</summary>
    inline constexpr UINT MESSAGE_CAPTION = 2;

    /// <summary>The type name of the mech or vehicle <paramref name="id"/>.</summary>
    CString moverTypeName(uint32_t id)
    {
        return objectLibrary->moverGroups[objectIdGroup(id)].types[objectIdType(id)].name.c_str();
    }

    /// <summary>Asks <paramref name="message"/> in a Yes/No box.</summary>
    bool askYesNo(CWnd& window, const CString& message)
    {
        CString caption;
        caption.LoadString(MESSAGE_CAPTION);
        return window.MessageBox(message, caption, MB_YESNO) == IDYES;
    }
}

UnitSettingsDialog::UnitSettingsDialog(EditorMap& map, MoverObject& mover, CWnd* pParent)
    : CDialog(IDD, pParent), mover(mover), map(map)
{
}

UnitSettingsDialog::~UnitSettingsDialog()
{
    delete ordersDialog;
    delete newBrain;
}

BEGIN_MESSAGE_MAP(UnitSettingsDialog, CDialog)
ON_CBN_SELCHANGE(IDC_ORDERS, &UnitSettingsDialog::OnSelchangeOrders)
ON_COMMAND_RANGE(IDC_ALLIED, IDC_ENEMY, &UnitSettingsDialog::OnAlliedRange)
END_MESSAGE_MAP()

void UnitSettingsDialog::DoDataExchange(CDataExchange* pDX)
{
    DDX_Control(pDX, IDC_PARAMETERS, parametersGroup);
    DDX_Control(pDX, IDC_UNIT_DESCRIPTION, description);
    DDX_Control(pDX, IDC_PILOT, pilotList);
    DDX_Control(pDX, IDC_ORDERS, ordersList);
    DDX_Control(pDX, IDC_PAINT_SCHEME, paintSchemeList);
    DDX_Radio(pDX, IDC_ALLIED, alignment);
    DDX_Radio(pDX, IDC_VARIANT_A, variant);
}

BOOL UnitSettingsDialog::OnInitDialog()
{
    CDialog::OnInitDialog();
    title = moverTypeName(mover.id);
    SetWindowText(title);
    alignment = mover.side != PLAYER_SIDE ? 1 : 0;
    checkedAlignment = IDC_ALLIED + static_cast<UINT>(alignment);
    CString text;

    if (objectIdClass(mover.id) == OBJECT_CLASS_MECH)
    {
        variant = static_cast<MechObject&>(mover).variant;

        for (int32_t i = 0; i < RANK_COUNT; i++)
        {
            text.LoadString(IDS_RANK_VEHICLE_CREW + i);
            pilotList.AddString(text);
        }
    }
    else
    {
        for (UINT id = FIRST_VARIANT; id < VARIANT_END; id++)
        {
            if (CWnd* button = GetDlgItem(id); button != nullptr)
            {
                button->EnableWindow(FALSE);
            }
        }

        text.LoadString(IDS_RANK_VEHICLE_CREW);
        pilotList.AddString(text);
        pilotList.SetCurSel(0);
        pilotList.EnableWindow(FALSE);
    }

    UpdateData(FALSE);

    for (int32_t i = 0; i < PAINT_SCHEME_COUNT; i++)
    {
        text.LoadString(FIRST_PAINT_SCHEME_NAME + i);
        paintSchemeList.AddString(text);
    }

    ordersList.SetCurSel(mover.brain->type);
    pilotList.SetCurSel(mover.rank);
    paintSchemeList.SetCurSel(mover.paintScheme);
    OnSelchangeOrders();
    return TRUE;
}

void UnitSettingsDialog::OnSelchangeOrders()
{
    const bool allied = GetDlgItem(IDC_ALLIED)->SendMessage(BM_GETCHECK, 0, 0) != 0;
    CString text;
    text.LoadString(FIRST_ORDER_DESCRIPTION + ordersList.GetCurSel());
    description.SetWindowText(text);

    if (ordersDialog == nullptr)
    {
        if (newBrain == nullptr)
        {
            // The first time: the unit's own orders, copied.
            newBrain = mover.brain->clone();
            ordersDialog = new UnitOrdersDialog(newBrain, allied ? PLAYER_SIDE : ENEMY_SIDE, this);
            ordersDialog->Create(UnitOrdersDialog::IDD, this);
            CRect group;
            parametersGroup.GetWindowRect(&group);
            ScreenToClient(&group);
            ordersDialog->SetWindowPos(nullptr, group.left + ORDERS_OFFSET_X, group.top + ORDERS_OFFSET_Y, 0, 0,
                                       SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);
        }
    }
    else
    {
        UnitBrain* old = newBrain;
        newBrain = createBrain(ordersList.GetCurSel());
        ordersDialog->setBrain(newBrain);
        delete old;
    }
}

void UnitSettingsDialog::OnOK()
{
    UpdateData(TRUE);

    if (ordersDialog != nullptr)
    {
        if (!ordersDialog->save(map))
        {
            return;
        }

        if (newBrain->referencesObject(mover, map))
        {
            CString message;
            message.LoadString(IDS_ORDER_TARGETS_ITSELF);
            CString caption;
            caption.LoadString(MESSAGE_CAPTION);
            MessageBox(message, caption, MB_OK);
            return;
        }

        UnitBrain* old = mover.brain;
        mover.brain = newBrain;
        delete old;
        newBrain = nullptr;
    }

    mover.rank = pilotList.GetCurSel();
    mover.paintScheme = paintSchemeList.GetCurSel();
    mover.side = alignment != 0 ? ENEMY_SIDE : PLAYER_SIDE;

    if (variant != NO_VARIANT && objectIdClass(mover.id) == OBJECT_CLASS_MECH)
    {
        static_cast<MechObject&>(mover).variant = variant;
    }

    CDialog::OnOK();
}

void UnitSettingsDialog::OnAlliedRange(UINT nID)
{
    int32_t answer = 1;
    bool proceed = true;

    // An objective about the unit goes when it changes sides.
    if (Objective* objective = map.objectiveFor(mover); objective != nullptr)
    {
        CString message;
        message.Format(IDS_THIS_S_IS_THE, static_cast<LPCTSTR>(title), static_cast<LPCTSTR>(objective->name));
        proceed = askYesNo(*this, message);

        if (proceed)
        {
            map.removeObjective(objective);
        }

        answer = proceed ? 1 : 0;
    }

    // So do the orders of the units that target it.
    for (EditorObject* object : map.movers)
    {
        auto& other = static_cast<MoverObject&>(*object);

        if (other.brain->referencesObject(mover, map))
        {
            const CString otherName = moverTypeName(other.id);
            CString message;
            message.Format(IDS_THIS_S_IS_TARGETED, static_cast<LPCTSTR>(moverTypeName(mover.id)),
                           static_cast<LPCTSTR>(otherName), static_cast<LPCTSTR>(otherName));

            if (askYesNo(*this, message))
            {
                other.resetBrain();
            }
            else
            {
                proceed = false;
                answer = 0;
            }
        }
    }

    if (proceed)
    {
        if (nID == IDC_ALLIED)
        {
            answer = ordersDialog->setSide(PLAYER_SIDE);
        }
        else if (nID == IDC_ENEMY)
        {
            answer = ordersDialog->setSide(ENEMY_SIDE);
        }
    }

    if (answer == RESET_ORDERS)
    {
        // Back to attack orders.
        ordersList.SetCurSel(0);
        OnSelchangeOrders();
    }
    else if (answer == REFUSED)
    {
        // The button checked before is checked again.
        GetDlgItem(checkedAlignment)->SendMessage(BM_SETCHECK, BST_CHECKED, 0);
        GetDlgItem(checkedAlignment != IDC_ENEMY ? IDC_ENEMY : IDC_ALLIED)->SendMessage(BM_SETCHECK, BST_UNCHECKED, 0);
    }
    else
    {
        checkedAlignment = nID;
    }
}
