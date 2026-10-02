#include "stdafx.h"
#include "object/gvehicl.h"
#include "ai/move.h"
#include "ai/tacordr.h"
#include "appear/apprtype.h"
#include "camera/camera.h"
#include "camera/camlist.h"
#include "engine/ceglist.h"
#include "engine/celine.h"
#include "engine/cellip.h"
#include "engine/cevfx.h"
#include "vfx/vfxfuncs.h"
#include "gui/afont.h"
#include "gui/aport.h"
#include "gui/asystem.h"
#include "iface/iface.h"
#include "lib/aerror.h"
#include "lib/heap.h"
#include "lib/inifile.h"
#include "logistics/logmain.h"
#include "main/main.h"
#include "mission/scenario.h"
#include "network/multplyr.h"
#include "object/aictrl.h"
#include "object/artlry.h"
#include "object/bldng.h"
#include "object/bridge.h"
#include "object/bullet.h"
#include "object/cmponent.h"
#include "object/collsn.h"
#include "object/contact.h"
#include "object/elemntl.h"
#include "object/explode.h"
#include "object/group.h"
#include "object/gvehctrl.h"
#include "object/gvehdyn.h"
#include "object/jet.h"
#include "object/laser.h"
#include "object/mech.h"
#include "object/prjlase.h"
#include "object/netctrl.h"
#include "object/object.h"
#include "object/objque.h"
#include "object/plyrctrl.h"
#include "object/smoke.h"
#include "object/team.h"
#include "object/warrior.h"
#include "sound/soundsys.h"
#include "sprite/gvactor.h"
#include "sprite/lactor.h"
#include "sprite/puactor.h"
#include "terrain/terrain.h"

int32_t GroundVehicleAttackerMoveModifier[4] = {};
int32_t GroundVehicleCriticalHitTable[11] = {};
float TileThrottleMultiplier[3][NUM_THROTTLE_TILE_TYPES] = {};
float OverlayThrottleMultiplier[3][NUM_THROTTLE_OVERLAY_TYPES] = {};

namespace
{
    /// <summary>Fills the throttle tables with MCX.EXE's initial data: every entry is 1.0 (no tile or overlay
    /// slows a vehicle).</summary>
    const bool throttleTablesFilled = []
    {
        std::fill_n(&TileThrottleMultiplier[0][0], 3 * NUM_THROTTLE_TILE_TYPES, 1.0f);
        std::fill_n(&OverlayThrottleMultiplier[0][0], 3 * NUM_THROTTLE_OVERLAY_TYPES, 1.0f);
        return true;
    }();
}

int32_t DefaultGroundVehicleCrashAvoidSelf = 1;
int32_t DefaultGroundVehicleCrashAvoidPath = 1;
int32_t DefaultGroundVehicleCrashBlockSelf = 1;
int32_t DefaultGroundVehicleCrashBlockPath = 1;
float DefaultGroundVehicleCrashYieldTime = 2.0f;
uint32_t weaponFXTable[32] = {455, 461, 462, 188, 467, 468, 0xffffffff, 458, 14,  190, 191, 192, 456, 463, 464, 457,
                              465, 466, 459, 460, 879, 880, 881,        882, 883, 884, 885, 886, 887, 888, 889, 890};
float gvCollisionThreshold = 0.0f;
float gvObjectCollisionThreshold = 0.0f;
float gvTonnageCollisionThreshold = 0.0f;
float gvTreeDeflection = 0.0f;
float gvSweepTime = 0.0f;
float gvHillSpeedFactor = 0.0f;
float MaxVelocityMag = 0.0f;

namespace
{
    /// <summary>Half pi, as MCX.EXE stores it (MCX.EXE @ 0x0077cb50).</summary>
    constexpr double HALF_PI = 0x1.921fb5443e88cp+0;
    /// <summary>Degrees to radians, as MCX.EXE stores it (MCX.EXE @ 0x0077c2a0; a hair under pi / 180).</summary>
    constexpr double DEGREES_TO_RADIANS = 0x1.1df46a2526c7ap-6;

    /// <summary>Turns a frame about its k axis (MC2's inline frame_of_ref::rotate_about_k).</summary>
    void rotateAboutK(frame_of_ref& frame, float s, float c)
    {
        const vector_3d oldI = frame.i;
        frame.i = frame.i * c + frame.j * s;
        frame.j = frame.j * c - oldI * s;
    }

    /// <summary>
    /// Hits <paramref name="victim"/> from <paramref name="shooter"/>'s side for <paramref name="damage"/> (attack
    /// source 1).
    /// </summary>
    void collisionHit(GameObject* victim, GameObject* shooter, float damage)
    {
        const int32_t hitLocation = victim->calcHitLocation(shooter, -1, 1, 0);
        const float entryAngle = victim->relFacingTo(shooter->getPosition(), -1);
        _WeaponShotInfo shotInfo;
        shotInfo.init(shooter, -1, damage, hitLocation, entryAngle);
        victim->handleWeaponHit(&shotInfo, MPlayer != nullptr);
    }
}

auto loadGroundVehicleGameSystem(FitIniFile* sysFile) -> int32_t
{
    int32_t result = sysFile->seekBlock("GroundVehicle:FireWeapon");

    if (result != 0)
    {
        return result;
    }

    if ((result = sysFile->readIdLongArray("AttackerMoveModifier", GroundVehicleAttackerMoveModifier, 4)) != 0)
    {
        return result;
    }

    if ((result = sysFile->seekBlock("GroundVehicle:Damage")) != 0)
    {
        return result;
    }

    if ((result = sysFile->readIdLongArray("CriticalHitTable", GroundVehicleCriticalHitTable, 11)) != 0)
    {
        return result;
    }

    if ((result = sysFile->seekBlock("GroundVehicle:Collision")) != 0)
    {
        return result;
    }

    if ((result = sysFile->readIdFloat("collisionThreshold", gvCollisionThreshold)) != 0)
    {
        return result;
    }

    if ((result = sysFile->readIdFloat("objectThreshold", gvObjectCollisionThreshold)) != 0)
    {
        return result;
    }

    if ((result = sysFile->readIdFloat("tonnageThreshold", gvTonnageCollisionThreshold)) != 0)
    {
        return result;
    }

    if ((result = sysFile->readIdFloat("treeDeflection", gvTreeDeflection)) != 0)
    {
        return result;
    }

    if ((result = sysFile->seekBlock("GroundVehicle:Movement")) != 0)
    {
        return result;
    }

    int32_t value = 0;

    if (sysFile->readIdLong("CrashAvoidSelf", value) == 0)
    {
        DefaultGroundVehicleCrashAvoidSelf = value;
    }

    if (sysFile->readIdLong("CrashAvoidPath", value) == 0)
    {
        DefaultGroundVehicleCrashAvoidPath = value;
    }

    if (sysFile->readIdLong("CrashBlockSelf", value) == 0)
    {
        DefaultGroundVehicleCrashBlockSelf = value;
    }

    if (sysFile->readIdLong("CrashBlockPath", value) == 0)
    {
        DefaultGroundVehicleCrashBlockPath = value;
    }

    float yieldTime = 0.0f;

    if (sysFile->readIdFloat("CrashYieldTime", yieldTime) == 0)
    {
        DefaultGroundVehicleCrashYieldTime = yieldTime;
    }

    if ((result = sysFile->readIdFloat("SweeperSlowTime", gvSweepTime)) != 0)
    {
        return result;
    }

    if ((result = sysFile->readIdFloat("WalkSpeed", gvWalkSpeed)) != 0)
    {
        return result;
    }

    return sysFile->readIdFloat("HillSpeedFactor", gvHillSpeedFactor);
}

//---------------------------------------------------------------------------
// GroundVehicleType
//---------------------------------------------------------------------------

auto GroundVehicleType::init() -> void
{
    crashAvoidSelf = DefaultGroundVehicleCrashAvoidSelf;
    crashAvoidPath = DefaultGroundVehicleCrashAvoidPath;
    crashBlockSelf = DefaultGroundVehicleCrashBlockSelf;
    crashBlockPath = DefaultGroundVehicleCrashBlockPath;
    vehicleId = 0;
    name = nullptr;
    alignment = 0;
    chassis = 0;
    tonnageClass = 0.0f;
    unknown40 = 0;
    unknown44 = 0;
    internalStructureTonnage = 0.0f;
    unknown54 = 0;
    unknown60 = 0;
    ammoTruck = 0;
    mineSweeper = 0;
    refitPoints = 0;
    minesToLay = 0;
    elementalCarrier = 0;
    crashYieldTime = DefaultGroundVehicleCrashYieldTime;
    seats = 0;
    explDmg = 0.0f;
    explRad = 0.0f;
}

auto GroundVehicleType::destroy() -> void
{
    if (name != nullptr)
    {
        systemHeap->free(name);
        name = nullptr;
    }

    delete dynamicsType;
    dynamicsType = nullptr;
    ObjectType::destroy();
}

auto GroundVehicleType::init(File* objFile, uint32_t fileSize) -> int32_t
{
    static const char* const locationNames[NUM_GROUNDVEHICLE_LOCATIONS] = {"Front", "Left", "Right", "Rear", "Turret"};

    FitIniFile vehicleFile;
    int32_t result = vehicleFile.open(objFile, fileSize, 50);

    if (result != 0)
    {
        return result;
    }

    if ((result = vehicleFile.seekBlock("Header")) != 0)
    {
        return result;
    }

    char fileType[128];

    if ((result = vehicleFile.readIdString("FileType", fileType, 127)) != 0)
    {
        return result;
    }

    if (std::strcmp(fileType, "GroundVehicleType") != 0)
    {
        return -1;
    }

    if ((result = vehicleFile.seekBlock("General")) != 0)
    {
        return result;
    }

    if ((result = vehicleFile.readIdULong("ID", vehicleId)) != 0)
    {
        return result;
    }

    // "Alignment" 0 is 1, 1 is -1.
    static constexpr uint8_t alignmentMap[2] = {1, 0xff};
    uint8_t fileAlignment = 0;

    if ((result = vehicleFile.readIdUChar("Alignment", fileAlignment)) != 0)
    {
        return result;
    }

    // Port fix: the original reads other values from past its two-entry table on the stack.
    alignment = fileAlignment < 2 ? alignmentMap[fileAlignment] : 0;
    char nameBuffer[128];
    vehicleFile.readIdString("Name", nameBuffer, 127);
    name = static_cast<char*>(systemHeap->malloc(static_cast<uint32_t>(std::strlen(nameBuffer) + 1)));
    std::strcpy(name, nameBuffer);

    if ((result = vehicleFile.readIdUChar("Chassis", chassis)) != 0)
    {
        return result;
    }

    if ((result = vehicleFile.readIdFloat("TonnageClass", tonnageClass)) != 0)
    {
        return result;
    }

    vehicleFile.readIdBoolean("AmmoTruck", ammoTruck);
    vehicleFile.readIdLong("RefitPoints", refitPoints);
    vehicleFile.readIdBoolean("MineSweeper", mineSweeper);
    vehicleFile.readIdLong("MinesToLay", minesToLay);
    vehicleFile.readIdBoolean("ElementalCarrier", elementalCarrier);
    vehicleFile.readIdUChar("Seats", seats);
    Assert(seats <= MAX_GROUNDVEHICLE_SEATS ? 1 : 0, seats, "Too many seats");

    if (vehicleFile.readIdFloat("ExplosionRadius", explRad) != 0)
    {
        explRad = 0.0f;
    }

    if (vehicleFile.readIdFloat("ExplosionDamage", explDmg) != 0)
    {
        explDmg = 0.0f;
    }

    if ((result = vehicleFile.seekBlock("InternalStructure")) != 0)
    {
        return result;
    }

    for (int32_t location = 0; location < NUM_GROUNDVEHICLE_LOCATIONS; location++)
    {
        if ((result = vehicleFile.readIdUChar(locationNames[location], internalStructure[location])) != 0)
        {
            return result;
        }
    }

    if ((result = vehicleFile.seekBlock("Dynamics")) != 0)
    {
        return result;
    }

    uint32_t dynamicsTypeId = 0;

    if ((result = vehicleFile.readIdULong("Type", dynamicsTypeId)) != 0)
    {
        return result;
    }

    if (dynamicsTypeId != 2)
    {
        return -0x5fffd;
    }

    dynamicsType = new GroundVehicleDynamicsType;

    if (dynamicsType == nullptr)
    {
        return -0x5fffe;
    }

    if ((result = dynamicsType->init(&vehicleFile)) != 0)
    {
        return result;
    }

    if (vehicleFile.seekBlock("MovementSystem") == 0)
    {
        int32_t value = 0;

        if (vehicleFile.readIdLong("CrashAvoidSelf", value) == 0)
        {
            crashAvoidSelf = value;
        }

        if (vehicleFile.readIdLong("CrashAvoidPath", value) == 0)
        {
            crashAvoidPath = value;
        }

        if (vehicleFile.readIdLong("CrashBlockSelf", value) == 0)
        {
            crashBlockSelf = value;
        }

        if (vehicleFile.readIdLong("CrashBlockPath", value) == 0)
        {
            crashBlockPath = value;
        }

        float yieldTime = 0.0f;

        if (vehicleFile.readIdFloat("CrashYieldTime", yieldTime) == 0)
        {
            crashYieldTime = yieldTime;
        }
    }

    return ObjectType::init(&vehicleFile);
}

auto GroundVehicleType::handleCollision(GameObject* collidee, GameObject* collider) -> int
{
    if (MPlayer != nullptr && MPlayer->isServer == 0)
    {
        return 0;
    }

    switch (collider->objectClass)
    {
        case BATTLEMECH:
        case GROUNDVEHICLE:
        case ELEMENTAL:
        {
            const int friendly = collidee->getPilot()->alignment == collider->getPilot()->alignment ? 1 : 0;

            if (collider->objectClass == ELEMENTAL && static_cast<Elemental*>(collider)->elementalCanJump == 0)
            {
                return 0;
            }

            if (friendly != 0)
            {
                return 0;
            }

            GameObject* collideeRamTarget = collidee->getPilot()->curTacOrder.getRamTarget();
            GameObject* colliderRamTarget = collider->getPilot()->curTacOrder.getRamTarget();

            if (collideeRamTarget != collider && colliderRamTarget != collidee)
            {
                return 0;
            }

            if (collidee->getCollisionFreeFrom() == collider && scenarioTime <= collidee->getCollisionFreeTime())
            {
                return 0;
            }

            collidee->setCollisionFreeFrom(collider);
            collidee->setCollisionFreeTime(scenarioTime + 2.0f);

            if (static_cast<GroundVehicleDynamicsType*>(dynamicsType)->maxVelocity != 0.0f)
            {
                frame_of_ref frame = collidee->getFrame();
                rotateAboutK(frame, static_cast<float>(std::sin(HALF_PI)), static_cast<float>(std::cos(HALF_PI)));
                collidee->setFrame(frame);
                collidee->getVelocity();
                static_cast<Mover*>(collidee)->bounceToAdjCell();
            }

            collisionHit(collidee, collider, 1.0f);
            break;
        }

        case BUILDING:
        case TREEBUILDING:
        {
            if (collidee->getCollisionFreeFrom() == collider && scenarioTime <= collidee->getCollisionFreeTime())
            {
                return 0;
            }

            collidee->setCollisionFreeFrom(collider);
            collidee->setCollisionFreeTime(scenarioTime + 2.0f);
            frame_of_ref frame = collidee->getFrame();
            // A big building turns the vehicle further.
            const float angle = collider->getObjectType()->extentRadius > gvObjectCollisionThreshold ? 135.0f : 45.0f;
            rotateAboutK(frame, static_cast<float>(std::sin(angle * DEGREES_TO_RADIANS)),
                         static_cast<float>(std::cos(angle * DEGREES_TO_RADIANS)));
            collidee->setFrame(frame);
            static_cast<Mover*>(collidee)->bounceToAdjCell();
            collisionHit(collidee, collider, static_cast<float>(collider->getTonnage() * 0.01 + 0.5));
            break;
        }

        case TREE:
        {
            if (collidee->getCollisionFreeFrom() == collider && scenarioTime <= collidee->getCollisionFreeTime())
            {
                return 0;
            }

            collidee->setCollisionFreeFrom(collider);
            collidee->setCollisionFreeTime(scenarioTime + 2.0f);
            frame_of_ref frame = collidee->getFrame();
            collider->getObjectType();
            double deflection = 0.0;

            if (tonnageClass < gvTonnageCollisionThreshold)
            {
                deflection = static_cast<double>(gvTonnageCollisionThreshold) / tonnageClass * gvTreeDeflection;
            }

            if (deflection > 0.0)
            {
                rotateAboutK(frame, static_cast<float>(std::sin(deflection * DEGREES_TO_RADIANS)),
                             static_cast<float>(std::cos(deflection * DEGREES_TO_RADIANS)));
                collidee->setFrame(frame);
            }
            break;
        }

        case TRAINCAR:
        {
            if (collidee->getCollisionFreeFrom() == collider && scenarioTime <= collidee->getCollisionFreeTime())
            {
                return 0;
            }

            collidee->setCollisionFreeFrom(collider);
            collidee->setCollisionFreeTime(scenarioTime + 2.0f);

            if (static_cast<GroundVehicleDynamicsType*>(dynamicsType)->maxVelocity != 0.0f)
            {
                frame_of_ref frame = collidee->getFrame();
                rotateAboutK(frame, static_cast<float>(std::sin(HALF_PI)), static_cast<float>(std::cos(HALF_PI)));
                collidee->setFrame(frame);
                collidee->getVelocity();
            }

            static_cast<Mover*>(collidee)->bounceToAdjCell();
            break;
        }

        default:
            return 0;
    }

    soundSystem->playDigitalSample(4, 1, collidee, 0, 0);
    return 0;
}

