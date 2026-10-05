#include "stdafx.h"
#include "object/turret.h"
#include "ai/move.h"
#include "appear/apprtype.h"
#include "camera/camera.h"
#include "color/color.h"
#include "engine/bitflag.h"
#include "engine/ceglist.h"
#include "engine/cellip.h"
#include "engine/cevfx.h"
#include "lib/aerror.h"
#include "lib/cvmath.h"
#include "lib/inifile.h"
#include "logistics/logmain.h"
#include "main/main.h"
#include "mission/scenario.h"
#include "network/multplyr.h"
#include "object/artlry.h"
#include "object/bldng.h"
#include "object/bullet.h"
#include "object/cmponent.h"
#include "object/fire.h"
#include "object/gvehicl.h"
#include "object/laser.h"
#include "object/mech.h"
#include "object/object.h"
#include "object/objevnt.h"
#include "object/objque.h"
#include "object/prjlase.h"
#include "object/smoke.h"
#include "object/team.h"
#include "object/warrior.h"
#include "sound/soundsys.h"
#include "sprite/gvactor.h"
#include "sprite/puactor.h"
#include "terrain/terrain.h"
#include "vfx/vfxfuncs.h"

namespace
{
    /// <summary>Degrees to radians, as MCX.EXE stores it (a hair under pi / 180).</summary>
    constexpr double DEGREES_TO_RADIANS = 0x1.1df46a2526c7ap-6;
    /// <summary>Radians to degrees, as MCX.EXE stores it.</summary>
    constexpr double RADIANS_TO_DEGREES = 0x1.ca5dc1a6402aap+5;
    /// <summary>Sixty degrees in radians, as MCX.EXE stores it.</summary>
    constexpr double SIXTY_DEGREES = 0x1.0c152382d45b2p+0;

    /// <summary>The entry angle of each packed chunk value (turret.cpp's own copy of the table).</summary>
    /// <remarks>MCX.EXE @ 0x00792ec4</remarks>
    const float ChunkEntryAngles[4] = {0.0f, 180.0f, -90.0f, 90.0f};

    /// <summary>A mech, vehicle, elemental or plain mover: something with a pilot.</summary>
    bool isMoverClass(const GameObject* object)
    {
        const int32_t objectClass = object->objectClass;
        return objectClass == BATTLEMECH || objectClass == GROUNDVEHICLE || objectClass == ELEMENTAL ||
               objectClass == MOVER;
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

    /// <summary>Makes the weapon's effect object (Fatal when it can't).</summary>
    GameObject* createWeaponFX(const MasterComponent& weapon)
    {
        GameObject* fx = createObject(static_cast<int32_t>(weaponFXTable[weapon.weaponEffect]));

        if (fx == nullptr)
        {
            Fatal(-1, " couldnt create weapon FX ");
        }

        return fx;
    }

    /// <summary>
    /// Sends a weapon effect from the turret (hot spot 0) at <paramref name="target"/> or, when it is null, at
    /// <paramref name="point"/>, carrying <paramref name="shot"/>; then adds it to the weapon list.
    /// </summary>
    void launchWeaponFX(Turret* turret, GameObject* fx, GameObject* target, vector_3d* point, _WeaponShotInfo& shot,
                        int32_t targetHotSpot)
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
                bullet->connect(turret, *point, 0);
            }
            else
            {
                bullet->owner = turret;
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
                laser->connect(turret, *point, &shot, 0);
            }
            else
            {
                laser->source.setWatcher(turret);
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
                projectile->connect(turret, *point, &shot, 0);
            }
            else
            {
                projectile->owner = turret;
                projectile->target = target;
                projectile->ownerHotSpot = 0;
                projectile->targetHotSpot = targetHotSpot;
                projectile->shotInfo.init(shot.attacker, shot.masterId, shot.damage, shot.hitLocation, shot.entryAngle);
            }
        }

        weaponList->addNode(fx);
    }

    /// <summary>
    /// Builds, packs and checks the chunk for a hit on <paramref name="target"/> (a mover, train car, camera drone or
    /// terrain object) or a shot at <paramref name="point"/> when it is null; then queues and logs it.
    /// </summary>
    void sendFireChunk(Turret* turret, GameObject* target, GameObject* logTarget, vector_3d* point, int hit,
                       float entryAngle, int32_t missiles, int32_t missilesPastAMS, int32_t antiMissileShots,
                       int32_t hitLocation)
    {
        WeaponFireChunk chunk;
        chunk.init();
        auto* bigTarget = static_cast<BigGameObject*>(target);

        if (target == nullptr)
        {
            chunk.buildLocationTarget(*point, 0, hit, missiles);
        }
        else if (isMoverClass(target))
        {
            chunk.buildMoverTarget(bigTarget, 0, hit, entryAngle, missiles, missilesPastAMS, antiMissileShots,
                                   hitLocation);
        }
        else if (target->objectClass == TRAINCAR)
        {
            chunk.buildTrainTarget(bigTarget, 0, hit, entryAngle, missiles);
        }
        else if (target->objectClass == CAMERADRONE)
        {
            chunk.buildCameraDroneTarget(bigTarget, 0, hit, entryAngle, missiles);
        }
        else
        {
            chunk.buildTerrainTarget(bigTarget, 0, hit, missiles);
        }

        if (target != nullptr)
        {
            objectList->findObjectFromPart(chunk.targetId);
        }

        chunk.pack();
        WeaponFireChunk check;
        check.init();
        check.data = chunk.data;
        check.unpack(turret);

        if (chunk.equalTo(&check) == 0)
        {
            Fatal(0, " Turret.fireWeapon: Bad WeaponFireChunk (save wfchunk.dbg file now) ");
        }

        turret->addWeaponFireChunk(0, &chunk);
        LogWeaponFireChunk(&chunk, turret, logTarget);
    }

    /// <summary>
    /// A missed shot's offset from its aim point, up to <paramref name="scatter"/>. Centred scatter (missiles in
    /// fireWeapon) spreads both ways; otherwise, as the original computes it, only one way.
    /// </summary>
    vector_3d scatterPoint(float scatter, int centred)
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
        return miss;
    }

    /// <summary>
    /// Firing gives a turret away: when one of the other side's mechs is within visual range, the ground around the
    /// turret is seen by that side (with <see cref="Terrain::markRadiusSeen"/> from fireWeapon, markSeen from
    /// handleWeaponFire).
    /// </summary>
    void revealFiring(Turret* turret, int radius)
    {
        ObjectQueueNode* enemies = nullptr;
        uint8_t seenBy = 0;

        if (turret->alignment == 1)
        {
            enemies = clanMechList;
            seenBy = 2;
        }
        else if (turret->alignment == -1)
        {
            enemies = innerSphereMechList;
            seenBy = 1;
        }
        else
        {
            return;
        }

        for (BaseObject* enemy = enemies->head; enemy != nullptr; enemy = enemy->next)
        {
            vector_3d enemyPosition = static_cast<GameObject*>(enemy)->getPosition();

            if (turret->distanceFrom(enemyPosition) < scenario->maxVisualRange)
            {
                vector_3d lookVector(0.0f, 1.0f, 0.0f);

                if (radius != 0)
                {
                    land->markRadiusSeen(turret->position, lookVector, 360.0f, scenario->fireVisualRange, seenBy);
                }
                else
                {
                    land->markSeen(turret->position, lookVector, 360.0f, scenario->fireVisualRange, seenBy);
                }

                return;
            }
        }
    }

    /// <summary>
    /// How many of the four corners of the turret's vertex square <paramref name="bits"/> marks (the turret's
    /// (row, col), (row + 1, col), (row + 1, col + 1), (row, col + 1)).
    /// </summary>
    int32_t countVisibleCorners(ByteFlag* bits, uint32_t row, uint32_t col, int stopAtFirst)
    {
        int32_t count = 0;
        const uint32_t corners[4][2] = {{row, col}, {row + 1, col}, {row + 1, col + 1}, {row, col + 1}};

        for (const auto& corner : corners)
        {
            if (bits->getFlag(corner[0], corner[1]) != 0)
            {
                count++;

                if (stopAtFirst != 0)
                {
                    break;
                }
            }
        }

        return count;
    }

    /// <summary>The map row and column of a turret's terrain vertex.</summary>
    void vertexRowCol(const Turret* turret, uint32_t& row, uint32_t& col)
    {
        col = static_cast<uint32_t>((turret->blockNumber % Terrain::blocksMapSide) * Terrain::verticesBlockSide +
                                    turret->vertexNumber % Terrain::verticesBlockSide);
        row = static_cast<uint32_t>((turret->blockNumber / Terrain::blocksMapSide) * Terrain::verticesBlockSide +
                                    turret->vertexNumber / Terrain::verticesBlockSide);
    }
}

