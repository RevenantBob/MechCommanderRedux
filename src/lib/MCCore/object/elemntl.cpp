#include "stdafx.h"
#include "object/elemntl.h"
#include "ai/move.h"
#include "ai/tacordr.h"
#include "appear/apprtype.h"
#include "camera/camera.h"
#include "camera/camlist.h"
#include "engine/ceglist.h"
#include "engine/celine.h"
#include "engine/cevfx.h"
#include "engine/crater.h"
#include "vfx/vfxfuncs.h"
#include "iface/iface.h"
#include "lib/aerror.h"
#include "lib/cvmath.h"
#include "lib/heap.h"
#include "lib/inifile.h"
#include "main/main.h"
#include "mission/scenario.h"
#include "network/multplyr.h"
#include "object/aictrl.h"
#include "object/artlry.h"
#include "object/bullet.h"
#include "object/cmponent.h"
#include "object/contact.h"
#include "object/elemctrl.h"
#include "object/elemdyn.h"
#include "object/group.h"
#include "object/gvehicl.h"
#include "object/laser.h"
#include "object/mech.h"
#include "object/object.h"
#include "object/objque.h"
#include "object/plyrctrl.h"
#include "object/prjlase.h"
#include "object/team.h"
#include "object/warrior.h"
#include "sound/soundsys.h"
#include "sprite/lactor.h"
#include "terrain/terrain.h"

float elmDamageOnImpact = 0.0f;
float ElementalTargetNoJumpDistance = 75.0f;
int useOldProject = 0;

namespace
{
    /// <summary>Half pi, as MCX.EXE stores it (MCX.EXE @ 0x0077cb50).</summary>
    constexpr double HALF_PI = 0x1.921fb5443e88cp+0;
    /// <summary>Degrees to radians, as MCX.EXE stores it (MCX.EXE @ 0x0077c2a0; a hair under pi / 180).</summary>
    constexpr double DEGREES_TO_RADIANS = 0x1.1df46a2526c7ap-6;
    /// <summary>getWeaponShots' answer for a weapon that needs no ammo.</summary>
    constexpr int32_t UNLIMITED_SHOTS = 9999;
    /// <summary>How far (world units) a missed shot lands from the target, either way.</summary>
    constexpr float MISS_SCATTER = 25.0f;

    /// <summary>Turns a frame about its k axis (MC2's inline frame_of_ref::rotate_about_k).</summary>
    void rotateAboutK(frame_of_ref& frame, float s, float c)
    {
        const vector_3d oldI = frame.i;
        frame.i = frame.i * c + frame.j * s;
        frame.j = frame.j * c - oldI * s;
    }

    /// <summary>Starts a collision's grace period: no more from <paramref name="collider"/> for two seconds.
    /// </summary>
    /// <returns>0 while the last one from <paramref name="collider"/> is still in its grace period.</returns>
    int startCollision(GameObject* collidee, GameObject* collider)
    {
        if (collidee->getCollisionFreeFrom() == collider && scenarioTime <= collidee->getCollisionFreeTime())
        {
            return 0;
        }

        collidee->setCollisionFreeFrom(collider);
        collidee->setCollisionFreeTime(scenarioTime + 2.0f);
        return 1;
    }

    /// <summary>Turns <paramref name="collidee"/> by <paramref name="radians"/>.</summary>
    void turnAway(GameObject* collidee, double radians)
    {
        frame_of_ref frame = collidee->getFrame();
        rotateAboutK(frame, static_cast<float>(std::sin(radians)), static_cast<float>(std::cos(radians)));
        collidee->setFrame(frame);
    }

    /// <summary>A marine's death: it is taken off the interface at once, leaving no wreck.</summary>
    /// <param name="deathTime">The death timer to set (0.8 when it goes quietly, 0 when shot).</param>
    void removeMarine(Elemental* marine, float deathTime)
    {
        marine->unknown794 = deathTime;
        marine->getPilot()->triggerAlarm(7, 0);
        marine->status = 2;
        marine->unknown8B8 = 0;
        marine->unknown798 = 0;
        theInterface->RemoveMech(marine->partId);
    }

    /// <summary>Adds a shot to a bullet, when it has room (5 at most).</summary>
    void addBulletShot(Bullet* bullet, _WeaponShotInfo& shot)
    {
        if (bullet->numShots != 5)
        {
            bullet->shotInfo[bullet->numShots++].init(shot.attacker, shot.masterId, shot.damage, shot.hitLocation,
                                                      shot.entryAngle);
        }
    }

    /// <summary>
    /// Points a weapon effect from the elemental at <paramref name="target"/>; lasers and projectiles also carry
    /// <paramref name="shot"/> (a bullet's shots are added as it is loaded). Elementals fire from hot spot 0.
    /// </summary>
    void aimWeaponFX(Elemental* elemental, GameObject* fx, GameObject* target, _WeaponShotInfo& shot,
                     int32_t targetHotSpot)
    {
        if (fx->objectClass == BULLET)
        {
            auto* bullet = static_cast<Bullet*>(fx);
            bullet->owner = elemental;
            bullet->target = target;
            bullet->ownerHotSpot = 0;
            bullet->targetHotSpot = targetHotSpot;
        }
        else if (fx->objectClass == LASER)
        {
            auto* laser = static_cast<Laser*>(fx);
            laser->source.setWatcher(elemental);
            laser->target.setWatcher(target);
            laser->sourceHotSpot = 0;
            laser->targetHotSpot = targetHotSpot;
            laser->shotInfo.init(shot.attacker, shot.masterId, shot.damage, shot.hitLocation, shot.entryAngle);
        }
        else
        {
            auto* projectile = static_cast<ProjectileLaser*>(fx);
            projectile->owner = elemental;
            projectile->target = target;
            projectile->ownerHotSpot = 0;
            projectile->targetHotSpot = targetHotSpot;
            projectile->shotInfo.init(shot.attacker, shot.masterId, shot.damage, shot.hitLocation, shot.entryAngle);
        }
    }

    /// <summary>Sends a missed weapon effect from the elemental to <paramref name="landing"/>.</summary>
    void connectMissFX(Elemental* elemental, GameObject* fx, vector_3d& landing, _WeaponShotInfo& shot)
    {
        if (fx->objectClass == BULLET)
        {
            static_cast<Bullet*>(fx)->connect(elemental, landing, 0);
        }
        else if (fx->objectClass == LASER)
        {
            static_cast<Laser*>(fx)->connect(elemental, landing, &shot, 0);
        }
        else
        {
            static_cast<ProjectileLaser*>(fx)->connect(elemental, landing, &shot, 0);
        }
    }

    /// <summary>Where a missed shot lands: up to <see cref="MISS_SCATTER"/> about the target.</summary>
    /// <param name="centred">Missiles scatter both ways; other shots (as the original computes them) only one.</param>
    vector_3d missPoint(GameObject* target, int centred)
    {
        vector_3d miss;
        miss.x = MISS_SCATTER;
        miss.y = MISS_SCATTER;
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
        const vector_3d base = target->getPosition();
        miss.x += base.x;
        miss.y += base.y;
        miss.z += base.z;
        return miss;
    }

