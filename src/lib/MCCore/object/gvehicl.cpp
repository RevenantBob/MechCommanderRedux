#include "stdafx.h"
#include "object/gvehicl.h"
#include "ai/move.h"
#include "ai/tacordr.h"
#include "appear/apprtype.h"
#include "camera/camera.h"
#include "camera/camlist.h"
#include "engine/MCElementBuffer.h"
#include "engine/MCLineElement.h"
#include "engine/MCEllipseElement.h"
#include "engine/MCVfxElement.h"
#include "vfx/MCVfxFunctions.h"
#include "gui/afont.h"
#include "gui/aport.h"
#include "gui/asystem.h"
#include "iface/iface.h"
#include "lib/MCFatal.h"
#include "lib/MCFitIniFile.h"
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
    const bool ThrottleTablesFilled = []
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
uint32_t WeaponFXTable[32] = {455, 461, 462, 188, 467, 468, 0xffffffff, 458, 14,  190, 191, 192, 456, 463, 464, 457,
                              465, 466, 459, 460, 879, 880, 881,        882, 883, 884, 885, 886, 887, 888, 889, 890};
float GvCollisionThreshold = 0.0f;
float GvObjectCollisionThreshold = 0.0f;
float GvTonnageCollisionThreshold = 0.0f;
float GvTreeDeflection = 0.0f;
float GvSweepTime = 0.0f;
float GvHillSpeedFactor = 0.0f;
float MaxVelocityMag = 0.0f;

namespace
{
    /// <summary>Half pi, as MCX.EXE stores it.</summary>
    constexpr double HALF_PI = 0x1.921fb5443e88cp+0;
    /// <summary>Degrees to radians, as MCX.EXE stores it (a hair under pi / 180).</summary>
    constexpr double DEGREES_TO_RADIANS = 0x1.1df46a2526c7ap-6;

    /// <summary>Turns a frame about its k axis (MC2's inline frame_of_ref::rotate_about_k).</summary>
    void RotateAboutK(MCFrameOfRef& frame, float s, float c)
    {
        const MCVector3D oldI = frame.I;
        frame.I = frame.I * c + frame.J * s;
        frame.J = frame.J * c - oldI * s;
    }

    /// <summary>
    /// Hits <paramref name="victim"/> from <paramref name="shooter"/>'s side for <paramref name="damage"/> (attack
    /// source 1).
    /// </summary>
    void CollisionHit(MCGameObject* victim, MCGameObject* shooter, float damage)
    {
        const int32_t hitLocation = victim->CalcHitLocation(shooter, -1, 1, 0);
        const float entryAngle = victim->RelFacingTo(shooter->GetPosition(), -1);
        MCWeaponShotInfo shotInfo;
        shotInfo.Init(shooter, -1, damage, hitLocation, entryAngle);
        victim->HandleWeaponHit(&shotInfo, MPlayer != nullptr);
    }
}

auto LoadGroundVehicleGameSystem(MCFitIniFile* sysFile) -> int32_t
{
    int32_t result = sysFile->SeekBlock("GroundVehicle:FireWeapon");

    if (result != 0)
    {
        return result;
    }

    if ((result = sysFile->ReadIdLongArray("AttackerMoveModifier", GroundVehicleAttackerMoveModifier, 4)) != 0)
    {
        return result;
    }

    if ((result = sysFile->SeekBlock("GroundVehicle:Damage")) != 0)
    {
        return result;
    }

    if ((result = sysFile->ReadIdLongArray("CriticalHitTable", GroundVehicleCriticalHitTable, 11)) != 0)
    {
        return result;
    }

    if ((result = sysFile->SeekBlock("GroundVehicle:Collision")) != 0)
    {
        return result;
    }

    if ((result = sysFile->ReadIdFloat("collisionThreshold", GvCollisionThreshold)) != 0)
    {
        return result;
    }

    if ((result = sysFile->ReadIdFloat("objectThreshold", GvObjectCollisionThreshold)) != 0)
    {
        return result;
    }

    if ((result = sysFile->ReadIdFloat("tonnageThreshold", GvTonnageCollisionThreshold)) != 0)
    {
        return result;
    }

    if ((result = sysFile->ReadIdFloat("treeDeflection", GvTreeDeflection)) != 0)
    {
        return result;
    }

    if ((result = sysFile->SeekBlock("GroundVehicle:Movement")) != 0)
    {
        return result;
    }

    int32_t value = 0;

    if (sysFile->ReadIdLong("CrashAvoidSelf", value) == 0)
    {
        DefaultGroundVehicleCrashAvoidSelf = value;
    }

    if (sysFile->ReadIdLong("CrashAvoidPath", value) == 0)
    {
        DefaultGroundVehicleCrashAvoidPath = value;
    }

    if (sysFile->ReadIdLong("CrashBlockSelf", value) == 0)
    {
        DefaultGroundVehicleCrashBlockSelf = value;
    }

    if (sysFile->ReadIdLong("CrashBlockPath", value) == 0)
    {
        DefaultGroundVehicleCrashBlockPath = value;
    }

    float yieldTime = 0.0f;

    if (sysFile->ReadIdFloat("CrashYieldTime", yieldTime) == 0)
    {
        DefaultGroundVehicleCrashYieldTime = yieldTime;
    }

    if ((result = sysFile->ReadIdFloat("SweeperSlowTime", GvSweepTime)) != 0)
    {
        return result;
    }

    if ((result = sysFile->ReadIdFloat("WalkSpeed", GvWalkSpeed)) != 0)
    {
        return result;
    }

    return sysFile->ReadIdFloat("HillSpeedFactor", GvHillSpeedFactor);
}

//---------------------------------------------------------------------------
// GroundVehicleType
//---------------------------------------------------------------------------

auto MCGroundVehicleType::Init() -> void
{
    CrashAvoidSelf = DefaultGroundVehicleCrashAvoidSelf;
    CrashAvoidPath = DefaultGroundVehicleCrashAvoidPath;
    CrashBlockSelf = DefaultGroundVehicleCrashBlockSelf;
    CrashBlockPath = DefaultGroundVehicleCrashBlockPath;
    VehicleId = 0;
    Name.clear();
    Alignment = 0;
    Chassis = 0;
    TonnageClass = 0.0f;
    InternalStructureTonnage = 0.0f;
    AmmoTruck = 0;
    MineSweeper = 0;
    RefitPoints = 0;
    MinesToLay = 0;
    ElementalCarrier = 0;
    CrashYieldTime = DefaultGroundVehicleCrashYieldTime;
    Seats = 0;
    ExplDmg = 0.0f;
    ExplRad = 0.0f;
}

auto MCGroundVehicleType::Destroy() -> void
{
    Name.clear();
    delete DynamicsType;
    DynamicsType = nullptr;
    MCObjectType::Destroy();
}

auto MCGroundVehicleType::Init(MCFile* objFile, uint32_t fileSize) -> int32_t
{
    static const char* const locationNames[NUM_GROUNDVEHICLE_LOCATIONS] = {"Front", "Left", "Right", "Rear", "Turret"};

    MCFitIniFile vehicleFile;
    int32_t result = vehicleFile.Open(objFile, fileSize);

    if (result != 0)
    {
        return result;
    }

    if ((result = vehicleFile.SeekBlock("Header")) != 0)
    {
        return result;
    }

    char fileType[128];

    if ((result = vehicleFile.ReadIdString("FileType", fileType, 127)) != 0)
    {
        return result;
    }

    if (std::strcmp(fileType, "GroundVehicleType") != 0)
    {
        return -1;
    }

    if ((result = vehicleFile.SeekBlock("General")) != 0)
    {
        return result;
    }

    if ((result = vehicleFile.ReadIdULong("ID", VehicleId)) != 0)
    {
        return result;
    }

    // "Alignment" 0 is 1, 1 is -1.
    static constexpr uint8_t alignmentMap[2] = {1, 0xff};
    uint8_t fileAlignment = 0;

    if ((result = vehicleFile.ReadIdUChar("Alignment", fileAlignment)) != 0)
    {
        return result;
    }

    // Port fix: the original reads other values from past its two-entry table on the stack.
    Alignment = fileAlignment < 2 ? alignmentMap[fileAlignment] : 0;
    char nameBuffer[128];
    vehicleFile.ReadIdString("Name", nameBuffer, 127);
    Name = nameBuffer;

    if ((result = vehicleFile.ReadIdUChar("Chassis", Chassis)) != 0)
    {
        return result;
    }

    if ((result = vehicleFile.ReadIdFloat("TonnageClass", TonnageClass)) != 0)
    {
        return result;
    }

    vehicleFile.ReadIdBoolean("AmmoTruck", AmmoTruck);
    vehicleFile.ReadIdLong("RefitPoints", RefitPoints);
    vehicleFile.ReadIdBoolean("MineSweeper", MineSweeper);
    vehicleFile.ReadIdLong("MinesToLay", MinesToLay);
    vehicleFile.ReadIdBoolean("ElementalCarrier", ElementalCarrier);
    vehicleFile.ReadIdUChar("Seats", Seats);
    Assert(Seats <= MAX_GROUNDVEHICLE_SEATS ? 1 : 0, Seats, "Too many seats");

    if (vehicleFile.ReadIdFloat("ExplosionRadius", ExplRad) != 0)
    {
        ExplRad = 0.0f;
    }

    if (vehicleFile.ReadIdFloat("ExplosionDamage", ExplDmg) != 0)
    {
        ExplDmg = 0.0f;
    }

    if ((result = vehicleFile.SeekBlock("InternalStructure")) != 0)
    {
        return result;
    }

    for (int32_t location = 0; location < NUM_GROUNDVEHICLE_LOCATIONS; location++)
    {
        if ((result = vehicleFile.ReadIdUChar(locationNames[location], InternalStructure[location])) != 0)
        {
            return result;
        }
    }

    if ((result = vehicleFile.SeekBlock("Dynamics")) != 0)
    {
        return result;
    }

    uint32_t dynamicsTypeId = 0;

    if ((result = vehicleFile.ReadIdULong("Type", dynamicsTypeId)) != 0)
    {
        return result;
    }

    if (dynamicsTypeId != 2)
    {
        return -0x5fffd;
    }

    DynamicsType = new MCGroundVehicleDynamicsType;

    if (DynamicsType == nullptr)
    {
        return -0x5fffe;
    }

    if ((result = DynamicsType->Init(&vehicleFile)) != 0)
    {
        return result;
    }

    if (vehicleFile.SeekBlock("MovementSystem") == 0)
    {
        int32_t value = 0;

        if (vehicleFile.ReadIdLong("CrashAvoidSelf", value) == 0)
        {
            CrashAvoidSelf = value;
        }

        if (vehicleFile.ReadIdLong("CrashAvoidPath", value) == 0)
        {
            CrashAvoidPath = value;
        }

        if (vehicleFile.ReadIdLong("CrashBlockSelf", value) == 0)
        {
            CrashBlockSelf = value;
        }

        if (vehicleFile.ReadIdLong("CrashBlockPath", value) == 0)
        {
            CrashBlockPath = value;
        }

        float yieldTime = 0.0f;

        if (vehicleFile.ReadIdFloat("CrashYieldTime", yieldTime) == 0)
        {
            CrashYieldTime = yieldTime;
        }
    }

    return MCObjectType::Init(&vehicleFile);
}

auto MCGroundVehicleType::HandleCollision(MCGameObject* collidee, MCGameObject* collider) -> int
{
    if (MPlayer != nullptr && MPlayer->IsServer == 0)
    {
        return 0;
    }

    switch (collider->ObjectClass)
    {
        case BATTLEMECH:
        case GROUNDVEHICLE:
        case ELEMENTAL:
        {
            const int friendly = collidee->GetPilot()->Alignment == collider->GetPilot()->Alignment ? 1 : 0;

            if (collider->ObjectClass == ELEMENTAL && static_cast<MCElemental*>(collider)->ElementalCanJump == 0)
            {
                return 0;
            }

            if (friendly != 0)
            {
                return 0;
            }

            MCGameObject* collideeRamTarget = collidee->GetPilot()->CurTacOrder.GetRamTarget();
            MCGameObject* colliderRamTarget = collider->GetPilot()->CurTacOrder.GetRamTarget();

            if (collideeRamTarget != collider && colliderRamTarget != collidee)
            {
                return 0;
            }

            if (collidee->GetCollisionFreeFrom() == collider && ScenarioTime <= collidee->GetCollisionFreeTime())
            {
                return 0;
            }

            collidee->SetCollisionFreeFrom(collider);
            collidee->SetCollisionFreeTime(ScenarioTime + 2.0f);

            if (static_cast<MCGroundVehicleDynamicsType*>(DynamicsType)->MaxVelocity != 0.0f)
            {
                MCFrameOfRef frame = collidee->GetFrame();
                RotateAboutK(frame, static_cast<float>(std::sin(HALF_PI)), static_cast<float>(std::cos(HALF_PI)));
                collidee->SetFrame(frame);
                collidee->GetVelocity();
                static_cast<MCMover*>(collidee)->BounceToAdjCell();
            }

            CollisionHit(collidee, collider, 1.0f);
            break;
        }

        case BUILDING:
        case TREEBUILDING:
        {
            if (collidee->GetCollisionFreeFrom() == collider && ScenarioTime <= collidee->GetCollisionFreeTime())
            {
                return 0;
            }

            collidee->SetCollisionFreeFrom(collider);
            collidee->SetCollisionFreeTime(ScenarioTime + 2.0f);
            MCFrameOfRef frame = collidee->GetFrame();
            // A big building turns the vehicle further.
            const float angle = collider->GetObjectType()->ExtentRadius > GvObjectCollisionThreshold ? 135.0f : 45.0f;
            RotateAboutK(frame, static_cast<float>(std::sin(angle * DEGREES_TO_RADIANS)),
                         static_cast<float>(std::cos(angle * DEGREES_TO_RADIANS)));
            collidee->SetFrame(frame);
            static_cast<MCMover*>(collidee)->BounceToAdjCell();
            CollisionHit(collidee, collider, static_cast<float>(collider->GetTonnage() * 0.01 + 0.5));
            break;
        }

        case TREE:
        {
            if (collidee->GetCollisionFreeFrom() == collider && ScenarioTime <= collidee->GetCollisionFreeTime())
            {
                return 0;
            }

            collidee->SetCollisionFreeFrom(collider);
            collidee->SetCollisionFreeTime(ScenarioTime + 2.0f);
            MCFrameOfRef frame = collidee->GetFrame();
            collider->GetObjectType();
            double deflection = 0.0;

            if (TonnageClass < GvTonnageCollisionThreshold)
            {
                deflection = static_cast<double>(GvTonnageCollisionThreshold) / TonnageClass * GvTreeDeflection;
            }

            if (deflection > 0.0)
            {
                RotateAboutK(frame, static_cast<float>(std::sin(deflection * DEGREES_TO_RADIANS)),
                             static_cast<float>(std::cos(deflection * DEGREES_TO_RADIANS)));
                collidee->SetFrame(frame);
            }
            break;
        }

        case TRAINCAR:
        {
            if (collidee->GetCollisionFreeFrom() == collider && ScenarioTime <= collidee->GetCollisionFreeTime())
            {
                return 0;
            }

            collidee->SetCollisionFreeFrom(collider);
            collidee->SetCollisionFreeTime(ScenarioTime + 2.0f);

            if (static_cast<MCGroundVehicleDynamicsType*>(DynamicsType)->MaxVelocity != 0.0f)
            {
                MCFrameOfRef frame = collidee->GetFrame();
                RotateAboutK(frame, static_cast<float>(std::sin(HALF_PI)), static_cast<float>(std::cos(HALF_PI)));
                collidee->SetFrame(frame);
                collidee->GetVelocity();
            }

            static_cast<MCMover*>(collidee)->BounceToAdjCell();
            break;
        }

        default:
            return 0;
    }

    SoundSystem->PlayDigitalSample(4, 1, collidee, 0, 0);
    return 0;
}

auto MCGroundVehicleType::HandleDestruction(MCGameObject* collidee, MCGameObject* collider) -> int
{
    auto* vehicle = static_cast<MCGroundVehicle*>(collidee);

    if (vehicle->GetPilot() == nullptr)
    {
        Fatal(0, " No Pilot in this vehicle! ");
    }

    if (vehicle->GetPoint() == vehicle)
    {
        vehicle->Group->SetPoint(nullptr);
    }

    if (vehicle->SensorSystem != nullptr)
    {
        vehicle->SensorSystem->Disable();
    }

    vehicle->DeathTimer = 0.0f;

    if (vehicle->Withdrawing == 0)
    {
        vehicle->GetPilot()->TriggerAlarm(7, collider == nullptr ? 0 : collider->IdNumber);
        vehicle->DeathExplosionDone = 0;
        vehicle->Status = 2;

        if (vehicle->GetAlignment() == HomeTeam->Alignment)
        {
            FriendlyDestroyed = 1;
        }
        else
        {
            EnemyDestroyed = 1;
        }
    }
    else
    {
        vehicle->GetPilot()->TriggerAlarm(8, 0);
    }

    TheInterface->RemoveMech(vehicle->PartId);
    return 1;
}

