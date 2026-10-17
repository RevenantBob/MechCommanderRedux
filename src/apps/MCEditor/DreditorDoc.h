#pragma once

#include "DaoDatabase.h"
#include "UndoList.h"

class EditorMap;
class MissionRecordSet;

/// <summary>The mission being edited.</summary>
class DreditorDoc : public CDocument
{
protected:
    /// <summary>
    /// Creates the object from serialization or a document template: <see cref="dataPath"/> from
    /// <see cref="EditorDataPath"/> (the original's settings, Paths\DataPath), with a trailing backslash.
    /// </summary>
    /// <remarks>0x004476b0.</remarks>
    DreditorDoc();
    DECLARE_DYNCREATE(DreditorDoc)

public:
    /// <summary>
    /// Starts a document: after the splash screen (at startup) or with the select tool, opens the mission database and
    /// asks for the solo or the multiplayer editor, then for a new or an existing mission, until one is made or
    /// opened or the choice is cancelled.
    /// </summary>
    /// <returns>Whether a mission was made or opened; when not, the last title and path come back.</returns>
    /// <remarks>0x00447930 (virtual table slot 28). A database that doesn't open ends it after a message.</remarks>
    BOOL OnNewDocument() override;
    /// <summary>
    /// The solo or multiplayer editor's start: at startup, asks for a new or an existing mission; a new one asks for
    /// the map's settings and makes it, an existing one goes through <see cref="openMission"/>. Shows the editor's
    /// view when done.
    /// </summary>
    /// <param name="startup">The main window doesn't exist yet: ask new or existing, and show the splash screen.</param>
    /// <returns>Whether a map was made (what the generator returned) or a mission opened.</returns>
    /// <remarks>0x00447ca0.</remarks>
    bool newOrOpen(bool multiplayer, bool startup);
    /// <summary>
    /// File > Open: asks whether to load the map with its mission data or the map only, then which mission or map,
    /// and opens it; asks again until one opens unless <paramref name="retry"/> is false.
    /// </summary>
    /// <returns>False when the first question is cancelled; else true.</returns>
    /// <remarks>0x00448100.</remarks>
    bool openMission(bool multiplayer, bool retry);
    /// <summary>
    /// Opens the mission named <paramref name="name"/> (<see cref="loadMissionRecord"/>), or says its file couldn't
    /// be opened.
    /// </summary>
    /// <remarks>0x0044d3d0. It begins a wait cursor it doesn't end.</remarks>
    bool loadMission(const CString& name);
    /// <summary>
    /// Opens the mission <paramref name="missionId"/> of <paramref name="missions"/>: titles the document after it,
    /// switches the libraries to the terrain set its map's .fit names, opens the map, reads the mission data onto it
    /// and shows its editor's view.
    /// </summary>
    /// <returns>Whether the mission was found and its map and data loaded; a database error says so in a message.</returns>
    /// <remarks>0x0044ca30.</remarks>
    bool loadMissionRecord(long missionId, MissionRecordSet& missions);
    /// <summary>
    /// The frame's File > Open (when no view takes the command): asks for a map and opens the mission of its name.
    /// </summary>
    /// <remarks>0x0044cea0.</remarks>
    void openMissionByMap();
    /// <summary>
    /// Covers the new map with water, forests and hills (<paramref name="water"/>, <paramref name="forest"/> and
    /// <paramref name="hill"/> percent of it) at random.
    /// </summary>
    /// <returns>True.</returns>
    /// <remarks>
    /// 0x00449290. Water (or land, above 60 percent) goes in ragged rectangles until the count of vertices selected
    /// for them reaches the coverage, the hills are single vertices raised and as many lowered, and the forest drops
    /// cover up to four tiles each. <paramref name="maxRow"/> and <paramref name="maxCol"/> are unused: the map's own
    /// sizes count.
    /// </remarks>
    bool generateMap(int32_t maxRow, int32_t maxCol, int32_t water, int32_t forest, int32_t hill);