//---------------------------------------------------------------------------
// TurretType
//---------------------------------------------------------------------------

auto TurretType::init() -> void
{
    typeClass = -1;
    destroyedObject = -1;
    explosionObject = -1;
    unknown18 = 0;
    appearName = 0;
    extentRadius = 0.0f;
    keepMe = 0;
    iconNumber = -1;
    dmgLevelClosed = 0;
    dmgLevel = 0;
    blownEffectId = 0xffffffff;
    normalEffectId = 0xffffffff;
    damageEffectId = 0xffffffff;
    explosionRadius = 0.0f;
    explosionDamage = 0.0f;
    tonnage = 0.0f;
    weaponType = -1;
    pilotSkill = 0;
    maxTurretYawRate = 0.0f;
    buildingName = 0;
    fireOffsetY = 0;
    fireOffsetX = 0;
    centerOffsetY = 0;
    centerOffsetX = 0;
}

auto TurretType::createInstance() -> BaseObject*
{
    auto* newTurret = new Turret;

    if (newTurret == nullptr)
    {
        return nullptr;
    }

    if (newTurret->init(this) != 0)
    {
        return nullptr;
    }

    newTurret->idNumber = NextIdNumber++;
    return newTurret;
}

auto TurretType::destroy() -> void
{
}

auto TurretType::init(File* objFile, uint32_t fileSize) -> int32_t
{
    FitIniFile turretFile;
    int32_t result = turretFile.open(objFile, fileSize, 50);

    if (result != 0)
    {
        return result;
    }

    if ((result = turretFile.seekBlock("TurretData")) != 0)
    {
        return result;
    }

    if ((result = turretFile.readIdULong("DmgLevel", dmgLevel)) != 0)
    {
        return result;
    }

    result = turretFile.readIdULong("DmgLevelClosed", dmgLevelClosed);

    if (result != 0)
    {
        dmgLevelClosed = dmgLevel;
    }

    // Original behaviour (OB-011): the three effect ids test DmgLevelClosed's result, not their own; a type without
    // DmgLevelClosed loses all three.
    turretFile.readIdULong("BlownEffectId", blownEffectId);

    if (result != 0)
    {
        blownEffectId = 0xffffffff;
    }

    turretFile.readIdULong("NormalEffectId", normalEffectId);

    if (result != 0)
    {
        normalEffectId = 0xffffffff;
    }

    turretFile.readIdULong("DamageEffectId", damageEffectId);

    if (result != 0)
    {
        damageEffectId = 0xffffffff;
    }

    if (turretFile.readIdLong("BasePixelOffsetX", basePixelOffsetX) != 0)
    {
        basePixelOffsetX = 0;
    }

    if (turretFile.readIdLong("BasePixelOffsetY", basePixelOffsetY) != 0)
    {
        basePixelOffsetY = 0;
    }

    if (turretFile.readIdFloat("ExplosionRadius", explosionRadius) != 0)
    {
        explosionRadius = 0.0f;
    }

    if (turretFile.readIdFloat("ExplosionDamage", explosionDamage) != 0)
    {
        explosionDamage = 0.0f;
    }

    if (turretFile.readIdFloat("Tonnage", tonnage) != 0)
    {
        tonnage = 20.0f;
    }

    if ((result = turretFile.readIdFloat("AttackRadius", attackRadius)) != 0)
    {
        return result;
    }

    if ((result = turretFile.readIdFloat("MaxTurretYawRate", maxTurretYawRate)) != 0)
    {
        return result;
    }

    if ((result = turretFile.readIdLong("WeaponType", weaponType)) != 0)
    {
        return result;
    }

    if ((result = turretFile.readIdLong("PilotSkill", pilotSkill)) != 0)
    {
        return result;
    }

    if (turretFile.readIdFloat("LittleExtent", littleExtent) != 0)
    {
        littleExtent = 20.0f;
    }

    if (turretFile.readIdLong("BuildingName", buildingName) != 0)
    {
        buildingName = 0xa4;
    }

    if (turretFile.readIdLong("FireOffsetX", fireOffsetX) != 0)
    {
        fireOffsetX = 0;
    }

    if (turretFile.readIdLong("FireOffsetY", fireOffsetY) != 0)
    {
        fireOffsetY = 0;
    }

    if (turretFile.readIdLong("CenterOffsetX", centerOffsetX) != 0)
    {
        centerOffsetX = 0;
    }

    if (turretFile.readIdLong("CenterOffsetY", centerOffsetY) != 0)
    {
        centerOffsetY = 0;
    }

    return ObjectType::init(&turretFile);
}