auto GroundVehicleType::handleDestruction(GameObject* collidee, GameObject* collider) -> int
{
    auto* vehicle = static_cast<GroundVehicle*>(collidee);

    if (vehicle->getPilot() == nullptr)
    {
        Fatal(0, " No Pilot in this vehicle! ");
    }

    if (vehicle->getPoint() == vehicle)
    {
        vehicle->group->setPoint(nullptr);
    }

    if (vehicle->sensorSystem != nullptr)
    {
        vehicle->sensorSystem->disable();
    }

    vehicle->unknown794 = 0.0f;

    if (vehicle->unknown79C == 0)
    {
        vehicle->getPilot()->triggerAlarm(7, collider == nullptr ? 0 : collider->idNumber);
        vehicle->unknown798 = 0;
        vehicle->status = 2;

        if (vehicle->getAlignment() == homeTeam->alignment)
        {
            friendlyDestroyed = 1;
        }
        else
        {
            enemyDestroyed = 1;
        }
    }
    else
    {
        vehicle->getPilot()->triggerAlarm(8, 0);
    }

    theInterface->RemoveMech(vehicle->partId);
    return 1;
}

auto GroundVehicleType::loadHotSpots(FitIniFile* vehicleFile) -> int32_t
{
    return 0;
}

auto GroundVehicleType::createInstance() -> BaseObject*
{
    auto* newVehicle = new GroundVehicle;

    if (newVehicle == nullptr)
    {
        return nullptr;
    }

    if (newVehicle->init(this) != 0)
    {
        return nullptr;
    }

    newVehicle->idNumber = NextIdNumber++;
    return newVehicle;
}

//---------------------------------------------------------------------------
// GroundVehicle
//---------------------------------------------------------------------------

auto GroundVehicle::relViewFacingTo(vector_3d goal) -> float
{
    return relFacingTo(goal, GROUNDVEHICLE_LOCATION_TURRET);
}

auto GroundVehicle::getBodyState() -> int32_t
{
    // The original reads +0x74 of either appearance: a pop-up turret's gives the bits of its shapeMaxY.
    if (gvAppearance == 0)
    {
        return std::bit_cast<int32_t>(static_cast<PUAppearance*>(appearance)->shapeMaxY);
    }

    return static_cast<GVAppearance*>(appearance)->currentState;
}

auto GroundVehicle::canMove() -> int
{
    return movementEnabled;
}

auto GroundVehicle::getThrottle() -> int32_t
{
    return static_cast<GroundVehicleControlData*>(control->controlData)->throttle;
}

auto GroundVehicle::isCaptureable() -> int
{
    if ((captureable != 0 || salvage != nullptr) && isCaptured() == 0 && isDestroyed() == 0)
    {
        return 1;
    }

    return 0;
}

auto GroundVehicle::getRefitPoints() -> float
{
    if (refitter != 0)
    {
        return armor[GROUNDVEHICLE_LOCATION_TURRET].curArmor;
    }

    return 0.0f;
}

auto GroundVehicle::burnRefitPoints(float pointsToBurn) -> int
{
    if (refitter != 0 && pointsToBurn <= armor[GROUNDVEHICLE_LOCATION_TURRET].curArmor)
    {
        armor[GROUNDVEHICLE_LOCATION_TURRET].curArmor -= pointsToBurn;
        return 1;
    }

    return 0;
}

auto GroundVehicle::handleStaticCollision() -> void
{
    if (collisionsOn == 0 || dynamics->getVelocity() <= 0.0f)
    {
        return;
    }

    int32_t blockNumber = 0;
    int32_t vertexNumber = 0;
    getBlockAndVertexNumber(blockNumber, vertexNumber);
    char listName[12];
    std::sprintf(listName, "TBlk%d", blockNumber);
    ObjectQueueNode* list = objectList->head;

    while (list != nullptr && list->operator==(listName) == 0)
    {
        list = list->next;
    }

    Assert(list != nullptr ? 1 : 0, blockNumber, "Could not find objlist for block");

    // Port fix: the original reads the objects of a missing list through null.
    if (list == nullptr)
    {
        return;
    }

    for (BaseObject* object = list->head; object != nullptr; object = object->next)
    {
        auto* other = static_cast<GameObject*>(object);

        if (other->getObjectType() == nullptr)
        {
            continue;
        }

        int collides = 0;
        int32_t otherBlock = -1;
        int32_t otherVertex = -1;

        switch (other->objectClass)
        {
            case BUILDING:
            case TREE:
            case TERRAINOBJECT:
            case TREEBUILDING:
            {
                other->getBlockAndVertexNumber(otherBlock, otherVertex);
                collides = other->collisionsOn;
                break;
            }
            case MISCTERRAINOBJECT:
            {
                getBlockAndVertexNumber(otherBlock, otherVertex);

                if (static_cast<uint32_t>(static_cast<MiscTerrainObject*>(other)->terrainObjectKind) > 6)
                {
                    collides = 1;
                }
                break;
            }
            default:
                break;
        }

        if (vertexNumber == otherVertex && collides != 0)
        {
            collisionSystem->detectStaticCollision(this, other);
        }
    }
}

auto GroundVehicle::init() -> void
{
    objectClass = GROUNDVEHICLE;
    body = static_cast<BodyLocation*>(
        ObjectTypeManager::objectCache->malloc(sizeof(BodyLocation) * NUM_GROUNDVEHICLE_LOCATIONS));
    numBodyLocations = NUM_GROUNDVEHICLE_LOCATIONS;
    armor = static_cast<ArmorLocation*>(
        ObjectTypeManager::objectCache->malloc(sizeof(ArmorLocation) * NUM_GROUNDVEHICLE_LOCATIONS));
    movementEnabled = 1;
    turretEnabled = 1;
    weaponsDeployed = 1;
    numArmorLocations = NUM_GROUNDVEHICLE_LOCATIONS;
    turretRotation = 0.0f;
    unknown8B0 = 0;
    smoke = nullptr;
    statusWindow = nullptr;
    captureable = 0;
    ammoTruck = 0;
    refitBuddy = nullptr;
    refitter = 0;
    unknown8CC = 0;
    sweepTime = -1.0f;
    mineLayer = 0;
    minesToLay = 0;
    elementalCarrier = 0;

    for (int32_t i = 0; i < 10; i++)
    {
        elementals[i] = nullptr;
    }

    for (int32_t i = 0; i < 4; i++)
    {
        passengers[i] = nullptr;
    }

    seats = 0;
    blipFrame = 0;
    cellRowToMine = -1;
    cellColToMine = -1;
    mineCellHandled = 0;
    mineLayTime = 0.0f;
}

auto GroundVehicle::init(ObjectType* objType) -> int32_t
{
    int32_t result = GameObject::init(objType);

    if (result != 0)
    {
        return result;
    }

    auto* vehicleType = static_cast<GroundVehicleType*>(objType);
    collisionsOn = 1;

    for (int32_t location = 0; location < NUM_GROUNDVEHICLE_LOCATIONS; location++)
    {
        bodyAt(location).maxInternalStructure = vehicleType->internalStructure[location];
        bodyAt(location).hasCASE = 0;
        bodyAt(location).damageState = 0;
        bodyAt(location).criticalSpaces = nullptr;
    }

    alignment = vehicleType->alignment;
    internalStructureTonnage = vehicleType->internalStructureTonnage;
    pathLockLevel = vehicleType->crashBlockSelf;
    chassis = vehicleType->chassis;
    tonnageClass = vehicleType->tonnageClass;
    ammoTruck = vehicleType->ammoTruck;
    crashAvoidSelf = vehicleType->crashAvoidSelf;
    crashAvoidPath = vehicleType->crashAvoidPath;
    pathLockRange = vehicleType->crashBlockPath;
    crashYieldTime = vehicleType->crashYieldTime;

    if (vehicleType->refitPoints != 0)
    {
        refitter = 1;
    }

    mineSweeper = vehicleType->mineSweeper;
    minesToLay = vehicleType->minesToLay;

    if (minesToLay > 0)
    {
        mineLayer = 1;
    }

    elementalCarrier = vehicleType->elementalCarrier;
    seats = vehicleType->seats;
    control = nullptr;
    dynamics = vehicleType->dynamicsType->createInstance();

    if (dynamics == nullptr)
    {
        return -0x5fff8;
    }

    if ((result = dynamics->init(vehicleType->dynamicsType, this)) != 0)
    {
        return result;
    }

    const uint32_t appearanceId = vehicleType->appearName;
    AppearanceType* apprType = appearanceTypeList->getAppearance(appearanceId, 0);

    if (apprType == nullptr)
    {
        return -0x2fff7;
    }

    switch (appearanceId & 0xff000000)
    {
        case 0x5000000:
        {
            auto* vehicleAppearance = new GVAppearance;
            appearance = vehicleAppearance;

            if (vehicleAppearance == nullptr)
            {
                return -0x2ffff;
            }

            vehicleAppearance->init(nullptr, nullptr);

            if ((apprType->appearanceNum & 0xff000000) != 0x5000000)
            {
                return -0x2fff6;
            }

            if ((result = vehicleAppearance->init(apprType, this)) != 0)
            {
                return result;
            }

            gvAppearance = 1;
            weaponsDeployed = 1;
            break;
        }

        case 0x9000000:
        {
            auto* turretAppearance = new PUAppearance;
            appearance = turretAppearance;

            if (turretAppearance == nullptr)
            {
                return -0x2ffff;
            }

            turretAppearance->init(nullptr, nullptr);

            if ((apprType->appearanceNum & 0xff000000) != 0x9000000)
            {
                return -0x2fff6;
            }

            if ((result = turretAppearance->init(apprType, this)) != 0)
            {
                return result;
            }

            gvAppearance = 0;
            weaponsDeployed = 0;
            break;
        }

        default:
            break;
    }

    objectClass = GROUNDVEHICLE;
    unknown7C8 = 1000.0f;
    return 0;
}

auto GroundVehicle::setControl(uint32_t controlType, uint32_t controlData, int32_t controlParam) -> int32_t
{
    int32_t result = 0;

    switch (controlType)
    {
        case 1:
        {
            delete control;
            auto* playerControl = new PlayerControl;
            control = playerControl;

            if (playerControl == nullptr)
            {
                return -0x5fffc;
            }

            if ((result = playerControl->init(this, 0)) != 0)
            {
                return result;
            }
            break;
        }

        case 2:
        {
            delete control;
            auto* aiControl = new GroundVehicleAIControl;
            control = aiControl;

            if (aiControl == nullptr)
            {
                return -0x5fffc;
            }

            if ((result = aiControl->init(this)) != 0)
            {
                return result;
            }
            break;
        }

        case 3:
        {
            delete control;
            auto* netControl = new GroundVehicleNetControl;
            control = netControl;

            if (netControl == nullptr)
            {
                return -0x5fffc;
            }

            if ((result = netControl->init(this)) != 0)
            {
                return result;
            }
            break;
        }

        default:
            return -0x5fffb;
    }

    if (controlData != 2 && controlData != 0xffffffff)
    {
        return -0x5fff9;
    }

    auto* vehicleControlData = new GroundVehicleControlData;
    control->controlData = vehicleControlData;

    if (vehicleControlData == nullptr)
    {
        return -0x5fffa;
    }

    return vehicleControlData->init(0);
}

