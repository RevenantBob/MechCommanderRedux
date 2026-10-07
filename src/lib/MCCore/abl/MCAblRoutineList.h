#pragma once

// The standard routines' runtime halves (the original's ablxstd.cpp), shared by the MCAblRoutines.cpp dispatch and
// the themed files that define them. Each reads its call the way the compiler wrote it: the routine's token, then
// "(", each argument expression evaluated onto the stack (by-reference arguments as the variable's address), a
// separator token between them, and ")". Most pop their arguments and leave the last slot on the stack holding the
// result. The ABL name each one runs is given as "ABL name (key)".

#include "abl/MCAblRoutines.h"
#include "abl/MCAblRuntime.h"
#include "abl/MCAblSymbolTable.h"

class MCBaseObject;
class MCGameObject;
class MCMechWarrior;
class MCMover;
class MCMoverGroup;
class MCTeam;

/// <summary>The slots of the script's global values (getglobalvalue / setglobalvalue).</summary>
inline constexpr int32_t AblGlobalValueCount = 50;

// ---- Helpers (MCAblRoutines.cpp) ----------------------------------------------------------------------------------

/// <summary>Whether <paramref name="object"/> is a mover (mech, vehicle, elemental or mover).</summary>
bool IsMover(MCBaseObject* object);

/// <summary>The object a script names by part id: -1 is the object whose brain runs.</summary>
MCGameObject* FindObject(MCAblRuntime& abl, int32_t partId);

/// <summary>Whether <paramref name="partId"/> names a group of movers (1..0x1ff) rather than one object.</summary>
bool IsGroupId(int32_t partId);

/// <summary>The movers of <paramref name="group"/>.</summary>
std::vector<MCMover*> GroupMovers(MCMoverGroup* group);

/// <summary>
/// The movers a group id names: 1..32 the player commander's groups, 0xa5..0xc4 commander 1's, 0x149..0x168
/// commander 2's, 500 / 501 / 502 the Inner Sphere, Clan and allied teams.
/// </summary>
/// <remarks>The original filled a 256-entry list; the port's has no limit.</remarks>
std::vector<MCMover*> GetGroupMovers(int32_t groupId);

/// <summary>The pilot a script names by index: -1 is the current one, otherwise 1..numWarriors.</summary>
/// <returns>Null for an index out of range.</returns>
MCMechWarrior* FindWarrior(MCAblRuntime& abl, int32_t warriorIndex);

/// <summary>
/// The damage that destroys <paramref name="object"/>, from its type: the building's, turret's or terrain object's
/// dmgLevel, or the misc terrain object's by kind (5 bridge, 6 forest, 7 wall, 8 medium wall, 9 light wall).
/// </summary>
/// <returns>False for any other class or kind.</returns>
bool GetDamageLevel(MCGameObject* object, uint32_t& damageLevel);

// ---- MCAblStdRoutines.cpp: the language's built-ins.

/// <summary>ABL return (2).</summary>
void ExecStdReturn(MCAblRuntime& abl);
/// <summary>ABL print (3).</summary>
void ExecStdPrint(MCAblRuntime& abl);
/// <summary>ABL concat (4).</summary>
MCAblType* ExecStdConcat(MCAblRuntime& abl);
/// <summary>ABL abs (5).</summary>
MCAblType* ExecStdAbs(MCAblRuntime& abl);
/// <summary>ABL round (6).</summary>
MCAblType* ExecStdRound(MCAblRuntime& abl);
/// <summary>ABL sqrt (7).</summary>
MCAblType* ExecStdSqrt(MCAblRuntime& abl);
/// <summary>ABL trunc (8).</summary>
MCAblType* ExecStdTrunc(MCAblRuntime& abl);
/// <summary>ABL random (9).</summary>
MCAblType* ExecStdRandom(MCAblRuntime& abl);
/// <summary>ABL getmodulehandle (13).</summary>
MCAblType* ExecStdGetModHandle(MCAblRuntime& abl);
/// <summary>ABL getmodulename (14).</summary>
MCAblType* ExecStdGetModName(MCAblRuntime& abl);
/// <summary>ABL setmodulename (15).</summary>
void ExecStdSetModName(MCAblRuntime& abl);
/// <summary>ABL setmaxloops (10): MaxLoopIterations = argument + 1.</summary>
MCAblType* ExecStdSetMaxLoops(MCAblRuntime& abl);
/// <summary>ABL fatal (11).</summary>
MCAblType* ExecStdFatal(MCAblRuntime& abl);
/// <summary>ABL assert (12).</summary>
MCAblType* ExecStdAssert(MCAblRuntime& abl);

