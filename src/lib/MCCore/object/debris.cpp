#include "stdafx.h"
#include "object/debris.h"
#include "appear/apprtype.h"
#include "camera/camera.h"
#include "camera/camlist.h"
#include "gui/asystem.h"
#include "lib/cvmath.h"
#include "lib/file.h"
#include "lib/inifile.h"
#include "main/main.h"
#include "sprite/armactor.h"
#include "terrain/terrain.h"

//---------------------------------------------------------------------------
// DebrisType
//---------------------------------------------------------------------------

DebrisType::DebrisType()
{
    armFallYaw = 0.0f;
    armFallYawRange = 0.0f;
    armFallVelMag = 0.0f;
    armFallVelRange = 0.0f;
    armFallDecelRate = 0.0f;
}

auto DebrisType::createInstance() -> BaseObject*
{
    auto* newDebris = new Debris;

    if (newDebris == nullptr)
    {
        return nullptr;
    }

    if (newDebris->init(this) != 0)
    {
        return nullptr;
    }

    newDebris->idNumber = NextIdNumber++;
    return newDebris;
}

auto DebrisType::destroy() -> void
{
}

auto DebrisType::init(File* objFile, uint32_t fileSize) -> int32_t
{
    FitIniFile debrisFile;
    int32_t result = debrisFile.open(objFile, fileSize, 50);

    if (result != 0)
    {
        return result;
    }

    if ((result = debrisFile.seekBlock("ArmFall")) != 0)
    {
        return result;
    }

    if ((result = debrisFile.readIdFloat("ArmFallYaw", armFallYaw)) != 0)
    {
        return result;
    }

    if ((result = debrisFile.readIdFloat("ArmFallYawRange", armFallYawRange)) != 0)
    {
        return result;
    }

    if ((result = debrisFile.readIdFloat("ArmFallVelMag", armFallVelMag)) != 0)
    {
        return result;
    }

    if ((result = debrisFile.readIdFloat("ArmFallVelRange", armFallVelRange)) != 0)
    {
        return result;
    }

    if ((result = debrisFile.readIdFloat("ArmFallDecelRate", armFallDecelRate)) != 0)
    {
        return result;
    }

    return ObjectType::init(&debrisFile);
}

auto DebrisType::handleCollision(GameObject*, GameObject*) -> int
{
    return 0;
}

auto DebrisType::handleDestruction(GameObject*, GameObject*) -> int
{
    return 0;
}

//---------------------------------------------------------------------------
// Debris
//---------------------------------------------------------------------------

Debris::Debris()
{
    frame.i = UnitX;
    frame.j = UnitY;
    frame.k = UnitZ;
    justCreated = 1;
    visible = 1;
    velocity.x = 0.0f;
    appearance = nullptr;
    decelRate = 0.0f;
    unknown90 = 0;
    velocity.z = 0.0f;
    velocity.y = 0.0f;
    stopped = 0;
    fallDone = 0;
}

auto Debris::init() -> void
{
}

auto Debris::onScreen() -> int
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

auto Debris::update() -> int32_t
{
    const auto* debrisType = static_cast<DebrisType*>(objType);

    if (justCreated != 0)
    {
        // The direction set by the creator becomes a velocity of armFallVelMag plus a random share.
        const float speed = static_cast<float>(RandomNumber(static_cast<int32_t>(debrisType->armFallVelRange))) +
                            debrisType->armFallVelMag;
        decelRate = debrisType->armFallDecelRate;
        justCreated = 0;
        velocity.x = speed * velocity.x;
        velocity.y = speed * velocity.y;
        velocity.z = speed * velocity.z;
        visible = onScreen();
    }

    // Slide along the ground; once the fall animation is over, slow down (by decelRate) until stopped.
    if (stopped == 0)
    {
        const float moveX = velocity.x * frameLength * worldUnitsPerMeter;
        const float moveY = worldUnitsPerMeter * frameLength * velocity.y;

        if (fallDone != 0)
        {
            const float change = frameLength * decelRate;
            float dirX = velocity.x;
            float dirY = velocity.y;
            float dirZ = velocity.z;
            const float speed = std::sqrt(dirX * dirX + dirZ * dirZ + dirY * dirY);

            if (speed != 0.0f)
            {
                dirX /= speed;
                dirY /= speed;
                dirZ /= speed;
            }

            velocity.z = 0.0f;
            velocity.x = dirX * change + velocity.x;
            velocity.y = dirY * change + velocity.y;
            velocity.z = dirZ * change + velocity.z;

            if (std::sqrt(velocity.z * velocity.z + velocity.y * velocity.y + velocity.x * velocity.x) <= 0.0f)
            {
                velocity.z = 0.0f;
                velocity.y = 0.0f;
                velocity.x = 0.0f;
                stopped = 1;
            }
        }

        position.x = moveX + position.x;
        position.y = moveY + position.y;
    }

    visible = onScreen();

    if (appearance != nullptr)
    {
        appearance->visible = visible;

        if (appearance->update() == 0)
        {
            fallDone = 1;
        }
    }

    return 1;
}

auto Debris::randomAngle(float& angle) -> void
{
    const auto* debrisType = static_cast<DebrisType*>(objType);
    const float yaw = debrisType->armFallYaw + angle;
    angle = yaw;

    if (RollDice(50) != 0)
    {
        angle = static_cast<float>(RandomNumber(static_cast<int32_t>(debrisType->armFallYawRange))) + yaw;
    }
    else
    {
        angle = yaw - static_cast<float>(RandomNumber(static_cast<int32_t>(debrisType->armFallYawRange)));
    }
}

auto Debris::render() -> void
{
    if (gamePaused != 0)
    {
        onScreen();
    }

    if (windowsVisible == turn && justCreated == 0 && appearance != nullptr)
    {
        appearance->render(0);
    }
}

auto Debris::destroy() -> void
{
    delete appearance;
    appearance = nullptr;
}

auto Debris::init(ObjectType* objType) -> int32_t
{
    int32_t result = GameObject::init(objType);

    if (result != 0)
    {
        return result;
    }

    justCreated = 1;
    collisionsOn = 0;
    AppearanceType* apprType = appearanceTypeList->getAppearance(objType->appearName, 0);

    if (apprType == nullptr)
    {
        return static_cast<int32_t>(0xdebb0002);
    }

    auto* armAppearance = new ArmAppearance;
    appearance = armAppearance;

    if (armAppearance == nullptr)
    {
        return static_cast<int32_t>(0xdebb0003);
    }

    armAppearance->init(nullptr, nullptr);
    armAppearance->ownerObject = nullptr;

    if ((apprType->appearanceNum & 0xff000000) != 0x6000000)
    {
        return static_cast<int32_t>(0xdebb0004);
    }

    if ((result = armAppearance->init(apprType, this)) != 0)
    {
        return result;
    }

    objectClass = DEBRIS;
    return 0;
}

auto Debris::setPaintScheme(int32_t paintScheme) -> void
{
    static_cast<ArmAppearance*>(appearance)->fadeTableIndex = paintScheme;
}
