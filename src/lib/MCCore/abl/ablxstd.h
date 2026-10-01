#pragma once

// Executing calls of ABL's standard routines: execStd* for the language built-ins, execHb* ("heartbeat") for the
// game functions unit brains and mission scripts call (ablstd.cpp compiles them). Each reads its arguments from the
// code segment (evaluating them onto the runtime stack), acts on the game, and leaves its result on top of the
// stack. execStandardRoutineCall dispatches on the routine's RoutineKey (ablsymt.h); the ABL name each one runs is
// given as "ABL name (key)".

#include "abl/ablscan.h"
#include "abl/ablsymt.h"

class GameObject;
class MechWarrior;
class Mover;
class MoverGroup;

/// <summary>Where the current tactical order came from (the order's origin, set by the order code).</summary>
extern int TacOrderOrigin;
/// <summary>The code a routine's return jumps to: an end-of-routine token sequence.</summary>
extern TokenCodeType ExitRoutineCodeSegment[2];
/// <summary>The code an order's return jumps to.</summary>
extern TokenCodeType ExitOrderCodeSegment[2];
/// <summary>The mission script messages sent (sendmessage), each three shorts; DebugMissionScriptMessages writes
/// them out.</summary>
extern int16_t MissionScriptMessageLog[1000][3];
/// <summary>How many entries <see cref="MissionScriptMessageLog"/> holds.</summary>
extern int32_t NumMissionScriptMessages;
/// <summary>Scratch list the routines that scan units fill (getunitmates, getattackers, ...).</summary>
extern Mover* moverList[256];
/// <summary>Set while the running brain is a unit's (group) order rather than one pilot's.</summary>
extern int IsUnitOrder;
/// <summary>The group whose brain runs.</summary>
extern MoverGroup* CurGroup;
/// <summary>The object whose brain runs.</summary>
extern GameObject* CurObject;
/// <summary>The object class of <see cref="CurObject"/> while a brain runs.</summary>
extern int32_t CurObjectClass;
/// <summary>The alarm being handled while a pilot's brain runs its handler.</summary>
extern int32_t CurAlarm;
/// <summary>The pilot whose brain runs.</summary>
extern MechWarrior* CurWarrior;
/// <summary>The contact selectcontact picked.</summary>
extern GameObject* CurContact;
/// <summary>The multiplayer message code and parameter sendmessage passes on.</summary>
extern int32_t CurMultiplayCode;
extern int32_t CurMultiplayParam;
/// <summary>The mission's global values (getglobalvalue / setglobalvalue).</summary>
extern float globalMissionValues[50];