auto MCGroundVehicleType::LoadHotSpots(MCFitIniFile* vehicleFile) -> int32_t
{
    return 0;
}

auto MCGroundVehicleType::CreateInstance() -> MCBaseObject*
{
    auto* newVehicle = new MCGroundVehicle;

    if (newVehicle == nullptr)
    {
        return nullptr;
    }

    if (newVehicle->Init(this) != 0)
    {
        return nullptr;
    }

    newVehicle->IdNumber = NextIdNumber++;
    return newVehicle;
}

//---------------------------------------------------------------------------
// GroundVehicle
//---------------------------------------------------------------------------

auto MCGroundVehicle::RelViewFacingTo(MCVector3D goal) -> float
{
    return RelFacingTo(goal, GROUNDVEHICLE_LOCATION_TURRET);
}

auto MCGroundVehicle::GetBodyState() -> int32_t
{
    // The original reads +0x74 of either appearance: a pop-up turret's gives the bits of its shapeMaxY.
    if (GvAppearance == 0)
    {
        return std::bit_cast<int32_t>(static_cast<MCPUAppearance*>(Appearance)->ShapeMaxY);
    }

    return static_cast<MCGVAppearance*>(Appearance)->CurrentState;
}

auto MCGroundVehicle::CanMove() -> int
{
    return MovementEnabled;
}

auto MCGroundVehicle::GetThrottle() -> int32_t
{
    return static_cast<MCGroundVehicleControlData*>(Control->ControlData)->Throttle;
}

auto MCGroundVehicle::IsCaptureable() -> int
{
    if ((Captureable != 0 || Salvage != nullptr) && IsCaptured() == 0 && IsDestroyed() == 0)
    {
        return 1;
    }

    return 0;
}

auto MCGroundVehicle::GetRefitPoints() -> float
{
    if (Refitter != 0)
    {
        return Armor[GROUNDVEHICLE_LOCATION_TURRET].CurArmor;
    }

    return 0.0f;
}

auto MCGroundVehicle::BurnRefitPoints(float pointsToBurn) -> int
{
    if (Refitter != 0 && pointsToBurn <= Armor[GROUNDVEHICLE_LOCATION_TURRET].CurArmor)
    {
        Armor[GROUNDVEHICLE_LOCATION_TURRET].CurArmor -= pointsToBurn;
        return 1;
    }

    return 0;
}

auto MCGroundVehicle::HandleStaticCollision() -> void
{
    if (CollisionsOn == 0 || Dynamics->GetVelocity() <= 0.0f)
    {
        return;
    }

    int32_t blockNumber = 0;
    int32_t vertexNumber = 0;
    GetBlockAndVertexNumber(blockNumber, vertexNumber);
    char listName[12];
    std::sprintf(listName, "TBlk%d", blockNumber);
    MCObjectQueueNode* list = ObjectList->Head;

    while (list != nullptr && list->operator==(listName) == 0)
    {
        list = list->Next;
    }

    Assert(list != nullptr ? 1 : 0, blockNumber, "Could not find objlist for block");

    // Port fix: the original reads the objects of a missing list through null.
    if (list == nullptr)
    {
        return;
    }

    for (MCBaseObject* object = list->Head; object != nullptr; object = object->Next)
    {
        auto* other = static_cast<MCGameObject*>(object);

        if (other->GetObjectType() == nullptr)
        {
            continue;
        }

        int collides = 0;
        int32_t otherBlock = -1;
        int32_t otherVertex = -1;

        switch (other->ObjectClass)
        {
            case BUILDING:
            case TREE:
            case TERRAINOBJECT:
            case TREEBUILDING:
            {
                other->GetBlockAndVertexNumber(otherBlock, otherVertex);
                collides = other->CollisionsOn;
                break;
            }
            case MISCTERRAINOBJECT:
            {
                GetBlockAndVertexNumber(otherBlock, otherVertex);

                if (static_cast<uint32_t>(static_cast<MCMiscTerrainObject*>(other)->TerrainObjectKind) > 6)
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
            CollisionSystem->DetectStaticCollision(this, other);
        }
    }
}

auto MCGroundVehicle::Init() -> void
{
    ObjectClass = GROUNDVEHICLE;
    Body = std::make_unique<MCBodyLocation[]>(NUM_GROUNDVEHICLE_LOCATIONS);
    NumBodyLocations = NUM_GROUNDVEHICLE_LOCATIONS;
    Armor = std::make_unique<MCArmorLocation[]>(NUM_GROUNDVEHICLE_LOCATIONS);
    MovementEnabled = 1;
    TurretEnabled = 1;
    WeaponsDeployed = 1;
    NumArmorLocations = NUM_GROUNDVEHICLE_LOCATIONS;
    TurretRotation = 0.0f;
    Smoke = nullptr;
    StatusWindow = nullptr;
    Captureable = 0;
    AmmoTruck = 0;
    RefitBuddy = nullptr;
    Refitter = 0;
    Refitting = 0;
    SweepTime = -1.0f;
    MineLayer = 0;
    MinesToLay = 0;
    ElementalCarrier = 0;

    for (int32_t i = 0; i < 10; i++)
    {
        Elementals[i] = nullptr;
    }

    for (int32_t i = 0; i < 4; i++)
    {
        Passengers[i] = nullptr;
    }

    Seats = 0;
    BlipFrame = 0;
    CellRowToMine = -1;
    CellColToMine = -1;
    MineCellHandled = 0;
    MineLayTime = 0.0f;
}

auto MCGroundVehicle::Init(MCObjectType* objType) -> int32_t
{
    int32_t result = MCGameObject::Init(objType);

    if (result != 0)
    {
        return result;
    }

    auto* vehicleType = static_cast<MCGroundVehicleType*>(objType);
    CollisionsOn = 1;

    for (int32_t location = 0; location < NUM_GROUNDVEHICLE_LOCATIONS; location++)
    {
        BodyAt(location).MaxInternalStructure = vehicleType->InternalStructure[location];
        BodyAt(location).HasCase = 0;
        BodyAt(location).DamageState = 0;
        BodyAt(location).CriticalSpaces = nullptr;
    }

    Alignment = vehicleType->Alignment;
    InternalStructureTonnage = vehicleType->InternalStructureTonnage;
    PathLockLevel = vehicleType->CrashBlockSelf;
    Chassis = vehicleType->Chassis;
    TonnageClass = vehicleType->TonnageClass;
    AmmoTruck = vehicleType->AmmoTruck;
    CrashAvoidSelf = vehicleType->CrashAvoidSelf;
    CrashAvoidPath = vehicleType->CrashAvoidPath;
    PathLockRange = vehicleType->CrashBlockPath;
    CrashYieldTime = vehicleType->CrashYieldTime;

    if (vehicleType->RefitPoints != 0)
    {
        Refitter = 1;
    }

    MineSweeper = vehicleType->MineSweeper;
    MinesToLay = vehicleType->MinesToLay;

    if (MinesToLay > 0)
    {
        MineLayer = 1;
    }

    ElementalCarrier = vehicleType->ElementalCarrier;
    Seats = vehicleType->Seats;
    Control = nullptr;
    Dynamics = vehicleType->DynamicsType->CreateInstance();

    if (Dynamics == nullptr)
    {
        return -0x5fff8;
    }

    if ((result = Dynamics->Init(vehicleType->DynamicsType, this)) != 0)
    {
        return result;
    }

    const uint32_t appearanceId = vehicleType->AppearName;
    MCAppearanceType* apprType = AppearanceTypeList->GetAppearance(appearanceId, 0);

    if (apprType == nullptr)
    {
        return -0x2fff7;
    }

    switch (appearanceId & 0xff000000)
    {
        case 0x5000000:
        {
            auto* vehicleAppearance = new MCGVAppearance;
            Appearance = vehicleAppearance;

            if (vehicleAppearance == nullptr)
            {
                return -0x2ffff;
            }

            vehicleAppearance->Init(nullptr, nullptr);

            if ((apprType->AppearanceNum & 0xff000000) != 0x5000000)
            {
                return -0x2fff6;
            }

            if ((result = vehicleAppearance->Init(apprType, this)) != 0)
            {
                return result;
            }

            GvAppearance = 1;
            WeaponsDeployed = 1;
            break;
        }

        case 0x9000000:
        {
            auto* turretAppearance = new MCPUAppearance;
            Appearance = turretAppearance;

            if (turretAppearance == nullptr)
            {
                return -0x2ffff;
            }

            turretAppearance->Init(nullptr, nullptr);

            if ((apprType->AppearanceNum & 0xff000000) != 0x9000000)
            {
                return -0x2fff6;
            }

            if ((result = turretAppearance->Init(apprType, this)) != 0)
            {
                return result;
            }

            GvAppearance = 0;
            WeaponsDeployed = 0;
            break;
        }

        default:
            break;
    }

    ObjectClass = GROUNDVEHICLE;
    DistanceSinceMarkSeen = 1000.0f;
    return 0;
}

auto MCGroundVehicle::SetControl(uint32_t controlType, uint32_t controlData, int32_t controlParam) -> int32_t
{
    int32_t result = 0;

    switch (controlType)
    {
        case 1:
        {
            delete Control;
            auto* playerControl = new MCPlayerControl;
            Control = playerControl;

            if (playerControl == nullptr)
            {
                return -0x5fffc;
            }

            if ((result = playerControl->Init(this, 0)) != 0)
            {
                return result;
            }
            break;
        }

        case 2:
        {
            delete Control;
            auto* aiControl = new MCGroundVehicleAIControl;
            Control = aiControl;

            if (aiControl == nullptr)
            {
                return -0x5fffc;
            }

            if ((result = aiControl->Init(this)) != 0)
            {
                return result;
            }
            break;
        }

        case 3:
        {
            delete Control;
            auto* netControl = new MCGroundVehicleNetControl;
            Control = netControl;

            if (netControl == nullptr)
            {
                return -0x5fffc;
            }

            if ((result = netControl->Init(this)) != 0)
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

    auto* vehicleControlData = new MCGroundVehicleControlData;
    Control->ControlData = vehicleControlData;

    if (vehicleControlData == nullptr)
    {
        return -0x5fffa;
    }

    return vehicleControlData->Init(0);
}

auto MCGroundVehicle::Init(MCFitIniFile* vehicleFile) -> int32_t
{
    static const char* const locationNames[NUM_GROUNDVEHICLE_LOCATIONS] = {"Front", "Left", "Right", "Rear", "Turret"};

    int32_t result = vehicleFile->SeekBlock("Header");

    if (result != 0)
    {
        return result;
    }

    char fileType[128];

    if ((result = vehicleFile->ReadIdString("FileType", fileType, 127)) != 0)
    {
        return result;
    }

    if (std::strcmp(fileType, "GroundVehicleProfile") != 0)
    {
        return -1;
    }

    if ((result = vehicleFile->SeekBlock("General")) != 0)
    {
        return result;
    }

    char crewBuffer[128];
    vehicleFile->ReadIdString("Crew", crewBuffer, 127);
    CrewName = crewBuffer;

    if (vehicleFile->ReadIdBoolean("NotMineYet", NotMineYet) != 0)
    {
        NotMineYet = 1;
    }

    if (vehicleFile->ReadIdLong("DescIndex", DescIndex) != 0)
    {
        DescIndex = -1;
    }

    char ifaceNameBuffer[256];
    CLoadString(ThisInstance, DescIndex + 700, ifaceNameBuffer, 0xfe);
    DebugStatus = ifaceNameBuffer;

    if ((result = vehicleFile->ReadIdLong("NameIndex", NameIndex)) != 0)
    {
        return result;
    }

    if ((result = vehicleFile->ReadIdFloat("CurTonnage", Tonnage)) != 0)
    {
        return result;
    }

    char fileStatus = 0;

    if ((result = vehicleFile->ReadIdChar("Status", fileStatus)) != 0)
    {
        return result;
    }

    Status = fileStatus;

    if ((result = vehicleFile->ReadIdString("icon", IconName, 0x13)) != 0)
    {
        return result;
    }

    if (vehicleFile->ReadIdLong("BattleRating", BattleRating) != 0)
    {
        BattleRating = -1;
    }

    if ((result = vehicleFile->SeekBlock("Engine")) != 0)
    {
        return result;
    }

    if ((result = vehicleFile->ReadIdFloat("Tonnage", EngineTonnage)) != 0)
    {
        return result;
    }

    if ((result = vehicleFile->ReadIdULong("Rating", EngineRating)) != 0)
    {
        return result;
    }

    uint8_t moveSpeed = 0;

    if ((result = vehicleFile->ReadIdUChar("MaxMoveSpeed", moveSpeed)) != 0)
    {
        return result;
    }

    MaxRunSpeed = static_cast<float>(moveSpeed);

    if (vehicleFile->SeekBlock("MovementSystem") == 0)
    {
        int32_t value = 0;

        if (vehicleFile->ReadIdLong("CrashAvoidSelf", value) == 0)
        {
            CrashAvoidSelf = value;
        }

        if (vehicleFile->ReadIdLong("CrashAvoidPath", value) == 0)
        {
            CrashAvoidPath = value;
        }

        if (vehicleFile->ReadIdLong("CrashBlockSelf", value) == 0)
        {
            PathLockLevel = value;
        }

        if (vehicleFile->ReadIdLong("CrashBlockPath", value) == 0)
        {
            PathLockRange = value;
        }

        float yieldTime = 0.0f;

        if (vehicleFile->ReadIdFloat("CrashYieldTime", yieldTime) == 0)
        {
            CrashYieldTime = yieldTime;
        }
    }

    if ((result = vehicleFile->SeekBlock("Armor")) != 0)
    {
        return result;
    }

    if ((result = vehicleFile->ReadIdUChar("Type", ArmorType)) != 0)
    {
        return result;
    }

    if ((result = vehicleFile->ReadIdFloat("Tonnage", ArmorTonnage)) != 0)
    {
        return result;
    }

    if ((result = vehicleFile->SeekBlock("InventoryInfo")) != 0)
    {
        return result;
    }

    if ((result = vehicleFile->ReadIdUChar("NumOther", NumOther)) != 0)
    {
        return result;
    }

    if ((result = vehicleFile->ReadIdUChar("NumWeapons", NumWeapons)) != 0)
    {
        return result;
    }

    if ((result = vehicleFile->ReadIdUChar("NumAmmo", NumAmmos)) != 0)
    {
        return result;
    }

    const int32_t firstWeapon = NumOther;
    const int32_t firstAmmo = NumOther + NumWeapons;
    const int32_t numItems = NumOther + NumAmmos + NumWeapons;
    Inventory = std::make_unique<MCInventoryItem[]>(static_cast<size_t>(numItems));

    NumAntiMissileSystems = 0;
    char blockName[32];

    for (int32_t item = 0; item < firstWeapon; item++)
    {
        std::sprintf(blockName, "Item:%d", item);

        if ((result = vehicleFile->SeekBlock(blockName)) != 0)
        {
            return result;
        }

        MCInventoryItem& other = Inventory[item];

        if ((result = vehicleFile->ReadIdUChar("MasterID", other.MasterID)) != 0)
        {
            return result;
        }

        other.Health = MasterComponentList[other.MasterID].Health;
        other.Disabled = 0;
        other.Amount = 1;
        other.AmmoIndex = -1;
        other.ReadyTime = 0.0f;
        other.BodyLocation = 0xff;
        other.RangeRatings = nullptr;

        switch (MasterComponentList[other.MasterID].Form)
        {
            case COMPONENT_FORM_COCKPIT:
                Cockpit = static_cast<uint8_t>(item);
                break;
            case COMPONENT_FORM_SENSOR:
            {
                Sensor = static_cast<uint8_t>(item);
                SensorSystem = SensorSystemManager->NewSensor();
                SensorSystem->Owner = this;
                SensorSystem->SetRange(MasterComponentList[Inventory[item].MasterID].RangeOrHeat);
                break;
            }
            case COMPONENT_FORM_ENGINE:
                Engine = static_cast<uint8_t>(item);
                break;
            case COMPONENT_FORM_LIFESUPPORT:
                LifeSupport = static_cast<uint8_t>(item);
                break;
            case COMPONENT_FORM_ECM:
                Ecm = static_cast<uint8_t>(item);
                break;
            case COMPONENT_FORM_PROBE:
                Probe = static_cast<uint8_t>(item);
                break;
            default:
                break;
        }
    }

    for (int32_t item = firstWeapon; item < firstAmmo; item++)
    {
        std::sprintf(blockName, "Item:%d", item);

        if ((result = vehicleFile->SeekBlock(blockName)) != 0)
        {
            return result;
        }

        MCInventoryItem& weapon = Inventory[item];

        if ((result = vehicleFile->ReadIdUChar("MasterID", weapon.MasterID)) != 0)
        {
            return result;
        }

        if ((result = vehicleFile->ReadIdUChar("FacesForward", weapon.FacesForward)) != 0)
        {
            return result;
        }

        const MCMasterComponent& component = MasterComponentList[weapon.MasterID];
        weapon.Health = component.Health;
        weapon.Disabled = 0;
        weapon.Amount = 1;
        weapon.AmmoIndex = -1;
        weapon.ReadyTime = 0.0f;
        weapon.BodyLocation = 0xff;
        // As BattleMech::init: damage per ten seconds, then scaled by the long range over 24.
        weapon.Effectiveness =
            static_cast<int16_t>(static_cast<int32_t>(component.Damage * 10.0 / component.RecycleTime));
        weapon.Effectiveness = static_cast<int16_t>(static_cast<int32_t>(
            static_cast<double>(component.WeaponRange[3]) * weapon.Effectiveness * static_cast<double>(1.0f / 24.0f)));
        weapon.RangeRatings = new float[NumRangeRatings * 2]();
        ObjectTypeManager->Load(
            static_cast<int32_t>(
                WeaponFXTable[static_cast<int8_t>(MasterComponentList[Inventory[item].MasterID].WeaponEffect)]),
            1);
    }

    for (int32_t item = firstAmmo; item < numItems; item++)
    {
        std::sprintf(blockName, "Item:%d", item);

        if ((result = vehicleFile->SeekBlock(blockName)) != 0)
        {
            return result;
        }

        MCInventoryItem& ammo = Inventory[item];

        if ((result = vehicleFile->ReadIdUChar("MasterID", ammo.MasterID)) != 0)
        {
            return result;
        }

        int32_t amount = 0;

        if (vehicleFile->ReadIdLong("Amount", amount) != 0)
        {
            uint8_t smallAmount = 0;

            if ((result = vehicleFile->ReadIdUChar("Amount", smallAmount)) != 0)
            {
                return result;
            }

            amount = smallAmount;
        }

        if (amount == -1)
        {
            amount = MasterComponentList[ammo.MasterID].LongValue;
        }

        ammo.Amount = static_cast<int16_t>(amount);
        ammo.StartAmount = ammo.Amount;
        ammo.AmmoIndex = -1;
        ammo.Health = MasterComponentList[ammo.MasterID].Health;
        ammo.Disabled = 0;
        ammo.ReadyTime = 0.0f;
        ammo.BodyLocation = 0xff;
        ammo.RangeRatings = nullptr;
    }

    for (int32_t location = 0; location < NUM_GROUNDVEHICLE_LOCATIONS; location++)
    {
        if ((result = vehicleFile->SeekBlock(locationNames[location])) != 0)
        {
            return result;
        }

        MCBodyLocation& bodyLocation = BodyAt(location);
        bodyLocation.HasCase = 0;
        uint8_t internalStructure = 0;

        if ((result = vehicleFile->ReadIdUChar("CurInternalStructure", internalStructure)) != 0)
        {
            return result;
        }

        bodyLocation.CurInternalStructure = static_cast<float>(internalStructure);
        const double structureLeft =
            static_cast<double>(internalStructure) / static_cast<double>(bodyLocation.MaxInternalStructure);

        if (structureLeft == 0.0)
        {
            bodyLocation.DamageState = 2;
        }
        else if (structureLeft > 0.5)
        {
            bodyLocation.DamageState = 0;
        }
        else
        {
            bodyLocation.DamageState = 1;
        }

        if ((result = vehicleFile->ReadIdUChar("MaxArmorPoints", Armor[location].MaxArmor)) != 0)
        {
            return result;
        }

        uint8_t points = 0;

        if ((result = vehicleFile->ReadIdUChar("CurArmorPoints", points)) != 0)
        {
            return result;
        }

        Armor[location].CurArmor = static_cast<float>(points);
        bodyLocation.CriticalSpaces = nullptr;
    }

    CalcAmmoTotals();

    for (int32_t item = firstWeapon; item < firstAmmo; item++)
    {
        for (int32_t ammoType = 0; ammoType < NumAmmoTypes; ammoType++)
        {
            if (static_cast<int32_t>(MasterComponentList[Inventory[item].MasterID].AmmoMasterId) ==
                AmmoTypeTotal[ammoType].MasterId)
            {
                Inventory[item].AmmoIndex = static_cast<int16_t>(ammoType);
                break;
            }
        }
    }

    for (int32_t item = 0; item < firstWeapon; item++)
    {
        const int32_t masterID = Inventory[item].MasterID;

        if (masterID != MasterClanAntiMissileSystemID && masterID != MasterInnerSphereAntiMissileSystemID)
        {
            continue;
        }

        for (int32_t ammoType = 0; ammoType < NumAmmoTypes; ammoType++)
        {
            if (static_cast<int32_t>(MasterComponentList[masterID].AmmoMasterId) == AmmoTypeTotal[ammoType].MasterId)
            {
                Inventory[item].AmmoIndex = static_cast<int16_t>(ammoType);
                break;
            }
        }
    }

    CalcLongestRangeWeapon();
    CalcWeaponEffectiveness(1);
    CalcWeaponEffectiveness(0);
    MaxCV = CalcCV(1);
    CurCV = CalcCV(0);

    if (Refitter != 0)
    {
        // The refit pool lives in the turret's armor slot.
        const uint8_t refitPoints = static_cast<uint8_t>(static_cast<MCGroundVehicleType*>(ObjType)->RefitPoints);
        Armor[GROUNDVEHICLE_LOCATION_TURRET].MaxArmor = refitPoints;
        Armor[GROUNDVEHICLE_LOCATION_TURRET].CurArmor = static_cast<float>(refitPoints);
    }

    return 0;
}

auto MCGroundVehicle::CalcCV(int calcMax) -> int32_t
{
    if (BattleRating != -1)
    {
        return BattleRating;
    }

    // Offense: the weapons' ratings, scaled by the top speed.
    double offense = 0.0;
    const int32_t firstWeapon = NumOther;

    for (int32_t item = firstWeapon; item < firstWeapon + NumWeapons; item++)
    {
        if (calcMax != 0 || Inventory[item].Disabled == 0)
        {
            offense += MasterComponentList[Inventory[item].MasterID].BattleRating;
        }
    }

    offense *= (MaxRunSpeed - 18.0) * 0.05555555555555555 + 1.0;

    // Defense: structure, armor, tonnage, the speed class and the other equipment.
    double defense = 0.0;

    for (int32_t location = 0; location < NUM_GROUNDVEHICLE_LOCATIONS; location++)
    {
        defense += calcMax != 0 ? static_cast<double>(BodyAt(location).MaxInternalStructure)
                                : BodyAt(location).CurInternalStructure;
    }

    for (int32_t location = 0; location < NUM_GROUNDVEHICLE_LOCATIONS; location++)
    {
        defense += calcMax != 0 ? static_cast<double>(Armor[location].MaxArmor) : Armor[location].CurArmor;
    }

    defense += TonnageClass;
    int32_t speedClass = 0;

    while (speedClass < 5 && static_cast<float>(TargetMoveModifierTable[speedClass][0]) < MaxRunSpeed)
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
        if (calcMax != 0 || Inventory[item].Disabled == 0)
        {
            defense += MasterComponentList[Inventory[item].MasterID].BattleRating;
        }
    }

    return static_cast<int32_t>(defense + offense);
}

auto MCGroundVehicle::Destroy() -> void
{
    CrewName.clear();

    if (StatusWindow != nullptr)
    {
        CloseStatusWindow();
        StatusWindow = nullptr;
    }
}

auto MCGroundVehicle::MineCheck() -> void
{
    if (MPlayer != nullptr && MPlayer->IsServer == 0)
    {
        return;
    }

    MCScenarioMap* map = GameMap;

    // The mine state bits of a tile's overlay: Inner Sphere 11..12, Clan 13..14; the spread counts 25..26, 27..28.
    if (MineCellHandled != 0)
    {
        const MCMapTile& tile = map->Map[ObjPosition->TileR * map->Width + ObjPosition->TileC];
        const uint32_t state = Alignment == -1 ? tile.Overlay >> 11 : tile.Overlay >> 13;

        if ((state & 3) == 0)
        {
            MineCellHandled = 0;
            const int32_t tileR = ObjPosition->TileR;
            const int32_t tileC = ObjPosition->TileC;
            MCMapTile& here = map->Map[map->Width * tileR + tileC];

            if (GetAlignment() == -1)
            {
                here.Overlay = (here.Overlay & 0xffffefff) | 0x800;
            }
            else
            {
                here.Overlay = (here.Overlay & 0xffffbfff) | 0x2000;
            }

            if (MPlayer != nullptr)
            {
                MPlayer->AddMineChunk(tileR * 3, tileC * 3, Alignment != -1 ? 1 : 0, 1, 0);
                map = GameMap;
            }
        }
    }

    const uint32_t mine =
        Alignment == -1
            ? map->GetInnerSphereMine(ObjPosition->TileR, ObjPosition->TileC, ObjPosition->CellR, ObjPosition->CellC)
            : map->GetClanMine(ObjPosition->TileR, ObjPosition->TileC, ObjPosition->CellR, ObjPosition->CellC);

    if (mine == 0)
    {
        return;
    }

    int32_t firstRow = ObjPosition->TileR - 1;
    int32_t firstCol = ObjPosition->TileC - 1;

    if (firstRow < 0)
    {
        firstRow = 0;
    }

    if (firstCol < 0)
    {
        firstCol = 0;
    }

    const int32_t mapSide = MCTerrain::VerticesBlockSide * MCTerrain::BlocksMapSide;

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
            const bool inMap = row >= 0 && row < GameMap->Height && col >= 0 && col < GameMap->Width;
            Assert(inMap ? 1 : 0, 0, " Map Tile out of bounds ");

            // Port fix: the original goes on to touch the tile past the map's edge.
            if (!inMap)
            {
                continue;
            }

            MCMapTile& tile = GameMap->Map[GameMap->Width * row + col];
            const bool innerSphere = GetAlignment() == -1;
            uint32_t count = ((innerSphere ? tile.Overlay >> 25 : tile.Overlay >> 27) & 3) + 1;

            if (count > 3)
            {
                count = 3;
            }

            if (GetAlignment() == -1)
            {
                tile.Overlay = (tile.Overlay & 0xf9ffffff) | (count << 25);
            }
            else
            {
                tile.Overlay = (tile.Overlay & 0xe7ffffff) | (count << 27);
            }
        }
    }

    int32_t chunkResult = 0;

    if (MineSweeper != 0)
    {
        // A sweeper sets the mine off harmlessly, at the cost of a point of front armor.
        SweepTime = 0.0f;
        MCVector3D position = GetPosition();
        CreateExplosion(MineExplosion, position, 0.0f, 0.0f);
        Armor[GROUNDVEHICLE_LOCATION_FRONT].CurArmor -= 1.0f;

        if (MPlayer != nullptr)
        {
            MCWeaponShotInfo shotInfo;
            shotInfo.Init(nullptr, -2, 1.0f, 0, 0.0f);
            MPlayer->AddWeaponHitChunk(this, &shotInfo, 0);
        }

        if (Armor[GROUNDVEHICLE_LOCATION_FRONT].CurArmor == 0.0f)
        {
            MineSweeper = 0;
            SweepTime = -1.0f;
            Pilot->ClearCurTacOrder(1, 0);
        }

        chunkResult = 1;
    }
    else
    {
        if (MineLayer != 0)
        {
            MineCellHandled = 1;
            return;
        }

        MCVector3D position = GetPosition();
        CreateExplosion(MineExplosion, position, MineSplashDamage, WorldUnitsPerMeter * MineSplashRange);
        const int32_t hitLocation = CalcHitLocation(nullptr, -1, 3, 0);
        MCWeaponShotInfo shotInfo;
        shotInfo.Init(nullptr, -2, MineBaseDamage, hitLocation, 0.0f);
        HandleWeaponHit(&shotInfo, MPlayer != nullptr);

        if (GetPilot() != nullptr)
        {
            GetPilot()->RadioMessage(0x16, 1);
        }

        Pilot->PausePath();
        chunkResult = 2;
    }

    const int32_t tileR = ObjPosition->TileR;
    const int32_t tileC = ObjPosition->TileC;
    MCMapTile& here = GameMap->Map[GameMap->Width * tileR + tileC];

    if (GetAlignment() == -1)
    {
        here.Overlay |= 0x1800;
    }
    else
    {
        here.Overlay |= 0x6000;
    }

    if (MPlayer != nullptr)
    {
        MPlayer->AddMineChunk(tileR * 3 + ObjPosition->CellR, tileC * 3 + ObjPosition->CellC, Alignment != -1 ? 1 : 0,
                              3, chunkResult);
    }

    MineCellHandled = 1;
}

