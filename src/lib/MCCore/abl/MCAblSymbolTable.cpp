#include "stdafx.h"
#include "abl/MCAblSymbolTable.h"
#include "abl/MCAblToken.h"
#include "lib/MCFatal.h"
#include "main/MCGameContext.h"

namespace
{
    /// <summary>A standard routine: its ABL name and key.</summary>
    struct MCStandardRoutine
    {
        std::string_view Name;
        MCAblRoutineKey Key{};
    };

    /// <summary>
    /// The standard routines in initSymTable's order (which shapes the global tree). The original also passed
    /// whether each was a tactical order, and stored it nowhere.
    /// </summary>
    constexpr MCStandardRoutine StandardRoutines[] = {
        {"return", MCAblRoutineKey::Return},
        {"print", MCAblRoutineKey::Print},
        {"concat", MCAblRoutineKey::Concat},
        {"abs", MCAblRoutineKey::Abs},
        {"random", MCAblRoutineKey::Random},
        {"round", MCAblRoutineKey::Round},
        {"sqrt", MCAblRoutineKey::Sqrt},
        {"trunc", MCAblRoutineKey::Trunc},
        {"getmodulehandle", MCAblRoutineKey::GetModuleHandle},
        {"getmodulename", MCAblRoutineKey::GetModuleName},
        {"setmodulename", MCAblRoutineKey::SetModuleName},
        {"setmaxloops", MCAblRoutineKey::SetMaxLoops},
        {"fatal", MCAblRoutineKey::Fatal},
        {"assert", MCAblRoutineKey::Assert},
        {"getmode", MCAblRoutineKey::GetMode},
        {"getaction", MCAblRoutineKey::GetAction},
        {"getphase", MCAblRoutineKey::GetPhase},
        {"getid", MCAblRoutineKey::GetId},
        {"gettime", MCAblRoutineKey::GetTime},
        {"gettimeleft", MCAblRoutineKey::GetTimeLeft},
        {"selectobject", MCAblRoutineKey::SelectObject},
        {"selectunit", MCAblRoutineKey::SelectUnit},
        {"selectwarrior", MCAblRoutineKey::SelectWarrior},
        {"getwarriorstatus", MCAblRoutineKey::GetWarriorStatus},
        {"getcontacts", MCAblRoutineKey::GetContacts},
        {"getenemycount", MCAblRoutineKey::GetEnemyCount},
        {"selectcontact", MCAblRoutineKey::SelectContact},
        {"getcontactid", MCAblRoutineKey::GetContactId},
        {"iscontact", MCAblRoutineKey::IsContact},
        {"getcontactstatus", MCAblRoutineKey::GetContactStatus},
        {"getcontactrelativeposition", MCAblRoutineKey::GetContactRelativePosition},
        {"setguardobjective", MCAblRoutineKey::SetGuardObjective},
        {"setguardpoint", MCAblRoutineKey::SetGuardPoint},
        {"setguardradii", MCAblRoutineKey::SetGuardRadii},
        {"getguardobjective", MCAblRoutineKey::GetGuardObjective},
        {"getguardpoint", MCAblRoutineKey::GetGuardPoint},
        {"getguardradii", MCAblRoutineKey::GetGuardRadii},
        {"getguarddistanceto", MCAblRoutineKey::GetGuardDistanceTo},
        {"gettarget", MCAblRoutineKey::GetTarget},
        {"settarget", MCAblRoutineKey::SetTarget},
        {"getweaponsready", MCAblRoutineKey::GetWeaponsReady},
        {"getweaponslocked", MCAblRoutineKey::GetWeaponsLocked},
        {"getweaponsinrange", MCAblRoutineKey::GetWeaponsInRange},
        {"getweaponshots", MCAblRoutineKey::GetWeaponShots},
        {"getweaponranges", MCAblRoutineKey::GetWeaponRanges},
        {"getobjectposition", MCAblRoutineKey::GetObjectPosition},
        {"getintegermemory", MCAblRoutineKey::GetIntegerMemory},
        {"getrealmemory", MCAblRoutineKey::GetRealMemory},
        {"getalarmtriggers", MCAblRoutineKey::GetAlarmTriggers},
        {"getchallenger", MCAblRoutineKey::GetChallenger},
        {"gettimewithoutorders", MCAblRoutineKey::GetTimeWithoutOrders},
        {"getfireranges", MCAblRoutineKey::GetFireRanges},
        {"getattackers", MCAblRoutineKey::GetAttackers},
        {"getattackerinfo", MCAblRoutineKey::GetAttackerInfo},
        {"setchallenger", MCAblRoutineKey::SetChallenger},
        {"setmode", MCAblRoutineKey::SetMode},
        {"setaction", MCAblRoutineKey::SetAction},
        {"setphase", MCAblRoutineKey::SetPhase},
        {"setmovegoal", MCAblRoutineKey::SetMoveGoal},
        {"setupdatetime", MCAblRoutineKey::SetUpdateTime},
        {"setintegermemory", MCAblRoutineKey::SetIntegerMemory},
        {"setrealmemory", MCAblRoutineKey::SetRealMemory},
        {"startfieldscan", MCAblRoutineKey::StartFieldScan},
        {"startvehiclescan", MCAblRoutineKey::StartVehicleScan},
        {"startenemyscan", MCAblRoutineKey::StartEnemyScan},
        {"startfriendlyscan", MCAblRoutineKey::StartFriendlyScan},
        {"startmovepath", MCAblRoutineKey::StartMovePath},
        {"hasmovegoal", MCAblRoutineKey::HasMoveGoal},
        {"hasmovepath", MCAblRoutineKey::HasMovePath},
        {"sortweapons", MCAblRoutineKey::SortWeapons},
        {"timetoimpact", MCAblRoutineKey::TimeToImpact},
        {"fireweapon", MCAblRoutineKey::FireWeapon},
        {"getvisualrange", MCAblRoutineKey::GetVisualRange},
        {"getunitmates", MCAblRoutineKey::GetUnitMates},
        {"gettacorder", MCAblRoutineKey::GetTacOrder},
        {"getlasttacorder", MCAblRoutineKey::GetLastTacOrder},
        {"setordermode", MCAblRoutineKey::SetOrderMode},
        {"orderwait", MCAblRoutineKey::OrderWait},
        {"ordermoveto", MCAblRoutineKey::OrderMoveTo},
        {"ordermovetoobject", MCAblRoutineKey::OrderMoveToObject},
        {"ordermovetocontact", MCAblRoutineKey::OrderMoveToContact},
        {"ordertraversepath", MCAblRoutineKey::OrderTraversePath},
        {"orderpatrolpath", MCAblRoutineKey::OrderPatrolPath},
        {"orderpowerdown", MCAblRoutineKey::OrderPowerDown},
        {"orderpowerup", MCAblRoutineKey::OrderPowerUp},
        {"orderattackobject", MCAblRoutineKey::OrderAttackObject},
        {"orderattackcontact", MCAblRoutineKey::OrderAttackContact},
        {"attackthreat", MCAblRoutineKey::AttackThreat},
        {"attackclosesttarget", MCAblRoutineKey::AttackClosestTarget},
        {"attackperorders", MCAblRoutineKey::AttackPerOrders},
        {"orderwithdraw", MCAblRoutineKey::OrderWithdraw},
        {"retreat", MCAblRoutineKey::Retreat},
        {"openfire", MCAblRoutineKey::OpenFire},
        {"fireuponenemyfireonly", MCAblRoutineKey::FireUponEnemyFireOnly},
        {"damageobject", MCAblRoutineKey::DamageObject},
        {"setattackradius", MCAblRoutineKey::SetAttackRadius},
        {"ordertest", MCAblRoutineKey::OrderTest},
        {"playsmacker", MCAblRoutineKey::PlaySmacker},
        {"fileexists", MCAblRoutineKey::FileExists},
        {"objectchangesides", MCAblRoutineKey::ObjectChangeSides},
        {"distancetoobject", MCAblRoutineKey::DistanceToObject},
        {"distancetoposition", MCAblRoutineKey::DistanceToPosition},
        {"objectsuicide", MCAblRoutineKey::ObjectSuicide},
        {"objectcreate", MCAblRoutineKey::ObjectCreate},
        {"objectexists", MCAblRoutineKey::ObjectExists},
        {"objectstatus", MCAblRoutineKey::ObjectStatus},
        {"objectstatuscount", MCAblRoutineKey::ObjectStatusCount},
        {"objectvisible", MCAblRoutineKey::ObjectVisible},
        {"objectside", MCAblRoutineKey::ObjectSide},
        {"objectcommander", MCAblRoutineKey::ObjectCommander},
        {"objectclass", MCAblRoutineKey::ObjectClass},
        {"settimer", MCAblRoutineKey::SetTimer},
        {"checktimer", MCAblRoutineKey::CheckTimer},
        {"endtimer", MCAblRoutineKey::EndTimer},
        {"setobjectivetimer", MCAblRoutineKey::SetObjectiveTimer},
        {"checkobjectivetimer", MCAblRoutineKey::CheckObjectiveTimer},
        {"setobjectivestatus", MCAblRoutineKey::SetObjectiveStatus},
        {"checkobjectivestatus", MCAblRoutineKey::CheckObjectiveStatus},
        {"setobjectivetype", MCAblRoutineKey::SetObjectiveType},
        {"checkobjectivetype", MCAblRoutineKey::CheckObjectiveType},
        {"playdigitalmusic", MCAblRoutineKey::PlayDigitalMusic},
        {"stopmusic", MCAblRoutineKey::StopMusic},
        {"playsoundeffect", MCAblRoutineKey::PlaySoundEffect},
        {"playvideo", MCAblRoutineKey::PlayVideo},
        {"setradio", MCAblRoutineKey::SetRadio},
        {"playspeech", MCAblRoutineKey::PlaySpeech},
        {"playbetty", MCAblRoutineKey::PlayBetty},
        {"setobjectactive", MCAblRoutineKey::SetObjectActive},
        {"objectinwithdrawal", MCAblRoutineKey::ObjectInWithdrawal},
        {"objecttypeid", MCAblRoutineKey::ObjectTypeId},
        {"getterrainobjectpartid", MCAblRoutineKey::GetTerrainObjectPartId},
        {"getvehiclepartid", MCAblRoutineKey::GetVehiclePartId},
        {"getweaponammo", MCAblRoutineKey::GetWeaponAmmo},
        {"inarea", MCAblRoutineKey::InArea},
        {"getsensorsworking", MCAblRoutineKey::GetSensorsWorking},
        {"getcurrentbrvalue", MCAblRoutineKey::GetCurrentBRValue},
        {"setcurrentbrvalue", MCAblRoutineKey::SetCurrentBRValue},
        {"getarmorpts", MCAblRoutineKey::GetArmorPts},
        {"getmaxarmor", MCAblRoutineKey::GetMaxArmor},
        {"getpilotid", MCAblRoutineKey::GetPilotId},
        {"getpilotwounds", MCAblRoutineKey::GetPilotWounds},
        // Original behaviour: registered with the get key (see RTN_GET_PILOT_WOUNDS).
        {"setpilotwounds", MCAblRoutineKey::GetPilotWounds},
        {"getobjectactive", MCAblRoutineKey::GetObjectActive},
        {"getobjectdamage", MCAblRoutineKey::GetObjectDamage},
        {"setobjectdamage", MCAblRoutineKey::SetObjectDamage},
        {"getobjectdmgpts", MCAblRoutineKey::GetObjectDmgPts},
        {"getobjectmaxdmg", MCAblRoutineKey::GetObjectMaxDmg},
        {"getglobalvalue", MCAblRoutineKey::GetGlobalValue},
        {"setglobalvalue", MCAblRoutineKey::SetGlobalValue},
        {"setobjectivepos", MCAblRoutineKey::SetObjectivePos},
        {"setmoverbehavior", MCAblRoutineKey::SetMoverBehavior},
        {"setmoveroverlayweight", MCAblRoutineKey::SetMoverOverlayWeight},
        {"setpotentialcontact", MCAblRoutineKey::SetPotentialContact},
        {"setsensorrange", MCAblRoutineKey::SetSensorRange},
        {"settonnage", MCAblRoutineKey::SetTonnage},
        {"playwavefile", MCAblRoutineKey::PlayWaveFile},
        {"setexplosiondamage", MCAblRoutineKey::SetExplosionDamage},
        {"setexplosionradius", MCAblRoutineKey::SetExplosionRadius},
        {"setsalvage", MCAblRoutineKey::SetSalvage},
        {"setsalvagestatus", MCAblRoutineKey::SetSalvageStatus},
        {"setanimation", MCAblRoutineKey::SetAnimation},
        {"setrevealed", MCAblRoutineKey::SetRevealed},
        {"getsalvage", MCAblRoutineKey::GetSalvage},
        {"orderrefit", MCAblRoutineKey::OrderRefit},
        {"setcaptured", MCAblRoutineKey::SetCaptured},
        {"setcaptureable", MCAblRoutineKey::SetCaptureable},
        {"ordercapture", MCAblRoutineKey::OrderCapture},
        {"iscaptured", MCAblRoutineKey::IsCaptured},
        {"iscapturable", MCAblRoutineKey::IsCapturable},
        {"wasevercapturable", MCAblRoutineKey::WasEverCapturable},
        {"setbuildingname", MCAblRoutineKey::SetBuildingName},
        {"callstrike", MCAblRoutineKey::CallStrike},
        {"orderloadelementals", MCAblRoutineKey::OrderLoadElementals},
        {"orderdeployelementals", MCAblRoutineKey::OrderDeployElementals},
        {"addprisoner", MCAblRoutineKey::AddPrisoner},
        {"settrainspeed", MCAblRoutineKey::SetTrainSpeed},
        {"lockgateopen", MCAblRoutineKey::LockGateOpen},
        {"lockgateclosed", MCAblRoutineKey::LockGateClosed},
        {"releasegatelock", MCAblRoutineKey::ReleaseGateLock},
        {"isgateopen", MCAblRoutineKey::IsGateOpen},
        {"callstrikeex", MCAblRoutineKey::CallStrikeEx},
        {"getrelativepositiontopoint", MCAblRoutineKey::GetRelativePositionToPoint},
        {"getrelativepositiontoobject", MCAblRoutineKey::GetRelativePositionToObject},
        {"getunitstatus", MCAblRoutineKey::GetUnitStatus},
        {"repair", MCAblRoutineKey::Repair},
        {"getfixed", MCAblRoutineKey::GetFixed},
        {"getrepairstate", MCAblRoutineKey::GetRepairState},
        {"isteamtargeting", MCAblRoutineKey::IsTeamTargeting},
        {"sendmessage", MCAblRoutineKey::SendMessage},
        {"getmessage", MCAblRoutineKey::GetMessage},
        {"gethometeam", MCAblRoutineKey::GetHomeTeam},
        {"getstrikes", MCAblRoutineKey::GetStrikes},
        {"setstrikes", MCAblRoutineKey::SetStrikes},
        {"addstrikes", MCAblRoutineKey::AddStrikes},
        {"isserver", MCAblRoutineKey::IsServer},
    };

