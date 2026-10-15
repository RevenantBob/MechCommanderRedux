#include "stdafx.h"
#include "FitAbl.h"

// FitAbl's objective actions (0x1103a4b0-0x11052a40): each ObjectiveScripting row of an objective becomes ABL code
// that checks the action's condition and sets the objective's (or the action's) success or failure.

namespace
{
    /// <summary>The script file's number.</summary>
    inline constexpr int SCRIPT_FILE = 2;
    /// <summary>The columns of an action's checks and the code inside them.</summary>
    inline constexpr int CHECK_COLUMN = 18;
    inline constexpr int INNER_COLUMN = 24;
    inline constexpr int INNER2_COLUMN = 30;
    /// <summary>The objectives' Alignment of the Inner Sphere's.</summary>
    inline constexpr short INNER_SPHERE_ALIGNMENT = 2;
    /// <summary>The objective's TimeLeft when it has no time limit.</summary>
    inline constexpr short NO_TIME_LIMIT = -1;
    /// <summary>A lance or star's slots: the eighth on are written as 7 + n.</summary>
    inline constexpr double LANCE_SLOTS = 8;
    inline constexpr double LAST_LANCE_SLOT = 7;
    /// <summary>The time-out check written when the objective has no time limit.</summary>
    const char* const NO_TIME_LIMIT_CHECK = "if (gettimeleft == 0.0) then";
    const char* const NO_TIME_LIMIT_CHECK_COMMENTED =
        "if (gettimeleft == 0.0) then  // supposedly will not happen unless time limit is set";

    /// <summary>The parts of a concatenation: literal text or a field's value.</summary>
    class Part
    {
    public:
        Part(const char* text) : value(Vb::String(text)) {}

        Part(const CString& text) : value(Vb::String(text)) {}

        Part(const COleVariant& field) : value(field) {}

        /// <summary>The part as VB's &amp; takes it.</summary>
        COleVariant value;
    };

    /// <summary>The parts joined with VB's &amp; (variants: Null counts as empty), as a string.</summary>
    CString cat(std::initializer_list<Part> parts)
    {
        COleVariant result = Vb::String("");

        for (const Part& part : parts)
        {
            result = Vb::Cat(result, part.value);
        }

        return Vb::CStr(result);
    }

    /// <summary>A number written as VB's CStr writes a Double.</summary>
    CString number(double value)
    {
        return Vb::CStr(COleVariant(value));
    }

    /// <summary>An Integer as VB's CStr writes it.</summary>
    CString integer(int value)
    {
        return Vb::CStr(static_cast<short>(value));
    }
}

