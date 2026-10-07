#pragma once

#include "platform/MCBlockStore.h"

// ABL symbol tables and types. Every identifier the compiler meets (constant, type, variable, parameter, function,
// module, library) is a _SymTableNode in a binary tree per scope; SymTableDisplay holds the scopes open while
// compiling (0 = the global/library scope, 1 = the module, 2 = a function). Types are refcounted _Type records.
//
// 64-bit: the nodes hold pointers (tree links, name, type, library, code segment) where the original had 4-byte
// slots; the port gives them their pointer types, so a node is larger than the original 0x4c bytes. Offsets in the
// comments are the original's. Nothing here is ever written to a file.

class MCAblModule;
struct MCWatch;
struct MCSymTableNode;
struct MCType;

/// <summary>An address in ABL data or code (the original's <c>Address</c>, a <c>char*</c>).</summary>
typedef char* MCAddress;
typedef MCSymTableNode* MCSymTableNodePtr;
typedef MCType* MCTypePtr;

/// <summary>How many scopes can be open at once (SymTableDisplay): global, module, function.</summary>
inline constexpr int32_t MAX_NESTING_LEVEL = 3;
/// <summary>How many libraries one module can call into (LibrariesUsed).</summary>
inline constexpr int32_t MAX_LIBRARIES_USED = 26;

/// <summary>What a symbol is (_SymTableNode::defn.key).</summary>
enum MCDefinitionType
{
    DFN_UNDEFINED = 0,
    DFN_CONST = 1,
    DFN_TYPE = 2,
    DFN_VAR = 3,
    /// <summary>A parameter passed by value.</summary>
    DFN_VALPARAM = 4,
    /// <summary>A parameter passed by reference (<c>@name</c>): its stack slot holds the variable's address.</summary>
    DFN_REFPARAM = 5,
    /// <summary>A module or library; its localSymTable holds its globals and functions.</summary>
    DFN_MODULE = 6,
    DFN_PROCEDURE = 7,
    /// <summary>A function, declared in ABL or standard (routine.key says which).</summary>
    DFN_FUNCTION = 8
};

