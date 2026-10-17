#include "stdafx.h"
#include "FitAbl.h"

// FitAbl's structures and lookout points (0x11028690-0x11038e50, 0x11059420-0x1105d83c): the _STR and _LOP include
// files, and the index of an assignment among the mission's parts.

namespace
{
    /// <summary>The number the include files are written on (the .ABL's, free again by then).</summary>
    inline constexpr int INCLUDE_FILE = 2;
    /// <summary>The include files' separator line.</summary>
    const char* const SEPARATOR = "//*********************************************************************************";
    /// <summary>The structures' Alignment values.</summary>
    inline constexpr short NEUTRAL_ALIGNMENT = 0;
    inline constexpr short CLAN_ALIGNMENT = 1;
    inline constexpr short INNER_SPHERE_ALIGNMENT = 2;
    /// <summary>The columns the structures' code is indented to.</summary>
    inline constexpr int COLUMN_1 = 6;
    inline constexpr int COLUMN_2 = 12;
    inline constexpr int COLUMN_3 = 18;
    inline constexpr int COLUMN_4 = 24;
    inline constexpr int COLUMN_5 = 30;
    /// <summary>The most entries the game's ObjectList takes.</summary>
    inline constexpr short OBJECT_LIST_LIMIT = 10;

    /// <summary>A text's variant.</summary>
    COleVariant literal(const char* value)
    {
        return Vb::String(value);
    }

    /// <summary>Whether <paramref name="value"/> = <paramref name="number"/>, as a Select Case tests it.</summary>
    bool is(const COleVariant& value, short number)
    {
        return Vb::Eq(value, Vb::Integer(number));
    }

    /// <summary>
    /// The objectChangeSides argument of the side <paramref name="alignment"/>, closing the call
    /// (null when it is none of the three).
    /// </summary>
    const char* sideArgument(const COleVariant& alignment)
    {
        if (is(alignment, NEUTRAL_ALIGNMENT))
        {
            return ",NEUTRAL)";
        }

        if (is(alignment, CLAN_ALIGNMENT))
        {
            return ",CLAN)";
        }

        if (is(alignment, INNER_SPHERE_ALIGNMENT))
        {
            return ",INNER_SPHERE)";
        }

        return nullptr;
    }
}

