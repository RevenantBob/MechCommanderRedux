#include "stdafx.h"
#include "SelectMissionDialog.h"
#include "DaoDatabase.h"
#include "MissionRecordSet.h"
#include "UnitRecord.h"
#include "resource.h"
#include "EditorFiles.h"

namespace
{
    /// <summary>The size of the list's icons.</summary>
    inline constexpr int ICON_WIDTH = 11;
    inline constexpr int ICON_HEIGHT = 12;
    /// <summary>How many icons the list's image list starts with, and grows by.</summary>
    inline constexpr int ICON_COUNT = 2;

    /// <summary>The files a map is made of in Data\Terrain, deleted with its mission (".log24.tga" twice).</summary>
    inline constexpr const char* MAP_FILE_EXTENSIONS[] = {
        ".elv", ".pre", ".bdg", ".gmm", ".dat", ".tga", ".log24.tga", ".log24.tga", ".log.tga", ".obj", ".fit"};
    /// <summary>The files a mission is made of in Data\Missions, deleted with it.</summary>
    inline constexpr const char* MISSION_FILE_SUFFIXES[] = {".abl",      ".fit",     "_MP.abi", "_LOP.abi",
                                                            "_INIT.abi", "_STR.abi", "_Var.abi"};

    /// <summary>Loads icon <paramref name="id"/> at the list's size and adds it to <paramref name="images"/>.</summary>
    void addIcon(CImageList& images, HINSTANCE instance, UINT id)
    {
        const auto icon = static_cast<HICON>(
            LoadImage(instance, MAKEINTRESOURCE(id), IMAGE_ICON, ICON_WIDTH, ICON_HEIGHT, LR_CREATEDIBSECTION));
        ImageList_ReplaceIcon(images.m_hImageList, -1, icon);
        // Original behaviour (EB-055): the icon is freed with DeleteObject, which fails for an icon.
        DeleteObject(icon);
    }

    /// <summary>The game folder the editor works in (the original's Paths\DataPath), without a backslash.</summary>
    CString settingsDataPath()
    {
        return EditorDataPath();
    }
}

SelectMissionDialog::SelectMissionDialog(DaoDatabase* database, int mode, CWnd* pParent)
    : CDialog(IDD, pParent), _database(database), _mode(mode)
{
    selectedName = "";
    _icons.Create(ICON_WIDTH, ICON_HEIGHT, ILC_COLOR8, ICON_COUNT, ICON_COUNT);
    const HINSTANCE instance = GetModuleHandle(nullptr);
    addIcon(_icons, instance, IDI_SOLO_MISSION);
    addIcon(_icons, instance, IDI_MULTIPLAYER_MISSION);
}

BEGIN_MESSAGE_MAP(SelectMissionDialog, CDialog)
ON_NOTIFY(LVN_KEYDOWN, IDC_MISSION_LIST, &SelectMissionDialog::OnKeydownMissionList)
ON_NOTIFY(LVN_ITEMCHANGED, IDC_MISSION_LIST, &SelectMissionDialog::OnItemchangedMissionList)
ON_BN_CLICKED(IDC_DELETE, &SelectMissionDialog::OnDelete)
ON_NOTIFY(NM_DBLCLK, IDC_MISSION_LIST, &SelectMissionDialog::OnDblclkMissionList)
END_MESSAGE_MAP()

void SelectMissionDialog::DoDataExchange(CDataExchange* pDX)
{
    DDX_Control(pDX, IDOK, _okButton);
    DDX_Control(pDX, IDC_MISSION_PREVIEW, _preview);
    DDX_Control(pDX, IDC_MISSION_LIST, _list);
}

BOOL SelectMissionDialog::OnInitDialog()
{
    CDialog::OnInitDialog();
    _list.SetImageList(&_icons, LVSIL_SMALL);
    MissionRecordSet missions(_database);
    // Original behaviour: the filter is made and never used; the mode is tested row by row.
    CString filter;
    filter.Format("[Multiplayer] = %d", _mode == MULTIPLAYER_MISSIONS);
    int count = 0;

    if (_mode == MAPS)
    {
        missions.Open(dbOpenDynaset, nullptr, 0);
        CString pattern = settingsDataPath();
        pattern += "\\Data\\Terrain\\*.fit";
        CFileFind finder;

        if (finder.FindFile(pattern, 0))
        {
            BOOL more;

            do
            {
                more = finder.FindNextFile();
                CString name = finder.GetFileName();
                name.MakeLower();
                name.Replace(".fit", "");
                _list.InsertItem(LVIF_TEXT, count, name, 0, 0, 0, 0);
                count++;
            } while (more);
        }
    }
    else
    {
        missions.Open(dbOpenDynaset, nullptr, 0);

        if (missions.IsOpen())
        {
            try
            {
                missions.MoveFirst();

                do
                {
                    COleVariant value;
                    missions.GetFieldValue("MissionName", value);
                    const CString name(value.bstrVal);

                    if (missions.m_Multiplayer == _mode)
                    {
                        _list.InsertItem(LVIF_TEXT | LVIF_IMAGE, count, name, 0, 0, missions.m_Multiplayer, 0);
                        count++;
                    }

                    missions.MoveNext();
                } while (!missions.IsEOF());
            }
            catch (CException* e)
            {
                // No missions at all (MoveFirst finds no record) or a database error.
                CString message;
                message.Format(IDS_CURRENTLY_THERE_ARE_NO);
                CString title;
                title.LoadString(IDS_MCEDITOR);
                MessageBox(message, title, MB_OK);
                EndDialog(IDCANCEL);
                e->Delete();
                return FALSE;
            }
        }
    }

    if (_list.GetItemCount() == 0)
    {
        CString message;
        message.Format(IDS_CURRENTLY_THERE_ARE_NO);
        CString title;
        title.LoadString(IDS_MCEDITOR);
        MessageBox(message, title, MB_OK);
        EndDialog(IDCANCEL);
    }

    _okButton.EnableWindow(FALSE);
    _list.RedrawItems(0, count);
    return TRUE;
}

