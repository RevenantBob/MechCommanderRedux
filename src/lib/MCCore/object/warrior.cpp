#include "stdafx.h"
#include "object/warrior.h"
#include "abl/MCAblDebugger.h"
#include "abl/MCScrollingTextWindow.h"
#include "abl/MCAblRuntime.h"
#include "ai/MCMoveSystem.h"
#include "ai/MCTacticalOrder.h"
#include "gui/afont.h"
#include "gui/aport.h"
#include "gui/asystem.h"
#include "iface/iface.h"
#include "lib/MCFatal.h"
#include "lib/MCFrameOfRef.h"
#include "lib/MCVector2D.h"
#include "lib/MCDice.h"
#include "lib/MCFile.h"
#include "lib/MCFitIniFile.h"
#include "lib/MCPacketFile.h"
#include "main/main.h"
#include "network/multplyr.h"
#include "object/MCMasterComponent.h"
#include "object/elemntl.h"
#include "object/MCMoverGroup.h"
#include "object/gvehicl.h"
#include "object/mech.h"
#include "object/mover.h"
#include "object/MCObjectQueue.h"
#include "object/MCObjectSystem.h"
#include "object/MCSortList.h"
#include "object/tbldng.h"
#include "object/MCForces.h"
#include "sound/radio.h"
#include "terrain/MCTerrain.h"
#include "vfx/MCVfxFunctions.h"

// The pilot data (MCX.EXE 0x007931d4..0x00793310), in the original's order.
float FireOddsTable[5] = {20.0f, 35.0f, 50.0f, 65.0f, 80.0f};
const char* PilotAlarmFunctionName[NUM_PILOT_ALARMS] = {"handletargetofweaponfire",
                                                        "handlehitbyweaponfire",
                                                        "handledamagetakenrate",
                                                        "handledeathofmate",
                                                        "handlecripplingoffriendlyvehicle",
                                                        "handledestructionoffriendlyvehicle",
                                                        "handleincapacitationofvehicle",
                                                        "handledestructionofvehicle",
                                                        "handlewithdraw",
                                                        "handlemoralebreak",
                                                        "handlecollision",
                                                        "handleguardbreach",
                                                        "handlekilledtarget",
                                                        "handlematefiredweapon",
                                                        "handleplayerorder",
                                                        "handlenomovepath",
                                                        "handlegateclosing"};
int8_t ProfessionalismOffsetTable[5][2] = {{10, 10}, {20, 5}, {30, 0}, {40, 5}, {100, -10}};
int8_t DecorumOffsetTable[5][2] = {{10, 10}, {20, 5}, {30, 0}, {40, 5}, {100, -10}};
int8_t AmmoConservationModifiers[2][2] = {{50, -5}, {20, -10}};
const char* SkillsTable[NUM_SKILLS] = {"Piloting", "Jumping", "Sensors", "Gunnery"};
float ThreatRatingEffect[8][3] = {{0.5f, -1.0f, 0.0f},  {0.75f, -0.2f, 0.0f},     {1.0f, 0.0f, 0.0f},
                                  {1.25f, 0.0f, 0.0f},  {1.5f, 0.0f, 0.0f},       {2.0f, 0.1f, 0.0f},
                                  {2.25f, 0.2f, -0.1f}, {1000000.0f, 0.2f, -1.0f}};
float BrainUpdateFrequency = 2.0f;
float MovementUpdateFrequency = 5.0f;
float CombatUpdateFrequency = 0.25f;
float CommandUpdateFrequency = 6.0f;
float ContactUpdateFrequency = 4.0f;
float PilotCheckUpdateFrequency = 1.0f;
int32_t PilotCheckModifierTable[2] = {25, 25};
float SkillWeightings[NUM_SKILLS] = {1.0f, 1.0f, 1.0f, 1.0f};
float WarriorRankScale[4] = {60.0f, 75.0f, 85.0f, 999.0f};
int32_t GroupMoveTrailLen[2] = {0, 1};
float MoveTimeOut = 30.0f;
float MoveYieldTime = 1.5f;
float DefaultAttackRadius = 275.0f;
int InitWayPath = 1;

MCQueuedTacOrder TacOrderQueue[MAX_QUEUED_TACORDERS];
int32_t MCMechWarrior::NumWarriors = 0;
int32_t MCMechWarrior::NumWarriorsInCombat = 0;
MCSortList* MCMechWarrior::SortList = nullptr;
int32_t LastMoveCalcErr = 0;
int32_t TacOrderQueuePos = 0;
MCScrollingTextWindow* GameSystemWindow = nullptr;

namespace
{
    /// <summary>Whether <paramref name="object"/> is a mover (mech, vehicle, elemental or plain mover).</summary>
    bool IsMover(const MCBaseObject* object)
    {
        const MCObjectClass objectClass = object->ObjectClass;
        return objectClass == MCObjectClass::BattleMech || objectClass == MCObjectClass::GroundVehicle ||
               objectClass == MCObjectClass::Elemental || objectClass == MCObjectClass::Mover;
    }

    /// <summary>A new[] copy of <paramref name="text"/>, as the original's inline strlen/malloc/strcpy.</summary>
    char* CopyString(const char* text)
    {
        const size_t size = std::strlen(text) + 1;
        auto* copy = new char[size];
        std::memcpy(copy, text, size);
        return copy;
    }

    /// <summary>Whether movement cell (cellR, cellC) of tile (tileR, tileC) can be entered.</summary>
    bool CellPassable(int32_t tileR, int32_t tileC, int32_t cellR, int32_t cellC)
    {
        // Port fix: the walks can leave the map, where the original reads outside it. Off the map is impassable.
        if (!GameMap()->OnMap(tileR, tileC))
        {
            return false;
        }

        return GameMap()->Map[GameMap()->Width * tileR + tileC].GetCellPassable(cellR, cellC) != 0;
    }

    /// <summary>Whether the movement cell under <paramref name="position"/> can be entered.</summary>
    bool PositionPassable(MCVector3D position)
    {
        int32_t tileR;
        int32_t tileC;
        int32_t cellR;
        int32_t cellC;
        GameMap()->WorldToMapPos(position, tileR, tileC, cellR, cellC);
        return CellPassable(tileR, tileC, cellR, cellC);
    }

    /// <summary>
    /// Lifts the path locks of the pilot's vehicle and, on a ramming attack, of the mover rammed, so that the path
    /// finder can plan through them (calcMovePath repeats this around each path it plans).
    /// </summary>
    void BeginPathCalc(MCMechWarrior* pilot, MCMover* mover)
    {
        PathFindMap()->MovingObject = mover;
        mover->UpdatePathLock(0);

        if (pilot->CurTacOrder.Code == MCTacticalOrderCode::AttackObject && pilot->CurTacOrder.AttackParams.Method == 2)
        {
            PathFindMap()->RamObject = pilot->CurTacOrder.Target;

            if (PathFindMap()->RamObject != nullptr && IsMover(PathFindMap()->RamObject))
            {
                static_cast<MCMover*>(PathFindMap()->RamObject)->UpdatePathLock(0);
            }
        }
        else
        {
            PathFindMap()->RamObject = nullptr;
        }
    }

    /// <summary>Puts back the path locks <see cref="BeginPathCalc"/> lifted.</summary>
    void EndPathCalc(MCMover* mover)
    {
        if (PathFindMap()->RamObject != nullptr && IsMover(PathFindMap()->RamObject))
        {
            static_cast<MCMover*>(PathFindMap()->RamObject)->UpdatePathLock(1);
        }

        mover->UpdatePathLock(1);
        PathFindMap()->MovingObject = nullptr;
        PathFindMap()->RamObject = nullptr;
    }

    /// <summary>The flags calcMovePath adds for the path finder: 0x40, and 0x80 unless the mover is an elemental.</summary>
    uint32_t PathFinderParams(const MCMover* mover, uint32_t moveParams)
    {
        if (mover->ObjectClass != MCObjectClass::Elemental)
        {
            moveParams |= 0x80;
        }

        return moveParams | 0x40;
    }

    /// <summary>Stores <paramref name="point"/> as a tactical order's first way point (the original's inline copy).</summary>
    void SetFirstWayPoint(MCTacticalOrder& order, MCVector3D point)
    {
        order.MoveParams.WayPath.Points[0] = point.X;
        order.MoveParams.WayPath.Points[1] = point.Y;
        order.MoveParams.WayPath.Points[2] = point.Z;
    }

    /// <summary>The distance past its fire range a mover may stand before its attack move goes on: two vertices.</summary>
    double AttackRangeSlack()
    {
        return static_cast<double>(MetersPerWorldUnit) * MCTerrain::MetersPerVertex +
               static_cast<double>(MetersPerWorldUnit) * MCTerrain::MetersPerVertex;
    }

    /// <summary>
    /// The refit flag orderGetFixed reads at +0x130 of its target: a repair bay's mechBay. On a refit vehicle the
    /// original read the mover's ECM tracker pointer there; the port gives what that pointer compared as.
    /// </summary>
    int32_t RepairBayKind(MCGameObject* target)
    {
        if (target->ObjectClass == MCObjectClass::TreeBuilding)
        {
            return static_cast<MCTreeBuilding*>(target)->MechBay;
        }

        if (IsMover(target))
        {
            return static_cast<MCMover*>(target)->EcmTracker != nullptr ? 2 : 0;
        }

        return 0;
    }
}

//---------------------------------------------------------------------------
// MechWarrior

auto MCMechWarrior::Lobotomy() -> void
{
    Brain.reset();

    for (int32_t i = 0; i < NUM_PILOT_ALARMS; i++)
    {
        BrainAlarmCallback[i] = nullptr;
    }
}

auto MCMechWarrior::Init() -> void
{
    Professionalism = 40;
    Decorum = 40;
    Aggressiveness = 40;
    Courage = 40;
    Name = nullptr;
    LastUnderAttackTime = -1000.0f;
    LastContactTime = -1000.0f;
    Callsign = nullptr;
    Picture = nullptr;
    Team = nullptr;
    VideoStr = nullptr;
    AudioStr = nullptr;
    BrainStr = nullptr;
    Vehicle = nullptr;
    Wounds = 0.0f;
    Status = 0;
    EscapesThruEjection = 0;
    Rank = 0;
    LastMessage = -1;
    Weapons50Sent = 0;
    WeaponsOutSent = 0;

    for (int32_t i = 0; i < NUM_SKILLS; i++)
    {
        NumSkillUses[i][0] = 0;
        NumSkillUses[i][1] = 0;
        NumSkillSuccesses[i][0] = 0;
        NumSkillSuccesses[i][1] = 0;
        SkillRank[i] = 0.0f;
        SkillPoints[i] = 0.0f;
    }

    for (int32_t i = 0; i < 7; i++)
    {
        NumKilled[i][0] = 0;
        NumKilled[i][1] = 0;
    }

    Brain = nullptr;

    for (int32_t i = 0; i < NUM_PILOT_ALARMS; i++)
    {
        BrainAlarmCallback[i] = nullptr;
    }

    // Spread the warriors' updates over the first frames.
    BrainUpdateTime = static_cast<float>(static_cast<double>(NumWarriors % 30) * 0.2);
    CombatUpdateTime = static_cast<float>(static_cast<double>(NumWarriors % 15) * 0.1);
    MovementUpdateTime = static_cast<float>(static_cast<double>(NumWarriors % 15) * 0.2);

    for (int32_t i = 0; i < MAX_WEAPONS_PER_WARRIOR; i++)
    {
        WeaponsStatus[i] = 0;
    }

    WeaponsStatusResult = -2;
    NewTacOrderReceived[ORDERSTATE_GENERAL] = 0;
    NewTacOrderReceived[ORDERSTATE_PLAYER] = 0;
    NewTacOrderReceived[ORDERSTATE_ALARM] = 0;
    TacOrder[ORDERSTATE_GENERAL].Reset();
    TacOrder[ORDERSTATE_PLAYER].Reset();
    TacOrder[ORDERSTATE_ALARM].Reset();
    QueuedOrders = nullptr;
    EnableTacOrderQueue();
    PlayerOrderFromQueue = 0;
    TacOrderQueueLocked = 0;
    TacOrderQueueExecuting = 0;
    NumTacOrdersQueued = 0;
    NextTacOrderId = 1;
    LastTacOrderId = 0;
    AlarmPriority = 0;
    CurTacOrder.Reset();
    LastTacOrder.Reset();
    OrderState = ORDERSTATE_GENERAL;
    MoveOrders.Init();
    AttackOrders.Init();
    OrderFireRange = -1.0f;
    OrderFireOdds = -1.0f;

    for (int32_t i = 0; i < 2; i++)
    {
        MCMovePath* path = new MCMovePath;

        if (path != nullptr)
        {
            // MovePath's inline constructor.
            path->Goal = MCVector3D(0.0f, 0.0f, 0.0f);
            path->NumSteps = 0;
            path->NumStepsWhenNotPaused = 0;
            path->CurStep = 0;
            path->Cost = 0;
            path->Marked = 0;
            path->GlobalStep = -1;
        }

        MoveOrders.Path[i] = path;

        if (path == nullptr)
        {
            Fatal(0, " No RAM for warrior path ");
        }
    }

    MovePathRequest = nullptr;
    LastTarget = nullptr;
    LastTargetTime = -1.0f;
    LastTargetObliterate = 0;
    LastTargetFriendly = 0;

    for (int32_t i = 0; i < NUM_PILOT_ALARMS; i++)
    {
        ClearAlarm(i);
    }

    TimeOfLastOrders = -1.0f;
    AttackRadius = DefaultAttackRadius;

    if (SortList == nullptr)
    {
        SortList = new MCSortList(100);
    }

    DebugFlags = 0;
    NumAttackers = 0;
    AmmoOutSent = 0;
    NumWarriors++;
}

auto MCMechWarrior::Init(MCFitIniFile* warriorFile) -> int32_t
{
    char audioName[512];
    char videoName[512];

    int32_t result = warriorFile->SeekBlock("General");

    if (result != 0)
    {
        return result;
    }

    result = warriorFile->ReadIdString("Name", audioName, 0x1ff);

    if (result != 0)
    {
        return result;
    }

    if (warriorFile->ReadIdLong("DescIndex", DescIndex) != 0)
    {
        DescIndex = -1;
    }

    if (warriorFile->ReadIdLong("NameIndex", NameIndex) != 0)
    {
        NameIndex = -1;
    }

    Name = CopyString(audioName);

    if (warriorFile->ReadIdBoolean("NotMineYet", NotMineYet) != 0)
    {
        NotMineYet = 0;
    }

    if (warriorFile->ReadIdString("Picture", audioName, 0x1ff) == 0)
    {
        Picture = CopyString(audioName);
    }
    else
    {
        Picture = CopyString("pilotx.gif");
    }

    result = warriorFile->ReadIdString("Callsign", audioName, 0x1ff);

    if (result != 0)
    {
        return result;
    }

    Callsign = CopyString(audioName);

    if (warriorFile->ReadIdUChar("OldPilot", OldPilot) != 0)
    {
        OldPilot = 0;
    }

    Radio = nullptr;

    if (warriorFile->ReadIdString("pilotAudio", audioName, 0x1ff) == 0)
    {
        AudioStr = CopyString(audioName);

        if (warriorFile->ReadIdString("pilotVideo", videoName, 0x1ff) != 0)
        {
            videoName[0] = '\0';
        }

        VideoStr = CopyString(videoName);

        MCRadio* newRadio = new MCRadio;

        if (newRadio != nullptr)
        {
            // Radio's inline constructor.
            newRadio->RadioFile = nullptr;
            newRadio->MovieName.clear();
            newRadio->Enabled = 1;
        }

        Radio = newRadio;

        if (newRadio->Init(audioName, 0x19000, videoName) != 0)
        {
            delete newRadio->RadioFile;
            newRadio->RadioFile = nullptr;
            delete newRadio;
            Radio = nullptr;
        }
    }

    if (warriorFile->ReadIdLong("PaintScheme", PaintScheme) != 0)
    {
        PaintScheme = -1;
    }

    result = warriorFile->SeekBlock("PersonalityTraits");

    if (result != 0)
    {
        return result;
    }

    result = warriorFile->ReadIdChar("Professionalism", reinterpret_cast<char&>(Professionalism));

    if (result != 0)
    {
        return result;
    }

    result = warriorFile->ReadIdChar("Decorum", reinterpret_cast<char&>(Decorum));

    if (result != 0)
    {
        return result;
    }

    result = warriorFile->ReadIdChar("Aggressiveness", reinterpret_cast<char&>(Aggressiveness));

    if (result != 0)
    {
        return result;
    }

    result = warriorFile->ReadIdChar("Courage", reinterpret_cast<char&>(Courage));

    if (result != 0)
    {
        return result;
    }

    BaseCourage = Courage;

    result = warriorFile->SeekBlock("Skills");

    if (result != 0)
    {
        return result;
    }

    for (int32_t i = 0; i < NUM_SKILLS; i++)
    {
        result = warriorFile->ReadIdChar(SkillsTable[i], reinterpret_cast<char&>(Skills[i]));

        if (result != 0)
        {
            return result;
        }

        SkillRank[i] = static_cast<float>(Skills[i]);
    }

    if (warriorFile->SeekBlock("OriginalSkills") == 0)
    {
        for (int32_t i = 0; i < NUM_SKILLS; i++)
        {
            if (warriorFile->ReadIdChar(SkillsTable[i], reinterpret_cast<char&>(OriginalSkills[i])) != 0)
            {
                OriginalSkills[i] = Skills[i];
            }
        }
    }
    else
    {
        for (int32_t i = 0; i < NUM_SKILLS; i++)
        {
            OriginalSkills[i] = Skills[i];
        }
    }

    if (warriorFile->SeekBlock("LatestSkills") == 0)
    {
        for (int32_t i = 0; i < NUM_SKILLS; i++)
        {
            if (warriorFile->ReadIdChar(SkillsTable[i], reinterpret_cast<char&>(LatestSkills[i])) != 0)
            {
                LatestSkills[i] = Skills[i];
            }
        }
    }
    else
    {
        for (int32_t i = 0; i < NUM_SKILLS; i++)
        {
            LatestSkills[i] = Skills[i];
        }
    }

    if (warriorFile->SeekBlock("SkillPoints") == 0)
    {
        for (int32_t i = 0; i < NUM_SKILLS; i++)
        {
            if (warriorFile->ReadIdFloat(SkillsTable[i], SkillPoints[i]) != 0)
            {
                SkillPoints[i] = 0.0f;
            }
        }
    }
    else
    {
        for (int32_t i = 0; i < NUM_SKILLS; i++)
        {
            SkillPoints[i] = 0.0f;
        }
    }

    CalcRank();

    result = warriorFile->SeekBlock("Status");

    if (result != 0)
    {
        return result;
    }

    char numWounds;
    result = warriorFile->ReadIdChar("Wounds", numWounds);

    if (result != 0)
    {
        return result;
    }

    Wounds = static_cast<float>(numWounds);

    for (int32_t i = 0; i < NUM_SKILLS; i++)
    {
        NumSkillUses[i][0] = 0;
        NumSkillUses[i][1] = 0;
        NumSkillSuccesses[i][0] = 0;
        NumSkillSuccesses[i][1] = 0;
    }

    for (int32_t i = 0; i < 5; i++)
    {
        NumKilled[i][0] = 0;
        NumKilled[i][1] = 0;
    }

    // Whether the pilot would survive ejecting: piloting + 30 percent, at most 94.
    const int32_t roll = RandomNumber(100);
    EscapesThruEjection = (roll <= Skills[MWS_PILOTING] + 30 && roll < 95) ? 1 : 0;
    return 0;
}

