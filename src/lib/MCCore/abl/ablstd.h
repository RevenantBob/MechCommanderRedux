#pragma once

// Compiling calls of ABL's standard routines: std* for the language built-ins, hb* ("heartbeat") for the game
// functions unit brains and mission scripts call. Each parses the argument list after the routine's name, checks
// the argument types and returns the result type (null, or declared void, for none). ablxstd.cpp executes them.
// The ABL name of each is in the RoutineKey enum (ablsymt.h); standardRoutineCall dispatches on it.

#include "abl/MCAblSymbolTable.h"

class MCAblCompiler;

/// <summary><c>return [value]</c>.</summary>
void StdReturn(MCAblCompiler& compiler);
/// <summary><c>print(value)</c>.</summary>
void StdPrint(MCAblCompiler& compiler);
/// <summary><c>concat(string, value)</c>.</summary>
MCAblType* StdConcat(MCAblCompiler& compiler);
MCAblType* StdAbs(MCAblCompiler& compiler);
MCAblType* StdRound(MCAblCompiler& compiler);
MCAblType* StdTrunc(MCAblCompiler& compiler);
MCAblType* StdSqrt(MCAblCompiler& compiler);
MCAblType* StdRandom(MCAblCompiler& compiler);
/// <summary><c>getmodulehandle</c>; also compiles <c>getmode</c>, <c>getaction</c>, <c>getphase</c> (no arguments, integer).</summary>
MCAblType* StdGetModHandle(MCAblCompiler& compiler);
MCAblType* StdGetModName(MCAblCompiler& compiler);
void StdSetModName(MCAblCompiler& compiler);
MCAblType* StdSetMaxLoops(MCAblCompiler& compiler);
MCAblType* StdFatal(MCAblCompiler& compiler);
MCAblType* StdAssert(MCAblCompiler& compiler);
/// <summary>Not dispatched by standardRoutineCall.</summary>
MCAblType* StdHandle(MCAblCompiler& compiler);
/// <summary><c>setmode</c>, <c>setaction</c>, <c>setphase</c>.</summary>
void HbSetMode(MCAblCompiler& compiler);
void HbSetUpdateTime(MCAblCompiler& compiler);
MCAblType* HbGetId(MCAblCompiler& compiler);
MCAblType* HbGetTime(MCAblCompiler& compiler);
MCAblType* HbGetTimeLeft(MCAblCompiler& compiler);
MCAblType* HbGetTarget(MCAblCompiler& compiler);
void HbSetTarget(MCAblCompiler& compiler);
/// <summary><c>getcontacts</c>.</summary>
MCAblType* HbGetContacts(MCAblCompiler& compiler);
MCAblType* HbGetEnemyCount(MCAblCompiler& compiler);
/// <summary><c>getweaponsready</c>, <c>getweaponslocked</c>, <c>getweaponsinrange</c>.</summary>
MCAblType* HbGetWeapons(MCAblCompiler& compiler);
MCAblType* HbGetWeaponShots(MCAblCompiler& compiler);
MCAblType* HbGetWeaponRanges(MCAblCompiler& compiler);
/// <summary><c>getintegermemory</c>.</summary>
MCAblType* HbGetMemoryInteger(MCAblCompiler& compiler);
/// <summary><c>getrealmemory</c>.</summary>
MCAblType* HbGetMemoryReal(MCAblCompiler& compiler);
MCAblType* HbGetAlarmTriggers(MCAblCompiler& compiler);
MCAblType* HbStartFieldScan(MCAblCompiler& compiler);
MCAblType* HbStartVehicleScan(MCAblCompiler& compiler);
/// <summary><c>startenemyscan</c>, <c>startfriendlyscan</c>.</summary>
MCAblType* HbStartContactScan(MCAblCompiler& compiler);
MCAblType* HbStartMovePath(MCAblCompiler& compiler);
void HbSetMoveGoal(MCAblCompiler& compiler);
/// <summary><c>setintegermemory</c>.</summary>
void HbSetMemoryInteger(MCAblCompiler& compiler);
/// <summary><c>setrealmemory</c>.</summary>
void HbSetMemoryReal(MCAblCompiler& compiler);
MCAblType* HbGetChallenger(MCAblCompiler& compiler);
void HbGetFireRanges(MCAblCompiler& compiler);
MCAblType* HbGetAttackers(MCAblCompiler& compiler);
MCAblType* HbGetAttackerInfo(MCAblCompiler& compiler);
MCAblType* HbGetTimeWithoutOrders(MCAblCompiler& compiler);
MCAblType* HbSetChallenger(MCAblCompiler& compiler);
/// <summary>Not dispatched by standardRoutineCall.</summary>
MCAblType* HbSelectUnit(MCAblCompiler& compiler);
MCAblType* HbSelectObject(MCAblCompiler& compiler);
MCAblType* HbSelectWarrior(MCAblCompiler& compiler);
MCAblType* HbGetWarriorStatus(MCAblCompiler& compiler);
MCAblType* HbSelectContact(MCAblCompiler& compiler);
MCAblType* HbIsContact(MCAblCompiler& compiler);
MCAblType* HbGetContactStatus(MCAblCompiler& compiler);
MCAblType* HbGetContactId(MCAblCompiler& compiler);
MCAblType* HbGetContactRelativePosition(MCAblCompiler& compiler);
MCAblType* HbSetPotentialContact(MCAblCompiler& compiler);
MCAblType* HbSetGuardObjective(MCAblCompiler& compiler);
MCAblType* HbSetGuardPoint(MCAblCompiler& compiler);
MCAblType* HbSetGuardRadii(MCAblCompiler& compiler);
MCAblType* HbGetGuardObjective(MCAblCompiler& compiler);
MCAblType* HbGetGuardPoint(MCAblCompiler& compiler);
MCAblType* HbGetGuardRadii(MCAblCompiler& compiler);
MCAblType* HbGetGuardDistanceTo(MCAblCompiler& compiler);
MCAblType* HbHasMoveGoal(MCAblCompiler& compiler);
MCAblType* HbHasMovePath(MCAblCompiler& compiler);
void HbSortWeapons(MCAblCompiler& compiler);
MCAblType* HbTimeToImpact(MCAblCompiler& compiler);
MCAblType* HbFireWeapon(MCAblCompiler& compiler);
MCAblType* HbGetObjectPosition(MCAblCompiler& compiler);
/// <summary>Not dispatched by standardRoutineCall.</summary>
MCAblType* HbGetMoveOrder(MCAblCompiler& compiler);
/// <summary>Not dispatched by standardRoutineCall.</summary>
MCAblType* HbGetAttackOrder(MCAblCompiler& compiler);
MCAblType* HbGetVisualRange(MCAblCompiler& compiler);
MCAblType* HbGetTacOrder(MCAblCompiler& compiler);
MCAblType* HbGetLastTacOrder(MCAblCompiler& compiler);
MCAblType* HbGetUnitMates(MCAblCompiler& compiler);
/// <summary><c>setordermode</c>.</summary>
MCAblType* HbSetOrderMode(MCAblCompiler& compiler);
/// <summary><c>orderwait</c>.</summary>
MCAblType* HbWait(MCAblCompiler& compiler);
/// <summary><c>ordermoveto</c>.</summary>
MCAblType* HbMoveToPoint(MCAblCompiler& compiler);
/// <summary><c>ordermovetoobject</c>.</summary>
MCAblType* HbMoveToObject(MCAblCompiler& compiler);
/// <summary><c>ordermovetocontact</c>.</summary>
MCAblType* HbMoveToContact(MCAblCompiler& compiler);
MCAblType* HbOrderPowerUp(MCAblCompiler& compiler);
MCAblType* HbOrderPowerDown(MCAblCompiler& compiler);
/// <summary>Not dispatched by standardRoutineCall.</summary>
MCAblType* HbOrderFormation(MCAblCompiler& compiler);
/// <summary>Not dispatched by standardRoutineCall.</summary>
MCAblType* HbGetFormation(MCAblCompiler& compiler);
/// <summary>Not dispatched by standardRoutineCall.</summary>
MCAblType* HbUseSpeed(MCAblCompiler& compiler);
/// <summary><c>orderattackobject</c>.</summary>
MCAblType* HbOrderAttackObject(MCAblCompiler& compiler);
MCAblType* HbOrderAttackContact(MCAblCompiler& compiler);
MCAblType* HbAttackThreat(MCAblCompiler& compiler);
MCAblType* HbOpenFire(MCAblCompiler& compiler);
/// <summary>Not dispatched by standardRoutineCall.</summary>
MCAblType* HbUseFireRange(MCAblCompiler& compiler);
/// <summary>Not dispatched by standardRoutineCall.</summary>
MCAblType* HbUseFireOdds(MCAblCompiler& compiler);
MCAblType* HbDamageObject(MCAblCompiler& compiler);
MCAblType* HbSetAttackRadius(MCAblCompiler& compiler);
MCAblType* HbOrderTest(MCAblCompiler& compiler);
MCAblType* HbPlaySmacker(MCAblCompiler& compiler);
void HbObjectChangeSides(MCAblCompiler& compiler);
MCAblType* HbDistanceToObject(MCAblCompiler& compiler);
MCAblType* HbDistanceToPosition(MCAblCompiler& compiler);
void HbObjectSuicide(MCAblCompiler& compiler);
MCAblType* HbObjectCreate(MCAblCompiler& compiler);
MCAblType* HbObjectExists(MCAblCompiler& compiler);
MCAblType* HbObjectStatus(MCAblCompiler& compiler);
MCAblType* HbObjectStatusCount(MCAblCompiler& compiler);
MCAblType* HbObjectVisible(MCAblCompiler& compiler);
MCAblType* HbObjectSide(MCAblCompiler& compiler);
MCAblType* HbObjectCommander(MCAblCompiler& compiler);
MCAblType* HbObjectClass(MCAblCompiler& compiler);
MCAblType* HbInArea(MCAblCompiler& compiler);
MCAblType* HbSetTimer(MCAblCompiler& compiler);
/// <summary><c>checktimer</c>.</summary>
MCAblType* HbChkTimer(MCAblCompiler& compiler);
void HbEndTimer(MCAblCompiler& compiler);
MCAblType* HbSetObjectiveTimer(MCAblCompiler& compiler);
MCAblType* HbCheckObjectiveTimer(MCAblCompiler& compiler);
MCAblType* HbSetObjectiveStatus(MCAblCompiler& compiler);
MCAblType* HbCheckObjectiveStatus(MCAblCompiler& compiler);
MCAblType* HbSetObjectiveType(MCAblCompiler& compiler);
MCAblType* HbCheckObjectiveType(MCAblCompiler& compiler);
MCAblType* HbPlayDigitalMusic(MCAblCompiler& compiler);
MCAblType* HbStopMusic(MCAblCompiler& compiler);
MCAblType* HbPlaySoundEffect(MCAblCompiler& compiler);
MCAblType* HbPlayVideo(MCAblCompiler& compiler);
MCAblType* HbFileExists(MCAblCompiler& compiler);
MCAblType* HbPlaySpeech(MCAblCompiler& compiler);
MCAblType* HbPlayBetty(MCAblCompiler& compiler);
MCAblType* HbSetRadio(MCAblCompiler& compiler);
/// <summary><c>setobjectactive</c>.</summary>
MCAblType* HbSetObjActive(MCAblCompiler& compiler);
/// <summary><c>orderwithdraw</c>.</summary>
MCAblType* HbObjWithdraw(MCAblCompiler& compiler);
/// <summary><c>objectinwithdrawal</c>.</summary>
MCAblType* HbObjInWithdraw(MCAblCompiler& compiler);
/// <summary><c>objecttypeid</c>.</summary>
MCAblType* HbObjTypeId(MCAblCompiler& compiler);
/// <summary><c>getterrainobjectpartid</c>.</summary>
MCAblType* HbTerrainObjectId(MCAblCompiler& compiler);
/// <summary><c>getvehiclepartid</c>.</summary>
MCAblType* HbVehicleId(MCAblCompiler& compiler);
MCAblType* HbGetWeaponAmmo(MCAblCompiler& compiler);
/// <summary><c>getsensorsworking</c>.</summary>
MCAblType* HbGetSensors(MCAblCompiler& compiler);
/// <summary><c>getcurrentbrvalue</c>; also compiles <c>setcurrentbrvalue</c> (Original behaviour).</summary>
MCAblType* HbGetBRValue(MCAblCompiler& compiler);
/// <summary>Not dispatched by standardRoutineCall (hbGetBRValue compiles <c>setcurrentbrvalue</c>).</summary>
void HbSetBRValue(MCAblCompiler& compiler);
/// <summary><c>getarmorpts</c>.</summary>
MCAblType* HbGetArmor(MCAblCompiler& compiler);
MCAblType* HbGetMaxArmor(MCAblCompiler& compiler);
MCAblType* HbGetPilotId(MCAblCompiler& compiler);
MCAblType* HbGetPilotWounds(MCAblCompiler& compiler);
void HbSetPilotWounds(MCAblCompiler& compiler);
/// <summary><c>getobjectactive</c>.</summary>
MCAblType* HbGetObjActive(MCAblCompiler& compiler);
/// <summary><c>getobjectdamage</c>.</summary>
MCAblType* HbGetObjDamage(MCAblCompiler& compiler);
/// <summary><c>getobjectdmgpts</c>.</summary>
MCAblType* HbGetObjDmgPts(MCAblCompiler& compiler);
/// <summary><c>getobjectmaxdmg</c>.</summary>
MCAblType* HbGetObjMaxDmg(MCAblCompiler& compiler);
/// <summary><c>setobjectdamage</c>.</summary>
void HbSetObjDamage(MCAblCompiler& compiler);
void HbSetObjectivePos(MCAblCompiler& compiler);
MCAblType* HbGetGlobalValue(MCAblCompiler& compiler);
void HbSetGlobalValue(MCAblCompiler& compiler);
MCAblType* HbSetSensorRange(MCAblCompiler& compiler);
void HbSetTonnage(MCAblCompiler& compiler);
/// <summary><c>setexplosiondamage</c>.</summary>
void HbSetExplDmg(MCAblCompiler& compiler);
/// <summary><c>setexplosionradius</c>.</summary>
void HbSetExplRad(MCAblCompiler& compiler);
MCAblType* HbSetSalvage(MCAblCompiler& compiler);
MCAblType* HbSetSalvageStatus(MCAblCompiler& compiler);
void HbSetAnimation(MCAblCompiler& compiler);
/// <summary><c>playwavefile</c>.</summary>
void HbPlayWave(MCAblCompiler& compiler);
/// <summary><c>setrevealed</c>.</summary>
void HbSetRevealed(MCAblCompiler& compiler);
void HbGetSalvage(MCAblCompiler& compiler);
/// <summary><c>orderrefit</c>.</summary>
void HbRefit(MCAblCompiler& compiler);
/// <summary><c>ordercapture</c>.</summary>
void HbCaptureObject(MCAblCompiler& compiler);
void HbSetCaptured(MCAblCompiler& compiler);
void HbSetCaptureable(MCAblCompiler& compiler);
MCAblType* HbIsCaptured(MCAblCompiler& compiler);
MCAblType* HbIsCapturable(MCAblCompiler& compiler);
MCAblType* HbWasEverCapturable(MCAblCompiler& compiler);
void HbSetBuildingName(MCAblCompiler& compiler);
void HbCallStrike(MCAblCompiler& compiler);
void HbCallStrikeEx(MCAblCompiler& compiler);
/// <summary><c>orderloadelementals</c>.</summary>
void HbLoadElementals(MCAblCompiler& compiler);
/// <summary><c>orderdeployelementals</c>.</summary>
void HbDeployElementals(MCAblCompiler& compiler);
MCAblType* HbAddPrisoner(MCAblCompiler& compiler);
void HbSetTrainSpeed(MCAblCompiler& compiler);
void HbLockGateOpen(MCAblCompiler& compiler);
void HbLockGateClosed(MCAblCompiler& compiler);
/// <summary><c>releasegatelock</c>.</summary>
void HbReleaseGateLock(MCAblCompiler& compiler);
MCAblType* HbIsGateOpen(MCAblCompiler& compiler);
/// <summary><c>getrelativepositiontopoint</c>.</summary>
void HbGetRelPosPoint(MCAblCompiler& compiler);
/// <summary><c>getunitstatus</c>.</summary>
MCAblType* HbGetUnitStatus(MCAblCompiler& compiler);
/// <summary><c>getrelativepositiontoobject</c>.</summary>
void HbGetRelPosObject(MCAblCompiler& compiler);
void HbRepair(MCAblCompiler& compiler);
MCAblType* HbGetFixed(MCAblCompiler& compiler);
MCAblType* HbGetRepairState(MCAblCompiler& compiler);
MCAblType* HbIsTeamTargeting(MCAblCompiler& compiler);
MCAblType* HbSendMessage(MCAblCompiler& compiler);
MCAblType* HbGetMessage(MCAblCompiler& compiler);
MCAblType* HbGetHomeTeam(MCAblCompiler& compiler);
MCAblType* HbGetStrikes(MCAblCompiler& compiler);
void HbSetStrikes(MCAblCompiler& compiler);
void HbAddStrikes(MCAblCompiler& compiler);
MCAblType* HbIsServer(MCAblCompiler& compiler);

/// <summary>Compiles a call of standard routine <paramref name="routineIdPtr"/> by its RoutineKey.</summary>
/// <returns>The result type, or null.</returns>
MCAblType* StandardRoutineCall(MCAblCompiler& compiler, MCAblSymbol* routineIdPtr);