// ---- MCAblUnitRoutines.cpp: the running unit, its pilot, targets, contacts, weapons and memory.

/// <summary>ABL getid (19).</summary>
MCAblType* ExecHbGetId(MCAblRuntime& abl);
/// <summary>ABL gettime (20).</summary>
MCAblType* ExecHbGetTime(MCAblRuntime& abl);
/// <summary>ABL gettimeleft (21).</summary>
MCAblType* ExecHbGetTimeLeft(MCAblRuntime& abl);
/// <summary>ABL gettarget (40).</summary>
MCAblType* ExecHbGetTarget(MCAblRuntime& abl);
/// <summary>ABL settarget (41).</summary>
void ExecHbSetTarget(MCAblRuntime& abl);
/// <summary>ABL selectunit (23).</summary>
MCAblType* ExecHbSelectUnit(MCAblRuntime& abl);
/// <summary>ABL selectobject (25).</summary>
MCAblType* ExecHbSelectObject(MCAblRuntime& abl);
/// <summary>ABL selectwarrior (24).</summary>
MCAblType* ExecHbSelectWarrior(MCAblRuntime& abl);
/// <summary>ABL getwarriorstatus (22).</summary>
MCAblType* ExecHbGetWarriorStatus(MCAblRuntime& abl);
/// <summary>ABL getcontacts (26).</summary>
MCAblType* ExecHbGetContacts(MCAblRuntime& abl);
/// <summary>ABL getenemycount (27).</summary>
MCAblType* ExecHbGetEnemyCount(MCAblRuntime& abl);
/// <summary>ABL selectcontact (28).</summary>
MCAblType* ExecHbSelectContact(MCAblRuntime& abl);
/// <summary>ABL iscontact (30).</summary>
MCAblType* ExecHbIsContact(MCAblRuntime& abl);
/// <summary>ABL getcontactid (29).</summary>
MCAblType* ExecHbGetContactId(MCAblRuntime& abl);
/// <summary>ABL getcontactstatus (31).</summary>
MCAblType* ExecHbGetContactStatus(MCAblRuntime& abl);
/// <summary>ABL getcontactrelativeposition (32).</summary>
MCAblType* ExecHbGetContactRelativePosition(MCAblRuntime& abl);
/// <summary>ABL setpotentialcontact (160).</summary>
MCAblType* ExecHbSetPotentialContact(MCAblRuntime& abl);
/// <summary>ABL getweaponsready (42), getweaponslocked (43), getweaponsinrange (44), by <paramref name="key"/>.
/// </summary>
MCAblType* ExecHbGetWeapons(MCAblRuntime& abl, MCAblRoutineKey key);
/// <summary>ABL getweaponshots (45).</summary>
MCAblType* ExecHbGetWeaponShots(MCAblRuntime& abl);
/// <summary>ABL getweaponranges (46).</summary>
void ExecHbGetWeaponRanges(MCAblRuntime& abl);
/// <summary>ABL setmovegoal (62).</summary>
MCAblType* ExecHbSetMoveGoal(MCAblRuntime& abl);
/// <summary>ABL getchallenger (51).</summary>
MCAblType* ExecHbGetChallenger(MCAblRuntime& abl);
/// <summary>ABL getfireranges (52).</summary>
MCAblType* ExecHbGetFireRanges(MCAblRuntime& abl);
/// <summary>ABL getattackers (53).</summary>
MCAblType* ExecHbGetAttackers(MCAblRuntime& abl);
/// <summary>ABL getattackerinfo (54).</summary>
MCAblType* ExecHbGetAttackerInfo(MCAblRuntime& abl);
/// <summary>ABL gettimewithoutorders (56).</summary>
MCAblType* ExecHbGetTimeWithoutOrders(MCAblRuntime& abl);
/// <summary>ABL setchallenger (55).</summary>
MCAblType* ExecHbSetChallenger(MCAblRuntime& abl);
/// <summary>ABL setintegermemory (63).</summary>
void ExecHbSetMemoryInteger(MCAblRuntime& abl);
/// <summary>ABL setrealmemory (64).</summary>
void ExecHbSetMemoryReal(MCAblRuntime& abl);
/// <summary>ABL hasmovegoal (72).</summary>
MCAblType* ExecHbHasMoveGoal(MCAblRuntime& abl);
/// <summary>ABL hasmovepath (73).</summary>
MCAblType* ExecHbHasMovePath(MCAblRuntime& abl);
/// <summary>ABL sortweapons (74).</summary>
void ExecHbSortWeapons(MCAblRuntime& abl);
/// <summary>ABL getobjectposition (47).</summary>
MCAblType* ExecHbGetObjectPosition(MCAblRuntime& abl);
/// <summary>ABL getvisualrange (77).</summary>
MCAblType* ExecHbGetVisualRange(MCAblRuntime& abl);
/// <summary>ABL getintegermemory (48).</summary>
MCAblType* ExecHbGetMemoryInteger(MCAblRuntime& abl);
/// <summary>ABL getrealmemory (49).</summary>
MCAblType* ExecHbGetMemoryReal(MCAblRuntime& abl);
/// <summary>ABL getalarmtriggers (50).</summary>
MCAblType* ExecHbGetAlarmTriggers(MCAblRuntime& abl);
/// <summary>ABL getunitmates (78).</summary>
MCAblType* ExecHbGetUnitMates(MCAblRuntime& abl);