void FitAbl::writeStructures(short missionId)
{
    VbFile& file = files[INCLUDE_FILE];
    const CString missionFilter = "[MissionID] = " + Vb::CStr(missionId);
    short mechBays = 0;
    short vehicleBays = 0;
    short objectList = 0;
    short perimeterAlarms = 0;
    // The variable that holds a captureable structure's side (set only for those; others keep the last one's).
    CString side;
    // Whether the next multiplayer event starts a case (a loop's first).
    bool first = false;

    line = outputFolder + scriptName + "_STR.ABI";
    file.Open(line);
    file.Line(SEPARATOR);
    file.Text("// Structure Include file for mission ").Value((*mission)["MissionName"]).EndLine();
    file.Line();
    FitAblRecordset structures(database);
    structures.OpenDynaset("MissionStructures");
    FitAblRecordset salvage(database);
    salvage.OpenDynaset("StructureSalvage");
    FitAblRecordset associates(database);
    associates.OpenDynaset("StructureAssociates");

    // The captureable vehicles.
    assignments = std::make_unique<FitAblRecordset>(database);
    assignments->OpenDynaset("MissionAssignments");
    const CString captureableFilter = missionFilter + " AND [Captureable] = TRUE";
    assignments->FindFirst(captureableFilter);

    while (!assignments->noMatch)
    {
        const COleVariant alignment = (*assignments)["Alignment"];

        if (Vb::Eq(alignment, literal("Clan")))
        {
            line = "CLAN_FORCE";
        }
        else if (Vb::Eq(alignment, literal("Inner Sphere")))
        {
            line = "PLAYER_FORCE";
        }
        else if (Vb::Eq(alignment, literal("Other")))
        {
            line = "ALLIED_FORCE";
        }
        else
        {
            // Original behaviour (EB-107): the _STR file is left open, unfinished.
            Vb::MsgBox("Alignment not set correctly on captureable vehicle.");
            return;
        }

        const short index = assignmentIndex(Vb::CLng((*assignments)["UniqueEntry"]));
        line = Vb::CStr(Vb::Cat(Vb::Cat(Vb::Cat(Vb::Cat(Vb::String(line), literal(",")),
                                                Vb::Sub((*assignments)["ForceGroup"], Vb::Integer(1))),
                                        literal(",")),
                                Vb::Integer(index)));
        addInitialization("setCaptureable(getVehicleID(" + line + "),TRUE)", "");
        assignments->FindNext(captureableFilter);
    }

    // The game's id of the structure's object, and of the associate's.
    const auto objectText = [&](FitAblRecordset& rows)
    {
        const short block = Vb::CInt(rows["Block"]);
        const short vertex = Vb::CInt(rows["Vertex"]);
        const short number = Vb::CInt(rows["Number"]);
        return Vb::CStr(Vb::Long(objectId(block, vertex, number)));
    };

    const auto multiplayer = [&] { return Vb::Eq((*mission)["Multiplayer"], Vb::Boolean(true)); };
    const auto associateFilter = [&]
    { return Vb::CStr(Vb::Cat(literal("[StructureID] = "), structures["StructureID"])); };
    // The associate's side changes with the structure's (an initialization).
    const auto associateSides = [&]
    {
        if (const char* argument = sideArgument(structures["Alignment"]))
        {
            addInitialization("objectChangeSides(" + objectText(associates) + argument, "");
        }
    };

    // The structure captured: "if" another side took it.
    const auto capturedLine = [&]
    {
        line = "if ((IsCaptured(" + text + ") == 1) AND (objectSide(" + text + ") <> " + side + ") AND (" + side +
               " <> -2)) then";
    };

    // Writes line as an event of handleMessage, the script sending it (SendMessage/HandleMessage) once per case.
    const auto sendEvent = [&](int column, const char* sendWord, const char* handleWord, const CString& argument)
    {
        const short code = addMultiplayerEvent(line, first);

        if (first)
        {
            file.Tab(column).Line(CString(sendWord) + "(" + Vb::CStr(code) + "," + argument + ");");
            file.Tab(column).Line(handleWord);
            first = false;
        }
    };

    // A captured turret or power control: the structure and its side change too (0x1102ddc6).
    const auto writeCapturedEnd = [&]
    {
        if (multiplayer())
        {
            line = side + " = MsgParams;";
            const short code = addMultiplayerEvent(line, first);

            if (first)
            {
                file.Tab(COLUMN_2).Line("SendMessage(" + Vb::CStr(code) + ",ObjectSide(" + text + "));");
                file.Tab(COLUMN_2).Line("HandleMessage;");
                first = false;
            }

            line = "objectChangeSides(" + text + ",MsgParams);";
            addMultiplayerEvent(line, false);
        }
        else
        {
            line = "objectChangeSides(" + text + ",INNER_SPHERE);";
            file.Tab(COLUMN_2).Line(line);
            line = side + " = INNER_SPHERE;";
            file.Tab(COLUMN_2).Line(line);
            line = "setPotentialContact(" + text + ",2);";
            file.Tab(COLUMN_2).Line(line);
        }

        file.Tab(COLUMN_1).Line("endif;");
    };

    // A captured structure without associates: its side changes (0x11036f68).
    const auto writeCapturedSides = [&]
    {
        if (multiplayer())
        {
            line = "objectChangeSides(" + text + ",MsgParams);";
            addMultiplayerEvent(line, true);
            line = side + " = MsgParams;";
            const short code = addMultiplayerEvent(line, false);
            line = "sendMessage(" + Vb::CStr(code) + ",ObjectSide(" + text + "));";
            file.Tab(COLUMN_2).Line(line);
            line = "handleMessage;";
            file.Tab(COLUMN_2).Line(line);
        }
        else
        {
            line = side + " = INNER_SPHERE;";
            file.Tab(COLUMN_2).Line(line);
            line = "setPotentialContact(" + text + ",2);";
            file.Tab(COLUMN_2).Line(line);
        }
    };

    if (structures.GetRecordCount() != 0)
    {
        structures.FindFirst("[MissionID] =" + Vb::CStr(missionId));

        while (!structures.noMatch)
        {
            text = Vb::CStr(structures["Identifier"]);
            // Original behaviour (EB-109): the identifier is checked (a message when it starts with a number) but used as
            // it was; the made one is thrown away.
            CString identifier = text;
            MakeAblIdentifier(identifier);

            if (text.Find('[') >= 0)
            {
                addInitialization(text, objectText(structures));
                ++objectList;
            }
            else
            {
                addVariable("eternal integer", text, objectText(structures));
            }

            if (Vb::Ne(structures["InitialDamage"], Vb::Integer(0)))
            {
                addInitialization(
                    Vb::CStr(Vb::Cat(Vb::Cat(literal("setObjectDamage("), Vb::Cat(Vb::String(text), literal(","))),
                                     Vb::Cat(structures["InitialDamage"], literal(")")))),
                    "");
            }

            if (const char* argument = sideArgument(structures["Alignment"]))
            {
                addInitialization("objectChangeSides(" + text + argument, "");
            }

            addInitialization(Vb::CStr(Vb::Cat(Vb::Cat(Vb::Cat(literal("setPotentialContact("), Vb::String(text + ",")),
                                                       structures["Contact"]),
                                               literal(")"))),
                              "");

            // Its salvage.
            salvage.FindFirst(Vb::CStr(Vb::Cat(literal("[StructureID] ="), structures["StructureID"])));

            while (!salvage.noMatch)
            {
                addInitialization(
                    Vb::CStr(Vb::Cat(
                        Vb::Cat(Vb::Cat(Vb::Cat(literal("setSalvage("), Vb::String(text + ",")), salvage["ItemID"]),
                                literal(",")),
                        Vb::Cat(salvage["Quantity"], literal(")")))),
                    "");
                // Original behaviour (EB-106): "+" adds the text to the number, which raises Type mismatch.
                salvage.FindNext(
                    Vb::CStr(Vb::Cat(Vb::Cat(literal("[StructureID] = "),
                                             Vb::Add(structures["StructureID"], literal(" AND [MissionID] ="))),
                                     Vb::Integer(missionId))));
            }

            if (Vb::IsTrue(structures["Captureable"]))
            {
                addInitialization("setCaptureable(" + text + ", TRUE)", "");

                if (text.Find('[') >= 0)
                {
                    side = "Object" + Vb::CStr(static_cast<short>(objectList - 1)) + "Alignment";
                }
                else
                {
                    side = text + "Alignment";
                }

                const COleVariant alignment = structures["Alignment"];

                if (is(alignment, CLAN_ALIGNMENT))
                {
                    addVariable("static integer", side, "CLAN");
                }
                else if (is(alignment, INNER_SPHERE_ALIGNMENT))
                {
                    addVariable("static integer", side, "INNER_SPHERE");
                }
                else if (is(alignment, NEUTRAL_ALIGNMENT))
                {
                    addVariable("static integer", side, "NEUTRAL");
                }

                // A structure of no kind is captured through a message in a multiplayer game.
                if (Vb::IsNull(structures["BuildingType"]) && multiplayer())
                {
                    line = "if (isAlive(" + text + ")) then";
                    file.Line(line);
                    capturedLine();
                    file.Tab(COLUMN_1).Line(line);
                    line = "objectChangeSides(" + text + ",MsgParams);";
                    addMultiplayerEvent(line, true);
                    line = side + " = MsgParams;";
                    const short code = addMultiplayerEvent(line, false);
                    file.Tab(COLUMN_2).Line("SendMessage(" + Vb::CStr(code) + ",objectSide(" + text + "));");
                    file.Tab(COLUMN_2).Line("HandleMessage;");
                    file.Tab(COLUMN_1).Line("endif;");
                    file.Line("endif;");
                }
            }

            if (Vb::IsTrue(structures["Captured"]))
            {
                addInitialization("setCaptured(" + text + ")", "");
            }

            if (!Vb::IsNull(structures["newtagname"]))
            {
                addInitialization(Vb::CStr(Vb::Cat(Vb::Cat(literal("setBuildingName("), Vb::String(text + ",")),
                                                   Vb::Cat(structures["newtagname"], literal(")")))),
                                  "");
            }

            const COleVariant kind = structures["BuildingType"];

            if (Vb::Eq(kind, literal("Perimeter Alarm")))
            {
                file.Line();
                ++perimeterAlarms;
                addVariable("static real", text + "TripTime", "-1.0");
                line = "if (isAlive(" + text + ")) then";
                file.Line(line);
                line = "     getObjectPosition(" + text + ",aPoint);";
                file.Line(line);
                const COleVariant alignment = structures["Alignment"];
                CString hotSpot;

                if (is(alignment, CLAN_ALIGNMENT))
                {
                    hotSpot = "if (inHotSpot(PLAYER_FORCE,aPoint[0],aPoint[1],100)) then";
                }
                else if (is(alignment, INNER_SPHERE_ALIGNMENT))
                {
                    hotSpot = "if (inHotSpot(CLAN_FORCE,aPoint[0],aPoint[1],100)) then";
                }
                else
                {
                    line = "// neutral perimeter alarm do nothing";
                    file.Line(line);
                    hotSpot = "if (1 == 0 ) then";
                }

                line = hotSpot;
                file.Line(line);
                file.Line("          PerimeterBreach = TRUE;");
                file.Line("          SetRevealed(1,10.0,aPoint);");
                line = "          if (" + text + "TripTime == -1.0) then";
                file.Line(line);
                line = "               " + text + "TripTime = gametime;";
                file.Line(line);
                file.Line("               if (NOT PlayPASound) then");

                if (multiplayer())
                {
                    file.Line("                     SendMessage(MSG_PERIMETER_ALARM_ON,0);");
                    file.Line("                     handleMessage;");
                }
                else
                {
                    file.Line("                     PlayPASound = TRUE;");
                }

                file.Line("               endif;");
                file.Line("          else");
                line = "               if (gametime - " + text + "TripTime >= 12.0) then";
                file.Line(line);
                file.Line("                    SetRevealed(2,100.0,aPoint);");
                file.Line("                    if (NOT GeneralAlarm) then");

                if (multiplayer())
                {
                    file.Line("                          SendMessage(MSG_GENERAL_ALARM_ON,0);");
                    file.Line("                          handleMessage;");
                }
                else
                {
                    file.Line("                          GeneralAlarm = TRUE;");
                }

                file.Line("                    endif;");
                file.Line("                    if (NOT PlayGASound) then");

                // Original behaviour (EB-109): a multiplayer game turns the general alarm on again here.
                if (multiplayer())
                {
                    file.Line("                          SendMessage(MSG_GENERAL_ALARM_ON,0);");
                    file.Line("                          handleMessage;");
                }
                else
                {
                    file.Line("                          PlayGASound = TRUE;");
                    file.Line("                          PlayPASound = FALSE;");
                }

                file.Line("                    endif;");
                file.Line("               else");
                file.Line("                    if (NOT PlayPASound) then");

                if (multiplayer())
                {
                    file.Line("                          SendMessage(MSG_PERIMETER_ALARM_ON,0);");
                    file.Line("                          handleMessage;");
                }
                else
                {
                    file.Line("                          PlayPASound = TRUE;");
                }

                file.Line("                    endif;");
                file.Line("               endif;");
                file.Line("          endif;");
                file.Line("     else");
                line =
                    "          if ((" + text + "TripTime <> -1.0) AND (gametime - " + text + "TripTime < 12.0)) then";
                file.Line(line);
                line = "               " + text + "TripTime = -1.0;";
                file.Line(line);
                file.Line("          endif;");
                file.Line("     endif;");
                file.Line("endif;");
            }
            else if (Vb::Eq(kind, literal("Prison")))
            {
                // Original behaviour (EB-105): the prisoners' recordset was never opened, so a prison ends the call with
                // "Object required".
                throw VbError(Vb::VB_ERROR_OBJECT_REQUIRED, "Object required");
            }
            else if (Vb::Eq(kind, literal("Turret Control")))
            {
                file.Line();
                line = "if ((isdead(" + text + ")) AND (" + side + " <> -2)) then";
                file.Line(line);
                first = true;
                associates.FindFirst(associateFilter());

                while (!associates.noMatch)
                {
                    associateSides();
                    line = "setObjectActive(" + objectText(associates) + ",FALSE);";

                    if (multiplayer())
                    {
                        sendEvent(COLUMN_1, "SendMessage", "HandleMessage;", "0");
                    }
                    else
                    {
                        file.Tab(COLUMN_1).Line(line);
                    }

                    associates.FindNext(associateFilter());
                }

                line = side + " = -2;";

                if (multiplayer())
                {
                    sendEvent(COLUMN_1, "SendMessage", "HandleMessage;", "0");
                }
                else
                {
                    file.Tab(COLUMN_1).Line(line);
                }

                if (Vb::IsTrue(structures["Captureable"]))
                {
                    file.Line("else");
                    capturedLine();
                    file.Tab(COLUMN_1).Line(line);
                    first = true;
                    associates.FindFirst(associateFilter());
                    const CString objectSide = "ObjectSide(" + text + ")";

                    while (!associates.noMatch)
                    {
                        if (Vb::Eq(associates["Inactive"], Vb::Boolean(true)))
                        {
                            addInitialization("setObjectActive(" + objectText(associates) + ",FALSE)", "");
                            line = "setObjectActive(" + objectText(associates) + ",TRUE);";

                            if (multiplayer())
                            {
                                sendEvent(COLUMN_2, "SendMessage", "HandleMessage;", objectSide);
                            }
                            else
                            {
                                file.Tab(COLUMN_2).Line(line);
                            }
                        }

                        line = "objectChangeSides(" + objectText(associates) +
                               (multiplayer() ? ",MsgParams);" : ",INNER_SPHERE);");

                        if (multiplayer())
                        {
                            sendEvent(COLUMN_2, "SendMessage", "HandleMessage;", objectSide);
                        }
                        else
                        {
                            file.Tab(COLUMN_2).Line(line);
                        }

                        line = "setPotentialContact(" + objectText(associates) + ",2);";

                        if (multiplayer())
                        {
                            sendEvent(COLUMN_2, "SendMessage", "HandleMessage;", objectSide);
                        }
                        else
                        {
                            file.Tab(COLUMN_2).Line(line);
                        }

                        associates.FindNext(associateFilter());
                    }

                    writeCapturedEnd();
                }

                file.Line("endif;");
            }
            else if (Vb::Eq(kind, literal("Sensor Control")))
            {
                file.Line();
                line = "if ((isdead(" + text + ")) AND (" + side + " <> -2)) then";
                file.Line(line);
                first = true;
                associates.FindFirst(associateFilter());

                while (!associates.noMatch)
                {
                    associateSides();

                    if (Vb::Eq(associates["Inactive"], Vb::Boolean(true)))
                    {
                        addInitialization("setObjectActive(" + objectText(associates) + ",FALSE)", "");
                    }

                    line = "setObjectActive(" + objectText(associates) + ",FALSE);";

                    if (multiplayer())
                    {
                        const short code = addMultiplayerEvent(line, first);

                        if (first)
                        {
                            line = "sendMessage(" + Vb::CStr(code) + ",0);";
                            file.Tab(COLUMN_1).Line(line);
                            file.Tab(COLUMN_1).Line("handleMessage;");
                            first = false;
                        }
                    }
                    else
                    {
                        file.Tab(COLUMN_1).Line(line);
                    }

                    associates.FindNext(associateFilter());
                }

                line = side + " = -2;";

                if (multiplayer())
                {
                    const short code = addMultiplayerEvent(line, first);

                    if (first)
                    {
                        line = "sendMessage(" + Vb::CStr(code) + ",0);";
                        file.Tab(COLUMN_1).Line(line);
                        file.Tab(COLUMN_1).Line("handleMessage;");
                        first = false;
                    }
                }
                else
                {
                    file.Tab(COLUMN_1).Line(line);
                }

                if (Vb::IsTrue(structures["Captureable"]))
                {
                    file.Line("else");

                    if (multiplayer())
                    {
                        capturedLine();
                        file.Tab(COLUMN_1).Line(line);
                        first = true;
                        const auto send = [&]
                        {
                            const short code = addMultiplayerEvent(line, first);

                            if (first)
                            {
                                line = "sendMessage(" + Vb::CStr(code) + ",ObjectSide(" + text + "));";
                                file.Tab(COLUMN_2).Line(line);
                                file.Tab(COLUMN_2).Line("handleMessage;");
                                first = false;
                            }
                        };

                        associates.FindFirst(associateFilter());

                        while (!associates.noMatch)
                        {
                            if (Vb::Eq(associates["Inactive"], Vb::Boolean(true)))
                            {
                                line = "setObjectActive(" + objectText(associates) + ",TRUE);";
                                send();
                            }

                            line = "objectChangeSides(" + objectText(associates) + ",MsgParams);";
                            send();
                            associates.FindNext(associateFilter());
                        }

                        line = "objectChangeSides(" + text + ",MsgParams);";
                        send();
                        line = side + " = MsgParams;";
                        addMultiplayerEvent(line, false);
                        file.Tab(COLUMN_1).Line("endif;");
                    }
                    else
                    {
                        const COleVariant alignment = structures["Alignment"];
                        const bool toInnerSphere = Vb::Ge(alignment, Vb::Integer(NEUTRAL_ALIGNMENT)) &&
                                                   Vb::Le(alignment, Vb::Integer(CLAN_ALIGNMENT));

                        if (toInnerSphere || is(alignment, INNER_SPHERE_ALIGNMENT))
                        {
                            const char* newSide = toInnerSphere ? "INNER_SPHERE" : "CLAN";
                            line = "if ((IsCaptured(" + text + ") == 1) AND (objectSide(" + text + ") <> " + side +
                                   ")) then";
                            file.Tab(COLUMN_1).Line(line);
                            associates.FindFirst(associateFilter());

                            while (!associates.noMatch)
                            {
                                if (Vb::Eq(associates["Inactive"], Vb::Boolean(true)))
                                {
                                    line = "setObjectActive(" + objectText(associates) + ",TRUE);";
                                    file.Tab(COLUMN_2).Line(line);
                                }

                                line = "objectChangeSides(" + objectText(associates) + "," + newSide + ");";
                                file.Tab(COLUMN_2).Line(line);

                                if (toInnerSphere)
                                {
                                    line = "setPotentialContact(" + objectText(associates) + ",2);";
                                    file.Tab(COLUMN_2).Line(line);
                                }

                                associates.FindNext(associateFilter());
                            }

                            line = "objectChangeSides(" + text + "," + newSide + ");";
                            file.Tab(COLUMN_2).Line(line);
                            line = side + " = " + newSide + ";";
                            file.Tab(COLUMN_2).Line(line);
                            file.Tab(COLUMN_1).Line("endif;");
                        }
                    }
                }

                file.Line("endif;");
            }
            else if (Vb::Eq(kind, literal("Barracks")))
            {
                file.Line();
                line = Vb::CStr(
                    Vb::Cat(Vb::Cat(Vb::String("if (getObjectDamage(" + text + ") > "), structures["InitialDamage"]),
                            literal(") then")));
                file.Line(line);
                file.Tab(COLUMN_1).Line("GeneralAlarm = TRUE;");
                file.Line("endif;");
            }
            else if (Vb::Eq(kind, literal("Gate Control")))
            {
                file.Line();
                line = "if ((isdead(" + text + ")) AND (" + side + " <> -2)) then";
                file.Line(line);
                associates.FindFirst(associateFilter());

                while (!associates.noMatch)
                {
                    associateSides();

                    if (Vb::Eq(associates["Inactive"], Vb::Boolean(true)))
                    {
                        addInitialization("LockGateClosed(" + objectText(associates) + ")", "");
                    }

                    line = "if (isGateOpen(" + objectText(associates) + ")) then";
                    file.Tab(COLUMN_1).Line(line);

                    // The gate locked as it is, open or closed.
                    for (const char* lock : {"LockGateOpen(", "LockGateClosed("})
                    {
                        line = lock + objectText(associates) + ");";
                        const bool open = std::strcmp(lock, "LockGateOpen(") == 0;

                        if (multiplayer())
                        {
                            addMultiplayerEvent(line, true);
                            line = side + " = -2;";
                            const short code = addMultiplayerEvent(line, false);
                            line = "sendMessage(" + Vb::CStr(code) + ",0);";
                            file.Tab(COLUMN_2).Line(line);
                            file.Tab(COLUMN_2).Line("handleMessage;");
                        }
                        else
                        {
                            file.Tab(COLUMN_2).Line(line);

                            if (!open)
                            {
                                line = "objectChangeSides(" + objectText(associates) + ",INNER_SPHERE);";
                                file.Tab(COLUMN_2).Line(line);
                                line = "setPotentialContact(" + objectText(associates) + ",2);";
                                file.Tab(COLUMN_2).Line(line);
                            }

                            line = side + " = -2;";
                            file.Tab(COLUMN_2).Line(line);
                        }

                        if (open)
                        {
                            file.Tab(COLUMN_1).Line("else");
                        }
                    }

                    file.Tab(COLUMN_1).Line("endif;");
                    associates.FindNext(associateFilter());
                }

                if (Vb::IsTrue(structures["Captureable"]))
                {
                    file.Line("else");
                    capturedLine();
                    file.Tab(COLUMN_1).Line(line);
                    first = true;
                    associates.FindFirst(associateFilter());

                    while (!associates.noMatch)
                    {
                        if (multiplayer())
                        {
                            line = "objectChangeSides(" + objectText(associates) + ",MsgParams);";
                            addMultiplayerEvent(line, first);
                            first = false;
                            line = "ReleaseGateLock(" + objectText(associates) + ");";
                            addMultiplayerEvent(line, false);
                        }
                        else
                        {
                            line = "objectChangeSides(" + objectText(associates) + ",INNER_SPHERE);";
                            file.Tab(COLUMN_2).Line(line);
                            line = "ReleaseGateLock(" + objectText(associates) + ");";
                            file.Tab(COLUMN_2).Line(line);
                            line = "setPotentialContact(" + objectText(associates) + ",2);";
                            file.Tab(COLUMN_2).Line(line);
                        }

                        associates.FindNext(associateFilter());
                    }

                    if (multiplayer())
                    {
                        line = side + " = MsgParams;";
                        addMultiplayerEvent(line, first);
                        line = "objectChangeSides(" + text + ",MsgParams);";
                        const short code = addMultiplayerEvent(line, false);
                        line = "sendMessage(" + Vb::CStr(code) + ",ObjectSide(" + text + "));";
                        file.Tab(COLUMN_2).Line(line);
                        file.Tab(COLUMN_2).Line("handleMessage;");
                    }
                    else
                    {
                        line = side + " = INNER_SPHERE;";
                        file.Tab(COLUMN_2).Line(line);
                    }

                    file.Tab(COLUMN_1).Line("endif;");
                }

                file.Line("endif;");
            }
            else if (Vb::Eq(kind, literal("Power Station")))
            {
                file.Line();
                line = "if (isdead(" + text + ")) then";
                file.Line(line);
                first = true;
                associates.FindFirst(associateFilter());

                while (!associates.noMatch)
                {
                    line = "setObjectActive(" + objectText(associates) + ",FALSE);";
                    file.Tab(COLUMN_1).Line(line);
                    associates.FindNext(associateFilter());
                }

                file.Line("endif;");
            }
            else if (Vb::Eq(kind, literal("Power Control")))
            {
                file.Line();
                line = "if ((isdead(" + text + ")) AND (" + side + " <> -2)) then";
                file.Line(line);
                first = true;
                associates.FindFirst(associateFilter());

                while (!associates.noMatch)
                {
                    associateSides();
                    line = "setObjectActive(" + objectText(associates) + ",FALSE);";

                    if (multiplayer())
                    {
                        sendEvent(COLUMN_1, "SendMessage", "HandleMessage;", "0");
                    }
                    else
                    {
                        file.Tab(COLUMN_1).Line(line);
                    }

                    associates.FindNext(associateFilter());
                }

                line = side + " = -2;";

                if (multiplayer())
                {
                    sendEvent(COLUMN_1, "SendMessage", "HandleMessage;", "0");
                }
                else
                {
                    file.Tab(COLUMN_1).Line(line);
                }

                if (Vb::IsTrue(structures["Captureable"]))
                {
                    file.Line("else");
                    capturedLine();
                    file.Tab(COLUMN_1).Line(line);
                    first = true;
                    associates.FindFirst(associateFilter());
                    const CString objectSide = "ObjectSide(" + text + ")";

                    while (!associates.noMatch)
                    {
                        if (Vb::Eq(associates["Inactive"], Vb::Boolean(true)))
                        {
                            // Original behaviour (EB-108): the initialization lacks its comma: "setObjectActive(idFALSE);".
                            addInitialization("setObjectActive(" + objectText(associates) + "FALSE)", "");
                            line = "setObjectActive(" + objectText(associates) + " , TRUE);";

                            if (multiplayer())
                            {
                                sendEvent(COLUMN_2, "SendMessage", "HandleMessage;", objectSide);
                            }
                            else
                            {
                                file.Tab(COLUMN_2).Line(line);
                            }
                        }

                        line = "objectChangeSides(" + objectText(associates) +
                               (multiplayer() ? ",MsgParams);" : ",INNER_SPHERE);");

                        if (multiplayer())
                        {
                            sendEvent(COLUMN_2, "SendMessage", "HandleMessage;", objectSide);
                        }
                        else
                        {
                            file.Tab(COLUMN_2).Line(line);
                        }

                        // Original behaviour (EB-108): a ")" too many; a multiplayer game doesn't write it at all.
                        line = "setPotentialContact(" + objectText(associates) + "),2);";

                        if (!multiplayer())
                        {
                            file.Tab(COLUMN_2).Line(line);
                        }

                        associates.FindNext(associateFilter());
                    }

                    writeCapturedEnd();
                }

                file.Line("endif;");
            }
            else if (Vb::Eq(kind, literal("No-link Captureable")))
            {
                file.Line();
                line = "if ((isdead(" + text + ")) AND (" + side + " <> -2)) then";
                file.Line(line);
                line = side + " = -2;";

                if (multiplayer())
                {
                    first = true;
                    sendEvent(COLUMN_1, "SendMessage", "HandleMessage;", "0");
                }
                else
                {
                    file.Tab(COLUMN_1).Line(line);
                }

                file.Line("else");
                capturedLine();
                file.Tab(COLUMN_1).Line(line);
                writeCapturedSides();
                file.Tab(COLUMN_1).Line("endif;");
                file.Line("endif;");
            }
            else if (Vb::Eq(kind, literal("Mech Bay")))
            {
                file.Line();

                if (is(structures["Alignment"], CLAN_ALIGNMENT))
                {
                    ++mechBays;
                    addInitialization("MechBays[" + Vb::CStr(static_cast<short>(mechBays - 1)) + "]", text);
                }

                line = "if (isdead(" + text + ")) then";
                file.Line(line);

                if (multiplayer())
                {
                    file.Line();
                }
                else
                {
                    if (is(structures["Alignment"], CLAN_ALIGNMENT))
                    {
                        line = "removeMechBay(" + text + ");";
                        file.Tab(COLUMN_1).Line(line);
                    }

                    line = side + " = -2;";
                    file.Tab(COLUMN_1).Line(line);
                }

                if (Vb::Eq(structures["Captureable"], Vb::Boolean(true)))
                {
                    file.Line("else");
                    capturedLine();
                    file.Tab(COLUMN_1).Line(line);
                    writeCapturedSides();
                    file.Tab(COLUMN_1).Line("endif;");
                }

                file.Line("endif;");
            }
            else if (Vb::Eq(kind, literal("Vehicle Bay")))
            {
                file.Line();
                const COleVariant alignment = structures["Alignment"];

                if (is(alignment, CLAN_ALIGNMENT))
                {
                    ++vehicleBays;
                    addInitialization("VehicleBays[" + Vb::CStr(static_cast<short>(vehicleBays - 1)) + "]", text);
                    line = "if (isdead(" + text + ")) then";
                    file.Line(line);
                    line = "removeVehicleBay(" + text + ");";
                    file.Tab(COLUMN_1).Line(line);
                    file.Line("endif;");
                }
                else if (is(alignment, INNER_SPHERE_ALIGNMENT))
                {
                    Vb::MsgBox("The design does not permit you to have an Inner Sphere Vehicle Bay.  Please change "
                               "the alignment to Clan.");
                }
            }
            else if (Vb::Eq(kind, literal("Artillery Crate")))
            {
                file.Line();

                if (multiplayer())
                {
                    line = "if (isAlive(" + text + ")) then";
                    file.Line(line);
                    line = "if ((IsCaptured(" + text + ") == 1) AND (" + side + " > -2)) then";
                    file.Tab(COLUMN_1).Line(line);
                    line = "objectChangeSides(" + text + ", NEUTRAL);";
                    addMultiplayerEvent(line, true);
                    line = "if (getStrikes(((MsgParams/10)-1),(MsgParams MOD 10)) < MAX_STRIKES) then";
                    addMultiplayerEvent(line, false);
                    line = "      addStrikes(((MsgParams/10)-1),(MsgParams MOD 10),1);";
                    addMultiplayerEvent(line, false);
                    line = "endif;";
                    addMultiplayerEvent(line, false);
                    line = "setCaptureable(" + text + ", FALSE);";
                    addMultiplayerEvent(line, false);
                    line = side + " = -2;";
                    short code = addMultiplayerEvent(line, false);
                    line = "X = Random(STRIKE_TYPE_LIMIT);";
                    file.Tab(COLUMN_2).Line(line);
                    line = "sendMessage(" + Vb::CStr(code) + ",(((objectCommander(" + text + ")+1)*10)+X));";
                    file.Tab(COLUMN_2).Line(line);
                    line = "handleMessage;";
                    file.Tab(COLUMN_2).Line(line);
                    file.Tab(COLUMN_1).Line("else");
                    line = "if (" + side + " < -1) then";
                    file.Tab(COLUMN_2).Line(line);
                    line = "if (" + side + " > -59) then";
                    file.Tab(COLUMN_3).Line(line);
                    line = "if (gametime == nextsecond - 1.0) then";
                    file.Tab(COLUMN_4).Line(line);
                    line = side + " = " + side + " - 1;";
                    code = addMultiplayerEvent(line, true);
                    line = "sendMessage(" + Vb::CStr(code) + ",0);";
                    file.Tab(COLUMN_5).Line(line);
                    line = "handleMessage;";
                    file.Tab(COLUMN_5).Line(line);
                    file.Tab(COLUMN_4).Line("endif;");
                    file.Tab(COLUMN_3).Line("else");
                    line = "setCaptureable(" + text + ", TRUE);";
                    addMultiplayerEvent(line, true);
                    line = side + " = NEUTRAL;";
                    code = addMultiplayerEvent(line, false);
                    line = "sendMessage(" + Vb::CStr(code) + ",0);";
                    file.Tab(COLUMN_4).Line(line);
                    line = "handleMessage;";
                    file.Tab(COLUMN_4).Line(line);
                    file.Tab(COLUMN_3).Line("endif;");
                    file.Tab(COLUMN_2).Line("endif;");
                    file.Tab(COLUMN_1).Line("endif;");
                    file.Line("endif;");
                }
                else
                {
                    line = "if (IsAlive(" + text + ")) then";
                    file.Line(line);
                    line = "if ((IsCaptured(" + text + ") == 1) AND (" + side + " <> -2)) then";
                    file.Tab(COLUMN_1).Line(line);
                    line = "X = Random( STRIKE_TYPE_LIMIT );";
                    file.Tab(COLUMN_2).Line(line);
                    line = "Y = ObjectCommander(" + text + ");";
                    file.Tab(COLUMN_2).Line(line);
                    line = "if (GetStrikes( Y, X ) < MAX_STRIKES ) then";
                    file.Tab(COLUMN_2).Line(line);
                    line = "AddStrikes( Y, X, 2);";
                    file.Tab(COLUMN_3).Line(line);
                    // Original behaviour: Print ends with ";", so the next Tab starts a new line.
                    file.Tab(COLUMN_2).Text("endif;");
                    line = "setCaptureable(" + text + ", FALSE );";
                    file.Tab(COLUMN_2).Line(line);
                    line = side + " = -2;";
                    file.Tab(COLUMN_2).Line(line);
                    file.Tab(COLUMN_1).Line("endif;");
                    file.Line("endif;");
                }
            }

            structures.FindNext(missionFilter);
        }
    }

    // The totals.
    if (mechBays > 0)
    {
        addInitialization("TotalMechBays", Vb::Str(Vb::Integer(mechBays)));
    }

    if (vehicleBays > 0)
    {
        addInitialization("TotalVehicleBays", Vb::Str(Vb::Integer(vehicleBays)));
    }

    if (objectList > 0)
    {
        addInitialization("ObjectListSize", Vb::Str(Vb::Integer(objectList)));

        if (objectList > OBJECT_LIST_LIMIT)
        {
            Vb::MsgBox("Too many items on the ObjectList. Max is 10 (0 to 9).");
        }
    }

    if (perimeterAlarms > 0)
    {
        file.Line();
        file.Line("if (NOT PerimeterBreach) then");
        file.Line("      if (PlayGASound) then");

        if (multiplayer())
        {
            file.Line("            SendMessage(MSG_GENERAL_ALARM_OFF,0);");
            file.Line("            HandleMessage;");
        }
        else
        {
            file.Line("            PlayGASound = FALSE;");
        }

        file.Line("      endif;");
        file.Line("      if (PlayPASound) then");

        if (multiplayer())
        {
            file.Line("            SendMessage(MSG_PERIMETER_ALARM_OFF,0);");
            file.Line("            HandleMessage;");
        }
        else
        {
            file.Line("            PlayPASound = FALSE;");
        }

        file.Line("      endif;");
        file.Line("endif;");
    }

    file.Close();
}

