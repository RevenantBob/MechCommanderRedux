#include "stdafx.h"
#include "object/bullet.h"
#include "appear/apprtype.h"
#include "camera/camera.h"
#include "camera/camlist.h"
#include "engine/crater.h"
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

namespace
{
    /// <summary>Radians to degrees, as MCX.EXE stores it.</summary>
    constexpr double RADIANS_TO_DEGREES = 0x1.ca5dc1a6402aap+5;

    /// <summary>
    /// Projects the object to the screen through the terrain (the 100% or 50% projection, by the camera's scale)
    /// into <c>screenPos</c>.
    /// </summary>
    void projectToScreen(BigGameObject* object, Camera* camera)
    {
        vector_2d screen100;
        vector_2d screen50;

        if (land != nullptr)
        {
            land->projectTerrain(object->position, screen100, screen50);
        }

        float screenY;

        if (camera->cameraScale == 1)
        {
            object->screenPos.x = (screen50.x - camera->screenUL50.x) + camera->halfWidth;
            screenY = screen50.y - camera->screenUL50.y;
        }
        else
        {
            object->screenPos.x = (screen100.x - camera->screenUL.x) + camera->halfWidth;
            screenY = screen100.y - camera->screenUL.y;
        }

        object->screenPos.y = screenY + camera->halfHeight;
    }
} // namespace

//---------------------------------------------------------------------------
// BulletType
//---------------------------------------------------------------------------

BulletType::BulletType()
{
    soundEffectId = 0xffffffff;
    bulletHitEffect = 0xffffffff;
    bulletMissEffect = 0xffffffff;
    smokeObjectId = 0xffffffff;
}

auto BulletType::createInstance() -> BaseObject*
{
    auto* newBullet = new Bullet;

    if (newBullet == nullptr)
    {
        return nullptr;
    }

    if (newBullet->init(this) != 0)
    {
        return nullptr;
    }

    newBullet->idNumber = NextIdNumber++;
    return newBullet;
}

auto BulletType::destroy() -> void
{
}

auto BulletType::init(File* objFile, uint32_t fileSize) -> int32_t
{
    FitIniFile bulletFile;
    int32_t result = bulletFile.open(objFile, fileSize, 50);

    if (result != 0)
    {
        return result;
    }

    if (bulletFile.seekBlock("BulletData") == 0)
    {
        if ((result = bulletFile.readIdULong("SoundEffectId", soundEffectId)) != 0)
        {
            return result;
        }

        if ((result = bulletFile.readIdULong("BulletHitEffect", bulletHitEffect)) != 0)
        {
            return result;
        }

        if ((result = bulletFile.readIdULong("BulletMissEffect", bulletMissEffect)) != 0)
        {
            return result;
        }

        if ((result = bulletFile.readIdFloat("Velocity", velocity)) != 0)
        {
            return result;
        }

        if ((result = bulletFile.readIdFloat("CloseDistance", closeDistance)) != 0)
        {
            return result;
        }

        if ((result = bulletFile.readIdULong("SmokeObjectId", smokeObjectId)) != 0)
        {
            return result;
        }

        if (bulletFile.readIdULong("LightObjectId", lightObjectId) != 0)
        {
            lightObjectId = 0xffffffff;
        }
    }

    result = ObjectType::init(&bulletFile);
    objectTypeManager->load(static_cast<int32_t>(bulletHitEffect), 1);
    objectTypeManager->load(static_cast<int32_t>(bulletMissEffect), 1);
    objectTypeManager->load(static_cast<int32_t>(smokeObjectId), 1);
    return result;
}

auto BulletType::handleCollision(GameObject*, GameObject*) -> int
{
    return 0;
}

auto BulletType::handleDestruction(GameObject*, GameObject*) -> int
{
    return 0;
}

//---------------------------------------------------------------------------
// Bullet
//---------------------------------------------------------------------------

Bullet::Bullet()
{
    ownerHotSpot = 0;
    targetHotSpot = 0;
    targetPosition = nullptr;
    justCreated = 1;
    appearance = nullptr;
    numShots = 0;
    smoke = nullptr;
    closestDistanceSq = 0.0f;
    target = nullptr;
    owner = nullptr;
    light = nullptr;
    drawRotation = 0;
}

auto Bullet::init() -> void
{
}

auto Bullet::isVisible() -> int
{
    Camera* camera = cameraList->findCameraFromIDNumber(1);

    if (camera == nullptr || camera->active == 0)
    {
        return 0;
    }

    projectToScreen(this, camera);

    if (appearance != nullptr && appearance->recalcBounds(camera) == 0)
    {
        return 0;
    }

    windowsVisible = turn;
    return 1;
}

