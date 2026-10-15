#include "stdafx.h"
#include "MultiView.h"
#include "DreditorDoc.h"
#include "DistributionFile.h"
#include "EditorObject.h"
#include "MissionDescriptionDialog.h"
#include "MultiMap.h"
#include "ObjectivesTeamDialog.h"
#include "PlaceObjectTool.h"
#include "SettingsPlayerDialog.h"
#include "ValueDialog.h"
#include "resource.h"
#include "EditorFiles.h"

#include <direct.h>
#include <mmsystem.h>
#include <rpc.h>

namespace
{
    /// <summary>Team 2's side (the Clans).</summary>
    inline constexpr int32_t TEAM_2_SIDE = 1;
    /// <summary>Team 2's drop zones beyond which Drop Zones (team 2) is off.</summary>
    inline constexpr int32_t MAX_TEAM_2_DROP_ZONES = 2;
    /// <summary>The object id of team 2's drop zone: the drop zone's frame 1.</summary>
    inline constexpr uint32_t TEAM_2_DROP_ZONE_ID = 1 << 16 | 2;
    /// <summary>The distribution file's extension and the save dialog's filter.</summary>
    const char* const DPK_EXTENSION = ".dpk";
    const char* const DPK_FILTER = "MCX Distribution Files (*.dpk)|*.dpk|All Files (*.*)|*.*||";
    /// <summary>The folders the mission's files are in, from the editor's current folder.</summary>
    const char* const MISSIONS_FOLDER = "data\\missions\\";
    const char* const TERRAIN_FOLDER = "data\\terrain\\";
    /// <summary>The teams' dispatches: the Clans' (team 2) and the Inner Sphere's (team 1), before the mission's name.</summary>
    const char* const CLAN_TEXT_PREFIX = "data\\missions\\C";
    const char* const IS_TEXT_PREFIX = "data\\missions\\IS";
    /// <summary>Where the mission's save is kept while the new one takes its name.</summary>
    const char* const SAVE_HOLDER = "holder.MPK";
    /// <summary>The list of files for the FastFile packer.</summary>
    const char* const RESPONSE_FILE = "work.rsp";
    /// <summary>The mission's files in the missions and terrain folders.</summary>
    const char* const MISSION_EXTENSIONS[] = {"_VAR.ABI", "_STR.ABI", "_MP.ABI", "_INIT.ABI", "_LOP.ABI"};
    const char* const TERRAIN_EXTENSIONS[] = {".BDG", ".DAT", ".ELV", ".FIT",    ".GMM",
                                              ".OBJ", ".PRE", ".TGA", ".LOG.TGA"};
}

IMPLEMENT_DYNCREATE(MultiView, DreditorView)

MultiView::MultiView() = default;

bool MultiView::askValues(UINT& first, UINT& second, UINT command, UINT firstMin, UINT firstMax, UINT secondMin,
                          UINT secondMax)
{
    ValueDialog dialog(ValueDialog::commandTitle(command), true, firstMin, firstMax, secondMin, secondMax, this);
    dialog.first = first;
    dialog.second = second;

    if (dialog.DoModal() != IDOK)
    {
        return false;
    }

    first = dialog.first;
    second = dialog.second;
    return true;
}

CMenu* MultiView::popupMenu(EditorObject* object, CMenu& menu)
{
    return loadPopupMenu(object, menu, IDR_TEAM_MISSION_POPUP, IDR_TEAM_BUILDING_POPUP);
}

