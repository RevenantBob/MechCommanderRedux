#include "stdafx.h"
#include "object/MCSmoke.h"
#include "camera/MCCamera.h"
#include "engine/MCElementBuffer.h"
#include "engine/MCVfxElement.h"
#include "gui/asystem.h"
#include "lib/MCFatal.h"
#include "lib/MCFrameOfRef.h"
#include "lib/MCDice.h"
#include "main/main.h"
#include "object/MCEffectSystem.h"
#include "object/MCObjectDrawing.h"
#include "object/MCSmokeType.h"
#include "terrain/MCTerrain.h"
#include "vfx/MCVfxFunctions.h"

namespace
{
    /// <summary>Radians to degrees, as MCX.EXE stores it.</summary>
    constexpr double RadiansToDegrees = 0x1.ca5dc1a6402aap+5;
    /// <summary>The angle the smoke's reference frame is turned by (a hair under pi / 4, as MCX.EXE stores it).</summary>
    constexpr double SmokeFrameAngle = 0x1.921fb5443e88cp-1;
    /// <summary>One over 360, as MCX.EXE stores it.</summary>
    constexpr double OneOver360 = 0x1.6c16c16c16c17p-9;
    /// <summary>The init error of a smoke whose type has no spheres (the original's empty sphere block was null).</summary>
    constexpr int32_t NoSpheresError = static_cast<int32_t>(0xdcdc000d);

    /// <summary>A random offset of up to twice <paramref name="range"/>, centred on zero.</summary>
    float RandomSpread(float range)
    {
        return static_cast<float>(RandomNumber(static_cast<int32_t>(range + range))) - range;
    }

    /// <summary>A random value of up to half <paramref name="speed"/>, positive or (on a coin flip) negative.</summary>
    float RandomBounce(float speed)
    {
        if (RollDice(50) != 0)
        {
            return static_cast<float>(RandomNumber(static_cast<int32_t>(speed * 0.5)));
        }

        return static_cast<float>(-RandomNumber(static_cast<int32_t>(speed * 0.5)));
    }

    /// <summary>
    /// The frame of a rotated smoke shape facing along <paramref name="velocity"/>: the angle from the reference
    /// direction (the world frame turned by SmokeFrameAngle, looking down its -j axis) in 32 steps.
    /// </summary>
    int32_t RotationIndex(const MCVector3D& velocity)
    {
        float vx = velocity.X;
        float vy = velocity.Y;
        float vz = velocity.Z;
        const float length = std::sqrt(vx * vx + vy * vy + vz * vz);

        if (length != 0.0f)
        {
            vx /= length;
            vy /= length;
            vz /= length;
        }

        // The world frame turned about k.
        const auto s = static_cast<float>(std::sin(SmokeFrameAngle));
        const auto c = static_cast<float>(std::cos(SmokeFrameAngle));
        MCFrameOfRef frame;
        frame.I = UnitX;
        frame.J = UnitY;
        frame.K = UnitZ;
        const MCVector3D oldI = frame.I;
        frame.I = frame.I * c + frame.J * s;
        frame.J = frame.J * c - oldI * s;

        // The angle between -j and the velocity, signed by the side it's on.
        const float refX = -frame.J.X;
        const float refY = -frame.J.Y;
        MCVector3D reference(refX, refY, -frame.J.Z);
        const auto refLength = static_cast<float>(reference.Magnitude());

        if (refLength != 0.0f)
        {
            reference.X /= refLength;
            reference.Y /= refLength;
            reference.Z /= refLength;
        }

        MCVector3D direction(vx, vy, vz);
        direction.Normalize();
        double angle = AcosMatherr(static_cast<double>(reference | direction)) * RadiansToDegrees;

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

        return static_cast<int16_t>(static_cast<int32_t>(std::floor(angle * 32.0f * OneOver360)));
    }
} // namespace

MCSmoke::~MCSmoke()
{
    // A mover's smoke goes with the mover, which may outlive the mission's effect system.
    if (MCEffectSystem* effects = EffectSystem(); effects != nullptr)
    {
        effects->GiveBackSmokeSpheres(static_cast<int32_t>(Spheres.size()));
    }
}

auto MCSmoke::StopSmoking() -> void
{
    EndTime = MCPort::Milliseconds();
}

auto MCSmoke::StartSmoking() -> void
{
    JustStarted = true;

    for (MCSmokeSphere& sphere : Spheres)
    {
        sphere.Active = false;
    }
}

