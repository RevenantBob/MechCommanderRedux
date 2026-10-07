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
    enum MCArgumentKind
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
    auto IsArrayOf(MCTypePtr typePtr, MCTypePtr elementTypePtr) -> bool
    {
        return typePtr->Form == FRM_ARRAY && typePtr->Info.Array.ElementTypePtr == elementTypePtr;
    }

    /// <summary>Compiles one argument expression and checks its base type against <paramref name="kind"/>.</summary>
    auto Argument(MCArgumentKind kind) -> void
    {
        MCTypePtr argType = BaseType(Expression());
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
                ok = IsArrayOf(argType, CharTypePtr);
                break;
            case ARG_INTEGER_ARRAY:
                ok = IsArrayOf(argType, IntegerTypePtr);
                break;
            case ARG_REAL_ARRAY:
                ok = IsArrayOf(argType, RealTypePtr);
                break;
            case ARG_PRINTABLE:
                ok = argType == IntegerTypePtr || argType == RealTypePtr || argType == CharTypePtr ||
                     IsArrayOf(argType, CharTypePtr);
                break;
        }

        if (!ok)
        {
            SyntaxError(ABL_ERR_SYNTAX_INCOMPATIBLE_TYPES);
        }
    }

    /// <summary>Compiles "(" arguments ")", each checked against its kind, separated by commas.</summary>
    auto Arguments(std::initializer_list<MCArgumentKind> kinds) -> void
    {
        if (CurToken != TKN_LPAREN)
        {
            SyntaxError(ABL_ERR_SYNTAX_WRONG_NUMBER_OF_PARAMS);
            return;
        }

        GetToken();
        bool first = true;

        for (MCArgumentKind kind : kinds)
        {
            if (!first)
            {
                IfTokenGetElseError(TKN_COMMA, ABL_ERR_SYNTAX_MISSING_COMMA);
            }

            first = false;
            Argument(kind);
        }

        IfTokenGetElseError(TKN_RPAREN, ABL_ERR_SYNTAX_MISSING_RPAREN);
    }

    /// <summary>A routine without arguments: "(" is an error.</summary>
    auto NoArguments() -> void
    {
        if (CurToken == TKN_LPAREN)
        {
            SyntaxError(ABL_ERR_SYNTAX_WRONG_NUMBER_OF_PARAMS);
        }
    }

    /// <summary>A routine called as a bare statement: anything but ";" after its name is an error.</summary>
    auto StatementOnly() -> void
    {
        if (CurToken != TKN_SEMICOLON)
        {
            SyntaxError(ABL_ERR_SYNTAX_WRONG_NUMBER_OF_PARAMS);
        }
    }
}

auto StdReturn() -> void
{
    if (CurToken == TKN_LPAREN)
    {
        GetToken();
        MCTypePtr returnType = BaseType(Expression());

        if (returnType != BaseType(CurRoutineIdPtr->TypePtr))
        {
            SyntaxError(ABL_ERR_SYNTAX_INCOMPATIBLE_TYPES);
        }

        IfTokenGetElseError(TKN_RPAREN, ABL_ERR_SYNTAX_MISSING_RPAREN);
        return;
    }

    if (CurRoutineIdPtr->TypePtr != nullptr)
    {
        SyntaxError(ABL_ERR_SYNTAX_WRONG_NUMBER_OF_PARAMS);
    }
}

auto StdPrint() -> void
{
    Arguments({ARG_PRINTABLE});
}

auto StdConcat() -> MCTypePtr
{
    Arguments({ARG_STRING, ARG_PRINTABLE});
    return IntegerTypePtr;
}

auto StdAbs() -> MCTypePtr
{
    Arguments({ARG_REAL});
    return RealTypePtr;
}

auto StdRound() -> MCTypePtr
{
    Arguments({ARG_REAL});
    return IntegerTypePtr;
}

auto StdTrunc() -> MCTypePtr
{
    Arguments({ARG_NUMBER});
    return IntegerTypePtr;
}

auto StdSqrt() -> MCTypePtr
{
    Arguments({ARG_NUMBER});
    return RealTypePtr;
}

auto StdRandom() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto StdGetModHandle() -> MCTypePtr
{
    NoArguments();
    return IntegerTypePtr;
}

auto StdGetModName() -> MCTypePtr
{
    NoArguments();
    return nullptr;
}

auto StdSetModName() -> void
{
    Arguments({ARG_STRING});
}

auto StdSetMaxLoops() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return nullptr;
}

auto StdFatal() -> MCTypePtr
{
    Arguments({ARG_INTEGER, ARG_STRING});
    return nullptr;
}

auto StdAssert() -> MCTypePtr
{
    Arguments({ARG_BOOLEAN, ARG_INTEGER, ARG_STRING});
    return nullptr;
}

auto StdHandle() -> MCTypePtr
{
    NoArguments();
    return IntegerTypePtr;
}

auto HbSetMode() -> void
{
    Arguments({ARG_INTEGER});
}

auto HbSetUpdateTime() -> void
{
    Arguments({ARG_REAL});
}

auto HbGetId() -> MCTypePtr
{
    NoArguments();
    return IntegerTypePtr;
}

