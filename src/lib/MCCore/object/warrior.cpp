#include "stdafx.h"
#include "object/warrior.h"
#include "abl/abldbug.h"
#include "abl/ablenv.h"
#include "abl/ablxstd.h"
#include "ai/move.h"
#include "ai/tacordr.h"
#include "gui/afont.h"
#include "gui/aport.h"
#include "gui/asystem.h"
#include "iface/iface.h"
#include "lib/aerror.h"
#include "lib/cvmath.h"
#include "lib/file.h"
#include "lib/heap.h"
#include "lib/inifile.h"
#include "lib/packet.h"
#include "main/main.h"
#include "network/multplyr.h"
#include "object/cmponent.h"
#include "object/elemntl.h"
#include "object/group.h"
#include "object/gvehicl.h"
#include "object/mech.h"
#include "object/mover.h"
#include "object/objque.h"
#include "object/object.h"
#include "object/sortlist.h"
#include "object/tbldng.h"
#include "object/team.h"
#include "sound/radio.h"
#include "terrain/terrain.h"
#include "vfx/vfxfuncs.h"

// The pilot data (MCX.EXE 0x007931d4..0x00793310), in the original's order.
float FireOddsTable[5] = {20.0f, 35.0f, 50.0f, 65.0f, 80.0f};
const char* pilotAlarmFunctionName[NUM_PILOT_ALARMS] = {"handletargetofweaponfire",
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

_QueuedTacOrder TacOrderQueue[MAX_QUEUED_TACORDERS];
int32_t MechWarrior::numWarriors = 0;
int32_t MechWarrior::numWarriorsInCombat = 0;
SortList* MechWarrior::sortList = nullptr;
int32_t LastMoveCalcErr = 0;
int32_t TacOrderQueuePos = 0;
ScrollingTextWindow* GameSystemWindow = nullptr;

namespace
{
    /// <summary>Whether <paramref name="object"/> is a mover (mech, vehicle, elemental or plain mover).</summary>
    bool IsMover(const BaseObject* object)
    {
        const ObjectClass objectClass = object->objectClass;
        return objectClass == BATTLEMECH || objectClass == GROUNDVEHICLE || objectClass == ELEMENTAL ||
               objectClass == MOVER;
    }

    /// <summary>A copy of <paramref name="text"/> in systemHeap, as the original's inline strlen/malloc/strcpy.</summary>
    char* CopyString(const char* text)
    {
        char* copy = static_cast<char*>(systemHeap->malloc(static_cast<uint32_t>(std::strlen(text) + 1)));

        if (copy != nullptr)
        {
            std::strcpy(copy, text);
        }

        return copy;
    }

    /// <summary>Whether movement cell (cellR, cellC) of tile (tileR, tileC) can be entered.</summary>
    bool CellPassable(int32_t tileR, int32_t tileC, int32_t cellR, int32_t cellC)
    {
        // Port fix: the walks can leave the map, where the original reads outside it. Off the map is impassable.
        if (!GameMap->onMap(tileR, tileC))
        {
            return false;
        }

        return GameMap->map[GameMap->width * tileR + tileC].getCellPassable(cellR, cellC) != 0;
    }

    /// <summary>Whether the movement cell under <paramref name="position"/> can be entered.</summary>
    bool PositionPassable(vector_3d position)
    {
        int32_t tileR;
        int32_t tileC;
        int32_t cellR;
        int32_t cellC;
        GameMap->worldToMapPos(position, tileR, tileC, cellR, cellC);
        return CellPassable(tileR, tileC, cellR, cellC);
    }

    /// <summary>
    /// Lifts the path locks of the pilot's vehicle and, on a ramming attack, of the mover rammed, so that the path
    /// finder can plan through them (calcMovePath repeats this around each path it plans).
    /// </summary>
    void BeginPathCalc(MechWarrior* pilot, Mover* mover)
    {
        MovingObject = mover;
        mover->updatePathLock(0);

        if (pilot->curTacOrder.code == TACTICAL_ORDER_ATTACK_OBJECT && pilot->curTacOrder.attackParams.method == 2)
        {
            RamObject = pilot->curTacOrder.target;

            if (RamObject != nullptr && IsMover(RamObject))
            {
                static_cast<Mover*>(RamObject)->updatePathLock(0);
            }
        }
        else
        {
            RamObject = nullptr;
        }
    }

    /// <summary>Puts back the path locks <see cref="BeginPathCalc"/> lifted.</summary>
    void EndPathCalc(Mover* mover)
    {
        if (RamObject != nullptr && IsMover(RamObject))
        {
            static_cast<Mover*>(RamObject)->updatePathLock(1);
        }

        mover->updatePathLock(1);
        MovingObject = nullptr;
        RamObject = nullptr;
    }

    /// <summary>The flags calcMovePath adds for the path finder: 0x40, and 0x80 unless the mover is an elemental.</summary>
    uint32_t PathFinderParams(const Mover* mover, uint32_t moveParams)
    {
        if (mover->objectClass != ELEMENTAL)
        {
            moveParams |= 0x80;
        }

        return moveParams | 0x40;
    }

    /// <summary>Stores <paramref name="point"/> as a tactical order's first way point (the original's inline copy).</summary>
    void SetFirstWayPoint(TacticalOrder& order, vector_3d point)
    {
        order.moveParams.wayPath.points[0] = point.x;
        order.moveParams.wayPath.points[1] = point.y;
        order.moveParams.wayPath.points[2] = point.z;
    }

    /// <summary>The distance past its fire range a mover may stand before its attack move goes on: two vertices.</summary>
    double AttackRangeSlack()
    {
        return static_cast<double>(metersPerWorldUnit) * Terrain::metersPerVertex +
               static_cast<double>(metersPerWorldUnit) * Terrain::metersPerVertex;
    }

    /// <summary>
    /// The refit flag orderGetFixed reads at +0x130 of its target: a repair bay's mechBay. On a refit vehicle the
    /// original read the mover's ECM tracker pointer there; the port gives what that pointer compared as.
    /// </summary>
    int32_t RepairBayKind(GameObject* target)
    {
        if (target->objectClass == TREEBUILDING)
        {
            return static_cast<TreeBuilding*>(target)->mechBay;
        }

        if (IsMover(target))
        {
            return static_cast<Mover*>(target)->ecmTracker != nullptr ? 2 : 0;
        }

        return 0;
    }
}

//---------------------------------------------------------------------------
// MechWarrior

auto MechWarrior::operator new(size_t size) noexcept -> void*
{
    return systemHeap->malloc(static_cast<uint32_t>(size));
}

auto MechWarrior::operator delete(void* ptr) -> void
{
    systemHeap->free(ptr);
}

auto MechWarrior::lobotomy() -> void
{
    if (brain != nullptr)
    {
        brain->destroy();
        delete brain;
    }

    brain = nullptr;

    for (int32_t i = 0; i < NUM_PILOT_ALARMS; i++)
    {
        brainAlarmCallback[i] = nullptr;
    }
}

auto MechWarrior::init() -> void
{
    professionalism = 40;
    decorum = 40;
    aggressiveness = 40;
    courage = 40;
    name = nullptr;
    lastUnderAttackTime = -1000.0f;
    lastContactTime = -1000.0f;
    callsign = nullptr;
    picture = nullptr;
    team = nullptr;
    videoStr = nullptr;
    audioStr = nullptr;
    brainStr = nullptr;
    vehicle = nullptr;
    wounds = 0.0f;
    unknown30 = 0;
    status = 0;
    escapesThruEjection = 0;
    rank = 0;
    lastMessage = -1;
    weapons50Sent = 0;
    weaponsOutSent = 0;

    for (int32_t i = 0; i < NUM_SKILLS; i++)
    {
        numSkillUses[i][0] = 0;
        numSkillUses[i][1] = 0;
        numSkillSuccesses[i][0] = 0;
        numSkillSuccesses[i][1] = 0;
        skillRank[i] = 0.0f;
        skillPoints[i] = 0.0f;
    }

    for (int32_t i = 0; i < 7; i++)
    {
        numKilled[i][0] = 0;
        numKilled[i][1] = 0;
    }

    brain = nullptr;

    for (int32_t i = 0; i < NUM_PILOT_ALARMS; i++)
    {
        brainAlarmCallback[i] = nullptr;
    }

    // Spread the warriors' updates over the first frames.
    brainUpdateTime = static_cast<float>(static_cast<double>(numWarriors % 30) * 0.2);
    combatUpdateTime = static_cast<float>(static_cast<double>(numWarriors % 15) * 0.1);
    movementUpdateTime = static_cast<float>(static_cast<double>(numWarriors % 15) * 0.2);

    for (int32_t i = 0; i < MAX_WEAPONS_PER_WARRIOR; i++)
    {
        weaponsStatus[i] = 0;
    }

    weaponsStatusResult = -2;
    newTacOrderReceived[ORDERSTATE_GENERAL] = 0;
    newTacOrderReceived[ORDERSTATE_PLAYER] = 0;
    newTacOrderReceived[ORDERSTATE_ALARM] = 0;
    tacOrder[ORDERSTATE_GENERAL].init();
    tacOrder[ORDERSTATE_PLAYER].init();
    tacOrder[ORDERSTATE_ALARM].init();
    tacOrderQueue = nullptr;
    enableTacOrderQueue();
    playerOrderFromQueue = 0;
    tacOrderQueueLocked = 0;
    tacOrderQueueExecuting = 0;
    numTacOrdersQueued = 0;
    nextTacOrderId = 1;
    lastTacOrderId = 0;
    alarmPriority = 0;
    curTacOrder.init();
    lastTacOrder.init();
    orderState = ORDERSTATE_GENERAL;
    moveOrders.init();
    attackOrders.init();
    unknown1E24 = 1;
    unknown1E04 = 10.0f;
    unknown1E08 = 10.0f;
    unknown1DFC = 0;
    unknown1E00 = 0;
    unknown1E0C = 0;
    orderFireRange = -1.0f;
    orderFireOdds = -1.0f;
    unknown1E20 = 0;

    for (int32_t i = 0; i < 2; i++)
    {
        MovePath* path = new MovePath;

        if (path != nullptr)
        {
            // MovePath's inline constructor.
            path->goal = vector_3d(0.0f, 0.0f, 0.0f);
            path->numSteps = 0;
            path->numStepsWhenNotPaused = 0;
            path->curStep = 0;
            path->cost = 0;
            path->marked = 0;
            path->globalStep = -1;
        }

        moveOrders.path[i] = path;

        if (path == nullptr)
        {
            Fatal(0, " No RAM for warrior path ");
        }
    }

    movePathRequest = nullptr;
    lastTarget = nullptr;
    lastTargetTime = -1.0f;
    lastTargetObliterate = 0;
    lastTargetFriendly = 0;

    for (int32_t i = 0; i < NUM_PILOT_ALARMS; i++)
    {
        clearAlarm(i);
    }

    timeOfLastOrders = -1.0f;
    attackRadius = DefaultAttackRadius;

    if (sortList == nullptr)
    {
        sortList = new SortList;

        if (sortList == nullptr)
        {
            Fatal(0, " Unable to create Warrior::sortList ");
        }

        sortList->init(100);
    }

    debugFlags = 0;
    unknown1E54 = 0;
    numAttackers = 0;
    ammoOutSent = 0;
    numWarriors++;
}

auto MechWarrior::init(FitIniFile* warriorFile) -> int32_t
{
    char audioName[512];
    char videoName[512];

    int32_t result = warriorFile->seekBlock("General");

    if (result != 0)
    {
        return result;
    }

    result = warriorFile->readIdString("Name", audioName, 0x1ff);

    if (result != 0)
    {
        return result;
    }

    if (warriorFile->readIdLong("DescIndex", descIndex) != 0)
    {
        descIndex = -1;
    }

    if (warriorFile->readIdLong("NameIndex", nameIndex) != 0)
    {
        nameIndex = -1;
    }

    name = CopyString(audioName);

    if (warriorFile->readIdBoolean("NotMineYet", notMineYet) != 0)
    {
        notMineYet = 0;
    }

    if (warriorFile->readIdString("Picture", audioName, 0x1ff) == 0)
    {
        picture = CopyString(audioName);
    }
    else
    {
        picture = static_cast<char*>(systemHeap->malloc(0xb));
        std::strcpy(picture, "pilotx.gif");
    }

    result = warriorFile->readIdString("Callsign", audioName, 0x1ff);

    if (result != 0)
    {
        return result;
    }

    callsign = CopyString(audioName);

    if (warriorFile->readIdUChar("OldPilot", oldPilot) != 0)
    {
        oldPilot = 0;
    }

    radio = nullptr;

    if (warriorFile->readIdString("pilotAudio", audioName, 0x1ff) == 0)
    {
        audioStr = CopyString(audioName);

        if (warriorFile->readIdString("pilotVideo", videoName, 0x1ff) != 0)
        {
            videoName[0] = '\0';
        }

        videoStr = CopyString(videoName);

        Radio* newRadio = new Radio;

        if (newRadio != nullptr)
        {
            // Radio's inline constructor.
            newRadio->radioFile = nullptr;
            newRadio->movieName = nullptr;
            newRadio->enabled = 1;
        }

        radio = newRadio;

        if (newRadio->init(audioName, 0x19000, videoName) != 0)
        {
            delete newRadio->radioFile;
            newRadio->radioFile = nullptr;
            delete newRadio;
            radio = nullptr;
        }
    }

    if (warriorFile->readIdLong("PaintScheme", paintScheme) != 0)
    {
        paintScheme = -1;
    }

    result = warriorFile->seekBlock("PersonalityTraits");

    if (result != 0)
    {
        return result;
    }

    result = warriorFile->readIdChar("Professionalism", reinterpret_cast<char&>(professionalism));

    if (result != 0)
    {
        return result;
    }

    result = warriorFile->readIdChar("Decorum", reinterpret_cast<char&>(decorum));

    if (result != 0)
    {
        return result;
    }

    result = warriorFile->readIdChar("Aggressiveness", reinterpret_cast<char&>(aggressiveness));

    if (result != 0)
    {
        return result;
    }

    result = warriorFile->readIdChar("Courage", reinterpret_cast<char&>(courage));

    if (result != 0)
    {
        return result;
    }

    baseCourage = courage;

    result = warriorFile->seekBlock("Skills");

    if (result != 0)
    {
        return result;
    }

    for (int32_t i = 0; i < NUM_SKILLS; i++)
    {
        result = warriorFile->readIdChar(SkillsTable[i], reinterpret_cast<char&>(skills[i]));

        if (result != 0)
        {
            return result;
        }

        skillRank[i] = static_cast<float>(skills[i]);
    }

    if (warriorFile->seekBlock("OriginalSkills") == 0)
    {
        for (int32_t i = 0; i < NUM_SKILLS; i++)
        {
            if (warriorFile->readIdChar(SkillsTable[i], reinterpret_cast<char&>(originalSkills[i])) != 0)
            {
                originalSkills[i] = skills[i];
            }
        }
    }
    else
    {
        for (int32_t i = 0; i < NUM_SKILLS; i++)
        {
            originalSkills[i] = skills[i];
        }
    }

    if (warriorFile->seekBlock("LatestSkills") == 0)
    {
        for (int32_t i = 0; i < NUM_SKILLS; i++)
        {
            if (warriorFile->readIdChar(SkillsTable[i], reinterpret_cast<char&>(latestSkills[i])) != 0)
            {
                latestSkills[i] = skills[i];
            }
        }
    }
    else
    {
        for (int32_t i = 0; i < NUM_SKILLS; i++)
        {
            latestSkills[i] = skills[i];
        }
    }

    if (warriorFile->seekBlock("SkillPoints") == 0)
    {
        for (int32_t i = 0; i < NUM_SKILLS; i++)
        {
            if (warriorFile->readIdFloat(SkillsTable[i], skillPoints[i]) != 0)
            {
                skillPoints[i] = 0.0f;
            }
        }
    }
    else
    {
        for (int32_t i = 0; i < NUM_SKILLS; i++)
        {
            skillPoints[i] = 0.0f;
        }
    }

    calcRank();

    result = warriorFile->seekBlock("Status");

    if (result != 0)
    {
        return result;
    }

    char numWounds;
    result = warriorFile->readIdChar("Wounds", numWounds);

    if (result != 0)
    {
        return result;
    }

    wounds = static_cast<float>(numWounds);

    for (int32_t i = 0; i < NUM_SKILLS; i++)
    {
        numSkillUses[i][0] = 0;
        numSkillUses[i][1] = 0;
        numSkillSuccesses[i][0] = 0;
        numSkillSuccesses[i][1] = 0;
    }

    for (int32_t i = 0; i < 5; i++)
    {
        numKilled[i][0] = 0;
        numKilled[i][1] = 0;
    }

    // Whether the pilot would survive ejecting: piloting + 30 percent, at most 94.
    const int32_t roll = RandomNumber(100);
    escapesThruEjection = (roll <= skills[MWS_PILOTING] + 30 && roll < 95) ? 1 : 0;
    return 0;
}

auto _MoveOrders::init() -> void
{
    time = scenarioTime;
    origin = 1;
    speedType = 3;
    goalObjectPosition = vector_3d(0.0f, 0.0f, 0.0f);
    goalLocation = vector_3d(-999999.0f, -999999.0f, -999999.0f);
    originalGlobalGoal[0] = vector_3d(-999999.0f, -999999.0f, -999999.0f);
    originalGlobalGoal[1] = vector_3d(-999999.0f, -999999.0f, -999999.0f);
    speedVelocity = 0.0f;
    globalGoalLocation = vector_3d(-666666.0f, -666666.0f, -666666.0f);
    speedState = 2;
    speedThrottle = 100;
    goalType = -1;
    goalObject = nullptr;
    nextUpdate = 0.0f;
    scriptGoal = 0;
    numWayPts = 0;
    curWayPt = 0;
    curWayDir = 0;
    pathType = 0;
    numGlobalSteps = 0;
    curGlobalStep = 0;
    path[0] = nullptr;
    path[1] = nullptr;
    timeOfLastStep = -1.0f;
    moveState = 1;
    moveStateGoal = 1;
    unknown1030 = 0;
    yieldTime = -1.0f;
    yieldState = 0;
    waitForPointTime = -1.0f;
    run = 0;
}

auto _AttackOrders::init() -> void
{
    time = scenarioTime;
    origin = 1;
    type = 0;
    target = nullptr;
    targetPoint = vector_3d(0.0f, 0.0f, 0.0f);
    aimLocation = -1;
    pursue = 0;
    targetTime = 0.0f;
}

auto MechWarrior::radioMessage(int32_t messageId, int propogateIfMultiplayer) -> void
{
    if (messageId >= NUM_RADIO_MESSAGES || radio == nullptr || status != 0 || messageId == -1 || turn <= 0)
    {
        return;
    }

    if (underHomeCommand() == 0)
    {
        if (MPlayer != nullptr && MPlayer->isServer != 0 && propogateIfMultiplayer != 0)
        {
            static_cast<Mover*>(vehicle)->addRadioChunk(0, static_cast<uint8_t>(messageId));
        }

        return;
    }

    switch (messageId)
    {
        case RADIO_SENSOR_CONTACT:
        {
            if (static_cast<double>(scenarioTime) - 15.0 < lastContactTime)
            {
                return;
            }

            lastContactTime = scenarioTime;
            break;
        }
        case RADIO_UNDER_ATTACK:
        {
            if (static_cast<double>(scenarioTime) - 20.0 < lastUnderAttackTime)
            {
                return;
            }

            lastUnderAttackTime = scenarioTime;
            break;
        }
        case RADIO_WEAPONS_50:
        {
            if (weapons50Sent != 0)
            {
                return;
            }

            weapons50Sent = 1;
            break;
        }
        case RADIO_WEAPONS_OUT:
        {
            if (weaponsOutSent != 0)
            {
                return;
            }

            weaponsOutSent = 1;
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
            if (lastMessageType == messageId && static_cast<double>(scenarioTime) - 10.0 < lastMessageTime)
            {
                return;
            }
            break;
        }
    }

    char message[128];
    std::snprintf(message, sizeof(message), "Radio Message %d\n", messageId);
    lastMessageTime = scenarioTime;
    const int32_t played = radio->playMessage(static_cast<RadioMessageType>(messageId));
    lastMessageType = messageId;
    lastMessage = played;
}

auto MechWarrior::destroy() -> void
{
    if (name != nullptr)
    {
        systemHeap->free(name);
        name = nullptr;
    }

    if (picture != nullptr)
    {
        systemHeap->free(picture);
        picture = nullptr;
    }

    if (callsign != nullptr)
    {
        systemHeap->free(callsign);
        callsign = nullptr;
    }

    if (brain != nullptr)
    {
        brain->destroy();
        delete brain;
        brain = nullptr;
    }

    for (int32_t i = 0; i < 2; i++)
    {
        if (moveOrders.path[i] != nullptr)
        {
            moveOrders.path[i]->destroy();
            delete moveOrders.path[i];
            moveOrders.path[i] = nullptr;
        }
    }

    numWarriors--;

    if (numWarriors == 0)
    {
        if (sortList != nullptr)
        {
            sortList->destroy();
            delete sortList;
        }

        sortList = nullptr;
    }

    systemHeap->free(brainStr);
    brainStr = nullptr;
    systemHeap->free(audioStr);
    audioStr = nullptr;
    systemHeap->free(videoStr);
    videoStr = nullptr;
}

auto MechWarrior::getAggressiveness(int current) -> int32_t
{
    if (current != 0 && curTacOrder.isCombatOrder() != 0)
    {
        return (100 - aggressiveness) / 2 + aggressiveness;
    }

    return aggressiveness;
}

auto MechWarrior::enableTacOrderQueue() -> int
{
    const int32_t first = TacOrderQueuePos;

    if (MAX_QUEUED_TACORDERS - TacOrderQueuePos < MAX_QUEUED_TACORDERS_PER_WARRIOR)
    {
        return 0;
    }

    TacOrderQueuePos += MAX_QUEUED_TACORDERS_PER_WARRIOR;
    numTacOrdersQueued = 0;
    tacOrderQueue = &TacOrderQueue[first];
    return 1;
}

auto MechWarrior::addQueuedTacOrder(TacticalOrder tacOrder) -> int32_t
{
    if (tacOrderQueue == nullptr)
    {
        return 1;
    }

    if (numTacOrdersQueued == MAX_QUEUED_TACORDERS_PER_WARRIOR)
    {
        return 2;
    }

    _QueuedTacOrder& queued = tacOrderQueue[numTacOrdersQueued];
    queued.point = tacOrder.getWayPoint(0);
    queued.id = tacOrder.id;
    queued.packedData[0] = tacOrder.data[0];
    queued.packedData[1] = tacOrder.data[1];
    numTacOrdersQueued++;

    // The first order queued starts at once, unless a player order is waiting or one from the queue is running.
    if ((MPlayer == nullptr || MPlayer->isServer != 0) && numTacOrdersQueued == 1 &&
        newTacOrderReceived[ORDERSTATE_PLAYER] == 0 &&
        (playerOrderFromQueue == 0 || curTacOrder.origin != ORDER_ORIGIN_PLAYER))
    {
        executeTacOrderQueue();
    }

    return 0;
}

auto MechWarrior::removeQueuedTacOrder(TacticalOrder* tacOrder) -> int32_t
{
    if (tacOrderQueue == nullptr)
    {
        return 1;
    }

    const int32_t numQueued = numTacOrdersQueued;

    if (numQueued == 0)
    {
        return 2;
    }

    tacOrder->data[0] = tacOrderQueue[0].packedData[0];
    tacOrder->data[1] = tacOrderQueue[0].packedData[1];
    const vector_3d point = tacOrderQueue[0].point;
    const int32_t id = tacOrderQueue[0].id;

    // Port fix: the original shifted numQueued entries, reading one past the last (past the pool for the last
    // pilot's queue); the slot it filled is dropped by the count below, so only the n - 1 real moves are kept.
    for (int32_t i = 0; i < numQueued - 1; i++)
    {
        tacOrderQueue[i] = tacOrderQueue[i + 1];
    }

    numTacOrdersQueued = static_cast<int8_t>(numQueued - 1);

    tacOrder->unpack();
    tacOrder->id = id;
    tacOrder->setWayPoint(0, point);
    return 0;
}

auto MechWarrior::peekQueuedTacOrder(TacticalOrder* tacOrder) -> int32_t
{
    if (tacOrderQueue == nullptr)
    {
        return 1;
    }

    if (numTacOrdersQueued == 0)
    {
        return 2;
    }

    const vector_3d point = tacOrderQueue[0].point;
    tacOrder->data[1] = tacOrderQueue[0].packedData[1];
    tacOrder->data[0] = tacOrderQueue[0].packedData[0];
    tacOrder->unpack();
    tacOrder->setWayPoint(0, point);
    tacOrder->id = tacOrderQueue[0].id;
    return 0;
}

auto MechWarrior::clearTacOrderQueue() -> void
{
    numTacOrdersQueued = 0;
    tacOrderQueueExecuting = 0;
}

auto MechWarrior::executeTacOrderQueue() -> void
{
    if (numTacOrdersQueued > 0)
    {
        tacOrderQueueExecuting = 1;
        TacticalOrder order;
        order.init();

        if (removeQueuedTacOrder(&order) == 0)
        {
            setPlayerTacOrder(order, 1);
        }

        return;
    }

    tacOrderQueueExecuting = 0;
}

auto MechWarrior::lockTacOrderQueue() -> void
{
    tacOrderQueueLocked = 1;
}

auto MechWarrior::unlockTacOrderQueue() -> void
{
    tacOrderQueueLocked = 0;
}

auto MechWarrior::getTacOrderQueue(_QueuedTacOrder* list) -> int32_t
{
    int32_t count = 0;

    if (playerOrderFromQueue != 0)
    {
        if (list != nullptr)
        {
            TacticalOrder& playerOrder = tacOrder[ORDERSTATE_PLAYER];
            list[0].id = playerOrder.id;
            list[0].point = playerOrder.getWayPoint(0);
            list[0].packedData[0] = playerOrder.data[0];
            list[0].packedData[1] = playerOrder.data[1];
        }

        count = 1;
    }

    const int32_t numQueued = numTacOrdersQueued;

    if (numQueued > 0)
    {
        if (list != nullptr)
        {
            for (int32_t i = 0; i < numQueued; i++)
            {
                list[count + i] = tacOrderQueue[i];
            }
        }

        count += numQueued;
    }

    return count;
}

auto compareTacOrderId(int32_t id1, int32_t id2) -> int32_t
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

auto MechWarrior::updateClientOrderQueue(int32_t tacOrderId) -> void
{
    TacticalOrder order;
    order.init();
    int32_t result = peekQueuedTacOrder(&order);

    if (tacOrderId == 0)
    {
        if (result == 0 && order.id == lastTacOrderId)
        {
            removeQueuedTacOrder(&order);
        }

        return;
    }

    lastTacOrderId = tacOrderId;

    while (result == 0 && compareTacOrderId(order.id, tacOrderId) < 0)
    {
        removeQueuedTacOrder(&order);
        result = peekQueuedTacOrder(&order);
    }
}

auto MechWarrior::getGroup() -> MoverGroup*
{
    if (vehicle != nullptr)
    {
        return static_cast<Mover*>(vehicle)->group;
    }

    return nullptr;
}

auto MechWarrior::getPoint() -> Mover*
{
    if (getGroup() != nullptr)
    {
        return getGroup()->getPoint();
    }

    return nullptr;
}

auto MechWarrior::onHomeTeam() -> int
{
    return team == homeTeam ? 1 : 0;
}

auto MechWarrior::underHomeCommand() -> int
{
    if (vehicle != nullptr)
    {
        return static_cast<Mover*>(vehicle)->netPlayerId >= 0 ? 1 : 0;
    }

    return 0;
}

auto MechWarrior::checkSkill(int32_t skillId, float factor) -> int32_t
{
    numSkillUses[skillId][1]++;
    skillPoints[skillId] = SkillTry[skillId] + skillPoints[skillId];
    const int32_t roll = RandomNumber(100);
    const int32_t margin = static_cast<int32_t>(static_cast<double>(skills[skillId]) * factor) - roll - 1;

    if (margin >= 0 && skillId != MWS_SENSORS)
    {
        numSkillSuccesses[skillId][1]++;
        skillPoints[skillId] = SkillSuccess[skillId] + skillPoints[skillId];
    }

    return margin;
}

auto MechWarrior::injure(float numWounds, int checkEject) -> int
{
    if (status != 0)
    {
        return 0;
    }

    if (numWounds > 0.0f)
    {
        radioMessage(RADIO_PILOT_HURT, 0);
    }

    wounds = numWounds + wounds;

    if (static_cast<double>(wounds) < 6.0)
    {
        return 0;
    }

    Mover* mover = static_cast<Mover*>(vehicle);
    Assert(mover != nullptr, 0, " Pilot has no vehicle ");

    if (checkEject != 0)
    {
        float points = SkillTry[MWS_PILOTING] + skillPoints[MWS_PILOTING];
        skillPoints[MWS_PILOTING] = points;
        numSkillUses[MWS_PILOTING][1]++;

        if (escapesThruEjection != 0)
        {
            points = points + SkillSuccess[MWS_PILOTING];
            wounds = 5.0f;
            numSkillSuccesses[MWS_PILOTING][1]++;
            skillPoints[MWS_PILOTING] = points;

            if (mover->handleEjection() == 0)
            {
                wounds = 6.0f;
            }
        }
    }

    if (static_cast<double>(wounds) >= 6.0)
    {
        radioMessage(RADIO_DEATH, 0);
        status = 4;
    }

    if (mover != nullptr)
    {
        mover->disable(2);
    }

    if (getGroup() != nullptr)
    {
        getGroup()->handleMateDestroyed(static_cast<uint32_t>(mover->partId));
    }

    if (radio != nullptr)
    {
        radio->enabled = 0;
    }

    return 1;
}

auto MechWarrior::eject() -> void
{
    if (status != 0 && status != 1)
    {
        return;
    }

    if (wounds < 6.0f)
    {
        wounds = wounds + 1.0f;
    }

    if (wounds < 6.0f)
    {
        radioMessage(RADIO_EJECTING, 0);
        status = 3;
    }
    else
    {
        radioMessage(RADIO_DEATH, 0);
        status = 4;
    }

    Mover* mover = static_cast<Mover*>(vehicle);

    if (mover != nullptr)
    {
        mover->disable(3);
    }

    if (getGroup() != nullptr)
    {
        getGroup()->handleMateEjected(static_cast<uint32_t>(mover->partId));
    }

    if (radio != nullptr)
    {
        radio->enabled = 0;
    }
}

auto MechWarrior::setTeam(Team* newTeam) -> void
{
    team = newTeam;
    alignment = static_cast<int8_t>(newTeam->alignment);
}

auto MechWarrior::setVehicle(GameObject* newVehicle) -> void
{
    const ObjectClass objectClass = newVehicle->objectClass;

    if (objectClass != BATTLEMECH && objectClass != GROUNDVEHICLE && objectClass != ELEMENTAL && objectClass != MOVER)
    {
        Fatal(0, " bad vehicle type ");
    }

    vehicle = newVehicle;

    if (radio != nullptr)
    {
        radio->owner = this;
    }
}

auto MechWarrior::setBrainName(char* brainName) -> void
{
    brainStr = CopyString(brainName);
}

auto MechWarrior::setBrain(int32_t brainHandle) -> int32_t
{
    if (brain != nullptr)
    {
        brain->destroy();
        delete brain;
        brain = nullptr;

        for (int32_t i = 0; i < NUM_PILOT_ALARMS; i++)
        {
            brainAlarmCallback[i] = nullptr;
        }
    }

    if (brainHandle < 0)
    {
        return 0;
    }

    ABLModule* newBrain = new ABLModule;
    brain = newBrain;
    const int32_t result = newBrain->init(brainHandle);

    if (result != 0)
    {
        return result;
    }

    char brainName[500];
    std::snprintf(brainName, sizeof(brainName), "Pilot %s", name);
    newBrain->setName(brainName);

    for (int32_t i = 0; i < NUM_PILOT_ALARMS; i++)
    {
        brainAlarmCallback[i] = brain->findFunction(const_cast<char*>(pilotAlarmFunctionName[i]), 1);
    }

    return 0;
}

auto MechWarrior::runBrain() -> int32_t
{
    if (brain == nullptr)
    {
        return 0;
    }

    IsUnitOrder = 0;
    CurGroup = getGroup();
    CurObject = vehicle;
    ABLModule* module = brain;
    CurObjectClass = CurObject->objectClass;
    CurContact = nullptr;
    CurWarrior = this;
    module->execute(nullptr);
    IsUnitOrder = 0;
    CurGroup = nullptr;
    CurObject = nullptr;
    CurObjectClass = 0;
    CurWarrior = nullptr;
    CurContact = nullptr;
    return module->returnVal;
}

auto MechWarrior::getVehicleStatus() -> int32_t
{
    if (vehicle != nullptr)
    {
        return static_cast<uint8_t>(vehicle->status);
    }

    return -1;
}

auto MechWarrior::updateAttackerStatus(uint32_t attackerId, float time) -> void
{
    int32_t index = 0;

    while (index < numAttackers && attackers[index].attackerId != attackerId)
    {
        index++;
    }

    if (index == numAttackers)
    {
        if (numAttackers == MAX_ATTACKERS)
        {
            return;
        }

        attackers[numAttackers].attackerId = attackerId;
        numAttackers++;
    }

    attackers[index].lastTime = time;
}

auto MechWarrior::getAttackerInfo(uint32_t attackerId) -> _AttackerRec*
{
    for (int32_t i = 0; i < numAttackers; i++)
    {
        if (attackers[i].attackerId == attackerId)
        {
            return &attackers[i];
        }
    }

    return nullptr;
}

auto MechWarrior::getAttackers(uint32_t* attackerList, float seconds) -> int32_t
{
    const float since = scenarioTime - seconds;
    int32_t count = 0;

    for (int32_t i = 0; i < numAttackers; i++)
    {
        if (since <= attackers[i].lastTime)
        {
            attackerList[count++] = attackers[i].attackerId;
        }
    }

    return count;
}

auto MechWarrior::setAttackTarget(GameObject* object) -> int32_t
{
    attackOrders.target = object;
    attackOrders.targetTime = scenarioTime;
    return 0;
}

auto MechWarrior::getLastTarget() -> GameObject*
{
    GameObject* target = lastTarget;

    if (target == nullptr)
    {
        return nullptr;
    }

    if (target->isDestroyed() == 0 && (target->isDisabled() == 0 || lastTargetObliterate != 0) &&
        (target->getAlignment() != alignment || lastTargetFriendly != 0))
    {
        if (lastTargetConserveAmmo != 0)
        {
            curTacOrder.attackParams.type = 3;
        }

        return target;
    }

    // Dead, disabled or friendly: forget it (and an attack order on it).
    setLastTarget(nullptr, 0, 0);
    lastTargetTime = -1.0f;
    lastTargetObliterate = 0;
    lastTargetFriendly = 0;

    if (curTacOrder.isCombatOrder() != 0)
    {
        clearCurTacOrder(1, 0);
    }

    return nullptr;
}

auto MechWarrior::setLastTarget(GameObject* target, int obliterate, int conserveAmmo) -> void
{
    if (vehicle != nullptr && static_cast<Mover*>(vehicle)->netPlayerId > -1)
    {
        if (lastTarget != nullptr && lastTarget->getObjectType() != nullptr)
        {
            lastTarget->decrementAttackers();
        }

        if (target != nullptr)
        {
            target->incrementAttackers();
        }
    }

    lastTarget = target;

    if (target == nullptr)
    {
        lastTargetFriendly = 0;
    }
    else
    {
        lastTargetFriendly = target->getAlignment() == alignment ? 1 : 0;
    }

    lastTargetTime = scenarioTime;
    lastTargetObliterate = obliterate;
    lastTargetConserveAmmo = conserveAmmo;
}

auto MechWarrior::setCurrentTarget(GameObject* target) -> void
{
    setLastTarget(target, 0, 0);
}

auto MechWarrior::getAttackTargetPosition(vector_3d& pos) -> GameObject*
{
    GameObject* target = attackOrders.target;

    if (target == nullptr)
    {
        clearAttackOrders();
        return nullptr;
    }

    pos = target->getPosition();
    return target;
}

auto MechWarrior::clearAttackOrders() -> void
{
    attackOrders.origin = 1;
    attackOrders.type = 0;
    attackOrders.target = nullptr;
    attackOrders.aimLocation = -1;
    attackOrders.pursue = 0;
    attackOrders.targetTime = -1.0f;
}

auto MechWarrior::clearMoveOrders() -> void
{
    setMoveGoal(0xffffffff, nullptr, nullptr);
    setMoveWayPath(nullptr, 0);

    for (int32_t i = 0; i < 2; i++)
    {
        if (moveOrders.path[i] != nullptr)
        {
            moveOrders.path[i]->clear();
        }
    }

    moveOrders.moveState = 1;
    moveOrders.moveStateGoal = 1;
    moveOrders.yieldState = 0;
    moveOrders.unknown1030 = 0;
    moveOrders.yieldTime = -1.0f;
    moveOrders.waitForPointTime = -1.0f;
    moveOrders.timeOfLastStep = -1.0f;
    setMoveGlobalPath(nullptr, 0);
    PathManager->remove(this);
}

auto MechWarrior::setMoveGoal(uint32_t type, vector_3d* location, GameObject* obj) -> int32_t
{
    moveOrders.goalType = static_cast<int32_t>(type);

    if (type == 0)
    {
        if (static_cast<double>(location->z) < -10.0)
        {
            location->z = land->getTerrainElevation(*location);
        }

        moveOrders.goalLocation = *location;
        moveOrders.goalObject = nullptr;
        return 0;
    }

    if (type != 0xffffffff)
    {
        if (static_cast<double>(location->z) < -10.0)
        {
            location->z = land->getTerrainElevation(*location);
        }

        moveOrders.goalLocation = *location;

        if (obj == nullptr)
        {
            obj = static_cast<GameObject*>(objectList->findObjectFromPart(static_cast<int32_t>(type)));
        }

        moveOrders.goalObject = obj;
        return 0;
    }

    moveOrders.origin = 1;
    moveOrders.goalType = -1;
    moveOrders.goalObject = nullptr;
    moveOrders.goalLocation = vector_3d(-999999.0f, -999999.0f, -999999.0f);
    return 0;
}

auto MechWarrior::pausePath() -> void
{
    if (moveOrders.path[0] != nullptr)
    {
        moveOrders.path[0]->numSteps = 0;
    }
}

auto MechWarrior::resumePath() -> void
{
    MovePath* path = moveOrders.path[0];

    if (path != nullptr)
    {
        path->numSteps = path->numStepsWhenNotPaused;
    }
}

auto MechWarrior::reachedPathEnd() -> void
{
    vector_3d nextPoint;
    const int haveNextPoint = getNextWayPoint(nextPoint, 0);

    if (moveOrders.pathType == 1)
    {
        if (haveNextPoint != 0)
        {
            const int32_t selectionIndex = curTacOrder.selectionIndex;
            moveOrders.path[0]->numSteps = 0;
            requestMovePath(selectionIndex, 0x281, 1);
            return;
        }
    }
    else
    {
        if (moveOrders.pathType != 2)
        {
            return;
        }

        if (moveOrders.path[0]->globalStep != moveOrders.numGlobalSteps - 1)
        {
            // On to the next leg of the global path.
            const int32_t selectionIndex = curTacOrder.selectionIndex;
            moveOrders.path[0]->numSteps = 0;
            requestMovePath(selectionIndex, 0x281, 2);
            return;
        }

        if (haveNextPoint != 0)
        {
            return;
        }
    }

    clearMoveOrders();

    if (curTacOrder.isMoveOrder() != 0 || curTacOrder.isWayPathOrder() != 0)
    {
        clearCurTacOrder(1, 0);
    }

    triggerAlarm(PILOT_ALARM_NO_MOVEPATH, static_cast<uint32_t>(-9));
}

auto MechWarrior::getMoveDistanceLeft() -> float
{
    MovePath* path = moveOrders.path[0];
    float distance = 0.0f;

    if (path != nullptr && path->numStepsWhenNotPaused > 0)
    {
        distance = path->getDistanceLeft(getVehicle()->getPosition(), -1);

        if (moveOrders.pathType == 2)
        {
            distance = distance + static_cast<float>(moveOrders.globalPath[path->globalStep].costToGoal);
        }
    }

    return distance;
}

auto MechWarrior::isJumping(vector_3d* jumpGoal) -> int
{
    if (vehicle != nullptr)
    {
        return static_cast<Mover*>(vehicle)->isJumping(jumpGoal);
    }

    return 0;
}

auto MechWarrior::getMovePath() -> MovePath*
{
    MovePath* donePath = moveOrders.path[0];
    Assert(moveOrders.path[0] != nullptr && moveOrders.path[1] != nullptr, 0, " NULL move paths ");

    if (donePath->numStepsWhenNotPaused != 0)
    {
        return moveOrders.path[0];
    }

    // The path walked is done: the next leg (if planned) becomes the current one.
    if (moveOrders.path[0] != nullptr)
    {
        moveOrders.path[0]->clear();
    }

    MovePath* path = moveOrders.path[1];
    moveOrders.path[1] = donePath;
    moveOrders.path[0] = path;

    if (path->numStepsWhenNotPaused <= 0)
    {
        return moveOrders.path[0];
    }

    GameObject* goalObject = moveOrders.goalObject;
    const int32_t goalType = moveOrders.goalType;

    if (goalType == -1)
    {
        path->numStepsWhenNotPaused = 0;
        return path;
    }

    BaseObject* goal = nullptr;

    if (goalType != 0)
    {
        goal = goalObject;

        if (goal == nullptr)
        {
            goal = objectList->findObjectFromPart(goalType);
        }

        if (goal == nullptr)
        {
            path->numStepsWhenNotPaused = 0;
            return path;
        }

        path->target = static_cast<GameObject*>(goal)->getPosition();
    }

    if (static_cast<double>(moveOrders.yieldTime) <= -1.0)
    {
        if (static_cast<double>(moveOrders.waitForPointTime) <= -1.0)
        {
            moveOrders.yieldTime = -1.0f;
            moveOrders.yieldState = 0;
        }
        else
        {
            path->numSteps = 0;
        }
    }
    else
    {
        path->numSteps = 0;
        moveOrders.yieldTime = static_cast<float>(static_cast<double>(scenarioTime) + 1.5);
    }

    setMoveGoal(goal != nullptr ? static_cast<uint32_t>(goal->partId) : 0, &path->goal, nullptr);
    MovePath* curPath = moveOrders.path[0];

    if (curPath->globalStep == moveOrders.numGlobalSteps - 1)
    {
        moveOrders.globalGoalLocation = curPath->stepList[curPath->numStepsWhenNotPaused - 1].destination;
        curTacOrder.setWayPoint(0, moveOrders.globalGoalLocation);
    }

    return moveOrders.path[0];
}

auto MechWarrior::setMoveWayPath(_WayPath* wayPath, int patrol) -> void
{
    if (wayPath == nullptr)
    {
        moveOrders.numWayPts = 0;
    }
    else
    {
        for (int32_t i = 0; i < wayPath->numPoints; i++)
        {
            moveOrders.wayPath[i] =
                vector_3d(wayPath->points[i * 3], wayPath->points[i * 3 + 1], wayPath->points[i * 3 + 2]);
        }

        moveOrders.numWayPts = static_cast<int8_t>(wayPath->numPoints);
    }

    if (InitWayPath != 0)
    {
        moveOrders.curWayPt = 0;
        moveOrders.curWayDir = patrol != 0 ? 1 : 0;
    }
}

auto MechWarrior::addMoveWayPoint(vector_3d wayPt, int patrol) -> void
{
    moveOrders.wayPath[moveOrders.numWayPts] = wayPt;
    moveOrders.numWayPts++;

    if (moveOrders.numWayPts == 1)
    {
        moveOrders.curWayDir = patrol != 0 ? 1 : 0;
    }
}

auto MechWarrior::setMoveGlobalPath(_GlobalPathStep* path, int32_t numSteps) -> void
{
    if (numSteps > MAX_GLOBAL_PATH)
    {
        Fatal(0, " Global Path Too Long ");
    }

    if (numSteps > 0)
    {
        std::memcpy(moveOrders.globalPath, path, static_cast<size_t>(numSteps) * sizeof(GlobalPathStep));
    }

    moveOrders.numGlobalSteps = static_cast<int8_t>(numSteps);
    moveOrders.curGlobalStep = 0;
}

auto MechWarrior::requestMovePath(int32_t selectionIndex, uint32_t moveParams, int32_t source) -> void
{
    PathManager->request(this, selectionIndex, moveParams, 255.0f, source);
}

auto MechWarrior::calcMovePath(int32_t selectionIndex, uint32_t moveParams, int32_t source) -> int32_t
{
    Mover* mover = static_cast<Mover*>(vehicle);

    // Start where the vehicle stands (its last valid position when on a blocked cell), or where it lands.
    vector_3d start;
    vector_3d jumpGoal;

    if (isJumping(&jumpGoal) == 0)
    {
        // The original tests for a queued jump order here, but both branches read the same position.
        start = mover->getPosition();
        const _ObjectPosition* position = mover->getObjPosition();

        if (!CellPassable(position->tileR, position->tileC, position->cellR, position->cellC))
        {
            start = mover->lastValidPosition;
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
    GameMap->worldToMapPos(start, startTileR, startTileC, startCellR, startCellC);
    const int32_t startArea = GlobalMoveMap->calcArea(startTileR, startTileC);

    const uint32_t escapeTile = (moveParams >> 13) & 1;
    GameObject* goalObject = moveOrders.goalObject;
    vector_3d goal = moveOrders.goalLocation;

    if (moveOrders.goalType == -1)
    {
        LastMoveCalcErr = -1;
        triggerAlarm(PILOT_ALARM_NO_MOVEPATH, static_cast<uint32_t>(-1));
        return LastMoveCalcErr;
    }

    GameObject* goalObj = nullptr;

    if (moveOrders.goalType != 0)
    {
        if (goalObject == nullptr)
        {
            goalObject = static_cast<GameObject*>(objectList->findObjectFromPart(moveOrders.goalType));
        }

        goalObj = goalObject;

        if (goalObj == nullptr)
        {
            LastMoveCalcErr = -2;
            triggerAlarm(PILOT_ALARM_NO_MOVEPATH, static_cast<uint32_t>(-2));
            return LastMoveCalcErr;
        }
    }

    // Which of the two paths to plan: the current one (0) or, while one is walked, the next leg (1).
    int32_t pathNum = -1;

    if ((moveParams & 0x100) != 0)
    {
        moveOrders.originalGlobalGoal[0] = goal;
        moveOrders.pathType = 0;
        moveOrders.numGlobalSteps = 0;
        const int32_t stateGoal = moveOrders.moveStateGoal;

        if (stateGoal == 3 || stateGoal == 4 || stateGoal == 5)
        {
            moveOrders.moveState = 1;
            moveOrders.moveStateGoal = 1;
        }

        pathNum = 0;
    }

    const bool yielding = static_cast<double>(moveOrders.yieldTime) > -1.0;

    if ((moveParams & 0x200) != 0)
    {
        // Start over toward the original goal.
        if (goalObj == nullptr)
        {
            setMoveGoal(0, &moveOrders.originalGlobalGoal[0], nullptr);
        }
        else
        {
            vector_3d goalPosition = goalObj->getPosition();
            setMoveGoal(static_cast<uint32_t>(goalObj->partId), &goalPosition, goalObj);
        }

        goal = moveOrders.goalLocation;

        for (int32_t i = 0; i < 2; i++)
        {
            if (moveOrders.path[i] != nullptr)
            {
                moveOrders.path[i]->clear();
            }
        }

        moveOrders.pathType = 0;
        moveOrders.numGlobalSteps = 0;
        moveOrders.moveState = 1;
        moveOrders.moveStateGoal = 1;
        pathNum = 0;
    }

    if (static_cast<double>(goal.x) < -666000.0)
    {
        LastMoveCalcErr = 0;
        return 0;
    }

    if (pathNum == -1)
    {
        pathNum = moveOrders.path[0]->numStepsWhenNotPaused != 0 ? 1 : 0;
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

    if (moveOrders.pathType != 0)
    {
        if (moveOrders.pathType == 2)
        {
            moveOrders.curGlobalStep++;
        }

        next = Next::GlobalLeg;
    }
    else
    {
        if (escapeTile == 0)
        {
            if (mover->netPlayerId > -1 && curTacOrder.code != TACTICAL_ORDER_NONE &&
                curTacOrder.origin == ORDER_ORIGIN_PLAYER)
            {
                moveParams |= 0x800;
            }

            if (mover->calcMoveGoal(goalObj, goal, 6, 6, 6, selectionIndex, goal, moveParams) != 0)
            {
                LastMoveCalcErr = -3;
                triggerAlarm(PILOT_ALARM_NO_MOVEPATH, static_cast<uint32_t>(-3));
                return LastMoveCalcErr;
            }
        }

        moveOrders.originalGlobalGoal[1] = goal;

        if (escapeTile != 0)
        {
            // Escape from a blocked cell: the nearest open cell toward the goal.
            Assert(pathNum == 0, static_cast<uint32_t>(pathNum),
                   " Warrior.calcMovePath: escapePath should be pathNum 0 ");
            moveOrders.pathType = 1;
            BeginPathCalc(this, mover);
            vector_3d escapeGoal;
            MovePath* path = moveOrders.path[pathNum];
            numSteps =
                mover->calcEscapePath(path, start, goal, nullptr, PathFinderParams(mover, moveParams), escapeGoal);
            EndPathCalc(mover);

            if (numSteps < 1)
            {
                LastMoveCalcErr = -5;
                triggerAlarm(PILOT_ALARM_NO_MOVEPATH, static_cast<uint32_t>(-5));
                return LastMoveCalcErr;
            }

            path->numSteps = numSteps;
            path->numStepsWhenNotPaused = numSteps;
            moveOrders.globalGoalLocation = path->stepList[numSteps - 1].destination;
            curTacOrder.setWayPoint(0, moveOrders.globalGoalLocation);
            uint32_t goalId = 0;

            if (goalObj != nullptr)
            {
                moveOrders.path[pathNum]->target = goalObj->getPosition();
                goalId = static_cast<uint32_t>(goalObj->partId);
            }

            setMoveGoal(goalId, &goal, nullptr);
            moveOrders.nextUpdate = MovementUpdateFrequency + scenarioTime;

            if (pathNum == 0)
            {
                if (yielding)
                {
                    LastMoveCalcErr = 0;
                    moveOrders.yieldTime = static_cast<float>(static_cast<double>(scenarioTime) + 1.5);
                    moveOrders.path[0]->numSteps = 0;
                    return 0;
                }

                if (static_cast<double>(moveOrders.waitForPointTime) > -1.0)
                {
                    LastMoveCalcErr = 0;
                    moveOrders.path[0]->numSteps = 0;
                    return 0;
                }

                moveOrders.yieldTime = -1.0f;
                moveOrders.yieldState = 0;
            }

            LastMoveCalcErr = 0;
            return 0;
        }

        if (mover->distanceFrom(goal) < MoveMarginOfError[1] && (moveParams & 1) == 0)
        {
            // Already there.
            moveOrders.origin = 1;
            moveOrders.goalType = -1;
            moveOrders.goalObject = nullptr;
            moveOrders.goalLocation = vector_3d(-999999.0f, -999999.0f, -999999.0f);
            LastMoveCalcErr = -4;
            triggerAlarm(PILOT_ALARM_NO_MOVEPATH, static_cast<uint32_t>(-4));
            return LastMoveCalcErr;
        }

        int32_t goalTileR;
        int32_t goalTileC;
        int32_t goalCellR;
        int32_t goalCellC;
        GameMap->worldToMapPos(goal, goalTileR, goalTileC, goalCellR, goalCellC);
        bool simple = std::abs(goalTileR - startTileR) <= SimpleMovePathRange &&
                      std::abs(goalTileC - startTileC) <= SimpleMovePathRange;
        const int32_t longRange = LongRangeMovementEnabled[team->id];
        const bool startAreaOpen = startArea >= 0 && GlobalMoveMap->areas[startArea].closed == 0;

        next = Next::GlobalPath;
        bool planLocal = simple;

        if (!simple && longRange == 0)
        {
            // Without long range movement, head SimpleMovePathRange tiles toward the goal.
            const float facing = mover->relFacingTo(goal, -1);
            const float range = static_cast<float>(static_cast<double>(SimpleMovePathRange) * metersPerWorldUnit *
                                                   Terrain::metersPerVertex);
            goal = mover->relativePosition(-facing, range, 2);
            moveOrders.originalGlobalGoal[1] = goal;
            GameMap->worldToMapPos(goal, goalTileR, goalTileC, goalCellR, goalCellC);
            simple = true;
            planLocal = true;
        }

        if (planLocal)
        {
            moveOrders.pathType = 1;
            BeginPathCalc(this, mover);
            numSteps = mover->calcMovePath(moveOrders.path[pathNum], 1, start, goal, nullptr,
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
                    triggerAlarm(PILOT_ALARM_NO_MOVEPATH, static_cast<uint32_t>(-5));
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
                MovePath* path = moveOrders.path[pathNum];
                moveOrders.globalGoalLocation = path->stepList[numSteps - 1].destination;
                path->numSteps = numSteps;
                path->numStepsWhenNotPaused = numSteps;
                curTacOrder.setWayPoint(0, moveOrders.globalGoalLocation);
                uint32_t goalId = 0;

                if (goalObj != nullptr)
                {
                    moveOrders.path[pathNum]->target = goalObj->getPosition();
                    goalId = static_cast<uint32_t>(goalObj->partId);
                }

                setMoveGoal(goalId, &goal, nullptr);
                moveOrders.nextUpdate = MovementUpdateFrequency + scenarioTime;
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
                TacticalOrder alarmOrder;
                alarmOrder.init();
                alarmOrder.init(ORDER_ORIGIN_SELF, TACTICAL_ORDER_MOVETO_POINT, 0);
                alarmOrder.setWayPoint(0, goal);
                alarmOrder.moveParams.wayPath.mode[0] = moveOrders.run != 0 ? 1 : 0;
                alarmOrder.moveParams.escapeTile = 1;
                alarmOrder.moveParams.wait = 0;
                setAlarmTacOrder(alarmOrder, 255);
                LastMoveCalcErr = -13;
                triggerAlarm(PILOT_ALARM_NO_MOVEPATH, static_cast<uint32_t>(-13));
                return LastMoveCalcErr;
            }

            // A global path: area by area through the doors.
            moveOrders.globalGoalLocation = goal;
            curTacOrder.setWayPoint(0, moveOrders.globalGoalLocation);
            const int32_t goalArea = GlobalMoveMap->calcArea(goalTileR, goalTileC);
            int32_t numGlobalSteps = -1;

            if (startAreaOpen)
            {
                numGlobalSteps = GlobalMoveMap->calcPath(startArea, goalArea, moveOrders.globalPath);
            }

            if (numGlobalSteps == -1)
            {
                Assert(pathNum == 0 || pathNum == 1, static_cast<uint32_t>(pathNum),
                       " Warrior.calcMovePath: pathNum should be 0 or 1 in Line 2157 ");
                TacticalOrder alarmOrder;
                alarmOrder.init();
                alarmOrder.init(ORDER_ORIGIN_SELF, TACTICAL_ORDER_MOVETO_POINT, 0);
                alarmOrder.setWayPoint(0, goal);
                alarmOrder.moveParams.wayPath.mode[0] = moveOrders.run != 0 ? 1 : 0;
                alarmOrder.moveParams.escapeTile = 1;
                alarmOrder.moveParams.wait = 0;
                setAlarmTacOrder(alarmOrder, 255);
                LastMoveCalcErr = -13;
                triggerAlarm(PILOT_ALARM_NO_MOVEPATH, static_cast<uint32_t>(-13));
                return LastMoveCalcErr;
            }

            if (numGlobalSteps == 0)
            {
                clearMoveOrders();
                LastMoveCalcErr = -7;
                triggerAlarm(PILOT_ALARM_NO_MOVEPATH, static_cast<uint32_t>(-7));

                if ((moveParams & 0x1000) != 0)
                {
                    radioMessage(RADIO_MOVE_BLOCKED, 1);
                }

                return LastMoveCalcErr;
            }

            moveOrders.pathType = 2;
            moveOrders.numGlobalSteps = static_cast<int8_t>(numGlobalSteps);
            moveOrders.curGlobalStep = 0;
            next = Next::GlobalLeg;
        }
    }

    if (next == Next::GlobalLeg)
    {
        const int8_t pathType = moveOrders.pathType;

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
            const int32_t step = moveOrders.curGlobalStep;

            if (step == moveOrders.numGlobalSteps)
            {
                LastMoveCalcErr = 0;
                return 0;
            }

            if (step != 0)
            {
                GlobalPathStep prevStep = moveOrders.globalPath[step - 1];
                start = GlobalMoveMap->getDoorWorldPos(-1, -1, prevStep.goalCell);
            }

            const int32_t lastStep = moveOrders.numGlobalSteps - 1;
            GlobalPathStep* curStep = &moveOrders.globalPath[step];

            if (step < lastStep && GlobalMoveMap->doors[curStep->goalDoor].open == 0)
            {
                // The door out is shut: plan again from the start.
                LastMoveCalcErr = -11;
                setMoveWayPath(nullptr, 0);
                moveOrders.timeOfLastStep = scenarioTime;
                setMoveGlobalPath(nullptr, 0);
                PathManager->request(this, selectionIndex, 0x201, 255.0f, source);
                triggerAlarm(PILOT_ALARM_NO_MOVEPATH, static_cast<uint32_t>(LastMoveCalcErr));
                return LastMoveCalcErr;
            }

            if (moveOrders.path[pathNum] != nullptr)
            {
                moveOrders.path[pathNum]->clear();
            }

            bool trimFailed = false;

            if (step < lastStep)
            {
                BeginPathCalc(this, mover);
                numSteps = mover->calcMovePath(moveOrders.path[pathNum], start, curStep->thruArea, curStep->goalDoor,
                                               moveOrders.globalGoalLocation, &goal, curStep->goalCell,
                                               PathFinderParams(mover, moveParams));
                EndPathCalc(mover);
            }
            else
            {
                goal = moveOrders.originalGlobalGoal[1];
                BeginPathCalc(this, mover);
                numSteps = mover->calcMovePath(moveOrders.path[pathNum], 2, start, goal, curStep->goalCell,
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
                        moveOrders.globalGoalLocation = moveOrders.path[0]->stepList[numSteps - 1].destination;
                        curTacOrder.setWayPoint(0, moveOrders.globalGoalLocation);
                    }
                }
            }

            if (trimFailed)
            {
                RamObject = nullptr;
                MovingObject = nullptr;
                clearMoveOrders();
                LastMoveCalcErr = -4;
                triggerAlarm(PILOT_ALARM_NO_MOVEPATH, static_cast<uint32_t>(-4));
                return LastMoveCalcErr;
            }

            if (numSteps < 1)
            {
                moveOrders.curGlobalStep--;
                LastMoveCalcErr = numSteps != -999 ? -8 : -12;
                triggerAlarm(PILOT_ALARM_NO_MOVEPATH, static_cast<uint32_t>(LastMoveCalcErr));
                return LastMoveCalcErr;
            }

            MovePath* path = moveOrders.path[pathNum];
            path->numSteps = numSteps;
            path->numStepsWhenNotPaused = numSteps;
            path->globalStep = step;

            if (pathNum != 0)
            {
                LastMoveCalcErr = 0;
                return 0;
            }

            uint32_t goalId = 0;

            if (goalObj != nullptr)
            {
                moveOrders.path[0]->target = goalObj->getPosition();
                goalId = static_cast<uint32_t>(goalObj->partId);
            }

            setMoveGoal(goalId, &goal, nullptr);
        }

        // The new path waits while the vehicle yields to another or waits for its point.
        if (!yielding)
        {
            if (static_cast<double>(moveOrders.waitForPointTime) <= -1.0)
            {
                moveOrders.yieldTime = -1.0f;
                moveOrders.yieldState = 0;
            }
            else
            {
                moveOrders.path[0]->numSteps = 0;
            }
        }
        else
        {
            moveOrders.yieldTime = static_cast<float>(static_cast<double>(scenarioTime) + 1.5);
            moveOrders.path[0]->numSteps = 0;
        }

        LastMoveCalcErr = 0;
        return 0;
    }

    // next == Next::TrimFailed: the group's trail cut the whole path.
    RamObject = nullptr;
    MovingObject = nullptr;
    clearMoveOrders();
    LastMoveCalcErr = -4;
    triggerAlarm(PILOT_ALARM_NO_MOVEPATH, static_cast<uint32_t>(-4));
    return LastMoveCalcErr;
}

auto MechWarrior::getNextWayPoint(vector_3d& nextPoint, int incWayPoint) -> int
{
    TacticalOrder order;
    order.init();

    if (peekQueuedTacOrder(&order) == 0 && order.code == TACTICAL_ORDER_MOVETO_POINT)
    {
        nextPoint = order.getWayPoint(0);
        return 1;
    }

    return 0;
}

auto MechWarrior::calcWeaponsStatus(GameObject* target, int32_t* weaponList, vector_3d* targetPoint) -> int32_t
{
    Mover* mover = static_cast<Mover*>(vehicle);

    if (mover->canFireWeapons() == 0)
    {
        return -1;
    }

    vector_3d targetPosition;

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
        targetPosition = target->getPosition();
    }

    const float distance = mover->distanceFrom(targetPosition);

    if (mover->getMaxFireRange() < distance)
    {
        return -3;
    }

    const int32_t aggressivenessModifier = (getAggressiveness(1) - 50) / 5;
    int32_t numReady = 0;

    for (int32_t i = 0; i < mover->numWeapons; i++)
    {
        const int32_t weaponIndex = mover->numOther + i;

        if (mover->isWeaponReady(weaponIndex) == 0)
        {
            weaponList[i] = -1;
        }
        else if (mover->getWeaponShots(weaponIndex) < 1)
        {
            weaponList[i] = -2;
        }
        else if (mover->weaponInRange(weaponIndex, distance) == 0)
        {
            weaponList[i] = -3;
        }
        else
        {
            const float lock = mover->weaponLocked(weaponIndex, targetPosition);
            const float fireArc = mover->getFireArc();

            if (lock < -fireArc || fireArc < lock)
            {
                weaponList[i] = -4;
            }
            else
            {
                const int32_t aimLocation =
                    curTacOrder.isCombatOrder() != 0 ? curTacOrder.attackParams.aimLocation : -1;
                const float attackChance =
                    mover->calcAttackChance(target, aimLocation, scenarioTime, weaponIndex, 0.0f, nullptr, targetPoint);
                const float ammoLevel = mover->getWeaponAmmoLevel(weaponIndex);
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

auto MechWarrior::combatDecisionTree() -> int32_t
{
    Mover* mover = static_cast<Mover*>(vehicle);
    combatUpdateTime = CombatUpdateFrequency + scenarioTime;
    int32_t result = -1;
    Assert(mover != nullptr, 0, " Pilot has no vehicle! ");

    int outOfAmmo = 0;

    if (ammoOutSent == 0 && mover->getNumAmmoTypes() > 0)
    {
        int32_t ammoType = 0;

        do
        {
            if (mover->getAmmoTypeTotal(ammoType) == 0)
            {
                outOfAmmo = 1;
                break;
            }

            ammoType++;
        } while (ammoType < mover->getNumAmmoTypes());
    }

    GameObject* target = getLastTarget();
    vector_3d* targetPoint = nullptr;
    vector_3d attackPoint;
    int32_t attackType = 1;
    int32_t aimLocation = -1;

    if (curTacOrder.isCombatOrder() == 0)
    {
        if (lastTargetConserveAmmo != 0)
        {
            attackType = 3;
        }
    }
    else
    {
        attackType = curTacOrder.attackParams.type;
        aimLocation = curTacOrder.attackParams.aimLocation;

        if (curTacOrder.code == TACTICAL_ORDER_ATTACK_POINT)
        {
            attackPoint = attackOrders.targetPoint;
            targetPoint = &attackPoint;
        }
    }

    char message[128];

    if (target != nullptr && curTacOrder.isCombatOrder() == 0)
    {
        // A target of the pilot's own choosing is dropped once it is out of his attack radius.
        vector_3d targetPosition = target->getPosition();

        if (attackRadius < mover->distanceFrom(targetPosition))
        {
            setLastTarget(nullptr, 0, 0);
            target = nullptr;
        }
    }

    if (target == nullptr)
    {
        if (curTacOrder.code != TACTICAL_ORDER_ATTACK_POINT)
        {
            if ((debugFlags & 1) != 0)
            {
                std::snprintf(message, sizeof(message), "%s (%.2f) has no attack target.\n", callsign,
                              static_cast<double>(orderFireRange));
                debugPrint(message, 1);
            }

            return -1;
        }
    }
    else
    {
        if (target->isDestroyed() != 0)
        {
            if ((debugFlags & 1) != 0)
            {
                std::snprintf(message, sizeof(message), "%s (%.2f) has a destroyed target.\n", callsign,
                              static_cast<double>(orderFireRange));
                debugPrint(message, 1);
            }

            return -1;
        }

        if (target->isDisabled() != 0 && lastTargetObliterate == 0)
        {
            if ((debugFlags & 1) != 0)
            {
                std::snprintf(message, sizeof(message), "%s (%.2f) has a disabled target.\n", callsign,
                              static_cast<double>(orderFireRange));
                debugPrint(message, 1);
            }

            return -1;
        }
    }

    if ((debugFlags & 1) != 0)
    {
        char line[512];
        const double range = orderFireRange;
        bool print = true;

        if (mover->canFireWeapons() == 0)
        {
            std::snprintf(line, sizeof(line), "%s's (%.2f) vehicle cannot fire now.\n", callsign, range);
        }
        else if (weaponsStatusResult > 0)
        {
            print = false;
        }
        else
        {
            switch (weaponsStatusResult)
            {
                case 0:
                {
                    int32_t notReady = 0;
                    int32_t noAmmo = 0;
                    int32_t notInRange = 0;
                    int32_t notLocked = 0;
                    int32_t noChance = 0;

                    for (int32_t i = 0; i < mover->numWeapons; i++)
                    {
                        const int32_t weaponStatus = weaponsStatus[i];

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
                        callsign, range, notReady, noAmmo, notInRange, notLocked, noChance, 0);
                    break;
                }

                case -3:
                    std::snprintf(line, sizeof(line), "%s (%.2f) out of range.\n", callsign, range);
                    break;
                case -2:
                    std::snprintf(line, sizeof(line), "%s (%.2f) has no target.\n", callsign, range);
                    break;
                case -1:
                    std::snprintf(line, sizeof(line), "%s's (%.2f) vehicle cannot fire now.", callsign, range);
                    break;
                default:
                    std::snprintf(line, sizeof(line), "%s (%.2f)  cannot fire for unknown reason.\n", callsign, range);
                    break;
            }
        }

        if (print)
        {
            debugPrint(line, 1);
        }
    }

    if (attackType != 3 && outOfAmmo != 0 && ammoOutSent == 0)
    {
        radioMessage(RADIO_AMMO_OUT, 1);
        ammoOutSent = 1;
    }

    // Conserving ammo, only unlimited weapons fire; when none can, the attack is given up.
    int conserving = attackType == 3 ? 1 : 0;

    if (mover->canFireWeapons() != 0 && weaponsStatusResult > 0)
    {
        if (target != nullptr)
        {
            radioMessage(RADIO_TAUNT, 1);
        }

        const float targetTime = lastTargetTime;

        for (int32_t i = 0; i < mover->numWeapons; i++)
        {
            const int32_t weaponIndex = mover->numOther + i;

            if (weaponsStatus[i] > 0 && (attackType != 3 || mover->getWeaponShots(weaponIndex) == 9999) &&
                mover->fireWeapon(target, targetTime, weaponIndex, attackType, aimLocation, targetPoint) == 0)
            {
                conserving = 0;
            }
        }

        result = 0;
    }

    if (conserving != 0)
    {
        for (int32_t weaponIndex = mover->numOther; weaponIndex < mover->numWeapons + mover->numOther; weaponIndex++)
        {
            if (mover->getWeaponShots(weaponIndex) == 9999 && mover->isWeaponWorking(weaponIndex) != 0)
            {
                return result;
            }
        }

        radioMessage(RADIO_ILLEGAL_ORDER, 0);
        setLastTarget(nullptr, 0, 0);
        clearCurTacOrder(1, 0);
    }

    return result;
}

auto vectorOffset(vector_3d start, vector_3d end, int32_t reverse) -> vector_3d
{
    float dx;
    float dy;

    if (reverse == 0)
    {
        dx = end.x - start.x;
        dy = end.y - start.y;
    }
    else
    {
        dx = start.x - end.x;
        dy = start.y - end.y;
    }

    const float length = std::sqrt(dy * dy + dx * dx);

    if (length != 0.0f)
    {
        dx = dx / length;
        dy = dy / length;
    }

    const float stepLength = static_cast<float>(Terrain::metersPerVertexDivMAPCELL_DIM * 0.5);
    dx = dx * stepLength;
    dy = dy * stepLength;

    if (std::sqrt(dy * dy + dx * dx) == 0.0f)
    {
        return start;
    }

    const float totalDistance = (start - end).magnitude();
    const float originX = reverse == 0 ? start.x : end.x;
    const float originY = reverse == 0 ? start.y : end.y;
    float x = originX;
    float y = originY;
    float distance = 0.0f;

    // Step until the cell stepped from is open (the result lands one step past it) or the whole way is walked.
    int32_t tileR;
    int32_t tileC;
    int32_t cellR;
    int32_t cellC;
    GameMap->worldToMapPos(vector_3d(x, y, 0.0f), tileR, tileC, cellR, cellC);
    bool open = CellPassable(tileR, tileC, cellR, cellC);

    while (!open && distance < totalDistance)
    {
        GameMap->worldToMapPos(vector_3d(x, y, 0.0f), tileR, tileC, cellR, cellC);
        x = dx + x;
        y = dy + y;
        open = CellPassable(tileR, tileC, cellR, cellC);
        distance = std::sqrt((y - originY) * (y - originY) + (x - originX) * (x - originX));
    }

    const float elevation = GameMap->getTerrainElevation(vector_3d(x, y, 0.0f));
    return vector_3d(x, y, elevation);
}

auto MechWarrior::calcWithdrawGoal(float withdrawRange) -> vector_3d
{
    Mover* mover = static_cast<Mover*>(vehicle);
    vector_3d escapeVector;

    if (team == innerSphereTeam || team == alliedTeam)
    {
        escapeVector = clanTeam->calcEscapeVector(mover, withdrawRange);
    }
    else
    {
        escapeVector = innerSphereTeam->calcEscapeVector(mover, withdrawRange);
    }

    Assert(mover != nullptr, 0, " Warrior has NULL Vehicle ");

    if (std::sqrt(escapeVector.z * escapeVector.z + escapeVector.y * escapeVector.y +
                  escapeVector.x * escapeVector.x) == 0.0f)
    {
        return mover->getPosition();
    }

    // Walk out along the escape vector in half-cell steps.
    const float stepLength = static_cast<float>(Terrain::metersPerVertexDivMAPCELL_DIM * 0.5);
    escapeVector.x = escapeVector.x * stepLength;
    escapeVector.y = escapeVector.y * stepLength;
    escapeVector.z = escapeVector.z * stepLength;
    vector_3d goal = mover->getPosition();

    auto withdrawDistance = [mover](const vector_3d& point) -> double
    {
        const vector_3d offset = point - mover->getPosition();
        return std::sqrt((static_cast<double>(offset.z) * offset.z + static_cast<double>(offset.y) * offset.y) +
                         static_cast<double>(offset.x) * offset.x) *
               metersPerWorldUnit;
    };

    double distance = static_cast<float>(withdrawDistance(goal));
    int32_t lastTileR;
    int32_t lastTileC;
    GameMap->worldToMapTilePos(goal, lastTileR, lastTileC);

    while (distance < withdrawRange)
    {
        int32_t tileR;
        int32_t tileC;
        GameMap->worldToMapTilePos(goal, tileR, tileC);

        if (tileR != lastTileR || tileC != lastTileC)
        {
            // Stop at the map's edge or at a tile with no open cell.
            if (tileR < 0 || tileR >= GameMap->height || tileC < 0 || tileC >= GameMap->width)
            {
                break;
            }

            Assert(tileR < GameMap->height && tileC < GameMap->width, 0, " Map Tile out of bounds ");

            if ((GameMap->map[GameMap->width * tileR + tileC].cells & 0x55554000) == 0)
            {
                break;
            }

            lastTileR = tileR;
            lastTileC = tileC;
        }

        goal.x = goal.x + escapeVector.x;
        goal.y = escapeVector.y + goal.y;
        goal.z = escapeVector.z + goal.z;
        distance = withdrawDistance(goal);
    }

    return goal;
}

auto MechWarrior::movingOverBlownBridge() -> int
{
    if (getMovePath() == nullptr)
    {
        return 0;
    }

    const _ObjectPosition* position = static_cast<Mover*>(vehicle)->getObjPosition();
    const int32_t tileR = position->tileR;
    const int32_t tileC = position->tileC;

    if (OverlayIsBridge[GameMap->map[GameMap->width * tileR + tileC].overlay & 0x7f] != 0)
    {
        const int32_t area = GlobalMoveMap->calcArea(tileR, tileC);

        // Port fix: the original read the area table at -1 for a tile outside every area.
        if (area >= 0 && GlobalMoveMap->areas[area].closed != 0)
        {
            return 1;
        }
    }

    const int32_t bridgeArea = getMovePath()->crossesBridge(-1, 3);

    if (bridgeArea >= 0 && GlobalMoveMap->areas[bridgeArea].closed != 0)
    {
        return 1;
    }

    if (moveOrders.pathType == 2)
    {
        for (int32_t step = moveOrders.curGlobalStep; step < moveOrders.numGlobalSteps; step++)
        {
            const GlobalMapArea& area = GlobalMoveMap->areas[moveOrders.globalPath[step].thruArea];

            if (area.type != 1 && area.type != 2)
            {
                return 0;
            }

            if (area.closed != 0)
            {
                return 1;
            }
        }
    }

    return 0;
}

auto MechWarrior::movementDecisionTree() -> int
{
    // A move that makes no progress for MoveTimeOut seconds is given up.
    if (static_cast<double>(moveOrders.timeOfLastStep) > -1.0 &&
        moveOrders.timeOfLastStep < static_cast<double>(scenarioTime) - MoveTimeOut)
    {
        clearMoveOrders();

        if ((curTacOrder.isMoveOrder() != 0 || curTacOrder.isWayPathOrder() != 0) &&
            curTacOrder.time < static_cast<double>(scenarioTime) - MoveTimeOut)
        {
            radioMessage(RADIO_MOVE_BLOCKED, 1);
            clearCurTacOrder(1, 0);
        }

        triggerAlarm(PILOT_ALARM_NO_MOVEPATH, static_cast<uint32_t>(-10));
    }

    Mover* mover = static_cast<Mover*>(vehicle);

    // An elemental that can't jump drops a path through a closed gate.
    if (mover->objectClass == ELEMENTAL && static_cast<Elemental*>(mover)->elementalCanJump == 0 &&
        getMovePath() != nullptr && getMovePath()->numSteps > 0 && getMovePath()->crossesClosedGate(-1, 2) > 0)
    {
        setMoveWayPath(nullptr, 0);

        for (int32_t i = 0; i < 2; i++)
        {
            if (moveOrders.path[i] != nullptr)
            {
                moveOrders.path[i]->clear();
            }
        }

        moveOrders.moveState = 1;
        moveOrders.moveStateGoal = 1;
        moveOrders.yieldTime = -1.0f;
        moveOrders.waitForPointTime = -1.0f;
        moveOrders.timeOfLastStep = -1.0f;
        moveOrders.yieldState = 0;
        moveOrders.unknown1030 = 0;
        setMoveGlobalPath(nullptr, 0);
        PathManager->remove(this);
    }

    if (static_cast<double>(moveOrders.yieldTime) > -1.0 && moveOrders.yieldTime < scenarioTime)
    {
        // Done yielding: plan again when on (or heading over) a blown bridge, and three times in four anyway.
        moveOrders.yieldTime = MoveYieldTime + scenarioTime;
        const _ObjectPosition* position = mover->getObjPosition();
        const int32_t tileR = position->tileR;
        const int32_t tileC = position->tileC;
        bool replan;

        if (OverlayIsBridge[GameMap->map[GameMap->width * tileR + tileC].overlay & 0x7f] != 0 &&
            GlobalMoveMap->areas[GlobalMoveMap->calcArea(tileR, tileC)].closed != 0)
        {
            replan = true;
        }
        else
        {
            replan = RandomNumber(100) < 75;
        }

        bool request = replan;

        if (getMovePath() != nullptr && getMovePath()->crossesBridge(-1, 3) >= 0 && !replan)
        {
            request = movingOverBlownBridge() != 0;
        }

        if (request)
        {
            requestMovePath(curTacOrder.selectionIndex, 0x201, 3);
        }
    }

    // Plan the next leg of a global path ahead of time.
    if (moveOrders.pathType == 2 && moveOrders.curGlobalStep < moveOrders.numGlobalSteps - 1 &&
        moveOrders.path[1]->numStepsWhenNotPaused == 0 && movePathRequest == nullptr)
    {
        requestMovePath(curTacOrder.selectionIndex, 1, 4);
    }

    if (scenarioTime < movementUpdateTime)
    {
        return 1;
    }

    const int32_t code = curTacOrder.code;
    movementUpdateTime = MovementUpdateFrequency + scenarioTime;

    GameObject* target;

    if (code == TACTICAL_ORDER_NONE || code == TACTICAL_ORDER_STOP)
    {
        target = getLastTarget();
    }
    else
    {
        target = moveOrders.goalObject;

        if (moveOrders.goalType != -1)
        {
            if (moveOrders.goalType == 0)
            {
                // Moving to a point: plan again once the path is walked.
                if (static_cast<double>(moveOrders.yieldTime) > -1.0 || isJumping(nullptr) != 0 ||
                    moveOrders.unknown1030 != 0 || static_cast<double>(moveOrders.waitForPointTime) > -1.0 ||
                    getMovePath()->numSteps != 0 || movePathRequest != nullptr)
                {
                    return 1;
                }

                requestMovePath(curTacOrder.selectionIndex, 0x101, 9);
                return 1;
            }

            // Moving to an object: follow it when it moves on.
            if (target == nullptr)
            {
                return 1;
            }

            const vector_3d targetPosition = target->getPosition();
            const double dx = static_cast<double>(targetPosition.x) - moveOrders.goalObjectPosition.x;
            const double dy = static_cast<double>(targetPosition.y) - moveOrders.goalObjectPosition.y;
            const float dz = targetPosition.z - moveOrders.goalObjectPosition.z;

            if (50.0 < std::sqrt((dx * dx + dy * dy) + static_cast<double>(dz) * dz))
            {
                vector_3d goal = target->getPosition();
                setMoveGoal(static_cast<uint32_t>(target->partId), &goal, target);
                requestMovePath(curTacOrder.selectionIndex, 0x101, 10);
                return 1;
            }

            GameObject* lastTargetNow = getLastTarget();
            mover->relViewFacingTo(targetPosition);

            if (lastTargetNow == nullptr || lastTargetNow != target || attackOrders.pursue == 0)
            {
                return 1;
            }

            int wantMove = 0;
            uint32_t extraParams = 0;
            bool move = false;

            if (IsMover(lastTargetNow))
            {
                if (mover->unknown1B8 < static_cast<Mover*>(lastTargetNow)->unknown1B4 &&
                    mover->calcOptimalRange(nullptr) != 0)
                {
                    wantMove = 1;
                    extraParams = 8;
                }
            }
            else if (getMovePath()->numSteps > 0)
            {
                return 1;
            }

            vector_3d lastTargetPosition = lastTargetNow->getPosition();
            const float distance = mover->distanceFrom(lastTargetPosition);
            const float fireRange = mover->getFireRange(curTacOrder.attackParams.range);
            const double slack = AttackRangeSlack();

            if (slack < static_cast<double>(distance) - fireRange ||
                static_cast<double>(distance) - fireRange < -slack || weaponsStatusResult == -3)
            {
                extraParams = 8;
                move = true;
            }
            else if (weaponsStatusResult >= 0)
            {
                int32_t notReady = 0;
                int32_t notLocked = 0;
                int32_t hot = 0;

                for (int32_t i = 0; i < mover->numWeapons; i++)
                {
                    if (weaponsStatus[i] == -1)
                    {
                        notReady++;
                    }

                    if (weaponsStatus[i] == -4)
                    {
                        notLocked++;
                    }

                    if (weaponsStatus[i] == -6)
                    {
                        hot++;
                    }
                }

                bool hold = false;

                switch (mover->objectClass)
                {
                    case BATTLEMECH:
                    {
                        if (weaponsStatusResult == 0)
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
                    case GROUNDVEHICLE:
                    {
                        if (weaponsStatusResult == 0 && notReady == 0)
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
                    case ELEMENTAL:
                    {
                        if (weaponsStatusResult == 0 && notReady == 0)
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

                if (hold && getMovePath()->numSteps == 0 && moveOrders.unknown1030 == 0)
                {
                    moveOrders.moveStateGoal = 5;
                }
            }

            if (!move && wantMove == 0)
            {
                return 1;
            }

            moveOrders.moveStateGoal = 1;
            vector_3d goal = lastTargetNow->getPosition();
            setMoveGoal(static_cast<uint32_t>(lastTargetNow->partId), &goal, lastTargetNow);
            requestMovePath(curTacOrder.selectionIndex, extraParams | 0x101, 11);
            return 1;
        }

        if (code == TACTICAL_ORDER_WITHDRAW)
        {
            vector_3d goal = calcWithdrawGoal(1000.0f);
            setMoveGoal(0, &goal, nullptr);
            requestMovePath(curTacOrder.selectionIndex, 0x101, 5);
            return 1;
        }

        if (curTacOrder.isCombatOrder() != 0)
        {
            GameObject* attackTarget = getLastTarget();
            vector_3d targetPosition;

            if (attackTarget == nullptr)
            {
                if (curTacOrder.code != TACTICAL_ORDER_ATTACK_POINT)
                {
                    return 1;
                }

                targetPosition = attackOrders.targetPoint;
            }
            else
            {
                targetPosition = attackTarget->getPosition();
            }

            if (curTacOrder.attackParams.method == 2)
            {
                // Ramming: head for the target itself.
                vector_3d goal = attackTarget->getPosition();
                setMoveGoal(static_cast<uint32_t>(attackTarget->partId), &goal, attackTarget);
                requestMovePath(curTacOrder.selectionIndex, 0x101, 6);
                return 1;
            }

            if (attackOrders.pursue == 0)
            {
                moveOrders.moveStateGoal = 5;
                return 1;
            }

            int wantMove = 0;
            uint32_t extraParams = 0;
            bool move = false;

            if (attackTarget != nullptr && IsMover(attackTarget) &&
                mover->unknown1B8 < static_cast<Mover*>(attackTarget)->unknown1B4 &&
                mover->calcOptimalRange(nullptr) != 0)
            {
                wantMove = 1;
                extraParams = 8;
            }

            const float distance = mover->distanceFrom(targetPosition);
            const float fireRange = mover->getFireRange(curTacOrder.attackParams.range);
            const double slack = AttackRangeSlack();

            if (slack < static_cast<double>(distance) - fireRange ||
                static_cast<double>(distance) - fireRange < -slack || weaponsStatusResult == -3)
            {
                extraParams = 8;
                move = true;
            }
            else if (weaponsStatusResult >= 0)
            {
                int32_t notReady = 0;
                int32_t notLocked = 0;
                int32_t hot = 0;

                for (int32_t i = 0; i < mover->numWeapons; i++)
                {
                    if (weaponsStatus[i] == -1)
                    {
                        notReady++;
                    }

                    if (weaponsStatus[i] == -4)
                    {
                        notLocked++;
                    }

                    if (weaponsStatus[i] == -6)
                    {
                        hot++;
                    }
                }

                const ObjectClass objectClass = mover->objectClass;

                if (weaponsStatusResult == 0 &&
                    ((objectClass == BATTLEMECH) ||
                     ((objectClass == GROUNDVEHICLE || objectClass == ELEMENTAL) && notReady == 0)))
                {
                    if (notReady < 1 && notLocked < 1 && hot < 1)
                    {
                        extraParams = 8;
                        move = true;
                    }
                    else if (getMovePath()->numSteps == 0 && moveOrders.unknown1030 == 0)
                    {
                        moveOrders.moveStateGoal = 5;
                    }
                }
            }

            if (!move && wantMove == 0)
            {
                return 1;
            }

            moveOrders.moveStateGoal = 1;

            if (attackTarget == nullptr)
            {
                setMoveGoal(0, &targetPosition, nullptr);
                requestMovePath(curTacOrder.selectionIndex, 0x101, 8);
                return 1;
            }

            setMoveGoal(static_cast<uint32_t>(attackTarget->partId), &targetPosition, attackTarget);
            requestMovePath(curTacOrder.selectionIndex, extraParams | 0x101, 7);
            return 1;
        }

        target = getLastTarget();
    }

    if (target == nullptr)
    {
        return 1;
    }

    moveOrders.moveStateGoal = 5;
    return 1;
}

auto MechWarrior::clearCurTacOrder(int updateTacOrder, int updateBrain) -> void
{
    if (curTacOrder.isCombatOrder() != 0)
    {
        numWarriorsInCombat--;
    }

    if (numWarriorsInCombat < 0)
    {
        Assert(false, 0, "numWarriorsInCombat >= 0");
    }

    curTacOrder.init();

    if (updateTacOrder == 0)
    {
        clearMoveOrders();
        triggerAlarm(PILOT_ALARM_NO_MOVEPATH, static_cast<uint32_t>(-14));
    }

    clearAttackOrders();
    lastTacOrder.lastTime = scenarioTime;

    if (updateTacOrder == 0)
    {
        return;
    }

    // Fall back to the order underneath: alarm to player (or general), player to general.
    TacticalOrder newOrder;
    newOrder.init();

    switch (orderState)
    {
        case ORDERSTATE_GENERAL:
        {
            if (newTacOrderReceived[ORDERSTATE_GENERAL] == 0)
            {
                tacOrder[ORDERSTATE_GENERAL].init();
            }
            break;
        }
        case ORDERSTATE_PLAYER:
        {
            if (newTacOrderReceived[ORDERSTATE_PLAYER] == 0)
            {
                tacOrder[ORDERSTATE_PLAYER].init();
                playerOrderFromQueue = 0;
            }

            newOrder = tacOrder[ORDERSTATE_GENERAL];
            orderState = ORDERSTATE_GENERAL;
            break;
        }
        case ORDERSTATE_ALARM:
        {
            if (newTacOrderReceived[ORDERSTATE_ALARM] == 0)
            {
                tacOrder[ORDERSTATE_ALARM].init();
            }

            alarmPriority = 0;

            if (tacOrder[ORDERSTATE_PLAYER].code != TACTICAL_ORDER_NONE)
            {
                newOrder = tacOrder[ORDERSTATE_PLAYER];
                orderState = ORDERSTATE_PLAYER;
            }
            else
            {
                newOrder = tacOrder[ORDERSTATE_GENERAL];
                orderState = ORDERSTATE_GENERAL;
            }
            break;
        }
        default:
            break;
    }

    if (playerOrderFromQueue == 0)
    {
        clearMoveOrders();
        triggerAlarm(PILOT_ALARM_NO_MOVEPATH, static_cast<uint32_t>(-14));
    }

    Assert(moveOrders.path[0] != nullptr && moveOrders.path[1] != nullptr, 0, " bad warrior path ");
    MovePath* paths[2];

    for (int32_t i = 0; i < 2; i++)
    {
        paths[i] = moveOrders.path[i];
        paths[i]->numSteps = 0;
    }

    const int32_t moveState = moveOrders.moveState;
    const int32_t moveStateGoal = moveOrders.moveStateGoal;
    moveOrders.init();
    PathManager->remove(this);
    moveOrders.path[1] = paths[1];
    moveOrders.moveState = moveState;
    moveOrders.moveStateGoal = moveStateGoal;
    moveOrders.path[0] = paths[0];
    attackOrders.init();
    int32_t message = -1;
    newOrder.execute(this, message);

    if (orderState == ORDERSTATE_PLAYER)
    {
        radioMessage(message, 1);
    }
}

auto MechWarrior::setCurTacOrder(TacticalOrder tacOrder) -> void
{
    curTacOrder = tacOrder;
    lastTacOrder = tacOrder;

    if (curTacOrder.isCombatOrder() != 0)
    {
        numWarriorsInCombat++;
    }
}

auto MechWarrior::setGeneralTacOrder(TacticalOrder order) -> void
{
    tacOrder[ORDERSTATE_GENERAL] = order;
    newTacOrderReceived[ORDERSTATE_GENERAL] = 1;
}

auto MechWarrior::setPlayerTacOrder(TacticalOrder order, int fromQueue) -> void
{
    tacOrder[ORDERSTATE_PLAYER] = order;
    newTacOrderReceived[ORDERSTATE_PLAYER] = 1;
    playerOrderFromQueue = fromQueue;

    if (fromQueue == 0)
    {
        clearTacOrderQueue();
    }
}

auto MechWarrior::setAlarmTacOrder(TacticalOrder order, int32_t priority) -> void
{
    if (alarmPriority <= priority)
    {
        tacOrder[ORDERSTATE_ALARM] = order;
        newTacOrderReceived[ORDERSTATE_ALARM] = 1;
        alarmPriority = priority;
    }
}

auto MechWarrior::triggerAlarm(int32_t alarmCode, uint32_t triggerId) -> int32_t
{
    _PilotAlarm& pilotAlarm = alarm[alarmCode];

    if (pilotAlarm.numTriggers == MAX_ALARM_TRIGGERS)
    {
        return -1;
    }

    pilotAlarm.trigger[pilotAlarm.numTriggers] = triggerId;
    pilotAlarm.numTriggers++;
    return 0;
}

auto MechWarrior::handleAlarm(int32_t alarmCode, uint32_t triggerId) -> int32_t
{
    Mover* mover = static_cast<Mover*>(vehicle);
    Assert(mover != nullptr, 0, " bad vehicle for pilot ");

    if (mover->getAwake() == 0)
    {
        return 0;
    }

    if (alarmCode == PILOT_ALARM_VEHICLE_INCAPACITATED)
    {
        handleOwnVehicleIncapacitation(triggerId);
    }
    else if (alarmCode == PILOT_ALARM_VEHICLE_DESTROYED)
    {
        handleOwnVehicleDestruction(triggerId);
    }
    else if (alarmCode == PILOT_ALARM_VEHICLE_WITHDRAWN)
    {
        handleOwnVehicleWithdrawn();
    }

    if ((MPlayer == nullptr || MPlayer->isServer != 0) && brainAlarmCallback[alarmCode] != nullptr)
    {
        IsUnitOrder = 0;
        CurGroup = getGroup();
        CurObject = vehicle;
        CurObjectClass = CurObject->objectClass;
        CurContact = nullptr;
        CurAlarm = alarmCode;
        CurWarrior = this;
        brain->execute(nullptr, brainAlarmCallback[alarmCode], nullptr);
        IsUnitOrder = 0;
        CurGroup = nullptr;
        CurObject = nullptr;
        CurObjectClass = 0;
        CurWarrior = nullptr;
        CurContact = nullptr;
    }

    return 0;
}

auto MechWarrior::getAlarmTriggers(int32_t alarmCode, uint32_t* triggerList) -> int32_t
{
    const uint8_t numTriggers = alarm[alarmCode].numTriggers;

    for (int32_t i = 0; i < numTriggers; i++)
    {
        triggerList[i] = alarm[alarmCode].trigger[i];
    }

    return numTriggers;
}

auto MechWarrior::clearAlarm(int32_t alarmCode) -> void
{
    alarm[alarmCode].numTriggers = 0;
}

auto MechWarrior::checkAlarms() -> int32_t
{
    if (brain != nullptr)
    {
        IsUnitOrder = 0;
        CurGroup = getGroup();
        CurObject = vehicle;
        CurObjectClass = CurObject->objectClass;
        CurContact = nullptr;
        CurWarrior = this;
    }

    for (int32_t alarmCode = 0; alarmCode < NUM_PILOT_ALARMS; alarmCode++)
    {
        if (alarm[alarmCode].numTriggers == 0)
        {
            continue;
        }

        switch (alarmCode)
        {
            case PILOT_ALARM_TARGET_OF_WEAPONFIRE:
                handleTargetOfWeaponFire();
                break;
            case PILOT_ALARM_HIT_BY_WEAPONFIRE:
                handleHitByWeaponFire();
                break;
            case PILOT_ALARM_DAMAGE_TAKEN_RATE:
                handleDamageTakenRate();
                break;
            case PILOT_ALARM_DEATH_OF_MATE:
                handleUnitMateDeath();
                break;
            case PILOT_ALARM_FRIENDLY_VEHICLE_CRIPPLED:
                handleFriendlyVehicleCrippled();
                break;
            case PILOT_ALARM_FRIENDLY_VEHICLE_DESTROYED:
                handleFriendlyVehicleDestruction();
                break;
            case PILOT_ALARM_VEHICLE_INCAPACITATED:
                handleOwnVehicleIncapacitation(0);
                break;
            case PILOT_ALARM_VEHICLE_DESTROYED:
                handleOwnVehicleDestruction(0);
                break;
            case PILOT_ALARM_VEHICLE_WITHDRAWN:
                handleOwnVehicleWithdrawn();
                break;
            case PILOT_ALARM_MORALE_BREAK:
                handleMoraleBreak();
                break;
            case PILOT_ALARM_COLLISION:
                handleCollision();
                break;
            case PILOT_ALARM_KILLED_TARGET:
                handleKilledTarget();
                break;
            case PILOT_ALARM_MATE_FIRED_WEAPON:
                handleUnitMateFiredWeapon();
                break;
            case PILOT_ALARM_PLAYER_ORDER:
                handlePlayerOrder();
                break;
            case PILOT_ALARM_NO_MOVEPATH:
                handleNoMovePath();
                break;
            case PILOT_ALARM_GATE_CLOSING:
                handleGateClosing();
                break;
            default:
                break;
        }

        if ((MPlayer == nullptr || MPlayer->isServer != 0) && brain != nullptr &&
            brainAlarmCallback[alarmCode] != nullptr)
        {
            CurAlarm = alarmCode;
            brain->execute(nullptr, brainAlarmCallback[alarmCode], nullptr);
        }

        alarm[alarmCode].numTriggers = 0;
    }

    if (brain != nullptr)
    {
        IsUnitOrder = 0;
        CurGroup = nullptr;
        CurObject = nullptr;
        CurObjectClass = 0;
        CurWarrior = nullptr;
        CurContact = nullptr;
    }

    return 0;
}

auto MechWarrior::updateActions() -> void
{
    if (static_cast<Mover*>(vehicle)->isCaptured() != 0)
    {
        clearCurTacOrder(1, 0);
        setLastTarget(nullptr, 0, 0);
        return;
    }

    if (combatUpdateTime <= scenarioTime)
    {
        combatDecisionTree();
    }

    movementDecisionTree();
}

auto MechWarrior::mainDecisionTree() -> int32_t
{
    Assert(moveOrders.path != nullptr, 0, " bad warrior path ");

    // The current order: when done, the next queued player order or the one underneath takes over.
    const bool server = MPlayer == nullptr || MPlayer->isServer != 0;

    if (server && curTacOrder.code == TACTICAL_ORDER_NONE && tacOrderQueueExecuting != 0 && numTacOrdersQueued > 0 &&
        newTacOrderReceived[ORDERSTATE_PLAYER] == 0)
    {
        executeTacOrderQueue();
    }

    if (curTacOrder.code != TACTICAL_ORDER_NONE && curTacOrder.status(this) == 1)
    {
        Assert(MPlayer == nullptr || MPlayer->isServer != 0, 0, " MechWarrior.mainDecisionTree: client! ");

        if (orderState == ORDERSTATE_PLAYER && tacOrderQueueExecuting != 0)
        {
            executeTacOrderQueue();
        }

        clearCurTacOrder(1, 0);
    }

    if (onHomeTeam() != 0 && curTacOrder.code == TACTICAL_ORDER_NONE && timeOfLastOrders < 0.0f)
    {
        timeOfLastOrders = scenarioTime;
    }

    if (brainUpdateTime <= scenarioTime || combatUpdateTime <= scenarioTime || movementUpdateTime <= scenarioTime)
    {
        GameObject* target = getLastTarget();
        vector_3d attackPoint;
        vector_3d* targetPoint = nullptr;
        bool update = true;

        if (target == nullptr)
        {
            if (curTacOrder.code != TACTICAL_ORDER_ATTACK_POINT)
            {
                update = false;
            }
            else
            {
                attackPoint = attackOrders.targetPoint;
                targetPoint = &attackPoint;
            }
        }

        if (update)
        {
            weaponsStatusResult = calcWeaponsStatus(target, weaponsStatus, targetPoint);
        }
    }

    if (brainUpdateTime <= scenarioTime)
    {
        if (alignment != -1 || Duh == 0)
        {
            runBrain();
        }

        brainUpdateTime = BrainUpdateFrequency + brainUpdateTime;
    }

    GameObject* target = getLastTarget();

    if (target != nullptr && lastTargetTime == scenarioTime)
    {
        weaponsStatusResult = calcWeaponsStatus(target, weaponsStatus, nullptr);
    }

    checkAlarms();

    // Take a new order: an alarm order overrides the player's, which overrides the general one.
    TacticalOrder newOrder;
    newOrder.init();

    switch (orderState)
    {
        case ORDERSTATE_GENERAL:
        {
            if (newTacOrderReceived[ORDERSTATE_ALARM] != 0)
            {
                clearCurTacOrder(0, 0);
                newOrder = tacOrder[ORDERSTATE_ALARM];
                orderState = ORDERSTATE_ALARM;
            }
            else if (newTacOrderReceived[ORDERSTATE_PLAYER] != 0)
            {
                tacOrder[ORDERSTATE_GENERAL].init();
                newOrder = tacOrder[ORDERSTATE_PLAYER];
                orderState = ORDERSTATE_PLAYER;
            }
            else if (newTacOrderReceived[ORDERSTATE_GENERAL] != 0)
            {
                newOrder = tacOrder[ORDERSTATE_GENERAL];
            }
            break;
        }
        case ORDERSTATE_PLAYER:
        {
            if (newTacOrderReceived[ORDERSTATE_ALARM] != 0)
            {
                newOrder = tacOrder[ORDERSTATE_ALARM];
                orderState = ORDERSTATE_ALARM;
            }
            else if (newTacOrderReceived[ORDERSTATE_PLAYER] != 0)
            {
                tacOrder[ORDERSTATE_GENERAL].init();
                newOrder = tacOrder[ORDERSTATE_PLAYER];
            }
            break;
        }
        case ORDERSTATE_ALARM:
        {
            if (newTacOrderReceived[ORDERSTATE_PLAYER] != 0)
            {
                // Original behaviour (OB-008): the player's order runs, but orderState stays ALARM.
                alarmPriority = 0;
                tacOrder[ORDERSTATE_ALARM].init();
                newOrder = tacOrder[ORDERSTATE_PLAYER];
            }
            else if (newTacOrderReceived[ORDERSTATE_ALARM] != 0)
            {
                newOrder = tacOrder[ORDERSTATE_ALARM];
            }
            break;
        }
        default:
            break;
    }

    if (newOrder.code != TACTICAL_ORDER_NONE)
    {
        // A move to a point or object keeps the path walked (a new path replaces it when planned).
        MovePath* paths[2];

        for (int32_t i = 0; i < 2; i++)
        {
            paths[i] = moveOrders.path[i];

            if (i > 0 ||
                (newOrder.code != TACTICAL_ORDER_MOVETO_POINT && newOrder.code != TACTICAL_ORDER_MOVETO_OBJECT))
            {
                paths[i]->numSteps = 0;
            }
        }

        const int32_t run = moveOrders.run;
        const int32_t moveState = moveOrders.moveState;
        const int32_t moveStateGoal = moveOrders.moveStateGoal;
        moveOrders.init();
        moveOrders.run = run;
        PathManager->remove(this);
        moveOrders.moveState = moveState;
        moveOrders.moveStateGoal = moveStateGoal;
        moveOrders.path[0] = paths[0];
        moveOrders.path[1] = paths[1];
        attackOrders.init();
        int32_t message = -1;
        newOrder.execute(this, message);

        if (orderState == ORDERSTATE_PLAYER)
        {
            radioMessage(message, 1);
        }

        setCurTacOrder(newOrder);
        timeOfLastOrders = -1.0f;
    }

    newTacOrderReceived[ORDERSTATE_GENERAL] = 0;
    newTacOrderReceived[ORDERSTATE_PLAYER] = 0;
    newTacOrderReceived[ORDERSTATE_ALARM] = 0;
    updateActions();
    return 0;
}

auto MechWarrior::setDebugFlag(uint32_t flag, int on) -> void
{
    if (on != 0)
    {
        debugFlags |= flag;
    }
    else
    {
        debugFlags &= ~flag;
    }
}

auto MechWarrior::getDebugFlag(uint32_t flag) -> int
{
    return (debugFlags & flag) != 0 ? 1 : 0;
}

auto MechWarrior::debugPrint(char* s, int debugMode) -> void
{
    if (debugger != nullptr)
    {
        debugger->print(s);

        if (debugMode != 0)
        {
            debugger->debugMode();
        }
    }
}

auto MechWarrior::debugOrders() -> void
{
    char line[256];
    const int32_t targetId = curTacOrder.target != nullptr ? curTacOrder.target->partId : 0;

    switch (curTacOrder.code)
    {
        case TACTICAL_ORDER_NONE:
            std::snprintf(line, sizeof(line), "CURRENT ORDERS: None");
            break;
        case TACTICAL_ORDER_WAIT:
            std::snprintf(line, sizeof(line), "CURRENT ORDERS: Wait");
            break;
        case TACTICAL_ORDER_MOVETO_POINT:
        {
            const vector_3d point = curTacOrder.getWayPoint(0);
            std::snprintf(line, sizeof(line), "CURRENT ORDERS: Move to (%.2f, %.2f, %.2f)",
                          static_cast<double>(point.x), static_cast<double>(point.y), static_cast<double>(point.z));
            break;
        }

        case TACTICAL_ORDER_MOVETO_OBJECT:
            std::snprintf(line, sizeof(line), "CURRENT ORDERS: Move To Object %d", targetId);
            break;
        case TACTICAL_ORDER_JUMPTO_POINT:
        {
            const vector_3d point = curTacOrder.getWayPoint(0);
            std::snprintf(line, sizeof(line), "CURRENT ORDERS: Jump to (%.2f, %.2f, %.2f)",
                          static_cast<double>(point.x), static_cast<double>(point.y), static_cast<double>(point.z));
            break;
        }

        case TACTICAL_ORDER_JUMPTO_OBJECT:
            std::snprintf(line, sizeof(line), "CURRENT ORDERS: Jump To Object %d", targetId);
            break;
        case TACTICAL_ORDER_TRAVERSE_PATH:
            std::snprintf(line, sizeof(line), "CURRENT ORDERS: Traverse Path");
            break;
        case TACTICAL_ORDER_PATROL_PATH:
            std::snprintf(line, sizeof(line), "CURRENT ORDERS: Patrol Path");
            break;
        case TACTICAL_ORDER_ESCORT:
            std::snprintf(line, sizeof(line), "CURRENT ORDERS: Escort");
            break;
        case TACTICAL_ORDER_FOLLOW:
            std::snprintf(line, sizeof(line), "CURRENT ORDERS: Follow");
            break;
        case TACTICAL_ORDER_GUARD:
            std::snprintf(line, sizeof(line), "CURRENT ORDERS: Guard");
            break;
        case TACTICAL_ORDER_STOP:
            std::snprintf(line, sizeof(line), "CURRENT ORDERS: Stop");
            break;
        case TACTICAL_ORDER_POWERUP:
            std::snprintf(line, sizeof(line), "CURRENT ORDERS: Power Up");
            break;
        case TACTICAL_ORDER_POWERDOWN:
            std::snprintf(line, sizeof(line), "CURRENT ORDERS: Power Down");
            break;
        case TACTICAL_ORDER_WAYPOINTS_DONE:
            std::snprintf(line, sizeof(line), "CURRENT ORDERS: Formation");
            break;
        case TACTICAL_ORDER_EJECT:
            std::snprintf(line, sizeof(line), "CURRENT ORDERS: Eject");
            break;
        case TACTICAL_ORDER_ATTACK_OBJECT:
            std::snprintf(line, sizeof(line), "CURRENT ORDERS: Attack Object %d", targetId);
            break;
        case TACTICAL_ORDER_HOLD_FIRE:
            std::snprintf(line, sizeof(line), "CURRENT ORDERS: Hold Fire");
            break;
        case TACTICAL_ORDER_WITHDRAW:
            std::snprintf(line, sizeof(line), "CURRENT ORDERS: Withdraw");
            break;
        default:
            std::snprintf(line, sizeof(line), "CURRENT ORDERS: Unknown Tac Order Type");
            break;
    }

    debugPrint(line, 0);
    GameObject* target = getLastTarget();
    std::snprintf(line, sizeof(line), "     CURRENT TARGET: Object %d", target != nullptr ? target->partId : 0);
    debugPrint(line, 0);
}

auto MechWarrior::setMoveSpeedType(int32_t type) -> void
{
    moveOrders.speedType = type;
}

auto MechWarrior::setMoveSpeedVelocity(float speed) -> void
{
    moveOrders.speedVelocity = speed;
    int32_t state = 0;
    int32_t throttle = 0;
    static_cast<Mover*>(vehicle)->calcSpriteSpeed(speed, 0, state, throttle);
    moveOrders.speedState = static_cast<int8_t>(state);
    moveOrders.speedThrottle = static_cast<int8_t>(throttle);
}

auto MechWarrior::openStatusWindow(int32_t x, int32_t y, int32_t w, int32_t h) -> int32_t
{
    WarriorStatusWindow* window = new WarriorStatusWindow;
    statusWindow = window;
    window->init(x, y, w, h, this);
    statusWindow->setBackColor(0);
    statusWindow->draw();
    screenWindow->addChild(statusWindow);
    return 0;
}

auto MechWarrior::closeStatusWindow() -> int32_t
{
    // Original behaviour (OB-009): the window is destroyed but its memory never freed.
    statusWindow->destroy();
    statusWindow = nullptr;
    return 0;
}

auto MechWarrior::orderWait(int unitOrder, int32_t origin, int32_t seconds, int clearLastTarget) -> int32_t
{
    TacticalOrder order;
    order.init();
    order.init(static_cast<OrderOriginType>(origin), TACTICAL_ORDER_WAIT, unitOrder);
    order.delayedTime = static_cast<float>(seconds) + scenarioTime;
    clearMoveOrders();
    clearAttackOrders();

    if (clearLastTarget != 0)
    {
        setLastTarget(nullptr, 0, 0);
    }

    if (origin == ORDER_ORIGIN_COMMANDER)
    {
        setGeneralTacOrder(order);
    }

    return order.status(this);
}

auto MechWarrior::orderStop(int unitOrder, int setTacOrder) -> int32_t
{
    TacticalOrder order;
    order.init();
    order.init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_STOP, unitOrder);
    clearTacOrderQueue();
    clearMoveOrders();
    clearAttackOrders();
    setLastTarget(nullptr, 0, 0);
    return order.status(this);
}

auto MechWarrior::orderMoveToPoint(int unitOrder, int setTacOrder, int32_t origin, vector_3d location,
                                   int32_t selectionIndex, uint32_t params) -> int32_t
{
    const uint32_t escapeTile = (params >> 6) & 1;
    const uint32_t run = params & 1;
    TacticalOrder order;
    order.init();
    order.init(static_cast<OrderOriginType>(origin), TACTICAL_ORDER_MOVETO_POINT, unitOrder);
    SetFirstWayPoint(order, location);
    order.moveParams.wayPath.mode[0] = run != 0 ? 1 : 0;
    order.moveParams.wait = (params >> 1) & 1;
    order.selectionIndex = selectionIndex;
    order.moveParams.mode = (params >> 3) & 1;
    order.moveParams.escapeTile = escapeTile;
    const int32_t result = order.status(this);

    if (result == 1)
    {
        return 1;
    }

    setMoveGoal(0xffffffff, nullptr, nullptr);
    setMoveWayPath(nullptr, 0);
    setMoveGoal(0, &location, nullptr);
    moveOrders.timeOfLastStep = scenarioTime;
    moveOrders.run = run;

    if (setTacOrder != 0)
    {
        clearAttackOrders();
    }

    uint32_t moveParams = escapeTile != 0 ? 0x2101 : 0x101;

    // The player's order to a unit's point (or to one mover) reports a blocked move on the radio.
    if (setTacOrder != 0 && origin == ORDER_ORIGIN_PLAYER && (unitOrder == 0 || getPoint() == vehicle))
    {
        moveParams |= 0x1000;
    }

    PathManager->request(this, selectionIndex, moveParams, 255.0f, 15);

    if (setTacOrder != 0 && result == 0 && origin == ORDER_ORIGIN_COMMANDER)
    {
        setGeneralTacOrder(order);
    }

    return result;
}

auto MechWarrior::orderMoveToObject(int unitOrder, int setTacOrder, int32_t origin, GameObject* target,
                                    int32_t selectionIndex, uint32_t params) -> int32_t
{
    const uint32_t faceObject = (params >> 2) & 1;

    if (target == nullptr)
    {
        return 1;
    }

    TacticalOrder order;
    order.init();
    order.init(static_cast<OrderOriginType>(origin), TACTICAL_ORDER_MOVETO_OBJECT, unitOrder);
    order.selectionIndex = selectionIndex;
    order.moveParams.wayPath.mode[0] = (params & 1) != 0 ? 1 : 0;
    order.moveParams.faceObject = faceObject;
    order.moveParams.mode = (params >> 3) & 1;
    order.target = target;
    order.moveParams.wait = 0;
    const int32_t result = order.status(this);

    if (result == 1)
    {
        return 1;
    }

    vector_3d goal = target->getPosition();
    setMoveGoal(static_cast<uint32_t>(target->partId), &goal, target);
    moveOrders.run = params & 1;

    if (setTacOrder != 0)
    {
        clearAttackOrders();
    }

    uint32_t moveParams = faceObject != 0 ? 0x101 : 0x100;

    if (setTacOrder != 0 && origin == ORDER_ORIGIN_PLAYER && (unitOrder == 0 || getPoint() == vehicle))
    {
        moveParams |= 0x1000;
    }

    requestMovePath(selectionIndex, moveParams, 12);
    moveOrders.goalObjectPosition = target->getPosition();

    if (setTacOrder != 0 && result == 0 && origin == ORDER_ORIGIN_COMMANDER)
    {
        setGeneralTacOrder(order);
    }

    return result;
}

auto MechWarrior::orderJumpToPoint(int unitOrder, int setTacOrder, int32_t origin, vector_3d location,
                                   int32_t selectionIndex) -> int32_t
{
    Mover* mover = static_cast<Mover*>(vehicle);
    const float jumpRange = mover->getJumpRange(nullptr, nullptr);

    if (mover->distanceFrom(location) <= jumpRange)
    {
        // A mech can't land on a blocked cell.
        if (mover->objectClass == BATTLEMECH && !PositionPassable(location))
        {
            return 1;
        }

        TacticalOrder order;
        order.init();
        order.init(static_cast<OrderOriginType>(origin), TACTICAL_ORDER_JUMPTO_POINT, unitOrder);
        order.selectionIndex = selectionIndex;
        SetFirstWayPoint(order, location);
        const int32_t result = order.status(this);

        if (result != 1 && setTacOrder != 0)
        {
            clearMoveOrders();
            clearAttackOrders();

            if (result == 0 && origin == ORDER_ORIGIN_COMMANDER)
            {
                setGeneralTacOrder(order);
            }
        }
    }

    return 1;
}

auto MechWarrior::orderJumpToObject(int unitOrder, int setTacOrder, int32_t origin, GameObject* target,
                                    int32_t selectionIndex) -> int32_t
{
    Mover* mover = static_cast<Mover*>(vehicle);
    vector_3d location = target->getPosition();

    if (IsMover(target) && target->getTeam() == mover->getTeam())
    {
        return 1;
    }

    const float jumpRange = mover->getJumpRange(nullptr, nullptr);

    if (mover->distanceFrom(location) <= jumpRange)
    {
        if (mover->objectClass == BATTLEMECH && !PositionPassable(location))
        {
            return 1;
        }

        TacticalOrder order;
        order.init();
        order.init(static_cast<OrderOriginType>(origin), TACTICAL_ORDER_JUMPTO_POINT, unitOrder);
        order.selectionIndex = selectionIndex;
        SetFirstWayPoint(order, location);
        order.target = target;
        const int32_t result = order.status(this);

        if (result != 1 && setTacOrder != 0)
        {
            clearMoveOrders();
            clearAttackOrders();

            if (result == 0 && origin == ORDER_ORIGIN_COMMANDER)
            {
                setGeneralTacOrder(order);
            }
        }
    }

    return 1;
}

auto MechWarrior::orderTraversePath(int unitOrder, int setTacOrder, int32_t origin, _WayPath* wayPath, uint32_t params)
    -> int32_t
{
    TacticalOrder order;
    order.init();
    order.init(static_cast<OrderOriginType>(origin), TACTICAL_ORDER_TRAVERSE_PATH, unitOrder);
    order.moveParams.wayPath = *wayPath;
    order.moveParams.mode = (params >> 3) & 1;
    const int32_t result = order.status(this);

    if (result == 1)
    {
        return 1;
    }

    vector_3d firstPoint(order.moveParams.wayPath.points[0], order.moveParams.wayPath.points[1],
                         order.moveParams.wayPath.points[2]);
    setMoveGoal(0, &firstPoint, nullptr);
    setMoveWayPath(wayPath, 0);

    if (setTacOrder != 0)
    {
        clearAttackOrders();
    }

    requestMovePath(-1, 0x101, 13);

    if (setTacOrder != 0 && result == 0 && origin == ORDER_ORIGIN_COMMANDER)
    {
        setGeneralTacOrder(order);
    }

    return result;
}

auto MechWarrior::orderPatrolPath(int unitOrder, int setTacOrder, int32_t origin, _WayPath* wayPath) -> int32_t
{
    TacticalOrder order;
    order.init();
    order.init(static_cast<OrderOriginType>(origin), TACTICAL_ORDER_PATROL_PATH, unitOrder);
    order.moveParams.wayPath = *wayPath;
    const int32_t result = order.status(this);

    if (result == 1)
    {
        return 1;
    }

    vector_3d firstPoint(order.moveParams.wayPath.points[0], order.moveParams.wayPath.points[1],
                         order.moveParams.wayPath.points[2]);
    setMoveGoal(0, &firstPoint, nullptr);
    setMoveWayPath(wayPath, 1);

    if (setTacOrder != 0)
    {
        clearAttackOrders();
    }

    requestMovePath(-1, 0x101, 14);

    if (setTacOrder != 0 && result == 0 && origin == ORDER_ORIGIN_COMMANDER)
    {
        setGeneralTacOrder(order);
    }

    return result;
}

auto MechWarrior::orderPowerUp(int unitOrder, int32_t origin) -> int32_t
{
    Mover* mover = static_cast<Mover*>(vehicle);

    if (static_cast<int8_t>(mover->status) != 5)
    {
        return 1;
    }

    TacticalOrder order;
    order.init();
    order.init(static_cast<OrderOriginType>(origin), TACTICAL_ORDER_POWERUP, unitOrder);
    const int32_t result = order.status(this);

    if (result == 1)
    {
        return 1;
    }

    clearMoveOrders();
    clearAttackOrders();

    if (mover != nullptr && mover->canPowerUp() != 0)
    {
        mover->startUp();
    }

    if (origin == ORDER_ORIGIN_COMMANDER)
    {
        setGeneralTacOrder(order);
    }
    else if (origin == ORDER_ORIGIN_SELF)
    {
        setAlarmTacOrder(order, 255);
    }

    return result;
}

auto MechWarrior::orderPowerDown(int unitOrder, int32_t origin) -> int32_t
{
    Mover* mover = static_cast<Mover*>(vehicle);
    const int8_t vehicleStatus = static_cast<int8_t>(mover->status);

    if (vehicleStatus == 5 || vehicleStatus == 4)
    {
        return 1;
    }

    TacticalOrder order;
    order.init();
    order.init(static_cast<OrderOriginType>(origin), TACTICAL_ORDER_POWERDOWN, unitOrder);
    const int32_t result = order.status(this);

    if (result == 1)
    {
        return 1;
    }

    clearMoveOrders();
    clearAttackOrders();

    if (mover != nullptr)
    {
        mover->shutDown();
    }

    if (origin == ORDER_ORIGIN_COMMANDER)
    {
        setGeneralTacOrder(order);
    }

    return result;
}

auto MechWarrior::orderUseSpeed(float speed) -> int32_t
{
    setMoveSpeedVelocity(speed);
    return 1;
}

auto MechWarrior::orderOrbitPoint(vector_3d location) -> int32_t
{
    return 1;
}

auto orderOrbitObject(GameObject* target) -> int32_t
{
    return 1;
}

auto orderUseOrbitRange(int32_t type, float range) -> int32_t
{
    return 1;
}

namespace
{
    /// <summary>What the attack orders print in GameSystemWindow when it exists.</summary>
    void PrintAttackOrder(MechWarrior* pilot)
    {
        if (GameSystemWindow == nullptr)
        {
            return;
        }

        char line[200];
        GameSystemWindow->print(const_cast<char*>(""));
        GameSystemWindow->print(const_cast<char*>("-----------------------------------"));
        std::snprintf(line, sizeof(line), "%s:", pilot->name);
        GameSystemWindow->print(line);
        Mover* mover = static_cast<Mover*>(pilot->vehicle);
        const MasterComponent& weapon = MasterComponentList[mover->inventory[mover->longestRangeWeapon].masterID];
        std::snprintf(line, sizeof(line), "Longest Range Weapon = %s (%.4f)", weapon.name,
                      static_cast<double>(weapon.weaponRange[3]));
        GameSystemWindow->print(line);
        std::snprintf(line, sizeof(line), "Optimal Range = %.4f", static_cast<double>(mover->optimalRange));
        GameSystemWindow->print(line);
        GameSystemWindow->print(const_cast<char*>("-----------------------------------"));
    }
}

auto MechWarrior::orderAttackObject(int unitOrder, int32_t origin, GameObject* target, int32_t type, int32_t method,
                                    int32_t range, int32_t aimLocation, uint32_t params) -> int32_t
{
    const uint32_t pursue = (params >> 4) & 1;
    const uint32_t obliterate = (params >> 5) & 1;
    const int conserveAmmo = type == 3 ? 1 : 0;

    if (target == nullptr)
    {
        clearAttackOrders();
        return 1;
    }

    // Only a ram needs no weapons.
    if (static_cast<Mover*>(vehicle)->numWeapons == 0 && method != 2)
    {
        clearAttackOrders();
        return 1;
    }

    TacticalOrder order;
    order.init();
    order.init(static_cast<OrderOriginType>(origin), TACTICAL_ORDER_ATTACK_OBJECT, unitOrder);
    order.target = target;
    order.attackParams.type = type;
    order.attackParams.method = method;
    order.attackParams.aimLocation = aimLocation;

    if (method == 2)
    {
        range = -3;
    }

    order.attackParams.range = range;
    order.attackParams.pursue = pursue;
    order.attackParams.obliterate = obliterate;

    if (order.status(this) == 1)
    {
        return 1;
    }

    orderUseFireRange(range);

    if (pursue == 0)
    {
        clearMoveOrders();
    }
    else
    {
        orderMoveToObject(unitOrder, 0, origin, target, -1, params | 4);
    }

    attackOrders.type = type;
    setAttackTarget(target);
    attackOrders.pursue = pursue;
    attackOrders.aimLocation = aimLocation;
    unknown1E0C = 1;
    setLastTarget(target, obliterate, conserveAmmo);

    if (origin == ORDER_ORIGIN_COMMANDER)
    {
        setGeneralTacOrder(order);
    }

    PrintAttackOrder(this);
    return 0;
}

auto MechWarrior::orderAttackPoint(int unitOrder, int32_t origin, vector_3d location, int32_t type, int32_t method,
                                   int32_t range, uint32_t params) -> int32_t
{
    const uint32_t pursue = (params >> 4) & 1;
    TacticalOrder order;
    order.init();
    order.init(static_cast<OrderOriginType>(origin), TACTICAL_ORDER_ATTACK_POINT, unitOrder);
    order.attackParams.type = type;
    order.attackParams.method = method;
    order.attackParams.range = range;
    order.attackParams.targetPoint = location;
    order.attackParams.pursue = pursue;

    if (order.status(this) == 1)
    {
        return 1;
    }

    orderUseFireRange(range);

    if (pursue == 0)
    {
        clearMoveOrders();
    }
    else
    {
        orderMoveToPoint(unitOrder, 0, origin, location, -1, params);
    }

    attackOrders.type = type;
    setAttackTarget(nullptr);
    setAttackTargetPoint(location);
    attackOrders.aimLocation = -1;
    attackOrders.pursue = pursue;
    unknown1E0C = 1;
    setLastTarget(nullptr, 0, 0);

    if (origin == ORDER_ORIGIN_COMMANDER)
    {
        setGeneralTacOrder(order);
    }

    PrintAttackOrder(this);
    return 0;
}

auto MechWarrior::orderWithdraw(int unitOrder, int32_t origin, vector_3d location) -> int32_t
{
    TacticalOrder order;
    order.init();
    order.init(static_cast<OrderOriginType>(origin), TACTICAL_ORDER_WITHDRAW, unitOrder);
    SetFirstWayPoint(order, location);
    const vector_3d goal = calcWithdrawGoal(1000.0f);
    const int32_t result = orderMoveToPoint(unitOrder, 1, origin, goal, -1, 1);
    Mover* mover = static_cast<Mover*>(vehicle);
    Assert(mover != nullptr, 0, " orderWithdraw:Warrior has no Vehicle ");
    mover->unknown79C = 1;

    if (origin == ORDER_ORIGIN_COMMANDER)
    {
        setGeneralTacOrder(order);
    }

    curTacOrder.code = TACTICAL_ORDER_WITHDRAW;
    return result;
}

auto MechWarrior::orderEject(int unitOrder, int setTacOrder, int32_t origin) -> int32_t
{
    TacticalOrder order;
    order.init();
    order.init(static_cast<OrderOriginType>(origin), TACTICAL_ORDER_EJECT, unitOrder);
    Mover* mover = static_cast<Mover*>(vehicle);
    Assert(mover != nullptr, 0, " orderWithdraw:Warrior has no Vehicle ");
    mover->handleEjection();

    if (origin == ORDER_ORIGIN_COMMANDER)
    {
        setGeneralTacOrder(order);
    }

    return 1;
}

auto MechWarrior::orderUseFireRange(int32_t range) -> int32_t
{
    orderFireRange = static_cast<Mover*>(vehicle)->getFireRange(range);
    return 1;
}

auto MechWarrior::orderUseFireOdds(int32_t odds) -> int32_t
{
    orderFireOdds = FireOddsTable[odds];
    return 1;
}

auto MechWarrior::orderRefit(int32_t origin, GameObject* target, uint32_t params) -> int32_t
{
    if (target == nullptr || target->objectClass != BATTLEMECH)
    {
        return 1;
    }

    TacticalOrder order;
    order.init();
    order.init(static_cast<OrderOriginType>(origin), TACTICAL_ORDER_REFIT, 0);
    order.target = target;
    order.selectionIndex = -1;
    order.moveParams.wayPath.mode[0] = static_cast<uint8_t>(params & 1);
    order.moveParams.faceObject = 1;
    order.moveParams.wait = 0;

    if (origin == ORDER_ORIGIN_COMMANDER)
    {
        setGeneralTacOrder(order);
    }

    return 0;
}

auto MechWarrior::orderGetFixed(int32_t origin, GameObject* target, uint32_t params) -> int32_t
{
    if (target == nullptr)
    {
        return 1;
    }

    if (target->objectClass != TREEBUILDING && target->getRefitPoints() <= 0.0)
    {
        return 1;
    }

    // A mech bay fixes mechs, a vehicle bay vehicles.
    const ObjectClass vehicleClass = vehicle->objectClass;
    const int32_t bayKind = RepairBayKind(target);

    if ((vehicleClass == BATTLEMECH && bayKind == 0) || (vehicleClass == GROUNDVEHICLE && bayKind == 1))
    {
        return 1;
    }

    TacticalOrder order;
    order.init();
    order.init(static_cast<OrderOriginType>(origin), TACTICAL_ORDER_GETFIXED, 0);
    order.target = target;
    order.selectionIndex = -1;
    order.moveParams.wayPath.mode[0] = static_cast<uint8_t>(params & 1);
    order.moveParams.faceObject = 1;
    order.moveParams.wait = 0;

    if (origin == ORDER_ORIGIN_COMMANDER)
    {
        setGeneralTacOrder(order);
    }

    return 0;
}

auto MechWarrior::orderLoadIntoCarrier(int32_t origin, GameObject* target, uint32_t params) -> int32_t
{
    if (vehicle->objectClass != ELEMENTAL || target == nullptr || target->objectClass != GROUNDVEHICLE ||
        static_cast<GroundVehicle*>(target)->elementalCarrier == 0)
    {
        return 1;
    }

    TacticalOrder order;
    order.init();
    order.init(static_cast<OrderOriginType>(origin), TACTICAL_ORDER_LOAD_INTO_CARRIER, 0);
    order.target = target;
    order.selectionIndex = -1;
    order.moveParams.wayPath.mode[0] = static_cast<uint8_t>(params & 1);
    order.moveParams.faceObject = 1;
    order.moveParams.wait = 0;

    if (origin == ORDER_ORIGIN_COMMANDER)
    {
        setGeneralTacOrder(order);
    }

    return 0;
}

auto MechWarrior::orderDeployElementals(int32_t origin, uint32_t params) -> int32_t
{
    if (vehicle->objectClass != GROUNDVEHICLE || static_cast<GroundVehicle*>(vehicle)->elementalCarrier == 0)
    {
        return 1;
    }

    TacticalOrder order;
    order.init();
    order.init(static_cast<OrderOriginType>(origin), TACTICAL_ORDER_DEPLOY_ELEMENTALS, 0);
    order.moveParams.wait = 0;
    order.moveParams.wayPath.mode[0] = static_cast<uint8_t>(params & 1);

    if (origin == ORDER_ORIGIN_COMMANDER)
    {
        setGeneralTacOrder(order);
    }

    return 0;
}

auto MechWarrior::orderCapture(int32_t origin, GameObject* target, uint32_t params) -> int32_t
{
    // Original behaviour: the test reads isCaptureable() == 0 (the slot's name may not match its meaning).
    if (target == nullptr || target->isCaptureable() != 0 || target->getAlignment() == alignment ||
        target->getCaptureBlocker(alignment) != nullptr)
    {
        return 1;
    }

    TacticalOrder order;
    order.init();
    order.init(static_cast<OrderOriginType>(origin), TACTICAL_ORDER_CAPTURE, 0);
    order.target = target;
    order.selectionIndex = -1;
    order.moveParams.wayPath.mode[0] = static_cast<uint8_t>(params & 1);
    order.moveParams.faceObject = 1;
    order.moveParams.wait = 0;

    if (origin == ORDER_ORIGIN_COMMANDER)
    {
        setGeneralTacOrder(order);
    }

    return 0;
}

auto MechWarrior::handleTargetOfWeaponFire() -> int32_t
{
    if (vehicle != nullptr)
    {
        theInterface->ObjectAttacked(vehicle->partId);
    }

    return 0;
}

auto MechWarrior::handleHitByWeaponFire() -> int32_t
{
    if (alarm[PILOT_ALARM_HIT_BY_WEAPONFIRE].trigger[0] != 0)
    {
        radioMessage(RADIO_UNDER_ATTACK, 1);
    }

    return 0;
}

auto MechWarrior::handleCollision() -> int32_t
{
    objectList->findObjectFromPart(static_cast<int32_t>(alarm[PILOT_ALARM_COLLISION].trigger[0]));
    return 0;
}

auto MechWarrior::handleDamageTakenRate() -> int32_t
{
    return 0;
}

auto MechWarrior::handleUnitMateDeath() -> int32_t
{
    const int32_t mateId = static_cast<int32_t>(alarm[PILOT_ALARM_DEATH_OF_MATE].trigger[0]);

    if (vehicle->partId == mateId)
    {
        return 0;
    }

    return getMoverFromPartId(mateId) != nullptr ? 0 : -1;
}

auto MechWarrior::handleFriendlyVehicleCrippled() -> int32_t
{
    return 0;
}

auto MechWarrior::handleFriendlyVehicleDestruction() -> int32_t
{
    return 0;
}

auto MechWarrior::handleOwnVehicleIncapacitation(uint32_t cause) -> int32_t
{
    Mover* mover = static_cast<Mover*>(vehicle);
    Assert(mover != nullptr, 0, " pilot has no vehicle ");

    if (cause < 2 || cause == 0x42)
    {
        mover->handleEjection();
    }

    clearCurTacOrder(0, 0);
    orderState = ORDERSTATE_GENERAL;
    MovePath* paths[2];

    for (int32_t i = 0; i < 2; i++)
    {
        paths[i] = moveOrders.path[i];
        paths[i]->numSteps = 0;
    }

    moveOrders.init();
    PathManager->remove(this);
    moveOrders.path[1] = paths[1];
    moveOrders.path[0] = paths[0];
    attackOrders.init();
    setLastTarget(nullptr, 0, 0);
    return 0;
}

auto MechWarrior::handleOwnVehicleDestruction(uint32_t cause) -> int32_t
{
    Assert(vehicle != nullptr, 0, "handleOwnVehicleDestruction:pilot has no vehicle ");
    return 0;
}

auto MechWarrior::handleOwnVehicleWithdrawn() -> int32_t
{
    Assert(vehicle != nullptr, 0, "handleOwnVehicleWithdrawn:pilot has no vehicle ");
    status = 2;
    return 0;
}

auto MechWarrior::handleMoraleBreak() -> int32_t
{
    TacticalOrder order;
    order.init();
    order.init(ORDER_ORIGIN_SELF, TACTICAL_ORDER_WITHDRAW, 0);
    setAlarmTacOrder(order, 10);
    return 0;
}

auto MechWarrior::handleCollisionAlert() -> int32_t
{
    return 0;
}

auto MechWarrior::handleKilledTarget() -> int32_t
{
    BaseObject* target =
        objectList->findObjectFromPart(static_cast<int32_t>(alarm[PILOT_ALARM_KILLED_TARGET].trigger[0]));

    if (target == nullptr)
    {
        return 0;
    }

    // Count the kill and score gunnery points by what it was.
    int32_t killType = -1;
    float points = 10.0f;
    int32_t message;

    switch (target->objectClass)
    {
        case BATTLEMECH:
        {
            killType = static_cast<GameObject*>(target)->getMechClass();
            points = KillSkill[killType];
            numKilled[killType][1]++;
            message = RADIO_MECH_DESTROYED;
            break;
        }
        case GROUNDVEHICLE:
        case TURRET:
        {
            killType = 5;
            points = KillSkill[4];
            numKilled[5][1]++;
            message = RADIO_VEHICLE_DESTROYED;
            break;
        }
        case ELEMENTAL:
        {
            killType = 6;
            points = KillSkill[5];
            numKilled[6][1]++;
            message = RADIO_OBJECT_DESTROYED;
            break;
        }
        default:
            message = RADIO_OBJECT_DESTROYED;
            break;
    }

    radioMessage(message, 0);

    // A tenth for killing one of our own (or an ally's).
    if (std::abs(static_cast<GameObject*>(target)->getAlignment() - alignment) < 2)
    {
        points = points * 0.1f;
    }

    skillPoints[MWS_GUNNERY] = points + skillPoints[MWS_GUNNERY];

    if (MPlayer != nullptr && MPlayer->isServer != 0 && killType != -1)
    {
        MPlayer->addPilotKillStat(static_cast<Mover*>(vehicle), killType);
    }

    return 0;
}

auto MechWarrior::handleUnitMateFiredWeapon() -> int32_t
{
    return 0;
}

auto MechWarrior::handlePlayerOrder() -> int32_t
{
    if (getVehicleStatus() == 5 && curTacOrder.code != TACTICAL_ORDER_POWERDOWN)
    {
        orderPowerUp(0, ORDER_ORIGIN_SELF);
    }

    return 0;
}

auto MechWarrior::handleNoMovePath() -> int32_t
{
    if (curTacOrder.code == TACTICAL_ORDER_GETFIXED)
    {
        clearCurTacOrder(1, 0);
        radioMessage(RADIO_MOVE_BLOCKED, 0);
    }

    return 0;
}

auto MechWarrior::handleGateClosing() -> int32_t
{
    return 0;
}

auto MechWarrior::missionLog(File* file, int32_t unitLevel) -> int32_t
{
    char line[80];

    for (int32_t i = unitLevel * 2; i > 0; i--)
    {
        file->writeString(" ");
    }

    std::snprintf(line, sizeof(line), "MechWarrior: %s\n", name);
    file->writeString(line);

    // Port fix: the original's skill loop never advanced or wrote its line (an endless loop); this writes each
    // skill's successes and tries once, in the original's format.
    for (int32_t skill = 0; skill < NUM_SKILLS; skill++)
    {
        for (int32_t i = unitLevel * 2 + 2; i > 0; i--)
        {
            file->writeString(" ");
        }

        std::snprintf(line, sizeof(line), "%s: %04d/04%d\n", SkillsTable[skill], numSkillSuccesses[skill][1],
                      numSkillUses[skill][1]);
        file->writeString(line);
    }

    return 0;
}

auto MechWarrior::calcRank() -> void
{
    double weightedSum = 0.0;
    double totalWeight = 0.0;

    for (int32_t i = 0; i < NUM_SKILLS; i++)
    {
        weightedSum = static_cast<double>(skillRank[i]) * SkillWeightings[i] + weightedSum;
        totalWeight = totalWeight + SkillWeightings[i];
    }

    const float rankValue = static_cast<float>(weightedSum / totalWeight);

    for (int32_t i = 0; i < 4; i++)
    {
        if (rankValue < WarriorRankScale[i])
        {
            rank = static_cast<uint8_t>(i);
            return;
        }
    }
}

auto MechWarrior::loadBrainParameters(FitIniFile* brainFile, int32_t warriorId) -> int32_t
{
    if (brain == nullptr)
    {
        Fatal(0, " Warrior.loadBrainParameters: NULL brain ");
    }

    char blockName[32];
    std::snprintf(blockName, sizeof(blockName), "Warrior%d", warriorId);
    int32_t result = brainFile->seekBlock(blockName);

    if (result != 0)
    {
        return result;
    }

    int32_t numCells = 0;
    result = brainFile->readIdLong("NumCells", numCells);

    if (result != 0)
    {
        return result;
    }

    int32_t numStaticVars = 0;
    result = brainFile->readIdLong("NumStaticVars", numStaticVars);

    if (result != 0)
    {
        return result;
    }

    char sectionName[64];

    for (int32_t i = 0; i < numCells; i++)
    {
        std::snprintf(sectionName, sizeof(sectionName), "%sCell%d", blockName, i);
        result = brainFile->seekBlock(sectionName);

        if (result != 0)
        {
            return result;
        }

        int32_t cell = 0;
        result = brainFile->readIdLong("Cell", cell);

        if (result != 0)
        {
            return result;
        }

        int32_t memType = 0;
        result = brainFile->readIdLong("MemType", memType);

        if (result != 0)
        {
            return result;
        }

        if (memType == 0)
        {
            int32_t value = 0;
            result = brainFile->readIdLong("Value", value);

            if (result != 0)
            {
                return result;
            }

            memory[cell].integer = value;
        }
        else if (memType == 1)
        {
            float value = 0.0f;
            result = brainFile->readIdFloat("Value", value);

            if (result != 0)
            {
                return result;
            }

            memory[cell].real = value;
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
        result = brainFile->seekBlock(sectionName);

        if (result != 0)
        {
            return result;
        }

        int32_t type = 0;
        result = brainFile->readIdLong("type", type);

        if (result != 0)
        {
            return result;
        }

        char varName[256];
        result = brainFile->readIdString("Name", varName, 0xff);

        if (result != 0)
        {
            return result;
        }

        switch (type)
        {
            case 0:
            {
                int32_t value = 0;
                result = brainFile->readIdLong("Value", value);

                if (result != 0)
                {
                    return result;
                }

                brain->setStaticInteger(varName, value);
                break;
            }

            case 1:
            {
                float value = 0.0f;
                result = brainFile->readIdFloat("Value", value);

                if (result != 0)
                {
                    return result;
                }

                brain->setStaticReal(varName, value);
                break;
            }

            case 2:
            {
                int32_t numValues = 0;
                result = brainFile->readIdLong("NumValues", numValues);

                if (result != 0)
                {
                    return result;
                }

                result = brainFile->readIdLongArray("Values", integerValues, static_cast<uint32_t>(numValues));

                if (result != 0)
                {
                    return result;
                }

                brain->setStaticIntegerArray(varName, numValues, integerValues);
                break;
            }

            case 3:
            {
                int32_t numValues = 0;
                result = brainFile->readIdLong("NumValues", numValues);

                if (result != 0)
                {
                    return result;
                }

                result = brainFile->readIdFloatArray("Values", realValues, static_cast<uint32_t>(numValues));

                if (result != 0)
                {
                    return result;
                }

                brain->setStaticRealArray(varName, numValues, realValues);
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

auto MechWarriorManager::destroy() -> void
{
    for (int32_t i = 0; i < numWarriors; i++)
    {
        MechWarrior* warrior = warriors[i];

        if (warrior != nullptr)
        {
            warrior->destroy();
            delete warrior;
            warriors[i] = nullptr;
        }
    }

    systemHeap->free(warriors);
    warriors = nullptr;
    numWarriors = 0;
}

auto MechWarriorManager::init(int32_t newNumWarriors) -> void
{
    warriors = static_cast<MechWarrior**>(
        systemHeap->malloc(static_cast<uint32_t>(newNumWarriors * static_cast<int32_t>(sizeof(MechWarrior*)))));

    if (warriors == nullptr)
    {
        Fatal(0, " No RAM for MechWarrior Manager ");
    }

    numWarriors = newNumWarriors;

    for (int32_t i = 0; i < newNumWarriors; i++)
    {
        warriors[i] = nullptr;
    }
}

auto MechWarriorManager::set(int32_t index, MechWarrior* warrior) -> void
{
    warriors[index] = warrior;
}

auto MechWarriorManager::get(int32_t index) -> MechWarrior*
{
    return warriors[index];
}

//---------------------------------------------------------------------------
// WarriorStatusWindow

WarriorStatusWindow::~WarriorStatusWindow()
{
    // The inlined aTitleWindow destructor; aObject's runs after.
    aTitleWindow::destroy();
}

auto WarriorStatusWindow::init(int32_t x, int32_t y, int32_t w, int32_t h, MechWarrior* newWarrior) -> void
{
    aTitleWindow::init(x, y, w, h, nullptr);
    warrior = newWarrior;
}

auto WarriorStatusWindow::handleEvent(aEvent* event) -> void
{
    aObject::handleEvent(event);
}

auto WarriorStatusWindow::resize(int32_t w, int32_t h) -> void
{
    aTitleWindow::resize(w, h);
}

auto WarriorStatusWindow::display() -> void
{
    VFX_pane_wipe(displayPort->frame(), backgroundColor);

    if (warrior != nullptr)
    {
        char text[256];
        std::snprintf(text, sizeof(text), "%s - %s", warrior->callsign, warrior->name);
        setTitle(text);
        aPort* port = displayPort;
        systemFont->writeString(port->frame(), 0x7d, 0x14, reinterpret_cast<uint8_t*>(const_cast<char*>("Wounds:")),
                                -1);
        std::snprintf(text, sizeof(text), "%.1f", static_cast<double>(warrior->wounds));
        systemFont->writeString(port->frame(), 0xbc, 0x14, reinterpret_cast<uint8_t*>(text), -1);
    }

    aObject::display();
}

auto WarriorStatusWindow::draw() -> void
{
    if (warrior != nullptr)
    {
        char text[256];
        std::snprintf(text, sizeof(text), "%s - %s", warrior->callsign, warrior->name);
        setTitle(text);
    }

    aTitleWindow::draw();
}
