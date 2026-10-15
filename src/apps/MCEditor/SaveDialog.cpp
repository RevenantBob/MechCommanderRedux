#include "stdafx.h"
#include "SaveDialog.h"
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
    /// <summary>The most characters a map's name may have.</summary>
    inline constexpr int NAME_LIMIT = 8;
    /// <summary>A map file's extension.</summary>
    const char* const MAP_EXTENSION = ".fit";

    /// <summary>The files a map is made of in Data\Terrain, deleted with it (".log24.tga" twice).</summary>
    inline constexpr const char* MAP_FILE_EXTENSIONS[] = {
        ".elv", ".pre", ".bdg", ".gmm", ".dat", ".tga", ".log24.tga", ".log24.tga", ".log.tga", ".obj", ".fit"};
    /// <summary>The files a mission is made of in Data\Missions, deleted with it.</summary>
    inline constexpr const char* MISSION_FILE_SUFFIXES[] = {".abl",      ".fit",     "_MP.abi", "_LOP.abi",
                                                            "_INIT.abi", "_STR.abi", "_Var.abi"};

    /// <summary>The game's own maps and missions, which a map may not be saved as (any case) (0x0052f528).</summary>
    inline constexpr const char* RESERVED_NAMES[] = {
        "AceHunt",  "Achilles", "Bakwoods",  "brf0101",   "brf0102",   "brf0103",  "brf0104",  "brf0105",  "brf0106",
        "brf0201",  "brf0202",  "brf0203",   "brf0204",   "brf0205",   "brf0206",  "brf0301",  "brf0302",  "brf0303",
        "brf0304",  "brf0304c", "brf0304s",  "brf0305",   "brf0306",   "brf0401",  "brf0402",  "brf0403",  "brf0404",
        "brf0405",  "brf0406",  "brf0501",   "brf0502",   "brf0503",   "brf0504",  "brf0505",  "brf0506",  "caldera",
        "campaign", "CaptIsld", "Cbrf0701",  "Cbrf0702",  "Cbrf0703",  "Cbrf0704", "Cbrf0705", "Cbrf0706", "Cbrf0708",
        "Cbrf0709", "Cbrf0712", "Cbrf0719",  "Cbrf0720",  "Cbrf0721",  "Cbrf0722", "Cbrf0723", "Cbrf0725", "Cbrf0726",
        "Cbrf0727", "Cbrf0728", "Cbrf0729",  "Cbrf0730",  "ccaldera",  "cDValley", "cHvrDmmt", "clanpur",  "Cmxm0502",
        "cUrbana",  "DValley",  "E3_0101",   "E3_0101a",  "E3_0101A",  "E3_0203",  "editscen", "EDITSCEN", "gamesys",
        "HvrDmmt",  "ispur",    "isUrbana",  "KingIsle",  "MCX0101",   "MCX0102",  "MCX0103",  "MCX0104",  "MCX0201",
        "MCX0202",  "MCX0203",  "MCX0204",   "MCX0301",   "MCX0302",   "MCX0303",  "MCX0304",  "MCXS0401", "MCXS0401",
        "MCXS0402", "MCXS0402", "MCXS0404",  "mechcmdr1", "mechint",   "MIS0101",  "MIS0101",  "MIS0101c", "MIS0101n",
        "MIS0101n", "MIS0101s", "MIS0102",   "MIS0102",   "mis0103",   "MIS0103",  "MIS0104",  "MIS0104",  "MIS0105",
        "MIS0105",  "MIS0106",  "MIS0106",   "MIS0201",   "MIS0201",   "MIS0202",  "MIS0202",  "MIS0203",  "MIS0203",
        "MIS0204",  "MIS0204",  "MIS0205",   "MIS0205",   "MIS0206",   "MIS0206",  "MIS0301",  "MIS0301",  "MIS0302",
        "MIS0302",  "MIS0303",  "MIS0303",   "MIS0304",   "MIS0304",   "MIS0305",  "MIS0305",  "MIS0306",  "MIS0306",
        "mis0401",  "MIS0401",  "MIS0402",   "MIS0402",   "MIS0403",   "MIS0403",  "MIS0403c", "MIS0403n", "MIS0403s",
        "MIS0404",  "MIS0405",  "MIS0406",   "mis0501",   "MIS0501",   "MIS0502",  "MIS0503",  "MIS0504",  "MIS0505",
        "MIS0506",  "mis0507",  "MIS0507",   "MIS0801",   "miscfunc",  "misconst", "MISE303",  "MouseTrp", "MP0304",
        "MP0701",   "MP0702",   "MP0703",    "MP0705",    "MP0706",    "MP0707",   "MP0708",   "MP0709",   "MP0712",
        "MP0714",   "MP0715",   "MP0716",    "MP0718",    "MP0719",    "MP0720",   "MP0721",   "MP0722",   "MP0723",
        "MP0725",   "MP0726",   "MP0727",    "MP0728",    "MP0729",    "MP0730",   "MP0731",   "MP7_12MP", "MP7_14MP",
        "MP7_15MP", "MP7_16MP", "MP7_18MP",  "MP7_19MP",  "MP7_1ACT",  "MP7_1CRE", "MP7_1LOP", "MP7_1MP",  "MP7_1STR",
        "MP7_1VAR", "MP7_20MP", "MP7_21MP",  "MP7_22MP",  "MP7_23MP",  "MP7_25MP", "MP7_26MP", "MP7_27MP", "MP7_28MP",
        "MP7_29MP", "MP7_2ACT", "MP7_2CRE",  "MP7_2LOP",  "MP7_2MP",   "MP7_2STR", "MP7_2VAR", "MP7_30MP", "MP7_31MP",
        "MP7_3ACT", "MP7_3CRE", "MP7_3LOP",  "MP7_3MP",   "MP7_3STR",  "MP7_3VAR", "MP7_5ACT", "MP7_5CRE", "MP7_5LOP",
        "MP7_5MP",  "MP7_5STR", "MP7_5VAR",  "MP7_6ACT",  "MP7_6CRE",  "MP7_6LOP", "MP7_6MP",  "MP7_6STR", "MP7_6VAR",
        "MP7_7ACT", "MP7_7CRE", "MP7_7LOP",  "MP7_7MP",   "MP7_7STR",  "MP7_7VAR", "MP7_8ACT", "MP7_8CRE", "MP7_8LOP",
        "MP7_8MP",  "MP7_8STR", "MP7_8VAR",  "MP7_9ACT",  "MP7_9CRE",  "MP7_9LOP", "MP7_9MP",  "MP7_9STR", "MP7_9VAR",
        "mpmech",   "MXM0502",  "netpur",    "OConst",    "orders",    "OType",    "OVar",     "PARedux",  "purchase",
        "std.h",    "testmis1", "TheGuns",   "UBInit",    "Urbana",    "WetNWild", "xbrf0101", "xbrf0102", "xbrf0103",
        "xbrf0104", "xbrf0201", "xbrf0202",  "xbrf0203",  "xbrf0204",  "xbrf0301", "xbrf0302", "xbrf0303", "xbrf0304",
        "xbrf0305", "xpur",     "xsbrf0401", "xsbrf0402", "xsbrf0404", "AceHunt",  "Achilles", "Arena",    "Bakwoods",
        "BLOODGRW", "BRCHLINE", "BrcLinNA",  "CACHKILL",  "caldera",   "CaptIsld", "CPTBASNA", "CPTBSDEA", "CptrBase",
        "CptrLOCK", "DValley",  "FORT",      "FortNA",    "GunBunk",   "HvrDmmt",  "IndStkNA", "INDYSTRK", "KingHill",
        "KingIsle", "KingLOCK", "ManyBase",  "ManyLOCK",  "MNYBSENA",  "MouseTrp", "PARedux",  "PowrLOCK", "POWRSRG2",
        "PowrSurg", "PwrSrgNA", "Random",    "start0",    "SWAMPNA",   "SWAMPRAT", "TheGuns",  "THREE",    "ThreeNA",
        "Urbana",   "WetNWild", "xstart0",   "Achilles",  "Bakwoods",  "caldera",  "CaptIsld", "DValley",  "HvrDmmt",
        "KingIsle", "m0101",    "m0102",     "M0103",     "m0103",     "M0103",    "m0104",    "m0105",    "m0106",
        "m0201",    "m0202",    "M0203",     "m0204",     "m0205",     "m0206",    "m0301",    "m0302",    "m0303",
        "m0304",    "m0305",    "m0306",     "m0401",     "m0402",     "m0403",    "m0404",    "m0405",    "m0406",
        "m0501",    "m0502",    "m0503",     "m0504",     "m0505",     "m0506",    "m0506m",   "Map0101A", "MCXS0404",
        "MouseTrp", "mp0701",   "MP0702",    "Mp0702",    "MP0702",    "Mp0702",   "mp0703",   "mp0704",   "mp0705",
        "mp0706",   "mp0707",   "mp0708",    "MP0709",    "mp0712",    "MP0719",   "mp0720",   "MP0721",   "MP0722",
        "MX0101",   "MX0102",   "mx0103",    "MX0104",    "mx0201",    "MX0202",   "mx0203",   "MX0204",   "MX0301",
        "mx0302",   "MX0303",   "MX0305",    "MXS001",    "mxs0402",   "Random",   "Swamp",    "Urbana",   "WetNWild"};

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

    /// <summary>The character's class as the original's CRT tested it (a char above 127 is in no class).</summary>
    bool isClass(char c, int (*test)(int))
    {
        // EB-111 fixed (UB): the original passes a char above 127 as a negative index into the class table.
        return test(static_cast<unsigned char>(c)) != 0;
    }
}