auto HbGetTime() -> MCTypePtr
{
    NoArguments();
    return RealTypePtr;
}

auto HbGetTimeLeft() -> MCTypePtr
{
    NoArguments();
    return RealTypePtr;
}

auto HbGetTarget() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbSetTarget() -> void
{
    Arguments({ARG_INTEGER, ARG_INTEGER});
}

auto HbGetContacts() -> MCTypePtr
{
    Arguments({ARG_INTEGER_ARRAY, ARG_INTEGER, ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbGetEnemyCount() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbGetWeapons() -> MCTypePtr
{
    Arguments({ARG_INTEGER_ARRAY, ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbGetWeaponShots() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbGetWeaponRanges() -> MCTypePtr
{
    Arguments({ARG_INTEGER, ARG_REAL_ARRAY});
    return nullptr;
}

auto HbGetMemoryInteger() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbGetMemoryReal() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return RealTypePtr;
}

auto HbGetAlarmTriggers() -> MCTypePtr
{
    Arguments({ARG_INTEGER_ARRAY});
    return IntegerTypePtr;
}

auto HbStartFieldScan() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbStartVehicleScan() -> MCTypePtr
{
    StatementOnly();
    return IntegerTypePtr;
}

auto HbStartContactScan() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbStartMovePath() -> MCTypePtr
{
    Arguments({ARG_INTEGER, ARG_INTEGER, ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbSetMoveGoal() -> void
{
    Arguments({ARG_INTEGER, ARG_REAL_ARRAY});
}

auto HbSetMemoryInteger() -> void
{
    Arguments({ARG_INTEGER, ARG_INTEGER});
}

auto HbSetMemoryReal() -> void
{
    Arguments({ARG_INTEGER, ARG_REAL});
}

auto HbGetChallenger() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbGetFireRanges() -> void
{
    Arguments({ARG_REAL_ARRAY});
}

auto HbGetAttackers() -> MCTypePtr
{
    Arguments({ARG_INTEGER_ARRAY, ARG_REAL});
    return IntegerTypePtr;
}

auto HbGetAttackerInfo() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return RealTypePtr;
}

auto HbGetTimeWithoutOrders() -> MCTypePtr
{
    NoArguments();
    return RealTypePtr;
}

auto HbSetChallenger() -> MCTypePtr
{
    Arguments({ARG_INTEGER, ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbSelectUnit() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbSelectObject() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbSelectWarrior() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbGetWarriorStatus() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbSelectContact() -> MCTypePtr
{
    Arguments({ARG_INTEGER, ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbIsContact() -> MCTypePtr
{
    Arguments({ARG_INTEGER, ARG_INTEGER, ARG_BOOLEAN});
    return IntegerTypePtr;
}

auto HbGetContactStatus() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbGetContactId() -> MCTypePtr
{
    NoArguments();
    return IntegerTypePtr;
}

auto HbGetContactRelativePosition() -> MCTypePtr
{
    Arguments({ARG_REAL, ARG_REAL});
    return IntegerTypePtr;
}

auto HbSetPotentialContact() -> MCTypePtr
{
    Arguments({ARG_INTEGER, ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbSetGuardObjective() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbSetGuardPoint() -> MCTypePtr
{
    Arguments({ARG_REAL_ARRAY});
    return IntegerTypePtr;
}

auto HbSetGuardRadii() -> MCTypePtr
{
    // Original behaviour: the second argument is preceded by getToken(), not a comma check, so any token separates
    // the two radii.
    if (CurToken == TKN_LPAREN)
    {
        GetToken();
        Argument(ARG_REAL);
        GetToken();
        Argument(ARG_REAL);
        IfTokenGetElseError(TKN_RPAREN, ABL_ERR_SYNTAX_MISSING_RPAREN);
    }
    else
    {
        SyntaxError(ABL_ERR_SYNTAX_WRONG_NUMBER_OF_PARAMS);
    }

    return IntegerTypePtr;
}

auto HbGetGuardObjective() -> MCTypePtr
{
    NoArguments();
    return IntegerTypePtr;
}

auto HbGetGuardPoint() -> MCTypePtr
{
    Arguments({ARG_REAL_ARRAY});
    return IntegerTypePtr;
}

auto HbGetGuardRadii() -> MCTypePtr
{
    Arguments({ARG_REAL, ARG_REAL});
    return IntegerTypePtr;
}

auto HbGetGuardDistanceTo() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbHasMoveGoal() -> MCTypePtr
{
    NoArguments();
    return BooleanTypePtr;
}

auto HbHasMovePath() -> MCTypePtr
{
    NoArguments();
    return BooleanTypePtr;
}

auto HbSortWeapons() -> void
{
    Arguments({ARG_INTEGER_ARRAY, ARG_INTEGER, ARG_INTEGER});
}

auto HbTimeToImpact() -> MCTypePtr
{
    Arguments({ARG_INTEGER, ARG_INTEGER});
    return RealTypePtr;
}

auto HbFireWeapon() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbGetObjectPosition() -> MCTypePtr
{
    Arguments({ARG_INTEGER, ARG_REAL_ARRAY});
    return IntegerTypePtr;
}

auto HbGetMoveOrder() -> MCTypePtr
{
    StatementOnly();
    return IntegerTypePtr;
}

auto HbGetAttackOrder() -> MCTypePtr
{
    StatementOnly();
    return IntegerTypePtr;
}

auto HbGetVisualRange() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return RealTypePtr;
}

auto HbGetTacOrder() -> MCTypePtr
{
    Arguments({ARG_INTEGER, ARG_REAL, ARG_INTEGER_ARRAY});
    return IntegerTypePtr;
}

auto HbGetLastTacOrder() -> MCTypePtr
{
    Arguments({ARG_INTEGER, ARG_REAL, ARG_INTEGER_ARRAY});
    return IntegerTypePtr;
}

auto HbGetUnitMates() -> MCTypePtr
{
    Arguments({ARG_INTEGER, ARG_INTEGER_ARRAY});
    return IntegerTypePtr;
}

auto HbSetOrderMode() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbWait() -> MCTypePtr
{
    Arguments({ARG_REAL, ARG_BOOLEAN});
    return IntegerTypePtr;
}

auto HbMoveToPoint() -> MCTypePtr
{
    Arguments({ARG_REAL_ARRAY, ARG_BOOLEAN});
    return IntegerTypePtr;
}

auto HbMoveToObject() -> MCTypePtr
{
    Arguments({ARG_INTEGER, ARG_BOOLEAN});
    return IntegerTypePtr;
}

auto HbMoveToContact() -> MCTypePtr
{
    if (CurToken == TKN_LPAREN)
    {
        GetToken();
        Argument(ARG_BOOLEAN);
        // Original behaviour: a missing ")" reports a missing comma.
        IfTokenGetElseError(TKN_RPAREN, ABL_ERR_SYNTAX_MISSING_COMMA);
    }
    else
    {
        SyntaxError(ABL_ERR_SYNTAX_WRONG_NUMBER_OF_PARAMS);
    }

    return IntegerTypePtr;
}

auto HbOrderPowerUp() -> MCTypePtr
{
    NoArguments();
    return IntegerTypePtr;
}

auto HbOrderPowerDown() -> MCTypePtr
{
    NoArguments();
    return IntegerTypePtr;
}

auto HbOrderFormation() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbGetFormation() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbUseSpeed() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbOrderAttackObject() -> MCTypePtr
{
    Arguments({ARG_INTEGER, ARG_INTEGER, ARG_INTEGER, ARG_INTEGER, ARG_BOOLEAN});
    return IntegerTypePtr;
}

auto HbOrderAttackContact() -> MCTypePtr
{
    Arguments({ARG_INTEGER, ARG_INTEGER, ARG_INTEGER, ARG_BOOLEAN});
    return IntegerTypePtr;
}

auto HbAttackThreat() -> MCTypePtr
{
    Arguments({ARG_INTEGER, ARG_INTEGER, ARG_INTEGER, ARG_BOOLEAN, ARG_INTEGER, ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbOpenFire() -> MCTypePtr
{
    StatementOnly();
    return IntegerTypePtr;
}

auto HbUseFireRange() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbUseFireOdds() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbDamageObject() -> MCTypePtr
{
    Arguments({ARG_INTEGER, ARG_INTEGER, ARG_INTEGER, ARG_REAL, ARG_INTEGER, ARG_REAL, ARG_REAL});
    return IntegerTypePtr;
}

auto HbSetAttackRadius() -> MCTypePtr
{
    Arguments({ARG_REAL});
    return RealTypePtr;
}

auto HbOrderTest() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbPlaySmacker() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbObjectChangeSides() -> void
{
    Arguments({ARG_INTEGER, ARG_INTEGER});
}

auto HbDistanceToObject() -> MCTypePtr
{
    Arguments({ARG_INTEGER, ARG_INTEGER});
    return RealTypePtr;
}

auto HbDistanceToPosition() -> MCTypePtr
{
    Arguments({ARG_INTEGER, ARG_REAL_ARRAY});
    return RealTypePtr;
}

auto HbObjectSuicide() -> void
{
    Arguments({ARG_INTEGER});
}

auto HbObjectCreate() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbObjectExists() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbObjectStatus() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbObjectStatusCount() -> MCTypePtr
{
    Arguments({ARG_INTEGER, ARG_INTEGER_ARRAY});
    return nullptr;
}

auto HbObjectVisible() -> MCTypePtr
{
    Arguments({ARG_INTEGER, ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbObjectSide() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbObjectCommander() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbObjectClass() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbInArea() -> MCTypePtr
{
    Arguments({ARG_INTEGER, ARG_REAL_ARRAY, ARG_REAL, ARG_INTEGER});
    return BooleanTypePtr;
}

auto HbSetTimer() -> MCTypePtr
{
    Arguments({ARG_INTEGER, ARG_NUMBER});
    return IntegerTypePtr;
}

auto HbChkTimer() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return RealTypePtr;
}

auto HbEndTimer() -> void
{
    Arguments({ARG_INTEGER});
}

auto HbSetObjectiveTimer() -> MCTypePtr
{
    Arguments({ARG_INTEGER, ARG_NUMBER});
    return IntegerTypePtr;
}

auto HbCheckObjectiveTimer() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return RealTypePtr;
}

auto HbSetObjectiveStatus() -> MCTypePtr
{
    Arguments({ARG_INTEGER, ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbCheckObjectiveStatus() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbSetObjectiveType() -> MCTypePtr
{
    Arguments({ARG_INTEGER, ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbCheckObjectiveType() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbPlayDigitalMusic() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbStopMusic() -> MCTypePtr
{
    // Original behaviour (OB-049): no getToken() after "(", so "stopmusic()" fails on the "(" as a missing ")".
    if (CurToken == TKN_LPAREN)
    {
        IfTokenGetElseError(TKN_RPAREN, ABL_ERR_SYNTAX_MISSING_RPAREN);
    }
    else
    {
        SyntaxError(ABL_ERR_SYNTAX_WRONG_NUMBER_OF_PARAMS);
    }

    return IntegerTypePtr;
}

auto HbPlaySoundEffect() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbPlayVideo() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbFileExists() -> MCTypePtr
{
    Arguments({ARG_STRING});
    return BooleanTypePtr;
}

auto HbPlaySpeech() -> MCTypePtr
{
    Arguments({ARG_INTEGER, ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbPlayBetty() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbSetRadio() -> MCTypePtr
{
    Arguments({ARG_INTEGER, ARG_BOOLEAN});
    return IntegerTypePtr;
}

auto HbSetObjActive() -> MCTypePtr
{
    Arguments({ARG_INTEGER, ARG_BOOLEAN});
    return IntegerTypePtr;
}

auto HbObjWithdraw() -> MCTypePtr
{
    StatementOnly();
    return IntegerTypePtr;
}

auto HbObjInWithdraw() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbObjTypeId() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbTerrainObjectId() -> MCTypePtr
{
    Arguments({ARG_INTEGER, ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbVehicleId() -> MCTypePtr
{
    Arguments({ARG_INTEGER, ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbGetWeaponAmmo() -> MCTypePtr
{
    Arguments({ARG_INTEGER, ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbGetSensors() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbGetBRValue() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbSetBRValue() -> void
{
    Arguments({ARG_INTEGER, ARG_INTEGER});
}

auto HbGetArmor() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbGetMaxArmor() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbGetPilotId() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbGetPilotWounds() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return RealTypePtr;
}

auto HbSetPilotWounds() -> void
{
    Arguments({ARG_INTEGER, ARG_INTEGER});
}

auto HbGetObjActive() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbGetObjDamage() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbGetObjDmgPts() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbGetObjMaxDmg() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbSetObjDamage() -> void
{
    Arguments({ARG_INTEGER, ARG_INTEGER});
}

auto HbSetObjectivePos() -> void
{
    Arguments({ARG_INTEGER, ARG_NUMBER, ARG_NUMBER, ARG_NUMBER});
}

auto HbGetGlobalValue() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return RealTypePtr;
}

auto HbSetGlobalValue() -> void
{
    Arguments({ARG_INTEGER, ARG_NUMBER});
}

auto HbSetSensorRange() -> MCTypePtr
{
    Arguments({ARG_INTEGER, ARG_REAL});
    return IntegerTypePtr;
}

auto HbSetTonnage() -> void
{
    Arguments({ARG_INTEGER, ARG_REAL});
}

auto HbSetExplDmg() -> void
{
    Arguments({ARG_INTEGER, ARG_REAL});
}

auto HbSetExplRad() -> void
{
    Arguments({ARG_INTEGER, ARG_REAL});
}

auto HbSetSalvage() -> MCTypePtr
{
    Arguments({ARG_INTEGER, ARG_INTEGER, ARG_INTEGER});
    return BooleanTypePtr;
}

auto HbSetSalvageStatus() -> MCTypePtr
{
    Arguments({ARG_INTEGER, ARG_BOOLEAN});
    return BooleanTypePtr;
}

auto HbSetAnimation() -> void
{
    Arguments({ARG_INTEGER, ARG_INTEGER, ARG_INTEGER});
}

auto HbPlayWave() -> void
{
    Arguments({ARG_INTEGER, ARG_REAL});
}

auto HbSetRevealed() -> void
{
    Arguments({ARG_INTEGER, ARG_NUMBER, ARG_REAL_ARRAY});
}

auto HbGetSalvage() -> void
{
    // Original behaviour: the third argument is preceded by getToken(), not a comma check.
    if (CurToken == TKN_LPAREN)
    {
        GetToken();
        Argument(ARG_INTEGER);
        IfTokenGetElseError(TKN_COMMA, ABL_ERR_SYNTAX_MISSING_COMMA);
        Argument(ARG_INTEGER);
        IfTokenGetElseError(TKN_COMMA, ABL_ERR_SYNTAX_MISSING_COMMA);
        GetToken();
        Argument(ARG_INTEGER_ARRAY);
        IfTokenGetElseError(TKN_COMMA, ABL_ERR_SYNTAX_MISSING_COMMA);
        Argument(ARG_INTEGER_ARRAY);
        IfTokenGetElseError(TKN_RPAREN, ABL_ERR_SYNTAX_MISSING_RPAREN);
    }
    else
    {
        SyntaxError(ABL_ERR_SYNTAX_WRONG_NUMBER_OF_PARAMS);
    }
}

auto HbRefit() -> void
{
    Arguments({ARG_INTEGER, ARG_INTEGER});
}

auto HbCaptureObject() -> void
{
    Arguments({ARG_INTEGER, ARG_INTEGER});
}

auto HbSetCaptured() -> void
{
    Arguments({ARG_INTEGER});
}

auto HbSetCaptureable() -> void
{
    Arguments({ARG_INTEGER, ARG_BOOLEAN});
}

auto HbIsCaptured() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbIsCapturable() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return BooleanTypePtr;
}

auto HbWasEverCapturable() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return BooleanTypePtr;
}

auto HbSetBuildingName() -> void
{
    Arguments({ARG_INTEGER, ARG_INTEGER});
}

auto HbCallStrike() -> void
{
    Arguments({ARG_INTEGER, ARG_INTEGER, ARG_REAL, ARG_REAL, ARG_REAL, ARG_BOOLEAN});
}

auto HbCallStrikeEx() -> void
{
    Arguments({ARG_INTEGER, ARG_INTEGER, ARG_REAL, ARG_REAL, ARG_REAL, ARG_BOOLEAN, ARG_REAL});
}

auto HbLoadElementals() -> void
{
    Arguments({ARG_INTEGER});
}

auto HbDeployElementals() -> void
{
    Arguments({ARG_INTEGER});
}

auto HbAddPrisoner() -> MCTypePtr
{
    Arguments({ARG_INTEGER, ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbSetTrainSpeed() -> void
{
    Arguments({ARG_INTEGER, ARG_REAL});
}

auto HbLockGateOpen() -> void
{
    Arguments({ARG_INTEGER});
}

auto HbLockGateClosed() -> void
{
    Arguments({ARG_INTEGER});
}

auto HbReleaseGateLock() -> void
{
    Arguments({ARG_INTEGER});
}

auto HbIsGateOpen() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return BooleanTypePtr;
}

auto HbGetRelPosPoint() -> void
{
    Arguments({ARG_REAL_ARRAY, ARG_REAL, ARG_REAL, ARG_INTEGER, ARG_REAL_ARRAY});
}

auto HbGetUnitStatus() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return RealTypePtr;
}

auto HbGetRelPosObject() -> void
{
    Arguments({ARG_INTEGER, ARG_REAL, ARG_REAL, ARG_INTEGER, ARG_REAL_ARRAY});
}

auto HbRepair() -> void
{
    Arguments({ARG_INTEGER, ARG_REAL});
}

auto HbGetFixed() -> MCTypePtr
{
    Arguments({ARG_INTEGER, ARG_INTEGER, ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbGetRepairState() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbIsTeamTargeting() -> MCTypePtr
{
    Arguments({ARG_INTEGER, ARG_INTEGER, ARG_INTEGER});
    return BooleanTypePtr;
}

auto HbSendMessage() -> MCTypePtr
{
    Arguments({ARG_INTEGER, ARG_INTEGER});
    return nullptr;
}

auto HbGetMessage() -> MCTypePtr
{
    Arguments({ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbGetHomeTeam() -> MCTypePtr
{
    NoArguments();
    return IntegerTypePtr;
}

auto HbGetStrikes() -> MCTypePtr
{
    Arguments({ARG_INTEGER, ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbSetStrikes() -> void
{
    Arguments({ARG_INTEGER, ARG_INTEGER, ARG_INTEGER});
}

auto HbAddStrikes() -> void
{
    Arguments({ARG_INTEGER, ARG_INTEGER, ARG_INTEGER});
}

auto HbIsServer() -> MCTypePtr
{
    NoArguments();
    return BooleanTypePtr;
}

auto StandardRoutineCall(MCSymTableNodePtr routineIdPtr) -> MCTypePtr
{
    switch (routineIdPtr->Defn.Info.Routine.Key)
    {
        case RTN_RETURN:
        {
            StdReturn();
            return nullptr;
        }
        case RTN_PRINT:
        {
            StdPrint();
            return nullptr;
        }
        case RTN_CONCAT:
            return StdConcat();
        case RTN_ABS:
            return StdAbs();
        case RTN_ROUND:
            return StdRound();
        case RTN_SQRT:
            return StdSqrt();
        case RTN_TRUNC:
            return StdTrunc();
        case RTN_RANDOM:
            return StdRandom();
        case RTN_SET_MAX_LOOPS:
            return StdSetMaxLoops();
        case RTN_FATAL:
            return StdFatal();
        case RTN_ASSERT:
            return StdAssert();
        case RTN_GET_MODULE_HANDLE:
        case RTN_GET_MODE:
        case RTN_GET_ACTION:
        case RTN_GET_PHASE:
            return StdGetModHandle();
        case RTN_GET_MODULE_NAME:
            return StdGetModName();
        case RTN_SET_MODULE_NAME:
        {
            StdSetModName();
            return nullptr;
        }
        case RTN_GET_ID:
            return HbGetId();
        case RTN_GET_TIME:
            return HbGetTime();
        case RTN_GET_TIME_LEFT:
            return HbGetTimeLeft();
        case RTN_GET_WARRIOR_STATUS:
            return HbGetWarriorStatus();
        case RTN_SELECT_WARRIOR:
            return HbSelectWarrior();
        case RTN_SELECT_OBJECT:
            return HbSelectObject();
        case RTN_GET_CONTACTS:
            return HbGetContacts();
        case RTN_GET_ENEMY_COUNT:
            return HbGetEnemyCount();
        case RTN_SELECT_CONTACT:
            return HbSelectContact();
        case RTN_GET_CONTACT_ID:
            return HbGetContactId();
        case RTN_IS_CONTACT:
            return HbIsContact();
        case RTN_GET_CONTACT_STATUS:
            return HbGetContactStatus();
        case RTN_GET_CONTACT_RELATIVE_POSITION:
            return HbGetContactRelativePosition();
        case RTN_SET_GUARD_OBJECTIVE:
            return HbSetGuardObjective();
        case RTN_SET_GUARD_POINT:
            return HbSetGuardPoint();
        case RTN_SET_GUARD_RADII:
            return HbSetGuardRadii();
        case RTN_GET_GUARD_OBJECTIVE:
            return HbGetGuardObjective();
        case RTN_GET_GUARD_POINT:
            return HbGetGuardPoint();
        case RTN_GET_GUARD_RADII:
            return HbGetGuardRadii();
        case RTN_GET_GUARD_DISTANCE_TO:
            return HbGetGuardDistanceTo();
        case RTN_GET_TARGET:
            return HbGetTarget();
        case RTN_SET_TARGET:
        {
            HbSetTarget();
            return nullptr;
        }
        case RTN_GET_WEAPONS_READY:
        case RTN_GET_WEAPONS_LOCKED:
        case RTN_GET_WEAPONS_IN_RANGE:
            return HbGetWeapons();
        case RTN_GET_WEAPON_SHOTS:
            return HbGetWeaponShots();
        case RTN_GET_WEAPON_RANGES:
            return HbGetWeaponRanges();
        case RTN_GET_OBJECT_POSITION:
            return HbGetObjectPosition();
        case RTN_GET_INTEGER_MEMORY:
            return HbGetMemoryInteger();
        case RTN_GET_REAL_MEMORY:
            return HbGetMemoryReal();
        case RTN_GET_ALARM_TRIGGERS:
            return HbGetAlarmTriggers();
        case RTN_GET_CHALLENGER:
            return HbGetChallenger();
        case RTN_GET_FIRE_RANGES:
        {
            HbGetFireRanges();
            return nullptr;
        }
        case RTN_GET_ATTACKERS:
            return HbGetAttackers();
        case RTN_GET_ATTACKER_INFO:
            return HbGetAttackerInfo();
        case RTN_SET_CHALLENGER:
            return HbSetChallenger();
        case RTN_GET_TIME_WITHOUT_ORDERS:
            return HbGetTimeWithoutOrders();
        case RTN_SET_RADIO:
            return HbSetRadio();
        case RTN_SET_MODE:
        case RTN_SET_ACTION:
        case RTN_SET_PHASE:
        {
            HbSetMode();
            return nullptr;
        }
        case RTN_SET_UPDATE_TIME:
        {
            HbSetUpdateTime();
            return nullptr;
        }
        case RTN_SET_MOVE_GOAL:
        {
            HbSetMoveGoal();
            return nullptr;
        }
        case RTN_SET_INTEGER_MEMORY:
        {
            HbSetMemoryInteger();
            return nullptr;
        }
        case RTN_SET_REAL_MEMORY:
        {
            HbSetMemoryReal();
            return nullptr;
        }
        case RTN_START_FIELD_SCAN:
            return HbStartFieldScan();
        case RTN_START_ENEMY_SCAN:
        case RTN_START_FRIENDLY_SCAN:
            return HbStartContactScan();
        case RTN_START_MOVE_PATH:
            return HbStartMovePath();
        case RTN_START_VEHICLE_SCAN:
            return HbStartVehicleScan();
        case RTN_HAS_MOVE_GOAL:
            return HbHasMoveGoal();
        case RTN_HAS_MOVE_PATH:
            return HbHasMovePath();
        case RTN_SORT_WEAPONS:
        {
            HbSortWeapons();
            return nullptr;
        }
        case RTN_TIME_TO_IMPACT:
            return HbTimeToImpact();
        case RTN_FIRE_WEAPON:
            return HbFireWeapon();
        case RTN_GET_VISUAL_RANGE:
            return HbGetVisualRange();
        case RTN_GET_UNIT_MATES:
            return HbGetUnitMates();
        case RTN_GET_TAC_ORDER:
            return HbGetTacOrder();
        case RTN_GET_LAST_TAC_ORDER:
            return HbGetLastTacOrder();
        case RTN_SET_ORDER_MODE:
            return HbSetOrderMode();
        case RTN_ORDER_WAIT:
            return HbWait();
        case RTN_ORDER_MOVE_TO:
            return HbMoveToPoint();
        case RTN_ORDER_MOVE_TO_OBJECT:
            return HbMoveToObject();
        case RTN_ORDER_MOVE_TO_CONTACT:
            return HbMoveToContact();
        case RTN_ORDER_POWER_UP:
            return HbOrderPowerUp();
        case RTN_ORDER_POWER_DOWN:
            return HbOrderPowerDown();
        case RTN_ORDER_ATTACK_OBJECT:
            return HbOrderAttackObject();
        case RTN_ORDER_ATTACK_CONTACT:
            return HbOrderAttackContact();
        case RTN_ATTACK_THREAT:
            return HbAttackThreat();
        case RTN_ORDER_WITHDRAW:
            return HbObjWithdraw();
        case RTN_OPEN_FIRE:
            return HbOpenFire();
        case RTN_DAMAGE_OBJECT:
            return HbDamageObject();
        case RTN_SET_ATTACK_RADIUS:
            return HbSetAttackRadius();
        case RTN_ORDER_TEST:
            return HbOrderTest();
        case RTN_PLAY_SMACKER:
            return HbPlaySmacker();
        case RTN_FILE_EXISTS:
            return HbFileExists();
        case RTN_OBJECT_CHANGE_SIDES:
        {
            HbObjectChangeSides();
            return nullptr;
        }
        case RTN_DISTANCE_TO_OBJECT:
            return HbDistanceToObject();
        case RTN_DISTANCE_TO_POSITION:
            return HbDistanceToPosition();
        case RTN_OBJECT_SUICIDE:
        {
            HbObjectSuicide();
            return nullptr;
        }
        case RTN_OBJECT_CREATE:
            return HbObjectCreate();
        case RTN_OBJECT_EXISTS:
            return HbObjectExists();
        case RTN_OBJECT_STATUS:
            return HbObjectStatus();
        case RTN_OBJECT_VISIBLE:
            return HbObjectVisible();
        case RTN_OBJECT_CLASS:
            return HbObjectClass();
        case RTN_OBJECT_SIDE:
            return HbObjectSide();
        case RTN_OBJECT_COMMANDER:
            return HbObjectCommander();
        case RTN_SET_TIMER:
            return HbSetTimer();
        case RTN_CHECK_TIMER:
            return HbChkTimer();
        case RTN_END_TIMER:
        {
            HbEndTimer();
            return nullptr;
        }
        case RTN_SET_OBJECTIVE_TIMER:
            return HbSetObjectiveTimer();
        case RTN_CHECK_OBJECTIVE_TIMER:
            return HbCheckObjectiveTimer();
        case RTN_SET_OBJECTIVE_STATUS:
            return HbSetObjectiveStatus();
        case RTN_CHECK_OBJECTIVE_STATUS:
            return HbCheckObjectiveStatus();
        case RTN_SET_OBJECTIVE_TYPE:
            return HbSetObjectiveType();
        case RTN_CHECK_OBJECTIVE_TYPE:
            return HbCheckObjectiveType();
        case RTN_PLAY_DIGITAL_MUSIC:
            return HbPlayDigitalMusic();
        case RTN_STOP_MUSIC:
            return HbStopMusic();
        case RTN_PLAY_SOUND_EFFECT:
            return HbPlaySoundEffect();
        case RTN_PLAY_VIDEO:
            return HbPlayVideo();
        case RTN_PLAY_SPEECH:
            return HbPlaySpeech();
        case RTN_PLAY_BETTY:
            return HbPlayBetty();
        case RTN_SET_OBJECT_ACTIVE:
            return HbSetObjActive();
        case RTN_OBJECT_IN_WITHDRAWAL:
            return HbObjInWithdraw();
        case RTN_OBJECT_TYPE_ID:
            return HbObjTypeId();
        case RTN_GET_TERRAIN_OBJECT_PART_ID:
            return HbTerrainObjectId();
        case RTN_GET_VEHICLE_PART_ID:
            return HbVehicleId();
        case RTN_GET_WEAPON_AMMO:
            return HbGetWeaponAmmo();
        case RTN_OBJECT_STATUS_COUNT:
            return HbObjectStatusCount();
        case RTN_IN_AREA:
            return HbInArea();
        case RTN_GET_RELATIVE_POSITION_TO_POINT:
        {
            HbGetRelPosPoint();
            return nullptr;
        }
        case RTN_GET_RELATIVE_POSITION_TO_OBJECT:
        {
            HbGetRelPosObject();
            return nullptr;
        }
        case RTN_GET_SENSORS_WORKING:
            return HbGetSensors();
        case RTN_GET_CURRENT_BR_VALUE:
            return HbGetBRValue();
        case RTN_SET_CURRENT_BR_VALUE:
        {
            // Original behaviour: compiled by the getter (one argument), not hbSetBRValue.
            HbGetBRValue();
            return nullptr;
        }
        case RTN_GET_ARMOR_PTS:
            return HbGetArmor();
        case RTN_GET_MAX_ARMOR:
            return HbGetMaxArmor();
        case RTN_GET_PILOT_ID:
            return HbGetPilotId();
        case RTN_GET_PILOT_WOUNDS:
            return HbGetPilotWounds();
        case RTN_SET_PILOT_WOUNDS:
        {
            HbSetPilotWounds();
            return nullptr;
        }
        case RTN_GET_OBJECT_ACTIVE:
            return HbGetObjActive();
        case RTN_GET_OBJECT_DMG_PTS:
            return HbGetObjDmgPts();
        case RTN_GET_OBJECT_MAX_DMG:
            return HbGetObjMaxDmg();
        case RTN_GET_OBJECT_DAMAGE:
            return HbGetObjDamage();
        case RTN_SET_OBJECT_DAMAGE:
        {
            HbSetObjDamage();
            return nullptr;
        }
        case RTN_GET_GLOBAL_VALUE:
            return HbGetGlobalValue();
        case RTN_SET_GLOBAL_VALUE:
        {
            HbSetGlobalValue();
            return nullptr;
        }
        case RTN_SET_OBJECTIVE_POS:
        {
            HbSetObjectivePos();
            return nullptr;
        }
        case RTN_SET_POTENTIAL_CONTACT:
            return HbSetPotentialContact();
        case RTN_SET_SENSOR_RANGE:
            return HbSetSensorRange();
        case RTN_SET_TONNAGE:
        {
            HbSetTonnage();
            return nullptr;
        }
        case RTN_PLAY_WAVE_FILE:
        {
            HbPlayWave();
            return nullptr;
        }
        case RTN_SET_EXPLOSION_DAMAGE:
        {
            HbSetExplDmg();
            return nullptr;
        }
        case RTN_SET_EXPLOSION_RADIUS:
        {
            HbSetExplRad();
            return nullptr;
        }
        case RTN_GET_SALVAGE:
        {
            HbGetSalvage();
            return nullptr;
        }
        case RTN_SET_SALVAGE:
            return HbSetSalvage();
        case RTN_SET_SALVAGE_STATUS:
            return HbSetSalvageStatus();
        case RTN_SET_ANIMATION:
        {
            HbSetAnimation();
            return nullptr;
        }
        case RTN_SET_REVEALED:
        {
            HbSetRevealed();
            return nullptr;
        }
        case RTN_ORDER_REFIT:
        {
            HbRefit();
            return nullptr;
        }
        case RTN_ORDER_CAPTURE:
        {
            HbCaptureObject();
            return nullptr;
        }
        case RTN_SET_CAPTURED:
        {
            HbSetCaptured();
            return nullptr;
        }
        case RTN_SET_CAPTUREABLE:
        {
            HbSetCaptureable();
            return nullptr;
        }
        case RTN_IS_CAPTURED:
            return HbIsCaptured();
        case RTN_IS_CAPTURABLE:
            return HbIsCapturable();
        case RTN_WAS_EVER_CAPTURABLE:
            return HbWasEverCapturable();
        case RTN_SET_BUILDING_NAME:
        {
            HbSetBuildingName();
            return nullptr;
        }
        case RTN_CALL_STRIKE:
        {
            HbCallStrike();
            return nullptr;
        }
        case RTN_ORDER_LOAD_ELEMENTALS:
        {
            HbLoadElementals();
            return nullptr;
        }
        case RTN_ORDER_DEPLOY_ELEMENTALS:
        {
            HbDeployElementals();
            return nullptr;
        }
        case RTN_ADD_PRISONER:
        {
            // Original behaviour: the integer result type is dropped, so the call compiles as a statement.
            HbAddPrisoner();
            return nullptr;
        }
        case RTN_SET_TRAIN_SPEED:
        {
            HbSetTrainSpeed();
            return nullptr;
        }
        case RTN_LOCK_GATE_OPEN:
        {
            HbLockGateOpen();
            return nullptr;
        }
        case RTN_LOCK_GATE_CLOSED:
        {
            HbLockGateClosed();
            return nullptr;
        }
        case RTN_RELEASE_GATE_LOCK:
        {
            HbReleaseGateLock();
            return nullptr;
        }
        case RTN_IS_GATE_OPEN:
            return HbIsGateOpen();
        case RTN_CALL_STRIKE_EX:
        {
            HbCallStrikeEx();
            return nullptr;
        }
        case RTN_GET_UNIT_STATUS:
            return HbGetUnitStatus();
        case RTN_REPAIR:
        {
            HbRepair();
            return nullptr;
        }
        case RTN_GET_FIXED:
            return HbGetFixed();
        case RTN_GET_REPAIR_STATE:
            return HbGetRepairState();
        case RTN_IS_TEAM_TARGETING:
            return HbIsTeamTargeting();
        case RTN_SEND_MESSAGE:
            return HbSendMessage();
        case RTN_GET_MESSAGE:
            return HbGetMessage();
        case RTN_GET_HOME_TEAM:
            return HbGetHomeTeam();
        case RTN_SET_STRIKES:
        {
            HbSetStrikes();
            return nullptr;
        }
        case RTN_GET_STRIKES:
            return HbGetStrikes();
        case RTN_IS_SERVER:
            return HbIsServer();
        case RTN_ADD_STRIKES:
        {
            HbAddStrikes();
            return nullptr;
        }
        default:
            return nullptr;
    }
}