auto MCMoveOrders::Init() -> void
{
    Time = ScenarioTime;
    Origin = 1;
    SpeedType = 3;
    GoalObjectPosition = MCVector3D(0.0f, 0.0f, 0.0f);
    GoalLocation = MCVector3D(-999999.0f, -999999.0f, -999999.0f);
    OriginalGlobalGoal[0] = MCVector3D(-999999.0f, -999999.0f, -999999.0f);
    OriginalGlobalGoal[1] = MCVector3D(-999999.0f, -999999.0f, -999999.0f);
    SpeedVelocity = 0.0f;
    GlobalGoalLocation = MCVector3D(-666666.0f, -666666.0f, -666666.0f);
    SpeedState = 2;
    SpeedThrottle = 100;
    GoalType = -1;
    GoalObject = nullptr;
    NextUpdate = 0.0f;
    ScriptGoal = 0;
    NumWayPts = 0;
    CurWayPt = 0;
    CurWayDir = 0;
    PathType = 0;
    NumGlobalSteps = 0;
    CurGlobalStep = 0;
    Path[0] = nullptr;
    Path[1] = nullptr;
    TimeOfLastStep = -1.0f;
    MoveState = 1;
    MoveStateGoal = 1;
    MoveStateGoalChanged = 0;
    YieldTime = -1.0f;
    YieldState = 0;
    WaitForPointTime = -1.0f;
    Run = 0;
}

auto MCAttackOrders::Init() -> void
{
    Time = ScenarioTime;
    Origin = 1;
    Type = 0;
    Target = nullptr;
    TargetPoint = MCVector3D(0.0f, 0.0f, 0.0f);
    AimLocation = -1;
    Pursue = 0;
    TargetTime = 0.0f;
}

auto MCMechWarrior::RadioMessage(int32_t messageId, int propogateIfMultiplayer) -> void
{
    if (messageId >= NUM_RADIO_MESSAGES || Radio == nullptr || Status != 0 || messageId == -1 || Turn <= 0)
    {
        return;
    }

    if (UnderHomeCommand() == 0)
    {
        if (MPlayer != nullptr && MPlayer->IsServer != 0 && propogateIfMultiplayer != 0)
        {
            static_cast<MCMover*>(Vehicle)->AddRadioChunk(0, static_cast<uint8_t>(messageId));
        }

        return;
    }

    switch (messageId)
    {
        case RADIO_SENSOR_CONTACT:
        {
            if (static_cast<double>(ScenarioTime) - 15.0 < LastContactTime)
            {
                return;
            }

            LastContactTime = ScenarioTime;
            break;
        }
        case RADIO_UNDER_ATTACK:
        {
            if (static_cast<double>(ScenarioTime) - 20.0 < LastUnderAttackTime)
            {
                return;
            }

            LastUnderAttackTime = ScenarioTime;
            break;
        }
        case RADIO_WEAPONS_50:
        {
            if (Weapons50Sent != 0)
            {
                return;
            }

            Weapons50Sent = 1;
            break;
        }
        case RADIO_WEAPONS_OUT:
        {
            if (WeaponsOutSent != 0)
            {
                return;
            }

            WeaponsOutSent = 1;
            break;
        }
        default:
            break;
    }

    // Some messages may repeat at once; the rest wait 10 seconds before the same one plays again.
    switch (messageId)
    {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        case 10:
        case RADIO_REFIT:
        case RADIO_POWER:
        case RADIO_MOVE_BLOCKED:
        case RADIO_ILLEGAL_ORDER:
        case RADIO_DEPLOY:
        case RADIO_LOAD:
            break;
        default:
        {
            if (LastMessageType == messageId && static_cast<double>(ScenarioTime) - 10.0 < LastMessageTime)
            {
                return;
            }
            break;
        }
    }

    char message[128];
    std::snprintf(message, sizeof(message), "Radio Message %d\n", messageId);
    LastMessageTime = ScenarioTime;
    const int32_t played = Radio->PlayMessage(static_cast<MCRadioMessageType>(messageId));
    LastMessageType = messageId;
    LastMessage = played;
}

auto MCMechWarrior::Destroy() -> void
{
    delete[] Name;
    Name = nullptr;
    delete[] Picture;
    Picture = nullptr;
    delete[] Callsign;
    Callsign = nullptr;

    Brain.reset();

    for (int32_t i = 0; i < 2; i++)
    {
        if (MoveOrders.Path[i] != nullptr)
        {
            delete MoveOrders.Path[i];
            MoveOrders.Path[i] = nullptr;
        }
    }

    NumWarriors--;

    if (NumWarriors == 0)
    {
        if (SortList != nullptr)
        {
            delete SortList;
        }

        SortList = nullptr;
    }

    delete[] BrainStr;
    BrainStr = nullptr;
    delete[] AudioStr;
    AudioStr = nullptr;
    delete[] VideoStr;
    VideoStr = nullptr;
}

auto MCMechWarrior::GetAggressiveness(int current) -> int32_t
{
    if (current != 0 && CurTacOrder.IsCombatOrder() != 0)
    {
        return (100 - Aggressiveness) / 2 + Aggressiveness;
    }

    return Aggressiveness;
}

auto MCMechWarrior::EnableTacOrderQueue() -> int
{
    const int32_t first = TacOrderQueuePos;

    if (MAX_QUEUED_TACORDERS - TacOrderQueuePos < MAX_QUEUED_TACORDERS_PER_WARRIOR)
    {
        return 0;
    }

    TacOrderQueuePos += MAX_QUEUED_TACORDERS_PER_WARRIOR;
    NumTacOrdersQueued = 0;
    QueuedOrders = &TacOrderQueue[first];
    return 1;
}

auto MCMechWarrior::AddQueuedTacOrder(MCTacticalOrder tacOrder) -> int32_t
{
    if (QueuedOrders == nullptr)
    {
        return 1;
    }

    if (NumTacOrdersQueued == MAX_QUEUED_TACORDERS_PER_WARRIOR)
    {
        return 2;
    }

    MCQueuedTacOrder& queued = QueuedOrders[NumTacOrdersQueued];
    queued.Point = tacOrder.GetWayPoint(0);
    queued.Id = tacOrder.Id;
    queued.PackedData[0] = tacOrder.Data[0];
    queued.PackedData[1] = tacOrder.Data[1];
    NumTacOrdersQueued++;

    // The first order queued starts at once, unless a player order is waiting or one from the queue is running.
    if ((MPlayer == nullptr || MPlayer->IsServer != 0) && NumTacOrdersQueued == 1 &&
        NewTacOrderReceived[ORDERSTATE_PLAYER] == 0 &&
        (PlayerOrderFromQueue == 0 || CurTacOrder.Origin != MCOrderOrigin::Player))
    {
        ExecuteTacOrderQueue();
    }

    return 0;
}

auto MCMechWarrior::RemoveQueuedTacOrder(MCTacticalOrder* tacOrder) -> int32_t
{
    if (QueuedOrders == nullptr)
    {
        return 1;
    }

    const int32_t numQueued = NumTacOrdersQueued;

    if (numQueued == 0)
    {
        return 2;
    }

    tacOrder->Data[0] = QueuedOrders[0].PackedData[0];
    tacOrder->Data[1] = QueuedOrders[0].PackedData[1];
    const MCVector3D point = QueuedOrders[0].Point;
    const int32_t id = QueuedOrders[0].Id;

    // Port fix: the original shifted numQueued entries, reading one past the last (past the pool for the last
    // pilot's queue); the slot it filled is dropped by the count below, so only the n - 1 real moves are kept.
    for (int32_t i = 0; i < numQueued - 1; i++)
    {
        QueuedOrders[i] = QueuedOrders[i + 1];
    }

    NumTacOrdersQueued = static_cast<int8_t>(numQueued - 1);

    tacOrder->Unpack();
    tacOrder->Id = id;
    tacOrder->SetWayPoint(0, point);
    return 0;
}

auto MCMechWarrior::PeekQueuedTacOrder(MCTacticalOrder* tacOrder) -> int32_t
{
    if (QueuedOrders == nullptr)
    {
        return 1;
    }

    if (NumTacOrdersQueued == 0)
    {
        return 2;
    }

    const MCVector3D point = QueuedOrders[0].Point;
    tacOrder->Data[1] = QueuedOrders[0].PackedData[1];
    tacOrder->Data[0] = QueuedOrders[0].PackedData[0];
    tacOrder->Unpack();
    tacOrder->SetWayPoint(0, point);
    tacOrder->Id = QueuedOrders[0].Id;
    return 0;
}

auto MCMechWarrior::ClearTacOrderQueue() -> void
{
    NumTacOrdersQueued = 0;
    TacOrderQueueExecuting = 0;
}

auto MCMechWarrior::ExecuteTacOrderQueue() -> void
{
    if (NumTacOrdersQueued > 0)
    {
        TacOrderQueueExecuting = 1;
        MCTacticalOrder order;
        order.Reset();

        if (RemoveQueuedTacOrder(&order) == 0)
        {
            SetPlayerTacOrder(order, 1);
        }

        return;
    }

    TacOrderQueueExecuting = 0;
}

auto MCMechWarrior::LockTacOrderQueue() -> void
{
    TacOrderQueueLocked = 1;
}

auto MCMechWarrior::UnlockTacOrderQueue() -> void
{
    TacOrderQueueLocked = 0;
}

auto MCMechWarrior::GetTacOrderQueue(MCQueuedTacOrder* list) -> int32_t
{
    int32_t count = 0;

    if (PlayerOrderFromQueue != 0)
    {
        if (list != nullptr)
        {
            MCTacticalOrder& playerOrder = TacOrder[ORDERSTATE_PLAYER];
            list[0].Id = playerOrder.Id;
            list[0].Point = playerOrder.GetWayPoint(0);
            list[0].PackedData[0] = playerOrder.Data[0];
            list[0].PackedData[1] = playerOrder.Data[1];
        }

        count = 1;
    }

    const int32_t numQueued = NumTacOrdersQueued;

    if (numQueued > 0)
    {
        if (list != nullptr)
        {
            for (int32_t i = 0; i < numQueued; i++)
            {
                list[count + i] = QueuedOrders[i];
            }
        }

        count += numQueued;
    }

    return count;
}

auto CompareTacOrderId(int32_t id1, int32_t id2) -> int32_t
{
    if (id1 < 0xf1)
    {
        if (id1 < 0x10 && id2 > 0xf0)
        {
            return (id1 - id2) + 0xff;
        }
    }
    else if (id2 < 0x10)
    {
        return (id1 - id2) - 0xff;
    }

    return id1 - id2;
}

auto MCMechWarrior::UpdateClientOrderQueue(int32_t tacOrderId) -> void
{
    MCTacticalOrder order;
    order.Reset();
    int32_t result = PeekQueuedTacOrder(&order);

    if (tacOrderId == 0)
    {
        if (result == 0 && order.Id == LastTacOrderId)
        {
            RemoveQueuedTacOrder(&order);
        }

        return;
    }

    LastTacOrderId = tacOrderId;

    while (result == 0 && CompareTacOrderId(order.Id, tacOrderId) < 0)
    {
        RemoveQueuedTacOrder(&order);
        result = PeekQueuedTacOrder(&order);
    }
}

auto MCMechWarrior::GetGroup() -> MCMoverGroup*
{
    if (Vehicle != nullptr)
    {
        return static_cast<MCMover*>(Vehicle)->Group;
    }

    return nullptr;
}

auto MCMechWarrior::GetPoint() -> MCMover*
{
    if (GetGroup() != nullptr)
    {
        return GetGroup()->GetPoint();
    }

    return nullptr;
}

auto MCMechWarrior::OnHomeTeam() -> int
{
    return Team == HomeTeam() ? 1 : 0;
}

auto MCMechWarrior::UnderHomeCommand() -> int
{
    if (Vehicle != nullptr)
    {
        return static_cast<MCMover*>(Vehicle)->NetPlayerId >= 0 ? 1 : 0;
    }

    return 0;
}

auto MCMechWarrior::CheckSkill(int32_t skillId, float factor) -> int32_t
{
    NumSkillUses[skillId][1]++;
    SkillPoints[skillId] = SkillTry[skillId] + SkillPoints[skillId];
    const int32_t roll = RandomNumber(100);
    const int32_t margin = static_cast<int32_t>(static_cast<double>(Skills[skillId]) * factor) - roll - 1;

    if (margin >= 0 && skillId != MWS_SENSORS)
    {
        NumSkillSuccesses[skillId][1]++;
        SkillPoints[skillId] = SkillSuccess[skillId] + SkillPoints[skillId];
    }

    return margin;
}

auto MCMechWarrior::Injure(float numWounds, int checkEject) -> int
{
    if (Status != 0)
    {
        return 0;
    }

    if (numWounds > 0.0f)
    {
        RadioMessage(RADIO_PILOT_HURT, 0);
    }

    Wounds = numWounds + Wounds;

    if (static_cast<double>(Wounds) < 6.0)
    {
        return 0;
    }

    MCMover* mover = static_cast<MCMover*>(Vehicle);
    Assert(mover != nullptr, 0, " Pilot has no vehicle ");

    if (checkEject != 0)
    {
        float points = SkillTry[MWS_PILOTING] + SkillPoints[MWS_PILOTING];
        SkillPoints[MWS_PILOTING] = points;
        NumSkillUses[MWS_PILOTING][1]++;

        if (EscapesThruEjection != 0)
        {
            points = points + SkillSuccess[MWS_PILOTING];
            Wounds = 5.0f;
            NumSkillSuccesses[MWS_PILOTING][1]++;
            SkillPoints[MWS_PILOTING] = points;

            if (mover->HandleEjection() == 0)
            {
                Wounds = 6.0f;
            }
        }
    }

    if (static_cast<double>(Wounds) >= 6.0)
    {
        RadioMessage(RADIO_DEATH, 0);
        Status = 4;
    }

    if (mover != nullptr)
    {
        mover->Disable(2);
    }

    if (GetGroup() != nullptr)
    {
        GetGroup()->HandleMateDestroyed(static_cast<uint32_t>(mover->PartId));
    }

    if (Radio != nullptr)
    {
        Radio->Enabled = 0;
    }

    return 1;
}

auto MCMechWarrior::Eject() -> void
{
    if (Status != 0 && Status != 1)
    {
        return;
    }

    if (Wounds < 6.0f)
    {
        Wounds = Wounds + 1.0f;
    }

    if (Wounds < 6.0f)
    {
        RadioMessage(RADIO_EJECTING, 0);
        Status = 3;
    }
    else
    {
        RadioMessage(RADIO_DEATH, 0);
        Status = 4;
    }

    MCMover* mover = static_cast<MCMover*>(Vehicle);

    if (mover != nullptr)
    {
        mover->Disable(3);
    }

    if (GetGroup() != nullptr)
    {
        GetGroup()->HandleMateEjected(static_cast<uint32_t>(mover->PartId));
    }

    if (Radio != nullptr)
    {
        Radio->Enabled = 0;
    }
}

auto MCMechWarrior::SetTeam(MCTeam* newTeam) -> void
{
    Team = newTeam;
    Alignment = static_cast<int8_t>(newTeam->Alignment);
}

auto MCMechWarrior::SetVehicle(MCGameObject* newVehicle) -> void
{
    const MCObjectClass objectClass = newVehicle->ObjectClass;

    if (objectClass != MCObjectClass::BattleMech && objectClass != MCObjectClass::GroundVehicle &&
        objectClass != MCObjectClass::Elemental && objectClass != MCObjectClass::Mover)
    {
        Fatal(0, " bad vehicle type ");
    }

    Vehicle = newVehicle;

    if (Radio != nullptr)
    {
        Radio->Owner = this;
    }
}

auto MCMechWarrior::SetBrainName(char* brainName) -> void
{
    BrainStr = CopyString(brainName);
}

auto MCMechWarrior::SetBrain(int32_t brainHandle) -> int32_t
{
    if (Brain != nullptr)
    {
        Brain.reset();

        for (int32_t i = 0; i < NUM_PILOT_ALARMS; i++)
        {
            BrainAlarmCallback[i] = nullptr;
        }
    }

    if (brainHandle < 0)
    {
        return 0;
    }

    Brain = std::make_unique<MCAblModule>(brainHandle);
    Brain->SetName(std::format("Pilot {}", Name != nullptr ? Name : "(null)"));

    for (int32_t i = 0; i < NUM_PILOT_ALARMS; i++)
    {
        BrainAlarmCallback[i] = Brain->FindFunction(PilotAlarmFunctionName[i], true);
    }

    return 0;
}

auto MCMechWarrior::RunBrain() -> int32_t
{
    if (Brain == nullptr)
    {
        return 0;
    }

    MCAblBrainScope brain(GetGroup(), Vehicle, static_cast<int32_t>(Vehicle->ObjectClass), this);
    Brain->Execute();
    return Brain->ReturnValue();
}

auto MCMechWarrior::GetVehicleStatus() -> int32_t
{
    if (Vehicle != nullptr)
    {
        return static_cast<uint8_t>(Vehicle->Status);
    }

    return -1;
}

auto MCMechWarrior::UpdateAttackerStatus(uint32_t attackerId, float time) -> void
{
    int32_t index = 0;

    while (index < NumAttackers && Attackers[index].AttackerId != attackerId)
    {
        index++;
    }

    if (index == NumAttackers)
    {
        if (NumAttackers == MAX_ATTACKERS)
        {
            return;
        }

        Attackers[NumAttackers].AttackerId = attackerId;
        NumAttackers++;
    }

    Attackers[index].LastTime = time;
}

auto MCMechWarrior::GetAttackerInfo(uint32_t attackerId) -> MCAttackerRec*
{
    for (int32_t i = 0; i < NumAttackers; i++)
    {
        if (Attackers[i].AttackerId == attackerId)
        {
            return &Attackers[i];
        }
    }

    return nullptr;
}

auto MCMechWarrior::GetAttackers(uint32_t* attackerList, float seconds) -> int32_t
{
    const float since = ScenarioTime - seconds;
    int32_t count = 0;

    for (int32_t i = 0; i < NumAttackers; i++)
    {
        if (since <= Attackers[i].LastTime)
        {
            attackerList[count++] = Attackers[i].AttackerId;
        }
    }

    return count;
}

auto MCMechWarrior::SetAttackTarget(MCGameObject* object) -> int32_t
{
    AttackOrders.Target = object;
    AttackOrders.TargetTime = ScenarioTime;
    return 0;
}

auto MCMechWarrior::GetLastTarget() -> MCGameObject*
{
    MCGameObject* target = LastTarget;

    if (target == nullptr)
    {
        return nullptr;
    }

    if (target->IsDestroyed() == 0 && (target->IsDisabled() == 0 || LastTargetObliterate != 0) &&
        (target->GetAlignment() != Alignment || LastTargetFriendly != 0))
    {
        if (LastTargetConserveAmmo != 0)
        {
            CurTacOrder.AttackParams.Type = 3;
        }

        return target;
    }

    // Dead, disabled or friendly: forget it (and an attack order on it).
    SetLastTarget(nullptr, 0, 0);
    LastTargetTime = -1.0f;
    LastTargetObliterate = 0;
    LastTargetFriendly = 0;

    if (CurTacOrder.IsCombatOrder() != 0)
    {
        ClearCurTacOrder(1, 0);
    }

    return nullptr;
}