SaveDialog::SaveDialog(DaoDatabase* database, bool saving, CWnd* pParent)
    : CDialog(IDD, pParent), _saving(saving), _database(database)
{
    _name = "";
    _icons.Create(ICON_WIDTH, ICON_HEIGHT, ILC_COLOR8, ICON_COUNT, ICON_COUNT);
    const HINSTANCE instance = GetModuleHandle(nullptr);
    addIcon(_icons, instance, IDI_SOLO_MISSION);
    addIcon(_icons, instance, IDI_MULTIPLAYER_MISSION);
}

BEGIN_MESSAGE_MAP(SaveDialog, CDialog)
ON_NOTIFY(LVN_ITEMCHANGED, IDC_SAVE_FILES, &SaveDialog::OnItemchangedSaveFiles)
ON_NOTIFY(LVN_KEYDOWN, IDC_SAVE_FILES, &SaveDialog::OnKeydownSaveFiles)
ON_NOTIFY(NM_DBLCLK, IDC_SAVE_FILES, &SaveDialog::OnDblclkSaveFiles)
END_MESSAGE_MAP()

void SaveDialog::DoDataExchange(CDataExchange* pDX)
{
    DDX_Control(pDX, IDC_SAVE_FILES, _list);
    DDX_Text(pDX, IDC_MAP_NAME, _name);
    DDV_MaxChars(pDX, _name, NAME_LIMIT);
}

