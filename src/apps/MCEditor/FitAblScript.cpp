#include "stdafx.h"
#include "FitAbl.h"

// FitAbl's ABL script writer (0x11026c80-0x11027ef0, 0x11038e50-0x11039c20, 0x11052a40-0x110593d0): the mission's
// .ABL module, the include files' helpers, and the objectives' success and failure code.

namespace
{
    /// <summary>The caption of the class's error messages.</summary>
    const char* const ERROR_TITLE = "Error";
    /// <summary>The files' numbers.</summary>
    inline constexpr int SCRIPT_FILE = 2;
    inline constexpr int VARIABLE_FILE = 3;
    inline constexpr int INIT_FILE = 4;
    inline constexpr int STRUCTURE_FILE = 5;
    inline constexpr int MULTIPLAYER_FILE = 6;
    /// <summary>The cases handleMessage can have before the reserved message codes (250 and up).</summary>
    inline constexpr short MULTIPLAYER_CASE_LIMIT = 249;
    /// <summary>The columns the code is indented to: an objective's actions, and each level under them.</summary>
    inline constexpr int ACTION_COLUMN = 18;
    inline constexpr int LEVEL_STEP = 6;
    /// <summary>The objectives' Alignment of the Clan's.</summary>
    inline constexpr short CLAN_ALIGNMENT = 1;
    /// <summary>The characters MakeAblIdentifier deletes.</summary>
    const char* const IDENTIFIER_PUNCTUATION = "&;=%^()-+.!\\/:";

    /// <summary>A text's variant.</summary>
    COleVariant literal(const char* value)
    {
        return Vb::String(value);
    }

    /// <summary>The column of indent <paramref name="level"/> (0 counts as 1).</summary>
    int levelColumn(short level)
    {
        return (level == 0 ? 1 : level) * LEVEL_STEP + ACTION_COLUMN;
    }
}

COleVariant FitAbl::MakeAblIdentifier(CString& text)
{
    // Original behaviour: the identifier is made in the argument (ByRef); the function's value stays Empty.
    if (Vb::Val(text) > 0)
    {
        Vb::MsgBox("ABL Identifier '" + text + "' starts with a number.", Vb::MSGBOX_CRITICAL, ERROR_TITLE);
        return COleVariant();
    }

    // Spaces become underscores, and one goes in front.
    CString rest = text;
    int space = rest.Find(' ') + 1;

    if (space == 0)
    {
        text = "_" + rest;
    }
    else
    {
        text.Empty();

        while (space > 0)
        {
            text = text + "_" + rest.Left(space - 1);
            rest = rest.Right(rest.GetLength() - space);
            space = rest.Find(' ') + 1;

            if (space == 0)
            {
                text = text + "_" + rest;
            }
        }
    }

    // The punctuation goes.
    const CString punctuation = IDENTIFIER_PUNCTUATION;
    int length = text.GetLength();

    for (int i = 1; i <= punctuation.GetLength(); ++i)
    {
        int found = text.Find(punctuation[i - 1]) + 1;

        while (found > 0)
        {
            text = text.Left(found - 1) + text.Right(length - found);
            --length;
            found = text.Find(punctuation[i - 1]) + 1;
        }
    }

    return COleVariant();
}

bool FitAbl::WriteMissionScript(const CString& outputPath, const CString& databasePath, const CString& databaseName,
                                short missionId)
{
    try
    {
        missionNumber = missionId;

        if (!openDatabase(databasePath, databaseName) || !setOutputPath(outputPath) || !openMission(missionId))
        {
            return false;
        }

        openVariableIncludes();
        openMultiplayerInclude();

        if (!writeScript(missionId))
        {
            return false;
        }

        writeStructures(missionId);
        closeVariableIncludes();
        closeMultiplayerInclude();
        return true;
    }
    catch (const VbError& error)
    {
        Vb::MsgBox(error.description, MB_ICONEXCLAMATION);
    }
    catch (CException* exception)
    {
        char message[512];
        exception->GetErrorMessage(message, sizeof message);
        exception->Delete();
        Vb::MsgBox(message, MB_ICONEXCLAMATION);
    }

    return false;
}

bool FitAbl::openMission(short missionId)
{
    mission = std::make_unique<FitAblRecordset>(database);
    mission->OpenDynaset("NewMissions");
    mission->FindFirst("[MissionID] = " + Vb::CStr(missionId));

    if (mission->GetRecordCount() < 1)
    {
        Vb::MsgBox("Incorrect record count in NewMissions table.", Vb::MSGBOX_CRITICAL, ERROR_TITLE);
        return false;
    }

    scriptName = Vb::CStr((*mission)["MissionFileName"]);

    if (scriptName.Right(4) == ".abl" || scriptName.Right(4) == ".ABL")
    {
        scriptName = scriptName.Left(scriptName.GetLength() - 4);
    }

    return true;
}

