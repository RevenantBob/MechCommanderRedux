#pragma once

class EditorMap;
class MoverObject;
class UnitBrain;
class UnitOrdersDialog;

/// <summary>
/// The IDD_UNIT_SETTINGS_DIALOG dialog: a mech's or vehicle's alignment, variant, paint scheme, pilot and orders, the
/// orders' values in a <see cref="UnitOrdersDialog"/> inside it. The view shows it modeless (it holds the view's tools
/// off while it is shown).
/// </summary>
/// <remarks>0x1a8 bytes, virtual table 0x004fe3e0.</remarks>
class UnitSettingsDialog : public CDialog
{
public:
    /// <summary>The dialog template.</summary>
    enum
    {
        IDD = IDD_UNIT_SETTINGS_DIALOG
    };

    /// <summary>The settings of <paramref name="mover"/> on <paramref name="map"/>, for the view <paramref name="pParent"/>.</summary>
    /// <remarks>0x004371b0.</remarks>
    UnitSettingsDialog(EditorMap& map, MoverObject& mover, CWnd* pParent);
    /// <summary>Deletes the orders dialog and the orders being edited.</summary>
    /// <remarks>0x004372d0.</remarks>
    ~UnitSettingsDialog() override;

protected:
    /// <remarks>0x004373a0.</remarks>
    void DoDataExchange(CDataExchange* pDX) override;
    /// <summary>
    /// Titles the dialog with the unit's type, fills the lists (a vehicle has its crew only and no variants) and
    /// shows the orders.
    /// </summary>
    /// <remarks>0x004375d0.</remarks>
    BOOL OnInitDialog() override;
    /// <summary>
    /// Saves the orders' values (stopping on an unset one, or orders that target the unit itself), then gives the unit
    /// the new orders, pilot, paint scheme, side and (a mech) variant.
    /// </summary>
    /// <remarks>0x00437440.</remarks>
    void OnOK() override;

    /// <summary>
    /// Handles ON_CBN_SELCHANGE(IDC_ORDERS): shows the order's description, and the first time makes the orders dialog
    /// with a copy of the unit's orders, after that new orders of the kind chosen.
    /// </summary>
    /// <remarks>0x004377e0.</remarks>
    afx_msg void OnSelchangeOrders();

    /// <summary>
    /// Handles ON_COMMAND_RANGE(IDC_ALLIED, IDC_ENEMY): changing sides, after asking, deletes the objective that
    /// refers to the unit and resets the orders of the units that target it; the orders may be reset, or the change
    /// undone.
    /// </summary>
    /// <remarks>0x004379a0.</remarks>
    afx_msg void OnAlliedRange(UINT nID);

    /// <summary>The Parameters group, the order's description and the lists (+0x5c, +0x98, +0xd4, +0x110, +0x14c).</summary>
    CStatic parametersGroup;
    CStatic description;
    CComboBox pilotList;
    CComboBox ordersList;
    CComboBox paintSchemeList;
    /// <summary>The Alignment buttons' choice: 0 Allied, 1 Enemy (+0x188).</summary>
    int alignment = 0;
    /// <summary>The Variant buttons' choice (+0x18c); 2 until a mech's is read.</summary>
    int variant = 2;
    /// <summary>The unit's type name, the title (+0x190).</summary>
    CString title;
    /// <summary>The unit (+0x194).</summary>
    MoverObject& mover;
    /// <summary>The orders being edited (+0x198), given to the unit by OK.</summary>
    UnitBrain* newBrain = nullptr;
    /// <summary>The orders dialog (+0x19c).</summary>
    UnitOrdersDialog* ordersDialog = nullptr;
    /// <summary>The map (+0x1a0).</summary>
    EditorMap& map;
    /// <summary>The Alignment button checked last (+0x1a4).</summary>
    UINT checkedAlignment = 0;

    DECLARE_MESSAGE_MAP()
};
