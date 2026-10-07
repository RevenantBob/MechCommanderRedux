#include "stdafx.h"
#include "abl/MCAblCallCompiler.h"
#include "abl/MCAblCompiler.h"

// Every standard routine's call compiles the same way in MCX.EXE (its hb* and std* functions, one per routine, with
// the type checks inlined): with arguments, "(" then each argument expression checked against its type and separated
// by ",", then ")"; without, a "(" is an error. Here a table gives each routine's argument types and result; the
// five calls that compile differently keep a function.

namespace
{
    /// <summary>The type an argument expression must have.</summary>
    enum class MCAblArgument : uint8_t
    {
        Integer,
        Real,
        Boolean,
        /// <summary>integer or real.</summary>
        Number,
        /// <summary>A char array (string).</summary>
        String,
        IntegerArray,
        RealArray,
        /// <summary>What print and concat take: integer, real, char or a string.</summary>
        Printable
    };

    /// <summary>How a call's arguments are read.</summary>
    enum class MCAblCallForm : uint8_t
    {
        /// <summary>Not a routine the compiler knows: nothing is read, no result.</summary>
        NotRoutine,
        /// <summary>"(" arguments ")".</summary>
        Arguments,
        /// <summary>None: a "(" is an error.</summary>
        NoArguments,
        /// <summary>A bare statement: anything but ";" after the name is an error.</summary>
        StatementOnly,
        /// <summary>A function of its own.</summary>
        Special
    };

    /// <summary>The type a call gives.</summary>
    enum class MCAblResult : uint8_t
    {
        None,
        Integer,
        Real,
        Boolean,
        /// <summary>What the special function returns.</summary>
        Special
    };

    /// <summary>The most arguments a standard routine takes.</summary>
    constexpr size_t MaxArguments = 7;

    /// <summary>How a standard routine's call compiles.</summary>
    struct MCAblCallSignature
    {
        MCAblCallForm Form = MCAblCallForm::NotRoutine;
        MCAblResult Result = MCAblResult::None;
        uint8_t Count = 0;
        std::array<MCAblArgument, MaxArguments> Kinds{};
        MCAblType* (*Compile)(MCAblCompiler&) = nullptr;
    };

    /// <summary>A call read by <paramref name="form"/> with <paramref name="kinds"/>, giving <paramref name="result"/>.</summary>
    constexpr auto Call(MCAblCallForm form, MCAblResult result, std::initializer_list<MCAblArgument> kinds = {})
        -> MCAblCallSignature
    {
        MCAblCallSignature signature{form, result, static_cast<uint8_t>(kinds.size())};
        std::ranges::copy(kinds, signature.Kinds.begin());
        return signature;
    }

    /// <summary>A call <paramref name="compile"/> reads (its result dropped when <paramref name="noResult"/>).</summary>
    constexpr auto Special(MCAblType* (*compile)(MCAblCompiler&), bool noResult) -> MCAblCallSignature
    {
        MCAblCallSignature signature{MCAblCallForm::Special, noResult ? MCAblResult::None : MCAblResult::Special};
        signature.Compile = compile;
        return signature;
    }

    /// <summary>Whether <paramref name="typePtr"/> is an array of <paramref name="elementTypePtr"/>.</summary>
    auto IsArrayOf(MCAblType* typePtr, MCAblType* elementTypePtr) -> bool
    {
        return typePtr->Form == MCAblTypeForm::Array && typePtr->Array.ElementTypePtr == elementTypePtr;
    }

    /// <summary>Compiles one argument expression and checks its type against <paramref name="kind"/>.</summary>
    auto Argument(MCAblCompiler& compiler, MCAblArgument kind) -> void
    {
        MCAblType* argType = compiler.Expression();
        bool ok = false;

        switch (kind)
        {
            case MCAblArgument::Integer:
            {
                ok = argType == IntegerTypePtr;
                break;
            }

            case MCAblArgument::Real:
            {
                ok = argType == RealTypePtr;
                break;
            }

            case MCAblArgument::Boolean:
            {
                ok = argType == BooleanTypePtr;
                break;
            }

            case MCAblArgument::Number:
            {
                ok = argType == IntegerTypePtr || argType == RealTypePtr;
                break;
            }

            case MCAblArgument::String:
            {
                ok = IsArrayOf(argType, CharTypePtr);
                break;
            }

            case MCAblArgument::IntegerArray:
            {
                ok = IsArrayOf(argType, IntegerTypePtr);
                break;
            }

            case MCAblArgument::RealArray:
            {
                ok = IsArrayOf(argType, RealTypePtr);
                break;
            }

            case MCAblArgument::Printable:
            {
                ok = argType == IntegerTypePtr || argType == RealTypePtr || argType == CharTypePtr ||
                     IsArrayOf(argType, CharTypePtr);
                break;
            }
        }

        if (!ok)
        {
            compiler.SyntaxError(MCAblSyntaxError::IncompatibleTypes);
        }
    }

    /// <summary>Compiles "(" arguments ")", each checked against its kind, separated by commas.</summary>
    auto Arguments(MCAblCompiler& compiler, std::span<const MCAblArgument> kinds) -> void
    {
        if (compiler.Token() != MCAblToken::LParen)
        {
            compiler.SyntaxError(MCAblSyntaxError::WrongNumberOfParams);
        }

        compiler.NextToken();

        for (size_t i = 0; i < kinds.size(); i++)
        {
            if (i > 0)
            {
                compiler.IfTokenGetElseError(MCAblToken::Comma, MCAblSyntaxError::MissingComma);
            }

            Argument(compiler, kinds[i]);
        }

        compiler.IfTokenGetElseError(MCAblToken::RParen, MCAblSyntaxError::MissingRParen);
    }