void SelectMissionDialog::takeSelectedName()
{
    for (int i = 0; i < _list.GetItemCount(); i++)
    {
        if (_list.GetItemState(i, LVIS_SELECTED) != 0)
        {
            selectedName = _list.GetItemText(i, 0);
        }
    }
}

void SelectMissionDialog::OnOK()
{
    takeSelectedName();
    CDialog::OnOK();
}

void SelectMissionDialog::OnCancel()
{
    UpdateData(TRUE);
    CDialog::OnCancel();
}

void SelectMissionDialog::OnKeydownMissionList(NMHDR* pNMHDR, LRESULT* pResult)
{
    const auto keyDown = reinterpret_cast<NMLVKEYDOWN*>(pNMHDR);

    if (keyDown->wVKey == VK_DELETE)
    {
        for (int i = 0; i < _list.GetItemCount(); i++)
        {
            if (_list.GetItemState(i, LVIS_SELECTED) != 0)
            {
                selectedName = _list.GetItemText(i, 0);
                _okButton.EnableWindow(TRUE);
            }
        }

        deleteMission();
    }

    *pResult = 0;
}

void SelectMissionDialog::OnItemchangedMissionList(NMHDR* pNMHDR, LRESULT* pResult)
{
    const auto listView = reinterpret_cast<NMLISTVIEW*>(pNMHDR);
    const CString name = _list.GetItemText(listView->iItem, 0);
    CString path = settingsDataPath();
    path += "\\Data\\Terrain\\";
    path += name;
    path += ".tga";
    _preview.load(path);
    ::RedrawWindow(_preview.m_hWnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
    _okButton.EnableWindow(TRUE);
    *pResult = 0;
}

void SelectMissionDialog::OnDelete()
{
    takeSelectedName();
    deleteMission();
}

void SelectMissionDialog::OnDblclkMissionList(NMHDR* pNMHDR, LRESULT* pResult)
{
    if (reinterpret_cast<NMITEMACTIVATE*>(pNMHDR)->iItem != -1)
    {
        OnItemchangedMissionList(pNMHDR, pResult);
        OnOK();
    }

    *pResult = 0;
}

void SelectMissionDialog::deleteMission()
{
    if (selectedName.IsEmpty())
    {
        return;
    }

    CString question;
    question.Format(IDS_DELETE_MISSION_S, static_cast<const char*>(selectedName));
    CString title;
    title.LoadString(IDS_MCEDITOR);

    if (MessageBox(question, title, MB_YESNO) != IDYES)
    {
        return;
    }

    const int item = _list.GetNextItem(-1, LVNI_SELECTED);
    // The original asks for the next selected item too and ignores the answer.
    _list.GetNextItem(item, LVNI_SELECTED);

    if (item == -1)
    {
        return;
    }

    _list.DeleteItem(item);
    const CString dataPath = settingsDataPath();
    const CString missionBase = dataPath + "\\Data\\Missions\\" + selectedName;
    const CString mapBase = dataPath + "\\Data\\Terrain\\" + selectedName;

    for (const char* extension : MAP_FILE_EXTENSIONS)
    {
        DeleteFile(mapBase + extension);
    }

    for (const char* suffix : MISSION_FILE_SUFFIXES)
    {
        DeleteFile(missionBase + suffix);
    }

    // No backslash after "Campaign", as in the original.
    DeleteFile(dataPath + "\\Data\\Missions\\Campaign" + selectedName + ".fit");
    DeleteFile(missionBase + ".txt");
    DeleteFile(dataPath + "\\Data\\Missions\\is" + selectedName + ".txt");
    DeleteFile(dataPath + "\\Data\\Missions\\c" + selectedName + ".txt");
    DeleteFile(dataPath + "\\Data\\SaveGame\\" + selectedName + ".mpk");
    DeleteFile(dataPath + "\\Data\\SaveGame\\" + selectedName + ".sol");

    MissionRecordSet missions(_database);
    missions.Open(dbOpenDynaset, nullptr, 0);
    CString missionFilter;
    missionFilter.Format("[MissionName] = '%s'", static_cast<const char*>(selectedName));
    UnitRecord units(_database);
    units.Open(dbOpenDynaset, nullptr, 0);
    const CString warriorsPath = dataPath + "\\Data\\Missions\\Warriors\\";

    if (missions.FindFirst(missionFilter))
    {
        // Original behaviour: the units are looked up by the first mission's id for every mission of the name.
        CString unitFilter;
        unitFilter.Format("[MissionID] = %ld", missions.m_MissionID);

        do
        {
            if (units.FindFirst(unitFilter))
            {
                do
                {
                    DeleteFile(warriorsPath + units.m_BrainFile + ".abl");
                } while (units.FindNext(unitFilter));
            }

            missions.Delete();
        } while (missions.FindNext(missionFilter));
    }

    selectedName = "";
    UpdateData(FALSE);
}