    /// <summary>
    /// Splits a missile flight into volleys: SRMs and LRMs fly in clusters (<see cref="ClusterSizeSRM"/>,
    /// <see cref="ClusterSizeLRM"/>), anything else in one.
    /// </summary>
    /// <returns>The number of volleys.</returns>
    int32_t missileVolleys(const MasterComponent& weapon, int32_t missiles, int32_t& volleySize)
    {
        volleySize = 1;

        if (weapon.missileType == 1 || weapon.missileType == 2)
        {
            volleySize = weapon.missileType == 1 ? ClusterSizeSRM : ClusterSizeLRM;
            int32_t numVolleys = missiles / volleySize;

            if (missiles % volleySize != 0)
            {
                numVolleys++;
            }

            return numVolleys;
        }

        return 1;
    }

    /// <summary>A turn of <paramref name="turn"/> degrees as a rotate request, no faster than
    /// <paramref name="maxTurn"/>.</summary>
    int8_t rotateRequest(float turn, float maxTurn)
    {
        return static_cast<int8_t>(static_cast<int32_t>(turn / maxTurn * 64.0f));
    }
}

auto loadElementalGameSystem(FitIniFile* sysFile) -> int32_t
{
    int32_t result = sysFile->seekBlock("Elemental:Collision");

    if (result != 0)
    {
        return result;
    }

    if ((result = sysFile->readIdFloat("DamageOnImpact", elmDamageOnImpact)) != 0)
    {
        return result;
    }

    if ((result = sysFile->seekBlock("Elemental:Combat")) != 0)
    {
        return result;
    }

    result = sysFile->readIdFloat("NoJumpRange", ElementalTargetNoJumpDistance);
    Assert(result == 0 ? 1 : 0, 0, " Unable to find Elemental NoJumpRange in gamesys.fit ");
    return 0;
}

//---------------------------------------------------------------------------
// ElementalType
//---------------------------------------------------------------------------

auto ElementalType::init() -> void
{
    canJump = 1;
    elementalId = 0;
    name = nullptr;
    alignment = 0;
    maxHealth = 0;
    unknown3C = 0;
    unknown4C = 0;
}

auto ElementalType::destroy() -> void
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

auto ElementalType::init(File* objFile, uint32_t fileSize) -> int32_t
{
    FitIniFile elementalFile;
    int32_t result = elementalFile.open(objFile, fileSize, 50);

    if (result != 0)
    {
        return result;
    }

    if ((result = elementalFile.seekBlock("Header")) != 0)
    {
        return result;
    }

    char fileType[128];

    if ((result = elementalFile.readIdString("FileType", fileType, 127)) != 0)
    {
        return result;
    }

    if (std::strcmp(fileType, "ElementalType") != 0)
    {
        return -1;
    }

    if ((result = elementalFile.seekBlock("General")) != 0)
    {
        return result;
    }

    if ((result = elementalFile.readIdULong("ID", elementalId)) != 0)
    {
        return result;
    }

    if (elementalFile.readIdBoolean("CanJump", canJump) != 0)
    {
        canJump = 1;
    }

    // "Type" 0 is 1, 1 is -1.
    static constexpr uint8_t alignmentMap[2] = {1, 0xff};
    uint8_t fileAlignment = 0;

    if ((result = elementalFile.readIdUChar("Type", fileAlignment)) != 0)
    {
        return result;
    }

    // Port fix: the original reads other values from past its two-entry table on the stack.
    alignment = fileAlignment < 2 ? alignmentMap[fileAlignment] : 0;
    char nameBuffer[128];
    elementalFile.readIdString("Name", nameBuffer, 127);
    name = static_cast<char*>(systemHeap->malloc(static_cast<uint32_t>(std::strlen(nameBuffer) + 1)));
    std::strcpy(name, nameBuffer);

    if ((result = elementalFile.readIdUChar("MaxHealth", maxHealth)) != 0)
    {
        return result;
    }

    if ((result = elementalFile.seekBlock("Dynamics")) != 0)
    {
        return result;
    }

    uint32_t dynamicsTypeId = 0;

    if ((result = elementalFile.readIdULong("Type", dynamicsTypeId)) != 0)
    {
        return result;
    }

    if (dynamicsTypeId != 3)
    {
        return -0x5fffd;
    }

    dynamicsType = new ElementalDynamicsType;

    if (dynamicsType == nullptr)
    {
        return -0x5fffe;
    }

    if ((result = dynamicsType->init(&elementalFile)) != 0)
    {
        return result;
    }

    return ObjectType::init(&elementalFile);
}