    /// <summary>
    /// Opens the map <paramref name="path"/> (with or without its extension): loads the tile and object libraries
    /// for the terrain set its .fit names when that changed, then replaces <see cref="map"/> with one read from its
    /// .elv and .obj files.
    /// </summary>
    /// <param name="multiplayer">Make the multiplayer map rather than the single-player one.</param>
    /// <returns>
    /// What reading the tiles returned; false when <c>CDocument::OnOpenDocument</c> fails or a file doesn't open.
    /// </returns>
    /// <remarks>0x004486f0, called by the Open dialog flow and when a mission is opened.</remarks>
    bool openMap(const char* path, bool multiplayer);
    /// <summary>
    /// Opens <see cref="systemConfig"/> if it isn't open, finds the game's files through its paths, and loads the
    /// tile library and the object library (each only when its terrain set changed), the palette, the fade and
    /// translucency tables (<see cref="loadPalette"/>) and Teditor\Template.txt (<see cref="templateText"/>).
    /// </summary>
    /// <returns>
    /// False when System.cfg doesn't open (after a message), a file isn't found, or the palette doesn't load; the
    /// libraries' own results are ignored.
    /// </returns>
    /// <remarks>0x00449960.</remarks>
    bool loadLibraries();
    /// <summary>
    /// Finds <paramref name="fileName"/> in the folder System.cfg names for <paramref name="key"/> (relative to
    /// <see cref="dataPath"/> unless it has a drive). With <paramref name="searchCD"/>, a file missing there is looked
    /// for in the folder of "CD" + <paramref name="key"/> on the CD-ROM, asking for the disc until it is in.
    /// </summary>
    /// <param name="path">Gets the file's full path, also when it wasn't found.</param>
    /// <returns>
    /// Whether the file exists; false also when the key is missing (after a message) or the disc request is cancelled.
    /// </returns>
    /// <remarks>0x00449f50.</remarks>
    bool findDataFile(CFile& config, const char* key, const CString& fileName, CString& path, bool searchCD);
    /// <summary>
    /// Reads the quoted value after the word <paramref name="key"/> (any case) in the FIT text of
    /// <paramref name="config"/>: the rest of its line, from the second quote from the end, quotes removed.
    /// </summary>
    /// <returns>False, after a message, when the key isn't before "FITend".</returns>
    /// <remarks>0x0044a2f0.</remarks>
    bool readConfigPath(CFile& config, const char* key, CString& value);
    /// <summary>
    /// Loads the fade table and the palette (hb.pal; g-prefixed names for GTiles) from the PalettePath folder or
    /// misc.fst, fills the translucency table, sets up <c>editorPalette</c> and adds Teditor\logistic.pal.
    /// </summary>
    /// <returns>False when the palette isn't found, misc.fst doesn't open (after a message) or AlphaPal.ini fails.</returns>
    /// <remarks>0x0044aac0. Empties findNearestColor's cache first.</remarks>
    bool loadPalette(CFile& config);

    /// <summary>The actions done on the map, to undo and redo (+0x50; <c>undoList</c> points to it).</summary>
    UndoList undoActions{this};
    /// <summary>The map being edited, or null.</summary>
    EditorMap* map = nullptr;
    /// <summary>Teditor\Template.txt as read (not zero-terminated), or null before the first load.</summary>
    char* templateText = nullptr;
    /// <summary>The size of <see cref="templateText"/>'s buffer (the port's, for EB-042).</summary>
    UINT templateCapacity = 0;
    /// <summary>The game folder (<see cref="EditorDataPath"/>), ending with a backslash.</summary>
    CString dataPath;
    /// <summary>The working files' path without extension, set by <see cref="workingPath"/> (+0x74).</summary>
    CString workingName;

