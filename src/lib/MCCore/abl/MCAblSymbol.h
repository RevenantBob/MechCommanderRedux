#pragma once

// ABL symbols and types. Every identifier the compiler meets (constant, type, variable, parameter, function, module,
// library) and every literal is an MCAblSymbol in a binary tree per scope (MCAblSymbolTable). Nothing here is ever
// written to a file.

class MCAblModule;
struct MCWatch;
struct MCAblSymbol;
struct MCAblType;

/// <summary>An address in ABL data or code (the original's <c>Address</c>, a <c>char*</c>).</summary>
using MCAddress = char*;

/// <summary>What a symbol is (the original's <c>DefinitionType</c>).</summary>
enum class MCAblSymbolKind : int32_t
{
    /// <summary>Also every literal (MCAblCompiler::LiteralSymbol).</summary>
    Undefined = 0,
    Const = 1,
    Type = 2,
    Var = 3,
    /// <summary>A parameter passed by value.</summary>
    ValueParam = 4,
    /// <summary>A parameter passed by reference (<c>@name</c>): its stack slot holds the variable's address.</summary>
    RefParam = 5,
    /// <summary>A module or library; its local symbol tree holds its globals and functions.</summary>
    Module = 6,
    Procedure = 7,
    /// <summary>A function, declared in ABL or standard (its routine key says which).</summary>
    Function = 8
};

