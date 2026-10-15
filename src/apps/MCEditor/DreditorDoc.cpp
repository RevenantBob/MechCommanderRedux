#include "stdafx.h"
#include "DreditorDoc.h"
#include "ChooseEditorDialog.h"
#include "DreditorApp.h"
#include "EditorMap.h"
#include "EditorPalette.h"
#include "EditorUtil.h"
#include "MissionEditorDialog.h"
#include "MissionRecordSet.h"
#include "MultiMap.h"
#include "NewMapDialog.h"
#include "ObjectLibrary.h"
#include "OpenDialog.h"
#include "OverlayTools.h"
#include "SelectMissionDialog.h"
#include "SplashDialog.h"
#include "TileLibrary.h"
#include "VertexTool.h"
#include "lib/ffile.h"
#include "lib/lzdecomp.h"
#include "resource.h"
#include "vfx/alphapalette.h"
#include "EditorFiles.h"

#include <direct.h>
#include <mmsystem.h>

#include <fstream>
#include <sstream>

/// <summary>The editor's tile library (a static of this file in the original; its name wasn't kept).</summary>
TileLibrary tileLibraryInstance;
/// <summary>The editor's object library (a static of this file in the original; its name wasn't kept).</summary>
ObjectLibrary objectLibraryInstance;
/// <summary>The editor's palette (a static of this file in the original; its name wasn't kept).</summary>
EditorPalette editorPaletteInstance;

namespace
{
    /// <summary>A word of System.cfg read by <see cref="DreditorDoc::readConfigPath"/>, and the line after the key.</summary>
    inline constexpr int32_t CONFIG_LINE_SIZE = 0x100;
    /// <summary>The bytes before the colours in a .pal file (two words).</summary>
    inline constexpr size_t PALETTE_HEADER_SIZE = 4;
    /// <summary>The colours of a .pal file: 256 RGB triples, 6 bits per component.</summary>
    inline constexpr UINT PALETTE_DATA_SIZE = 0x300;
    /// <summary>A file the CD-ROM check looks for on the drive.</summary>
    inline constexpr const char* CD_PROBE_FILE = "\\data\\tiles\\gtiles90.pak";

    /// <summary>The first word of a packet file (.elv, .obj).</summary>
    inline constexpr uint32_t PACKET_FILE_MAGIC = 0xfeedface;
    /// <summary>The archive buffer the map files are read through.</summary>
    inline constexpr UINT MAP_ARCHIVE_BUFFER = 0x1000;
    /// <summary>A line of the map's .fit file.</summary>
    inline constexpr int32_t FIT_LINE_SIZE = 0x100;
    /// <summary>The size of one object record in a .obj file.</summary>
    inline constexpr uint32_t OBJ_RECORD_SIZE = 11;
    /// <summary>
    /// "Couldn't open the %s file." (0xe006; the resource extractor named it after MFC's id of the same value).
    /// </summary>
    inline constexpr UINT IDS_COULDNT_OPEN_FILE = AFX_IDS_APP_ID;
    /// <summary>The mission database, under the data path.</summary>
    inline constexpr char MISSION_DATABASE[] = "Teditor\\FitAbl.mdb";
    /// <summary>The time the splash screen shows when the editor starts, in milliseconds.</summary>
    inline constexpr DWORD STARTUP_SPLASH_TIME = 3000;
    /// <summary>A new map's side in terrain blocks: this plus 3 for each step of its size.</summary>
    inline constexpr int32_t NEW_MAP_MIN_BLOCKS = 6;
    inline constexpr int32_t NEW_MAP_BLOCK_STEP = 3;
    /// <summary>The water coverage up to which a new map's ground starts raised.</summary>
    inline constexpr int32_t NEW_MAP_LOW_WATER = 60;
    /// <summary>The elevation a new map's ground starts at when it has little water, and when it has much.</summary>
    inline constexpr int32_t NEW_MAP_DRY_ELEVATION = 4;
    inline constexpr int32_t NEW_MAP_WET_ELEVATION = 0;
    /// <summary>The elevation generateMap gives its land patches on a map that starts under water.</summary>
    inline constexpr int32_t GENERATED_LAND_ELEVATION = 3;
    /// <summary>The coverages are percentages.</summary>
    inline constexpr int32_t PERCENT = 100;
    /// <summary>The longest side of a generated water or land patch's rectangle, in vertices.</summary>
    inline constexpr int32_t PATCH_MAX_SIDE = 20;
    /// <summary>How far a patch's top, bottom and sides may reach past its rectangle, plus one.</summary>
    inline constexpr int32_t PATCH_TOP_RAGGEDNESS = 3;
    inline constexpr int32_t PATCH_BOTTOM_RAGGEDNESS = 4;
    inline constexpr int32_t PATCH_SIDE_RAGGEDNESS = 3;
    /// <summary>The steps of generateMap's hills (0x004faed4 for the tool's own step).</summary>
    inline constexpr int32_t RAISE_STEP = 1;
    inline constexpr int32_t LOWER_STEP = -1;