    /// <summary><c>return [(value)]</c>: the value must have the routine's type, and a function must return one.</summary>
    auto CompileReturn(MCAblCompiler& compiler) -> MCAblType*
    {
        if (compiler.Token() == MCAblToken::LParen)
        {
            compiler.NextToken();

            if (compiler.Expression() != compiler.CurrentRoutine()->TypePtr)
            {
                compiler.SyntaxError(MCAblSyntaxError::IncompatibleTypes);
            }

            compiler.IfTokenGetElseError(MCAblToken::RParen, MCAblSyntaxError::MissingRParen);
            return nullptr;
        }

        if (compiler.CurrentRoutine()->TypePtr != nullptr)
        {
            compiler.SyntaxError(MCAblSyntaxError::WrongNumberOfParams);
        }

        return nullptr;
    }

    /// <summary><c>setguardradii(real, real)</c>.</summary>
    auto CompileSetGuardRadii(MCAblCompiler& compiler) -> MCAblType*
    {
        if (compiler.Token() != MCAblToken::LParen)
        {
            compiler.SyntaxError(MCAblSyntaxError::WrongNumberOfParams);
        }

        // Original behaviour: the second argument is preceded by getToken(), not a comma check, so any token
        // separates the two radii.
        compiler.NextToken();
        Argument(compiler, MCAblArgument::Real);
        compiler.NextToken();
        Argument(compiler, MCAblArgument::Real);
        compiler.IfTokenGetElseError(MCAblToken::RParen, MCAblSyntaxError::MissingRParen);
        return IntegerTypePtr;
    }

    /// <summary><c>ordermovetocontact(boolean)</c>.</summary>
    auto CompileMoveToContact(MCAblCompiler& compiler) -> MCAblType*
    {
        if (compiler.Token() != MCAblToken::LParen)
        {
            compiler.SyntaxError(MCAblSyntaxError::WrongNumberOfParams);
        }

        compiler.NextToken();
        Argument(compiler, MCAblArgument::Boolean);
        // Original behaviour: a missing ")" reports a missing comma.
        compiler.IfTokenGetElseError(MCAblToken::RParen, MCAblSyntaxError::MissingComma);
        return IntegerTypePtr;
    }

    /// <summary><c>stopmusic</c>.</summary>
    auto CompileStopMusic(MCAblCompiler& compiler) -> MCAblType*
    {
        if (compiler.Token() != MCAblToken::LParen)
        {
            compiler.SyntaxError(MCAblSyntaxError::WrongNumberOfParams);
        }

        // Original behaviour (OB-049): no getToken() after "(", so "stopmusic()" fails on the "(" as a missing ")".
        compiler.IfTokenGetElseError(MCAblToken::RParen, MCAblSyntaxError::MissingRParen);
        return IntegerTypePtr;
    }

    /// <summary><c>getsalvage(integer, integer, integer[], integer[])</c>.</summary>
    auto CompileGetSalvage(MCAblCompiler& compiler) -> MCAblType*
    {
        if (compiler.Token() != MCAblToken::LParen)
        {
            compiler.SyntaxError(MCAblSyntaxError::WrongNumberOfParams);
        }

        compiler.NextToken();
        Argument(compiler, MCAblArgument::Integer);
        compiler.IfTokenGetElseError(MCAblToken::Comma, MCAblSyntaxError::MissingComma);
        Argument(compiler, MCAblArgument::Integer);
        compiler.IfTokenGetElseError(MCAblToken::Comma, MCAblSyntaxError::MissingComma);
        // Original behaviour: the third argument is preceded by getToken(), not a comma check.
        compiler.NextToken();
        Argument(compiler, MCAblArgument::IntegerArray);
        compiler.IfTokenGetElseError(MCAblToken::Comma, MCAblSyntaxError::MissingComma);
        Argument(compiler, MCAblArgument::IntegerArray);
        compiler.IfTokenGetElseError(MCAblToken::RParen, MCAblSyntaxError::MissingRParen);
        return nullptr;
    }