/// <summary>
/// Which routine a function symbol is: <c>Declared</c> / <c>Forward</c> for functions written in ABL, otherwise the
/// standard routine (ablstd.cpp compiles its call, ablxstd.cpp runs it). The values are the original's, from
/// initSymTable; the names are the port's, after the ABL names in the comments. Values 65, 66, 82 and 96 are not
/// registered.
/// </summary>
enum class MCAblRoutineKey : int32_t
{
    Declared = 0,
    Forward = 1,
    Return = 2,                        // return
    Print = 3,                         // print
    Concat = 4,                        // concat
    Abs = 5,                           // abs
    Round = 6,                         // round
    Sqrt = 7,                          // sqrt
    Trunc = 8,                         // trunc
    Random = 9,                        // random
    SetMaxLoops = 10,                  // setmaxloops
    Fatal = 11,                        // fatal
    Assert = 12,                       // assert
    GetModuleHandle = 13,              // getmodulehandle
    GetModuleName = 14,                // getmodulename
    SetModuleName = 15,                // setmodulename
    GetMode = 16,                      // getmode
    GetAction = 17,                    // getaction
    GetPhase = 18,                     // getphase
    GetId = 19,                        // getid
    GetTime = 20,                      // gettime
    GetTimeLeft = 21,                  // gettimeleft
    GetWarriorStatus = 22,             // getwarriorstatus
    SelectUnit = 23,                   // selectunit
    SelectWarrior = 24,                // selectwarrior
    SelectObject = 25,                 // selectobject
    GetContacts = 26,                  // getcontacts
    GetEnemyCount = 27,                // getenemycount
    SelectContact = 28,                // selectcontact
    GetContactId = 29,                 // getcontactid
    IsContact = 30,                    // iscontact
    GetContactStatus = 31,             // getcontactstatus
    GetContactRelativePosition = 32,   // getcontactrelativeposition
    SetGuardObjective = 33,            // setguardobjective
    SetGuardPoint = 34,                // setguardpoint
    SetGuardRadii = 35,                // setguardradii
    GetGuardObjective = 36,            // getguardobjective
    GetGuardPoint = 37,                // getguardpoint
    GetGuardRadii = 38,                // getguardradii
    GetGuardDistanceTo = 39,           // getguarddistanceto
    GetTarget = 40,                    // gettarget
    SetTarget = 41,                    // settarget
    GetWeaponsReady = 42,              // getweaponsready
    GetWeaponsLocked = 43,             // getweaponslocked
    GetWeaponsInRange = 44,            // getweaponsinrange
    GetWeaponShots = 45,               // getweaponshots
    GetWeaponRanges = 46,              // getweaponranges
    GetObjectPosition = 47,            // getobjectposition
    GetIntegerMemory = 48,             // getintegermemory
    GetRealMemory = 49,                // getrealmemory
    GetAlarmTriggers = 50,             // getalarmtriggers
    GetChallenger = 51,                // getchallenger
    GetFireRanges = 52,                // getfireranges
    GetAttackers = 53,                 // getattackers
    GetAttackerInfo = 54,              // getattackerinfo
    SetChallenger = 55,                // setchallenger
    GetTimeWithoutOrders = 56,         // gettimewithoutorders
    SetRadio = 57,                     // setradio
    SetMode = 58,                      // setmode
    SetAction = 59,                    // setaction
    SetPhase = 60,                     // setphase
    SetUpdateTime = 61,                // setupdatetime
    SetMoveGoal = 62,                  // setmovegoal
    SetIntegerMemory = 63,             // setintegermemory
    SetRealMemory = 64,                // setrealmemory
    StartFieldScan = 67,               // startfieldscan
    StartEnemyScan = 68,               // startenemyscan
    StartFriendlyScan = 69,            // startfriendlyscan
    StartMovePath = 70,                // startmovepath
    StartVehicleScan = 71,             // startvehiclescan
    HasMoveGoal = 72,                  // hasmovegoal
    HasMovePath = 73,                  // hasmovepath
    SortWeapons = 74,                  // sortweapons
    TimeToImpact = 75,                 // timetoimpact
    FireWeapon = 76,                   // fireweapon
    GetVisualRange = 77,               // getvisualrange
    GetUnitMates = 78,                 // getunitmates
    GetTacOrder = 79,                  // gettacorder
    GetLastTacOrder = 80,              // getlasttacorder
    SetOrderMode = 81,                 // setordermode
    OrderWait = 83,                    // orderwait
    OrderMoveTo = 84,                  // ordermoveto
    OrderMoveToObject = 85,            // ordermovetoobject
    OrderMoveToContact = 86,           // ordermovetocontact
    OrderTraversePath = 87,            // ordertraversepath
    OrderPatrolPath = 88,              // orderpatrolpath
    OrderPowerUp = 89,                 // orderpowerup
    OrderPowerDown = 90,               // orderpowerdown
    OrderAttackObject = 91,            // orderattackobject
    OrderAttackContact = 92,           // orderattackcontact
    AttackThreat = 93,                 // attackthreat
    AttackClosestTarget = 94,          // attackclosesttarget
    AttackPerOrders = 95,              // attackperorders
    OrderWithdraw = 97,                // orderwithdraw
    Retreat = 98,                      // retreat
    OpenFire = 99,                     // openfire
    FireUponEnemyFireOnly = 100,       // fireuponenemyfireonly
    DamageObject = 101,                // damageobject
    SetAttackRadius = 102,             // setattackradius
    OrderTest = 103,                   // ordertest
    PlaySmacker = 104,                 // playsmacker
    FileExists = 105,                  // fileexists
    ObjectChangeSides = 106,           // objectchangesides
    DistanceToObject = 107,            // distancetoobject
    DistanceToPosition = 108,          // distancetoposition
    ObjectSuicide = 109,               // objectsuicide
    ObjectCreate = 110,                // objectcreate
    ObjectExists = 111,                // objectexists
    ObjectStatus = 112,                // objectstatus
    ObjectVisible = 113,               // objectvisible
    ObjectClass = 114,                 // objectclass
    ObjectSide = 115,                  // objectside
    ObjectCommander = 116,             // objectcommander
    SetTimer = 117,                    // settimer
    CheckTimer = 118,                  // checktimer
    EndTimer = 119,                    // endtimer
    SetObjectiveTimer = 120,           // setobjectivetimer
    CheckObjectiveTimer = 121,         // checkobjectivetimer
    SetObjectiveStatus = 122,          // setobjectivestatus
    CheckObjectiveStatus = 123,        // checkobjectivestatus
    SetObjectiveType = 124,            // setobjectivetype
    CheckObjectiveType = 125,          // checkobjectivetype
    PlayDigitalMusic = 126,            // playdigitalmusic
    StopMusic = 127,                   // stopmusic
    PlaySoundEffect = 128,             // playsoundeffect
    PlayVideo = 129,                   // playvideo
    PlaySpeech = 130,                  // playspeech
    PlayBetty = 131,                   // playbetty
    SetObjectActive = 132,             // setobjectactive
    ObjectInWithdrawal = 133,          // objectinwithdrawal
    ObjectTypeId = 134,                // objecttypeid
    GetTerrainObjectPartId = 135,      // getterrainobjectpartid
    GetVehiclePartId = 136,            // getvehiclepartid
    GetWeaponAmmo = 137,               // getweaponammo
    ObjectStatusCount = 138,           // objectstatuscount
    InArea = 139,                      // inarea
    GetRelativePositionToPoint = 140,  // getrelativepositiontopoint
    GetRelativePositionToObject = 141, // getrelativepositiontoobject
    GetSensorsWorking = 142,           // getsensorsworking
    GetCurrentBRValue = 143,           // getcurrentbrvalue
    SetCurrentBRValue = 144,           // setcurrentbrvalue
    GetArmorPts = 145,                 // getarmorpts
    GetMaxArmor = 146,                 // getmaxarmor
    GetPilotId = 147,                  // getpilotid
    /// <summary>Original behaviour: "setpilotwounds" is registered with this key too, so it reads the wounds.</summary>
    GetPilotWounds = 148, // getpilotwounds
    /// <summary>Handled by both dispatchers but no ABL name maps to it (see RTN_GET_PILOT_WOUNDS).</summary>
    SetPilotWounds = 149,
    GetObjectActive = 150, // getobjectactive
    GetObjectDmgPts = 151, // getobjectdmgpts
    GetObjectMaxDmg = 152, // getobjectmaxdmg
    GetObjectDamage = 153, // getobjectdamage
    SetObjectDamage = 154, // setobjectdamage
    GetGlobalValue = 155,  // getglobalvalue
    SetGlobalValue = 156,  // setglobalvalue
    SetObjectivePos = 157, // setobjectivepos
    /// <summary>Registered, but neither dispatcher handles it.</summary>
    SetMoverBehavior = 158, // setmoverbehavior
    /// <summary>Registered, but neither dispatcher handles it.</summary>
    SetMoverOverlayWeight = 159, // setmoveroverlayweight
    SetPotentialContact = 160,   // setpotentialcontact
    SetSensorRange = 161,        // setsensorrange
    SetTonnage = 162,            // settonnage
    PlayWaveFile = 163,          // playwavefile
    SetExplosionDamage = 164,    // setexplosiondamage
    SetExplosionRadius = 165,    // setexplosionradius
    GetSalvage = 166,            // getsalvage
    SetSalvage = 167,            // setsalvage
    SetSalvageStatus = 168,      // setsalvagestatus
    SetAnimation = 169,          // setanimation
    SetRevealed = 170,           // setrevealed
    OrderRefit = 171,            // orderrefit
    OrderCapture = 172,          // ordercapture
    SetCaptured = 173,           // setcaptured
    SetCaptureable = 174,        // setcaptureable
    IsCaptured = 175,            // iscaptured
    IsCapturable = 176,          // iscapturable
    WasEverCapturable = 177,     // wasevercapturable
    SetBuildingName = 178,       // setbuildingname
    CallStrike = 179,            // callstrike
    OrderLoadElementals = 180,   // orderloadelementals
    OrderDeployElementals = 181, // orderdeployelementals
    AddPrisoner = 182,           // addprisoner
    SetTrainSpeed = 183,         // settrainspeed
    LockGateOpen = 184,          // lockgateopen
    LockGateClosed = 185,        // lockgateclosed
    ReleaseGateLock = 186,       // releasegatelock
    IsGateOpen = 187,            // isgateopen
    CallStrikeEx = 188,          // callstrikeex
    GetUnitStatus = 189,         // getunitstatus
    Repair = 190,                // repair
    GetFixed = 191,              // getfixed
    GetRepairState = 192,        // getrepairstate
    IsTeamTargeting = 193,       // isteamtargeting
    SendMessage = 194,           // sendmessage
    GetMessage = 195,            // getmessage
    GetHomeTeam = 196,           // gethometeam
    SetStrikes = 197,            // setstrikes
    GetStrikes = 198,            // getstrikes
    IsServer = 199,              // isserver
    AddStrikes = 200,            // addstrikes
    Count
};

