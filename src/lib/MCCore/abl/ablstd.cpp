#include "stdafx.h"
#include "abl/ablstd.h"
#include "abl/MCAblCompiler.h"

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
    auto IsArrayOf(MCAblType* typePtr, MCAblType* elementTypePtr) -> bool
    {
        return typePtr->Form == MCAblTypeForm::Array && typePtr->Array.ElementTypePtr == elementTypePtr;
    }

    /// <summary>Compiles one argument expression and checks its base type against <paramref name="kind"/>.</summary>
    auto Argument(MCAblCompiler& compiler, MCArgumentKind kind) -> void
    {
        MCAblType* argType = compiler.Expression();
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
            compiler.SyntaxError(MCAblSyntaxError::IncompatibleTypes);
        }
    }

    /// <summary>Compiles "(" arguments ")", each checked against its kind, separated by commas.</summary>
    auto Arguments(MCAblCompiler& compiler, std::initializer_list<MCArgumentKind> kinds) -> void
    {
        if (compiler.Token() != MCAblToken::LParen)
        {
            compiler.SyntaxError(MCAblSyntaxError::WrongNumberOfParams);
            return;
        }

        compiler.NextToken();
        bool first = true;

        for (MCArgumentKind kind : kinds)
        {
            if (!first)
            {
                compiler.IfTokenGetElseError(MCAblToken::Comma, MCAblSyntaxError::MissingComma);
            }

            first = false;
            Argument(compiler, kind);
        }

        compiler.IfTokenGetElseError(MCAblToken::RParen, MCAblSyntaxError::MissingRParen);
    }

    /// <summary>A routine without arguments: "(" is an error.</summary>
    auto NoArguments(MCAblCompiler& compiler) -> void
    {
        if (compiler.Token() == MCAblToken::LParen)
        {
            compiler.SyntaxError(MCAblSyntaxError::WrongNumberOfParams);
        }
    }

    /// <summary>A routine called as a bare statement: anything but ";" after its name is an error.</summary>
    auto StatementOnly(MCAblCompiler& compiler) -> void
    {
        if (compiler.Token() != MCAblToken::Semicolon)
        {
            compiler.SyntaxError(MCAblSyntaxError::WrongNumberOfParams);
        }
    }
}

auto StdReturn(MCAblCompiler& compiler) -> void
{
    if (compiler.Token() == MCAblToken::LParen)
    {
        compiler.NextToken();
        MCAblType* returnType = compiler.Expression();

        if (returnType != compiler.CurrentRoutine()->TypePtr)
        {
            compiler.SyntaxError(MCAblSyntaxError::IncompatibleTypes);
        }

        compiler.IfTokenGetElseError(MCAblToken::RParen, MCAblSyntaxError::MissingRParen);
        return;
    }

    if (compiler.CurrentRoutine()->TypePtr != nullptr)
    {
        compiler.SyntaxError(MCAblSyntaxError::WrongNumberOfParams);
    }
}

auto StdPrint(MCAblCompiler& compiler) -> void
{
    Arguments(compiler, {ARG_PRINTABLE});
}

auto StdConcat(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_STRING, ARG_PRINTABLE});
    return IntegerTypePtr;
}

auto StdAbs(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_REAL});
    return RealTypePtr;
}

auto StdRound(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_REAL});
    return IntegerTypePtr;
}

auto StdTrunc(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_NUMBER});
    return IntegerTypePtr;
}

auto StdSqrt(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_NUMBER});
    return RealTypePtr;
}

auto StdRandom(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return IntegerTypePtr;
}

auto StdGetModHandle(MCAblCompiler& compiler) -> MCAblType*
{
    NoArguments(compiler);
    return IntegerTypePtr;
}

auto StdGetModName(MCAblCompiler& compiler) -> MCAblType*
{
    NoArguments(compiler);
    return nullptr;
}

auto StdSetModName(MCAblCompiler& compiler) -> void
{
    Arguments(compiler, {ARG_STRING});
}

auto StdSetMaxLoops(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return nullptr;
}

auto StdFatal(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER, ARG_STRING});
    return nullptr;
}

auto StdAssert(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_BOOLEAN, ARG_INTEGER, ARG_STRING});
    return nullptr;
}

auto StdHandle(MCAblCompiler& compiler) -> MCAblType*
{
    NoArguments(compiler);
    return IntegerTypePtr;
}

auto HbSetMode(MCAblCompiler& compiler) -> void
{
    Arguments(compiler, {ARG_INTEGER});
}

auto HbSetUpdateTime(MCAblCompiler& compiler) -> void
{
    Arguments(compiler, {ARG_REAL});
}

auto HbGetId(MCAblCompiler& compiler) -> MCAblType*
{
    NoArguments(compiler);
    return IntegerTypePtr;
}

auto HbGetTime(MCAblCompiler& compiler) -> MCAblType*
{
    NoArguments(compiler);
    return RealTypePtr;
}

auto HbGetTimeLeft(MCAblCompiler& compiler) -> MCAblType*
{
    NoArguments(compiler);
    return RealTypePtr;
}