    /// <summary>Each routine's call (the original's standardRoutineCall switch; a key without a case reads nothing).</summary>
    constexpr auto Signatures = []
    {
        std::array<MCAblCallSignature, static_cast<size_t>(MCAblRoutineKey::Count)> table{};
        const std::pair<MCAblRoutineKey, MCAblCallSignature> rows[] = {
            {MCAblRoutineKey::Return, Special(CompileReturn, true)},
            {MCAblRoutineKey::Print,
             Call(MCAblCallForm::Arguments, MCAblResult::None, {MCAblArgument::Printable})}, // StdPrint
            {MCAblRoutineKey::Concat, Call(MCAblCallForm::Arguments, MCAblResult::Integer,
                                           {MCAblArgument::String, MCAblArgument::Printable})}, // StdConcat
            {MCAblRoutineKey::Abs, Call(MCAblCallForm::Arguments, MCAblResult::Real, {MCAblArgument::Real})}, // StdAbs
            {MCAblRoutineKey::Round,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer, {MCAblArgument::Real})}, // StdRound
            {MCAblRoutineKey::Sqrt,
             Call(MCAblCallForm::Arguments, MCAblResult::Real, {MCAblArgument::Number})}, // StdSqrt
            {MCAblRoutineKey::Trunc,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer, {MCAblArgument::Number})}, // StdTrunc
            {MCAblRoutineKey::Random,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer, {MCAblArgument::Integer})}, // StdRandom
            {MCAblRoutineKey::SetMaxLoops,
             Call(MCAblCallForm::Arguments, MCAblResult::None, {MCAblArgument::Integer})}, // StdSetMaxLoops
            {MCAblRoutineKey::Fatal, Call(MCAblCallForm::Arguments, MCAblResult::None,
                                          {MCAblArgument::Integer, MCAblArgument::String})}, // StdFatal
            {MCAblRoutineKey::Assert,
             Call(MCAblCallForm::Arguments, MCAblResult::None,
                  {MCAblArgument::Boolean, MCAblArgument::Integer, MCAblArgument::String})}, // StdAssert
            {MCAblRoutineKey::GetModuleHandle,
             Call(MCAblCallForm::NoArguments, MCAblResult::Integer, {})},                             // StdGetModHandle
            {MCAblRoutineKey::GetMode, Call(MCAblCallForm::NoArguments, MCAblResult::Integer, {})},   // StdGetModHandle
            {MCAblRoutineKey::GetAction, Call(MCAblCallForm::NoArguments, MCAblResult::Integer, {})}, // StdGetModHandle
            {MCAblRoutineKey::GetPhase, Call(MCAblCallForm::NoArguments, MCAblResult::Integer, {})},  // StdGetModHandle
            {MCAblRoutineKey::GetModuleName, Call(MCAblCallForm::NoArguments, MCAblResult::None, {})}, // StdGetModName
            {MCAblRoutineKey::SetModuleName,
             Call(MCAblCallForm::Arguments, MCAblResult::None, {MCAblArgument::String})},            // StdSetModName
            {MCAblRoutineKey::GetId, Call(MCAblCallForm::NoArguments, MCAblResult::Integer, {})},    // HbGetId
            {MCAblRoutineKey::GetTime, Call(MCAblCallForm::NoArguments, MCAblResult::Real, {})},     // HbGetTime
            {MCAblRoutineKey::GetTimeLeft, Call(MCAblCallForm::NoArguments, MCAblResult::Real, {})}, // HbGetTimeLeft
            {MCAblRoutineKey::GetWarriorStatus,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer, {MCAblArgument::Integer})}, // HbGetWarriorStatus
            {MCAblRoutineKey::SelectWarrior,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer, {MCAblArgument::Integer})}, // HbSelectWarrior
            {MCAblRoutineKey::SelectObject,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer, {MCAblArgument::Integer})}, // HbSelectObject
            {MCAblRoutineKey::GetContacts,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer,
                  {MCAblArgument::IntegerArray, MCAblArgument::Integer, MCAblArgument::Integer})}, // HbGetContacts
            {MCAblRoutineKey::GetEnemyCount,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer, {MCAblArgument::Integer})}, // HbGetEnemyCount
            {MCAblRoutineKey::SelectContact, Call(MCAblCallForm::Arguments, MCAblResult::Integer,
                                                  {MCAblArgument::Integer, MCAblArgument::Integer})}, // HbSelectContact
            {MCAblRoutineKey::GetContactId,
             Call(MCAblCallForm::NoArguments, MCAblResult::Integer, {})}, // HbGetContactId
            {MCAblRoutineKey::IsContact,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer,
                  {MCAblArgument::Integer, MCAblArgument::Integer, MCAblArgument::Boolean})}, // HbIsContact
            {MCAblRoutineKey::GetContactStatus,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer, {MCAblArgument::Integer})}, // HbGetContactStatus
            {MCAblRoutineKey::GetContactRelativePosition,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer,
                  {MCAblArgument::Real, MCAblArgument::Real})}, // HbGetContactRelativePosition
            {MCAblRoutineKey::SetGuardObjective,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer, {MCAblArgument::Integer})}, // HbSetGuardObjective
            {MCAblRoutineKey::SetGuardPoint,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer, {MCAblArgument::RealArray})}, // HbSetGuardPoint
            {MCAblRoutineKey::SetGuardRadii, Special(CompileSetGuardRadii, false)},
            {MCAblRoutineKey::GetGuardObjective,
             Call(MCAblCallForm::NoArguments, MCAblResult::Integer, {})}, // HbGetGuardObjective
            {MCAblRoutineKey::GetGuardPoint,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer, {MCAblArgument::RealArray})}, // HbGetGuardPoint
            {MCAblRoutineKey::GetGuardRadii, Call(MCAblCallForm::Arguments, MCAblResult::Integer,
                                                  {MCAblArgument::Real, MCAblArgument::Real})}, // HbGetGuardRadii
            {MCAblRoutineKey::GetGuardDistanceTo,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer, {MCAblArgument::Integer})}, // HbGetGuardDistanceTo
            {MCAblRoutineKey::GetTarget,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer, {MCAblArgument::Integer})}, // HbGetTarget
            {MCAblRoutineKey::SetTarget, Call(MCAblCallForm::Arguments, MCAblResult::None,
                                              {MCAblArgument::Integer, MCAblArgument::Integer})}, // HbSetTarget
            {MCAblRoutineKey::GetWeaponsReady,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer,
                  {MCAblArgument::IntegerArray, MCAblArgument::Integer})}, // HbGetWeapons
            {MCAblRoutineKey::GetWeaponsLocked,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer,
                  {MCAblArgument::IntegerArray, MCAblArgument::Integer})}, // HbGetWeapons
            {MCAblRoutineKey::GetWeaponsInRange,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer,
                  {MCAblArgument::IntegerArray, MCAblArgument::Integer})}, // HbGetWeapons
            {MCAblRoutineKey::GetWeaponShots,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer, {MCAblArgument::Integer})}, // HbGetWeaponShots
            {MCAblRoutineKey::GetWeaponRanges,
             Call(MCAblCallForm::Arguments, MCAblResult::None,
                  {MCAblArgument::Integer, MCAblArgument::RealArray})}, // HbGetWeaponRanges
            {MCAblRoutineKey::GetObjectPosition,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer,
                  {MCAblArgument::Integer, MCAblArgument::RealArray})}, // HbGetObjectPosition
            {MCAblRoutineKey::GetIntegerMemory,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer, {MCAblArgument::Integer})}, // HbGetMemoryInteger
            {MCAblRoutineKey::GetRealMemory,
             Call(MCAblCallForm::Arguments, MCAblResult::Real, {MCAblArgument::Integer})}, // HbGetMemoryReal
            {MCAblRoutineKey::GetAlarmTriggers,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer, {MCAblArgument::IntegerArray})}, // HbGetAlarmTriggers
            {MCAblRoutineKey::GetChallenger,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer, {MCAblArgument::Integer})}, // HbGetChallenger
            {MCAblRoutineKey::GetFireRanges,
             Call(MCAblCallForm::Arguments, MCAblResult::None, {MCAblArgument::RealArray})}, // HbGetFireRanges
            {MCAblRoutineKey::GetAttackers, Call(MCAblCallForm::Arguments, MCAblResult::Integer,
                                                 {MCAblArgument::IntegerArray, MCAblArgument::Real})}, // HbGetAttackers
            {MCAblRoutineKey::GetAttackerInfo,
             Call(MCAblCallForm::Arguments, MCAblResult::Real, {MCAblArgument::Integer})}, // HbGetAttackerInfo
            {MCAblRoutineKey::SetChallenger, Call(MCAblCallForm::Arguments, MCAblResult::Integer,
                                                  {MCAblArgument::Integer, MCAblArgument::Integer})}, // HbSetChallenger
            {MCAblRoutineKey::GetTimeWithoutOrders,
             Call(MCAblCallForm::NoArguments, MCAblResult::Real, {})}, // HbGetTimeWithoutOrders
            {MCAblRoutineKey::SetRadio, Call(MCAblCallForm::Arguments, MCAblResult::Integer,
                                             {MCAblArgument::Integer, MCAblArgument::Boolean})}, // HbSetRadio
            {MCAblRoutineKey::SetMode,
             Call(MCAblCallForm::Arguments, MCAblResult::None, {MCAblArgument::Integer})}, // HbSetMode
            {MCAblRoutineKey::SetAction,
             Call(MCAblCallForm::Arguments, MCAblResult::None, {MCAblArgument::Integer})}, // HbSetMode
            {MCAblRoutineKey::SetPhase,
             Call(MCAblCallForm::Arguments, MCAblResult::None, {MCAblArgument::Integer})}, // HbSetMode
            {MCAblRoutineKey::SetUpdateTime,
             Call(MCAblCallForm::Arguments, MCAblResult::None, {MCAblArgument::Real})}, // HbSetUpdateTime
            {MCAblRoutineKey::SetMoveGoal, Call(MCAblCallForm::Arguments, MCAblResult::None,
                                                {MCAblArgument::Integer, MCAblArgument::RealArray})}, // HbSetMoveGoal
            {MCAblRoutineKey::SetIntegerMemory,
             Call(MCAblCallForm::Arguments, MCAblResult::None,
                  {MCAblArgument::Integer, MCAblArgument::Integer})}, // HbSetMemoryInteger
            {MCAblRoutineKey::SetRealMemory, Call(MCAblCallForm::Arguments, MCAblResult::None,
                                                  {MCAblArgument::Integer, MCAblArgument::Real})}, // HbSetMemoryReal
            {MCAblRoutineKey::StartFieldScan,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer, {MCAblArgument::Integer})}, // HbStartFieldScan
            {MCAblRoutineKey::StartEnemyScan,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer, {MCAblArgument::Integer})}, // HbStartContactScan
            {MCAblRoutineKey::StartFriendlyScan,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer, {MCAblArgument::Integer})}, // HbStartContactScan
            {MCAblRoutineKey::StartMovePath,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer,
                  {MCAblArgument::Integer, MCAblArgument::Integer, MCAblArgument::Integer})}, // HbStartMovePath
            {MCAblRoutineKey::StartVehicleScan,
             Call(MCAblCallForm::StatementOnly, MCAblResult::Integer, {})}, // HbStartVehicleScan
            {MCAblRoutineKey::HasMoveGoal, Call(MCAblCallForm::NoArguments, MCAblResult::Boolean, {})}, // HbHasMoveGoal
            {MCAblRoutineKey::HasMovePath, Call(MCAblCallForm::NoArguments, MCAblResult::Boolean, {})}, // HbHasMovePath
            {MCAblRoutineKey::SortWeapons,
             Call(MCAblCallForm::Arguments, MCAblResult::None,
                  {MCAblArgument::IntegerArray, MCAblArgument::Integer, MCAblArgument::Integer})}, // HbSortWeapons
            {MCAblRoutineKey::TimeToImpact, Call(MCAblCallForm::Arguments, MCAblResult::Real,
                                                 {MCAblArgument::Integer, MCAblArgument::Integer})}, // HbTimeToImpact
            {MCAblRoutineKey::FireWeapon,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer, {MCAblArgument::Integer})}, // HbFireWeapon
            {MCAblRoutineKey::GetVisualRange,
             Call(MCAblCallForm::Arguments, MCAblResult::Real, {MCAblArgument::Integer})}, // HbGetVisualRange
            {MCAblRoutineKey::GetUnitMates,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer,
                  {MCAblArgument::Integer, MCAblArgument::IntegerArray})}, // HbGetUnitMates
            {MCAblRoutineKey::GetTacOrder,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer,
                  {MCAblArgument::Integer, MCAblArgument::Real, MCAblArgument::IntegerArray})}, // HbGetTacOrder
            {MCAblRoutineKey::GetLastTacOrder,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer,
                  {MCAblArgument::Integer, MCAblArgument::Real, MCAblArgument::IntegerArray})}, // HbGetLastTacOrder
            {MCAblRoutineKey::SetOrderMode,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer, {MCAblArgument::Integer})}, // HbSetOrderMode
            {MCAblRoutineKey::OrderWait, Call(MCAblCallForm::Arguments, MCAblResult::Integer,
                                              {MCAblArgument::Real, MCAblArgument::Boolean})}, // HbWait
            {MCAblRoutineKey::OrderMoveTo, Call(MCAblCallForm::Arguments, MCAblResult::Integer,
                                                {MCAblArgument::RealArray, MCAblArgument::Boolean})}, // HbMoveToPoint
            {MCAblRoutineKey::OrderMoveToObject,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer,
                  {MCAblArgument::Integer, MCAblArgument::Boolean})}, // HbMoveToObject
            {MCAblRoutineKey::OrderMoveToContact, Special(CompileMoveToContact, false)},
            {MCAblRoutineKey::OrderPowerUp,
             Call(MCAblCallForm::NoArguments, MCAblResult::Integer, {})}, // HbOrderPowerUp
            {MCAblRoutineKey::OrderPowerDown,
             Call(MCAblCallForm::NoArguments, MCAblResult::Integer, {})}, // HbOrderPowerDown
            {MCAblRoutineKey::OrderAttackObject,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer,
                  {MCAblArgument::Integer, MCAblArgument::Integer, MCAblArgument::Integer, MCAblArgument::Integer,
                   MCAblArgument::Boolean})}, // HbOrderAttackObject
            {MCAblRoutineKey::OrderAttackContact,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer,
                  {MCAblArgument::Integer, MCAblArgument::Integer, MCAblArgument::Integer,
                   MCAblArgument::Boolean})}, // HbOrderAttackContact
            {MCAblRoutineKey::AttackThreat,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer,
                  {MCAblArgument::Integer, MCAblArgument::Integer, MCAblArgument::Integer, MCAblArgument::Boolean,
                   MCAblArgument::Integer, MCAblArgument::Integer})}, // HbAttackThreat
            {MCAblRoutineKey::OrderWithdraw,
             Call(MCAblCallForm::StatementOnly, MCAblResult::Integer, {})},                            // HbObjWithdraw
            {MCAblRoutineKey::OpenFire, Call(MCAblCallForm::StatementOnly, MCAblResult::Integer, {})}, // HbOpenFire
            {MCAblRoutineKey::DamageObject,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer,
                  {MCAblArgument::Integer, MCAblArgument::Integer, MCAblArgument::Integer, MCAblArgument::Real,
                   MCAblArgument::Integer, MCAblArgument::Real, MCAblArgument::Real})}, // HbDamageObject
            {MCAblRoutineKey::SetAttackRadius,
             Call(MCAblCallForm::Arguments, MCAblResult::Real, {MCAblArgument::Real})}, // HbSetAttackRadius
            {MCAblRoutineKey::OrderTest,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer, {MCAblArgument::Integer})}, // HbOrderTest
            {MCAblRoutineKey::PlaySmacker,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer, {MCAblArgument::Integer})}, // HbPlaySmacker
            {MCAblRoutineKey::FileExists,
             Call(MCAblCallForm::Arguments, MCAblResult::Boolean, {MCAblArgument::String})}, // HbFileExists
            {MCAblRoutineKey::ObjectChangeSides,
             Call(MCAblCallForm::Arguments, MCAblResult::None,
                  {MCAblArgument::Integer, MCAblArgument::Integer})}, // HbObjectChangeSides
            {MCAblRoutineKey::DistanceToObject,
             Call(MCAblCallForm::Arguments, MCAblResult::Real,
                  {MCAblArgument::Integer, MCAblArgument::Integer})}, // HbDistanceToObject
            {MCAblRoutineKey::DistanceToPosition,
             Call(MCAblCallForm::Arguments, MCAblResult::Real,
                  {MCAblArgument::Integer, MCAblArgument::RealArray})}, // HbDistanceToPosition
            {MCAblRoutineKey::ObjectSuicide,
             Call(MCAblCallForm::Arguments, MCAblResult::None, {MCAblArgument::Integer})}, // HbObjectSuicide
            {MCAblRoutineKey::ObjectCreate,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer, {MCAblArgument::Integer})}, // HbObjectCreate
            {MCAblRoutineKey::ObjectExists,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer, {MCAblArgument::Integer})}, // HbObjectExists
            {MCAblRoutineKey::ObjectStatus,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer, {MCAblArgument::Integer})}, // HbObjectStatus
            {MCAblRoutineKey::ObjectVisible, Call(MCAblCallForm::Arguments, MCAblResult::Integer,
                                                  {MCAblArgument::Integer, MCAblArgument::Integer})}, // HbObjectVisible
            {MCAblRoutineKey::ObjectClass,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer, {MCAblArgument::Integer})}, // HbObjectClass
            {MCAblRoutineKey::ObjectSide,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer, {MCAblArgument::Integer})}, // HbObjectSide
            {MCAblRoutineKey::ObjectCommander,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer, {MCAblArgument::Integer})}, // HbObjectCommander
            {MCAblRoutineKey::SetTimer, Call(MCAblCallForm::Arguments, MCAblResult::Integer,
                                             {MCAblArgument::Integer, MCAblArgument::Number})}, // HbSetTimer
            {MCAblRoutineKey::CheckTimer,
             Call(MCAblCallForm::Arguments, MCAblResult::Real, {MCAblArgument::Integer})}, // HbChkTimer
            {MCAblRoutineKey::EndTimer,
             Call(MCAblCallForm::Arguments, MCAblResult::None, {MCAblArgument::Integer})}, // HbEndTimer
            {MCAblRoutineKey::SetObjectiveTimer,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer,
                  {MCAblArgument::Integer, MCAblArgument::Number})}, // HbSetObjectiveTimer
            {MCAblRoutineKey::CheckObjectiveTimer,
             Call(MCAblCallForm::Arguments, MCAblResult::Real, {MCAblArgument::Integer})}, // HbCheckObjectiveTimer
            {MCAblRoutineKey::SetObjectiveStatus,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer,
                  {MCAblArgument::Integer, MCAblArgument::Integer})}, // HbSetObjectiveStatus
            {MCAblRoutineKey::CheckObjectiveStatus,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer, {MCAblArgument::Integer})}, // HbCheckObjectiveStatus
            {MCAblRoutineKey::SetObjectiveType,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer,
                  {MCAblArgument::Integer, MCAblArgument::Integer})}, // HbSetObjectiveType
            {MCAblRoutineKey::CheckObjectiveType,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer, {MCAblArgument::Integer})}, // HbCheckObjectiveType
            {MCAblRoutineKey::PlayDigitalMusic,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer, {MCAblArgument::Integer})}, // HbPlayDigitalMusic
            {MCAblRoutineKey::StopMusic, Special(CompileStopMusic, false)},
            {MCAblRoutineKey::PlaySoundEffect,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer, {MCAblArgument::Integer})}, // HbPlaySoundEffect
            {MCAblRoutineKey::PlayVideo,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer, {MCAblArgument::Integer})}, // HbPlayVideo
            {MCAblRoutineKey::PlaySpeech, Call(MCAblCallForm::Arguments, MCAblResult::Integer,
                                               {MCAblArgument::Integer, MCAblArgument::Integer})}, // HbPlaySpeech
            {MCAblRoutineKey::PlayBetty,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer, {MCAblArgument::Integer})}, // HbPlayBetty
            {MCAblRoutineKey::SetObjectActive,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer,
                  {MCAblArgument::Integer, MCAblArgument::Boolean})}, // HbSetObjActive
            {MCAblRoutineKey::ObjectInWithdrawal,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer, {MCAblArgument::Integer})}, // HbObjInWithdraw
            {MCAblRoutineKey::ObjectTypeId,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer, {MCAblArgument::Integer})}, // HbObjTypeId
            {MCAblRoutineKey::GetTerrainObjectPartId,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer,
                  {MCAblArgument::Integer, MCAblArgument::Integer})}, // HbTerrainObjectId
            {MCAblRoutineKey::GetVehiclePartId, Call(MCAblCallForm::Arguments, MCAblResult::Integer,
                                                     {MCAblArgument::Integer, MCAblArgument::Integer})}, // HbVehicleId
            {MCAblRoutineKey::GetWeaponAmmo, Call(MCAblCallForm::Arguments, MCAblResult::Integer,
                                                  {MCAblArgument::Integer, MCAblArgument::Integer})}, // HbGetWeaponAmmo
            {MCAblRoutineKey::ObjectStatusCount,
             Call(MCAblCallForm::Arguments, MCAblResult::None,
                  {MCAblArgument::Integer, MCAblArgument::IntegerArray})}, // HbObjectStatusCount
            {MCAblRoutineKey::InArea, Call(MCAblCallForm::Arguments, MCAblResult::Boolean,
                                           {MCAblArgument::Integer, MCAblArgument::RealArray, MCAblArgument::Real,
                                            MCAblArgument::Integer})}, // HbInArea
            {MCAblRoutineKey::GetRelativePositionToPoint,
             Call(MCAblCallForm::Arguments, MCAblResult::None,
                  {MCAblArgument::RealArray, MCAblArgument::Real, MCAblArgument::Real, MCAblArgument::Integer,
                   MCAblArgument::RealArray})}, // HbGetRelPosPoint
            {MCAblRoutineKey::GetRelativePositionToObject,
             Call(MCAblCallForm::Arguments, MCAblResult::None,
                  {MCAblArgument::Integer, MCAblArgument::Real, MCAblArgument::Real, MCAblArgument::Integer,
                   MCAblArgument::RealArray})}, // HbGetRelPosObject
            {MCAblRoutineKey::GetSensorsWorking,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer, {MCAblArgument::Integer})}, // HbGetSensors
            {MCAblRoutineKey::GetCurrentBRValue,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer, {MCAblArgument::Integer})}, // HbGetBRValue
            {MCAblRoutineKey::SetCurrentBRValue,
             Call(MCAblCallForm::Arguments, MCAblResult::None, {MCAblArgument::Integer})}, // HbGetBRValue
            {MCAblRoutineKey::GetArmorPts,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer, {MCAblArgument::Integer})}, // HbGetArmor
            {MCAblRoutineKey::GetMaxArmor,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer, {MCAblArgument::Integer})}, // HbGetMaxArmor
            {MCAblRoutineKey::GetPilotId,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer, {MCAblArgument::Integer})}, // HbGetPilotId
            {MCAblRoutineKey::GetPilotWounds,
             Call(MCAblCallForm::Arguments, MCAblResult::Real, {MCAblArgument::Integer})}, // HbGetPilotWounds
            {MCAblRoutineKey::SetPilotWounds,
             Call(MCAblCallForm::Arguments, MCAblResult::None,
                  {MCAblArgument::Integer, MCAblArgument::Integer})}, // HbSetPilotWounds
            {MCAblRoutineKey::GetObjectActive,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer, {MCAblArgument::Integer})}, // HbGetObjActive
            {MCAblRoutineKey::GetObjectDmgPts,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer, {MCAblArgument::Integer})}, // HbGetObjDmgPts
            {MCAblRoutineKey::GetObjectMaxDmg,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer, {MCAblArgument::Integer})}, // HbGetObjMaxDmg
            {MCAblRoutineKey::GetObjectDamage,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer, {MCAblArgument::Integer})}, // HbGetObjDamage
            {MCAblRoutineKey::SetObjectDamage,
             Call(MCAblCallForm::Arguments, MCAblResult::None,
                  {MCAblArgument::Integer, MCAblArgument::Integer})}, // HbSetObjDamage
            {MCAblRoutineKey::GetGlobalValue,
             Call(MCAblCallForm::Arguments, MCAblResult::Real, {MCAblArgument::Integer})}, // HbGetGlobalValue
            {MCAblRoutineKey::SetGlobalValue,
             Call(MCAblCallForm::Arguments, MCAblResult::None,
                  {MCAblArgument::Integer, MCAblArgument::Number})}, // HbSetGlobalValue
            {MCAblRoutineKey::SetObjectivePos,
             Call(MCAblCallForm::Arguments, MCAblResult::None,
                  {MCAblArgument::Integer, MCAblArgument::Number, MCAblArgument::Number,
                   MCAblArgument::Number})}, // HbSetObjectivePos
            {MCAblRoutineKey::SetPotentialContact,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer,
                  {MCAblArgument::Integer, MCAblArgument::Integer})}, // HbSetPotentialContact
            {MCAblRoutineKey::SetSensorRange, Call(MCAblCallForm::Arguments, MCAblResult::Integer,
                                                   {MCAblArgument::Integer, MCAblArgument::Real})}, // HbSetSensorRange
            {MCAblRoutineKey::SetTonnage, Call(MCAblCallForm::Arguments, MCAblResult::None,
                                               {MCAblArgument::Integer, MCAblArgument::Real})}, // HbSetTonnage
            {MCAblRoutineKey::PlayWaveFile, Call(MCAblCallForm::Arguments, MCAblResult::None,
                                                 {MCAblArgument::Integer, MCAblArgument::Real})}, // HbPlayWave
            {MCAblRoutineKey::SetExplosionDamage, Call(MCAblCallForm::Arguments, MCAblResult::None,
                                                       {MCAblArgument::Integer, MCAblArgument::Real})}, // HbSetExplDmg
            {MCAblRoutineKey::SetExplosionRadius, Call(MCAblCallForm::Arguments, MCAblResult::None,
                                                       {MCAblArgument::Integer, MCAblArgument::Real})}, // HbSetExplRad
            {MCAblRoutineKey::GetSalvage, Special(CompileGetSalvage, true)},
            {MCAblRoutineKey::SetSalvage,
             Call(MCAblCallForm::Arguments, MCAblResult::Boolean,
                  {MCAblArgument::Integer, MCAblArgument::Integer, MCAblArgument::Integer})}, // HbSetSalvage
            {MCAblRoutineKey::SetSalvageStatus,
             Call(MCAblCallForm::Arguments, MCAblResult::Boolean,
                  {MCAblArgument::Integer, MCAblArgument::Boolean})}, // HbSetSalvageStatus
            {MCAblRoutineKey::SetAnimation,
             Call(MCAblCallForm::Arguments, MCAblResult::None,
                  {MCAblArgument::Integer, MCAblArgument::Integer, MCAblArgument::Integer})}, // HbSetAnimation
            {MCAblRoutineKey::SetRevealed,
             Call(MCAblCallForm::Arguments, MCAblResult::None,
                  {MCAblArgument::Integer, MCAblArgument::Number, MCAblArgument::RealArray})}, // HbSetRevealed
            {MCAblRoutineKey::OrderRefit, Call(MCAblCallForm::Arguments, MCAblResult::None,
                                               {MCAblArgument::Integer, MCAblArgument::Integer})}, // HbRefit
            {MCAblRoutineKey::OrderCapture, Call(MCAblCallForm::Arguments, MCAblResult::None,
                                                 {MCAblArgument::Integer, MCAblArgument::Integer})}, // HbCaptureObject
            {MCAblRoutineKey::SetCaptured,
             Call(MCAblCallForm::Arguments, MCAblResult::None, {MCAblArgument::Integer})}, // HbSetCaptured
            {MCAblRoutineKey::SetCaptureable,
             Call(MCAblCallForm::Arguments, MCAblResult::None,
                  {MCAblArgument::Integer, MCAblArgument::Boolean})}, // HbSetCaptureable
            {MCAblRoutineKey::IsCaptured,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer, {MCAblArgument::Integer})}, // HbIsCaptured
            {MCAblRoutineKey::IsCapturable,
             Call(MCAblCallForm::Arguments, MCAblResult::Boolean, {MCAblArgument::Integer})}, // HbIsCapturable
            {MCAblRoutineKey::WasEverCapturable,
             Call(MCAblCallForm::Arguments, MCAblResult::Boolean, {MCAblArgument::Integer})}, // HbWasEverCapturable
            {MCAblRoutineKey::SetBuildingName,
             Call(MCAblCallForm::Arguments, MCAblResult::None,
                  {MCAblArgument::Integer, MCAblArgument::Integer})}, // HbSetBuildingName
            {MCAblRoutineKey::CallStrike,
             Call(MCAblCallForm::Arguments, MCAblResult::None,
                  {MCAblArgument::Integer, MCAblArgument::Integer, MCAblArgument::Real, MCAblArgument::Real,
                   MCAblArgument::Real, MCAblArgument::Boolean})}, // HbCallStrike
            {MCAblRoutineKey::OrderLoadElementals,
             Call(MCAblCallForm::Arguments, MCAblResult::None, {MCAblArgument::Integer})}, // HbLoadElementals
            {MCAblRoutineKey::OrderDeployElementals,
             Call(MCAblCallForm::Arguments, MCAblResult::None, {MCAblArgument::Integer})}, // HbDeployElementals
            {MCAblRoutineKey::AddPrisoner, Call(MCAblCallForm::Arguments, MCAblResult::None,
                                                {MCAblArgument::Integer, MCAblArgument::Integer})}, // HbAddPrisoner
            {MCAblRoutineKey::SetTrainSpeed, Call(MCAblCallForm::Arguments, MCAblResult::None,
                                                  {MCAblArgument::Integer, MCAblArgument::Real})}, // HbSetTrainSpeed
            {MCAblRoutineKey::LockGateOpen,
             Call(MCAblCallForm::Arguments, MCAblResult::None, {MCAblArgument::Integer})}, // HbLockGateOpen
            {MCAblRoutineKey::LockGateClosed,
             Call(MCAblCallForm::Arguments, MCAblResult::None, {MCAblArgument::Integer})}, // HbLockGateClosed
            {MCAblRoutineKey::ReleaseGateLock,
             Call(MCAblCallForm::Arguments, MCAblResult::None, {MCAblArgument::Integer})}, // HbReleaseGateLock
            {MCAblRoutineKey::IsGateOpen,
             Call(MCAblCallForm::Arguments, MCAblResult::Boolean, {MCAblArgument::Integer})}, // HbIsGateOpen
            {MCAblRoutineKey::CallStrikeEx,
             Call(MCAblCallForm::Arguments, MCAblResult::None,
                  {MCAblArgument::Integer, MCAblArgument::Integer, MCAblArgument::Real, MCAblArgument::Real,
                   MCAblArgument::Real, MCAblArgument::Boolean, MCAblArgument::Real})}, // HbCallStrikeEx
            {MCAblRoutineKey::GetUnitStatus,
             Call(MCAblCallForm::Arguments, MCAblResult::Real, {MCAblArgument::Integer})}, // HbGetUnitStatus
            {MCAblRoutineKey::Repair, Call(MCAblCallForm::Arguments, MCAblResult::None,
                                           {MCAblArgument::Integer, MCAblArgument::Real})}, // HbRepair
            {MCAblRoutineKey::GetFixed,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer,
                  {MCAblArgument::Integer, MCAblArgument::Integer, MCAblArgument::Integer})}, // HbGetFixed
            {MCAblRoutineKey::GetRepairState,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer, {MCAblArgument::Integer})}, // HbGetRepairState
            {MCAblRoutineKey::IsTeamTargeting,
             Call(MCAblCallForm::Arguments, MCAblResult::Boolean,
                  {MCAblArgument::Integer, MCAblArgument::Integer, MCAblArgument::Integer})}, // HbIsTeamTargeting
            {MCAblRoutineKey::SendMessage, Call(MCAblCallForm::Arguments, MCAblResult::None,
                                                {MCAblArgument::Integer, MCAblArgument::Integer})}, // HbSendMessage
            {MCAblRoutineKey::GetMessage,
             Call(MCAblCallForm::Arguments, MCAblResult::Integer, {MCAblArgument::Integer})},           // HbGetMessage
            {MCAblRoutineKey::GetHomeTeam, Call(MCAblCallForm::NoArguments, MCAblResult::Integer, {})}, // HbGetHomeTeam
            {MCAblRoutineKey::SetStrikes,
             Call(MCAblCallForm::Arguments, MCAblResult::None,
                  {MCAblArgument::Integer, MCAblArgument::Integer, MCAblArgument::Integer})}, // HbSetStrikes
            {MCAblRoutineKey::GetStrikes, Call(MCAblCallForm::Arguments, MCAblResult::Integer,
                                               {MCAblArgument::Integer, MCAblArgument::Integer})},   // HbGetStrikes
            {MCAblRoutineKey::IsServer, Call(MCAblCallForm::NoArguments, MCAblResult::Boolean, {})}, // HbIsServer
            {MCAblRoutineKey::AddStrikes,
             Call(MCAblCallForm::Arguments, MCAblResult::None,
                  {MCAblArgument::Integer, MCAblArgument::Integer, MCAblArgument::Integer})}, // HbAddStrikes
        };

        for (const auto& [key, signature] : rows)
        {
            table[static_cast<size_t>(key)] = signature;
        }

        return table;
    }();
}

