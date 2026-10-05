#include "stdafx.h"
#include "object/prjlase.h"
#include "appear/apprtype.h"
#include "camera/camera.h"
#include "camera/camlist.h"
#include "engine/ceglist.h"
#include "engine/cepoly.h"
#include "engine/crater.h"
#include "gui/asystem.h"
#include "lib/cvmath.h"
#include "lib/file.h"
#include "lib/inifile.h"
#include "main/main.h"
#include "mission/scenario.h"
#include "network/multplyr.h"
#include "object/explode.h"
#include "object/mech.h"
#include "object/object.h"
#include "object/objque.h"
#include "object/smoke.h"
#include "sound/soundsys.h"
#include "sprite/armactor.h"
#include "terrain/terrain.h"
#include "terrain/terrmap.h"
#include "vfx/vfx.h"

namespace
{
    /// <summary>Radians to degrees, as MCX.EXE stores it.</summary>
    constexpr double RADIANS_TO_DEGREES = 0x1.ca5dc1a6402aap+5;

    /// <summary>
    /// The camera's isometric projection of <paramref name="point"/> (inlined five times in render): the offset from
    /// the eye, halved at the 50% scale, turned by the view angle and dropped by its height.
    /// </summary>
    void projectPoint(const vector_3d& point, int32_t& screenX, int32_t& screenY)
    {
        const float scale = eye->cameraScale == 1 ? 0.5f : 1.0f;
        const float dx = (point.x - eye->position.x) * scale;
        const float dy = (point.y - eye->position.y) * scale;
        const float dz = (point.z - eye->position.z) * scale;
        const float x = dy * eye->cosAngle + dx * eye->cosAngle + eye->halfWidth;
        const float y = ((dx * eye->sinAngle + eye->halfHeight) - dy * eye->sinAngle) - dz;
        screenX = static_cast<int32_t>(x);
        screenY = static_cast<int32_t>(y);
    }

    /// <summary>A screen vertex in palette colour <paramref name="color"/> (u, v and w zero).</summary>
    SCRNVERTEX colorVertex(const vector_3d& point, uint8_t color)
    {
        SCRNVERTEX vertex;
        projectPoint(point, vertex.x, vertex.y);
        vertex.c = static_cast<FIXED16>(color) << 16;
        vertex.u = 0;
        vertex.v = 0;
        vertex.w = 0;
        return vertex;
    }
} // namespace

//---------------------------------------------------------------------------
// ProjectileLaserType
//---------------------------------------------------------------------------

ProjectileLaserType::ProjectileLaserType()
{
    soundEffectId = 0xffffffff;
    projectileHitEffect = 0xffffffff;
    projectileMissEffect = 0xffffffff;
    bulgeWidth = 0.0f;
    bulgeLength = 0.0f;
    projectileLength = 0.0f;

    for (int32_t i = 0; i < 4; i++)
    {
        eColor[i] = 0;
        fColor[i] = 0;
    }
}

auto ProjectileLaserType::createInstance() -> BaseObject*
{
    auto* newLaser = new ProjectileLaser;

    if (newLaser == nullptr)
    {
        return nullptr;
    }

    if (newLaser->init(this) != 0)
    {
        return nullptr;
    }

    newLaser->idNumber = NextIdNumber++;
    return newLaser;
}

auto ProjectileLaserType::destroy() -> void
{
}

