#include "stdafx.h"
#include "abl/ablsymt.h"
#include "abl/ablerr.h"
#include "abl/ablscan.h"
#include "lib/aerror.h"

MCSymTableNodePtr SymTableDisplay[MAX_NESTING_LEVEL];
int32_t Level;
MCBlockStore AblMemory;
MCTypePtr IntegerTypePtr;
MCTypePtr CharTypePtr;
MCTypePtr RealTypePtr;
MCTypePtr BooleanTypePtr;
MCType DummyType;
MCAblModule* LibrariesUsed[MAX_LIBRARIES_USED];
int32_t NumLibrariesUsed;

namespace
{
    /// <summary>A standard routine: its ABL name, key and whether it is a tactical order.</summary>
    struct MCStandardRoutine
    {
        const char* Name = nullptr;
        MCRoutineKey Key{};
        int IsOrder = 0;
    };

    /// <summary>The standard routines in initSymTable's order.</summary>
    const MCStandardRoutine StandardRoutines[] = {
        {"return", RTN_RETURN, 0},
        {"print", RTN_PRINT, 0},
        {"concat", RTN_CONCAT, 0},
        {"abs", RTN_ABS, 0},
        {"random", RTN_RANDOM, 0},
        {"round", RTN_ROUND, 0},
        {"sqrt", RTN_SQRT, 0},
        {"trunc", RTN_TRUNC, 0},
        {"getmodulehandle", RTN_GET_MODULE_HANDLE, 0},
        {"getmodulename", RTN_GET_MODULE_NAME, 0},
        {"setmodulename", RTN_SET_MODULE_NAME, 0},
        {"setmaxloops", RTN_SET_MAX_LOOPS, 0},
        {"fatal", RTN_FATAL, 0},
        {"assert", RTN_ASSERT, 0},
        {"getmode", RTN_GET_MODE, 0},
        {"getaction", RTN_GET_ACTION, 0},
        {"getphase", RTN_GET_PHASE, 0},
        {"getid", RTN_GET_ID, 0},
        {"gettime", RTN_GET_TIME, 0},
        {"gettimeleft", RTN_GET_TIME_LEFT, 0},
        {"selectobject", RTN_SELECT_OBJECT, 0},
        {"selectunit", RTN_SELECT_UNIT, 0},
        {"selectwarrior", RTN_SELECT_WARRIOR, 0},
        {"getwarriorstatus", RTN_GET_WARRIOR_STATUS, 0},
        {"getcontacts", RTN_GET_CONTACTS, 0},
        {"getenemycount", RTN_GET_ENEMY_COUNT, 0},
        {"selectcontact", RTN_SELECT_CONTACT, 0},
        {"getcontactid", RTN_GET_CONTACT_ID, 0},
        {"iscontact", RTN_IS_CONTACT, 0},
        {"getcontactstatus", RTN_GET_CONTACT_STATUS, 0},
        {"getcontactrelativeposition", RTN_GET_CONTACT_RELATIVE_POSITION, 0},
        {"setguardobjective", RTN_SET_GUARD_OBJECTIVE, 0},
        {"setguardpoint", RTN_SET_GUARD_POINT, 0},
        {"setguardradii", RTN_SET_GUARD_RADII, 0},
        {"getguardobjective", RTN_GET_GUARD_OBJECTIVE, 0},
        {"getguardpoint", RTN_GET_GUARD_POINT, 0},
        {"getguardradii", RTN_GET_GUARD_RADII, 0},
        {"getguarddistanceto", RTN_GET_GUARD_DISTANCE_TO, 0},
        {"gettarget", RTN_GET_TARGET, 0},
        {"settarget", RTN_SET_TARGET, 0},
        {"getweaponsready", RTN_GET_WEAPONS_READY, 0},
        {"getweaponslocked", RTN_GET_WEAPONS_LOCKED, 0},
        {"getweaponsinrange", RTN_GET_WEAPONS_IN_RANGE, 0},
        {"getweaponshots", RTN_GET_WEAPON_SHOTS, 0},
        {"getweaponranges", RTN_GET_WEAPON_RANGES, 0},
        {"getobjectposition", RTN_GET_OBJECT_POSITION, 0},
        {"getintegermemory", RTN_GET_INTEGER_MEMORY, 0},
        {"getrealmemory", RTN_GET_REAL_MEMORY, 0},
        {"getalarmtriggers", RTN_GET_ALARM_TRIGGERS, 0},
        {"getchallenger", RTN_GET_CHALLENGER, 0},
        {"gettimewithoutorders", RTN_GET_TIME_WITHOUT_ORDERS, 0},
        {"getfireranges", RTN_GET_FIRE_RANGES, 0},
        {"getattackers", RTN_GET_ATTACKERS, 0},
        {"getattackerinfo", RTN_GET_ATTACKER_INFO, 0},
        {"setchallenger", RTN_SET_CHALLENGER, 0},
        {"setmode", RTN_SET_MODE, 0},
        {"setaction", RTN_SET_ACTION, 0},
        {"setphase", RTN_SET_PHASE, 0},
        {"setmovegoal", RTN_SET_MOVE_GOAL, 0},
        {"setupdatetime", RTN_SET_UPDATE_TIME, 0},
        {"setintegermemory", RTN_SET_INTEGER_MEMORY, 0},
        {"setrealmemory", RTN_SET_REAL_MEMORY, 0},
        {"startfieldscan", RTN_START_FIELD_SCAN, 0},
        {"startvehiclescan", RTN_START_VEHICLE_SCAN, 0},
        {"startenemyscan", RTN_START_ENEMY_SCAN, 0},
        {"startfriendlyscan", RTN_START_FRIENDLY_SCAN, 0},
        {"startmovepath", RTN_START_MOVE_PATH, 0},
        {"hasmovegoal", RTN_HAS_MOVE_GOAL, 0},
        {"hasmovepath", RTN_HAS_MOVE_PATH, 0},
        {"sortweapons", RTN_SORT_WEAPONS, 0},
        {"timetoimpact", RTN_TIME_TO_IMPACT, 0},
        {"fireweapon", RTN_FIRE_WEAPON, 0},
        {"getvisualrange", RTN_GET_VISUAL_RANGE, 0},
        {"getunitmates", RTN_GET_UNIT_MATES, 0},
        {"gettacorder", RTN_GET_TAC_ORDER, 0},
        {"getlasttacorder", RTN_GET_LAST_TAC_ORDER, 0},
        {"setordermode", RTN_SET_ORDER_MODE, 1},
        {"orderwait", RTN_ORDER_WAIT, 1},
        {"ordermoveto", RTN_ORDER_MOVE_TO, 1},
        {"ordermovetoobject", RTN_ORDER_MOVE_TO_OBJECT, 1},
        {"ordermovetocontact", RTN_ORDER_MOVE_TO_CONTACT, 1},
        {"ordertraversepath", RTN_ORDER_TRAVERSE_PATH, 1},
        {"orderpatrolpath", RTN_ORDER_PATROL_PATH, 1},
        {"orderpowerdown", RTN_ORDER_POWER_DOWN, 1},
        {"orderpowerup", RTN_ORDER_POWER_UP, 1},
        {"orderattackobject", RTN_ORDER_ATTACK_OBJECT, 1},
        {"orderattackcontact", RTN_ORDER_ATTACK_CONTACT, 1},
        {"attackthreat", RTN_ATTACK_THREAT, 1},
        {"attackclosesttarget", RTN_ATTACK_CLOSEST_TARGET, 1},
        {"attackperorders", RTN_ATTACK_PER_ORDERS, 1},
        {"orderwithdraw", RTN_ORDER_WITHDRAW, 1},
        {"retreat", RTN_RETREAT, 1},
        {"openfire", RTN_OPEN_FIRE, 1},
        {"fireuponenemyfireonly", RTN_FIRE_UPON_ENEMY_FIRE_ONLY, 1},
        {"damageobject", RTN_DAMAGE_OBJECT, 1},
        {"setattackradius", RTN_SET_ATTACK_RADIUS, 1},
        {"ordertest", RTN_ORDER_TEST, 1},
        {"playsmacker", RTN_PLAY_SMACKER, 0},
        {"fileexists", RTN_FILE_EXISTS, 0},
        {"objectchangesides", RTN_OBJECT_CHANGE_SIDES, 0},
        {"distancetoobject", RTN_DISTANCE_TO_OBJECT, 0},
        {"distancetoposition", RTN_DISTANCE_TO_POSITION, 0},
        {"objectsuicide", RTN_OBJECT_SUICIDE, 0},
        {"objectcreate", RTN_OBJECT_CREATE, 0},
        {"objectexists", RTN_OBJECT_EXISTS, 0},
        {"objectstatus", RTN_OBJECT_STATUS, 0},
        {"objectstatuscount", RTN_OBJECT_STATUS_COUNT, 0},
        {"objectvisible", RTN_OBJECT_VISIBLE, 0},
        {"objectside", RTN_OBJECT_SIDE, 0},
        {"objectcommander", RTN_OBJECT_COMMANDER, 0},
        {"objectclass", RTN_OBJECT_CLASS, 0},
        {"settimer", RTN_SET_TIMER, 0},
        {"checktimer", RTN_CHECK_TIMER, 0},
        {"endtimer", RTN_END_TIMER, 0},
        {"setobjectivetimer", RTN_SET_OBJECTIVE_TIMER, 0},
        {"checkobjectivetimer", RTN_CHECK_OBJECTIVE_TIMER, 0},
        {"setobjectivestatus", RTN_SET_OBJECTIVE_STATUS, 0},
        {"checkobjectivestatus", RTN_CHECK_OBJECTIVE_STATUS, 0},
        {"setobjectivetype", RTN_SET_OBJECTIVE_TYPE, 0},
        {"checkobjectivetype", RTN_CHECK_OBJECTIVE_TYPE, 0},
        {"playdigitalmusic", RTN_PLAY_DIGITAL_MUSIC, 0},
        {"stopmusic", RTN_STOP_MUSIC, 0},
        {"playsoundeffect", RTN_PLAY_SOUND_EFFECT, 0},
        {"playvideo", RTN_PLAY_VIDEO, 0},
        {"setradio", RTN_SET_RADIO, 0},
        {"playspeech", RTN_PLAY_SPEECH, 0},
        {"playbetty", RTN_PLAY_BETTY, 0},
        {"setobjectactive", RTN_SET_OBJECT_ACTIVE, 0},
        {"objectinwithdrawal", RTN_OBJECT_IN_WITHDRAWAL, 0},
        {"objecttypeid", RTN_OBJECT_TYPE_ID, 0},
        {"getterrainobjectpartid", RTN_GET_TERRAIN_OBJECT_PART_ID, 0},
        {"getvehiclepartid", RTN_GET_VEHICLE_PART_ID, 0},
        {"getweaponammo", RTN_GET_WEAPON_AMMO, 0},
        {"inarea", RTN_IN_AREA, 0},
        {"getsensorsworking", RTN_GET_SENSORS_WORKING, 0},
        {"getcurrentbrvalue", RTN_GET_CURRENT_BR_VALUE, 0},
        {"setcurrentbrvalue", RTN_SET_CURRENT_BR_VALUE, 0},
        {"getarmorpts", RTN_GET_ARMOR_PTS, 0},
        {"getmaxarmor", RTN_GET_MAX_ARMOR, 0},
        {"getpilotid", RTN_GET_PILOT_ID, 0},
        {"getpilotwounds", RTN_GET_PILOT_WOUNDS, 0},
        // Original behaviour: registered with the get key (see RTN_GET_PILOT_WOUNDS).
        {"setpilotwounds", RTN_GET_PILOT_WOUNDS, 0},
        {"getobjectactive", RTN_GET_OBJECT_ACTIVE, 0},
        {"getobjectdamage", RTN_GET_OBJECT_DAMAGE, 0},
        {"setobjectdamage", RTN_SET_OBJECT_DAMAGE, 0},
        {"getobjectdmgpts", RTN_GET_OBJECT_DMG_PTS, 0},
        {"getobjectmaxdmg", RTN_GET_OBJECT_MAX_DMG, 0},
        {"getglobalvalue", RTN_GET_GLOBAL_VALUE, 0},
        {"setglobalvalue", RTN_SET_GLOBAL_VALUE, 0},
        {"setobjectivepos", RTN_SET_OBJECTIVE_POS, 0},
        {"setmoverbehavior", RTN_SET_MOVER_BEHAVIOR, 0},
        {"setmoveroverlayweight", RTN_SET_MOVER_OVERLAY_WEIGHT, 0},
        {"setpotentialcontact", RTN_SET_POTENTIAL_CONTACT, 0},
        {"setsensorrange", RTN_SET_SENSOR_RANGE, 0},
        {"settonnage", RTN_SET_TONNAGE, 0},
        {"playwavefile", RTN_PLAY_WAVE_FILE, 0},
        {"setexplosiondamage", RTN_SET_EXPLOSION_DAMAGE, 0},
        {"setexplosionradius", RTN_SET_EXPLOSION_RADIUS, 0},
        {"setsalvage", RTN_SET_SALVAGE, 0},
        {"setsalvagestatus", RTN_SET_SALVAGE_STATUS, 0},
        {"setanimation", RTN_SET_ANIMATION, 0},
        {"setrevealed", RTN_SET_REVEALED, 0},
        {"getsalvage", RTN_GET_SALVAGE, 0},
        {"orderrefit", RTN_ORDER_REFIT, 0},
        {"setcaptured", RTN_SET_CAPTURED, 0},
        {"setcaptureable", RTN_SET_CAPTUREABLE, 0},
        {"ordercapture", RTN_ORDER_CAPTURE, 0},
        {"iscaptured", RTN_IS_CAPTURED, 0},
        {"iscapturable", RTN_IS_CAPTURABLE, 0},
        {"wasevercapturable", RTN_WAS_EVER_CAPTURABLE, 0},
        {"setbuildingname", RTN_SET_BUILDING_NAME, 0},
        {"callstrike", RTN_CALL_STRIKE, 0},
        {"orderloadelementals", RTN_ORDER_LOAD_ELEMENTALS, 0},
        {"orderdeployelementals", RTN_ORDER_DEPLOY_ELEMENTALS, 0},
        {"addprisoner", RTN_ADD_PRISONER, 0},
        {"settrainspeed", RTN_SET_TRAIN_SPEED, 0},
        {"lockgateopen", RTN_LOCK_GATE_OPEN, 0},
        {"lockgateclosed", RTN_LOCK_GATE_CLOSED, 0},
        {"releasegatelock", RTN_RELEASE_GATE_LOCK, 0},
        {"isgateopen", RTN_IS_GATE_OPEN, 0},
        {"callstrikeex", RTN_CALL_STRIKE_EX, 0},
        {"getrelativepositiontopoint", RTN_GET_RELATIVE_POSITION_TO_POINT, 0},
        {"getrelativepositiontoobject", RTN_GET_RELATIVE_POSITION_TO_OBJECT, 0},
        {"getunitstatus", RTN_GET_UNIT_STATUS, 0},
        {"repair", RTN_REPAIR, 0},
        {"getfixed", RTN_GET_FIXED, 0},
        {"getrepairstate", RTN_GET_REPAIR_STATE, 0},
        {"isteamtargeting", RTN_IS_TEAM_TARGETING, 0},
        {"sendmessage", RTN_SEND_MESSAGE, 0},
        {"getmessage", RTN_GET_MESSAGE, 0},
        {"gethometeam", RTN_GET_HOME_TEAM, 0},
        {"getstrikes", RTN_GET_STRIKES, 0},
        {"setstrikes", RTN_SET_STRIKES, 0},
        {"addstrikes", RTN_ADD_STRIKES, 0},
        {"isserver", RTN_IS_SERVER, 0},
    };
}