/// <summary>
/// Which routine a DFN_FUNCTION symbol is: RTN_DECLARED / RTN_FORWARD for functions written in ABL, otherwise the
/// standard routine (ablstd.cpp compiles its call, ablxstd.cpp runs it). The values are the original's, from
/// initSymTable; the names are the port's, after the ABL names in the comments. Values 65, 66, 82 and 96 are not
/// registered.
/// </summary>
enum MCRoutineKey
{
    RTN_DECLARED = 0,
    RTN_FORWARD = 1,
    RTN_RETURN = 2,                            // return
    RTN_PRINT = 3,                             // print
    RTN_CONCAT = 4,                            // concat
    RTN_ABS = 5,                               // abs
    RTN_ROUND = 6,                             // round
    RTN_SQRT = 7,                              // sqrt
    RTN_TRUNC = 8,                             // trunc
    RTN_RANDOM = 9,                            // random
    RTN_SET_MAX_LOOPS = 10,                    // setmaxloops
    RTN_FATAL = 11,                            // fatal
    RTN_ASSERT = 12,                           // assert
    RTN_GET_MODULE_HANDLE = 13,                // getmodulehandle
    RTN_GET_MODULE_NAME = 14,                  // getmodulename
    RTN_SET_MODULE_NAME = 15,                  // setmodulename
    RTN_GET_MODE = 16,                         // getmode
    RTN_GET_ACTION = 17,                       // getaction
    RTN_GET_PHASE = 18,                        // getphase
    RTN_GET_ID = 19,                           // getid
    RTN_GET_TIME = 20,                         // gettime
    RTN_GET_TIME_LEFT = 21,                    // gettimeleft
    RTN_GET_WARRIOR_STATUS = 22,               // getwarriorstatus
    RTN_SELECT_UNIT = 23,                      // selectunit
    RTN_SELECT_WARRIOR = 24,                   // selectwarrior
    RTN_SELECT_OBJECT = 25,                    // selectobject
    RTN_GET_CONTACTS = 26,                     // getcontacts
    RTN_GET_ENEMY_COUNT = 27,                  // getenemycount
    RTN_SELECT_CONTACT = 28,                   // selectcontact
    RTN_GET_CONTACT_ID = 29,                   // getcontactid
    RTN_IS_CONTACT = 30,                       // iscontact
    RTN_GET_CONTACT_STATUS = 31,               // getcontactstatus
    RTN_GET_CONTACT_RELATIVE_POSITION = 32,    // getcontactrelativeposition
    RTN_SET_GUARD_OBJECTIVE = 33,              // setguardobjective
    RTN_SET_GUARD_POINT = 34,                  // setguardpoint
    RTN_SET_GUARD_RADII = 35,                  // setguardradii
    RTN_GET_GUARD_OBJECTIVE = 36,              // getguardobjective
    RTN_GET_GUARD_POINT = 37,                  // getguardpoint
    RTN_GET_GUARD_RADII = 38,                  // getguardradii
    RTN_GET_GUARD_DISTANCE_TO = 39,            // getguarddistanceto
    RTN_GET_TARGET = 40,                       // gettarget
    RTN_SET_TARGET = 41,                       // settarget
    RTN_GET_WEAPONS_READY = 42,                // getweaponsready
    RTN_GET_WEAPONS_LOCKED = 43,               // getweaponslocked
    RTN_GET_WEAPONS_IN_RANGE = 44,             // getweaponsinrange
    RTN_GET_WEAPON_SHOTS = 45,                 // getweaponshots
    RTN_GET_WEAPON_RANGES = 46,                // getweaponranges
    RTN_GET_OBJECT_POSITION = 47,              // getobjectposition
    RTN_GET_INTEGER_MEMORY = 48,               // getintegermemory
    RTN_GET_REAL_MEMORY = 49,                  // getrealmemory
    RTN_GET_ALARM_TRIGGERS = 50,               // getalarmtriggers
    RTN_GET_CHALLENGER = 51,                   // getchallenger
    RTN_GET_FIRE_RANGES = 52,                  // getfireranges
    RTN_GET_ATTACKERS = 53,                    // getattackers
    RTN_GET_ATTACKER_INFO = 54,                // getattackerinfo
    RTN_SET_CHALLENGER = 55,                   // setchallenger
    RTN_GET_TIME_WITHOUT_ORDERS = 56,          // gettimewithoutorders
    RTN_SET_RADIO = 57,                        // setradio
    RTN_SET_MODE = 58,                         // setmode
    RTN_SET_ACTION = 59,                       // setaction
    RTN_SET_PHASE = 60,                        // setphase
    RTN_SET_UPDATE_TIME = 61,                  // setupdatetime
    RTN_SET_MOVE_GOAL = 62,                    // setmovegoal
    RTN_SET_INTEGER_MEMORY = 63,               // setintegermemory
    RTN_SET_REAL_MEMORY = 64,                  // setrealmemory
    RTN_START_FIELD_SCAN = 67,                 // startfieldscan
    RTN_START_ENEMY_SCAN = 68,                 // startenemyscan
    RTN_START_FRIENDLY_SCAN = 69,              // startfriendlyscan
    RTN_START_MOVE_PATH = 70,                  // startmovepath
    RTN_START_VEHICLE_SCAN = 71,               // startvehiclescan
    RTN_HAS_MOVE_GOAL = 72,                    // hasmovegoal
    RTN_HAS_MOVE_PATH = 73,                    // hasmovepath
    RTN_SORT_WEAPONS = 74,                     // sortweapons
    RTN_TIME_TO_IMPACT = 75,                   // timetoimpact
    RTN_FIRE_WEAPON = 76,                      // fireweapon
    RTN_GET_VISUAL_RANGE = 77,                 // getvisualrange
    RTN_GET_UNIT_MATES = 78,                   // getunitmates
    RTN_GET_TAC_ORDER = 79,                    // gettacorder
    RTN_GET_LAST_TAC_ORDER = 80,               // getlasttacorder
    RTN_SET_ORDER_MODE = 81,                   // setordermode
    RTN_ORDER_WAIT = 83,                       // orderwait
    RTN_ORDER_MOVE_TO = 84,                    // ordermoveto
    RTN_ORDER_MOVE_TO_OBJECT = 85,             // ordermovetoobject
    RTN_ORDER_MOVE_TO_CONTACT = 86,            // ordermovetocontact
    RTN_ORDER_TRAVERSE_PATH = 87,              // ordertraversepath
    RTN_ORDER_PATROL_PATH = 88,                // orderpatrolpath
    RTN_ORDER_POWER_UP = 89,                   // orderpowerup
    RTN_ORDER_POWER_DOWN = 90,                 // orderpowerdown
    RTN_ORDER_ATTACK_OBJECT = 91,              // orderattackobject
    RTN_ORDER_ATTACK_CONTACT = 92,             // orderattackcontact
    RTN_ATTACK_THREAT = 93,                    // attackthreat
    RTN_ATTACK_CLOSEST_TARGET = 94,            // attackclosesttarget
    RTN_ATTACK_PER_ORDERS = 95,                // attackperorders
    RTN_ORDER_WITHDRAW = 97,                   // orderwithdraw
    RTN_RETREAT = 98,                          // retreat
    RTN_OPEN_FIRE = 99,                        // openfire
    RTN_FIRE_UPON_ENEMY_FIRE_ONLY = 100,       // fireuponenemyfireonly
    RTN_DAMAGE_OBJECT = 101,                   // damageobject
    RTN_SET_ATTACK_RADIUS = 102,               // setattackradius
    RTN_ORDER_TEST = 103,                      // ordertest
    RTN_PLAY_SMACKER = 104,                    // playsmacker
    RTN_FILE_EXISTS = 105,                     // fileexists
    RTN_OBJECT_CHANGE_SIDES = 106,             // objectchangesides
    RTN_DISTANCE_TO_OBJECT = 107,              // distancetoobject
    RTN_DISTANCE_TO_POSITION = 108,            // distancetoposition
    RTN_OBJECT_SUICIDE = 109,                  // objectsuicide
    RTN_OBJECT_CREATE = 110,                   // objectcreate
    RTN_OBJECT_EXISTS = 111,                   // objectexists
    RTN_OBJECT_STATUS = 112,                   // objectstatus
    RTN_OBJECT_VISIBLE = 113,                  // objectvisible
    RTN_OBJECT_CLASS = 114,                    // objectclass
    RTN_OBJECT_SIDE = 115,                     // objectside
    RTN_OBJECT_COMMANDER = 116,                // objectcommander
    RTN_SET_TIMER = 117,                       // settimer
    RTN_CHECK_TIMER = 118,                     // checktimer
    RTN_END_TIMER = 119,                       // endtimer
    RTN_SET_OBJECTIVE_TIMER = 120,             // setobjectivetimer
    RTN_CHECK_OBJECTIVE_TIMER = 121,           // checkobjectivetimer
    RTN_SET_OBJECTIVE_STATUS = 122,            // setobjectivestatus
    RTN_CHECK_OBJECTIVE_STATUS = 123,          // checkobjectivestatus
    RTN_SET_OBJECTIVE_TYPE = 124,              // setobjectivetype
    RTN_CHECK_OBJECTIVE_TYPE = 125,            // checkobjectivetype
    RTN_PLAY_DIGITAL_MUSIC = 126,              // playdigitalmusic
    RTN_STOP_MUSIC = 127,                      // stopmusic
    RTN_PLAY_SOUND_EFFECT = 128,               // playsoundeffect
    RTN_PLAY_VIDEO = 129,                      // playvideo
    RTN_PLAY_SPEECH = 130,                     // playspeech
    RTN_PLAY_BETTY = 131,                      // playbetty
    RTN_SET_OBJECT_ACTIVE = 132,               // setobjectactive
    RTN_OBJECT_IN_WITHDRAWAL = 133,            // objectinwithdrawal
    RTN_OBJECT_TYPE_ID = 134,                  // objecttypeid
    RTN_GET_TERRAIN_OBJECT_PART_ID = 135,      // getterrainobjectpartid
    RTN_GET_VEHICLE_PART_ID = 136,             // getvehiclepartid
    RTN_GET_WEAPON_AMMO = 137,                 // getweaponammo
    RTN_OBJECT_STATUS_COUNT = 138,             // objectstatuscount
    RTN_IN_AREA = 139,                         // inarea
    RTN_GET_RELATIVE_POSITION_TO_POINT = 140,  // getrelativepositiontopoint
    RTN_GET_RELATIVE_POSITION_TO_OBJECT = 141, // getrelativepositiontoobject
    RTN_GET_SENSORS_WORKING = 142,             // getsensorsworking
    RTN_GET_CURRENT_BR_VALUE = 143,            // getcurrentbrvalue
    RTN_SET_CURRENT_BR_VALUE = 144,            // setcurrentbrvalue
    RTN_GET_ARMOR_PTS = 145,                   // getarmorpts
    RTN_GET_MAX_ARMOR = 146,                   // getmaxarmor
    RTN_GET_PILOT_ID = 147,                    // getpilotid
    /// <summary>Original behaviour: "setpilotwounds" is registered with this key too, so it reads the wounds.</summary>
    RTN_GET_PILOT_WOUNDS = 148, // getpilotwounds
    /// <summary>Handled by both dispatchers but no ABL name maps to it (see RTN_GET_PILOT_WOUNDS).</summary>
    RTN_SET_PILOT_WOUNDS = 149,
    RTN_GET_OBJECT_ACTIVE = 150,  // getobjectactive
    RTN_GET_OBJECT_DMG_PTS = 151, // getobjectdmgpts
    RTN_GET_OBJECT_MAX_DMG = 152, // getobjectmaxdmg
    RTN_GET_OBJECT_DAMAGE = 153,  // getobjectdamage
    RTN_SET_OBJECT_DAMAGE = 154,  // setobjectdamage
    RTN_GET_GLOBAL_VALUE = 155,   // getglobalvalue
    RTN_SET_GLOBAL_VALUE = 156,   // setglobalvalue
    RTN_SET_OBJECTIVE_POS = 157,  // setobjectivepos
    /// <summary>Registered, but neither dispatcher handles it.</summary>
    RTN_SET_MOVER_BEHAVIOR = 158, // setmoverbehavior
    /// <summary>Registered, but neither dispatcher handles it.</summary>
    RTN_SET_MOVER_OVERLAY_WEIGHT = 159, // setmoveroverlayweight
    RTN_SET_POTENTIAL_CONTACT = 160,    // setpotentialcontact
    RTN_SET_SENSOR_RANGE = 161,         // setsensorrange
    RTN_SET_TONNAGE = 162,              // settonnage
    RTN_PLAY_WAVE_FILE = 163,           // playwavefile
    RTN_SET_EXPLOSION_DAMAGE = 164,     // setexplosiondamage
    RTN_SET_EXPLOSION_RADIUS = 165,     // setexplosionradius
    RTN_GET_SALVAGE = 166,              // getsalvage
    RTN_SET_SALVAGE = 167,              // setsalvage
    RTN_SET_SALVAGE_STATUS = 168,       // setsalvagestatus
    RTN_SET_ANIMATION = 169,            // setanimation
    RTN_SET_REVEALED = 170,             // setrevealed
    RTN_ORDER_REFIT = 171,              // orderrefit
    RTN_ORDER_CAPTURE = 172,            // ordercapture
    RTN_SET_CAPTURED = 173,             // setcaptured
    RTN_SET_CAPTUREABLE = 174,          // setcaptureable
    RTN_IS_CAPTURED = 175,              // iscaptured
    RTN_IS_CAPTURABLE = 176,            // iscapturable
    RTN_WAS_EVER_CAPTURABLE = 177,      // wasevercapturable
    RTN_SET_BUILDING_NAME = 178,        // setbuildingname
    RTN_CALL_STRIKE = 179,              // callstrike
    RTN_ORDER_LOAD_ELEMENTALS = 180,    // orderloadelementals
    RTN_ORDER_DEPLOY_ELEMENTALS = 181,  // orderdeployelementals
    RTN_ADD_PRISONER = 182,             // addprisoner
    RTN_SET_TRAIN_SPEED = 183,          // settrainspeed
    RTN_LOCK_GATE_OPEN = 184,           // lockgateopen
    RTN_LOCK_GATE_CLOSED = 185,         // lockgateclosed
    RTN_RELEASE_GATE_LOCK = 186,        // releasegatelock
    RTN_IS_GATE_OPEN = 187,             // isgateopen
    RTN_CALL_STRIKE_EX = 188,           // callstrikeex
    RTN_GET_UNIT_STATUS = 189,          // getunitstatus
    RTN_REPAIR = 190,                   // repair
    RTN_GET_FIXED = 191,                // getfixed
    RTN_GET_REPAIR_STATE = 192,         // getrepairstate
    RTN_IS_TEAM_TARGETING = 193,        // isteamtargeting
    RTN_SEND_MESSAGE = 194,             // sendmessage
    RTN_GET_MESSAGE = 195,              // getmessage
    RTN_GET_HOME_TEAM = 196,            // gethometeam
    RTN_SET_STRIKES = 197,              // setstrikes
    RTN_GET_STRIKES = 198,              // getstrikes
    RTN_IS_SERVER = 199,                // isserver
    RTN_ADD_STRIKES = 200,              // addstrikes
    NUM_ABL_ROUTINES
};

