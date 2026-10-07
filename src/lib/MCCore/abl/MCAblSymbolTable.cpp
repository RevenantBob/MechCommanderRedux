#include "stdafx.h"
#include "abl/MCAblSymbolTable.h"
#include "main/MCGameContext.h"

MCAblType* IntegerTypePtr = nullptr;
MCAblType* CharTypePtr = nullptr;
MCAblType* RealTypePtr = nullptr;
MCAblType* BooleanTypePtr = nullptr;

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
    auto DefineType(MCAblSymbol* symbol, MCAblType* type, MCAblTypeForm form, int32_t size) -> MCAblType*
    {
        symbol->Defn.Key = MCAblSymbolKind::Type;
        symbol->TypePtr = type;
        type->Form = form;
        type->Size = size;
        return type;
    }
}

MCAblSymbolTable::MCAblSymbolTable()
{
    auto enter = [this](std::string_view name) { return EnterSymTable(MakeSymbol(name, 0), _GlobalScope); };

    MCAblSymbol* integerSymbol = enter("integer");
    MCAblSymbol* charSymbol = enter("char");
    MCAblSymbol* realSymbol = enter("real");
    MCAblSymbol* booleanSymbol = enter("boolean");
    MCAblSymbol* falseSymbol = enter("false");
    MCAblSymbol* trueSymbol = enter("true");

    IntegerTypePtr = DefineType(integerSymbol, MakeType(), MCAblTypeForm::Scalar, 4);
    CharTypePtr = DefineType(charSymbol, MakeType(), MCAblTypeForm::Scalar, 1);
    RealTypePtr = DefineType(realSymbol, MakeType(), MCAblTypeForm::Scalar, 4);
    BooleanTypePtr = DefineType(booleanSymbol, MakeType(), MCAblTypeForm::Enum, 4);

    falseSymbol->Defn.Key = MCAblSymbolKind::Const;
    falseSymbol->Defn.Info.Constant.Value.Integer = 0;
    falseSymbol->TypePtr = BooleanTypePtr;
    falseSymbol->Next = trueSymbol;

    trueSymbol->Defn.Key = MCAblSymbolKind::Const;
    trueSymbol->Defn.Info.Constant.Value.Integer = 1;
    trueSymbol->TypePtr = BooleanTypePtr;

    for (const MCStandardRoutine& routine : StandardRoutines)
    {
        MCAblSymbol* symbol = enter(routine.Name);
        symbol->Defn.Key = MCAblSymbolKind::Function;
        symbol->Defn.Info.Routine.Key = routine.Key;
    }
}

MCAblSymbolTable::~MCAblSymbolTable()
{
    for (MCAblType** global : {&IntegerTypePtr, &CharTypePtr, &RealTypePtr, &BooleanTypePtr})
    {
        if (std::ranges::any_of(_Types, [global](const MCAblType& type) { return &type == *global; }))
        {
            *global = nullptr;
        }
    }
}

auto MCAblSymbolTable::MakeSymbol(std::string_view name, int32_t level) -> MCAblSymbol*
{
    MCAblSymbol& symbol = _Symbols.emplace_back();
    // The definition is a union: zero all of it, as the original's heap gave it (P1c).
    std::memset(&symbol.Defn, 0, sizeof(symbol.Defn));
    symbol.Name = name;
    symbol.Level = level;
    return &symbol;
}

auto MCAblSymbolTable::MakeType() -> MCAblType*
{
    return &_Types.emplace_back();
}

auto MCAblSymbolTable::MakeStringType(int32_t length) -> MCAblType*
{
    MCAblType* type = MakeType();
    type->Form = MCAblTypeForm::Array;
    type->Size = length;
    type->Array.IndexTypePtr = IntegerTypePtr;
    type->Array.ElementTypePtr = CharTypePtr;
    type->Array.ElementCount = length + 1;
    return type;
}

auto AblSymbols() -> MCAblSymbolTable*
{
    return MCGameContext::Current().AblSymbols();
}

auto SearchSymTable(std::string_view name, MCAblSymbol* root) -> MCAblSymbol*
{
    while (root != nullptr)
    {
        const int compareResult = name.compare(root->Name);

        if (compareResult == 0)
        {
            return root;
        }

        root = compareResult < 0 ? root->Left : root->Right;
    }

    return nullptr;
}

auto SearchLibrarySymTable(std::string_view name, MCAblSymbol* root) -> MCAblSymbol*
{
    if (root == nullptr)
    {
        return nullptr;
    }

    if (name == root->Name)
    {
        return root;
    }

    if (root->Library != nullptr && root->Defn.Key == MCAblSymbolKind::Module)
    {
        if (MCAblSymbol* found = SearchSymTable(name, root->Defn.Info.Routine.LocalSymTable))
        {
            return found;
        }
    }

    if (MCAblSymbol* found = SearchLibrarySymTable(name, root->Left))
    {
        return found;
    }

    return SearchLibrarySymTable(name, root->Right);
}

auto EnterSymTable(MCAblSymbol* symbol, MCAblSymbol*& root) -> MCAblSymbol*
{
    MCAblSymbol* parent = nullptr;
    MCAblSymbol** link = &root;

    while (*link != nullptr)
    {
        parent = *link;
        link = symbol->Name.compare(parent->Name) < 0 ? &parent->Left : &parent->Right;
    }

    *link = symbol;
    symbol->Parent = parent;
    return symbol;
}
