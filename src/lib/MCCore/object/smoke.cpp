#include "stdafx.h"
#include "object/smoke.h"
#include "camera/camera.h"
#include "camera/camlist.h"
#include "engine/ceglist.h"
#include "engine/cevfx.h"
#include "gui/asystem.h"
#include "lib/aerror.h"
#include "lib/cident.h"
#include "lib/cvmath.h"
#include "lib/file.h"
#include "lib/inifile.h"
#include "logistics/logmain.h"
#include "main/main.h"
#include "object/object.h"
#include "terrain/terrain.h"
#include "vfx/vfxfuncs.h"
#include "platform/MCRenderer.h"

namespace
{
    /// <summary>Radians to degrees, as MCX.EXE stores it.</summary>
    constexpr double RADIANS_TO_DEGREES = 0x1.ca5dc1a6402aap+5;
    /// <summary>The angle the smoke's reference frame is turned by (a hair under pi / 4, as MCX.EXE stores it).</summary>
    constexpr double SMOKE_FRAME_ANGLE = 0x1.921fb5443e88cp-1;
    /// <summary>One over 360, as MCX.EXE stores it.</summary>
    constexpr double ONE_OVER_360 = 0x1.6c16c16c16c17p-9;

    /// <summary>A random offset of up to twice <paramref name="range"/>, centred on zero.</summary>
    float randomSpread(float range)
    {
        return static_cast<float>(RandomNumber(static_cast<int32_t>(range + range))) - range;
    }

    /// <summary>A random value of up to half <paramref name="speed"/>, positive or (on a coin flip) negative.</summary>
    float randomBounce(float speed)
    {
        if (RollDice(50) != 0)
        {
            return static_cast<float>(RandomNumber(static_cast<int32_t>(speed * 0.5)));
        }

        return static_cast<float>(-RandomNumber(static_cast<int32_t>(speed * 0.5)));
    }

    /// <summary>
    /// The frame of a rotated smoke shape facing along <paramref name="velocity"/>: the angle from the reference
    /// direction (the world frame turned by SMOKE_FRAME_ANGLE, looking down its -j axis) in 32 steps.
    /// </summary>
    int32_t rotationIndex(const vector_3d& velocity)
    {
        float vx = velocity.x;
        float vy = velocity.y;
        float vz = velocity.z;
        const float length = std::sqrt(vx * vx + vy * vy + vz * vz);

        if (length != 0.0f)
        {
            vx /= length;
            vy /= length;
            vz /= length;
        }

        // The world frame turned about k.
        const auto s = static_cast<float>(std::sin(SMOKE_FRAME_ANGLE));
        const auto c = static_cast<float>(std::cos(SMOKE_FRAME_ANGLE));
        frame_of_ref frame;
        frame.i = UnitX;
        frame.j = UnitY;
        frame.k = UnitZ;
        const vector_3d oldI = frame.i;
        frame.i = frame.i * c + frame.j * s;
        frame.j = frame.j * c - oldI * s;

        // The angle between -j and the velocity, signed by the side it's on.
        const float refX = -frame.j.x;
        const float refY = -frame.j.y;
        vector_3d reference(refX, refY, -frame.j.z);
        const auto refLength = static_cast<float>(reference.magnitude());

        if (refLength != 0.0f)
        {
            reference.x /= refLength;
            reference.y /= refLength;
            reference.z /= refLength;
        }

        vector_3d direction(vx, vy, vz);
        direction.normalize();
        double angle = acosMatherr(static_cast<double>(reference | direction)) * RADIANS_TO_DEGREES;

        if (0.0f <= vy * refX - refY * vx)
        {
            angle = -angle;
        }

        if (0.0 <= angle)
        {
            angle = 360.0f - angle;
        }
        else
        {
            angle = std::abs(static_cast<int32_t>(angle));
        }

        return static_cast<int16_t>(static_cast<int32_t>(std::floor(angle * 32.0f * ONE_OVER_360)));
    }
} // namespace

SmokeManager* smokeManager = nullptr;

//---------------------------------------------------------------------------
// SmokeType
//---------------------------------------------------------------------------