BOOL SaveDialog::OnInitDialog()
{
    CDialog::OnInitDialog();
    CString pattern = settingsDataPath();
    pattern += "\\Data\\Terrain\\*.fit";
    _list.SetImageList(&_icons, LVSIL_SMALL);
    CFileFind finder;

    if (!finder.FindFile(pattern, 0))
    {
        if (!_saving)
        {
            CString message;
            message.LoadString(IDS_CURRENTLY_THERE_ARE_NO);
            CString title;
            title.LoadString(IDS_MCEDITOR);
            MessageBox(message, title, MB_OK);
            EndDialog(IDCANCEL);
        }

        return TRUE;
    }

    int count = 0;
    BOOL more;

    do
    {
        more = finder.FindNextFile();
        CString name = finder.GetFileName();
        name = name.Left(name.GetLength() - static_cast<int>(std::strlen(MAP_EXTENSION)));
        _list.InsertItem(LVIF_TEXT, count, name, 0, 0, 0, 0);
        count++;
    } while (more);

    return TRUE;
}

void SaveDialog::OnOK()
{
    if (!UpdateData(TRUE))
    {
        return;
    }

    CString title;
    title.LoadString(IDS_MCEDITOR);
    const auto refuse = [&](const CString& message) { MessageBox(message, title, MB_OK); };

    if (_name.IsEmpty())
    {
        CString message;
        message.LoadString(IDS_PLEASE_ENTER_A_NAME);
        refuse(message);
        return;
    }

    mapPath = _name;

    for (const char* reserved : RESERVED_NAMES)
    {
        if (_stricmp(mapPath, reserved) == 0)
        {
            CString message;
            message.Format(IDS_THE_S_FILE_IS, static_cast<const char*>(mapPath));
            refuse(message);
            return;
        }
    }

    if (_saving)
    {
        for (int i = 0; i < mapPath.GetLength(); i++)
        {
            const char c = mapPath[i];

            if (!isClass(c, isdigit) && !isClass(c, isalpha) && c != '_')
            {
                CString message;

                if (isClass(c, isspace))
                {
                    message.LoadString(IDS_SPACES_CANNOT_BE_USED);
                }
                else
                {
                    message.Format(IDS_CS_CANNOT_BE_USED, c);
                }

                refuse(message);
                return;
            }
        }

        if (!isClass(mapPath[0], isalpha))
        {
            CString message;
            message.LoadString(IDS_FILE_NAME_MUST_START_WITH_LETTER);
            refuse(message);
            return;
        }
    }

    if (mapPath.Find(MAP_EXTENSION) == -1)
    {
        mapPath += MAP_EXTENSION;
    }

    if (mapPath.Find(' ') != -1)
    {
        CString message;
        message.LoadString(IDS_SPACES_CANNOT_BE_USED);
        refuse(message);
        return;
    }

    if (mapPath.IsEmpty())
    {
        return;
    }

    mapPath = settingsDataPath() + "\\Data\\Terrain\\" + mapPath;
    CFileFind finder;

    if (!finder.FindFile(mapPath, 0))
    {
        if (_saving)
        {
            CDialog::OnOK();
        }
        else
        {
            CString message;
            message.LoadString(IDS_THE_SPECIFIED_FILE_WAS);
            refuse(message);
        }

        return;
    }

    if (!_saving)
    {
        CDialog::OnOK();
        return;
    }

    CString question;
    question.Format(IDS_OVERWRITE_THE_S_FILE, static_cast<const char*>(_name));

    if (MessageBox(question, title, MB_YESNO) == IDYES)
    {
        CDialog::OnOK();
    }
}