BEGIN_MESSAGE_MAP(MultiView, DreditorView)
ON_COMMAND(ID_MISSION_OBJECTIVES_TEAM_1, &MultiView::OnMissionObjectivesTeam1)
ON_UPDATE_COMMAND_UI(ID_MISSION_DROP_ZONES_TEAM_2, &MultiView::OnUpdateMissionDropZonesTeam2)
ON_COMMAND(ID_MISSION_OBJECTIVES_TEAM_2, &MultiView::OnMissionObjectivesTeam2)
ON_COMMAND(ID_FILE_OPEN, &MultiView::OnFileOpen)
ON_COMMAND(ID_MISSION_SETTINGS_TEAM_1, &MultiView::OnMissionSettingsTeam1)
ON_COMMAND(ID_MISSION_SETTINGS_TEAM_2, &MultiView::OnMissionSettingsTeam2)
ON_COMMAND(ID_MISSION_DROP_ZONES_TEAM_2, &MultiView::OnMissionDropZonesTeam2)
ON_COMMAND(ID_MISSION_DESCRIPTION, &MultiView::OnMissionDescription)
ON_COMMAND(ID_FILE_WRITE_DISTRIBUTION_FILE_DPK, &MultiView::OnFileWriteDistributionFileDpk)
END_MESSAGE_MAP()

void MultiView::openObjectives(bool team2)
{
    delete objectivesDialog;
    objectivesDialog = new ObjectivesTeamDialog(*map(), this, team2, true);
    objectivesDialog->Create(ObjectivesTeamDialog::IDD, this);
    m_pDocument->SetModifiedFlag(TRUE);
    pickingForUnit = false;
}

void MultiView::OnMissionObjectivesTeam1()
{
    openObjectives(false);
}

void MultiView::OnUpdateMissionDropZonesTeam2(CCmdUI* pCmdUI)
{
    int32_t teamZones = 0;

    for (const EditorObject* zone : map()->dropZones)
    {
        if (zone->side == TEAM_2_SIDE)
        {
            teamZones++;
        }
    }

    pCmdUI->Enable(teamZones > MAX_TEAM_2_DROP_ZONES ? FALSE : TRUE);
}

void MultiView::OnMissionObjectivesTeam2()
{
    openObjectives(true);
}

void MultiView::OnFileOpen()
{
    static_cast<DreditorDoc*>(m_pDocument)->openMission(true, false);
}

void MultiView::OnMissionSettingsTeam1()
{
    auto& multiMap = static_cast<MultiMap&>(*map());
    MissionSettings settings = multiMap.team1Settings();
    SettingsPlayerDialog dialog(multiMap.missionText, settings, true, IDS_SETTINGS_TEAM_1, this, true);

    if (dialog.DoModal() == IDOK)
    {
        multiMap.setTeam1Settings(settings);
        m_pDocument->SetModifiedFlag(TRUE);
    }
}

void MultiView::OnMissionSettingsTeam2()
{
    // Team 2's dialog has no time limit: the mission's is team 1's.
    auto& multiMap = static_cast<MultiMap&>(*map());
    MissionSettings settings = multiMap.team2Settings();
    SettingsPlayerDialog dialog(multiMap.clanText, settings, true, IDS_SETTINGS_TEAM_2, this, false);

    if (dialog.DoModal() == IDOK)
    {
        multiMap.setTeam2Settings(settings);
        m_pDocument->SetModifiedFlag(TRUE);
    }
}

void MultiView::OnMissionDropZonesTeam2()
{
    // Original behaviour: unlike team 1's command, no click sound, a tool also made during a drag, and no redraw.
    delete tool;
    tool = new PlaceObjectTool(TEAM_2_DROP_ZONE_ID, TEAM_2_SIDE, false);
    toolCommand = ID_MISSION_DROP_ZONES_TEAM_2;
}

void MultiView::OnMissionDescription()
{
    EditorMap& editorMap = *map();
    MissionDescriptionDialog dialog(nullptr);
    dialog.description = editorMap.description;

    if (dialog.DoModal() == IDOK)
    {
        editorMap.description = dialog.description;
        m_pDocument->SetModifiedFlag(TRUE);
    }
}