auto SmokeType::createInstance() -> BaseObject*
{
    auto* newSmoke = new Smoke;

    if (newSmoke == nullptr)
    {
        return nullptr;
    }

    if (newSmoke->init(this) != 0)
    {
        return nullptr;
    }

    newSmoke->idNumber = NextIdNumber++;
    return newSmoke;
}

auto SmokeType::destroy() -> void
{
    if (smokeManager != nullptr)
    {
        smokeManager->sphereBlocks.Free(smokeShape);
    }
}

auto SmokeType::init(File* objFile, uint32_t fileSize) -> int32_t
{
    FitIniFile smokeFile;
    int32_t result = smokeFile.open(objFile, fileSize, 50);

    if (result != 0)
    {
        return result;
    }

    if ((result = smokeFile.seekBlock("SmokeData")) != 0)
    {
        return result;
    }

    if ((result = smokeFile.readIdFloat("zVelocity", zVelocity)) != 0)
    {
        return result;
    }

    if ((result = smokeFile.readIdLong("Duration", duration)) != 0)
    {
        return result;
    }

    if ((result = smokeFile.readIdFloat("SmokePerSecond", smokePerSecond)) != 0)
    {
        return result;
    }

    if ((result = smokeFile.readIdULong("MaxSmokeSpheres", maxSmokeSpheres)) != 0)
    {
        return result;
    }

    if ((result = smokeFile.readIdFloat("SlowDownPercent", slowDownPercent)) != 0)
    {
        return result;
    }

    if ((result = smokeFile.readIdFloat("randomVelX", randomVelX)) != 0)
    {
        return result;
    }

    if ((result = smokeFile.readIdFloat("randomVelY", randomVelY)) != 0)
    {
        return result;
    }

    if ((result = smokeFile.readIdFloat("randomVelZ", randomVelZ)) != 0)
    {
        return result;
    }

    if ((result = smokeFile.readIdFloat("randomPosX", randomPosX)) != 0)
    {
        return result;
    }

    if ((result = smokeFile.readIdFloat("randomPosY", randomPosY)) != 0)
    {
        return result;
    }

    if ((result = smokeFile.readIdFloat("randomPosZ", randomPosZ)) != 0)
    {
        return result;
    }

    if (smokeFile.readIdFloat("FrameRate", frameRate) != 0)
    {
        frameRate = 15.0f;
    }

    if (smokeFile.readIdBoolean("HasRotation", hasRotation) == 0)
    {
        if ((result = smokeFile.readIdLong("NumRotations", numRotations)) != 0)
        {
            return result;
        }
    }
    else
    {
        hasRotation = 0;
    }

    char shapeName[80];

    if ((result = smokeFile.readIdString("SmokeShape", shapeName, 79)) != 0)
    {
        return result;
    }

    FullPathFileName shapePath;
    shapePath.init(shapesPath, shapeName, ".shp");
    File shapeFile;

    if ((result = shapeFile.open(shapePath, READ, 50)) != 0)
    {
        return result;
    }

    const uint32_t size = shapeFile.fileSize();

    if (size != 0)
    {
        if (smokeManager != nullptr)
        {
            smokeShape = static_cast<uint8_t*>(smokeManager->sphereBlocks.Allocate(size));
        }

        if (smokeShape != nullptr)
        {
            shapeFile.read(smokeShape, static_cast<int32_t>(size));
            MCRenderer::RegisterData(smokeShape, size, MCDataKind::Shapes);
        }
    }

    zVelocity = worldUnitsPerMeter * zVelocity;
    return ObjectType::init(&smokeFile);
}

auto SmokeType::handleCollision(GameObject*, GameObject*) -> int
{
    return 0;
}

auto SmokeType::handleDestruction(GameObject*, GameObject*) -> int
{
    return 0;
}

//---------------------------------------------------------------------------
// Smoke
//---------------------------------------------------------------------------

Smoke::Smoke()
{
    endTime = 0;
    nextSphereTime = 0;
    nextSphere = 0;
    spheres = nullptr;
    numSpheres = 0;
    ownerHotSpot = 0;
    ownerPosition = nullptr;
    ownerVelocity = nullptr;
    unknownA8 = 0;
    owner = nullptr;
}