auto ElementalType::handleCollision(GameObject* collidee, GameObject* collider) -> int
{
    auto* elemental = static_cast<Elemental*>(collidee);

    switch (collider->objectClass)
    {
        case BATTLEMECH:
        case GROUNDVEHICLE:
        {
            // An enemy ramming it knocks an elemental aside; a marine is knocked aside by anything, every time.
            int knockedAside = 0;

            if (collidee->getPilot()->alignment != collider->getPilot()->alignment)
            {
                GameObject* collideeRamTarget = collidee->getPilot()->curTacOrder.getRamTarget();
                GameObject* colliderRamTarget = collider->getPilot()->curTacOrder.getRamTarget();

                if ((collideeRamTarget == collider || colliderRamTarget == collidee) &&
                    (collidee->getCollisionFreeFrom() != collider || collidee->getCollisionFreeTime() < scenarioTime))
                {
                    knockedAside = 1;
                }
            }

            if (knockedAside == 0 && elemental->elementalCanJump != 0)
            {
                return 0;
            }

            collidee->setCollisionFreeFrom(collider);
            collidee->setCollisionFreeTime(scenarioTime + 2.0f);
            turnAway(collidee, HALF_PI);
            collidee->getVelocity();
            const float entryAngle = collidee->relFacingTo(collider->getPosition(), -1);
            _WeaponShotInfo shotInfo;
            shotInfo.init(collider, -1, 5.0f, 0, entryAngle);
            collidee->handleWeaponHit(&shotInfo, 0);
            return 0;
        }

        case BUILDING:
        case TREEBUILDING:
        {
            if (startCollision(collidee, collider) == 0)
            {
                return 0;
            }

            // A big building turns the elemental further.
            const float angle = objectCollisionThreshold < collider->getObjectType()->extentRadius ? 135.0f : 45.0f;
            turnAway(collidee, angle * DEGREES_TO_RADIANS);
            collidee->getVelocity();
            const int32_t hitLocation = collidee->calcHitLocation(collider, -1, 1, 0);
            const float entryAngle = collidee->relFacingTo(collider->getPosition(), -1);
            const auto damage = static_cast<int32_t>(collider->getTonnage() * 0.1 + 0.5);
            _WeaponShotInfo shotInfo;
            shotInfo.init(collider, -1, static_cast<float>(damage), hitLocation, entryAngle);
            collidee->handleWeaponHit(&shotInfo, 0);
            break;
        }

        case TREE:
        {
            if (startCollision(collidee, collider) == 0)
            {
                return 0;
            }

            frame_of_ref frame = collidee->getFrame();
            collider->getObjectType();
            float deflection = 0.0f;

            if (collidee->getTonnage() < tonnageCollisionThreshold)
            {
                deflection = static_cast<float>(tonnageCollisionThreshold / collidee->getTonnage() * treeDeflection);
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
            if (collidee->getCollisionFreeFrom() != collider || collidee->getCollisionFreeTime() < scenarioTime)
            {
                collidee->setCollisionFreeFrom(collider);
                collidee->setCollisionFreeTime(scenarioTime + 2.0f);
                turnAway(collidee, HALF_PI);
                collidee->getVelocity();
            }

            return 0;
        }
        default:
            return 0;
    }

    soundSystem->playDigitalSample(4, 1, collidee, 0, 0);
    return 0;
}

auto ElementalType::handleDestruction(GameObject* collidee, GameObject* collider) -> int
{
    auto* elemental = static_cast<Elemental*>(collidee);

    if (elemental->getPilot() == nullptr)
    {
        Fatal(0, " No Pilot in this elemental! ");
    }

    if (elemental->getPoint() == elemental)
    {
        elemental->group->setPoint(nullptr);
    }

    if (elemental->sensorSystem != nullptr)
    {
        elemental->sensorSystem->disable();
    }

    if (elemental->unknown79C == 0)
    {
        elemental->unknown794 = 0.8f;
        elemental->getPilot()->triggerAlarm(7, collider == nullptr ? 0 : collider->idNumber);
    }
    else
    {
        elemental->unknown794 = 0.0f;
        elemental->getPilot()->triggerAlarm(8, 0);
    }

    elemental->status = 2;
    elemental->unknown8B8 = 0;
    elemental->unknown798 = 0;
    theInterface->RemoveMech(elemental->partId);

    // Original behaviour (OB-003): the type's alignment (1 or 0xff) against the home team's (1 or -1), so a clan
    // home team never counts its own.
    if (static_cast<uint32_t>(alignment) == static_cast<uint32_t>(homeTeam->alignment))
    {
        friendlyDestroyed = 1;
    }
    else
    {
        enemyDestroyed = 1;
    }

    return 1;
}

auto ElementalType::createInstance() -> BaseObject*
{
    auto* newElemental = new Elemental;

    if (newElemental == nullptr)
    {
        return nullptr;
    }

    if (newElemental->init(this) != 0)
    {
        return nullptr;
    }

    newElemental->idNumber = NextIdNumber++;
    return newElemental;
}

//---------------------------------------------------------------------------
// Elemental
//---------------------------------------------------------------------------

auto Elemental::getThrottle() -> int32_t
{
    return static_cast<ElementalControlData*>(control->controlData)->throttle;
}

auto Elemental::init() -> void
{
    objectClass = ELEMENTAL;
    jumpRange = 0.0f;
    jumpTime = -100.0f;
    inJump = 0;
    jumpGoal.z = 0.0f;
    jumpGoal.y = 0.0f;
    jumpGoal.x = 0.0f;
    unknown8B8 = 0;
    maxHealth = 11;
    curHealth = 11;
    unknown8C8 = 0;
    elementalCanJump = 1;
    transport = nullptr;
}

auto Elemental::init(ObjectType* objType) -> int32_t
{
    int32_t result = GameObject::init(objType);

    if (result != 0)
    {
        return result;
    }

    auto* elementalType = static_cast<ElementalType*>(objType);
    alignment = elementalType->alignment;
    collisionsOn = 1;
    maxHealth = elementalType->maxHealth;
    control = nullptr;
    dynamics = elementalType->dynamicsType->createInstance();

    if (dynamics == nullptr)
    {
        return -0x5fff8;
    }

    if ((result = dynamics->init(elementalType->dynamicsType, this)) != 0)
    {
        return result;
    }

    AppearanceType* apprType = appearanceTypeList->getAppearance(elementalType->appearName, 0);

    if (apprType == nullptr)
    {
        return -0x2fff7;
    }

    auto* actor = new ElementalActor;
    appearance = actor;

    if (actor == nullptr)
    {
        return -0x2ffff;
    }

    actor->init(nullptr, nullptr);

    if ((apprType->appearanceNum & 0xff000000) != 0x8000000)
    {
        return -0x5fff6;
    }

    if ((result = actor->init(apprType, this)) != 0)
    {
        return result;
    }

    objectClass = ELEMENTAL;
    unknown7C8 = 1000.0f;
    removed = 0;
    elementalCanJump = elementalType->canJump;
    return 0;
}

auto Elemental::setControl(uint32_t controlType, uint32_t controlData, int32_t controlParam) -> int32_t
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
            auto* aiControl = new ElementalAIControl;
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
            // Keeps the control it has.
            break;
        default:
            return -0x5fffb;
    }

    if (controlData != 3)
    {
        return -0x5fff9;
    }

    auto* elementalControlData = new ElementalControlData;
    control->controlData = elementalControlData;

    if (elementalControlData == nullptr)
    {
        return -0x5fffa;
    }

    return elementalControlData->init(0);
}