    /// <summary>
    /// Plays the data path's Data\Sound\steam.wav (<see cref="EditorDataPath"/>, as the original reads it), the sound of a
    /// map being made or opened.
    /// </summary>
    void playSteamSound()
    {
        CString path = EditorDataPath();
        path += "\\Data\\Sound\\steam.wav";
        PlaySoundA(path, nullptr, SND_FILENAME | SND_ASYNC);
    }

    /// <summary>Whether the editor's main window exists yet.</summary>
    bool hasMainWindow()
    {
        CWinThread* thread = AfxGetThread();
        return thread != nullptr && thread->GetMainWnd() != nullptr;
    }

    /// <summary>The view type <c>DreditorApp::SwitchView</c> takes for the solo or the multiplayer editor.</summary>
    int editorView(bool multiplayer)
    {
        return multiplayer ? ChooseEditorDialog::EDITOR_MULTIPLAYER : ChooseEditorDialog::EDITOR_SOLO;
    }

    /// <summary>Opens a packet file in a load archive positioned at its first packet.</summary>
    /// <returns>The offset of the first packet.</returns>
    int32_t openPacketArchive(CFile& file, CArchive& archive, const CString& name)
    {
        int32_t magic = 0;
        archive >> magic;

        if (static_cast<uint32_t>(magic) != PACKET_FILE_MAGIC)
        {
            // Original behaviour: the warning is all; the file is read anyway.
            CString format;
            format.LoadString(IDS_THE_S_FILE_HAS);
            CString message;
            message.Format(format, static_cast<const char*>(name));
            AfxMessageBox(message);
        }

        int32_t first = 0;
        archive >> first;
        archive.Flush();
        file.Seek(first, CFile::begin);
        return first;
    }

    /// <summary>Shows "Couldn't open the <paramref name="name"/> file." in a plain message box.</summary>
    void reportOpenFailure(const CString& name)
    {
        CString format;
        format.LoadString(IDS_COULDNT_OPEN_FILE);
        CString message;
        message.Format(format, static_cast<const char*>(name));
        CString title;
        title.LoadString(IDS_MCEDITOR);
        MessageBoxA(nullptr, message, title, MB_OK);
    }
}

IMPLEMENT_DYNCREATE(DreditorDoc, CDocument)

DreditorDoc::DreditorDoc()
{
    dataPath = EditorDataPath();

    if (dataPath.IsEmpty() || dataPath[dataPath.GetLength() - 1] != '\\')
    {
        dataPath += '\\';
    }

    // Dropped (EB-002): EnableAutomation and AfxOleLockApp.
}

BEGIN_MESSAGE_MAP(DreditorDoc, CDocument)
ON_COMMAND(ID_QUICK_SAVE, &DreditorDoc::OnQuickSave)
ON_COMMAND(ID_FILE_SAVE, &DreditorDoc::OnFileSave)
ON_COMMAND(ID_FILE_SAVE_AS, &DreditorDoc::OnFileSaveAs)
ON_COMMAND(ID_FILE_SAVE_MAP_ONLY, &DreditorDoc::OnFileSaveMapOnly)
END_MESSAGE_MAP()

BOOL DreditorDoc::OnNewDocument()
{
    // The title and path to put back if no mission is chosen: the file name without ".fit".
    const int slash = m_strPathName.ReverseFind('\\');
    CString previousTitle;
    const CString previousPath = m_strPathName;

    if (slash != -1)
    {
        previousTitle = m_strPathName.Right(m_strPathName.GetLength() - slash - 1);
    }

    previousTitle.Replace(".fit", "");
    CWnd* mainWindow = nullptr;

    if (CWinThread* thread = AfxGetThread(); thread != nullptr)
    {
        mainWindow = thread->GetMainWnd();
    }

    if (mainWindow != nullptr)
    {
        ::SendMessage(mainWindow->m_hWnd, WM_COMMAND, ID_TOOLS_SELECT, 0);
    }
    else
    {
        SplashDialog::ShowSplashScreen(nullptr);
        Sleep(STARTUP_SPLASH_TIME);
        SplashDialog::HideSplashScreen();
    }

    bool result = true;
    undoActions.reset();

    if (!CDocument::OnNewDocument())
    {
        return FALSE;
    }

    BeginWaitCursor();
    const CString databasePath = dataPath + MISSION_DATABASE;
    _chdir(dataPath);
    MissionRecordSet* missions = nullptr;

    try
    {
        if (!database.IsOpen())
        {
            database.Open(databasePath, FALSE, FALSE, "");
        }

        missions = new MissionRecordSet(&database);
        missions->Open(dbOpenDynaset, nullptr, 0);
    }
    catch (DaoException* e)
    {
        // Original behaviour: the recordset and the wait cursor are left as they are.
        CString format;
        format.LoadString(IDS_COULDNT_OPEN_THE_DATABASE);
        CString message;
        message.Format(format, static_cast<const char*>(e->m_strDescription));
        CString title;
        title.LoadString(IDS_MCEDITOR);
        MessageBoxA(nullptr, message, title, MB_OK);
        e->Delete();
        return FALSE;
    }

    ChooseEditorDialog chooser(missions);

    for (;;)
    {
        const INT_PTR choice = chooser.DoModal();
        RestoreWaitCursor();

        if (choice == IDCANCEL)
        {
            result = false;
            break;
        }

        if (choice == ChooseEditorDialog::EDITOR_MULTIPLAYER && newOrOpen(true, mainWindow == nullptr))
        {
            result = true;
            break;
        }

        if (choice == ChooseEditorDialog::EDITOR_SOLO && newOrOpen(false, mainWindow == nullptr))
        {
            result = true;
            break;
        }
    }

    if (missions != nullptr)
    {
        missions->Close();
        delete missions;
    }

    EndWaitCursor();

    if (!result && previousTitle.GetLength() != 0)
    {
        SetTitle(previousTitle);
        m_strPathName = previousPath;
    }

    return result;
}