auto SearchLocalSymTable(MCSymTableNodePtr& idPtr) -> void
{
    idPtr = SearchSymTable(WordString, SymTableDisplay[Level]);
}

auto SearchAllSymTables(MCSymTableNodePtr& idPtr) -> void
{
    idPtr = SearchSymTableDisplay(WordString);
}

auto EnterLocalSymTable(MCSymTableNodePtr& idPtr) -> void
{
    idPtr = EnterSymTable(WordString, &SymTableDisplay[Level]);
}

auto SearchAndFindAllSymTables(MCSymTableNodePtr& idPtr) -> void
{
    idPtr = SearchSymTableDisplay(WordString);

    if (idPtr == nullptr)
    {
        SyntaxError(ABL_ERR_SYNTAX_UNDEFINED_IDENTIFIER);
        idPtr = EnterSymTable(WordString, &SymTableDisplay[Level]);
        idPtr->Defn.Key = DFN_UNDEFINED;
        idPtr->TypePtr = &DummyType;
    }
}

auto SearchAndEnterLocalSymTable(MCSymTableNodePtr& idPtr) -> void
{
    idPtr = SearchSymTable(WordString, SymTableDisplay[Level]);

    if (idPtr == nullptr)
    {
        idPtr = EnterSymTable(WordString, &SymTableDisplay[Level]);
    }
    else
    {
        SyntaxError(ABL_ERR_SYNTAX_REDEFINED_IDENTIFIER);
    }
}