auto HbGetTarget(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbSetTarget(MCAblCompiler& compiler) -> void
{
    Arguments(compiler, {ARG_INTEGER, ARG_INTEGER});
}

auto HbGetContacts(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER_ARRAY, ARG_INTEGER, ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbGetEnemyCount(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbGetWeapons(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER_ARRAY, ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbGetWeaponShots(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbGetWeaponRanges(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER, ARG_REAL_ARRAY});
    return nullptr;
}

auto HbGetMemoryInteger(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbGetMemoryReal(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return RealTypePtr;
}

auto HbGetAlarmTriggers(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER_ARRAY});
    return IntegerTypePtr;
}

auto HbStartFieldScan(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbStartVehicleScan(MCAblCompiler& compiler) -> MCAblType*
{
    StatementOnly(compiler);
    return IntegerTypePtr;
}

auto HbStartContactScan(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbStartMovePath(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER, ARG_INTEGER, ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbSetMoveGoal(MCAblCompiler& compiler) -> void
{
    Arguments(compiler, {ARG_INTEGER, ARG_REAL_ARRAY});
}

auto HbSetMemoryInteger(MCAblCompiler& compiler) -> void
{
    Arguments(compiler, {ARG_INTEGER, ARG_INTEGER});
}

auto HbSetMemoryReal(MCAblCompiler& compiler) -> void
{
    Arguments(compiler, {ARG_INTEGER, ARG_REAL});
}

auto HbGetChallenger(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbGetFireRanges(MCAblCompiler& compiler) -> void
{
    Arguments(compiler, {ARG_REAL_ARRAY});
}

auto HbGetAttackers(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER_ARRAY, ARG_REAL});
    return IntegerTypePtr;
}

auto HbGetAttackerInfo(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return RealTypePtr;
}

auto HbGetTimeWithoutOrders(MCAblCompiler& compiler) -> MCAblType*
{
    NoArguments(compiler);
    return RealTypePtr;
}

auto HbSetChallenger(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER, ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbSelectUnit(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbSelectObject(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbSelectWarrior(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbGetWarriorStatus(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbSelectContact(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER, ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbIsContact(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER, ARG_INTEGER, ARG_BOOLEAN});
    return IntegerTypePtr;
}

auto HbGetContactStatus(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbGetContactId(MCAblCompiler& compiler) -> MCAblType*
{
    NoArguments(compiler);
    return IntegerTypePtr;
}

auto HbGetContactRelativePosition(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_REAL, ARG_REAL});
    return IntegerTypePtr;
}

auto HbSetPotentialContact(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER, ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbSetGuardObjective(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbSetGuardPoint(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_REAL_ARRAY});
    return IntegerTypePtr;
}

auto HbSetGuardRadii(MCAblCompiler& compiler) -> MCAblType*
{
    // Original behaviour: the second argument is preceded by getToken(), not a comma check, so any token separates
    // the two radii.
    if (compiler.Token() == MCAblToken::LParen)
    {
        compiler.NextToken();
        Argument(compiler, ARG_REAL);
        compiler.NextToken();
        Argument(compiler, ARG_REAL);
        compiler.IfTokenGetElseError(MCAblToken::RParen, MCAblSyntaxError::MissingRParen);
    }
    else
    {
        compiler.SyntaxError(MCAblSyntaxError::WrongNumberOfParams);
    }

    return IntegerTypePtr;
}

auto HbGetGuardObjective(MCAblCompiler& compiler) -> MCAblType*
{
    NoArguments(compiler);
    return IntegerTypePtr;
}

auto HbGetGuardPoint(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_REAL_ARRAY});
    return IntegerTypePtr;
}

auto HbGetGuardRadii(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_REAL, ARG_REAL});
    return IntegerTypePtr;
}

auto HbGetGuardDistanceTo(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbHasMoveGoal(MCAblCompiler& compiler) -> MCAblType*
{
    NoArguments(compiler);
    return BooleanTypePtr;
}

auto HbHasMovePath(MCAblCompiler& compiler) -> MCAblType*
{
    NoArguments(compiler);
    return BooleanTypePtr;
}

auto HbSortWeapons(MCAblCompiler& compiler) -> void
{
    Arguments(compiler, {ARG_INTEGER_ARRAY, ARG_INTEGER, ARG_INTEGER});
}

auto HbTimeToImpact(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER, ARG_INTEGER});
    return RealTypePtr;
}

auto HbFireWeapon(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbGetObjectPosition(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER, ARG_REAL_ARRAY});
    return IntegerTypePtr;
}

auto HbGetMoveOrder(MCAblCompiler& compiler) -> MCAblType*
{
    StatementOnly(compiler);
    return IntegerTypePtr;
}

auto HbGetAttackOrder(MCAblCompiler& compiler) -> MCAblType*
{
    StatementOnly(compiler);
    return IntegerTypePtr;
}

auto HbGetVisualRange(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return RealTypePtr;
}

auto HbGetTacOrder(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER, ARG_REAL, ARG_INTEGER_ARRAY});
    return IntegerTypePtr;
}

auto HbGetLastTacOrder(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER, ARG_REAL, ARG_INTEGER_ARRAY});
    return IntegerTypePtr;
}

auto HbGetUnitMates(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER, ARG_INTEGER_ARRAY});
    return IntegerTypePtr;
}

auto HbSetOrderMode(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbWait(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_REAL, ARG_BOOLEAN});
    return IntegerTypePtr;
}

auto HbMoveToPoint(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_REAL_ARRAY, ARG_BOOLEAN});
    return IntegerTypePtr;
}

auto HbMoveToObject(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER, ARG_BOOLEAN});
    return IntegerTypePtr;
}