// ---- MCAblOrderRoutines.cpp: tactical orders.

/// <summary>ABL gettacorder (79).</summary>
MCAblType* ExecHbGetTacOrder(MCAblRuntime& abl);
/// <summary>ABL getlasttacorder (80).</summary>
MCAblType* ExecHbGetLastTacOrder(MCAblRuntime& abl);
/// <summary>ABL setordermode (81).</summary>
MCAblType* ExecHbSetOrderMode(MCAblRuntime& abl);
/// <summary>ABL orderwait (83).</summary>
MCAblType* ExecHbWait(MCAblRuntime& abl);
/// <summary>ABL setattackradius (102).</summary>
MCAblType* ExecHbSetAttackRadius(MCAblRuntime& abl);
/// <summary>ABL ordermoveto (84).</summary>
MCAblType* ExecHbMoveToPoint(MCAblRuntime& abl);
/// <summary>ABL ordermovetoobject (85).</summary>
MCAblType* ExecHbMoveToObject(MCAblRuntime& abl);
/// <summary>ABL ordermovetocontact (86).</summary>
MCAblType* ExecHbMoveToContact(MCAblRuntime& abl);
/// <summary>ABL orderpowerdown (90).</summary>
MCAblType* ExecHbOrderPowerDown(MCAblRuntime& abl);
/// <summary>ABL orderpowerup (89). (ordertraversepath, orderpatrolpath, attackclosesttarget, attackperorders,
/// retreat and fireuponenemyfireonly do nothing at all in the dispatch.)</summary>
MCAblType* ExecHbOrderPowerUp(MCAblRuntime& abl);
/// <summary>ABL orderattackobject (91).</summary>
MCAblType* ExecHbOrderAttackObject(MCAblRuntime& abl);
/// <summary>ABL orderattackcontact (92).</summary>
MCAblType* ExecHbOrderAttackContact(MCAblRuntime& abl);
/// <summary>ABL ordertest (103).</summary>
MCAblType* ExecHbOrderTest(MCAblRuntime& abl);
/// <summary>ABL playsmacker (104).</summary>
MCAblType* ExecHbPlaySmacker(MCAblRuntime& abl);
/// <summary>ABL objectchangesides (106).</summary>
void ExecHbObjectChangeSides(MCAblRuntime& abl);

// ---- MCAblObjectRoutines.cpp: object queries, areas, damage.