auto SearchAndEnterThisTable(MCSymTableNodePtr& idPtr, MCSymTableNodePtr root) -> void
{
    idPtr = SearchSymTable(WordString, root);

    if (idPtr == nullptr)
    {
        // Original behaviour: entered through the local copy of root, so into an empty tree the new node is lost.
        idPtr = EnterSymTable(WordString, &root);
    }
    else
    {
        SyntaxError(ABL_ERR_SYNTAX_REDEFINED_IDENTIFIER);
    }
}

auto CreateType() -> MCTypePtr
{
    // The original set only numInstances, form, size and typeIdPtr; the port clears the whole record.
    MCTypePtr type = AblMemory.Make<MCType>();
    type->NumInstances = 1;
    type->Form = FRM_NONE;
    type->Size = 0;
    type->TypeIdPtr = nullptr;
    return type;
}

auto SetType(MCTypePtr type) -> MCTypePtr
{
    if (type != nullptr)
    {
        type->NumInstances++;
    }

    return type;
}

auto ClearType(MCTypePtr& type) -> void
{
    if (type != nullptr && --type->NumInstances == 0)
    {
        AblMemory.Free(type);
        type = nullptr;
    }
}

auto RecordLibraryUsed(MCSymTableNodePtr idPtr) -> void
{
    MCAblModule* library = idPtr->Library;

    for (int32_t i = 0; i < NumLibrariesUsed; i++)
    {
        if (LibrariesUsed[i] == library)
        {
            return;
        }
    }

    if (NumLibrariesUsed >= MAX_LIBRARIES_USED)
    {
        Fatal(0, " ABL: Too many libraries referenced from module ");
    }

    LibrariesUsed[NumLibrariesUsed++] = library;
}