auto MCGroundVehicle::PivotTo() -> int
{
    MCMechWarrior* warrior = Pilot;
    MCMovePath* path = warrior->GetMovePath();
    const int32_t moveStateGoal = warrior->MoveOrders.MoveStateGoal;
    const int32_t moveState = warrior->MoveOrders.MoveState;
    const int32_t run = MPlayer == nullptr || MPlayer->IsServer != 0 ? warrior->MoveOrders.Run : MoveChunk.Run;
    int hasTarget = 0;
    MCGameObject* target = warrior->GetLastTarget();
    float targetFacing = 0.0f;
    MCVector3D targetPosition;
    auto* dynType = static_cast<MCGroundVehicleDynamicsType*>(static_cast<MCGroundVehicleType*>(ObjType)->DynamicsType);
    const float maxPivot = static_cast<float>(dynType->MaxVehiclePivotRate) * FrameLength;

    if (target == nullptr)
    {
        if (warrior->CurTacOrder.Code == TACTICAL_ORDER_ATTACK_POINT)
        {
            targetPosition = warrior->AttackOrders.TargetPoint;
            targetFacing = RelFacingTo(targetPosition, -1);
            hasTarget = 1;
        }
    }
    else
    {
        targetPosition = target->GetPosition();
        targetFacing = RelFacingTo(targetPosition, -1);
        hasTarget = 1;
    }

    // Starts the pivot: a turn of <paramref name="turn"/> degrees, no faster than the pivot rate.
    const auto pivot = [&](float turn) -> int
    {
        if (maxPivot < std::fabs(turn))
        {
            turn = turn <= 0.0f ? -maxPivot : maxPivot;
        }

        auto* controlData = static_cast<MCGroundVehicleControlData*>(Control->ControlData);
        controlData->Rotate = static_cast<int8_t>(static_cast<int32_t>(turn / maxPivot * 64.0f));
        controlData->Pivot = 1;
        UpdateTurret(turn);
        return 1;
    };

    const auto choosePivotDirection = [&]()
    {
        if (PivotDirection == 0xff)
        {
            PivotDirection = targetFacing >= 0.0f ? 1 : 0;
        }
    };

    const auto hasNextStep = [&]()
    { return path->NumStepsWhenNotPaused >= 1 && path->CurStep < path->NumStepsWhenNotPaused; };

    if (moveState == MOVESTATE_PIVOT_FORWARD)
    {
        if (moveStateGoal == MOVESTATE_PIVOT_FORWARD || moveStateGoal == MOVESTATE_FORWARD)
        {
            if (!hasNextStep())
            {
                Pilot->MoveOrders.MoveStateGoal = MOVESTATE_FORWARD;
            }
            else
            {
                const MCVector3D destination = path->StepList[path->CurStep].Destination;
                static_cast<MCGroundVehicleControlData*>(Control->ControlData)->Throttle = 0;
                const float stepFacing = RelFacingTo(destination, -1);

                if (stepFacing < -45.0f || stepFacing > 45.0f)
                {
                    float turn = -stepFacing;

                    if (hasTarget != 0 && run == 0)
                    {
                        choosePivotDirection();

                        if (PivotDirection == 0)
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

                Pilot->MoveOrders.MoveState = MOVESTATE_FORWARD;

                if (Pilot->MoveOrders.MoveStateGoalChanged != 0)
                {
                    Pilot->MoveOrders.MoveStateGoalChanged = 0;
                }
            }
        }
        else
        {
            Pilot->MoveOrders.MoveState = MOVESTATE_FORWARD;
        }
    }
    else if (moveState == MOVESTATE_PIVOT_REVERSE)
    {
        if (moveStateGoal == MOVESTATE_PIVOT_REVERSE || moveStateGoal == MOVESTATE_REVERSE)
        {
            if (!hasNextStep())
            {
                Pilot->MoveOrders.MoveStateGoal = MOVESTATE_FORWARD;
            }
            else
            {
                const MCVector3D destination = path->StepList[path->CurStep].Destination;
                static_cast<MCGroundVehicleControlData*>(Control->ControlData)->Throttle = 0;
                const float stepFacing = RelFacingTo(destination, -1);

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
                        turnLeft = PivotDirection != 0;
                    }

                    return pivot(turnLeft ? -180.0f - stepFacing : 180.0f - stepFacing);
                }

                MCMechWarrior* orders = Pilot;

                if (orders->MoveOrders.MoveStateGoalChanged != 0)
                {
                    orders->MoveOrders.MoveStateGoalChanged = 0;
                }

                if (moveStateGoal == MOVESTATE_REVERSE)
                {
                    orders->MoveOrders.MoveState = MOVESTATE_REVERSE;
                }
                else
                {
                    orders->MoveOrders.MoveStateGoal = MOVESTATE_FORWARD;
                }
            }
        }
        else
        {
            Pilot->MoveOrders.MoveState = MOVESTATE_FORWARD;
        }
    }
    else if (moveState != MOVESTATE_PIVOT_TARGET)
    {
        if (moveStateGoal == MOVESTATE_PIVOT_TARGET || moveStateGoal == MOVESTATE_PIVOT_FORWARD ||
            moveStateGoal == MOVESTATE_PIVOT_REVERSE)
        {
            Pilot->MoveOrders.MoveState = moveStateGoal;
        }
    }
    else if (moveStateGoal != MOVESTATE_PIVOT_TARGET)
    {
        Pilot->MoveOrders.MoveState = MOVESTATE_FORWARD;
    }
    else if (run == 0 && hasTarget != 0)
    {
        static_cast<MCGroundVehicleControlData*>(Control->ControlData)->Throttle = 0;
        const float facing = RelFacingTo(targetPosition, -1);
        const float fireArc = GetFireArc();

        if (facing < -fireArc || fireArc < facing)
        {
            return pivot(-facing);
        }

        Pilot->MoveOrders.MoveStateGoal = MOVESTATE_FORWARD;
    }
    else
    {
        Pilot->MoveOrders.MoveStateGoal = MOVESTATE_FORWARD;
    }

    MCMechWarrior* orders = Pilot;

    if (!(orders->MoveOrders.YieldTime > -1.0f || orders->MoveOrders.WaitForPointTime > -1.0f))
    {
        orders->ResumePath();
    }

    PivotDirection = 0xff;
    return 0;
}