auto MCMechWarrior::SetLastTarget(MCGameObject* target, int obliterate, int conserveAmmo) -> void
{
    if (Vehicle != nullptr && static_cast<MCMover*>(Vehicle)->NetPlayerId > -1)
    {
        if (LastTarget != nullptr && LastTarget->GetObjectType() != nullptr)
        {
            LastTarget->DecrementAttackers();
        }

        if (target != nullptr)
        {
            target->IncrementAttackers();
        }
    }

    LastTarget = target;

    if (target == nullptr)
    {
        LastTargetFriendly = 0;
    }
    else
    {
        LastTargetFriendly = target->GetAlignment() == Alignment ? 1 : 0;
    }

    LastTargetTime = ScenarioTime;
    LastTargetObliterate = obliterate;
    LastTargetConserveAmmo = conserveAmmo;
}

auto MCMechWarrior::SetCurrentTarget(MCGameObject* target) -> void
{
    SetLastTarget(target, 0, 0);
}

auto MCMechWarrior::GetAttackTargetPosition(MCVector3D& pos) -> MCGameObject*
{
    MCGameObject* target = AttackOrders.Target;

    if (target == nullptr)
    {
        ClearAttackOrders();
        return nullptr;
    }

    pos = target->GetPosition();
    return target;
}

auto MCMechWarrior::ClearAttackOrders() -> void
{
    AttackOrders.Origin = 1;
    AttackOrders.Type = 0;
    AttackOrders.Target = nullptr;
    AttackOrders.AimLocation = -1;
    AttackOrders.Pursue = 0;
    AttackOrders.TargetTime = -1.0f;
}

auto MCMechWarrior::ClearMoveOrders() -> void
{
    SetMoveGoal(0xffffffff, nullptr, nullptr);
    SetMoveWayPath(nullptr, 0);

    for (int32_t i = 0; i < 2; i++)
    {
        if (MoveOrders.Path[i] != nullptr)
        {
            MoveOrders.Path[i]->Clear();
        }
    }

    MoveOrders.MoveState = 1;
    MoveOrders.MoveStateGoal = 1;
    MoveOrders.YieldState = 0;
    MoveOrders.MoveStateGoalChanged = 0;
    MoveOrders.YieldTime = -1.0f;
    MoveOrders.WaitForPointTime = -1.0f;
    MoveOrders.TimeOfLastStep = -1.0f;
    SetMoveGlobalPath(nullptr, 0);
    PathManager()->Remove(this);
}

auto MCMechWarrior::SetMoveGoal(uint32_t type, MCVector3D* location, MCGameObject* obj) -> int32_t
{
    MoveOrders.GoalType = static_cast<int32_t>(type);

    if (type == 0)
    {
        if (static_cast<double>(location->Z) < -10.0)
        {
            location->Z = Terrain()->GetTerrainElevation(*location);
        }

        MoveOrders.GoalLocation = *location;
        MoveOrders.GoalObject = nullptr;
        return 0;
    }

    if (type != 0xffffffff)
    {
        if (static_cast<double>(location->Z) < -10.0)
        {
            location->Z = Terrain()->GetTerrainElevation(*location);
        }

        MoveOrders.GoalLocation = *location;

        if (obj == nullptr)
        {
            obj = static_cast<MCGameObject*>(ObjectList()->FindObjectFromPart(static_cast<int32_t>(type)));
        }

        MoveOrders.GoalObject = obj;
        return 0;
    }

    MoveOrders.Origin = 1;
    MoveOrders.GoalType = -1;
    MoveOrders.GoalObject = nullptr;
    MoveOrders.GoalLocation = MCVector3D(-999999.0f, -999999.0f, -999999.0f);
    return 0;
}

auto MCMechWarrior::PausePath() -> void
{
    if (MoveOrders.Path[0] != nullptr)
    {
        MoveOrders.Path[0]->NumSteps = 0;
    }
}

auto MCMechWarrior::ResumePath() -> void
{
    MCMovePath* path = MoveOrders.Path[0];

    if (path != nullptr)
    {
        path->NumSteps = path->NumStepsWhenNotPaused;
    }
}

auto MCMechWarrior::ReachedPathEnd() -> void
{
    MCVector3D nextPoint;
    const int haveNextPoint = GetNextWayPoint(nextPoint, 0);

    if (MoveOrders.PathType == 1)
    {
        if (haveNextPoint != 0)
        {
            const int32_t selectionIndex = CurTacOrder.SelectionIndex;
            MoveOrders.Path[0]->NumSteps = 0;
            RequestMovePath(selectionIndex, 0x281, 1);
            return;
        }
    }
    else
    {
        if (MoveOrders.PathType != 2)
        {
            return;
        }

        if (MoveOrders.Path[0]->GlobalStep != MoveOrders.NumGlobalSteps - 1)
        {
            // On to the next leg of the global path.
            const int32_t selectionIndex = CurTacOrder.SelectionIndex;
            MoveOrders.Path[0]->NumSteps = 0;
            RequestMovePath(selectionIndex, 0x281, 2);
            return;
        }

        if (haveNextPoint != 0)
        {
            return;
        }
    }

    ClearMoveOrders();

    if (CurTacOrder.IsMoveOrder() != 0 || CurTacOrder.IsWayPathOrder() != 0)
    {
        ClearCurTacOrder(1, 0);
    }

    TriggerAlarm(PILOT_ALARM_NO_MOVEPATH, static_cast<uint32_t>(-9));
}

auto MCMechWarrior::GetMoveDistanceLeft() -> float
{
    MCMovePath* path = MoveOrders.Path[0];
    float distance = 0.0f;

    if (path != nullptr && path->NumStepsWhenNotPaused > 0)
    {
        distance = path->GetDistanceLeft(GetVehicle()->GetPosition(), -1);

        if (MoveOrders.PathType == 2)
        {
            distance = distance + static_cast<float>(MoveOrders.GlobalPath[path->GlobalStep].CostToGoal);
        }
    }

    return distance;
}

auto MCMechWarrior::IsJumping(MCVector3D* jumpGoal) -> int
{
    if (Vehicle != nullptr)
    {
        return static_cast<MCMover*>(Vehicle)->IsJumping(jumpGoal);
    }

    return 0;
}

auto MCMechWarrior::GetMovePath() -> MCMovePath*
{
    MCMovePath* donePath = MoveOrders.Path[0];
    Assert(MoveOrders.Path[0] != nullptr && MoveOrders.Path[1] != nullptr, 0, " NULL move paths ");

    if (donePath->NumStepsWhenNotPaused != 0)
    {
        return MoveOrders.Path[0];
    }

    // The path walked is done: the next leg (if planned) becomes the current one.
    if (MoveOrders.Path[0] != nullptr)
    {
        MoveOrders.Path[0]->Clear();
    }

    MCMovePath* path = MoveOrders.Path[1];
    MoveOrders.Path[1] = donePath;
    MoveOrders.Path[0] = path;

    if (path->NumStepsWhenNotPaused <= 0)
    {
        return MoveOrders.Path[0];
    }

    MCGameObject* goalObject = MoveOrders.GoalObject;
    const int32_t goalType = MoveOrders.GoalType;

    if (goalType == -1)
    {
        path->NumStepsWhenNotPaused = 0;
        return path;
    }

    MCBaseObject* goal = nullptr;

    if (goalType != 0)
    {
        goal = goalObject;

        if (goal == nullptr)
        {
            goal = ObjectList()->FindObjectFromPart(goalType);
        }

        if (goal == nullptr)
        {
            path->NumStepsWhenNotPaused = 0;
            return path;
        }

        path->Target = static_cast<MCGameObject*>(goal)->GetPosition();
    }

    if (static_cast<double>(MoveOrders.YieldTime) <= -1.0)
    {
        if (static_cast<double>(MoveOrders.WaitForPointTime) <= -1.0)
        {
            MoveOrders.YieldTime = -1.0f;
            MoveOrders.YieldState = 0;
        }
        else
        {
            path->NumSteps = 0;
        }
    }
    else
    {
        path->NumSteps = 0;
        MoveOrders.YieldTime = static_cast<float>(static_cast<double>(ScenarioTime) + 1.5);
    }

    SetMoveGoal(goal != nullptr ? static_cast<uint32_t>(goal->PartId) : 0, &path->Goal, nullptr);
    MCMovePath* curPath = MoveOrders.Path[0];

    if (curPath->GlobalStep == MoveOrders.NumGlobalSteps - 1)
    {
        MoveOrders.GlobalGoalLocation = curPath->StepList[curPath->NumStepsWhenNotPaused - 1].Destination;
        CurTacOrder.SetWayPoint(0, MoveOrders.GlobalGoalLocation);
    }

    return MoveOrders.Path[0];
}

auto MCMechWarrior::SetMoveWayPath(MCWayPath* wayPath, int patrol) -> void
{
    if (wayPath == nullptr)
    {
        MoveOrders.NumWayPts = 0;
    }
    else
    {
        for (int32_t i = 0; i < wayPath->NumPoints; i++)
        {
            MoveOrders.WayPath[i] =
                MCVector3D(wayPath->Points[i * 3], wayPath->Points[i * 3 + 1], wayPath->Points[i * 3 + 2]);
        }

        MoveOrders.NumWayPts = static_cast<int8_t>(wayPath->NumPoints);
    }

    if (InitWayPath != 0)
    {
        MoveOrders.CurWayPt = 0;
        MoveOrders.CurWayDir = patrol != 0 ? 1 : 0;
    }
}

auto MCMechWarrior::AddMoveWayPoint(MCVector3D wayPt, int patrol) -> void
{
    MoveOrders.WayPath[MoveOrders.NumWayPts] = wayPt;
    MoveOrders.NumWayPts++;

    if (MoveOrders.NumWayPts == 1)
    {
        MoveOrders.CurWayDir = patrol != 0 ? 1 : 0;
    }
}

auto MCMechWarrior::SetMoveGlobalPath(MCGlobalPathStep* path, int32_t numSteps) -> void
{
    if (numSteps > MCGlobalMap::MaxPathSteps)
    {
        Fatal(0, " Global Path Too Long ");
    }

    if (numSteps > 0)
    {
        std::memcpy(MoveOrders.GlobalPath, path, static_cast<size_t>(numSteps) * sizeof(MCGlobalPathStep));
    }

    MoveOrders.NumGlobalSteps = static_cast<int8_t>(numSteps);
    MoveOrders.CurGlobalStep = 0;
}

auto MCMechWarrior::RequestMovePath(int32_t selectionIndex, uint32_t moveParams, int32_t source) -> void
{
    PathManager()->Request(this, selectionIndex, moveParams, 255.0f, source);
}