void FitAbl::openMultiplayerInclude()
{
    multiplayerCases = 0;
    line = outputFolder + scriptName + "_MP.ABI";
    VbFile& file = files[MULTIPLAYER_FILE];
    file.Open(line);
    file.Line("//*********************************************************************************");
    file.Text("// MultiPlayer Include file for mission ").Value((*mission)["MissionName"]).EndLine();
    file.Line();
    file.Line("function handleMessage;");
    file.Line();
    file.Tab(6).Line("Var");
    file.Line();
    file.Tab(12).Line("integer MsgParams;");
    file.Tab(12).Line("integer MsgCode;");
    file.Line();
    file.Tab(6).Line("Code");
    file.Line();
    file.Tab(12).Line("MsgCode = getMessage(MsgParams);");
    file.Line();
    file.Tab(12).Line("Switch (MsgCode)");
    file.Line();
    file.Tab(18).Line("case MSG_GENERAL_ALARM_ON:");
    file.Line();
    file.Tab(24).Line("GeneralAlarm = TRUE;");
    file.Tab(24).Line("PlayGASound = TRUE;");
    file.Tab(24).Line("PlayPASound = FALSE;");
    file.Tab(24).Line("endcase;");
    file.Line();
    file.Tab(18).Line("case MSG_GENERAL_ALARM_OFF:");
    file.Line();
    file.Tab(24).Line("PlayGASound = FALSE;");
    file.Tab(24).Line("endcase;");
    file.Line();
    file.Tab(18).Line("case MSG_PERIMETER_ALARM_ON:");
    file.Line();
    file.Tab(24).Line("PlayPASound = TRUE;");
    file.Tab(24).Line("endcase;");
    file.Line();
    file.Tab(18).Line("case MSG_PERIMETER_ALARM_OFF:");
    file.Line();
    file.Tab(24).Line("PlayPASound = FALSE;");
    file.Tab(24).Line("endcase;");
    file.Line();
    file.Tab(18).Line("case MSG_SET_EXIT_TIMER:");
    file.Line();
    file.Tab(24).Line("setTimer(EXIT_TIMER,2.0);");
    file.Tab(24).Line("ExitTimerSet = TRUE;");
    file.Tab(24).Line("endcase;");
    file.Line();
    file.Tab(18).Line("case MSG_PLAY_END_MUSIC:");
    file.Line();
    file.Tab(24).Line("switch (MsgParams)");
    file.Tab(30).Line("case INNER_SPHERE_VICTORY:");
    file.Tab(36).Line("if (getHomeTeam == PLAYER_FORCE) then");
    file.Tab(40).Line("PlayDigitalMusic(VICTORY_MUSIC);");
    file.Tab(36).Line("else");
    file.Tab(40).Line("PlayDigitalMusic(DEFEAT_MUSIC);");
    file.Tab(36).Line("endif;");
    file.Tab(36).Line("endcase;");
    file.Tab(30).Line("case CLAN_VICTORY:");
    file.Tab(36).Line("if (getHomeTeam == CLAN_FORCE) then");
    file.Tab(40).Line("PlayDigitalMusic(VICTORY_MUSIC);");
    file.Tab(36).Line("else");
    file.Tab(40).Line("PlayDigitalMusic(DEFEAT_MUSIC);");
    file.Tab(36).Line("endif;");
    file.Tab(36).Line("endcase;");
    file.Tab(30).Line("case JOINT_LOSS:");
    file.Tab(36).Line("PlayDigitalMusic(DEFEAT_MUSIC);");
    file.Tab(36).Line("endcase;");
    file.Tab(30).Line("case JOINT_VICTORY:");
    file.Tab(36).Line("PlayDigitalMusic(VICTORY_MUSIC);");
    file.Tab(36).Line("endcase;");
    file.Tab(24).Line("endswitch;");
    file.Tab(24).Line("endcase;");
    file.Line();
}

void FitAbl::closeMultiplayerInclude()
{
    VbFile& file = files[MULTIPLAYER_FILE];

    if (multiplayerCases > 0)
    {
        file.Tab(24).Line("endcase;");
        file.Line();
    }

    file.Tab(12).Line("endswitch;");
    file.Line();
    file.Line("endfunction;");
    file.Line();
    file.Close();
}

short FitAbl::addMultiplayerEvent(const CString& code, bool newCase)
{
    VbFile& file = files[MULTIPLAYER_FILE];

    if (newCase)
    {
        if (multiplayerCases >= multiplayerCaseLimit)
        {
            Vb::MsgBox("Too many Multiplayer Events.");
            return multiplayerCases;
        }

        if (multiplayerCases > 0)
        {
            file.Tab(24).Line("endcase;");
            file.Line();
        }

        ++multiplayerCases;
        file.Tab(18).Line("Case " + Vb::CStr(multiplayerCases) + ":");
        file.Line();
    }

    file.Tab(24).Line(code);
    return multiplayerCases;
}

void FitAbl::openVariableIncludes()
{
    line = outputFolder + scriptName + "_VAR.ABI";
    files[VARIABLE_FILE].Open(line);
    files[VARIABLE_FILE].Line("//*********************************************************************************");
    files[VARIABLE_FILE].Text("// Variable Include file for mission ").Value((*mission)["MissionName"]).EndLine();
    files[VARIABLE_FILE].Line();
    line = outputFolder + scriptName + "_INIT.ABI";
    files[INIT_FILE].Open(line);
    files[INIT_FILE].Line("//*********************************************************************************");
    files[INIT_FILE].Text("// Init Include file for mission ").Value((*mission)["MissionName"]).EndLine();
    files[INIT_FILE].Line();
}

void FitAbl::addVariable(const CString& type, const CString& name, const CString& initialValue)
{
    files[VARIABLE_FILE].Text("     ").Text(type).Tab(40).Text(name).Line(";");

    if (!initialValue.IsEmpty())
    {
        files[INIT_FILE].Line("     " + name + " = " + initialValue + ";");
    }
}

void FitAbl::addInitialization(const CString& name, const CString& value)
{
    if (!value.IsEmpty())
    {
        files[INIT_FILE].Line("     " + name + " = " + value + ";");
    }
    else if (name.Right(1) == ";")
    {
        files[INIT_FILE].Line("     " + name);
    }
    else
    {
        files[INIT_FILE].Line("     " + name + ";");
    }
}

void FitAbl::addStructureLine(const CString& name, const CString& value)
{
    files[STRUCTURE_FILE].Line("     " + name + " = " + value + ";");
}

void FitAbl::closeVariableIncludes()
{
    files[VARIABLE_FILE].Close();
    files[INIT_FILE].Close();
}

long FitAbl::objectId(short block, short vertex, short index)
{
    // VB's Integer and Long arithmetic, which raises Overflow rather than wrapping.
    const long id = (static_cast<long>(block) * 400 * 8) + static_cast<long>(vertex) * 8 + 0x1000 + index;
    return id;
}