auto HbMoveToContact(MCAblCompiler& compiler) -> MCAblType*
{
    if (compiler.Token() == MCAblToken::LParen)
    {
        compiler.NextToken();
        Argument(compiler, ARG_BOOLEAN);
        // Original behaviour: a missing ")" reports a missing comma.
        compiler.IfTokenGetElseError(MCAblToken::RParen, MCAblSyntaxError::MissingComma);
    }
    else
    {
        compiler.SyntaxError(MCAblSyntaxError::WrongNumberOfParams);
    }

    return IntegerTypePtr;
}

auto HbOrderPowerUp(MCAblCompiler& compiler) -> MCAblType*
{
    NoArguments(compiler);
    return IntegerTypePtr;
}

auto HbOrderPowerDown(MCAblCompiler& compiler) -> MCAblType*
{
    NoArguments(compiler);
    return IntegerTypePtr;
}

auto HbOrderFormation(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbGetFormation(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbUseSpeed(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbOrderAttackObject(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER, ARG_INTEGER, ARG_INTEGER, ARG_INTEGER, ARG_BOOLEAN});
    return IntegerTypePtr;
}

auto HbOrderAttackContact(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER, ARG_INTEGER, ARG_INTEGER, ARG_BOOLEAN});
    return IntegerTypePtr;
}

auto HbAttackThreat(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER, ARG_INTEGER, ARG_INTEGER, ARG_BOOLEAN, ARG_INTEGER, ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbOpenFire(MCAblCompiler& compiler) -> MCAblType*
{
    StatementOnly(compiler);
    return IntegerTypePtr;
}

auto HbUseFireRange(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbUseFireOdds(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbDamageObject(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER, ARG_INTEGER, ARG_INTEGER, ARG_REAL, ARG_INTEGER, ARG_REAL, ARG_REAL});
    return IntegerTypePtr;
}

auto HbSetAttackRadius(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_REAL});
    return RealTypePtr;
}

auto HbOrderTest(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbPlaySmacker(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbObjectChangeSides(MCAblCompiler& compiler) -> void
{
    Arguments(compiler, {ARG_INTEGER, ARG_INTEGER});
}

auto HbDistanceToObject(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER, ARG_INTEGER});
    return RealTypePtr;
}

auto HbDistanceToPosition(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER, ARG_REAL_ARRAY});
    return RealTypePtr;
}

auto HbObjectSuicide(MCAblCompiler& compiler) -> void
{
    Arguments(compiler, {ARG_INTEGER});
}

auto HbObjectCreate(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbObjectExists(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbObjectStatus(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbObjectStatusCount(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER, ARG_INTEGER_ARRAY});
    return nullptr;
}

auto HbObjectVisible(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER, ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbObjectSide(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbObjectCommander(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbObjectClass(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbInArea(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER, ARG_REAL_ARRAY, ARG_REAL, ARG_INTEGER});
    return BooleanTypePtr;
}

auto HbSetTimer(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER, ARG_NUMBER});
    return IntegerTypePtr;
}

auto HbChkTimer(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return RealTypePtr;
}

auto HbEndTimer(MCAblCompiler& compiler) -> void
{
    Arguments(compiler, {ARG_INTEGER});
}

auto HbSetObjectiveTimer(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER, ARG_NUMBER});
    return IntegerTypePtr;
}

auto HbCheckObjectiveTimer(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return RealTypePtr;
}