auto TurretType::handleCollision(GameObject* collidee, GameObject* collider) -> int
{
    // Only the server picks targets.
    if (MPlayer != nullptr && MPlayer->isServer == 0)
    {
        return 1;
    }

    auto* turret = static_cast<Turret*>(collidee);
    // The squared distance to the current target, or far.
    float targetDistance = 1e+07f;

    if (turret->target != nullptr)
    {
        const vector_3d targetPosition = turret->target->getPosition();
        const vector_3d turretPosition = turret->getPosition();
        targetDistance = (turretPosition.x - targetPosition.x) * (turretPosition.x - targetPosition.x) +
                         (turretPosition.y - targetPosition.y) * (turretPosition.y - targetPosition.y);
    }

    const int32_t alignmentGap = std::abs(collider->getAlignment() - collidee->getAlignment());

    if (!((turret->unknown118 != 0 && alignmentGap > 0) || alignmentGap > 1))
    {
        return 1;
    }

    const int32_t colliderClass = collider->objectClass;

    if (colliderClass < 2)
    {
        return 1;
    }

    if (colliderClass < 5)
    {
        // Mechs, vehicles and elementals: only whole ones.
        if (collider->isDisabled() != 0 || collider->isDestroyed() != 0)
        {
            return 1;
        }
    }
    else if (colliderClass != CAMERADRONE || collider->isDestroyed() != 0)
    {
        return 1;
    }

    const vector_3d colliderPosition = collider->getPosition();
    const vector_3d turretPosition = collidee->getPosition();

    if ((turretPosition.x - colliderPosition.x) * (turretPosition.x - colliderPosition.x) +
            (turretPosition.y - colliderPosition.y) * (turretPosition.y - colliderPosition.y) <
        targetDistance)
    {
        turret->target = collider;
    }

    return 1;
}

auto TurretType::handleDestruction(GameObject*, GameObject*) -> int
{
    return 0;
}

//---------------------------------------------------------------------------
// Turret
//---------------------------------------------------------------------------

Turret::Turret()
{
    justCreated = 1;
    appearance = nullptr;
    vertexNumber = 0;
    blockNumber = 0;
    onFire = 0;
    fireObject = nullptr;
    unknown9C = 0;
    unknownA0 = 500000;
    destroyed = 0;
    unknownD0 = 0;
    name.clear();
    markedSeenInnerSphere = 0;
    markedSeenClan = 0;
    unknown100 = 0;
    turretRotation = 0.0f;
    awake = 1;
    weaponDeployed = 1;
    smoke = nullptr;
    smokeTime = 0.0f;
    target = nullptr;
    netRosterIndex = -1;
    numWeaponFireChunks[0] = 0;
    numWeaponFireChunks[1] = 0;
    // Port fix: nothing in MCX.EXE sets this, so the original's turrets fired or not by what the object heap held at
    // +0xf0 (OB-014). The port's turrets are armed.
    weaponEnabled = 1;
    init();
}

auto Turret::init() -> void
{
}

auto Turret::isVisible(Camera* cam) -> int
{
    if (cam == nullptr || cam->active == 0)
    {
        return 0;
    }

    int visible = cam->vertexProject(blockNumber, vertexNumber, screenPos);

    if (appearance != nullptr)
    {
        visible = appearance->recalcBounds(cam);
    }

    if (visible == 0)
    {
        return 0;
    }

    windowsVisible = turn;
    return 1;
}