    /// <summary>Makes <paramref name="symbol"/> a predefined scalar or enumeration type of <paramref name="size"/> bytes.</summary>
    auto DefineType(MCAblSymbol& symbol, MCAblType& type, MCAblTypeForm form, int32_t size) -> void
    {
        symbol.Defn.Key = MCAblSymbolKind::Type;
        symbol.Type = type.Id;
        type.Form = form;
        type.Size = size;
    }
}

MCAblSymbolTable::MCAblSymbolTable()
{
    // The code a return jumps to (the original's static exitRoutineCode).
    _CodeSegments.push_back(std::make_shared<const std::vector<char>>(
        std::vector<char>{static_cast<char>(MCAblToken::EndFunction), static_cast<char>(MCAblToken::Semicolon),
                          static_cast<char>(MCAblToken::None)}));

    auto enter = [this](std::string_view name) -> MCAblSymbol&
    { return EnterSymTable(MakeSymbol(name, 0), _GlobalScope); };

    MCAblSymbol& integerSymbol = enter("integer");
    MCAblSymbol& charSymbol = enter("char");
    MCAblSymbol& realSymbol = enter("real");
    MCAblSymbol& booleanSymbol = enter("boolean");
    MCAblSymbol& falseSymbol = enter("false");
    MCAblSymbol& trueSymbol = enter("true");

    // Made first, so they have the predefined ids.
    DefineType(integerSymbol, MakeType(), MCAblTypeForm::Scalar, 4);
    DefineType(charSymbol, MakeType(), MCAblTypeForm::Scalar, 1);
    DefineType(realSymbol, MakeType(), MCAblTypeForm::Scalar, 4);
    DefineType(booleanSymbol, MakeType(), MCAblTypeForm::Enum, 4);

    falseSymbol.Defn.Key = MCAblSymbolKind::Const;
    falseSymbol.Defn.Info.Constant.Value.Integer = 0;
    falseSymbol.Type = MCAblTypeId::Boolean;
    falseSymbol.Next = trueSymbol.Id;

    trueSymbol.Defn.Key = MCAblSymbolKind::Const;
    trueSymbol.Defn.Info.Constant.Value.Integer = 1;
    trueSymbol.Type = MCAblTypeId::Boolean;

    for (const MCStandardRoutine& routine : StandardRoutines)
    {
        MCAblSymbol& symbol = enter(routine.Name);
        symbol.Defn.Key = MCAblSymbolKind::Function;
        symbol.Defn.Info.Routine.Key = routine.Key;
    }
}