bool DreditorDoc::newOrOpen(bool multiplayer, bool startup)
{
    MissionEditorDialog chooser(multiplayer ? IDD_MULTIPLAYER_MISSION_EDITOR_DIALOG : IDD_SOLO_MISSION_EDITOR_DIALOG);
    bool done = false;
    bool result = true;

    for (;;)
    {
        INT_PTR choice = IDC_NEW_MISSION;

        if (startup)
        {
            choice = chooser.DoModal();
        }

        if (choice == IDC_NEW_MISSION)
        {
            NewMapDialog newMap;

            if (newMap.DoModal() == IDOK)
            {
                if (!hasMainWindow())
                {
                    SplashDialog::ShowSplashScreen(nullptr);
                }

                BeginWaitCursor();
                playSteamSound();
                defaultElevation =
                    newMap.waterCoverage <= NEW_MAP_LOW_WATER ? NEW_MAP_DRY_ELEVATION : NEW_MAP_WET_ELEVATION;
                defaultTexture = newMap.terrain == 0;
                const int32_t size = MAP_BLOCK_TILES * (newMap.mapSize * NEW_MAP_BLOCK_STEP + NEW_MAP_MIN_BLOCKS) - 1;
                // Original behaviour (EB-056): the old map is deleted but kept in map until a new one is made.
                delete map;
                map = nullptr;

                if (loadLibraries())
                {
                    if (multiplayer)
                    {
                        map = new MultiMap(size, size);
                    }
                    else
                    {
                        map = new SingleMap(size, size);
                    }

                    result = map != nullptr &&
                             generateMap(size, size, newMap.waterCoverage, newMap.forestCoverage, newMap.hillCoverage);
                    done = true;
                    EndWaitCursor();
                }

                SplashDialog::HideSplashScreen();
            }
            else if (!startup)
            {
                return false;
            }
        }
        else if (choice == IDC_EDIT_MISSION)
        {
            if (openMission(multiplayer, true))
            {
                result = true;
                done = true;
            }
        }
        else if (choice == IDCANCEL)
        {
            return false;
        }

        if (done)
        {
            break;
        }
    }

    static_cast<DreditorApp*>(AfxGetApp())->SwitchView(editorView(multiplayer));
    return result;
}

bool DreditorDoc::openMission(bool multiplayer, bool retry)
{
    CString path;
    SelectMissionDialog missionChooser(&database, multiplayer ? SelectMissionDialog::MULTIPLAYER_MISSIONS
                                                              : SelectMissionDialog::SOLO_MISSIONS);
    SelectMissionDialog mapChooser(&database, SelectMissionDialog::MAPS);
    bool opened = false;

    do
    {
        OpenDialog openDialog;
        const INT_PTR choice = openDialog.DoModal();

        if (choice == IDC_LOAD_MAP_WITH_MISSION_DATA)
        {
            if (missionChooser.DoModal() == IDOK)
            {
                BeginWaitCursor();
                playSteamSound();

                if (!hasMainWindow())
                {
                    SplashDialog::ShowSplashScreen(nullptr);
                }

                opened = loadMission(missionChooser.selectedName);
                SplashDialog::HideSplashScreen();
                EndWaitCursor();
            }
        }
        else if (choice == IDC_LOAD_MAP_ONLY)
        {
            if (mapChooser.DoModal() == IDOK)
            {
                BeginWaitCursor();
                playSteamSound();

                if (!hasMainWindow())
                {
                    SplashDialog::ShowSplashScreen(nullptr);
                }

                path = EditorDataPath();
                path += "\\Data\\Terrain\\" + mapChooser.selectedName + ".fit";
                opened = openMap(path, multiplayer);
                CString title;
                title.LoadString(AFX_IDS_UNTITLED);
                SetTitle(title);
                SplashDialog::HideSplashScreen();
                EndWaitCursor();
            }
        }
        else if (choice == IDCANCEL)
        {
            return false;
        }

        if (!retry)
        {
            opened = true;
        }
    } while (!opened);

    return true;
}