auto MCMechWarrior::CalcMovePath(int32_t selectionIndex, uint32_t moveParams, int32_t source) -> int32_t
{
    MCMover* mover = static_cast<MCMover*>(Vehicle);

    // Start where the vehicle stands (its last valid position when on a blocked cell), or where it lands.
    MCVector3D start;
    MCVector3D jumpGoal;

    if (IsJumping(&jumpGoal) == 0)
    {
        // The original tests for a queued jump order here, but both branches read the same position.
        start = mover->GetPosition();
        const MCObjectPosition* position = mover->GetObjPosition();

        if (!CellPassable(position->TileR, position->TileC, position->CellR, position->CellC))
        {
            start = mover->LastValidPosition;
        }
    }
    else
    {
        start = jumpGoal;
    }

    int32_t startTileR;
    int32_t startTileC;
    int32_t startCellR;
    int32_t startCellC;
    GameMap()->WorldToMapPos(start, startTileR, startTileC, startCellR, startCellC);
    const int32_t startArea = GlobalMoveMap()->CalcArea(startTileR, startTileC);

    const uint32_t escapeTile = (moveParams >> 13) & 1;
    MCGameObject* goalObject = MoveOrders.GoalObject;
    MCVector3D goal = MoveOrders.GoalLocation;

    if (MoveOrders.GoalType == -1)
    {
        LastMoveCalcErr = -1;
        TriggerAlarm(PILOT_ALARM_NO_MOVEPATH, static_cast<uint32_t>(-1));
        return LastMoveCalcErr;
    }

    MCGameObject* goalObj = nullptr;

    if (MoveOrders.GoalType != 0)
    {
        if (goalObject == nullptr)
        {
            goalObject = static_cast<MCGameObject*>(ObjectList()->FindObjectFromPart(MoveOrders.GoalType));
        }

        goalObj = goalObject;

        if (goalObj == nullptr)
        {
            LastMoveCalcErr = -2;
            TriggerAlarm(PILOT_ALARM_NO_MOVEPATH, static_cast<uint32_t>(-2));
            return LastMoveCalcErr;
        }
    }

    // Which of the two paths to plan: the current one (0) or, while one is walked, the next leg (1).
    int32_t pathNum = -1;

    if ((moveParams & 0x100) != 0)
    {
        MoveOrders.OriginalGlobalGoal[0] = goal;
        MoveOrders.PathType = 0;
        MoveOrders.NumGlobalSteps = 0;
        const int32_t stateGoal = MoveOrders.MoveStateGoal;

        if (stateGoal == 3 || stateGoal == 4 || stateGoal == 5)
        {
            MoveOrders.MoveState = 1;
            MoveOrders.MoveStateGoal = 1;
        }

        pathNum = 0;
    }

    const bool yielding = static_cast<double>(MoveOrders.YieldTime) > -1.0;

    if ((moveParams & 0x200) != 0)
    {
        // Start over toward the original goal.
        if (goalObj == nullptr)
        {
            SetMoveGoal(0, &MoveOrders.OriginalGlobalGoal[0], nullptr);
        }
        else
        {
            MCVector3D goalPosition = goalObj->GetPosition();
            SetMoveGoal(static_cast<uint32_t>(goalObj->PartId), &goalPosition, goalObj);
        }

        goal = MoveOrders.GoalLocation;

        for (int32_t i = 0; i < 2; i++)
        {
            if (MoveOrders.Path[i] != nullptr)
            {
                MoveOrders.Path[i]->Clear();
            }
        }

        MoveOrders.PathType = 0;
        MoveOrders.NumGlobalSteps = 0;
        MoveOrders.MoveState = 1;
        MoveOrders.MoveStateGoal = 1;
        pathNum = 0;
    }

    if (static_cast<double>(goal.X) < -666000.0)
    {
        LastMoveCalcErr = 0;
        return 0;
    }

    if (pathNum == -1)
    {
        pathNum = MoveOrders.Path[0]->NumStepsWhenNotPaused != 0 ? 1 : 0;
    }

    int32_t numSteps = 0;
    enum class Next
    {
        GlobalLeg,
        GlobalPath,
        TrimFailed,
        TooClose,
    };

    Next next;

    if (MoveOrders.PathType != 0)
    {
        if (MoveOrders.PathType == 2)
        {
            MoveOrders.CurGlobalStep++;
        }

        next = Next::GlobalLeg;
    }
    else
    {
        if (escapeTile == 0)
        {
            if (mover->NetPlayerId > -1 && CurTacOrder.Code != MCTacticalOrderCode::None &&
                CurTacOrder.Origin == MCOrderOrigin::Player)
            {
                moveParams |= 0x800;
            }

            if (mover->CalcMoveGoal(goalObj, goal, 6, 6, 6, selectionIndex, goal, moveParams) != 0)
            {
                LastMoveCalcErr = -3;
                TriggerAlarm(PILOT_ALARM_NO_MOVEPATH, static_cast<uint32_t>(-3));
                return LastMoveCalcErr;
            }
        }

        MoveOrders.OriginalGlobalGoal[1] = goal;

        if (escapeTile != 0)
        {
            // Escape from a blocked cell: the nearest open cell toward the goal.
            Assert(pathNum == 0, static_cast<uint32_t>(pathNum),
                   " Warrior.calcMovePath: escapePath should be pathNum 0 ");
            MoveOrders.PathType = 1;
            BeginPathCalc(this, mover);
            MCVector3D escapeGoal;
            MCMovePath* path = MoveOrders.Path[pathNum];
            numSteps =
                mover->CalcEscapePath(path, start, goal, nullptr, PathFinderParams(mover, moveParams), escapeGoal);
            EndPathCalc(mover);

            if (numSteps < 1)
            {
                LastMoveCalcErr = -5;
                TriggerAlarm(PILOT_ALARM_NO_MOVEPATH, static_cast<uint32_t>(-5));
                return LastMoveCalcErr;
            }

            path->NumSteps = numSteps;
            path->NumStepsWhenNotPaused = numSteps;
            MoveOrders.GlobalGoalLocation = path->StepList[numSteps - 1].Destination;
            CurTacOrder.SetWayPoint(0, MoveOrders.GlobalGoalLocation);
            uint32_t goalId = 0;

            if (goalObj != nullptr)
            {
                MoveOrders.Path[pathNum]->Target = goalObj->GetPosition();
                goalId = static_cast<uint32_t>(goalObj->PartId);
            }

            SetMoveGoal(goalId, &goal, nullptr);
            MoveOrders.NextUpdate = MovementUpdateFrequency + ScenarioTime;

            if (pathNum == 0)
            {
                if (yielding)
                {
                    LastMoveCalcErr = 0;
                    MoveOrders.YieldTime = static_cast<float>(static_cast<double>(ScenarioTime) + 1.5);
                    MoveOrders.Path[0]->NumSteps = 0;
                    return 0;
                }

                if (static_cast<double>(MoveOrders.WaitForPointTime) > -1.0)
                {
                    LastMoveCalcErr = 0;
                    MoveOrders.Path[0]->NumSteps = 0;
                    return 0;
                }

                MoveOrders.YieldTime = -1.0f;
                MoveOrders.YieldState = 0;
            }

            LastMoveCalcErr = 0;
            return 0;
        }

        if (mover->DistanceFrom(goal) < MoveMarginOfError[1] && (moveParams & 1) == 0)
        {
            // Already there.
            MoveOrders.Origin = 1;
            MoveOrders.GoalType = -1;
            MoveOrders.GoalObject = nullptr;
            MoveOrders.GoalLocation = MCVector3D(-999999.0f, -999999.0f, -999999.0f);
            LastMoveCalcErr = -4;
            TriggerAlarm(PILOT_ALARM_NO_MOVEPATH, static_cast<uint32_t>(-4));
            return LastMoveCalcErr;
        }

        int32_t goalTileR;
        int32_t goalTileC;
        int32_t goalCellR;
        int32_t goalCellC;
        GameMap()->WorldToMapPos(goal, goalTileR, goalTileC, goalCellR, goalCellC);
        bool simple = std::abs(goalTileR - startTileR) <= SimpleMovePathRange &&
                      std::abs(goalTileC - startTileC) <= SimpleMovePathRange;
        const int32_t longRange = LongRangeMovementEnabled[Team->Id];
        const bool startAreaOpen = startArea >= 0 && GlobalMoveMap()->Areas[startArea].Closed == 0;

        next = Next::GlobalPath;
        bool planLocal = simple;

        if (!simple && longRange == 0)
        {
            // Without long range movement, head SimpleMovePathRange tiles toward the goal.
            const float facing = mover->RelFacingTo(goal, -1);
            const float range = static_cast<float>(static_cast<double>(SimpleMovePathRange) * MetersPerWorldUnit *
                                                   MCTerrain::MetersPerVertex);
            goal = mover->RelativePosition(-facing, range, 2);
            MoveOrders.OriginalGlobalGoal[1] = goal;
            GameMap()->WorldToMapPos(goal, goalTileR, goalTileC, goalCellR, goalCellC);
            simple = true;
            planLocal = true;
        }

        if (planLocal)
        {
            MoveOrders.PathType = 1;
            BeginPathCalc(this, mover);
            numSteps = mover->CalcMovePath(MoveOrders.Path[pathNum], 1, start, goal, nullptr,
                                           PathFinderParams(mover, moveParams));
            EndPathCalc(mover);

            if (numSteps < 1)
            {
                if (startArea == -1)
                {
                    next = Next::GlobalPath; // Handled below: the alarm order to walk out.
                }
                else if (longRange == 0)
                {
                    LastMoveCalcErr = -5;
                    TriggerAlarm(PILOT_ALARM_NO_MOVEPATH, static_cast<uint32_t>(-5));
                    return LastMoveCalcErr;
                }
                else
                {
                    next = Next::GlobalPath;
                }
            }
            else if (selectionIndex >= 1 &&
                     (numSteps -= (selectionIndex / GroupMoveTrailLen[1]) * GroupMoveTrailLen[0]) <= 0)
            {
                next = Next::TrimFailed;
            }
            else
            {
                MCMovePath* path = MoveOrders.Path[pathNum];
                MoveOrders.GlobalGoalLocation = path->StepList[numSteps - 1].Destination;
                path->NumSteps = numSteps;
                path->NumStepsWhenNotPaused = numSteps;
                CurTacOrder.SetWayPoint(0, MoveOrders.GlobalGoalLocation);
                uint32_t goalId = 0;

                if (goalObj != nullptr)
                {
                    MoveOrders.Path[pathNum]->Target = goalObj->GetPosition();
                    goalId = static_cast<uint32_t>(goalObj->PartId);
                }

                SetMoveGoal(goalId, &goal, nullptr);
                MoveOrders.NextUpdate = MovementUpdateFrequency + ScenarioTime;
                next = simple ? Next::GlobalLeg : Next::GlobalPath;
            }
        }

        if (next == Next::GlobalPath)
        {
            if (planLocal && numSteps < 1 && startArea == -1)
            {
                // No local path out of an area-less tile: first walk out as an alarm order.
                Assert(pathNum == 0 || pathNum == 1, static_cast<uint32_t>(pathNum),
                       " Warrior.calcMovePath: pathNum should be 0 or 1 in Line 2117 ");
                MCTacticalOrder alarmOrder;
                alarmOrder.Reset();
                alarmOrder.Reset(MCOrderOrigin::Self, MCTacticalOrderCode::MoveToPoint, 0);
                alarmOrder.SetWayPoint(0, goal);
                alarmOrder.MoveParams.WayPath.Mode[0] = MoveOrders.Run != 0 ? 1 : 0;
                alarmOrder.MoveParams.EscapeTile = 1;
                alarmOrder.MoveParams.Wait = 0;
                SetAlarmTacOrder(alarmOrder, 255);
                LastMoveCalcErr = -13;
                TriggerAlarm(PILOT_ALARM_NO_MOVEPATH, static_cast<uint32_t>(-13));
                return LastMoveCalcErr;
            }

            // A global path: area by area through the doors.
            MoveOrders.GlobalGoalLocation = goal;
            CurTacOrder.SetWayPoint(0, MoveOrders.GlobalGoalLocation);
            const int32_t goalArea = GlobalMoveMap()->CalcArea(goalTileR, goalTileC);
            int32_t numGlobalSteps = -1;

            if (startAreaOpen)
            {
                numGlobalSteps = GlobalMoveMap()->CalcPath(startArea, goalArea, MoveOrders.GlobalPath);
            }

            if (numGlobalSteps == -1)
            {
                Assert(pathNum == 0 || pathNum == 1, static_cast<uint32_t>(pathNum),
                       " Warrior.calcMovePath: pathNum should be 0 or 1 in Line 2157 ");
                MCTacticalOrder alarmOrder;
                alarmOrder.Reset();
                alarmOrder.Reset(MCOrderOrigin::Self, MCTacticalOrderCode::MoveToPoint, 0);
                alarmOrder.SetWayPoint(0, goal);
                alarmOrder.MoveParams.WayPath.Mode[0] = MoveOrders.Run != 0 ? 1 : 0;
                alarmOrder.MoveParams.EscapeTile = 1;
                alarmOrder.MoveParams.Wait = 0;
                SetAlarmTacOrder(alarmOrder, 255);
                LastMoveCalcErr = -13;
                TriggerAlarm(PILOT_ALARM_NO_MOVEPATH, static_cast<uint32_t>(-13));
                return LastMoveCalcErr;
            }

            if (numGlobalSteps == 0)
            {
                ClearMoveOrders();
                LastMoveCalcErr = -7;
                TriggerAlarm(PILOT_ALARM_NO_MOVEPATH, static_cast<uint32_t>(-7));

                if ((moveParams & 0x1000) != 0)
                {
                    RadioMessage(RADIO_MOVE_BLOCKED, 1);
                }

                return LastMoveCalcErr;
            }

            MoveOrders.PathType = 2;
            MoveOrders.NumGlobalSteps = static_cast<int8_t>(numGlobalSteps);
            MoveOrders.CurGlobalStep = 0;
            next = Next::GlobalLeg;
        }
    }

    if (next == Next::GlobalLeg)
    {
        const int8_t pathType = MoveOrders.PathType;

        if (pathType != 2)
        {
            if (pathType != 1 && pathType != 0)
            {
                Fatal(0, " Bad Move Path Type ");
            }

            if (pathNum != 0)
            {
                LastMoveCalcErr = 0;
                return 0;
            }
        }
        else
        {
            // Plan the leg of the global path at curGlobalStep.
            const int32_t step = MoveOrders.CurGlobalStep;

            if (step == MoveOrders.NumGlobalSteps)
            {
                LastMoveCalcErr = 0;
                return 0;
            }

            if (step != 0)
            {
                MCGlobalPathStep prevStep = MoveOrders.GlobalPath[step - 1];
                start = GlobalMoveMap()->GetDoorWorldPos(prevStep.GoalCell);
            }

            const int32_t lastStep = MoveOrders.NumGlobalSteps - 1;
            MCGlobalPathStep* curStep = &MoveOrders.GlobalPath[step];

            if (step < lastStep && GlobalMoveMap()->Doors[curStep->GoalDoor].Open == 0)
            {
                // The door out is shut: plan again from the start.
                LastMoveCalcErr = -11;
                SetMoveWayPath(nullptr, 0);
                MoveOrders.TimeOfLastStep = ScenarioTime;
                SetMoveGlobalPath(nullptr, 0);
                PathManager()->Request(this, selectionIndex, 0x201, 255.0f, source);
                TriggerAlarm(PILOT_ALARM_NO_MOVEPATH, static_cast<uint32_t>(LastMoveCalcErr));
                return LastMoveCalcErr;
            }

            if (MoveOrders.Path[pathNum] != nullptr)
            {
                MoveOrders.Path[pathNum]->Clear();
            }

            bool trimFailed = false;

            if (step < lastStep)
            {
                BeginPathCalc(this, mover);
                numSteps = mover->CalcMovePath(MoveOrders.Path[pathNum], start, curStep->ThruArea, curStep->GoalDoor,
                                               MoveOrders.GlobalGoalLocation, &goal, curStep->GoalCell,
                                               PathFinderParams(mover, moveParams));
                EndPathCalc(mover);
            }
            else
            {
                goal = MoveOrders.OriginalGlobalGoal[1];
                BeginPathCalc(this, mover);
                numSteps = mover->CalcMovePath(MoveOrders.Path[pathNum], 2, start, goal, curStep->GoalCell,
                                               PathFinderParams(mover, moveParams));
                EndPathCalc(mover);

                if (numSteps >= 1 && selectionIndex > 0)
                {
                    numSteps -= (selectionIndex / GroupMoveTrailLen[1]) * GroupMoveTrailLen[0];

                    if (numSteps < 1)
                    {
                        trimFailed = true;
                    }
                    else if (pathNum == 0)
                    {
                        MoveOrders.GlobalGoalLocation = MoveOrders.Path[0]->StepList[numSteps - 1].Destination;
                        CurTacOrder.SetWayPoint(0, MoveOrders.GlobalGoalLocation);
                    }
                }
            }

            if (trimFailed)
            {
                PathFindMap()->RamObject = nullptr;
                PathFindMap()->MovingObject = nullptr;
                ClearMoveOrders();
                LastMoveCalcErr = -4;
                TriggerAlarm(PILOT_ALARM_NO_MOVEPATH, static_cast<uint32_t>(-4));
                return LastMoveCalcErr;
            }

            if (numSteps < 1)
            {
                MoveOrders.CurGlobalStep--;
                LastMoveCalcErr = numSteps != -999 ? -8 : -12;
                TriggerAlarm(PILOT_ALARM_NO_MOVEPATH, static_cast<uint32_t>(LastMoveCalcErr));
                return LastMoveCalcErr;
            }

            MCMovePath* path = MoveOrders.Path[pathNum];
            path->NumSteps = numSteps;
            path->NumStepsWhenNotPaused = numSteps;
            path->GlobalStep = step;

            if (pathNum != 0)
            {
                LastMoveCalcErr = 0;
                return 0;
            }

            uint32_t goalId = 0;

            if (goalObj != nullptr)
            {
                MoveOrders.Path[0]->Target = goalObj->GetPosition();
                goalId = static_cast<uint32_t>(goalObj->PartId);
            }

            SetMoveGoal(goalId, &goal, nullptr);
        }

        // The new path waits while the vehicle yields to another or waits for its point.
        if (!yielding)
        {
            if (static_cast<double>(MoveOrders.WaitForPointTime) <= -1.0)
            {
                MoveOrders.YieldTime = -1.0f;
                MoveOrders.YieldState = 0;
            }
            else
            {
                MoveOrders.Path[0]->NumSteps = 0;
            }
        }
        else
        {
            MoveOrders.YieldTime = static_cast<float>(static_cast<double>(ScenarioTime) + 1.5);
            MoveOrders.Path[0]->NumSteps = 0;
        }

        LastMoveCalcErr = 0;
        return 0;
    }

    // next == Next::TrimFailed: the group's trail cut the whole path.
    PathFindMap()->RamObject = nullptr;
    PathFindMap()->MovingObject = nullptr;
    ClearMoveOrders();
    LastMoveCalcErr = -4;
    TriggerAlarm(PILOT_ALARM_NO_MOVEPATH, static_cast<uint32_t>(-4));
    return LastMoveCalcErr;
}

auto MCMechWarrior::GetNextWayPoint(MCVector3D& nextPoint, int incWayPoint) -> int
{
    MCTacticalOrder order;
    order.Reset();

    if (PeekQueuedTacOrder(&order) == 0 && order.Code == MCTacticalOrderCode::MoveToPoint)
    {
        nextPoint = order.GetWayPoint(0);
        return 1;
    }

    return 0;
}

auto MCMechWarrior::CalcWeaponsStatus(MCGameObject* target, int32_t* weaponList, MCVector3D* targetPoint) -> int32_t
{
    MCMover* mover = static_cast<MCMover*>(Vehicle);

    if (mover->CanFireWeapons() == 0)
    {
        return -1;
    }

    MCVector3D targetPosition;

    if (target == nullptr)
    {
        if (targetPoint == nullptr)
        {
            return -2;
        }

        targetPosition = *targetPoint;
    }
    else
    {
        targetPosition = target->GetPosition();
    }

    const auto distance = static_cast<float>(mover->DistanceFrom(targetPosition));

    if (mover->GetMaxFireRange() < distance)
    {
        return -3;
    }

    const int32_t aggressivenessModifier = (GetAggressiveness(1) - 50) / 5;
    int32_t numReady = 0;

    for (int32_t i = 0; i < mover->NumWeapons; i++)
    {
        const int32_t weaponIndex = mover->NumOther + i;

        if (mover->IsWeaponReady(weaponIndex) == 0)
        {
            weaponList[i] = -1;
        }
        else if (mover->GetWeaponShots(weaponIndex) < 1)
        {
            weaponList[i] = -2;
        }
        else if (mover->WeaponInRange(weaponIndex, distance) == 0)
        {
            weaponList[i] = -3;
        }
        else
        {
            const float lock = mover->WeaponLocked(weaponIndex, targetPosition);
            const float fireArc = mover->GetFireArc();

            if (lock < -fireArc || fireArc < lock)
            {
                weaponList[i] = -4;
            }
            else
            {
                const int32_t aimLocation =
                    CurTacOrder.IsCombatOrder() != 0 ? CurTacOrder.AttackParams.AimLocation : -1;
                const float attackChance =
                    mover->CalcAttackChance(target, aimLocation, ScenarioTime, weaponIndex, 0.0f, nullptr, targetPoint);
                const float ammoLevel = mover->GetWeaponAmmoLevel(weaponIndex);
                const int32_t chance = static_cast<int32_t>(attackChance);
                int32_t odds = chance + aggressivenessModifier;

                // Low ammo lowers the odds a pilot will fire at.
                if (ammoLevel < static_cast<double>(AmmoConservationModifiers[1][0]) * 0.01)
                {
                    odds += AmmoConservationModifiers[1][1];
                }
                else if (ammoLevel < static_cast<double>(AmmoConservationModifiers[0][0]) * 0.01)
                {
                    odds += AmmoConservationModifiers[0][1];
                }

                if (static_cast<double>(odds) > 0.0)
                {
                    numReady++;
                    weaponList[i] = chance;
                }
                else
                {
                    weaponList[i] = -5;
                }
            }
        }
    }

    return numReady;
}

auto MCMechWarrior::CombatDecisionTree() -> int32_t
{
    MCMover* mover = static_cast<MCMover*>(Vehicle);
    CombatUpdateTime = CombatUpdateFrequency + ScenarioTime;
    int32_t result = -1;
    Assert(mover != nullptr, 0, " Pilot has no vehicle! ");

    int outOfAmmo = 0;

    if (AmmoOutSent == 0 && mover->GetNumAmmoTypes() > 0)
    {
        int32_t ammoType = 0;

        do
        {
            if (mover->GetAmmoTypeTotal(ammoType) == 0)
            {
                outOfAmmo = 1;
                break;
            }

            ammoType++;
        } while (ammoType < mover->GetNumAmmoTypes());
    }

    MCGameObject* target = GetLastTarget();
    MCVector3D* targetPoint = nullptr;
    MCVector3D attackPoint;
    int32_t attackType = 1;
    int32_t aimLocation = -1;

    if (CurTacOrder.IsCombatOrder() == 0)
    {
        if (LastTargetConserveAmmo != 0)
        {
            attackType = 3;
        }
    }
    else
    {
        attackType = CurTacOrder.AttackParams.Type;
        aimLocation = CurTacOrder.AttackParams.AimLocation;

        if (CurTacOrder.Code == MCTacticalOrderCode::AttackPoint)
        {
            attackPoint = AttackOrders.TargetPoint;
            targetPoint = &attackPoint;
        }
    }

    char message[128];

    if (target != nullptr && CurTacOrder.IsCombatOrder() == 0)
    {
        // A target of the pilot's own choosing is dropped once it is out of his attack radius.
        MCVector3D targetPosition = target->GetPosition();

        if (AttackRadius < mover->DistanceFrom(targetPosition))
        {
            SetLastTarget(nullptr, 0, 0);
            target = nullptr;
        }
    }

    if (target == nullptr)
    {
        if (CurTacOrder.Code != MCTacticalOrderCode::AttackPoint)
        {
            if ((DebugFlags & 1) != 0)
            {
                std::snprintf(message, sizeof(message), "%s (%.2f) has no attack target.\n", Callsign,
                              static_cast<double>(OrderFireRange));
                DebugPrint(message, 1);
            }

            return -1;
        }
    }
    else
    {
        if (target->IsDestroyed() != 0)
        {
            if ((DebugFlags & 1) != 0)
            {
                std::snprintf(message, sizeof(message), "%s (%.2f) has a destroyed target.\n", Callsign,
                              static_cast<double>(OrderFireRange));
                DebugPrint(message, 1);
            }

            return -1;
        }

        if (target->IsDisabled() != 0 && LastTargetObliterate == 0)
        {
            if ((DebugFlags & 1) != 0)
            {
                std::snprintf(message, sizeof(message), "%s (%.2f) has a disabled target.\n", Callsign,
                              static_cast<double>(OrderFireRange));
                DebugPrint(message, 1);
            }

            return -1;
        }
    }

    if ((DebugFlags & 1) != 0)
    {
        char line[512];
        const double range = OrderFireRange;
        bool print = true;

        if (mover->CanFireWeapons() == 0)
        {
            std::snprintf(line, sizeof(line), "%s's (%.2f) vehicle cannot fire now.\n", Callsign, range);
        }
        else if (WeaponsStatusResult > 0)
        {
            print = false;
        }
        else
        {
            switch (WeaponsStatusResult)
            {
                case 0:
                {
                    int32_t notReady = 0;
                    int32_t noAmmo = 0;
                    int32_t notInRange = 0;
                    int32_t notLocked = 0;
                    int32_t noChance = 0;

                    for (int32_t i = 0; i < mover->NumWeapons; i++)
                    {
                        const int32_t weaponStatus = WeaponsStatus[i];

                        if (weaponStatus == -1)
                        {
                            notReady++;
                        }

                        if (weaponStatus == -2)
                        {
                            noAmmo++;
                        }

                        if (weaponStatus == -3)
                        {
                            notInRange++;
                        }

                        if (weaponStatus == -4)
                        {
                            notLocked++;
                        }

                        if (weaponStatus == -5)
                        {
                            noChance++;
                        }
                    }

                    // Port fix: the original passed no value for the last %d ("hot").
                    std::snprintf(
                        line, sizeof(line),
                        "%s (%.2f) has no shot: %d !ready, %d !ammo, %d !inrange, %d !locked, %d !chance, %d hot\n",
                        Callsign, range, notReady, noAmmo, notInRange, notLocked, noChance, 0);
                    break;
                }

                case -3:
                    std::snprintf(line, sizeof(line), "%s (%.2f) out of range.\n", Callsign, range);
                    break;
                case -2:
                    std::snprintf(line, sizeof(line), "%s (%.2f) has no target.\n", Callsign, range);
                    break;
                case -1:
                    std::snprintf(line, sizeof(line), "%s's (%.2f) vehicle cannot fire now.", Callsign, range);
                    break;
                default:
                    std::snprintf(line, sizeof(line), "%s (%.2f)  cannot fire for unknown reason.\n", Callsign, range);
                    break;
            }
        }

        if (print)
        {
            DebugPrint(line, 1);
        }
    }

    if (attackType != 3 && outOfAmmo != 0 && AmmoOutSent == 0)
    {
        RadioMessage(RADIO_AMMO_OUT, 1);
        AmmoOutSent = 1;
    }

    // Conserving ammo, only unlimited weapons fire; when none can, the attack is given up.
    int conserving = attackType == 3 ? 1 : 0;

    if (mover->CanFireWeapons() != 0 && WeaponsStatusResult > 0)
    {
        if (target != nullptr)
        {
            RadioMessage(RADIO_TAUNT, 1);
        }

        const float targetTime = LastTargetTime;

        for (int32_t i = 0; i < mover->NumWeapons; i++)
        {
            const int32_t weaponIndex = mover->NumOther + i;

            if (WeaponsStatus[i] > 0 && (attackType != 3 || mover->GetWeaponShots(weaponIndex) == 9999) &&
                mover->FireWeapon(target, targetTime, weaponIndex, attackType, aimLocation, targetPoint) == 0)
            {
                conserving = 0;
            }
        }

        result = 0;
    }

    if (conserving != 0)
    {
        for (int32_t weaponIndex = mover->NumOther; weaponIndex < mover->NumWeapons + mover->NumOther; weaponIndex++)
        {
            if (mover->GetWeaponShots(weaponIndex) == 9999 && mover->IsWeaponWorking(weaponIndex) != 0)
            {
                return result;
            }
        }

        RadioMessage(RADIO_ILLEGAL_ORDER, 0);
        SetLastTarget(nullptr, 0, 0);
        ClearCurTacOrder(1, 0);
    }

    return result;
}