/// <summary>ABL distancetoobject (107).</summary>
MCAblType* ExecHbDistanceToObject(MCAblRuntime& abl);
/// <summary>ABL distancetoposition (108).</summary>
MCAblType* ExecHbDistanceToPosition(MCAblRuntime& abl);
/// <summary>ABL objectsuicide (109).</summary>
void ExecHbObjectSuicide(MCAblRuntime& abl);
/// <summary>ABL objectcreate (110).</summary>
MCAblType* ExecHbObjectCreate(MCAblRuntime& abl);
/// <summary>ABL objectexists (111).</summary>
MCAblType* ExecHbObjectExists(MCAblRuntime& abl);
/// <summary>ABL objectstatus (112).</summary>
MCAblType* ExecHbObjectStatus(MCAblRuntime& abl);
/// <summary>ABL objectstatuscount (138): counts by status the objects of a part, a commander's (1..32) or a
/// group's, into an array.</summary>
MCAblType* ExecHbObjectStatusCount(MCAblRuntime& abl);
/// <summary>ABL objectvisible (113).</summary>
MCAblType* ExecHbObjectVisible(MCAblRuntime& abl);
/// <summary>ABL objectside (115).</summary>
MCAblType* ExecHbObjectSide(MCAblRuntime& abl);
/// <summary>ABL objectcommander (116).</summary>
MCAblType* ExecHbObjectCommander(MCAblRuntime& abl);
/// <summary>ABL objectclass (114).</summary>
MCAblType* ExecHbObjectClass(MCAblRuntime& abl);
/// <summary>ABL inarea (139).</summary>
MCAblType* ExecHbInArea(MCAblRuntime& abl);
/// <summary>ABL setobjectactive (132).</summary>
MCAblType* ExecHbSetObjActive(MCAblRuntime& abl);
/// <summary>ABL orderwithdraw (97).</summary>
MCAblType* ExecHbObjWithdraw(MCAblRuntime& abl);
/// <summary>ABL objectinwithdrawal (133).</summary>
MCAblType* ExecHbObjInWithdraw(MCAblRuntime& abl);
/// <summary>ABL objecttypeid (134).</summary>
MCAblType* ExecHbObjTypeId(MCAblRuntime& abl);
/// <summary>ABL getterrainobjectpartid (135).</summary>
MCAblType* ExecHbTerrainObjectId(MCAblRuntime& abl);
/// <summary>ABL getvehiclepartid (136).</summary>
MCAblType* ExecHbVehicleId(MCAblRuntime& abl);
/// <summary>ABL getweaponammo (137).</summary>
MCAblType* ExecHbGetWeaponAmmo(MCAblRuntime& abl);
/// <summary>ABL getsensorsworking (142).</summary>
MCAblType* ExecHbGetSensors(MCAblRuntime& abl);
/// <summary>ABL getcurrentbrvalue (143).</summary>
MCAblType* ExecHbGetBRValue(MCAblRuntime& abl);
/// <summary>ABL setcurrentbrvalue (144) by its name; never called (the key has no dispatch case).</summary>
MCAblType* ExecHbSetBRValue(MCAblRuntime& abl);
/// <summary>ABL getarmorpts (145): a mover's armor points left (0 for other objects).</summary>
MCAblType* ExecHbGetArmorPts(MCAblRuntime& abl);
/// <summary>ABL getmaxarmor (146) by its name; never called (the key has no dispatch case).</summary>
MCAblType* ExecHbGetMaxArmor(MCAblRuntime& abl);
/// <summary>ABL getpilotid (147).</summary>
MCAblType* ExecHbGetPilotId(MCAblRuntime& abl);
/// <summary>ABL getpilotwounds (148).</summary>
MCAblType* ExecHbGetPilotWounds(MCAblRuntime& abl);
/// <summary>ABL setpilotwounds (149).</summary>
MCAblType* ExecHbSetPilotWounds(MCAblRuntime& abl);
/// <summary>ABL getobjectactive (150).</summary>
MCAblType* ExecHbGetObjActive(MCAblRuntime& abl);
/// <summary>ABL getobjectdamage (153).</summary>
MCAblType* ExecHbGetObjDamage(MCAblRuntime& abl);
/// <summary>The dispatch sends ABL getobjectmaxdmg (152) here.</summary>
MCAblType* ExecHbGetObjDmgPts(MCAblRuntime& abl);
/// <summary>
/// The damage that destroys an object, by its name. Never called: getobjectdmgpts (151) has no dispatch case
/// (it is an undefined-routine Fatal) and getobjectmaxdmg runs execHbGetObjDmgPts.
/// </summary>
MCAblType* ExecHbGetMaxDmg(MCAblRuntime& abl);
/// <summary>ABL setobjectdamage (154).</summary>
void ExecHbSetObjDamage(MCAblRuntime& abl);
/// <summary>ABL damageobject (101).</summary>
MCAblType* ExecHbDamageObject(MCAblRuntime& abl);