auto Smoke::init() -> void
{
}

auto Smoke::stopSmoking() -> void
{
    endTime = MCPort::Milliseconds();
}

auto Smoke::startSmoking() -> void
{
    justStarted = 1;

    for (int32_t i = 0; i < numSpheres; i++)
    {
        spheres[i].active = 0;
    }
}

auto Smoke::isVisible(int32_t sphereIndex) -> int
{
    if (spheres == nullptr || sphereIndex >= numSpheres)
    {
        return 0;
    }

    Camera* camera = cameraList->findCameraFromIDNumber(1);

    if (camera == nullptr || camera->active == 0)
    {
        return 0;
    }

    SmokeSphere& sphere = spheres[sphereIndex];
    vector_2d screen100;
    vector_2d screen50;

    if (land != nullptr)
    {
        vector_3d spherePos = sphere.position;
        land->projectTerrain(spherePos, screen100, screen50);
    }

    float screenY;

    if (camera->cameraScale == 1)
    {
        sphere.screenX = (screen50.x - camera->screenUL50.x) + camera->halfWidth;
        screenY = screen50.y - camera->screenUL50.y;
    }
    else
    {
        sphere.screenX = (screen100.x - camera->screenUL.x) + camera->halfWidth;
        screenY = screen100.y - camera->screenUL.y;
    }

    screenPos.x = sphere.screenX;
    sphere.screenY = screenY + camera->halfHeight;
    screenPos.y = sphere.screenY;

    if (sphere.shape == nullptr)
    {
        return 0;
    }

    // On screen when the frame's box overlaps the view.
    if (std::memcmp(sphere.shape, "1.10", 4) != 0)
    {
        Fatal(0, " BAD VFX Shape ");
    }

    const int32_t count = VFX_shape_count(sphere.shape);

    if (count <= sphere.frame)
    {
        sphere.frame = count - 1;
    }

    const int32_t minXY = VFX_shape_minxy(sphere.shape, sphere.frame);
    const int32_t resolution = VFX_shape_resolution(sphere.shape, sphere.frame);
    const float scale = camera->cameraScale == 1 ? 0.5f : 1.0f;
    const float left = scale * static_cast<float>(minXY >> 16) + sphere.screenX;
    const float top = scale * static_cast<float>(static_cast<int16_t>(minXY)) + sphere.screenY;
    const float right = scale * static_cast<float>(resolution >> 16) + left;
    const float bottom = scale * static_cast<float>(static_cast<int16_t>(resolution)) + top;

    if (0.0f <= right && 0.0f <= bottom &&
        left <= static_cast<float>(static_cast<int16_t>(std::floor(camera->viewWidth))) &&
        top <= static_cast<float>(static_cast<int16_t>(std::floor(camera->viewHeight))))
    {
        windowsVisible = turn;
        return 1;
    }

    return 0;
}