auto VectorOffset(MCVector3D start, MCVector3D end, int32_t reverse) -> MCVector3D
{
    float dx;
    float dy;

    if (reverse == 0)
    {
        dx = end.X - start.X;
        dy = end.Y - start.Y;
    }
    else
    {
        dx = start.X - end.X;
        dy = start.Y - end.Y;
    }

    const float length = std::sqrt(dy * dy + dx * dx);

    if (length != 0.0f)
    {
        dx = dx / length;
        dy = dy / length;
    }

    const float stepLength = static_cast<float>(MCTerrain::MetersPerVertexDivMapcellDim * 0.5);
    dx = dx * stepLength;
    dy = dy * stepLength;

    if (std::sqrt(dy * dy + dx * dx) == 0.0f)
    {
        return start;
    }

    const auto totalDistance = static_cast<float>((start - end).Magnitude());
    const float originX = reverse == 0 ? start.X : end.X;
    const float originY = reverse == 0 ? start.Y : end.Y;
    float x = originX;
    float y = originY;
    float distance = 0.0f;

    // Step until the cell stepped from is open (the result lands one step past it) or the whole way is walked.
    int32_t tileR;
    int32_t tileC;
    int32_t cellR;
    int32_t cellC;
    GameMap()->WorldToMapPos(MCVector3D(x, y, 0.0f), tileR, tileC, cellR, cellC);
    bool open = CellPassable(tileR, tileC, cellR, cellC);

    while (!open && distance < totalDistance)
    {
        GameMap()->WorldToMapPos(MCVector3D(x, y, 0.0f), tileR, tileC, cellR, cellC);
        x = dx + x;
        y = dy + y;
        open = CellPassable(tileR, tileC, cellR, cellC);
        distance = std::sqrt((y - originY) * (y - originY) + (x - originX) * (x - originX));
    }

    const float elevation = GameMap()->GetTerrainElevation(MCVector3D(x, y, 0.0f));
    return MCVector3D(x, y, elevation);
}

auto MCMechWarrior::CalcWithdrawGoal(float withdrawRange) -> MCVector3D
{
    MCMover* mover = static_cast<MCMover*>(Vehicle);
    MCVector3D escapeVector;

    if (Team == InnerSphereTeam() || Team == AlliedTeam())
    {
        escapeVector = ClanTeam()->CalcEscapeVector(mover, withdrawRange);
    }
    else
    {
        escapeVector = InnerSphereTeam()->CalcEscapeVector(mover, withdrawRange);
    }

    Assert(mover != nullptr, 0, " Warrior has NULL Vehicle ");

    if (std::sqrt(escapeVector.Z * escapeVector.Z + escapeVector.Y * escapeVector.Y +
                  escapeVector.X * escapeVector.X) == 0.0f)
    {
        return mover->GetPosition();
    }

    // Walk out along the escape vector in half-cell steps.
    const float stepLength = static_cast<float>(MCTerrain::MetersPerVertexDivMapcellDim * 0.5);
    escapeVector.X = escapeVector.X * stepLength;
    escapeVector.Y = escapeVector.Y * stepLength;
    escapeVector.Z = escapeVector.Z * stepLength;
    MCVector3D goal = mover->GetPosition();

    auto withdrawDistance = [mover](const MCVector3D& point) -> double
    {
        const MCVector3D offset = point - mover->GetPosition();
        return std::sqrt((static_cast<double>(offset.Z) * offset.Z + static_cast<double>(offset.Y) * offset.Y) +
                         static_cast<double>(offset.X) * offset.X) *
               MetersPerWorldUnit;
    };

    double distance = static_cast<float>(withdrawDistance(goal));
    int32_t lastTileR;
    int32_t lastTileC;
    GameMap()->WorldToMapTilePos(goal, lastTileR, lastTileC);

    while (distance < withdrawRange)
    {
        int32_t tileR;
        int32_t tileC;
        GameMap()->WorldToMapTilePos(goal, tileR, tileC);

        if (tileR != lastTileR || tileC != lastTileC)
        {
            // Stop at the map's edge or at a tile with no open cell.
            if (tileR < 0 || tileR >= GameMap()->Height || tileC < 0 || tileC >= GameMap()->Width)
            {
                break;
            }

            Assert(tileR < GameMap()->Height && tileC < GameMap()->Width, 0, " Map Tile out of bounds ");

            if ((GameMap()->Map[GameMap()->Width * tileR + tileC].Cells & 0x55554000) == 0)
            {
                break;
            }

            lastTileR = tileR;
            lastTileC = tileC;
        }

        goal.X = goal.X + escapeVector.X;
        goal.Y = escapeVector.Y + goal.Y;
        goal.Z = escapeVector.Z + goal.Z;
        distance = withdrawDistance(goal);
    }

    return goal;
}

auto MCMechWarrior::MovingOverBlownBridge() -> int
{
    if (GetMovePath() == nullptr)
    {
        return 0;
    }

    const MCObjectPosition* position = static_cast<MCMover*>(Vehicle)->GetObjPosition();
    const int32_t tileR = position->TileR;
    const int32_t tileC = position->TileC;

    if (OverlayIsBridge[GameMap()->Map[GameMap()->Width * tileR + tileC].Overlay & 0x7f] != 0)
    {
        const int32_t area = GlobalMoveMap()->CalcArea(tileR, tileC);

        // Port fix: the original read the area table at -1 for a tile outside every area.
        if (area >= 0 && GlobalMoveMap()->Areas[area].Closed != 0)
        {
            return 1;
        }
    }

    const int32_t bridgeArea = GetMovePath()->CrossesBridge(-1, 3);

    if (bridgeArea >= 0 && GlobalMoveMap()->Areas[bridgeArea].Closed != 0)
    {
        return 1;
    }

    if (MoveOrders.PathType == 2)
    {
        for (int32_t step = MoveOrders.CurGlobalStep; step < MoveOrders.NumGlobalSteps; step++)
        {
            const MCGlobalMapArea& area = GlobalMoveMap()->Areas[MoveOrders.GlobalPath[step].ThruArea];

            if (area.Type != 1 && area.Type != 2)
            {
                return 0;
            }

            if (area.Closed != 0)
            {
                return 1;
            }
        }
    }

    return 0;
}

auto MCMechWarrior::MovementDecisionTree() -> int
{
    // A move that makes no progress for MoveTimeOut seconds is given up.
    if (static_cast<double>(MoveOrders.TimeOfLastStep) > -1.0 &&
        MoveOrders.TimeOfLastStep < static_cast<double>(ScenarioTime) - MoveTimeOut)
    {
        ClearMoveOrders();

        if ((CurTacOrder.IsMoveOrder() != 0 || CurTacOrder.IsWayPathOrder() != 0) &&
            CurTacOrder.Time < static_cast<double>(ScenarioTime) - MoveTimeOut)
        {
            RadioMessage(RADIO_MOVE_BLOCKED, 1);
            ClearCurTacOrder(1, 0);
        }

        TriggerAlarm(PILOT_ALARM_NO_MOVEPATH, static_cast<uint32_t>(-10));
    }

    MCMover* mover = static_cast<MCMover*>(Vehicle);

    // An elemental that can't jump drops a path through a closed gate.
    if (mover->ObjectClass == MCObjectClass::Elemental && static_cast<MCElemental*>(mover)->ElementalCanJump == 0 &&
        GetMovePath() != nullptr && GetMovePath()->NumSteps > 0 && GetMovePath()->CrossesClosedGate(-1, 2) > 0)
    {
        SetMoveWayPath(nullptr, 0);

        for (int32_t i = 0; i < 2; i++)
        {
            if (MoveOrders.Path[i] != nullptr)
            {
                MoveOrders.Path[i]->Clear();
            }
        }

        MoveOrders.MoveState = 1;
        MoveOrders.MoveStateGoal = 1;
        MoveOrders.YieldTime = -1.0f;
        MoveOrders.WaitForPointTime = -1.0f;
        MoveOrders.TimeOfLastStep = -1.0f;
        MoveOrders.YieldState = 0;
        MoveOrders.MoveStateGoalChanged = 0;
        SetMoveGlobalPath(nullptr, 0);
        PathManager()->Remove(this);
    }

    if (static_cast<double>(MoveOrders.YieldTime) > -1.0 && MoveOrders.YieldTime < ScenarioTime)
    {
        // Done yielding: plan again when on (or heading over) a blown bridge, and three times in four anyway.
        MoveOrders.YieldTime = MoveYieldTime + ScenarioTime;
        const MCObjectPosition* position = mover->GetObjPosition();
        const int32_t tileR = position->TileR;
        const int32_t tileC = position->TileC;
        bool replan;

        if (OverlayIsBridge[GameMap()->Map[GameMap()->Width * tileR + tileC].Overlay & 0x7f] != 0 &&
            GlobalMoveMap()->Areas[GlobalMoveMap()->CalcArea(tileR, tileC)].Closed != 0)
        {
            replan = true;
        }
        else
        {
            replan = RandomNumber(100) < 75;
        }

        bool request = replan;

        if (GetMovePath() != nullptr && GetMovePath()->CrossesBridge(-1, 3) >= 0 && !replan)
        {
            request = MovingOverBlownBridge() != 0;
        }

        if (request)
        {
            RequestMovePath(CurTacOrder.SelectionIndex, 0x201, 3);
        }
    }

    // Plan the next leg of a global path ahead of time.
    if (MoveOrders.PathType == 2 && MoveOrders.CurGlobalStep < MoveOrders.NumGlobalSteps - 1 &&
        MoveOrders.Path[1]->NumStepsWhenNotPaused == 0 && MovePathRequest == nullptr)
    {
        RequestMovePath(CurTacOrder.SelectionIndex, 1, 4);
    }

    if (ScenarioTime < MovementUpdateTime)
    {
        return 1;
    }

    const MCTacticalOrderCode code = CurTacOrder.Code;
    MovementUpdateTime = MovementUpdateFrequency + ScenarioTime;

    MCGameObject* target;

    if (code == MCTacticalOrderCode::None || code == MCTacticalOrderCode::Stop)
    {
        target = GetLastTarget();
    }
    else
    {
        target = MoveOrders.GoalObject;

        if (MoveOrders.GoalType != -1)
        {
            if (MoveOrders.GoalType == 0)
            {
                // Moving to a point: plan again once the path is walked.
                if (static_cast<double>(MoveOrders.YieldTime) > -1.0 || IsJumping(nullptr) != 0 ||
                    MoveOrders.MoveStateGoalChanged != 0 || static_cast<double>(MoveOrders.WaitForPointTime) > -1.0 ||
                    GetMovePath()->NumSteps != 0 || MovePathRequest != nullptr)
                {
                    return 1;
                }

                RequestMovePath(CurTacOrder.SelectionIndex, 0x101, 9);
                return 1;
            }

            // Moving to an object: follow it when it moves on.
            if (target == nullptr)
            {
                return 1;
            }

            const MCVector3D targetPosition = target->GetPosition();
            const double dx = static_cast<double>(targetPosition.X) - MoveOrders.GoalObjectPosition.X;
            const double dy = static_cast<double>(targetPosition.Y) - MoveOrders.GoalObjectPosition.Y;
            const float dz = targetPosition.Z - MoveOrders.GoalObjectPosition.Z;

            if (50.0 < std::sqrt((dx * dx + dy * dy) + static_cast<double>(dz) * dz))
            {
                MCVector3D goal = target->GetPosition();
                SetMoveGoal(static_cast<uint32_t>(target->PartId), &goal, target);
                RequestMovePath(CurTacOrder.SelectionIndex, 0x101, 10);
                return 1;
            }

            MCGameObject* lastTargetNow = GetLastTarget();
            mover->RelViewFacingTo(targetPosition);

            if (lastTargetNow == nullptr || lastTargetNow != target || AttackOrders.Pursue == 0)
            {
                return 1;
            }

            int wantMove = 0;
            uint32_t extraParams = 0;
            bool move = false;

            if (IsMover(lastTargetNow))
            {
                if (mover->LastOptimalRangeCalc < static_cast<MCMover*>(lastTargetNow)->LastWeaponEffectivenessCalc &&
                    mover->CalcOptimalRange(nullptr) != 0)
                {
                    wantMove = 1;
                    extraParams = 8;
                }
            }
            else if (GetMovePath()->NumSteps > 0)
            {
                return 1;
            }

            MCVector3D lastTargetPosition = lastTargetNow->GetPosition();
            const auto distance = static_cast<float>(mover->DistanceFrom(lastTargetPosition));
            const float fireRange = mover->GetFireRange(CurTacOrder.AttackParams.Range);
            const double slack = AttackRangeSlack();

            if (slack < static_cast<double>(distance) - fireRange ||
                static_cast<double>(distance) - fireRange < -slack || WeaponsStatusResult == -3)
            {
                extraParams = 8;
                move = true;
            }
            else if (WeaponsStatusResult >= 0)
            {
                int32_t notReady = 0;
                int32_t notLocked = 0;
                int32_t hot = 0;

                for (int32_t i = 0; i < mover->NumWeapons; i++)
                {
                    if (WeaponsStatus[i] == -1)
                    {
                        notReady++;
                    }

                    if (WeaponsStatus[i] == -4)
                    {
                        notLocked++;
                    }

                    if (WeaponsStatus[i] == -6)
                    {
                        hot++;
                    }
                }

                bool hold = false;

                switch (mover->ObjectClass)
                {
                    case MCObjectClass::BattleMech:
                    {
                        if (WeaponsStatusResult == 0)
                        {
                            if (notReady < 1 && notLocked < 1 && hot < 1)
                            {
                                extraParams = 8;
                                move = true;
                            }
                            else
                            {
                                hold = true;
                            }
                        }
                        break;
                    }
                    case MCObjectClass::GroundVehicle:
                    {
                        if (WeaponsStatusResult == 0 && notReady == 0)
                        {
                            if (notLocked < 1 && hot < 1)
                            {
                                extraParams = 0x400;
                                move = true;
                            }
                            else
                            {
                                hold = true;
                            }
                        }
                        break;
                    }
                    case MCObjectClass::Elemental:
                    {
                        if (WeaponsStatusResult == 0 && notReady == 0)
                        {
                            if (notLocked < 1 && hot < 1)
                            {
                                extraParams = 8;
                                move = true;
                            }
                            else
                            {
                                hold = true;
                            }
                        }
                        break;
                    }
                    default:
                        break;
                }

                if (hold && GetMovePath()->NumSteps == 0 && MoveOrders.MoveStateGoalChanged == 0)
                {
                    MoveOrders.MoveStateGoal = 5;
                }
            }

            if (!move && wantMove == 0)
            {
                return 1;
            }

            MoveOrders.MoveStateGoal = 1;
            MCVector3D goal = lastTargetNow->GetPosition();
            SetMoveGoal(static_cast<uint32_t>(lastTargetNow->PartId), &goal, lastTargetNow);
            RequestMovePath(CurTacOrder.SelectionIndex, extraParams | 0x101, 11);
            return 1;
        }

        if (code == MCTacticalOrderCode::Withdraw)
        {
            MCVector3D goal = CalcWithdrawGoal(1000.0f);
            SetMoveGoal(0, &goal, nullptr);
            RequestMovePath(CurTacOrder.SelectionIndex, 0x101, 5);
            return 1;
        }

        if (CurTacOrder.IsCombatOrder() != 0)
        {
            MCGameObject* attackTarget = GetLastTarget();
            MCVector3D targetPosition;

            if (attackTarget == nullptr)
            {
                if (CurTacOrder.Code != MCTacticalOrderCode::AttackPoint)
                {
                    return 1;
                }

                targetPosition = AttackOrders.TargetPoint;
            }
            else
            {
                targetPosition = attackTarget->GetPosition();
            }

            if (CurTacOrder.AttackParams.Method == 2)
            {
                // Ramming: head for the target itself.
                MCVector3D goal = attackTarget->GetPosition();
                SetMoveGoal(static_cast<uint32_t>(attackTarget->PartId), &goal, attackTarget);
                RequestMovePath(CurTacOrder.SelectionIndex, 0x101, 6);
                return 1;
            }

            if (AttackOrders.Pursue == 0)
            {
                MoveOrders.MoveStateGoal = 5;
                return 1;
            }

            int wantMove = 0;
            uint32_t extraParams = 0;
            bool move = false;

            if (attackTarget != nullptr && IsMover(attackTarget) &&
                mover->LastOptimalRangeCalc < static_cast<MCMover*>(attackTarget)->LastWeaponEffectivenessCalc &&
                mover->CalcOptimalRange(nullptr) != 0)
            {
                wantMove = 1;
                extraParams = 8;
            }

            const auto distance = static_cast<float>(mover->DistanceFrom(targetPosition));
            const float fireRange = mover->GetFireRange(CurTacOrder.AttackParams.Range);
            const double slack = AttackRangeSlack();

            if (slack < static_cast<double>(distance) - fireRange ||
                static_cast<double>(distance) - fireRange < -slack || WeaponsStatusResult == -3)
            {
                extraParams = 8;
                move = true;
            }
            else if (WeaponsStatusResult >= 0)
            {
                int32_t notReady = 0;
                int32_t notLocked = 0;
                int32_t hot = 0;

                for (int32_t i = 0; i < mover->NumWeapons; i++)
                {
                    if (WeaponsStatus[i] == -1)
                    {
                        notReady++;
                    }

                    if (WeaponsStatus[i] == -4)
                    {
                        notLocked++;
                    }

                    if (WeaponsStatus[i] == -6)
                    {
                        hot++;
                    }
                }

                const MCObjectClass objectClass = mover->ObjectClass;

                if (WeaponsStatusResult == 0 &&
                    ((objectClass == MCObjectClass::BattleMech) ||
                     ((objectClass == MCObjectClass::GroundVehicle || objectClass == MCObjectClass::Elemental) &&
                      notReady == 0)))
                {
                    if (notReady < 1 && notLocked < 1 && hot < 1)
                    {
                        extraParams = 8;
                        move = true;
                    }
                    else if (GetMovePath()->NumSteps == 0 && MoveOrders.MoveStateGoalChanged == 0)
                    {
                        MoveOrders.MoveStateGoal = 5;
                    }
                }
            }

            if (!move && wantMove == 0)
            {
                return 1;
            }

            MoveOrders.MoveStateGoal = 1;

            if (attackTarget == nullptr)
            {
                SetMoveGoal(0, &targetPosition, nullptr);
                RequestMovePath(CurTacOrder.SelectionIndex, 0x101, 8);
                return 1;
            }

            SetMoveGoal(static_cast<uint32_t>(attackTarget->PartId), &targetPosition, attackTarget);
            RequestMovePath(CurTacOrder.SelectionIndex, extraParams | 0x101, 7);
            return 1;
        }

        target = GetLastTarget();
    }

    if (target == nullptr)
    {
        return 1;
    }

    MoveOrders.MoveStateGoal = 5;
    return 1;
}

