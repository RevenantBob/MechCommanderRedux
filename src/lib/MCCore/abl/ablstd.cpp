#include "stdafx.h"
#include "abl/ablstd.h"
#include "abl/abldecl.h"
#include "abl/ablerr.h"
#include "abl/ablexpr.h"
#include "abl/ablrtn.h"
#include "abl/ablscan.h"
#include "abl/ablsymt.h"

// Every routine below is the same shape in MCX.EXE: with arguments, "(" then each argument expression checked
// against its type and separated by ",", then ")"; without, a "(" is an error. The per-type checks are inlined in
// every function there; here they are the helpers below.

namespace
{
    /// <summary>The type an argument expression must have.</summary>
    enum ArgumentKind
    {
        /// <summary>integer.</summary>
        ARG_INTEGER,
        /// <summary>real.</summary>
        ARG_REAL,
        /// <summary>boolean.</summary>
        ARG_BOOLEAN,
        /// <summary>integer or real.</summary>
        ARG_NUMBER,
        /// <summary>A char array (string).</summary>
        ARG_STRING,
        /// <summary>An integer array.</summary>
        ARG_INTEGER_ARRAY,
        /// <summary>A real array.</summary>
        ARG_REAL_ARRAY,
        /// <summary>What print and concat take: integer, real, char or a string.</summary>
        ARG_PRINTABLE
    };

    /// <summary>Whether <paramref name="typePtr"/> is an array of <paramref name="elementTypePtr"/>.</summary>
    auto isArrayOf(TypePtr typePtr, TypePtr elementTypePtr) -> bool
    {
        return typePtr->form == FRM_ARRAY && typePtr->info.array.elementTypePtr == elementTypePtr;
    }

    /// <summary>Compiles one argument expression and checks its base type against <paramref name="kind"/>.</summary>
    auto argument(ArgumentKind kind) -> void
    {
        TypePtr argType = baseType(expression());
        bool ok = false;

        switch (kind)
        {
            case ARG_INTEGER:
                ok = argType == IntegerTypePtr;
                break;
            case ARG_REAL:
                ok = argType == RealTypePtr;
                break;
            case ARG_BOOLEAN:
                ok = argType == BooleanTypePtr;
                break;
            case ARG_NUMBER:
                ok = argType == IntegerTypePtr || argType == RealTypePtr;
                break;
            case ARG_STRING:
                ok = isArrayOf(argType, CharTypePtr);
                break;
            case ARG_INTEGER_ARRAY:
                ok = isArrayOf(argType, IntegerTypePtr);
                break;
            case ARG_REAL_ARRAY:
                ok = isArrayOf(argType, RealTypePtr);
                break;
            case ARG_PRINTABLE:
                ok = argType == IntegerTypePtr || argType == RealTypePtr || argType == CharTypePtr ||
                     isArrayOf(argType, CharTypePtr);
                break;
        }

        if (!ok)
        {
            syntaxError(ABL_ERR_SYNTAX_INCOMPATIBLE_TYPES);
        }
    }

    /// <summary>Compiles "(" arguments ")", each checked against its kind, separated by commas.</summary>
    auto arguments(std::initializer_list<ArgumentKind> kinds) -> void
    {
        if (curToken != TKN_LPAREN)
        {
            syntaxError(ABL_ERR_SYNTAX_WRONG_NUMBER_OF_PARAMS);
            return;
        }

        getToken();
        bool first = true;

        for (ArgumentKind kind : kinds)
        {
            if (!first)
            {
                ifTokenGetElseError(TKN_COMMA, ABL_ERR_SYNTAX_MISSING_COMMA);
            }

            first = false;
            argument(kind);
        }

        ifTokenGetElseError(TKN_RPAREN, ABL_ERR_SYNTAX_MISSING_RPAREN);
    }

    /// <summary>A routine without arguments: "(" is an error.</summary>
    auto noArguments() -> void
    {
        if (curToken == TKN_LPAREN)
        {
            syntaxError(ABL_ERR_SYNTAX_WRONG_NUMBER_OF_PARAMS);
        }
    }

    /// <summary>A routine called as a bare statement: anything but ";" after its name is an error.</summary>
    auto statementOnly() -> void
    {
        if (curToken != TKN_SEMICOLON)
        {
            syntaxError(ABL_ERR_SYNTAX_WRONG_NUMBER_OF_PARAMS);
        }
    }
}

auto stdReturn() -> void
{
    if (curToken == TKN_LPAREN)
    {
        getToken();
        TypePtr returnType = baseType(expression());

        if (returnType != baseType(CurRoutineIdPtr->typePtr))
        {
            syntaxError(ABL_ERR_SYNTAX_INCOMPATIBLE_TYPES);
        }

        ifTokenGetElseError(TKN_RPAREN, ABL_ERR_SYNTAX_MISSING_RPAREN);
        return;
    }

    if (CurRoutineIdPtr->typePtr != nullptr)
    {
        syntaxError(ABL_ERR_SYNTAX_WRONG_NUMBER_OF_PARAMS);
    }
}

auto stdPrint() -> void
{
    arguments({ARG_PRINTABLE});
}

auto stdConcat() -> TypePtr
{
    arguments({ARG_STRING, ARG_PRINTABLE});
    return IntegerTypePtr;
}

auto stdAbs() -> TypePtr
{
    arguments({ARG_REAL});
    return RealTypePtr;
}

auto stdRound() -> TypePtr
{
    arguments({ARG_REAL});
    return IntegerTypePtr;
}