auto Bullet::update() -> int32_t
{
    if (justCreated != 0)
    {
        justCreated = 0;
        collisionsOn = 0;

        if (owner != nullptr)
        {
            position = owner->getPositionFromHS(static_cast<uint32_t>(ownerHotSpot));
        }

        const uint32_t soundId = static_cast<BulletType*>(objType)->soundEffectId;

        if (soundId != 0xffffffff)
        {
            soundSystem->playDigitalSample(soundId, 1, this, 0, 0);
        }

        bulletPosition = position;
        closestDistanceSq = 1.0e8f;
    }

    GameObject* shooter = owner;

    if (shooter != nullptr)
    {
        position = shooter->getPositionFromHS(static_cast<uint32_t>(ownerHotSpot));
    }

    const int visibleNow = isVisible();

    if (appearance != nullptr)
    {
        appearance->visible = visibleNow;
        appearance->update();
    }

    // Fly toward the target (following it if it moves); once the ground distance stops shrinking, we're there.
    vector_3d from = bulletPosition;

    if (target != nullptr)
    {
        // Original behaviour (OB-017): the flight follows the target's hot spot numbered like the owner's
        // (ownerHotSpot), not targetHotSpot.
        const uint32_t hotSpot = target->objectClass == TURRET ? 0xffffffff : static_cast<uint32_t>(ownerHotSpot);
        setTargetPosition(target->getPositionFromHS(hotSpot));
    }

    // Port fix: the original leaves the destination uninitialised when there is no target position.
    vector_3d to;
    to.x = 0.0f;
    to.y = 0.0f;
    to.z = 0.0f;

    if (targetPosition != nullptr)
    {
        to = *targetPosition;
    }

    const float step = static_cast<BulletType*>(objType)->velocity * frameLength;
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    const float dz = to.z - from.z;
    const float groundDistanceSq = dx * dx + dy * dy;
    const bool arrived = closestDistanceSq <= groundDistanceSq;

    if (!arrived)
    {
        closestDistanceSq = groundDistanceSq;
    }

    int32_t result = arrived ? 0 : 1;

    vector_3d velocity;
    velocity.x = dx;
    velocity.y = dy;
    velocity.z = dz;
    const float length = std::sqrt(dz * dz + dx * dx + dy * dy);

    if (length != 0.0f)
    {
        velocity.x = dx / length;
        velocity.y = dy / length;
        velocity.z = dz / length;
    }

    velocity.x *= step;
    velocity.y *= step;
    velocity.z *= step;
    bulletPosition.x += velocity.x;
    bulletPosition.y += velocity.y;
    bulletPosition.z += velocity.z;

    if (smoke != nullptr)
    {
        result = 1;
        smoke->setOwnerPosition(bulletPosition);
        smoke->setOwnerVelocity(velocity);
        smoke->update();

        if (arrived)
        {
            smoke->stopSmoking();
            result = 0;
        }
    }

    if (light != nullptr)
    {
        vector_3d lightPos = bulletPosition;
        light->setPosition(lightPos);
        light->update();
    }

    // A mech facing the other way draws the bullet mirrored.
    drawRotation = -150;

    // Port fix: the original reads the owner's class without checking it for null.
    if (shooter != nullptr && shooter->objectClass == BATTLEMECH)
    {
        const frame_of_ref frame = shooter->getFrame();
        float cosFacing = UnitX.y * frame.i.y + UnitX.x * frame.i.x + UnitX.z * frame.i.z;

        if (cosFacing < -1.0f)
        {
            cosFacing = -1.0f;
        }

        if (1.0f < cosFacing)
        {
            cosFacing = 1.0f;
        }

        double facing = std::acos(static_cast<double>(cosFacing)) * RADIANS_TO_DEGREES;

        if (frame.i.y < 0.0f)
        {
            facing = -facing;
        }

        drawRotation = std::abs(facing) <= 90.0 ? -150 : 150;
    }

    if (result != 0)
    {
        return result;
    }

    // Arrived: apply the shots (in multiplayer only the server does, and sends them on).
    if (target != nullptr)
    {
        if (MPlayer == nullptr)
        {
            for (int32_t i = 0; i < numShots; i++)
            {
                target->handleWeaponHit(&shotInfo[i], 0);
            }
        }
        else if (MPlayer->isServer != 0)
        {
            for (int32_t i = 0; i < numShots; i++)
            {
                target->handleWeaponHit(&shotInfo[i], 1);
            }
        }
    }

    const BulletType* bulletType = static_cast<BulletType*>(objType);
    GameObject* effect = createObject(
        static_cast<int32_t>(target == nullptr ? bulletType->bulletMissEffect : bulletType->bulletHitEffect));

    if (effect == nullptr)
    {
        return result;
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

    if (unknown2C == 0)
    {
        if (objectList->head != nullptr)
        {
            objectList->head->addNode(effect);
        }
    }
    else
    {
        delete effect;
    }

    // A miss leaves a crater and sets off a live mine where it lands.
    if (target == nullptr && targetPosition != nullptr)
    {
        if (unknown2C == 0)
        {
            craterManager->addCrater(6, *targetPosition, 1);
        }

        int32_t tileR = 0;
        int32_t tileC = 0;
        int32_t cellR = 0;
        int32_t cellC = 0;
        GameMap->worldToMapPos(*targetPosition, tileR, tileC, cellR, cellC);

        // Port fix: a miss can land off the map, where the original reads (and writes) outside it.
        if (!GameMap->onMap(tileR, tileC))
        {
            return result;
        }

        MapTile& tile = GameMap->map[GameMap->width * tileR + tileC];

        if ((tile.overlay & 0x1800) == 0x1000 || (tile.overlay & 0x6000) == 0x4000)
        {
            CreateExplosion(MineExplosion, *targetPosition, MineSplashDamage, worldUnitsPerMeter * MineSplashRange);
            tile.overlay |= 0x1800;
            tile.overlay |= 0x6000;
        }
    }

    return result;
}

auto Bullet::render() -> void
{
    if (unknown2C != 0)
    {
        return;
    }

    const int32_t firstFrame = justCreated;

    if (firstFrame == 0 && appearance != nullptr)
    {
        appearance->render(drawRotation);
    }

    if (smoke != nullptr && firstFrame == 0)
    {
        smoke->render();
    }

    if (light != nullptr)
    {
        light->render();
    }
}

auto Bullet::destroy() -> void
{
    delete targetPosition;
    targetPosition = nullptr;
    delete appearance;
    appearance = nullptr;

    if (smoke != nullptr)
    {
        delete smoke;
        smoke = nullptr;
    }

    delete light;
    light = nullptr;
}

auto Bullet::init(ObjectType* objType) -> int32_t
{
    int32_t result = GameObject::init(objType);

    if (result != 0)
    {
        return result;
    }

    justCreated = 1;
    AppearanceType* apprType = appearanceTypeList->getAppearance(objType->appearName, 0);

    if (apprType != nullptr)
    {
        if ((apprType->appearanceNum & 0xff000000) != 0x6000000)
        {
            return static_cast<int32_t>(0xdcdc0010);
        }

        auto* armAppearance = new ArmAppearance;
        appearance = armAppearance;

        if (armAppearance == nullptr)
        {
            return static_cast<int32_t>(0xdcdc000f);
        }

        armAppearance->init(nullptr, nullptr);
        armAppearance->ownerObject = nullptr;

        if ((result = armAppearance->init(apprType, this)) != 0)
        {
            return result;
        }

        armAppearance->ownerObject = this;
    }

    const auto* bulletType = static_cast<BulletType*>(objType);

    if (static_cast<int32_t>(bulletType->smokeObjectId) != -1)
    {
        smoke = static_cast<Smoke*>(createObject(static_cast<int32_t>(bulletType->smokeObjectId)));
    }

    if (static_cast<int32_t>(bulletType->lightObjectId) != -1)
    {
        light = createObject(static_cast<int32_t>(bulletType->lightObjectId));
    }

    objectClass = BULLET;
    return 0;
}

auto Bullet::setOwner(BaseObject* newOwner) -> void
{
    owner = static_cast<GameObject*>(newOwner);
}

auto Bullet::setTarget(BaseObject* newTarget) -> void
{
    target = static_cast<GameObject*>(newTarget);
}

auto Bullet::setTargetPosition(vector_3d position) -> void
{
    if (targetPosition == nullptr)
    {
        targetPosition = new vector_3d;
    }

    *targetPosition = position;
}

auto Bullet::connect(GameObject* source, vector_3d targetPos, int32_t sourceHotSpot) -> void
{
    owner = source;
    ownerHotSpot = sourceHotSpot;
    setTargetPosition(targetPos);
}