auto MCMechWarrior::ClearCurTacOrder(int updateTacOrder, int updateBrain) -> void
{
    if (CurTacOrder.IsCombatOrder() != 0)
    {
        NumWarriorsInCombat--;
    }

    if (NumWarriorsInCombat < 0)
    {
        Assert(false, 0, "numWarriorsInCombat >= 0");
    }

    CurTacOrder.Reset();

    if (updateTacOrder == 0)
    {
        ClearMoveOrders();
        TriggerAlarm(PILOT_ALARM_NO_MOVEPATH, static_cast<uint32_t>(-14));
    }

    ClearAttackOrders();
    LastTacOrder.LastTime = ScenarioTime;

    if (updateTacOrder == 0)
    {
        return;
    }

    // Fall back to the order underneath: alarm to player (or general), player to general.
    MCTacticalOrder newOrder;
    newOrder.Reset();

    switch (OrderState)
    {
        case ORDERSTATE_GENERAL:
        {
            if (NewTacOrderReceived[ORDERSTATE_GENERAL] == 0)
            {
                TacOrder[ORDERSTATE_GENERAL].Reset();
            }
            break;
        }
        case ORDERSTATE_PLAYER:
        {
            if (NewTacOrderReceived[ORDERSTATE_PLAYER] == 0)
            {
                TacOrder[ORDERSTATE_PLAYER].Reset();
                PlayerOrderFromQueue = 0;
            }

            newOrder = TacOrder[ORDERSTATE_GENERAL];
            OrderState = ORDERSTATE_GENERAL;
            break;
        }
        case ORDERSTATE_ALARM:
        {
            if (NewTacOrderReceived[ORDERSTATE_ALARM] == 0)
            {
                TacOrder[ORDERSTATE_ALARM].Reset();
            }

            AlarmPriority = 0;

            if (TacOrder[ORDERSTATE_PLAYER].Code != MCTacticalOrderCode::None)
            {
                newOrder = TacOrder[ORDERSTATE_PLAYER];
                OrderState = ORDERSTATE_PLAYER;
            }
            else
            {
                newOrder = TacOrder[ORDERSTATE_GENERAL];
                OrderState = ORDERSTATE_GENERAL;
            }
            break;
        }
        default:
            break;
    }

    if (PlayerOrderFromQueue == 0)
    {
        ClearMoveOrders();
        TriggerAlarm(PILOT_ALARM_NO_MOVEPATH, static_cast<uint32_t>(-14));
    }

    Assert(MoveOrders.Path[0] != nullptr && MoveOrders.Path[1] != nullptr, 0, " bad warrior path ");
    MCMovePath* paths[2];

    for (int32_t i = 0; i < 2; i++)
    {
        paths[i] = MoveOrders.Path[i];
        paths[i]->NumSteps = 0;
    }

    const int32_t moveState = MoveOrders.MoveState;
    const int32_t moveStateGoal = MoveOrders.MoveStateGoal;
    MoveOrders.Init();
    PathManager()->Remove(this);
    MoveOrders.Path[1] = paths[1];
    MoveOrders.MoveState = moveState;
    MoveOrders.MoveStateGoal = moveStateGoal;
    MoveOrders.Path[0] = paths[0];
    AttackOrders.Init();
    int32_t message = -1;
    newOrder.Execute(this, message);

    if (OrderState == ORDERSTATE_PLAYER)
    {
        RadioMessage(message, 1);
    }
}

auto MCMechWarrior::SetCurTacOrder(MCTacticalOrder tacOrder) -> void
{
    CurTacOrder = tacOrder;
    LastTacOrder = tacOrder;

    if (CurTacOrder.IsCombatOrder() != 0)
    {
        NumWarriorsInCombat++;
    }
}

auto MCMechWarrior::SetGeneralTacOrder(MCTacticalOrder order) -> void
{
    TacOrder[ORDERSTATE_GENERAL] = order;
    NewTacOrderReceived[ORDERSTATE_GENERAL] = 1;
}

auto MCMechWarrior::SetPlayerTacOrder(MCTacticalOrder order, int fromQueue) -> void
{
    TacOrder[ORDERSTATE_PLAYER] = order;
    NewTacOrderReceived[ORDERSTATE_PLAYER] = 1;
    PlayerOrderFromQueue = fromQueue;

    if (fromQueue == 0)
    {
        ClearTacOrderQueue();
    }
}

auto MCMechWarrior::SetAlarmTacOrder(MCTacticalOrder order, int32_t priority) -> void
{
    if (AlarmPriority <= priority)
    {
        TacOrder[ORDERSTATE_ALARM] = order;
        NewTacOrderReceived[ORDERSTATE_ALARM] = 1;
        AlarmPriority = priority;
    }
}

auto MCMechWarrior::TriggerAlarm(int32_t alarmCode, uint32_t triggerId) -> int32_t
{
    MCPilotAlarm& pilotAlarm = Alarm[alarmCode];

    if (pilotAlarm.NumTriggers == MAX_ALARM_TRIGGERS)
    {
        return -1;
    }

    pilotAlarm.Trigger[pilotAlarm.NumTriggers] = triggerId;
    pilotAlarm.NumTriggers++;
    return 0;
}

auto MCMechWarrior::HandleAlarm(int32_t alarmCode, uint32_t triggerId) -> int32_t
{
    MCMover* mover = static_cast<MCMover*>(Vehicle);
    Assert(mover != nullptr, 0, " bad vehicle for pilot ");

    if (mover->GetAwake() == 0)
    {
        return 0;
    }

    if (alarmCode == PILOT_ALARM_VEHICLE_INCAPACITATED)
    {
        HandleOwnVehicleIncapacitation(triggerId);
    }
    else if (alarmCode == PILOT_ALARM_VEHICLE_DESTROYED)
    {
        HandleOwnVehicleDestruction(triggerId);
    }
    else if (alarmCode == PILOT_ALARM_VEHICLE_WITHDRAWN)
    {
        HandleOwnVehicleWithdrawn();
    }

    if ((MPlayer == nullptr || MPlayer->IsServer != 0) && BrainAlarmCallback[alarmCode] != nullptr)
    {
        MCAblBrainScope brain(GetGroup(), Vehicle, static_cast<int32_t>(Vehicle->ObjectClass), this);
        AblRuntime()->Brain.Alarm = alarmCode;
        Brain->Execute({}, BrainAlarmCallback[alarmCode]);
    }

    return 0;
}

auto MCMechWarrior::GetAlarmTriggers(int32_t alarmCode, uint32_t* triggerList) -> int32_t
{
    const uint8_t numTriggers = Alarm[alarmCode].NumTriggers;

    for (int32_t i = 0; i < numTriggers; i++)
    {
        triggerList[i] = Alarm[alarmCode].Trigger[i];
    }

    return numTriggers;
}

auto MCMechWarrior::ClearAlarm(int32_t alarmCode) -> void
{
    Alarm[alarmCode].NumTriggers = 0;
}

auto MCMechWarrior::CheckAlarms() -> int32_t
{
    std::optional<MCAblBrainScope> brain;

    if (Brain != nullptr)
    {
        brain.emplace(GetGroup(), Vehicle, static_cast<int32_t>(Vehicle->ObjectClass), this);
    }

    for (int32_t alarmCode = 0; alarmCode < NUM_PILOT_ALARMS; alarmCode++)
    {
        if (Alarm[alarmCode].NumTriggers == 0)
        {
            continue;
        }

        switch (alarmCode)
        {
            case PILOT_ALARM_TARGET_OF_WEAPONFIRE:
                HandleTargetOfWeaponFire();
                break;
            case PILOT_ALARM_HIT_BY_WEAPONFIRE:
                HandleHitByWeaponFire();
                break;
            case PILOT_ALARM_DAMAGE_TAKEN_RATE:
                HandleDamageTakenRate();
                break;
            case PILOT_ALARM_DEATH_OF_MATE:
                HandleUnitMateDeath();
                break;
            case PILOT_ALARM_FRIENDLY_VEHICLE_CRIPPLED:
                HandleFriendlyVehicleCrippled();
                break;
            case PILOT_ALARM_FRIENDLY_VEHICLE_DESTROYED:
                HandleFriendlyVehicleDestruction();
                break;
            case PILOT_ALARM_VEHICLE_INCAPACITATED:
                HandleOwnVehicleIncapacitation(0);
                break;
            case PILOT_ALARM_VEHICLE_DESTROYED:
                HandleOwnVehicleDestruction(0);
                break;
            case PILOT_ALARM_VEHICLE_WITHDRAWN:
                HandleOwnVehicleWithdrawn();
                break;
            case PILOT_ALARM_MORALE_BREAK:
                HandleMoraleBreak();
                break;
            case PILOT_ALARM_COLLISION:
                HandleCollision();
                break;
            case PILOT_ALARM_KILLED_TARGET:
                HandleKilledTarget();
                break;
            case PILOT_ALARM_MATE_FIRED_WEAPON:
                HandleUnitMateFiredWeapon();
                break;
            case PILOT_ALARM_PLAYER_ORDER:
                HandlePlayerOrder();
                break;
            case PILOT_ALARM_NO_MOVEPATH:
                HandleNoMovePath();
                break;
            case PILOT_ALARM_GATE_CLOSING:
                HandleGateClosing();
                break;
            default:
                break;
        }

        if ((MPlayer == nullptr || MPlayer->IsServer != 0) && Brain != nullptr &&
            BrainAlarmCallback[alarmCode] != nullptr)
        {
            AblRuntime()->Brain.Alarm = alarmCode;
            Brain->Execute({}, BrainAlarmCallback[alarmCode]);
        }

        Alarm[alarmCode].NumTriggers = 0;
    }

    return 0;
}

auto MCMechWarrior::UpdateActions() -> void
{
    if (static_cast<MCMover*>(Vehicle)->IsCaptured() != 0)
    {
        ClearCurTacOrder(1, 0);
        SetLastTarget(nullptr, 0, 0);
        return;
    }

    if (CombatUpdateTime <= ScenarioTime)
    {
        CombatDecisionTree();
    }

    MovementDecisionTree();
}

auto MCMechWarrior::MainDecisionTree() -> int32_t
{
    Assert(MoveOrders.Path != nullptr, 0, " bad warrior path ");

    // The current order: when done, the next queued player order or the one underneath takes over.
    const bool server = MPlayer == nullptr || MPlayer->IsServer != 0;

    if (server && CurTacOrder.Code == MCTacticalOrderCode::None && TacOrderQueueExecuting != 0 &&
        NumTacOrdersQueued > 0 && NewTacOrderReceived[ORDERSTATE_PLAYER] == 0)
    {
        ExecuteTacOrderQueue();
    }

    if (CurTacOrder.Code != MCTacticalOrderCode::None && CurTacOrder.Status(this) == 1)
    {
        Assert(MPlayer == nullptr || MPlayer->IsServer != 0, 0, " MechWarrior.mainDecisionTree: client! ");

        if (OrderState == ORDERSTATE_PLAYER && TacOrderQueueExecuting != 0)
        {
            ExecuteTacOrderQueue();
        }

        ClearCurTacOrder(1, 0);
    }

    if (OnHomeTeam() != 0 && CurTacOrder.Code == MCTacticalOrderCode::None && TimeOfLastOrders < 0.0f)
    {
        TimeOfLastOrders = ScenarioTime;
    }

    if (BrainUpdateTime <= ScenarioTime || CombatUpdateTime <= ScenarioTime || MovementUpdateTime <= ScenarioTime)
    {
        MCGameObject* target = GetLastTarget();
        MCVector3D attackPoint;
        MCVector3D* targetPoint = nullptr;
        bool update = true;

        if (target == nullptr)
        {
            if (CurTacOrder.Code != MCTacticalOrderCode::AttackPoint)
            {
                update = false;
            }
            else
            {
                attackPoint = AttackOrders.TargetPoint;
                targetPoint = &attackPoint;
            }
        }

        if (update)
        {
            WeaponsStatusResult = CalcWeaponsStatus(target, WeaponsStatus, targetPoint);
        }
    }

    if (BrainUpdateTime <= ScenarioTime)
    {
        if (Alignment != -1 || Duh == 0)
        {
            RunBrain();
        }

        BrainUpdateTime = BrainUpdateFrequency + BrainUpdateTime;
    }

    MCGameObject* target = GetLastTarget();

    if (target != nullptr && LastTargetTime == ScenarioTime)
    {
        WeaponsStatusResult = CalcWeaponsStatus(target, WeaponsStatus, nullptr);
    }

    CheckAlarms();

    // Take a new order: an alarm order overrides the player's, which overrides the general one.
    MCTacticalOrder newOrder;
    newOrder.Reset();

    switch (OrderState)
    {
        case ORDERSTATE_GENERAL:
        {
            if (NewTacOrderReceived[ORDERSTATE_ALARM] != 0)
            {
                ClearCurTacOrder(0, 0);
                newOrder = TacOrder[ORDERSTATE_ALARM];
                OrderState = ORDERSTATE_ALARM;
            }
            else if (NewTacOrderReceived[ORDERSTATE_PLAYER] != 0)
            {
                TacOrder[ORDERSTATE_GENERAL].Reset();
                newOrder = TacOrder[ORDERSTATE_PLAYER];
                OrderState = ORDERSTATE_PLAYER;
            }
            else if (NewTacOrderReceived[ORDERSTATE_GENERAL] != 0)
            {
                newOrder = TacOrder[ORDERSTATE_GENERAL];
            }
            break;
        }
        case ORDERSTATE_PLAYER:
        {
            if (NewTacOrderReceived[ORDERSTATE_ALARM] != 0)
            {
                newOrder = TacOrder[ORDERSTATE_ALARM];
                OrderState = ORDERSTATE_ALARM;
            }
            else if (NewTacOrderReceived[ORDERSTATE_PLAYER] != 0)
            {
                TacOrder[ORDERSTATE_GENERAL].Reset();
                newOrder = TacOrder[ORDERSTATE_PLAYER];
            }
            break;
        }
        case ORDERSTATE_ALARM:
        {
            if (NewTacOrderReceived[ORDERSTATE_PLAYER] != 0)
            {
                // Original behaviour (OB-008): the player's order runs, but orderState stays ALARM.
                AlarmPriority = 0;
                TacOrder[ORDERSTATE_ALARM].Reset();
                newOrder = TacOrder[ORDERSTATE_PLAYER];
            }
            else if (NewTacOrderReceived[ORDERSTATE_ALARM] != 0)
            {
                newOrder = TacOrder[ORDERSTATE_ALARM];
            }
            break;
        }
        default:
            break;
    }

    if (newOrder.Code != MCTacticalOrderCode::None)
    {
        // A move to a point or object keeps the path walked (a new path replaces it when planned).
        MCMovePath* paths[2];

        for (int32_t i = 0; i < 2; i++)
        {
            paths[i] = MoveOrders.Path[i];

            if (i > 0 || (newOrder.Code != MCTacticalOrderCode::MoveToPoint &&
                          newOrder.Code != MCTacticalOrderCode::MoveToObject))
            {
                paths[i]->NumSteps = 0;
            }
        }

        const int32_t run = MoveOrders.Run;
        const int32_t moveState = MoveOrders.MoveState;
        const int32_t moveStateGoal = MoveOrders.MoveStateGoal;
        MoveOrders.Init();
        MoveOrders.Run = run;
        PathManager()->Remove(this);
        MoveOrders.MoveState = moveState;
        MoveOrders.MoveStateGoal = moveStateGoal;
        MoveOrders.Path[0] = paths[0];
        MoveOrders.Path[1] = paths[1];
        AttackOrders.Init();
        int32_t message = -1;
        newOrder.Execute(this, message);

        if (OrderState == ORDERSTATE_PLAYER)
        {
            RadioMessage(message, 1);
        }

        SetCurTacOrder(newOrder);
        TimeOfLastOrders = -1.0f;
    }

    NewTacOrderReceived[ORDERSTATE_GENERAL] = 0;
    NewTacOrderReceived[ORDERSTATE_PLAYER] = 0;
    NewTacOrderReceived[ORDERSTATE_ALARM] = 0;
    UpdateActions();
    return 0;
}

auto MCMechWarrior::SetDebugFlag(uint32_t flag, int on) -> void
{
    if (on != 0)
    {
        DebugFlags |= flag;
    }
    else
    {
        DebugFlags &= ~flag;
    }
}

auto MCMechWarrior::GetDebugFlag(uint32_t flag) -> int
{
    return (DebugFlags & flag) != 0 ? 1 : 0;
}

auto MCMechWarrior::DebugPrint(char* s, int debugMode) -> void
{
    if (MCAblDebugger* debugger = AblGetDebugger())
    {
        debugger->Print(s);

        if (debugMode != 0)
        {
            debugger->DebugMode();
        }
    }
}

auto MCMechWarrior::DebugOrders() -> void
{
    char line[256];
    const int32_t targetId = CurTacOrder.Target != nullptr ? CurTacOrder.Target->PartId : 0;

    switch (CurTacOrder.Code)
    {
        case MCTacticalOrderCode::None:
            std::snprintf(line, sizeof(line), "CURRENT ORDERS: None");
            break;
        case MCTacticalOrderCode::Wait:
            std::snprintf(line, sizeof(line), "CURRENT ORDERS: Wait");
            break;
        case MCTacticalOrderCode::MoveToPoint:
        {
            const MCVector3D point = CurTacOrder.GetWayPoint(0);
            std::snprintf(line, sizeof(line), "CURRENT ORDERS: Move to (%.2f, %.2f, %.2f)",
                          static_cast<double>(point.X), static_cast<double>(point.Y), static_cast<double>(point.Z));
            break;
        }

        case MCTacticalOrderCode::MoveToObject:
            std::snprintf(line, sizeof(line), "CURRENT ORDERS: Move To Object %d", targetId);
            break;
        case MCTacticalOrderCode::JumpToPoint:
        {
            const MCVector3D point = CurTacOrder.GetWayPoint(0);
            std::snprintf(line, sizeof(line), "CURRENT ORDERS: Jump to (%.2f, %.2f, %.2f)",
                          static_cast<double>(point.X), static_cast<double>(point.Y), static_cast<double>(point.Z));
            break;
        }

        case MCTacticalOrderCode::JumpToObject:
            std::snprintf(line, sizeof(line), "CURRENT ORDERS: Jump To Object %d", targetId);
            break;
        case MCTacticalOrderCode::TraversePath:
            std::snprintf(line, sizeof(line), "CURRENT ORDERS: Traverse Path");
            break;
        case MCTacticalOrderCode::PatrolPath:
            std::snprintf(line, sizeof(line), "CURRENT ORDERS: Patrol Path");
            break;
        case MCTacticalOrderCode::Escort:
            std::snprintf(line, sizeof(line), "CURRENT ORDERS: Escort");
            break;
        case MCTacticalOrderCode::Follow:
            std::snprintf(line, sizeof(line), "CURRENT ORDERS: Follow");
            break;
        case MCTacticalOrderCode::Guard:
            std::snprintf(line, sizeof(line), "CURRENT ORDERS: Guard");
            break;
        case MCTacticalOrderCode::Stop:
            std::snprintf(line, sizeof(line), "CURRENT ORDERS: Stop");
            break;
        case MCTacticalOrderCode::PowerUp:
            std::snprintf(line, sizeof(line), "CURRENT ORDERS: Power Up");
            break;
        case MCTacticalOrderCode::PowerDown:
            std::snprintf(line, sizeof(line), "CURRENT ORDERS: Power Down");
            break;
        case MCTacticalOrderCode::WayPointsDone:
            std::snprintf(line, sizeof(line), "CURRENT ORDERS: Formation");
            break;
        case MCTacticalOrderCode::Eject:
            std::snprintf(line, sizeof(line), "CURRENT ORDERS: Eject");
            break;
        case MCTacticalOrderCode::AttackObject:
            std::snprintf(line, sizeof(line), "CURRENT ORDERS: Attack Object %d", targetId);
            break;
        case MCTacticalOrderCode::HoldFire:
            std::snprintf(line, sizeof(line), "CURRENT ORDERS: Hold Fire");
            break;
        case MCTacticalOrderCode::Withdraw:
            std::snprintf(line, sizeof(line), "CURRENT ORDERS: Withdraw");
            break;
        default:
            std::snprintf(line, sizeof(line), "CURRENT ORDERS: Unknown Tac Order Type");
            break;
    }

    DebugPrint(line, 0);
    MCGameObject* target = GetLastTarget();
    std::snprintf(line, sizeof(line), "     CURRENT TARGET: Object %d", target != nullptr ? target->PartId : 0);
    DebugPrint(line, 0);
}