auto MCGroundVehicle::CalcThrottleLimits(int32_t& minThrottle, int32_t& maxThrottle) -> void
{
    const MCMapTile& tile = GameMap->Map[ObjPosition->TileR * GameMap->Width + ObjPosition->TileC];
    const float tileFactor = TileThrottleMultiplier[Chassis][tile.Cells & 0x7f];
    const float overlayFactor = OverlayThrottleMultiplier[Chassis][tile.Overlay & 0x7f];
    // Each limit goes through a short, as in the original.
    maxThrottle = static_cast<int16_t>(static_cast<int32_t>(std::floor(static_cast<float>(maxThrottle) * tileFactor)));
    minThrottle = static_cast<int16_t>(static_cast<int32_t>(std::floor(static_cast<float>(minThrottle) * tileFactor)));
    maxThrottle =
        static_cast<int16_t>(static_cast<int32_t>(std::floor(static_cast<float>(maxThrottle) * overlayFactor)));
    minThrottle =
        static_cast<int16_t>(static_cast<int32_t>(std::floor(static_cast<float>(minThrottle) * overlayFactor)));
}

auto MCGroundVehicle::GetSpeedState() -> int32_t
{
    return GetBodyState() == 1 ? 2 : 0;
}

auto MCGroundVehicle::UpdateMoveStateGoal() -> void
{
    MCMechWarrior* warrior = Pilot;
    MCMovePath* path = warrior->GetMovePath();
    const int32_t moveStateGoal = warrior->MoveOrders.MoveStateGoal;

    if (path->NumSteps < 1)
    {
        if (moveStateGoal != MOVESTATE_PIVOT_TARGET && moveStateGoal != MOVESTATE_PIVOT_FORWARD &&
            moveStateGoal != MOVESTATE_PIVOT_REVERSE)
        {
            warrior->MoveOrders.MoveStateGoal = MOVESTATE_FORWARD;
        }

        return;
    }

    const int32_t run = MPlayer == nullptr || MPlayer->IsServer != 0 ? warrior->MoveOrders.Run : MoveChunk.Run;

    if (run != 0)
    {
        warrior->MoveOrders.MoveStateGoal = MOVESTATE_FORWARD;
        return;
    }

    MCVector3D targetPosition;
    MCGameObject* target = warrior->GetLastTarget();

    if (target == nullptr)
    {
        if (warrior->CurTacOrder.Code != TACTICAL_ORDER_ATTACK_POINT)
        {
            warrior->MoveOrders.MoveStateGoal = MOVESTATE_FORWARD;
            return;
        }

        targetPosition = warrior->AttackOrders.TargetPoint;
    }
    else
    {
        targetPosition = target->GetPosition();
    }

    if (path->NumStepsWhenNotPaused <= 0 || path->CurStep >= path->NumStepsWhenNotPaused)
    {
        return;
    }

    const double delta = RelFacingDelta(path->StepList[path->CurStep].Destination, targetPosition);
    MCMechWarrior* orders = Pilot;
    const double turretArc =
        static_cast<MCGroundVehicleDynamicsType*>(static_cast<MCGroundVehicleType*>(ObjType)->DynamicsType)
            ->MaxTurretYaw;

    if (orders->MoveOrders.MoveStateGoal == MOVESTATE_FORWARD)
    {
        // The target is behind: drive backward.
        if (turretArc < delta && 180.0 - delta <= turretArc && orders->MoveOrders.MoveStateGoalChanged == 0)
        {
            orders->MoveOrders.MoveStateGoalChanged = 1;
            orders->MoveOrders.MoveStateGoal = MOVESTATE_REVERSE;
        }
    }
    else if (turretArc < 180.0 - delta && delta <= turretArc && orders->MoveOrders.MoveStateGoalChanged == 0)
    {
        orders->MoveOrders.MoveStateGoalChanged = 1;
        orders->MoveOrders.MoveStateGoal = MOVESTATE_FORWARD;
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
    int SteerAlongPath(MCGroundVehicle* vehicle, MCMovePath* path, char& newRotate, char& newThrottleSetting,
                       float& newRotatePerSec, int32_t& newMoveState, int32_t& maxThrottle)
    {
        auto* dynType = static_cast<MCGroundVehicleDynamicsType*>(
            static_cast<MCGroundVehicleType*>(vehicle->ObjType)->DynamicsType);
        int result = 0;
        const auto steer = [&]()
        {
            if (path->NumSteps < 1)
            {
                newThrottleSetting = 0;
                return;
            }

            int32_t step = path->CurStep;

            if (step == path->NumSteps)
            {
                result = 1;
                return;
            }

            MCVector3D destination = path->StepList[step].Destination;
            vehicle->LastValidPosition = destination;
            const auto distance = static_cast<float>(vehicle->DistanceFrom(destination));
            const int32_t numSteps = path->NumSteps;
            const float margin = step == numSteps - 1 ? MoveMarginOfError[1] : MoveMarginOfError[0];
            MaxVelocityMag = WorldUnitsPerMeter * 100.0f;

            if (distance < margin)
            {
                // Reached the step: on to the next.
                step++;
                vehicle->Pilot->MoveOrders.TimeOfLastStep = ScenarioTime;
                path->CurStep = step;

                if (numSteps <= step)
                {
                    MaxVelocityMag = WorldUnitsPerMeter * distance;
                    result = 1;
                    return;
                }

                destination = path->StepList[step].Destination;
            }

            const float facing = vehicle->RelFacingTo(destination, -1);
            MCMechWarrior* orders = vehicle->Pilot;
            const int32_t moveState = orders->MoveOrders.MoveState;
            const int32_t moveStateGoal = orders->MoveOrders.MoveStateGoal;
            const float maxTurn = static_cast<float>(dynType->MaxVehicleYawRate) * FrameLength;

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

                orders->PausePath();

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

                orders->PausePath();

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
                orders->PausePath();
                newMoveState = MOVESTATE_PIVOT_FORWARD;
            }
            else if (moveStateGoal == MOVESTATE_REVERSE || moveStateGoal == MOVESTATE_PIVOT_REVERSE)
            {
                orders->PausePath();
                newMoveState = MOVESTATE_PIVOT_REVERSE;
            }
        };

        steer();

        if (vehicle->MineSweeper != 0 && vehicle->SweepTime > 0.0f && vehicle->SweepTime < GvSweepTime)
        {
            maxThrottle = MineSweepThrottle;
        }

        if (vehicle->MineLayer != 0 && vehicle->Pilot->CurTacOrder.MoveParams.Mode == 1)
        {
            maxThrottle = MineLayThrottle;
        }

        return result;
    }
}

auto MCGroundVehicle::UpdateMovePath(char& newRotate, char& newThrottleSetting, float& newRotatePerSec,
                                     int32_t& newMoveState, int32_t& minThrottle, int32_t& maxThrottle) -> int
{
    MCMechWarrior* warrior = Pilot;
    auto* controlData = static_cast<MCGroundVehicleControlData*>(Control->ControlData);
    MCMovePath* path = warrior->GetMovePath();
    newThrottleSetting = static_cast<char>(controlData->Throttle);
    const int32_t running = warrior->MoveOrders.Run;
    newRotatePerSec = 0.0f;
    UpdateHustleTime();
    const bool hustling = ScenarioTime < LastHustleTime + 2.0f;
    warrior = Pilot;
    MCMover* point = warrior->GetPoint();
    const bool groupMove = warrior->CurTacOrder.IsGroupOrder() != 0 && warrior->CurTacOrder.IsMoveOrder() != 0;

    if (running == 0 && !hustling && point != nullptr && point->IsDisabled() == 0 && point != this && groupMove)
    {
        // Keep pace with the group's point: wait (at most five seconds while moving) when ahead of it.
        MCMechWarrior* pointPilot = point->GetPilot();
        pointPilot->GetMovePath();
        const float pointDistanceLeft = pointPilot->GetMoveDistanceLeft();

        if (pointDistanceLeft <= warrior->GetMoveDistanceLeft())
        {
            warrior->MoveOrders.WaitForPointTime = -1.0f;

            if (warrior->MoveOrders.YieldTime <= -1.0f)
            {
                warrior->ResumePath();
            }
        }
        else
        {
            const int32_t speedState = GetSpeedState();
            warrior = Pilot;

            if (speedState == 2)
            {
                if (warrior->MoveOrders.WaitForPointTime <= -1.0f)
                {
                    warrior->MoveOrders.WaitForPointTime = ScenarioTime + 5.0f;
                }
            }
            else if (warrior->MoveOrders.WaitForPointTime < ScenarioTime)
            {
                warrior->PausePath();
                warrior->MoveOrders.WaitForPointTime = 999999.0f;
            }
        }
    }
    else
    {
        warrior->MoveOrders.WaitForPointTime = -1.0f;
    }

    int result = SteerAlongPath(this, path, newRotate, newThrottleSetting, newRotatePerSec, newMoveState, maxThrottle);
    warrior = Pilot;

    if (result != 0)
    {
        if (warrior->MoveOrders.PathType == 2 &&
            warrior->MoveOrders.Path[0]->GlobalStep < warrior->MoveOrders.NumGlobalSteps - 1)
        {
            result = 0;
        }

        if (warrior->MoveOrders.Path[0] != nullptr)
        {
            warrior->MoveOrders.Path[0]->Clear();
        }

        newThrottleSetting = 0;
    }

    return result;
}

auto MCGroundVehicle::SetNextMovePath(char& newThrottleSetting) -> void
{
    MCMechWarrior* warrior = Pilot;

    if (warrior->PlayerOrderFromQueue != 0 && warrior->CurTacOrder.IsMoveOrder() != 0)
    {
        if (warrior->MoveOrders.Path[0] != nullptr)
        {
            warrior->MoveOrders.Path[0]->Clear();
        }

        return;
    }

    warrior->ClearMoveOrders();
    newThrottleSetting = 0;
}

auto MCGroundVehicle::SetControlSettings(char& newRotate, char& newThrottleSetting, float& newRotatePerSec,
                                         int32_t& minThrottle, int32_t& maxThrottle) -> void
{
    MCMechWarrior* warrior = Pilot;
    MCMovePath* path = warrior->GetMovePath();
    const int32_t run = MPlayer == nullptr || MPlayer->IsServer != 0 ? warrior->MoveOrders.Run : MoveChunk.Run;

    if (path->NumSteps == 0)
    {
        newThrottleSetting = 0;
    }

    auto* controlData = static_cast<MCGroundVehicleControlData*>(Control->ControlData);

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

        controlData->Throttle = newThrottleSetting;
    }

    if (newRotate != 0)
    {
        controlData->Rotate = newRotate;
    }

    // Anything but a run order moves at the walk speed.
    controlData->Walk = run == 0 ? 1 : 0;
}