auto Smoke::update() -> int32_t
{
    const auto* smokeType = static_cast<SmokeType*>(objType);

    if (justStarted != 0)
    {
        justStarted = 0;
        const int32_t durationMs = smokeType->duration * 1000;
        nextSphereTime = 0;
        endTime = MCPort::Milliseconds() + static_cast<uint32_t>(durationMs);
    }

    if (spheres == nullptr)
    {
        return 0;
    }

    // Puff a new sphere smokePerSecond times a second until the smoke ends.
    const uint32_t now = MCPort::Milliseconds();

    if (now < endTime && nextSphereTime <= now)
    {
        const auto interval =
            static_cast<int32_t>(std::floor(1.0 / static_cast<double>(smokeType->smokePerSecond) * 1000.0));
        nextSphereTime = static_cast<uint32_t>(interval) + now;
        newSmokeSphere();
    }

    // Drift the spheres; one that sinks into the ground bounces off sideways and creeps along it.
    for (int32_t i = 0; i < numSpheres; i++)
    {
        SmokeSphere& sphere = spheres[i];

        if (sphere.active == 0)
        {
            continue;
        }

        const float stepY = frameLength * sphere.velocity.y;
        const float stepZ = frameLength * sphere.velocity.z;
        sphere.position.x =
            static_cast<float>(static_cast<double>(sphere.velocity.x) * frameLength + sphere.position.x);
        sphere.position.y = stepY + sphere.position.y;
        sphere.position.z = stepZ + sphere.position.z;

        if (sphere.onGround == 0 && smokeType->hasRotation == 0)
        {
            const float elevation = land->getTerrainElevation(sphere.position);

            if (sphere.position.z < elevation)
            {
                sphere.velocity.x = randomBounce(sphere.velocity.z);
                sphere.velocity.y = randomBounce(sphere.velocity.z);
                sphere.velocity.z = 0.1f;
                sphere.position.z = elevation;
                sphere.onGround = 1;
            }
        }

        // Faithful: a sphere that goes off the screen dies.
        spheres[i].active = isVisible(i);
    }

    // Done once the time is up and every sphere has gone.
    if (now <= endTime)
    {
        return 1;
    }

    int32_t result = 0;

    for (uint32_t i = 0; i < smokeType->maxSmokeSpheres; i++)
    {
        if (spheres[i].active != 0)
        {
            result = 1;
        }
    }

    return result;
}

auto Smoke::render() -> void
{
    if (gamePaused != 0)
    {
        onScreen();
    }

    if (justStarted != 0 || spheres == nullptr)
    {
        return;
    }

    ElementList->openGroup(static_cast<int32_t>(static_cast<float>(unknownB0) - screenPos.y), 1);
    const auto* smokeType = static_cast<SmokeType*>(objType);

    for (int32_t i = 0; i < numSpheres; i++)
    {
        SmokeSphere& sphere = spheres[i];

        if (sphere.active == 0)
        {
            continue;
        }

        position = sphere.position;
        screenPos.x = sphere.screenX;
        screenPos.y = sphere.screenY;

        // Advance the animation by the whole frames the time now covers; a sphere past its last frame is done.
        sphere.frameTime = frameLength + sphere.frameTime;
        const double frames = std::floor(static_cast<double>(sphere.frameTime * smokeType->frameRate));

        if (sphere.frameCount < static_cast<int32_t>(frames))
        {
            const int32_t advanced = static_cast<int32_t>(frames) - sphere.frameCount;
            sphere.frameCount = static_cast<int32_t>(frames);

            if (advanced != 0)
            {
                sphere.frame += advanced;
                int32_t lastFrame = VFX_shape_count(sphere.shape);

                if (smokeType->hasRotation != 0)
                {
                    lastFrame /= smokeType->numRotations;
                }

                if (lastFrame <= sphere.frame)
                {
                    sphere.active = 0;
                }
            }
        }

        if (sphere.active == 0)
        {
            continue;
        }

        VFXElement* element;

        if (smokeType->hasRotation == 0)
        {
            element =
                ElementPool::Make<VFXElement>(sphere.shape, screenPos.x, screenPos.y, sphere.frame, 0, nullptr, 0, 0);
            std::strcpy(element->name, "smoke2");
        }
        else
        {
            // Rotated smoke picks its facing's set of frames.
            const int32_t rotation = rotationIndex(sphere.velocity);
            const int32_t framesPerRotation = VFX_shape_count(sphere.shape) / smokeType->numRotations;
            element = ElementPool::Make<VFXElement>(sphere.shape, screenPos.x, screenPos.y,
                                                    framesPerRotation * rotation + sphere.frame, 0, nullptr, 0, 0);
            std::strcpy(element->name, "smoke1");
        }

        ElementList->add(element);
    }
}

auto Smoke::destroy() -> void
{
    if (ownerPosition != nullptr)
    {
        delete ownerPosition;
        ownerPosition = nullptr;
    }

    if (ownerVelocity != nullptr)
    {
        delete ownerVelocity;
        ownerVelocity = nullptr;
    }

    smokeManager->freeSpheres(spheres, numSpheres);
    spheres = nullptr;
}