auto SearchSymTable(char* name, MCSymTableNodePtr nodePtr) -> MCSymTableNodePtr
{
    while (nodePtr != nullptr)
    {
        int compareResult = strcmp(name, nodePtr->Name);

        if (compareResult == 0)
        {
            return nodePtr;
        }

        nodePtr = compareResult < 0 ? nodePtr->Left : nodePtr->Right;
    }

    return nullptr;
}

auto SearchLibrarySymTable(char* name, MCSymTableNodePtr nodePtr) -> MCSymTableNodePtr
{
    if (nodePtr == nullptr)
    {
        return nullptr;
    }

    if (strcmp(name, nodePtr->Name) == 0)
    {
        return nodePtr;
    }

    if (nodePtr->Library != nullptr && nodePtr->Defn.Key == DFN_MODULE)
    {
        MCSymTableNodePtr found = SearchSymTable(name, nodePtr->Defn.Info.Routine.LocalSymTable);

        if (found != nullptr)
        {
            return found;
        }
    }

    MCSymTableNodePtr found = SearchLibrarySymTable(name, nodePtr->Left);

    if (found != nullptr)
    {
        return found;
    }

    return SearchLibrarySymTable(name, nodePtr->Right);
}

auto SearchLibrarySymTableDisplay(char* name) -> MCSymTableNodePtr
{
    return SearchLibrarySymTable(name, SymTableDisplay[0]);
}

