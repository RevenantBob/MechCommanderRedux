#include "stdafx.h"
#include "abl/ablsymt.h"
#include "abl/ablerr.h"
#include "abl/ablscan.h"
#include "lib/aerror.h"

SymTableNodePtr SymTableDisplay[MAX_NESTING_LEVEL];
int32_t level;
MCBlockStore AblMemory;
TypePtr IntegerTypePtr;
TypePtr CharTypePtr;
TypePtr RealTypePtr;
TypePtr BooleanTypePtr;
_Type DummyType;
ABLModule* LibrariesUsed[MAX_LIBRARIES_USED];
int32_t NumLibrariesUsed;

namespace
{
    /// <summary>A standard routine: its ABL name, key and whether it is a tactical order.</summary>
    struct StandardRoutine
    {
        const char* name = nullptr;
        RoutineKey key{};
        int isOrder = 0;
    };

    /// <summary>The standard routines in initSymTable's order.</summary>
    const StandardRoutine StandardRoutines[] = {
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

auto searchLocalSymTable(SymTableNodePtr& idPtr) -> void
{
    idPtr = searchSymTable(wordString, SymTableDisplay[level]);
}

auto searchAllSymTables(SymTableNodePtr& idPtr) -> void
{
    idPtr = searchSymTableDisplay(wordString);
}

auto enterLocalSymTable(SymTableNodePtr& idPtr) -> void
{
    idPtr = enterSymTable(wordString, &SymTableDisplay[level]);
}

auto searchAndFindAllSymTables(SymTableNodePtr& idPtr) -> void
{
    idPtr = searchSymTableDisplay(wordString);

    if (idPtr == nullptr)
    {
        syntaxError(ABL_ERR_SYNTAX_UNDEFINED_IDENTIFIER);
        idPtr = enterSymTable(wordString, &SymTableDisplay[level]);
        idPtr->defn.key = DFN_UNDEFINED;
        idPtr->typePtr = &DummyType;
    }
}

auto searchAndEnterLocalSymTable(SymTableNodePtr& idPtr) -> void
{
    idPtr = searchSymTable(wordString, SymTableDisplay[level]);

    if (idPtr == nullptr)
    {
        idPtr = enterSymTable(wordString, &SymTableDisplay[level]);
    }
    else
    {
        syntaxError(ABL_ERR_SYNTAX_REDEFINED_IDENTIFIER);
    }
}

auto searchAndEnterThisTable(SymTableNodePtr& idPtr, SymTableNodePtr root) -> void
{
    idPtr = searchSymTable(wordString, root);

    if (idPtr == nullptr)
    {
        // Original behaviour: entered through the local copy of root, so into an empty tree the new node is lost.
        idPtr = enterSymTable(wordString, &root);
    }
    else
    {
        syntaxError(ABL_ERR_SYNTAX_REDEFINED_IDENTIFIER);
    }
}

auto createType() -> TypePtr
{
    // The original set only numInstances, form, size and typeIdPtr; the port clears the whole record.
    TypePtr type = AblMemory.Make<_Type>();
    type->numInstances = 1;
    type->form = FRM_NONE;
    type->size = 0;
    type->typeIdPtr = nullptr;
    return type;
}

auto setType(TypePtr type) -> TypePtr
{
    if (type != nullptr)
    {
        type->numInstances++;
    }

    return type;
}

auto clearType(TypePtr& type) -> void
{
    if (type != nullptr && --type->numInstances == 0)
    {
        AblMemory.Free(type);
        type = nullptr;
    }
}

auto recordLibraryUsed(SymTableNodePtr idPtr) -> void
{
    ABLModule* library = idPtr->library;

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

auto searchSymTable(char* name, SymTableNodePtr nodePtr) -> SymTableNodePtr
{
    while (nodePtr != nullptr)
    {
        int compareResult = strcmp(name, nodePtr->name);

        if (compareResult == 0)
        {
            return nodePtr;
        }

        nodePtr = compareResult < 0 ? nodePtr->left : nodePtr->right;
    }

    return nullptr;
}

auto searchLibrarySymTable(char* name, SymTableNodePtr nodePtr) -> SymTableNodePtr
{
    if (nodePtr == nullptr)
    {
        return nullptr;
    }

    if (strcmp(name, nodePtr->name) == 0)
    {
        return nodePtr;
    }

    if (nodePtr->library != nullptr && nodePtr->defn.key == DFN_MODULE)
    {
        SymTableNodePtr found = searchSymTable(name, nodePtr->defn.info.routine.localSymTable);

        if (found != nullptr)
        {
            return found;
        }
    }

    SymTableNodePtr found = searchLibrarySymTable(name, nodePtr->left);

    if (found != nullptr)
    {
        return found;
    }

    return searchLibrarySymTable(name, nodePtr->right);
}

auto searchLibrarySymTableDisplay(char* name) -> SymTableNodePtr
{
    return searchLibrarySymTable(name, SymTableDisplay[0]);
}

auto searchSymTableDisplay(char* name) -> SymTableNodePtr
{
    char* separator = strchr(name, '.');

    if (separator == nullptr)
    {
        for (int32_t i = level; i >= 0; i--)
        {
            SymTableNodePtr found = searchSymTable(name, SymTableDisplay[i]);

            if (found != nullptr)
            {
                return found;
            }
        }

        SymTableNodePtr found = searchLibrarySymTableDisplay(name);

        if (found != nullptr)
        {
            recordLibraryUsed(found);
        }

        return found;
    }

    // library.name: the name is split in place (the '.' stays a NUL).
    *separator = '\0';
    SymTableNodePtr libraryIdPtr = searchSymTable(name, SymTableDisplay[0]);

    if (libraryIdPtr == nullptr)
    {
        return nullptr;
    }

    SymTableNodePtr found = searchSymTable(separator + 1, libraryIdPtr->defn.info.routine.localSymTable);

    if (found == nullptr)
    {
        return nullptr;
    }

    recordLibraryUsed(found);
    return found;
}

auto enterSymTable(char* name, SymTableNodePtr* ptrToNodePtr) -> SymTableNodePtr
{
    // The original cleared the links, info, defn.key, the first two words of defn.info, typePtr and labelIndex,
    // leaving the rest as the heap had it; the port clears the whole node.
    SymTableNodePtr newNode = AblMemory.Make<_SymTableNode>();
    newNode->name = AblMemory.CopyString(name);
    newNode->level = level;

    SymTableNodePtr parent = nullptr;

    while (*ptrToNodePtr != nullptr)
    {
        parent = *ptrToNodePtr;
        ptrToNodePtr = strcmp(name, parent->name) < 0 ? &parent->left : &parent->right;
    }

    *ptrToNodePtr = newNode;
    newNode->parent = parent;
    return newNode;
}

auto insertSymTable(SymTableNodePtr* tableRoot, SymTableNodePtr newNode) -> SymTableNodePtr
{
    newNode->left = nullptr;
    newNode->parent = nullptr;
    newNode->right = nullptr;
    SymTableNodePtr parent = nullptr;

    while (*tableRoot != nullptr)
    {
        parent = *tableRoot;
        tableRoot = strcmp(newNode->name, parent->name) < 0 ? &parent->left : &parent->right;
    }

    newNode->parent = parent;
    *tableRoot = newNode;
    return newNode;
}

auto extractSymTable(SymTableNodePtr* tableRoot, SymTableNodePtr nodeKill) -> SymTableNodePtr
{
    // The node to unlink: nodeKill itself, or with two children a stand-in whose contents move into nodeKill.
    SymTableNodePtr y = nodeKill;

    if (nodeKill->left != nullptr && nodeKill->right != nullptr)
    {
        // Original behaviour (OB-038): the stand-in should be the in-order successor (leftmost of the right
        // subtree); the original takes the leftmost of the LEFT subtree, which breaks the tree's order.
        y = nodeKill->left;

        while (y->left != nullptr)
        {
            y = y->left;
        }
    }

    SymTableNodePtr x = y->left != nullptr ? y->left : y->right;

    if (x != nullptr)
    {
        x->parent = y->parent;
    }

    if (y->parent == nullptr)
    {
        *tableRoot = x;
    }
    else if (y == y->parent->left)
    {
        y->parent->left = x;
    }
    else
    {
        y->parent->right = x;
    }

    if (y != nodeKill)
    {
        // Everything but the links and the library.
        nodeKill->next = y->next;
        nodeKill->name = y->name;
        nodeKill->info = y->info;
        nodeKill->defn = y->defn;
        nodeKill->typePtr = y->typePtr;
        nodeKill->level = y->level;
        nodeKill->labelIndex = y->labelIndex;
    }

    return y;
}

auto enterStandardRoutine(char* name, RoutineKey routineKey, DefinitionType definitionType, int) -> void
{
    SymTableNodePtr routineIdPtr = enterSymTable(name, &SymTableDisplay[level]);
    routineIdPtr->defn.key = definitionType;
    routineIdPtr->defn.info.routine.key = routineKey;
    routineIdPtr->defn.info.routine.params = nullptr;
    routineIdPtr->defn.info.routine.localSymTable = nullptr;
    routineIdPtr->library = nullptr;
    routineIdPtr->typePtr = nullptr;
}

auto enterScope(SymTableNodePtr symTableRoot) -> void
{
    if (++level >= MAX_NESTING_LEVEL)
    {
        syntaxError(ABL_ERR_SYNTAX_NESTING_TOO_DEEP);
        exit(-ABL_ERR_SYNTAX_NESTING_TOO_DEEP);
    }

    SymTableDisplay[level] = symTableRoot;
}

auto exitScope() -> SymTableNodePtr
{
    return SymTableDisplay[level--];
}

auto initSymTable() -> void
{
    SymTableDisplay[0] = nullptr;

    SymTableNodePtr integerIdPtr = enterSymTable(const_cast<char*>("integer"), &SymTableDisplay[level]);
    SymTableNodePtr charIdPtr = enterSymTable(const_cast<char*>("char"), &SymTableDisplay[level]);
    SymTableNodePtr realIdPtr = enterSymTable(const_cast<char*>("real"), &SymTableDisplay[level]);
    SymTableNodePtr booleanIdPtr = enterSymTable(const_cast<char*>("boolean"), &SymTableDisplay[level]);
    SymTableNodePtr falseIdPtr = enterSymTable(const_cast<char*>("false"), &SymTableDisplay[level]);
    SymTableNodePtr trueIdPtr = enterSymTable(const_cast<char*>("true"), &SymTableDisplay[level]);

    IntegerTypePtr = createType();
    CharTypePtr = createType();
    RealTypePtr = createType();
    BooleanTypePtr = createType();

    integerIdPtr->defn.key = DFN_TYPE;
    integerIdPtr->typePtr = IntegerTypePtr;
    IntegerTypePtr->form = FRM_SCALAR;
    IntegerTypePtr->size = 4;
    IntegerTypePtr->typeIdPtr = integerIdPtr;

    charIdPtr->defn.key = DFN_TYPE;
    charIdPtr->typePtr = CharTypePtr;
    CharTypePtr->form = FRM_SCALAR;
    CharTypePtr->size = 1;
    CharTypePtr->typeIdPtr = charIdPtr;

    realIdPtr->defn.key = DFN_TYPE;
    realIdPtr->typePtr = RealTypePtr;
    RealTypePtr->form = FRM_SCALAR;
    RealTypePtr->size = 4;
    RealTypePtr->typeIdPtr = realIdPtr;

    booleanIdPtr->defn.key = DFN_TYPE;
    booleanIdPtr->typePtr = BooleanTypePtr;
    BooleanTypePtr->form = FRM_ENUM;
    BooleanTypePtr->size = 4;
    BooleanTypePtr->typeIdPtr = booleanIdPtr;
    BooleanTypePtr->info.enumeration.max = 1;
    BooleanTypePtr->info.enumeration.constIdPtr = falseIdPtr;

    falseIdPtr->defn.key = DFN_CONST;
    falseIdPtr->defn.info.constant.value.integer = 0;
    falseIdPtr->typePtr = BooleanTypePtr;
    falseIdPtr->next = trueIdPtr;

    trueIdPtr->defn.key = DFN_CONST;
    trueIdPtr->defn.info.constant.value.integer = 1;
    trueIdPtr->typePtr = BooleanTypePtr;

    for (const StandardRoutine& routine : StandardRoutines)
    {
        enterStandardRoutine(const_cast<char*>(routine.name), routine.key, DFN_FUNCTION, routine.isOrder);
    }
}

auto freeSymTable(SymTableNodePtr) -> void
{
}