/// <summary>Where a variable lives (the original's <c>VariableType</c>).</summary>
enum class MCAblStorage : int32_t
{
    /// <summary>A local (or module-level) variable: a slot of the stack frame, Offset items past its base.</summary>
    Normal = 0,
    /// <summary>A <c>static</c> variable: slot Offset of the module instance's static data.</summary>
    Static = 1,
    /// <summary>An <c>eternal</c> variable: slot Offset at the bottom of the ABL stack, shared by all modules.</summary>
    Eternal = 2
};

/// <summary>The shape of a type (the original's <c>FormType</c>).</summary>
enum class MCAblTypeForm : int32_t
{
    None = 0,
    /// <summary>integer, real, char.</summary>
    Scalar = 1,
    /// <summary>An enumeration (boolean is one); its values are integers.</summary>
    Enum = 2,
    /// <summary>An array: its value is the address of memory holding the elements.</summary>
    Array = 3
};

/// <summary>The value of a constant symbol (and of a literal).</summary>
/// <remarks>4 bytes in the original; pointer-sized in the port because of StringPtr.</remarks>
union MCAblValue
{
    int32_t Integer;
    float Real;
    char Character;
    /// <summary>A string constant's text (a char-array type from MakeStringType).</summary>
    char* StringPtr;
};

