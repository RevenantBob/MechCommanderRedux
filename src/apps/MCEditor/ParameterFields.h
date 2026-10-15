#pragma once

#include <list>

class AreaParameter;
class BoolParameter;
class DirectionParameter;
class EditorMap;
class NumberParameter;
class Parameter;
class PathParameter;
class TargetParameter;

// The fields the order and objective dialogs make for their values, one per value with its name as a label above it
// (the base's virtual table 0x004fd6b8; the kinds' 0x004faf98, 0x004fd358, 0x004fd8d0, 0x004fe018, 0x004fb080 and
// 0x004fb270). The location, path and target fields pick their value on the map: they put a pick tool on the view
// (WM_EDITOR_PICK) and take what it sends back (WM_EDITOR_PICKED). Their source file names weren't kept; the names are the
// port's.

/// <summary>A field for one value: the value's name as a label above it.</summary>
/// <remarks>0x7c bytes, virtual table 0x004fd6b8 (48 slots; 46 and 47 pure).</remarks>
class ParameterField : public CWnd
{
public:
    /// <summary>A field for <paramref name="parameter"/>, not made yet.</summary>
    /// <remarks>0x0042d800.</remarks>
    explicit ParameterField(Parameter& parameter);
    /// <remarks>0x0042d880.</remarks>
    ~ParameterField() override;

    /// <summary>
    /// Makes the field's window in <paramref name="rect"/> of <paramref name="parent"/> (with
    /// <paramref name="style"/> and id <paramref name="id"/>), filled with the value, and its label.
    /// </summary>
    /// <remarks>Virtual table slot 46.</remarks>
    virtual BOOL createField(DWORD style, const RECT& rect, CWnd* parent, UINT id) = 0;
    /// <summary>Takes the field's value into the value.</summary>
    /// <returns>Whether the field holds a value.</returns>
    /// <remarks>Virtual table slot 47.</remarks>
    virtual bool save(EditorMap& map) = 0;

protected:
    /// <summary>
    /// Takes the template's control info when a field makes its window from a dialog template (CreateDlg); modern
    /// MFC asserts in CWnd's own. The fields' templates have no ActiveX controls.
    /// </summary>
    BOOL SetOccDialogInfo(_AFX_OCC_DIALOG_INFO* pOccDialogInfo) override;
    /// <summary>
    /// Makes the label: the value's name in the rectangle one <paramref name="rect"/> height above it, in
    /// <paramref name="parent"/>'s font.
    /// </summary>
    /// <returns>TRUE.</returns>
    /// <remarks>0x0042d8e0.</remarks>
    BOOL createLabel(const RECT& rect, CWnd* parent);
    /// <summary>Asks the active view for a pick tool of <paramref name="mode"/> that answers this field.</summary>
    void requestPick(int32_t mode);
    /// <summary>Shows the dialog the field's dialog is in (hidden while the pick went on).</summary>
    void showOwnerDialog();

    /// <summary>A right click clears the value.</summary>
    /// <remarks>0x0042d9c0.</remarks>
    afx_msg void OnRButtonDown(UINT nFlags, CPoint point);

    /// <summary>The label (+0x3c).</summary>
    CStatic label;
    /// <summary>The value (+0x78).</summary>
    Parameter& parameter;

    DECLARE_MESSAGE_MAP()
};

/// <summary>
/// An area's field (template IDD_OBJECTIVE_AREA_DIALOG): its centre as text, a button that picks it on the map, and
/// "Enemies Allowed in Area". Saving puts the area's flag object on the map at the centre, or moves it there.
/// </summary>
/// <remarks>0x90 bytes, virtual table 0x004faf98.</remarks>
class AreaField : public ParameterField
{
public:
    /// <summary>A field for <paramref name="area"/>.</summary>
    /// <remarks>0x004183a0; the second argument isn't used.</remarks>
    explicit AreaField(AreaParameter& area);

