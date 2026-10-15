#pragma once

#include <vector>

class EditorMap;
class Objective;
class ParameterField;

/// <summary>
/// The IDD_OBJECTIVE_ROW_DIALOG dialog: one row of the objectives dialog (<see cref="ObjectivesTeamDialog"/>), a child
/// window with the objective's kind, description, Primary, Marker and Complete Previous boxes and a field per value.
/// It edits a copy of the objective, which <see cref="save"/> hands back.
/// </summary>
/// <remarks>0x1a8 bytes, virtual table 0x004fd480.</remarks>
class ObjectiveRowDialog : public CDialog
{
public:
    /// <summary>The dialog template.</summary>
    enum
    {
        IDD = IDD_OBJECTIVE_ROW_DIALOG
    };

    /// <summary>
    /// A row for <paramref name="objective"/> (null for an empty row) of side 1 when <paramref name="otherSide"/>, else
    /// side 2.
    /// </summary>
    /// <param name="noUnitObjectives">Leaves the kinds that target units out of the list.</param>
    /// <param name="first">The first row: no "None" kind and no Complete Previous.</param>
    /// <remarks>0x0042c3c0.</remarks>
    ObjectiveRowDialog(Objective* objective, bool otherSide, CWnd* pParent, bool noUnitObjectives, bool first);
    /// <summary>Deletes the fields and the copy being edited.</summary>
    /// <remarks>0x0042c5c0.</remarks>
    ~ObjectiveRowDialog() override;

    /// <summary>
    /// Takes the fields' values and the boxes into the copy, refusing a value that isn't set (the first must be) and
    /// two equal values. A row that had an objective puts a copy of the edited one on <paramref name="map"/> in its
    /// place (<paramref name="result"/> unset), or with "None" hands it back to be removed; an empty row hands back a
    /// new objective to be added.
    /// </summary>
    /// <returns>0 when refused (after a message box), 1 for an empty row left at "None", else 2.</returns>
    /// <remarks>0x0042cea0.</remarks>
    int32_t save(Objective*& result, bool& added, bool& removed, EditorMap& map);

protected:
    /// <remarks>0x0042c6e0.</remarks>
    void DoDataExchange(CDataExchange* pDX) override;
    /// <summary>Fills the kinds' list, chooses the objective's kind and shows its fields.</summary>
    /// <remarks>0x0042cd30.</remarks>
    BOOL OnInitDialog() override;
    /// <summary>Nothing: Enter doesn't close a row.</summary>
    /// <remarks>0x0042d360.</remarks>
    void OnOK() override;
    /// <summary>Nothing: Escape doesn't close a row.</summary>
    /// <remarks>0x0042d370.</remarks>
    void OnCancel() override;

    /// <summary>
    /// Sets the boxes for the kind (no marker or Complete Previous for "Destroy All Enemies", nothing for "None") and
    /// makes a field per value across the fields' area, hiding a target's field until the one before it is set.
    /// </summary>
    /// <remarks>0x0042c980.</remarks>
    void showFields();
    /// <summary>Deletes the fields and the copy being edited.</summary>
    void deleteObjective();

    /// <summary>Handles ON_WM_CREATE().</summary>
    /// <remarks>0x0042c7a0.</remarks>
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);

    /// <summary>
    /// Handles ON_CBN_SELCHANGE(IDC_OBJECTIVE_KIND): a new objective of the kind chosen, on the same side, named
    /// after its kind.
    /// </summary>
    /// <remarks>0x0042c7b0.</remarks>
    afx_msg void OnSelchangeObjectiveKind();

    /// <summary>Handles ON_WM_CLOSE(): the default handling only.</summary>
    /// <remarks>0x0042d350.</remarks>
    afx_msg void OnClose();

    /// <summary>The boxes, the fields' area and the kinds' list (+0x5c to +0x14c).</summary>
    CButton primaryBox;
    CButton completePreviousBox;
    CStatic fieldsArea;
    CComboBox kindList;
    CButton markerBox;
    /// <summary>The Marker box (+0x188).</summary>
    int marker;
    /// <summary>The description (+0x18c).</summary>
    CString description;
    /// <summary>The Complete Previous box (+0x190).</summary>
    int completePrevious;
    /// <summary>The Primary box (+0x194).</summary>
    int primary;
    /// <summary>A field per value of the copy (+0x198).</summary>
    std::vector<ParameterField*> fields;
    /// <summary>The map's objective the row shows, or null (+0x19c).</summary>
    Objective* original;
    /// <summary>The copy being edited (+0x1a0).</summary>
    Objective* objective;
    /// <summary>Whether the unit kinds are left out (+0x1a4).</summary>
    bool noUnitObjectives;
    /// <summary>Whether this is the first row (+0x1a5).</summary>
    bool first;

    DECLARE_MESSAGE_MAP()
};