auto CompileStandardRoutineCall(MCAblCompiler& compiler, MCAblRoutineKey key) -> MCAblType*
{
    const MCAblCallSignature& signature = Signatures[static_cast<size_t>(key)];

    switch (signature.Form)
    {
        case MCAblCallForm::NotRoutine:
        {
            return nullptr;
        }

        case MCAblCallForm::Arguments:
        {
            Arguments(compiler, std::span(signature.Kinds).first(signature.Count));
            break;
        }

        case MCAblCallForm::NoArguments:
        {
            if (compiler.Token() == MCAblToken::LParen)
            {
                compiler.SyntaxError(MCAblSyntaxError::WrongNumberOfParams);
            }
            break;
        }

        case MCAblCallForm::StatementOnly:
        {
            if (compiler.Token() != MCAblToken::Semicolon)
            {
                compiler.SyntaxError(MCAblSyntaxError::WrongNumberOfParams);
            }
            break;
        }

        case MCAblCallForm::Special:
        {
            MCAblType* result = signature.Compile(compiler);
            return signature.Result == MCAblResult::Special ? result : nullptr;
        }
    }

    switch (signature.Result)
    {
        case MCAblResult::Integer:
            return IntegerTypePtr;
        case MCAblResult::Real:
            return RealTypePtr;
        case MCAblResult::Boolean:
            return BooleanTypePtr;
        default:
            return nullptr;
    }
}