auto ProjectileLaserType::init(File* objFile, uint32_t fileSize) -> int32_t
{
    FitIniFile laserFile;
    int32_t result = laserFile.open(objFile, fileSize, 50);

    if (result != 0)
    {
        return result;
    }

    if (laserFile.seekBlock("ProjectileLaserData") == 0)
    {
        if ((result = laserFile.readIdULong("SoundEffectId", soundEffectId)) != 0)
        {
            return result;
        }

        if ((result = laserFile.readIdULong("ProjectileHitEffect", projectileHitEffect)) != 0)
        {
            return result;
        }

        if ((result = laserFile.readIdULong("ProjectileMissEffect", projectileMissEffect)) != 0)
        {
            return result;
        }

        if ((result = laserFile.readIdFloat("Velocity", velocity)) != 0)
        {
            return result;
        }

        if ((result = laserFile.readIdFloat("CloseDistance", closeDistance)) != 0)
        {
            return result;
        }

        if ((result = laserFile.readIdFloat("ProjectileLength", projectileLength)) != 0)
        {
            return result;
        }

        if ((result = laserFile.readIdFloat("BulgeLength", bulgeLength)) != 0)
        {
            return result;
        }

        if ((result = laserFile.readIdFloat("BulgeWidth", bulgeWidth)) != 0)
        {
            return result;
        }

        if ((result = laserFile.readIdUChar("e0Color", eColor[0])) != 0)
        {
            return result;
        }

        if ((result = laserFile.readIdUChar("e1Color", eColor[1])) != 0)
        {
            return result;
        }

        if ((result = laserFile.readIdUChar("e2Color", eColor[2])) != 0)
        {
            return result;
        }

        if ((result = laserFile.readIdUChar("e3Color", eColor[3])) != 0)
        {
            return result;
        }

        if ((result = laserFile.readIdUChar("f0Color", fColor[0])) != 0)
        {
            return result;
        }

        if ((result = laserFile.readIdUChar("f1Color", fColor[1])) != 0)
        {
            return result;
        }

        if ((result = laserFile.readIdUChar("f2Color", fColor[2])) != 0)
        {
            return result;
        }

        if ((result = laserFile.readIdUChar("f3Color", fColor[3])) != 0)
        {
            return result;
        }

        if (laserFile.readIdULong("SmokeObjectId", smokeObjectId) != 0)
        {
            smokeObjectId = 0xffffffff;
        }

        if (laserFile.readIdULong("LightObjectId", lightObjectId) != 0)
        {
            lightObjectId = 0xffffffff;
        }
    }

    result = ObjectType::init(&laserFile);
    objectTypeManager->load(static_cast<int32_t>(projectileHitEffect), 1);
    objectTypeManager->load(static_cast<int32_t>(projectileMissEffect), 1);
    return result;
}

auto ProjectileLaserType::handleCollision(GameObject*, GameObject*) -> int
{
    return 0;
}

auto ProjectileLaserType::handleDestruction(GameObject*, GameObject*) -> int
{
    return 0;
}

//---------------------------------------------------------------------------
// ProjectileLaser
//---------------------------------------------------------------------------

ProjectileLaser::ProjectileLaser()
{
    frame.reset_to_world_frame();
    smokeDisplacement.x = 0.0f;
    smokeDisplacement.y = 0.0f;
    smokeDisplacement.z = 0.0f;
    frame.i = UnitX;
    frame.j = UnitY;
    frame.k = UnitZ;
    ownerHotSpot = 0;
    targetHotSpot = 0;
    targetPosition = nullptr;
    justCreated = 1;
    appearance = nullptr;
    smoke = nullptr;
    closestDistanceSq = 0.0f;
    target = nullptr;
    owner = nullptr;
    light = nullptr;
    drawRotation = 0;
}

auto ProjectileLaser::init() -> void
{
}

auto ProjectileLaser::isVisible() -> int
{
    Camera* camera = cameraList->findCameraFromIDNumber(1);

    if (camera == nullptr || camera->active == 0)
    {
        return 0;
    }

    vector_2d screen100;
    vector_2d screen50;

    if (land != nullptr)
    {
        land->projectTerrain(position, screen100, screen50);
    }

    float screenY;

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

    if (appearance != nullptr && appearance->recalcBounds(camera) != 0)
    {
        windowsVisible = turn;
        return 1;
    }

    return 0;
}