bool FitAbl::WriteLookOutIncludeFile(short missionId, const CString& pathName)
{
    mission = std::make_unique<FitAblRecordset>(database);
    mission->OpenDynaset("NewMissions");
    mission->FindFirst("[MissionID] = " + Vb::CStr(missionId));

    if (mission->noMatch)
    {
        return false;
    }

    CString folder = pathName;

    if (CString(folder).Trim().IsEmpty())
    {
        folder = "c:";
    }

    VbFile& file = files[INCLUDE_FILE];
    line = folder + scriptName + "_LOP.ABI";
    file.Open(line);
    file.Line(SEPARATOR);
    file.Text("// Lookout Point Include file for mission ").Text(line).EndLine();
    file.Line();
    FitAblRecordset points(database);
    points.OpenDynaset("LookoutPoints");
    const CString filter = "MissionID = " + Vb::CStr(missionId);
    short point = -1;
    points.FindFirst(filter);

    while (!points.noMatch)
    {
        const bool multiplayer = [&] { return Vb::Eq((*mission)["Multiplayer"], Vb::Boolean(true)); }();
        ++point;
        const CString flag = "LOP" + Vb::CStr(point);
        addVariable("static boolean", flag, "TRUE");
        line = "if (" + flag + ") then";
        file.Line(line);
        line = Vb::CStr(Vb::Cat(Vb::Cat(literal("      aPoint[0] = "), points["XCoordinate"]), literal(";")));
        file.Line(line);
        line = Vb::CStr(Vb::Cat(Vb::Cat(literal("      aPoint[1] = "), points["YCoordinate"]), literal(";")));
        file.Line(line);
        line = "      aPoint[2] = 0.0;";
        file.Line(line);

        // Who sets it off: the player's force unless Side is set (True: the Clan's); by distance, by an ABL
        // condition, or both. Original behaviour (EB-110): another UseABLorRadius writes the line before again.
        const COleVariant sideValue = points["Side"];
        const bool clan = !Vb::IsNull(sideValue) && Vb::Eq(sideValue, Vb::Integer(-1));
        const CString force = clan ? "CLAN_FORCE" : "PLAYER_FORCE";
        const COleVariant use = points["UseABLorRadius"];

        if (Vb::IsNull(use) || Vb::Eq(use, literal("Radius")))
        {
            line = Vb::CStr(
                Vb::Cat(Vb::Cat(Vb::String("      if (distanceToPosition(" + force + ", aPoint) < "), points["Radius"]),
                        literal(") then")));
        }
        else if (Vb::Eq(use, literal("Both")))
        {
            line = Vb::CStr(Vb::Cat(Vb::Cat(Vb::Cat(Vb::Cat(literal("      if (("), points["ABLCondition"]),
                                                    Vb::String(") AND (distanceToPosition(" + force + ", aPoint) < ")),
                                            points["Radius"]),
                                    literal(")) then")));
        }
        else if (Vb::Eq(use, literal("ABL")))
        {
            line = Vb::CStr(Vb::Cat(Vb::Cat(literal("      if ("), points["ABLCondition"]), literal(") then")));
        }

        file.Line(line);
        const char* reveal = clan ? "setRevealed(-1," : "setRevealed(1,";

        if (multiplayer)
        {
            line = Vb::CStr(Vb::Cat(Vb::Cat(literal("aPoint[0] = "), points["XCoordinate"]), literal(";")));
            addMultiplayerEvent(line, true);
            line = Vb::CStr(Vb::Cat(Vb::Cat(literal("aPoint[1] = "), points["YCoordinate"]), literal(";")));
            addMultiplayerEvent(line, false);
            line = "aPoint[2] = 0.0;";
            addMultiplayerEvent(line, false);
            line = Vb::CStr(Vb::Cat(Vb::Cat(literal(reveal), points["RevealRadius"]), literal(".0,aPoint);")));
            const short code = addMultiplayerEvent(line, false);
            line = "            sendMessage(" + Vb::CStr(code) + ",0);";
            file.Line(line);
            file.Line("            handleMessage;");
            line = flag + " = FALSE;";
            addMultiplayerEvent(line, false);
        }
        else
        {
            line = Vb::CStr(Vb::Cat(Vb::Cat(Vb::String(CString("            ") + reveal), points["RevealRadius"]),
                                    literal(".0,aPoint);")));
            file.Line(line);
            line = "            " + flag + " = FALSE;";
            file.Line(line);
        }

        file.Line("      endif;");
        file.Line("endif;");
        points.FindNext(filter);
    }

    file.Close();
    return true;
}