// ---- MCAblMissionRoutines.cpp: timers, objectives, music and sound, global values, messages, strikes.

/// <summary>ABL settimer (117).</summary>
MCAblType* ExecHbSetTimer(MCAblRuntime& abl);
/// <summary>ABL checktimer (118).</summary>
MCAblType* ExecHbChkTimer(MCAblRuntime& abl);
/// <summary>ABL endtimer (119).</summary>
void ExecHbEndTimer(MCAblRuntime& abl);
/// <summary>ABL setobjectivetimer (120).</summary>
MCAblType* ExecHbSetObjectiveTimer(MCAblRuntime& abl);
/// <summary>ABL checkobjectivetimer (121).</summary>
MCAblType* ExecHbCheckObjectiveTimer(MCAblRuntime& abl);
/// <summary>ABL setobjectivestatus (122).</summary>
MCAblType* ExecHbSetObjectiveStatus(MCAblRuntime& abl);
/// <summary>ABL checkobjectivestatus (123).</summary>
MCAblType* ExecHbCheckObjectiveStatus(MCAblRuntime& abl);
/// <summary>ABL setobjectivetype (124).</summary>
MCAblType* ExecHbSetObjectiveType(MCAblRuntime& abl);
/// <summary>ABL checkobjectivetype (125).</summary>
MCAblType* ExecHbCheckObjectiveType(MCAblRuntime& abl);
/// <summary>ABL playdigitalmusic (126).</summary>
MCAblType* ExecHbPlayDigitalMusic(MCAblRuntime& abl);
/// <summary>ABL stopmusic (127).</summary>
MCAblType* ExecHbStopMusic(MCAblRuntime& abl);
/// <summary>ABL playsoundeffect (128).</summary>
MCAblType* ExecHbPlaySoundEffect(MCAblRuntime& abl);
/// <summary>ABL playvideo (129).</summary>
MCAblType* ExecHbPlayVideo(MCAblRuntime& abl);
/// <summary>ABL setradio (57).</summary>
void ExecHbSetRadio(MCAblRuntime& abl);
/// <summary>ABL playspeech (130).</summary>
MCAblType* ExecHbPlaySpeech(MCAblRuntime& abl);
/// <summary>ABL playbetty (131).</summary>
MCAblType* ExecHbPlayBetty(MCAblRuntime& abl);
/// <summary>ABL getglobalvalue (155).</summary>
MCAblType* ExecHbGetGlobalValue(MCAblRuntime& abl);
/// <summary>ABL setglobalvalue (156).</summary>
void ExecHbSetGlobalValue(MCAblRuntime& abl);
/// <summary>Writes <see cref="MissionScriptMessageLog"/> to the debug output.</summary>
void DebugMissionScriptMessages();
/// <summary>ABL sendmessage (194).</summary>
void ExecHbSendMessage(MCAblRuntime& abl);
/// <summary>ABL getmessage (195).</summary>
MCAblType* ExecHbGetMessage(MCAblRuntime& abl);
/// <summary>ABL getstrikes (198).</summary>
MCAblType* ExecHbGetStrikes(MCAblRuntime& abl);
/// <summary>ABL setstrikes (197).</summary>
void ExecHbSetStrikes(MCAblRuntime& abl);
/// <summary>ABL addstrikes (200).</summary>
void ExecHbAddStrikes(MCAblRuntime& abl);
/// <summary>ABL isserver (199).</summary>
MCAblType* ExecHbIsServer(MCAblRuntime& abl);
/// <summary>ABL gethometeam (196).</summary>
MCAblType* ExecHbGetHomeTeam(MCAblRuntime& abl);

// ---- MCAblObjectActionRoutines.cpp: changing objects (salvage, capture, gates, trains, repair, ...).

