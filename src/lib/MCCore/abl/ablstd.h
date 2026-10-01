#pragma once

// Compiling calls of ABL's standard routines: std* for the language built-ins, hb* ("heartbeat") for the game
// functions unit brains and mission scripts call. Each parses the argument list after the routine's name, checks
// the argument types and returns the result type (null, or declared void, for none). ablxstd.cpp executes them.
// The ABL name of each is in the RoutineKey enum (ablsymt.h); standardRoutineCall dispatches on it.

#include "abl/ablsymt.h"

/// <summary><c>return [value]</c>.</summary>
/// <remarks>MCX.EXE @ 0x00626890</remarks>
void stdReturn();
/// <summary><c>print(value)</c>.</summary>
/// <remarks>MCX.EXE @ 0x006268f0</remarks>
void stdPrint();
/// <summary><c>concat(string, value)</c>.</summary>
/// <remarks>MCX.EXE @ 0x00626960</remarks>
TypePtr stdConcat();
/// <remarks>MCX.EXE @ 0x00626a10</remarks>
TypePtr stdAbs();
/// <remarks>MCX.EXE @ 0x00626a70</remarks>
TypePtr stdRound();
/// <remarks>MCX.EXE @ 0x00626ad0</remarks>
TypePtr stdTrunc();
/// <remarks>MCX.EXE @ 0x00626b30</remarks>
TypePtr stdSqrt();
/// <remarks>MCX.EXE @ 0x00626b90</remarks>
TypePtr stdRandom();
/// <summary><c>getmodulehandle</c>; also compiles <c>getmode</c>, <c>getaction</c>, <c>getphase</c> (no arguments, integer).</summary>
/// <remarks>MCX.EXE @ 0x00626bf0</remarks>
TypePtr stdGetModHandle();
/// <remarks>MCX.EXE @ 0x00626c10</remarks>
TypePtr stdGetModName();
/// <remarks>MCX.EXE @ 0x00626c30</remarks>
void stdSetModName();
/// <remarks>MCX.EXE @ 0x00626c90</remarks>
TypePtr stdSetMaxLoops();
/// <remarks>MCX.EXE @ 0x00626ce0</remarks>
TypePtr stdFatal();
/// <remarks>MCX.EXE @ 0x00626d70</remarks>
TypePtr stdAssert();
/// <summary>Not dispatched by standardRoutineCall.</summary>
/// <remarks>MCX.EXE @ 0x00626e30</remarks>
TypePtr stdHandle();
/// <summary><c>setmode</c>, <c>setaction</c>, <c>setphase</c>.</summary>
/// <remarks>MCX.EXE @ 0x00626e50</remarks>
void hbSetMode();
/// <remarks>MCX.EXE @ 0x00626ea0</remarks>
void hbSetUpdateTime();
/// <remarks>MCX.EXE @ 0x00626ef0</remarks>
TypePtr hbGetId();
/// <remarks>MCX.EXE @ 0x00626f10</remarks>
TypePtr hbGetTime();
/// <remarks>MCX.EXE @ 0x00626f30</remarks>
TypePtr hbGetTimeLeft();
/// <remarks>MCX.EXE @ 0x00626f50</remarks>
TypePtr hbGetTarget();
/// <remarks>MCX.EXE @ 0x00626fb0</remarks>
void hbSetTarget();
/// <summary><c>getcontacts</c>.</summary>
/// <remarks>MCX.EXE @ 0x00627030 (unnamed in the symbols)</remarks>
TypePtr hbGetContacts();
/// <remarks>MCX.EXE @ 0x006270f0</remarks>
TypePtr hbGetEnemyCount();
/// <summary><c>getweaponsready</c>, <c>getweaponslocked</c>, <c>getweaponsinrange</c>.</summary>
/// <remarks>MCX.EXE @ 0x00627150</remarks>
TypePtr hbGetWeapons();
/// <remarks>MCX.EXE @ 0x006271e0</remarks>
TypePtr hbGetWeaponShots();
/// <remarks>MCX.EXE @ 0x00627240</remarks>
TypePtr hbGetWeaponRanges();
/// <summary><c>getintegermemory</c>.</summary>
/// <remarks>MCX.EXE @ 0x006272d0</remarks>
TypePtr hbGetMemoryInteger();
/// <summary><c>getrealmemory</c>.</summary>
/// <remarks>MCX.EXE @ 0x00627330 (unnamed in the symbols)</remarks>
TypePtr hbGetMemoryReal();
/// <remarks>MCX.EXE @ 0x00627390</remarks>
TypePtr hbGetAlarmTriggers();
/// <remarks>MCX.EXE @ 0x006273f0</remarks>
TypePtr hbStartFieldScan();
/// <remarks>MCX.EXE @ 0x00627450</remarks>
TypePtr hbStartVehicleScan();
/// <summary><c>startenemyscan</c>, <c>startfriendlyscan</c>.</summary>
/// <remarks>MCX.EXE @ 0x00627470</remarks>
TypePtr hbStartContactScan();
/// <remarks>MCX.EXE @ 0x006274d0</remarks>
TypePtr hbStartMovePath();
/// <remarks>MCX.EXE @ 0x00627580</remarks>
void hbSetMoveGoal();
/// <summary><c>setintegermemory</c>.</summary>
/// <remarks>MCX.EXE @ 0x00627600</remarks>
void hbSetMemoryInteger();
/// <summary><c>setrealmemory</c>.</summary>
/// <remarks>MCX.EXE @ 0x00627680</remarks>
void hbSetMemoryReal();
/// <remarks>MCX.EXE @ 0x00627700</remarks>
TypePtr hbGetChallenger();
/// <remarks>MCX.EXE @ 0x00627760</remarks>
void hbGetFireRanges();
/// <remarks>MCX.EXE @ 0x006277c0</remarks>
TypePtr hbGetAttackers();
/// <remarks>MCX.EXE @ 0x00627850</remarks>
TypePtr hbGetAttackerInfo();
/// <remarks>MCX.EXE @ 0x006278b0</remarks>
TypePtr hbGetTimeWithoutOrders();
/// <remarks>MCX.EXE @ 0x006278d0</remarks>
TypePtr hbSetChallenger();
/// <summary>Not dispatched by standardRoutineCall.</summary>
/// <remarks>MCX.EXE @ 0x00627950</remarks>
TypePtr hbSelectUnit();
/// <remarks>MCX.EXE @ 0x006279b0</remarks>
TypePtr hbSelectObject();
/// <remarks>MCX.EXE @ 0x00627a10</remarks>
TypePtr hbSelectWarrior();
/// <remarks>MCX.EXE @ 0x00627a70</remarks>
TypePtr hbGetWarriorStatus();
/// <remarks>MCX.EXE @ 0x00627ad0</remarks>
TypePtr hbSelectContact();
/// <remarks>MCX.EXE @ 0x00627b50</remarks>
TypePtr hbIsContact();
/// <remarks>MCX.EXE @ 0x00627c00</remarks>
TypePtr hbGetContactStatus();
/// <remarks>MCX.EXE @ 0x00627c60</remarks>
TypePtr hbGetContactId();
/// <remarks>MCX.EXE @ 0x00627c80</remarks>
TypePtr hbGetContactRelativePosition();
/// <remarks>MCX.EXE @ 0x00627d00</remarks>
TypePtr hbSetPotentialContact();
/// <remarks>MCX.EXE @ 0x00627d80</remarks>
TypePtr hbSetGuardObjective();
/// <remarks>MCX.EXE @ 0x00627de0</remarks>
TypePtr hbSetGuardPoint();
/// <remarks>MCX.EXE @ 0x00627e40</remarks>
TypePtr hbSetGuardRadii();
/// <remarks>MCX.EXE @ 0x00627ed0</remarks>
TypePtr hbGetGuardObjective();
/// <remarks>MCX.EXE @ 0x00627ef0</remarks>
TypePtr hbGetGuardPoint();
/// <remarks>MCX.EXE @ 0x00627f50</remarks>
TypePtr hbGetGuardRadii();
/// <remarks>MCX.EXE @ 0x00627fd0</remarks>
TypePtr hbGetGuardDistanceTo();
/// <remarks>MCX.EXE @ 0x00628030</remarks>
TypePtr hbHasMoveGoal();
/// <remarks>MCX.EXE @ 0x00628050</remarks>
TypePtr hbHasMovePath();
/// <remarks>MCX.EXE @ 0x00628070</remarks>
void hbSortWeapons();
/// <remarks>MCX.EXE @ 0x00628120</remarks>
TypePtr hbTimeToImpact();
/// <remarks>MCX.EXE @ 0x006281a0</remarks>
TypePtr hbFireWeapon();
/// <remarks>MCX.EXE @ 0x00628200</remarks>
TypePtr hbGetObjectPosition();
/// <summary>Not dispatched by standardRoutineCall.</summary>
/// <remarks>MCX.EXE @ 0x00628290</remarks>
TypePtr hbGetMoveOrder();
/// <summary>Not dispatched by standardRoutineCall.</summary>
/// <remarks>MCX.EXE @ 0x006282b0</remarks>
TypePtr hbGetAttackOrder();
/// <remarks>MCX.EXE @ 0x006282d0</remarks>
TypePtr hbGetVisualRange();
/// <remarks>MCX.EXE @ 0x00628330</remarks>
TypePtr hbGetTacOrder();
/// <remarks>MCX.EXE @ 0x006283f0</remarks>
TypePtr hbGetLastTacOrder();
/// <remarks>MCX.EXE @ 0x006284b0</remarks>
TypePtr hbGetUnitMates();
/// <summary><c>setordermode</c>.</summary>
/// <remarks>MCX.EXE @ 0x00628540 (unnamed in the symbols)</remarks>
TypePtr hbSetOrderMode();
/// <summary><c>orderwait</c>.</summary>
/// <remarks>MCX.EXE @ 0x006285a0</remarks>
TypePtr hbWait();
/// <summary><c>ordermoveto</c>.</summary>
/// <remarks>MCX.EXE @ 0x00628620</remarks>
TypePtr hbMoveToPoint();
/// <summary><c>ordermovetoobject</c>.</summary>
/// <remarks>MCX.EXE @ 0x006286b0</remarks>
TypePtr hbMoveToObject();
/// <summary><c>ordermovetocontact</c>.</summary>
/// <remarks>MCX.EXE @ 0x00628730</remarks>
TypePtr hbMoveToContact();
/// <remarks>MCX.EXE @ 0x00628790</remarks>
TypePtr hbOrderPowerUp();
/// <remarks>MCX.EXE @ 0x006287b0</remarks>
TypePtr hbOrderPowerDown();
/// <summary>Not dispatched by standardRoutineCall.</summary>
/// <remarks>MCX.EXE @ 0x006287d0</remarks>
TypePtr hbOrderFormation();
/// <summary>Not dispatched by standardRoutineCall.</summary>
/// <remarks>MCX.EXE @ 0x00628830</remarks>
TypePtr hbGetFormation();
/// <summary>Not dispatched by standardRoutineCall.</summary>
/// <remarks>MCX.EXE @ 0x00628890</remarks>
TypePtr hbUseSpeed();
/// <summary><c>orderattackobject</c>.</summary>
/// <remarks>MCX.EXE @ 0x006288f0 (unnamed in the symbols)</remarks>
TypePtr hbOrderAttackObject();
/// <remarks>MCX.EXE @ 0x00628a00</remarks>
TypePtr hbOrderAttackContact();
/// <remarks>MCX.EXE @ 0x00628ae0</remarks>
TypePtr hbAttackThreat();
/// <remarks>MCX.EXE @ 0x00628c10</remarks>
TypePtr hbOpenFire();
/// <summary>Not dispatched by standardRoutineCall.</summary>
/// <remarks>MCX.EXE @ 0x00628c30</remarks>
TypePtr hbUseFireRange();
/// <summary>Not dispatched by standardRoutineCall.</summary>
/// <remarks>MCX.EXE @ 0x00628c90</remarks>
TypePtr hbUseFireOdds();
/// <remarks>MCX.EXE @ 0x00628cf0</remarks>
TypePtr hbDamageObject();
/// <remarks>MCX.EXE @ 0x00628e50</remarks>
TypePtr hbSetAttackRadius();
/// <remarks>MCX.EXE @ 0x00628eb0</remarks>
TypePtr hbOrderTest();
/// <remarks>MCX.EXE @ 0x00628f10</remarks>
TypePtr hbPlaySmacker();
/// <remarks>MCX.EXE @ 0x00628f70</remarks>
void hbObjectChangeSides();
/// <remarks>MCX.EXE @ 0x00628ff0</remarks>
TypePtr hbDistanceToObject();
/// <remarks>MCX.EXE @ 0x00629070</remarks>
TypePtr hbDistanceToPosition();
/// <remarks>MCX.EXE @ 0x00629100</remarks>
void hbObjectSuicide();
/// <remarks>MCX.EXE @ 0x00629150</remarks>
TypePtr hbObjectCreate();
/// <remarks>MCX.EXE @ 0x006291b0</remarks>
TypePtr hbObjectExists();
/// <remarks>MCX.EXE @ 0x00629210</remarks>
TypePtr hbObjectStatus();
/// <remarks>MCX.EXE @ 0x00629270</remarks>
TypePtr hbObjectStatusCount();
/// <remarks>MCX.EXE @ 0x00629300</remarks>
TypePtr hbObjectVisible();
/// <remarks>MCX.EXE @ 0x00629380</remarks>
TypePtr hbObjectSide();
/// <remarks>MCX.EXE @ 0x006293e0</remarks>
TypePtr hbObjectCommander();
/// <remarks>MCX.EXE @ 0x00629440</remarks>
TypePtr hbObjectClass();
/// <remarks>MCX.EXE @ 0x006294a0</remarks>
TypePtr hbInArea();
/// <remarks>MCX.EXE @ 0x00629590</remarks>
TypePtr hbSetTimer();
/// <summary><c>checktimer</c>.</summary>
/// <remarks>MCX.EXE @ 0x00629620</remarks>
TypePtr hbChkTimer();
/// <remarks>MCX.EXE @ 0x00629680</remarks>
void hbEndTimer();
/// <remarks>MCX.EXE @ 0x006296d0</remarks>
TypePtr hbSetObjectiveTimer();
/// <remarks>MCX.EXE @ 0x00629760</remarks>
TypePtr hbCheckObjectiveTimer();
/// <remarks>MCX.EXE @ 0x006297c0</remarks>
TypePtr hbSetObjectiveStatus();
/// <remarks>MCX.EXE @ 0x00629840</remarks>
TypePtr hbCheckObjectiveStatus();
/// <remarks>MCX.EXE @ 0x006298a0</remarks>
TypePtr hbSetObjectiveType();
/// <remarks>MCX.EXE @ 0x00629920</remarks>
TypePtr hbCheckObjectiveType();
/// <remarks>MCX.EXE @ 0x00629980</remarks>
TypePtr hbPlayDigitalMusic();
/// <remarks>MCX.EXE @ 0x006299e0</remarks>
TypePtr hbStopMusic();
/// <remarks>MCX.EXE @ 0x00629a10</remarks>
TypePtr hbPlaySoundEffect();
/// <remarks>MCX.EXE @ 0x00629a70</remarks>
TypePtr hbPlayVideo();
/// <remarks>MCX.EXE @ 0x00629ad0</remarks>
TypePtr hbFileExists();
/// <remarks>MCX.EXE @ 0x00629b30</remarks>
TypePtr hbPlaySpeech();
/// <remarks>MCX.EXE @ 0x00629bb0</remarks>
TypePtr hbPlayBetty();
/// <remarks>MCX.EXE @ 0x00629c10</remarks>
TypePtr hbSetRadio();
/// <summary><c>setobjectactive</c>.</summary>
/// <remarks>MCX.EXE @ 0x00629c90</remarks>
TypePtr hbSetObjActive();
/// <summary><c>orderwithdraw</c>.</summary>
/// <remarks>MCX.EXE @ 0x00629d10</remarks>
TypePtr hbObjWithdraw();
/// <summary><c>objectinwithdrawal</c>.</summary>
/// <remarks>MCX.EXE @ 0x00629d30</remarks>
TypePtr hbObjInWithdraw();
/// <summary><c>objecttypeid</c>.</summary>
/// <remarks>MCX.EXE @ 0x00629d90</remarks>
TypePtr hbObjTypeId();
/// <summary><c>getterrainobjectpartid</c>.</summary>
/// <remarks>MCX.EXE @ 0x00629df0</remarks>
TypePtr hbTerrainObjectId();
/// <summary><c>getvehiclepartid</c>.</summary>
/// <remarks>MCX.EXE @ 0x00629e70</remarks>
TypePtr hbVehicleId();
/// <remarks>MCX.EXE @ 0x00629ef0</remarks>
TypePtr hbGetWeaponAmmo();
/// <summary><c>getsensorsworking</c>.</summary>
/// <remarks>MCX.EXE @ 0x00629f70</remarks>
TypePtr hbGetSensors();
/// <summary><c>getcurrentbrvalue</c>; also compiles <c>setcurrentbrvalue</c> (Original behaviour).</summary>
/// <remarks>MCX.EXE @ 0x00629fd0</remarks>
TypePtr hbGetBRValue();
/// <summary>Not dispatched by standardRoutineCall (hbGetBRValue compiles <c>setcurrentbrvalue</c>).</summary>
/// <remarks>MCX.EXE @ 0x0062a030</remarks>
void hbSetBRValue();
/// <summary><c>getarmorpts</c>.</summary>
/// <remarks>MCX.EXE @ 0x0062a0b0</remarks>
TypePtr hbGetArmor();
/// <remarks>MCX.EXE @ 0x0062a110</remarks>
TypePtr hbGetMaxArmor();
/// <remarks>MCX.EXE @ 0x0062a170</remarks>
TypePtr hbGetPilotId();
/// <remarks>MCX.EXE @ 0x0062a1d0</remarks>
TypePtr hbGetPilotWounds();
/// <remarks>MCX.EXE @ 0x0062a230</remarks>
void hbSetPilotWounds();
/// <summary><c>getobjectactive</c>.</summary>
/// <remarks>MCX.EXE @ 0x0062a2b0</remarks>
TypePtr hbGetObjActive();
/// <summary><c>getobjectdamage</c>.</summary>
/// <remarks>MCX.EXE @ 0x0062a310</remarks>
TypePtr hbGetObjDamage();
/// <summary><c>getobjectdmgpts</c>.</summary>
/// <remarks>MCX.EXE @ 0x0062a370</remarks>
TypePtr hbGetObjDmgPts();
/// <summary><c>getobjectmaxdmg</c>.</summary>
/// <remarks>MCX.EXE @ 0x0062a3d0</remarks>
TypePtr hbGetObjMaxDmg();
/// <summary><c>setobjectdamage</c>.</summary>
/// <remarks>MCX.EXE @ 0x0062a430 (unnamed in the symbols)</remarks>
void hbSetObjDamage();
/// <remarks>MCX.EXE @ 0x0062a4b0</remarks>
void hbSetObjectivePos();
/// <remarks>MCX.EXE @ 0x0062a5a0</remarks>
TypePtr hbGetGlobalValue();
/// <remarks>MCX.EXE @ 0x0062a600</remarks>
void hbSetGlobalValue();
/// <remarks>MCX.EXE @ 0x0062a680</remarks>
TypePtr hbSetSensorRange();
/// <remarks>MCX.EXE @ 0x0062a700</remarks>
void hbSetTonnage();
/// <summary><c>setexplosiondamage</c>.</summary>
/// <remarks>MCX.EXE @ 0x0062a780</remarks>
void hbSetExplDmg();
/// <summary><c>setexplosionradius</c>.</summary>
/// <remarks>MCX.EXE @ 0x0062a800</remarks>
void hbSetExplRad();
/// <remarks>MCX.EXE @ 0x0062a880</remarks>
TypePtr hbSetSalvage();
/// <remarks>MCX.EXE @ 0x0062a930</remarks>
TypePtr hbSetSalvageStatus();
/// <remarks>MCX.EXE @ 0x0062a9b0</remarks>
void hbSetAnimation();
/// <summary><c>playwavefile</c>.</summary>
/// <remarks>MCX.EXE @ 0x0062aa50</remarks>
void hbPlayWave();
/// <summary><c>setrevealed</c>.</summary>
/// <remarks>MCX.EXE @ 0x0062aad0 (unnamed in the symbols)</remarks>
void hbSetRevealed();
/// <remarks>MCX.EXE @ 0x0062ab90</remarks>
void hbGetSalvage();
/// <summary><c>orderrefit</c>.</summary>
/// <remarks>MCX.EXE @ 0x0062ac80</remarks>
void hbRefit();
/// <summary><c>ordercapture</c>.</summary>
/// <remarks>MCX.EXE @ 0x0062ad00</remarks>
void hbCaptureObject();
/// <remarks>MCX.EXE @ 0x0062ad80</remarks>
void hbSetCaptured();
/// <remarks>MCX.EXE @ 0x0062add0</remarks>
void hbSetCaptureable();
/// <remarks>MCX.EXE @ 0x0062ae50</remarks>
TypePtr hbIsCaptured();
/// <remarks>MCX.EXE @ 0x0062aeb0</remarks>
TypePtr hbIsCapturable();
/// <remarks>MCX.EXE @ 0x0062af10</remarks>
TypePtr hbWasEverCapturable();
/// <remarks>MCX.EXE @ 0x0062af70</remarks>
void hbSetBuildingName();
/// <remarks>MCX.EXE @ 0x0062aff0</remarks>
void hbCallStrike();
/// <remarks>MCX.EXE @ 0x0062b120</remarks>
void hbCallStrikeEx();
/// <summary><c>orderloadelementals</c>.</summary>
/// <remarks>MCX.EXE @ 0x0062b270</remarks>
void hbLoadElementals();
/// <summary><c>orderdeployelementals</c>.</summary>
/// <remarks>MCX.EXE @ 0x0062b2c0</remarks>
void hbDeployElementals();
/// <remarks>MCX.EXE @ 0x0062b310</remarks>
TypePtr hbAddPrisoner();
/// <remarks>MCX.EXE @ 0x0062b390</remarks>
void hbSetTrainSpeed();
/// <remarks>MCX.EXE @ 0x0062b410</remarks>
void hbLockGateOpen();
/// <remarks>MCX.EXE @ 0x0062b460</remarks>
void hbLockGateClosed();
/// <summary><c>releasegatelock</c>.</summary>
/// <remarks>MCX.EXE @ 0x0062b4b0 (unnamed in the symbols)</remarks>
void hbReleaseGateLock();
/// <remarks>MCX.EXE @ 0x0062b500</remarks>
TypePtr hbIsGateOpen();
/// <summary><c>getrelativepositiontopoint</c>.</summary>
/// <remarks>MCX.EXE @ 0x0062b560</remarks>
void hbGetRelPosPoint();
/// <summary><c>getunitstatus</c>.</summary>
/// <remarks>MCX.EXE @ 0x0062b670 (unnamed in the symbols)</remarks>
TypePtr hbGetUnitStatus();
/// <summary><c>getrelativepositiontoobject</c>.</summary>
/// <remarks>MCX.EXE @ 0x0062b6d0</remarks>
void hbGetRelPosObject();
/// <remarks>MCX.EXE @ 0x0062b7e0</remarks>
void hbRepair();
/// <remarks>MCX.EXE @ 0x0062b860</remarks>
TypePtr hbGetFixed();
/// <remarks>MCX.EXE @ 0x0062b910</remarks>
TypePtr hbGetRepairState();
/// <remarks>MCX.EXE @ 0x0062b970</remarks>
TypePtr hbIsTeamTargeting();
/// <remarks>MCX.EXE @ 0x0062ba20</remarks>
TypePtr hbSendMessage();
/// <remarks>MCX.EXE @ 0x0062baa0</remarks>
TypePtr hbGetMessage();
/// <remarks>MCX.EXE @ 0x0062bb00</remarks>
TypePtr hbGetHomeTeam();
/// <remarks>MCX.EXE @ 0x0062bb20</remarks>
TypePtr hbGetStrikes();
/// <remarks>MCX.EXE @ 0x0062bba0</remarks>
void hbSetStrikes();
/// <remarks>MCX.EXE @ 0x0062bc40</remarks>
void hbAddStrikes();
/// <remarks>MCX.EXE @ 0x0062bce0</remarks>
TypePtr hbIsServer();

/// <summary>Compiles a call of standard routine <paramref name="routineIdPtr"/> by its RoutineKey.</summary>
/// <returns>The result type, or null.</returns>
/// <remarks>MCX.EXE @ 0x0062bd00</remarks>
TypePtr standardRoutineCall(SymTableNodePtr routineIdPtr);