bool DreditorDoc::loadMission(const CString& name)
{
    BeginWaitCursor();
    MissionRecordSet missions(&database);
    missions.Open(dbOpenDynaset, nullptr, 0);
    CString filter;
    filter.Format("[MissionName] = \"%s\"", static_cast<const char*>(name));

    if (missions.FindFirst(filter))
    {
        return loadMissionRecord(missions.m_MissionID, missions);
    }

    // The message is formatted twice: the second time with the first's text as the format.
    CString format;
    format.Format(IDS_COULDNT_OPEN_FILE, static_cast<const char*>(name));
    CString message;
    message.Format(format, static_cast<const char*>(name));
    CString title;
    title.LoadString(IDS_MCEDITOR);
    MessageBoxA(nullptr, message, title, MB_OK);
    return false;
}

bool DreditorDoc::loadMissionRecord(long missionId, MissionRecordSet& missions)
{
    bool result = false;

    try
    {
        CString filter;
        filter.Format("MissionID = %ld", missionId);

        if (!missions.FindFirst(filter))
        {
            return false;
        }

        COleVariant multiplayerValue;
        missions.GetFieldValue("MultiPlayer", multiplayerValue);
        const COleVariant nameValue = missions.GetFieldValue("MissionName");
        const CString name(nameValue.bstrVal);
        SetTitle(name);
        const CString fitPath = dataPath + "Data\\Terrain\\" + name + ".fit";
        SetPathName(fitPath, TRUE);

        // The terrain set the map's .fit names, loaded whether it changed or not.
        {
            std::ifstream fit(static_cast<const char*>(fitPath));
            char line[FIT_LINE_SIZE];

            while (!fit.bad() && !fit.eof() && !fit.fail())
            {
                fit.getline(line, FIT_LINE_SIZE);

                if (std::strstr(line, "TerrainTileFile") == nullptr)
                {
                    continue;
                }

                const bool gTiles = std::strstr(line, "GTiles") != nullptr;
                tileLibrary->setGTiles(gTiles);

                if (objectLibrary->gTiles != gTiles)
                {
                    objectLibrary->reloadNeeded = true;
                }

                objectLibrary->gTiles = gTiles;
                loadLibraries();
                break;
            }
        }

        // The variant's low byte, as the original tests it.
        const bool multiplayer = multiplayerValue.bVal != 0;
        result = openMap(fitPath, multiplayer);

        if (result)
        {
            result = map->loadMission(database, name);
            static_cast<DreditorApp*>(AfxGetApp())->SwitchView(editorView(multiplayer));
        }
    }
    catch (DaoException* e)
    {
        CString message;
        message.Format(IDS_THE_MISSION_LOAD_FAILED, static_cast<const char*>(e->m_strDescription));
        CString title;
        title.LoadString(IDS_MCEDITOR);
        MessageBoxA(nullptr, message, title, MB_OK);
        result = false;
        e->Delete();
    }

    return result;
}

void DreditorDoc::openMissionByMap()
{
    MissionRecordSet missions(&database);
    missions.Open(dbOpenDynaset, nullptr, 0);
    SelectMissionDialog mapChooser(&database, SelectMissionDialog::MAPS);

    if (mapChooser.DoModal() != IDOK)
    {
        return;
    }

    CString path = dataPath;
    path += "Data\\Terrain\\";
    path += mapChooser.selectedName;
    path += ".fit";
    SetPathName(path, TRUE);
    CString filter;
    filter.Format("[MissionName] = '%s'", static_cast<const char*>(mapChooser.selectedName));

    if (missions.FindFirst(filter))
    {
        loadMissionRecord(missions.m_MissionID, missions);
    }
}