auto GroundVehicle::init(FitIniFile* vehicleFile) -> int32_t
{
    static const char* const locationNames[NUM_GROUNDVEHICLE_LOCATIONS] = {"Front", "Left", "Right", "Rear", "Turret"};

    int32_t result = vehicleFile->seekBlock("Header");

    if (result != 0)
    {
        return result;
    }

    char fileType[128];

    if ((result = vehicleFile->readIdString("FileType", fileType, 127)) != 0)
    {
        return result;
    }

    if (std::strcmp(fileType, "GroundVehicleProfile") != 0)
    {
        return -1;
    }

    if ((result = vehicleFile->seekBlock("General")) != 0)
    {
        return result;
    }

    char crewBuffer[128];
    vehicleFile->readIdString("Crew", crewBuffer, 127);
    crewName = static_cast<char*>(systemHeap->malloc(static_cast<uint32_t>(std::strlen(crewBuffer) + 1)));
    std::strcpy(crewName, crewBuffer);

    if (vehicleFile->readIdBoolean("NotMineYet", notMineYet) != 0)
    {
        notMineYet = 1;
    }

    if (vehicleFile->readIdLong("DescIndex", descIndex) != 0)
    {
        descIndex = -1;
    }

    char ifaceNameBuffer[256];
    cLoadString(thisInstance, descIndex + 700, ifaceNameBuffer, 0xfe);
    debugStatus = static_cast<char*>(systemHeap->malloc(static_cast<uint32_t>(std::strlen(ifaceNameBuffer) + 1)));
    std::strcpy(debugStatus, ifaceNameBuffer);

    if ((result = vehicleFile->readIdLong("NameIndex", nameIndex)) != 0)
    {
        return result;
    }

    if ((result = vehicleFile->readIdFloat("CurTonnage", tonnage)) != 0)
    {
        return result;
    }

    char fileStatus = 0;

    if ((result = vehicleFile->readIdChar("Status", fileStatus)) != 0)
    {
        return result;
    }

    status = fileStatus;

    if ((result = vehicleFile->readIdString("icon", iconName, 0x13)) != 0)
    {
        return result;
    }

    if (vehicleFile->readIdLong("BattleRating", battleRating) != 0)
    {
        battleRating = -1;
    }

    if ((result = vehicleFile->seekBlock("Engine")) != 0)
    {
        return result;
    }

    if ((result = vehicleFile->readIdFloat("Tonnage", engineTonnage)) != 0)
    {
        return result;
    }

    if ((result = vehicleFile->readIdULong("Rating", engineRating)) != 0)
    {
        return result;
    }

    uint8_t moveSpeed = 0;

    if ((result = vehicleFile->readIdUChar("MaxMoveSpeed", moveSpeed)) != 0)
    {
        return result;
    }

    maxRunSpeed = static_cast<float>(moveSpeed);

    if (vehicleFile->seekBlock("MovementSystem") == 0)
    {
        int32_t value = 0;

        if (vehicleFile->readIdLong("CrashAvoidSelf", value) == 0)
        {
            crashAvoidSelf = value;
        }

        if (vehicleFile->readIdLong("CrashAvoidPath", value) == 0)
        {
            crashAvoidPath = value;
        }

        if (vehicleFile->readIdLong("CrashBlockSelf", value) == 0)
        {
            pathLockLevel = value;
        }

        if (vehicleFile->readIdLong("CrashBlockPath", value) == 0)
        {
            pathLockRange = value;
        }

        float yieldTime = 0.0f;

        if (vehicleFile->readIdFloat("CrashYieldTime", yieldTime) == 0)
        {
            crashYieldTime = yieldTime;
        }
    }

    if ((result = vehicleFile->seekBlock("Armor")) != 0)
    {
        return result;
    }

    if ((result = vehicleFile->readIdUChar("Type", armorType)) != 0)
    {
        return result;
    }

    if ((result = vehicleFile->readIdFloat("Tonnage", armorTonnage)) != 0)
    {
        return result;
    }

    if ((result = vehicleFile->seekBlock("InventoryInfo")) != 0)
    {
        return result;
    }

    if ((result = vehicleFile->readIdUChar("NumOther", numOther)) != 0)
    {
        return result;
    }

    if ((result = vehicleFile->readIdUChar("NumWeapons", numWeapons)) != 0)
    {
        return result;
    }

    if ((result = vehicleFile->readIdUChar("NumAmmo", numAmmos)) != 0)
    {
        return result;
    }

    const int32_t firstWeapon = numOther;
    const int32_t firstAmmo = numOther + numWeapons;
    const int32_t numItems = numOther + numAmmos + numWeapons;
    inventory = static_cast<InventoryItem*>(
        ObjectTypeManager::objectCache->malloc(static_cast<uint32_t>(numItems * sizeof(InventoryItem))));

    if (inventory == nullptr)
    {
        return -2;
    }

    numAntiMissileSystems = 0;
    char blockName[32];

    for (int32_t item = 0; item < firstWeapon; item++)
    {
        std::sprintf(blockName, "Item:%d", item);

        if ((result = vehicleFile->seekBlock(blockName)) != 0)
        {
            return result;
        }

        InventoryItem& other = inventory[item];

        if ((result = vehicleFile->readIdUChar("MasterID", other.masterID)) != 0)
        {
            return result;
        }

        other.health = MasterComponentList[other.masterID].health;
        other.disabled = 0;
        other.amount = 1;
        other.ammoIndex = -1;
        other.readyTime = 0.0f;
        other.bodyLocation = 0xff;
        other.rangeRatings = nullptr;

        switch (MasterComponentList[other.masterID].form)
        {
            case COMPONENT_FORM_COCKPIT:
                cockpit = static_cast<uint8_t>(item);
                break;
            case COMPONENT_FORM_SENSOR:
            {
                sensor = static_cast<uint8_t>(item);
                sensorSystem = sensorSystemManager->newSensor();
                sensorSystem->owner = this;
                sensorSystem->setRange(MasterComponentList[inventory[item].masterID].rangeOrHeat);
                break;
            }
            case COMPONENT_FORM_ENGINE:
                engine = static_cast<uint8_t>(item);
                break;
            case COMPONENT_FORM_LIFESUPPORT:
                lifeSupport = static_cast<uint8_t>(item);
                break;
            case COMPONENT_FORM_ECM:
                ecm = static_cast<uint8_t>(item);
                break;
            case COMPONENT_FORM_PROBE:
                probe = static_cast<uint8_t>(item);
                break;
            default:
                break;
        }
    }

    for (int32_t item = firstWeapon; item < firstAmmo; item++)
    {
        std::sprintf(blockName, "Item:%d", item);

        if ((result = vehicleFile->seekBlock(blockName)) != 0)
        {
            return result;
        }

        InventoryItem& weapon = inventory[item];

        if ((result = vehicleFile->readIdUChar("MasterID", weapon.masterID)) != 0)
        {
            return result;
        }

        if ((result = vehicleFile->readIdUChar("FacesForward", weapon.facesForward)) != 0)
        {
            return result;
        }

        const MasterComponent& component = MasterComponentList[weapon.masterID];
        weapon.health = component.health;
        weapon.disabled = 0;
        weapon.amount = 1;
        weapon.ammoIndex = -1;
        weapon.readyTime = 0.0f;
        weapon.bodyLocation = 0xff;
        // As BattleMech::init: damage per ten seconds, then scaled by the long range over 24.
        weapon.effectiveness =
            static_cast<int16_t>(static_cast<int32_t>(component.damage * 10.0 / component.recycleTime));
        weapon.effectiveness = static_cast<int16_t>(static_cast<int32_t>(
            static_cast<double>(component.weaponRange[3]) * weapon.effectiveness * static_cast<double>(1.0f / 24.0f)));
        weapon.rangeRatings =
            static_cast<float*>(ObjectTypeManager::objectCache->malloc(NumRangeRatings * 2 * sizeof(float)));

        if (weapon.rangeRatings == nullptr)
        {
            Fatal(0, " No RAM for Weapon Range Ratings ");
        }

        std::memset(weapon.rangeRatings, 0, NumRangeRatings * 2 * sizeof(float));
        objectTypeManager->load(
            static_cast<int32_t>(
                weaponFXTable[static_cast<int8_t>(MasterComponentList[inventory[item].masterID].weaponEffect)]),
            1);
    }

    for (int32_t item = firstAmmo; item < numItems; item++)
    {
        std::sprintf(blockName, "Item:%d", item);

        if ((result = vehicleFile->seekBlock(blockName)) != 0)
        {
            return result;
        }

        InventoryItem& ammo = inventory[item];

        if ((result = vehicleFile->readIdUChar("MasterID", ammo.masterID)) != 0)
        {
            return result;
        }

        int32_t amount = 0;

        if (vehicleFile->readIdLong("Amount", amount) != 0)
        {
            uint8_t smallAmount = 0;

            if ((result = vehicleFile->readIdUChar("Amount", smallAmount)) != 0)
            {
                return result;
            }

            amount = smallAmount;
        }

        if (amount == -1)
        {
            amount = MasterComponentList[ammo.masterID].longValue;
        }

        ammo.amount = static_cast<int16_t>(amount);
        ammo.startAmount = ammo.amount;
        ammo.ammoIndex = -1;
        ammo.health = MasterComponentList[ammo.masterID].health;
        ammo.disabled = 0;
        ammo.readyTime = 0.0f;
        ammo.bodyLocation = 0xff;
        ammo.rangeRatings = nullptr;
    }

    for (int32_t location = 0; location < NUM_GROUNDVEHICLE_LOCATIONS; location++)
    {
        if ((result = vehicleFile->seekBlock(locationNames[location])) != 0)
        {
            return result;
        }

        BodyLocation& bodyLocation = bodyAt(location);
        bodyLocation.hasCASE = 0;
        uint8_t internalStructure = 0;

        if ((result = vehicleFile->readIdUChar("CurInternalStructure", internalStructure)) != 0)
        {
            return result;
        }

        bodyLocation.curInternalStructure = static_cast<float>(internalStructure);
        const double structureLeft =
            static_cast<double>(internalStructure) / static_cast<double>(bodyLocation.maxInternalStructure);

        if (structureLeft == 0.0)
        {
            bodyLocation.damageState = 2;
        }
        else if (structureLeft > 0.5)
        {
            bodyLocation.damageState = 0;
        }
        else
        {
            bodyLocation.damageState = 1;
        }

        if ((result = vehicleFile->readIdUChar("MaxArmorPoints", armor[location].maxArmor)) != 0)
        {
            return result;
        }

        uint8_t points = 0;

        if ((result = vehicleFile->readIdUChar("CurArmorPoints", points)) != 0)
        {
            return result;
        }

        armor[location].curArmor = static_cast<float>(points);
        bodyLocation.criticalSpaces = nullptr;
    }

    calcAmmoTotals();

    for (int32_t item = firstWeapon; item < firstAmmo; item++)
    {
        for (int32_t ammoType = 0; ammoType < numAmmoTypes; ammoType++)
        {
            if (static_cast<int32_t>(MasterComponentList[inventory[item].masterID].ammoMasterId) ==
                ammoTypeTotal[ammoType].masterId)
            {
                inventory[item].ammoIndex = static_cast<int16_t>(ammoType);
                break;
            }
        }
    }

    for (int32_t item = 0; item < firstWeapon; item++)
    {
        const int32_t masterID = inventory[item].masterID;

        if (masterID != MasterClanAntiMissileSystemID && masterID != MasterInnerSphereAntiMissileSystemID)
        {
            continue;
        }

        for (int32_t ammoType = 0; ammoType < numAmmoTypes; ammoType++)
        {
            if (static_cast<int32_t>(MasterComponentList[masterID].ammoMasterId) == ammoTypeTotal[ammoType].masterId)
            {
                inventory[item].ammoIndex = static_cast<int16_t>(ammoType);
                break;
            }
        }
    }

    calcLongestRangeWeapon();
    calcWeaponEffectiveness(1);
    calcWeaponEffectiveness(0);
    maxCV = calcCV(1);
    curCV = calcCV(0);

    if (refitter != 0)
    {
        // The refit pool lives in the turret's armor slot.
        const uint8_t refitPoints = static_cast<uint8_t>(static_cast<GroundVehicleType*>(objType)->refitPoints);
        armor[GROUNDVEHICLE_LOCATION_TURRET].maxArmor = refitPoints;
        armor[GROUNDVEHICLE_LOCATION_TURRET].curArmor = static_cast<float>(refitPoints);
    }

    return 0;
}

auto GroundVehicle::calcCV(int calcMax) -> int32_t
{
    if (battleRating != -1)
    {
        return battleRating;
    }

    // Offense: the weapons' ratings, scaled by the top speed.
    double offense = 0.0;
    const int32_t firstWeapon = numOther;

    for (int32_t item = firstWeapon; item < firstWeapon + numWeapons; item++)
    {
        if (calcMax != 0 || inventory[item].disabled == 0)
        {
            offense += MasterComponentList[inventory[item].masterID].battleRating;
        }
    }

    offense *= (maxRunSpeed - 18.0) * 0.05555555555555555 + 1.0;

    // Defense: structure, armor, tonnage, the speed class and the other equipment.
    double defense = 0.0;

    for (int32_t location = 0; location < NUM_GROUNDVEHICLE_LOCATIONS; location++)
    {
        defense += calcMax != 0 ? static_cast<double>(bodyAt(location).maxInternalStructure)
                                : bodyAt(location).curInternalStructure;
    }

    for (int32_t location = 0; location < NUM_GROUNDVEHICLE_LOCATIONS; location++)
    {
        defense += calcMax != 0 ? static_cast<double>(armor[location].maxArmor) : armor[location].curArmor;
    }

    defense += tonnageClass;
    int32_t speedClass = 0;

    while (speedClass < 5 && static_cast<float>(TargetMoveModifierTable[speedClass][0]) < maxRunSpeed)
    {
        speedClass++;
    }

    // Port fix: past the table (a top speed over 999) the original reads the word after it.
    if (speedClass == 5)
    {
        speedClass = 4;
    }

    defense += TargetMoveModifierTable[speedClass][1] * 10;

    for (int32_t item = 0; item < firstWeapon; item++)
    {
        if (calcMax != 0 || inventory[item].disabled == 0)
        {
            defense += MasterComponentList[inventory[item].masterID].battleRating;
        }
    }

    return static_cast<int32_t>(defense + offense);
}

auto GroundVehicle::destroy() -> void
{
    systemHeap->free(crewName);
    crewName = nullptr;

    if (statusWindow != nullptr)
    {
        closeStatusWindow();
        statusWindow = nullptr;
    }
}

auto GroundVehicle::mineCheck() -> void
{
    if (MPlayer != nullptr && MPlayer->isServer == 0)
    {
        return;
    }

    ScenarioMap* map = GameMap;

    // The mine state bits of a tile's overlay: Inner Sphere 11..12, Clan 13..14; the spread counts 25..26, 27..28.
    if (mineCellHandled != 0)
    {
        const MapTile& tile = map->map[objPosition->tileR * map->width + objPosition->tileC];
        const uint32_t state = alignment == -1 ? tile.overlay >> 11 : tile.overlay >> 13;

        if ((state & 3) == 0)
        {
            mineCellHandled = 0;
            const int32_t tileR = objPosition->tileR;
            const int32_t tileC = objPosition->tileC;
            MapTile& here = map->map[map->width * tileR + tileC];

            if (getAlignment() == -1)
            {
                here.overlay = (here.overlay & 0xffffefff) | 0x800;
            }
            else
            {
                here.overlay = (here.overlay & 0xffffbfff) | 0x2000;
            }

            if (MPlayer != nullptr)
            {
                MPlayer->addMineChunk(tileR * 3, tileC * 3, alignment != -1 ? 1 : 0, 1, 0);
                map = GameMap;
            }
        }
    }

    const uint32_t mine =
        alignment == -1
            ? map->getInnerSphereMine(objPosition->tileR, objPosition->tileC, objPosition->cellR, objPosition->cellC)
            : map->getClanMine(objPosition->tileR, objPosition->tileC, objPosition->cellR, objPosition->cellC);

    if (mine == 0)
    {
        return;
    }

    int32_t firstRow = objPosition->tileR - 1;
    int32_t firstCol = objPosition->tileC - 1;

    if (firstRow < 0)
    {
        firstRow = 0;
    }

    if (firstCol < 0)
    {
        firstCol = 0;
    }

    const int32_t mapSide = Terrain::verticesBlockSide * Terrain::blocksMapSide;

    if (mapSide <= firstCol + 3)
    {
        firstCol = mapSide - 1;
    }

    if (mapSide <= firstRow + 3)
    {
        firstRow = mapSide - 1;
    }

    for (int32_t row = firstRow; row < firstRow + 3; row++)
    {
        for (int32_t col = firstCol; col < firstCol + 3; col++)
        {
            const bool inMap = row >= 0 && row < GameMap->height && col >= 0 && col < GameMap->width;
            Assert(inMap ? 1 : 0, 0, " Map Tile out of bounds ");

            // Port fix: the original goes on to touch the tile past the map's edge.
            if (!inMap)
            {
                continue;
            }

            MapTile& tile = GameMap->map[GameMap->width * row + col];
            const bool innerSphere = getAlignment() == -1;
            uint32_t count = ((innerSphere ? tile.overlay >> 25 : tile.overlay >> 27) & 3) + 1;

            if (count > 3)
            {
                count = 3;
            }

            if (getAlignment() == -1)
            {
                tile.overlay = (tile.overlay & 0xf9ffffff) | (count << 25);
            }
            else
            {
                tile.overlay = (tile.overlay & 0xe7ffffff) | (count << 27);
            }
        }
    }

    int32_t chunkResult = 0;

    if (mineSweeper != 0)
    {
        // A sweeper sets the mine off harmlessly, at the cost of a point of front armor.
        sweepTime = 0.0f;
        vector_3d position = getPosition();
        CreateExplosion(MineExplosion, position, 0.0f, 0.0f);
        armor[GROUNDVEHICLE_LOCATION_FRONT].curArmor -= 1.0f;

        if (MPlayer != nullptr)
        {
            _WeaponShotInfo shotInfo;
            shotInfo.init(nullptr, -2, 1.0f, 0, 0.0f);
            MPlayer->addWeaponHitChunk(this, &shotInfo, 0);
        }

        if (armor[GROUNDVEHICLE_LOCATION_FRONT].curArmor == 0.0f)
        {
            mineSweeper = 0;
            sweepTime = -1.0f;
            pilot->clearCurTacOrder(1, 0);
        }

        chunkResult = 1;
    }
    else
    {
        if (mineLayer != 0)
        {
            mineCellHandled = 1;
            return;
        }

        vector_3d position = getPosition();
        CreateExplosion(MineExplosion, position, MineSplashDamage, worldUnitsPerMeter * MineSplashRange);
        const int32_t hitLocation = calcHitLocation(nullptr, -1, 3, 0);
        _WeaponShotInfo shotInfo;
        shotInfo.init(nullptr, -2, MineBaseDamage, hitLocation, 0.0f);
        handleWeaponHit(&shotInfo, MPlayer != nullptr);

        if (getPilot() != nullptr)
        {
            getPilot()->radioMessage(0x16, 1);
        }

        pilot->pausePath();
        chunkResult = 2;
    }

    const int32_t tileR = objPosition->tileR;
    const int32_t tileC = objPosition->tileC;
    MapTile& here = GameMap->map[GameMap->width * tileR + tileC];

    if (getAlignment() == -1)
    {
        here.overlay |= 0x1800;
    }
    else
    {
        here.overlay |= 0x6000;
    }

    if (MPlayer != nullptr)
    {
        MPlayer->addMineChunk(tileR * 3 + objPosition->cellR, tileC * 3 + objPosition->cellC, alignment != -1 ? 1 : 0,
                              3, chunkResult);
    }

    mineCellHandled = 1;
}