auto MCAblSymbolTable::MakeSymbol(std::string_view name, int32_t level) -> MCAblSymbol&
{
    MCAblSymbol& symbol = _Symbols.emplace_back();
    // The definition is a union: zero all of it, as the original's heap gave it (P1c). Zero ids are none.
    std::memset(&symbol.Defn, 0, sizeof(symbol.Defn));
    symbol.Id = static_cast<MCAblSymbolId>(_Symbols.size());
    symbol.Name = name;
    symbol.Level = level;
    return symbol;
}

auto MCAblSymbolTable::MakeType() -> MCAblType&
{
    MCAblType& type = _Types.emplace_back();
    type.Id = static_cast<MCAblTypeId>(_Types.size());
    return type;
}

auto MCAblSymbolTable::MakeStringType(int32_t length) -> MCAblTypeId
{
    MCAblType& type = MakeType();
    type.Form = MCAblTypeForm::Array;
    type.Size = length;
    type.Array.IndexType = MCAblTypeId::Integer;
    type.Array.ElementType = MCAblTypeId::Char;
    type.Array.ElementCount = length + 1;
    return type.Id;
}

auto AblSymbols() -> MCAblSymbolTable&
{
    const std::shared_ptr<MCAblSymbolTable> symbols = MCGameContext::Current().AblSymbols();

    if (symbols == nullptr)
    {
        Fatal(0, " ABL: compiling before ABLi_init ");
    }

    // The context keeps it until AblClose.
    return *symbols;
}