auto MCGroundVehicle::UpdateTurret(float newRotatePerSec) -> void
{
    MCMechWarrior* warrior = Pilot;
    MCGameObject* target = warrior->GetLastTarget();
    double facing;

    if (target != nullptr)
    {
        facing = static_cast<double>(RelFacingTo(target->GetPosition(), -1)) + TurretRotation + newRotatePerSec;
    }
    else if (warrior->CurTacOrder.Code == TACTICAL_ORDER_ATTACK_POINT)
    {
        facing =
            static_cast<double>(RelFacingTo(warrior->GetAttackTargetPoint(), -1)) + TurretRotation + newRotatePerSec;
    }
    else
    {
        facing = TurretRotation;
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
    auto* dynType = static_cast<MCGroundVehicleDynamicsType*>(static_cast<MCGroundVehicleType*>(ObjType)->DynamicsType);
    const float maxTurn = static_cast<float>(dynType->MaxTurretYawRate) * FrameLength;

    if (maxTurn < std::fabs(turn))
    {
        turn = turn < 0.0 ? -maxTurn : maxTurn;
    }

    static_cast<MCGroundVehicleControlData*>(Control->ControlData)->TurretRotate =
        static_cast<int8_t>(static_cast<int32_t>(turn / maxTurn * 64.0f));
}

auto MCGroundVehicle::UpdateMovement() -> void
{
    auto* controlData = static_cast<MCGroundVehicleControlData*>(Control->ControlData);

    if (DisableThisFrame != 0)
    {
        DisableThisFrame = 0;
        ShutDownThisFrame = 0;
        StartUpThisFrame = 0;
        Status = 1;
        controlData->Throttle = 0;
        return;
    }

    if (ShutDownThisFrame != 0)
    {
        controlData->Throttle = 0;
        ShutDownThisFrame = 0;
        StartUpThisFrame = 0;
        Status = 5;
        return;
    }

    if (StartUpThisFrame != 0)
    {
        controlData->Throttle = 100;
        StartUpThisFrame = 0;
        Status = 0;
        return;
    }

    if (IsCaptured() != 0 || IsDisabled() != 0)
    {
        controlData->Throttle = 0;
        return;
    }

    if (EngineBlowTime > -1.0f)
    {
        return;
    }

    float newRotatePerSec = static_cast<float>(PivotTo());

    if (newRotatePerSec != 0.0f)
    {
        return;
    }

    int32_t minThrottle = -100;
    int32_t maxThrottle = 100;
    char newRotate = 0;
    char newThrottleSetting = 0;
    int32_t newMoveState = -1;
    CalcThrottleLimits(minThrottle, maxThrottle);
    UpdateMoveStateGoal();

    if (UpdateMovePath(newRotate, newThrottleSetting, newRotatePerSec, newMoveState, minThrottle, maxThrottle) != 0)
    {
        SetNextMovePath(newThrottleSetting);
    }

    if (newMoveState != -1)
    {
        Pilot->MoveOrders.MoveState = newMoveState;
    }

    SetControlSettings(newRotate, newThrottleSetting, newRotatePerSec, minThrottle, maxThrottle);
    UpdateTurret(newRotatePerSec);
}

auto MCGroundVehicle::NetUpdateMovePath(char& newRotate, char& newThrottleSetting, float& newRotatePerSec,
                                        int32_t& newMoveState, int32_t& minThrottle, int32_t& maxThrottle) -> int
{
    auto* controlData = static_cast<MCGroundVehicleControlData*>(Control->ControlData);
    MCMovePath* path = Pilot->GetMovePath();
    newRotatePerSec = 0.0f;
    newThrottleSetting = static_cast<char>(controlData->Throttle);
    return SteerAlongPath(this, path, newRotate, newThrottleSetting, newRotatePerSec, newMoveState, maxThrottle);
}

auto MCGroundVehicle::NetUpdateMovement() -> void
{
    auto* controlData = static_cast<MCGroundVehicleControlData*>(Control->ControlData);
    MCMovePath* path = Pilot->GetMovePath();
    const auto distance = static_cast<float>(DistanceFrom(path->StepList[path->CurStep].Destination));

    if (path->CurStep == path->NumSteps - 1 && distance < MoveMarginOfError[1])
    {
        // At the end of the path: stop once the server says the vehicle has.
        GetBodyState();

        if (StatusChunk.BodyState == 0)
        {
            Pilot->ClearMoveOrders();
            controlData->Throttle = 0;
        }
    }

    if (DisableThisFrame != 0)
    {
        DisableThisFrame = 0;
        ShutDownThisFrame = 0;
        StartUpThisFrame = 0;
        Status = 1;
        controlData->Throttle = 0;
        return;
    }

    if (ShutDownThisFrame != 0)
    {
        controlData->Throttle = 0;
        ShutDownThisFrame = 0;
        StartUpThisFrame = 0;
        Status = 5;
        return;
    }

    if (StartUpThisFrame != 0)
    {
        controlData->Throttle = 100;
        StartUpThisFrame = 0;
        Status = 0;
        return;
    }

    if (IsCaptured() != 0 || IsDisabled() != 0)
    {
        controlData->Throttle = 0;
        return;
    }

    if (EngineBlowTime > -1.0f)
    {
        return;
    }

    float newRotatePerSec = static_cast<float>(PivotTo());

    if (newRotatePerSec != 0.0f)
    {
        return;
    }

    int32_t minThrottle = -100;
    int32_t maxThrottle = 100;
    char newRotate = 0;
    char newThrottleSetting = 0;
    int32_t newMoveState = -1;
    CalcThrottleLimits(minThrottle, maxThrottle);
    UpdateMoveStateGoal();
    NetUpdateMovePath(newRotate, newThrottleSetting, newRotatePerSec, newMoveState, minThrottle, maxThrottle);

    if (newMoveState != -1)
    {
        Pilot->MoveOrders.MoveState = newMoveState;
    }

    SetControlSettings(newRotate, newThrottleSetting, newRotatePerSec, minThrottle, maxThrottle);
    UpdateTurret(newRotatePerSec);
}

auto MCGroundVehicle::GetPositionFromHS(uint32_t hotSpot) -> MCVector3D
{
    return Position;
}

auto MCGroundVehicle::OnScreen() -> int
{
    MCCamera* camera = CameraList->FindCameraFromIDNumber(1);
    ScreenPos.Y = 0.0f;
    ScreenPos.X = 0.0f;

    if (camera == nullptr || camera->Active == 0)
    {
        return 0;
    }

    float screenY;

    if (UseOldProject == 0)
    {
        MCVector2D screen100;
        MCVector2D screen50;

        if (Land != nullptr)
        {
            Land->ProjectTerrain(Position, screen100, screen50);
        }

        if (camera->CameraScale == 1)
        {
            ScreenPos.X = (screen50.X - camera->ScreenUL50.X) + camera->HalfWidth;
            screenY = screen50.Y - camera->ScreenUL50.Y;
        }
        else
        {
            ScreenPos.X = (screen100.X - camera->ScreenUL.X) + camera->HalfWidth;
            screenY = screen100.Y - camera->ScreenUL.Y;
        }

        ScreenPos.Y = screenY + camera->HalfHeight;
    }
    else
    {
        const float scale = camera->CameraScale != 1 ? 1.0f : 0.5f;
        MCVector3D relative(Position.X - camera->Position.X, Position.Y - camera->Position.Y,
                            Position.Z - camera->Position.Z);
        relative *= scale;
        ScreenPos.X = relative.Y * camera->CosAngle + relative.X * camera->CosAngle + camera->HalfWidth;
        screenY = ((relative.X * camera->SinAngle + camera->HalfHeight) - relative.Y * camera->SinAngle) - relative.Z;
        ScreenPos.Y = screenY;

        if (ScreenPos.X < 0.0f || screenY < 0.0f || camera->ViewWidth < ScreenPos.X || camera->ViewHeight < screenY)
        {
            WindowsVisible = 0;
        }
        else
        {
            WindowsVisible = 1;
        }
    }

    if (Appearance != nullptr && Appearance->RecalcBounds(camera) != 0)
    {
        WindowsVisible = Turn;
        return 1;
    }

    return 0;
}

auto MCGroundVehicle::Disable(uint32_t cause) -> void
{
    MCMover::Disable(cause);
    DeathTimer = 0.0f;
    Smoke = static_cast<MCSmoke*>(CreateObject(0x1c2));

    if (Smoke != nullptr)
    {
        Smoke->SetOwner(this);
        Smoke->SetOwnerPosition(Position);
    }
}

auto MCGroundVehicle::CrashAvoidanceSystem() -> int
{
    if (MPlayer != nullptr && MPlayer->IsServer == 0)
    {
        return 0;
    }

    MCMechWarrior* warrior = Pilot;
    MCMovePath* path = warrior->GetMovePath();

    if (path->NumStepsWhenNotPaused == 0)
    {
        return 0;
    }

    if (static_cast<double>(warrior->MoveOrders.WaitForPointTime) > 999990.0)
    {
        return 0;
    }

    // A look a frame ahead along the frame turned by a quarter pi (its result is unused).
    const float speed = -Dynamics->GetVelocity();
    MCFrameOfRef ahead = Frame;
    RotateAboutK(ahead, static_cast<float>(std::sin(HALF_PI / 2.0)), static_cast<float>(std::cos(HALF_PI / 2.0)));
    MCVector3D lookAhead(ahead.J.X * speed * FrameLength * WorldUnitsPerMeter + Position.X,
                         ahead.J.Y * speed * FrameLength * WorldUnitsPerMeter + Position.Y,
                         ahead.J.Z * speed * FrameLength * WorldUnitsPerMeter + Position.Z);
    int32_t tileR;
    int32_t tileC;
    int32_t cellR;
    int32_t cellC;
    GameMap->WorldToMapPos(lookAhead, tileR, tileC, cellR, cellC);

    int cornerBlocked = 0;
    const int32_t direction = static_cast<int8_t>(path->StepList[path->CurStep].Direction);

    if (direction == 1 || direction == 3 || direction == 5 || direction == 7)
    {
        // A diagonal step: blocked when both cells beside it are locked.
        const int first = GetAdjacentCellPathLocked(ObjPosition->TileR, ObjPosition->TileC, ObjPosition->CellR,
                                                    ObjPosition->CellC, AdjClippedCell[direction][0]);
        const int second = GetAdjacentCellPathLocked(ObjPosition->TileR, ObjPosition->TileC, ObjPosition->CellR,
                                                     ObjPosition->CellC, AdjClippedCell[direction][1]);
        cornerBlocked = first != 0 && second != 0 ? 1 : 0;
    }

    int lockReachedEnd = 0;
    int blockReachedEnd = 0;
    const int locked = GetPathRangeLock(CrashAvoidPath, &lockReachedEnd);
    const int blocked = GetPathRangeBlocked(CrashAvoidPath, &blockReachedEnd);
    const int32_t closedGates = path->CrossesClosedGate(-1, 2);
    warrior = Pilot;
    const bool clear = locked == 0 && blocked == 0 && cornerBlocked == 0 && closedGates < 2;

    if (warrior->MoveOrders.YieldTime > -1.0f)
    {
        // Yielding: go on once the way is clear.
        if (clear)
        {
            warrior->ResumePath();
            warrior->MoveOrders.YieldTime = -1.0f;
            return 0;
        }

        warrior->PausePath();
        return 1;
    }

    if (clear)
    {
        return 0;
    }

    if (lockReachedEnd == 0 && blockReachedEnd == 0)
    {
        warrior->PausePath();
        warrior->MoveOrders.YieldTime = ScenarioTime + CrashYieldTime;
        Control->ControlData->Brake();
        return 1;
    }

    warrior->ReachedPathEnd();
    Control->ControlData->Brake();
    return 1;
}

auto MCGroundVehicle::CreateVehiclePilot() -> void
{
    auto* marine = static_cast<MCMover*>(CreateObject(DefaultPilotId));
    VehiclePilot = marine;

    if (marine == nullptr)
    {
        Fatal(-1, " Couldnt create Marine for vehicle ");
    }

    marine->SetAwake(1);
    std::string profileName;
    profileName = GamePath(ProfilePath, MarineProfileName, ".fit");
    MCFitIniFile profileFile;
    const int32_t result = profileFile.Open(profileName);

    if (result != 0)
    {
        Fatal(result, " Unable to open Vehicle Marine Profile ");
    }

    if (marine->Init(&profileFile) != 0)
    {
        Fatal(-1, " Bad Vehicle Marine Profile File ");
    }

    profileFile.Close();

    // The vehicle's pilot bails out as the marine.
    MCMechWarrior* warrior = Pilot;
    marine->SetPilot(warrior);
    warrior->SetVehicle(marine);
    warrior->Lobotomy();
    marine->SetControl(2, 3, -1);
    marine->SetTeam(GetTeam());
    VehiclePilot->SetPosition(Position);
    VehiclePilot->SetLastValidPosition(Position);
    VehiclePilot->SetFrame(Frame);
    auto* marineAppearance = static_cast<MCElementalActor*>(VehiclePilot->GetAppearance());

    if (marineAppearance != nullptr)
    {
        marineAppearance->SetGesture(0);
        marineAppearance->FadeTableIndex = GetAlignment() == -1 ? 0x1d : 0x20;
    }

    MCMover* newMarine = VehiclePilot;
    newMarine->IdNumber = IdNumber + 1000;
    newMarine->SetPartId(0xfff - NumMarines++);
    newMarine->SetAlignment(GetAlignment());
    MCObjectQueueNode* list = GetAlignment() == -1 ? ClanMechList : InnerSphereMechList;

    if (list != nullptr)
    {
        list->AddNode(marine);
    }

    marine->SetExists(1);
    marine->SetPotentialContact(0);
    GameObjectMap->AddObject(marine);
    warrior = Pilot;
    warrior->ClearAttackOrders();
    warrior->ClearMoveOrders();
    warrior->OrderMoveToPoint(0, 1, 0, MCVector3D(0.0f, 0.0f, 0.0f), -1, 1);
}

namespace
{
    /// <summary>Moves the smoke along with the vehicle and runs it; frees it once the smoke time is 30 s past.</summary>
    void UpdateSmoke(MCGroundVehicle* vehicle)
    {
        MCSmoke* smoke = vehicle->Smoke;
        smoke->SetOwner(vehicle);
        smoke->SetOwnerPosition(vehicle->Position);
        smoke->SetOwnerVelocity(vehicle->Velocity);
        smoke->Update();
        vehicle->DeathTimer -= FrameLength;

        if (vehicle->DeathTimer > -30.0)
        {
            return;
        }

        delete smoke;
        vehicle->Smoke = nullptr;
    }
}

auto MCGroundVehicle::Update() -> int32_t
{
    if (Withdrawing != 0 && Pilot->Status == 2)
    {
        CollisionsOn = 0;
        return 1;
    }

    TerrainNormal = Land->GetTerrainNormal(Position);
    UpdatePathLock(0);

    if (PotentialContact != nullptr)
    {
        if (Team->Id == 1)
        {
            PotentialContact->UpdateStatus(InnerSphereTeam);
            PotentialContact->UpdateStatus(AlliedTeam);
        }
        else if (Team->Id == 0)
        {
            PotentialContact->UpdateStatus(ClanTeam);
            PotentialContact->UpdateStatus(AlliedTeam);
        }
        else
        {
            PotentialContact->UpdateStatus(InnerSphereTeam);
            PotentialContact->UpdateStatus(ClanTeam);
        }
    }

    if (DeselectTime != 0.0f && DeselectTime < ScenarioTime)
    {
        DeselectTime = 0.0f;
        Selected = 0;
    }

    if (IsDestroyed() != 0 && DeathTimer < 0.0)
    {
        // The wreck: only its appearance and smoke go on.
        if (Appearance != nullptr)
        {
            Appearance->Visible = OnScreen();
            Appearance->Update();
        }

        if (Smoke == nullptr)
        {
            return 1;
        }

        UpdateSmoke(this);
        return 1;
    }

    if (IsDestroyed() == 0 || DeathTimer < 0.0)
    {
        if (GetAwake() == 0 || IsDisabled() != 0 || DistanceSinceMarkSeen < MCTerrain::MetersPerVertex)
        {
            if (IsDisabled() != 0 && DeathTimer != 0.0f && Smoke != nullptr)
            {
                UpdateSmoke(this);
            }
        }
        else
        {
            // Every vertex travelled, the vehicle marks what it sees.
            if (Alignment == 1)
            {
                Land->MarkSeen(Position, Frame.J, 360.0f, GetProbeEffect() + Scenario->MaxVisualRange, 1);
            }
            else if (Alignment == -1)
            {
                Land->MarkSeen(Position, Frame.J, 360.0f, GetProbeEffect() + Scenario->MaxVisualRange, 2);
            }

            DistanceSinceMarkSeen = 0.0f;
        }
    }
    else
    {
        // Just destroyed: when the death timer runs out, it blows up and its crew bails out.
        DeathTimer -= FrameLength;

        if (DeathTimer < 0.0)
        {
            if (GvAppearance == 0)
            {
                static_cast<MCPUAppearance*>(Appearance)->SetDestroyed();
            }
            else
            {
                static_cast<MCGVAppearance*>(Appearance)->SetTypeId(GV_ACTOR_STATE_DESTROYED);
            }

            if (Appearance != nullptr)
            {
                Appearance->Visible = OnScreen();
                Appearance->Update();
            }

            auto* vehicleType = static_cast<MCGroundVehicleType*>(ObjType);
            vehicleType->CreateExplosion(Position, vehicleType->ExplDmg, vehicleType->ExplRad);
            DeathExplosionDone = 1;
            Smoke = static_cast<MCSmoke*>(CreateObject(0x1c2));
            CollisionsOn = 0;

            if (MPlayer != nullptr)
            {
                return 1;
            }

            if (GetAwake() == 0)
            {
                return 1;
            }

            CreateVehiclePilot();
            return 1;
        }
    }

    int32_t result = Control->Update();

    if (result != 1)
    {
        return result;
    }

    result = Dynamics->Update();

    if (result != 1)
    {
        return result;
    }

    int avoiding = 0;

    if (IsDisabled() == 0)
    {
        avoiding = CrashAvoidanceSystem();
    }

    float speed = 0.0f;

    if (avoiding == 0)
    {
        speed = Dynamics->GetVelocity();
    }

    if (GvAppearance != 0)
    {
        auto* vehicleAppearance = static_cast<MCGVAppearance*>(Appearance);

        if (speed != 0.0)
        {
            vehicleAppearance->SetTypeId(GV_ACTOR_STATE_DAMAGED);
        }
        else if (Refitting == 0)
        {
            vehicleAppearance->SetTypeId(GV_ACTOR_STATE_NORMAL);
        }
        else
        {
            vehicleAppearance->SetTypeId(GV_ACTOR_STATE_EXTRA);
        }
    }

    int visibleNow = 0;

    if (Appearance != nullptr)
    {
        Appearance->Update();
        visibleNow = OnScreen();
        Appearance->Visible = visibleNow;

        if (GvAppearance == 0)
        {
            // A pop-up turret opens for a target; its weapons work once it is up.
            const int combat =
                Pilot->GetLastTarget() != nullptr || Pilot->CurTacOrder.Code == TACTICAL_ORDER_ATTACK_POINT ? 1 : 0;
            WeaponsDeployed = static_cast<MCPUAppearance*>(Appearance)->SetCombatMode(combat) == 2 ? 1 : 0;
        }
    }

    if (Withdrawing != 0 && visibleNow == 0 && Pilot->Status != 2)
    {
        ObjType->HandleDestruction(this, nullptr);
    }

    // Slopes slow the vehicle: by the hill factor times the cosine of the angle between its heading and the
    // terrain's normal.
    MCFrameOfRef turned = Frame;
    speed = -speed;
    MCVector3D normal = Land->GetTerrainNormal(Position);
    MCVector3D heading = Frame.J;
    const double headingLength =
        std::sqrt(static_cast<double>(heading.X) * heading.X + static_cast<double>(heading.Y) * heading.Y +
                  static_cast<double>(heading.Z) * heading.Z);

    if (headingLength != 0.0)
    {
        heading.X = static_cast<float>(heading.X / headingLength);
        heading.Y = static_cast<float>(heading.Y / headingLength);
        heading.Z = static_cast<float>(heading.Z / headingLength);
    }

    const double normalLength =
        std::sqrt(static_cast<double>(normal.X) * normal.X + static_cast<double>(normal.Y) * normal.Y +
                  static_cast<double>(normal.Z) * normal.Z);

    if (normalLength != 0.0)
    {
        normal.X = static_cast<float>(normal.X / normalLength);
        normal.Y = static_cast<float>(normal.Y / normalLength);
        normal.Z = static_cast<float>(normal.Z / normalLength);
    }

    const double headingDotNormal = static_cast<double>(heading.Z) * normal.Z +
                                    static_cast<double>(heading.Y) * normal.Y +
                                    static_cast<double>(heading.X) * normal.X;
    const double slope = AcosMatherr(headingDotNormal) * 57.2957795132;

    if (slope != 90.0)
    {
        speed = static_cast<float>(speed - std::cos(slope * DEGREES_TO_RADIANS) * GvHillSpeedFactor * speed);
    }

    RotateAboutK(turned, static_cast<float>(std::sin(HALF_PI / 2.0)), static_cast<float>(std::cos(HALF_PI / 2.0)));
    Velocity.Y = turned.J.Y * speed;
    Velocity.X = turned.J.X * speed;
    Velocity.Z = turned.J.Z * speed;
    MCVector3D move;
    move.X = static_cast<float>(static_cast<double>(Velocity.X) * FrameLength * WorldUnitsPerMeter);
    move.Y = Velocity.Y * FrameLength * WorldUnitsPerMeter;
    move.Z = Velocity.Z * FrameLength * WorldUnitsPerMeter;

    if (NewMoveChunk != 0)
    {
        // A new move chunk: warp to its first step when too far off.
        if (StatusChunk.JumpOrder == 0)
        {
            const int32_t tileR = MoveChunk.StepPos[0][0];
            const int32_t tileC = MoveChunk.StepPos[0][1];
            MCVector3D stepPos;
            MapTileCellToWorldPos(tileR, tileC, MoveChunk.StepPos[0][2], MoveChunk.StepPos[0][3], stepPos);
            // Original behaviour (OB-006): z is measured against 0, not the vehicle's elevation.
            const float dx = Position.X - stepPos.X;
            const float dz = -stepPos.Z;
            const float dy = Position.Y - stepPos.Y;

            if (WarpFactor < std::sqrt(dz * dz + dy * dy + dx * dx))
            {
                move.X = stepPos.X - Position.X;
                move.Y = stepPos.Y - Position.Y;
                move.Z = stepPos.Z;
            }

            if (tileR < 0 || GameMap->Height <= tileR || tileC < 0 || GameMap->Width <= tileC)
            {
                Fatal(0, " gvehicl.update: newMoveChunk stepPos not on map! ");
            }
        }

        NewMoveChunk = 0;
    }

    DistanceSinceMarkSeen =
        static_cast<float>(std::sqrt(static_cast<double>(move.X) * move.X + static_cast<double>(move.Y) * move.Y +
                                     static_cast<double>(move.Z) * move.Z) +
                           DistanceSinceMarkSeen);
    MCVector3D newPosition;
    newPosition.X = move.X + Position.X;
    newPosition.Y = move.Y + Position.Y;
    newPosition.Z = move.Z + Position.Z;
    SetPosition(newPosition);

    if (IsDisabled() == 0)
    {
        UpdatePathLock(1);
    }

    SweepTime = FrameLength + SweepTime;
    MineCheck();

    // A mine layer lays one per tile: at the cell in the middle, or once it has waited MineWaitTime.
    if ((MPlayer == nullptr || MPlayer->IsServer != 0) && MineLayer != 0 && Pilot->CurTacOrder.MoveParams.Mode == 1 &&
        (GetObjPosition()->TileC != CellColToMine || GetObjPosition()->TileR != CellRowToMine))
    {
        MineLayTime = FrameLength + MineLayTime;

        if ((GetObjPosition()->CellC == 1 && GetObjPosition()->CellR == 1) || MineWaitTime < MineLayTime)
        {
            CellColToMine = GetObjPosition()->TileC;
            const int32_t tileR = GetObjPosition()->TileR;
            const int32_t tileC = CellColToMine;
            CellRowToMine = tileR;
            MineLayTime = 0.0f;
            MCMapTile& tile = GameMap->Map[GameMap->Width * tileR + tileC];

            if (Alignment == -1)
            {
                tile.Overlay = (tile.Overlay & 0xffffdfff) | 0x4000;
            }
            else
            {
                tile.Overlay = (tile.Overlay & 0xfffff7ff) | 0x1000;
            }

            if (MPlayer != nullptr)
            {
                MPlayer->AddMineChunk(tileR * 3, tileC * 3, Alignment == -1 ? 1 : 0, 2, 0);
            }
        }
    }

    Position.Z = Land->GetTerrainElevation(Position);
    // Original behaviour (OB-005): adds the map's top edge to y here rather than subtracting.
    const float blockSize = static_cast<float>(MCTerrain::VerticesBlockSide) * MCTerrain::MetersPerVertex;
    const float blockColumn = (Position.X - MCTerrain::MapTopLeft3d100.X) / blockSize;
    const auto blockRow =
        static_cast<int32_t>(std::floor(static_cast<double>((MCTerrain::MapTopLeft3d100.Y + Position.Y) / blockSize)));
    const auto column = static_cast<int32_t>(std::floor(static_cast<double>(blockColumn)));
    AddMoverToList(column + blockRow * MCTerrain::BlocksMapSide);
    return 1;
}

namespace
{
    /// <summary>A world point on <see cref="Eye"/>'s screen (the camera's inline projection).</summary>
    MCVector2D EyeProject(const MCVector3D& point)
    {
        const float scale = Eye->CameraScale != 1 ? 1.0f : 0.5f;
        const float dy = point.Y - Eye->Position.Y;
        const float dz = point.Z - Eye->Position.Z;
        const float sx = (point.X - Eye->Position.X) * scale;
        const float sy = dy * scale;
        MCVector2D screen;
        screen.X = sx * Eye->CosAngle + sy * Eye->CosAngle + Eye->HalfWidth;
        screen.Y = ((sx * Eye->SinAngle + Eye->HalfHeight) - sy * Eye->SinAngle) - scale * dz;
        return screen;
    }
}

auto MCGroundVehicle::Render() -> void
{
    if (GamePaused != 0)
    {
        OnScreen();
    }

    if (Withdrawing != 0 && Pilot->Status == 2)
    {
        return;
    }

    int tagged = 0;

    if (Alignment == HomeTeam->Alignment)
    {
        if (WindowsVisible == Turn)
        {
            if (GetAwake() == 0)
            {
                if (IsRevealed() != 0)
                {
                    Appearance->Render(0);
                }
            }
            else
            {
                Appearance->Render(0);
            }

            if (Smoke != nullptr)
            {
                Smoke->Render();
            }
        }
    }
    else
    {
        const int32_t contactType = GetContactType(HomeTeam->Id, tagged);

        if (contactType == 1)
        {
            if (WindowsVisible == Turn)
            {
                // A wreck draws behind the living.
                Appearance->Render(IsDestroyed() == 0 && IsDisabled() == 0 ? 0 : 150);

                if (Smoke != nullptr)
                {
                    Smoke->Render();
                }
            }
        }
        else if (contactType == 2)
        {
            // A sensor contact: a blip sized by tonnage, at the zoom's scale.
            const int zoomedOut = Eye->CameraScale == 1;
            int32_t shapeIndex;
            const char* shapeName;

            if (50.0f < GetTonnage())
            {
                shapeIndex = zoomedOut ? 1 : 0;
                shapeName = zoomedOut ? "vblip1" : "vblip2";
            }
            else if (35.0f < GetTonnage())
            {
                shapeIndex = zoomedOut ? 3 : 2;
                shapeName = zoomedOut ? "vblip3" : "vblip4";
            }
            else
            {
                shapeIndex = zoomedOut ? 5 : 4;
                shapeName = zoomedOut ? "vblip5" : "vblip6";
            }

            uint8_t* shape = Scenario->SensorContactShapes[shapeIndex];

            if (shape != nullptr)
            {
                if (VfxShapeCount(shape) <= BlipFrame)
                {
                    if (SoundSystem != nullptr && UseSound != 0)
                    {
                        SoundSystem->PlayDigitalSample(0x14, 1, this, 0, 1);
                    }

                    BlipFrame = 0;
                }

                ElementList()->OpenGroup(-100000, 1);
                auto* element =
                    ElementList()->Make<MCVfxElement>(shape, ScreenPos.X, ScreenPos.Y, BlipFrame, 0, nullptr, 0);
                ElementList()->Add(element);
                BlipTime = FrameLength + BlipTime;

                if (0.067 < BlipTime)
                {
                    BlipFrame = static_cast<int32_t>(BlipTime * (1.0 / 0.067) + BlipFrame + 0.5);
                    BlipTime = 0.0f;
                }
            }
        }
    }

    if (DrawExtents != 0)
    {
        // Debug: the extent radius as an ellipse.
        float radius = GetExtentRadius();

        if (Eye->CameraScale == 1)
        {
            radius *= 0.5f;
        }

        MCVector2D center = EyeProject(Position);
        MCVector2D size(radius, radius);
        ElementList()->OpenGroup(-50000, 1);
        // Port: an overlay, on the screen over the view: it follows the object through the zoom.
        center = MCOverlayPoint(center);
        size.X *= MCOverlay.ScaleX;
        size.Y *= MCOverlay.ScaleY;
        ElementList()->Add(ElementList()->Make<MCEllipseElement>(center, size, 0xfe, -50000));
    }

    if (DrawTerrainGrid != 0)
    {
        // Debug: the move path's steps as lines.
        MCMovePath* path = Pilot->GetMovePath();
        const int32_t numSteps = path->NumSteps;

        for (int32_t i = 0; i < numSteps; i++)
        {
            if (i == numSteps - 1)
            {
                continue;
            }

            MCVector3D from = path->StepList[i].Destination;
            MCVector3D to = path->StepList[i + 1].Destination;
            from.Z = Land->GetTerrainElevation(from);
            to.Z = Land->GetTerrainElevation(to);
            MCVector2D fromScreen = EyeProject(from);
            MCVector2D toScreen = EyeProject(to);
            ElementList()->OpenGroup(-100000, 1);
            ElementList()->Add(ElementList()->Make<MCLineElement>(fromScreen, toScreen, 0xfd, nullptr, -100000, -1));
        }
    }

    // The selected vehicle's queued orders: waypoint markers, joined by lines when the queue is drawn as a path.
    if (WaypointMarkers != nullptr && Selected != 0 && Pilot != nullptr && Pilot->GetTacOrderQueue(nullptr) > 0)
    {
        MCTacticalOrder tacOrder;
        tacOrder.Init();
        MCQueuedTacOrder queue[MAX_QUEUED_TACORDERS_PER_WARRIOR];
        const int32_t numOrders = Pilot->GetTacOrderQueue(queue);
        MCVector2D fromScreen = EyeProject(Position);
        const int32_t drawLines = DrawOrderLines;

        for (int32_t i = 0; i < numOrders; i++)
        {
            MCVector2D toScreen = EyeProject(queue[i].Point);
            tacOrder.Data[0] = queue[i].PackedData[0];
            tacOrder.Data[1] = queue[i].PackedData[1];
            tacOrder.Unpack();
            int32_t marker;

            if (tacOrder.Code == TACTICAL_ORDER_JUMPTO_POINT || tacOrder.Code == TACTICAL_ORDER_JUMPTO_OBJECT)
            {
                marker = 4;
            }
            else
            {
                marker = tacOrder.MoveParams.WayPath.Mode[0] << 1;
            }

            if (drawLines != 0)
            {
                ElementList()->OpenGroup(-99999, 1);
                ElementList()->Add(
                    ElementList()->Make<MCLineElement>(fromScreen, toScreen, 0xeb, nullptr, -100000, -1));
                fromScreen = toScreen;
                marker++;
            }

            const int32_t bounds = VfxShapeBounds(WaypointMarkers, marker);
            ElementList()->OpenGroup(-100000, 1);
            auto* element = ElementList()->Make<MCVfxElement>(
                WaypointMarkers, static_cast<float>((bounds >> 16) / 2) + toScreen.X,
                toScreen.Y - static_cast<float>(bounds >> 1 & 0x7fff), marker, 1, nullptr, 1);
            ElementList()->Add(element);
        }

        tacOrder.Destroy();
    }
}

auto MCGroundVehicle::RelFacingTo(MCVector3D goal, int32_t bodyPart) -> float
{
    double facing = MCMover::RelFacingTo(goal, -1);

    if (bodyPart == GROUNDVEHICLE_LOCATION_TURRET)
    {
        facing += TurretRotation;
    }

    if (facing < -180.0)
    {
        return static_cast<float>(facing + 360.0);
    }

    if (180.0f < facing)
    {
        facing = facing - 360.0;
    }

    return static_cast<float>(facing);
}

auto MCGroundVehicle::CalcAttackChance(MCGameObject* target, int32_t aimLocation, float targetTime, int32_t weaponIndex,
                                       float modifiers, int32_t* range, MCVector3D* targetPoint) -> float
{
    if (weaponIndex < NumOther || NumOther + NumWeapons <= weaponIndex)
    {
        return -1000.0f;
    }

    return MCMover::CalcAttackChance(target, aimLocation, targetTime, weaponIndex, modifiers, range, targetPoint);
}

auto MCGroundVehicle::CalcHitLocation(MCGameObject* attacker, int32_t weaponIndex, int32_t attackSource,
                                      int32_t attackType) -> int32_t
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
        facing = RelFacingTo(attacker->GetPosition(), -1);
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

auto MCGroundVehicle::HitInventoryItem(int32_t itemIndex, int setupOnly) -> int
{
    Fatal(0, " Vehicles should never suffer inventory item hit ");
    return 0;
}

auto MCGroundVehicle::DestroyBodyLocation(int32_t location) -> void
{
}

auto MCGroundVehicle::CalcCriticalHitV(int32_t& hitLocation) -> int
{
    if (MPlayer != nullptr && MPlayer->IsServer == 0)
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
        AddCriticalHitChunk(0, 0, hitLocation);
    }

    switch (hitLocation)
    {
        case 1:
        case 2:
            return 1;
        case 3:
        {
            // The crew is hurt.
            Pilot->Injure(6.0f, 1);
            return 0;
        }
        case 4:
        {
            // The engine is knocked out.
            Inventory[Engine].Health = 0;
            Inventory[Engine].Disabled = 1;
            MovementEnabled = 0;
            return 0;
        }
        case 5:
        {
            // The first weapon jams for ten seconds.
            if (Inventory[NumOther].ReadyTime < ScenarioTime)
            {
                StartWeaponRecycle(NumOther);
            }

            Inventory[NumOther].ReadyTime += 10.0f;
            return 0;
        }
        case 7:
        {
            MovementEnabled = 0;
            return 0;
        }
        case 9:
        {
            // Only chassis 2 takes this one; for the others it is no hit.
            if (Chassis != 2)
            {
                hitLocation = 0;
                return 0;
            }

            [[fallthrough]];
        }
        case 8:
        {
            // Drive damage: 10 off the top speed, immobile at 0.
            MaxRunSpeed -= 10.0f;

            if (MaxRunSpeed <= 0.0f)
            {
                MaxRunSpeed = 0.0f;
                MovementEnabled = 0;
            }

            return 0;
        }
        case 10:
        {
            TurretEnabled = 0;
            return 0;
        }
        default:
            return 0;
    }
}