auto GroundVehicle::pivotTo() -> int
{
    MechWarrior* warrior = pilot;
    MovePath* path = warrior->getMovePath();
    const int32_t moveStateGoal = warrior->moveOrders.moveStateGoal;
    const int32_t moveState = warrior->moveOrders.moveState;
    const int32_t run = MPlayer == nullptr || MPlayer->isServer != 0 ? warrior->moveOrders.run : moveChunk.run;
    int hasTarget = 0;
    GameObject* target = warrior->getLastTarget();
    float targetFacing = 0.0f;
    vector_3d targetPosition;
    auto* dynType = static_cast<GroundVehicleDynamicsType*>(static_cast<GroundVehicleType*>(objType)->dynamicsType);
    const float maxPivot = static_cast<float>(dynType->maxVehiclePivotRate) * frameLength;

    if (target == nullptr)
    {
        if (warrior->curTacOrder.code == TACTICAL_ORDER_ATTACK_POINT)
        {
            targetPosition = warrior->attackOrders.targetPoint;
            targetFacing = relFacingTo(targetPosition, -1);
            hasTarget = 1;
        }
    }
    else
    {
        targetPosition = target->getPosition();
        targetFacing = relFacingTo(targetPosition, -1);
        hasTarget = 1;
    }

    // Starts the pivot: a turn of <paramref name="turn"/> degrees, no faster than the pivot rate.
    const auto pivot = [&](float turn) -> int
    {
        if (maxPivot < std::fabs(turn))
        {
            turn = turn <= 0.0f ? -maxPivot : maxPivot;
        }

        auto* controlData = static_cast<GroundVehicleControlData*>(control->controlData);
        controlData->rotate = static_cast<int8_t>(static_cast<int32_t>(turn / maxPivot * 64.0f));
        controlData->pivot = 1;
        updateTurret(turn);
        return 1;
    };

    const auto choosePivotDirection = [&]()
    {
        if (pivotDirection == 0xff)
        {
            pivotDirection = targetFacing >= 0.0f ? 1 : 0;
        }
    };

    const auto hasNextStep = [&]()
    { return path->numStepsWhenNotPaused >= 1 && path->curStep < path->numStepsWhenNotPaused; };

    if (moveState == MOVESTATE_PIVOT_FORWARD)
    {
        if (moveStateGoal == MOVESTATE_PIVOT_FORWARD || moveStateGoal == MOVESTATE_FORWARD)
        {
            if (!hasNextStep())
            {
                pilot->moveOrders.moveStateGoal = MOVESTATE_FORWARD;
            }
            else
            {
                const vector_3d destination = path->stepList[path->curStep].destination;
                static_cast<GroundVehicleControlData*>(control->controlData)->throttle = 0;
                const float stepFacing = relFacingTo(destination, -1);

                if (stepFacing < -45.0f || stepFacing > 45.0f)
                {
                    float turn = -stepFacing;

                    if (hasTarget != 0 && run == 0)
                    {
                        choosePivotDirection();

                        if (pivotDirection == 0)
                        {
                            if (stepFacing >= 0.0f)
                            {
                                turn = 360.0f - stepFacing;
                            }
                        }
                        else if (stepFacing < 0.0f)
                        {
                            turn = -360.0f - stepFacing;
                        }
                    }

                    return pivot(turn);
                }

                pilot->moveOrders.moveState = MOVESTATE_FORWARD;

                if (pilot->moveOrders.unknown1030 != 0)
                {
                    pilot->moveOrders.unknown1030 = 0;
                }
            }
        }
        else
        {
            pilot->moveOrders.moveState = MOVESTATE_FORWARD;
        }
    }
    else if (moveState == MOVESTATE_PIVOT_REVERSE)
    {
        if (moveStateGoal == MOVESTATE_PIVOT_REVERSE || moveStateGoal == MOVESTATE_REVERSE)
        {
            if (!hasNextStep())
            {
                pilot->moveOrders.moveStateGoal = MOVESTATE_FORWARD;
            }
            else
            {
                const vector_3d destination = path->stepList[path->curStep].destination;
                static_cast<GroundVehicleControlData*>(control->controlData)->throttle = 0;
                const float stepFacing = relFacingTo(destination, -1);

                if (stepFacing > -135.0f && stepFacing < 135.0f)
                {
                    bool turnLeft;

                    if (hasTarget == 0 || run != 0)
                    {
                        turnLeft = stepFacing < 0.0f;
                    }
                    else
                    {
                        choosePivotDirection();
                        turnLeft = pivotDirection != 0;
                    }

                    return pivot(turnLeft ? -180.0f - stepFacing : 180.0f - stepFacing);
                }

                MechWarrior* orders = pilot;

                if (orders->moveOrders.unknown1030 != 0)
                {
                    orders->moveOrders.unknown1030 = 0;
                }

                if (moveStateGoal == MOVESTATE_REVERSE)
                {
                    orders->moveOrders.moveState = MOVESTATE_REVERSE;
                }
                else
                {
                    orders->moveOrders.moveStateGoal = MOVESTATE_FORWARD;
                }
            }
        }
        else
        {
            pilot->moveOrders.moveState = MOVESTATE_FORWARD;
        }
    }
    else if (moveState != MOVESTATE_PIVOT_TARGET)
    {
        if (moveStateGoal == MOVESTATE_PIVOT_TARGET || moveStateGoal == MOVESTATE_PIVOT_FORWARD ||
            moveStateGoal == MOVESTATE_PIVOT_REVERSE)
        {
            pilot->moveOrders.moveState = moveStateGoal;
        }
    }
    else if (moveStateGoal != MOVESTATE_PIVOT_TARGET)
    {
        pilot->moveOrders.moveState = MOVESTATE_FORWARD;
    }
    else if (run == 0 && hasTarget != 0)
    {
        static_cast<GroundVehicleControlData*>(control->controlData)->throttle = 0;
        const float facing = relFacingTo(targetPosition, -1);
        const float fireArc = getFireArc();

        if (facing < -fireArc || fireArc < facing)
        {
            return pivot(-facing);
        }

        pilot->moveOrders.moveStateGoal = MOVESTATE_FORWARD;
    }
    else
    {
        pilot->moveOrders.moveStateGoal = MOVESTATE_FORWARD;
    }

    MechWarrior* orders = pilot;

    if (!(orders->moveOrders.yieldTime > -1.0f || orders->moveOrders.waitForPointTime > -1.0f))
    {
        orders->resumePath();
    }

    pivotDirection = 0xff;
    return 0;
}

auto GroundVehicle::calcThrottleLimits(int32_t& minThrottle, int32_t& maxThrottle) -> void
{
    const MapTile& tile = GameMap->map[objPosition->tileR * GameMap->width + objPosition->tileC];
    const float tileFactor = TileThrottleMultiplier[chassis][tile.cells & 0x7f];
    const float overlayFactor = OverlayThrottleMultiplier[chassis][tile.overlay & 0x7f];
    // Each limit goes through a short, as in the original.
    maxThrottle = static_cast<int16_t>(static_cast<int32_t>(std::floor(static_cast<float>(maxThrottle) * tileFactor)));
    minThrottle = static_cast<int16_t>(static_cast<int32_t>(std::floor(static_cast<float>(minThrottle) * tileFactor)));
    maxThrottle =
        static_cast<int16_t>(static_cast<int32_t>(std::floor(static_cast<float>(maxThrottle) * overlayFactor)));
    minThrottle =
        static_cast<int16_t>(static_cast<int32_t>(std::floor(static_cast<float>(minThrottle) * overlayFactor)));
}

auto GroundVehicle::getSpeedState() -> int32_t
{
    return getBodyState() == 1 ? 2 : 0;
}

auto GroundVehicle::updateMoveStateGoal() -> void
{
    MechWarrior* warrior = pilot;
    MovePath* path = warrior->getMovePath();
    const int32_t moveStateGoal = warrior->moveOrders.moveStateGoal;

    if (path->numSteps < 1)
    {
        if (moveStateGoal != MOVESTATE_PIVOT_TARGET && moveStateGoal != MOVESTATE_PIVOT_FORWARD &&
            moveStateGoal != MOVESTATE_PIVOT_REVERSE)
        {
            warrior->moveOrders.moveStateGoal = MOVESTATE_FORWARD;
        }

        return;
    }

    const int32_t run = MPlayer == nullptr || MPlayer->isServer != 0 ? warrior->moveOrders.run : moveChunk.run;

    if (run != 0)
    {
        warrior->moveOrders.moveStateGoal = MOVESTATE_FORWARD;
        return;
    }

    vector_3d targetPosition;
    GameObject* target = warrior->getLastTarget();

    if (target == nullptr)
    {
        if (warrior->curTacOrder.code != TACTICAL_ORDER_ATTACK_POINT)
        {
            warrior->moveOrders.moveStateGoal = MOVESTATE_FORWARD;
            return;
        }

        targetPosition = warrior->attackOrders.targetPoint;
    }
    else
    {
        targetPosition = target->getPosition();
    }

    if (path->numStepsWhenNotPaused <= 0 || path->curStep >= path->numStepsWhenNotPaused)
    {
        return;
    }

    const double delta = relFacingDelta(path->stepList[path->curStep].destination, targetPosition);
    MechWarrior* orders = pilot;
    const double turretArc =
        static_cast<GroundVehicleDynamicsType*>(static_cast<GroundVehicleType*>(objType)->dynamicsType)->maxTurretYaw;

    if (orders->moveOrders.moveStateGoal == MOVESTATE_FORWARD)
    {
        // The target is behind: drive backward.
        if (turretArc < delta && 180.0 - delta <= turretArc && orders->moveOrders.unknown1030 == 0)
        {
            orders->moveOrders.unknown1030 = 1;
            orders->moveOrders.moveStateGoal = MOVESTATE_REVERSE;
        }
    }
    else if (turretArc < 180.0 - delta && delta <= turretArc && orders->moveOrders.unknown1030 == 0)
    {
        orders->moveOrders.unknown1030 = 1;
        orders->moveOrders.moveStateGoal = MOVESTATE_FORWARD;
    }
}

namespace
{
    /// <summary>
    /// Steers along the move path, the part updateMovePath and netUpdateMovePath share (each has its own copy in
    /// MCX.EXE): advances past reached steps, turns toward the step and sets the throttle for the move state, or
    /// pauses the path and asks for a pivot. Then caps the throttle of a sweeper that just cleared a mine and of a
    /// layer laying mines.
    /// </summary>
    /// <returns>1 at the path's end, else 0.</returns>
    int steerAlongPath(GroundVehicle* vehicle, char& newRotate, char& newThrottleSetting, float& newRotatePerSec,
                       int32_t& newMoveState, int32_t& maxThrottle)
    {
        auto* dynType =
            static_cast<GroundVehicleDynamicsType*>(static_cast<GroundVehicleType*>(vehicle->objType)->dynamicsType);
        MovePath* path = vehicle->pilot->getMovePath();
        int result = 0;
        const auto steer = [&]()
        {
            if (path->numSteps < 1)
            {
                newThrottleSetting = 0;
                return;
            }

            int32_t step = path->curStep;

            if (step == path->numSteps)
            {
                result = 1;
                return;
            }

            vector_3d destination = path->stepList[step].destination;
            vehicle->lastValidPosition = destination;
            const float distance = vehicle->distanceFrom(destination);
            const int32_t numSteps = path->numSteps;
            const float margin = step == numSteps - 1 ? MoveMarginOfError[1] : MoveMarginOfError[0];
            MaxVelocityMag = worldUnitsPerMeter * 100.0f;

            if (distance < margin)
            {
                // Reached the step: on to the next.
                step++;
                vehicle->pilot->moveOrders.timeOfLastStep = scenarioTime;
                path->curStep = step;

                if (numSteps <= step)
                {
                    MaxVelocityMag = worldUnitsPerMeter * distance;
                    result = 1;
                    return;
                }

                destination = path->stepList[step].destination;
            }

            const float facing = vehicle->relFacingTo(destination, -1);
            MechWarrior* orders = vehicle->pilot;
            const int32_t moveState = orders->moveOrders.moveState;
            const int32_t moveStateGoal = orders->moveOrders.moveStateGoal;
            const float maxTurn = static_cast<float>(dynType->maxVehicleYawRate) * frameLength;

            if (moveState == MOVESTATE_FORWARD)
            {
                if (moveStateGoal == MOVESTATE_FORWARD)
                {
                    newThrottleSetting = 100;

                    if (facing < -5.0f || facing > 5.0f)
                    {
                        newRotatePerSec = -facing;

                        if (maxTurn < std::fabs(newRotatePerSec))
                        {
                            newRotatePerSec = newRotatePerSec <= 0.0f ? -maxTurn : maxTurn;
                        }

                        newRotate = static_cast<char>(static_cast<int32_t>(newRotatePerSec / maxTurn * 64.0f));
                    }

                    return;
                }

                orders->pausePath();

                if (moveStateGoal == MOVESTATE_REVERSE || moveStateGoal == MOVESTATE_PIVOT_REVERSE)
                {
                    newMoveState = MOVESTATE_PIVOT_REVERSE;
                }
                else if (moveStateGoal == MOVESTATE_PIVOT_FORWARD)
                {
                    newMoveState = MOVESTATE_PIVOT_FORWARD;
                }
                else
                {
                    newMoveState = MOVESTATE_FORWARD;
                }

                return;
            }

            if (moveState == MOVESTATE_REVERSE)
            {
                if (moveStateGoal == MOVESTATE_REVERSE)
                {
                    newThrottleSetting = -100;
                    newRotatePerSec = facing >= 0.0f ? -(facing - 180.0f) : -(facing + 180.0f);

                    if (std::fabs(newRotatePerSec) <= maxTurn)
                    {
                        newThrottleSetting = -100;
                    }
                    else
                    {
                        // Turning hard: back up at half speed.
                        newRotatePerSec = newRotatePerSec <= 0.0f ? -maxTurn : maxTurn;
                        newThrottleSetting = -50;
                    }

                    newRotate = static_cast<char>(static_cast<int32_t>(newRotatePerSec / maxTurn * 64.0f));
                    return;
                }

                orders->pausePath();

                if (moveStateGoal == MOVESTATE_FORWARD || moveStateGoal == MOVESTATE_PIVOT_FORWARD)
                {
                    newMoveState = MOVESTATE_PIVOT_FORWARD;
                }
                else if (moveStateGoal == MOVESTATE_PIVOT_REVERSE)
                {
                    newMoveState = MOVESTATE_PIVOT_REVERSE;
                }
                else
                {
                    newMoveState = MOVESTATE_FORWARD;
                }

                return;
            }

            if (moveStateGoal == MOVESTATE_FORWARD || moveStateGoal == MOVESTATE_PIVOT_FORWARD)
            {
                orders->pausePath();
                newMoveState = MOVESTATE_PIVOT_FORWARD;
            }
            else if (moveStateGoal == MOVESTATE_REVERSE || moveStateGoal == MOVESTATE_PIVOT_REVERSE)
            {
                orders->pausePath();
                newMoveState = MOVESTATE_PIVOT_REVERSE;
            }
        };

        steer();

        if (vehicle->mineSweeper != 0 && vehicle->sweepTime > 0.0f && vehicle->sweepTime < gvSweepTime)
        {
            maxThrottle = MineSweepThrottle;
        }

        if (vehicle->mineLayer != 0 && vehicle->pilot->curTacOrder.moveParams.mode == 1)
        {
            maxThrottle = MineLayThrottle;
        }

        return result;
    }
}

auto GroundVehicle::updateMovePath(char& newRotate, char& newThrottleSetting, float& newRotatePerSec,
                                   int32_t& newMoveState, int32_t& minThrottle, int32_t& maxThrottle) -> int
{
    MechWarrior* warrior = pilot;
    auto* controlData = static_cast<GroundVehicleControlData*>(control->controlData);
    warrior->getMovePath();
    newThrottleSetting = static_cast<char>(controlData->throttle);
    const int32_t running = warrior->moveOrders.run;
    newRotatePerSec = 0.0f;
    updateHustleTime();
    const bool hustling = scenarioTime < lastHustleTime + 2.0f;
    warrior = pilot;
    Mover* point = warrior->getPoint();
    const bool groupMove = warrior->curTacOrder.isGroupOrder() != 0 && warrior->curTacOrder.isMoveOrder() != 0;

    if (running == 0 && !hustling && point != nullptr && point->isDisabled() == 0 && point != this && groupMove)
    {
        // Keep pace with the group's point: wait (at most five seconds while moving) when ahead of it.
        MechWarrior* pointPilot = point->getPilot();
        pointPilot->getMovePath();
        const float pointDistanceLeft = pointPilot->getMoveDistanceLeft();

        if (pointDistanceLeft <= warrior->getMoveDistanceLeft())
        {
            warrior->moveOrders.waitForPointTime = -1.0f;

            if (warrior->moveOrders.yieldTime <= -1.0f)
            {
                warrior->resumePath();
            }
        }
        else
        {
            const int32_t speedState = getSpeedState();
            warrior = pilot;

            if (speedState == 2)
            {
                if (warrior->moveOrders.waitForPointTime <= -1.0f)
                {
                    warrior->moveOrders.waitForPointTime = scenarioTime + 5.0f;
                }
            }
            else if (warrior->moveOrders.waitForPointTime < scenarioTime)
            {
                warrior->pausePath();
                warrior->moveOrders.waitForPointTime = 999999.0f;
            }
        }
    }
    else
    {
        warrior->moveOrders.waitForPointTime = -1.0f;
    }

    int result = steerAlongPath(this, newRotate, newThrottleSetting, newRotatePerSec, newMoveState, maxThrottle);
    warrior = pilot;

    if (result != 0)
    {
        if (warrior->moveOrders.pathType == 2 &&
            warrior->moveOrders.path[0]->globalStep < warrior->moveOrders.numGlobalSteps - 1)
        {
            result = 0;
        }

        if (warrior->moveOrders.path[0] != nullptr)
        {
            warrior->moveOrders.path[0]->clear();
        }

        newThrottleSetting = 0;
    }

    return result;
}

auto GroundVehicle::setNextMovePath(char& newThrottleSetting) -> void
{
    MechWarrior* warrior = pilot;

    if (warrior->playerOrderFromQueue != 0 && warrior->curTacOrder.isMoveOrder() != 0)
    {
        if (warrior->moveOrders.path[0] != nullptr)
        {
            warrior->moveOrders.path[0]->clear();
        }

        return;
    }

    warrior->clearMoveOrders();
    newThrottleSetting = 0;
}

auto GroundVehicle::setControlSettings(char& newRotate, char& newThrottleSetting, float& newRotatePerSec,
                                       int32_t& minThrottle, int32_t& maxThrottle) -> void
{
    MechWarrior* warrior = pilot;
    MovePath* path = warrior->getMovePath();
    const int32_t run = MPlayer == nullptr || MPlayer->isServer != 0 ? warrior->moveOrders.run : moveChunk.run;

    if (path->numSteps == 0)
    {
        newThrottleSetting = 0;
    }

    auto* controlData = static_cast<GroundVehicleControlData*>(control->controlData);

    if (newThrottleSetting != -1)
    {
        if (newThrottleSetting < minThrottle)
        {
            newThrottleSetting = static_cast<char>(minThrottle);
        }
        else if (maxThrottle < newThrottleSetting)
        {
            newThrottleSetting = static_cast<char>(maxThrottle);
        }

        controlData->throttle = newThrottleSetting;
    }

    if (newRotate != 0)
    {
        controlData->rotate = newRotate;
    }

    // Anything but a run order moves at the walk speed.
    controlData->walk = run == 0 ? 1 : 0;
}

auto GroundVehicle::updateTurret(float newRotatePerSec) -> void
{
    MechWarrior* warrior = pilot;
    GameObject* target = warrior->getLastTarget();
    double facing;

    if (target != nullptr)
    {
        facing = static_cast<double>(relFacingTo(target->getPosition(), -1)) + turretRotation + newRotatePerSec;
    }
    else if (warrior->curTacOrder.code == TACTICAL_ORDER_ATTACK_POINT)
    {
        facing =
            static_cast<double>(relFacingTo(warrior->getAttackTargetPoint(), -1)) + turretRotation + newRotatePerSec;
    }
    else
    {
        facing = turretRotation;
    }

    if (facing < -180.0)
    {
        facing += 360.0;
    }
    else if (facing > 180.0f)
    {
        facing -= 360.0;
    }

    if (facing >= -2.0 && facing <= 2.0)
    {
        return;
    }

    double turn = -facing;
    auto* dynType = static_cast<GroundVehicleDynamicsType*>(static_cast<GroundVehicleType*>(objType)->dynamicsType);
    const float maxTurn = static_cast<float>(dynType->maxTurretYawRate) * frameLength;

    if (maxTurn < std::fabs(turn))
    {
        turn = turn < 0.0 ? -maxTurn : maxTurn;
    }

    static_cast<GroundVehicleControlData*>(control->controlData)->turretRotate =
        static_cast<int8_t>(static_cast<int32_t>(turn / maxTurn * 64.0f));
}

