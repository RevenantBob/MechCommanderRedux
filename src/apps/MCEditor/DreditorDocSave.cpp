#include "stdafx.h"
#include "DreditorDoc.h"
#include "EditorMap.h"
#include "EditorObject.h"
#include "EditorPalette.h"
#include "FitAbl.h"
#include "MissionRecordSet.h"
#include "ObjectLibrary.h"
#include "SaveDialog.h"
#include "TileLibrary.h"
#include "ai/move.h"
#include "resource.h"

#include <direct.h>

// The document's saves (0x00448670, 0x0044a700-0x0044c990, 0x0044d110-0x0044d950): the save commands, the working
// files, the map's files, the pathfinding maps built from them and the mission's files.

namespace
{
    /// <summary>"Couldn't open the %s file."</summary>
    inline constexpr UINT FILE_OPEN_FAILED = 0xe006;
    /// <summary>The caption of the editor's message boxes ("MCEditor").</summary>
    inline constexpr UINT MESSAGE_CAPTION = 2;
    /// <summary>The working files without a document path, under the current folder.</summary>
    const char* const WORKING_FOLDER = "\\data\\terrain";
    const char* const WORKING_NAME = "\\Working";
    /// <summary>Added to the document's name for its working files.</summary>
    const char* const WORKING_SUFFIX = "~1";
    /// <summary>The map files' extensions.</summary>
    const char* const ELV_EXTENSION = ".elv";
    const char* const OBJ_EXTENSION = ".obj";
    const char* const FIT_EXTENSION = ".fit";
    const char* const GMM_EXTENSION = ".gmm";
    const char* const DAT_EXTENSION = ".dat";
    const char* const PRE_EXTENSION = ".pre";
    const char* const BDG_EXTENSION = ".bdg";
    const char* const TGA_EXTENSION = ".tga";
    const char* const LOGISTIC_TGA_EXTENSION = ".log.tga";
    /// <summary>The folders under the game folder the mission's files go to, and FitAbl's database.</summary>
    const char* const MISSIONS_FOLDER = "Data\\Missions";
    const char* const TEDITOR_FOLDER = "Teditor";
    const char* const WARRIORS_FOLDER = "\\WARRIORS";
    const char* const FITABL_DATABASE = "FitAbl.mdb";
    /// <summary>The size of the buffer an exception's message is read into.</summary>
    inline constexpr UINT ERROR_TEXT_SIZE = 0x100;
    /// <summary>A scenario map tile's cell bits: two per cell from bit 14, the first whether it is passable.</summary>
    inline constexpr uint32_t PASSABLE_BIT = 14;
    inline constexpr int32_t CELL_BITS = 2;
    /// <summary>A path's extension with its dot, as the save drops it.</summary>
    inline constexpr int EXTENSION_LENGTH = 4;
    /// <summary>The tile set string offset of the GTiles set.</summary>
    inline constexpr int32_t GTILES_OFFSET = 0x32;
    /// <summary>The files the save writes: made, written only, other writers kept out.</summary>
    inline constexpr UINT SAVE_OPEN_FLAGS = CFile::modeCreate | CFile::shareDenyWrite | CFile::modeWrite;
    inline constexpr UINT PATH_MAP_OPEN_FLAGS = CFile::modeCreate | CFile::modeWrite;
    /// <summary>The archive buffer the save uses.</summary>
    inline constexpr int ARCHIVE_BUFFER = 0x1000;

    /// <summary><paramref name="path"/> without a 3-letter extension.</summary>
    CString withoutExtension(const CString& path)
    {
        const int length = path.GetLength();
        return length >= EXTENSION_LENGTH && path[length - EXTENSION_LENGTH] == '.'
                   ? path.Left(length - EXTENSION_LENGTH)
                   : path;
    }

    /// <summary>Tells that <paramref name="fileName"/> didn't open, by the format string <paramref name="format"/>.</summary>
    void reportOpenFailure(const CString& format, const CString& fileName)
    {
        CString message;
        message.Format(format, static_cast<LPCTSTR>(fileName));
        CString caption;
        caption.LoadString(MESSAGE_CAPTION);
        MessageBoxA(nullptr, message, caption, MB_OK);
    }
}

CString DreditorDoc::workingPath()
{
    const CString path = m_strPathName;
    char folder[MAX_PATH];
    _getcwd(folder, MAX_PATH);

    if (path.IsEmpty())
    {
        workingName = CString(folder) + WORKING_FOLDER;
        _mkdir(workingName);
        workingName += WORKING_NAME;
    }
    else
    {
        workingName = path.Left(path.ReverseFind('.'));
        // Original behaviour: the name is compared with a one-character string of 0xfe, never it, so "~1" is always
        // added.
        workingName += WORKING_SUFFIX;
    }

    return workingName;
}