auto SearchSymTableDisplay(char* name) -> MCSymTableNodePtr
{
    char* separator = strchr(name, '.');

    if (separator == nullptr)
    {
        for (int32_t i = Level; i >= 0; i--)
        {
            MCSymTableNodePtr found = SearchSymTable(name, SymTableDisplay[i]);

            if (found != nullptr)
            {
                return found;
            }
        }

        MCSymTableNodePtr found = SearchLibrarySymTableDisplay(name);

        if (found != nullptr)
        {
            RecordLibraryUsed(found);
        }

        return found;
    }

    // library.name: the name is split in place (the '.' stays a NUL).
    *separator = '\0';
    MCSymTableNodePtr libraryIdPtr = SearchSymTable(name, SymTableDisplay[0]);

    if (libraryIdPtr == nullptr)
    {
        return nullptr;
    }

    MCSymTableNodePtr found = SearchSymTable(separator + 1, libraryIdPtr->Defn.Info.Routine.LocalSymTable);

    if (found == nullptr)
    {
        return nullptr;
    }

    RecordLibraryUsed(found);
    return found;
}

auto EnterSymTable(char* name, MCSymTableNodePtr* ptrToNodePtr) -> MCSymTableNodePtr
{
    // The original cleared the links, info, defn.key, the first two words of defn.info, typePtr and labelIndex,
    // leaving the rest as the heap had it; the port clears the whole node.
    MCSymTableNodePtr newNode = AblMemory.Make<MCSymTableNode>();
    newNode->Name = AblMemory.CopyString(name);
    newNode->Level = Level;

    MCSymTableNodePtr parent = nullptr;

    while (*ptrToNodePtr != nullptr)
    {
        parent = *ptrToNodePtr;
        ptrToNodePtr = strcmp(name, parent->Name) < 0 ? &parent->Left : &parent->Right;
    }

    *ptrToNodePtr = newNode;
    newNode->Parent = parent;
    return newNode;
}