auto GroundVehicle::updateMovement() -> void
{
    auto* controlData = static_cast<GroundVehicleControlData*>(control->controlData);

    if (disableThisFrame != 0)
    {
        disableThisFrame = 0;
        shutDownThisFrame = 0;
        startUpThisFrame = 0;
        status = 1;
        controlData->throttle = 0;
        return;
    }

    if (shutDownThisFrame != 0)
    {
        controlData->throttle = 0;
        shutDownThisFrame = 0;
        startUpThisFrame = 0;
        status = 5;
        return;
    }

    if (startUpThisFrame != 0)
    {
        controlData->throttle = 100;
        startUpThisFrame = 0;
        status = 0;
        return;
    }

    if (isCaptured() != 0 || isDisabled() != 0)
    {
        controlData->throttle = 0;
        return;
    }

    if (unknown170 > -1.0f)
    {
        return;
    }

    float newRotatePerSec = static_cast<float>(pivotTo());

    if (newRotatePerSec != 0.0f)
    {
        return;
    }

    int32_t minThrottle = -100;
    int32_t maxThrottle = 100;
    char newRotate = 0;
    char newThrottleSetting = 0;
    int32_t newMoveState = -1;
    calcThrottleLimits(minThrottle, maxThrottle);
    updateMoveStateGoal();

    if (updateMovePath(newRotate, newThrottleSetting, newRotatePerSec, newMoveState, minThrottle, maxThrottle) != 0)
    {
        setNextMovePath(newThrottleSetting);
    }

    if (newMoveState != -1)
    {
        pilot->moveOrders.moveState = newMoveState;
    }

    setControlSettings(newRotate, newThrottleSetting, newRotatePerSec, minThrottle, maxThrottle);
    updateTurret(newRotatePerSec);
}

auto GroundVehicle::netUpdateMovePath(char& newRotate, char& newThrottleSetting, float& newRotatePerSec,
                                      int32_t& newMoveState, int32_t& minThrottle, int32_t& maxThrottle) -> int
{
    auto* controlData = static_cast<GroundVehicleControlData*>(control->controlData);
    pilot->getMovePath();
    newRotatePerSec = 0.0f;
    newThrottleSetting = static_cast<char>(controlData->throttle);
    return steerAlongPath(this, newRotate, newThrottleSetting, newRotatePerSec, newMoveState, maxThrottle);
}

auto GroundVehicle::netUpdateMovement() -> void
{
    auto* controlData = static_cast<GroundVehicleControlData*>(control->controlData);
    MovePath* path = pilot->getMovePath();
    const float distance = distanceFrom(path->stepList[path->curStep].destination);

    if (path->curStep == path->numSteps - 1 && distance < MoveMarginOfError[1])
    {
        // At the end of the path: stop once the server says the vehicle has.
        getBodyState();

        if (statusChunk.bodyState == 0)
        {
            pilot->clearMoveOrders();
            controlData->throttle = 0;
        }
    }

    if (disableThisFrame != 0)
    {
        disableThisFrame = 0;
        shutDownThisFrame = 0;
        startUpThisFrame = 0;
        status = 1;
        controlData->throttle = 0;
        return;
    }

    if (shutDownThisFrame != 0)
    {
        controlData->throttle = 0;
        shutDownThisFrame = 0;
        startUpThisFrame = 0;
        status = 5;
        return;
    }

    if (startUpThisFrame != 0)
    {
        controlData->throttle = 100;
        startUpThisFrame = 0;
        status = 0;
        return;
    }

    if (isCaptured() != 0 || isDisabled() != 0)
    {
        controlData->throttle = 0;
        return;
    }

    if (unknown170 > -1.0f)
    {
        return;
    }

    float newRotatePerSec = static_cast<float>(pivotTo());

    if (newRotatePerSec != 0.0f)
    {
        return;
    }

    int32_t minThrottle = -100;
    int32_t maxThrottle = 100;
    char newRotate = 0;
    char newThrottleSetting = 0;
    int32_t newMoveState = -1;
    calcThrottleLimits(minThrottle, maxThrottle);
    updateMoveStateGoal();
    netUpdateMovePath(newRotate, newThrottleSetting, newRotatePerSec, newMoveState, minThrottle, maxThrottle);

    if (newMoveState != -1)
    {
        pilot->moveOrders.moveState = newMoveState;
    }

    setControlSettings(newRotate, newThrottleSetting, newRotatePerSec, minThrottle, maxThrottle);
    updateTurret(newRotatePerSec);
}

auto GroundVehicle::getPositionFromHS(uint32_t hotSpot) -> vector_3d
{
    return position;
}

auto GroundVehicle::onScreen() -> int
{
    Camera* camera = cameraList->findCameraFromIDNumber(1);
    screenPos.y = 0.0f;
    screenPos.x = 0.0f;

    if (camera == nullptr || camera->active == 0)
    {
        return 0;
    }

    float screenY;

    if (useOldProject == 0)
    {
        vector_2d screen100;
        vector_2d screen50;

        if (land != nullptr)
        {
            land->projectTerrain(position, screen100, screen50);
        }

        if (camera->cameraScale == 1)
        {
            screenPos.x = (screen50.x - camera->screenUL50.x) + camera->halfWidth;
            screenY = screen50.y - camera->screenUL50.y;
        }
        else
        {
            screenPos.x = (screen100.x - camera->screenUL.x) + camera->halfWidth;
            screenY = screen100.y - camera->screenUL.y;
        }

        screenPos.y = screenY + camera->halfHeight;
    }
    else
    {
        const float scale = camera->cameraScale != 1 ? 1.0f : 0.5f;
        vector_3d relative(position.x - camera->position.x, position.y - camera->position.y,
                           position.z - camera->position.z);
        relative *= scale;
        screenPos.x = relative.y * camera->cosAngle + relative.x * camera->cosAngle + camera->halfWidth;
        screenY = ((relative.x * camera->sinAngle + camera->halfHeight) - relative.y * camera->sinAngle) - relative.z;
        screenPos.y = screenY;

        if (screenPos.x < 0.0f || screenY < 0.0f || camera->viewWidth < screenPos.x || camera->viewHeight < screenY)
        {
            windowsVisible = 0;
        }
        else
        {
            windowsVisible = 1;
        }
    }

    if (appearance != nullptr && appearance->recalcBounds(camera) != 0)
    {
        windowsVisible = turn;
        return 1;
    }

    return 0;
}

auto GroundVehicle::disable(uint32_t cause) -> void
{
    Mover::disable(cause);
    unknown794 = 0.0f;
    smoke = static_cast<Smoke*>(createObject(0x1c2));

    if (smoke != nullptr)
    {
        smoke->setOwner(this);
        smoke->setOwnerPosition(position);
    }
}

auto GroundVehicle::crashAvoidanceSystem() -> int
{
    if (MPlayer != nullptr && MPlayer->isServer == 0)
    {
        return 0;
    }

    MechWarrior* warrior = pilot;
    MovePath* path = warrior->getMovePath();

    if (path->numStepsWhenNotPaused == 0)
    {
        return 0;
    }

    if (warrior->moveOrders.waitForPointTime > 999990.0f)
    {
        return 0;
    }

    // A look a frame ahead along the frame turned by a quarter pi (its result is unused).
    const float speed = -dynamics->getVelocity();
    frame_of_ref ahead = frame;
    rotateAboutK(ahead, static_cast<float>(std::sin(HALF_PI / 2.0)), static_cast<float>(std::cos(HALF_PI / 2.0)));
    vector_3d lookAhead(ahead.j.x * speed * frameLength * worldUnitsPerMeter + position.x,
                        ahead.j.y * speed * frameLength * worldUnitsPerMeter + position.y,
                        ahead.j.z * speed * frameLength * worldUnitsPerMeter + position.z);
    int32_t tileR;
    int32_t tileC;
    int32_t cellR;
    int32_t cellC;
    GameMap->worldToMapPos(lookAhead, tileR, tileC, cellR, cellC);

    int cornerBlocked = 0;
    const int32_t direction = static_cast<int8_t>(path->stepList[path->curStep].direction);

    if (direction == 1 || direction == 3 || direction == 5 || direction == 7)
    {
        // A diagonal step: blocked when both cells beside it are locked.
        const int first = getAdjacentCellPathLocked(objPosition->tileR, objPosition->tileC, objPosition->cellR,
                                                    objPosition->cellC, adjClippedCell[direction][0]);
        const int second = getAdjacentCellPathLocked(objPosition->tileR, objPosition->tileC, objPosition->cellR,
                                                     objPosition->cellC, adjClippedCell[direction][1]);
        cornerBlocked = first != 0 && second != 0 ? 1 : 0;
    }

    int lockReachedEnd = 0;
    int blockReachedEnd = 0;
    const int locked = getPathRangeLock(crashAvoidPath, &lockReachedEnd);
    const int blocked = getPathRangeBlocked(crashAvoidPath, &blockReachedEnd);
    const int32_t closedGates = path->crossesClosedGate(-1, 2);
    warrior = pilot;
    const bool clear = locked == 0 && blocked == 0 && cornerBlocked == 0 && closedGates < 2;

    if (warrior->moveOrders.yieldTime > -1.0f)
    {
        // Yielding: go on once the way is clear.
        if (clear)
        {
            warrior->resumePath();
            warrior->moveOrders.yieldTime = -1.0f;
            return 0;
        }

        warrior->pausePath();
        return 1;
    }

    if (clear)
    {
        return 0;
    }

    if (lockReachedEnd == 0 && blockReachedEnd == 0)
    {
        warrior->pausePath();
        warrior->moveOrders.yieldTime = scenarioTime + crashYieldTime;
        control->controlData->brake();
        return 1;
    }

    warrior->reachedPathEnd();
    control->controlData->brake();
    return 1;
}

auto GroundVehicle::createVehiclePilot() -> void
{
    auto* marine = static_cast<Mover*>(createObject(DefaultPilotId));
    vehiclePilot = marine;

    if (marine == nullptr)
    {
        Fatal(-1, " Couldnt create Marine for vehicle ");
    }

    marine->setAwake(1);
    FullPathFileName profileName;
    profileName.init(profilePath, marineProfileName, ".fit");
    FitIniFile profileFile;
    const int32_t result = profileFile.open(profileName, READ, 50);

    if (result != 0)
    {
        Fatal(result, " Unable to open Vehicle Marine Profile ");
    }

    if (marine->init(&profileFile) != 0)
    {
        Fatal(-1, " Bad Vehicle Marine Profile File ");
    }

    profileFile.close();

    // The vehicle's pilot bails out as the marine.
    MechWarrior* warrior = pilot;
    marine->setPilot(warrior);
    warrior->setVehicle(marine);
    warrior->lobotomy();
    marine->setControl(2, 3, -1);
    marine->setTeam(getTeam());
    vehiclePilot->setPosition(position);
    vehiclePilot->setLastValidPosition(position);
    vehiclePilot->setFrame(frame);
    auto* marineAppearance = static_cast<ElementalActor*>(vehiclePilot->getAppearance());

    if (marineAppearance != nullptr)
    {
        marineAppearance->setGesture(0);
        marineAppearance->fadeTableIndex = getAlignment() == -1 ? 0x1d : 0x20;
    }

    Mover* newMarine = vehiclePilot;
    newMarine->idNumber = idNumber + 1000;
    newMarine->setPartId(0xfff - NumMarines++);
    newMarine->setAlignment(getAlignment());
    ObjectQueueNode* list = getAlignment() == -1 ? clanMechList : innerSphereMechList;

    if (list != nullptr)
    {
        list->addNode(marine);
    }

    marine->setExists(1);
    marine->setPotentialContact(0);
    GameObjectMap->addObject(marine);
    warrior = pilot;
    warrior->clearAttackOrders();
    warrior->clearMoveOrders();
    warrior->orderMoveToPoint(0, 1, 0, vector_3d(0.0f, 0.0f, 0.0f), -1, 1);
}

namespace
{
    /// <summary>Moves the smoke along with the vehicle and runs it; frees it once the smoke time is 30 s past.</summary>
    void updateSmoke(GroundVehicle* vehicle)
    {
        Smoke* smoke = vehicle->smoke;
        smoke->setOwner(vehicle);
        smoke->setOwnerPosition(vehicle->position);
        smoke->setOwnerVelocity(vehicle->velocity);
        smoke->update();
        vehicle->unknown794 -= frameLength;

        if (vehicle->unknown794 > -30.0)
        {
            return;
        }

        delete smoke;
        vehicle->smoke = nullptr;
    }
}