auto MCGroundVehicle::InjureBodyLocation(int32_t bodyLocation, float damage) -> int
{
    MCBodyLocation& location = BodyAt(bodyLocation);

    if (location.CurInternalStructure <= damage)
    {
        location.CurInternalStructure = 0.0f;
        return 1;
    }

    location.CurInternalStructure -= damage;
    const float structureLeft = location.CurInternalStructure / static_cast<float>(location.MaxInternalStructure);

    if (structureLeft == 0.0)
    {
        location.DamageState = 2;
    }
    else if (structureLeft <= 0.5)
    {
        location.DamageState = 1;
    }
    else
    {
        location.DamageState = 0;
    }

    return 0;
}

auto MCGroundVehicle::BuildStatusChunk() -> int32_t
{
    StatusChunk.TargetCellRC[0] = -1;
    StatusChunk.TargetCellRC[1] = -1;
    StatusChunk.BodyState = 0;
    StatusChunk.TargetType = 0;
    StatusChunk.TargetId = 0;
    StatusChunk.TargetBlockOrTrainNumber = 0;
    StatusChunk.TargetVertexOrCarNumber = 0;
    StatusChunk.TargetItemNumber = 0;
    StatusChunk.EjectOrderGiven = 0;
    StatusChunk.JumpOrder = 0;
    StatusChunk.Data = 0;

    const int32_t bodyState = GetBodyState();
    StatusChunk.BodyState = bodyState < 0 || bodyState > 3 ? 0 : static_cast<uint32_t>(bodyState);

    if (Pilot != nullptr)
    {
        MCGameObject* target = Pilot->GetLastTarget();

        if (target != nullptr)
        {
            const int32_t targetClass = target->ObjectClass;

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
                    StatusChunk.TargetType = 2;
                    StatusChunk.TargetId = target->PartId;
                    const int32_t terrainPart = target->PartId - 0x1000;
                    StatusChunk.TargetBlockOrTrainNumber = terrainPart / 0xc80;
                    const int32_t inBlock = terrainPart % 0xc80;
                    StatusChunk.TargetVertexOrCarNumber = inBlock / 8;
                    StatusChunk.TargetItemNumber = static_cast<uint8_t>(inBlock % 8);
                    break;
                }

                case BATTLEMECH:
                case GROUNDVEHICLE:
                case ELEMENTAL:
                {
                    StatusChunk.TargetType = 1;
                    StatusChunk.TargetId = static_cast<MCMover*>(target)->NetRosterIndex;
                    break;
                }
                case CAMERADRONE:
                {
                    StatusChunk.TargetType = 3;
                    StatusChunk.TargetId = target->PartId;
                    StatusChunk.TargetBlockOrTrainNumber = 0x80;
                    StatusChunk.TargetVertexOrCarNumber = target->PartId - 0x802c8;
                    break;
                }
                case TRAINCAR:
                {
                    StatusChunk.TargetType = 3;
                    StatusChunk.TargetId = target->PartId;
                    const int32_t trainPart = target->PartId - 0x7d000;
                    StatusChunk.TargetBlockOrTrainNumber = trainPart / 100;
                    StatusChunk.TargetVertexOrCarNumber = trainPart % 100;
                    break;
                }

                default:
                    Fatal(targetClass, " GroundVehicle.buildStatusChunk: bad target object class ");
            }
        }
    }

    StatusChunk.EjectOrderGiven = EjectOrderGiven;
    StatusChunk.Pack(this);

    // Checks the chunk unpacks to what was packed.
    MCStatusChunk check;
    check.Data = StatusChunk.Data;
    check.MCStatusChunk::Unpack(this);

    if (StatusChunk.EqualTo(&check) == 0)
    {
        Fatal(0, " BAD Statuschunk: save stchunk.dbg file! ");
    }

    return 0;
}

