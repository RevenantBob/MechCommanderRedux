#pragma once

// Executing calls of ABL's standard routines: execStd* for the language built-ins, execHb* ("heartbeat") for the
// game functions unit brains and mission scripts call (ablstd.cpp compiles them). Each reads its arguments from the
// code segment (evaluating them onto the runtime stack), acts on the game, and leaves its result on top of the
// stack. execStandardRoutineCall dispatches on the routine's RoutineKey (ablsymt.h); the ABL name each one runs is
// given as "ABL name (key)".

#include "abl/MCAblScanner.h"
#include "abl/MCAblSymbolTable.h"

class MCGameObject;
class MCMechWarrior;
class MCMover;
class MCMoverGroup;

/// <summary>Where the current tactical order came from (the order's origin, set by the order code).</summary>
extern int TacOrderOrigin;
/// <summary>The code a routine's return jumps to: an end-of-routine token sequence.</summary>
extern MCAblToken ExitRoutineCodeSegment[2];
/// <summary>The code an order's return jumps to.</summary>
extern MCAblToken ExitOrderCodeSegment[2];
/// <summary>The mission script messages sent (sendmessage), each three shorts; DebugMissionScriptMessages writes
/// them out.</summary>
extern int16_t MissionScriptMessageLog[1000][3];
/// <summary>How many entries <see cref="MissionScriptMessageLog"/> holds.</summary>
extern int32_t NumMissionScriptMessages;
/// <summary>Scratch list the routines that scan units fill (getunitmates, getattackers, ...).</summary>
extern MCMover* MoverList[256];
/// <summary>Set while the running brain is a unit's (group) order rather than one pilot's.</summary>
extern int IsUnitOrder;
/// <summary>The group whose brain runs.</summary>
extern MCMoverGroup* CurGroup;
/// <summary>The object whose brain runs.</summary>
extern MCGameObject* CurObject;
/// <summary>The object class of <see cref="CurObject"/> while a brain runs.</summary>
extern int32_t CurObjectClass;
/// <summary>The alarm being handled while a pilot's brain runs its handler.</summary>
extern int32_t CurAlarm;
/// <summary>The pilot whose brain runs.</summary>
extern MCMechWarrior* CurWarrior;
/// <summary>The contact selectcontact picked.</summary>
extern MCGameObject* CurContact;
/// <summary>The multiplayer message code and parameter sendmessage passes on.</summary>
extern int32_t CurMultiplayCode;
extern int32_t CurMultiplayParam;
/// <summary>The mission's global values (getglobalvalue / setglobalvalue).</summary>
extern float GlobalMissionValues[50];