auto GroundVehicle::update() -> int32_t
{
    if (unknown79C != 0 && pilot->status == 2)
    {
        collisionsOn = 0;
        return 1;
    }

    terrainNormal = land->getTerrainNormal(position);
    updatePathLock(0);

    if (potentialContact != nullptr)
    {
        if (team->id == 1)
        {
            potentialContact->updateStatus(innerSphereTeam);
            potentialContact->updateStatus(alliedTeam);
        }
        else if (team->id == 0)
        {
            potentialContact->updateStatus(clanTeam);
            potentialContact->updateStatus(alliedTeam);
        }
        else
        {
            potentialContact->updateStatus(innerSphereTeam);
            potentialContact->updateStatus(clanTeam);
        }
    }

    if (deselectTime != 0.0f && deselectTime < scenarioTime)
    {
        deselectTime = 0.0f;
        selected = 0;
    }

    if (isDestroyed() != 0 && unknown794 < 0.0)
    {
        // The wreck: only its appearance and smoke go on.
        if (appearance != nullptr)
        {
            appearance->visible = onScreen();
            appearance->update();
        }

        if (smoke == nullptr)
        {
            return 1;
        }

        updateSmoke(this);
        return 1;
    }

    if (isDestroyed() == 0 || unknown794 < 0.0)
    {
        if (getAwake() == 0 || isDisabled() != 0 || unknown7C8 < Terrain::metersPerVertex)
        {
            if (isDisabled() != 0 && unknown794 != 0.0f && smoke != nullptr)
            {
                updateSmoke(this);
            }
        }
        else
        {
            // Every vertex travelled, the vehicle marks what it sees.
            if (alignment == 1)
            {
                land->markSeen(position, frame.j, 360.0f, getProbeEffect() + scenario->maxVisualRange, 1);
            }
            else if (alignment == -1)
            {
                land->markSeen(position, frame.j, 360.0f, getProbeEffect() + scenario->maxVisualRange, 2);
            }

            unknown7C8 = 0.0f;
        }
    }
    else
    {
        // Just destroyed: when the death timer runs out, it blows up and its crew bails out.
        unknown794 -= frameLength;

        if (unknown794 < 0.0)
        {
            if (gvAppearance == 0)
            {
                static_cast<PUAppearance*>(appearance)->setDestroyed();
            }
            else
            {
                static_cast<GVAppearance*>(appearance)->setTypeId(GV_ACTOR_STATE_DESTROYED);
            }

            if (appearance != nullptr)
            {
                appearance->visible = onScreen();
                appearance->update();
            }

            auto* vehicleType = static_cast<GroundVehicleType*>(objType);
            vehicleType->createExplosion(position, vehicleType->explDmg, vehicleType->explRad);
            unknown798 = 1;
            smoke = static_cast<Smoke*>(createObject(0x1c2));
            collisionsOn = 0;

            if (MPlayer != nullptr)
            {
                return 1;
            }

            if (getAwake() == 0)
            {
                return 1;
            }

            createVehiclePilot();
            return 1;
        }
    }

    int32_t result = control->update();

    if (result != 1)
    {
        return result;
    }

    result = dynamics->update();

    if (result != 1)
    {
        return result;
    }

    int avoiding = 0;

    if (isDisabled() == 0)
    {
        avoiding = crashAvoidanceSystem();
    }

    float speed = 0.0f;

    if (avoiding == 0)
    {
        speed = dynamics->getVelocity();
    }

    if (gvAppearance != 0)
    {
        auto* vehicleAppearance = static_cast<GVAppearance*>(appearance);

        if (speed != 0.0)
        {
            vehicleAppearance->setTypeId(GV_ACTOR_STATE_DAMAGED);
        }
        else if (unknown8CC == 0)
        {
            vehicleAppearance->setTypeId(GV_ACTOR_STATE_NORMAL);
        }
        else
        {
            vehicleAppearance->setTypeId(GV_ACTOR_STATE_EXTRA);
        }
    }

    int visibleNow = 0;

    if (appearance != nullptr)
    {
        appearance->update();
        visibleNow = onScreen();
        appearance->visible = visibleNow;

        if (gvAppearance == 0)
        {
            // A pop-up turret opens for a target; its weapons work once it is up.
            const int combat =
                pilot->getLastTarget() != nullptr || pilot->curTacOrder.code == TACTICAL_ORDER_ATTACK_POINT ? 1 : 0;
            weaponsDeployed = static_cast<PUAppearance*>(appearance)->setCombatMode(combat) == 2 ? 1 : 0;
        }
    }

    if (unknown79C != 0 && visibleNow == 0 && pilot->status != 2)
    {
        objType->handleDestruction(this, nullptr);
    }

    // Slopes slow the vehicle: by the hill factor times the cosine of the angle between its heading and the
    // terrain's normal.
    frame_of_ref turned = frame;
    speed = -speed;
    vector_3d normal = land->getTerrainNormal(position);
    vector_3d heading = frame.j;
    const float headingLength = heading.magnitude();

    if (headingLength != 0.0)
    {
        heading.x /= headingLength;
        heading.y /= headingLength;
        heading.z /= headingLength;
    }

    normal.normalize();
    const double slope = std::acos(heading | normal) * 0x1.ca5dc1a6402aap+5;

    if (slope != 90.0)
    {
        speed = static_cast<float>(speed - std::cos(slope * DEGREES_TO_RADIANS) * gvHillSpeedFactor * speed);
    }

    rotateAboutK(turned, static_cast<float>(std::sin(HALF_PI / 2.0)), static_cast<float>(std::cos(HALF_PI / 2.0)));
    velocity.y = turned.j.y * speed;
    velocity.x = turned.j.x * speed;
    velocity.z = turned.j.z * speed;
    vector_3d move;
    move.x = velocity.x * frameLength * worldUnitsPerMeter;
    move.y = velocity.y * frameLength * worldUnitsPerMeter;
    move.z = velocity.z * frameLength * worldUnitsPerMeter;

    if (unknown20C != 0)
    {
        // A new move chunk: warp to its first step when too far off.
        if (statusChunk.jumpOrder == 0)
        {
            const int32_t tileR = moveChunk.stepPos[0][0];
            const int32_t tileC = moveChunk.stepPos[0][1];
            vector_3d stepPos;
            mapTileCellToWorldPos(tileR, tileC, moveChunk.stepPos[0][2], moveChunk.stepPos[0][3], stepPos);
            // Original behaviour (OB-006): z is measured against 0, not the vehicle's elevation.
            const float dx = position.x - stepPos.x;
            const float dz = -stepPos.z;
            const float dy = position.y - stepPos.y;

            if (WarpFactor < std::sqrt(dz * dz + dy * dy + dx * dx))
            {
                move.x = stepPos.x - position.x;
                move.y = stepPos.y - position.y;
                move.z = stepPos.z;
            }

            if (tileR < 0 || GameMap->height <= tileR || tileC < 0 || GameMap->width <= tileC)
            {
                Fatal(0, " gvehicl.update: newMoveChunk stepPos not on map! ", nullptr);
            }
        }

        unknown20C = 0;
    }

    unknown7C8 = std::sqrt(move.z * move.z + move.y * move.y + move.x * move.x) + unknown7C8;
    vector_3d newPosition;
    newPosition.x = move.x + position.x;
    newPosition.y = move.y + position.y;
    newPosition.z = move.z + position.z;
    setPosition(newPosition);

    if (isDisabled() == 0)
    {
        updatePathLock(1);
    }

    sweepTime = frameLength + sweepTime;
    mineCheck();

    // A mine layer lays one per tile: at the cell in the middle, or once it has waited MineWaitTime.
    if ((MPlayer == nullptr || MPlayer->isServer != 0) && mineLayer != 0 && pilot->curTacOrder.moveParams.mode == 1 &&
        (getObjPosition()->tileC != cellColToMine || getObjPosition()->tileR != cellRowToMine))
    {
        mineLayTime = frameLength + mineLayTime;

        if ((getObjPosition()->cellC == 1 && getObjPosition()->cellR == 1) || MineWaitTime < mineLayTime)
        {
            cellColToMine = getObjPosition()->tileC;
            const int32_t tileR = getObjPosition()->tileR;
            const int32_t tileC = cellColToMine;
            cellRowToMine = tileR;
            mineLayTime = 0.0f;
            MapTile& tile = GameMap->map[GameMap->width * tileR + tileC];

            if (alignment == -1)
            {
                tile.overlay = (tile.overlay & 0xffffdfff) | 0x4000;
            }
            else
            {
                tile.overlay = (tile.overlay & 0xfffff7ff) | 0x1000;
            }

            if (MPlayer != nullptr)
            {
                MPlayer->addMineChunk(tileR * 3, tileC * 3, alignment == -1 ? 1 : 0, 2, 0);
            }
        }
    }

    position.z = land->getTerrainElevation(position);
    // Original behaviour (OB-005): adds the map's top edge to y here rather than subtracting.
    const float blockSize = static_cast<float>(Terrain::verticesBlockSide) * Terrain::metersPerVertex;
    const float blockColumn = (position.x - Terrain::mapTopLeft3d100.x) / blockSize;
    const auto blockRow =
        static_cast<int32_t>(std::floor(static_cast<double>((Terrain::mapTopLeft3d100.y + position.y) / blockSize)));
    const auto column = static_cast<int32_t>(std::floor(static_cast<double>(blockColumn)));
    addMoverToList(column + blockRow * Terrain::blocksMapSide);
    return 1;
}

namespace
{
    /// <summary>A world point on <see cref="eye"/>'s screen (the camera's inline projection).</summary>
    vector_2d eyeProject(const vector_3d& point)
    {
        const float scale = eye->cameraScale != 1 ? 1.0f : 0.5f;
        const float dy = point.y - eye->position.y;
        const float dz = point.z - eye->position.z;
        const float sx = (point.x - eye->position.x) * scale;
        const float sy = dy * scale;
        vector_2d screen;
        screen.x = sx * eye->cosAngle + sy * eye->cosAngle + eye->halfWidth;
        screen.y = ((sx * eye->sinAngle + eye->halfHeight) - sy * eye->sinAngle) - scale * dz;
        return screen;
    }
}

auto GroundVehicle::render() -> void
{
    if (gamePaused != 0)
    {
        onScreen();
    }

    if (unknown79C != 0 && pilot->status == 2)
    {
        return;
    }

    int tagged = 0;

    if (alignment == homeTeam->alignment)
    {
        if (windowsVisible == turn)
        {
            if (getAwake() == 0)
            {
                if (isRevealed() != 0)
                {
                    appearance->render(0);
                }
            }
            else
            {
                appearance->render(0);
            }

            if (smoke != nullptr)
            {
                smoke->render();
            }
        }
    }
    else
    {
        const int32_t contactType = getContactType(homeTeam->id, tagged);

        if (contactType == 1)
        {
            if (windowsVisible == turn)
            {
                // A wreck draws behind the living.
                appearance->render(isDestroyed() == 0 && isDisabled() == 0 ? 0 : 150);

                if (smoke != nullptr)
                {
                    smoke->render();
                }
            }
        }
        else if (contactType == 2)
        {
            // A sensor contact: a blip sized by tonnage, at the zoom's scale.
            const int zoomedOut = eye->cameraScale == 1;
            int32_t shapeIndex;
            const char* shapeName;

            if (50.0f < getTonnage())
            {
                shapeIndex = zoomedOut ? 1 : 0;
                shapeName = zoomedOut ? "vblip1" : "vblip2";
            }
            else if (35.0f < getTonnage())
            {
                shapeIndex = zoomedOut ? 3 : 2;
                shapeName = zoomedOut ? "vblip3" : "vblip4";
            }
            else
            {
                shapeIndex = zoomedOut ? 5 : 4;
                shapeName = zoomedOut ? "vblip5" : "vblip6";
            }

            uint8_t* shape = scenario->sensorContactShapes[shapeIndex];

            if (shape != nullptr)
            {
                if (VFX_shape_count(shape) <= blipFrame)
                {
                    if (soundSystem != nullptr && useSound != 0)
                    {
                        soundSystem->playDigitalSample(0x14, 1, this, 0, 1);
                    }

                    blipFrame = 0;
                }

                ElementList->openGroup(-100000, 1);
                auto* element = new VFXElement(shape, screenPos.x, screenPos.y, blipFrame, 0, nullptr, 0, 1);
                std::strcpy(element->name, shapeName);
                ElementList->add(element);
                blipTime = frameLength + blipTime;

                if (0.067 < blipTime)
                {
                    blipFrame = static_cast<int32_t>(blipTime * (1.0 / 0.067) + blipFrame + 0.5);
                    blipTime = 0.0f;
                }
            }
        }
    }

    if (drawExtents != 0)
    {
        // Debug: the extent radius as an ellipse.
        float radius = getExtentRadius();

        if (eye->cameraScale == 1)
        {
            radius *= 0.5f;
        }

        vector_2d center = eyeProject(position);
        vector_2d size(radius, radius);
        ElementList->openGroup(-50000, 1);
        ElementList->add(new EllipseElement(center, size, 0xfe, -50000));
    }

    if (drawTerrainGrid != 0)
    {
        // Debug: the move path's steps as lines.
        MovePath* path = pilot->getMovePath();
        const int32_t numSteps = path->numSteps;

        for (int32_t i = 0; i < numSteps; i++)
        {
            if (i == numSteps - 1)
            {
                continue;
            }

            vector_3d from = path->stepList[i].destination;
            vector_3d to = path->stepList[i + 1].destination;
            from.z = land->getTerrainElevation(from);
            to.z = land->getTerrainElevation(to);
            vector_2d fromScreen = eyeProject(from);
            vector_2d toScreen = eyeProject(to);
            ElementList->openGroup(-100000, 1);
            ElementList->add(new LineElement(fromScreen, toScreen, 0xfd, nullptr, -100000, -1));
        }
    }

    // The selected vehicle's queued orders: waypoint markers, joined by lines when the queue is drawn as a path.
    if (waypointMarkers != nullptr && selected != 0 && pilot != nullptr && pilot->getTacOrderQueue(nullptr) > 0)
    {
        TacticalOrder tacOrder;
        tacOrder.init();
        _QueuedTacOrder queue[MAX_QUEUED_TACORDERS_PER_WARRIOR];
        const int32_t numOrders = pilot->getTacOrderQueue(queue);
        vector_2d fromScreen = eyeProject(position);
        const int32_t drawLines = unknown89C;

        for (int32_t i = 0; i < numOrders; i++)
        {
            vector_2d toScreen = eyeProject(queue[i].point);
            tacOrder.data[0] = queue[i].packedData[0];
            tacOrder.data[1] = queue[i].packedData[1];
            tacOrder.unpack();
            int32_t marker;

            if (tacOrder.code == TACTICAL_ORDER_JUMPTO_POINT || tacOrder.code == TACTICAL_ORDER_JUMPTO_OBJECT)
            {
                marker = 4;
            }
            else
            {
                marker = tacOrder.moveParams.wayPath.mode[0] << 1;
            }

            if (drawLines != 0)
            {
                ElementList->openGroup(-99999, 1);
                ElementList->add(new LineElement(fromScreen, toScreen, 0xeb, nullptr, -100000, -1));
                fromScreen = toScreen;
                marker++;
            }

            const int32_t bounds = VFX_shape_bounds(waypointMarkers, marker);
            ElementList->openGroup(-100000, 1);
            auto* element =
                new VFXElement(waypointMarkers, static_cast<float>((bounds >> 16) / 2) + toScreen.x,
                               toScreen.y - static_cast<float>(bounds >> 1 & 0x7fff), marker, 1, nullptr, 1, 0);
            std::strcpy(element->name, "gwp");
            ElementList->add(element);
        }

        tacOrder.destroy();
    }
}

auto GroundVehicle::relFacingTo(vector_3d goal, int32_t bodyPart) -> float
{
    float facing = Mover::relFacingTo(goal, -1);

    if (bodyPart == GROUNDVEHICLE_LOCATION_TURRET)
    {
        facing += turretRotation;
    }

    if (facing < -180.0)
    {
        return static_cast<float>(facing + 360.0);
    }

    if (180.0f < facing)
    {
        facing = static_cast<float>(facing - 360.0);
    }

    return facing;
}

auto GroundVehicle::calcAttackChance(GameObject* target, int32_t aimLocation, float targetTime, int32_t weaponIndex,
                                     float modifiers, int32_t* range, vector_3d* targetPoint) -> float
{
    if (weaponIndex < numOther || numOther + numWeapons <= weaponIndex)
    {
        return -1000.0f;
    }

    return Mover::calcAttackChance(target, aimLocation, targetTime, weaponIndex, modifiers, range, targetPoint);
}

auto GroundVehicle::calcHitLocation(GameObject* attacker, int32_t weaponIndex, int32_t attackSource, int32_t attackType)
    -> int32_t
{
    if (attackSource == 2)
    {
        return GROUNDVEHICLE_LOCATION_FRONT;
    }

    // Nearly a third of hits land on the turret; the rest on the side facing the attacker.
    const int32_t roll = RandomNumber(100);

    if ((RandomNumber(100) + roll) / 2 > 69)
    {
        return GROUNDVEHICLE_LOCATION_TURRET;
    }

    float facing = 0.0f;

    if (attacker != nullptr)
    {
        facing = relFacingTo(attacker->getPosition(), -1);
    }

    if (-45.0 <= facing && facing <= 45.0)
    {
        return GROUNDVEHICLE_LOCATION_FRONT;
    }

    if (-135.0 < facing && facing < -45.0)
    {
        return GROUNDVEHICLE_LOCATION_LEFT;
    }

    if (45.0 < facing && facing < 135.0f)
    {
        return GROUNDVEHICLE_LOCATION_RIGHT;
    }

    return GROUNDVEHICLE_LOCATION_REAR;
}

auto GroundVehicle::hitInventoryItem(int32_t itemIndex, int setupOnly) -> int
{
    Fatal(0, " Vehicles should never suffer inventory item hit ");
    return 0;
}

auto GroundVehicle::destroyBodyLocation(int32_t location) -> void
{
}

auto GroundVehicle::calcCriticalHitV(int32_t& hitLocation) -> int
{
    if (MPlayer != nullptr && MPlayer->isServer == 0)
    {
        return 0;
    }

    int32_t roll = RandomNumber(100);
    hitLocation = 0;

    do
    {
        if (roll < GroundVehicleCriticalHitTable[hitLocation])
        {
            break;
        }

        roll -= GroundVehicleCriticalHitTable[hitLocation];
        hitLocation++;
    } while (hitLocation < 11);

    if (MPlayer != nullptr)
    {
        addCriticalHitChunk(0, 0, hitLocation);
    }

    switch (hitLocation)
    {
        case 1:
        case 2:
            return 1;
        case 3:
        {
            // The crew is hurt.
            pilot->injure(6.0f, 1);
            return 0;
        }
        case 4:
        {
            // The engine is knocked out.
            inventory[engine].health = 0;
            inventory[engine].disabled = 1;
            movementEnabled = 0;
            return 0;
        }
        case 5:
        {
            // The first weapon jams for ten seconds.
            if (inventory[numOther].readyTime < scenarioTime)
            {
                startWeaponRecycle(numOther);
            }

            inventory[numOther].readyTime += 10.0f;
            return 0;
        }
        case 7:
        {
            movementEnabled = 0;
            return 0;
        }
        case 9:
        {
            // Only chassis 2 takes this one; for the others it is no hit.
            if (chassis != 2)
            {
                hitLocation = 0;
                return 0;
            }

            [[fallthrough]];
        }
        case 8:
        {
            // Drive damage: 10 off the top speed, immobile at 0.
            maxRunSpeed -= 10.0f;

            if (maxRunSpeed <= 0.0f)
            {
                maxRunSpeed = 0.0f;
                movementEnabled = 0;
            }

            return 0;
        }
        case 10:
        {
            turretEnabled = 0;
            return 0;
        }
        default:
            return 0;
    }
}

auto GroundVehicle::injureBodyLocation(int32_t bodyLocation, float damage) -> int
{
    BodyLocation& location = bodyAt(bodyLocation);

    if (location.curInternalStructure <= damage)
    {
        location.curInternalStructure = 0.0f;
        return 1;
    }

    location.curInternalStructure -= damage;
    const float structureLeft = location.curInternalStructure / static_cast<float>(location.maxInternalStructure);

    if (structureLeft == 0.0)
    {
        location.damageState = 2;
    }
    else if (structureLeft <= 0.5)
    {
        location.damageState = 1;
    }
    else
    {
        location.damageState = 0;
    }

    return 0;
}

