#include "stdafx.h"
#include "SingleView.h"
#include "DreditorDoc.h"
#include "EditorMap.h"
#include "ObjectivesTeamDialog.h"
#include "DistributionFile.h"
#include "MissionRecordSet.h"
#include "SettingsPlayerDialog.h"
#include "ValueDialog.h"
#include "resource.h"
#include "EditorFiles.h"

#include <direct.h>
#include <mmsystem.h>
#include <rpc.h>

namespace
{
    /// <summary>The distribution file's extension and the save dialog's filter.</summary>
    const char* const DPK_EXTENSION = ".dpk";
    const char* const DPK_FILTER = "MCX Distribution Files (*.dpk)|*.dpk|All Files (*.*)|*.*||";
    /// <summary>The folders the mission's files are in, from the editor's current folder.</summary>
    const char* const MISSIONS_FOLDER = "data\\missions\\";
    const char* const TERRAIN_FOLDER = "data\\terrain\\";
    const char* const WARRIORS_FOLDER = "data\\missions\\warriors\\";
    /// <summary>A campaign file's name before the mission's.</summary>
    const char* const CAMPAIGN_PREFIX = "data\\missions\\campaign";
    /// <summary>Where the mission's campaign file is kept while the .dpk is made.</summary>
    const char* const CAMPAIGN_HOLDER = "data\\missions\\quasitover8.holder";
    /// <summary>The list of files for the FastFile packer.</summary>
    const char* const RESPONSE_FILE = "work.rsp";
    /// <summary>The mission's files in the missions and terrain folders.</summary>
    const char* const MISSION_EXTENSIONS[] = {"_VAR.ABI", "_STR.ABI", "_MP.ABI", "_INIT.ABI", "_LOP.ABI", ".TXT"};
    const char* const TERRAIN_EXTENSIONS[] = {".BDG", ".DAT", ".ELV", ".FIT",    ".GMM",
                                              ".OBJ", ".PRE", ".TGA", ".LOG.TGA"};
}

IMPLEMENT_DYNCREATE(SingleView, DreditorView)

SingleView::SingleView() = default;

bool SingleView::askValue(UINT& value, UINT command, UINT low, UINT high)
{
    ValueDialog dialog(ValueDialog::commandTitle(command), false, low, high, ValueDialog::NO_RANGE,
                       ValueDialog::NO_RANGE, this);
    dialog.first = value;

    if (dialog.DoModal() != IDOK)
    {
        return false;
    }

    value = dialog.first;
    return true;
}

CMenu* SingleView::popupMenu(EditorObject* object, CMenu& menu)
{
    return loadPopupMenu(object, menu, IDR_MISSION_POPUP, IDR_BUILDING_POPUP);
}

BEGIN_MESSAGE_MAP(SingleView, DreditorView)
ON_COMMAND(ID_MISSION_OBJECTIVES_TEAM_1, &SingleView::OnMissionObjectivesTeam1)
ON_COMMAND(ID_FILE_OPEN, &SingleView::OnFileOpen)
ON_COMMAND(ID_MISSION_SETTINGS_TEAM_1, &SingleView::OnMissionSettingsTeam1)
ON_COMMAND(ID_FILE_WRITE_DISTRIBUTION_FILE_DPK, &SingleView::OnFileWriteDistributionFileDpk)
END_MESSAGE_MAP()

void SingleView::OnMissionObjectivesTeam1()
{
    delete objectivesDialog;
    objectivesDialog = new ObjectivesTeamDialog(*map(), this, false, false);

    // The dialog is titled as the menu command, without its '&'.
    CString title;
    CMenu* menu = GetParent()->GetMenu();
    menu->GetMenuString(ID_MISSION_OBJECTIVES_TEAM_1, title, MF_BYCOMMAND);
    title.Remove('&');
    objectivesDialog->Create(ObjectivesTeamDialog::IDD, this);
    objectivesDialog->SetWindowText(title);
    m_pDocument->SetModifiedFlag(TRUE);
    pickingForUnit = false;
}