/// <summary>ABL setobjectivepos (157).</summary>
void ExecHbSetObjectivePos(MCAblRuntime& abl);
/// <summary>ABL settonnage (162).</summary>
void ExecHbSetTonnage(MCAblRuntime& abl);
/// <summary>ABL setsensorrange (161).</summary>
void ExecHbSetSensorRange(MCAblRuntime& abl);
/// <summary>ABL setexplosiondamage (164).</summary>
void ExecHbSetExplDmg(MCAblRuntime& abl);
/// <summary>ABL setexplosionradius (165).</summary>
void ExecHbSetExplRad(MCAblRuntime& abl);
/// <summary>ABL setsalvage (167).</summary>
MCAblType* ExecHbSetSalvage(MCAblRuntime& abl);
/// <summary>ABL setsalvagestatus (168).</summary>
MCAblType* ExecHbSetSalvageStatus(MCAblRuntime& abl);
/// <summary>ABL setanimation (169).</summary>
void ExecHbSetAnimation(MCAblRuntime& abl);
/// <summary>ABL playwavefile (163).</summary>
void ExecHbPlayWave(MCAblRuntime& abl);
/// <summary>ABL setrevealed (170).</summary>
void ExecHbSetRevealed(MCAblRuntime& abl);
/// <summary>ABL getsalvage (166).</summary>
void ExecHbGetSalvage(MCAblRuntime& abl);
/// <summary>ABL orderrefit (171).</summary>
void ExecHbRefit(MCAblRuntime& abl);
/// <summary>ABL setcaptured (173).</summary>
void ExecHbSetCaptured(MCAblRuntime& abl);
/// <summary>ABL ordercapture (172).</summary>
void ExecHbCaptureObject(MCAblRuntime& abl);
/// <summary>ABL setcaptureable (174).</summary>
void ExecHbSetCaptureable(MCAblRuntime& abl);
/// <summary>ABL iscaptured (175).</summary>
MCAblType* ExecHbIsCaptured(MCAblRuntime& abl);
/// <summary>ABL iscapturable (176).</summary>
MCAblType* ExecHbIsCapturable(MCAblRuntime& abl);
/// <summary>ABL wasevercapturable (177).</summary>
MCAblType* ExecHbWasEverCapturable(MCAblRuntime& abl);
/// <summary>ABL setbuildingname (178): gives a building (class 0x10) of the part the string table entry as its
/// name.</summary>
void ExecHbSetBuildingName(MCAblRuntime& abl);
/// <summary>ABL callstrike (179).</summary>
void ExecHbCallStrike(MCAblRuntime& abl);
/// <summary>ABL callstrikeex (188).</summary>
void ExecHbCallStrikeEx(MCAblRuntime& abl);
/// <summary>ABL orderloadelementals (180).</summary>
void ExecHbLoadElementals(MCAblRuntime& abl);
/// <summary>ABL orderdeployelementals (181).</summary>
void ExecHbDeployElementals(MCAblRuntime& abl);
/// <summary>ABL addprisoner (182).</summary>
MCAblType* ExecHbAddPrisoner(MCAblRuntime& abl);
/// <summary>ABL settrainspeed (183).</summary>
void ExecHbSetTrainSpeed(MCAblRuntime& abl);
/// <summary>ABL lockgateopen (184).</summary>
void ExecHbLockGateOpen(MCAblRuntime& abl);
/// <summary>ABL lockgateclosed (185).</summary>
void ExecHbLockGateClosed(MCAblRuntime& abl);
/// <summary>ABL releasegatelock (186).</summary>
void ExecHbReleaseGateLock(MCAblRuntime& abl);
/// <summary>ABL isgateopen (187).</summary>
MCAblType* ExecHbIsGateOpen(MCAblRuntime& abl);
/// <summary>ABL getunitstatus (189).</summary>
MCAblType* ExecHbGetUnitStatus(MCAblRuntime& abl);
/// <summary>ABL getrelativepositiontopoint (140).</summary>
void ExecHbRelPosPoint(MCAblRuntime& abl);
/// <summary>ABL getrelativepositiontoobject (141).</summary>
void ExecHbRelPosObject(MCAblRuntime& abl);
/// <summary>ABL repair (190).</summary>
void ExecHbRepair(MCAblRuntime& abl);
/// <summary>ABL getrepairstate (192).</summary>
MCAblType* ExecHbGetRepairState(MCAblRuntime& abl);
/// <summary>ABL isteamtargeting (193).</summary>
MCAblType* ExecHbIsTeamTargeting(MCAblRuntime& abl);
/// <summary>ABL getfixed (191).</summary>
MCAblType* ExecHbGetFixed(MCAblRuntime& abl);