auto GroundVehicle::buildStatusChunk() -> int32_t
{
    statusChunk.targetCellRC[0] = -1;
    statusChunk.targetCellRC[1] = -1;
    statusChunk.bodyState = 0;
    statusChunk.targetType = 0;
    statusChunk.targetId = 0;
    statusChunk.targetBlockOrTrainNumber = 0;
    statusChunk.targetVertexOrCarNumber = 0;
    statusChunk.targetItemNumber = 0;
    statusChunk.ejectOrderGiven = 0;
    statusChunk.jumpOrder = 0;
    statusChunk.data = 0;

    const int32_t bodyState = getBodyState();
    statusChunk.bodyState = bodyState < 0 || bodyState > 3 ? 0 : static_cast<uint32_t>(bodyState);

    if (pilot != nullptr)
    {
        GameObject* target = pilot->getLastTarget();

        if (target != nullptr)
        {
            const int32_t targetClass = target->objectClass;

            switch (targetClass)
            {
                case 1:
                case BUILDING:
                case DEBRIS:
                case TREE:
                case TERRAINOBJECT:
                case 0x17:
                case MISCTERRAINOBJECT:
                case JET:
                case TREEBUILDING:
                case TURRET:
                case GATE:
                case LIGHT:
                {
                    // A terrain object: its block, vertex and item from the part id.
                    statusChunk.targetType = 2;
                    statusChunk.targetId = target->partId;
                    const int32_t terrainPart = target->partId - 0x1000;
                    statusChunk.targetBlockOrTrainNumber = terrainPart / 0xc80;
                    const int32_t inBlock = terrainPart % 0xc80;
                    statusChunk.targetVertexOrCarNumber = inBlock / 8;
                    statusChunk.targetItemNumber = static_cast<uint8_t>(inBlock % 8);
                    break;
                }

                case BATTLEMECH:
                case GROUNDVEHICLE:
                case ELEMENTAL:
                {
                    statusChunk.targetType = 1;
                    statusChunk.targetId = static_cast<Mover*>(target)->netRosterIndex;
                    break;
                }
                case CAMERADRONE:
                {
                    statusChunk.targetType = 3;
                    statusChunk.targetId = target->partId;
                    statusChunk.targetBlockOrTrainNumber = 0x80;
                    statusChunk.targetVertexOrCarNumber = target->partId - 0x802c8;
                    break;
                }
                case TRAINCAR:
                {
                    statusChunk.targetType = 3;
                    statusChunk.targetId = target->partId;
                    const int32_t trainPart = target->partId - 0x7d000;
                    statusChunk.targetBlockOrTrainNumber = trainPart / 100;
                    statusChunk.targetVertexOrCarNumber = trainPart % 100;
                    break;
                }

                default:
                    Fatal(targetClass, " GroundVehicle.buildStatusChunk: bad target object class ", nullptr);
            }
        }
    }

    statusChunk.ejectOrderGiven = unknown790;
    statusChunk.pack(this);

    // Checks the chunk unpacks to what was packed.
    StatusChunk check;
    check.data = statusChunk.data;
    check.StatusChunk::unpack(this);

    if (statusChunk.equalTo(&check) == 0)
    {
        Fatal(0, " BAD Statuschunk: save stchunk.dbg file! ", nullptr);
    }

    return 0;
}

auto GroundVehicle::handleStatusChunk(int32_t updateAge, uint32_t chunk) -> int32_t
{
    statusChunk.targetCellRC[0] = -1;
    statusChunk.targetCellRC[1] = -1;
    statusChunk.data = 0;
    statusChunk.bodyState = 0;
    statusChunk.targetType = 0;
    statusChunk.targetId = 0;
    statusChunk.targetBlockOrTrainNumber = 0;
    statusChunk.targetVertexOrCarNumber = 0;
    statusChunk.targetItemNumber = 0;
    statusChunk.ejectOrderGiven = 0;
    statusChunk.jumpOrder = 0;
    statusChunk.data = chunk;
    statusChunk.unpack(this);

    if (StatusChunkUnpackErr != 0)
    {
        return 0;
    }

    int32_t targetPartId = 0;

    if (statusChunk.jumpOrder == 0 && static_cast<int8_t>(statusChunk.targetType) > 0)
    {
        if (statusChunk.targetType == 1)
        {
            targetPartId = MPlayer->moverRoster[statusChunk.targetId]->partId;
        }
        else if (statusChunk.targetType < 4)
        {
            targetPartId = statusChunk.targetId;
        }
    }

    if (pilot == nullptr)
    {
        return 0;
    }

    GameObject* target = nullptr;
    int keepTarget = 0;

    if (targetPartId != 0)
    {
        GameObject* lastTarget = pilot->getLastTarget();

        if (lastTarget != nullptr && lastTarget->partId == targetPartId)
        {
            keepTarget = 1;
        }
        else
        {
            target = static_cast<GameObject*>(objectList->findObjectFromPart(targetPartId));
        }
    }

    if (keepTarget == 0)
    {
        pilot->setLastTarget(target, 0, 0);
    }

    if (unknown790 == 0 && statusChunk.ejectOrderGiven != 0)
    {
        unknown790 = 1;
        handleEjection();
    }

    return 0;
}

auto GroundVehicle::buildMoveChunk() -> int32_t
{
    moveChunk.init();

    if (pilot != nullptr)
    {
        pilot->getMovePath();
        moveChunk.build(this, pilot->moveOrders.path[0], pilot->moveOrders.path[1]);
    }

    moveChunk.pack(this);

    // Checks the chunk unpacks to what was packed; a chunk that can't is replaced by an empty one.
    MoveChunk check;
    check.stepPos[0][0] = -1;
    check.stepPos[0][1] = -1;
    check.run = 0;
    check.numSteps = 0;
    check.data = moveChunk.data;
    check.unpack(this);

    if (MoveChunkUnpackErr == 0)
    {
        if (moveChunk.equalTo(this, &check) == 0)
        {
            Fatal(0, " Bad gvehicl movechunk: save mvchunk.dbg file! ", nullptr);
        }
    }
    else
    {
        moveChunk.init();
        moveChunk.build(this, nullptr, nullptr);
        moveChunk.pack(this);
    }

    return 0;
}

auto GroundVehicle::handleMoveChunk(uint32_t chunk) -> int32_t
{
    moveChunk.init();
    moveChunk.data = chunk;
    moveChunk.unpack(this);

    if (MoveChunkUnpackErr == 0)
    {
        MovePath* path = getPilot()->getMovePath();
        path->setMoveChunk(&moveChunk);

        // Skip ahead to the step nearest the vehicle.
        if (path->numStepsWhenNotPaused > 1)
        {
            int32_t step = path->numStepsWhenNotPaused;

            do
            {
                step--;

                if (step < 1)
                {
                    break;
                }
            } while (MapCellDiagonal < distanceFrom(path->stepList[step].destination));

            path->curStep = step;
        }

        unknown20C = 1;
    }

    return 0;
}

auto GroundVehicle::weaponLocked(int32_t weaponIndex, vector_3d targetPosition) -> float
{
    return relFacingTo(targetPosition, GROUNDVEHICLE_LOCATION_TURRET);
}

namespace
{
    /// <summary>Whether <paramref name="object"/> is a mover (mech, vehicle, elemental or plain mover).</summary>
    bool isMoverClass(const GameObject* object)
    {
        const int32_t objectClass = object->objectClass;
        return objectClass == BATTLEMECH || objectClass == GROUNDVEHICLE || objectClass == ELEMENTAL ||
               objectClass == MOVER;
    }
}

auto GroundVehicle::handleWeaponHit(_WeaponShotInfo* shotInfo, int addMultiplayChunk) -> int32_t
{
    if ((MPlayer == nullptr && CantHitMe != 0 && pilot->onHomeTeam() != 0) || shotInfo == nullptr)
    {
        return 0;
    }

    if (addMultiplayChunk != 0)
    {
        MPlayer->addWeaponHitChunk(this, shotInfo, 0);
    }

    if (shotInfo->damage <= 0.0f)
    {
        return 0;
    }

    if (isDestroyed() != 0)
    {
        return 0;
    }

    const int32_t hitLocation = shotInfo->hitLocation;
    const _WeaponShotInfo originalShot = *shotInfo;

    if (hitLocation < 0 || hitLocation > 4)
    {
        char attackerName[64];
        GameObject* attacker = shotInfo->attacker;

        if (attacker == nullptr)
        {
            std::strcpy(attackerName, "attacker?");
        }
        else if (isMoverClass(attacker))
        {
            std::strcpy(attackerName, static_cast<Mover*>(attacker)->debugStatus);
        }
        else
        {
            std::sprintf(attackerName, "ID:%d", attacker->partId);
        }

        char message[128];
        std::sprintf(message, "GVehicle.handleWeaponHit: [%s]%d for %.2f at %d", attackerName, shotInfo->masterId,
                     static_cast<double>(shotInfo->damage), hitLocation);
        Fatal(0, message);
    }

    ArmorLocation& hitArmor = armor[hitLocation];

    if (hitArmor.curArmor > 0.0f)
    {
        if (shotInfo->damage <= hitArmor.curArmor)
        {
            hitArmor.curArmor -= shotInfo->damage;
            shotInfo->setDamage(0.0f);
        }
        else
        {
            shotInfo->setDamage(shotInfo->damage - hitArmor.curArmor);
            armor[shotInfo->hitLocation].curArmor = 0.0f;
        }

        // A sweeper sweeps with its front: a hit there ends that.
        if (shotInfo->hitLocation == GROUNDVEHICLE_LOCATION_FRONT)
        {
            mineSweeper = 0;
        }
    }

    const int wasDisabled = isDisabled();

    if (shotInfo->damage > 0.0f && injureBodyLocation(hitLocation, shotInfo->damage) != 0)
    {
        pilot->handleOwnVehicleIncapacitation(0);
        objType->handleDestruction(this, nullptr);
    }

    curCV = calcCV(0);

    GameObject* attacker = shotInfo->attacker;

    if (wasDisabled == 0 && isDisabled() != 0)
    {
        // The attacker's pilot hears of the kill.
        if (attacker != nullptr && isMoverClass(attacker))
        {
            attacker->getPilot()->triggerAlarm(12, partId);
        }
    }
    else if (attacker != nullptr)
    {
        pilot->triggerAlarm(shotInfo->masterId > -1 || shotInfo->masterId == -4 ? 1 : 10, attacker->partId);
    }
    else
    {
        pilot->triggerAlarm(
            1, shotInfo->masterId == -4 || shotInfo->masterId >= 0 ? 0 : static_cast<uint32_t>(shotInfo->masterId));
    }

    shotInfo->init(originalShot.attacker, originalShot.masterId, originalShot.damage, originalShot.hitLocation,
                   originalShot.entryAngle);
    return 0;
}

namespace
{
    /// <summary>Ammo count that marks a weapon as never running out.</summary>
    constexpr int32_t UNLIMITED_SHOTS = 9999;

    /// <summary>
    /// Builds, packs and checks the chunk for a shot at <paramref name="target"/> (a mover, train car, camera drone
    /// or terrain object) or, when it is null, at <paramref name="point"/>; then queues and logs it.
    /// </summary>
    void sendTargetFireChunk(GroundVehicle* vehicle, GameObject* target, vector_3d* point, int32_t weapon, int hit,
                             float entryAngle, int32_t missiles, int32_t missilesPastAMS, int32_t antiMissileShots,
                             int32_t hitLocation, const char* badChunkMessage)
    {
        WeaponFireChunk chunk;
        chunk.init();
        auto* bigTarget = static_cast<BigGameObject*>(target);

        if (target == nullptr)
        {
            chunk.buildLocationTarget(*point, weapon, hit, missiles);
        }
        else if (isMoverClass(target))
        {
            chunk.buildMoverTarget(bigTarget, weapon, hit, entryAngle, missiles, missilesPastAMS, antiMissileShots,
                                   hitLocation);
        }
        else if (target->objectClass == TRAINCAR)
        {
            chunk.buildTrainTarget(bigTarget, weapon, hit, entryAngle, missiles);
        }
        else if (target->objectClass == CAMERADRONE)
        {
            chunk.buildCameraDroneTarget(bigTarget, weapon, hit, entryAngle, missiles);
        }
        else
        {
            chunk.buildTerrainTarget(bigTarget, weapon, hit, missiles);
        }

        chunk.pack();
        WeaponFireChunk check;
        check.init();
        check.data = chunk.data;
        check.unpack(vehicle);

        if (chunk.equalTo(&check) == 0)
        {
            Fatal(0, badChunkMessage, nullptr);
        }

        vehicle->addWeaponFireChunk(0, &chunk);
        LogWeaponFireChunk(&chunk, vehicle, target);
    }

    /// <summary>
    /// Sends a weapon effect on its way from the vehicle, at <paramref name="target"/> or, when it is null, at
    /// <paramref name="point"/>, carrying <paramref name="shot"/>; then adds it to the weapon list. Vehicles fire from
    /// hot spot 0.
    /// </summary>
    void launchWeaponFX(GroundVehicle* vehicle, GameObject* fx, GameObject* target, vector_3d* point,
                        _WeaponShotInfo& shot, int32_t targetHotSpot)
    {
        if (fx->objectClass == BULLET)
        {
            auto* bullet = static_cast<Bullet*>(fx);

            if (bullet->numShots != 5)
            {
                bullet->shotInfo[bullet->numShots++].init(shot.attacker, shot.masterId, shot.damage, shot.hitLocation,
                                                          shot.entryAngle);
            }

            if (target == nullptr)
            {
                bullet->connect(vehicle, *point, 0);
            }
            else
            {
                bullet->owner = vehicle;
                bullet->target = target;
                bullet->ownerHotSpot = 0;
                bullet->targetHotSpot = targetHotSpot;
            }
        }
        else if (fx->objectClass == LASER)
        {
            auto* laser = static_cast<Laser*>(fx);

            if (target == nullptr)
            {
                laser->connect(vehicle, *point, &shot, 0);
            }
            else
            {
                laser->source.setWatcher(vehicle);
                laser->target.setWatcher(target);
                laser->sourceHotSpot = 0;
                laser->targetHotSpot = targetHotSpot;
                laser->shotInfo.init(shot.attacker, shot.masterId, shot.damage, shot.hitLocation, shot.entryAngle);
            }
        }
        else
        {
            auto* projectile = static_cast<ProjectileLaser*>(fx);

            if (target == nullptr)
            {
                projectile->connect(vehicle, *point, &shot, 0);
            }
            else
            {
                projectile->owner = vehicle;
                projectile->target = target;
                projectile->ownerHotSpot = 0;
                projectile->targetHotSpot = targetHotSpot;
                projectile->shotInfo.init(shot.attacker, shot.masterId, shot.damage, shot.hitLocation, shot.entryAngle);
            }
        }

        weaponList->addNode(fx);
    }

    /// <summary>Makes a weapon's effect object (Fatal when it can't).</summary>
    GameObject* createWeaponFX(const MasterComponent& weapon)
    {
        GameObject* fx = createObject(static_cast<int32_t>(weaponFXTable[weapon.weaponEffect]));

        if (fx == nullptr)
        {
            Fatal(-1, " couldnt create weapon FX ");
        }

        return fx;
    }

    /// <summary>The hot spot of the hit location, on a mech target; 0 otherwise.</summary>
    int32_t targetHotSpotOf(GameObject* target, int32_t hitLocation)
    {
        if (target != nullptr && target->objectClass == BATTLEMECH)
        {
            // Port fix: the original reads body[hitLocation], past the eight body locations for a rear torso hit
            // (8..10); the torso it maps to is read instead.
            return static_cast<BattleMech*>(target)->bodyAt(MechArmorToBodyLocation[hitLocation]).hotSpotNumber;
        }

        return 0;
    }

    /// <summary>Firing gives a vehicle away to the other side's mechs within visual range.</summary>
    void revealFiring(GroundVehicle* vehicle)
    {
        ObjectQueueNode* enemies = nullptr;
        uint8_t seenBy = 0;

        if (vehicle->alignment == 1)
        {
            enemies = clanMechList;
            seenBy = 2;
        }
        else if (vehicle->alignment == -1)
        {
            enemies = innerSphereMechList;
            seenBy = 1;
        }

        if (enemies == nullptr)
        {
            return;
        }

        for (BaseObject* enemy = enemies->head; enemy != nullptr; enemy = enemy->next)
        {
            vector_3d enemyPosition = static_cast<GameObject*>(enemy)->getPosition();

            if (vehicle->distanceFrom(enemyPosition) < scenario->maxVisualRange)
            {
                land->markRadiusSeen(vehicle->position, vehicle->frame.j, 360.0f, scenario->fireVisualRange, seenBy);
                return;
            }
        }
    }

    /// <summary>Where a missed shot lands: scattered up to <paramref name="scatter"/> about the aim point.</summary>
    /// <param name="centred">Missiles scatter both ways; other shots (as the original computes them) only one.</param>
    vector_3d missPoint(GameObject* target, vector_3d* targetPoint, float scatter, int centred)
    {
        vector_3d miss;
        miss.x = scatter;
        miss.y = scatter;
        miss.z = 0.0f;
        const auto offsetX = static_cast<float>(RandomNumber(static_cast<int32_t>(miss.x + miss.x)) - miss.x);
        const auto offsetY = static_cast<float>(RandomNumber(static_cast<int32_t>(miss.y + miss.y)) - miss.y);
        const auto offsetZ = static_cast<float>(RandomNumber(static_cast<int32_t>(miss.z + miss.z)) - miss.z);

        if (centred != 0)
        {
            miss.x = offsetX;
            miss.y = offsetY;
        }
        else
        {
            miss.x = miss.x + offsetX;
            miss.y = miss.y + offsetY;
        }

        miss.z = miss.z + offsetZ;
        const vector_3d base = target != nullptr ? target->getPosition() : *targetPoint;
        miss.x += base.x;
        miss.y += base.y;
        miss.z += base.z;
        return miss;
    }
}