bool DreditorDoc::generateMap(int32_t maxRow, int32_t maxCol, int32_t water, int32_t forest, int32_t hill)
{
    const uint8_t savedTexture = TileLibrary::getDefaultTexture();
    EditorMap& editorMap = *map;
    VertexTool heights(RAISE_STEP, TEXTURE_NONE, false, 0);
    int32_t elevation = NEW_MAP_WET_ELEVATION;
    heights.begin(editorMap);
    int32_t target = editorMap.maxCol * editorMap.maxRow * water / PERCENT;

    // With much water the map starts under water and the patches are land instead.
    if (water > NEW_MAP_LOW_WATER)
    {
        target = editorMap.maxCol * editorMap.maxRow - target;
        elevation = GENERATED_LAND_ELEVATION;
    }

    // Each patch is a rectangle of vertices with ragged sides; the count adds every vertex selected, even twice.
    for (int32_t count = 0; count < target;)
    {
        const int32_t cell = rand() % ((editorMap.maxCol + 1) * (editorMap.maxRow + 1));
        const int32_t top = cell / (editorMap.maxCol + 1);
        const int32_t left = cell % (editorMap.maxCol + 1);
        const int32_t rows = rand() % PATCH_MAX_SIDE + 1;
        const int32_t cols = rand() % PATCH_MAX_SIDE + 1;

        for (int32_t i = 0; i < rows; i++)
        {
            for (int32_t j = 0; j < cols; j++)
            {
                editorMap.setVertexSelected(left + j, top + i, true);
            }

            count += cols;
        }

        for (int32_t col = left; col < left + cols; col++)
        {
            const int32_t first = top - rand() % PATCH_TOP_RAGGEDNESS;

            for (int32_t row = first; row < top; row++)
            {
                editorMap.setVertexSelected(col, row, true);
                count++;
            }

            const int32_t bottom = top + rows - 1;

            for (int32_t row = rand() % PATCH_BOTTOM_RAGGEDNESS + rows + top; row > bottom; row--)
            {
                editorMap.setVertexSelected(col, row, true);
                count++;
            }
        }

        for (int32_t row = top; row < top + rows; row++)
        {
            for (int32_t col = left - rand() % PATCH_SIDE_RAGGEDNESS; col < left; col++)
            {
                editorMap.setVertexSelected(col, row, true);
                count++;
            }

            const int32_t last = rand() % PATCH_SIDE_RAGGEDNESS + cols + left;

            for (int32_t col = cols + left; col < last; col++)
            {
                editorMap.setVertexSelected(col, row, true);
                count++;
            }
        }

        heights.setSelectedElevation(editorMap, elevation);
        editorMap.selectedVertices.clear();
    }

    // As many single vertices raised as lowered, but none in the water.
    const int32_t bumps = editorMap.maxCol * editorMap.maxRow * hill / PERCENT >> 1;

    for (const int32_t step : {RAISE_STEP, LOWER_STEP})
    {
        for (int32_t i = 0; i < bumps; i++)
        {
            const int32_t cell = rand() % ((editorMap.maxCol + 1) * (editorMap.maxRow + 1));
            const int32_t row = cell / (editorMap.maxCol + 1);
            const int32_t col = cell % (editorMap.maxCol + 1);

            if (editorMap.vertexElevation(col, row) != 0)
            {
                editorMap.setVertexSelected(col, row, true);
            }

            heights.applyToSelection(editorMap, step);
            editorMap.selectedVertices.clear();
        }
    }

    delete heights.end();
    ForestTool trees;
    trees.begin(editorMap);
    const int32_t forestTiles = editorMap.maxRow * editorMap.maxCol * forest / PERCENT;

    // Each tree drop covers one to four tiles in the play area or on its edge (the odd column and row grow it).
    if (forestTiles > 0)
    {
        for (uint32_t drops = static_cast<uint32_t>(forestTiles + 1) >> 1; drops != 0; drops--)
        {
            const int32_t left = rand() % editorMap.maxRow;
            const int32_t top = rand() % editorMap.maxCol;

            for (int32_t row = top; row < top + (top & 1) + 1; row++)
            {
                for (int32_t col = left; col < left + (left & 1) + 1; col++)
                {
                    if (ForestTool::canPlant(editorMap, col, row) && editorMap.regionOf(col, row) != REGION_OUTSIDE)
                    {
                        trees.plant(editorMap, col, row, true);
                    }
                }
            }
        }
    }

    delete trees.end();
    defaultTexture = savedTexture;
    return true;
}