auto Turret::update() -> int32_t
{
    auto* type = static_cast<TurretType*>(objType);

    if (justCreated != 0)
    {
        // Set the turret on its vertex: the block's corner, the vertex within it, then the offset within the tile
        // (turned into the isometric grid's 60-degree axes).
        justCreated = 0;
        const int32_t verticesBlockSide = Terrain::verticesBlockSide;
        float blockX = static_cast<float>(blockNumber % Terrain::blocksMapSide - Terrain::blocksMapSide / 2) *
                       Terrain::metersBlockSide;
        float blockY = static_cast<float>(Terrain::blocksMapSide / 2 - blockNumber / Terrain::blocksMapSide) *
                       Terrain::metersBlockSide;

        if ((Terrain::blocksMapSide & 1) != 0)
        {
            blockX = blockX - Terrain::metersBlockSide * 0.5f;
            blockY = Terrain::metersBlockSide * 0.5f + blockY;
        }

        const float vertexX = static_cast<float>(vertexNumber % verticesBlockSide) * Terrain::metersPerVertex;
        blockY = blockY - static_cast<float>(vertexNumber / verticesBlockSide) * Terrain::metersPerVertex;
        const double offsetY = static_cast<double>(tileOffsetY);
        const double offsetX = static_cast<double>(tileOffsetX);
        double offsetAngle;

        if (offsetY == 0.0)
        {
            offsetAngle = 90.0;
        }
        else
        {
            offsetAngle = std::atan(offsetX / offsetY) * RADIANS_TO_DEGREES;
        }

        position.y = blockY;
        const auto offsetDistance = static_cast<float>(std::sqrt(offsetY * offsetY + offsetX * offsetX));
        const double axisAngle = (60.0 - offsetAngle) * DEGREES_TO_RADIANS;
        const auto alongAxis = static_cast<float>(std::sin(axisAngle) * offsetDistance / std::sin(SIXTY_DEGREES));
        position.x = vertexX + blockX;
        const float elevation = land->getTerrainElevation(position);
        position.x =
            static_cast<float>(std::cos(SIXTY_DEGREES) * alongAxis + std::cos(axisAngle) * offsetDistance + position.x);
        position.y = position.y - alongAxis;
        position.z = elevation;

        tileCol = (blockNumber % Terrain::blocksMapSide) * verticesBlockSide + vertexNumber % verticesBlockSide;
        const int32_t halfMap = (verticesBlockSide * Terrain::blocksMapSide) >> 1;
        tilePositionX = static_cast<float>(tileCol - halfMap) * Terrain::metersPerVertex;
        tileRow = vertexNumber / verticesBlockSide + (blockNumber / Terrain::blocksMapSide) * verticesBlockSide;
        tilePositionY = static_cast<float>(halfMap - tileRow) * Terrain::metersPerVertex;
        const auto inBounds = [&]
        { return tileRow < 0 || GameMap->height <= tileRow || tileCol < 0 || GameMap->width <= tileCol ? 0u : 1u; };
        Assert(inBounds(), 0, " tbldg MapTile Out of Bounds ");
        Assert(inBounds(), 0, " Map Tile out of bounds ");
        const MapTile& tile = GameMap->map[GameMap->width * tileRow + tileCol];
        const int32_t elevationLevel = static_cast<int32_t>((tile.cells >> 7) & 0x3f) + GameMap->baseElevation;
        appearance->visible = 1;
        tileElevation = static_cast<float>(elevationLevel) * Terrain::metersPerElevLevel;
        appearance->update();
        appearance->recalcBounds(eye);
    }

    if (destroyed != 0)
    {
        collisionsOn = 0;
        return 1;
    }

    // The server drops a target that's out of reach or out of the fight.
    if ((MPlayer == nullptr || MPlayer->isServer != 0) && target != nullptr)
    {
        const vector_3d targetPosition = target->getPosition();
        const float dx = targetPosition.x - position.x;
        const float dy = targetPosition.y - position.y;
        const float reach = getExtentRadius();

        if (target->isDisabled() != 0 || target->isDestroyed() != 0 ||
            static_cast<double>(reach) * reach < static_cast<double>(dy) * dy + static_cast<double>(dx) * dx)
        {
            target = nullptr;
        }
    }

    // An awake turret shows its side the ground around it, once.
    awake = getAwake();
    bool unmarked = false;

    if (alignment == 1)
    {
        unmarked = markedSeenInnerSphere == 0;
    }
    else if (alignment == -1)
    {
        unmarked = markedSeenClan == 0;
    }

    if (1 < turn && awake != 0 && unmarked)
    {
        vector_3d lookVector(0.0f, 0.0f, 0.0f);

        if (alignment == 1)
        {
            land->markSeen(position, lookVector, 360.0f, scenario->maxVisualRange, 1);
            markedSeenInnerSphere = 1;
        }
        else if (alignment == -1)
        {
            land->markSeen(position, lookVector, 360.0f, scenario->maxVisualRange, 2);
            markedSeenClan = 1;
        }
    }

    if (awake != 0 && target != nullptr)
    {
        // Turn toward the target and open up.
        float relAngle = relFacingTo(target->getPosition(), -1) + turretRotation;
        float turnStep = 0.0f;

        if (relAngle < -2.0 || 2.0 < relAngle)
        {
            turnStep = type->maxTurretYawRate * frameLength;

            if (!(relAngle < 0.0))
            {
                turnStep = -turnStep;
            }

            relAngle = -relAngle;

            // Original behaviour (OB-013): meant to cap the turn at the yaw rate, but whichever way the two differ
            // the full angle is taken, so the turret snaps onto its target.
            if (relAngle < turnStep || turnStep < relAngle)
            {
                turnStep = relAngle;
            }
        }

        turretRotation = turnStep + turretRotation;
        int32_t combatState = 0;

        if (fixedTurret == 0)
        {
            combatState = static_cast<PUAppearance*>(appearance)->setCombatMode(1);
        }

        if (weaponDeployed == 0)
        {
            weaponDeployed = fixedTurret == 0 ? (combatState == 2 ? 1 : 0) : 1;
        }
    }
    else if (awake == 0)
    {
        // Asleep: close up.
        if (fixedTurret == 0)
        {
            weaponDeployed = static_cast<PUAppearance*>(appearance)->setCombatMode(0) == 2 ? 1 : 0;
        }
    }
    else
    {
        // Nothing to shoot at: close up.
        int32_t combatState = 0;

        if (fixedTurret == 0)
        {
            combatState = static_cast<PUAppearance*>(appearance)->setCombatMode(0);
        }

        if (weaponDeployed != 0)
        {
            weaponDeployed = fixedTurret == 0 ? (combatState == 2 ? 1 : 0) : 0;
        }
    }

    if (destroyed == 0 && target != nullptr && isWeaponReady() != 0 && awake != 0 &&
        (MPlayer == nullptr || MPlayer->isServer != 0))
    {
        fireWeapon(target);
    }

    if (0 < numWeaponFireChunks[1])
    {
        updateWeaponFireChunks(1);
    }

    return 1;
}

auto Turret::isWeaponReady() -> int
{
    return readyTime <= scenarioTime && weaponDeployed != 0 && weaponEnabled != 0 ? 1 : 0;
}

auto Turret::isWeaponMissile() -> int
{
    return MasterComponentList[static_cast<TurretType*>(objType)->weaponType].form == 9 ? 1 : 0;
}

auto Turret::isWeaponStreak() -> int
{
    return MasterComponentList[static_cast<TurretType*>(objType)->weaponType].weaponFlags & 1;
}

auto Turret::calcAttackChance(GameObject* target, int32_t* range) -> float
{
    auto* type = static_cast<TurretType*>(objType);
    // Port fix: the original measures to an uninitialised point when there's no target (no caller passes none).
    vector_3d targetPosition(0.0f, 0.0f, 0.0f);

    if (target != nullptr)
    {
        targetPosition = target->getPosition();
    }

    const int32_t pilotSkill = type->pilotSkill;
    const auto distance = static_cast<float>(distanceFrom(targetPosition));

    if (range != nullptr)
    {
        if (WeaponRange[0] < distance)
        {
            *range = WeaponRange[1] < distance ? 2 : 1;
        }
        else
        {
            *range = 0;
        }
    }

    // The weapon's own range bands; beyond the last it can't hit.
    const MasterComponent& weapon = MasterComponentList[type->weaponType];
    float modifier;

    if (distance <= weapon.weaponRange[0])
    {
        modifier = WeaponFireModifiers[14];
    }
    else if (distance <= weapon.weaponRange[1])
    {
        modifier = WeaponFireModifiers[11];
    }
    else if (distance <= weapon.weaponRange[2])
    {
        modifier = WeaponFireModifiers[12];
    }
    else if (distance <= weapon.weaponRange[3])
    {
        modifier = WeaponFireModifiers[13];
    }
    else
    {
        return -1.0f;
    }

    const float chance = modifier + static_cast<float>(pilotSkill);

    if (target == nullptr)
    {
        return chance;
    }

    // A still target is easier.
    const vector_3d velocity = target->getVelocity();

    if (velocity.x * velocity.x + velocity.y * velocity.y != 0.0)
    {
        return chance;
    }

    return static_cast<float>(chance + 50.0);
}