auto HbSetObjectiveStatus(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER, ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbCheckObjectiveStatus(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbSetObjectiveType(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER, ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbCheckObjectiveType(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbPlayDigitalMusic(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbStopMusic(MCAblCompiler& compiler) -> MCAblType*
{
    // Original behaviour (OB-049): no getToken() after "(", so "stopmusic()" fails on the "(" as a missing ")".
    if (compiler.Token() == MCAblToken::LParen)
    {
        compiler.IfTokenGetElseError(MCAblToken::RParen, MCAblSyntaxError::MissingRParen);
    }
    else
    {
        compiler.SyntaxError(MCAblSyntaxError::WrongNumberOfParams);
    }

    return IntegerTypePtr;
}

auto HbPlaySoundEffect(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbPlayVideo(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbFileExists(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_STRING});
    return BooleanTypePtr;
}

auto HbPlaySpeech(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER, ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbPlayBetty(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbSetRadio(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER, ARG_BOOLEAN});
    return IntegerTypePtr;
}

auto HbSetObjActive(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER, ARG_BOOLEAN});
    return IntegerTypePtr;
}

auto HbObjWithdraw(MCAblCompiler& compiler) -> MCAblType*
{
    StatementOnly(compiler);
    return IntegerTypePtr;
}

auto HbObjInWithdraw(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbObjTypeId(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbTerrainObjectId(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER, ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbVehicleId(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER, ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbGetWeaponAmmo(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER, ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbGetSensors(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbGetBRValue(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbSetBRValue(MCAblCompiler& compiler) -> void
{
    Arguments(compiler, {ARG_INTEGER, ARG_INTEGER});
}

auto HbGetArmor(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbGetMaxArmor(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbGetPilotId(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbGetPilotWounds(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return RealTypePtr;
}

auto HbSetPilotWounds(MCAblCompiler& compiler) -> void
{
    Arguments(compiler, {ARG_INTEGER, ARG_INTEGER});
}

auto HbGetObjActive(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbGetObjDamage(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbGetObjDmgPts(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbGetObjMaxDmg(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbSetObjDamage(MCAblCompiler& compiler) -> void
{
    Arguments(compiler, {ARG_INTEGER, ARG_INTEGER});
}

auto HbSetObjectivePos(MCAblCompiler& compiler) -> void
{
    Arguments(compiler, {ARG_INTEGER, ARG_NUMBER, ARG_NUMBER, ARG_NUMBER});
}

auto HbGetGlobalValue(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return RealTypePtr;
}

auto HbSetGlobalValue(MCAblCompiler& compiler) -> void
{
    Arguments(compiler, {ARG_INTEGER, ARG_NUMBER});
}

auto HbSetSensorRange(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER, ARG_REAL});
    return IntegerTypePtr;
}

auto HbSetTonnage(MCAblCompiler& compiler) -> void
{
    Arguments(compiler, {ARG_INTEGER, ARG_REAL});
}

auto HbSetExplDmg(MCAblCompiler& compiler) -> void
{
    Arguments(compiler, {ARG_INTEGER, ARG_REAL});
}

auto HbSetExplRad(MCAblCompiler& compiler) -> void
{
    Arguments(compiler, {ARG_INTEGER, ARG_REAL});
}

auto HbSetSalvage(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER, ARG_INTEGER, ARG_INTEGER});
    return BooleanTypePtr;
}

auto HbSetSalvageStatus(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER, ARG_BOOLEAN});
    return BooleanTypePtr;
}

auto HbSetAnimation(MCAblCompiler& compiler) -> void
{
    Arguments(compiler, {ARG_INTEGER, ARG_INTEGER, ARG_INTEGER});
}

auto HbPlayWave(MCAblCompiler& compiler) -> void
{
    Arguments(compiler, {ARG_INTEGER, ARG_REAL});
}

auto HbSetRevealed(MCAblCompiler& compiler) -> void
{
    Arguments(compiler, {ARG_INTEGER, ARG_NUMBER, ARG_REAL_ARRAY});
}

auto HbGetSalvage(MCAblCompiler& compiler) -> void
{
    // Original behaviour: the third argument is preceded by getToken(), not a comma check.
    if (compiler.Token() == MCAblToken::LParen)
    {
        compiler.NextToken();
        Argument(compiler, ARG_INTEGER);
        compiler.IfTokenGetElseError(MCAblToken::Comma, MCAblSyntaxError::MissingComma);
        Argument(compiler, ARG_INTEGER);
        compiler.IfTokenGetElseError(MCAblToken::Comma, MCAblSyntaxError::MissingComma);
        compiler.NextToken();
        Argument(compiler, ARG_INTEGER_ARRAY);
        compiler.IfTokenGetElseError(MCAblToken::Comma, MCAblSyntaxError::MissingComma);
        Argument(compiler, ARG_INTEGER_ARRAY);
        compiler.IfTokenGetElseError(MCAblToken::RParen, MCAblSyntaxError::MissingRParen);
    }
    else
    {
        compiler.SyntaxError(MCAblSyntaxError::WrongNumberOfParams);
    }
}

auto HbRefit(MCAblCompiler& compiler) -> void
{
    Arguments(compiler, {ARG_INTEGER, ARG_INTEGER});
}

auto HbCaptureObject(MCAblCompiler& compiler) -> void
{
    Arguments(compiler, {ARG_INTEGER, ARG_INTEGER});
}

auto HbSetCaptured(MCAblCompiler& compiler) -> void
{
    Arguments(compiler, {ARG_INTEGER});
}

auto HbSetCaptureable(MCAblCompiler& compiler) -> void
{
    Arguments(compiler, {ARG_INTEGER, ARG_BOOLEAN});
}

auto HbIsCaptured(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbIsCapturable(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return BooleanTypePtr;
}

auto HbWasEverCapturable(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return BooleanTypePtr;
}

auto HbSetBuildingName(MCAblCompiler& compiler) -> void
{
    Arguments(compiler, {ARG_INTEGER, ARG_INTEGER});
}

auto HbCallStrike(MCAblCompiler& compiler) -> void
{
    Arguments(compiler, {ARG_INTEGER, ARG_INTEGER, ARG_REAL, ARG_REAL, ARG_REAL, ARG_BOOLEAN});
}

auto HbCallStrikeEx(MCAblCompiler& compiler) -> void
{
    Arguments(compiler, {ARG_INTEGER, ARG_INTEGER, ARG_REAL, ARG_REAL, ARG_REAL, ARG_BOOLEAN, ARG_REAL});
}

auto HbLoadElementals(MCAblCompiler& compiler) -> void
{
    Arguments(compiler, {ARG_INTEGER});
}

auto HbDeployElementals(MCAblCompiler& compiler) -> void
{
    Arguments(compiler, {ARG_INTEGER});
}

auto HbAddPrisoner(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER, ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbSetTrainSpeed(MCAblCompiler& compiler) -> void
{
    Arguments(compiler, {ARG_INTEGER, ARG_REAL});
}

auto HbLockGateOpen(MCAblCompiler& compiler) -> void
{
    Arguments(compiler, {ARG_INTEGER});
}

auto HbLockGateClosed(MCAblCompiler& compiler) -> void
{
    Arguments(compiler, {ARG_INTEGER});
}

auto HbReleaseGateLock(MCAblCompiler& compiler) -> void
{
    Arguments(compiler, {ARG_INTEGER});
}

auto HbIsGateOpen(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return BooleanTypePtr;
}

auto HbGetRelPosPoint(MCAblCompiler& compiler) -> void
{
    Arguments(compiler, {ARG_REAL_ARRAY, ARG_REAL, ARG_REAL, ARG_INTEGER, ARG_REAL_ARRAY});
}

auto HbGetUnitStatus(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return RealTypePtr;
}

auto HbGetRelPosObject(MCAblCompiler& compiler) -> void
{
    Arguments(compiler, {ARG_INTEGER, ARG_REAL, ARG_REAL, ARG_INTEGER, ARG_REAL_ARRAY});
}

auto HbRepair(MCAblCompiler& compiler) -> void
{
    Arguments(compiler, {ARG_INTEGER, ARG_REAL});
}

auto HbGetFixed(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER, ARG_INTEGER, ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbGetRepairState(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbIsTeamTargeting(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER, ARG_INTEGER, ARG_INTEGER});
    return BooleanTypePtr;
}

auto HbSendMessage(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER, ARG_INTEGER});
    return nullptr;
}

auto HbGetMessage(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbGetHomeTeam(MCAblCompiler& compiler) -> MCAblType*
{
    NoArguments(compiler);
    return IntegerTypePtr;
}

auto HbGetStrikes(MCAblCompiler& compiler) -> MCAblType*
{
    Arguments(compiler, {ARG_INTEGER, ARG_INTEGER});
    return IntegerTypePtr;
}

auto HbSetStrikes(MCAblCompiler& compiler) -> void
{
    Arguments(compiler, {ARG_INTEGER, ARG_INTEGER, ARG_INTEGER});
}

auto HbAddStrikes(MCAblCompiler& compiler) -> void
{
    Arguments(compiler, {ARG_INTEGER, ARG_INTEGER, ARG_INTEGER});
}

auto HbIsServer(MCAblCompiler& compiler) -> MCAblType*
{
    NoArguments(compiler);
    return BooleanTypePtr;
}

auto StandardRoutineCall(MCAblCompiler& compiler, MCAblSymbol* routineIdPtr) -> MCAblType*
{
    switch (routineIdPtr->Defn.Info.Routine.Key)
    {
        case MCAblRoutineKey::Return:
        {
            StdReturn(compiler);
            return nullptr;
        }
        case MCAblRoutineKey::Print:
        {
            StdPrint(compiler);
            return nullptr;
        }
        case MCAblRoutineKey::Concat:
            return StdConcat(compiler);
        case MCAblRoutineKey::Abs:
            return StdAbs(compiler);
        case MCAblRoutineKey::Round:
            return StdRound(compiler);
        case MCAblRoutineKey::Sqrt:
            return StdSqrt(compiler);
        case MCAblRoutineKey::Trunc:
            return StdTrunc(compiler);
        case MCAblRoutineKey::Random:
            return StdRandom(compiler);
        case MCAblRoutineKey::SetMaxLoops:
            return StdSetMaxLoops(compiler);
        case MCAblRoutineKey::Fatal:
            return StdFatal(compiler);
        case MCAblRoutineKey::Assert:
            return StdAssert(compiler);
        case MCAblRoutineKey::GetModuleHandle:
        case MCAblRoutineKey::GetMode:
        case MCAblRoutineKey::GetAction:
        case MCAblRoutineKey::GetPhase:
            return StdGetModHandle(compiler);
        case MCAblRoutineKey::GetModuleName:
            return StdGetModName(compiler);
        case MCAblRoutineKey::SetModuleName:
        {
            StdSetModName(compiler);
            return nullptr;
        }
        case MCAblRoutineKey::GetId:
            return HbGetId(compiler);
        case MCAblRoutineKey::GetTime:
            return HbGetTime(compiler);
        case MCAblRoutineKey::GetTimeLeft:
            return HbGetTimeLeft(compiler);
        case MCAblRoutineKey::GetWarriorStatus:
            return HbGetWarriorStatus(compiler);
        case MCAblRoutineKey::SelectWarrior:
            return HbSelectWarrior(compiler);
        case MCAblRoutineKey::SelectObject:
            return HbSelectObject(compiler);
        case MCAblRoutineKey::GetContacts:
            return HbGetContacts(compiler);
        case MCAblRoutineKey::GetEnemyCount:
            return HbGetEnemyCount(compiler);
        case MCAblRoutineKey::SelectContact:
            return HbSelectContact(compiler);
        case MCAblRoutineKey::GetContactId:
            return HbGetContactId(compiler);
        case MCAblRoutineKey::IsContact:
            return HbIsContact(compiler);
        case MCAblRoutineKey::GetContactStatus:
            return HbGetContactStatus(compiler);
        case MCAblRoutineKey::GetContactRelativePosition:
            return HbGetContactRelativePosition(compiler);
        case MCAblRoutineKey::SetGuardObjective:
            return HbSetGuardObjective(compiler);
        case MCAblRoutineKey::SetGuardPoint:
            return HbSetGuardPoint(compiler);
        case MCAblRoutineKey::SetGuardRadii:
            return HbSetGuardRadii(compiler);
        case MCAblRoutineKey::GetGuardObjective:
            return HbGetGuardObjective(compiler);
        case MCAblRoutineKey::GetGuardPoint:
            return HbGetGuardPoint(compiler);
        case MCAblRoutineKey::GetGuardRadii:
            return HbGetGuardRadii(compiler);
        case MCAblRoutineKey::GetGuardDistanceTo:
            return HbGetGuardDistanceTo(compiler);
        case MCAblRoutineKey::GetTarget:
            return HbGetTarget(compiler);
        case MCAblRoutineKey::SetTarget:
        {
            HbSetTarget(compiler);
            return nullptr;
        }
        case MCAblRoutineKey::GetWeaponsReady:
        case MCAblRoutineKey::GetWeaponsLocked:
        case MCAblRoutineKey::GetWeaponsInRange:
            return HbGetWeapons(compiler);
        case MCAblRoutineKey::GetWeaponShots:
            return HbGetWeaponShots(compiler);
        case MCAblRoutineKey::GetWeaponRanges:
            return HbGetWeaponRanges(compiler);
        case MCAblRoutineKey::GetObjectPosition:
            return HbGetObjectPosition(compiler);
        case MCAblRoutineKey::GetIntegerMemory:
            return HbGetMemoryInteger(compiler);
        case MCAblRoutineKey::GetRealMemory:
            return HbGetMemoryReal(compiler);
        case MCAblRoutineKey::GetAlarmTriggers:
            return HbGetAlarmTriggers(compiler);
        case MCAblRoutineKey::GetChallenger:
            return HbGetChallenger(compiler);
        case MCAblRoutineKey::GetFireRanges:
        {
            HbGetFireRanges(compiler);
            return nullptr;
        }
        case MCAblRoutineKey::GetAttackers:
            return HbGetAttackers(compiler);
        case MCAblRoutineKey::GetAttackerInfo:
            return HbGetAttackerInfo(compiler);
        case MCAblRoutineKey::SetChallenger:
            return HbSetChallenger(compiler);
        case MCAblRoutineKey::GetTimeWithoutOrders:
            return HbGetTimeWithoutOrders(compiler);
        case MCAblRoutineKey::SetRadio:
            return HbSetRadio(compiler);
        case MCAblRoutineKey::SetMode:
        case MCAblRoutineKey::SetAction:
        case MCAblRoutineKey::SetPhase:
        {
            HbSetMode(compiler);
            return nullptr;
        }
        case MCAblRoutineKey::SetUpdateTime:
        {
            HbSetUpdateTime(compiler);
            return nullptr;
        }
        case MCAblRoutineKey::SetMoveGoal:
        {
            HbSetMoveGoal(compiler);
            return nullptr;
        }
        case MCAblRoutineKey::SetIntegerMemory:
        {
            HbSetMemoryInteger(compiler);
            return nullptr;
        }
        case MCAblRoutineKey::SetRealMemory:
        {
            HbSetMemoryReal(compiler);
            return nullptr;
        }
        case MCAblRoutineKey::StartFieldScan:
            return HbStartFieldScan(compiler);
        case MCAblRoutineKey::StartEnemyScan:
        case MCAblRoutineKey::StartFriendlyScan:
            return HbStartContactScan(compiler);
        case MCAblRoutineKey::StartMovePath:
            return HbStartMovePath(compiler);
        case MCAblRoutineKey::StartVehicleScan:
            return HbStartVehicleScan(compiler);
        case MCAblRoutineKey::HasMoveGoal:
            return HbHasMoveGoal(compiler);
        case MCAblRoutineKey::HasMovePath:
            return HbHasMovePath(compiler);
        case MCAblRoutineKey::SortWeapons:
        {
            HbSortWeapons(compiler);
            return nullptr;
        }
        case MCAblRoutineKey::TimeToImpact:
            return HbTimeToImpact(compiler);
        case MCAblRoutineKey::FireWeapon:
            return HbFireWeapon(compiler);
        case MCAblRoutineKey::GetVisualRange:
            return HbGetVisualRange(compiler);
        case MCAblRoutineKey::GetUnitMates:
            return HbGetUnitMates(compiler);
        case MCAblRoutineKey::GetTacOrder:
            return HbGetTacOrder(compiler);
        case MCAblRoutineKey::GetLastTacOrder:
            return HbGetLastTacOrder(compiler);
        case MCAblRoutineKey::SetOrderMode:
            return HbSetOrderMode(compiler);
        case MCAblRoutineKey::OrderWait:
            return HbWait(compiler);
        case MCAblRoutineKey::OrderMoveTo:
            return HbMoveToPoint(compiler);
        case MCAblRoutineKey::OrderMoveToObject:
            return HbMoveToObject(compiler);
        case MCAblRoutineKey::OrderMoveToContact:
            return HbMoveToContact(compiler);
        case MCAblRoutineKey::OrderPowerUp:
            return HbOrderPowerUp(compiler);
        case MCAblRoutineKey::OrderPowerDown:
            return HbOrderPowerDown(compiler);
        case MCAblRoutineKey::OrderAttackObject:
            return HbOrderAttackObject(compiler);
        case MCAblRoutineKey::OrderAttackContact:
            return HbOrderAttackContact(compiler);
        case MCAblRoutineKey::AttackThreat:
            return HbAttackThreat(compiler);
        case MCAblRoutineKey::OrderWithdraw:
            return HbObjWithdraw(compiler);
        case MCAblRoutineKey::OpenFire:
            return HbOpenFire(compiler);
        case MCAblRoutineKey::DamageObject:
            return HbDamageObject(compiler);
        case MCAblRoutineKey::SetAttackRadius:
            return HbSetAttackRadius(compiler);
        case MCAblRoutineKey::OrderTest:
            return HbOrderTest(compiler);
        case MCAblRoutineKey::PlaySmacker:
            return HbPlaySmacker(compiler);
        case MCAblRoutineKey::FileExists:
            return HbFileExists(compiler);
        case MCAblRoutineKey::ObjectChangeSides:
        {
            HbObjectChangeSides(compiler);
            return nullptr;
        }
        case MCAblRoutineKey::DistanceToObject:
            return HbDistanceToObject(compiler);
        case MCAblRoutineKey::DistanceToPosition:
            return HbDistanceToPosition(compiler);
        case MCAblRoutineKey::ObjectSuicide:
        {
            HbObjectSuicide(compiler);
            return nullptr;
        }
        case MCAblRoutineKey::ObjectCreate:
            return HbObjectCreate(compiler);
        case MCAblRoutineKey::ObjectExists:
            return HbObjectExists(compiler);
        case MCAblRoutineKey::ObjectStatus:
            return HbObjectStatus(compiler);
        case MCAblRoutineKey::ObjectVisible:
            return HbObjectVisible(compiler);
        case MCAblRoutineKey::ObjectClass:
            return HbObjectClass(compiler);
        case MCAblRoutineKey::ObjectSide:
            return HbObjectSide(compiler);
        case MCAblRoutineKey::ObjectCommander:
            return HbObjectCommander(compiler);
        case MCAblRoutineKey::SetTimer:
            return HbSetTimer(compiler);
        case MCAblRoutineKey::CheckTimer:
            return HbChkTimer(compiler);
        case MCAblRoutineKey::EndTimer:
        {
            HbEndTimer(compiler);
            return nullptr;
        }
        case MCAblRoutineKey::SetObjectiveTimer:
            return HbSetObjectiveTimer(compiler);
        case MCAblRoutineKey::CheckObjectiveTimer:
            return HbCheckObjectiveTimer(compiler);
        case MCAblRoutineKey::SetObjectiveStatus:
            return HbSetObjectiveStatus(compiler);
        case MCAblRoutineKey::CheckObjectiveStatus:
            return HbCheckObjectiveStatus(compiler);
        case MCAblRoutineKey::SetObjectiveType:
            return HbSetObjectiveType(compiler);
        case MCAblRoutineKey::CheckObjectiveType:
            return HbCheckObjectiveType(compiler);
        case MCAblRoutineKey::PlayDigitalMusic:
            return HbPlayDigitalMusic(compiler);
        case MCAblRoutineKey::StopMusic:
            return HbStopMusic(compiler);
        case MCAblRoutineKey::PlaySoundEffect:
            return HbPlaySoundEffect(compiler);
        case MCAblRoutineKey::PlayVideo:
            return HbPlayVideo(compiler);
        case MCAblRoutineKey::PlaySpeech:
            return HbPlaySpeech(compiler);
        case MCAblRoutineKey::PlayBetty:
            return HbPlayBetty(compiler);
        case MCAblRoutineKey::SetObjectActive:
            return HbSetObjActive(compiler);
        case MCAblRoutineKey::ObjectInWithdrawal:
            return HbObjInWithdraw(compiler);
        case MCAblRoutineKey::ObjectTypeId:
            return HbObjTypeId(compiler);
        case MCAblRoutineKey::GetTerrainObjectPartId:
            return HbTerrainObjectId(compiler);
        case MCAblRoutineKey::GetVehiclePartId:
            return HbVehicleId(compiler);
        case MCAblRoutineKey::GetWeaponAmmo:
            return HbGetWeaponAmmo(compiler);
        case MCAblRoutineKey::ObjectStatusCount:
            return HbObjectStatusCount(compiler);
        case MCAblRoutineKey::InArea:
            return HbInArea(compiler);
        case MCAblRoutineKey::GetRelativePositionToPoint:
        {
            HbGetRelPosPoint(compiler);
            return nullptr;
        }
        case MCAblRoutineKey::GetRelativePositionToObject:
        {
            HbGetRelPosObject(compiler);
            return nullptr;
        }
        case MCAblRoutineKey::GetSensorsWorking:
            return HbGetSensors(compiler);
        case MCAblRoutineKey::GetCurrentBRValue:
            return HbGetBRValue(compiler);
        case MCAblRoutineKey::SetCurrentBRValue:
        {
            // Original behaviour: compiled by the getter (one argument), not hbSetBRValue.
            HbGetBRValue(compiler);
            return nullptr;
        }
        case MCAblRoutineKey::GetArmorPts:
            return HbGetArmor(compiler);
        case MCAblRoutineKey::GetMaxArmor:
            return HbGetMaxArmor(compiler);
        case MCAblRoutineKey::GetPilotId:
            return HbGetPilotId(compiler);
        case MCAblRoutineKey::GetPilotWounds:
            return HbGetPilotWounds(compiler);
        case MCAblRoutineKey::SetPilotWounds:
        {
            HbSetPilotWounds(compiler);
            return nullptr;
        }
        case MCAblRoutineKey::GetObjectActive:
            return HbGetObjActive(compiler);
        case MCAblRoutineKey::GetObjectDmgPts:
            return HbGetObjDmgPts(compiler);
        case MCAblRoutineKey::GetObjectMaxDmg:
            return HbGetObjMaxDmg(compiler);
        case MCAblRoutineKey::GetObjectDamage:
            return HbGetObjDamage(compiler);
        case MCAblRoutineKey::SetObjectDamage:
        {
            HbSetObjDamage(compiler);
            return nullptr;
        }
        case MCAblRoutineKey::GetGlobalValue:
            return HbGetGlobalValue(compiler);
        case MCAblRoutineKey::SetGlobalValue:
        {
            HbSetGlobalValue(compiler);
            return nullptr;
        }
        case MCAblRoutineKey::SetObjectivePos:
        {
            HbSetObjectivePos(compiler);
            return nullptr;
        }
        case MCAblRoutineKey::SetPotentialContact:
            return HbSetPotentialContact(compiler);
        case MCAblRoutineKey::SetSensorRange:
            return HbSetSensorRange(compiler);
        case MCAblRoutineKey::SetTonnage:
        {
            HbSetTonnage(compiler);
            return nullptr;
        }
        case MCAblRoutineKey::PlayWaveFile:
        {
            HbPlayWave(compiler);
            return nullptr;
        }
        case MCAblRoutineKey::SetExplosionDamage:
        {
            HbSetExplDmg(compiler);
            return nullptr;
        }
        case MCAblRoutineKey::SetExplosionRadius:
        {
            HbSetExplRad(compiler);
            return nullptr;
        }
        case MCAblRoutineKey::GetSalvage:
        {
            HbGetSalvage(compiler);
            return nullptr;
        }
        case MCAblRoutineKey::SetSalvage:
            return HbSetSalvage(compiler);
        case MCAblRoutineKey::SetSalvageStatus:
            return HbSetSalvageStatus(compiler);
        case MCAblRoutineKey::SetAnimation:
        {
            HbSetAnimation(compiler);
            return nullptr;
        }
        case MCAblRoutineKey::SetRevealed:
        {
            HbSetRevealed(compiler);
            return nullptr;
        }
        case MCAblRoutineKey::OrderRefit:
        {
            HbRefit(compiler);
            return nullptr;
        }
        case MCAblRoutineKey::OrderCapture:
        {
            HbCaptureObject(compiler);
            return nullptr;
        }
        case MCAblRoutineKey::SetCaptured:
        {
            HbSetCaptured(compiler);
            return nullptr;
        }
        case MCAblRoutineKey::SetCaptureable:
        {
            HbSetCaptureable(compiler);
            return nullptr;
        }
        case MCAblRoutineKey::IsCaptured:
            return HbIsCaptured(compiler);
        case MCAblRoutineKey::IsCapturable:
            return HbIsCapturable(compiler);
        case MCAblRoutineKey::WasEverCapturable:
            return HbWasEverCapturable(compiler);
        case MCAblRoutineKey::SetBuildingName:
        {
            HbSetBuildingName(compiler);
            return nullptr;
        }
        case MCAblRoutineKey::CallStrike:
        {
            HbCallStrike(compiler);
            return nullptr;
        }
        case MCAblRoutineKey::OrderLoadElementals:
        {
            HbLoadElementals(compiler);
            return nullptr;
        }
        case MCAblRoutineKey::OrderDeployElementals:
        {
            HbDeployElementals(compiler);
            return nullptr;
        }
        case MCAblRoutineKey::AddPrisoner:
        {
            // Original behaviour: the integer result type is dropped, so the call compiles as a statement.
            HbAddPrisoner(compiler);
            return nullptr;
        }
        case MCAblRoutineKey::SetTrainSpeed:
        {
            HbSetTrainSpeed(compiler);
            return nullptr;
        }
        case MCAblRoutineKey::LockGateOpen:
        {
            HbLockGateOpen(compiler);
            return nullptr;
        }
        case MCAblRoutineKey::LockGateClosed:
        {
            HbLockGateClosed(compiler);
            return nullptr;
        }
        case MCAblRoutineKey::ReleaseGateLock:
        {
            HbReleaseGateLock(compiler);
            return nullptr;
        }
        case MCAblRoutineKey::IsGateOpen:
            return HbIsGateOpen(compiler);
        case MCAblRoutineKey::CallStrikeEx:
        {
            HbCallStrikeEx(compiler);
            return nullptr;
        }
        case MCAblRoutineKey::GetUnitStatus:
            return HbGetUnitStatus(compiler);
        case MCAblRoutineKey::Repair:
        {
            HbRepair(compiler);
            return nullptr;
        }
        case MCAblRoutineKey::GetFixed:
            return HbGetFixed(compiler);
        case MCAblRoutineKey::GetRepairState:
            return HbGetRepairState(compiler);
        case MCAblRoutineKey::IsTeamTargeting:
            return HbIsTeamTargeting(compiler);
        case MCAblRoutineKey::SendMessage:
            return HbSendMessage(compiler);
        case MCAblRoutineKey::GetMessage:
            return HbGetMessage(compiler);
        case MCAblRoutineKey::GetHomeTeam:
            return HbGetHomeTeam(compiler);
        case MCAblRoutineKey::SetStrikes:
        {
            HbSetStrikes(compiler);
            return nullptr;
        }
        case MCAblRoutineKey::GetStrikes:
            return HbGetStrikes(compiler);
        case MCAblRoutineKey::IsServer:
            return HbIsServer(compiler);
        case MCAblRoutineKey::AddStrikes:
        {
            HbAddStrikes(compiler);
            return nullptr;
        }
        default:
            return nullptr;
    }
}