auto MCGroundVehicle::HandleStatusChunk(int32_t updateAge, uint32_t chunk) -> int32_t
{
    StatusChunk.TargetCellRC[0] = -1;
    StatusChunk.TargetCellRC[1] = -1;
    StatusChunk.Data = 0;
    StatusChunk.BodyState = 0;
    StatusChunk.TargetType = 0;
    StatusChunk.TargetId = 0;
    StatusChunk.TargetBlockOrTrainNumber = 0;
    StatusChunk.TargetVertexOrCarNumber = 0;
    StatusChunk.TargetItemNumber = 0;
    StatusChunk.EjectOrderGiven = 0;
    StatusChunk.JumpOrder = 0;
    StatusChunk.Data = chunk;
    StatusChunk.Unpack(this);

    if (StatusChunkUnpackErr != 0)
    {
        return 0;
    }

    int32_t targetPartId = 0;

    if (StatusChunk.JumpOrder == 0 && static_cast<int8_t>(StatusChunk.TargetType) > 0)
    {
        if (StatusChunk.TargetType == 1)
        {
            targetPartId = MPlayer->MoverRoster[StatusChunk.TargetId]->PartId;
        }
        else if (StatusChunk.TargetType < 4)
        {
            targetPartId = StatusChunk.TargetId;
        }
    }

    if (Pilot == nullptr)
    {
        return 0;
    }

    MCGameObject* target = nullptr;
    int keepTarget = 0;

    if (targetPartId != 0)
    {
        MCGameObject* lastTarget = Pilot->GetLastTarget();

        if (lastTarget != nullptr && lastTarget->PartId == targetPartId)
        {
            keepTarget = 1;
        }
        else
        {
            target = static_cast<MCGameObject*>(ObjectList->FindObjectFromPart(targetPartId));
        }
    }

    if (keepTarget == 0)
    {
        Pilot->SetLastTarget(target, 0, 0);
    }

    if (EjectOrderGiven == 0 && StatusChunk.EjectOrderGiven != 0)
    {
        EjectOrderGiven = 1;
        HandleEjection();
    }

    return 0;
}

auto MCGroundVehicle::BuildMoveChunk() -> int32_t
{
    MoveChunk.Init();

    if (Pilot != nullptr)
    {
        Pilot->GetMovePath();
        MoveChunk.Build(this, Pilot->MoveOrders.Path[0], Pilot->MoveOrders.Path[1]);
    }

    MoveChunk.Pack(this);

    // Checks the chunk unpacks to what was packed; a chunk that can't is replaced by an empty one.
    MCMoveChunk check;
    check.StepPos[0][0] = -1;
    check.StepPos[0][1] = -1;
    check.Run = 0;
    check.NumSteps = 0;
    check.Data = MoveChunk.Data;
    check.Unpack(this);

    if (MoveChunkUnpackErr == 0)
    {
        if (MoveChunk.EqualTo(this, &check) == 0)
        {
            Fatal(0, " Bad gvehicl movechunk: save mvchunk.dbg file! ");
        }
    }
    else
    {
        MoveChunk.Init();
        MoveChunk.Build(this, nullptr, nullptr);
        MoveChunk.Pack(this);
    }

    return 0;
}

auto MCGroundVehicle::HandleMoveChunk(uint32_t chunk) -> int32_t
{
    MoveChunk.Init();
    MoveChunk.Data = chunk;
    MoveChunk.Unpack(this);

    if (MoveChunkUnpackErr == 0)
    {
        MCMovePath* path = GetPilot()->GetMovePath();
        path->SetMoveChunk(&MoveChunk);

        // Skip ahead to the step nearest the vehicle.
        if (path->NumStepsWhenNotPaused > 1)
        {
            int32_t step = path->NumStepsWhenNotPaused;

            do
            {
                step--;

                if (step < 1)
                {
                    break;
                }
            } while (MapCellDiagonal < DistanceFrom(path->StepList[step].Destination));

            path->CurStep = step;
        }

        NewMoveChunk = 1;
    }

    return 0;
}

auto MCGroundVehicle::WeaponLocked(int32_t weaponIndex, MCVector3D targetPosition) -> float
{
    return RelFacingTo(targetPosition, GROUNDVEHICLE_LOCATION_TURRET);
}

namespace
{
    /// <summary>Whether <paramref name="object"/> is a mover (mech, vehicle, elemental or plain mover).</summary>
    bool IsMoverClass(const MCGameObject* object)
    {
        const int32_t objectClass = object->ObjectClass;
        return objectClass == BATTLEMECH || objectClass == GROUNDVEHICLE || objectClass == ELEMENTAL ||
               objectClass == MOVER;
    }
}

auto MCGroundVehicle::HandleWeaponHit(MCWeaponShotInfo* shotInfo, int addMultiplayChunk) -> int32_t
{
    if ((MPlayer == nullptr && CantHitMe != 0 && Pilot->OnHomeTeam() != 0) || shotInfo == nullptr)
    {
        return 0;
    }

    if (addMultiplayChunk != 0)
    {
        MPlayer->AddWeaponHitChunk(this, shotInfo, 0);
    }

    if (shotInfo->Damage <= 0.0f)
    {
        return 0;
    }

    if (IsDestroyed() != 0)
    {
        return 0;
    }

    const int32_t hitLocation = shotInfo->HitLocation;
    const MCWeaponShotInfo originalShot = *shotInfo;

    if (hitLocation < 0 || hitLocation > 4)
    {
        char attackerName[64];
        MCGameObject* attacker = shotInfo->Attacker;

        if (attacker == nullptr)
        {
            std::strcpy(attackerName, "attacker?");
        }
        else if (IsMoverClass(attacker))
        {
            std::strcpy(attackerName, static_cast<MCMover*>(attacker)->DebugStatus.c_str());
        }
        else
        {
            std::sprintf(attackerName, "ID:%d", attacker->PartId);
        }

        char message[128];
        std::sprintf(message, "GVehicle.handleWeaponHit: [%s]%d for %.2f at %d", attackerName, shotInfo->MasterId,
                     static_cast<double>(shotInfo->Damage), hitLocation);
        Fatal(0, message);
    }

    MCArmorLocation& hitArmor = Armor[hitLocation];

    if (hitArmor.CurArmor > 0.0f)
    {
        if (shotInfo->Damage <= hitArmor.CurArmor)
        {
            hitArmor.CurArmor -= shotInfo->Damage;
            shotInfo->SetDamage(0.0f);
        }
        else
        {
            shotInfo->SetDamage(shotInfo->Damage - hitArmor.CurArmor);
            Armor[shotInfo->HitLocation].CurArmor = 0.0f;
        }

        // A sweeper sweeps with its front: a hit there ends that.
        if (shotInfo->HitLocation == GROUNDVEHICLE_LOCATION_FRONT)
        {
            MineSweeper = 0;
        }
    }

    const int wasDisabled = IsDisabled();

    if (shotInfo->Damage > 0.0f && InjureBodyLocation(hitLocation, shotInfo->Damage) != 0)
    {
        Pilot->HandleOwnVehicleIncapacitation(0);
        ObjType->HandleDestruction(this, nullptr);
    }

    CurCV = CalcCV(0);

    MCGameObject* attacker = shotInfo->Attacker;

    if (wasDisabled == 0 && IsDisabled() != 0)
    {
        // The attacker's pilot hears of the kill.
        if (attacker != nullptr && IsMoverClass(attacker))
        {
            attacker->GetPilot()->TriggerAlarm(12, PartId);
        }
    }
    else if (attacker != nullptr)
    {
        Pilot->TriggerAlarm(shotInfo->MasterId > -1 || shotInfo->MasterId == -4 ? 1 : 10, attacker->PartId);
    }
    else
    {
        Pilot->TriggerAlarm(
            1, shotInfo->MasterId == -4 || shotInfo->MasterId >= 0 ? 0 : static_cast<uint32_t>(shotInfo->MasterId));
    }

    shotInfo->Init(originalShot.Attacker, originalShot.MasterId, originalShot.Damage, originalShot.HitLocation,
                   originalShot.EntryAngle);
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
    void SendTargetFireChunk(MCGroundVehicle* vehicle, MCGameObject* target, MCVector3D* point, int32_t weapon, int hit,
                             float entryAngle, int32_t missiles, int32_t missilesPastAMS, int32_t antiMissileShots,
                             int32_t hitLocation, const char* badChunkMessage)
    {
        MCWeaponFireChunk chunk;
        chunk.Init();
        auto* bigTarget = static_cast<MCBigGameObject*>(target);

        if (target == nullptr)
        {
            chunk.BuildLocationTarget(*point, weapon, hit, missiles);
        }
        else if (IsMoverClass(target))
        {
            chunk.BuildMoverTarget(bigTarget, weapon, hit, entryAngle, missiles, missilesPastAMS, antiMissileShots,
                                   hitLocation);
        }
        else if (target->ObjectClass == TRAINCAR)
        {
            chunk.BuildTrainTarget(bigTarget, weapon, hit, entryAngle, missiles);
        }
        else if (target->ObjectClass == CAMERADRONE)
        {
            chunk.BuildCameraDroneTarget(bigTarget, weapon, hit, entryAngle, missiles);
        }
        else
        {
            chunk.BuildTerrainTarget(bigTarget, weapon, hit, missiles);
        }

        chunk.Pack();
        MCWeaponFireChunk check;
        check.Init();
        check.Data = chunk.Data;
        check.Unpack(vehicle);

        if (chunk.EqualTo(&check) == 0)
        {
            Fatal(0, badChunkMessage);
        }

        vehicle->AddWeaponFireChunk(0, &chunk);
        LogWeaponFireChunk(&chunk, vehicle, target);
    }

    /// <summary>
    /// Sends a weapon effect on its way from the vehicle, at <paramref name="target"/> or, when it is null, at
    /// <paramref name="point"/>, carrying <paramref name="shot"/>; then adds it to the weapon list. Vehicles fire from
    /// hot spot 0.
    /// </summary>
    void LaunchWeaponFX(MCGroundVehicle* vehicle, MCGameObject* fx, MCGameObject* target, MCVector3D* point,
                        MCWeaponShotInfo& shot, int32_t targetHotSpot)
    {
        if (fx->ObjectClass == BULLET)
        {
            auto* bullet = static_cast<MCBullet*>(fx);

            if (bullet->NumShots != 5)
            {
                bullet->ShotInfo[bullet->NumShots++].Init(shot.Attacker, shot.MasterId, shot.Damage, shot.HitLocation,
                                                          shot.EntryAngle);
            }

            if (target == nullptr)
            {
                bullet->Connect(vehicle, *point, 0);
            }
            else
            {
                bullet->Owner = vehicle;
                bullet->Target = target;
                bullet->OwnerHotSpot = 0;
                bullet->TargetHotSpot = targetHotSpot;
            }
        }
        else if (fx->ObjectClass == LASER)
        {
            auto* laser = static_cast<MCLaser*>(fx);

            if (target == nullptr)
            {
                laser->Connect(vehicle, *point, &shot, 0);
            }
            else
            {
                laser->Source.SetWatcher(vehicle);
                laser->Target.SetWatcher(target);
                laser->SourceHotSpot = 0;
                laser->TargetHotSpot = targetHotSpot;
                laser->ShotInfo.Init(shot.Attacker, shot.MasterId, shot.Damage, shot.HitLocation, shot.EntryAngle);
            }
        }
        else
        {
            auto* projectile = static_cast<MCProjectileLaser*>(fx);

            if (target == nullptr)
            {
                projectile->Connect(vehicle, *point, &shot, 0);
            }
            else
            {
                projectile->Owner = vehicle;
                projectile->Target = target;
                projectile->OwnerHotSpot = 0;
                projectile->TargetHotSpot = targetHotSpot;
                projectile->ShotInfo.Init(shot.Attacker, shot.MasterId, shot.Damage, shot.HitLocation, shot.EntryAngle);
            }
        }

        WeaponList->AddNode(fx);
    }

    /// <summary>Makes a weapon's effect object (Fatal when it can't).</summary>
    MCGameObject* CreateWeaponFX(const MCMasterComponent& weapon)
    {
        MCGameObject* fx = CreateObject(static_cast<int32_t>(WeaponFXTable[weapon.WeaponEffect]));

        if (fx == nullptr)
        {
            Fatal(-1, " couldnt create weapon FX ");
        }

        return fx;
    }

    /// <summary>The hot spot of the hit location, on a mech target; 0 otherwise.</summary>
    int32_t TargetHotSpotOf(MCGameObject* target, int32_t hitLocation)
    {
        if (target != nullptr && target->ObjectClass == BATTLEMECH)
        {
            // Port fix: the original reads body[hitLocation], past the eight body locations for a rear torso hit
            // (8..10); the torso it maps to is read instead.
            return static_cast<MCBattleMech*>(target)->BodyAt(MechArmorToBodyLocation[hitLocation]).HotSpotNumber;
        }

        return 0;
    }

    /// <summary>Firing gives a vehicle away to the other side's mechs within visual range.</summary>
    void RevealFiring(MCGroundVehicle* vehicle)
    {
        MCObjectQueueNode* enemies = nullptr;
        uint8_t seenBy = 0;

        if (vehicle->Alignment == 1)
        {
            enemies = ClanMechList;
            seenBy = 2;
        }
        else if (vehicle->Alignment == -1)
        {
            enemies = InnerSphereMechList;
            seenBy = 1;
        }

        if (enemies == nullptr)
        {
            return;
        }

        for (MCBaseObject* enemy = enemies->Head; enemy != nullptr; enemy = enemy->Next)
        {
            MCVector3D enemyPosition = static_cast<MCGameObject*>(enemy)->GetPosition();

            if (vehicle->DistanceFrom(enemyPosition) < Scenario->MaxVisualRange)
            {
                Land->MarkRadiusSeen(vehicle->Position, vehicle->Frame.J, 360.0f, Scenario->FireVisualRange, seenBy);
                return;
            }
        }
    }

    /// <summary>Where a missed shot lands: scattered up to <paramref name="scatter"/> about the aim point.</summary>
    /// <param name="centred">Missiles scatter both ways; other shots (as the original computes them) only one.</param>
    MCVector3D MissPoint(MCGameObject* target, MCVector3D* targetPoint, float scatter, int centred)
    {
        MCVector3D miss;
        miss.X = scatter;
        miss.Y = scatter;
        miss.Z = 0.0f;
        const auto offsetX = static_cast<float>(RandomNumber(static_cast<int32_t>(miss.X + miss.X)) - miss.X);
        const auto offsetY = static_cast<float>(RandomNumber(static_cast<int32_t>(miss.Y + miss.Y)) - miss.Y);
        const auto offsetZ = static_cast<float>(RandomNumber(static_cast<int32_t>(miss.Z + miss.Z)) - miss.Z);

        if (centred != 0)
        {
            miss.X = offsetX;
            miss.Y = offsetY;
        }
        else
        {
            miss.X = miss.X + offsetX;
            miss.Y = miss.Y + offsetY;
        }

        miss.Z = miss.Z + offsetZ;
        const MCVector3D base = target != nullptr ? target->GetPosition() : *targetPoint;
        miss.X += base.X;
        miss.Y += base.Y;
        miss.Z += base.Z;
        return miss;
    }
}