void SaveDialog::OnItemchangedSaveFiles(NMHDR* pNMHDR, LRESULT* pResult)
{
    const auto* item = reinterpret_cast<NMLISTVIEW*>(pNMHDR);
    UpdateData(TRUE);
    _name = _list.GetItemText(item->iItem, 0);
    UpdateData(FALSE);
    *pResult = 0;
}

void SaveDialog::OnKeydownSaveFiles(NMHDR* pNMHDR, LRESULT* pResult)
{
    const auto* key = reinterpret_cast<NMLVKEYDOWN*>(pNMHDR);

    if (key->wVKey == VK_DELETE)
    {
        UpdateData(TRUE);

        if (!_name.IsEmpty())
        {
            CString question;
            question.Format(IDS_DELETE_MISSION_S, static_cast<const char*>(_name));
            CString title;
            title.LoadString(IDS_MCEDITOR);

            if (MessageBox(question, title, MB_YESNO) == IDYES)
            {
                const int item = _list.GetNextItem(-1, LVNI_SELECTED);
                // The original asks for the next selected item too and ignores the answer.
                _list.GetNextItem(item, LVNI_SELECTED);

                if (item == -1)
                {
                    return;
                }

                _list.DeleteItem(item);

                if (const int dot = _name.ReverseFind('.'); dot != -1)
                {
                    _name = _name.Left(dot);
                }

                const CString dataPath = settingsDataPath();
                const CString missionBase = dataPath + "\\Data\\Missions\\" + _name;
                const CString mapBase = dataPath + "\\Data\\Terrain\\" + _name;

                for (const char* extension : MAP_FILE_EXTENSIONS)
                {
                    DeleteFile(mapBase + extension);
                }

                for (const char* suffix : MISSION_FILE_SUFFIXES)
                {
                    DeleteFile(missionBase + suffix);
                }

                // No backslash after "Campaign", as in the original.
                DeleteFile(dataPath + "\\Data\\Missions\\Campaign" + _name + ".fit");
                DeleteFile(missionBase + ".txt");
                DeleteFile(dataPath + "\\Data\\Missions\\is" + _name + ".txt");
                DeleteFile(dataPath + "\\Data\\Missions\\c" + _name + ".txt");
                DeleteFile(dataPath + "\\Data\\SaveGame\\" + _name + ".mpk");
                DeleteFile(dataPath + "\\Data\\SaveGame\\" + _name + ".sol");

                // Original behaviour (EB-112): the mission's and its units' records are opened but not deleted, and
                // its warriors' brain scripts stay; only its Params.fit goes.
                MissionRecordSet missions(_database);
                missions.Open(dbOpenDynaset, nullptr, 0);
                CString missionFilter;
                missionFilter.Format("[MissionName] = '%s'", static_cast<const char*>(_name));
                UnitRecord units(_database);
                units.Open(dbOpenDynaset, nullptr, 0);
                DeleteFile(dataPath + "\\Data\\Missions\\Warriors\\" + _name + "Params.fit");
                _name = "";
                UpdateData(FALSE);
            }
        }
    }

    *pResult = 0;
}

void SaveDialog::OnDblclkSaveFiles(NMHDR* pNMHDR, LRESULT* pResult)
{
    OnItemchangedSaveFiles(pNMHDR, pResult);
    *pResult = 0;
    OnOK();
}