auto Smoke::newSmokeSphere() -> void
{
    if (spheres == nullptr)
    {
        return;
    }

    auto* source = static_cast<GameObject*>(owner);

    if (source != nullptr)
    {
        setOwnerPosition(source->getPositionFromHS(ownerHotSpot));
    }

    if (ownerPosition == nullptr)
    {
        return;
    }

    // A new sphere near the owner, drifting with (a share of) its velocity plus a little randomness, and rising.
    const auto* smokeType = static_cast<SmokeType*>(objType);
    SmokeSphere& sphere = spheres[nextSphere];
    sphere.position = *ownerPosition;
    const float offsetX = randomSpread(smokeType->randomPosX);
    const float offsetY = randomSpread(smokeType->randomPosY);
    const float offsetZ = randomSpread(smokeType->randomPosZ);
    sphere.position.x = offsetX + sphere.position.x;
    sphere.position.y = offsetY + sphere.position.y;
    sphere.position.z = offsetZ + sphere.position.z;

    if (source != nullptr)
    {
        setOwnerVelocity(source->getVelocity());
    }

    if (ownerVelocity == nullptr)
    {
        sphere.velocity.z = 0.0f;
        sphere.velocity.y = 0.0f;
        sphere.velocity.x = 0.0f;
    }
    else
    {
        sphere.velocity = *ownerVelocity;
        sphere.velocity.x = smokeType->slowDownPercent * sphere.velocity.x;
        sphere.velocity.y = smokeType->slowDownPercent * sphere.velocity.y;
        sphere.velocity.z = smokeType->slowDownPercent * sphere.velocity.z;
        const float velX = randomSpread(smokeType->randomVelX);
        const float velY = randomSpread(smokeType->randomVelY);
        const float velZ = randomSpread(smokeType->randomVelZ);
        sphere.velocity.x = velX + sphere.velocity.x;
        sphere.velocity.y = velY + sphere.velocity.y;
        sphere.velocity.z = velZ + sphere.velocity.z;
    }

    // Faithful: the rise speed replaces the vertical velocity just worked out.
    sphere.velocity.z = smokeType->zVelocity;
    sphere.active = 1;
    sphere.frame = 0;
    sphere.frameCount = 0;
    sphere.frameTime = 0.0f;
    sphere.onGround = 0;
    nextSphere++;

    if (nextSphere == numSpheres)
    {
        nextSphere = 0;
    }
}

auto Smoke::init(ObjectType* objType) -> int32_t
{
    int32_t result = GameObject::init(objType);

    if (result != 0)
    {
        return result;
    }

    justStarted = 1;
    const auto* smokeType = static_cast<SmokeType*>(this->objType);
    numSpheres = static_cast<int32_t>(smokeType->maxSmokeSpheres);
    spheres = smokeManager->getSpheres(numSpheres);

    if (spheres == nullptr)
    {
        return static_cast<int32_t>(0xdcdc000d);
    }

    for (int32_t i = 0; i < numSpheres; i++)
    {
        SmokeSphere& sphere = spheres[i];
        sphere.position.z = 0.0f;
        sphere.position.y = 0.0f;
        sphere.position.x = 0.0f;
        sphere.velocity.z = 0.0f;
        sphere.velocity.y = 0.0f;
        sphere.velocity.x = 0.0f;
        sphere.active = 0;
        sphere.shape = smokeType->smokeShape;
        sphere.frame = 0;
        sphere.frameCount = 0;
        sphere.frameTime = 0.0f;
    }

    objectClass = SMOKE;
    unknownB0 = -200;
    return 0;
}

auto Smoke::setOwner(BaseObject* owner) -> void
{
    this->owner = owner;
}

auto Smoke::setOwnerPosition(vector_3d position) -> void
{
    if (ownerPosition == nullptr)
    {
        ownerPosition = new vector_3d;
    }

    *ownerPosition = position;
}

auto Smoke::setOwnerVelocity(vector_3d velocity) -> void
{
    if (ownerVelocity == nullptr)
    {
        ownerVelocity = new vector_3d;
    }

    *ownerVelocity = velocity;
}