auto InsertSymTable(MCSymTableNodePtr* tableRoot, MCSymTableNodePtr newNode) -> MCSymTableNodePtr
{
    newNode->Left = nullptr;
    newNode->Parent = nullptr;
    newNode->Right = nullptr;
    MCSymTableNodePtr parent = nullptr;

    while (*tableRoot != nullptr)
    {
        parent = *tableRoot;
        tableRoot = strcmp(newNode->Name, parent->Name) < 0 ? &parent->Left : &parent->Right;
    }

    newNode->Parent = parent;
    *tableRoot = newNode;
    return newNode;
}

auto ExtractSymTable(MCSymTableNodePtr* tableRoot, MCSymTableNodePtr nodeKill) -> MCSymTableNodePtr
{
    // The node to unlink: nodeKill itself, or with two children a stand-in whose contents move into nodeKill.
    MCSymTableNodePtr y = nodeKill;

    if (nodeKill->Left != nullptr && nodeKill->Right != nullptr)
    {
        // Original behaviour (OB-038): the stand-in should be the in-order successor (leftmost of the right
        // subtree); the original takes the leftmost of the LEFT subtree, which breaks the tree's order.
        y = nodeKill->Left;

        while (y->Left != nullptr)
        {
            y = y->Left;
        }
    }

    MCSymTableNodePtr x = y->Left != nullptr ? y->Left : y->Right;

    if (x != nullptr)
    {
        x->Parent = y->Parent;
    }

    if (y->Parent == nullptr)
    {
        *tableRoot = x;
    }
    else if (y == y->Parent->Left)
    {
        y->Parent->Left = x;
    }
    else
    {
        y->Parent->Right = x;
    }

    if (y != nodeKill)
    {
        // Everything but the links and the library.
        nodeKill->Next = y->Next;
        nodeKill->Name = y->Name;
        nodeKill->Info = y->Info;
        nodeKill->Defn = y->Defn;
        nodeKill->TypePtr = y->TypePtr;
        nodeKill->Level = y->Level;
        nodeKill->LabelIndex = y->LabelIndex;
    }

    return y;
}

