#include "stdafx.h"
#include "ParameterFields.h"
#include "EditorMap.h"
#include "EditorObject.h"
#include "MapPickTool.h"
#include "ObjectLibrary.h"
#include "Parameter.h"
#include "StaticObject.h"
#include "resource.h"

namespace
{
    /// <summary>The label's style: a centred-vertically static, and its id.</summary>
    inline constexpr DWORD LABEL_STYLE = WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE;
    inline constexpr UINT LABEL_ID = 0xffff;
    /// <summary>A location's text and a number's.</summary>
    const char* const POINT_FORMAT = "(%ld, %ld)";
    const char* const NUMBER_FORMAT = "%ld";
    /// <summary>The number box: an edit box taking digits, or a list (disabled and hidden, as the original makes it).</summary>
    inline constexpr DWORD NUMBER_EDIT_STYLE = WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_NUMBER;
    inline constexpr DWORD NUMBER_LIST_STYLE = WS_DISABLED | WS_VSCROLL | WS_TABSTOP | CBS_DROPDOWNLIST;
    /// <summary>How much taller a list's rectangle is than its box, for its drop-down.</summary>
    inline constexpr int32_t LIST_DROP_HEIGHT = 0x32;
    /// <summary>The numbers a number list offers after All.</summary>
    inline constexpr int32_t FIRST_LIST_NUMBER = 1;
    inline constexpr int32_t LIST_NUMBER_END = 0x32;
    /// <summary>The target box's styles and the pick button's.</summary>
    inline constexpr DWORD TARGET_EX_STYLE = WS_EX_CLIENTEDGE | WS_EX_NOPARENTNOTIFY;
    inline constexpr DWORD TARGET_STYLE = 0x810880;
    inline constexpr DWORD BUTTON_STYLE = WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON;
    /// <summary>The target's pick button's id.</summary>
    inline constexpr UINT TARGET_BUTTON_ID = 0x75;
    /// <summary>The check box: an auto check box with its text on the left.</summary>
    inline constexpr DWORD CHECK_STYLE = WS_CHILD | WS_VISIBLE | BS_LEFTTEXT | BS_AUTOCHECKBOX;
    /// <summary>The direction list.</summary>
    inline constexpr DWORD DIRECTION_STYLE = WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST;
    inline constexpr int32_t DIRECTION_COUNT = 8;

    /// <summary>The window's font as a WPARAM, for WM_SETFONT.</summary>
    WPARAM fontOf(CWnd& window)
    {
        CFont* font = CFont::FromHandle(reinterpret_cast<HFONT>(window.SendMessage(WM_GETFONT, 0, 0)));
        return font != nullptr ? reinterpret_cast<WPARAM>(font->m_hObject) : 0;
    }

    /// <summary>The object of the map's objects or drop zones whose footprint covers the cell, or null.</summary>
    EditorObject* coveringObject(EditorMap& map, int32_t col, int32_t row)
    {
        if (map.objectAtCell(col, row) == nullptr)
        {
            return nullptr;
        }

        for (const std::list<EditorObject*>* list : {&map.objects, &map.dropZones})
        {
            for (EditorObject* object : *list)
            {
                if (object->coversCell(col, row))
                {
                    return object;
                }
            }
        }

        return nullptr;
    }
}

BEGIN_MESSAGE_MAP(ParameterField, CWnd)
ON_WM_RBUTTONDOWN()
END_MESSAGE_MAP()

ParameterField::ParameterField(Parameter& parameter) : parameter(parameter)
{
}

ParameterField::~ParameterField() = default;

BOOL ParameterField::SetOccDialogInfo([[maybe_unused]] _AFX_OCC_DIALOG_INFO* pOccDialogInfo)
{
    return TRUE;
}

BOOL ParameterField::createLabel(const RECT& rect, CWnd* parent)
{
    CRect above = rect;
    above.top = rect.top * 2 - rect.bottom;
    above.bottom = rect.top;
    label.Create(parameter.name, LABEL_STYLE, above, parent, LABEL_ID);
    label.SendMessage(WM_SETFONT, fontOf(*parent), TRUE);
    return TRUE;
}

void ParameterField::requestPick(int32_t mode)
{
    // The original asks the thread for its main window and drops the answer.
    auto* frame = static_cast<CFrameWnd*>(AfxGetMainWnd());
    ::SendMessageA(frame->GetActiveView()->m_hWnd, WM_EDITOR_PICK, static_cast<WPARAM>(mode),
                   reinterpret_cast<LPARAM>(this));
}