    /// <summary>Makes the field's dialog at <paramref name="rect"/>'s top-left, then the label.</summary>
    /// <remarks>0x00418500.</remarks>
    BOOL createField(DWORD style, const RECT& rect, CWnd* parent, UINT id) override;
    /// <summary>
    /// Takes the centre and the box into the area, and puts the flag object on the new centre's cell (or moves the
    /// one on the old centre's).
    /// </summary>
    /// <returns>false when no centre was picked.</returns>
    /// <remarks>0x00418560.</remarks>
    bool save(EditorMap& map) override;

protected:
    /// <remarks>0x004188b0.</remarks>
    void DoDataExchange(CDataExchange* pDX) override;
    /// <summary>The button: picks a location.</summary>
    /// <remarks>0x004188f0.</remarks>
    afx_msg void OnPickButton();
    /// <summary>The picked location: shows it and the dialog again.</summary>
    /// <remarks>0x00418930.</remarks>
    afx_msg LRESULT OnPicked(WPARAM wParam, LPARAM lParam);

    /// <summary>The box "Enemies Allowed in Area" (+0x7c).</summary>
    BOOL enemiesAllowed = FALSE;
    /// <summary>The centre as text, "(x, y)" (+0x80).</summary>
    CString text;
    /// <summary>The area (+0x84).</summary>
    AreaParameter& area;
    /// <summary>The centre picked; -1 for none (+0x88, +0x8c).</summary>
    int32_t x = -1;
    int32_t y = -1;

    DECLARE_MESSAGE_MAP()
};

/// <summary>A number's field: an edit box (or, unused, a list of All and 1-49).</summary>
/// <remarks>0x84 bytes, virtual table 0x004fd358.</remarks>
class NumberField : public ParameterField
{
public:
    /// <summary>A field for <paramref name="number"/>, a list with <paramref name="list"/>.</summary>
    /// <remarks>0x0042bfd0.</remarks>
    NumberField(NumberParameter& number, bool list);

    /// <summary>
    /// Makes the box with the value (an edit box shows "All" as its number), then the label.
    /// </summary>
    /// <remarks>0x0042c030.</remarks>
    BOOL createField(DWORD style, const RECT& rect, CWnd* parent, UINT id) override;
    /// <summary>Takes the box's text: "All" or a number; an empty box leaves the value.</summary>
    /// <returns>true.</returns>
    /// <remarks>0x0042c290.</remarks>
    bool save(EditorMap& map) override;

protected:
    /// <summary>Gives the box a sunken edge.</summary>
    /// <remarks>0x0042c390.</remarks>
    BOOL PreCreateWindow(CREATESTRUCT& cs) override;

    /// <summary>The number (+0x7c).</summary>
    NumberParameter& number;
    /// <summary>The box is a list (+0x80).</summary>
    bool list;
};

/// <summary>
/// A path's field (template IDD_BROWSE_FIELD_DIALOG): its points as text and a button that picks them on the map.
/// </summary>
/// <remarks>0x98 bytes, virtual table 0x004fd8d0.</remarks>
class PathField : public ParameterField
{
public:
    /// <summary>A field for <paramref name="path"/>, showing its points.</summary>
    /// <remarks>0x0042e1a0; the second argument isn't used.</remarks>
    explicit PathField(PathParameter& path);

    /// <summary>Makes the field's dialog at <paramref name="rect"/>'s top-left, then the label.</summary>
    /// <remarks>0x0042e400.</remarks>
    BOOL createField(DWORD style, const RECT& rect, CWnd* parent, UINT id) override;
    /// <summary>Replaces the path's points with the field's.</summary>
    /// <returns>true.</returns>
    /// <remarks>0x0042e460.</remarks>
    bool save(EditorMap& map) override;

protected:
    /// <remarks>0x0042e730.</remarks>
    void DoDataExchange(CDataExchange* pDX) override;
    /// <summary>The button: picks a path.</summary>
    /// <remarks>0x0042e760.</remarks>
    afx_msg void OnPickButton();
    /// <summary>The picked path: shows it and the dialog again.</summary>
    /// <remarks>0x0042e7a0.</remarks>
    afx_msg LRESULT OnPicked(WPARAM wParam, LPARAM lParam);

    /// <summary>The points as text, "(x, y)" each (+0x80).</summary>
    CString text;
    /// <summary>The path (+0x84).</summary>
    PathParameter& path;
    /// <summary>The points (+0x90).</summary>
    std::list<POINT> points;

    DECLARE_MESSAGE_MAP()
};