/// <summary>An ABL type. The symbol table owns every type; symbols share them.</summary>
/// <remarks>
/// <c>Size</c> is the size of the value in ABL data (4 for integer, real and enums, 1 for char, element count times
/// element size for arrays). ABL arrays are stored with those sizes, not as stack items, so they keep the original
/// layout in the port. The executor resizes an array's last dimension in place (ablxstmt.cpp).
/// </remarks>
struct MCAblType
{
    MCAblTypeForm Form = MCAblTypeForm::None;
    int32_t Size = 0;

    /// <summary>An array's dimension: index and element types, and how many elements.</summary>
    struct ArrayInfo
    {
        MCAblType* IndexTypePtr = nullptr;
        /// <summary>The element type; for a multi-dimensional array, the next dimension's array type.</summary>
        MCAblType* ElementTypePtr = nullptr;
        int32_t ElementCount = 0;
    };

    /// <summary>Set when Form is Array.</summary>
    ArrayInfo Array{};
};

/// <summary>What a symbol stands for, by its kind (the original's <c>Definition</c>).</summary>
/// <remarks>
/// A union, as in the original: a string literal is entered under its text in the module's scope, so a literal
/// spelled like a module identifier writes its value over that identifier's definition (no retail script does; the
/// test "abl: no retail string literal reuses an identifier's symbol" checks it).
/// </remarks>
struct MCAblDefinition
{
    MCAblSymbolKind Key = MCAblSymbolKind::Undefined;

    union
    {
        /// <summary>Const (and literals).</summary>
        struct
        {
            MCAblValue Value;
        } Constant;

        /// <summary>Function and Module.</summary>
        struct
        {
            MCAblRoutineKey Key;
            /// <summary>Stack items the parameters take (the first is item 4, after the frame header).</summary>
            int32_t TotalParamSize;
            /// <summary>The parameters, linked through Next.</summary>
            MCAblSymbol* Params;
            /// <summary>The local variables, linked through Next.</summary>
            MCAblSymbol* Locals;
            /// <summary>The scope's symbol tree (for a module, its globals and functions).</summary>
            MCAblSymbol* LocalSymTable;
            /// <summary>The crunched code (MCAblCodeWriter::CreateSegment).</summary>
            MCAddress CodeSegment;
        } Routine;

        /// <summary>Var, ValueParam, RefParam.</summary>
        struct
        {
            MCAblStorage VarType;
            /// <summary>Slot index: in the stack frame, the static data, or the eternal area (by VarType).</summary>
            int32_t Offset;
        } Data;
    } Info{};
};

/// <summary>A symbol: a node of a scope's binary tree, ordered by the bytes of its name.</summary>
struct MCAblSymbol
{
    MCAblSymbol* Left = nullptr;
    MCAblSymbol* Parent = nullptr;
    MCAblSymbol* Right = nullptr;
    /// <summary>The next symbol of a list (parameters, locals).</summary>
    MCAblSymbol* Next = nullptr;
    std::string Name;
    /// <summary>The debugger's watch on this symbol (MCWatchManager), or null.</summary>
    MCWatch* Watch = nullptr;
    /// <summary>A string literal's text, for literals of two or more characters (the executor pushes its address).</summary>
    std::string LiteralText;
    MCAblDefinition Defn{};
    MCAblType* TypePtr = nullptr;
    /// <summary>The library that defines this symbol, or null for the module being compiled.</summary>
    MCAblModule* Library = nullptr;
    /// <summary>The scope level it was entered at (0 global, 1 module, 2 function).</summary>
    int32_t Level = 0;
};