auto MCGroundVehicle::FireWeapon(MCGameObject* target, float targetTime, int32_t weaponIndex, int32_t attackType,
                                 int32_t aimLocation, MCVector3D* targetPoint) -> int32_t
{
    if (Status != 0)
    {
        return 1;
    }

    if (IsWeaponIndex(weaponIndex) == 0)
    {
        return 2;
    }

    if (IsWeaponReady(weaponIndex) == 0)
    {
        return 3;
    }

    float distance;

    if (target == nullptr)
    {
        if (targetPoint == nullptr || LineOfSight(*targetPoint) == 0)
        {
            return 4;
        }

        distance = static_cast<float>(DistanceFrom(*targetPoint));
    }
    else
    {
        // A camera drone can't be shot for two seconds after launch.
        if (target->ObjectClass == CAMERADRONE && ScenarioTime < static_cast<MCCameraDrone*>(target)->LaunchTime + 2.0)
        {
            return 4;
        }

        if (target->IsDestroyed() != 0)
        {
            return 4;
        }

        if (LineOfSight(target) == 0)
        {
            return 4;
        }

        MCVector3D targetPosition = target->GetPosition();
        distance = static_cast<float>(DistanceFrom(targetPosition));
    }

    const int32_t inRange = WeaponInRange(weaponIndex, distance);

    if ((MPlayer == nullptr || MPlayer->IsServer != 0) && inRange == 0)
    {
        return 4;
    }

    const MCMasterComponent& weapon = MasterComponentList[Inventory[weaponIndex].MasterID];

    if (weapon.MissileType != 2 && weapon.MissileType != 1 && weapon.MissileType != 3)
    {
        // Direct fire needs a clear line.
        if (target == nullptr)
        {
            if (targetPoint == nullptr || LineOfFire(*targetPoint) == 0)
            {
                return 4;
            }
        }
        else if (LineOfFire(target) == 0)
        {
            return 4;
        }
    }

    // A pop-up turret fires only once it is up.
    if (WeaponsDeployed == 0)
    {
        return 5;
    }

    const int32_t numShots = GetWeaponShots(weaponIndex);

    if (numShots == 0)
    {
        return 4;
    }

    MCMechWarrior* targetPilot = nullptr;

    if (target != nullptr && IsMoverClass(target))
    {
        targetPilot = target->GetPilot();
        targetPilot->UpdateAttackerStatus(static_cast<uint32_t>(PartId), ScenarioTime);
    }

    float entryAngle = 0.0f;

    if (target != nullptr)
    {
        entryAngle = target->RelFacingTo(Position, -1);
    }

    const int isStreak = weapon.WeaponFlags & 1;
    int32_t range = 0;
    int32_t hitChance =
        static_cast<int32_t>(CalcAttackChance(target, aimLocation, targetTime, weaponIndex, 0.0f, &range, targetPoint));
    const int32_t hitRoll = RandomNumber(100);

    if (target != nullptr && target->GetAlignment() == -1)
    {
        Pilot->NumSkillUses[MWS_GUNNERY][1]++;
    }

    // Aimed shots only from a standing vehicle.
    if (aimLocation != -1 && 0.0 < GetVelocity().Magnitude())
    {
        hitChance = 0;
    }

    int32_t hitLocation = -1;

    if (target != nullptr && hitRoll < hitChance)
    {
        if (target->GetAlignment() == -1)
        {
            Pilot->NumSkillSuccesses[MWS_GUNNERY][1]++;
        }

        if (aimLocation != -1)
        {
            hitLocation = aimLocation;
        }
    }

    StartWeaponRecycle(weaponIndex);

    const int32_t chunkWeapon = weaponIndex - NumOther;
    const char* const badChunk = " GVehicle.fireWeapon: Bad WeaponFireChunk (save wfchunk.dbg file now) ";
    const char* const badMissChunk = " GVehicl.fireWeapon: Bad WeaponFireChunk (save wfchunk.dbg file now) ";

    if (hitRoll < hitChance)
    {
        if (numShots != UNLIMITED_SHOTS)
        {
            DeductWeaponShot(weaponIndex, 1);
        }

        MCInventoryItem& item = Inventory[weaponIndex];
        const MCMasterComponent& fired = MasterComponentList[item.MasterID];

        if (fired.Form == 9)
        {
            // Missiles: a streak fires them all, anything else about half; anti-missile systems take some out.
            const int32_t rackSize = fired.NumMissiles;
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
                missilesLeft = target->FireAntiMissileSystem(missiles, antiMissileShots);

                if (antiMissileShots > 0)
                {
                    target->ReduceAntiMissileAmmo(antiMissileShots);
                }
            }

            if (missilesLeft != 0)
            {
                MCGameObject* fx = CreateWeaponFX(fired);
                int32_t targetHotSpot = 0;

                if (target == nullptr)
                {
                    hitLocation = -1;
                }
                else
                {
                    if (aimLocation == -1)
                    {
                        hitLocation = target->CalcHitLocation(this, weaponIndex, 0, attackType);
                    }

                    targetHotSpot = TargetHotSpotOf(target, hitLocation);
                }

                Assert(hitLocation != -2 ? 1 : 0, 0, " GroundVehicle.FireWeapon: Bad Hit Location ");
                MCWeaponShotInfo shot;
                shot.Init(this, item.MasterID, fired.Damage * static_cast<float>(missilesLeft), hitLocation,
                          entryAngle);

                if (MPlayer != nullptr && MPlayer->IsServer != 0)
                {
                    SendTargetFireChunk(this, target, targetPoint, chunkWeapon, 1, entryAngle, missiles, missilesLeft,
                                        antiMissileShots, hitLocation, badChunk);
                }

                LaunchWeaponFX(this, fx, target, targetPoint, shot, targetHotSpot);

                if (target == nullptr)
                {
                    Pilot->ClearCurTacOrder(1, 0);
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
                hitLocation = target->CalcHitLocation(this, weaponIndex, 0, attackType);
            }

            Assert(hitLocation != -2 ? 1 : 0, 0, " GroundVehicle.FireWeapon: Bad Hit Location ");
            MCWeaponShotInfo shot;
            shot.Init(this, item.MasterID, fired.Damage, hitLocation, entryAngle);

            if (MPlayer != nullptr && MPlayer->IsServer != 0)
            {
                SendTargetFireChunk(this, target, targetPoint, chunkWeapon, 1, entryAngle, 0, 0, 0, hitLocation,
                                    badChunk);
            }

            MCGameObject* fx = CreateWeaponFX(fired);
            LaunchWeaponFX(this, fx, target, targetPoint, shot, TargetHotSpotOf(target, hitLocation));

            if (target == nullptr)
            {
                Pilot->ClearCurTacOrder(1, 0);
            }
        }
    }
    else if (isStreak == 0)
    {
        // A miss (a streak doesn't fire without a lock): the shot lands somewhere near.
        if (numShots != UNLIMITED_SHOTS)
        {
            DeductWeaponShot(weaponIndex, 1);
        }

        MCInventoryItem& item = Inventory[weaponIndex];
        const MCMasterComponent& fired = MasterComponentList[item.MasterID];
        const float scatter = target != nullptr ? 25.0f : 5.0f;

        if (fired.Form == 9)
        {
            const int32_t rackSize = fired.NumMissiles;
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
                MCGameObject* fx = CreateWeaponFX(fired);
                MCWeaponShotInfo shot;
                shot.Init(this, item.MasterID, fired.Damage * static_cast<float>(missiles), -1, entryAngle);
                MCVector3D landing = MissPoint(target, targetPoint, scatter, 1);

                if (MPlayer != nullptr && MPlayer->IsServer != 0)
                {
                    SendTargetFireChunk(this, nullptr, &landing, chunkWeapon, 0, 0.0f, missiles, 0, 0, 0, badMissChunk);
                }

                LaunchWeaponFX(this, fx, nullptr, &landing, shot, 0);
            }
        }
        else
        {
            MCWeaponShotInfo shot;
            shot.Init(this, item.MasterID, fired.Damage, -1, entryAngle);
            MCGameObject* fx = CreateWeaponFX(fired);
            MCVector3D landing = MissPoint(target, targetPoint, scatter, 0);

            if (MPlayer != nullptr && MPlayer->IsServer != 0)
            {
                SendTargetFireChunk(this, nullptr, &landing, chunkWeapon, 0, 0.0f, 0, 0, 0, 0, badMissChunk);
            }

            LaunchWeaponFX(this, fx, nullptr, &landing, shot, 0);
        }
    }

    if (targetPilot != nullptr)
    {
        targetPilot->TriggerAlarm(0, static_cast<uint32_t>(PartId));
    }

    RevealFiring(this);

    if (Group != nullptr)
    {
        Group->HandleMateFiredWeapon(static_cast<uint32_t>(PartId));
    }

    return 0;
}

auto MCGroundVehicle::HandleWeaponFire(int32_t weaponIndex, MCGameObject* target, MCVector3D* targetPoint, int hit,
                                       float entryAngle, int32_t numMissiles, int32_t missilesPastAMS,
                                       int32_t antiMissileShots, int32_t hitLocation) -> int32_t
{
    const int32_t numShots = GetWeaponShots(weaponIndex);
    StartWeaponRecycle(weaponIndex);
    MCInventoryItem& item = Inventory[weaponIndex];
    const MCMasterComponent& fired = MasterComponentList[item.MasterID];
    const int isStreak = fired.WeaponFlags & 1;
    MCWeaponShotInfo shot;

    if (hit == 0)
    {
        Assert(target == nullptr ? 1 : 0, 0, " GVehicl.handleWeaponFire: target should be NULL with network miss! ");
        Assert(targetPoint != nullptr ? 1 : 0, 0,
               " GVehicl.handleWeaponFire: MUST have targetpoint with network miss! ");

        if (isStreak != 0)
        {
            CurMoverWeaponFireChunk.Unpack(this);
            DebugWeaponFireChunk(&CurMoverWeaponFireChunk, nullptr, this);
            Assert(0, 0, " GVehicl.handleWeaponFire: streaks shouldn't miss! ");
        }

        if (numShots != UNLIMITED_SHOTS)
        {
            DeductWeaponShot(weaponIndex, 1);
        }

        if (fired.Form == 9)
        {
            if (numMissiles > 0)
            {
                MCGameObject* fx = CreateWeaponFX(fired);
                shot.Init(this, item.MasterID, fired.Damage * static_cast<float>(numMissiles), -1, entryAngle);
                LaunchWeaponFX(this, fx, nullptr, targetPoint, shot, 0);
            }
        }
        else
        {
            shot.Init(this, item.MasterID, fired.Damage, -1, entryAngle);
            MCGameObject* fx = CreateWeaponFX(fired);
            LaunchWeaponFX(this, fx, nullptr, targetPoint, shot, 0);
        }
    }
    else
    {
        if (numShots != UNLIMITED_SHOTS)
        {
            DeductWeaponShot(weaponIndex, 1);
        }

        if (fired.Form == 9)
        {
            if (antiMissileShots > 0)
            {
                target->ReduceAntiMissileAmmo(antiMissileShots);
            }

            if (missilesPastAMS != 0)
            {
                MCGameObject* fx = CreateWeaponFX(fired);
                Assert(hitLocation != -2 ? 1 : 0, static_cast<uint32_t>(TargetRolo),
                       " GroundVehicle.handleWeaponFire: Bad Hit Location ");
                const int32_t targetHotSpot = TargetHotSpotOf(target, hitLocation);
                shot.Init(this, item.MasterID, fired.Damage * static_cast<float>(missilesPastAMS), hitLocation,
                          entryAngle);
                LaunchWeaponFX(this, fx, target, targetPoint, shot, targetHotSpot);

                if (target == nullptr)
                {
                    Pilot->ClearCurTacOrder(1, 0);
                }
            }
        }
        else
        {
            shot.Init(this, item.MasterID, fired.Damage, hitLocation, entryAngle);
            MCGameObject* fx = CreateWeaponFX(fired);
            LaunchWeaponFX(this, fx, target, targetPoint, shot, TargetHotSpotOf(target, hitLocation));

            if (target == nullptr)
            {
                Pilot->ClearCurTacOrder(1, 0);
            }
        }
    }

    if (target != nullptr && IsMoverClass(target))
    {
        MCMechWarrior* targetPilot = target->GetPilot();
        targetPilot->UpdateAttackerStatus(static_cast<uint32_t>(PartId), ScenarioTime);
        targetPilot->TriggerAlarm(0, static_cast<uint32_t>(PartId));
    }

    RevealFiring(this);

    if (Group != nullptr)
    {
        Group->HandleMateFiredWeapon(static_cast<uint32_t>(PartId));
    }

    return 0;
}

auto MCGroundVehicle::OpenStatusWindow(int32_t left, int32_t top, int32_t right, int32_t bottom) -> int32_t
{
    auto* window = new MCGroundVehicleStatusWindow;
    StatusWindow = window;
    window->Init(left, top, right, bottom, this);
    StatusWindow->SetBackColor(0);
    StatusWindow->Draw();
    ScreenWindow->AddChild(StatusWindow);

    if (Pilot != nullptr)
    {
        Pilot->OpenStatusWindow(left + 30, top + 30, right, bottom);
    }

    return 0;
}

MCGroundVehicleStatusWindow::~MCGroundVehicleStatusWindow()
{
    // The inline ~aTitleWindow.
    MCGuiTitleWindow::Destroy();
}

auto MCGroundVehicle::CloseStatusWindow() -> int32_t
{
    if (Pilot != nullptr)
    {
        Pilot->CloseStatusWindow();
    }

    // The window is destroyed, not deleted.
    StatusWindow->Destroy();
    StatusWindow = nullptr;
    return 0;
}

auto MCGroundVehicle::GetVitalInfo(void* vitalInfo) -> int32_t
{
    const int32_t size = MCMover::GetVitalInfo(nullptr);

    if (vitalInfo != nullptr)
    {
        MCMover::GetVitalInfo(vitalInfo);
        static_cast<uint8_t*>(vitalInfo)[size] = static_cast<uint8_t>(MovementEnabled);
    }

    return size + 1;
}

auto MCGroundVehicle::GetTotalEffectiveness() -> float
{
    if (IsDestroyed() != 0 || IsDisabled() != 0)
    {
        return 0.0f;
    }

    const float weaponRatio = MaxWeaponEffectiveness == 0.0f ? 1.0f : WeaponEffectiveness / MaxWeaponEffectiveness;
    // Each location's armor share, scaled to 0.4..1; a turret without armor counts in full.
    const auto armorFactor = [&](int32_t location)
    { return static_cast<float>(Armor[location].CurArmor / static_cast<float>(Armor[location].MaxArmor) * 0.6 + 0.4); };
    const float front = armorFactor(GROUNDVEHICLE_LOCATION_FRONT);
    const float left = armorFactor(GROUNDVEHICLE_LOCATION_LEFT);
    const float right = armorFactor(GROUNDVEHICLE_LOCATION_RIGHT);
    const float rear = armorFactor(GROUNDVEHICLE_LOCATION_REAR);
    float turret = 1.0f;

    if (static_cast<float>(Armor[GROUNDVEHICLE_LOCATION_TURRET].MaxArmor) != 0.0)
    {
        turret = armorFactor(GROUNDVEHICLE_LOCATION_TURRET);
    }

    // Wounds wear the crew down.
    const float woundFactor[7] = {1.0f, 0.95f, 0.85f, 0.75f, 0.5f, 0.3f, 0.0f};
    int32_t wounds = static_cast<int32_t>(GetPilot()->Wounds);
    // Port fix: the original indexes the table unchecked.
    wounds = std::clamp(wounds, 0, 6);
    return turret * rear * right * left * front * woundFactor[wounds] * weaponRatio;
}

auto MCGroundVehicleStatusWindow::Init(int32_t x, int32_t y, int32_t w, int32_t h, MCGroundVehicle* newVehicle) -> void
{
    MCGuiTitleWindow::Init(x, y, w, h, nullptr);

    if (TitleBar != nullptr)
    {
        TitleBar->ShowCloseButton(1);
    }

    Vehicle = newVehicle;
}

auto MCGroundVehicleStatusWindow::HandleEvent(MCGuiEvent* event) -> void
{
    if (event->Type == 0xd)
    {
        Vehicle->CloseStatusWindow();
    }

    MCGuiObject::HandleEvent(event);
}

auto MCGroundVehicleStatusWindow::Resize(int32_t w, int32_t h) -> void
{
    MCGuiTitleWindow::Resize(w, h);
}

namespace
{
    /// <summary>Alignment names, by alignment + 1.</summary>
    const char* const AlignmentNames[3] = {"Clan", "Neutral", "Inner Sphere"};

    /// <summary>The status window's title: alignment, vehicle name and crew callsign.</summary>
    void SetVehicleTitle(MCGroundVehicleStatusWindow* window)
    {
        MCGroundVehicle* vehicle = window->Vehicle;

        if (vehicle == nullptr)
        {
            return;
        }

        char title[256];
        std::snprintf(title, sizeof(title), "%s %s (%s)", AlignmentNames[vehicle->GetAlignment() + 1],
                      vehicle->DebugStatus.c_str(), vehicle->GetPilot()->Callsign);
        window->SetTitle(title);
    }
}

auto MCGroundVehicleStatusWindow::Display() -> void
{
    VfxPaneWipe(DisplayPort->Frame(), BackgroundColor);
    SetVehicleTitle(this);
    MCGuiObject::Display();
}

auto MCGroundVehicleStatusWindow::Draw() -> void
{
    SetVehicleTitle(this);
    MCGuiTitleWindow::Draw();
}