/// <summary>Returns from a tactical order with <paramref name="returnValue"/>: stores it in the order's frame, sets
/// the exit flags, and (unless it is 1) jumps to <see cref="ExitOrderCodeSegment"/>.</summary>
void ExecOrderReturn(MCAblSymbol* routineIdPtr, int32_t returnValue);
/// <summary>ABL return (2).</summary>
void ExecStdReturn(MCAblSymbol* routineIdPtr);
/// <summary>ABL print (3).</summary>
void ExecStdPrint(MCAblSymbol* routineIdPtr);
/// <summary>ABL concat (4).</summary>
MCAblType* ExecStdConcat(MCAblSymbol* routineIdPtr);
/// <summary>ABL abs (5).</summary>
MCAblType* ExecStdAbs(MCAblSymbol* routineIdPtr);
/// <summary>ABL round (6).</summary>
MCAblType* ExecStdRound(MCAblSymbol* routineIdPtr);
/// <summary>ABL sqrt (7).</summary>
MCAblType* ExecStdSqrt(MCAblSymbol* routineIdPtr);
/// <summary>ABL trunc (8).</summary>
MCAblType* ExecStdTrunc(MCAblSymbol* routineIdPtr);
/// <summary>ABL random (9).</summary>
MCAblType* ExecStdRandom(MCAblSymbol* routineIdPtr);
/// <summary>ABL getmodulehandle (13).</summary>
MCAblType* ExecStdGetModHandle(MCAblSymbol* routineIdPtr);
/// <summary>ABL getmodulename (14).</summary>
MCAblType* ExecStdGetModName(MCAblSymbol* routineIdPtr);
/// <summary>ABL setmodulename (15).</summary>
void ExecStdSetModName(MCAblSymbol* routineIdPtr);
/// <summary>ABL setmaxloops (10): MaxLoopIterations = argument + 1.</summary>
MCAblType* ExecStdSetMaxLoops(MCAblSymbol* routineIdPtr);
/// <summary>ABL fatal (11).</summary>
MCAblType* ExecStdFatal(MCAblSymbol* routineIdPtr);
/// <summary>ABL assert (12).</summary>
MCAblType* ExecStdAssert(MCAblSymbol* routineIdPtr);
/// <summary>ABL getid (19).</summary>
MCAblType* ExecHbGetId(MCAblSymbol* routineIdPtr);
/// <summary>ABL gettime (20).</summary>
MCAblType* ExecHbGetTime(MCAblSymbol* routineIdPtr);
/// <summary>ABL gettimeleft (21).</summary>
MCAblType* ExecHbGetTimeLeft(MCAblSymbol* routineIdPtr);
/// <summary>ABL gettarget (40).</summary>
MCAblType* ExecHbGetTarget(MCAblSymbol* routineIdPtr);
/// <summary>ABL settarget (41).</summary>
void ExecHbSetTarget(MCAblSymbol* routineIdPtr);
/// <summary>ABL selectunit (23).</summary>
MCAblType* ExecHbSelectUnit(MCAblSymbol* routineIdPtr);
/// <summary>ABL selectobject (25).</summary>
MCAblType* ExecHbSelectObject(MCAblSymbol* routineIdPtr);
/// <summary>ABL selectwarrior (24).</summary>
MCAblType* ExecHbSelectWarrior(MCAblSymbol* routineIdPtr);
/// <summary>ABL getwarriorstatus (22).</summary>
MCAblType* ExecHbGetWarriorStatus(MCAblSymbol* routineIdPtr);
/// <summary>ABL getcontacts (26).</summary>
MCAblType* ExecHbGetContacts(MCAblSymbol* routineIdPtr);
/// <summary>ABL getenemycount (27).</summary>
MCAblType* ExecHbGetEnemyCount(MCAblSymbol* routineIdPtr);
/// <summary>ABL selectcontact (28).</summary>
MCAblType* ExecHbSelectContact(MCAblSymbol* routineIdPtr);
/// <summary>ABL iscontact (30).</summary>
MCAblType* ExecHbIsContact(MCAblSymbol* routineIdPtr);
/// <summary>ABL getcontactid (29).</summary>
MCAblType* ExecHbGetContactId(MCAblSymbol* routineIdPtr);
/// <summary>ABL getcontactstatus (31).</summary>
MCAblType* ExecHbGetContactStatus(MCAblSymbol* routineIdPtr);
/// <summary>ABL getcontactrelativeposition (32).</summary>
MCAblType* ExecHbGetContactRelativePosition(MCAblSymbol* routineIdPtr);
/// <summary>ABL setpotentialcontact (160).</summary>
MCAblType* ExecHbSetPotentialContact(MCAblSymbol* routineIdPtr);
/// <summary>ABL getweaponsready (42), getweaponslocked (43), getweaponsinrange (44), by <paramref name="key"/>.
/// </summary>
MCAblType* ExecHbGetWeapons(MCAblSymbol* routineIdPtr, MCAblRoutineKey key);
/// <summary>ABL getweaponshots (45).</summary>
MCAblType* ExecHbGetWeaponShots(MCAblSymbol* routineIdPtr);
/// <summary>ABL getweaponranges (46).</summary>
void ExecHbGetWeaponRanges(MCAblSymbol* routineIdPtr);
/// <summary>ABL setmovegoal (62).</summary>
MCAblType* ExecHbSetMoveGoal(MCAblSymbol* routineIdPtr);
/// <summary>ABL getchallenger (51).</summary>
MCAblType* ExecHbGetChallenger(MCAblSymbol* routineIdPtr);
/// <summary>ABL getfireranges (52).</summary>
MCAblType* ExecHbGetFireRanges(MCAblSymbol* routineIdPtr);
/// <summary>ABL getattackers (53).</summary>
MCAblType* ExecHbGetAttackers(MCAblSymbol* routineIdPtr);
/// <summary>ABL getattackerinfo (54).</summary>
MCAblType* ExecHbGetAttackerInfo(MCAblSymbol* routineIdPtr);
/// <summary>ABL gettimewithoutorders (56).</summary>
MCAblType* ExecHbGetTimeWithoutOrders(MCAblSymbol* routineIdPtr);
/// <summary>ABL setchallenger (55).</summary>
MCAblType* ExecHbSetChallenger(MCAblSymbol* routineIdPtr);
/// <summary>ABL setintegermemory (63).</summary>
void ExecHbSetMemoryInteger(MCAblSymbol* routineIdPtr);
/// <summary>ABL setrealmemory (64).</summary>
void ExecHbSetMemoryReal(MCAblSymbol* routineIdPtr);
/// <summary>ABL hasmovegoal (72).</summary>
MCAblType* ExecHbHasMoveGoal(MCAblSymbol* routineIdPtr);
/// <summary>ABL hasmovepath (73).</summary>
MCAblType* ExecHbHasMovePath(MCAblSymbol* routineIdPtr);
/// <summary>ABL sortweapons (74).</summary>
void ExecHbSortWeapons(MCAblSymbol* routineIdPtr);
/// <summary>ABL getobjectposition (47).</summary>
MCAblType* ExecHbGetObjectPosition(MCAblSymbol* routineIdPtr);
/// <summary>ABL getvisualrange (77).</summary>
MCAblType* ExecHbGetVisualRange(MCAblSymbol* routineIdPtr);
/// <summary>ABL getintegermemory (48).</summary>
MCAblType* ExecHbGetMemoryInteger(MCAblSymbol* routineIdPtr);
/// <summary>ABL getrealmemory (49).</summary>
MCAblType* ExecHbGetMemoryReal(MCAblSymbol* routineIdPtr);
/// <summary>ABL getalarmtriggers (50).</summary>
MCAblType* ExecHbGetAlarmTriggers(MCAblSymbol* routineIdPtr);
/// <summary>ABL getunitmates (78).</summary>
MCAblType* ExecHbGetUnitMates(MCAblSymbol* routineIdPtr);
/// <summary>ABL gettacorder (79).</summary>
MCAblType* ExecHbGetTacOrder(MCAblSymbol* routineIdPtr);
/// <summary>ABL getlasttacorder (80).</summary>
MCAblType* ExecHbGetLastTacOrder(MCAblSymbol* routineIdPtr);
/// <summary>ABL setordermode (81).</summary>
MCAblType* ExecHbSetOrderMode(MCAblSymbol* routineIdPtr);
/// <summary>ABL orderwait (83).</summary>
MCAblType* ExecHbWait(MCAblSymbol* routineIdPtr);
/// <summary>ABL setattackradius (102).</summary>
MCAblType* ExecHbSetAttackRadius(MCAblSymbol* routineIdPtr);
/// <summary>ABL ordermoveto (84).</summary>
MCAblType* ExecHbMoveToPoint(MCAblSymbol* routineIdPtr);
/// <summary>ABL ordermovetoobject (85).</summary>
MCAblType* ExecHbMoveToObject(MCAblSymbol* routineIdPtr);
/// <summary>ABL ordermovetocontact (86).</summary>
MCAblType* ExecHbMoveToContact(MCAblSymbol* routineIdPtr);
/// <summary>ABL orderpowerdown (90).</summary>
MCAblType* ExecHbOrderPowerDown(MCAblSymbol* routineIdPtr);
/// <summary>ABL orderpowerup (89). (ordertraversepath, orderpatrolpath, attackclosesttarget, attackperorders,
/// retreat and fireuponenemyfireonly do nothing at all in the dispatch.)</summary>
MCAblType* ExecHbOrderPowerUp(MCAblSymbol* routineIdPtr);
/// <summary>ABL orderattackobject (91).</summary>
MCAblType* ExecHbOrderAttackObject(MCAblSymbol* routineIdPtr);
/// <summary>ABL orderattackcontact (92).</summary>
MCAblType* ExecHbOrderAttackContact(MCAblSymbol* routineIdPtr);
/// <summary>ABL ordertest (103).</summary>
MCAblType* ExecHbOrderTest(MCAblSymbol* routineIdPtr);
/// <summary>ABL playsmacker (104).</summary>
MCAblType* ExecHbPlaySmacker(MCAblSymbol* routineIdPtr);
/// <summary>ABL objectchangesides (106).</summary>
void ExecHbObjectChangeSides(MCAblSymbol* routineIdPtr);
/// <summary>ABL distancetoobject (107).</summary>
MCAblType* ExecHbDistanceToObject(MCAblSymbol* routineIdPtr);
/// <summary>ABL distancetoposition (108).</summary>
MCAblType* ExecHbDistanceToPosition(MCAblSymbol* routineIdPtr);
/// <summary>ABL objectsuicide (109).</summary>
void ExecHbObjectSuicide(MCAblSymbol* routineIdPtr);
/// <summary>ABL objectcreate (110).</summary>
MCAblType* ExecHbObjectCreate(MCAblSymbol* routineIdPtr);
/// <summary>ABL objectexists (111).</summary>
MCAblType* ExecHbObjectExists(MCAblSymbol* routineIdPtr);
/// <summary>ABL objectstatus (112).</summary>
MCAblType* ExecHbObjectStatus(MCAblSymbol* routineIdPtr);
/// <summary>ABL objectstatuscount (138): counts by status the objects of a part, a commander's (1..32) or a
/// group's, into an array.</summary>
MCAblType* ExecHbObjectStatusCount(MCAblSymbol* routineIdPtr);
/// <summary>ABL objectvisible (113).</summary>
MCAblType* ExecHbObjectVisible(MCAblSymbol* routineIdPtr);
/// <summary>ABL objectside (115).</summary>
MCAblType* ExecHbObjectSide(MCAblSymbol* routineIdPtr);
/// <summary>ABL objectcommander (116).</summary>
MCAblType* ExecHbObjectCommander(MCAblSymbol* routineIdPtr);
/// <summary>ABL objectclass (114).</summary>
MCAblType* ExecHbObjectClass(MCAblSymbol* routineIdPtr);
/// <summary>ABL inarea (139).</summary>
MCAblType* ExecHbInArea(MCAblSymbol* routineIdPtr);
/// <summary>ABL settimer (117).</summary>
MCAblType* ExecHbSetTimer(MCAblSymbol* routineIdPtr);
/// <summary>ABL checktimer (118).</summary>
MCAblType* ExecHbChkTimer(MCAblSymbol* routineIdPtr);
/// <summary>ABL endtimer (119).</summary>
void ExecHbEndTimer(MCAblSymbol* routineIdPtr);
/// <summary>ABL setobjectivetimer (120).</summary>
MCAblType* ExecHbSetObjectiveTimer(MCAblSymbol* routineIdPtr);
/// <summary>ABL checkobjectivetimer (121).</summary>
MCAblType* ExecHbCheckObjectiveTimer(MCAblSymbol* routineIdPtr);
/// <summary>ABL setobjectivestatus (122).</summary>
MCAblType* ExecHbSetObjectiveStatus(MCAblSymbol* routineIdPtr);
/// <summary>ABL checkobjectivestatus (123).</summary>
MCAblType* ExecHbCheckObjectiveStatus(MCAblSymbol* routineIdPtr);
/// <summary>ABL setobjectivetype (124).</summary>
MCAblType* ExecHbSetObjectiveType(MCAblSymbol* routineIdPtr);
/// <summary>ABL checkobjectivetype (125).</summary>
MCAblType* ExecHbCheckObjectiveType(MCAblSymbol* routineIdPtr);
/// <summary>ABL playdigitalmusic (126).</summary>
MCAblType* ExecHbPlayDigitalMusic(MCAblSymbol* routineIdPtr);
/// <summary>ABL stopmusic (127).</summary>
MCAblType* ExecHbStopMusic(MCAblSymbol* routineIdPtr);
/// <summary>ABL playsoundeffect (128).</summary>
MCAblType* ExecHbPlaySoundEffect(MCAblSymbol* routineIdPtr);
/// <summary>ABL playvideo (129).</summary>
MCAblType* ExecHbPlayVideo(MCAblSymbol* routineIdPtr);
/// <summary>ABL setradio (57).</summary>
void ExecHbSetRadio(MCAblSymbol* routineIdPtr);
/// <summary>ABL playspeech (130).</summary>
MCAblType* ExecHbPlaySpeech(MCAblSymbol* routineIdPtr);
/// <summary>ABL playbetty (131).</summary>
MCAblType* ExecHbPlayBetty(MCAblSymbol* routineIdPtr);
/// <summary>ABL setobjectactive (132).</summary>
MCAblType* ExecHbSetObjActive(MCAblSymbol* routineIdPtr);
/// <summary>ABL orderwithdraw (97).</summary>
MCAblType* ExecHbObjWithdraw(MCAblSymbol* routineIdPtr);
/// <summary>ABL objectinwithdrawal (133).</summary>
MCAblType* ExecHbObjInWithdraw(MCAblSymbol* routineIdPtr);
/// <summary>ABL objecttypeid (134).</summary>
MCAblType* ExecHbObjTypeId(MCAblSymbol* routineIdPtr);
/// <summary>ABL getterrainobjectpartid (135).</summary>
MCAblType* ExecHbTerrainObjectId(MCAblSymbol* routineIdPtr);
/// <summary>ABL getvehiclepartid (136).</summary>
MCAblType* ExecHbVehicleId(MCAblSymbol* routineIdPtr);
/// <summary>ABL getweaponammo (137).</summary>
MCAblType* ExecHbGetWeaponAmmo(MCAblSymbol* routineIdPtr);
/// <summary>ABL getsensorsworking (142).</summary>
MCAblType* ExecHbGetSensors(MCAblSymbol* routineIdPtr);
/// <summary>ABL getcurrentbrvalue (143).</summary>
MCAblType* ExecHbGetBRValue(MCAblSymbol* routineIdPtr);
/// <summary>ABL setcurrentbrvalue (144) by its name; never called (the key has no dispatch case).</summary>
MCAblType* ExecHbSetBRValue(MCAblSymbol* routineIdPtr);
/// <summary>ABL getarmorpts (145): a mover's armor points left (0 for other objects).</summary>
MCAblType* ExecHbGetArmorPts(MCAblSymbol* routineIdPtr);
/// <summary>ABL getmaxarmor (146) by its name; never called (the key has no dispatch case).</summary>
MCAblType* ExecHbGetMaxArmor(MCAblSymbol* routineIdPtr);
/// <summary>ABL getpilotid (147).</summary>
MCAblType* ExecHbGetPilotId(MCAblSymbol* routineIdPtr);
/// <summary>ABL getpilotwounds (148).</summary>
MCAblType* ExecHbGetPilotWounds(MCAblSymbol* routineIdPtr);
/// <summary>ABL setpilotwounds (149).</summary>
MCAblType* ExecHbSetPilotWounds(MCAblSymbol* routineIdPtr);
/// <summary>ABL getobjectactive (150).</summary>
MCAblType* ExecHbGetObjActive(MCAblSymbol* routineIdPtr);
/// <summary>ABL getobjectdamage (153).</summary>
MCAblType* ExecHbGetObjDamage(MCAblSymbol* routineIdPtr);
/// <summary>The dispatch sends ABL getobjectmaxdmg (152) here.</summary>
MCAblType* ExecHbGetObjDmgPts(MCAblSymbol* routineIdPtr);
/// <summary>
/// The damage that destroys an object, by its name. Never called: getobjectdmgpts (151) has no dispatch case
/// (it is an undefined-routine Fatal) and getobjectmaxdmg runs execHbGetObjDmgPts.
/// </summary>
MCAblType* ExecHbGetMaxDmg(MCAblSymbol* routineIdPtr);
/// <summary>ABL setobjectdamage (154).</summary>
void ExecHbSetObjDamage(MCAblSymbol* routineIdPtr);
/// <summary>ABL damageobject (101).</summary>
MCAblType* ExecHbDamageObject(MCAblSymbol* routineIdPtr);
/// <summary>ABL getglobalvalue (155).</summary>
MCAblType* ExecHbGetGlobalValue(MCAblSymbol* routineIdPtr);
/// <summary>ABL setglobalvalue (156).</summary>
void ExecHbSetGlobalValue(MCAblSymbol* routineIdPtr);
/// <summary>ABL setobjectivepos (157).</summary>
void ExecHbSetObjectivePos(MCAblSymbol* routineIdPtr);
/// <summary>ABL settonnage (162).</summary>
void ExecHbSetTonnage(MCAblSymbol* routineIdPtr);
/// <summary>ABL setsensorrange (161).</summary>
void ExecHbSetSensorRange(MCAblSymbol* routineIdPtr);
/// <summary>ABL setexplosiondamage (164).</summary>
void ExecHbSetExplDmg(MCAblSymbol* routineIdPtr);
/// <summary>ABL setexplosionradius (165).</summary>
void ExecHbSetExplRad(MCAblSymbol* routineIdPtr);
/// <summary>ABL setsalvage (167).</summary>
MCAblType* ExecHbSetSalvage(MCAblSymbol* routineIdPtr);
/// <summary>ABL setsalvagestatus (168).</summary>
MCAblType* ExecHbSetSalvageStatus(MCAblSymbol* routineIdPtr);
/// <summary>ABL setanimation (169).</summary>
void ExecHbSetAnimation(MCAblSymbol* routineIdPtr);
/// <summary>ABL playwavefile (163).</summary>
void ExecHbPlayWave(MCAblSymbol* routineIdPtr);
/// <summary>ABL setrevealed (170).</summary>
void ExecHbSetRevealed(MCAblSymbol* routineIdPtr);
/// <summary>ABL getsalvage (166).</summary>
void ExecHbGetSalvage(MCAblSymbol* routineIdPtr);
/// <summary>ABL orderrefit (171).</summary>
void ExecHbRefit(MCAblSymbol* routineIdPtr);
/// <summary>ABL setcaptured (173).</summary>
void ExecHbSetCaptured(MCAblSymbol* routineIdPtr);
/// <summary>ABL ordercapture (172).</summary>
void ExecHbCaptureObject(MCAblSymbol* routineIdPtr);
/// <summary>ABL setcaptureable (174).</summary>
void ExecHbSetCaptureable(MCAblSymbol* routineIdPtr);
/// <summary>ABL iscaptured (175).</summary>
MCAblType* ExecHbIsCaptured(MCAblSymbol* routineIdPtr);
/// <summary>ABL iscapturable (176).</summary>
MCAblType* ExecHbIsCapturable(MCAblSymbol* routineIdPtr);
/// <summary>ABL wasevercapturable (177).</summary>
MCAblType* ExecHbWasEverCapturable(MCAblSymbol* routineIdPtr);
/// <summary>ABL setbuildingname (178): gives a building (class 0x10) of the part the string table entry as its
/// name.</summary>
void ExecHbSetBuildingName(MCAblSymbol* routineIdPtr);
/// <summary>ABL callstrike (179).</summary>
void ExecHbCallStrike(MCAblSymbol* routineIdPtr);
/// <summary>ABL callstrikeex (188).</summary>
void ExecHbCallStrikeEx(MCAblSymbol* routineIdPtr);
/// <summary>ABL orderloadelementals (180).</summary>
void ExecHbLoadElementals(MCAblSymbol* routineIdPtr);
/// <summary>ABL orderdeployelementals (181).</summary>
void ExecHbDeployElementals(MCAblSymbol* routineIdPtr);
/// <summary>ABL addprisoner (182).</summary>
MCAblType* ExecHbAddPrisoner(MCAblSymbol* routineIdPtr);
/// <summary>ABL settrainspeed (183).</summary>
void ExecHbSetTrainSpeed(MCAblSymbol* routineIdPtr);
/// <summary>ABL lockgateopen (184).</summary>
void ExecHbLockGateOpen(MCAblSymbol* routineIdPtr);
/// <summary>ABL lockgateclosed (185).</summary>
void ExecHbLockGateClosed(MCAblSymbol* routineIdPtr);
/// <summary>ABL releasegatelock (186).</summary>
void ExecHbReleaseGateLock(MCAblSymbol* routineIdPtr);
/// <summary>ABL isgateopen (187).</summary>
MCAblType* ExecHbIsGateOpen(MCAblSymbol* routineIdPtr);
/// <summary>ABL getunitstatus (189).</summary>
MCAblType* ExecHbGetUnitStatus(MCAblSymbol* routineIdPtr);
/// <summary>ABL getrelativepositiontopoint (140).</summary>
void ExecHbRelPosPoint(MCAblSymbol* routineIdPtr);
/// <summary>ABL getrelativepositiontoobject (141).</summary>
void ExecHbRelPosObject(MCAblSymbol* routineIdPtr);
/// <summary>ABL repair (190).</summary>
void ExecHbRepair(MCAblSymbol* routineIdPtr);
/// <summary>ABL getrepairstate (192).</summary>
MCAblType* ExecHbGetRepairState(MCAblSymbol* routineIdPtr);
/// <summary>ABL isteamtargeting (193).</summary>
MCAblType* ExecHbIsTeamTargeting(MCAblSymbol* routineIdPtr);
/// <summary>ABL getfixed (191).</summary>
MCAblType* ExecHbGetFixed(MCAblSymbol* routineIdPtr);
/// <summary>Writes <see cref="MissionScriptMessageLog"/> to the debug output.</summary>
void DebugMissionScriptMessages();
/// <summary>ABL sendmessage (194).</summary>
void ExecHbSendMessage(MCAblSymbol* routineIdPtr);
/// <summary>ABL getmessage (195).</summary>
MCAblType* ExecHbGetMessage(MCAblSymbol* routineIdPtr);
/// <summary>ABL getstrikes (198).</summary>
MCAblType* ExecHbGetStrikes(MCAblSymbol* routineIdPtr);
/// <summary>ABL setstrikes (197).</summary>
void ExecHbSetStrikes(MCAblSymbol* routineIdPtr);
/// <summary>ABL addstrikes (200).</summary>
void ExecHbAddStrikes(MCAblSymbol* routineIdPtr);
/// <summary>ABL isserver (199).</summary>
MCAblType* ExecHbIsServer(MCAblSymbol* routineIdPtr);
/// <summary>ABL gethometeam (196).</summary>
MCAblType* ExecHbGetHomeTeam(MCAblSymbol* routineIdPtr);
/// <summary>Runs standard routine <paramref name="routineIdPtr"/> by its RoutineKey.</summary>
/// <returns>The routine's result type (null for none).</returns>
MCAblType* ExecStandardRoutineCall(MCAblSymbol* routineIdPtr);