/// <summary>Where a variable lives (_SymTableNode::defn.info.data.varType).</summary>
enum MCVariableType
{
    /// <summary>A local (or module-level) variable: a slot of the stack frame, data.offset items past its base.</summary>
    VAR_TYPE_NORMAL = 0,
    /// <summary>A <c>static</c> variable: slot data.offset of the module instance's static data.</summary>
    VAR_TYPE_STATIC = 1,
    /// <summary>An <c>eternal</c> variable: slot data.offset at the bottom of the ABL stack, shared by all modules.</summary>
    VAR_TYPE_ETERNAL = 2
};

/// <summary>The shape of a type (_Type::form).</summary>
enum MCFormType
{
    FRM_NONE = 0,
    /// <summary>integer, real, char.</summary>
    FRM_SCALAR = 1,
    /// <summary>An enumeration (boolean is one); its values are integers.</summary>
    FRM_ENUM = 2,
    /// <summary>An array: its value is the address of heap memory holding the elements.</summary>
    FRM_ARRAY = 3
};

/// <summary>The value of a DFN_CONST symbol.</summary>
/// <remarks>4 bytes in the original; the port's is pointer-sized because of stringPtr.</remarks>
union MCValue
{
    int32_t Integer;
    float Real;
    char Character;
    /// <summary>A string constant (a char-array type from makeStringType).</summary>
    char* StringPtr;
};