auto Turret::lineOfFire(GameObject* target) -> int
{
    int32_t tileR;
    int32_t tileC;
    int32_t cellR;
    int32_t cellC;
    GameMap->worldToMapPos(target->getPosition(), tileR, tileC, cellR, cellC);
    ByteFlag* visibleBits;

    if (alignment == 1)
    {
        visibleBits = Terrain::terrainVisibleBits;
    }
    else if (alignment == -1)
    {
        visibleBits = Terrain::ClanVisibleBits;
    }
    else
    {
        // Port fix: for a neutral turret the original uses the tile column it just computed as the visibility bits'
        // pointer, and crashes.
        return 0;
    }

    const int32_t numVisible =
        countVisibleCorners(visibleBits, static_cast<uint32_t>(tileR), static_cast<uint32_t>(tileC), 0);
    const int lineClear = GameMap->lineOfFire(position, target->getPosition());

    if (numVisible == 0)
    {
        return 0;
    }

    vector_3d targetPosition = target->getPosition();
    return distanceFrom(targetPosition) < MaxVisualRadius && lineClear != 0 ? 1 : 0;
}

auto Turret::recordWeaponFireTime() -> void
{
    lastFireTime = scenarioTime;
}

auto Turret::startWeaponRecycle() -> void
{
    readyTime = MasterComponentList[static_cast<TurretType*>(objType)->weaponType].recycleTime + scenarioTime;
}

auto Turret::clearWeaponFireChunks(int32_t which) -> int32_t
{
    const int32_t numChunks = numWeaponFireChunks[which];
    numWeaponFireChunks[which] = 0;
    return numChunks;
}

auto Turret::addWeaponFireChunk(int32_t which, WeaponFireChunk* chunk) -> int32_t
{
    // Port fix: the original checks against 0x80 (Mover's limit) and overruns the 8-entry list.
    if (numWeaponFireChunks[which] == MAX_WEAPONFIRE_CHUNKS)
    {
        Fatal(0, " Turret::addWeaponFireChunk--Too many weaponfire chunks ");
    }

    chunk->pack();
    weaponFireChunks[which][numWeaponFireChunks[which]] = chunk->data;
    numWeaponFireChunks[which]++;
    return numWeaponFireChunks[which];
}

auto Turret::addWeaponFireChunks(int32_t which, uint32_t* packedChunkBuffer, int32_t numChunks) -> int32_t
{
    // Port fix: the original checks against 0x80 (Mover's limit) and overruns the 8-entry list.
    if (numWeaponFireChunks[which] + numChunks > MAX_WEAPONFIRE_CHUNKS)
    {
        Fatal(0, " Turret::addWeaponFireChunks--Too many weaponfire chunks ");
    }

    for (int32_t i = 0; i < numChunks; i++)
    {
        weaponFireChunks[which][numWeaponFireChunks[which]] = packedChunkBuffer[i];
        numWeaponFireChunks[which]++;
        // Unpacked into a scratch chunk (the result isn't kept).
        WeaponFireChunk chunk;
        chunk.init();
        chunk.data = packedChunkBuffer[i];
        chunk.unpack(this);
    }

    return numWeaponFireChunks[which];
}

auto Turret::grabWeaponFireChunks(int32_t which, uint32_t* packedChunkBuffer) -> int32_t
{
    const int32_t numChunks = numWeaponFireChunks[which];

    for (int32_t i = 0; i < numChunks; i++)
    {
        packedChunkBuffer[i] = weaponFireChunks[which][i];
    }

    return numChunks;
}

auto Turret::updateWeaponFireChunks(int32_t which) -> int32_t
{
    // Replays the weapon fire the server sent.
    for (int32_t i = 0; i < numWeaponFireChunks[which]; i++)
    {
        WeaponFireChunk chunk;
        chunk.init();
        chunk.data = weaponFireChunks[which][i];
        chunk.unpack(this);

        switch (chunk.targetType)
        {
            case 0:
            case 1:
            case 2:
            {
                BaseObject* chunkTarget = nullptr;
                const char* missing = nullptr;

                if (chunk.targetType == 0)
                {
                    chunkTarget = MPlayer->moverRoster[chunk.targetId];
                    missing = " Turret.updateWeaponFireChunks: NULL Mover Target (save wfchunk.dbg file) ";
                }
                else
                {
                    chunkTarget = objectList->findObjectFromPart(chunk.targetId);
                    missing = chunk.targetType == 1
                                  ? " Turret.updateWeaponFireChunks: NULL Terrain Target (save wfchunk.dbg file) "
                                  : " Turret.updateWeaponFireChunks: NULL Special Target (save wfchunk.dbg file) ";
                }

                if (chunkTarget == nullptr)
                {
                    DebugWeaponFireChunk(&chunk, nullptr, this);
                    Assert(0, 0, missing);
                }

                handleWeaponFire(0, static_cast<GameObject*>(chunkTarget), nullptr, chunk.hit,
                                 ChunkEntryAngles[chunk.entryAngle], chunk.numMissiles, chunk.numMissilesPastAMS,
                                 chunk.numAntiMissileShots, chunk.hitLocation);
                break;
            }

            case 3:
            {
                // A point on the ground: the middle of the target cell, at height 0.
                const float halfSide = worldUnitsMapSide * 0.5f;
                vector_3d point;
                point.x = (static_cast<float>(chunk.targetCell[1]) + 0.5f) * MetersPerCell - halfSide;
                point.y = (halfSide - static_cast<float>(chunk.targetCell[0]) * MetersPerCell) - MetersPerCell * 0.5f;
                point.z = 0.0f;
                handleWeaponFire(0, nullptr, &point, chunk.hit, 0.0f, 0, 0, 0, 0);
                break;
            }

            default:
                Fatal(0, " Mover.updateWeaponFireChunk: bad targetType ");
        }
    }

    numWeaponFireChunks[which] = 0;
    return 0;
}

auto Turret::getPositionFromHS(uint32_t nodeId) -> vector_3d
{
    auto* type = static_cast<TurretType*>(objType);
    float x = position.x;
    double y = position.y;
    float z = position.z;

    if (type->fireOffsetX != 0 || type->fireOffsetY != 0 || type->centerOffsetX != 0 || type->centerOffsetY != 0)
    {
        float muzzleX = 0.0f;
        double muzzleY = 0.0;
        float muzzleZ = 0.0f;

        if (nodeId != 0xffffffff)
        {
            const double angle = (static_cast<double>(turretRotation) + 45.0) * DEGREES_TO_RADIANS;
            muzzleX = static_cast<float>(std::sin(angle) * type->fireOffsetX);
            muzzleY = std::cos(angle) * type->fireOffsetX;
            muzzleZ = static_cast<float>(type->fireOffsetY);
        }

        x = static_cast<float>(type->centerOffsetX) + x + muzzleX;
        y = y - (static_cast<double>(type->centerOffsetY) + muzzleY);
        z = z + muzzleZ;
    }

    return vector_3d(x, static_cast<float>(y), z);
}