bool DreditorDoc::saveMap(const CString& path, bool withMission)
{
    const CString base = withoutExtension(path);
    CString name = base.Right(base.GetLength() - base.ReverseFind('\\') - 1);

    if (withMission && !map->checkMission(database, name))
    {
        return false;
    }

    CreateDirectoryA(path.Left(path.ReverseFind('\\')), nullptr);
    const CString elvName = base + ELV_EXTENSION;
    const CString objName = base + OBJ_EXTENSION;
    const CString fitName = base + FIT_EXTENSION;
    CString openFailed;
    openFailed.LoadString(FILE_OPEN_FAILED);
    CFile elvFile;

    if (!elvFile.Open(elvName, SAVE_OPEN_FLAGS))
    {
        reportOpenFailure(openFailed, elvName);
        return false;
    }

    {
        CArchive archive(&elvFile, CArchive::store, ARCHIVE_BUFFER);
        map->writeElevations(archive);
        archive.Close();
    }

    elvFile.Close();
    CFile objFile;

    if (!objFile.Open(objName, CFile::modeCreate | CFile::modeWrite))
    {
        reportOpenFailure(openFailed, objName);
        return false;
    }

    {
        CArchive archive(&objFile, CArchive::store, ARCHIVE_BUFFER);
        map->writeObjects(archive, objFile);
        archive.Close();
    }

    // The terrain's settings: Template.txt with the block count, the elevations' range and the tile set.
    FILE* fit = nullptr;
    fopen_s(&fit, fitName, "w+");
    int32_t highest = 0;
    int32_t lowest = 0x7fffffff;

    for (int32_t col = 0; col < map->maxRow; col++)
    {
        for (int32_t row = 0; row < map->maxCol; row++)
        {
            const int32_t elevation = map->vertexElevation(col, row);
            highest = (std::max)(highest, elevation);
            lowest = (std::min)(lowest, elevation);
        }
    }

    const auto blocks = static_cast<int32_t>(static_cast<uint32_t>(map->maxRow + 1) / MAP_BLOCK_TILES);
    const char* const tileSet = objectLibrary->gTiles ? "GTiles" : "Tiles";

    // Original behaviour (EB-097): the lowest elevation goes in as HighestPoint and the highest as LowestPoint.
    if (fit != nullptr)
    {
        fprintf(fit, templateText, blocks, lowest + 1, highest + 1, tileSet);
        fclose(fit);
    }

    if (withMission && !map->saveMission(database, name))
    {
        return false;
    }

    // Original behaviour: a save to the working files leaves the document unmodified too.
    SetModifiedFlag(FALSE);
    return true;
}

void DreditorDoc::buildPathMaps()
{
    const CString working = workingPath();
    saveMap(working, false);
    const CString base = withoutExtension(working);
    const CString elvName = base + ELV_EXTENSION;
    const CString gmmName = base + GMM_EXTENSION;
    const CString datName = base + DAT_EXTENSION;
    // EB-099: the message's format string is never loaded, so a file that doesn't open shows an empty box.
    const CString noFormat;

    if (map->scenarioMap != nullptr)
    {
        map->scenarioMap->destroy();
        delete map->scenarioMap;
    }

    map->scenarioMap = new ScenarioMap();
    map->scenarioMap->buildFromTerrain(elvName, map, dataPath, tileSetStringOffset == GTILES_OFFSET);
    CFile datFile;

    if (datFile.Open(datName, PATH_MAP_OPEN_FLAGS))
    {
        map->scenarioMap->write(&datFile);
    }
    else
    {
        // EB-099: the original writes to the file that didn't open as well.
        reportOpenFailure(noFormat, datName);
    }

    if (map->globalMap != nullptr)
    {
        map->globalMap->destroy();
        delete map->globalMap;
    }

    map->globalMap = new GlobalMap();
    map->globalMap->init(map->scenarioMap);
    CFile gmmFile;

    if (gmmFile.Open(gmmName, PATH_MAP_OPEN_FLAGS))
    {
        map->globalMap->write(&gmmFile);
    }
    else
    {
        reportOpenFailure(noFormat, gmmName);
    }
}

void DreditorDoc::autoSave()
{
    saveMap(workingPath(), false);
}