/// <summary>An ABL type. Refcounted: setType adds a user, clearType drops one and frees it at zero.</summary>
/// <remarks>
/// 0x1c bytes in the original (createType). <c>size</c> is the size of the value in ABL data (4 for integer, real
/// and enums, 1 for char, elementCount * element size for arrays). ABL arrays are stored with those sizes in heap
/// memory, not as StackItems, so they keep the original layout in the port.
/// </remarks>
struct MCType
{
    /// <summary>How many symbols share this type.</summary>
    int32_t NumInstances = 0;
    MCFormType Form{};
    int32_t Size = 0;
    /// <summary>The type's name, if it has one.</summary>
    MCSymTableNodePtr TypeIdPtr = nullptr;
    union
    {
        struct
        {
            /// <summary>The first value; the rest follow through _SymTableNode::next.</summary>
            MCSymTableNodePtr ConstIdPtr;
            /// <summary>The largest value.</summary>
            int32_t Max;
        } Enumeration;
        struct
        {
            MCTypePtr IndexTypePtr;
            MCTypePtr ElementTypePtr;
            int32_t ElementCount;
        } Array;
    } Info{};
};

/// <summary>What a symbol stands for, by DefinitionType (the original's <c>Definition</c>).</summary>
/// <remarks>0x24 bytes in the original (+0x18 .. +0x3c of _SymTableNode; extractSymTable copies it as 9 words).</remarks>
struct MCDefinition
{
    MCDefinitionType Key{};
    union
    {
        /// <summary>DFN_CONST.</summary>
        struct
        {
            MCValue Value;
        } Constant;
        /// <summary>DFN_FUNCTION and DFN_MODULE.</summary>
        struct
        {
            MCRoutineKey Key;
            int32_t ParamCount;
            /// <summary>Stack items the parameters take (paramCount; the first is item 4, after the frame header).</summary>
            int32_t TotalParamSize;
            /// <summary>Bytes of ABL data the locals take (bookkeeping only).</summary>
            int32_t TotalLocalSize;
            /// <summary>The parameters, linked through next.</summary>
            MCSymTableNodePtr Params;
            /// <summary>The local variables, linked through next.</summary>
            MCSymTableNodePtr Locals;
            /// <summary>The scope's symbol tree (for a module, its globals and functions).</summary>
            MCSymTableNodePtr LocalSymTable;
            /// <summary>The crunched code (createCodeSegment).</summary>
            MCAddress CodeSegment;
        } Routine;
        /// <summary>DFN_VAR, DFN_VALPARAM, DFN_REFPARAM.</summary>
        struct
        {
            MCVariableType VarType;
            /// <summary>Slot index: in the stack frame, the static data, or the eternal area (by varType).</summary>
            int32_t Offset;
        } Data;
    } Info{};
};