bool DreditorDoc::openMap(const char* path, bool multiplayer)
{
    if (CWinThread* thread = AfxGetThread(); thread != nullptr)
    {
        if (CWnd* mainWnd = thread->GetMainWnd(); mainWnd != nullptr)
        {
            ::SendMessage(mainWnd->m_hWnd, WM_COMMAND, ID_TOOLS_SELECT, 0);
        }
    }

    if (!CDocument::OnOpenDocument(path))
    {
        return false;
    }

    CString name = path;

    if (name.GetLength() >= 4 && name[name.GetLength() - 4] == '.')
    {
        name = name.Left(name.GetLength() - 4);
    }

    // The terrain set the map's .fit names: the TerrainTileFile line, "GTiles" or not.
    {
        std::ifstream fit(static_cast<const char*>(name + ".fit"));
        const bool currentGTiles = tileSetStringOffset != 0;
        const bool loaded = tileLibrary->initialized;
        char line[FIT_LINE_SIZE];

        // Original behaviour: the loop stops at the end of the file or a bad stream only, so a line too long for the
        // buffer (a failed read that is neither) would loop for ever.
        while (!fit.bad() && !fit.eof())
        {
            fit.getline(line, FIT_LINE_SIZE);

            if (std::strstr(line, "TerrainTileFile") == nullptr)
            {
                continue;
            }

            const bool gTiles = std::strstr(line, "GTiles") != nullptr;

            if (gTiles == currentGTiles && loaded)
            {
                break;
            }

            tileLibrary->setGTiles(gTiles);

            if (objectLibrary->gTiles != gTiles)
            {
                objectLibrary->reloadNeeded = true;
            }

            objectLibrary->gTiles = gTiles;
            loadLibraries();
            break;
        }
    }

    const CString elvName = name + ".elv";
    const CString objName = name + ".obj";
    CFile elvFile;

    if (!elvFile.Open(elvName, CFile::modeRead))
    {
        reportOpenFailure(elvName);
        return false;
    }

    delete map;
    map = nullptr;
    CArchive elvArchive(&elvFile, CArchive::load, MAP_ARCHIVE_BUFFER);
    const int32_t first = openPacketArchive(elvFile, elvArchive, elvName);
    const auto blocks = static_cast<int32_t>(std::sqrt(static_cast<double>(first / 4 - 2)));
    undoList->reset();

    const int32_t tiles = MAP_BLOCK_TILES * blocks - 1;

    if (multiplayer)
    {
        map = new MultiMap(tiles, tiles);
    }
    else
    {
        map = new SingleMap(tiles, tiles);
    }

    const bool result = map->readElevations(elvArchive);
    CFile objFile;

    if (!objFile.Open(objName, CFile::modeRead))
    {
        reportOpenFailure(objName);
        return false;
    }

    const auto length = static_cast<uint32_t>(objFile.GetLength());
    CArchive objArchive(&objFile, CArchive::load, MAP_ARCHIVE_BUFFER);
    const int32_t firstObject = openPacketArchive(objFile, objArchive, objName);
    map->readObjects(static_cast<int32_t>((length - static_cast<uint32_t>(firstObject)) / OBJ_RECORD_SIZE), blocks,
                     objArchive);
    return result;
}

bool DreditorDoc::loadLibraries()
{
    if (dataPath.IsEmpty() || dataPath[dataPath.GetLength() - 1] != '\\')
    {
        dataPath += '\\';
    }

    if (systemConfig.m_hFile == CFile::hFileNull &&
        !systemConfig.Open(dataPath + "system.cfg", CFile::modeRead | CFile::shareDenyNone))
    {
        CString message;
        AfxFormatString1(message, IDS_COULDNT_OPEN_THE_SYSTEM, dataPath);
        CString title;
        title.LoadString(IDS_MCEDITOR);
        MessageBoxA(nullptr, message, title, MB_OK);
        return false;
    }

    CString spritePath;

    if (!findDataFile(systemConfig, "spritePath", "Sprites.pak", spritePath, true))
    {
        return false;
    }

    CString shapes90Path;

    if (!findDataFile(systemConfig, "spritePath", "Shapes90.pak", shapes90Path, true))
    {
        return false;
    }

    // Original behaviour: Shapes.pak is Shapes90.pak's path cut at the first "90" in it, wherever that is.
    CString shapesPath = shapes90Path;
    shapesPath = shapesPath.Left(shapesPath.Find("90"));
    shapesPath += ".pak";
    CString tile90Path;

    if (!findDataFile(systemConfig, "tile90Path", objectLibrary->gTiles ? "GTiles90.pak" : "Tiles90.pak", tile90Path,
                      true))
    {
        return false;
    }

    CString tilePath;

    if (!findDataFile(systemConfig, "tilePath", objectLibrary->gTiles ? "GTiles.pak" : "Tiles.pak", tilePath, true))
    {
        return false;
    }

    CString objectPath;

    if (!findDataFile(systemConfig, "objectPath", "Object2.pak", objectPath, false))
    {
        return false;
    }

    const CString tilesFile = dataPath + "Teditor\\Tiles.csv";
    const CString idsFile = dataPath + "Teditor\\TedIds.csv";
    tileLibraryInstance.init(tile90Path, tilePath, tilesFile);
    objectLibraryInstance.init(idsFile, objectPath, spritePath, shapes90Path, shapesPath);

    if (!loadPalette(systemConfig))
    {
        return false;
    }

    // Original behaviour (EB-042): the editor opens "Teditor\Template.txt" in the current folder (the data path once
    // OnNewDocument has changed to it), ignores the result, and reads every later load into the first load's buffer.
    CFile templateFile;
    templateFile.Open(dataPath + "Teditor\\Template.txt", CFile::modeRead);
    const auto length = static_cast<UINT>(templateFile.GetLength());

    if (templateText == nullptr || length > templateCapacity)
    {
        delete[] templateText;
        // EB-098: the map save prints the text as a format string, which the original doesn't end.
        templateText = new char[length + 1];
        templateCapacity = length;
    }

    templateFile.Read(templateText, length);
    templateText[length] = '\0';
    return true;
}