auto MCSmoke::IsVisible(size_t sphereIndex) -> bool
{
    if (sphereIndex >= Spheres.size())
    {
        return false;
    }

    MCCamera* camera = ActiveMainCamera();

    if (camera == nullptr)
    {
        return false;
    }

    MCSmokeSphere& sphere = Spheres[sphereIndex];
    const MCVector2D screen = ProjectToScreen(sphere.Position, *camera);
    sphere.ScreenX = screen.X;
    sphere.ScreenY = screen.Y;
    ScreenPos = screen;
    uint8_t* shape = static_cast<MCSmokeType*>(ObjType)->SmokeShape.Data();

    if (shape == nullptr)
    {
        return false;
    }

    // On screen when the frame's box overlaps the view.
    if (std::memcmp(shape, "1.10", 4) != 0)
    {
        Fatal(0, " BAD VFX Shape ");
    }

    const int32_t count = VfxShapeCount(shape);

    if (count <= sphere.Frame)
    {
        sphere.Frame = count - 1;
    }

    const int32_t minXY = VfxShapeMinxy(shape, sphere.Frame);
    const int32_t resolution = VfxShapeResolution(shape, sphere.Frame);
    const float scale = camera->CameraScale == 1 ? 0.5f : 1.0f;
    const float left = scale * static_cast<float>(minXY >> 16) + sphere.ScreenX;
    const float top = scale * static_cast<float>(static_cast<int16_t>(minXY)) + sphere.ScreenY;
    const float right = scale * static_cast<float>(resolution >> 16) + left;
    const float bottom = scale * static_cast<float>(static_cast<int16_t>(resolution)) + top;

    if (0.0f <= right && 0.0f <= bottom &&
        left <= static_cast<float>(static_cast<int16_t>(std::floor(camera->ViewWidth))) &&
        top <= static_cast<float>(static_cast<int16_t>(std::floor(camera->ViewHeight))))
    {
        WindowsVisible = Turn;
        return true;
    }

    return false;
}

auto MCSmoke::Update() -> int32_t
{
    const auto* smokeType = static_cast<MCSmokeType*>(ObjType);

    if (JustStarted)
    {
        JustStarted = false;
        const int32_t durationMs = smokeType->Duration * 1000;
        NextSphereTime = 0;
        EndTime = MCPort::Milliseconds() + static_cast<uint32_t>(durationMs);
    }

    if (Spheres.empty())
    {
        return 0;
    }

    // Puff a new sphere smokePerSecond times a second until the smoke ends.
    const uint32_t now = MCPort::Milliseconds();

    if (now < EndTime && NextSphereTime <= now)
    {
        const auto interval =
            static_cast<int32_t>(std::floor(1.0 / static_cast<double>(smokeType->SmokePerSecond) * 1000.0));
        NextSphereTime = static_cast<uint32_t>(interval) + now;
        NewSmokeSphere();
    }

    // Drift the spheres; one that sinks into the ground bounces off sideways and creeps along it.
    for (size_t i = 0; i < Spheres.size(); i++)
    {
        MCSmokeSphere& sphere = Spheres[i];

        if (!sphere.Active)
        {
            continue;
        }

        const float stepY = FrameLength * sphere.Velocity.Y;
        const float stepZ = FrameLength * sphere.Velocity.Z;
        sphere.Position.X =
            static_cast<float>(static_cast<double>(sphere.Velocity.X) * FrameLength + sphere.Position.X);
        sphere.Position.Y = stepY + sphere.Position.Y;
        sphere.Position.Z = stepZ + sphere.Position.Z;

        if (!sphere.OnGround && !smokeType->HasRotation)
        {
            const float elevation = Terrain()->GetTerrainElevation(sphere.Position);

            if (sphere.Position.Z < elevation)
            {
                sphere.Velocity.X = RandomBounce(sphere.Velocity.Z);
                sphere.Velocity.Y = RandomBounce(sphere.Velocity.Z);
                sphere.Velocity.Z = 0.1f;
                sphere.Position.Z = elevation;
                sphere.OnGround = true;
            }
        }

        // Faithful: a sphere that goes off the screen dies.
        Spheres[i].Active = IsVisible(i);
    }

    // Done once the time is up and every sphere has gone.
    if (now <= EndTime)
    {
        return 1;
    }

    return std::ranges::any_of(Spheres, &MCSmokeSphere::Active) ? 1 : 0;
}