bool DreditorDoc::saveAll(const CString& path, bool withMission)
{
    // Original behaviour (EB-113): the wait cursor is ended only when the save succeeds.
    BeginWaitCursor();
    undoActions.reset();
    const CString missionsFolder = dataPath + MISSIONS_FOLDER;
    const CString teditorFolder = dataPath + TEDITOR_FOLDER;
    CreateDirectoryA(missionsFolder, nullptr);
    CreateDirectoryA(missionsFolder + WARRIORS_FOLDER, nullptr);

    try
    {
        if (!saveMap(path, withMission))
        {
            return false;
        }

        const CString base = withoutExtension(path);
        const CString name = base.Right(base.GetLength() - base.ReverseFind('\\') - 1);
        const CString elvName = base + ELV_EXTENSION;
        const CString preName = base + PRE_EXTENSION;
        const CString bdgName = base + BDG_EXTENSION;
        const CString gmmName = base + GMM_EXTENSION;
        const CString datName = base + DAT_EXTENSION;
        const CString tgaName = base + TGA_EXTENSION;
        const CString logisticName = base + LOGISTIC_TGA_EXTENSION;
        // EB-099: the message's format string is never loaded, so a file that doesn't open shows an empty box.
        const CString noFormat;

        CFile bdgFile;

        if (!bdgFile.Open(bdgName, CFile::modeCreate | CFile::modeWrite))
        {
            reportOpenFailure(noFormat, bdgName);
            return false;
        }

        {
            CArchive archive(&bdgFile, CArchive::store, ARCHIVE_BUFFER);

            if (!map->writeBuildings(archive, bdgFile))
            {
                return false;
            }

            archive.Close();
        }

        if (map->scenarioMap != nullptr)
        {
            map->scenarioMap->destroy();
            delete map->scenarioMap;
        }

        map->scenarioMap = new ScenarioMap();
        map->scenarioMap->buildFromTerrain(elvName, map, dataPath, tileSetStringOffset == GTILES_OFFSET);
        CFile datFile;

        if (!datFile.Open(datName, CFile::modeCreate | CFile::modeWrite))
        {
            reportOpenFailure(noFormat, datName);
            return false;
        }

        map->scenarioMap->write(&datFile);

        if (map->globalMap != nullptr)
        {
            map->globalMap->destroy();
            delete map->globalMap;
        }

        map->globalMap = new GlobalMap();
        map->globalMap->init(map->scenarioMap);
        CFile gmmFile;

        if (!gmmFile.Open(gmmName, CFile::modeCreate | CFile::modeWrite))
        {
            reportOpenFailure(noFormat, gmmName);
            return false;
        }

        map->globalMap->write(&gmmFile);
        CFile preFile;

        if (!preFile.Open(preName, CFile::modeCreate | CFile::modeWrite))
        {
            reportOpenFailure(noFormat, preName);
            return false;
        }

        map->writePreload(preFile);
        map->writeTacticalImages(reinterpret_cast<const uint8_t*>(editorPalette->bitmapInfo->bmiColors), tgaName,
                                 nullptr, logisticName);

        if (withMission)
        {
            warnStuckMovers();

            MissionRecordSet missions(&database);
            missions.Open(dbOpenDynaset, nullptr, 0);
            CString filter;
            filter.Format("[MissionName] = '%s'", static_cast<const char*>(name));
            // Original behaviour: whether it was found isn't asked; a mission that isn't there takes the id of the
            // row the recordset is on.
            missions.FindFirst(filter);
            const auto missionId = static_cast<short>(missions.m_MissionID);

            // The original creates FitAbl.WriteFitAbl through COM from its registered class id ("GuidFromString
            // failed", "LoadFitAbl.dll failed" when it can't); the port's FitAbl is built in.
            FitAbl writer;

            if (!writer.WriteMissionFITFile(missionsFolder, teditorFolder, FITABL_DATABASE, missionId) ||
                !writer.WriteMissionScript(missionsFolder, teditorFolder, FITABL_DATABASE, missionId))
            {
                return false;
            }
        }
    }
    catch (CException* exception)
    {
        char error[ERROR_TEXT_SIZE];
        exception->GetErrorMessage(error, sizeof(error));
        exception->Delete();
        CString message;
        message.Format(IDS_FAILED_TO_SAVE_S, static_cast<const char*>(path), error);
        CString caption;
        caption.LoadString(MESSAGE_CAPTION);
        MessageBoxA(nullptr, message, caption, MB_OK);
        return false;
    }

    AfxMessageBox(IDS_MISSION_SUCCESSFULLY_SAVED, MB_OK, 0);
    EndWaitCursor();
    return true;
}