void ParameterField::showOwnerDialog()
{
    CWnd::FromHandle(::GetParent(CWnd::FromHandle(::GetParent(m_hWnd))->m_hWnd))->ShowWindow(SW_SHOW);
}

void ParameterField::OnRButtonDown([[maybe_unused]] UINT nFlags, [[maybe_unused]] CPoint point)
{
    parameter.reset();
    Default();
}

BEGIN_MESSAGE_MAP(AreaField, ParameterField)
ON_COMMAND(IDC_OBJECTIVE_AREA_PICK, &AreaField::OnPickButton)
ON_MESSAGE(WM_EDITOR_PICKED, &AreaField::OnPicked)
END_MESSAGE_MAP()

AreaField::AreaField(AreaParameter& area)
    : ParameterField(area), enemiesAllowed(area.extractionZone), area(area), x(area.x), y(area.y)
{
    if (area.x != -1)
    {
        text.Format(POINT_FORMAT, x, y);
    }
}

BOOL AreaField::createField([[maybe_unused]] DWORD style, const RECT& rect, CWnd* parent, [[maybe_unused]] UINT id)
{
    CreateDlg(MAKEINTRESOURCE(IDD_OBJECTIVE_AREA_DIALOG), parent);
    SetWindowPos(nullptr, rect.left, rect.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    ShowWindow(SW_SHOW);
    UpdateData(FALSE);
    return createLabel(rect, parent);
}

bool AreaField::save(EditorMap& map)
{
    UpdateData(TRUE);

    if (x == -1 && y == -1)
    {
        return false;
    }

    int32_t col = 0;
    int32_t row = 0;

    if (!area.isSet())
    {
        // A new centre: the flag object goes on its cell.
        map.worldToCell(x, y, &col, &row);
        map.addObject(new StaticObject(flagObjectId(), col, row, 0));
    }
    else
    {
        // The flag on the old centre's cell moves to the new one's.
        map.worldToCell(area.x, area.y, &col, &row);
        EditorObject* flag = coveringObject(map, col, row);
        map.worldToCell(x, y, &col, &row);

        // EB-093: the original moves no object (null) as well.
        if (flag != nullptr)
        {
            map.moveObject(*flag, col, row);
        }
    }

    area.extractionZone = enemiesAllowed != FALSE;
    area.x = x;
    area.y = y;
    return true;
}

void AreaField::DoDataExchange(CDataExchange* pDX)
{
    DDX_Check(pDX, IDC_ENEMIES_ALLOWED_IN_AREA, enemiesAllowed);
    DDX_Text(pDX, IDC_OBJECTIVE_AREA_TEXT, text);
}

void AreaField::OnPickButton()
{
    requestPick(PICK_LOCATION);
}

LRESULT AreaField::OnPicked([[maybe_unused]] WPARAM wParam, LPARAM lParam)
{
    const POINT& point = *reinterpret_cast<const POINT*>(lParam);
    x = point.x;
    y = point.y;
    CString picked;
    picked.Format(POINT_FORMAT, x, y);

    // The original empties the text for no point, then takes the formatted one anyway.
    text = picked;
    UpdateData(FALSE);
    showOwnerDialog();
    return 0;
}

NumberField::NumberField(NumberParameter& number, bool list) : ParameterField(number), number(number), list(list)
{
}

BOOL NumberField::createField(DWORD style, const RECT& rect, CWnd* parent, UINT id)
{
    CRect box = rect;

    if (!list)
    {
        Create("EDIT", nullptr, NUMBER_EDIT_STYLE, box, parent, id, nullptr);
        ModifyStyleEx(0, WS_EX_CLIENTEDGE, SWP_NOMOVE);
    }
    else
    {
        box.bottom += LIST_DROP_HEIGHT;
        Create("COMBOBOX", nullptr, NUMBER_LIST_STYLE, box, parent, id, nullptr);
    }

    SendMessage(WM_SETFONT, fontOf(*CWnd::FromHandle(::GetParent(m_hWnd))), TRUE);
    CString all;
    all.LoadString(IDS_ALL);

    if (m_hWnd == nullptr || !list)
    {
        // Original behaviour: an edit box shows "All" as its number.
        CString value;
        value.Format(NUMBER_FORMAT, number.value);
        SetWindowText(value);
    }
    else
    {
        SendMessage(CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(static_cast<LPCTSTR>(all)));

        for (int32_t value = FIRST_LIST_NUMBER; value < LIST_NUMBER_END; value++)
        {
            char digits[0x100];
            _ltoa_s(value, digits, 10);
            SendMessage(CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(digits));
        }

        if (number.value == PARAMETER_ALL)
        {
            SendMessage(CB_SELECTSTRING, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(static_cast<LPCTSTR>(all)));
        }
        else
        {
            char digits[0x100];
            _ltoa_s(number.value, digits, 10);
            SendMessage(CB_SELECTSTRING, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(digits));
        }
    }

    // The original passes its first argument, the style, as the label's (unused) style.
    static_cast<void>(style);
    return createLabel(rect, parent);
}

bool NumberField::save([[maybe_unused]] EditorMap& map)
{
    CString value;
    GetWindowText(value);

    if (!value.IsEmpty())
    {
        CString all;
        all.LoadString(IDS_ALL);
        number.value = value == all ? PARAMETER_ALL : atol(value);
    }

    return true;
}

BOOL NumberField::PreCreateWindow(CREATESTRUCT& cs)
{
    cs.dwExStyle |= WS_EX_CLIENTEDGE;
    return CWnd::PreCreateWindow(cs);
}

BEGIN_MESSAGE_MAP(PathField, ParameterField)
ON_COMMAND(IDC_BROWSE_FIELD_PICK, &PathField::OnPickButton)
ON_MESSAGE(WM_EDITOR_PICKED, &PathField::OnPicked)
END_MESSAGE_MAP()

PathField::PathField(PathParameter& path) : ParameterField(path), path(path)
{
    for (const LocationParameter& point : path.points)
    {
        CString part;
        part.Format(POINT_FORMAT, point.x, point.y);
        text += part;
        points.push_back({point.x, point.y});
    }
}

BOOL PathField::createField([[maybe_unused]] DWORD style, const RECT& rect, CWnd* parent, [[maybe_unused]] UINT id)
{
    CreateDlg(MAKEINTRESOURCE(IDD_BROWSE_FIELD_DIALOG), parent);
    SetWindowPos(nullptr, rect.left, rect.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    ShowWindow(SW_SHOW);
    UpdateData(FALSE);
    return createLabel(rect, parent);
}

bool PathField::save([[maybe_unused]] EditorMap& map)
{
    UpdateData(TRUE);
    path.points.clear();

    for (const POINT& point : points)
    {
        LocationParameter location;
        location.x = point.x;
        location.y = point.y;
        path.points.push_back(location);
    }

    return true;
}

void PathField::DoDataExchange(CDataExchange* pDX)
{
    DDX_Text(pDX, IDC_BROWSE_FIELD_TEXT, text);
}

void PathField::OnPickButton()
{
    requestPick(PICK_PATH);
}

LRESULT PathField::OnPicked(WPARAM wParam, LPARAM lParam)
{
    text.Empty();
    points.clear();
    const auto* picked = reinterpret_cast<const POINT*>(lParam);

    for (WPARAM i = 0; i < wParam; i++)
    {
        CString part;
        part.Format(POINT_FORMAT, picked[i].x, picked[i].y);
        text += part;
        points.push_back(picked[i]);
    }

    UpdateData(FALSE);
    showOwnerDialog();
    return 0;
}

BEGIN_MESSAGE_MAP(TargetField, ParameterField)
ON_WM_CREATE()
ON_COMMAND(TARGET_BUTTON_ID, &TargetField::OnPickButton)
ON_MESSAGE(WM_EDITOR_PICKED, &TargetField::OnPicked)
ON_WM_RBUTTONDOWN()
END_MESSAGE_MAP()

TargetField::TargetField(TargetParameter& target, int32_t filter)
    : ParameterField(target), target(target), filter(filter)
{
}

BOOL TargetField::createField(DWORD style, const RECT& rect, CWnd* parent, UINT id)
{
    CreateEx(TARGET_EX_STYLE, "STATIC", nullptr, style | TARGET_STYLE, rect, parent, id, nullptr);

    if ((style & WS_VISIBLE) == 0)
    {
        ShowWindow(SW_HIDE);
    }

    return createLabel(rect, parent);
}

BOOL TargetField::PreCreateWindow(CREATESTRUCT& cs)
{
    return CWnd::PreCreateWindow(cs);
}

int TargetField::OnCreate([[maybe_unused]] LPCREATESTRUCT lpCreateStruct)
{
    if (static_cast<int>(Default()) == -1)
    {
        return -1;
    }

    CRect rect;
    GetWindowRect(&rect);
    ScreenToClient(&rect);
    CWnd* dialog = CWnd::FromHandle(::GetParent(CWnd::FromHandle(::GetParent(m_hWnd))->m_hWnd));
    SendMessage(WM_SETFONT, fontOf(*dialog), TRUE);
    // The button takes the box's last quarter.
    rect.left += rect.Width() * 3 / 4;
    rect.top += 2;
    rect.bottom -= 2;
    button.Create("...", BUTTON_STYLE, rect, this, TARGET_BUTTON_ID);
    button.SendMessage(WM_SETFONT, fontOf(*this), TRUE);

    if (target.objectId != -1)
    {
        SetWindowText(objectTypeName(static_cast<uint32_t>(target.objectId)));
    }

    ShowWindow(SW_SHOW);
    cellCol = target.cellCol;
    cellRow = target.cellRow;
    objectId = target.objectId;
    return 0;
}

void TargetField::OnPickButton()
{
    // A structure target picks a structure, a unit target a unit.
    requestPick((target.structure ? PICK_STRUCTURE : PICK_UNIT) | filter << 0x10);
}

bool TargetField::save(EditorMap& map)
{
    if (const EditorObject* object = map.objectAt(cellCol, cellRow); object != nullptr)
    {
        target.cellCol = object->cellCol;
        target.cellRow = object->cellRow;
        target.objectId = objectId;
        return true;
    }

    target.objectId = -1;
    target.cellCol = -1;
    target.cellRow = -1;
    return true;
}

LRESULT TargetField::OnPicked(WPARAM wParam, LPARAM lParam)
{
    const POINT& cell = *reinterpret_cast<const POINT*>(lParam);
    cellCol = cell.x;
    cellRow = cell.y;

    if (cellCol == -1)
    {
        cellCol = -1;
        cellRow = -1;
        objectId = -1;
        SetWindowText(nullptr);
    }
    else
    {
        const auto id = static_cast<uint32_t>(wParam);
        SetWindowText(objectTypeName(id));
        objectId = static_cast<int32_t>(id);
        showOwnerDialog();

        // The next fields (the following targets) show once one is picked.
        if (CWnd* next = GetWindow(GW_HWNDNEXT); next != nullptr)
        {
            next->ShowWindow(SW_SHOW);

            if (CWnd* after = next->GetWindow(GW_HWNDNEXT); after != nullptr)
            {
                after->ShowWindow(SW_SHOW);
            }
        }
    }

    showOwnerDialog();
    return 0;
}

void TargetField::OnRButtonDown(UINT nFlags, CPoint point)
{
    SetWindowText(nullptr);
    ParameterField::OnRButtonDown(nFlags, point);
    cellCol = -1;
    cellRow = -1;
    objectId = -1;
}

BoolField::BoolField(BoolParameter& value) : ParameterField(value), value(value)
{
}

BOOL BoolField::createField(DWORD style, const RECT& rect, CWnd* parent, UINT id)
{
    CRect half = rect;
    half.right = (rect.right - rect.left) / 2 + rect.left;
    // The original passes the box's style to the label too, which doesn't use it.
    static_cast<void>(style);
    createLabel(rect, parent);
    const BOOL created = Create("BUTTON", nullptr, CHECK_STYLE, half, parent, id, nullptr);
    SendMessage(BM_SETCHECK, value.value ? BST_CHECKED : BST_UNCHECKED, 0);
    return created;
}

bool BoolField::save([[maybe_unused]] EditorMap& map)
{
    value.value = SendMessage(BM_GETCHECK, 0, 0) == BST_CHECKED;
    return true;
}

DirectionField::DirectionField(DirectionParameter& direction) : ParameterField(direction), direction(direction)
{
}

BOOL DirectionField::createField(DWORD style, const RECT& rect, CWnd* parent, UINT id)
{
    CRect list = rect;
    list.bottom += LIST_DROP_HEIGHT;
    static_cast<void>(style);
    createLabel(rect, parent);
    const BOOL created = Create("ComboBox", nullptr, DIRECTION_STYLE, list, parent, id, nullptr);
    SendMessage(WM_SETFONT, fontOf(*parent), TRUE);

    for (int32_t i = 0; i < DIRECTION_COUNT; i++)
    {
        CString name;
        name.LoadString(IDS_NORTH + i);
        SendMessage(CB_INSERTSTRING, static_cast<WPARAM>(i), reinterpret_cast<LPARAM>(static_cast<LPCTSTR>(name)));
    }

    SendMessage(CB_SETCURSEL, static_cast<WPARAM>(direction.value), 0);
    return created;
}

bool DirectionField::save([[maybe_unused]] EditorMap& map)
{
    direction.value = static_cast<long>(SendMessage(CB_GETCURSEL, 0, 0));
    return true;
}
