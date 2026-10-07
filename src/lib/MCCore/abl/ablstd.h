#pragma once

// Compiling calls of ABL's standard routines: std* for the language built-ins, hb* ("heartbeat") for the game
// functions unit brains and mission scripts call. Each parses the argument list after the routine's name, checks
// the argument types and returns the result type (null, or declared void, for none). ablxstd.cpp executes them.
// The ABL name of each is in the RoutineKey enum (ablsymt.h); standardRoutineCall dispatches on it.

#include "abl/ablsymt.h"

/// <summary><c>return [value]</c>.</summary>
void StdReturn();
/// <summary><c>print(value)</c>.</summary>
void StdPrint();
/// <summary><c>concat(string, value)</c>.</summary>
MCTypePtr StdConcat();
MCTypePtr StdAbs();
MCTypePtr StdRound();
MCTypePtr StdTrunc();
MCTypePtr StdSqrt();
MCTypePtr StdRandom();
/// <summary><c>getmodulehandle</c>; also compiles <c>getmode</c>, <c>getaction</c>, <c>getphase</c> (no arguments, integer).</summary>
MCTypePtr StdGetModHandle();
MCTypePtr StdGetModName();
void StdSetModName();
MCTypePtr StdSetMaxLoops();
MCTypePtr StdFatal();
MCTypePtr StdAssert();
/// <summary>Not dispatched by standardRoutineCall.</summary>
MCTypePtr StdHandle();
/// <summary><c>setmode</c>, <c>setaction</c>, <c>setphase</c>.</summary>
void HbSetMode();
void HbSetUpdateTime();
MCTypePtr HbGetId();
MCTypePtr HbGetTime();
MCTypePtr HbGetTimeLeft();
MCTypePtr HbGetTarget();
void HbSetTarget();
/// <summary><c>getcontacts</c>.</summary>
MCTypePtr HbGetContacts();
MCTypePtr HbGetEnemyCount();
/// <summary><c>getweaponsready</c>, <c>getweaponslocked</c>, <c>getweaponsinrange</c>.</summary>
MCTypePtr HbGetWeapons();
MCTypePtr HbGetWeaponShots();
MCTypePtr HbGetWeaponRanges();
/// <summary><c>getintegermemory</c>.</summary>
MCTypePtr HbGetMemoryInteger();
/// <summary><c>getrealmemory</c>.</summary>
MCTypePtr HbGetMemoryReal();
MCTypePtr HbGetAlarmTriggers();
MCTypePtr HbStartFieldScan();
MCTypePtr HbStartVehicleScan();
/// <summary><c>startenemyscan</c>, <c>startfriendlyscan</c>.</summary>
MCTypePtr HbStartContactScan();
MCTypePtr HbStartMovePath();
void HbSetMoveGoal();
/// <summary><c>setintegermemory</c>.</summary>
void HbSetMemoryInteger();
/// <summary><c>setrealmemory</c>.</summary>
void HbSetMemoryReal();
MCTypePtr HbGetChallenger();
void HbGetFireRanges();
MCTypePtr HbGetAttackers();
MCTypePtr HbGetAttackerInfo();
MCTypePtr HbGetTimeWithoutOrders();
MCTypePtr HbSetChallenger();
/// <summary>Not dispatched by standardRoutineCall.</summary>
MCTypePtr HbSelectUnit();
MCTypePtr HbSelectObject();
MCTypePtr HbSelectWarrior();
MCTypePtr HbGetWarriorStatus();
MCTypePtr HbSelectContact();
MCTypePtr HbIsContact();
MCTypePtr HbGetContactStatus();
MCTypePtr HbGetContactId();
MCTypePtr HbGetContactRelativePosition();
MCTypePtr HbSetPotentialContact();
MCTypePtr HbSetGuardObjective();
MCTypePtr HbSetGuardPoint();
MCTypePtr HbSetGuardRadii();
MCTypePtr HbGetGuardObjective();
MCTypePtr HbGetGuardPoint();
MCTypePtr HbGetGuardRadii();
MCTypePtr HbGetGuardDistanceTo();
MCTypePtr HbHasMoveGoal();
MCTypePtr HbHasMovePath();
void HbSortWeapons();
MCTypePtr HbTimeToImpact();
MCTypePtr HbFireWeapon();
MCTypePtr HbGetObjectPosition();
/// <summary>Not dispatched by standardRoutineCall.</summary>
MCTypePtr HbGetMoveOrder();
/// <summary>Not dispatched by standardRoutineCall.</summary>
MCTypePtr HbGetAttackOrder();
MCTypePtr HbGetVisualRange();
MCTypePtr HbGetTacOrder();
MCTypePtr HbGetLastTacOrder();
MCTypePtr HbGetUnitMates();
/// <summary><c>setordermode</c>.</summary>
MCTypePtr HbSetOrderMode();
/// <summary><c>orderwait</c>.</summary>
MCTypePtr HbWait();
/// <summary><c>ordermoveto</c>.</summary>
MCTypePtr HbMoveToPoint();
/// <summary><c>ordermovetoobject</c>.</summary>
MCTypePtr HbMoveToObject();
/// <summary><c>ordermovetocontact</c>.</summary>
MCTypePtr HbMoveToContact();
MCTypePtr HbOrderPowerUp();
MCTypePtr HbOrderPowerDown();
/// <summary>Not dispatched by standardRoutineCall.</summary>
MCTypePtr HbOrderFormation();
/// <summary>Not dispatched by standardRoutineCall.</summary>
MCTypePtr HbGetFormation();
/// <summary>Not dispatched by standardRoutineCall.</summary>
MCTypePtr HbUseSpeed();
/// <summary><c>orderattackobject</c>.</summary>
MCTypePtr HbOrderAttackObject();
MCTypePtr HbOrderAttackContact();
MCTypePtr HbAttackThreat();
MCTypePtr HbOpenFire();
/// <summary>Not dispatched by standardRoutineCall.</summary>
MCTypePtr HbUseFireRange();
/// <summary>Not dispatched by standardRoutineCall.</summary>
MCTypePtr HbUseFireOdds();
MCTypePtr HbDamageObject();
MCTypePtr HbSetAttackRadius();
MCTypePtr HbOrderTest();
MCTypePtr HbPlaySmacker();
void HbObjectChangeSides();
MCTypePtr HbDistanceToObject();
MCTypePtr HbDistanceToPosition();
void HbObjectSuicide();
MCTypePtr HbObjectCreate();
MCTypePtr HbObjectExists();
MCTypePtr HbObjectStatus();
MCTypePtr HbObjectStatusCount();
MCTypePtr HbObjectVisible();
MCTypePtr HbObjectSide();
MCTypePtr HbObjectCommander();
MCTypePtr HbObjectClass();
MCTypePtr HbInArea();
MCTypePtr HbSetTimer();
/// <summary><c>checktimer</c>.</summary>
MCTypePtr HbChkTimer();
void HbEndTimer();
MCTypePtr HbSetObjectiveTimer();
MCTypePtr HbCheckObjectiveTimer();
MCTypePtr HbSetObjectiveStatus();
MCTypePtr HbCheckObjectiveStatus();
MCTypePtr HbSetObjectiveType();
MCTypePtr HbCheckObjectiveType();
MCTypePtr HbPlayDigitalMusic();
MCTypePtr HbStopMusic();
MCTypePtr HbPlaySoundEffect();
MCTypePtr HbPlayVideo();
MCTypePtr HbFileExists();
MCTypePtr HbPlaySpeech();
MCTypePtr HbPlayBetty();
MCTypePtr HbSetRadio();
/// <summary><c>setobjectactive</c>.</summary>
MCTypePtr HbSetObjActive();
/// <summary><c>orderwithdraw</c>.</summary>
MCTypePtr HbObjWithdraw();
/// <summary><c>objectinwithdrawal</c>.</summary>
MCTypePtr HbObjInWithdraw();
/// <summary><c>objecttypeid</c>.</summary>
MCTypePtr HbObjTypeId();
/// <summary><c>getterrainobjectpartid</c>.</summary>
MCTypePtr HbTerrainObjectId();
/// <summary><c>getvehiclepartid</c>.</summary>
MCTypePtr HbVehicleId();
MCTypePtr HbGetWeaponAmmo();
/// <summary><c>getsensorsworking</c>.</summary>
MCTypePtr HbGetSensors();
/// <summary><c>getcurrentbrvalue</c>; also compiles <c>setcurrentbrvalue</c> (Original behaviour).</summary>
MCTypePtr HbGetBRValue();
/// <summary>Not dispatched by standardRoutineCall (hbGetBRValue compiles <c>setcurrentbrvalue</c>).</summary>
void HbSetBRValue();
/// <summary><c>getarmorpts</c>.</summary>
MCTypePtr HbGetArmor();
MCTypePtr HbGetMaxArmor();
MCTypePtr HbGetPilotId();
MCTypePtr HbGetPilotWounds();
void HbSetPilotWounds();
/// <summary><c>getobjectactive</c>.</summary>
MCTypePtr HbGetObjActive();
/// <summary><c>getobjectdamage</c>.</summary>
MCTypePtr HbGetObjDamage();
/// <summary><c>getobjectdmgpts</c>.</summary>
MCTypePtr HbGetObjDmgPts();
/// <summary><c>getobjectmaxdmg</c>.</summary>
MCTypePtr HbGetObjMaxDmg();
/// <summary><c>setobjectdamage</c>.</summary>
void HbSetObjDamage();
void HbSetObjectivePos();
MCTypePtr HbGetGlobalValue();
void HbSetGlobalValue();
MCTypePtr HbSetSensorRange();
void HbSetTonnage();
/// <summary><c>setexplosiondamage</c>.</summary>
void HbSetExplDmg();
/// <summary><c>setexplosionradius</c>.</summary>
void HbSetExplRad();
MCTypePtr HbSetSalvage();
MCTypePtr HbSetSalvageStatus();
void HbSetAnimation();
/// <summary><c>playwavefile</c>.</summary>
void HbPlayWave();
/// <summary><c>setrevealed</c>.</summary>
void HbSetRevealed();
void HbGetSalvage();
/// <summary><c>orderrefit</c>.</summary>
void HbRefit();
/// <summary><c>ordercapture</c>.</summary>
void HbCaptureObject();
void HbSetCaptured();
void HbSetCaptureable();
MCTypePtr HbIsCaptured();
MCTypePtr HbIsCapturable();
MCTypePtr HbWasEverCapturable();
void HbSetBuildingName();
void HbCallStrike();
void HbCallStrikeEx();
/// <summary><c>orderloadelementals</c>.</summary>
void HbLoadElementals();
/// <summary><c>orderdeployelementals</c>.</summary>
void HbDeployElementals();
MCTypePtr HbAddPrisoner();
void HbSetTrainSpeed();
void HbLockGateOpen();
void HbLockGateClosed();
/// <summary><c>releasegatelock</c>.</summary>
void HbReleaseGateLock();
MCTypePtr HbIsGateOpen();
/// <summary><c>getrelativepositiontopoint</c>.</summary>
void HbGetRelPosPoint();
/// <summary><c>getunitstatus</c>.</summary>
MCTypePtr HbGetUnitStatus();
/// <summary><c>getrelativepositiontoobject</c>.</summary>
void HbGetRelPosObject();
void HbRepair();
MCTypePtr HbGetFixed();
MCTypePtr HbGetRepairState();
MCTypePtr HbIsTeamTargeting();
MCTypePtr HbSendMessage();
MCTypePtr HbGetMessage();
MCTypePtr HbGetHomeTeam();
MCTypePtr HbGetStrikes();
void HbSetStrikes();
void HbAddStrikes();
MCTypePtr HbIsServer();

/// <summary>Compiles a call of standard routine <paramref name="routineIdPtr"/> by its RoutineKey.</summary>
/// <returns>The result type, or null.</returns>
MCTypePtr StandardRoutineCall(MCSymTableNodePtr routineIdPtr);