auto GroundVehicle::fireWeapon(GameObject* target, float targetTime, int32_t weaponIndex, int32_t attackType,
                               int32_t aimLocation, vector_3d* targetPoint) -> int32_t
{
    if (status != 0)
    {
        return 1;
    }

    if (isWeaponIndex(weaponIndex) == 0)
    {
        return 2;
    }

    if (isWeaponReady(weaponIndex) == 0)
    {
        return 3;
    }

    float distance;

    if (target == nullptr)
    {
        if (targetPoint == nullptr || lineOfSight(*targetPoint) == 0)
        {
            return 4;
        }

        distance = distanceFrom(*targetPoint);
    }
    else
    {
        // A camera drone can't be shot for two seconds after launch.
        if (target->objectClass == CAMERADRONE && scenarioTime < static_cast<CameraDrone*>(target)->launchTime + 2.0)
        {
            return 4;
        }

        if (target->isDestroyed() != 0)
        {
            return 4;
        }

        if (lineOfSight(target) == 0)
        {
            return 4;
        }

        vector_3d targetPosition = target->getPosition();
        distance = distanceFrom(targetPosition);
    }

    const int32_t inRange = weaponInRange(weaponIndex, distance);

    if ((MPlayer == nullptr || MPlayer->isServer != 0) && inRange == 0)
    {
        return 4;
    }

    const MasterComponent& weapon = MasterComponentList[inventory[weaponIndex].masterID];

    if (weapon.missileType != 2 && weapon.missileType != 1 && weapon.missileType != 3)
    {
        // Direct fire needs a clear line.
        if (target == nullptr)
        {
            if (targetPoint == nullptr || lineOfFire(*targetPoint) == 0)
            {
                return 4;
            }
        }
        else if (lineOfFire(target) == 0)
        {
            return 4;
        }
    }

    // A pop-up turret fires only once it is up.
    if (weaponsDeployed == 0)
    {
        return 5;
    }

    const int32_t numShots = getWeaponShots(weaponIndex);

    if (numShots == 0)
    {
        return 4;
    }

    MechWarrior* targetPilot = nullptr;

    if (target != nullptr && isMoverClass(target))
    {
        targetPilot = target->getPilot();
        targetPilot->updateAttackerStatus(static_cast<uint32_t>(partId), scenarioTime);
    }

    float entryAngle = 0.0f;

    if (target != nullptr)
    {
        entryAngle = target->relFacingTo(position, -1);
    }

    const int isStreak = weapon.weaponFlags & 1;
    int32_t range = 0;
    int32_t hitChance =
        static_cast<int32_t>(calcAttackChance(target, aimLocation, targetTime, weaponIndex, 0.0f, &range, targetPoint));
    const int32_t hitRoll = RandomNumber(100);

    if (target != nullptr && target->getAlignment() == -1)
    {
        pilot->numSkillUses[MWS_GUNNERY][1]++;
    }

    // Aimed shots only from a standing vehicle.
    if (aimLocation != -1 && 0.0 < getVelocity().magnitude())
    {
        hitChance = 0;
    }

    int32_t hitLocation = -1;

    if (target != nullptr && hitRoll < hitChance)
    {
        if (target->getAlignment() == -1)
        {
            pilot->numSkillSuccesses[MWS_GUNNERY][1]++;
        }

        if (aimLocation != -1)
        {
            hitLocation = aimLocation;
        }
    }

    startWeaponRecycle(weaponIndex);

    const int32_t chunkWeapon = weaponIndex - numOther;
    const char* const badChunk = " GVehicle.fireWeapon: Bad WeaponFireChunk (save wfchunk.dbg file now) ";
    const char* const badMissChunk = " GVehicl.fireWeapon: Bad WeaponFireChunk (save wfchunk.dbg file now) ";

    if (hitRoll < hitChance)
    {
        if (numShots != UNLIMITED_SHOTS)
        {
            deductWeaponShot(weaponIndex, 1);
        }

        InventoryItem& item = inventory[weaponIndex];
        const MasterComponent& fired = MasterComponentList[item.masterID];

        if (fired.form == 9)
        {
            // Missiles: a streak fires them all, anything else about half; anti-missile systems take some out.
            const int32_t rackSize = fired.numMissiles;
            int32_t missiles = rackSize;

            if (isStreak == 0)
            {
                missiles = static_cast<int32_t>((rackSize + 1.0) * 0.5);

                if (missiles < 1)
                {
                    missiles = 1;
                }

                if (rackSize < missiles)
                {
                    missiles = rackSize;
                }
            }

            int32_t antiMissileShots = 0;
            int32_t missilesLeft = missiles;

            if (target != nullptr)
            {
                missilesLeft = target->fireAntiMissileSystem(missiles, antiMissileShots);

                if (antiMissileShots > 0)
                {
                    target->reduceAntiMissileAmmo(antiMissileShots);
                }
            }

            if (missilesLeft != 0)
            {
                GameObject* fx = createWeaponFX(fired);
                int32_t targetHotSpot = 0;

                if (target == nullptr)
                {
                    hitLocation = -1;
                }
                else
                {
                    if (aimLocation == -1)
                    {
                        hitLocation = target->calcHitLocation(this, weaponIndex, 0, attackType);
                    }

                    targetHotSpot = targetHotSpotOf(target, hitLocation);
                }

                Assert(hitLocation != -2 ? 1 : 0, 0, " GroundVehicle.FireWeapon: Bad Hit Location ", nullptr);
                _WeaponShotInfo shot;
                shot.init(this, item.masterID, fired.damage * static_cast<float>(missilesLeft), hitLocation,
                          entryAngle);

                if (MPlayer != nullptr && MPlayer->isServer != 0)
                {
                    sendTargetFireChunk(this, target, targetPoint, chunkWeapon, 1, entryAngle, missiles, missilesLeft,
                                        antiMissileShots, hitLocation, badChunk);
                }

                launchWeaponFX(this, fx, target, targetPoint, shot, targetHotSpot);

                if (target == nullptr)
                {
                    pilot->clearCurTacOrder(1, 0);
                }
            }
        }
        else
        {
            if (target == nullptr)
            {
                hitLocation = -1;
            }
            else if (aimLocation == -1)
            {
                hitLocation = target->calcHitLocation(this, weaponIndex, 0, attackType);
            }

            Assert(hitLocation != -2 ? 1 : 0, 0, " GroundVehicle.FireWeapon: Bad Hit Location ", nullptr);
            _WeaponShotInfo shot;
            shot.init(this, item.masterID, fired.damage, hitLocation, entryAngle);

            if (MPlayer != nullptr && MPlayer->isServer != 0)
            {
                sendTargetFireChunk(this, target, targetPoint, chunkWeapon, 1, entryAngle, 0, 0, 0, hitLocation,
                                    badChunk);
            }

            GameObject* fx = createWeaponFX(fired);
            launchWeaponFX(this, fx, target, targetPoint, shot, targetHotSpotOf(target, hitLocation));

            if (target == nullptr)
            {
                pilot->clearCurTacOrder(1, 0);
            }
        }
    }
    else if (isStreak == 0)
    {
        // A miss (a streak doesn't fire without a lock): the shot lands somewhere near.
        if (numShots != UNLIMITED_SHOTS)
        {
            deductWeaponShot(weaponIndex, 1);
        }

        InventoryItem& item = inventory[weaponIndex];
        const MasterComponent& fired = MasterComponentList[item.masterID];
        const float scatter = target != nullptr ? 25.0f : 5.0f;

        if (fired.form == 9)
        {
            const int32_t rackSize = fired.numMissiles;
            int32_t missiles = static_cast<int32_t>(rackSize * 0.5 + 0.5);

            if (missiles < 1)
            {
                missiles = 1;
            }

            if (rackSize < missiles)
            {
                missiles = rackSize;
            }

            if (missiles != 0)
            {
                GameObject* fx = createWeaponFX(fired);
                _WeaponShotInfo shot;
                shot.init(this, item.masterID, fired.damage * static_cast<float>(missiles), -1, entryAngle);
                vector_3d landing = missPoint(target, targetPoint, scatter, 1);

                if (MPlayer != nullptr && MPlayer->isServer != 0)
                {
                    sendTargetFireChunk(this, nullptr, &landing, chunkWeapon, 0, 0.0f, missiles, 0, 0, 0, badMissChunk);
                }

                launchWeaponFX(this, fx, nullptr, &landing, shot, 0);
            }
        }
        else
        {
            _WeaponShotInfo shot;
            shot.init(this, item.masterID, fired.damage, -1, entryAngle);
            GameObject* fx = createWeaponFX(fired);
            vector_3d landing = missPoint(target, targetPoint, scatter, 0);

            if (MPlayer != nullptr && MPlayer->isServer != 0)
            {
                sendTargetFireChunk(this, nullptr, &landing, chunkWeapon, 0, 0.0f, 0, 0, 0, 0, badMissChunk);
            }

            launchWeaponFX(this, fx, nullptr, &landing, shot, 0);
        }
    }

    if (targetPilot != nullptr)
    {
        targetPilot->triggerAlarm(0, static_cast<uint32_t>(partId));
    }

    revealFiring(this);

    if (group != nullptr)
    {
        group->handleMateFiredWeapon(static_cast<uint32_t>(partId));
    }

    return 0;
}

auto GroundVehicle::handleWeaponFire(int32_t weaponIndex, GameObject* target, vector_3d* targetPoint, int hit,
                                     float entryAngle, int32_t numMissiles, int32_t missilesPastAMS,
                                     int32_t antiMissileShots, int32_t hitLocation) -> int32_t
{
    const int32_t numShots = getWeaponShots(weaponIndex);
    startWeaponRecycle(weaponIndex);
    InventoryItem& item = inventory[weaponIndex];
    const MasterComponent& fired = MasterComponentList[item.masterID];
    const int isStreak = fired.weaponFlags & 1;
    _WeaponShotInfo shot;

    if (hit == 0)
    {
        Assert(target == nullptr ? 1 : 0, 0, " GVehicl.handleWeaponFire: target should be NULL with network miss! ",
               nullptr);
        Assert(targetPoint != nullptr ? 1 : 0, 0,
               " GVehicl.handleWeaponFire: MUST have targetpoint with network miss! ", nullptr);

        if (isStreak != 0)
        {
            CurMoverWeaponFireChunk.unpack(this);
            DebugWeaponFireChunk(&CurMoverWeaponFireChunk, nullptr, this);
            Assert(0, 0, " GVehicl.handleWeaponFire: streaks shouldn't miss! ", nullptr);
        }

        if (numShots != UNLIMITED_SHOTS)
        {
            deductWeaponShot(weaponIndex, 1);
        }

        if (fired.form == 9)
        {
            if (numMissiles > 0)
            {
                GameObject* fx = createWeaponFX(fired);
                shot.init(this, item.masterID, fired.damage * static_cast<float>(numMissiles), -1, entryAngle);
                launchWeaponFX(this, fx, nullptr, targetPoint, shot, 0);
            }
        }
        else
        {
            shot.init(this, item.masterID, fired.damage, -1, entryAngle);
            GameObject* fx = createWeaponFX(fired);
            launchWeaponFX(this, fx, nullptr, targetPoint, shot, 0);
        }
    }
    else
    {
        if (numShots != UNLIMITED_SHOTS)
        {
            deductWeaponShot(weaponIndex, 1);
        }

        if (fired.form == 9)
        {
            if (antiMissileShots > 0)
            {
                target->reduceAntiMissileAmmo(antiMissileShots);
            }

            if (missilesPastAMS != 0)
            {
                GameObject* fx = createWeaponFX(fired);
                Assert(hitLocation != -2 ? 1 : 0, static_cast<uint32_t>(TargetRolo),
                       " GroundVehicle.handleWeaponFire: Bad Hit Location ", nullptr);
                const int32_t targetHotSpot = targetHotSpotOf(target, hitLocation);
                shot.init(this, item.masterID, fired.damage * static_cast<float>(missilesPastAMS), hitLocation,
                          entryAngle);
                launchWeaponFX(this, fx, target, targetPoint, shot, targetHotSpot);

                if (target == nullptr)
                {
                    pilot->clearCurTacOrder(1, 0);
                }
            }
        }
        else
        {
            shot.init(this, item.masterID, fired.damage, hitLocation, entryAngle);
            GameObject* fx = createWeaponFX(fired);
            launchWeaponFX(this, fx, target, targetPoint, shot, targetHotSpotOf(target, hitLocation));

            if (target == nullptr)
            {
                pilot->clearCurTacOrder(1, 0);
            }
        }
    }

    if (target != nullptr && isMoverClass(target))
    {
        MechWarrior* targetPilot = target->getPilot();
        targetPilot->updateAttackerStatus(static_cast<uint32_t>(partId), scenarioTime);
        targetPilot->triggerAlarm(0, static_cast<uint32_t>(partId));
    }

    revealFiring(this);

    if (group != nullptr)
    {
        group->handleMateFiredWeapon(static_cast<uint32_t>(partId));
    }

    return 0;
}

auto GroundVehicle::openStatusWindow(int32_t left, int32_t top, int32_t right, int32_t bottom) -> int32_t
{
    auto* window = new GroundVehicleStatusWindow;
    statusWindow = window;
    window->init(left, top, right, bottom, this);
    statusWindow->setBackColor(0);
    statusWindow->draw();
    screenWindow->addChild(statusWindow);

    if (pilot != nullptr)
    {
        pilot->openStatusWindow(left + 30, top + 30, right, bottom);
    }

    return 0;
}

GroundVehicleStatusWindow::~GroundVehicleStatusWindow()
{
    // The inline ~aTitleWindow.
    aTitleWindow::destroy();
}

auto GroundVehicle::closeStatusWindow() -> int32_t
{
    if (pilot != nullptr)
    {
        pilot->closeStatusWindow();
    }

    // The window is destroyed, not deleted.
    statusWindow->destroy();
    statusWindow = nullptr;
    return 0;
}

auto GroundVehicle::getVitalInfo(void* vitalInfo) -> int32_t
{
    const int32_t size = Mover::getVitalInfo(nullptr);

    if (vitalInfo != nullptr)
    {
        Mover::getVitalInfo(vitalInfo);
        static_cast<uint8_t*>(vitalInfo)[size] = static_cast<uint8_t>(movementEnabled);
    }

    return size + 1;
}

auto GroundVehicle::getTotalEffectiveness() -> float
{
    if (isDestroyed() != 0 || isDisabled() != 0)
    {
        return 0.0f;
    }

    const float weaponRatio = maxWeaponEffectiveness == 0.0f ? 1.0f : weaponEffectiveness / maxWeaponEffectiveness;
    // Each location's armor share, scaled to 0.4..1; a turret without armor counts in full.
    const auto armorFactor = [&](int32_t location)
    { return static_cast<float>(armor[location].curArmor / static_cast<float>(armor[location].maxArmor) * 0.6 + 0.4); };
    const float front = armorFactor(GROUNDVEHICLE_LOCATION_FRONT);
    const float left = armorFactor(GROUNDVEHICLE_LOCATION_LEFT);
    const float right = armorFactor(GROUNDVEHICLE_LOCATION_RIGHT);
    const float rear = armorFactor(GROUNDVEHICLE_LOCATION_REAR);
    float turret = 1.0f;

    if (static_cast<float>(armor[GROUNDVEHICLE_LOCATION_TURRET].maxArmor) != 0.0)
    {
        turret = armorFactor(GROUNDVEHICLE_LOCATION_TURRET);
    }

    // Wounds wear the crew down.
    const float woundFactor[7] = {1.0f, 0.95f, 0.85f, 0.75f, 0.5f, 0.3f, 0.0f};
    int32_t wounds = static_cast<int32_t>(getPilot()->wounds);
    // Port fix: the original indexes the table unchecked.
    wounds = std::clamp(wounds, 0, 6);
    return turret * rear * right * left * front * woundFactor[wounds] * weaponRatio;
}

auto GroundVehicleStatusWindow::init(int32_t x, int32_t y, int32_t w, int32_t h, GroundVehicle* newVehicle) -> void
{
    aTitleWindow::init(x, y, w, h, nullptr);

    if (titleBar != nullptr)
    {
        titleBar->showCloseButton(1);
    }

    vehicle = newVehicle;
}

auto GroundVehicleStatusWindow::handleEvent(aEvent* event) -> void
{
    if (event->type == 0xd)
    {
        vehicle->closeStatusWindow();
    }

    aObject::handleEvent(event);
}

auto GroundVehicleStatusWindow::resize(int32_t w, int32_t h) -> void
{
    aTitleWindow::resize(w, h);
}

namespace
{
    /// <summary>Alignment names, by alignment + 1.</summary>
    const char* const AlignmentNames[3] = {"Clan", "Neutral", "Inner Sphere"};

    /// <summary>The status window's title: alignment, vehicle name and crew callsign.</summary>
    void setVehicleTitle(GroundVehicleStatusWindow* window)
    {
        GroundVehicle* vehicle = window->vehicle;

        if (vehicle == nullptr)
        {
            return;
        }

        char title[256];
        std::snprintf(title, sizeof(title), "%s %s (%s)", AlignmentNames[vehicle->getAlignment() + 1],
                      vehicle->debugStatus, vehicle->getPilot()->callsign);
        window->setTitle(title);
    }
}

auto GroundVehicleStatusWindow::display() -> void
{
    VFX_pane_wipe(displayPort->frame(), backgroundColor);
    setVehicleTitle(this);
    aObject::display();
}

auto GroundVehicleStatusWindow::draw() -> void
{
    setVehicleTitle(this);
    aTitleWindow::draw();
}