auto ProjectileLaser::update() -> int32_t
{
    const auto* laserType = static_cast<ProjectileLaserType*>(objType);

    if (justCreated != 0)
    {
        // Start at the owner's hot spot, facing its way, with the head one projectile length toward the target.
        GameObject* shooter = owner;
        justCreated = 0;
        collisionsOn = 0;

        if (shooter != nullptr)
        {
            position = shooter->getPositionFromHS(static_cast<uint32_t>(ownerHotSpot));
            frame = shooter->getFrame();
        }

        if (laserType->soundEffectId != 0xffffffff)
        {
            soundSystem->playDigitalSample(laserType->soundEffectId, 1, this, 0, 0);
        }

        headPosition = position;
        closestDistanceSq = 1.0e8f;

        if (target != nullptr)
        {
            // Port fix (OB-017): the original aimed at the target's hot spot numbered like the owner's (ownerHotSpot).
            const uint32_t hotSpot = target->objectClass == TURRET ? 0xffffffff : static_cast<uint32_t>(targetHotSpot);
            setTargetPosition(target->getPositionFromHS(hotSpot));
        }

        // Port fix: the original reads an unset destination when there is no target position.
        vector_3d to = headPosition;

        if (targetPosition != nullptr)
        {
            to = *targetPosition;
        }

        float dx = to.x - headPosition.x;
        float dy = to.y - headPosition.y;
        float dz = to.z - headPosition.z;
        const float length = std::sqrt(dx * dx + dy * dy + dz * dz);

        if (length != 0.0f)
        {
            dx /= length;
            dy /= length;
            dz /= length;
        }

        headPosition.x = dx * laserType->projectileLength + headPosition.x;
        headPosition.y = dy * laserType->projectileLength + headPosition.y;
        headPosition.z = dz * laserType->projectileLength + headPosition.z;
    }

    GameObject* shooter = owner;

    if (shooter != nullptr)
    {
        position = shooter->getPositionFromHS(static_cast<uint32_t>(ownerHotSpot));
    }

    int arrived = 0;
    const int visibleNow = isVisible();

    if (appearance != nullptr)
    {
        appearance->visible = visibleNow;
        appearance->update();
    }

    // A mech facing the other way draws the bolt mirrored.
    drawRotation = -150;

    // Port fix: the original reads the owner's class without checking it for null.
    if (shooter != nullptr && shooter->objectClass == BATTLEMECH)
    {
        frame_of_ref ownerFrame = shooter->getFrame();
        const float cosFacing = UnitX.y * ownerFrame.i.y + UnitX.x * ownerFrame.i.x + UnitX.z * ownerFrame.i.z;
        float facing = static_cast<float>(ownerFrame.my_acos(cosFacing) * RADIANS_TO_DEGREES);

        if (ownerFrame.i.y < 0.0f)
        {
            facing = -facing;
        }

        drawRotation = std::abs(facing) <= 90.0f ? -150 : 150;
    }

    // Move the head toward the target; once its ground distance stops shrinking, it has arrived.
    const vector_3d from = headPosition;

    if (target != nullptr)
    {
        // Port fix (OB-017): aim at the hot spot the hit effect plays at (the original used ownerHotSpot).
        const uint32_t hotSpot = target->objectClass == TURRET ? 0xffffffff : static_cast<uint32_t>(targetHotSpot);
        setTargetPosition(target->getPositionFromHS(hotSpot));
    }

    vector_3d to = from;

    if (targetPosition != nullptr)
    {
        to = *targetPosition;
    }

    const double step = static_cast<double>(laserType->velocity) * frameLength;
    const double dxWide = static_cast<double>(to.x) - from.x;
    const auto dx = static_cast<float>(dxWide);
    float dy = to.y - from.y;
    float dz = to.z - from.z;
    const double dxSq = static_cast<double>(dx) * dx;
    const double dySq = static_cast<double>(dy) * dy;
    const double groundDistanceSq = dxSq + dySq;

    if (closestDistanceSq <= groundDistanceSq)
    {
        arrived = 1;
    }
    else
    {
        closestDistanceSq = static_cast<float>(groundDistanceSq);
    }

    const double travelLength = std::sqrt(static_cast<double>(dz) * dz + dxSq + dySq);
    const auto travelLengthF = static_cast<float>(travelLength);
    double unitX = dxWide;

    if (travelLength != 0.0)
    {
        unitX = static_cast<double>(dx) / travelLengthF;
        dy = static_cast<float>(static_cast<double>(dy) / travelLengthF);
        dz = static_cast<float>(static_cast<double>(dz) / travelLengthF);
    }

    const double moveX = unitX * step;
    const auto moveY = static_cast<float>(dy * step);
    const auto moveZ = static_cast<float>(dz * step);
    headPosition.x = static_cast<float>(moveX + headPosition.x);
    headPosition.y = moveY + headPosition.y;
    headPosition.z = moveZ + headPosition.z;

    // The shape: a bulge bulgeLength behind the head, bulgeWidth to each side, and the tail projectileLength back.
    const auto backX = static_cast<float>(-moveX);
    float backY = -moveY;
    float backZ = -moveZ;
    const double backLength = std::sqrt(static_cast<double>(backZ) * backZ + static_cast<double>(backY) * backY +
                                        static_cast<double>(backX) * backX);
    const auto backLengthF = static_cast<float>(backLength);
    double backXWide = -moveX;

    if (backLength != 0.0)
    {
        backXWide = static_cast<double>(backX) / backLengthF;
        backY = static_cast<float>(static_cast<double>(backY) / backLengthF);
        backZ = static_cast<float>(static_cast<double>(backZ) / backLengthF);
    }

    const float bulgeLength = laserType->bulgeLength;
    const float bulgeWidth = laserType->bulgeWidth;
    bulgeSide1.y = backY * bulgeLength + headPosition.y;
    bulgeSide1.x = static_cast<float>(backXWide * bulgeLength + headPosition.x);
    bulgeSide1.z = backZ * bulgeLength + headPosition.z;
    bulgeSide2 = bulgeSide1;
    bulgeCenter = bulgeSide1;
    const double backYWidth = static_cast<double>(backY) * bulgeWidth;
    const auto backXWidth = static_cast<float>(backXWide * bulgeWidth);
    bulgeSide2.x = static_cast<float>(backYWidth + bulgeSide2.x);
    bulgeSide2.y -= backXWidth;
    bulgeSide1.x = static_cast<float>(bulgeSide1.x - backYWidth);
    bulgeSide1.y += backXWidth;
    tailPosition.y = backY * laserType->projectileLength + headPosition.y;
    tailPosition.x = static_cast<float>(backXWide * laserType->projectileLength + headPosition.x);
    tailPosition.z = backZ * laserType->projectileLength + headPosition.z;

    // With a smoke trail, arrival is instead the tail coming within closeDistance (plus a step) of the target.
    if (smoke != nullptr)
    {
        const vector_3d tail = tailPosition;
        arrived = 0;

        if (target != nullptr)
        {
            // Port fix (OB-017): see above.
            const uint32_t hotSpot = target->objectClass == TURRET ? 0xffffffff : static_cast<uint32_t>(targetHotSpot);
            setTargetPosition(target->getPositionFromHS(hotSpot));
        }

        vector_3d smokeTo = tail;

        if (targetPosition != nullptr)
        {
            smokeTo = *targetPosition;
        }

        const float smokeStep = laserType->velocity * frameLength;
        float sx = smokeTo.x - tail.x;
        float sy = smokeTo.y - tail.y;
        float sz = smokeTo.z - tail.z;
        const double reach = static_cast<double>(smokeStep) + laserType->closeDistance;
        const float sySq = sy * sy;
        const float sxSq = sx * sx;
        const double groundSq = static_cast<double>(sx) * sx + sySq;

        if (groundSq <= reach * reach)
        {
            smoke->stopSmoking();
            arrived = 1;
        }

        const double smokeLength = std::sqrt(static_cast<double>(sz) * sz + sxSq + sySq);

        if (smokeLength != 0.0)
        {
            sx = static_cast<float>(sx / smokeLength);
            sy = static_cast<float>(sy / smokeLength);
            sz = static_cast<float>(sz / smokeLength);
        }

        vector_3d smokeVelocity;
        smokeVelocity.x = sx * smokeStep;
        smokeVelocity.y = sy * smokeStep;
        smokeVelocity.z = sz * smokeStep;
        smokeDisplacement.x += smokeVelocity.x;
        smokeDisplacement.y += smokeVelocity.y;
        smokeDisplacement.z += smokeVelocity.z;
        smoke->setOwnerPosition(tailPosition);
        smoke->setOwnerVelocity(smokeVelocity);
        smoke->update();
    }

    if (light != nullptr)
    {
        vector_3d lightPos = tailPosition;
        light->setPosition(lightPos);
        light->update();
    }

    if (arrived == 0)
    {
        return 1;
    }

    // Arrived: apply the shot (in multiplayer only the server does, and sends it on).
    if (target != nullptr)
    {
        if (MPlayer == nullptr)
        {
            target->handleWeaponHit(&shotInfo, 0);
        }
        else if (MPlayer->isServer != 0)
        {
            target->handleWeaponHit(&shotInfo, 1);
        }
    }

    GameObject* effect = createObject(
        static_cast<int32_t>(target == nullptr ? laserType->projectileMissEffect : laserType->projectileHitEffect));

    if (effect == nullptr)
    {
        return 0;
    }

    if (target != nullptr)
    {
        const uint32_t hotSpot = target->objectClass == TURRET ? 0xffffffff : static_cast<uint32_t>(targetHotSpot);
        vector_3d hitPos = target->getPositionFromHS(hotSpot);
        effect->setPosition(hitPos);
    }
    else if (targetPosition != nullptr)
    {
        effect->setPosition(*targetPosition);
    }

    if (objectList->head != nullptr)
    {
        objectList->head->addNode(effect);
    }

    // A miss leaves a crater and sets off a live mine where it lands.
    if (target == nullptr && targetPosition != nullptr)
    {
        craterManager->addCrater(7, *targetPosition, 1);

        int32_t tileR = 0;
        int32_t tileC = 0;
        int32_t cellR = 0;
        int32_t cellC = 0;
        GameMap->worldToMapPos(*targetPosition, tileR, tileC, cellR, cellC);

        // Port fix: a miss can land off the map, where the original reads (and writes) outside it.
        if (!GameMap->onMap(tileR, tileC))
        {
            return 0;
        }

        MapTile& tile = GameMap->map[GameMap->width * tileR + tileC];

        if ((tile.overlay & 0x1800) == 0x1000 || (tile.overlay & 0x6000) == 0x4000)
        {
            CreateExplosion(MineExplosion, *targetPosition, MineSplashDamage, worldUnitsPerMeter * MineSplashRange);
            tile.overlay |= 0x1800;
            tile.overlay |= 0x6000;
        }
    }

    return 0;
}