auto MCMechWarrior::SetMoveSpeedType(int32_t type) -> void
{
    MoveOrders.SpeedType = type;
}

auto MCMechWarrior::SetMoveSpeedVelocity(float speed) -> void
{
    MoveOrders.SpeedVelocity = speed;
    int32_t state = 0;
    int32_t throttle = 0;
    static_cast<MCMover*>(Vehicle)->CalcSpriteSpeed(speed, 0, state, throttle);
    MoveOrders.SpeedState = static_cast<int8_t>(state);
    MoveOrders.SpeedThrottle = static_cast<int8_t>(throttle);
}

auto MCMechWarrior::OpenStatusWindow(int32_t x, int32_t y, int32_t w, int32_t h) -> int32_t
{
    MCWarriorStatusWindow* window = new MCWarriorStatusWindow;
    StatusWindow = window;
    window->Init(x, y, w, h, this);
    StatusWindow->SetBackColor(0);
    StatusWindow->Draw();
    ScreenWindow->AddChild(StatusWindow);
    return 0;
}

auto MCMechWarrior::CloseStatusWindow() -> int32_t
{
    // Original behaviour (OB-009): the window is destroyed but its memory never freed.
    StatusWindow->Destroy();
    StatusWindow = nullptr;
    return 0;
}

auto MCMechWarrior::OrderWait(int unitOrder, MCOrderOrigin origin, int32_t seconds, int clearLastTarget) -> int32_t
{
    MCTacticalOrder order;
    order.Reset();
    order.Reset(origin, MCTacticalOrderCode::Wait, unitOrder);
    order.DelayedTime = static_cast<float>(seconds) + ScenarioTime;
    ClearMoveOrders();
    ClearAttackOrders();

    if (clearLastTarget != 0)
    {
        SetLastTarget(nullptr, 0, 0);
    }

    if (origin == MCOrderOrigin::Commander)
    {
        SetGeneralTacOrder(order);
    }

    return order.Status(this);
}

auto MCMechWarrior::OrderStop(int unitOrder, int setTacOrder) -> int32_t
{
    MCTacticalOrder order;
    order.Reset();
    order.Reset(MCOrderOrigin::Player, MCTacticalOrderCode::Stop, unitOrder);
    ClearTacOrderQueue();
    ClearMoveOrders();
    ClearAttackOrders();
    SetLastTarget(nullptr, 0, 0);
    return order.Status(this);
}

auto MCMechWarrior::OrderMoveToPoint(int unitOrder, int setTacOrder, MCOrderOrigin origin, MCVector3D location,
                                     int32_t selectionIndex, uint32_t params) -> int32_t
{
    const uint32_t escapeTile = (params >> 6) & 1;
    const uint32_t run = params & 1;
    MCTacticalOrder order;
    order.Reset();
    order.Reset(origin, MCTacticalOrderCode::MoveToPoint, unitOrder);
    SetFirstWayPoint(order, location);
    order.MoveParams.WayPath.Mode[0] = run != 0 ? 1 : 0;
    order.MoveParams.Wait = (params >> 1) & 1;
    order.SelectionIndex = selectionIndex;
    order.MoveParams.Mode = (params >> 3) & 1;
    order.MoveParams.EscapeTile = escapeTile;
    const int32_t result = order.Status(this);

    if (result == 1)
    {
        return 1;
    }

    SetMoveGoal(0xffffffff, nullptr, nullptr);
    SetMoveWayPath(nullptr, 0);
    SetMoveGoal(0, &location, nullptr);
    MoveOrders.TimeOfLastStep = ScenarioTime;
    MoveOrders.Run = run;

    if (setTacOrder != 0)
    {
        ClearAttackOrders();
    }

    uint32_t moveParams = escapeTile != 0 ? 0x2101 : 0x101;

    // The player's order to a unit's point (or to one mover) reports a blocked move on the radio.
    if (setTacOrder != 0 && origin == MCOrderOrigin::Player && (unitOrder == 0 || GetPoint() == Vehicle))
    {
        moveParams |= 0x1000;
    }

    PathManager()->Request(this, selectionIndex, moveParams, 255.0f, 15);

    if (setTacOrder != 0 && result == 0 && origin == MCOrderOrigin::Commander)
    {
        SetGeneralTacOrder(order);
    }

    return result;
}

auto MCMechWarrior::OrderMoveToObject(int unitOrder, int setTacOrder, MCOrderOrigin origin, MCGameObject* target,
                                      int32_t selectionIndex, uint32_t params) -> int32_t
{
    const uint32_t faceObject = (params >> 2) & 1;

    if (target == nullptr)
    {
        return 1;
    }

    MCTacticalOrder order;
    order.Reset();
    order.Reset(origin, MCTacticalOrderCode::MoveToObject, unitOrder);
    order.SelectionIndex = selectionIndex;
    order.MoveParams.WayPath.Mode[0] = (params & 1) != 0 ? 1 : 0;
    order.MoveParams.FaceObject = faceObject;
    order.MoveParams.Mode = (params >> 3) & 1;
    order.Target = target;
    order.MoveParams.Wait = 0;
    const int32_t result = order.Status(this);

    if (result == 1)
    {
        return 1;
    }

    MCVector3D goal = target->GetPosition();
    SetMoveGoal(static_cast<uint32_t>(target->PartId), &goal, target);
    MoveOrders.Run = params & 1;

    if (setTacOrder != 0)
    {
        ClearAttackOrders();
    }

    uint32_t moveParams = faceObject != 0 ? 0x101 : 0x100;

    if (setTacOrder != 0 && origin == MCOrderOrigin::Player && (unitOrder == 0 || GetPoint() == Vehicle))
    {
        moveParams |= 0x1000;
    }

    RequestMovePath(selectionIndex, moveParams, 12);
    MoveOrders.GoalObjectPosition = target->GetPosition();

    if (setTacOrder != 0 && result == 0 && origin == MCOrderOrigin::Commander)
    {
        SetGeneralTacOrder(order);
    }

    return result;
}

auto MCMechWarrior::OrderJumpToPoint(int unitOrder, int setTacOrder, MCOrderOrigin origin, MCVector3D location,
                                     int32_t selectionIndex) -> int32_t
{
    MCMover* mover = static_cast<MCMover*>(Vehicle);
    const float jumpRange = mover->GetJumpRange(nullptr, nullptr);

    if (mover->DistanceFrom(location) <= jumpRange)
    {
        // A mech can't land on a blocked cell.
        if (mover->ObjectClass == MCObjectClass::BattleMech && !PositionPassable(location))
        {
            return 1;
        }

        MCTacticalOrder order;
        order.Reset();
        order.Reset(origin, MCTacticalOrderCode::JumpToPoint, unitOrder);
        order.SelectionIndex = selectionIndex;
        SetFirstWayPoint(order, location);
        const int32_t result = order.Status(this);

        if (result != 1 && setTacOrder != 0)
        {
            ClearMoveOrders();
            ClearAttackOrders();

            if (result == 0 && origin == MCOrderOrigin::Commander)
            {
                SetGeneralTacOrder(order);
            }
        }
    }

    return 1;
}

auto MCMechWarrior::OrderJumpToObject(int unitOrder, int setTacOrder, MCOrderOrigin origin, MCGameObject* target,
                                      int32_t selectionIndex) -> int32_t
{
    MCMover* mover = static_cast<MCMover*>(Vehicle);
    MCVector3D location = target->GetPosition();

    if (IsMover(target) && target->GetTeam() == mover->GetTeam())
    {
        return 1;
    }

    const float jumpRange = mover->GetJumpRange(nullptr, nullptr);

    if (mover->DistanceFrom(location) <= jumpRange)
    {
        if (mover->ObjectClass == MCObjectClass::BattleMech && !PositionPassable(location))
        {
            return 1;
        }

        MCTacticalOrder order;
        order.Reset();
        order.Reset(origin, MCTacticalOrderCode::JumpToPoint, unitOrder);
        order.SelectionIndex = selectionIndex;
        SetFirstWayPoint(order, location);
        order.Target = target;
        const int32_t result = order.Status(this);

        if (result != 1 && setTacOrder != 0)
        {
            ClearMoveOrders();
            ClearAttackOrders();

            if (result == 0 && origin == MCOrderOrigin::Commander)
            {
                SetGeneralTacOrder(order);
            }
        }
    }

    return 1;
}

auto MCMechWarrior::OrderTraversePath(int unitOrder, int setTacOrder, MCOrderOrigin origin, MCWayPath* wayPath,
                                      uint32_t params) -> int32_t
{
    MCTacticalOrder order;
    order.Reset();
    order.Reset(origin, MCTacticalOrderCode::TraversePath, unitOrder);
    order.MoveParams.WayPath = *wayPath;
    order.MoveParams.Mode = (params >> 3) & 1;
    const int32_t result = order.Status(this);

    if (result == 1)
    {
        return 1;
    }

    MCVector3D firstPoint(order.MoveParams.WayPath.Points[0], order.MoveParams.WayPath.Points[1],
                          order.MoveParams.WayPath.Points[2]);
    SetMoveGoal(0, &firstPoint, nullptr);
    SetMoveWayPath(wayPath, 0);

    if (setTacOrder != 0)
    {
        ClearAttackOrders();
    }

    RequestMovePath(-1, 0x101, 13);

    if (setTacOrder != 0 && result == 0 && origin == MCOrderOrigin::Commander)
    {
        SetGeneralTacOrder(order);
    }

    return result;
}

auto MCMechWarrior::OrderPatrolPath(int unitOrder, int setTacOrder, MCOrderOrigin origin, MCWayPath* wayPath) -> int32_t
{
    MCTacticalOrder order;
    order.Reset();
    order.Reset(origin, MCTacticalOrderCode::PatrolPath, unitOrder);
    order.MoveParams.WayPath = *wayPath;
    const int32_t result = order.Status(this);

    if (result == 1)
    {
        return 1;
    }

    MCVector3D firstPoint(order.MoveParams.WayPath.Points[0], order.MoveParams.WayPath.Points[1],
                          order.MoveParams.WayPath.Points[2]);
    SetMoveGoal(0, &firstPoint, nullptr);
    SetMoveWayPath(wayPath, 1);

    if (setTacOrder != 0)
    {
        ClearAttackOrders();
    }

    RequestMovePath(-1, 0x101, 14);

    if (setTacOrder != 0 && result == 0 && origin == MCOrderOrigin::Commander)
    {
        SetGeneralTacOrder(order);
    }

    return result;
}

auto MCMechWarrior::OrderPowerUp(int unitOrder, MCOrderOrigin origin) -> int32_t
{
    MCMover* mover = static_cast<MCMover*>(Vehicle);

    if (static_cast<int8_t>(mover->Status) != 5)
    {
        return 1;
    }

    MCTacticalOrder order;
    order.Reset();
    order.Reset(origin, MCTacticalOrderCode::PowerUp, unitOrder);
    const int32_t result = order.Status(this);

    if (result == 1)
    {
        return 1;
    }

    ClearMoveOrders();
    ClearAttackOrders();

    if (mover != nullptr && mover->CanPowerUp() != 0)
    {
        mover->StartUp();
    }

    if (origin == MCOrderOrigin::Commander)
    {
        SetGeneralTacOrder(order);
    }
    else if (origin == MCOrderOrigin::Self)
    {
        SetAlarmTacOrder(order, 255);
    }

    return result;
}

auto MCMechWarrior::OrderPowerDown(int unitOrder, MCOrderOrigin origin) -> int32_t
{
    MCMover* mover = static_cast<MCMover*>(Vehicle);
    const int8_t vehicleStatus = static_cast<int8_t>(mover->Status);

    if (vehicleStatus == 5 || vehicleStatus == 4)
    {
        return 1;
    }

    MCTacticalOrder order;
    order.Reset();
    order.Reset(origin, MCTacticalOrderCode::PowerDown, unitOrder);
    const int32_t result = order.Status(this);

    if (result == 1)
    {
        return 1;
    }

    ClearMoveOrders();
    ClearAttackOrders();

    if (mover != nullptr)
    {
        mover->ShutDown();
    }

    if (origin == MCOrderOrigin::Commander)
    {
        SetGeneralTacOrder(order);
    }

    return result;
}

auto MCMechWarrior::OrderUseSpeed(float speed) -> int32_t
{
    SetMoveSpeedVelocity(speed);
    return 1;
}

auto MCMechWarrior::OrderOrbitPoint(MCVector3D location) -> int32_t
{
    return 1;
}

auto OrderOrbitObject(MCGameObject* target) -> int32_t
{
    return 1;
}

auto OrderUseOrbitRange(int32_t type, float range) -> int32_t
{
    return 1;
}

namespace
{
    /// <summary>What the attack orders print in GameSystemWindow when it exists.</summary>
    void PrintAttackOrder(MCMechWarrior* pilot)
    {
        if (GameSystemWindow == nullptr)
        {
            return;
        }

        char line[200];
        GameSystemWindow->Print(const_cast<char*>(""));
        GameSystemWindow->Print(const_cast<char*>("-----------------------------------"));
        std::snprintf(line, sizeof(line), "%s:", pilot->Name);
        GameSystemWindow->Print(line);
        MCMover* mover = static_cast<MCMover*>(pilot->Vehicle);
        const MCMasterComponent& weapon = MasterComponentList[mover->Inventory[mover->LongestRangeWeapon].MasterID];
        std::snprintf(line, sizeof(line), "Longest Range Weapon = %s (%.4f)", weapon.Name.c_str(),
                      static_cast<double>(weapon.WeaponRange[3]));
        GameSystemWindow->Print(line);
        std::snprintf(line, sizeof(line), "Optimal Range = %.4f", static_cast<double>(mover->OptimalRange));
        GameSystemWindow->Print(line);
        GameSystemWindow->Print(const_cast<char*>("-----------------------------------"));
    }
}

auto MCMechWarrior::OrderAttackObject(int unitOrder, MCOrderOrigin origin, MCGameObject* target, int32_t type,
                                      int32_t method, int32_t range, int32_t aimLocation, uint32_t params) -> int32_t
{
    const uint32_t pursue = (params >> 4) & 1;
    const uint32_t obliterate = (params >> 5) & 1;
    const int conserveAmmo = type == 3 ? 1 : 0;

    if (target == nullptr)
    {
        ClearAttackOrders();
        return 1;
    }

    // Only a ram needs no weapons.
    if (static_cast<MCMover*>(Vehicle)->NumWeapons == 0 && method != 2)
    {
        ClearAttackOrders();
        return 1;
    }

    MCTacticalOrder order;
    order.Reset();
    order.Reset(origin, MCTacticalOrderCode::AttackObject, unitOrder);
    order.Target = target;
    order.AttackParams.Type = type;
    order.AttackParams.Method = method;
    order.AttackParams.AimLocation = aimLocation;

    if (method == 2)
    {
        range = -3;
    }

    order.AttackParams.Range = range;
    order.AttackParams.Pursue = pursue;
    order.AttackParams.Obliterate = obliterate;

    if (order.Status(this) == 1)
    {
        return 1;
    }

    OrderUseFireRange(range);

    if (pursue == 0)
    {
        ClearMoveOrders();
    }
    else
    {
        OrderMoveToObject(unitOrder, 0, origin, target, -1, params | 4);
    }

    AttackOrders.Type = type;
    SetAttackTarget(target);
    AttackOrders.Pursue = pursue;
    AttackOrders.AimLocation = aimLocation;
    SetLastTarget(target, obliterate, conserveAmmo);

    if (origin == MCOrderOrigin::Commander)
    {
        SetGeneralTacOrder(order);
    }

    PrintAttackOrder(this);
    return 0;
}

auto MCMechWarrior::OrderAttackPoint(int unitOrder, MCOrderOrigin origin, MCVector3D location, int32_t type,
                                     int32_t method, int32_t range, uint32_t params) -> int32_t
{
    const uint32_t pursue = (params >> 4) & 1;
    MCTacticalOrder order;
    order.Reset();
    order.Reset(origin, MCTacticalOrderCode::AttackPoint, unitOrder);
    order.AttackParams.Type = type;
    order.AttackParams.Method = method;
    order.AttackParams.Range = range;
    order.AttackParams.TargetPoint = location;
    order.AttackParams.Pursue = pursue;

    if (order.Status(this) == 1)
    {
        return 1;
    }

    OrderUseFireRange(range);

    if (pursue == 0)
    {
        ClearMoveOrders();
    }
    else
    {
        OrderMoveToPoint(unitOrder, 0, origin, location, -1, params);
    }

    AttackOrders.Type = type;
    SetAttackTarget(nullptr);
    SetAttackTargetPoint(location);
    AttackOrders.AimLocation = -1;
    AttackOrders.Pursue = pursue;
    SetLastTarget(nullptr, 0, 0);

    if (origin == MCOrderOrigin::Commander)
    {
        SetGeneralTacOrder(order);
    }

    PrintAttackOrder(this);
    return 0;
}

auto MCMechWarrior::OrderWithdraw(int unitOrder, MCOrderOrigin origin, MCVector3D location) -> int32_t
{
    MCTacticalOrder order;
    order.Reset();
    order.Reset(origin, MCTacticalOrderCode::Withdraw, unitOrder);
    SetFirstWayPoint(order, location);
    const MCVector3D goal = CalcWithdrawGoal(1000.0f);
    const int32_t result = OrderMoveToPoint(unitOrder, 1, origin, goal, -1, 1);
    MCMover* mover = static_cast<MCMover*>(Vehicle);
    Assert(mover != nullptr, 0, " orderWithdraw:Warrior has no Vehicle ");
    mover->Withdrawing = 1;

    if (origin == MCOrderOrigin::Commander)
    {
        SetGeneralTacOrder(order);
    }

    CurTacOrder.Code = MCTacticalOrderCode::Withdraw;
    return result;
}