auto MCSmoke::Render() -> void
{
    if (GamePaused != 0)
    {
        OnScreen();
    }

    if (JustStarted || Spheres.empty())
    {
        return;
    }

    ElementList()->OpenGroup(static_cast<int32_t>(static_cast<float>(DepthBias) - ScreenPos.Y), 1);
    const auto* smokeType = static_cast<MCSmokeType*>(ObjType);
    uint8_t* shape = smokeType->SmokeShape.Data();

    for (MCSmokeSphere& sphere : Spheres)
    {
        if (!sphere.Active)
        {
            continue;
        }

        Position = sphere.Position;
        ScreenPos.X = sphere.ScreenX;
        ScreenPos.Y = sphere.ScreenY;

        // Advance the animation by the whole frames the time now covers; a sphere past its last frame is done.
        sphere.FrameTime = FrameLength + sphere.FrameTime;
        const double frames = std::floor(static_cast<double>(sphere.FrameTime * smokeType->FrameRate));

        if (sphere.FrameCount < static_cast<int32_t>(frames))
        {
            const int32_t advanced = static_cast<int32_t>(frames) - sphere.FrameCount;
            sphere.FrameCount = static_cast<int32_t>(frames);

            if (advanced != 0)
            {
                sphere.Frame += advanced;
                int32_t lastFrame = VfxShapeCount(shape);

                if (smokeType->HasRotation)
                {
                    lastFrame /= smokeType->NumRotations;
                }

                if (lastFrame <= sphere.Frame)
                {
                    sphere.Active = false;
                }
            }
        }

        if (!sphere.Active)
        {
            continue;
        }

        MCVfxElement* element;

        if (!smokeType->HasRotation)
        {
            element = ElementList()->Make<MCVfxElement>(shape, ScreenPos.X, ScreenPos.Y, sphere.Frame, 0, nullptr, 0);
        }
        else
        {
            // Rotated smoke picks its facing's set of frames.
            const int32_t rotation = RotationIndex(sphere.Velocity);
            const int32_t framesPerRotation = VfxShapeCount(shape) / smokeType->NumRotations;
            element = ElementList()->Make<MCVfxElement>(shape, ScreenPos.X, ScreenPos.Y,
                                                        framesPerRotation * rotation + sphere.Frame, 0, nullptr, 0);
        }

        ElementList()->Add(element);
    }
}

auto MCSmoke::NewSmokeSphere() -> void
{
    if (Spheres.empty())
    {
        return;
    }

    auto* source = static_cast<MCGameObject*>(Owner);

    if (source != nullptr)
    {
        SetOwnerPosition(source->GetPositionFromHS(OwnerHotSpot));
    }

    if (!OwnerPosition.has_value())
    {
        return;
    }

    // A new sphere near the owner, drifting with (a share of) its velocity plus a little randomness, and rising.
    const auto* smokeType = static_cast<MCSmokeType*>(ObjType);
    MCSmokeSphere& sphere = Spheres[NextSphere];
    sphere.Position = *OwnerPosition;
    const float offsetX = RandomSpread(smokeType->RandomPosX);
    const float offsetY = RandomSpread(smokeType->RandomPosY);
    const float offsetZ = RandomSpread(smokeType->RandomPosZ);
    sphere.Position.X = offsetX + sphere.Position.X;
    sphere.Position.Y = offsetY + sphere.Position.Y;
    sphere.Position.Z = offsetZ + sphere.Position.Z;

    if (source != nullptr)
    {
        SetOwnerVelocity(source->GetVelocity());
    }

    if (!OwnerVelocity.has_value())
    {
        sphere.Velocity.Z = 0.0f;
        sphere.Velocity.Y = 0.0f;
        sphere.Velocity.X = 0.0f;
    }
    else
    {
        sphere.Velocity = *OwnerVelocity;
        sphere.Velocity.X = smokeType->SlowDownPercent * sphere.Velocity.X;
        sphere.Velocity.Y = smokeType->SlowDownPercent * sphere.Velocity.Y;
        sphere.Velocity.Z = smokeType->SlowDownPercent * sphere.Velocity.Z;
        const float velX = RandomSpread(smokeType->RandomVelX);
        const float velY = RandomSpread(smokeType->RandomVelY);
        const float velZ = RandomSpread(smokeType->RandomVelZ);
        sphere.Velocity.X = velX + sphere.Velocity.X;
        sphere.Velocity.Y = velY + sphere.Velocity.Y;
        sphere.Velocity.Z = velZ + sphere.Velocity.Z;
    }

    // Faithful: the rise speed replaces the vertical velocity just worked out.
    sphere.Velocity.Z = smokeType->ZVelocity;
    sphere.Active = true;
    sphere.Frame = 0;
    sphere.FrameCount = 0;
    sphere.FrameTime = 0.0f;
    sphere.OnGround = false;
    NextSphere++;

    if (NextSphere == Spheres.size())
    {
        NextSphere = 0;
    }
}

auto MCSmoke::Init(MCObjectType* objType) -> int32_t
{
    if (const int32_t result = MCGameObject::Init(objType); result != 0)
    {
        return result;
    }

    JustStarted = true;
    const auto* smokeType = static_cast<MCSmokeType*>(ObjType);

    // Original behaviour, OB-152 fixed: the spheres came from the smoke manager's pool (gamesys.fit's
    // MaxSmokeSpheres), and a smoke that found too few left wasn't made. Only a type without spheres fails now.
    if (smokeType->MaxSmokeSpheres == 0)
    {
        return NoSpheresError;
    }

    Spheres.resize(smokeType->MaxSmokeSpheres);

    if (MCEffectSystem* effects = EffectSystem(); effects != nullptr)
    {
        effects->TakeSmokeSpheres(static_cast<int32_t>(Spheres.size()));
    }

    ObjectClass = MCObjectClass::Smoke;
    DepthBias = -200;
    return 0;
}