bool DreditorDoc::findDataFile(CFile& config, const char* key, const CString& fileName, CString& path, bool searchCD)
{
    if (!readConfigPath(config, key, path))
    {
        return false;
    }

    bool found = true;
    path += fileName;
    CString fullPath;
    WIN32_FIND_DATA findData;

    if (path.Find(':') == -1)
    {
        fullPath = dataPath + path;
    }
    else
    {
        // A path with a drive is taken for the CD-ROM: wait for a disc with the game on that drive.
        fullPath = path;
        const char drive[3] = {fullPath[0], ':', '\0'};
        bool ready = false;

        while (!ready)
        {
            // Original behaviour (EB-038): a drive that isn't a CD-ROM is asked again for ever.
            if (GetDriveType(drive) != DRIVE_CDROM)
            {
                continue;
            }

            const CString probe = CString(drive) + CD_PROBE_FILE;

            // Original behaviour (EB-038): the probe's find handle is never closed.
            if (FindFirstFile(probe, &findData) != INVALID_HANDLE_VALUE)
            {
                ready = true;
            }
            else if (AfxMessageBox(IDS_PLEASE_INSERT_THE_MECHCOMMANDER, MB_OKCANCEL) == IDCANCEL)
            {
                return false;
            }
        }
    }

    HANDLE find = FindFirstFile(fullPath, &findData);

    if (find == INVALID_HANDLE_VALUE)
    {
        if (!searchCD)
        {
            found = false;
        }
        else if (!readConfigPath(config, CString("CD") + key, path))
        {
            found = false;
        }
        else
        {
            path += fileName;
            fullPath = path;
            find = FindFirstFile(fullPath, &findData);

            if (find == INVALID_HANDLE_VALUE)
            {
                fullPath = path;
                const char drive[3] = {fullPath[0], ':', '\0'};

                for (;;)
                {
                    // Original behaviour (EB-038): waits for ever when the CD path's drive isn't a CD-ROM.
                    while (GetDriveType(drive) != DRIVE_CDROM)
                    {
                    }

                    const CString probe = CString(drive) + CD_PROBE_FILE;
                    const HANDLE disc = FindFirstFile(probe, &findData);

                    if (disc != INVALID_HANDLE_VALUE)
                    {
                        // The file itself isn't looked for again: the CD path is taken as found.
                        FindClose(disc);
                        return true;
                    }

                    if (AfxMessageBox(IDS_PLEASE_INSERT_THE_MECHCOMMANDER, MB_OKCANCEL) == IDCANCEL)
                    {
                        return false;
                    }
                }
            }
        }
    }

    // The original also passes the invalid handle when the file wasn't found.
    if (find != INVALID_HANDLE_VALUE)
    {
        FindClose(find);
    }

    path = fullPath;
    return found;
}

bool DreditorDoc::readConfigPath(CFile& config, const char* key, CString& value)
{
    config.Seek(0, CFile::begin);
    const auto length = static_cast<UINT>(config.GetLength());
    std::vector<char> text(length);
    config.Read(text.data(), length);
    std::istringstream stream(std::string(text.data(), length));
    // Original behaviour (EB-039): the first comparison is with an uninitialised buffer, which the port starts empty;
    // the words are read into it without a limit.
    std::string word;

    while (_stricmp(word.c_str(), key) != 0)
    {
        stream >> word;

        if (_stricmp(word.c_str(), "fitend") == 0 || stream.fail() || stream.bad())
        {
            CString format;
            format.LoadString(IDS_COULDNT_FIND_S_IN);
            CString message;
            message.Format(format, key, config.GetFilePath().GetString());
            CString title;
            title.LoadString(IDS_MCEDITOR);
            MessageBoxA(nullptr, message, title, MB_OK);
            return false;
        }
    }

    char line[CONFIG_LINE_SIZE] = {};
    stream.getline(line, CONFIG_LINE_SIZE);
    const auto lineLength = static_cast<int32_t>(std::strlen(line));
    int32_t start = lineLength - 1;
    int32_t quotes = 0;

    for (; start > -1; start--)
    {
        if (line[start] == '"')
        {
            quotes++;
        }

        if (quotes == 2)
        {
            break;
        }
    }

    // Drops the line's last character: the '\r' of a CRLF line, else the closing quote. Original behaviour (EB-039):
    // an empty line has the byte before the buffer cleared.
    if (lineLength > 0)
    {
        line[lineLength - 1] = '\0';
    }

    value = &line[start + 1];
    value.Remove('"');
    return true;
}