auto Turret::fireWeapon(GameObject* target) -> void
{
    if (target == nullptr)
    {
        return;
    }

    // A camera drone can't be shot for two seconds after launch.
    if (target->objectClass == CAMERADRONE && scenarioTime < static_cast<CameraDrone*>(target)->launchTime + 2.0)
    {
        return;
    }

    auto* type = static_cast<TurretType*>(objType);
    const MasterComponent& weapon = MasterComponentList[type->weaponType];
    // Indirect fire needs no clear line.
    const bool indirect = weapon.missileType == 2 || weapon.missileType == 1 || weapon.missileType == 3;

    if (!indirect && lineOfFire(target) == 0)
    {
        return;
    }

    const float entryAngle = target->relFacingTo(position, -1);
    const int isStreak = weapon.weaponFlags & 1;
    int32_t range;
    const auto hitChance = static_cast<int32_t>(calcAttackChance(target, &range));

    if (static_cast<double>(hitChance) == -1.0)
    {
        return;
    }

    const int32_t hitRoll = RandomNumber(100);
    MechWarrior* targetPilot = nullptr;

    if (isMoverClass(target))
    {
        targetPilot = target->getPilot();
        targetPilot->updateAttackerStatus(static_cast<uint32_t>(partId), scenarioTime);
    }

    startWeaponRecycle();

    if (hitRoll < hitChance)
    {
        recordWeaponFireTime();

        if (weapon.form == 9)
        {
            // Missiles: a streak fires them all, anything else about half; anti-missile systems take some out.
            const int32_t rackSize = weapon.numMissiles;
            int32_t missiles = rackSize;

            if (isStreak == 0)
            {
                missiles = static_cast<int32_t>(rackSize * 0.5 + 0.5);

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
            const int32_t missilesLeft = target->fireAntiMissileSystem(missiles, antiMissileShots);

            if (0 < antiMissileShots)
            {
                target->reduceAntiMissileAmmo(antiMissileShots);
            }

            if (0 < missilesLeft)
            {
                GameObject* fx = createWeaponFX(weapon);
                const int32_t hitLocation = target->calcHitLocation(this, type->weaponType, 0, 1);
                Assert(hitLocation != -2 ? 1 : 0, 0, " Turret.FireWeapon: Bad Hit Location ");
                const int32_t targetHotSpot = targetHotSpotOf(target, hitLocation);
                _WeaponShotInfo shot;
                shot.init(this, type->weaponType, weapon.damage * static_cast<float>(missilesLeft), hitLocation,
                          entryAngle);

                if (MPlayer != nullptr && MPlayer->isServer != 0)
                {
                    sendFireChunk(this, target, target, nullptr, 1, entryAngle, missiles, missilesLeft,
                                  antiMissileShots, hitLocation);
                }

                launchWeaponFX(this, fx, target, nullptr, shot, targetHotSpot);
            }
        }
        else
        {
            const int32_t hitLocation = target->calcHitLocation(this, type->weaponType, 0, 1);
            Assert(hitLocation != -2 ? 1 : 0, 0, " Turret.FireWeapon: Bad Hit Location 2 ");
            _WeaponShotInfo shot;
            shot.init(this, type->weaponType, weapon.damage, hitLocation, entryAngle);

            if (MPlayer != nullptr && MPlayer->isServer != 0)
            {
                sendFireChunk(this, target, target, nullptr, 1, entryAngle, 0, 0, 0, hitLocation);
            }

            GameObject* fx = createWeaponFX(weapon);
            launchWeaponFX(this, fx, target, nullptr, shot, targetHotSpotOf(target, hitLocation));
        }
    }
    else if (isStreak == 0)
    {
        // A miss (a streak doesn't fire without a lock): the shot lands up to 25 meters off.
        recordWeaponFireTime();

        if (weapon.form == 9)
        {
            const int32_t rackSize = weapon.numMissiles;
            int32_t missiles = static_cast<int32_t>(rackSize * 0.5 + 0.5);

            if (missiles < 1)
            {
                missiles = 1;
            }

            if (rackSize < missiles)
            {
                missiles = rackSize;
            }

            if (0 < missiles)
            {
                GameObject* fx = createWeaponFX(weapon);
                _WeaponShotInfo shot;
                shot.init(this, type->weaponType, weapon.damage * static_cast<float>(missiles), -1, entryAngle);
                vector_3d landing = scatterPoint(25.0f, 1);
                const vector_3d targetPosition = target->getPosition();
                landing.x += targetPosition.x;
                landing.y += targetPosition.y;
                landing.z += targetPosition.z;

                if (MPlayer != nullptr && MPlayer->isServer != 0)
                {
                    sendFireChunk(this, nullptr, target, &landing, 0, 0.0f, missiles, 0, 0, 0);
                }

                launchWeaponFX(this, fx, nullptr, &landing, shot, 0);
            }
        }
        else
        {
            _WeaponShotInfo shot;
            shot.init(this, type->weaponType, weapon.damage, -1, entryAngle);
            GameObject* fx = createWeaponFX(weapon);
            vector_3d landing(25.0f, 25.0f, 0.0f);

            // Original behaviour (OB-012): the chunk goes out before the point is scattered and moved to the target,
            // so other players see the shot land at (25, 25, 0).
            if (MPlayer != nullptr && MPlayer->isServer != 0)
            {
                sendFireChunk(this, nullptr, target, &landing, 0, 0.0f, 0, 0, 0, 0);
            }

            landing = scatterPoint(25.0f, 0);
            const vector_3d targetPosition = target->getPosition();
            landing.x += targetPosition.x;
            landing.y += targetPosition.y;
            landing.z += targetPosition.z;
            launchWeaponFX(this, fx, nullptr, &landing, shot, 0);
        }
    }

    if (targetPilot != nullptr)
    {
        targetPilot->triggerAlarm(0, static_cast<uint32_t>(partId));
    }

    revealFiring(this, 1);
}

auto Turret::handleWeaponFire(int32_t, GameObject* target, vector_3d* targetPoint, int hit, float entryAngle,
                              int32_t numMissiles, int32_t numHits, int32_t numAntiMissiles, int32_t hitLocation)
    -> int32_t
{
    auto* type = static_cast<TurretType*>(objType);
    const int32_t masterId = type->weaponType;
    const MasterComponent& weapon = MasterComponentList[masterId];
    const int isStreak = weapon.weaponFlags & 1;
    startWeaponRecycle();

    if (hit != 0)
    {
        if (weapon.form == 9)
        {
            if (0 < numAntiMissiles)
            {
                target->reduceAntiMissileAmmo(numAntiMissiles);
            }

            if (0 < numHits)
            {
                GameObject* fx = createWeaponFX(weapon);
                Assert(hitLocation != -2 ? 1 : 0, 0, " Turret.handleWeaponFire: Bad Hit Location ");
                const int32_t targetHotSpot = targetHotSpotOf(target, hitLocation);
                _WeaponShotInfo shot;
                shot.init(this, masterId, weapon.damage * static_cast<float>(numHits), hitLocation, entryAngle);
                launchWeaponFX(this, fx, target, targetPoint, shot, targetHotSpot);
            }
        }
        else
        {
            _WeaponShotInfo shot;
            shot.init(this, masterId, weapon.damage, hitLocation, entryAngle);
            GameObject* fx = createWeaponFX(weapon);
            launchWeaponFX(this, fx, target, targetPoint, shot, targetHotSpotOf(target, hitLocation));
        }
    }
    else if (isStreak == 0 && (weapon.form != 9 || 0 < numHits))
    {
        // A miss lands up to 25 meters off a target, 5 off a point.
        GameObject* fx = nullptr;
        _WeaponShotInfo shot;

        if (weapon.form == 9)
        {
            fx = createWeaponFX(weapon);
            shot.init(this, masterId, weapon.damage * static_cast<float>(numMissiles), -1, entryAngle);
        }
        else
        {
            shot.init(this, masterId, weapon.damage, -1, entryAngle);
            fx = createWeaponFX(weapon);
        }

        vector_3d landing = scatterPoint(target != nullptr ? 25.0f : 5.0f, 0);
        const vector_3d base = target != nullptr ? target->getPosition() : *targetPoint;
        landing.x += base.x;
        landing.y += base.y;
        landing.z += base.z;
        launchWeaponFX(this, fx, nullptr, &landing, shot, 0);
    }

    if (target != nullptr && isMoverClass(target))
    {
        MechWarrior* targetPilot = target->getPilot();
        targetPilot->updateAttackerStatus(static_cast<uint32_t>(partId), scenarioTime);
        targetPilot->triggerAlarm(0, static_cast<uint32_t>(partId));
    }

    revealFiring(this, 0);
    return 0;
}

auto Turret::setAlignment(int32_t align) -> void
{
    BigGameObject::setAlignment(align);
    target = nullptr;
}

auto Turret::handleEvent(ObjectEvent* event) -> int32_t
{
    if (event->type == 0)
    {
        switch (event->id)
        {
            case 0x1c:
                selected = 1;
                break;
            case 0x1d:
                selected = 0;
                break;
            case 0x1e:
                unknown2C = 1;
                break;
            case 0x1f:
                unknown2C = 0;
                break;
        }
    }

    return 0;
}

auto Turret::lightOnFire(float timeToBurn) -> void
{
    auto* type = static_cast<TurretType*>(objType);

    if (type->blownEffectId == 0xffffffff)
    {
        // Nothing to burn: a point of damage instead.
        _WeaponShotInfo shot;
        shot.init(nullptr, -1, 1.0f, 0, 0.0f);

        if (MPlayer == nullptr)
        {
            handleWeaponHit(&shot, 0);
        }
        else if (MPlayer->isServer != 0)
        {
            handleWeaponHit(&shot, 1);
        }

        return;
    }

    if (fireObject == nullptr)
    {
        GameObject* newFire = createObject(static_cast<int32_t>(type->blownEffectId));

        if (newFire != nullptr)
        {
            newFire->setPosition(position);

            if (newFire->objectClass == FIRE)
            {
                fireObject = static_cast<Fire*>(newFire);
                fireObject->setPotentialContact(3);
                fireObject->burningObject = this;
                fireObject->setTonnage(40.0f);
            }
            else
            {
                destroyObject(newFire);
            }
        }
    }

    if (fireObject != nullptr)
    {
        fireObject->addTimeLeftToBurn(timeToBurn);
        onFire = 1;
    }
}

auto Turret::isRevealed() -> int
{
    ByteFlag* visibleBits = homeTeam->alignment == -1 ? Terrain::ClanVisibleBits : Terrain::terrainVisibleBits;
    uint32_t row;
    uint32_t col;
    vertexRowCol(this, row, col);
    return countVisibleCorners(visibleBits, row, col, 1) != 0 ? 1 : 0;
}

auto Turret::enemyRevealed() -> int
{
    ByteFlag* visibleBits = homeTeam->alignment == -1 ? Terrain::terrainVisibleBits : Terrain::ClanVisibleBits;
    uint32_t row;
    uint32_t col;
    vertexRowCol(this, row, col);
    return countVisibleCorners(visibleBits, row, col, 1) != 0 ? 1 : 0;
}

auto Turret::render() -> void
{
    if (justCreated != 0)
    {
        return;
    }

    if (appearance != nullptr)
    {
        appearance->visible = isVisible(eye);
        appearance->update();
    }

    if (getContactType(homeTeam->id) == 2)
    {
        // A sensor contact: a blip sized by tonnage.
        uint8_t* shape;
        const char* shapeName;

        if (50.0f < getTonnage())
        {
            shape = scenario->sensorContactShapes[0];
            shapeName = "tblip1";
        }
        else if (35.0f < getTonnage())
        {
            shape = scenario->sensorContactShapes[2];
            shapeName = "tblip2";
        }
        else
        {
            shape = scenario->sensorContactShapes[4];
            shapeName = "tblip3";
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
            auto* element = ElementPool::Make<VFXElement>(shape, screenPos.x, screenPos.y, blipFrame, 0, nullptr, 0, 0);
            std::strcpy(element->name, shapeName);
            ElementList->add(element);
            blipFrame++;
        }
    }

    if (windowsVisible != turn)
    {
        return;
    }

    // Hazed by how many corners of its vertex square the home team sees; drawn when all are (a fixed turret, or the
    // pop-up of object type 0x2f2, when any is).
    uint32_t row;
    uint32_t col;
    vertexRowCol(this, row, col);
    ByteFlag* visibleBits = homeTeam->alignment == -1 ? Terrain::ClanVisibleBits : Terrain::terrainVisibleBits;
    const int32_t numVisible = countVisibleCorners(visibleBits, row, col, 0);
    uint8_t* hazePalette = nullptr;
    const int32_t hazeLevel = eye->hazeLevel;

    if (numVisible != 0 && numVisible != 4 && hazeLevel != 0x7fff)
    {
        int32_t level;

        if (hazeLevel < 0 && 0 < eye->hazeInc * numVisible + hazeLevel)
        {
            level = 0;
        }
        else
        {
            level = hazeLevel + eye->hazeInc * numVisible;
        }

        hazePalette = gamePalette->getHazePalette(level);
    }

    if (fixedTurret == 0)
    {
        static_cast<PUAppearance*>(appearance)->hazePalette = hazePalette;
    }
    else
    {
        static_cast<GVAppearance*>(appearance)->hazePalette = hazePalette;
    }

    const bool anyCornerPopUp = numVisible != 0 && fixedTurret == 0 && objType->objTypeNum == 0x2f2;

    if ((numVisible != 0 && fixedTurret != 0) || (numVisible == 4 && fixedTurret == 0) || anyCornerPopUp)
    {
        appearance->render(0);

        if (fireObject != nullptr)
        {
            fireObject->render();
        }
    }

    if (drawExtents != 0)
    {
        // Debug: the extent radius as an ellipse.
        float radius = objType->extentRadius;

        if (eye->cameraScale == 1)
        {
            radius *= 0.5f;
        }

        const float scale = eye->cameraScale != 1 ? 1.0f : 0.5f;
        const float sx = (position.x - eye->position.x) * scale;
        const float sy = (position.y - eye->position.y) * scale;
        vector_2d center;
        center.x = sx * eye->cosAngle + sy * eye->cosAngle + eye->halfWidth;
        center.y =
            ((sx * eye->sinAngle + eye->halfHeight) - sy * eye->sinAngle) - scale * (position.z - eye->position.z);
        vector_2d size(radius, radius);
        ElementList->openGroup(-50000, 1);
        // Port: an overlay, on the screen over the view: it follows the object through the zoom.
        center = MCOverlayPoint(center);
        size.x *= MCOverlay.ScaleX;
        size.y *= MCOverlay.ScaleY;
        ElementList->add(ElementPool::Make<EllipseElement>(center, size, 0xfe, -50000));
    }
}

auto Turret::destroy() -> void
{
    delete appearance;
    appearance = nullptr;

    if (fireObject != nullptr)
    {
        fireObject->setPotentialContact(0);
        fireObject->burningObject = nullptr;
        delete fireObject;
        fireObject = nullptr;
    }

    name.clear();
}

auto Turret::init(ObjectType* objType) -> int32_t
{
    int32_t result = GameObject::init(objType);

    if (result != 0)
    {
        return result;
    }

    const uint32_t appearId = objType->appearName;
    justCreated = 1;
    AppearanceType* apprType = appearanceTypeList->getAppearance(appearId, 0);

    if (apprType == nullptr)
    {
        return static_cast<int32_t>(0xdcdc0007);
    }

    switch (appearId & 0xff000000)
    {
        case 0x5000000:
        {
            auto* fixedAppearance = new GVAppearance;
            appearance = fixedAppearance;

            if (fixedAppearance == nullptr)
            {
                return -0x2ffff;
            }

            fixedAppearance->init(nullptr, nullptr);

            if ((apprType->appearanceNum & 0xff000000) != 0x5000000)
            {
                return -0x2fff6;
            }

            if ((result = fixedAppearance->init(apprType, this)) != 0)
            {
                return result;
            }

            weaponDeployed = 1;
            fixedTurret = 1;
            break;
        }

        case 0x9000000:
        {
            auto* popUpAppearance = new PUAppearance;
            appearance = popUpAppearance;

            if (popUpAppearance == nullptr)
            {
                return -0x2ffff;
            }

            popUpAppearance->init(nullptr, nullptr);

            if ((apprType->appearanceNum & 0xff000000) != 0x9000000)
            {
                return -0x2fff6;
            }

            if ((result = popUpAppearance->init(apprType, this)) != 0)
            {
                return result;
            }

            weaponDeployed = 0;
            fixedTurret = 0;
            break;
        }

        default:
            return -0x2fff6;
    }

    auto* type = static_cast<TurretType*>(this->objType);
    objectClass = TURRET;
    destroyed = 0;
    unknownEC = 1;
    alignment = -1;
    readyTime = 0.0f;

    // The attack radius, when given, is the collision extent: whatever enters it is a candidate target.
    if (type->attackRadius != 0.0)
    {
        type->extentRadius = type->attackRadius;
    }

    if (0.0 < type->extentRadius)
    {
        collisionsOn = 1;
    }

    setPotentialContact(2);
    tonnage = type->tonnage;
    explDamage = type->explosionDamage;
    explRadius = type->explosionRadius;
    char nameBuffer[256];
    cLoadString(thisInstance, static_cast<uint32_t>(type->buildingName), nameBuffer, 0xfe);
    name = nameBuffer;
    unknownDC = 0;
    smoke = nullptr;
    unknown118 = 0;
    target = nullptr;
    return 0;
}

auto Turret::handleWeaponHit(_WeaponShotInfo* shotInfo, int addMultiplayChunk) -> int32_t
{
    if (shotInfo == nullptr)
    {
        return 0;
    }

    if (addMultiplayChunk != 0)
    {
        MPlayer->addWeaponHitChunk(this, shotInfo, 0);
    }

    const float newDamage = getDamage() + shotInfo->damage;
    setDamage(newDamage);
    auto* type = static_cast<TurretType*>(objType);

    if (newDamage < static_cast<float>(static_cast<int32_t>(type->dmgLevel)))
    {
        return 0;
    }

    // Destroyed: the wreck, its smoke, fire and explosion.
    destroyed = 1;

    if (fixedTurret == 0)
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

    if (smoke != nullptr)
    {
        smoke->setOwner(this);
        smoke->setOwnerPosition(position);
        smoke->update();
        smokeTime = smokeTime - frameLength;

        if (smokeTime <= -30.0)
        {
            delete smoke;
            smoke = nullptr;
        }
    }

    collisionsOn = 0;
    status = 2;

    if (onFire == 0)
    {
        if (type->blownEffectId == 0xffffffff)
        {
            if (fireObject != nullptr)
            {
                fireObject->addTimeLeftToBurn(2.0f);
            }
        }
        else
        {
            GameObject* newFire = createObject(static_cast<int32_t>(type->blownEffectId));

            if (newFire != nullptr)
            {
                newFire->setPosition(position);

                if (newFire->objectClass == FIRE)
                {
                    fireObject = static_cast<Fire*>(newFire);
                    fireObject->setPotentialContact(3);
                    fireObject->burningObject = this;
                    fireObject->setTonnage(40.0f);
                    onFire = 1;
                }
                else if (objectList->head != nullptr)
                {
                    objectList->head->addNode(newFire);
                }
            }
        }

        type->createExplosion(position, explDamage, explRadius);
    }

    return 0;
}
