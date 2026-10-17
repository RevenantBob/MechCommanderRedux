#pragma once

#include "DaoDatabase.h"
#include "DaoRecordset.h"
#include "VbRuntime.h"

// FitAbl.dll's WriteFitAbl class (VB6, FASA Interactive 1999, version 99.03.0002), translated to C++: writes a
// mission's .FIT file and its ABL scripts (.ABL and the _VAR, _INIT, _STR, _LOP and _MP include files) from the
// mission database. The editor called it through COM when saving a mission; the port calls it directly. Method
// names follow what each of the class's procedures does; their order and the fields' meaning are the original's
// (docs/editor/plan.md, E7, has the procedure map).

/// <summary>A recordset as FitAbl used DAO's: NoMatch after a Find, rows read by name.</summary>
class FitAblRecordset : public DaoRecordset
{
public:
    /// <summary>A closed recordset on <paramref name="database"/>.</summary>
    explicit FitAblRecordset(DaoDatabase& database);

    /// <summary>Opens <paramref name="sql"/> (a SELECT, or a table's name) as a dynaset.</summary>
    void OpenDynaset(const CString& sql);
    /// <summary>FindFirst, setting <see cref="noMatch"/>.</summary>
    void FindFirst(const CString& filter);
    /// <summary>FindNext, setting <see cref="noMatch"/>.</summary>
    void FindNext(const CString& filter);
    /// <summary>rs!<paramref name="name"/>: the current row's value of that column (Null when it is NULL).</summary>
    COleVariant operator[](const char* name);

    /// <summary>Whether the last Find found no row (DAO's NoMatch).</summary>
    bool noMatch = false;
};

/// <summary>FitAbl.WriteFitAbl: the mission's .FIT and ABL writers.</summary>
class FitAbl
{
public:
    FitAbl() = default;
    FitAbl(const FitAbl&) = delete;
    FitAbl& operator=(const FitAbl&) = delete;

    /// <summary>
    /// Writes mission <paramref name="missionId"/>'s .FIT file (its name the mission's MissionFileName) to
    /// <paramref name="outputPath"/>, from the database <paramref name="databaseName"/> in
    /// <paramref name="databasePath"/>.
    /// </summary>
    /// <returns>Whether it was written; a problem with the data is told in a message box first.</returns>
    /// <remarks>Public procedure 1 (dispatch id 0x60030003), 0x11026bd0.</remarks>
    bool WriteMissionFITFile(const CString& outputPath, const CString& databasePath, const CString& databaseName,
                             short missionId);
    /// <summary>
    /// <paramref name="text"/> made into an ABL identifier: punctuation and spaces become underscores, and a text
    /// starting with a digit is refused with a message.
    /// </summary>
    /// <remarks>Public procedure 2 (0x6003000d), 0x11027ef0.</remarks>
    COleVariant MakeAblIdentifier(CString& text);
    /// <summary>
    /// Writes mission <paramref name="missionId"/>'s ABL script (.ABL) and its include files to
    /// <paramref name="outputPath"/>.
    /// </summary>
    /// <returns>Whether they were written.</returns>
    /// <remarks>Public procedure 3 (0x60030014), 0x11059290.</remarks>
    bool WriteMissionScript(const CString& outputPath, const CString& databasePath, const CString& databaseName,
                            short missionId);
    /// <summary>Writes the lookout points' include file <paramref name="pathName"/> for <paramref name="missionId"/>.</summary>
    /// <remarks>Public procedure 4 (0x60030016), 0x11059420.</remarks>
    bool WriteLookOutIncludeFile(short missionId, const CString& pathName);

private:
    /// <summary>
    /// Opens the database <paramref name="name"/> (".MDB" added when it has no .mdb) in <paramref name="path"/>.
    /// </summary>
    /// <returns>False, after a message, when the file isn't there.</returns>
    /// <remarks>0x11011880.</remarks>
    bool openDatabase(const CString& path, const CString& name);
    /// <summary>Sets the folder the files go to (a backslash added).</summary>
    /// <returns>False, after a message, when the folder isn't there.</returns>
    /// <remarks>0x11011d90.</remarks>
    bool setOutputPath(const CString& path);
    /// <summary>Checks mission <paramref name="missionId"/>'s data and writes its .FIT file.</summary>
    /// <returns>False, after a message, when the data is wrong.</returns>
    /// <remarks>0x11011fb0.</remarks>
    bool writeFitFile(short missionId);
    /// <summary>Writes a drop zone's slot offsets for its formation and rotation.</summary>
    /// <remarks>Part of 0x11011fb0 (0x11016b6a-0x1101e733).</remarks>
    void writeDropZoneOffsets(FitAblRecordset& dropZone);