void FitAbl::writeObjectiveSuccess(short objective, short action, short level)
{
    VbFile& script = files[SCRIPT_FILE];
    const int column = levelColumn(level);
    const bool multiplayer = Vb::Eq((*mission)["Multiplayer"], Vb::Boolean(true));

    if (actionCount == 1)
    {
        if (multiplayer)
        {
            line = "SetObjectiveStatus(" + Vb::CStr(objective) + ",SUCCESS);";
            short messageCode = addMultiplayerEvent(line, true);

            if (Vb::Eq((*objectives)["Priority"], Vb::Integer(0)))
            {
                messageCode = addMultiplayerEvent(Vb::Eq((*objectives)["Alignment"], Vb::Integer(CLAN_ALIGNMENT))
                                                      ? "ClanVictoryLevel = ClanVictoryLevel + 1;"
                                                      : "VictoryLevel = VictoryLevel + 1;",
                                                  false);
            }

            line = Vb::Eq((*objectives)["Alignment"], Vb::Integer(CLAN_ALIGNMENT))
                       ? "if (getHomeTeam == CLAN_FORCE) then"
                       : "if (getHomeTeam == PLAYER_FORCE) then";
            messageCode = addMultiplayerEvent(line, false);
            messageCode = addMultiplayerEvent("     playBetty(BETTY_OBJECTIVE_COMPLETE);", false);
            line = Vb::CStr(Vb::Cat(Vb::Cat(Vb::Cat(Vb::Cat(literal("     if ((VictoryLevel < "), innerSpherePrimaries),
                                                    literal(") AND (ClanVictoryLevel < ")),
                                            Vb::Integer(clanPrimaries)),
                                    literal(")) then")));
            messageCode = addMultiplayerEvent(line, false);
            messageCode = addMultiplayerEvent("          PlayDigitalMusic(OBJECTIVE_SUCCESS_MUSIC);", false);
            messageCode = addMultiplayerEvent("     endif;", false);
            messageCode = addMultiplayerEvent("endif;", false);
            line = "Objective_" + Vb::CStr(objective) + "_Decided = TRUE;";
            messageCode = addMultiplayerEvent(line, false);
            line = "sendMessage(" + Vb::CStr(messageCode) + ",0);";
            script.Tab(column).Line(line);
            script.Tab(column).Line("handleMessage;");
        }
        else
        {
            line = "SetObjectiveStatus(" + Vb::CStr(objective) + ",SUCCESS);";
            script.Tab(column).Line(line);
            script.Tab(column).Line("playBetty(BETTY_OBJECTIVE_COMPLETE);");

            // Original behaviour (EB-101): a secondary objective writes its status line again where a primary one counts
            // its victory level.
            if (Vb::Eq((*objectives)["Priority"], Vb::Integer(0)))
            {
                line = "VictoryLevel = VictoryLevel + 1;";
            }

            script.Tab(column).Line(line);
            line = "Objective_" + Vb::CStr(objective) + "_Decided = TRUE;";
            script.Tab(column).Line(line);
            line = Vb::CStr(Vb::Cat(Vb::Cat(literal("if (VictoryLevel < "), innerSpherePrimaries), literal(") then")));
            script.Tab(column).Line(line);
            script.Tab(column + LEVEL_STEP).Line("PlayDigitalMusic(OBJECTIVE_SUCCESS_MUSIC);");
            script.Tab(column).Line("endif;");
        }

        return;
    }

    // An objective of several actions: this action's success, the objective's when all are (writeActionsSuccess).
    line = "Obj_" + Vb::CStr(objective) + "_Action_" + Vb::CStr(action) + "_Success = TRUE;";

    if (Vb::Eq((*mission)["Multiplayer"], Vb::Boolean(true)))
    {
        line = "sendMessage(" + Vb::CStr(addMultiplayerEvent(line, true)) + ",0);";
        script.Tab(column).Line(line);
        script.Tab(column).Line("handleMessage;");
    }
    else
    {
        script.Tab(column).Line(line);
    }
}

void FitAbl::writeObjectiveFailure(short objective, short action, short level)
{
    UNREFERENCED_PARAMETER(action);
    VbFile& script = files[SCRIPT_FILE];
    const int column = levelColumn(level);

    if (Vb::Eq((*mission)["Multiplayer"], Vb::Boolean(true)))
    {
        short messageCode = addMultiplayerEvent("SetObjectiveStatus(" + Vb::CStr(objective) + ",FAILED);", true);
        line = Vb::Eq((*objectives)["Alignment"], Vb::Integer(CLAN_ALIGNMENT))
                   ? "if (getHomeTeam == CLAN_FORCE) then"
                   : "if (getHomeTeam == PLAYER_FORCE) then";
        messageCode = addMultiplayerEvent(line, false);
        messageCode = addMultiplayerEvent(Vb::Eq((*objectives)["Priority"], Vb::Integer(0))
                                              ? "     playBetty(BETTY_CANNOT_COMP_OBJ);"
                                              : "     playBetty(BETTY_SECONDARY_FAILED);",
                                          false);

        if (Vb::Ne((*objectives)["Priority"], Vb::Integer(0)))
        {
            messageCode = addMultiplayerEvent("     PlayDigitalMusic(OBJECTIVE_FAIL_MUSIC);", false);
        }

        messageCode = addMultiplayerEvent("endif;", false);
        messageCode = addMultiplayerEvent("Objective_" + Vb::CStr(objective) + "_Decided = TRUE;", false);
        line = "sendMessage(" + Vb::CStr(messageCode) + ",0);";
        script.Tab(column).Line(line);
        script.Tab(column).Line("handleMessage;");
        return;
    }

    line = "SetObjectiveStatus(" + Vb::CStr(objective) + ",FAILED);";
    script.Tab(column).Line(line);
    script.Tab(column).Line(Vb::Eq((*objectives)["Priority"], Vb::Integer(0)) ? "playBetty(BETTY_CANNOT_COMP_OBJ);"
                                                                              : "playBetty(BETTY_SECONDARY_FAILED);");
    line = "Objective_" + Vb::CStr(objective) + "_Decided = TRUE;";
    script.Tab(column).Line(line);

    if (Vb::Ne((*objectives)["Priority"], Vb::Integer(0)))
    {
        script.Tab(column).Line("PlayDigitalMusic(OBJECTIVE_FAIL_MUSIC);");
    }
}