/// <summary>
/// A target's field: the target's type name in a sunken box, and a button that picks a structure or unit (meeting the
/// filter) on the map.
/// </summary>
/// <remarks>0xcc bytes, virtual table 0x004fe018.</remarks>
class TargetField : public ParameterField
{
public:
    /// <summary>A field for <paramref name="target"/>, picking by <paramref name="filter"/> (MapPickFilter).</summary>
    /// <remarks>0x00431930.</remarks>
    TargetField(TargetParameter& target, int32_t filter);

    /// <summary>Makes the box (hidden without WS_VISIBLE in <paramref name="style"/>), then the label.</summary>
    /// <remarks>0x00431a30.</remarks>
    BOOL createField(DWORD style, const RECT& rect, CWnd* parent, UINT id) override;
    /// <summary>
    /// Takes the object on the picked cell (or the mover one cell down-right) as the target; none unsets it.
    /// </summary>
    /// <returns>true.</returns>
    /// <remarks>0x00431d00.</remarks>
    bool save(EditorMap& map) override;

protected:
    /// <remarks>0x00431ca0.</remarks>
    BOOL PreCreateWindow(CREATESTRUCT& cs) override;
    /// <summary>
    /// Adds the button in the box's last quarter, shows the target's type name and takes the target's cell and id.
    /// </summary>
    /// <remarks>0x00431ab0.</remarks>
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    /// <summary>The button: picks a structure or unit.</summary>
    /// <remarks>0x00431cb0.</remarks>
    afx_msg void OnPickButton();
    /// <summary>
    /// The picked object: shows its type name and the dialog again (and the two windows after the box).
    /// </summary>
    /// <remarks>0x00431df0.</remarks>
    afx_msg LRESULT OnPicked(WPARAM wParam, LPARAM lParam);
    /// <summary>A right click clears the target.</summary>
    /// <remarks>0x00431f80.</remarks>
    afx_msg void OnRButtonDown(UINT nFlags, CPoint point);

    /// <summary>The pick button (+0x7c).</summary>
    CButton button;
    /// <summary>The target (+0xb8).</summary>
    TargetParameter& target;
    /// <summary>The picked cell and object id; -1 for none (+0xbc, +0xc0, +0xc4).</summary>
    int32_t cellCol = -1;
    int32_t cellRow = -1;
    int32_t objectId = -1;
    /// <summary>The pick's filter (+0xc8).</summary>
    int32_t filter;

    DECLARE_MESSAGE_MAP()
};

/// <summary>A yes/no value's field: a check box in the left half.</summary>
/// <remarks>0x80 bytes, virtual table 0x004fb080.</remarks>
class BoolField : public ParameterField
{
public:
    /// <summary>A field for <paramref name="value"/>.</summary>
    /// <remarks>0x00418a40.</remarks>
    explicit BoolField(BoolParameter& value);

    /// <summary>Makes the label, then the check box (checked as the value).</summary>
    /// <remarks>0x00418aa0.</remarks>
    BOOL createField(DWORD style, const RECT& rect, CWnd* parent, UINT id) override;
    /// <summary>Takes the box's check.</summary>
    /// <returns>true.</returns>
    /// <remarks>0x00418b30.</remarks>
    bool save(EditorMap& map) override;

protected:
    /// <summary>The value (+0x7c).</summary>
    BoolParameter& value;
};

/// <summary>A direction's field: a list of the eight directions.</summary>
/// <remarks>0x80 bytes, virtual table 0x004fb270.</remarks>
class DirectionField : public ParameterField
{
public:
    /// <summary>A field for <paramref name="direction"/>.</summary>
    /// <remarks>0x004197c0.</remarks>
    explicit DirectionField(DirectionParameter& direction);

    /// <summary>Makes the label, then the list (North to Northwest) with the direction chosen.</summary>
    /// <remarks>0x00419820.</remarks>
    BOOL createField(DWORD style, const RECT& rect, CWnd* parent, UINT id) override;
    /// <summary>Takes the list's choice.</summary>
    /// <returns>true.</returns>
    /// <remarks>0x00419940.</remarks>
    bool save(EditorMap& map) override;

protected:
    /// <summary>The direction (+0x7c).</summary>
    DirectionParameter& direction;
};