auto EnterStandardRoutine(char* name, MCRoutineKey routineKey, MCDefinitionType definitionType, int) -> void
{
    MCSymTableNodePtr routineIdPtr = EnterSymTable(name, &SymTableDisplay[Level]);
    routineIdPtr->Defn.Key = definitionType;
    routineIdPtr->Defn.Info.Routine.Key = routineKey;
    routineIdPtr->Defn.Info.Routine.Params = nullptr;
    routineIdPtr->Defn.Info.Routine.LocalSymTable = nullptr;
    routineIdPtr->Library = nullptr;
    routineIdPtr->TypePtr = nullptr;
}

auto EnterScope(MCSymTableNodePtr symTableRoot) -> void
{
    if (++Level >= MAX_NESTING_LEVEL)
    {
        SyntaxError(ABL_ERR_SYNTAX_NESTING_TOO_DEEP);
        exit(-ABL_ERR_SYNTAX_NESTING_TOO_DEEP);
    }

    SymTableDisplay[Level] = symTableRoot;
}

auto ExitScope() -> MCSymTableNodePtr
{
    return SymTableDisplay[Level--];
}

auto InitSymTable() -> void
{
    SymTableDisplay[0] = nullptr;

    MCSymTableNodePtr integerIdPtr = EnterSymTable(const_cast<char*>("integer"), &SymTableDisplay[Level]);
    MCSymTableNodePtr charIdPtr = EnterSymTable(const_cast<char*>("char"), &SymTableDisplay[Level]);
    MCSymTableNodePtr realIdPtr = EnterSymTable(const_cast<char*>("real"), &SymTableDisplay[Level]);
    MCSymTableNodePtr booleanIdPtr = EnterSymTable(const_cast<char*>("boolean"), &SymTableDisplay[Level]);
    MCSymTableNodePtr falseIdPtr = EnterSymTable(const_cast<char*>("false"), &SymTableDisplay[Level]);
    MCSymTableNodePtr trueIdPtr = EnterSymTable(const_cast<char*>("true"), &SymTableDisplay[Level]);

    IntegerTypePtr = CreateType();
    CharTypePtr = CreateType();
    RealTypePtr = CreateType();
    BooleanTypePtr = CreateType();

    integerIdPtr->Defn.Key = DFN_TYPE;
    integerIdPtr->TypePtr = IntegerTypePtr;
    IntegerTypePtr->Form = FRM_SCALAR;
    IntegerTypePtr->Size = 4;
    IntegerTypePtr->TypeIdPtr = integerIdPtr;

    charIdPtr->Defn.Key = DFN_TYPE;
    charIdPtr->TypePtr = CharTypePtr;
    CharTypePtr->Form = FRM_SCALAR;
    CharTypePtr->Size = 1;
    CharTypePtr->TypeIdPtr = charIdPtr;

    realIdPtr->Defn.Key = DFN_TYPE;
    realIdPtr->TypePtr = RealTypePtr;
    RealTypePtr->Form = FRM_SCALAR;
    RealTypePtr->Size = 4;
    RealTypePtr->TypeIdPtr = realIdPtr;

    booleanIdPtr->Defn.Key = DFN_TYPE;
    booleanIdPtr->TypePtr = BooleanTypePtr;
    BooleanTypePtr->Form = FRM_ENUM;
    BooleanTypePtr->Size = 4;
    BooleanTypePtr->TypeIdPtr = booleanIdPtr;
    BooleanTypePtr->Info.Enumeration.Max = 1;
    BooleanTypePtr->Info.Enumeration.ConstIdPtr = falseIdPtr;

    falseIdPtr->Defn.Key = DFN_CONST;
    falseIdPtr->Defn.Info.Constant.Value.Integer = 0;
    falseIdPtr->TypePtr = BooleanTypePtr;
    falseIdPtr->Next = trueIdPtr;

    trueIdPtr->Defn.Key = DFN_CONST;
    trueIdPtr->Defn.Info.Constant.Value.Integer = 1;
    trueIdPtr->TypePtr = BooleanTypePtr;

    for (const MCStandardRoutine& routine : StandardRoutines)
    {
        EnterStandardRoutine(const_cast<char*>(routine.Name), routine.Key, DFN_FUNCTION, routine.IsOrder);
    }
}

auto FreeSymTable(MCSymTableNodePtr) -> void
{
}