auto MCMechWarrior::OrderEject(int unitOrder, int setTacOrder, MCOrderOrigin origin) -> int32_t
{
    MCTacticalOrder order;
    order.Reset();
    order.Reset(origin, MCTacticalOrderCode::Eject, unitOrder);
    MCMover* mover = static_cast<MCMover*>(Vehicle);
    Assert(mover != nullptr, 0, " orderWithdraw:Warrior has no Vehicle ");
    mover->HandleEjection();

    if (origin == MCOrderOrigin::Commander)
    {
        SetGeneralTacOrder(order);
    }

    return 1;
}

auto MCMechWarrior::OrderUseFireRange(int32_t range) -> int32_t
{
    OrderFireRange = static_cast<MCMover*>(Vehicle)->GetFireRange(range);
    return 1;
}

auto MCMechWarrior::OrderUseFireOdds(int32_t odds) -> int32_t
{
    OrderFireOdds = FireOddsTable[odds];
    return 1;
}

auto MCMechWarrior::OrderRefit(MCOrderOrigin origin, MCGameObject* target, uint32_t params) -> int32_t
{
    if (target == nullptr || target->ObjectClass != MCObjectClass::BattleMech)
    {
        return 1;
    }

    MCTacticalOrder order;
    order.Reset();
    order.Reset(origin, MCTacticalOrderCode::Refit, 0);
    order.Target = target;
    order.SelectionIndex = -1;
    order.MoveParams.WayPath.Mode[0] = static_cast<uint8_t>(params & 1);
    order.MoveParams.FaceObject = 1;
    order.MoveParams.Wait = 0;

    if (origin == MCOrderOrigin::Commander)
    {
        SetGeneralTacOrder(order);
    }

    return 0;
}

auto MCMechWarrior::OrderGetFixed(MCOrderOrigin origin, MCGameObject* target, uint32_t params) -> int32_t
{
    if (target == nullptr)
    {
        return 1;
    }

    if (target->ObjectClass != MCObjectClass::TreeBuilding && target->GetRefitPoints() <= 0.0)
    {
        return 1;
    }

    // A mech bay fixes mechs, a vehicle bay vehicles.
    const MCObjectClass vehicleClass = Vehicle->ObjectClass;
    const int32_t bayKind = RepairBayKind(target);

    if ((vehicleClass == MCObjectClass::BattleMech && bayKind == 0) ||
        (vehicleClass == MCObjectClass::GroundVehicle && bayKind == 1))
    {
        return 1;
    }

    MCTacticalOrder order;
    order.Reset();
    order.Reset(origin, MCTacticalOrderCode::GetFixed, 0);
    order.Target = target;
    order.SelectionIndex = -1;
    order.MoveParams.WayPath.Mode[0] = static_cast<uint8_t>(params & 1);
    order.MoveParams.FaceObject = 1;
    order.MoveParams.Wait = 0;

    if (origin == MCOrderOrigin::Commander)
    {
        SetGeneralTacOrder(order);
    }

    return 0;
}

auto MCMechWarrior::OrderLoadIntoCarrier(MCOrderOrigin origin, MCGameObject* target, uint32_t params) -> int32_t
{
    if (Vehicle->ObjectClass != MCObjectClass::Elemental || target == nullptr ||
        target->ObjectClass != MCObjectClass::GroundVehicle ||
        static_cast<MCGroundVehicle*>(target)->ElementalCarrier == 0)
    {
        return 1;
    }

    MCTacticalOrder order;
    order.Reset();
    order.Reset(origin, MCTacticalOrderCode::LoadIntoCarrier, 0);
    order.Target = target;
    order.SelectionIndex = -1;
    order.MoveParams.WayPath.Mode[0] = static_cast<uint8_t>(params & 1);
    order.MoveParams.FaceObject = 1;
    order.MoveParams.Wait = 0;

    if (origin == MCOrderOrigin::Commander)
    {
        SetGeneralTacOrder(order);
    }

    return 0;
}

auto MCMechWarrior::OrderDeployElementals(MCOrderOrigin origin, uint32_t params) -> int32_t
{
    if (Vehicle->ObjectClass != MCObjectClass::GroundVehicle ||
        static_cast<MCGroundVehicle*>(Vehicle)->ElementalCarrier == 0)
    {
        return 1;
    }

    MCTacticalOrder order;
    order.Reset();
    order.Reset(origin, MCTacticalOrderCode::DeployElementals, 0);
    order.MoveParams.Wait = 0;
    order.MoveParams.WayPath.Mode[0] = static_cast<uint8_t>(params & 1);

    if (origin == MCOrderOrigin::Commander)
    {
        SetGeneralTacOrder(order);
    }

    return 0;
}

auto MCMechWarrior::OrderCapture(MCOrderOrigin origin, MCGameObject* target, uint32_t params) -> int32_t
{
    // Original behaviour: the test reads isCaptureable() == 0 (the slot's name may not match its meaning).
    if (target == nullptr || target->IsCaptureable() != 0 || target->GetAlignment() == Alignment ||
        target->GetCaptureBlocker(Alignment) != nullptr)
    {
        return 1;
    }

    MCTacticalOrder order;
    order.Reset();
    order.Reset(origin, MCTacticalOrderCode::Capture, 0);
    order.Target = target;
    order.SelectionIndex = -1;
    order.MoveParams.WayPath.Mode[0] = static_cast<uint8_t>(params & 1);
    order.MoveParams.FaceObject = 1;
    order.MoveParams.Wait = 0;

    if (origin == MCOrderOrigin::Commander)
    {
        SetGeneralTacOrder(order);
    }

    return 0;
}

auto MCMechWarrior::HandleTargetOfWeaponFire() -> int32_t
{
    if (Vehicle != nullptr)
    {
        TheInterface->ObjectAttacked(Vehicle->PartId);
    }

    return 0;
}

auto MCMechWarrior::HandleHitByWeaponFire() -> int32_t
{
    if (Alarm[PILOT_ALARM_HIT_BY_WEAPONFIRE].Trigger[0] != 0)
    {
        RadioMessage(RADIO_UNDER_ATTACK, 1);
    }

    return 0;
}

auto MCMechWarrior::HandleCollision() -> int32_t
{
    ObjectList()->FindObjectFromPart(static_cast<int32_t>(Alarm[PILOT_ALARM_COLLISION].Trigger[0]));
    return 0;
}

auto MCMechWarrior::HandleDamageTakenRate() -> int32_t
{
    return 0;
}

auto MCMechWarrior::HandleUnitMateDeath() -> int32_t
{
    const int32_t mateId = static_cast<int32_t>(Alarm[PILOT_ALARM_DEATH_OF_MATE].Trigger[0]);

    if (Vehicle->PartId == mateId)
    {
        return 0;
    }

    return GetMoverFromPartId(mateId) != nullptr ? 0 : -1;
}

auto MCMechWarrior::HandleFriendlyVehicleCrippled() -> int32_t
{
    return 0;
}

auto MCMechWarrior::HandleFriendlyVehicleDestruction() -> int32_t
{
    return 0;
}

auto MCMechWarrior::HandleOwnVehicleIncapacitation(uint32_t cause) -> int32_t
{
    MCMover* mover = static_cast<MCMover*>(Vehicle);
    Assert(mover != nullptr, 0, " pilot has no vehicle ");

    if (cause < 2 || cause == 0x42)
    {
        mover->HandleEjection();
    }

    ClearCurTacOrder(0, 0);
    OrderState = ORDERSTATE_GENERAL;
    MCMovePath* paths[2];

    for (int32_t i = 0; i < 2; i++)
    {
        paths[i] = MoveOrders.Path[i];
        paths[i]->NumSteps = 0;
    }

    MoveOrders.Init();
    PathManager()->Remove(this);
    MoveOrders.Path[1] = paths[1];
    MoveOrders.Path[0] = paths[0];
    AttackOrders.Init();
    SetLastTarget(nullptr, 0, 0);
    return 0;
}

auto MCMechWarrior::HandleOwnVehicleDestruction(uint32_t cause) -> int32_t
{
    Assert(Vehicle != nullptr, 0, "handleOwnVehicleDestruction:pilot has no vehicle ");
    return 0;
}

auto MCMechWarrior::HandleOwnVehicleWithdrawn() -> int32_t
{
    Assert(Vehicle != nullptr, 0, "handleOwnVehicleWithdrawn:pilot has no vehicle ");
    Status = 2;
    return 0;
}

auto MCMechWarrior::HandleMoraleBreak() -> int32_t
{
    MCTacticalOrder order;
    order.Reset();
    order.Reset(MCOrderOrigin::Self, MCTacticalOrderCode::Withdraw, 0);
    SetAlarmTacOrder(order, 10);
    return 0;
}

auto MCMechWarrior::HandleCollisionAlert() -> int32_t
{
    return 0;
}

auto MCMechWarrior::HandleKilledTarget() -> int32_t
{
    MCBaseObject* target =
        ObjectList()->FindObjectFromPart(static_cast<int32_t>(Alarm[PILOT_ALARM_KILLED_TARGET].Trigger[0]));

    if (target == nullptr)
    {
        return 0;
    }

    // Count the kill and score gunnery points by what it was.
    int32_t killType = -1;
    float points = 10.0f;
    int32_t message;

    switch (target->ObjectClass)
    {
        case MCObjectClass::BattleMech:
        {
            killType = static_cast<int32_t>(static_cast<MCGameObject*>(target)->GetMechClass());
            points = KillSkill[killType];
            NumKilled[killType][1]++;
            message = RADIO_MECH_DESTROYED;
            break;
        }
        case MCObjectClass::GroundVehicle:
        case MCObjectClass::Turret:
        {
            killType = 5;
            points = KillSkill[4];
            NumKilled[5][1]++;
            message = RADIO_VEHICLE_DESTROYED;
            break;
        }
        case MCObjectClass::Elemental:
        {
            killType = 6;
            points = KillSkill[5];
            NumKilled[6][1]++;
            message = RADIO_OBJECT_DESTROYED;
            break;
        }
        default:
            message = RADIO_OBJECT_DESTROYED;
            break;
    }

    RadioMessage(message, 0);

    // A tenth for killing one of our own (or an ally's).
    if (std::abs(static_cast<MCGameObject*>(target)->GetAlignment() - Alignment) < 2)
    {
        points = points * 0.1f;
    }

    SkillPoints[MWS_GUNNERY] = points + SkillPoints[MWS_GUNNERY];

    if (MPlayer != nullptr && MPlayer->IsServer != 0 && killType != -1)
    {
        MPlayer->AddPilotKillStat(static_cast<MCMover*>(Vehicle), killType);
    }

    return 0;
}

auto MCMechWarrior::HandleUnitMateFiredWeapon() -> int32_t
{
    return 0;
}

auto MCMechWarrior::HandlePlayerOrder() -> int32_t
{
    if (GetVehicleStatus() == 5 && CurTacOrder.Code != MCTacticalOrderCode::PowerDown)
    {
        OrderPowerUp(0, MCOrderOrigin::Self);
    }

    return 0;
}

auto MCMechWarrior::HandleNoMovePath() -> int32_t
{
    if (CurTacOrder.Code == MCTacticalOrderCode::GetFixed)
    {
        ClearCurTacOrder(1, 0);
        RadioMessage(RADIO_MOVE_BLOCKED, 0);
    }

    return 0;
}

auto MCMechWarrior::HandleGateClosing() -> int32_t
{
    return 0;
}

auto MCMechWarrior::MissionLog(MCFile* file, int32_t unitLevel) -> int32_t
{
    char line[80];

    for (int32_t i = unitLevel * 2; i > 0; i--)
    {
        file->WriteString(" ");
    }

    std::snprintf(line, sizeof(line), "MechWarrior: %s\n", Name);
    file->WriteString(line);

    // Port fix: the original's skill loop never advanced or wrote its line (an endless loop); this writes each
    // skill's successes and tries once, in the original's format.
    for (int32_t skill = 0; skill < NUM_SKILLS; skill++)
    {
        for (int32_t i = unitLevel * 2 + 2; i > 0; i--)
        {
            file->WriteString(" ");
        }

        std::snprintf(line, sizeof(line), "%s: %04d/04%d\n", SkillsTable[skill], NumSkillSuccesses[skill][1],
                      NumSkillUses[skill][1]);
        file->WriteString(line);
    }

    return 0;
}

auto MCMechWarrior::CalcRank() -> void
{
    double weightedSum = 0.0;
    double totalWeight = 0.0;

    for (int32_t i = 0; i < NUM_SKILLS; i++)
    {
        weightedSum = static_cast<double>(SkillRank[i]) * SkillWeightings[i] + weightedSum;
        totalWeight = totalWeight + SkillWeightings[i];
    }

    const float rankValue = static_cast<float>(weightedSum / totalWeight);

    for (int32_t i = 0; i < 4; i++)
    {
        if (rankValue < WarriorRankScale[i])
        {
            Rank = static_cast<uint8_t>(i);
            return;
        }
    }
}

auto MCMechWarrior::LoadBrainParameters(MCFitIniFile* brainFile, int32_t warriorId) -> int32_t
{
    if (Brain == nullptr)
    {
        Fatal(0, " Warrior.loadBrainParameters: NULL brain ");
    }

    char blockName[32];
    std::snprintf(blockName, sizeof(blockName), "Warrior%d", warriorId);
    int32_t result = brainFile->SeekBlock(blockName);

    if (result != 0)
    {
        return result;
    }

    int32_t numCells = 0;
    result = brainFile->ReadIdLong("NumCells", numCells);

    if (result != 0)
    {
        return result;
    }

    int32_t numStaticVars = 0;
    result = brainFile->ReadIdLong("NumStaticVars", numStaticVars);

    if (result != 0)
    {
        return result;
    }

    char sectionName[64];

    for (int32_t i = 0; i < numCells; i++)
    {
        std::snprintf(sectionName, sizeof(sectionName), "%sCell%d", blockName, i);
        result = brainFile->SeekBlock(sectionName);

        if (result != 0)
        {
            return result;
        }

        int32_t cell = 0;
        result = brainFile->ReadIdLong("Cell", cell);

        if (result != 0)
        {
            return result;
        }

        int32_t memType = 0;
        result = brainFile->ReadIdLong("MemType", memType);

        if (result != 0)
        {
            return result;
        }

        if (memType == 0)
        {
            int32_t value = 0;
            result = brainFile->ReadIdLong("Value", value);

            if (result != 0)
            {
                return result;
            }

            Memory[cell].Integer = value;
        }
        else if (memType == 1)
        {
            float value = 0.0f;
            result = brainFile->ReadIdFloat("Value", value);

            if (result != 0)
            {
                return result;
            }

            Memory[cell].Real = value;
        }
        else
        {
            return 0x29a;
        }
    }

    static int32_t integerValues[1024];
    static float realValues[1024];

    for (int32_t i = 0; i < numStaticVars; i++)
    {
        std::snprintf(sectionName, sizeof(sectionName), "%sStatic%d", blockName, i);
        result = brainFile->SeekBlock(sectionName);

        if (result != 0)
        {
            return result;
        }

        int32_t type = 0;
        result = brainFile->ReadIdLong("type", type);

        if (result != 0)
        {
            return result;
        }

        char varName[256];
        result = brainFile->ReadIdString("Name", varName, 0xff);

        if (result != 0)
        {
            return result;
        }

        switch (type)
        {
            case 0:
            {
                int32_t value = 0;
                result = brainFile->ReadIdLong("Value", value);

                if (result != 0)
                {
                    return result;
                }

                Brain->SetStaticInteger(varName, value);
                break;
            }

            case 1:
            {
                float value = 0.0f;
                result = brainFile->ReadIdFloat("Value", value);

                if (result != 0)
                {
                    return result;
                }

                Brain->SetStaticReal(varName, value);
                break;
            }

            case 2:
            {
                int32_t numValues = 0;
                result = brainFile->ReadIdLong("NumValues", numValues);

                if (result != 0)
                {
                    return result;
                }

                result = brainFile->ReadIdLongArray("Values", integerValues, static_cast<uint32_t>(numValues));

                if (result != 0)
                {
                    return result;
                }

                Brain->SetStaticIntegerArray(varName,
                                             std::span<const int32_t>(integerValues, static_cast<size_t>(numValues)));
                break;
            }

            case 3:
            {
                int32_t numValues = 0;
                result = brainFile->ReadIdLong("NumValues", numValues);

                if (result != 0)
                {
                    return result;
                }

                result = brainFile->ReadIdFloatArray("Values", realValues, static_cast<uint32_t>(numValues));

                if (result != 0)
                {
                    return result;
                }

                Brain->SetStaticRealArray(varName, std::span<const float>(realValues, static_cast<size_t>(numValues)));
                break;
            }

            default:
                return 0x29b;
        }
    }

    return 0;
}

//---------------------------------------------------------------------------
// MechWarriorManager

auto MCMechWarriorManager::Destroy() -> void
{
    for (int32_t i = 0; i < NumWarriors; i++)
    {
        MCMechWarrior* warrior = Warriors[i];

        if (warrior != nullptr)
        {
            warrior->Destroy();
            delete warrior;
            Warriors[i] = nullptr;
        }
    }

    Warriors.reset();
    NumWarriors = 0;
}

auto MCMechWarriorManager::Init(int32_t newNumWarriors) -> void
{
    Warriors = std::make_unique<MCMechWarrior*[]>(static_cast<size_t>(newNumWarriors));
    NumWarriors = newNumWarriors;
}

auto MCMechWarriorManager::Set(int32_t index, MCMechWarrior* warrior) -> void
{
    Warriors[index] = warrior;
}

auto MCMechWarriorManager::Get(int32_t index) -> MCMechWarrior*
{
    return Warriors[index];
}

//---------------------------------------------------------------------------
// WarriorStatusWindow

MCWarriorStatusWindow::~MCWarriorStatusWindow()
{
    // The inlined aTitleWindow destructor; aObject's runs after.
    MCGuiTitleWindow::Destroy();
}

auto MCWarriorStatusWindow::Init(int32_t x, int32_t y, int32_t w, int32_t h, MCMechWarrior* newWarrior) -> void
{
    MCGuiTitleWindow::Init(x, y, w, h, nullptr);
    Warrior = newWarrior;
}

auto MCWarriorStatusWindow::HandleEvent(MCGuiEvent* event) -> void
{
    MCGuiObject::HandleEvent(event);
}

auto MCWarriorStatusWindow::Resize(int32_t w, int32_t h) -> void
{
    MCGuiTitleWindow::Resize(w, h);
}

auto MCWarriorStatusWindow::Display() -> void
{
    VfxPaneWipe(DisplayPort->Frame(), BackgroundColor);

    if (Warrior != nullptr)
    {
        char text[256];
        std::snprintf(text, sizeof(text), "%s - %s", Warrior->Callsign, Warrior->Name);
        SetTitle(text);
        MCGuiPort* port = DisplayPort;
        SystemFont->WriteString(port->Frame(), 0x7d, 0x14, reinterpret_cast<uint8_t*>(const_cast<char*>("Wounds:")),
                                -1);
        std::snprintf(text, sizeof(text), "%.1f", static_cast<double>(Warrior->Wounds));
        SystemFont->WriteString(port->Frame(), 0xbc, 0x14, reinterpret_cast<uint8_t*>(text), -1);
    }

    MCGuiObject::Display();
}

auto MCWarriorStatusWindow::Draw() -> void
{
    if (Warrior != nullptr)
    {
        char text[256];
        std::snprintf(text, sizeof(text), "%s - %s", Warrior->Callsign, Warrior->Name);
        SetTitle(text);
    }

    MCGuiTitleWindow::Draw();
}