void SingleView::OnFileOpen()
{
    static_cast<DreditorDoc*>(m_pDocument)->openMission(false, false);
}

void SingleView::OnMissionSettingsTeam1()
{
    auto& singleMap = static_cast<SingleMap&>(*map());
    MissionSettings settings = singleMap.settings();
    SettingsPlayerDialog dialog(singleMap.missionText, settings, false, IDS_SETTINGS, this, true);

    if (dialog.DoModal() == IDOK)
    {
        singleMap.setSettings(settings);
    }

    m_pDocument->SetModifiedFlag(TRUE);
}

void SingleView::OnFileWriteDistributionFileDpk()
{
    CString name = m_pDocument->GetTitle();

    if (name.IsEmpty())
    {
        CString message;
        message.LoadString(IDS_THE_MISSION_MUST_BE);
        CString caption;
        caption.LoadString(IDS_MCEDITOR);
        MessageBox(message, caption, MB_ICONERROR);
        return;
    }

    // The mission's name without its folder and extension.
    if (const int slash = name.ReverseFind('\\'); slash != -1)
    {
        name = name.Right(name.GetLength() - slash - 1);
    }

    if (name.GetLength() > 3 && name[name.GetLength() - 4] == '.')
    {
        name = name.Left(name.GetLength() - 4);
    }

    // Where the .dpk goes: asked until a file there can be made.
    CString folder;
    CString dpkName;
    CFileDialog dialog(FALSE, DPK_EXTENSION, name + DPK_EXTENSION,
                       OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_OVERWRITEPROMPT, DPK_FILTER, nullptr);
    bool chosen = false;

    while (!chosen)
    {
        if (dialog.DoModal() != IDOK)
        {
            return;
        }

        folder = dialog.GetPathName();
        CFile test;
        CFileException exception;

        if (test.Open(folder, CFile::modeCreate | CFile::modeNoTruncate, &exception))
        {
            if (const int slash = folder.ReverseFind('\\'); slash != -1)
            {
                dpkName = folder.Right(folder.GetLength() - slash - 1);
                dpkName.Replace(DPK_EXTENSION, "");
                folder = folder.Left(slash + 1);
            }
            else
            {
                dpkName = folder;
                dpkName.Replace(DPK_EXTENSION, "");
                folder.Empty();
            }

            chosen = true;
        }
    }

    BeginWaitCursor();

    // The files are copied under a GUID, so the mission's own stay as they are.
    UUID uuid;
    UuidCreate(&uuid);
    RPC_CSTR uuidText = nullptr;
    UuidToStringA(&uuid, &uuidText);
    CString guid = reinterpret_cast<const char*>(uuidText);
    guid.Replace('-', '_');
    RpcStringFreeA(&uuidText);

    MissionRecordSet missions(&static_cast<DreditorDoc*>(m_pDocument)->database);
    missions.Open(dbOpenDynaset, nullptr, 0);

    if (!missions.FindFirst("[MissionName] = '" + name + "'"))
    {
        CString message;
        message.LoadString(IDS_COULDNT_FIND_THE_PROFILE);
        CString caption;
        caption.LoadString(IDS_MCEDITOR);
        MessageBox(message, caption, MB_OK);
        return;
    }

    for (const char* extension : MISSION_EXTENSIONS)
    {
        copyMissionFile(name, guid, MISSIONS_FOLDER, extension);
    }

    for (const char* extension : TERRAIN_EXTENSIONS)
    {
        copyMissionFile(name, guid, TERRAIN_FOLDER, extension);
    }

    copyMissionFile(name, guid, WARRIORS_FOLDER, "Params.fit");

    // The copies' scripts and FIT files name the copies. The first rewrite lower-cases the name.
    CStringArray lines;
    lines.SetSize(0);
    lines.Add("#include_ \"" + name + "_VAR.ABI\"");
    lines.Add("#include_ \"" + name + "_INIT.ABI\"");
    lines.Add("#include_ \"" + name + "_STR.ABI\"");
    lines.Add("#include_ \"" + name + "_LOP.ABI\"");
    rewriteMissionFile(name, guid, MISSIONS_FOLDER, ".ABL", lines);
    lines.SetSize(0);
    lines.Add("st TerrainFileName = \"" + name + "\"");
    lines.Add("st TacMapGifName = \"" + name + "\"");
    lines.Add("st ScenarioScript = \"" + name + "\"");
    lines.Add("st BriefingFile = \"" + name);
    lines.Add("st MapFile = \"" + name + "\"");
    lines.Add("st BrainParameterFile = \"" + name + "Params");
    rewriteMissionFile(name, guid, MISSIONS_FOLDER, ".FIT", lines);
    lines.SetSize(0);
    lines.Add("st Scenario0 = \"" + name + "\"");
    rewriteMissionFile(name, guid, CAMPAIGN_PREFIX, ".FIT", lines);

    // The campaign file is kept aside while the copy takes the .dpk's name.
    CFile::Rename(CAMPAIGN_PREFIX + name + ".FIT", CAMPAIGN_HOLDER);
    CFile::Rename(CAMPAIGN_PREFIX + guid + ".FIT", CAMPAIGN_PREFIX + dpkName + ".FIT");

    CStdioFile response;
    response.Open(RESPONSE_FILE, CFile::modeCreate | CFile::modeWrite);

    for (const char* extension :
         {"_VAR.ABI\n", "_STR.ABI\n", "_LOP.ABI\n", ".TXT\n", "_MP.ABI\n", "_INIT.ABI\n", ".ABL\n", ".FIT\n"})
    {
        response.WriteString(MISSIONS_FOLDER + guid + extension);
    }

    response.WriteString(CAMPAIGN_PREFIX + dpkName + ".FIT\n");

    for (const char* extension :
         {".BDG\n", ".DAT\n", ".ELV\n", ".FIT\n", ".GMM\n", ".OBJ\n", ".PRE\n", ".TGA\n", ".LOG.TGA\n"})
    {
        response.WriteString(TERRAIN_FOLDER + guid + extension);
    }

    response.WriteString(WARRIORS_FOLDER + guid + "Params.fit\n\n");
    response.Close();
    buildFastFile(RESPONSE_FILE, name + ".fst");

    // The copies go, from the terrain, missions and warriors folders.
    const CString copies = "*" + guid + "*.*";

    for (const char* subfolder : {"data\\terrain", "..\\missions", "warriors"})
    {
        _chdir(subfolder);
        CFileFind find;
        BOOL more = find.FindFile(copies);

        while (more)
        {
            more = find.FindNextFile();
            DeleteFileA(find.GetFilePath());
            GetLastError();
        }

        find.Close();
    }

    _chdir("..\\..\\..");
    DeleteFileA(CAMPAIGN_PREFIX + dpkName + ".FIT");
    CFile::Rename(CAMPAIGN_HOLDER, CAMPAIGN_PREFIX + name + ".Fit");

    // The .dpk: the mission's save and the FastFile of its files.
    response.Open(RESPONSE_FILE, CFile::modeCreate | CFile::modeWrite);
    response.WriteString("data\\savegame\\" + name + ".sol\n");
    response.WriteString(name + ".FST\n\n");
    response.Close();
    buildFastFile(RESPONSE_FILE, folder + dpkName + DPK_EXTENSION);
    DeleteFileA(RESPONSE_FILE);
    DeleteFileA(name + ".FST");

    CString sound = EditorDataPath();
    sound += "\\Data\\Sound\\cmpwin.wav";
    PlaySoundA(sound, nullptr, SND_FILENAME | SND_ASYNC);
    EndWaitCursor();
}