auto MCAblSymbolTable::AddCodeSegment(const std::vector<char>& code) -> MCAblSegmentId
{
    std::vector<char> segment(code.size() + 1);
    std::ranges::copy(code, segment.begin());
    segment[code.size()] = static_cast<char>(MCAblToken::None);
    _CodeSegments.push_back(std::make_shared<const std::vector<char>>(std::move(segment)));
    return static_cast<MCAblSegmentId>(_CodeSegments.size());
}

auto MCAblSymbolTable::SearchSymTable(std::string_view name, MCAblSymbolId root) const -> MCAblSymbolId
{
    while (root != MCAblSymbolId::None)
    {
        const MCAblSymbol& node = Symbol(root);
        const int compareResult = name.compare(node.Name);

        if (compareResult == 0)
        {
            return root;
        }

        root = compareResult < 0 ? node.Left : node.Right;
    }

    return MCAblSymbolId::None;
}

auto MCAblSymbolTable::SearchLibrarySymTable(std::string_view name, MCAblSymbolId root) const -> MCAblSymbolId
{
    if (root == MCAblSymbolId::None)
    {
        return MCAblSymbolId::None;
    }

    const MCAblSymbol& node = Symbol(root);

    if (name == node.Name)
    {
        return root;
    }

    if (node.Library != MCAblLibraryId::None && node.Defn.Key == MCAblSymbolKind::Module)
    {
        if (const MCAblSymbolId found = SearchSymTable(name, node.Defn.Info.Routine.LocalSymTable);
            found != MCAblSymbolId::None)
        {
            return found;
        }
    }

    if (const MCAblSymbolId found = SearchLibrarySymTable(name, node.Left); found != MCAblSymbolId::None)
    {
        return found;
    }

    return SearchLibrarySymTable(name, node.Right);
}

auto MCAblSymbolTable::EnterSymTable(MCAblSymbol& symbol, MCAblSymbolId& root) -> MCAblSymbol&
{
    symbol.Parent = MCAblSymbolId::None;

    if (root == MCAblSymbolId::None)
    {
        root = symbol.Id;
        return symbol;
    }

    for (MCAblSymbolId parent = root;;)
    {
        MCAblSymbol& parentSymbol = Symbol(parent);
        MCAblSymbolId& link = symbol.Name.compare(parentSymbol.Name) < 0 ? parentSymbol.Left : parentSymbol.Right;

        if (link == MCAblSymbolId::None)
        {
            link = symbol.Id;
            symbol.Parent = parent;
            return symbol;
        }

        parent = link;
    }
}