auto ProjectileLaser::render() -> void
{
    if (gamePaused != 0)
    {
        onScreen();
    }

    if (justCreated != 0)
    {
        return;
    }

    if (appearance != nullptr)
    {
        appearance->render(drawRotation);
    }

    // Two quads, head-bulge-tail-centre on each side, in the friendly or enemy colours.
    const auto* laserType = static_cast<ProjectileLaserType*>(objType);
    const bool enemy = owner == nullptr || owner->getAlignment() == -1;
    const uint8_t headColor = enemy ? laserType->eColor[0] : laserType->fColor[0];
    // Original behaviour (OB-020): the other corners test "no owner" and then ask that null owner for its
    // alignment, so with an owner they always take the enemy colours (without one, MCX.EXE crashes).
    const uint8_t* cornerColors = laserType->eColor;

    PolyElementData side1;
    side1.numVertices = 4;
    side1.vertices[0] = colorVertex(headPosition, headColor);
    side1.vertices[1] = colorVertex(bulgeSide1, cornerColors[1]);
    side1.vertices[2] = colorVertex(tailPosition, cornerColors[2]);
    side1.vertices[3] = colorVertex(bulgeCenter, cornerColors[3]);
    PolyElementData side2;
    side2.numVertices = 4;
    side2.vertices[0] = side1.vertices[0];
    side2.vertices[1] = colorVertex(bulgeSide2, cornerColors[1]);
    side2.vertices[2] = side1.vertices[2];
    side2.vertices[3] = side1.vertices[3];

    const int32_t depth = -side1.vertices[0].y;
    ElementList->openGroup(depth, 1);
    ElementList->add(ElementPool::Make<PolygonElement>(&side1, depth));
    ElementList->add(ElementPool::Make<PolygonElement>(&side2, depth));

    if (smoke != nullptr)
    {
        smoke->render();
    }

    if (light != nullptr)
    {
        light->render();
    }
}