/// <summary>A symbol: a node of a scope's binary tree (ordered by strcmp of the name).</summary>
/// <remarks>0x4c bytes in the original (enterSymTable).</remarks>
struct MCSymTableNode
{
    MCSymTableNodePtr Left = nullptr;
    MCSymTableNodePtr Parent = nullptr;
    MCSymTableNodePtr Right = nullptr;
    /// <summary>The next symbol of a list (parameters, locals, enumeration values).</summary>
    MCSymTableNodePtr Next = nullptr;
    char* Name = nullptr;
    union
    {
        /// <summary>The debugger's watch on this symbol (WatchManager), or null.</summary>
        MCWatch* Info;
        /// <summary>
        /// A string literal's text (a copy of the name), for literals of two or more characters. factor enters every
        /// literal as a symbol of the module scope, named by its text.
        /// </summary>
        char* LiteralString;
    };

    MCDefinition Defn{};
    MCTypePtr TypePtr = nullptr;
    /// <summary>The library that defines this symbol, or null for the module being compiled.</summary>
    MCAblModule* Library = nullptr;
    /// <summary>The scope level it was declared at (0 global, 1 module, 2 function).</summary>
    int32_t Level = 0;
    int32_t LabelIndex = 0;
};

/// <summary>The open scopes (0 .. level), each the root of a symbol tree.</summary>
extern MCSymTableNodePtr SymTableDisplay[MAX_NESTING_LEVEL];
/// <summary>The innermost open scope while compiling; the current routine's level while executing.</summary>
extern int32_t Level;