    /// <summary>
    /// Finds mission <paramref name="missionId"/> in NewMissions (<see cref="mission"/>) and takes its script's name
    /// (<see cref="scriptName"/>, MissionFileName without ".abl").
    /// </summary>
    /// <returns>False, after a message, when it isn't there.</returns>
    /// <remarks>0x11026c80.</remarks>
    bool openMission(short missionId);
    /// <summary>Opens the _MP include file (#6) and writes handleMessage's start and its fixed cases.</summary>
    /// <remarks>0x110270e0.</remarks>
    void openMultiplayerInclude();
    /// <summary>Ends handleMessage (the last case, the switch, the function) and closes the _MP file.</summary>
    /// <remarks>0x11027620.</remarks>
    void closeMultiplayerInclude();
    /// <summary>
    /// Adds the line <paramref name="code"/> to handleMessage: with <paramref name="newCase"/>, as the first line of
    /// a new case (ending the last one), unless the cases are used up.
    /// </summary>
    /// <returns>The number of the case it went to (the message code that runs it).</returns>
    /// <remarks>0x11027710.</remarks>
    short addMultiplayerEvent(const CString& code, bool newCase);
    /// <summary>Opens the _VAR (#3) and _INIT (#4) include files and writes their headers.</summary>
    /// <remarks>0x11027930.</remarks>
    void openVariableIncludes();
    /// <summary>
    /// Declares the variable <paramref name="name"/> of <paramref name="type"/> in _VAR and, with an initial value,
    /// sets it in _INIT.
    /// </summary>
    /// <remarks>0x11027b60.</remarks>
    void addVariable(const CString& type, const CString& name, const CString& initialValue);
    /// <summary>
    /// Adds a line to _INIT: "<paramref name="name"/> = <paramref name="value"/>;", or the statement
    /// <paramref name="name"/> (";" added unless it ends with one) when there's no value.
    /// </summary>
    /// <remarks>0x11027c40.</remarks>
    void addInitialization(const CString& name, const CString& value);
    /// <summary>Adds "<paramref name="name"/> = <paramref name="value"/>;" to the _STR include file (#5).</summary>
    /// <remarks>0x11027db0.</remarks>
    void addStructureLine(const CString& name, const CString& value);
    /// <summary>Closes the _VAR and _INIT files.</summary>
    /// <remarks>0x11027e60.</remarks>
    void closeVariableIncludes();
    /// <summary>Writes the structures' include file (_STR.ABI) and their variables and initialization.</summary>
    /// <remarks>0x11028690.</remarks>
    void writeStructures(short missionId);
    /// <summary>
    /// Writes objective <paramref name="objective"/>'s success: its status, the Betty and music, or, when it has more
    /// than one action, action <paramref name="action"/>'s success flag; at indent <paramref name="level"/>.
    /// </summary>
    /// <remarks>0x11038e50.</remarks>
    void writeObjectiveSuccess(short objective, short action, short level);
    /// <summary>Writes objective <paramref name="objective"/>'s failure at indent <paramref name="level"/>.</summary>
    /// <remarks>0x11039c20.</remarks>
    void writeObjectiveFailure(short objective, short action, short level);
    /// <summary>
    /// Writes the checks of objective <paramref name="objectiveId"/>'s actions (its ObjectiveScripting rows), the
    /// objective numbered <paramref name="objective"/> of the side <paramref name="alignment"/>.
    /// </summary>
    /// <returns>False when the objective has no actions (the script then gets a place for hand-written checks).</returns>
    /// <remarks>0x1103a4b0.</remarks>
    bool writeObjectiveActions(long objectiveId, short objective, short alignment);
    /// <summary>
    /// Writes the success of objective <paramref name="objective"/> when all its <paramref name="actionCount"/>
    /// actions have succeeded (nothing for one action).
    /// </summary>
    /// <remarks>0x11052a40.</remarks>
    void writeActionsSuccess(short actionCount, short objective);
    /// <summary>Writes the mission's ABL script (.ABL), its objectives' checks and the lookout points' file.</summary>
    /// <returns>False, after a message, when the objectives' data is wrong.</returns>
    /// <remarks>0x11053610.</remarks>
    bool writeScript(short missionId);
    /// <summary>The game's id of the object on a map cell: block, vertex and the index of the object there.</summary>
    /// <remarks>0x110593d0.</remarks>
    static long objectId(short block, short vertex, short index);
    /// <summary>
    /// The index of the assignment <paramref name="uniqueEntry"/> among the mission's parts (its position in the
    /// .FIT's parts), counted as the FIT writer orders them.
    /// </summary>
    /// <remarks>0x1105cc20.</remarks>
    short assignmentIndex(long uniqueEntry);

    /// <summary>The mission's NewMissions row (open while the script is written).</summary>
    std::unique_ptr<FitAblRecordset> mission;
    /// <summary>The mission's objectives and their actions.</summary>
    std::unique_ptr<FitAblRecordset> objectives;
    std::unique_ptr<FitAblRecordset> actions;
    /// <summary>The MissionAssignments table.</summary>
    std::unique_ptr<FitAblRecordset> assignments;
    /// <summary>The handleMessage cases written so far, and the most there can be.</summary>
    short multiplayerCases = 0;
    short multiplayerCaseLimit = 0;
    /// <summary>The number of actions of the objective being written.</summary>
    short actionCount = 0;
    /// <summary>The Inner Sphere's primary objectives (a variant) and the Clan's.</summary>
    COleVariant innerSpherePrimaries;
    short clanPrimaries = 0;
    /// <summary>The script's name: MissionFileName without ".abl".</summary>
    CString scriptName;
    /// <summary>The mission's id.</summary>
    short missionNumber = 0;

    /// <summary>The database (DAO's Database object).</summary>
    DaoDatabase database;
    /// <summary>The database file's path.</summary>
    CString databaseFile;
    /// <summary>The output folder, ending with a backslash.</summary>
    CString outputFolder;
    /// <summary>The line being built (the class's work string).</summary>
    CString line;
    /// <summary>A second work string (a unit's name, the module line).</summary>
    CString text;
    /// <summary>A double quote.</summary>
    CString quote;
    /// <summary>The class's variant counters.</summary>
    COleVariant counter;
    COleVariant counter2;
    COleVariant counter3;
    COleVariant counter4;
    /// <summary>The Mechs, Vehicles and NewMW tables, open while the .FIT file is written.</summary>
    std::unique_ptr<FitAblRecordset> mechs;
    std::unique_ptr<FitAblRecordset> vehicles;
    std::unique_ptr<FitAblRecordset> warriors;
    /// <summary>The files by VB file number (#2 the .FIT or .ABL, #3 _VAR, #4 _INIT, #5 _STR, #6 _MP, #7 _LOP).</summary>
    std::array<VbFile, 8> files;
};