auto stdTrunc() -> TypePtr
{
    arguments({ARG_NUMBER});
    return IntegerTypePtr;
}

auto stdSqrt() -> TypePtr
{
    arguments({ARG_NUMBER});
    return RealTypePtr;
}

auto stdRandom() -> TypePtr
{
    arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto stdGetModHandle() -> TypePtr
{
    noArguments();
    return IntegerTypePtr;
}

auto stdGetModName() -> TypePtr
{
    noArguments();
    return nullptr;
}

auto stdSetModName() -> void
{
    arguments({ARG_STRING});
}

auto stdSetMaxLoops() -> TypePtr
{
    arguments({ARG_INTEGER});
    return nullptr;
}

auto stdFatal() -> TypePtr
{
    arguments({ARG_INTEGER, ARG_STRING});
    return nullptr;
}

auto stdAssert() -> TypePtr
{
    arguments({ARG_BOOLEAN, ARG_INTEGER, ARG_STRING});
    return nullptr;
}

auto stdHandle() -> TypePtr
{
    noArguments();
    return IntegerTypePtr;
}

auto hbSetMode() -> void
{
    arguments({ARG_INTEGER});
}

auto hbSetUpdateTime() -> void
{
    arguments({ARG_REAL});
}

auto hbGetId() -> TypePtr
{
    noArguments();
    return IntegerTypePtr;
}

auto hbGetTime() -> TypePtr
{
    noArguments();
    return RealTypePtr;
}

auto hbGetTimeLeft() -> TypePtr
{
    noArguments();
    return RealTypePtr;
}

auto hbGetTarget() -> TypePtr
{
    arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbSetTarget() -> void
{
    arguments({ARG_INTEGER, ARG_INTEGER});
}

auto hbGetContacts() -> TypePtr
{
    arguments({ARG_INTEGER_ARRAY, ARG_INTEGER, ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbGetEnemyCount() -> TypePtr
{
    arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbGetWeapons() -> TypePtr
{
    arguments({ARG_INTEGER_ARRAY, ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbGetWeaponShots() -> TypePtr
{
    arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbGetWeaponRanges() -> TypePtr
{
    arguments({ARG_INTEGER, ARG_REAL_ARRAY});
    return nullptr;
}

auto hbGetMemoryInteger() -> TypePtr
{
    arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbGetMemoryReal() -> TypePtr
{
    arguments({ARG_INTEGER});
    return RealTypePtr;
}

auto hbGetAlarmTriggers() -> TypePtr
{
    arguments({ARG_INTEGER_ARRAY});
    return IntegerTypePtr;
}

auto hbStartFieldScan() -> TypePtr
{
    arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbStartVehicleScan() -> TypePtr
{
    statementOnly();
    return IntegerTypePtr;
}

auto hbStartContactScan() -> TypePtr
{
    arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbStartMovePath() -> TypePtr
{
    arguments({ARG_INTEGER, ARG_INTEGER, ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbSetMoveGoal() -> void
{
    arguments({ARG_INTEGER, ARG_REAL_ARRAY});
}

auto hbSetMemoryInteger() -> void
{
    arguments({ARG_INTEGER, ARG_INTEGER});
}

auto hbSetMemoryReal() -> void
{
    arguments({ARG_INTEGER, ARG_REAL});
}

auto hbGetChallenger() -> TypePtr
{
    arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbGetFireRanges() -> void
{
    arguments({ARG_REAL_ARRAY});
}

auto hbGetAttackers() -> TypePtr
{
    arguments({ARG_INTEGER_ARRAY, ARG_REAL});
    return IntegerTypePtr;
}

auto hbGetAttackerInfo() -> TypePtr
{
    arguments({ARG_INTEGER});
    return RealTypePtr;
}

auto hbGetTimeWithoutOrders() -> TypePtr
{
    noArguments();
    return RealTypePtr;
}

auto hbSetChallenger() -> TypePtr
{
    arguments({ARG_INTEGER, ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbSelectUnit() -> TypePtr
{
    arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbSelectObject() -> TypePtr
{
    arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbSelectWarrior() -> TypePtr
{
    arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbGetWarriorStatus() -> TypePtr
{
    arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbSelectContact() -> TypePtr
{
    arguments({ARG_INTEGER, ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbIsContact() -> TypePtr
{
    arguments({ARG_INTEGER, ARG_INTEGER, ARG_BOOLEAN});
    return IntegerTypePtr;
}

auto hbGetContactStatus() -> TypePtr
{
    arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbGetContactId() -> TypePtr
{
    noArguments();
    return IntegerTypePtr;
}

auto hbGetContactRelativePosition() -> TypePtr
{
    arguments({ARG_REAL, ARG_REAL});
    return IntegerTypePtr;
}

auto hbSetPotentialContact() -> TypePtr
{
    arguments({ARG_INTEGER, ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbSetGuardObjective() -> TypePtr
{
    arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbSetGuardPoint() -> TypePtr
{
    arguments({ARG_REAL_ARRAY});
    return IntegerTypePtr;
}

auto hbSetGuardRadii() -> TypePtr
{
    // Original behaviour: the second argument is preceded by getToken(), not a comma check, so any token separates
    // the two radii.
    if (curToken == TKN_LPAREN)
    {
        getToken();
        argument(ARG_REAL);
        getToken();
        argument(ARG_REAL);
        ifTokenGetElseError(TKN_RPAREN, ABL_ERR_SYNTAX_MISSING_RPAREN);
    }
    else
    {
        syntaxError(ABL_ERR_SYNTAX_WRONG_NUMBER_OF_PARAMS);
    }

    return IntegerTypePtr;
}

auto hbGetGuardObjective() -> TypePtr
{
    noArguments();
    return IntegerTypePtr;
}

auto hbGetGuardPoint() -> TypePtr
{
    arguments({ARG_REAL_ARRAY});
    return IntegerTypePtr;
}

auto hbGetGuardRadii() -> TypePtr
{
    arguments({ARG_REAL, ARG_REAL});
    return IntegerTypePtr;
}

auto hbGetGuardDistanceTo() -> TypePtr
{
    arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbHasMoveGoal() -> TypePtr
{
    noArguments();
    return BooleanTypePtr;
}

auto hbHasMovePath() -> TypePtr
{
    noArguments();
    return BooleanTypePtr;
}

auto hbSortWeapons() -> void
{
    arguments({ARG_INTEGER_ARRAY, ARG_INTEGER, ARG_INTEGER});
}

auto hbTimeToImpact() -> TypePtr
{
    arguments({ARG_INTEGER, ARG_INTEGER});
    return RealTypePtr;
}

auto hbFireWeapon() -> TypePtr
{
    arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbGetObjectPosition() -> TypePtr
{
    arguments({ARG_INTEGER, ARG_REAL_ARRAY});
    return IntegerTypePtr;
}

auto hbGetMoveOrder() -> TypePtr
{
    statementOnly();
    return IntegerTypePtr;
}

auto hbGetAttackOrder() -> TypePtr
{
    statementOnly();
    return IntegerTypePtr;
}

auto hbGetVisualRange() -> TypePtr
{
    arguments({ARG_INTEGER});
    return RealTypePtr;
}

auto hbGetTacOrder() -> TypePtr
{
    arguments({ARG_INTEGER, ARG_REAL, ARG_INTEGER_ARRAY});
    return IntegerTypePtr;
}

auto hbGetLastTacOrder() -> TypePtr
{
    arguments({ARG_INTEGER, ARG_REAL, ARG_INTEGER_ARRAY});
    return IntegerTypePtr;
}

auto hbGetUnitMates() -> TypePtr
{
    arguments({ARG_INTEGER, ARG_INTEGER_ARRAY});
    return IntegerTypePtr;
}

auto hbSetOrderMode() -> TypePtr
{
    arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbWait() -> TypePtr
{
    arguments({ARG_REAL, ARG_BOOLEAN});
    return IntegerTypePtr;
}

auto hbMoveToPoint() -> TypePtr
{
    arguments({ARG_REAL_ARRAY, ARG_BOOLEAN});
    return IntegerTypePtr;
}

auto hbMoveToObject() -> TypePtr
{
    arguments({ARG_INTEGER, ARG_BOOLEAN});
    return IntegerTypePtr;
}

auto hbMoveToContact() -> TypePtr
{
    if (curToken == TKN_LPAREN)
    {
        getToken();
        argument(ARG_BOOLEAN);
        // Original behaviour: a missing ")" reports a missing comma.
        ifTokenGetElseError(TKN_RPAREN, ABL_ERR_SYNTAX_MISSING_COMMA);
    }
    else
    {
        syntaxError(ABL_ERR_SYNTAX_WRONG_NUMBER_OF_PARAMS);
    }

    return IntegerTypePtr;
}

auto hbOrderPowerUp() -> TypePtr
{
    noArguments();
    return IntegerTypePtr;
}

auto hbOrderPowerDown() -> TypePtr
{
    noArguments();
    return IntegerTypePtr;
}

auto hbOrderFormation() -> TypePtr
{
    arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbGetFormation() -> TypePtr
{
    arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbUseSpeed() -> TypePtr
{
    arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbOrderAttackObject() -> TypePtr
{
    arguments({ARG_INTEGER, ARG_INTEGER, ARG_INTEGER, ARG_INTEGER, ARG_BOOLEAN});
    return IntegerTypePtr;
}

auto hbOrderAttackContact() -> TypePtr
{
    arguments({ARG_INTEGER, ARG_INTEGER, ARG_INTEGER, ARG_BOOLEAN});
    return IntegerTypePtr;
}

auto hbAttackThreat() -> TypePtr
{
    arguments({ARG_INTEGER, ARG_INTEGER, ARG_INTEGER, ARG_BOOLEAN, ARG_INTEGER, ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbOpenFire() -> TypePtr
{
    statementOnly();
    return IntegerTypePtr;
}

auto hbUseFireRange() -> TypePtr
{
    arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbUseFireOdds() -> TypePtr
{
    arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbDamageObject() -> TypePtr
{
    arguments({ARG_INTEGER, ARG_INTEGER, ARG_INTEGER, ARG_REAL, ARG_INTEGER, ARG_REAL, ARG_REAL});
    return IntegerTypePtr;
}

auto hbSetAttackRadius() -> TypePtr
{
    arguments({ARG_REAL});
    return RealTypePtr;
}

auto hbOrderTest() -> TypePtr
{
    arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbPlaySmacker() -> TypePtr
{
    arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbObjectChangeSides() -> void
{
    arguments({ARG_INTEGER, ARG_INTEGER});
}

auto hbDistanceToObject() -> TypePtr
{
    arguments({ARG_INTEGER, ARG_INTEGER});
    return RealTypePtr;
}

auto hbDistanceToPosition() -> TypePtr
{
    arguments({ARG_INTEGER, ARG_REAL_ARRAY});
    return RealTypePtr;
}

auto hbObjectSuicide() -> void
{
    arguments({ARG_INTEGER});
}

auto hbObjectCreate() -> TypePtr
{
    arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbObjectExists() -> TypePtr
{
    arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbObjectStatus() -> TypePtr
{
    arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbObjectStatusCount() -> TypePtr
{
    arguments({ARG_INTEGER, ARG_INTEGER_ARRAY});
    return nullptr;
}

auto hbObjectVisible() -> TypePtr
{
    arguments({ARG_INTEGER, ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbObjectSide() -> TypePtr
{
    arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbObjectCommander() -> TypePtr
{
    arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbObjectClass() -> TypePtr
{
    arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbInArea() -> TypePtr
{
    arguments({ARG_INTEGER, ARG_REAL_ARRAY, ARG_REAL, ARG_INTEGER});
    return BooleanTypePtr;
}

auto hbSetTimer() -> TypePtr
{
    arguments({ARG_INTEGER, ARG_NUMBER});
    return IntegerTypePtr;
}

auto hbChkTimer() -> TypePtr
{
    arguments({ARG_INTEGER});
    return RealTypePtr;
}

auto hbEndTimer() -> void
{
    arguments({ARG_INTEGER});
}

auto hbSetObjectiveTimer() -> TypePtr
{
    arguments({ARG_INTEGER, ARG_NUMBER});
    return IntegerTypePtr;
}

auto hbCheckObjectiveTimer() -> TypePtr
{
    arguments({ARG_INTEGER});
    return RealTypePtr;
}

auto hbSetObjectiveStatus() -> TypePtr
{
    arguments({ARG_INTEGER, ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbCheckObjectiveStatus() -> TypePtr
{
    arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbSetObjectiveType() -> TypePtr
{
    arguments({ARG_INTEGER, ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbCheckObjectiveType() -> TypePtr
{
    arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbPlayDigitalMusic() -> TypePtr
{
    arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbStopMusic() -> TypePtr
{
    // Original behaviour (OB-049): no getToken() after "(", so "stopmusic()" fails on the "(" as a missing ")".
    if (curToken == TKN_LPAREN)
    {
        ifTokenGetElseError(TKN_RPAREN, ABL_ERR_SYNTAX_MISSING_RPAREN);
    }
    else
    {
        syntaxError(ABL_ERR_SYNTAX_WRONG_NUMBER_OF_PARAMS);
    }

    return IntegerTypePtr;
}

auto hbPlaySoundEffect() -> TypePtr
{
    arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbPlayVideo() -> TypePtr
{
    arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbFileExists() -> TypePtr
{
    arguments({ARG_STRING});
    return BooleanTypePtr;
}

auto hbPlaySpeech() -> TypePtr
{
    arguments({ARG_INTEGER, ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbPlayBetty() -> TypePtr
{
    arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbSetRadio() -> TypePtr
{
    arguments({ARG_INTEGER, ARG_BOOLEAN});
    return IntegerTypePtr;
}

auto hbSetObjActive() -> TypePtr
{
    arguments({ARG_INTEGER, ARG_BOOLEAN});
    return IntegerTypePtr;
}

auto hbObjWithdraw() -> TypePtr
{
    statementOnly();
    return IntegerTypePtr;
}

auto hbObjInWithdraw() -> TypePtr
{
    arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbObjTypeId() -> TypePtr
{
    arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbTerrainObjectId() -> TypePtr
{
    arguments({ARG_INTEGER, ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbVehicleId() -> TypePtr
{
    arguments({ARG_INTEGER, ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbGetWeaponAmmo() -> TypePtr
{
    arguments({ARG_INTEGER, ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbGetSensors() -> TypePtr
{
    arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbGetBRValue() -> TypePtr
{
    arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbSetBRValue() -> void
{
    arguments({ARG_INTEGER, ARG_INTEGER});
}

auto hbGetArmor() -> TypePtr
{
    arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbGetMaxArmor() -> TypePtr
{
    arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbGetPilotId() -> TypePtr
{
    arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbGetPilotWounds() -> TypePtr
{
    arguments({ARG_INTEGER});
    return RealTypePtr;
}

auto hbSetPilotWounds() -> void
{
    arguments({ARG_INTEGER, ARG_INTEGER});
}

auto hbGetObjActive() -> TypePtr
{
    arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbGetObjDamage() -> TypePtr
{
    arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbGetObjDmgPts() -> TypePtr
{
    arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbGetObjMaxDmg() -> TypePtr
{
    arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbSetObjDamage() -> void
{
    arguments({ARG_INTEGER, ARG_INTEGER});
}

auto hbSetObjectivePos() -> void
{
    arguments({ARG_INTEGER, ARG_NUMBER, ARG_NUMBER, ARG_NUMBER});
}

auto hbGetGlobalValue() -> TypePtr
{
    arguments({ARG_INTEGER});
    return RealTypePtr;
}

auto hbSetGlobalValue() -> void
{
    arguments({ARG_INTEGER, ARG_NUMBER});
}

auto hbSetSensorRange() -> TypePtr
{
    arguments({ARG_INTEGER, ARG_REAL});
    return IntegerTypePtr;
}

auto hbSetTonnage() -> void
{
    arguments({ARG_INTEGER, ARG_REAL});
}

auto hbSetExplDmg() -> void
{
    arguments({ARG_INTEGER, ARG_REAL});
}

auto hbSetExplRad() -> void
{
    arguments({ARG_INTEGER, ARG_REAL});
}

auto hbSetSalvage() -> TypePtr
{
    arguments({ARG_INTEGER, ARG_INTEGER, ARG_INTEGER});
    return BooleanTypePtr;
}

auto hbSetSalvageStatus() -> TypePtr
{
    arguments({ARG_INTEGER, ARG_BOOLEAN});
    return BooleanTypePtr;
}

auto hbSetAnimation() -> void
{
    arguments({ARG_INTEGER, ARG_INTEGER, ARG_INTEGER});
}

auto hbPlayWave() -> void
{
    arguments({ARG_INTEGER, ARG_REAL});
}

auto hbSetRevealed() -> void
{
    arguments({ARG_INTEGER, ARG_NUMBER, ARG_REAL_ARRAY});
}

auto hbGetSalvage() -> void
{
    // Original behaviour: the third argument is preceded by getToken(), not a comma check.
    if (curToken == TKN_LPAREN)
    {
        getToken();
        argument(ARG_INTEGER);
        ifTokenGetElseError(TKN_COMMA, ABL_ERR_SYNTAX_MISSING_COMMA);
        argument(ARG_INTEGER);
        ifTokenGetElseError(TKN_COMMA, ABL_ERR_SYNTAX_MISSING_COMMA);
        getToken();
        argument(ARG_INTEGER_ARRAY);
        ifTokenGetElseError(TKN_COMMA, ABL_ERR_SYNTAX_MISSING_COMMA);
        argument(ARG_INTEGER_ARRAY);
        ifTokenGetElseError(TKN_RPAREN, ABL_ERR_SYNTAX_MISSING_RPAREN);
    }
    else
    {
        syntaxError(ABL_ERR_SYNTAX_WRONG_NUMBER_OF_PARAMS);
    }
}

auto hbRefit() -> void
{
    arguments({ARG_INTEGER, ARG_INTEGER});
}

auto hbCaptureObject() -> void
{
    arguments({ARG_INTEGER, ARG_INTEGER});
}

auto hbSetCaptured() -> void
{
    arguments({ARG_INTEGER});
}

auto hbSetCaptureable() -> void
{
    arguments({ARG_INTEGER, ARG_BOOLEAN});
}

auto hbIsCaptured() -> TypePtr
{
    arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbIsCapturable() -> TypePtr
{
    arguments({ARG_INTEGER});
    return BooleanTypePtr;
}

auto hbWasEverCapturable() -> TypePtr
{
    arguments({ARG_INTEGER});
    return BooleanTypePtr;
}

auto hbSetBuildingName() -> void
{
    arguments({ARG_INTEGER, ARG_INTEGER});
}

auto hbCallStrike() -> void
{
    arguments({ARG_INTEGER, ARG_INTEGER, ARG_REAL, ARG_REAL, ARG_REAL, ARG_BOOLEAN});
}

auto hbCallStrikeEx() -> void
{
    arguments({ARG_INTEGER, ARG_INTEGER, ARG_REAL, ARG_REAL, ARG_REAL, ARG_BOOLEAN, ARG_REAL});
}

auto hbLoadElementals() -> void
{
    arguments({ARG_INTEGER});
}

auto hbDeployElementals() -> void
{
    arguments({ARG_INTEGER});
}

auto hbAddPrisoner() -> TypePtr
{
    arguments({ARG_INTEGER, ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbSetTrainSpeed() -> void
{
    arguments({ARG_INTEGER, ARG_REAL});
}

auto hbLockGateOpen() -> void
{
    arguments({ARG_INTEGER});
}

auto hbLockGateClosed() -> void
{
    arguments({ARG_INTEGER});
}

auto hbReleaseGateLock() -> void
{
    arguments({ARG_INTEGER});
}

auto hbIsGateOpen() -> TypePtr
{
    arguments({ARG_INTEGER});
    return BooleanTypePtr;
}

auto hbGetRelPosPoint() -> void
{
    arguments({ARG_REAL_ARRAY, ARG_REAL, ARG_REAL, ARG_INTEGER, ARG_REAL_ARRAY});
}

auto hbGetUnitStatus() -> TypePtr
{
    arguments({ARG_INTEGER});
    return RealTypePtr;
}

auto hbGetRelPosObject() -> void
{
    arguments({ARG_INTEGER, ARG_REAL, ARG_REAL, ARG_INTEGER, ARG_REAL_ARRAY});
}

auto hbRepair() -> void
{
    arguments({ARG_INTEGER, ARG_REAL});
}

auto hbGetFixed() -> TypePtr
{
    arguments({ARG_INTEGER, ARG_INTEGER, ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbGetRepairState() -> TypePtr
{
    arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbIsTeamTargeting() -> TypePtr
{
    arguments({ARG_INTEGER, ARG_INTEGER, ARG_INTEGER});
    return BooleanTypePtr;
}

auto hbSendMessage() -> TypePtr
{
    arguments({ARG_INTEGER, ARG_INTEGER});
    return nullptr;
}

auto hbGetMessage() -> TypePtr
{
    arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbGetHomeTeam() -> TypePtr
{
    noArguments();
    return IntegerTypePtr;
}

auto hbGetStrikes() -> TypePtr
{
    arguments({ARG_INTEGER, ARG_INTEGER});
    return IntegerTypePtr;
}

auto hbSetStrikes() -> void
{
    arguments({ARG_INTEGER, ARG_INTEGER, ARG_INTEGER});
}

auto hbAddStrikes() -> void
{
    arguments({ARG_INTEGER, ARG_INTEGER, ARG_INTEGER});
}

auto hbIsServer() -> TypePtr
{
    noArguments();
    return BooleanTypePtr;
}

auto standardRoutineCall(SymTableNodePtr routineIdPtr) -> TypePtr
{
    switch (routineIdPtr->defn.info.routine.key)
    {
        case RTN_RETURN:
        {
            stdReturn();
            return nullptr;
        }
        case RTN_PRINT:
        {
            stdPrint();
            return nullptr;
        }
        case RTN_CONCAT:
            return stdConcat();
        case RTN_ABS:
            return stdAbs();
        case RTN_ROUND:
            return stdRound();
        case RTN_SQRT:
            return stdSqrt();
        case RTN_TRUNC:
            return stdTrunc();
        case RTN_RANDOM:
            return stdRandom();
        case RTN_SET_MAX_LOOPS:
            return stdSetMaxLoops();
        case RTN_FATAL:
            return stdFatal();
        case RTN_ASSERT:
            return stdAssert();
        case RTN_GET_MODULE_HANDLE:
        case RTN_GET_MODE:
        case RTN_GET_ACTION:
        case RTN_GET_PHASE:
            return stdGetModHandle();
        case RTN_GET_MODULE_NAME:
            return stdGetModName();
        case RTN_SET_MODULE_NAME:
        {
            stdSetModName();
            return nullptr;
        }
        case RTN_GET_ID:
            return hbGetId();
        case RTN_GET_TIME:
            return hbGetTime();
        case RTN_GET_TIME_LEFT:
            return hbGetTimeLeft();
        case RTN_GET_WARRIOR_STATUS:
            return hbGetWarriorStatus();
        case RTN_SELECT_WARRIOR:
            return hbSelectWarrior();
        case RTN_SELECT_OBJECT:
            return hbSelectObject();
        case RTN_GET_CONTACTS:
            return hbGetContacts();
        case RTN_GET_ENEMY_COUNT:
            return hbGetEnemyCount();
        case RTN_SELECT_CONTACT:
            return hbSelectContact();
        case RTN_GET_CONTACT_ID:
            return hbGetContactId();
        case RTN_IS_CONTACT:
            return hbIsContact();
        case RTN_GET_CONTACT_STATUS:
            return hbGetContactStatus();
        case RTN_GET_CONTACT_RELATIVE_POSITION:
            return hbGetContactRelativePosition();
        case RTN_SET_GUARD_OBJECTIVE:
            return hbSetGuardObjective();
        case RTN_SET_GUARD_POINT:
            return hbSetGuardPoint();
        case RTN_SET_GUARD_RADII:
            return hbSetGuardRadii();
        case RTN_GET_GUARD_OBJECTIVE:
            return hbGetGuardObjective();
        case RTN_GET_GUARD_POINT:
            return hbGetGuardPoint();
        case RTN_GET_GUARD_RADII:
            return hbGetGuardRadii();
        case RTN_GET_GUARD_DISTANCE_TO:
            return hbGetGuardDistanceTo();
        case RTN_GET_TARGET:
            return hbGetTarget();
        case RTN_SET_TARGET:
        {
            hbSetTarget();
            return nullptr;
        }
        case RTN_GET_WEAPONS_READY:
        case RTN_GET_WEAPONS_LOCKED:
        case RTN_GET_WEAPONS_IN_RANGE:
            return hbGetWeapons();
        case RTN_GET_WEAPON_SHOTS:
            return hbGetWeaponShots();
        case RTN_GET_WEAPON_RANGES:
            return hbGetWeaponRanges();
        case RTN_GET_OBJECT_POSITION:
            return hbGetObjectPosition();
        case RTN_GET_INTEGER_MEMORY:
            return hbGetMemoryInteger();
        case RTN_GET_REAL_MEMORY:
            return hbGetMemoryReal();
        case RTN_GET_ALARM_TRIGGERS:
            return hbGetAlarmTriggers();
        case RTN_GET_CHALLENGER:
            return hbGetChallenger();
        case RTN_GET_FIRE_RANGES:
        {
            hbGetFireRanges();
            return nullptr;
        }
        case RTN_GET_ATTACKERS:
            return hbGetAttackers();
        case RTN_GET_ATTACKER_INFO:
            return hbGetAttackerInfo();
        case RTN_SET_CHALLENGER:
            return hbSetChallenger();
        case RTN_GET_TIME_WITHOUT_ORDERS:
            return hbGetTimeWithoutOrders();
        case RTN_SET_RADIO:
            return hbSetRadio();
        case RTN_SET_MODE:
        case RTN_SET_ACTION:
        case RTN_SET_PHASE:
        {
            hbSetMode();
            return nullptr;
        }
        case RTN_SET_UPDATE_TIME:
        {
            hbSetUpdateTime();
            return nullptr;
        }
        case RTN_SET_MOVE_GOAL:
        {
            hbSetMoveGoal();
            return nullptr;
        }
        case RTN_SET_INTEGER_MEMORY:
        {
            hbSetMemoryInteger();
            return nullptr;
        }
        case RTN_SET_REAL_MEMORY:
        {
            hbSetMemoryReal();
            return nullptr;
        }
        case RTN_START_FIELD_SCAN:
            return hbStartFieldScan();
        case RTN_START_ENEMY_SCAN:
        case RTN_START_FRIENDLY_SCAN:
            return hbStartContactScan();
        case RTN_START_MOVE_PATH:
            return hbStartMovePath();
        case RTN_START_VEHICLE_SCAN:
            return hbStartVehicleScan();
        case RTN_HAS_MOVE_GOAL:
            return hbHasMoveGoal();
        case RTN_HAS_MOVE_PATH:
            return hbHasMovePath();
        case RTN_SORT_WEAPONS:
        {
            hbSortWeapons();
            return nullptr;
        }
        case RTN_TIME_TO_IMPACT:
            return hbTimeToImpact();
        case RTN_FIRE_WEAPON:
            return hbFireWeapon();
        case RTN_GET_VISUAL_RANGE:
            return hbGetVisualRange();
        case RTN_GET_UNIT_MATES:
            return hbGetUnitMates();
        case RTN_GET_TAC_ORDER:
            return hbGetTacOrder();
        case RTN_GET_LAST_TAC_ORDER:
            return hbGetLastTacOrder();
        case RTN_SET_ORDER_MODE:
            return hbSetOrderMode();
        case RTN_ORDER_WAIT:
            return hbWait();
        case RTN_ORDER_MOVE_TO:
            return hbMoveToPoint();
        case RTN_ORDER_MOVE_TO_OBJECT:
            return hbMoveToObject();
        case RTN_ORDER_MOVE_TO_CONTACT:
            return hbMoveToContact();
        case RTN_ORDER_POWER_UP:
            return hbOrderPowerUp();
        case RTN_ORDER_POWER_DOWN:
            return hbOrderPowerDown();
        case RTN_ORDER_ATTACK_OBJECT:
            return hbOrderAttackObject();
        case RTN_ORDER_ATTACK_CONTACT:
            return hbOrderAttackContact();
        case RTN_ATTACK_THREAT:
            return hbAttackThreat();
        case RTN_ORDER_WITHDRAW:
            return hbObjWithdraw();
        case RTN_OPEN_FIRE:
            return hbOpenFire();
        case RTN_DAMAGE_OBJECT:
            return hbDamageObject();
        case RTN_SET_ATTACK_RADIUS:
            return hbSetAttackRadius();
        case RTN_ORDER_TEST:
            return hbOrderTest();
        case RTN_PLAY_SMACKER:
            return hbPlaySmacker();
        case RTN_FILE_EXISTS:
            return hbFileExists();
        case RTN_OBJECT_CHANGE_SIDES:
        {
            hbObjectChangeSides();
            return nullptr;
        }
        case RTN_DISTANCE_TO_OBJECT:
            return hbDistanceToObject();
        case RTN_DISTANCE_TO_POSITION:
            return hbDistanceToPosition();
        case RTN_OBJECT_SUICIDE:
        {
            hbObjectSuicide();
            return nullptr;
        }
        case RTN_OBJECT_CREATE:
            return hbObjectCreate();
        case RTN_OBJECT_EXISTS:
            return hbObjectExists();
        case RTN_OBJECT_STATUS:
            return hbObjectStatus();
        case RTN_OBJECT_VISIBLE:
            return hbObjectVisible();
        case RTN_OBJECT_CLASS:
            return hbObjectClass();
        case RTN_OBJECT_SIDE:
            return hbObjectSide();
        case RTN_OBJECT_COMMANDER:
            return hbObjectCommander();
        case RTN_SET_TIMER:
            return hbSetTimer();
        case RTN_CHECK_TIMER:
            return hbChkTimer();
        case RTN_END_TIMER:
        {
            hbEndTimer();
            return nullptr;
        }
        case RTN_SET_OBJECTIVE_TIMER:
            return hbSetObjectiveTimer();
        case RTN_CHECK_OBJECTIVE_TIMER:
            return hbCheckObjectiveTimer();
        case RTN_SET_OBJECTIVE_STATUS:
            return hbSetObjectiveStatus();
        case RTN_CHECK_OBJECTIVE_STATUS:
            return hbCheckObjectiveStatus();
        case RTN_SET_OBJECTIVE_TYPE:
            return hbSetObjectiveType();
        case RTN_CHECK_OBJECTIVE_TYPE:
            return hbCheckObjectiveType();
        case RTN_PLAY_DIGITAL_MUSIC:
            return hbPlayDigitalMusic();
        case RTN_STOP_MUSIC:
            return hbStopMusic();
        case RTN_PLAY_SOUND_EFFECT:
            return hbPlaySoundEffect();
        case RTN_PLAY_VIDEO:
            return hbPlayVideo();
        case RTN_PLAY_SPEECH:
            return hbPlaySpeech();
        case RTN_PLAY_BETTY:
            return hbPlayBetty();
        case RTN_SET_OBJECT_ACTIVE:
            return hbSetObjActive();
        case RTN_OBJECT_IN_WITHDRAWAL:
            return hbObjInWithdraw();
        case RTN_OBJECT_TYPE_ID:
            return hbObjTypeId();
        case RTN_GET_TERRAIN_OBJECT_PART_ID:
            return hbTerrainObjectId();
        case RTN_GET_VEHICLE_PART_ID:
            return hbVehicleId();
        case RTN_GET_WEAPON_AMMO:
            return hbGetWeaponAmmo();
        case RTN_OBJECT_STATUS_COUNT:
            return hbObjectStatusCount();
        case RTN_IN_AREA:
            return hbInArea();
        case RTN_GET_RELATIVE_POSITION_TO_POINT:
        {
            hbGetRelPosPoint();
            return nullptr;
        }
        case RTN_GET_RELATIVE_POSITION_TO_OBJECT:
        {
            hbGetRelPosObject();
            return nullptr;
        }
        case RTN_GET_SENSORS_WORKING:
            return hbGetSensors();
        case RTN_GET_CURRENT_BR_VALUE:
            return hbGetBRValue();
        case RTN_SET_CURRENT_BR_VALUE:
        {
            // Original behaviour: compiled by the getter (one argument), not hbSetBRValue.
            hbGetBRValue();
            return nullptr;
        }
        case RTN_GET_ARMOR_PTS:
            return hbGetArmor();
        case RTN_GET_MAX_ARMOR:
            return hbGetMaxArmor();
        case RTN_GET_PILOT_ID:
            return hbGetPilotId();
        case RTN_GET_PILOT_WOUNDS:
            return hbGetPilotWounds();
        case RTN_SET_PILOT_WOUNDS:
        {
            hbSetPilotWounds();
            return nullptr;
        }
        case RTN_GET_OBJECT_ACTIVE:
            return hbGetObjActive();
        case RTN_GET_OBJECT_DMG_PTS:
            return hbGetObjDmgPts();
        case RTN_GET_OBJECT_MAX_DMG:
            return hbGetObjMaxDmg();
        case RTN_GET_OBJECT_DAMAGE:
            return hbGetObjDamage();
        case RTN_SET_OBJECT_DAMAGE:
        {
            hbSetObjDamage();
            return nullptr;
        }
        case RTN_GET_GLOBAL_VALUE:
            return hbGetGlobalValue();
        case RTN_SET_GLOBAL_VALUE:
        {
            hbSetGlobalValue();
            return nullptr;
        }
        case RTN_SET_OBJECTIVE_POS:
        {
            hbSetObjectivePos();
            return nullptr;
        }
        case RTN_SET_POTENTIAL_CONTACT:
            return hbSetPotentialContact();
        case RTN_SET_SENSOR_RANGE:
            return hbSetSensorRange();
        case RTN_SET_TONNAGE:
        {
            hbSetTonnage();
            return nullptr;
        }
        case RTN_PLAY_WAVE_FILE:
        {
            hbPlayWave();
            return nullptr;
        }
        case RTN_SET_EXPLOSION_DAMAGE:
        {
            hbSetExplDmg();
            return nullptr;
        }
        case RTN_SET_EXPLOSION_RADIUS:
        {
            hbSetExplRad();
            return nullptr;
        }
        case RTN_GET_SALVAGE:
        {
            hbGetSalvage();
            return nullptr;
        }
        case RTN_SET_SALVAGE:
            return hbSetSalvage();
        case RTN_SET_SALVAGE_STATUS:
            return hbSetSalvageStatus();
        case RTN_SET_ANIMATION:
        {
            hbSetAnimation();
            return nullptr;
        }
        case RTN_SET_REVEALED:
        {
            hbSetRevealed();
            return nullptr;
        }
        case RTN_ORDER_REFIT:
        {
            hbRefit();
            return nullptr;
        }
        case RTN_ORDER_CAPTURE:
        {
            hbCaptureObject();
            return nullptr;
        }
        case RTN_SET_CAPTURED:
        {
            hbSetCaptured();
            return nullptr;
        }
        case RTN_SET_CAPTUREABLE:
        {
            hbSetCaptureable();
            return nullptr;
        }
        case RTN_IS_CAPTURED:
            return hbIsCaptured();
        case RTN_IS_CAPTURABLE:
            return hbIsCapturable();
        case RTN_WAS_EVER_CAPTURABLE:
            return hbWasEverCapturable();
        case RTN_SET_BUILDING_NAME:
        {
            hbSetBuildingName();
            return nullptr;
        }
        case RTN_CALL_STRIKE:
        {
            hbCallStrike();
            return nullptr;
        }
        case RTN_ORDER_LOAD_ELEMENTALS:
        {
            hbLoadElementals();
            return nullptr;
        }
        case RTN_ORDER_DEPLOY_ELEMENTALS:
        {
            hbDeployElementals();
            return nullptr;
        }
        case RTN_ADD_PRISONER:
        {
            // Original behaviour: the integer result type is dropped, so the call compiles as a statement.
            hbAddPrisoner();
            return nullptr;
        }
        case RTN_SET_TRAIN_SPEED:
        {
            hbSetTrainSpeed();
            return nullptr;
        }
        case RTN_LOCK_GATE_OPEN:
        {
            hbLockGateOpen();
            return nullptr;
        }
        case RTN_LOCK_GATE_CLOSED:
        {
            hbLockGateClosed();
            return nullptr;
        }
        case RTN_RELEASE_GATE_LOCK:
        {
            hbReleaseGateLock();
            return nullptr;
        }
        case RTN_IS_GATE_OPEN:
            return hbIsGateOpen();
        case RTN_CALL_STRIKE_EX:
        {
            hbCallStrikeEx();
            return nullptr;
        }
        case RTN_GET_UNIT_STATUS:
            return hbGetUnitStatus();
        case RTN_REPAIR:
        {
            hbRepair();
            return nullptr;
        }
        case RTN_GET_FIXED:
            return hbGetFixed();
        case RTN_GET_REPAIR_STATE:
            return hbGetRepairState();
        case RTN_IS_TEAM_TARGETING:
            return hbIsTeamTargeting();
        case RTN_SEND_MESSAGE:
            return hbSendMessage();
        case RTN_GET_MESSAGE:
            return hbGetMessage();
        case RTN_GET_HOME_TEAM:
            return hbGetHomeTeam();
        case RTN_SET_STRIKES:
        {
            hbSetStrikes();
            return nullptr;
        }
        case RTN_GET_STRIKES:
            return hbGetStrikes();
        case RTN_IS_SERVER:
            return hbIsServer();
        case RTN_ADD_STRIKES:
        {
            hbAddStrikes();
            return nullptr;
        }
        default:
            return nullptr;
    }
}