    /// <summary>
    /// The path of the working files the cell display and the autosave write: the document's path without its
    /// extension and "~1" added; with no path, the current folder's data\terrain\Working (making data\terrain).
    /// </summary>
    /// <remarks>0x0044a880.</remarks>
    CString workingPath();
    /// <summary>
    /// Saves the map to <paramref name="path"/> (any 3-letter extension dropped), making its folder: the terrain
    /// (.elv), the objects (.obj) and the terrain's settings from Template.txt (.fit); with
    /// <paramref name="withMission"/> the mission's data as well. A file that doesn't open is named in a message box.
    /// </summary>
    /// <returns>Whether it was saved; the document is then unmodified.</returns>
    /// <remarks>0x0044c4a0.</remarks>
    bool saveMap(const CString& path, bool withMission);
    /// <summary>
    /// Saves the map to the working files and builds its scenario and global (pathfinding) maps from them, writing
    /// them (.dat and .gmm) beside.
    /// </summary>
    /// <remarks>0x0044b180.</remarks>
    void buildPathMaps();
    /// <summary>Saves the map to the working files.</summary>
    /// <remarks>0x0044aa60, the view's timer's work (the view never starts the timer).</remarks>
    void autoSave();
    /// <summary>
    /// The full save: makes Data\Missions and its WARRIORS folder, saves the map (<see cref="saveMap"/>), then writes
    /// beside it the buildings (.bdg), the scenario and global maps rebuilt (.dat, .gmm), the tiles to preload (.pre)
    /// and the tactical map pictures (.tga, .log.tga). With <paramref name="withMission"/>, warns of units placed where
    /// they can't move and writes the mission's .fit and ABL scripts (FitAbl) into Data\Missions.
    /// </summary>
    /// <returns>Whether all of it was written; then "Mission successfully saved." is said.</returns>
    /// <remarks>0x0044b560. The undo list is emptied (and the document marked unmodified) first.</remarks>
    bool saveAll(const CString& path, bool withMission);
    /// <summary>
    /// Says of each mech and vehicle whose cell the scenario map doesn't let move that it will not be able to move.
    /// </summary>
    /// <remarks>Part of 0x0044b560 (0x0044bb4d-0x0044bc63).</remarks>
    void warnStuckMovers();
    /// <summary>Saves the map and its mission under <paramref name="lpszPathName"/>, which becomes the document's path.</summary>
    /// <remarks>0x00448670 (virtual table slot 30).</remarks>
    BOOL OnSaveDocument(LPCTSTR lpszPathName) override;
    /// <summary>
    /// MFC's DoSave with the editor's <see cref="SaveDialog"/> in place of the file dialog: without
    /// <paramref name="lpszPathName"/>, asks for the name; then saves, deleting a new file that failed to save.
    /// </summary>
    /// <remarks>0x0044d6b0 (virtual table slot 38).</remarks>
    BOOL DoSave(LPCTSTR lpszPathName, BOOL bReplace = TRUE) override;
    /// <summary>The game's System.cfg, opened by the first <see cref="loadLibraries"/> and kept open.</summary>
    CFile systemConfig;
    /// <summary>
    /// The mission database, Teditor\FitAbl.mdb under <see cref="dataPath"/> (FitAbl.db beside it in the port), opened
    /// when the first mission is chosen and kept open.
    /// </summary>
    DaoDatabase database;

protected:
    /// <summary>Handles ON_COMMAND(ID_QUICK_SAVE).</summary>
    afx_msg void OnQuickSave();

    /// <summary>Handles ON_COMMAND(ID_FILE_SAVE).</summary>
    afx_msg void OnFileSave();

    /// <summary>Handles ON_COMMAND(ID_FILE_SAVE_AS).</summary>
    afx_msg void OnFileSaveAs();

    /// <summary>Handles ON_COMMAND(ID_FILE_SAVE_MAP_ONLY).</summary>
    afx_msg void OnFileSaveMapOnly();

    DECLARE_MESSAGE_MAP()
};