auto ProjectileLaser::destroy() -> void
{
    delete targetPosition;
    targetPosition = nullptr;
    delete appearance;
    appearance = nullptr;
    delete smoke;
    smoke = nullptr;
    delete light;
    light = nullptr;
}

auto ProjectileLaser::init(ObjectType* objType) -> int32_t
{
    int32_t result = GameObject::init(objType);

    if (result != 0)
    {
        return result;
    }

    const uint32_t appearId = objType->appearName;
    justCreated = 1;

    if (appearId != 0)
    {
        AppearanceType* apprType = appearanceTypeList->getAppearance(appearId, 0);

        if (apprType == nullptr)
        {
            return -0x2d;
        }

        if ((apprType->appearanceNum & 0xff000000) != 0x6000000)
        {
            return -0x38;
        }

        auto* armAppearance = new ArmAppearance;
        appearance = armAppearance;

        if (armAppearance == nullptr)
        {
            return -0x2e;
        }

        armAppearance->init(nullptr, nullptr);
        armAppearance->ownerObject = nullptr;

        if ((result = armAppearance->init(apprType, this)) != 0)
        {
            return result;
        }

        armAppearance->ownerObject = this;
    }

    const auto* laserType = static_cast<ProjectileLaserType*>(objType);

    if (static_cast<int32_t>(laserType->smokeObjectId) != -1)
    {
        smoke = static_cast<Smoke*>(createObject(static_cast<int32_t>(laserType->smokeObjectId)));
    }

    if (static_cast<int32_t>(laserType->lightObjectId) != -1)
    {
        light = createObject(static_cast<int32_t>(laserType->lightObjectId));
    }

    objectClass = PROJECTILELASER;
    return 0;
}

auto ProjectileLaser::setOwner(BaseObject* newOwner) -> void
{
    owner = static_cast<GameObject*>(newOwner);
}

auto ProjectileLaser::setTargetPosition(vector_3d position) -> void
{
    if (targetPosition == nullptr)
    {
        targetPosition = new vector_3d;
    }

    *targetPosition = position;
}

auto ProjectileLaser::connect(GameObject* source, vector_3d targetPos, _WeaponShotInfo* shotInfo, int32_t sourceHotSpot)
    -> void
{
    owner = source;
    ownerHotSpot = sourceHotSpot;
    setTargetPosition(targetPos);

    if (shotInfo != nullptr)
    {
        const _WeaponShotInfo shot = *shotInfo;
        this->shotInfo.init(shot.attacker, shot.masterId, shot.damage, shot.hitLocation, shot.entryAngle);
    }
}