/// <summary>Returns from a tactical order with <paramref name="returnValue"/>: stores it in the order's frame, sets
/// the exit flags, and (unless it is 1) jumps to <see cref="ExitOrderCodeSegment"/>.</summary>
/// <remarks>MCX.EXE @ 0x0062ef30</remarks>
void execOrderReturn(SymTableNodePtr routineIdPtr, int32_t returnValue);
/// <summary>ABL return (2).</summary>
/// <remarks>MCX.EXE @ 0x0062efa0</remarks>
void execStdReturn(SymTableNodePtr routineIdPtr);
/// <summary>ABL print (3).</summary>
/// <remarks>MCX.EXE @ 0x0062f0c0</remarks>
void execStdPrint(SymTableNodePtr routineIdPtr);
/// <summary>ABL concat (4).</summary>
/// <remarks>MCX.EXE @ 0x0062f270</remarks>
TypePtr execStdConcat(SymTableNodePtr routineIdPtr);
/// <summary>ABL abs (5).</summary>
/// <remarks>MCX.EXE @ 0x0062f380</remarks>
TypePtr execStdAbs(SymTableNodePtr routineIdPtr);
/// <summary>ABL round (6).</summary>
/// <remarks>MCX.EXE @ 0x0062f3e0</remarks>
TypePtr execStdRound(SymTableNodePtr routineIdPtr);
/// <summary>ABL sqrt (7).</summary>
/// <remarks>MCX.EXE @ 0x0062f440</remarks>
TypePtr execStdSqrt(SymTableNodePtr routineIdPtr);
/// <summary>ABL trunc (8).</summary>
/// <remarks>MCX.EXE @ 0x0062f4b0</remarks>
TypePtr execStdTrunc(SymTableNodePtr routineIdPtr);
/// <summary>ABL random (9).</summary>
/// <remarks>MCX.EXE @ 0x0062f4f0</remarks>
TypePtr execStdRandom(SymTableNodePtr routineIdPtr);
/// <summary>ABL getmodulehandle (13).</summary>
/// <remarks>MCX.EXE @ 0x0062f530</remarks>
TypePtr execStdGetModHandle(SymTableNodePtr routineIdPtr);
/// <summary>ABL getmodulename (14).</summary>
/// <remarks>MCX.EXE @ 0x0062f550</remarks>
TypePtr execStdGetModName(SymTableNodePtr routineIdPtr);
/// <summary>ABL setmodulename (15).</summary>
/// <remarks>MCX.EXE @ 0x0062f560</remarks>
void execStdSetModName(SymTableNodePtr routineIdPtr);
/// <summary>ABL setmaxloops (10): MaxLoopIterations = argument + 1.</summary>
/// <remarks>MCX.EXE @ 0x0062f5a0 (unnamed in Ghidra; the name is the port's)</remarks>
TypePtr execStdSetMaxLoops(SymTableNodePtr routineIdPtr);
/// <summary>ABL fatal (11).</summary>
/// <remarks>MCX.EXE @ 0x0062f5d0</remarks>
TypePtr execStdFatal(SymTableNodePtr routineIdPtr);
/// <summary>ABL assert (12).</summary>
/// <remarks>MCX.EXE @ 0x0062f720</remarks>
TypePtr execStdAssert(SymTableNodePtr routineIdPtr);
/// <summary>ABL getid (19).</summary>
/// <remarks>MCX.EXE @ 0x0062f8a0</remarks>
TypePtr execHbGetId(SymTableNodePtr routineIdPtr);
/// <summary>ABL gettime (20).</summary>
/// <remarks>MCX.EXE @ 0x0062f8d0</remarks>
TypePtr execHbGetTime(SymTableNodePtr routineIdPtr);
/// <summary>ABL gettimeleft (21).</summary>
/// <remarks>MCX.EXE @ 0x0062f8f0</remarks>
TypePtr execHbGetTimeLeft(SymTableNodePtr routineIdPtr);
/// <summary>ABL gettarget (40).</summary>
/// <remarks>MCX.EXE @ 0x0062f950</remarks>
TypePtr execHbGetTarget(SymTableNodePtr routineIdPtr);
/// <summary>ABL settarget (41).</summary>
/// <remarks>MCX.EXE @ 0x0062fa30</remarks>
void execHbSetTarget(SymTableNodePtr routineIdPtr);
/// <summary>ABL selectunit (23).</summary>
/// <remarks>MCX.EXE @ 0x0062fbe0</remarks>
TypePtr execHbSelectUnit(SymTableNodePtr routineIdPtr);
/// <summary>ABL selectobject (25).</summary>
/// <remarks>MCX.EXE @ 0x0062fc10</remarks>
TypePtr execHbSelectObject(SymTableNodePtr routineIdPtr);
/// <summary>ABL selectwarrior (24).</summary>
/// <remarks>MCX.EXE @ 0x0062fc80</remarks>
TypePtr execHbSelectWarrior(SymTableNodePtr routineIdPtr);
/// <summary>ABL getwarriorstatus (22).</summary>
/// <remarks>MCX.EXE @ 0x0062fd00</remarks>
TypePtr execHbGetWarriorStatus(SymTableNodePtr routineIdPtr);
/// <summary>ABL getcontacts (26).</summary>
/// <remarks>MCX.EXE @ 0x0062fd60</remarks>
TypePtr execHbGetContacts(SymTableNodePtr routineIdPtr);
/// <summary>ABL getenemycount (27).</summary>
/// <remarks>MCX.EXE @ 0x0062fe10</remarks>
TypePtr execHbGetEnemyCount(SymTableNodePtr routineIdPtr);
/// <summary>ABL selectcontact (28).</summary>
/// <remarks>MCX.EXE @ 0x0062ff40</remarks>
TypePtr execHbSelectContact(SymTableNodePtr routineIdPtr);
/// <summary>ABL iscontact (30).</summary>
/// <remarks>MCX.EXE @ 0x0062fff0</remarks>
TypePtr execHbIsContact(SymTableNodePtr routineIdPtr);
/// <summary>ABL getcontactid (29).</summary>
/// <remarks>MCX.EXE @ 0x006300e0</remarks>
TypePtr execHbGetContactId(SymTableNodePtr routineIdPtr);
/// <summary>ABL getcontactstatus (31).</summary>
/// <remarks>MCX.EXE @ 0x00630110</remarks>
TypePtr execHbGetContactStatus(SymTableNodePtr routineIdPtr);
/// <summary>ABL getcontactrelativeposition (32).</summary>
/// <remarks>MCX.EXE @ 0x006301a0</remarks>
TypePtr execHbGetContactRelativePosition(SymTableNodePtr routineIdPtr);
/// <summary>ABL setpotentialcontact (160).</summary>
/// <remarks>MCX.EXE @ 0x00630290</remarks>
TypePtr execHbSetPotentialContact(SymTableNodePtr routineIdPtr);
/// <summary>ABL getweaponsready (42), getweaponslocked (43), getweaponsinrange (44), by <paramref name="key"/>.
/// </summary>
/// <remarks>MCX.EXE @ 0x006303f0</remarks>
TypePtr execHbGetWeapons(SymTableNodePtr routineIdPtr, int32_t key);
/// <summary>ABL getweaponshots (45).</summary>
/// <remarks>MCX.EXE @ 0x006304f0</remarks>
TypePtr execHbGetWeaponShots(SymTableNodePtr routineIdPtr);
/// <summary>ABL getweaponranges (46).</summary>
/// <remarks>MCX.EXE @ 0x00630550</remarks>
void execHbGetWeaponRanges(SymTableNodePtr routineIdPtr);
/// <summary>ABL setmovegoal (62).</summary>
/// <remarks>MCX.EXE @ 0x00630680</remarks>
TypePtr execHbSetMoveGoal(SymTableNodePtr routineIdPtr);
/// <summary>ABL getchallenger (51).</summary>
/// <remarks>MCX.EXE @ 0x00630730</remarks>
TypePtr execHbGetChallenger(SymTableNodePtr routineIdPtr);
/// <summary>ABL getfireranges (52).</summary>
/// <remarks>MCX.EXE @ 0x006307b0</remarks>
TypePtr execHbGetFireRanges(SymTableNodePtr routineIdPtr);
/// <summary>ABL getattackers (53).</summary>
/// <remarks>MCX.EXE @ 0x00630810</remarks>
TypePtr execHbGetAttackers(SymTableNodePtr routineIdPtr);
/// <summary>ABL getattackerinfo (54).</summary>
/// <remarks>MCX.EXE @ 0x00630890</remarks>
TypePtr execHbGetAttackerInfo(SymTableNodePtr routineIdPtr);
/// <summary>ABL gettimewithoutorders (56).</summary>
/// <remarks>MCX.EXE @ 0x006308f0</remarks>
TypePtr execHbGetTimeWithoutOrders(SymTableNodePtr routineIdPtr);
/// <summary>ABL setchallenger (55).</summary>
/// <remarks>MCX.EXE @ 0x00630940</remarks>
TypePtr execHbSetChallenger(SymTableNodePtr routineIdPtr);
/// <summary>ABL setintegermemory (63).</summary>
/// <remarks>MCX.EXE @ 0x00630a20</remarks>
void execHbSetMemoryInteger(SymTableNodePtr routineIdPtr);
/// <summary>ABL setrealmemory (64).</summary>
/// <remarks>MCX.EXE @ 0x00630a70</remarks>
void execHbSetMemoryReal(SymTableNodePtr routineIdPtr);
/// <summary>ABL hasmovegoal (72).</summary>
/// <remarks>MCX.EXE @ 0x00630ad0</remarks>
TypePtr execHbHasMoveGoal(SymTableNodePtr routineIdPtr);
/// <summary>ABL hasmovepath (73).</summary>
/// <remarks>MCX.EXE @ 0x00630b20</remarks>
TypePtr execHbHasMovePath(SymTableNodePtr routineIdPtr);
/// <summary>ABL sortweapons (74).</summary>
/// <remarks>MCX.EXE @ 0x00630b80</remarks>
void execHbSortWeapons(SymTableNodePtr routineIdPtr);
/// <summary>ABL getobjectposition (47).</summary>
/// <remarks>MCX.EXE @ 0x00630c30</remarks>
TypePtr execHbGetObjectPosition(SymTableNodePtr routineIdPtr);
/// <summary>ABL getvisualrange (77).</summary>
/// <remarks>MCX.EXE @ 0x00630d10</remarks>
TypePtr execHbGetVisualRange(SymTableNodePtr routineIdPtr);
/// <summary>ABL getintegermemory (48).</summary>
/// <remarks>MCX.EXE @ 0x00630d90</remarks>
TypePtr execHbGetMemoryInteger(SymTableNodePtr routineIdPtr);
/// <summary>ABL getrealmemory (49).</summary>
/// <remarks>MCX.EXE @ 0x00630dc0</remarks>
TypePtr execHbGetMemoryReal(SymTableNodePtr routineIdPtr);
/// <summary>ABL getalarmtriggers (50).</summary>
/// <remarks>MCX.EXE @ 0x00630df0</remarks>
TypePtr execHbGetAlarmTriggers(SymTableNodePtr routineIdPtr);
/// <summary>ABL getunitmates (78).</summary>
/// <remarks>MCX.EXE @ 0x00630e40</remarks>
TypePtr execHbGetUnitMates(SymTableNodePtr routineIdPtr);
/// <summary>ABL gettacorder (79).</summary>
/// <remarks>MCX.EXE @ 0x00630ff0</remarks>
TypePtr execHbGetTacOrder(SymTableNodePtr routineIdPtr);
/// <summary>ABL getlasttacorder (80).</summary>
/// <remarks>MCX.EXE @ 0x006310d0</remarks>
TypePtr execHbGetLastTacOrder(SymTableNodePtr routineIdPtr);
/// <summary>ABL setordermode (81).</summary>
/// <remarks>MCX.EXE @ 0x006311b0</remarks>
TypePtr execHbSetOrderMode(SymTableNodePtr routineIdPtr);
/// <summary>ABL orderwait (83).</summary>
/// <remarks>MCX.EXE @ 0x006311f0</remarks>
TypePtr execHbWait(SymTableNodePtr routineIdPtr);
/// <summary>ABL setattackradius (102).</summary>
/// <remarks>MCX.EXE @ 0x00631290</remarks>
TypePtr execHbSetAttackRadius(SymTableNodePtr routineIdPtr);
/// <summary>ABL ordermoveto (84).</summary>
/// <remarks>MCX.EXE @ 0x006312d0</remarks>
TypePtr execHbMoveToPoint(SymTableNodePtr routineIdPtr);
/// <summary>ABL ordermovetoobject (85).</summary>
/// <remarks>MCX.EXE @ 0x006313a0</remarks>
TypePtr execHbMoveToObject(SymTableNodePtr routineIdPtr);
/// <summary>ABL ordermovetocontact (86).</summary>
/// <remarks>MCX.EXE @ 0x00631480</remarks>
TypePtr execHbMoveToContact(SymTableNodePtr routineIdPtr);
/// <summary>ABL orderpowerdown (90).</summary>
/// <remarks>MCX.EXE @ 0x00631510</remarks>
TypePtr execHbOrderPowerDown(SymTableNodePtr routineIdPtr);
/// <summary>ABL orderpowerup (89). (ordertraversepath, orderpatrolpath, attackclosesttarget, attackperorders,
/// retreat and fireuponenemyfireonly do nothing at all in the dispatch.)</summary>
/// <remarks>MCX.EXE @ 0x00631570</remarks>
TypePtr execHbOrderPowerUp(SymTableNodePtr routineIdPtr);
/// <summary>ABL orderattackobject (91).</summary>
/// <remarks>MCX.EXE @ 0x006315d0</remarks>
TypePtr execHbOrderAttackObject(SymTableNodePtr routineIdPtr);
/// <summary>ABL orderattackcontact (92).</summary>
/// <remarks>MCX.EXE @ 0x00631700</remarks>
TypePtr execHbOrderAttackContact(SymTableNodePtr routineIdPtr);
/// <summary>ABL ordertest (103).</summary>
/// <remarks>MCX.EXE @ 0x006317b0</remarks>
TypePtr execHbOrderTest(SymTableNodePtr routineIdPtr);
/// <summary>ABL playsmacker (104).</summary>
/// <remarks>MCX.EXE @ 0x006317d0</remarks>
TypePtr execHbPlaySmacker(SymTableNodePtr routineIdPtr);
/// <summary>ABL objectchangesides (106).</summary>
/// <remarks>MCX.EXE @ 0x00631800</remarks>
void execHbObjectChangeSides(SymTableNodePtr routineIdPtr);
/// <summary>ABL distancetoobject (107).</summary>
/// <remarks>MCX.EXE @ 0x00631890</remarks>
TypePtr execHbDistanceToObject(SymTableNodePtr routineIdPtr);
/// <summary>ABL distancetoposition (108).</summary>
/// <remarks>MCX.EXE @ 0x00631b20</remarks>
TypePtr execHbDistanceToPosition(SymTableNodePtr routineIdPtr);
/// <summary>ABL objectsuicide (109).</summary>
/// <remarks>MCX.EXE @ 0x00631da0</remarks>
void execHbObjectSuicide(SymTableNodePtr routineIdPtr);
/// <summary>ABL objectcreate (110).</summary>
/// <remarks>MCX.EXE @ 0x00631f20</remarks>
TypePtr execHbObjectCreate(SymTableNodePtr routineIdPtr);
/// <summary>ABL objectexists (111).</summary>
/// <remarks>MCX.EXE @ 0x00631fb0</remarks>
TypePtr execHbObjectExists(SymTableNodePtr routineIdPtr);
/// <summary>ABL objectstatus (112).</summary>
/// <remarks>MCX.EXE @ 0x006320d0</remarks>
TypePtr execHbObjectStatus(SymTableNodePtr routineIdPtr);
/// <summary>ABL objectstatuscount (138): counts by status the objects of a part, a commander's (1..32) or a
/// group's, into an array.</summary>
/// <remarks>MCX.EXE @ 0x00632250 (unnamed in Ghidra; the name is the port's)</remarks>
TypePtr execHbObjectStatusCount(SymTableNodePtr routineIdPtr);
/// <summary>ABL objectvisible (113).</summary>
/// <remarks>MCX.EXE @ 0x006323d0</remarks>
TypePtr execHbObjectVisible(SymTableNodePtr routineIdPtr);
/// <summary>ABL objectside (115).</summary>
/// <remarks>MCX.EXE @ 0x006324c0</remarks>
TypePtr execHbObjectSide(SymTableNodePtr routineIdPtr);
/// <summary>ABL objectcommander (116).</summary>
/// <remarks>MCX.EXE @ 0x00632530</remarks>
TypePtr execHbObjectCommander(SymTableNodePtr routineIdPtr);
/// <summary>ABL objectclass (114).</summary>
/// <remarks>MCX.EXE @ 0x006325a0</remarks>
TypePtr execHbObjectClass(SymTableNodePtr routineIdPtr);
/// <summary>ABL inarea (139).</summary>
/// <remarks>MCX.EXE @ 0x00632600</remarks>
TypePtr execHbInArea(SymTableNodePtr routineIdPtr);
/// <summary>ABL settimer (117).</summary>
/// <remarks>MCX.EXE @ 0x00632960</remarks>
TypePtr execHbSetTimer(SymTableNodePtr routineIdPtr);
/// <summary>ABL checktimer (118).</summary>
/// <remarks>MCX.EXE @ 0x006329e0</remarks>
TypePtr execHbChkTimer(SymTableNodePtr routineIdPtr);
/// <summary>ABL endtimer (119).</summary>
/// <remarks>MCX.EXE @ 0x00632a60</remarks>
void execHbEndTimer(SymTableNodePtr routineIdPtr);
/// <summary>ABL setobjectivetimer (120).</summary>
/// <remarks>MCX.EXE @ 0x00632aa0</remarks>
TypePtr execHbSetObjectiveTimer(SymTableNodePtr routineIdPtr);
/// <summary>ABL checkobjectivetimer (121).</summary>
/// <remarks>MCX.EXE @ 0x00632b10</remarks>
TypePtr execHbCheckObjectiveTimer(SymTableNodePtr routineIdPtr);
/// <summary>ABL setobjectivestatus (122).</summary>
/// <remarks>MCX.EXE @ 0x00632b50</remarks>
TypePtr execHbSetObjectiveStatus(SymTableNodePtr routineIdPtr);
/// <summary>ABL checkobjectivestatus (123).</summary>
/// <remarks>MCX.EXE @ 0x00632ba0</remarks>
TypePtr execHbCheckObjectiveStatus(SymTableNodePtr routineIdPtr);
/// <summary>ABL setobjectivetype (124).</summary>
/// <remarks>MCX.EXE @ 0x00632be0</remarks>
TypePtr execHbSetObjectiveType(SymTableNodePtr routineIdPtr);
/// <summary>ABL checkobjectivetype (125).</summary>
/// <remarks>MCX.EXE @ 0x00632c30</remarks>
TypePtr execHbCheckObjectiveType(SymTableNodePtr routineIdPtr);
/// <summary>ABL playdigitalmusic (126).</summary>
/// <remarks>MCX.EXE @ 0x00632c70</remarks>
TypePtr execHbPlayDigitalMusic(SymTableNodePtr routineIdPtr);
/// <summary>ABL stopmusic (127).</summary>
/// <remarks>MCX.EXE @ 0x00632cb0</remarks>
TypePtr execHbStopMusic(SymTableNodePtr routineIdPtr);
/// <summary>ABL playsoundeffect (128).</summary>
/// <remarks>MCX.EXE @ 0x00632ce0</remarks>
TypePtr execHbPlaySoundEffect(SymTableNodePtr routineIdPtr);
/// <summary>ABL playvideo (129).</summary>
/// <remarks>MCX.EXE @ 0x00632d20</remarks>
TypePtr execHbPlayVideo(SymTableNodePtr routineIdPtr);
/// <summary>ABL setradio (57).</summary>
/// <remarks>MCX.EXE @ 0x00632d60</remarks>
void execHbSetRadio(SymTableNodePtr routineIdPtr);
/// <summary>ABL playspeech (130).</summary>
/// <remarks>MCX.EXE @ 0x00632e00</remarks>
TypePtr execHbPlaySpeech(SymTableNodePtr routineIdPtr);
/// <summary>ABL playbetty (131).</summary>
/// <remarks>MCX.EXE @ 0x00632e80</remarks>
TypePtr execHbPlayBetty(SymTableNodePtr routineIdPtr);
/// <summary>ABL setobjectactive (132).</summary>
/// <remarks>MCX.EXE @ 0x00632ec0</remarks>
TypePtr execHbSetObjActive(SymTableNodePtr routineIdPtr);
/// <summary>ABL orderwithdraw (97).</summary>
/// <remarks>MCX.EXE @ 0x00632fd0</remarks>
TypePtr execHbObjWithdraw(SymTableNodePtr routineIdPtr);
/// <summary>ABL objectinwithdrawal (133).</summary>
/// <remarks>MCX.EXE @ 0x00633070</remarks>
TypePtr execHbObjInWithdraw(SymTableNodePtr routineIdPtr);
/// <summary>ABL objecttypeid (134).</summary>
/// <remarks>MCX.EXE @ 0x00633140</remarks>
TypePtr execHbObjTypeId(SymTableNodePtr routineIdPtr);
/// <summary>ABL getterrainobjectpartid (135).</summary>
/// <remarks>MCX.EXE @ 0x006331a0</remarks>
TypePtr execHbTerrainObjectId(SymTableNodePtr routineIdPtr);
/// <summary>ABL getvehiclepartid (136).</summary>
/// <remarks>MCX.EXE @ 0x006331f0</remarks>
TypePtr execHbVehicleId(SymTableNodePtr routineIdPtr);
/// <summary>ABL getweaponammo (137).</summary>
/// <remarks>MCX.EXE @ 0x00633230</remarks>
TypePtr execHbGetWeaponAmmo(SymTableNodePtr routineIdPtr);
/// <summary>ABL getsensorsworking (142).</summary>
/// <remarks>MCX.EXE @ 0x006332e0</remarks>
TypePtr execHbGetSensors(SymTableNodePtr routineIdPtr);
/// <summary>ABL getcurrentbrvalue (143).</summary>
/// <remarks>MCX.EXE @ 0x00633360</remarks>
TypePtr execHbGetBRValue(SymTableNodePtr routineIdPtr);
/// <summary>ABL setcurrentbrvalue (144) by its name; never called (the key has no dispatch case).</summary>
/// <remarks>MCX.EXE @ 0x006333d0</remarks>
TypePtr execHbSetBRValue(SymTableNodePtr routineIdPtr);
/// <summary>ABL getarmorpts (145): a mover's armor points left (0 for other objects).</summary>
/// <remarks>MCX.EXE @ 0x00633440 (unnamed in Ghidra; the name is the port's)</remarks>
TypePtr execHbGetArmorPts(SymTableNodePtr routineIdPtr);
/// <summary>ABL getmaxarmor (146) by its name; never called (the key has no dispatch case).</summary>
/// <remarks>MCX.EXE @ 0x006334e0</remarks>
TypePtr execHbGetMaxArmor(SymTableNodePtr routineIdPtr);
/// <summary>ABL getpilotid (147).</summary>
/// <remarks>MCX.EXE @ 0x00633570</remarks>
TypePtr execHbGetPilotId(SymTableNodePtr routineIdPtr);
/// <summary>ABL getpilotwounds (148).</summary>
/// <remarks>MCX.EXE @ 0x006335f0</remarks>
TypePtr execHbGetPilotWounds(SymTableNodePtr routineIdPtr);
/// <summary>ABL setpilotwounds (149).</summary>
/// <remarks>MCX.EXE @ 0x00633670</remarks>
TypePtr execHbSetPilotWounds(SymTableNodePtr routineIdPtr);
/// <summary>ABL getobjectactive (150).</summary>
/// <remarks>MCX.EXE @ 0x00633710</remarks>
TypePtr execHbGetObjActive(SymTableNodePtr routineIdPtr);
/// <summary>ABL getobjectdamage (153).</summary>
/// <remarks>MCX.EXE @ 0x006337e0</remarks>
TypePtr execHbGetObjDamage(SymTableNodePtr routineIdPtr);
/// <summary>The dispatch sends ABL getobjectmaxdmg (152) here.</summary>
/// <remarks>MCX.EXE @ 0x00633950</remarks>
TypePtr execHbGetObjDmgPts(SymTableNodePtr routineIdPtr);
/// <summary>
/// The damage that destroys an object, by its name. Never called: getobjectdmgpts (151) has no dispatch case
/// (it is an undefined-routine Fatal) and getobjectmaxdmg runs execHbGetObjDmgPts.
/// </summary>
/// <remarks>MCX.EXE @ 0x006339e0</remarks>
TypePtr execHbGetMaxDmg(SymTableNodePtr routineIdPtr);
/// <summary>ABL setobjectdamage (154).</summary>
/// <remarks>MCX.EXE @ 0x00633b70</remarks>
void execHbSetObjDamage(SymTableNodePtr routineIdPtr);
/// <summary>ABL damageobject (101).</summary>
/// <remarks>MCX.EXE @ 0x00633d20</remarks>
TypePtr execHbDamageObject(SymTableNodePtr routineIdPtr);
/// <summary>ABL getglobalvalue (155).</summary>
/// <remarks>MCX.EXE @ 0x00634040</remarks>
TypePtr execHbGetGlobalValue(SymTableNodePtr routineIdPtr);
/// <summary>ABL setglobalvalue (156).</summary>
/// <remarks>MCX.EXE @ 0x00634080</remarks>
void execHbSetGlobalValue(SymTableNodePtr routineIdPtr);
/// <summary>ABL setobjectivepos (157).</summary>
/// <remarks>MCX.EXE @ 0x006340e0</remarks>
void execHbSetObjectivePos(SymTableNodePtr routineIdPtr);
/// <summary>ABL settonnage (162).</summary>
/// <remarks>MCX.EXE @ 0x00634180</remarks>
void execHbSetTonnage(SymTableNodePtr routineIdPtr);
/// <summary>ABL setsensorrange (161).</summary>
/// <remarks>MCX.EXE @ 0x00634200</remarks>
void execHbSetSensorRange(SymTableNodePtr routineIdPtr);
/// <summary>ABL setexplosiondamage (164).</summary>
/// <remarks>MCX.EXE @ 0x00634320</remarks>
void execHbSetExplDmg(SymTableNodePtr routineIdPtr);
/// <summary>ABL setexplosionradius (165).</summary>
/// <remarks>MCX.EXE @ 0x006343a0</remarks>
void execHbSetExplRad(SymTableNodePtr routineIdPtr);
/// <summary>ABL setsalvage (167).</summary>
/// <remarks>MCX.EXE @ 0x00634420</remarks>
TypePtr execHbSetSalvage(SymTableNodePtr routineIdPtr);
/// <summary>ABL setsalvagestatus (168).</summary>
/// <remarks>MCX.EXE @ 0x00634530</remarks>
TypePtr execHbSetSalvageStatus(SymTableNodePtr routineIdPtr);
/// <summary>ABL setanimation (169).</summary>
/// <remarks>MCX.EXE @ 0x00634600</remarks>
void execHbSetAnimation(SymTableNodePtr routineIdPtr);
/// <summary>ABL playwavefile (163).</summary>
/// <remarks>MCX.EXE @ 0x006346e0</remarks>
void execHbPlayWave(SymTableNodePtr routineIdPtr);
/// <summary>ABL setrevealed (170).</summary>
/// <remarks>MCX.EXE @ 0x00634700</remarks>
void execHbSetRevealed(SymTableNodePtr routineIdPtr);
/// <summary>ABL getsalvage (166).</summary>
/// <remarks>MCX.EXE @ 0x006347e0</remarks>
void execHbGetSalvage(SymTableNodePtr routineIdPtr);
/// <summary>ABL orderrefit (171).</summary>
/// <remarks>MCX.EXE @ 0x006348f0</remarks>
void execHbRefit(SymTableNodePtr routineIdPtr);
/// <summary>ABL setcaptured (173).</summary>
/// <remarks>MCX.EXE @ 0x00634980</remarks>
void execHbSetCaptured(SymTableNodePtr routineIdPtr);
/// <summary>ABL ordercapture (172).</summary>
/// <remarks>MCX.EXE @ 0x006349c0</remarks>
void execHbCaptureObject(SymTableNodePtr routineIdPtr);
/// <summary>ABL setcaptureable (174).</summary>
/// <remarks>MCX.EXE @ 0x00634a60</remarks>
void execHbSetCaptureable(SymTableNodePtr routineIdPtr);
/// <summary>ABL iscaptured (175).</summary>
/// <remarks>MCX.EXE @ 0x00634b40</remarks>
TypePtr execHbIsCaptured(SymTableNodePtr routineIdPtr);
/// <summary>ABL iscapturable (176).</summary>
/// <remarks>MCX.EXE @ 0x00634c90</remarks>
TypePtr execHbIsCapturable(SymTableNodePtr routineIdPtr);
/// <summary>ABL wasevercapturable (177).</summary>
/// <remarks>MCX.EXE @ 0x00634ce0</remarks>
TypePtr execHbWasEverCapturable(SymTableNodePtr routineIdPtr);
/// <summary>ABL setbuildingname (178): gives a building (class 0x10) of the part the string table entry as its
/// name.</summary>
/// <remarks>MCX.EXE @ 0x00634d80 (unnamed in Ghidra; the name is the port's)</remarks>
void execHbSetBuildingName(SymTableNodePtr routineIdPtr);
/// <summary>ABL callstrike (179).</summary>
/// <remarks>MCX.EXE @ 0x00634f90</remarks>
void execHbCallStrike(SymTableNodePtr routineIdPtr);
/// <summary>ABL callstrikeex (188).</summary>
/// <remarks>MCX.EXE @ 0x006350c0</remarks>
void execHbCallStrikeEx(SymTableNodePtr routineIdPtr);
/// <summary>ABL orderloadelementals (180).</summary>
/// <remarks>MCX.EXE @ 0x00635220</remarks>
void execHbLoadElementals(SymTableNodePtr routineIdPtr);
/// <summary>ABL orderdeployelementals (181).</summary>
/// <remarks>MCX.EXE @ 0x00635290</remarks>
void execHbDeployElementals(SymTableNodePtr routineIdPtr);
/// <summary>ABL addprisoner (182).</summary>
/// <remarks>MCX.EXE @ 0x006352e0</remarks>
TypePtr execHbAddPrisoner(SymTableNodePtr routineIdPtr);
/// <summary>ABL settrainspeed (183).</summary>
/// <remarks>MCX.EXE @ 0x006353f0</remarks>
void execHbSetTrainSpeed(SymTableNodePtr routineIdPtr);
/// <summary>ABL lockgateopen (184).</summary>
/// <remarks>MCX.EXE @ 0x00635490</remarks>
void execHbLockGateOpen(SymTableNodePtr routineIdPtr);
/// <summary>ABL lockgateclosed (185).</summary>
/// <remarks>MCX.EXE @ 0x006354e0</remarks>
void execHbLockGateClosed(SymTableNodePtr routineIdPtr);
/// <summary>ABL releasegatelock (186).</summary>
/// <remarks>MCX.EXE @ 0x00635530</remarks>
void execHbReleaseGateLock(SymTableNodePtr routineIdPtr);
/// <summary>ABL isgateopen (187).</summary>
/// <remarks>MCX.EXE @ 0x00635580</remarks>
TypePtr execHbIsGateOpen(SymTableNodePtr routineIdPtr);
/// <summary>ABL getunitstatus (189).</summary>
/// <remarks>MCX.EXE @ 0x00635600</remarks>
TypePtr execHbGetUnitStatus(SymTableNodePtr routineIdPtr);
/// <summary>ABL getrelativepositiontopoint (140).</summary>
/// <remarks>MCX.EXE @ 0x00635be0</remarks>
void execHbRelPosPoint(SymTableNodePtr routineIdPtr);
/// <summary>ABL getrelativepositiontoobject (141).</summary>
/// <remarks>MCX.EXE @ 0x00635ce0</remarks>
void execHbRelPosObject(SymTableNodePtr routineIdPtr);
/// <summary>ABL repair (190).</summary>
/// <remarks>MCX.EXE @ 0x00635dd0</remarks>
void execHbRepair(SymTableNodePtr routineIdPtr);
/// <summary>ABL getrepairstate (192).</summary>
/// <remarks>MCX.EXE @ 0x00635f80</remarks>
TypePtr execHbGetRepairState(SymTableNodePtr routineIdPtr);
/// <summary>ABL isteamtargeting (193).</summary>
/// <remarks>MCX.EXE @ 0x006360c0</remarks>
TypePtr execHbIsTeamTargeting(SymTableNodePtr routineIdPtr);
/// <summary>ABL getfixed (191).</summary>
/// <remarks>MCX.EXE @ 0x00636170</remarks>
TypePtr execHbGetFixed(SymTableNodePtr routineIdPtr);
/// <summary>Writes <see cref="MissionScriptMessageLog"/> to the debug output.</summary>
/// <remarks>MCX.EXE @ 0x00636370</remarks>
void DebugMissionScriptMessages();
/// <summary>ABL sendmessage (194).</summary>
/// <remarks>MCX.EXE @ 0x006364a0</remarks>
void execHbSendMessage(SymTableNodePtr routineIdPtr);
/// <summary>ABL getmessage (195).</summary>
/// <remarks>MCX.EXE @ 0x00636570</remarks>
TypePtr execHbGetMessage(SymTableNodePtr routineIdPtr);
/// <summary>ABL getstrikes (198).</summary>
/// <remarks>MCX.EXE @ 0x006365c0</remarks>
TypePtr execHbGetStrikes(SymTableNodePtr routineIdPtr);
/// <summary>ABL setstrikes (197).</summary>
/// <remarks>MCX.EXE @ 0x006366a0</remarks>
void execHbSetStrikes(SymTableNodePtr routineIdPtr);
/// <summary>ABL addstrikes (200).</summary>
/// <remarks>MCX.EXE @ 0x00636780</remarks>
void execHbAddStrikes(SymTableNodePtr routineIdPtr);
/// <summary>ABL isserver (199).</summary>
/// <remarks>MCX.EXE @ 0x00636890</remarks>
TypePtr execHbIsServer(SymTableNodePtr routineIdPtr);
/// <summary>ABL gethometeam (196).</summary>
/// <remarks>MCX.EXE @ 0x006368d0</remarks>
TypePtr execHbGetHomeTeam(SymTableNodePtr routineIdPtr);
/// <summary>Runs standard routine <paramref name="routineIdPtr"/> by its RoutineKey.</summary>
/// <returns>The routine's result type (null for none).</returns>
/// <remarks>MCX.EXE @ 0x00636900</remarks>
TypePtr execStandardRoutineCall(SymTableNodePtr routineIdPtr);