void DreditorDoc::warnStuckMovers()
{
    for (const EditorObject* mover : map->movers)
    {
        const MapTile& tile = map->scenarioMap->map[static_cast<size_t>(
            (mover->cellRow / MAP_CELLS_PER_TILE) * map->scenarioMap->width + mover->cellCol / MAP_CELLS_PER_TILE)];
        const int32_t cell =
            (mover->cellRow % MAP_CELLS_PER_TILE) * MAP_CELLS_PER_TILE + mover->cellCol % MAP_CELLS_PER_TILE;

        if ((tile.cells >> (PASSABLE_BIT + cell * CELL_BITS) & 1) != 0)
        {
            continue;
        }

        int32_t x = 0;
        int32_t y = 0;
        map->cellToWorld(mover->cellCol, mover->cellRow, &x, &y);
        const std::string& typeName =
            objectLibrary->moverGroups[objectIdGroup(mover->id)].types[objectIdType(mover->id)].name;
        CString message;
        message.Format(IDS_THE_S_AT_LD, typeName.c_str(), static_cast<long>(x), static_cast<long>(y));
        AfxMessageBox(message, MB_OK, 0);
    }
}

BOOL DreditorDoc::OnSaveDocument(LPCTSTR lpszPathName)
{
    const CString path = lpszPathName;
    SetPathName(path, TRUE);
    return saveAll(path, true) ? TRUE : FALSE;
}

BOOL DreditorDoc::DoSave(LPCTSTR lpszPathName, BOOL bReplace)
{
    CString newName = lpszPathName;

    if (newName.IsEmpty())
    {
        // MFC's DoSave proposes a name from the title; the editor's dialog doesn't take it.
        CDocTemplate* docTemplate = GetDocTemplate();
        newName = m_strPathName;

        if (bReplace && newName.IsEmpty())
        {
            newName = m_strTitle;

            if (const int bad = newName.FindOneOf(" #%;/\\"); bad != -1)
            {
                newName.ReleaseBuffer(bad);
            }

            CString extension;

            if (docTemplate->GetDocString(extension, CDocTemplate::filterExt) && !extension.IsEmpty())
            {
                newName += extension;
            }
        }

        SaveDialog dialog(&database, true);

        if (dialog.DoModal() != IDOK)
        {
            return FALSE;
        }

        SetPathName(dialog.mapPath, TRUE);
        newName = dialog.mapPath;
    }

    CWaitCursor wait;

    if (!OnSaveDocument(newName))
    {
        if (lpszPathName == nullptr)
        {
            // MFC's DoSave: a new file that failed to save is deleted.
            try
            {
                CFile::Remove(newName);
            }
            catch (CException* exception)
            {
                exception->Delete();
            }
        }

        return FALSE;
    }

    if (bReplace)
    {
        SetPathName(newName, TRUE);
    }

    return TRUE;
}

void DreditorDoc::OnQuickSave()
{
    CString path = m_strPathName;

    if (path.IsEmpty())
    {
        SaveDialog dialog(&database, true);

        if (dialog.DoModal() == IDCANCEL)
        {
            return;
        }

        path = dialog.mapPath;
        SetPathName(path, TRUE);
    }

    saveMap(path, true);
    undoActions.reset();
}

void DreditorDoc::OnFileSave()
{
    if (!map->checkMission(database, CString()))
    {
        return;
    }

    if (!m_strPathName.IsEmpty())
    {
        OnSaveDocument(m_strPathName);
        return;
    }

    // Original behaviour: the mission is checked a second time.
    if (!map->checkMission(database, CString()))
    {
        return;
    }

    SaveDialog dialog(&database, true);

    if (dialog.DoModal() == IDOK)
    {
        SetPathName(dialog.mapPath, TRUE);
        OnSaveDocument(dialog.mapPath);
    }
}

void DreditorDoc::OnFileSaveAs()
{
    if (!map->checkMission(database, CString()))
    {
        return;
    }

    SaveDialog dialog(&database, true);

    if (dialog.DoModal() == IDOK)
    {
        SetPathName(dialog.mapPath, TRUE);
        OnSaveDocument(dialog.mapPath);
    }
}

void DreditorDoc::OnFileSaveMapOnly()
{
    if (m_strPathName.IsEmpty())
    {
        SaveDialog dialog(&database, true);

        if (dialog.DoModal() != IDOK)
        {
            return;
        }

        SetPathName(dialog.mapPath, TRUE);
    }

    if (!saveAll(m_strPathName, false))
    {
        CString message;
        message.LoadString(AFX_IDP_FAILED_TO_SAVE_DOC);
        CString caption;
        caption.LoadString(MESSAGE_CAPTION);
        MessageBoxA(nullptr, message, caption, MB_OK);
    }
}