short FitAbl::assignmentIndex(long uniqueEntry)
{
    FitAblRecordset rows(database);
    rows.OpenDynaset("MissionAssignments");
    rows.FindFirst("[UniqueEntry] = " + Vb::CStr(Vb::Long(uniqueEntry)));

    if (rows.noMatch)
    {
        Vb::MsgBox("Couldn't find MissionAssignment" + Vb::CStr(Vb::Long(uniqueEntry)));
        rows.Close();
        return 0;
    }

    const long missionId = Vb::CLng(rows["MissionID"]);
    const CString alignment = Vb::CStr(rows["Alignment"]);
    const short forceGroup = Vb::CInt(rows["ForceGroup"]);
    const short role = Vb::CInt(rows["Role"]);
    const CString filter = "[MissionID] = " + Vb::CStr(Vb::Long(missionId)) + " AND [Alignment] = '" + alignment +
                           "' AND [ForceGroup] = " + Vb::CStr(forceGroup) + " AND [Role] <= " + Vb::CStr(role);
    // The parts of its group before it: a lower role, or the same role entered earlier.
    short index = 0;
    rows.FindFirst(filter);

    while (!rows.noMatch)
    {
        if (Vb::Lt(rows["Role"], Vb::Integer(role)))
        {
            ++index;
        }
        else if (Vb::IsTrue(Vb::And(Vb::CmpEq(rows["Role"], Vb::Integer(role)),
                                    Vb::CmpLt(rows["UniqueEntry"], Vb::Long(uniqueEntry)))))
        {
            ++index;
        }

        rows.FindNext(filter);
    }

    rows.Close();
    return index;
}