/// <summary>
/// The memory ABL owns between ABLi_init and ABLi_close: symbol nodes, types, names and string literals, the code
/// buffer and segments, the stack, the registries, static data and array blocks. ABLi_close clears it; many blocks
/// (symbol nodes, static arrays) are only ever freed that way.
/// </summary>
/// <remarks>
/// Port: replaces the original's three ABL heaps (AblSymTableHeap, AblStackHeap, AblCodeHeap, sized from the mission
/// files). Nothing runs out, so the "unable to malloc" paths are gone.
/// </remarks>
extern MCBlockStore AblMemory;
/// <summary>The predefined types.</summary>
extern MCTypePtr IntegerTypePtr;
extern MCTypePtr CharTypePtr;
extern MCTypePtr RealTypePtr;
extern MCTypePtr BooleanTypePtr;
/// <summary>The type given to symbols after an error, so compiling can go on.</summary>
extern MCType DummyType;
/// <summary>The libraries the module being compiled uses (recordLibraryUsed).</summary>
extern MCAblModule* LibrariesUsed[MAX_LIBRARIES_USED];
extern int32_t NumLibrariesUsed;

/// <summary>Looks wordString up in the innermost scope.</summary>
void SearchLocalSymTable(MCSymTableNodePtr& idPtr);