bool DreditorDoc::loadPalette(CFile& config)
{
    ColorCacheReady = false;
    CString palettePath;
    CFile file;
    std::vector<uint8_t> paletteData;
    const uint8_t* rgb = nullptr;
    CString fadePath;
    const bool fadeFound =
        findDataFile(config, "PalettePath", objectLibrary->gTiles ? "gfade.tbl" : "fade.tbl", fadePath, false);

    if (fadeFound)
    {
        file.Open(fadePath, CFile::modeRead);
        auto table = new uint8_t[static_cast<size_t>(file.GetLength())];
        file.Read(table, static_cast<UINT>(file.GetLength()));
        editorPaletteInstance.setFadeTable(database, table, static_cast<int32_t>(file.GetLength()));
        file.Close();
    }

    // misc.fst names its entries relative to the game folder.
    fadePath.Replace(dataPath, "");

    if (!findDataFile(config, "PalettePath", objectLibrary->gTiles ? "ghb.pal" : "hb.pal", palettePath, false))
    {
        if (!file.Open(dataPath + "misc.fst", CFile::modeRead | CFile::shareDenyNone))
        {
            CString message;
            message.LoadString(IDS_YOU_CANNOT_OPEN_MECHCOMMANDER);
            CString title;
            title.LoadString(IDS_MCEDITOR);
            MessageBoxA(nullptr, message, title, MB_OK);
            return false;
        }

        palettePath.Replace(dataPath, "");
        int32_t count = 0;
        file.Read(&count, sizeof(count));
        bool paletteLoaded = false;
        bool fadeLoaded = fadeFound;

        for (int32_t i = 0; i < count; i++)
        {
            FILEENTRY entry;
            file.Read(&entry, sizeof(entry));

            if (_stricmp(entry.name, palettePath) == 0)
            {
                const ULONGLONG position = file.GetPosition();
                file.Seek(entry.offset, CFile::begin);

                if (entry.size != entry.realSize)
                {
                    // The original unpacks into a stack buffer of 0x30c bytes.
                    std::vector<uint8_t> packed(entry.size);
                    file.Read(packed.data(), entry.size);
                    paletteData.resize(std::max<size_t>(entry.realSize, PALETTE_HEADER_SIZE + PALETTE_DATA_SIZE));
                    LZDecomp(paletteData.data(), packed.data(), entry.size);
                    rgb = paletteData.data() + PALETTE_HEADER_SIZE;
                }
                else
                {
                    uint16_t header[2];
                    file.Read(&header[0], sizeof(header[0]));
                    file.Read(&header[1], sizeof(header[1]));
                    paletteData.resize(PALETTE_DATA_SIZE);
                    file.Read(paletteData.data(), PALETTE_DATA_SIZE);
                    rgb = paletteData.data();
                }

                file.Seek(static_cast<LONGLONG>(position), CFile::begin);
                paletteLoaded = true;
            }
            else if (!fadeLoaded && _stricmp(entry.name, fadePath) == 0)
            {
                const ULONGLONG position = file.GetPosition();
                file.Seek(entry.offset, CFile::begin);
                // The original reads the unpacked size into this buffer of the stored size (EB-040).
                auto stored = new uint8_t[entry.size];
                file.Read(stored, entry.size);

                if (entry.size != entry.realSize)
                {
                    auto table = new uint8_t[entry.realSize];
                    LZDecomp(table, stored, entry.size);
                    editorPaletteInstance.setFadeTable(database, table, entry.realSize);
                    delete[] stored;
                }
                else
                {
                    editorPaletteInstance.setFadeTable(database, stored, entry.realSize);
                }

                file.Seek(static_cast<LONGLONG>(position), CFile::begin);
                fadeLoaded = true;
            }

            if (paletteLoaded && fadeLoaded)
            {
                break;
            }
        }

        if (!paletteLoaded)
        {
            return false;
        }
    }
    else
    {
        // Original behaviour (EB-041): findDataFile's path is already complete, and the data path goes before it a
        // second time, so a loose palette never opens and the reads throw.
        palettePath = dataPath + palettePath;
        file.Open(palettePath, CFile::modeRead);
        uint16_t header[2];
        file.Read(&header[0], sizeof(header[0]));
        file.Read(&header[1], sizeof(header[1]));
        paletteData.resize(PALETTE_DATA_SIZE);
        file.Read(paletteData.data(), PALETTE_DATA_SIZE);
        rgb = paletteData.data();
    }

    file.Close();

    if (!InitAlphaLookup(rgb, dataPath))
    {
        return false;
    }

    const bool initialized = editorPaletteInstance.init(rgb);
    CFileException exception;
    file.Open(dataPath + "teditor\\logistic.pal", CFile::modeRead, &exception);
    uint16_t header[2];
    file.Read(&header[0], sizeof(header[0]));
    file.Read(&header[1], sizeof(header[1]));
    uint8_t logistic[PALETTE_DATA_SIZE];
    file.Read(logistic, PALETTE_DATA_SIZE);
    return editorPaletteInstance.setLogisticPalette(logistic) && initialized;
}