void FitAbl::writeActionsSuccess(short count, short objective)
{
    if (count <= 1)
    {
        return;
    }

    VbFile& script = files[SCRIPT_FILE];
    line = "if (";

    for (short action = 1; action <= count; ++action)
    {
        line = line + "(Obj_" + Vb::CStr(objective) + "_Action_" + Vb::CStr(action) + "_Success)";
        line = line + (action < count ? " AND " : ") then");
    }

    script.Tab(ACTION_COLUMN).Line(line);

    if (Vb::Eq((*mission)["Multiplayer"], Vb::Boolean(true)))
    {
        CString text = "SetObjectiveStatus(" + Vb::CStr(objective) + ",SUCCESS);";
        short messageCode = addMultiplayerEvent(text, true);

        if (Vb::Eq((*objectives)["Priority"], Vb::Integer(0)))
        {
            messageCode = addMultiplayerEvent(Vb::Eq((*objectives)["Alignment"], Vb::Integer(CLAN_ALIGNMENT))
                                                  ? "ClanVictoryLevel = ClanVictoryLevel + 1;"
                                                  : "VictoryLevel = VictoryLevel + 1;",
                                              false);
        }

        text = Vb::Eq((*objectives)["Alignment"], Vb::Integer(CLAN_ALIGNMENT))
                   ? "if (getHomeTeam == CLAN_FORCE) then"
                   : "if (getHomeTeam == PLAYER_FORCE) then";
        messageCode = addMultiplayerEvent(text, false);
        messageCode = addMultiplayerEvent("     playBetty(BETTY_OBJECTIVE_COMPLETE);", false);
        text = Vb::CStr(Vb::Cat(Vb::Cat(Vb::Cat(Vb::Cat(literal("     if ((VictoryLevel < "), innerSpherePrimaries),
                                                literal(") AND (ClanVictoryLevel < ")),
                                        Vb::Integer(clanPrimaries)),
                                literal(")) then")));
        messageCode = addMultiplayerEvent(text, false);
        messageCode = addMultiplayerEvent("          PlayDigitalMusic(OBJECTIVE_SUCCESS_MUSIC);", false);
        messageCode = addMultiplayerEvent("     endif;", false);
        messageCode = addMultiplayerEvent("endif;", false);
        text = "Objective_" + Vb::CStr(objective) + "_Decided = TRUE;";
        messageCode = addMultiplayerEvent(text, false);
        text = "sendMessage(" + Vb::CStr(messageCode) + ",0);";
        script.Tab(ACTION_COLUMN + LEVEL_STEP).Line(text);
        script.Tab(ACTION_COLUMN + LEVEL_STEP).Line("handleMessage;");
    }
    else
    {
        CString text = "SetObjectiveStatus(" + Vb::CStr(objective) + ",SUCCESS);";
        script.Tab(ACTION_COLUMN + LEVEL_STEP).Line(text);
        script.Tab(ACTION_COLUMN + LEVEL_STEP).Line("playBetty(BETTY_OBJECTIVE_COMPLETE);");

        // Original behaviour (EB-101): as writeObjectiveSuccess, the status line again for a secondary objective.
        if (Vb::Eq((*objectives)["Priority"], Vb::Integer(0)))
        {
            text = "VictoryLevel = VictoryLevel + 1;";
        }

        script.Tab(ACTION_COLUMN + LEVEL_STEP).Line(text);
        text = "Objective_" + Vb::CStr(objective) + "_Decided = TRUE;";
        script.Tab(ACTION_COLUMN + LEVEL_STEP).Line(text);
        text = Vb::CStr(Vb::Cat(Vb::Cat(literal("if (VictoryLevel < "), innerSpherePrimaries), literal(") then")));
        script.Tab(ACTION_COLUMN + LEVEL_STEP).Line(text);
        script.Tab(ACTION_COLUMN + 2 * LEVEL_STEP).Line("PlayDigitalMusic(OBJECTIVE_SUCCESS_MUSIC);");
        script.Tab(ACTION_COLUMN + LEVEL_STEP).Line("endif;");
    }

    script.Tab(ACTION_COLUMN).Line("endif;");
    script.Line();
}

