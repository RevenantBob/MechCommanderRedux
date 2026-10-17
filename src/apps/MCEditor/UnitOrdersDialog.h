#pragma once

#include <vector>

class EditorMap;
class ParameterField;
class UnitBrain;

/// <summary>
/// The IDD_UNIT_ORDERS_DIALOG dialog: the part of the Unit Settings dialog that edits an order's values, the engage
/// radius, tactic and move speed and a field for each of the order's own values.
/// </summary>
/// <remarks>0x1d4 bytes, virtual table 0x004fb180.</remarks>
class UnitOrdersDialog : public CDialog
{
public:
    /// <summary>The dialog template.</summary>
    enum
    {
        IDD = IDD_UNIT_ORDERS_DIALOG
    };

    /// <summary>
    /// The values of <paramref name="brain"/> (which the dialog doesn't own) for a unit of <paramref name="side"/>, in
    /// <paramref name="pParent"/>.
    /// </summary>
    /// <remarks>0x00418b80.</remarks>
    UnitOrdersDialog(UnitBrain* brain, int32_t side, CWnd* pParent);
    /// <summary>Destroys and deletes the fields.</summary>
    /// <remarks>0x00418ca0.</remarks>
    ~UnitOrdersDialog() override;

    /// <summary>
    /// Shows <paramref name="newBrain"/>'s values: the old fields go, a field is made for each of its own values in a
    /// row below the engage radius, and the common lists take its values; orders with no values hide everything.
    /// </summary>
    /// <remarks>0x00418e70.</remarks>
    void setBrain(UnitBrain* newBrain);
    /// <summary>
    /// Takes the lists' and the fields' values into the orders; a value left unset is named in a message box.
    /// </summary>
    /// <returns>Whether every field held a value and every value is set.</returns>
    /// <remarks>0x004193b0.</remarks>
    bool save(EditorMap& map);
    /// <summary>
    /// Takes the unit's new side; for orders whose targets depend on it (guard, destroy, kill, escort) asks first
    /// whether to reset them.
    /// </summary>
    /// <returns>1 when nothing was asked, -1 when the orders are to be reset, 0 when the change was refused.</returns>
    /// <remarks>0x00419670. Original behaviour: the side is taken even when the change is refused.</remarks>
    int32_t setSide(int32_t newSide);

protected:
    /// <remarks>0x00418db0.</remarks>
    void DoDataExchange(CDataExchange* pDX) override;
    /// <summary>Shows the orders it was made with.</summary>
    /// <remarks>0x00418e40.</remarks>
    BOOL OnInitDialog() override;
    /// <summary>Enter does nothing in the child dialog.</summary>
    /// <remarks>0x004195e0.</remarks>
    void OnOK() override;
    /// <summary>Escape does nothing in the child dialog.</summary>
    /// <remarks>0x004195f0.</remarks>
    void OnCancel() override;

    /// <summary>Handles ON_WM_CLOSE(): nothing.</summary>
    /// <remarks>0x004195d0.</remarks>
    afx_msg void OnClose();

    /// <summary>The filter the target fields pick by, for the orders and side.</summary>
    /// <remarks>0x00419600, also inlined in <see cref="setBrain"/>.</remarks>
    int32_t targetFilter() const;
    /// <summary>Destroys and deletes the fields.</summary>
    void deleteFields();

    /// <summary>The labels and lists of the common values (+0x5c, +0x98, +0xd4, +0x110, +0x14c, +0x188).</summary>
    CStatic tacticLabel;
    CStatic moveSpeedLabel;
    CComboBox tacticList;
    CComboBox moveSpeedList;
    CStatic engageRadiusLabel;
    CComboBox engageRadiusList;
    /// <summary>The orders shown (+0x1c4).</summary>
    UnitBrain* brain;
    /// <summary>A field per value of the orders; the common three (and any value missing) have none (+0x1c8, +0x1cc).</summary>
    std::vector<ParameterField*> fields;
    /// <summary>The unit's side (+0x1d0).</summary>
    int32_t side;

    DECLARE_MESSAGE_MAP()
};