void MultiView::OnFileWriteDistributionFileDpk()
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

    for (const char* extension : MISSION_EXTENSIONS)
    {
        copyMissionFile(name, guid, MISSIONS_FOLDER, extension);
    }

    copyMissionFile(name, guid, CLAN_TEXT_PREFIX, ".TXT");
    copyMissionFile(name, guid, IS_TEXT_PREFIX, ".TXT");

    for (const char* extension : TERRAIN_EXTENSIONS)
    {
        copyMissionFile(name, guid, TERRAIN_FOLDER, extension);
    }

    // The copies' script and FIT file name the copies. The first rewrite lower-cases the name.
    CStringArray lines;
    lines.SetSize(0);
    lines.Add("#include_ \"" + name + "_VAR.ABI\"");
    lines.Add("#include_ \"" + name + "_INIT.ABI\"");
    lines.Add("#include_ \"" + name + "_MP.ABI\"");
    lines.Add("#include_ \"" + name + "_STR.ABI\"");
    lines.Add("#include_ \"" + name + "_LOP.ABI\"");
    rewriteMissionFile(name, guid, MISSIONS_FOLDER, ".ABL", lines);
    lines.SetSize(0);
    lines.Add("st TerrainFileName = \"" + name + "\"");
    lines.Add("st TacMapGifName = \"" + name + "\"");
    lines.Add("st ScenarioScript = \"" + name + "\"");
    lines.Add("st ISBriefingFile = \"IS" + name + ".TXT\"");
    lines.Add("st ClanBriefingFile = \"C" + name + ".TXT\"");
    lines.Add("st MapFile = \"" + name + "\"");
    rewriteMissionFile(name, guid, MISSIONS_FOLDER, ".FIT", lines);

    CStdioFile response;
    response.Open(RESPONSE_FILE, CFile::modeCreate | CFile::modeWrite);
    response.WriteString(MISSIONS_FOLDER + guid + "_VAR.ABI\n");
    response.WriteString(MISSIONS_FOLDER + guid + "_STR.ABI\n");
    response.WriteString(CLAN_TEXT_PREFIX + guid + ".TXT\n");
    response.WriteString(IS_TEXT_PREFIX + guid + ".TXT\n");

    for (const char* extension : {"_MP.ABI\n", "_INIT.ABI\n", "_LOP.ABI\n", ".ABL\n", ".FIT\n"})
    {
        response.WriteString(MISSIONS_FOLDER + guid + extension);
    }

    for (const char* extension :
         {".BDG\n", ".DAT\n", ".ELV\n", ".FIT\n", ".GMM\n", ".OBJ\n", ".PRE\n", ".TGA\n", ".LOG.TGA\n\n"})
    {
        response.WriteString(TERRAIN_FOLDER + guid + extension);
    }

    response.Close();
    buildFastFile(RESPONSE_FILE, name + ".fst");

    // The copies go, from the missions and terrain folders.
    const CString copies = "*" + guid + "*.*";

    for (const char* subfolder : {"data\\missions", "..\\terrain"})
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

    _chdir("..\\..");

    // The .dpk: a save of the mission as it is now, under the save's name, and the FastFile of its files.
    response.Open(RESPONSE_FILE, CFile::modeCreate | CFile::modeWrite);
    response.WriteString("data\\savegame\\" + name + ".MPK\n");
    response.WriteString(name + ".FST\n\n");
    response.Close();
    static_cast<MultiMap&>(*map()).writeSave(name, guid);
    _chdir("data\\savegame");
    CFile::Rename(name + ".MPK", SAVE_HOLDER);
    CFile::Rename(guid + ".MPK", name + ".MPK");
    _chdir("..\\..");
    buildFastFile(RESPONSE_FILE, folder + dpkName + ".DPK");
    DeleteFileA(RESPONSE_FILE);
    _chdir("data\\savegame");
    DeleteFileA(name + ".MPK");
    CFile::Rename(SAVE_HOLDER, name + ".MPK");
    _chdir("..\\..");
    DeleteFileA(name + ".FST");

    CString sound = EditorDataPath();
    sound += "\\Data\\Sound\\cmpwin.wav";
    PlaySoundA(sound, nullptr, SND_FILENAME | SND_ASYNC);
    EndWaitCursor();
}