auto Elemental::init(FitIniFile* elementalFile) -> int32_t
{
    int32_t result = elementalFile->seekBlock("Header");

    if (result != 0)
    {
        return result;
    }

    char fileType[128];

    if ((result = elementalFile->readIdString("FileType", fileType, 127)) != 0)
    {
        return result;
    }

    if (std::strcmp(fileType, "ElementalProfile") != 0)
    {
        return -1;
    }

    if ((result = elementalFile->seekBlock("General")) != 0)
    {
        return result;
    }

    char nameBuffer[128];
    elementalFile->readIdString("Name", nameBuffer, 127);
    debugStatus = static_cast<char*>(systemHeap->malloc(static_cast<uint32_t>(std::strlen(nameBuffer) + 1)));
    std::strcpy(debugStatus, nameBuffer);

    if ((result = elementalFile->readIdFloat("CurTonnage", tonnage)) != 0)
    {
        return result;
    }

    if ((result = elementalFile->readIdLong("CurHealth", curHealth)) != 0)
    {
        return result;
    }

    if ((result = elementalFile->readIdString("icon", iconName, 0x13)) != 0)
    {
        return result;
    }

    if ((result = elementalFile->seekBlock("Engine")) != 0)
    {
        return result;
    }

    uint8_t moveSpeed = 0;

    if ((result = elementalFile->readIdUChar("MaxMoveSpeed", moveSpeed)) != 0)
    {
        return result;
    }

    maxRunSpeed = static_cast<float>(moveSpeed);

    if ((result = elementalFile->readIdFloat("JumpRange", jumpRange)) != 0)
    {
        return result;
    }

    if ((result = elementalFile->seekBlock("InventoryInfo")) != 0)
    {
        return result;
    }

    if ((result = elementalFile->readIdUChar("NumOther", numOther)) != 0)
    {
        return result;
    }

    if ((result = elementalFile->readIdUChar("NumWeapons", numWeapons)) != 0)
    {
        return result;
    }

    if ((result = elementalFile->readIdUChar("NumAmmo", numAmmos)) != 0)
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
    // An anti-missile system joins the list, whether it is listed with the other equipment or the weapons.
    const auto addAntiMissileSystem = [&](int32_t item)
    {
        const int32_t masterID = inventory[item].masterID;

        if (masterID != MasterClanAntiMissileSystemID && masterID != MasterInnerSphereAntiMissileSystemID)
        {
            return;
        }

        if (numAntiMissileSystems == 16)
        {
            Fatal(0, "Too many Anti-Missile Systems");
        }

        antiMissileSystem[numAntiMissileSystems] = static_cast<uint8_t>(item);
        numAntiMissileSystems++;
    };

    char blockName[128];

    for (int32_t item = 0; item < firstWeapon; item++)
    {
        std::sprintf(blockName, "Item:%d", item);

        if ((result = elementalFile->seekBlock(blockName)) != 0)
        {
            return result;
        }

        InventoryItem& other = inventory[item];

        if ((result = elementalFile->readIdUChar("MasterID", other.masterID)) != 0)
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
            case COMPONENT_FORM_WEAPON_BALLISTIC:
                addAntiMissileSystem(item);
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

        if ((result = elementalFile->seekBlock(blockName)) != 0)
        {
            return result;
        }

        InventoryItem& weapon = inventory[item];

        if ((result = elementalFile->readIdUChar("MasterID", weapon.masterID)) != 0)
        {
            return result;
        }

        if ((result = elementalFile->readIdUChar("FacesForward", weapon.facesForward)) != 0)
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

        if (MasterComponentList[inventory[item].masterID].form == COMPONENT_FORM_WEAPON_BALLISTIC)
        {
            addAntiMissileSystem(item);
        }

        objectTypeManager->load(
            static_cast<int32_t>(
                weaponFXTable[static_cast<int8_t>(MasterComponentList[inventory[item].masterID].weaponEffect)]),
            1);
    }

    for (int32_t item = firstAmmo; item < numItems; item++)
    {
        std::sprintf(blockName, "Item:%d", item);

        if ((result = elementalFile->seekBlock(blockName)) != 0)
        {
            return result;
        }

        InventoryItem& ammo = inventory[item];

        if ((result = elementalFile->readIdUChar("MasterID", ammo.masterID)) != 0)
        {
            return result;
        }

        int32_t amount = 0;

        if (elementalFile->readIdLong("Amount", amount) != 0)
        {
            uint8_t smallAmount = 0;

            if ((result = elementalFile->readIdUChar("Amount", smallAmount)) != 0)
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

    if (elementalCanJump == 0)
    {
        // Marines are worth a fixed amount.
        curCV = 50000;
        maxCV = 50000;
    }

    return 0;
}

auto Elemental::destroy() -> void
{
}

auto Elemental::calcCV(int calcMax) -> int32_t
{
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

    // Defense: health, tonnage, the speed class and the other equipment.
    double defense = static_cast<double>(calcMax != 0 ? maxHealth : curHealth);
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

auto Elemental::isJumping(vector_3d* jumpGoalOut) -> int
{
    if (jumpGoalOut != nullptr)
    {
        *jumpGoalOut = jumpGoal;
    }

    return inJump;
}

auto Elemental::getJumpRange(int32_t* numOffsets, int32_t* jumpCost) -> float
{
    if (elementalCanJump == 0)
    {
        return 0.0f;
    }

    if (numOffsets != nullptr)
    {
        *numOffsets = 32;
    }

    if (jumpCost != nullptr)
    {
        *jumpCost = 20;
    }

    return metersPerWorldUnit * Terrain::metersPerVertex;
}

auto Elemental::updateJump() -> int
{
    if (isJumping(nullptr) == 0)
    {
        return 0;
    }

    auto* controlData = static_cast<ElementalControlData*>(control->controlData);
    auto* actor = static_cast<ElementalActor*>(appearance);

    if (actor->jumping == 0)
    {
        if (actor->jumpSetup == 0)
        {
            // Landed: on to the step after the jump.
            inJump = 0;
            MovePath* path = pilot->getMovePath();
            path->numSteps = path->numStepsWhenNotPaused;
            path->curStep++;
            lastValidPosition = position;
            return 1;
        }

        controlData->jump = 1;
        controlData->jumpDistance = distanceFrom(jumpGoal);
        controlData->throttle = 0;
        return 1;
    }

    // In the air: turns toward the landing point.
    auto* dynType = static_cast<ElementalDynamicsType*>(static_cast<ElementalType*>(objType)->dynamicsType);
    const float facing = relFacingTo(jumpGoal, -1);
    double maxTurn = static_cast<double>(dynType->maxElementalYawRate);

    if (maxTurn > 180.0)
    {
        maxTurn = 180.0f;
    }

    if (facing >= -5.0 && facing <= 5.0)
    {
        return 1;
    }

    double turn = -(facing / frameLength);

    if (turn > maxTurn)
    {
        turn = maxTurn;
    }
    else
    {
        const auto minTurn = static_cast<float>(-maxTurn);

        if (turn < minTurn)
        {
            turn = minTurn;
        }
    }

    controlData->rotate = static_cast<int8_t>(static_cast<int32_t>(turn / maxTurn * 64.0f));
    return 1;
}

auto Elemental::pivotTo() -> int
{
    MechWarrior* warrior = pilot;
    MovePath* path = warrior->getMovePath();
    const int32_t moveState = warrior->moveOrders.moveState;
    const int32_t moveStateGoal = warrior->moveOrders.moveStateGoal;
    auto* controlData = static_cast<ElementalControlData*>(control->controlData);
    auto* dynType = static_cast<ElementalDynamicsType*>(static_cast<ElementalType*>(objType)->dynamicsType);

    // Starts a turn of <turn> degrees, no faster than the yaw rate allows this frame.
    const auto pivot = [&](float turn) -> int
    {
        float maxTurn = static_cast<float>(dynType->maxElementalYawRate) * frameLength;

        if (maxTurn > 180.0)
        {
            maxTurn = 180.0f;
        }

        if (static_cast<float>(std::abs(static_cast<int32_t>(turn))) > maxTurn)
        {
            turn = turn > 0.0f ? maxTurn : -maxTurn;
        }

        controlData->rotate = rotateRequest(turn, maxTurn);
        return 1;
    };

    const auto hasNextStep = [&]()
    { return path->numStepsWhenNotPaused > 0 && path->curStep < path->numStepsWhenNotPaused; };

    if (moveState == MOVESTATE_PIVOT_FORWARD)
    {
        if (moveStateGoal != MOVESTATE_PIVOT_FORWARD && moveStateGoal != MOVESTATE_FORWARD)
        {
            warrior->moveOrders.moveState = MOVESTATE_FORWARD;
        }
        else if (!hasNextStep())
        {
            warrior->moveOrders.moveStateGoal = MOVESTATE_FORWARD;
        }
        else
        {
            const vector_3d destination = path->stepList[path->curStep].destination;
            appearance->setGestureGoal(0);
            controlData->throttle = 0;
            const float facing = relFacingTo(destination, -1);

            if (facing < -15.0 || facing > 15.0)
            {
                return pivot(-facing);
            }

            pilot->moveOrders.moveState = MOVESTATE_FORWARD;
        }
    }
    else if (moveState == MOVESTATE_PIVOT_REVERSE)
    {
        if (moveStateGoal != MOVESTATE_PIVOT_REVERSE && moveStateGoal != MOVESTATE_REVERSE)
        {
            warrior->moveOrders.moveState = MOVESTATE_FORWARD;
        }
        else if (!hasNextStep())
        {
            warrior->moveOrders.moveStateGoal = MOVESTATE_FORWARD;
        }
        else
        {
            const vector_3d destination = path->stepList[path->curStep].destination;
            appearance->setGestureGoal(0);
            // Original behaviour (OB-004): it clears the second byte of the jump request, not the throttle.
            controlData->jump &= ~0xff00;
            const float facing = relFacingTo(destination, -1);

            if (facing > -165.0 && facing < 165.0)
            {
                return pivot(-(facing < 0.0f ? facing + 180.0f : facing - 180.0f));
            }

            if (moveStateGoal == MOVESTATE_REVERSE)
            {
                pilot->moveOrders.moveState = MOVESTATE_REVERSE;
            }
            else
            {
                pilot->moveOrders.moveStateGoal = MOVESTATE_FORWARD;
            }
        }
    }
    else if (moveState == MOVESTATE_PIVOT_TARGET)
    {
        if (moveStateGoal != MOVESTATE_PIVOT_TARGET)
        {
            warrior->moveOrders.moveState = MOVESTATE_FORWARD;
        }
        else
        {
            vector_3d targetPosition;
            GameObject* target = warrior->getLastTarget();

            if (target != nullptr)
            {
                targetPosition = target->getPosition();
            }
            else if (warrior->curTacOrder.code == TACTICAL_ORDER_ATTACK_POINT)
            {
                targetPosition = warrior->attackOrders.targetPoint;
            }
            else
            {
                warrior->moveOrders.moveStateGoal = MOVESTATE_FORWARD;
                warrior->getMovePath()->numSteps = warrior->getMovePath()->numStepsWhenNotPaused;
                return 0;
            }

            appearance->setGestureGoal(0);
            controlData->throttle = 0;
            const float facing = relFacingTo(targetPosition, -1);
            const float fireArc = getFireArc();

            if (facing < -fireArc || fireArc < facing)
            {
                return pivot(-facing);
            }

            pilot->moveOrders.moveStateGoal = MOVESTATE_FORWARD;
        }
    }
    else if (moveStateGoal == MOVESTATE_PIVOT_TARGET || moveStateGoal == MOVESTATE_PIVOT_FORWARD ||
             moveStateGoal == MOVESTATE_PIVOT_REVERSE)
    {
        warrior->moveOrders.moveState = moveStateGoal;
    }

    warrior = pilot;

    if (warrior->moveOrders.yieldTime <= -1.0)
    {
        warrior->getMovePath()->numSteps = warrior->getMovePath()->numStepsWhenNotPaused;
    }

    return 0;
}

auto Elemental::updateMoveStateGoal() -> void
{
    MechWarrior* warrior = pilot;
    MovePath* path = warrior->getMovePath();

    if (path->numSteps > 0 || (warrior->moveOrders.moveStateGoal != MOVESTATE_PIVOT_TARGET &&
                               warrior->moveOrders.moveStateGoal != MOVESTATE_PIVOT_FORWARD))
    {
        warrior->moveOrders.moveStateGoal = MOVESTATE_FORWARD;
    }
}

auto Elemental::updateMovePath(char& newRotate, char& newThrottleSetting, float& newRotatePerSec,
                               int32_t& newGestureStateGoal, int32_t& newMoveState, int32_t& minThrottle,
                               int32_t& maxThrottle) -> int
{
    MechWarrior* warrior = pilot;
    auto* controlData = static_cast<ElementalControlData*>(control->controlData);
    auto* dynType = static_cast<ElementalDynamicsType*>(static_cast<ElementalType*>(objType)->dynamicsType);
    MovePath* path = warrior->getMovePath();
    newThrottleSetting = controlData->throttle;
    newRotatePerSec = 0.0f;

    if (path->numSteps < 1)
    {
        newGestureStateGoal = 0;
        return 0;
    }

    // The end of the path: done, unless more global steps are to come.
    const auto reachedEnd = [&]() -> int
    {
        int finished = 1;

        if (warrior->moveOrders.pathType == 2 &&
            warrior->moveOrders.path[0]->globalStep < warrior->moveOrders.numGlobalSteps - 1)
        {
            finished = 0;
        }

        if (warrior->moveOrders.path[0] != nullptr)
        {
            warrior->moveOrders.path[0]->clear();
        }

        return finished;
    };

    // A step whose direction is past 7 is a jump.
    const auto isJumpStep = [&](int32_t step) { return static_cast<int8_t>(path->stepList[step].direction) > 7; };

    int32_t step = path->curStep;

    if (step == path->numSteps)
    {
        return reachedEnd();
    }

    newGestureStateGoal = 1;
    vector_3d destination = path->stepList[step].destination;
    lastValidPosition = destination;
    const float distance = distanceFrom(destination);
    // (The original also measures the distance to the path object itself, and drops it.)
    const float margin = step == path->numSteps - 1 ? MoveMarginOfError[1] : MoveMarginOfError[0];

    if (distance < margin)
    {
        // Reached the step: on to the next.
        step++;
        pilot->moveOrders.timeOfLastStep = scenarioTime;
        path->curStep = step;

        if (path->numSteps <= step)
        {
            return reachedEnd();
        }

        if (isJumpStep(step))
        {
            newGestureStateGoal = 2;
            return 0;
        }

        destination = path->stepList[step].destination;
    }
    else if (isJumpStep(step))
    {
        newGestureStateGoal = 2;
        return 0;
    }

    const float facing = relFacingTo(destination, -1);
    warrior = pilot;
    const int32_t moveState = warrior->moveOrders.moveState;
    const int32_t moveStateGoal = warrior->moveOrders.moveStateGoal;
    // Stops on the path to change the move state.
    const auto changeState = [&](int32_t state)
    {
        warrior->getMovePath()->numSteps = 0;
        newMoveState = state;
        return 0;
    };

    // Turns toward the step, no faster than the yaw rate allows this frame.
    const auto steer = [&]()
    {
        float maxTurn = static_cast<float>(dynType->maxElementalYawRate) * frameLength;

        if (maxTurn > 180.0)
        {
            maxTurn = 180.0f;
        }

        if (std::fabs(newRotatePerSec) > maxTurn)
        {
            newRotatePerSec = newRotatePerSec <= 0.0 ? -maxTurn : maxTurn;
        }

        newRotate = static_cast<char>(rotateRequest(newRotatePerSec, maxTurn));
        return 0;
    };

    if (moveState == MOVESTATE_FORWARD)
    {
        switch (moveStateGoal)
        {
            case MOVESTATE_FORWARD:
            {
                newGestureStateGoal = 1;
                newThrottleSetting = 100;

                if (facing >= -5.0 && facing <= 5.0)
                {
                    return 0;
                }

                newRotatePerSec = -facing;
                return steer();
            }
            case MOVESTATE_PIVOT_FORWARD:
                return changeState(MOVESTATE_PIVOT_FORWARD);
            case MOVESTATE_REVERSE:
            case MOVESTATE_PIVOT_REVERSE:
                return changeState(MOVESTATE_PIVOT_REVERSE);
            default:
                return changeState(MOVESTATE_FORWARD);
        }
    }

    if (moveState == MOVESTATE_REVERSE)
    {
        switch (moveStateGoal)
        {
            case MOVESTATE_REVERSE:
            {
                newGestureStateGoal = 1;
                newThrottleSetting = -100;
                newRotatePerSec = facing >= 0.0f ? -(facing - 180.0f) : -(facing + 180.0f);
                return steer();
            }
            case MOVESTATE_FORWARD:
            case MOVESTATE_PIVOT_FORWARD:
                return changeState(MOVESTATE_PIVOT_FORWARD);
            case MOVESTATE_PIVOT_REVERSE:
                return changeState(MOVESTATE_PIVOT_REVERSE);
            default:
                return changeState(MOVESTATE_FORWARD);
        }
    }

    switch (moveStateGoal)
    {
        case MOVESTATE_FORWARD:
        case MOVESTATE_PIVOT_FORWARD:
            return changeState(MOVESTATE_PIVOT_FORWARD);
        case MOVESTATE_REVERSE:
        case MOVESTATE_PIVOT_REVERSE:
            return changeState(MOVESTATE_PIVOT_REVERSE);
        default:
            return 0;
    }
}

auto Elemental::setNextMovePath(char& newThrottleSetting, int32_t& newGestureStateGoal) -> void
{
    MechWarrior* warrior = pilot;
    vector_3d nextWayPoint;

    if (warrior->getNextWayPoint(nextWayPoint, 1) != 0)
    {
        warrior->setMoveGoal(0, &nextWayPoint, nullptr);
        warrior->requestMovePath(warrior->curTacOrder.selectionIndex, 1, 0);
        return;
    }

    warrior->clearMoveOrders();
    newGestureStateGoal = 0;
}

auto Elemental::setControlSettings(char& newRotate, char& newThrottleSetting, float& newRotatePerSec,
                                   int32_t& newGestureStateGoal, int32_t& minThrottle, int32_t& maxThrottle) -> void
{
    auto* actor = static_cast<ElementalActor*>(appearance);

    if (inJump != 0 && actor->jumping == 0)
    {
        // The jump is over: back on the path.
        inJump = 0;
        MovePath* path = pilot->getMovePath();
        path->numSteps = path->numStepsWhenNotPaused;
    }

    if (newGestureStateGoal == 2)
    {
        // A jump step: jump to it.
        MovePath* path = pilot->getMovePath();
        path->numSteps = 0;
        jumpGoal = path->stepList[path->curStep].destination;
        actor->setJumpParameters(distanceFrom(jumpGoal));
    }

    MechWarrior* warrior = pilot;

    if (warrior->curTacOrder.isJumpOrder() != 0 && inJump == 0)
    {
        // A jump order: to its first way point.
        newGestureStateGoal = 2;
        const float* point = warrior->curTacOrder.moveParams.wayPath.points;
        jumpGoal.x = point[0];
        jumpGoal.y = point[1];
        jumpGoal.z = point[2];
        actor->setJumpParameters(distanceFrom(jumpGoal));
    }

    auto* controlData = static_cast<ElementalControlData*>(control->controlData);

    if (newGestureStateGoal != -1)
    {
        switch (newGestureStateGoal)
        {
            case 0:
                newThrottleSetting = 0;
                break;
            case 1:
                newThrottleSetting = 100;
                break;
            case 2:
            {
                inJump = 1;
                newThrottleSetting = 0;
                break;
            }
            default:
                break;
        }

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
}

auto Elemental::updateMovement() -> void
{
    auto* controlData = static_cast<ElementalControlData*>(control->controlData);

    if (disableThisFrame != 0 || shutDownThisFrame != 0 || startUpThisFrame != 0)
    {
        // Elementals just stand still for these; the requests clear once the actor stops.
        if (appearance->setGestureGoal(0) == 0)
        {
            disableThisFrame = 0;
            shutDownThisFrame = 0;
            startUpThisFrame = 0;
        }

        controlData->throttle = 0;
        return;
    }

    if (isCaptured() != 0 || status == 4 || status == 5 || status == 1)
    {
        return;
    }

    if (updateJump() != 0)
    {
        return;
    }

    if (pivotTo() != 0)
    {
        return;
    }

    float newRotatePerSec = 0.0f;
    int32_t minThrottle = -100;
    int32_t maxThrottle = 100;
    char newRotate = 0;
    int32_t newMoveState = -1;
    int32_t newGestureStateGoal = -1;
    char newThrottleSetting = controlData->throttle;

    if (elementalCanJump == 0)
    {
        MechWarrior* warrior = pilot;

        if (warrior->getMovePath()->numSteps == 0)
        {
            // An idle marine wanders. Original behaviour (OB-001): RollDice(100) is always 1, so always toward -x, -y.
            vector_3d wanderPoint = position;

            if (RollDice(100) < 51)
            {
                wanderPoint.x = wanderPoint.x - static_cast<float>(RandomNumber(200));
            }
            else
            {
                wanderPoint.x = static_cast<float>(RandomNumber(200)) + wanderPoint.x;
            }

            if (RollDice(100) < 51)
            {
                wanderPoint.y = wanderPoint.y - static_cast<float>(RandomNumber(200));
            }
            else
            {
                wanderPoint.y = static_cast<float>(RandomNumber(200)) + wanderPoint.y;
            }

            warrior->orderMoveToPoint(0, 1, 0, wanderPoint, -1, 1);
        }
    }

    updateMoveStateGoal();

    if (updateMovePath(newRotate, newThrottleSetting, newRotatePerSec, newGestureStateGoal, newMoveState, minThrottle,
                       maxThrottle) != 0)
    {
        setNextMovePath(newThrottleSetting, newGestureStateGoal);
    }

    if (newMoveState != -1)
    {
        pilot->moveOrders.moveState = newMoveState;
    }

    setControlSettings(newRotate, newThrottleSetting, newRotatePerSec, newGestureStateGoal, minThrottle, maxThrottle);
}

auto Elemental::getPositionFromHS(uint32_t hotSpot) -> vector_3d
{
    return position;
}

auto Elemental::onScreen() -> int
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

        screenY = screenY + camera->halfHeight;
    }
    else
    {
        const float scale = camera->cameraScale != 1 ? 1.0f : 0.5f;
        vector_3d relative(position.x - camera->position.x, position.y - camera->position.y,
                           position.z - camera->position.z);
        relative *= scale;
        screenPos.x = relative.y * camera->cosAngle + relative.x * camera->cosAngle + camera->halfWidth;
        screenY = ((relative.x * camera->sinAngle + camera->halfHeight) - relative.y * camera->sinAngle) - relative.z;
    }

    screenPos.y = screenY;

    if (appearance != nullptr && appearance->recalcBounds(camera) != 0)
    {
        windowsVisible = turn;
        return 1;
    }

    return 0;
}

auto Elemental::update() -> int32_t
{
    if (inTransport() != 0)
    {
        return 1;
    }

    const float blockSize = static_cast<float>(Terrain::verticesBlockSide) * Terrain::metersPerVertex;

    if (isDestroyed() != 0)
    {
        if (removed != 0)
        {
            return 1;
        }

        // The body: when the death timer runs low it blows up and leaves a crater.
        unknown794 -= frameLength;

        if (unknown794 < 0.4 && unknown798 == 0 && unknown79C == 0)
        {
            objType->createExplosion(position, 0.0f, 0.0f);
            craterManager->addCrater(7, position, 0);
            unknown798 = 1;
            return 1;
        }

        if (unknown794 < 0.0)
        {
            return 1;
        }
    }
    else
    {
        if (deselectTime != 0.0f && deselectTime < scenarioTime)
        {
            deselectTime = 0.0f;
            selected = 0;
        }

        if (getAwake() != 0 && isDisabled() == 0 && Terrain::metersPerVertex <= unknown7C8)
        {
            // Every vertex travelled, the elemental marks what it sees.
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

        auto* controlData = static_cast<ElementalControlData*>(control->controlData);
        auto* actor = static_cast<ElementalActor*>(appearance);

        if (controlData->jump != 0)
        {
            actor->setJumpParameters(controlData->jumpDistance);
            controlData->throttle = 0;
        }

        // In the air the actor moves it (and nothing collides with it); on the ground the dynamics do.
        float speed = actor->getVelocityMagnitude();

        if (actor->currentGesture == 2)
        {
            collisionsOn = 0;
        }
        else
        {
            speed = dynamics->getVelocity();
            collisionsOn = 1;
        }

        frame_of_ref turned = frame;
        speed = -speed;
        rotateAboutK(turned, static_cast<float>(std::sin(HALF_PI / 2.0)), static_cast<float>(std::cos(HALF_PI / 2.0)));
        velocity.y = turned.j.y * speed;
        velocity.x = turned.j.x * speed;
        velocity.z = turned.j.z * speed;
        vector_3d move;
        move.x = velocity.x * frameLength * worldUnitsPerMeter;
        move.y = velocity.y * frameLength * worldUnitsPerMeter;
        move.z = velocity.z * frameLength * worldUnitsPerMeter;
        vector_3d newPosition;
        newPosition.x = move.x + position.x;
        newPosition.y = move.y + position.y;
        newPosition.z = move.z + position.z;
        setPosition(newPosition);
        unknown7C8 = std::sqrt(move.z * move.z + move.y * move.y + move.x * move.x) + unknown7C8;
        position.z = land->getTerrainElevation(position);

        const int visibleNow = onScreen();
        const int offScreen = visibleNow == 0 ? 1 : 0;

        if (unknown79C != 0)
        {
            if (visibleNow == 0)
            {
                objType->handleDestruction(this, nullptr);
                removed = 1;
            }
        }

        // Original behaviour (OB-002): a marine off the screen is removed.
        if (offScreen && elementalCanJump == 0)
        {
            removeMarine(this, 0.8f);
            removed = 1;
        }

        if (appearance != nullptr)
        {
            appearance->visible = visibleNow;
            appearance->update();
        }
    }

    // Original behaviour (OB-005): adds the map's top edge to y here rather than subtracting.
    const float blockColumn = (position.x - Terrain::mapTopLeft3d100.x) / blockSize;
    const auto blockRow =
        static_cast<int32_t>(std::floor(static_cast<double>((Terrain::mapTopLeft3d100.y + position.y) / blockSize)));
    const auto column = static_cast<int32_t>(std::floor(static_cast<double>(blockColumn)));
    addMoverToList(column + blockRow * Terrain::blocksMapSide);
    return 1;
}

auto Elemental::render() -> void
{
    int tagged = 0;

    if (isDestroyed() == 0)
    {
        if (alignment == homeTeam->alignment)
        {
            if (windowsVisible == turn)
            {
                appearance->render(0);
            }
        }
        else
        {
            const int32_t contactType = getContactType(homeTeam->id, tagged);

            if (contactType == 1)
            {
                if (windowsVisible == turn)
                {
                    appearance->render(0);
                }
            }
            else if (contactType == 2)
            {
                // A sensor contact: a blip sized by tonnage.
                uint8_t* shape;

                if (50.0f < getTonnage())
                {
                    shape = scenario->sensorContactShapes[0];
                }
                else if (35.0f < getTonnage())
                {
                    shape = scenario->sensorContactShapes[2];
                }
                else
                {
                    shape = scenario->sensorContactShapes[4];
                }

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
                    ElementList->add(new VFXElement(shape, screenPos.x, screenPos.y, blipFrame, 0, nullptr, 0, 0));
                    blipFrame++;
                }
            }
            else if (elementalCanJump == 0)
            {
                // An unseen marine: drawn once revealed. Original behaviour (OB-002): removed when it isn't on the
                // screen.
                if (windowsVisible == 0)
                {
                    onScreen();
                }

                if (windowsVisible == turn)
                {
                    if (isRevealed() != 0)
                    {
                        appearance->render(0);
                    }
                }
                else
                {
                    removeMarine(this, 0.8f);
                    removed = 1;
                }
            }
        }
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
            const auto project = [](const vector_3d& point)
            {
                const float scale = eye->cameraScale != 1 ? 1.0f : 0.5f;
                const float sx = (point.x - eye->position.x) * scale;
                const float sy = (point.y - eye->position.y) * scale;
                vector_2d screen;
                screen.x = sx * eye->cosAngle + sy * eye->cosAngle + eye->halfWidth;
                screen.y =
                    ((sx * eye->sinAngle + eye->halfHeight) - sy * eye->sinAngle) - scale * (point.z - eye->position.z);
                return screen;
            };

            vector_2d fromScreen = project(from);
            vector_2d toScreen = project(to);
            ElementList->openGroup(-100000, 1);
            ElementList->add(new LineElement(fromScreen, toScreen, 0xfe, nullptr, -100000, -1));
        }
    }
}

auto Elemental::getBodyState() -> int32_t
{
    return 0;
}

auto Elemental::calcAttackChance(GameObject* target, int32_t aimLocation, float targetTime, int32_t weaponIndex,
                                 float modifiers, int32_t* range, vector_3d* targetPoint) -> float
{
    if (numOther <= weaponIndex && weaponIndex < numOther + numWeapons)
    {
        return Mover::calcAttackChance(target, aimLocation, targetTime, weaponIndex, modifiers, range, targetPoint);
    }

    return -1000.0f;
}

auto Elemental::calcHitLocation(GameObject* attacker, int32_t weaponIndex, int32_t attackSource, int32_t attackType)
    -> int32_t
{
    return 0;
}

auto Elemental::hitInventoryItem(int32_t itemIndex, int setupOnly) -> int
{
    return 0;
}

auto Elemental::handleWeaponHit(_WeaponShotInfo* shotInfo, int addMultiplayChunk) -> int32_t
{
    if (shotInfo == nullptr)
    {
        return 0;
    }

    GameObject* attacker = shotInfo->attacker;
    BadGuy = attacker;

    if (shotInfo->damage <= 0.0f || shotInfo->hitLocation == -1)
    {
        return 0;
    }

    if (isDestroyed() != 0)
    {
        return 0;
    }

    curHealth = static_cast<int32_t>(curHealth - shotInfo->damage);

    if (curHealth < 1)
    {
        pilot->handleOwnVehicleIncapacitation(0);

        if (elementalCanJump == 0)
        {
            removeMarine(this, 0.0f);
        }
        else
        {
            objType->handleDestruction(this, nullptr);
        }
    }

    damageRateTally = shotInfo->damage + damageRateTally;
    totalDamageTaken = shotInfo->damage + totalDamageTaken;

    if (attacker == nullptr)
    {
        pilot->triggerAlarm(1, 0);
    }
    else if (shotInfo->masterId < 0)
    {
        pilot->triggerAlarm(10, static_cast<uint32_t>(attacker->partId));
    }
    else
    {
        pilot->triggerAlarm(1, static_cast<uint32_t>(attacker->partId));
    }

    curCV = calcCV(0);
    return 0;
}

auto Elemental::fireWeapon(GameObject* target, float targetTime, int32_t weaponIndex, int32_t attackType,
                           int32_t aimLocation, vector_3d* targetPoint) -> int32_t
{
    if (status == 5 || status == 4 || status == 1 || status == 2)
    {
        return 1;
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

    const int32_t numShots = getWeaponShots(weaponIndex);

    if (numShots == 0)
    {
        return 4;
    }

    // No aimed missiles.
    if (aimLocation != -1 && weapon.form == COMPONENT_FORM_WEAPON_MISSILE)
    {
        return 4;
    }

    // Port fix: from here the original assumes a target (it dereferences it for the entry angle).
    if (target == nullptr)
    {
        return 4;
    }

    const float entryAngle = target->relFacingTo(position, -1);
    const int isStreak = weapon.weaponFlags & 1;
    int32_t range = 0;
    int32_t hitChance =
        static_cast<int32_t>(calcAttackChance(target, aimLocation, targetTime, weaponIndex, 0.0f, &range, nullptr));
    const int32_t hitRoll = RandomNumber(100);
    pilot->numSkillUses[MWS_GUNNERY][1]++;
    int32_t hitLocation = -1;

    if (hitRoll < hitChance)
    {
        pilot->numSkillSuccesses[MWS_GUNNERY][1]++;

        if (aimLocation != -1)
        {
            hitLocation = aimLocation;
        }
    }

    MechWarrior* targetPilot = nullptr;
    const int32_t targetClass = target->objectClass;

    if (targetClass == BATTLEMECH || targetClass == GROUNDVEHICLE || targetClass == ELEMENTAL || targetClass == MOVER)
    {
        targetPilot = target->getPilot();
        targetPilot->updateAttackerStatus(static_cast<uint32_t>(partId), scenarioTime);
    }

    // Aimed shots only from a standing elemental.
    if (aimLocation != -1 && 0.0 < getVelocity().magnitude())
    {
        hitChance = 0;
    }

    startWeaponRecycle(weaponIndex);

    InventoryItem& item = inventory[weaponIndex];
    const auto fired = [&]() -> const MasterComponent& { return MasterComponentList[item.masterID]; };
    const auto hotSpotOf = [&](int32_t location)
    {
        if (target->objectClass == BATTLEMECH)
        {
            // Port fix: the original reads body[location], past the eight body locations for a rear torso hit
            // (8..10); the torso it maps to is read instead.
            const BodyLocation& body = static_cast<BattleMech*>(target)->bodyAt(MechArmorToBodyLocation[location]);
            return static_cast<int32_t>(body.hotSpotNumber);
        }

        return 0;
    };

    GameObject* fx = nullptr;

    if (hitRoll < hitChance)
    {
        if (numShots != UNLIMITED_SHOTS)
        {
            deductWeaponShot(weaponIndex, 1);
        }

        if (fired().form == COMPONENT_FORM_WEAPON_MISSILE)
        {
            // Missiles: a streak fires them all, anything else about half; anti-missile systems take some out.
            // The rest fly in volleys, each with its own hit location.
            int32_t missiles = fired().numMissiles;

            if (isStreak == 0)
            {
                missiles = static_cast<int32_t>(missiles * 0.5 + 0.5);
            }

            int32_t antiMissileShots = 0;
            missiles = target->fireAntiMissileSystem(missiles, antiMissileShots);

            if (antiMissileShots > 0)
            {
                target->reduceAntiMissileAmmo(antiMissileShots);
            }

            int32_t volleySize = 1;
            const int32_t numVolleys = missileVolleys(fired(), missiles, volleySize);

            if (numVolleys != 0)
            {
                fx = createObject(static_cast<int32_t>(weaponFXTable[fired().weaponEffect]));
                int32_t targetHotSpot = 0;
                _WeaponShotInfo shot;

                for (int32_t volley = 0; volley < numVolleys; volley++)
                {
                    if (missiles < volleySize)
                    {
                        volleySize = missiles;
                    }

                    missiles -= volleySize;

                    if (aimLocation == -1)
                    {
                        hitLocation = target->calcHitLocation(this, weaponIndex, 0, attackType);
                    }

                    Assert(hitLocation != -1 ? 1 : 0, 0, " Elemental.FireWeapon: Bad Hit Location ");

                    if (volley == 0)
                    {
                        targetHotSpot = hotSpotOf(hitLocation);
                    }

                    shot.init(this, item.masterID, fired().damage * static_cast<float>(volleySize), hitLocation,
                              entryAngle);

                    if (fx->objectClass == BULLET)
                    {
                        addBulletShot(static_cast<Bullet*>(fx), shot);
                    }
                }

                aimWeaponFX(this, fx, target, shot, targetHotSpot);
            }
        }
        else
        {
            if (aimLocation == -1)
            {
                hitLocation = target->calcHitLocation(this, weaponIndex, 0, attackType);
            }

            Assert(hitLocation != -1 ? 1 : 0, 0, " Elemental.FireWeapon: Bad Hit Location ");
            _WeaponShotInfo shot;
            shot.init(this, item.masterID, fired().damage, hitLocation, entryAngle);
            fx = createObject(static_cast<int32_t>(weaponFXTable[fired().weaponEffect]));

            if (fx->objectClass == BULLET)
            {
                addBulletShot(static_cast<Bullet*>(fx), shot);
            }

            aimWeaponFX(this, fx, target, shot, hotSpotOf(hitLocation));
        }
    }
    else if (isStreak == 0)
    {
        // A miss (a streak doesn't fire without a lock): the shot lands somewhere near.
        if (numShots != UNLIMITED_SHOTS)
        {
            deductWeaponShot(weaponIndex, 1);
        }

        _WeaponShotInfo shot;

        if (fired().form == COMPONENT_FORM_WEAPON_MISSILE)
        {
            int32_t missiles = static_cast<int32_t>(fired().numMissiles * 0.5 + 0.5);
            int32_t volleySize = 1;
            const int32_t numVolleys = missileVolleys(fired(), missiles, volleySize);

            if (numVolleys != 0)
            {
                fx = createObject(static_cast<int32_t>(weaponFXTable[fired().weaponEffect]));

                for (int32_t volley = 0; volley < numVolleys; volley++)
                {
                    if (missiles < volleySize)
                    {
                        volleySize = missiles;
                    }

                    missiles -= volleySize;
                    shot.init(this, item.masterID, fired().damage * static_cast<float>(volleySize), -1, entryAngle);

                    if (fx->objectClass == BULLET)
                    {
                        addBulletShot(static_cast<Bullet*>(fx), shot);
                    }
                }

                vector_3d landing = missPoint(target, 1);
                connectMissFX(this, fx, landing, shot);
            }
        }
        else
        {
            shot.init(this, item.masterID, fired().damage, -1, entryAngle);
            fx = createObject(static_cast<int32_t>(weaponFXTable[fired().weaponEffect]));
            vector_3d landing = missPoint(target, 0);

            if (fx->objectClass == BULLET)
            {
                addBulletShot(static_cast<Bullet*>(fx), shot);
            }

            connectMissFX(this, fx, landing, shot);
        }
    }

    if (fx != nullptr)
    {
        weaponList->addNode(fx);
    }

    if (targetPilot != nullptr)
    {
        targetPilot->triggerAlarm(0, static_cast<uint32_t>(partId));
    }

    // Firing gives an unrevealed elemental away to the other side's mechs within visual range.
    ObjectQueueNode* enemies = nullptr;
    uint8_t seenBy = 0;

    if (alignment == 1 && isRevealed() == 0)
    {
        enemies = clanMechList;
        seenBy = 2;
    }
    else if (alignment == -1 && isRevealed() == 0)
    {
        enemies = innerSphereMechList;
        seenBy = 1;
    }

    if (enemies != nullptr)
    {
        for (BaseObject* enemy = enemies->head; enemy != nullptr; enemy = enemy->next)
        {
            vector_3d enemyPosition = static_cast<GameObject*>(enemy)->getPosition();

            if (distanceFrom(enemyPosition) < scenario->maxVisualRange)
            {
                land->markRadiusSeen(position, frame.j, 360.0f, scenario->fireVisualRange, seenBy);
                break;
            }
        }
    }

    if (group != nullptr)
    {
        group->handleMateFiredWeapon(static_cast<uint32_t>(partId));
    }

    return 0;
}

auto Elemental::getVitalInfo(void* vitalInfo) -> int32_t
{
    const int32_t size = Mover::getVitalInfo(nullptr);

    if (vitalInfo != nullptr)
    {
        Mover::getVitalInfo(vitalInfo);
        auto* info = reinterpret_cast<uint8_t*>(vitalInfo) + size;
        std::memcpy(info, &jumpRange, 4);
        std::memcpy(info + 4, &unknown8BC, 4);
        std::memcpy(info + 8, &maxHealth, 4);
        std::memcpy(info + 12, &curHealth, 4);
    }

    return size + 0x10;
}