/// <summary>Looks wordString up in all open scopes (and the libraries).</summary>
void SearchAllSymTables(MCSymTableNodePtr& idPtr);

/// <summary>Enters wordString in the innermost scope.</summary>
void EnterLocalSymTable(MCSymTableNodePtr& idPtr);

/// <summary>Looks wordString up in all scopes; if it is undefined, reports it and enters it as an undefined symbol.</summary>
void SearchAndFindAllSymTables(MCSymTableNodePtr& idPtr);

/// <summary>Enters wordString in the innermost scope, reporting it if it is already there.</summary>
void SearchAndEnterLocalSymTable(MCSymTableNodePtr& idPtr);

/// <summary>Enters wordString in <paramref name="root"/>'s tree, reporting it if it is already there.</summary>
void SearchAndEnterThisTable(MCSymTableNodePtr& idPtr, MCSymTableNodePtr root);

/// <summary>A new, empty type with one user.</summary>
MCTypePtr CreateType();

/// <summary>Adds a user to <paramref name="type"/>.</summary>
/// <returns>The type.</returns>
MCTypePtr SetType(MCTypePtr type);

/// <summary>Drops a user from <paramref name="type"/>, freeing it (and nulling the pointer) at zero.</summary>
void ClearType(MCTypePtr& type);

/// <summary>Adds the library defining <paramref name="idPtr"/> to LibrariesUsed.</summary>
void RecordLibraryUsed(MCSymTableNodePtr idPtr);

/// <summary>Finds <paramref name="name"/> in the tree at <paramref name="nodePtr"/>.</summary>
MCSymTableNodePtr SearchSymTable(char* name, MCSymTableNodePtr nodePtr);

/// <summary>Finds <paramref name="name"/> in the tree or in any library module in it.</summary>
MCSymTableNodePtr SearchLibrarySymTable(char* name, MCSymTableNodePtr nodePtr);

/// <summary>Finds <paramref name="name"/> in the libraries of the global scope.</summary>
MCSymTableNodePtr SearchLibrarySymTableDisplay(char* name);

/// <summary>
/// Finds <paramref name="name"/> in the open scopes, innermost first, then the libraries; <c>library.name</c> looks in
/// that library only. A library symbol found is recorded in LibrariesUsed.
/// </summary>
MCSymTableNodePtr SearchSymTableDisplay(char* name);

/// <summary>Adds a new symbol <paramref name="name"/> to the tree at <paramref name="ptrToNodePtr"/>.</summary>
MCSymTableNodePtr EnterSymTable(char* name, MCSymTableNodePtr* ptrToNodePtr);

/// <summary>Links an existing node into the tree at <paramref name="tableRoot"/>.</summary>
MCSymTableNodePtr InsertSymTable(MCSymTableNodePtr* tableRoot, MCSymTableNodePtr newNode);

/// <summary>Unlinks <paramref name="nodeKill"/> from the tree at <paramref name="tableRoot"/>.</summary>
/// <returns>The node actually removed (its contents moved into nodeKill when it had two children).</returns>
MCSymTableNodePtr ExtractSymTable(MCSymTableNodePtr* tableRoot, MCSymTableNodePtr nodeKill);

/// <summary>
/// Enters a standard routine in the innermost scope. <paramref name="isOrder"/> (1 for the tactical-order routines)
/// isn't stored.
/// </summary>
void EnterStandardRoutine(char* name, MCRoutineKey routineKey, MCDefinitionType definitionType, int isOrder);

/// <summary>Opens a scope rooted at <paramref name="symTableRoot"/> (fatal past MAX_NESTING_LEVEL).</summary>
void EnterScope(MCSymTableNodePtr symTableRoot);

/// <summary>Closes the innermost scope.</summary>
/// <returns>Its symbol tree.</returns>
MCSymTableNodePtr ExitScope();

/// <summary>Builds the global scope: the predefined types and constants and every standard routine.</summary>
void InitSymTable();

/// <summary>Frees a symbol tree (does nothing in MCX.EXE: the heap is dropped whole).</summary>
void FreeSymTable(MCSymTableNodePtr tableRoot);