bool FitAbl::writeObjectiveActions(long objectiveId, short objective, short alignment)
{
    VbFile& script = files[SCRIPT_FILE];

    // The other side, for the capture checks.
    CString enemySide;

    if (alignment == 1)
    {
        enemySide = "CLAN";
    }
    else if (alignment == 2)
    {
        enemySide = "INNER_SPHERE";
    }

    const CString filter = "[ObjectiveID] = " + Vb::CStr(Vb::Long(objectiveId));
    actions->FindFirst(filter);
    actionCount = 0;

    while (Vb::Eq(Vb::Boolean(actions->noMatch), Vb::Boolean(false)))
    {
        ++actionCount;
        actions->FindNext(filter);
    }

    if (actionCount == 0)
    {
        return false;
    }

    actions->FindFirst(filter);

    for (short action = 1; action <= actionCount; ++action)
    {
        FitAblRecordset& row = *actions;
        const COleVariant kind = row["Action"];
        const COleVariant p1 = row["ActionParm1"];
        const COleVariant p2 = row["ActionParm2"];
        const COleVariant p3 = row["ActionParm3"];
        const COleVariant p4 = row["ActionParm4"];
        const COleVariant p5 = row["ActionParm5"];
        const bool innerSphere = Vb::Eq((*objectives)["Alignment"], Vb::Integer(INNER_SPHERE_ALIGNMENT));
        const COleVariant timeLeft = (*objectives)["TimeLeft"];
        const bool timed = Vb::Ne(timeLeft, Vb::Integer(NO_TIME_LIMIT));
        const bool afterLastAction = Vb::Eq(p4, Vb::String("After Last Action"));
        const CString objectiveText = integer(objective);
        const CString actionText = integer(action);
        const CString lastAction = integer(action - 1);
        const CString lastObjective = integer(objective - 1);
        auto is = [](const COleVariant& value, const char* text) { return Vb::Eq(value, Vb::String(text)); };

        // Val(x) = 0 Or IsNull(x): the count parameter is unset (Val of Null raises, as VB's does).
        auto noCount = [](const COleVariant& value)
        {
            const bool zero = Vb::Val(Vb::CStr(value)) == 0;
            return zero || Vb::IsNull(value);
        };

        // The checks after the action's condition and its "else".
        enum class Tail
        {
            // Only the blank line.
            Blank,
            // "endif;" at 24 and 18, then the blank line.
            EndIfs,
        };

        Tail tail = Tail::Blank;

        // Condition, success, else, time-out check, failure (the Destroy actions).
        auto destroyCheck = [&](const CString& condition, bool commented)
        {
            script.Tab(CHECK_COLUMN).Line(condition);
            writeObjectiveSuccess(objective, action, 0);
            script.Tab(CHECK_COLUMN).Line("else");
            line = timed ? cat({"if (gametime > ", timeLeft, ") then"})
                         : CString(commented ? NO_TIME_LIMIT_CHECK_COMMENTED : NO_TIME_LIMIT_CHECK);
            script.Tab(INNER_COLUMN).Line(line);
            writeObjectiveFailure(objective, action, 2);
            tail = Tail::EndIfs;
        };

        // The time-out check after "After Last Action": or the action before succeeding.
        auto afterLastActionCheck = [&]()
        {
            if (timed)
            {
                return action > 1
                           ? cat({"if ((gametime > ", timeLeft, ") OR (Obj_", objectiveText, "_Action_", lastAction,
                                  "_Success)) then"})
                           : cat({"if ((gametime > ", timeLeft, ") OR (Objective_", lastObjective, "_Decided)) then"});
            }

            return action > 1 ? "if ((gettimeleft == 0.0) OR (Obj_" + objectiveText + "_Action_" + lastAction +
                                    "_Success)) then"
                              : "if ((gettimeleft == 0.0) OR (Objective_" + lastObjective + "_Decided)) then";
        };

        // Condition (a failure), failure, else, time-out check, success (the Prevent actions).
        auto preventCheck = [&](const CString& condition)
        {
            script.Tab(CHECK_COLUMN).Line(condition);
            writeObjectiveFailure(objective, action, 0);
            script.Tab(CHECK_COLUMN).Line("else");

            if (afterLastAction && action > 1)
            {
                line = afterLastActionCheck();
            }
            else
            {
                line = timed ? cat({"if (gametime > ", timeLeft, ") then"}) : CString(NO_TIME_LIMIT_CHECK_COMMENTED);
            }

            script.Tab(INNER_COLUMN).Line(line);
            writeObjectiveSuccess(objective, action, 2);
            tail = Tail::EndIfs;
        };

        // The time-out without "After Last Action" of the Guard actions.
        enum class GuardTimeout
        {
            // Without a limit: the guarding side's force dead.
            Alignment,
            // Without a limit: the Clan's force dead (Guard Ally, Guard Player).
            Clan,
            // Without a limit: the player's force dead (Guard Clanner).
            Player,
            // With a limit too, and the guarding side's force dead (Guard Num Objects).
            Both,
        };

        // Condition (a failure), failure, else, time-out check, success (the Guard actions).
        auto guardCheck = [&](const CString& condition, GuardTimeout kind)
        {
            script.Tab(CHECK_COLUMN).Line(condition);
            writeObjectiveFailure(objective, action, 0);
            script.Tab(CHECK_COLUMN).Line("else");

            if (afterLastAction)
            {
                line = afterLastActionCheck();
            }
            else if (timed)
            {
                line = kind == GuardTimeout::Both
                           ? cat({"if ((gametime > ", timeLeft,
                                  innerSphere ? ") OR (ClanForceDead)) then" : ") OR (PlayerForceDead)) then"})
                           : cat({"if (gametime > ", timeLeft, ") then"});
            }
            else
            {
                const bool clan = kind == GuardTimeout::Clan ||
                                  ((kind == GuardTimeout::Alignment || kind == GuardTimeout::Both) && innerSphere);
                line = clan ? "if ((gettimeleft == 0.0) OR (ClanForceDead)) then  // supposedly will not happen "
                              "unless time limit is set"
                            : "if ((gettimeleft == 0.0) OR (PlayerForceDead)) then  // supposedly will not happen "
                              "unless time limit is set";
            }

            script.Tab(INNER_COLUMN).Line(line);
            writeObjectiveSuccess(objective, action, 2);
            tail = Tail::EndIfs;
        };

        // A lance or star slot: the slot's number, or 7 + n from the eighth on.
        auto slot = [&](const char* prefix, const char* bigPrefix, const char* suffix)
        {
            const double value = Vb::Val(Vb::CStr(p2));
            return value < LANCE_SLOTS ? cat({prefix, p2, suffix})
                                       : CString(bigPrefix) + number(value - LAST_LANCE_SLOT) + suffix;
        };

        // The object id of the action's block, vertex and index ("x = id;").
        auto objectLine = [&](const COleVariant& indexParameter)
        {
            const long id = objectId(Vb::CInt(p1), Vb::CInt(p2), Vb::CInt(indexParameter));
            return "x = " + Vb::CStr(Vb::Long(id)) + ";";
        };

        if (is(kind, "Destroy Enemy"))
        {
            if (is(p1, "All"))
            {
                destroyCheck(innerSphere ? "if (isDead(CLAN_FORCE)) then" : "if (isDead(PLAYER_FORCE)) then", true);
            }
            else if (is(p1, "Lance/Star"))
            {
                const COleVariant group = Vb::Sub(p2, Vb::Integer(1));

                if (noCount(p3))
                {
                    destroyCheck(innerSphere ? cat({"if (isDead(ClanStar(", group, "))) then"})
                                             : cat({"if (isDead(PLAYER_LANCE", group, ")) then"}),
                                 false);
                }
                else
                {
                    destroyCheck(innerSphere ? cat({"if (NumDead(ClanStar(", group, ")) >= ", p3, ") then"})
                                             : cat({"if (NumDead(PLAYER_LANCE", group, ") >= ", p3, ") then"}),
                                 false);
                }
            }
            else if (is(p1, "Any"))
            {
                if (Vb::IsNull(p3))
                {
                    destroyCheck(innerSphere ? "if (NumDead(CLAN_FORCE) >= 1) then"
                                             : "if (NumDead(PLAYER_FORCE) >= 1) then",
                                 false);
                }
                else
                {
                    destroyCheck(cat({innerSphere ? "if (NumDead(CLAN_FORCE) >= " : "if (NumDead(PLAYER_FORCE) >= ", p3,
                                      ") then"}),
                                 false);
                }
            }
        }
        else if (is(kind, "Enemy Dead or Fled"))
        {
            if (is(p1, "All"))
            {
                destroyCheck(
                    innerSphere ? "if (isDeadorFled(CLAN_FORCE)) then" : "if (isDeadorFled(PLAYER_FORCE)) then", true);
            }
            else if (is(p1, "Lance/Star"))
            {
                if (noCount(p3))
                {
                    line = innerSphere ? cat({"if (isDeadorFled(ClanStar(", p2, "))) then"})
                                       : "if (isDeadorFled(PLAYER_LANCE" + number(Vb::Val(Vb::CStr(p2))) + ")) then";
                }
                else if (innerSphere)
                {
                    line =
                        cat({"if (NumDead(ClanStar(", p2, ")) + NumWithdrawn(ClanStar(", p2, ")) >= ", p3, ") then"});
                }
                else
                {
                    const CString lance = number(Vb::Val(Vb::CStr(p2)));
                    line =
                        cat({"if (NumDead(PLAYER_LANCE" + lance + ") + NumWithdrawn(PLAYER_LANCE" + lance + ") >= ", p3,
                             ") then"});
                }

                destroyCheck(line, false);
            }
            else if (is(p1, "Any"))
            {
                if (noCount(p3))
                {
                    destroyCheck(innerSphere ? "if (NumDead(CLAN_FORCE) + NumWithdrawn(CLAN_FORCE) >= 1) then"
                                             : "if (NumDead(PLAYER_FORCE) + NumWithdrawn(PLAYER_FORCE) >= 1) then",
                                 false);
                }
                else
                {
                    destroyCheck(cat({innerSphere ? "if (NumDead(CLAN_FORCE) + NumWithdrawn(CLAN_FORCE) >= "
                                                  : "if (NumDead(PLAYER_FORCE) + NumWithdrawn(PLAYER_FORCE) >= ",
                                      p3, ") then"}),
                                 false);
                }
            }
        }
        else if (is(kind, "Prevent Enemy Dead or Fled"))
        {
            if (is(p1, "All"))
            {
                preventCheck(innerSphere ? "if (Numdeadorfled(CLAN_FORCE) >= 1) then"
                                         : "if (Numdeadorfled(PLAYER_FORCE) >= 1) then");
            }
            else if (is(p1, "Lance/Star"))
            {
                if (noCount(p3))
                {
                    preventCheck(innerSphere ? cat({"if (NumDeadorFled(ClanStar(", p2, ")) > 0) then"})
                                             : "if (NumDeadorFled(PLAYER_LANCE" + number(Vb::Val(Vb::CStr(p2))) +
                                                   ") > 0) then");
                }
                else
                {
                    preventCheck(innerSphere
                                     ? cat({"if (NumDeadorFled(ClanStar(", p2, ")) >= ", p3, ") then"})
                                     : cat({"if (NumDeadorFled(PLAYER_LANCE" + number(Vb::Val(Vb::CStr(p2))) + ") >= ",
                                            p3, ") then"}));
                }
            }
            else if (is(p1, "Any"))
            {
                if (noCount(p3))
                {
                    preventCheck(innerSphere ? "if (NumDeadorFled(CLAN_FORCE) > 0) then"
                                             : "if (NumDeadorFled(PLAYER_FORCE) > 0) then");
                }
                else
                {
                    preventCheck(cat(
                        {innerSphere ? "if (NumDeadorFled(CLAN_FORCE) >= " : "if (NumDeadorFled(PLAYER_FORCE) >= ", p3,
                         ") then"}));
                }
            }
        }
        else if (is(kind, "Prevent Enemy Withdraw"))
        {
            if (is(p1, "All"))
            {
                preventCheck(innerSphere ? "if (NumWithdrawn(CLAN_FORCE) > 0) then"
                                         : "if (NumWithdrawn(PLAYER_FORCE) > 0) then");
            }
            else if (is(p1, "Lance/Star"))
            {
                if (noCount(p3))
                {
                    preventCheck(innerSphere
                                     ? cat({"if (NumWithdrawn(ClanStar(", p2, ")) > 0) then"})
                                     : "if (NumWithdrawn(PLAYER_LANCE" + number(Vb::Val(Vb::CStr(p2))) + ") > 0) then");
                }
                else
                {
                    preventCheck(innerSphere
                                     ? cat({"if (NumWithdrawn(ClanStar(", p2, ")) >= ", p3, ") then"})
                                     : cat({"if (NumWithdrawn(PLAYER_LANCE" + number(Vb::Val(Vb::CStr(p2))) + ") >= ",
                                            p3, ") then"}));
                }
            }
            else if (is(p1, "Any"))
            {
                if (noCount(p3))
                {
                    preventCheck(innerSphere ? "if (NumWithdrawn(CLAN_FORCE) > 0) then"
                                             : "if (NumWithdrawn(PLAYER_FORCE) > 0) then");
                }
                else
                {
                    preventCheck(cat(
                        {innerSphere ? "if (NumWithdrawn(CLAN_FORCE) >= " : "if (NumWithdrawn(PLAYER_FORCE) >= ", p3,
                         ") then"}));
                }
            }
        }
        else if (is(kind, "Destroy Object"))
        {
            if (is(p3, "Yes"))
            {
                addInitialization(cat({"getobjectposition(", p1, ",aPoint)"}), "");
                addInitialization("setObjectivePos(" + objectiveText + ",aPoint[0],aPoint[1],aPoint[2])", "");
            }

            destroyCheck(cat({"if (isDead(", p1, ")) then"}), false);
        }
        else if (is(kind, "Destroy Num Objects"))
        {
            destroyCheck(is(p3, "ALL") ? CString("if (NumObjectListDead >= ObjectListSize) then")
                                       : cat({"if (NumObjectListDead >= ", p3, ") then"}),
                         false);
        }
        else if (is(kind, "Guard Num Objects"))
        {
            guardCheck(is(p3, "ALL") ? CString("if (NumObjectListAlive < ObjectListSize) then")
                                     : cat({"if (NumObjectListAlive < ", p3, ") then"}),
                       GuardTimeout::Both);
        }
        else if (is(kind, "Capture Num Objects"))
        {
            destroyCheck(is(p3, "ALL") ? CString("if (NumObjectListCaptured >= ObjectListSize) then")
                                       : cat({"if (NumObjectListCaptured >= ", p3, ") then"}),
                         false);
        }
        else if (is(kind, "Destroy Structure"))
        {
            line = objectLine(p4);
            script.Tab(CHECK_COLUMN).Line(line);

            if (is(p3, "Yes"))
            {
                addInitialization(line, "");
                addInitialization("getobjectposition(x,aPoint)", "");
                addInitialization("setObjectivePos(" + objectiveText + ",aPoint[0],aPoint[1],aPoint[2])", "");
            }

            destroyCheck("if (isDead(x)) then", false);
        }
        else if (is(kind, "Destroy Vehicle"))
        {
            // Original behaviour (EB-103): another group keeps the last line's text as the vehicle's group.
            if (is(p1, "CLAN STAR"))
            {
                line = "CLAN_STAR";
            }
            else if (is(p1, "ALLIED LANCE"))
            {
                line = "ALLIED_LANCE";
            }
            else if (is(p1, "PLAYER LANCE"))
            {
                line = "PLAYER_LANCE";
            }

            const double value = Vb::Val(Vb::CStr(p2));
            line = value < LANCE_SLOTS ? cat({line, p2}) : line + "7 + " + number(value - LAST_LANCE_SLOT);
            line = cat({"if (isDead(VehicleID(" + line + ",", p3, ",0))) then"});
            destroyCheck(line, false);
        }
        else if (is(kind, "Destroy Vehicle Type"))
        {
            line = cat({"x = NumVehicleType(", p2, ",", p1, ",y);"});
            script.Tab(CHECK_COLUMN).Line(line);
            destroyCheck(noCount(p3) ? CString("if (x == y) then") : cat({"if (y >= ", p3, ") then"}), false);
        }
        else if (is(kind, "Guard Structure"))
        {
            if (is(p3, "Yes"))
            {
                // Original behaviour (EB-102): the index is read from ActionParm3, "Yes", which isn't a number.
                line = objectLine(p3);
                script.Tab(CHECK_COLUMN).Line(line);
                addInitialization(line, "");
                line = "getobjectposition(x,aPoint);";
                addInitialization(line, "");
                line = "setObjectivePos(" + objectiveText + ",aPoint[0],aPoint[1],aPoint[2]);";
                addInitialization(line, "");
            }
            else
            {
                const long id = objectId(Vb::CInt(p1), Vb::CInt(p2), Vb::CInt(p4));
                line = "x =" + Vb::CStr(Vb::Long(id)) + ";";
                script.Tab(CHECK_COLUMN).Line(line);
            }

            guardCheck("if (isDead(x)) then", GuardTimeout::Alignment);
        }
        else if (is(kind, "Guard Ally"))
        {
            if (is(p1, "All"))
            {
                line = "if (NumDead(ALLIED_FORCE) >= 1) then";
                guardCheck(line, GuardTimeout::Alignment);
            }
            else if (is(p1, "Lance/Star"))
            {
                guardCheck(noCount(p3)
                               ? slot("if (isDead(ALLIED_LANCE", "if (isDead(ALLIED_LANCE7 + ", ")) then")
                               : cat({slot("if (NumAlive(ALLIED_LANCE", "if (NumAlive(ALLIED_LANCE7 + ", ") < "), p3,
                                      ") then"}),
                           GuardTimeout::Clan);
            }
            else if (is(p1, "Any"))
            {
                guardCheck(noCount(p3) ? CString("if (NumDead(ALLIED_FORCE) > 0) then")
                                       : cat({"if (NumAlive(ALLIED_FORCE) < ", p3, ") then"}),
                           GuardTimeout::Clan);
            }
        }
        else if (is(kind, "Guard Vehicle Type"))
        {
            line = cat({"x = NumVehicleType(", p2, ",", p1, ",y);"});
            script.Tab(CHECK_COLUMN).Line(line);
            guardCheck(noCount(p3) ? CString("if (y > 0) then") : cat({"if (x - y < ", p3, ") then"}),
                       GuardTimeout::Alignment);
        }
        else if (is(kind, "Guard Vehicle"))
        {
            line = cat({"ALLIED_FORCE,", Vb::Sub(p2, Vb::Integer(1))});
            line = cat({"if (isDead(getVehicleID(" + line + ",", p3, "))) then"});
            guardCheck(line, GuardTimeout::Alignment);
        }
        else if (is(kind, "Move to Area"))
        {
            const bool marker = is(p5, "Yes");
            line = cat({"aPoint[0] = ", p1, ";"});
            script.Tab(CHECK_COLUMN).Line(line);

            if (marker)
            {
                addInitialization(line, "");
            }

            line = cat({"aPoint[1] = ", p2, ";"});
            script.Tab(CHECK_COLUMN).Line(line);

            if (is(p5, "Yes"))
            {
                addInitialization(line, "");
            }

            line = "aPoint[2] = 0.0;";
            script.Tab(CHECK_COLUMN).Line(line);

            if (is(p5, "Yes"))
            {
                addInitialization(line, "");
            }

            if (is(p5, "Yes"))
            {
                line = "setObjectivePos(" + objectiveText + ",aPoint[0],aPoint[1],aPoint[2]);";
                addInitialization(line, "");
            }

            const char* force = innerSphere ? "PLAYER_FORCE" : "CLAN_FORCE";

            // Original behaviour (EB-103): another ActionParm4 checks the last line written.
            if (is(p4, "Anytime"))
            {
                line = cat({CString("if (distancetoposition(") + force + ",aPoint) < ", p3, ") then"});
            }
            else if (afterLastAction)
            {
                line = action > 1 ? cat({"if ((Obj_" + objectiveText + "_Action_" + lastAction +
                                             "_Success) AND (distancetoposition(" + force + ",aPoint) < ",
                                         p3, ")) then"})
                                  : cat({"if ((Objective_" + lastObjective + "_Decided) AND (distancetoposition(" +
                                             force + ",aPoint) < ",
                                         p3, ")) then"});
            }

            destroyCheck(line, false);
        }
        else if (is(kind, "Move All to Area") || is(kind, "Move All Allied to Area"))
        {
            const bool allied = is(kind, "Move All Allied to Area");
            line = cat({"aPoint[0] = ", p1, ";"});
            script.Tab(CHECK_COLUMN).Line(line);
            line = cat({"aPoint[1] = ", p2, ";"});
            script.Tab(CHECK_COLUMN).Line(line);
            line = "aPoint[2] = 0.0;";
            script.Tab(CHECK_COLUMN).Line(line);
            const bool all = is(p5, "ALL");

            if (is(p4, "Anytime"))
            {
                const char* force = allied ? "ALLIED_FORCE" : innerSphere ? "PLAYER_FORCE" : "CLAN_FORCE";
                const char* lance = allied ? "ALLIED_LANCE" : innerSphere ? "PLAYER_LANCE" : "CLAN_STAR";
                line = all ? cat({CString("if (isWithinArea(") + force + ",aPoint,", p3, ")) then"})
                           : cat({CString("if (isWithinArea(") + lance, p5, ",aPoint,", p3, ")) then"});
            }
            else if (afterLastAction)
            {
                // Original behaviour (EB-104): the allied action checks the player's force or lance after another action.
                const char* force = allied || innerSphere ? "PLAYER_FORCE" : "CLAN_FORCE";
                const char* lance = allied || innerSphere ? "PLAYER_LANCE" : "CLAN_STAR";
                const CString start = action > 1 ? "if ((Obj_" + objectiveText + "_Action_" + lastAction + "_Success"
                                                 : "if ((Objective_" + lastObjective + "_Decided";
                line = all ? cat({start + ") AND (isWithinArea(" + force + ",aPoint,", p3, "))) then"})
                           : cat({start + ") AND (isWithinArea(" + lance, p5, ",aPoint,", p3, "))) then"});
            }

            destroyCheck(line, false);
        }
        else if (is(kind, "Extraction Zone") || is(kind, "Allied Extraction Zone"))
        {
            const bool allied = is(kind, "Allied Extraction Zone");
            line = cat({"aPoint[0] = ", p1, ";"});
            script.Tab(CHECK_COLUMN).Line(line);
            line = cat({"aPoint[1] = ", p2, ";"});
            script.Tab(CHECK_COLUMN).Line(line);
            line = "aPoint[2] = 0.0;";
            script.Tab(CHECK_COLUMN).Line(line);
            const bool all = is(p5, "ALL");
            const bool player = allied || innerSphere;
            const char* hotSpot = player ? ")) AND (NOT InHotSpot(CLAN_FORCE," : ")) AND (NOT InHotSpot(PLAYER_FORCE,";
            const char* force = allied        ? "ALLIED_FORCE,aPoint,"
                                : innerSphere ? "PLAYER_FORCE,aPoint,"
                                              : "CLAN_FORCE,aPoint,";
            const char* lance = allied ? "ALLIED_LANCE" : innerSphere ? "PLAYER_LANCE" : "ClanStar(";
            const char* lanceEnd = allied || innerSphere ? ",aPoint," : "),aPoint,";
            CString start;

            if (is(p4, "Anytime"))
            {
                start = "if ((";
            }
            else if (afterLastAction)
            {
                start = action > 1 ? "if ((Obj_" + objectiveText + "_Action_" + lastAction + "_Success) AND ("
                                   : "if ((Objective_" + lastObjective + "_Decided) AND (";
            }

            if (is(p4, "Anytime") || afterLastAction)
            {
                const CString radius = number(Vb::Val(Vb::CStr(p3)) * 2);
                line = all ? cat({start + "isWithinArea(" + force, p3, hotSpot, p1, ",", p2, ",", radius, "))) then"})
                           : cat({start + "isWithinArea(" + lance, p5, lanceEnd, p3, hotSpot, p1, ",", p2, ",", radius,
                                  "))) then"});
            }

            destroyCheck(line, false);
        }
        else if (is(kind, "Guard Player"))
        {
            if (is(p1, "All"))
            {
                guardCheck("if (NumDead(PLAYER_FORCE) >= 1) then", GuardTimeout::Clan);
            }
            else if (is(p1, "Lance/Star"))
            {
                guardCheck(noCount(p3)
                               ? slot("if (isDead(PLAYER_LANCE", "if (isDead(PLAYER_LANCE7 + ", ")) then")
                               : cat({slot("if (NumAlive(PLAYER_LANCE", "if (NumAlive(PLAYER_LANCE7 + ", ") < "), p3,
                                      ") then"}),
                           GuardTimeout::Clan);
            }
            else if (is(p1, "Any"))
            {
                guardCheck(noCount(p3) ? CString("if (NumDead(PLAYER_FORCE) >= 1) then")
                                       : cat({"if (NumAlive(PLAYER_FORCE) < ", p3, ") then"}),
                           GuardTimeout::Clan);
            }
        }
        else if (is(kind, "Guard Clanner"))
        {
            if (is(p1, "All"))
            {
                guardCheck("if (NumDead(CLAN_FORCE) >= 1) then", GuardTimeout::Player);
            }
            else if (is(p1, "Lance/Star"))
            {
                guardCheck(noCount(p3) ? slot("if (isDead(CLAN_STAR", "if (isDead(CLAN_STAR7 + ", ")) then")
                                       : cat({slot("if (NumAlive(CLAN_STAR", "if (NumAlive(CLAN_STAR7 + ", ") < "), p3,
                                              ") then"}),
                           GuardTimeout::Player);
            }
            else if (is(p1, "Any"))
            {
                guardCheck(noCount(p3) ? CString("if (NumDead(CLAN_FORCE) >= 1) then")
                                       : cat({"if (NumAlive(CLAN_FORCE) < ", p3, ") then"}),
                           GuardTimeout::Player);
            }
        }
        else if (is(kind, "Capture Structure") || is(kind, "King Of Hill"))
        {
            line = objectLine(p4);

            if (is(p3, "Yes"))
            {
                addInitialization(line, "");
                script.Tab(CHECK_COLUMN).Line(line);
                line = "getobjectposition(x,aPoint);";
                addInitialization(line, "");
                line = "setObjectivePos(" + objectiveText + ",aPoint[0],aPoint[1],aPoint[2]);";
                addInitialization(line, "");
            }
            else
            {
                script.Tab(CHECK_COLUMN).Line(line);
            }

            if (is(kind, "Capture Structure"))
            {
                script.Tab(CHECK_COLUMN)
                    .Line("if ((IsCaptured(x) == 1) AND ( objectSide(x) ==  " + enemySide + "))  then");
            }
            else
            {
                line = innerSphere ? "clanforcedead" : "playerforcedead";
                script.Tab(CHECK_COLUMN).Line("if (( " + line + " AND ( objectSide(x) ==  " + enemySide + ")) ");
                script.Tab(CHECK_COLUMN)
                    .Line("or (( gettimeleft() < 0.5 AND ( objectSide(x) ==  " + enemySide + ")) )");
            }

            writeObjectiveSuccess(objective, action, 0);
            script.Tab(CHECK_COLUMN).Line("else");
            line = timed ? cat({"if ((gametime > ", timeLeft, ") OR (getObjectDamage(x) > 99)) then"})
                         : CString("if ((gettimeleft == 0.0) OR (getObjectDamage(x) > 99)) then");
            script.Tab(INNER_COLUMN).Line(line);
            writeObjectiveFailure(objective, action, 2);
            tail = Tail::EndIfs;
        }
        else if (is(kind, "Capture Enemy Vehicles"))
        {
            // The enemy's captureable units.
            assignments = std::make_unique<FitAblRecordset>(database);
            assignments->OpenDynaset("MissionAssignments");
            short captureable = 0;
            const CString captureFilter =
                cat({"[MissionID] = ", (*mission)["MissionID"],
                     innerSphere ? " AND [Alignment] = 'Clan' AND [Captureable] = TRUE"
                                 : " AND [Alignment] = 'Inner Sphere' AND [Captureable] = TRUE"});
            assignments->FindFirst(captureFilter);

            while (!assignments->noMatch)
            {
                ++captureable;
                assignments->FindNext(captureFilter);
            }

            if (captureable == 0)
            {
                Vb::MsgBox("No captureable enemy vehicles were found for Capture Enemy Vehicle Action.");
            }
            else
            {
                const CString count = integer(captureable);
                const char* force = innerSphere ? "CLAN_FORCE" : "PLAYER_FORCE";
                // Computed only where it is written, as the original did: ActionParm2 needn't be a number otherwise.
                const auto group = [&] { return Vb::Sub(p2, Vb::Integer(1)); };
                const char* groupStart = innerSphere ? "ClanStar(" : "PLAYER_LANCE";
                const char* groupEnd = innerSphere ? "))" : ")";
                bool written = true;
                bool asAll = false;

                if (is(p1, "Any"))
                {
                    if (noCount(p3))
                    {
                        asAll = true;
                    }
                    else
                    {
                        line = cat({CString("if (isCaptured(") + force + ") >= ", p3, ") then"});
                    }
                }

                if (asAll || is(p1, "All"))
                {
                    line = CString("if (isCaptured(") + force + ") >= " + count + ") then";
                }
                else if (is(p1, "Lance/Star"))
                {
                    line = noCount(p3) ? cat({CString("if (isCaptured(") + groupStart, group(),
                                              CString(groupEnd) + " >= " + count + ") then"})
                                       : cat({CString("if (isCaptured(") + groupStart, group(),
                                              CString(groupEnd) + " >= ", p3, ") then"});
                }
                else if (!is(p1, "Any"))
                {
                    written = false;
                }

                // Original behaviour (EB-103): another ActionParm1 writes no condition, but its success and failure.
                if (written)
                {
                    script.Tab(CHECK_COLUMN).Line(line);
                }

                writeObjectiveSuccess(objective, action, 0);
                script.Tab(CHECK_COLUMN).Line("else");
                written = true;
                const CString deadOrFled = CString(") OR (NumCaptureDeadorFled(") + force + ") > 0)) then";

                if (is(p1, "Any"))
                {
                    if (noCount(p3))
                    {
                        line = timed ? cat({"if ((gametime > ", timeLeft, deadOrFled})
                                     : "if ((gettimeleft == 0.0" + deadOrFled;
                    }
                    else
                    {
                        line =
                            timed ? cat({"if ((gametime > ", timeLeft,
                                         ") OR (" + count + " - NumCaptureDeadorFled(" + force + ") < ", p3, ")) then"})
                                  : cat({"if ((gettimeleft == 0.0) OR (" + count + " - NumCaptureDeadorFled(" + force +
                                             ") < ",
                                         p3, ")) then"});
                    }
                }
                else if (is(p1, "All"))
                {
                    line = timed ? cat({"if ((gametime > ", timeLeft, deadOrFled})
                                 : "if ((gettimeleft == 0.0" + deadOrFled;
                }
                else if (is(p1, "Lance/Star"))
                {
                    if (noCount(p3))
                    {
                        line = timed ? cat({"if ((gametime > ", timeLeft,
                                            CString(") OR (NumCaptureDeadorFled(") + groupStart, group(),
                                            CString(groupEnd) + " > 0)) then"})
                                     : cat({CString("if ((gettimeleft == 0.0) OR (NumCaptureDeadorFled(") + groupStart,
                                            group(), CString(groupEnd) + " > 0)) then"});
                    }
                    else
                    {
                        line = timed ? cat({"if ((gametime > ", timeLeft,
                                            ") OR (" + count + " - NumCaptureDeadorFled(" + groupStart, group(),
                                            CString(groupEnd) + " < ", p3, ")) then"})
                                     : cat({"if ((gettimeleft == 0.0) OR (" + count + " - NumCaptureDeadorFled(" +
                                                groupStart,
                                            group(), CString(groupEnd) + " < ", p3, ")) then"});
                    }
                }
                else
                {
                    written = false;
                }

                if (written)
                {
                    script.Tab(INNER_COLUMN).Line(line);
                }

                writeObjectiveFailure(objective, action, 2);
                tail = Tail::EndIfs;
            }
        }
        else if (is(kind, "Set Objective Marker"))
        {
            const CString marker = cat({"setObjectivePos(" + objectiveText + ",", p1, ".0,", p2, ".0,0.0);"});
            const CString success = "Obj_" + objectiveText + "_Action_" + actionText + "_Success = TRUE;";

            if (is(p4, "Anytime"))
            {
                line = marker;
                addInitialization(line, "");
                line = success;
                addInitialization(line, "");
            }
            else
            {
                line = action != 1 ? "if ((Obj_" + objectiveText + "_Action_" + lastAction + "_Success) AND (NOT Obj_" +
                                         objectiveText + "_Action_" + actionText + "_Success)) then"
                                   : "if ((Objective_" + lastObjective + "_Decided) AND (NOT Obj_" + objectiveText +
                                         "_Action_" + actionText + "_Success)) then";
                script.Tab(CHECK_COLUMN).Line(line);

                if (Vb::Eq((*mission)["Multiplayer"], Vb::Boolean(true)))
                {
                    line = marker;
                    short messageCode = addMultiplayerEvent(line, true);
                    line = success;
                    messageCode = addMultiplayerEvent(line, false);
                    line = "sendMessage(" + Vb::CStr(messageCode) + ",0);";
                    script.Tab(INNER_COLUMN).Line(line);
                    script.Tab(INNER_COLUMN).Line("handlemessage;");
                }
                else
                {
                    line = marker;
                    script.Tab(INNER_COLUMN).Line(line);
                    line = success;
                    script.Tab(INNER_COLUMN).Line(line);
                }

                script.Tab(CHECK_COLUMN).Line("endif;");
            }
        }
        else if (is(kind, "Fail Secondary Objective"))
        {
            addInitialization("Obj_" + objectiveText + "_Action_" + actionText + "_Success = TRUE;", "");

            if (innerSphere)
            {
                script.Tab(CHECK_COLUMN).Text("if (VictoryLevel >= ");
                script.Value(Vb::Cat(innerSpherePrimaries, Vb::String(") then"))).EndLine();
            }
            else
            {
                script.Tab(CHECK_COLUMN).Text("if (ClanVictoryLevel >= ").Line(Vb::CStr(clanPrimaries) + ") then");
            }

            if (Vb::Eq((*mission)["Multiplayer"], Vb::Boolean(true)))
            {
                short messageCode = addMultiplayerEvent("SetObjectiveStatus(" + objectiveText + ",FAILED);", true);
                messageCode = addMultiplayerEvent("Objective_" + objectiveText + "_Decided = TRUE;", false);
                script.Line("sendMessage(" + Vb::CStr(messageCode) + ",0);");
                script.Line("handleMessage;");
            }
            else
            {
                line = "SetObjectiveStatus(" + objectiveText + ",FAILED);";
                script.Tab(INNER_COLUMN).Line(line);
                line = "Objective_" + objectiveText + "_Decided = TRUE;";
                script.Tab(INNER_COLUMN).Line(line);
            }

            script.Tab(CHECK_COLUMN).Line("endif;");
        }
        else if (is(kind, "ABL SUCCESS") || is(kind, "ABL SUCCESS/FAIL"))
        {
            // The ABL condition the designer wrote: ActionParm1 succeeds, ActionParm2 fails.
            if (is(p4, "Anytime"))
            {
                line = cat({"if (", p1, ") then"});
            }
            else if (afterLastAction)
            {
                line = action > 1 ? cat({"if ((Obj_" + objectiveText + "_Action_" + lastAction + "_Success) AND (", p1,
                                         ")) then"})
                                  : cat({"if ((Objective_" + lastObjective + "_Decided) AND (", p1, ")) then"});
            }

            script.Tab(CHECK_COLUMN).Line(line);
            writeObjectiveSuccess(objective, action, 0);
            script.Tab(CHECK_COLUMN).Line("else");

            if (is(kind, "ABL SUCCESS"))
            {
                line = timed ? cat({"if (gametime > ", timeLeft, ") then"}) : CString(NO_TIME_LIMIT_CHECK);
                script.Tab(INNER_COLUMN).Line(line);
                writeObjectiveFailure(objective, action, 2);
            }
            else if (is(p4, "Anytime"))
            {
                line = cat({"if (", p2, ") then"});
                script.Tab(INNER_COLUMN).Line(line);
                writeObjectiveFailure(objective, action, 2);
            }
            else if (afterLastAction)
            {
                line = action > 1 ? cat({"if ((Obj_" + objectiveText + "_Action_" + lastAction + "_Success) AND (NOT (",
                                         p2, "))) then"})
                                  : cat({"if ((Objective_" + lastObjective + "_Decided) AND (NOT (", p2, "))) then"});
                script.Tab(INNER_COLUMN).Line(line);
                writeObjectiveSuccess(objective, action, 2);
                script.Tab(INNER_COLUMN).Line("else");
                line = cat({"if (", p2, ") then"});
                script.Tab(INNER2_COLUMN).Line(line);
                writeObjectiveFailure(objective, action, 3);
                script.Tab(INNER2_COLUMN).Line("endif;");
            }

            tail = Tail::EndIfs;
        }
        else if (is(kind, "ABL FAIL"))
        {
            line = cat({"if (", p2, ") then"});
            script.Tab(CHECK_COLUMN).Line(line);
            writeObjectiveFailure(objective, action, 0);
            script.Tab(CHECK_COLUMN).Line("else");

            if (is(p4, "Anytime"))
            {
                line = timed ? cat({"if (gametime > ", timeLeft, ") then"}) : CString(NO_TIME_LIMIT_CHECK);
            }
            else if (afterLastAction)
            {
                line = action > 1 ? "if (Obj_" + objectiveText + "_Action_" + lastAction + "_Success) then"
                                  : "if (Objective_" + lastObjective + "_Decided) then";
            }

            script.Tab(INNER_COLUMN).Line(line);
            writeObjectiveSuccess(objective, action, 2);
            tail = Tail::EndIfs;
        }

        if (tail == Tail::EndIfs)
        {
            script.Tab(INNER_COLUMN).Line("endif;");
            script.Tab(CHECK_COLUMN).Line("endif;");
        }

        script.Line();
        actions->FindNext(filter);
    }

    writeActionsSuccess(actionCount, objective);
    return true;
}