bool FitAbl::writeScript(short missionId)
{
    const CString quote = "\"";
    const CString missionFilter = "[MissionID] = " + Vb::CStr(missionNumber);
    objectives = std::make_unique<FitAblRecordset>(database);
    objectives->OpenDynaset("SELECT * FROM [Objectives] WHERE [MissionID] = " + Vb::CStr(missionId) +
                            " ORDER BY [Alignment] DESC, [Objective] ASC");
    objectives->MoveLast();
    const bool multiplayer = Vb::Eq((*mission)["Multiplayer"], Vb::Boolean(true));

    if (multiplayer ? objectives->GetRecordCount() < 2 : objectives->GetRecordCount() < 1)
    {
        Vb::MsgBox("Incorrect record count in Objectives table.", Vb::MSGBOX_CRITICAL, ERROR_TITLE);
        return false;
    }

    actions = std::make_unique<FitAblRecordset>(database);
    actions->OpenDynaset("SELECT * FROM [ObjectiveScripting] WHERE [MISSIONID] = " + Vb::CStr(missionId));
    actions->MoveLast();

    if (actions->GetRecordCount() < objectives->GetRecordCount())
    {
        Vb::MsgBox("Incorrect record count in ObjectiveScripting table.", Vb::MSGBOX_CRITICAL, ERROR_TITLE);
        return false;
    }

    text = "module MissionBrain_" + scriptName + " : integer;";
    line = outputFolder + scriptName + ".ABL";
    VbFile& script = files[SCRIPT_FILE];
    script.Open(line);
    script.Line("//*********************************************************************************");
    script.Line();
    script.Line(text);
    script.Line();
    script.Line("     //--------------------------------------------------------");
    script.Text("     // Mission Name: ").Value((*mission)["MissionName"]).EndLine();
    script.Text("     // Created: ").Value(Vb::Today()).EndLine();
    script.Line("     //--------------------------------------------------------");
    script.Line();
    script.Line("//------------------------------------------------------------------");
    script.Line("//");
    script.Line("//     Constant Definitions");
    script.Line("//");
    script.Line("//------------------------------------------------------------------");
    script.Line("     const");
    script.Line();
    script.Line("          #include_ " + quote + "misconst.abi" + quote);
    script.Line();

    if (multiplayer)
    {
        script.Line("          // Message Codes");
        script.Line("          MSG_GENERAL_ALARM_ON = 255;");
        script.Line("          MSG_GENERAL_ALARM_OFF = 254;");
        script.Line("          MSG_PERIMETER_ALARM_ON = 253;");
        script.Line("          MSG_PERIMETER_ALARM_OFF = 252;");
        script.Line("          MSG_SET_EXIT_TIMER = 251;");
        script.Line("          MSG_PLAY_END_MUSIC = 250;");
        multiplayerCaseLimit = MULTIPLAYER_CASE_LIMIT;
        script.Line();
        script.Line("          // Multiplayer Victory Codes");
        script.Line("          INNER_SPHERE_VICTORY = 1;");
        script.Line("          CLAN_VICTORY = 2;");
        script.Line("          JOINT_LOSS = 3;");
        script.Line("          JOINT_VICTORY = 4;");
    }

    script.Line();
    script.Line("//------------------------------------------------------------------");
    script.Line("//");
    script.Line("//     Type Definitions");
    script.Line("//");
    script.Line("//------------------------------------------------------------------");
    script.Line("     type");
    script.Line();
    script.Line();
    script.Line("//------------------------------------------------------------------");
    script.Line("//");
    script.Line("//     Variable Declarations");
    script.Line("//");
    script.Line("//------------------------------------------------------------------");
    script.Line("      var");
    script.Line();
    line = scriptName + "_VAR.ABI";
    script.Line("            #include_ " + quote + line + quote);
    script.Line();
    script.Line("            static integer      ScenarioResult;");
    script.Line("            eternal boolean     ExitTimerSet;");
    script.Line("            static integer      VictoryLevel;");

    if (multiplayer)
    {
        script.Line("            static integer      ClanVictoryLevel;");
    }

    script.Line("            char[40]            dstring;");
    script.Line("            eternal real        gametime;");
    script.Line("            static real         nextsecond;");
    script.Line("            integer             x;");
    script.Line("            integer             y;");
    script.Line("            Position            aPoint;");
    script.Line("            eternal boolean     PlayerForceDead;");
    script.Line("            eternal boolean     ClanForceDead;");
    script.Line("            eternal boolean     AlliedForceDead;");
    script.Line("            eternal boolean     GeneralAlarm;");
    script.Line("            eternal boolean     Flag1;");
    script.Line("            eternal boolean     Flag2;");
    script.Line("            eternal boolean     Flag3;");
    script.Line("            eternal boolean     Flag4;");
    script.Line("            eternal boolean     Flag5;");
    script.Line("            eternal boolean     Flag6;");
    script.Line("            eternal boolean     Flag7;");
    script.Line("            eternal boolean     Flag8;");
    script.Line("            eternal boolean     Flag9;");
    script.Line("            eternal boolean     Flag10;");
    script.Line("            static boolean      PlayPASound;");
    script.Line("            static boolean      PlayGASound;");
    script.Line("            eternal integer     GeneralAlarmCounter;");
    script.Line("            boolean             PerimeterBreach;");

    // The objectives' variables: each one's Decided flag and, for several actions, each action's Success flag;
    // count the primary objectives of each side and the Clan's objectives.
    objectives->MoveFirst();
    COleVariant objective = Vb::Integer(-1);
    clanPrimaries = 0;
    innerSpherePrimaries = Vb::Integer(0);
    COleVariant objectiveCount = Vb::Integer(0);
    short clanObjectives = 0;

    while (Vb::IsTrue(Vb::Not(Vb::Boolean(objectives->IsEOF()))))
    {
        objective = Vb::Add(objective, Vb::Integer(1));
        objectiveCount = Vb::Add(objectiveCount, Vb::Integer(1));

        if (Vb::Eq((*objectives)["Alignment"], Vb::Integer(CLAN_ALIGNMENT)))
        {
            ++clanObjectives;
        }

        if (Vb::Eq((*objectives)["Priority"], Vb::Integer(0)))
        {
            if (Vb::Eq((*objectives)["Alignment"], Vb::Integer(CLAN_ALIGNMENT)))
            {
                ++clanPrimaries;
            }
            else
            {
                innerSpherePrimaries = Vb::Add(innerSpherePrimaries, Vb::Integer(1));
            }
        }

        line = Vb::CStr(Vb::Cat(Vb::Cat(literal("Objective_"), objective), literal("_Decided")));
        addVariable("static boolean", line, "FALSE");
        const CString actionFilter = Vb::CStr(Vb::Cat(literal("[ObjectiveID] = "), (*objectives)["ObjectiveID"]));
        actions->FindFirst(actionFilter);
        short objectiveActions = 0;

        while (Vb::Eq(Vb::Boolean(actions->noMatch), Vb::Boolean(false)))
        {
            ++objectiveActions;
            actions->FindNext(actionFilter);
        }

        if (objectiveActions > 1)
        {
            for (COleVariant action = Vb::Integer(1); Vb::Le(action, Vb::Integer(objectiveActions));
                 action = Vb::Add(action, Vb::Integer(1)))
            {
                line =
                    Vb::CStr(Vb::Cat(Vb::Cat(Vb::Cat(Vb::Cat(literal("Obj_"), objective), literal("_Action_")), action),
                                     literal("_Success")));
                addVariable("static boolean", line, "FALSE");
            }
        }

        objectives->MoveNext();
    }

    // The vehicles created or activated by the game when a unit comes close: their flags.
    COleVariant createdVehicles = Vb::Integer(0);
    assignments = std::make_unique<FitAblRecordset>(database);
    assignments->OpenDynaset("MissionAssignments");
    assignments->FindFirst(missionFilter + " AND [Exists] = FALSE AND [CreateRadius] > 0");

    while (!assignments->noMatch)
    {
        createdVehicles = Vb::Add(createdVehicles, Vb::Integer(1));
        // Original behaviour (EB-100): the next ones are found without the [Exists] = FALSE condition.
        assignments->FindNext(missionFilter + " AND [CreateRadius] > 0");
    }

    if (Vb::Gt(createdVehicles, Vb::Integer(0)))
    {
        line = Vb::CStr(
            Vb::Cat(Vb::Cat(literal("            static boolean["), createdVehicles), literal("]   VehicleCreated;")));
        script.Line(line);
    }

    short activatedVehicles = 0;
    assignments->FindFirst(missionFilter + " AND [Active] = FALSE AND [ActivationRadius] > 0");

    while (!assignments->noMatch)
    {
        ++activatedVehicles;
        assignments->FindNext(missionFilter + " AND [ActivationRadius] > 0");
    }

    if (activatedVehicles > 0)
    {
        line = "            static boolean[" + Vb::CStr(activatedVehicles) + "]   VehicleActivated;";
        script.Line(line);
    }

    assignments->Close();
    script.Line();
    script.Line("//------------------------------------------------------------------");
    script.Line("//");
    script.Line("//     Init Function    (automatically run first time through)");
    script.Line("//");
    script.Line("//------------------------------------------------------------------");
    script.Line("function init;");
    script.Line();
    script.Line("       code");
    script.Line();
    script.Line("            ScenarioResult = PLAYING;");
    script.Line("            PlayerForceDead = FALSE;");
    script.Line("            ClanForceDead = FALSE;");
    script.Line("            AlliedForceDead = FALSE;");
    script.Line("            ExitTimerSet = FALSE;");
    script.Line("            VictoryLevel = 0; // New Scheme");

    if (multiplayer)
    {
        script.Line("            ClanVictoryLevel = 0; // New Scheme");
    }

    script.Line("            NextSecond = 1.0;");
    script.Line("            GeneralAlarmCounter = -1;");
    script.Line("            GeneralAlarm = FALSE;");
    script.Line("            Flag1 = FALSE;");
    script.Line("            Flag2 = FALSE;");
    script.Line("            Flag3 = FALSE;");
    script.Line("            Flag4 = FALSE;");
    script.Line("            Flag5 = FALSE;");
    script.Line("            Flag6 = FALSE;");
    script.Line("            Flag7 = FALSE;");
    script.Line("            Flag8 = FALSE;");
    script.Line("            Flag9 = FALSE;");
    script.Line("            Flag10 = FALSE;");
    script.Line("            PlayPASound = FALSE;");
    script.Line("            PlayGASound = FALSE;");

    if (Vb::Gt(createdVehicles, Vb::Integer(0)))
    {
        line = Vb::CStr(Vb::Cat(Vb::Cat(literal("            for x = 0 to "), Vb::Sub(createdVehicles, Vb::Integer(1))),
                                literal(" do")));
        script.Line(line);
        script.Line("                  VehicleCreated[x] = FALSE;");
        script.Line("            endfor;");
    }

    if (activatedVehicles > 0)
    {
        line = "            for x = 0 to " + Vb::CStr(static_cast<short>(activatedVehicles - 1)) + " do";
        script.Line(line);
        script.Line("                  VehicleActivated[x] = FALSE;");
        script.Line("            endfor;");
    }

    script.Line();
    line = scriptName + "_INIT.ABI";
    script.Line("            #include_ " + quote + line + quote);
    script.Line();
    script.Line("endfunction;");
    script.Line();

    if (multiplayer)
    {
        line = scriptName + "_MP.ABI";
        script.Line("//****************************");
        script.Line("// Multiplayer Event Handling");
        script.Line("//****************************");
        script.Line("#include_ " + quote + line + quote);
        script.Line();
    }

    script.Line("//------------------------------------------------------------------");
    script.Line("//");
    script.Line("//     Main Code");
    script.Line("//");
    script.Line("//------------------------------------------------------------------");
    script.Line("      code");
    script.Line();
    script.Line("            //------------------------------------------------");
    script.Line("            // Debug Window Game Clock Second Counter");
    script.Line("            // Note:  This is used by some included functions.");
    script.Line("            //------------------------------------------------");
    script.Line("            gametime = gettime;");
    script.Line("            If (gametime >= nextsecond) Then");
    script.Line("                  nextsecond = gametime + 1;");
    script.Line("                  If (GeneralAlarm) then");
    script.Line("                       GeneralAlarmCounter = GeneralAlarmCounter + 1;");
    script.Line("                  endif;");
    script.Line("                  // dstring = " + quote + "Gametime: " + quote + ";");
    script.Line("                  // concat(dstring,gametime);");
    script.Line("                  // Print (dstring);");
    script.Line("            endif;");
    script.Line("            if ((PlayGASound) and (NextSecond == gametime + 1)) then");
    script.Line("                  playSoundEffect(GENERAL_ALARM_SOUND);");
    script.Line("            endif;");
    script.Line("            if (PlayPASound) then");
    script.Line("                  playSoundEffect(PERIMETER_ALARM_SOUND);");
    script.Line("            endif;");
    script.Line("            PerimeterBreach = FALSE;");
    script.Line();

    if (multiplayer)
    {
        script.Line("         //*********************");
        script.Line("         // SERVER CODE");
        script.Line("         //*********************");
        script.Line("         if (isServer) then");
        script.Line();
    }

    script.Line("            //---------------------------------------");
    script.Line("            // Structure");
    script.Line("            //---------------------------------------");
    line = scriptName + "_STR.ABI";
    script.Line("            #include_ " + quote + line + quote);
    script.Line();
    script.Line("            //---------------------------------------");
    script.Line("            // Lookout Points");
    script.Line("            //---------------------------------------");
    line = scriptName + "_LOP.ABI";
    script.Line("            #include_ " + quote + line + quote);
    script.Line();
    script.Line("            //---------------------------------------");
    script.Line("            // Force Dead Checks");
    script.Line("            //---------------------------------------");
    script.Line("            if (isDeadorFled(PLAYER_FORCE)) then");
    script.Line("                  PlayerForceDead = TRUE;");
    script.Line("            endif;");
    script.Line("            if (isDeadorFled(CLAN_FORCE)) then");
    script.Line("                  ClanForceDead = TRUE;");
    script.Line("            endif;");
    script.Line("            if (isDeadorFled(ALLIED_FORCE)) then");
    script.Line("                  AlliedForceDead = TRUE;");
    script.Line("            endif;");
    script.Line();

    if (Vb::Eq((*mission)["Multiplayer"], Vb::Boolean(false)))
    {
        // A dead player force fails the objectives that may fail so.
        script.Line("            //---------------------------------------");
        script.Line("            // SET EXIT TIMER IF PLAYER DEAD/DISABLED");
        script.Line("            //---------------------------------------");
        script.Line();
        script.Line("            if ((NOT ExitTimerSet) AND (PlayerForceDead)) then");
        script.Line("                  // Fail any Undecided Player Objectives");
        objective = Vb::Integer(0);
        objectives->FindFirst(missionFilter);

        while (Vb::IsTrue(Vb::Not(Vb::Boolean(objectives->noMatch))))
        {
            if (!Vb::IsNull((*objectives)["DoNotFailIfDead"]) &&
                Vb::Eq((*objectives)["DoNotFailIfDead"], Vb::Boolean(false)))
            {
                line = Vb::CStr(Vb::Cat(Vb::Cat(literal("                  if (checkObjectiveStatus("), objective),
                                        literal(") == INCOMPLETE) then")));
                script.Line(line);
                line = Vb::CStr(Vb::Cat(Vb::Cat(literal("                        setObjectiveStatus("), objective),
                                        literal(", FAILED);")));
                script.Line(line);
                script.Line("                  endif;");
            }

            objectives->FindNext(missionFilter);
            objective = Vb::Add(objective, Vb::Integer(1));
        }
    }
    else
    {
        // Each side's dead force fails its objectives, through handleMessage.
        script.Line("            //---------------------------------------");
        script.Line("            // FAIL OBJECTIVES IF TEAM DIES");
        script.Line("            //---------------------------------------");
        script.Line();
        script.Line("            if (PlayerForceDead) then");
        objective = Vb::Integer(0);
        objectives->FindFirst("[Alignment] <> 1");

        while (Vb::IsTrue(Vb::Not(Vb::Boolean(objectives->noMatch))))
        {
            if (!Vb::IsNull((*objectives)["DoNotFailIfDead"]) &&
                Vb::Eq((*objectives)["DoNotFailIfDead"], Vb::Boolean(false)))
            {
                line = Vb::CStr(Vb::Cat(Vb::Cat(literal("                  if (checkObjectiveStatus("), objective),
                                        literal(") == INCOMPLETE) then")));
                script.Line(line);
                line = Vb::CStr(Vb::Cat(Vb::Cat(literal("setObjectiveStatus("), objective), literal(", FAILED);")));
                const short messageCode = addMultiplayerEvent(line, true);
                line = "                        sendMessage(" + Vb::CStr(messageCode) + ",0);";
                script.Line(line);
                script.Line("                        handleMessage;");
                script.Line("                  endif;");
            }

            objectives->FindNext("[Alignment] <> 1");
            objective = Vb::Add(objective, Vb::Integer(1));
        }

        script.Line("            endif;");
        script.Line();
        script.Line("            if (ClanForceDead) then");
        objective = Vb::Sub(objectiveCount, Vb::Integer(clanObjectives));
        objectives->FindFirst("[Alignment] = 1");

        while (Vb::IsTrue(Vb::Not(Vb::Boolean(objectives->noMatch))))
        {
            if (!Vb::IsNull((*objectives)["DoNotFailIfDead"]) &&
                Vb::Eq((*objectives)["DoNotFailIfDead"], Vb::Boolean(false)))
            {
                line = Vb::CStr(Vb::Cat(Vb::Cat(literal("                  if (checkObjectiveStatus("), objective),
                                        literal(") == INCOMPLETE) then")));
                script.Line(line);
                line = Vb::CStr(Vb::Cat(Vb::Cat(literal("setObjectiveStatus("), objective), literal(", FAILED);")));
                const short messageCode = addMultiplayerEvent(line, true);
                line = "                        sendMessage(" + Vb::CStr(messageCode) + ",0);";
                script.Line(line);
                script.Line("                        handleMessage;");
                script.Line("                  endif;");
            }

            objectives->FindNext("[Alignment] = 1");
            objective = Vb::Add(objective, Vb::Integer(1));
        }
    }

    script.Line("            endif;");
    script.Line();
    script.Line("            //---------------------------------------");
    script.Line("            // Objective Resolution");
    script.Line("            //---------------------------------------");
    script.Line();
    objectives->MoveFirst();
    objective = Vb::Integer(-1);

    while (Vb::IsTrue(Vb::Not(Vb::Boolean(objectives->IsEOF()))))
    {
        objective = Vb::Add(objective, Vb::Integer(1));
        bool delayed = false;

        if (!Vb::Eq((*objectives)["DelayDetermination"], Vb::Boolean(false)))
        {
            if (Vb::Gt(objective, Vb::Integer(0)))
            {
                // A delayed objective is decided only after the ones before it.
                line = Vb::CStr(Vb::Cat(Vb::Cat(literal("            if ((checkObjectiveStatus("), objective),
                                        literal(") == INCOMPLETE)")));
                const COleVariant last = Vb::Sub(objective, Vb::Integer(1));

                for (COleVariant earlier = Vb::Integer(0); Vb::Le(earlier, last);
                     earlier = Vb::Add(earlier, Vb::Integer(1)))
                {
                    line = Vb::CStr(
                        Vb::Cat(Vb::Cat(Vb::String(line + " AND (Objective_"), earlier), literal("_Decided)")));
                }

                line += ") then";
                script.Line(line);
                delayed = true;
            }
            else
            {
                Vb::MsgBox("The first objective cannot have its determination delayed.");
            }
        }

        if (!delayed)
        {
            line = Vb::CStr(Vb::Cat(Vb::Cat(literal("            if (checkObjectiveStatus("), objective),
                                    literal(") == INCOMPLETE) then")));
            script.Line(line);
        }

        script.Line();

        if (!writeObjectiveActions(Vb::CLng((*objectives)["ObjectiveID"]), Vb::CInt(objective),
                                   Vb::CInt((*objectives)["Alignment"])))
        {
            // No actions: a place for the objective's hand-written checks.
            line = Vb::CStr(Vb::Cat(Vb::Cat(literal("                   // Enter Objective "), objective),
                                    literal(" Success/Fail Checks Here")));
            script.Line(line);
            script.Line();
            script.Line("                   // Uncomment these lines and place them where Objective is set Failed");
            line = Vb::CStr(Vb::Cat(Vb::Cat(literal("//                        SetObjectiveStatus("), objective),
                                    literal(",FAILED);")));
            script.Line(line);
            line = "//                        playBetty(BETTY_CANNOT_COMP_OBJ);";
            script.Line(line);
            line = Vb::CStr(Vb::Cat(Vb::Cat(literal("//                        Objective_"), objective),
                                    literal("_Decided = TRUE;")));
            script.Line(line);
            script.Line("//                        PlayDigitalMusic(OBJECTIVE_FAIL_MUSIC);");
            script.Line();
            script.Line("                   // Uncomment these lines and place them where Objective is set Success");
            line = Vb::CStr(Vb::Cat(Vb::Cat(literal("//                        SetObjectiveStatus("), objective),
                                    literal(",SUCCESS);")));
            script.Line(line);
            line = "//                        playBetty(BETTY_OBJECTIVE_COMPLETE);";
            script.Line(line);

            if (Vb::Eq((*objectives)["Priority"], Vb::Integer(0)))
            {
                line = "//                        VictoryLevel = VictoryLevel + 1;";
                script.Line(line);
            }

            line = Vb::CStr(Vb::Cat(Vb::Cat(literal("//                        Objective_"), objective),
                                    literal("_Decided = TRUE;")));
            script.Line(line);
            script.Line("//                        PlayDigitalMusic(OBJECTIVE_SUCCESS_MUSIC);");
        }

        if (Vb::Eq((*objectives)["Priority"], Vb::Integer(0)))
        {
            // A failed primary objective ends the mission.
            script.Line("            else");
            script.Line("                  if (NOT ExitTimerSet) then        ");
            script.Line("                        //if Failed primary Objective, fail mission");
            line = Vb::CStr(Vb::Cat(Vb::Cat(literal("                        if (checkObjectiveStatus("), objective),
                                    literal(") == FAILED) then")));
            script.Line(line);

            if (multiplayer)
            {
                if (Vb::Eq((*objectives)["Alignment"], Vb::Integer(CLAN_ALIGNMENT)))
                {
                    line = Vb::CStr(Vb::Cat(
                        Vb::Cat(literal("                              if ((VictoryLevel >= "), innerSpherePrimaries),
                        literal(") OR (PlayerForceDead)) then")));
                }
                else
                {
                    line = "                              if ((ClanVictoryLevel >= " + Vb::CStr(clanPrimaries) +
                           ") OR (ClanForceDead)) then";
                }

                script.Line(line);
                script.Line("                                    sendMessage(MSG_SET_EXIT_TIMER,0);");
                script.Line("                                    handleMessage;");
                script.Line("                              endif;");
            }
            else
            {
                script.Line("                              setTimer(EXIT_TIMER,2.0);");
                script.Line("                              ExitTimerSet = TRUE;");
            }

            script.Line("                        endif;");
            script.Line("                  endif;");
            script.Line("            endif;");
            script.Line();
        }
        else
        {
            script.Line("            endif;");
            script.Line();
        }

        objectives->MoveNext();
    }

    // The mission ends when every objective (each side's, in multiplayer) is decided.
    if (multiplayer)
    {
        const COleVariant firstClan = Vb::Sub(objectiveCount, Vb::Integer(clanObjectives));
        const COleVariant lastInnerSphere = Vb::Sub(firstClan, Vb::Integer(1));
        const COleVariant lastObjective = Vb::Sub(objectiveCount, Vb::Integer(1));
        line = "            if ((NOT ExitTimerSet) AND ";

        for (objective = Vb::Integer(0); Vb::Le(objective, lastInnerSphere);
             objective = Vb::Add(objective, Vb::Integer(1)))
        {
            line = Vb::CStr(Vb::Cat(Vb::Cat(Vb::String(line + "(Objective_"), objective), literal("_Decided)")));

            if (Vb::Lt(objective, lastInnerSphere))
            {
                line += " AND ";
            }
        }

        line += ") then";
        script.Line(line);

        for (objective = firstClan; Vb::Le(objective, lastObjective); objective = Vb::Add(objective, Vb::Integer(1)))
        {
            line = Vb::CStr(Vb::Cat(Vb::Cat(literal("                  if (checkObjectiveStatus("), objective),
                                    literal(") == INCOMPLETE) then")));
            script.Line(line);
            line = Vb::CStr(Vb::Cat(Vb::Cat(literal("setObjectiveStatus("), objective), literal(", FAILED);")));
            const short messageCode = addMultiplayerEvent(line, true);
            line = "                        sendMessage(" + Vb::CStr(messageCode) + ",0);";
            script.Line(line);
            script.Line("                        handleMessage;");
            script.Line("                  endif;");
        }

        script.Line("                  sendMessage(MSG_SET_EXIT_TIMER,0);");
        script.Line("                  handleMessage;");
        script.Line("            endif;");
        script.Line();
        line = "            if ((NOT ExitTimerSet) AND ";

        for (objective = firstClan; Vb::Le(objective, lastObjective); objective = Vb::Add(objective, Vb::Integer(1)))
        {
            line = Vb::CStr(Vb::Cat(Vb::Cat(Vb::String(line + "(Objective_"), objective), literal("_Decided)")));

            if (Vb::Lt(objective, lastObjective))
            {
                line += " AND ";
            }
        }

        line += ") then";
        script.Line(line);

        for (objective = Vb::Integer(0); Vb::Le(objective, lastInnerSphere);
             objective = Vb::Add(objective, Vb::Integer(1)))
        {
            line = Vb::CStr(Vb::Cat(Vb::Cat(literal("                  if (checkObjectiveStatus("), objective),
                                    literal(") == INCOMPLETE) then")));
            script.Line(line);
            line = Vb::CStr(Vb::Cat(Vb::Cat(literal("setObjectiveStatus("), objective), literal(", FAILED);")));
            const short messageCode = addMultiplayerEvent(line, true);
            line = "                        sendMessage(" + Vb::CStr(messageCode) + ",0);";
            script.Line(line);
            script.Line("                        handleMessage;");
            script.Line("                  endif;");
        }

        script.Line("                  sendMessage(MSG_SET_EXIT_TIMER,0);");
        script.Line("                  handleMessage;");
        script.Line("            endif;");
    }
    else
    {
        const COleVariant lastObjective = Vb::Sub(objectiveCount, Vb::Integer(1));
        line = "            if ((NOT ExitTimerSet) AND ";

        for (objective = Vb::Integer(0); Vb::Le(objective, lastObjective);
             objective = Vb::Add(objective, Vb::Integer(1)))
        {
            line = Vb::CStr(Vb::Cat(Vb::Cat(Vb::String(line + "(Objective_"), objective), literal("_Decided)")));

            if (Vb::Lt(objective, lastObjective))
            {
                line += " AND ";
            }
        }

        line += ") then";
        script.Line(line);
        script.Line("                  setTimer(EXIT_TIMER,2.0);");
        script.Line("                  ExitTimerSet = TRUE;");
        script.Line("            endif;");
    }

    script.Line();
    script.Line("            //---------------------------------------");
    script.Line("            // END SCENARIO");
    script.Line("            //---------------------------------------");
    script.Line();
    script.Line("            if ((ExitTimerSet) AND (CheckTimer(EXIT_TIMER) == 0.0)) then");

    if (multiplayer)
    {
        line = Vb::CStr(
            Vb::Cat(Vb::Cat(Vb::Cat(Vb::Cat(literal("                  if ((VictoryLevel >= "), innerSpherePrimaries),
                                    literal(") AND (ClanVictoryLevel < ")),
                            Vb::Integer(clanPrimaries)),
                    literal(")) then")));
        script.Line(line);
        script.Line("                       ScenarioResult = INNER_SPHERE_VICTORY;");
        script.Line("                  else");
        line = Vb::CStr(Vb::Cat(
            Vb::Cat(Vb::Cat(Vb::Cat(literal("                       if ((ClanVictoryLevel >= "), innerSpherePrimaries),
                            literal(") AND (VictoryLevel < ")),
                    Vb::Integer(clanPrimaries)),
            literal(")) then")));
        script.Line(line);
        script.Line("                            ScenarioResult = CLAN_VICTORY;");
        script.Line("                       else");
        line =
            Vb::CStr(Vb::Cat(Vb::Cat(Vb::Cat(Vb::Cat(literal("                            if ((ClanVictoryLevel >= "),
                                                     innerSpherePrimaries),
                                             literal(") AND (VictoryLevel >= ")),
                                     Vb::Integer(clanPrimaries)),
                             literal(")) then")));
        script.Line(line);
        script.Line("                                 ScenarioResult = JOINT_VICTORY;");
        script.Line("                            else");
        script.Line("                                 ScenarioResult = JOINT_LOSS;");
        script.Line("                            endif;");
        script.Line("                       endif;");
        script.Line("                  endif;");
        script.Line();
        script.Line("                  sendMessage(MSG_PLAY_END_MUSIC,ScenarioResult);");
        script.Line("                  handleMessage;");
    }
    else
    {
        line = Vb::CStr(Vb::Cat(Vb::Cat(literal("                  if (VictoryLevel >= "), innerSpherePrimaries),
                                literal(") then")));
        script.Line(line);
        script.Line("                        ScenarioResult = PLAYER_WIN_BIG;");
        script.Line("                        PlayDigitalMusic(VICTORY_MUSIC);");
        script.Line("                  else");
        script.Line("                        ScenarioResult = PLAYER_LOST_BIG;");
        script.Line("                        PlayDigitalMusic(DEFEAT_MUSIC);");
        script.Line("                  endif;");
    }

    script.Line("            endif;");
    script.Line();

    if (multiplayer)
    {
        script.Line("         endif;");
        script.Line("         //*********************");
        script.Line("         //  End of Server Code");
        script.Line("         //*********************");
    }

    script.Line("            //---------------------------------------");
    script.Line("            // RETURN RESULT");
    script.Line("            //---------------------------------------");
    script.Line();
    script.Line("            return (ScenarioResult);");
    script.Line("endmodule.");
    script.Line("//******************************************************************");
    script.Close();
    WriteLookOutIncludeFile(missionNumber, outputFolder);
    return true;
}
