#pragma once

// Executing calls of ABL's standard routines: execStd* for the language built-ins, execHb* ("heartbeat") for the
// game functions unit brains and mission scripts call (ablstd.cpp compiles them). Each reads its arguments from the
// code segment (evaluating them onto the runtime stack), acts on the game, and leaves its result on top of the
// stack. execStandardRoutineCall dispatches on the routine's RoutineKey (ablsymt.h); the ABL name each one runs is
// given as "ABL name (key)".

#include "abl/ablscan.h"
#include "abl/ablsymt.h"

class MCGameObject;
class MCMechWarrior;
class MCMover;
class MCMoverGroup;

/// <summary>Where the current tactical order came from (the order's origin, set by the order code).</summary>
extern int TacOrderOrigin;
/// <summary>The code a routine's return jumps to: an end-of-routine token sequence.</summary>
extern MCTokenCodeType ExitRoutineCodeSegment[2];
/// <summary>The code an order's return jumps to.</summary>
extern MCTokenCodeType ExitOrderCodeSegment[2];
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
void ExecOrderReturn(MCSymTableNodePtr routineIdPtr, int32_t returnValue);
/// <summary>ABL return (2).</summary>
void ExecStdReturn(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL print (3).</summary>
void ExecStdPrint(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL concat (4).</summary>
MCTypePtr ExecStdConcat(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL abs (5).</summary>
MCTypePtr ExecStdAbs(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL round (6).</summary>
MCTypePtr ExecStdRound(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL sqrt (7).</summary>
MCTypePtr ExecStdSqrt(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL trunc (8).</summary>
MCTypePtr ExecStdTrunc(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL random (9).</summary>
MCTypePtr ExecStdRandom(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL getmodulehandle (13).</summary>
MCTypePtr ExecStdGetModHandle(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL getmodulename (14).</summary>
MCTypePtr ExecStdGetModName(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL setmodulename (15).</summary>
void ExecStdSetModName(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL setmaxloops (10): MaxLoopIterations = argument + 1.</summary>
MCTypePtr ExecStdSetMaxLoops(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL fatal (11).</summary>
MCTypePtr ExecStdFatal(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL assert (12).</summary>
MCTypePtr ExecStdAssert(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL getid (19).</summary>
MCTypePtr ExecHbGetId(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL gettime (20).</summary>
MCTypePtr ExecHbGetTime(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL gettimeleft (21).</summary>
MCTypePtr ExecHbGetTimeLeft(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL gettarget (40).</summary>
MCTypePtr ExecHbGetTarget(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL settarget (41).</summary>
void ExecHbSetTarget(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL selectunit (23).</summary>
MCTypePtr ExecHbSelectUnit(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL selectobject (25).</summary>
MCTypePtr ExecHbSelectObject(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL selectwarrior (24).</summary>
MCTypePtr ExecHbSelectWarrior(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL getwarriorstatus (22).</summary>
MCTypePtr ExecHbGetWarriorStatus(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL getcontacts (26).</summary>
MCTypePtr ExecHbGetContacts(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL getenemycount (27).</summary>
MCTypePtr ExecHbGetEnemyCount(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL selectcontact (28).</summary>
MCTypePtr ExecHbSelectContact(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL iscontact (30).</summary>
MCTypePtr ExecHbIsContact(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL getcontactid (29).</summary>
MCTypePtr ExecHbGetContactId(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL getcontactstatus (31).</summary>
MCTypePtr ExecHbGetContactStatus(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL getcontactrelativeposition (32).</summary>
MCTypePtr ExecHbGetContactRelativePosition(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL setpotentialcontact (160).</summary>
MCTypePtr ExecHbSetPotentialContact(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL getweaponsready (42), getweaponslocked (43), getweaponsinrange (44), by <paramref name="key"/>.
/// </summary>
MCTypePtr ExecHbGetWeapons(MCSymTableNodePtr routineIdPtr, int32_t key);
/// <summary>ABL getweaponshots (45).</summary>
MCTypePtr ExecHbGetWeaponShots(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL getweaponranges (46).</summary>
void ExecHbGetWeaponRanges(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL setmovegoal (62).</summary>
MCTypePtr ExecHbSetMoveGoal(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL getchallenger (51).</summary>
MCTypePtr ExecHbGetChallenger(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL getfireranges (52).</summary>
MCTypePtr ExecHbGetFireRanges(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL getattackers (53).</summary>
MCTypePtr ExecHbGetAttackers(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL getattackerinfo (54).</summary>
MCTypePtr ExecHbGetAttackerInfo(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL gettimewithoutorders (56).</summary>
MCTypePtr ExecHbGetTimeWithoutOrders(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL setchallenger (55).</summary>
MCTypePtr ExecHbSetChallenger(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL setintegermemory (63).</summary>
void ExecHbSetMemoryInteger(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL setrealmemory (64).</summary>
void ExecHbSetMemoryReal(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL hasmovegoal (72).</summary>
MCTypePtr ExecHbHasMoveGoal(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL hasmovepath (73).</summary>
MCTypePtr ExecHbHasMovePath(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL sortweapons (74).</summary>
void ExecHbSortWeapons(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL getobjectposition (47).</summary>
MCTypePtr ExecHbGetObjectPosition(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL getvisualrange (77).</summary>
MCTypePtr ExecHbGetVisualRange(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL getintegermemory (48).</summary>
MCTypePtr ExecHbGetMemoryInteger(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL getrealmemory (49).</summary>
MCTypePtr ExecHbGetMemoryReal(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL getalarmtriggers (50).</summary>
MCTypePtr ExecHbGetAlarmTriggers(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL getunitmates (78).</summary>
MCTypePtr ExecHbGetUnitMates(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL gettacorder (79).</summary>
MCTypePtr ExecHbGetTacOrder(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL getlasttacorder (80).</summary>
MCTypePtr ExecHbGetLastTacOrder(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL setordermode (81).</summary>
MCTypePtr ExecHbSetOrderMode(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL orderwait (83).</summary>
MCTypePtr ExecHbWait(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL setattackradius (102).</summary>
MCTypePtr ExecHbSetAttackRadius(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL ordermoveto (84).</summary>
MCTypePtr ExecHbMoveToPoint(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL ordermovetoobject (85).</summary>
MCTypePtr ExecHbMoveToObject(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL ordermovetocontact (86).</summary>
MCTypePtr ExecHbMoveToContact(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL orderpowerdown (90).</summary>
MCTypePtr ExecHbOrderPowerDown(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL orderpowerup (89). (ordertraversepath, orderpatrolpath, attackclosesttarget, attackperorders,
/// retreat and fireuponenemyfireonly do nothing at all in the dispatch.)</summary>
MCTypePtr ExecHbOrderPowerUp(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL orderattackobject (91).</summary>
MCTypePtr ExecHbOrderAttackObject(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL orderattackcontact (92).</summary>
MCTypePtr ExecHbOrderAttackContact(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL ordertest (103).</summary>
MCTypePtr ExecHbOrderTest(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL playsmacker (104).</summary>
MCTypePtr ExecHbPlaySmacker(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL objectchangesides (106).</summary>
void ExecHbObjectChangeSides(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL distancetoobject (107).</summary>
MCTypePtr ExecHbDistanceToObject(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL distancetoposition (108).</summary>
MCTypePtr ExecHbDistanceToPosition(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL objectsuicide (109).</summary>
void ExecHbObjectSuicide(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL objectcreate (110).</summary>
MCTypePtr ExecHbObjectCreate(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL objectexists (111).</summary>
MCTypePtr ExecHbObjectExists(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL objectstatus (112).</summary>
MCTypePtr ExecHbObjectStatus(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL objectstatuscount (138): counts by status the objects of a part, a commander's (1..32) or a
/// group's, into an array.</summary>
MCTypePtr ExecHbObjectStatusCount(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL objectvisible (113).</summary>
MCTypePtr ExecHbObjectVisible(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL objectside (115).</summary>
MCTypePtr ExecHbObjectSide(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL objectcommander (116).</summary>
MCTypePtr ExecHbObjectCommander(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL objectclass (114).</summary>
MCTypePtr ExecHbObjectClass(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL inarea (139).</summary>
MCTypePtr ExecHbInArea(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL settimer (117).</summary>
MCTypePtr ExecHbSetTimer(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL checktimer (118).</summary>
MCTypePtr ExecHbChkTimer(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL endtimer (119).</summary>
void ExecHbEndTimer(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL setobjectivetimer (120).</summary>
MCTypePtr ExecHbSetObjectiveTimer(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL checkobjectivetimer (121).</summary>
MCTypePtr ExecHbCheckObjectiveTimer(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL setobjectivestatus (122).</summary>
MCTypePtr ExecHbSetObjectiveStatus(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL checkobjectivestatus (123).</summary>
MCTypePtr ExecHbCheckObjectiveStatus(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL setobjectivetype (124).</summary>
MCTypePtr ExecHbSetObjectiveType(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL checkobjectivetype (125).</summary>
MCTypePtr ExecHbCheckObjectiveType(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL playdigitalmusic (126).</summary>
MCTypePtr ExecHbPlayDigitalMusic(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL stopmusic (127).</summary>
MCTypePtr ExecHbStopMusic(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL playsoundeffect (128).</summary>
MCTypePtr ExecHbPlaySoundEffect(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL playvideo (129).</summary>
MCTypePtr ExecHbPlayVideo(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL setradio (57).</summary>
void ExecHbSetRadio(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL playspeech (130).</summary>
MCTypePtr ExecHbPlaySpeech(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL playbetty (131).</summary>
MCTypePtr ExecHbPlayBetty(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL setobjectactive (132).</summary>
MCTypePtr ExecHbSetObjActive(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL orderwithdraw (97).</summary>
MCTypePtr ExecHbObjWithdraw(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL objectinwithdrawal (133).</summary>
MCTypePtr ExecHbObjInWithdraw(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL objecttypeid (134).</summary>
MCTypePtr ExecHbObjTypeId(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL getterrainobjectpartid (135).</summary>
MCTypePtr ExecHbTerrainObjectId(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL getvehiclepartid (136).</summary>
MCTypePtr ExecHbVehicleId(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL getweaponammo (137).</summary>
MCTypePtr ExecHbGetWeaponAmmo(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL getsensorsworking (142).</summary>
MCTypePtr ExecHbGetSensors(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL getcurrentbrvalue (143).</summary>
MCTypePtr ExecHbGetBRValue(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL setcurrentbrvalue (144) by its name; never called (the key has no dispatch case).</summary>
MCTypePtr ExecHbSetBRValue(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL getarmorpts (145): a mover's armor points left (0 for other objects).</summary>
MCTypePtr ExecHbGetArmorPts(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL getmaxarmor (146) by its name; never called (the key has no dispatch case).</summary>
MCTypePtr ExecHbGetMaxArmor(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL getpilotid (147).</summary>
MCTypePtr ExecHbGetPilotId(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL getpilotwounds (148).</summary>
MCTypePtr ExecHbGetPilotWounds(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL setpilotwounds (149).</summary>
MCTypePtr ExecHbSetPilotWounds(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL getobjectactive (150).</summary>
MCTypePtr ExecHbGetObjActive(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL getobjectdamage (153).</summary>
MCTypePtr ExecHbGetObjDamage(MCSymTableNodePtr routineIdPtr);
/// <summary>The dispatch sends ABL getobjectmaxdmg (152) here.</summary>
MCTypePtr ExecHbGetObjDmgPts(MCSymTableNodePtr routineIdPtr);
/// <summary>
/// The damage that destroys an object, by its name. Never called: getobjectdmgpts (151) has no dispatch case
/// (it is an undefined-routine Fatal) and getobjectmaxdmg runs execHbGetObjDmgPts.
/// </summary>
MCTypePtr ExecHbGetMaxDmg(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL setobjectdamage (154).</summary>
void ExecHbSetObjDamage(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL damageobject (101).</summary>
MCTypePtr ExecHbDamageObject(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL getglobalvalue (155).</summary>
MCTypePtr ExecHbGetGlobalValue(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL setglobalvalue (156).</summary>
void ExecHbSetGlobalValue(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL setobjectivepos (157).</summary>
void ExecHbSetObjectivePos(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL settonnage (162).</summary>
void ExecHbSetTonnage(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL setsensorrange (161).</summary>
void ExecHbSetSensorRange(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL setexplosiondamage (164).</summary>
void ExecHbSetExplDmg(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL setexplosionradius (165).</summary>
void ExecHbSetExplRad(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL setsalvage (167).</summary>
MCTypePtr ExecHbSetSalvage(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL setsalvagestatus (168).</summary>
MCTypePtr ExecHbSetSalvageStatus(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL setanimation (169).</summary>
void ExecHbSetAnimation(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL playwavefile (163).</summary>
void ExecHbPlayWave(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL setrevealed (170).</summary>
void ExecHbSetRevealed(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL getsalvage (166).</summary>
void ExecHbGetSalvage(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL orderrefit (171).</summary>
void ExecHbRefit(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL setcaptured (173).</summary>
void ExecHbSetCaptured(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL ordercapture (172).</summary>
void ExecHbCaptureObject(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL setcaptureable (174).</summary>
void ExecHbSetCaptureable(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL iscaptured (175).</summary>
MCTypePtr ExecHbIsCaptured(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL iscapturable (176).</summary>
MCTypePtr ExecHbIsCapturable(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL wasevercapturable (177).</summary>
MCTypePtr ExecHbWasEverCapturable(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL setbuildingname (178): gives a building (class 0x10) of the part the string table entry as its
/// name.</summary>
void ExecHbSetBuildingName(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL callstrike (179).</summary>
void ExecHbCallStrike(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL callstrikeex (188).</summary>
void ExecHbCallStrikeEx(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL orderloadelementals (180).</summary>
void ExecHbLoadElementals(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL orderdeployelementals (181).</summary>
void ExecHbDeployElementals(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL addprisoner (182).</summary>
MCTypePtr ExecHbAddPrisoner(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL settrainspeed (183).</summary>
void ExecHbSetTrainSpeed(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL lockgateopen (184).</summary>
void ExecHbLockGateOpen(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL lockgateclosed (185).</summary>
void ExecHbLockGateClosed(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL releasegatelock (186).</summary>
void ExecHbReleaseGateLock(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL isgateopen (187).</summary>
MCTypePtr ExecHbIsGateOpen(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL getunitstatus (189).</summary>
MCTypePtr ExecHbGetUnitStatus(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL getrelativepositiontopoint (140).</summary>
void ExecHbRelPosPoint(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL getrelativepositiontoobject (141).</summary>
void ExecHbRelPosObject(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL repair (190).</summary>
void ExecHbRepair(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL getrepairstate (192).</summary>
MCTypePtr ExecHbGetRepairState(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL isteamtargeting (193).</summary>
MCTypePtr ExecHbIsTeamTargeting(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL getfixed (191).</summary>
MCTypePtr ExecHbGetFixed(MCSymTableNodePtr routineIdPtr);
/// <summary>Writes <see cref="MissionScriptMessageLog"/> to the debug output.</summary>
void DebugMissionScriptMessages();
/// <summary>ABL sendmessage (194).</summary>
void ExecHbSendMessage(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL getmessage (195).</summary>
MCTypePtr ExecHbGetMessage(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL getstrikes (198).</summary>
MCTypePtr ExecHbGetStrikes(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL setstrikes (197).</summary>
void ExecHbSetStrikes(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL addstrikes (200).</summary>
void ExecHbAddStrikes(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL isserver (199).</summary>
MCTypePtr ExecHbIsServer(MCSymTableNodePtr routineIdPtr);
/// <summary>ABL gethometeam (196).</summary>
MCTypePtr ExecHbGetHomeTeam(MCSymTableNodePtr routineIdPtr);
/// <summary>Runs standard routine <paramref name="routineIdPtr"/> by its RoutineKey.</summary>
/// <returns>The routine's result type (null for none).</returns>
MCTypePtr ExecStandardRoutineCall(MCSymTableNodePtr routineIdPtr);
