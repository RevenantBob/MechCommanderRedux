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
    /// direction (the world frame turned by SMOKE_FRAME_ANGLE, looking down its -j axis) in 32 steps.
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
        const auto s = static_cast<float>(std::sin(SMOKE_FRAME_ANGLE));
        const auto c = static_cast<float>(std::cos(SMOKE_FRAME_ANGLE));
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
        double angle = AcosMatherr(static_cast<double>(reference | direction)) * RADIANS_TO_DEGREES;

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

MCSmokeManager* SmokeManager = nullptr;

//---------------------------------------------------------------------------
// SmokeType
//---------------------------------------------------------------------------

auto MCSmokeType::CreateInstance() -> MCBaseObject*
{
    auto* newSmoke = new MCSmoke;

    if (newSmoke == nullptr)
    {
        return nullptr;
    }

    if (newSmoke->Init(this) != 0)
    {
        return nullptr;
    }

    newSmoke->IdNumber = NextIdNumber++;
    return newSmoke;
}

auto MCSmokeType::Destroy() -> void
{
    if (SmokeManager != nullptr)
    {
        SmokeManager->SphereBlocks.Free(SmokeShape);
    }
}

auto MCSmokeType::Init(MCFile* objFile, uint32_t fileSize) -> int32_t
{
    MCFitIniFile smokeFile;
    int32_t result = smokeFile.Open(objFile, fileSize, 50);

    if (result != 0)
    {
        return result;
    }

    if ((result = smokeFile.SeekBlock("SmokeData")) != 0)
    {
        return result;
    }

    if ((result = smokeFile.ReadIdFloat("zVelocity", ZVelocity)) != 0)
    {
        return result;
    }

    if ((result = smokeFile.ReadIdLong("Duration", Duration)) != 0)
    {
        return result;
    }

    if ((result = smokeFile.ReadIdFloat("SmokePerSecond", SmokePerSecond)) != 0)
    {
        return result;
    }

    if ((result = smokeFile.ReadIdULong("MaxSmokeSpheres", MaxSmokeSpheres)) != 0)
    {
        return result;
    }

    if ((result = smokeFile.ReadIdFloat("SlowDownPercent", SlowDownPercent)) != 0)
    {
        return result;
    }

    if ((result = smokeFile.ReadIdFloat("randomVelX", RandomVelX)) != 0)
    {
        return result;
    }

    if ((result = smokeFile.ReadIdFloat("randomVelY", RandomVelY)) != 0)
    {
        return result;
    }

    if ((result = smokeFile.ReadIdFloat("randomVelZ", RandomVelZ)) != 0)
    {
        return result;
    }

    if ((result = smokeFile.ReadIdFloat("randomPosX", RandomPosX)) != 0)
    {
        return result;
    }

    if ((result = smokeFile.ReadIdFloat("randomPosY", RandomPosY)) != 0)
    {
        return result;
    }

    if ((result = smokeFile.ReadIdFloat("randomPosZ", RandomPosZ)) != 0)
    {
        return result;
    }

    if (smokeFile.ReadIdFloat("FrameRate", FrameRate) != 0)
    {
        FrameRate = 15.0f;
    }

    if (smokeFile.ReadIdBoolean("HasRotation", HasRotation) == 0)
    {
        if ((result = smokeFile.ReadIdLong("NumRotations", NumRotations)) != 0)
        {
            return result;
        }
    }
    else
    {
        HasRotation = 0;
    }

    char shapeName[80];

    if ((result = smokeFile.ReadIdString("SmokeShape", shapeName, 79)) != 0)
    {
        return result;
    }

    MCFullPathFileName shapePath;
    shapePath.Init(ShapesPath, shapeName, ".shp");
    MCFile shapeFile;

    if ((result = shapeFile.Open(shapePath, READ, 50)) != 0)
    {
        return result;
    }

    const uint32_t size = shapeFile.FileSize();

    if (size != 0)
    {
        if (SmokeManager != nullptr)
        {
            SmokeShape = static_cast<uint8_t*>(SmokeManager->SphereBlocks.Allocate(size));
        }

        if (SmokeShape != nullptr)
        {
            shapeFile.Read(SmokeShape, static_cast<int32_t>(size));
            MCRenderer::RegisterData(SmokeShape, size, MCDataKind::Shapes);
        }
    }

    ZVelocity = WorldUnitsPerMeter * ZVelocity;
    return MCObjectType::Init(&smokeFile);
}

auto MCSmokeType::HandleCollision(MCGameObject*, MCGameObject*) -> int
{
    return 0;
}

auto MCSmokeType::HandleDestruction(MCGameObject*, MCGameObject*) -> int
{
    return 0;
}

//---------------------------------------------------------------------------
// Smoke
//---------------------------------------------------------------------------

MCSmoke::MCSmoke()
{
    EndTime = 0;
    NextSphereTime = 0;
    NextSphere = 0;
    Spheres = nullptr;
    NumSpheres = 0;
    OwnerHotSpot = 0;
    OwnerPosition = nullptr;
    OwnerVelocity = nullptr;
    Owner = nullptr;
}

auto MCSmoke::Init() -> void
{
}

auto MCSmoke::StopSmoking() -> void
{
    EndTime = MCPort::Milliseconds();
}

auto MCSmoke::StartSmoking() -> void
{
    JustStarted = 1;

    for (int32_t i = 0; i < NumSpheres; i++)
    {
        Spheres[i].Active = 0;
    }
}

auto MCSmoke::IsVisible(int32_t sphereIndex) -> int
{
    if (Spheres == nullptr || sphereIndex >= NumSpheres)
    {
        return 0;
    }

    MCCamera* camera = CameraList->FindCameraFromIDNumber(1);

    if (camera == nullptr || camera->Active == 0)
    {
        return 0;
    }

    MCSmokeSphere& sphere = Spheres[sphereIndex];
    MCVector2D screen100;
    MCVector2D screen50;

    if (Land != nullptr)
    {
        MCVector3D spherePos = sphere.Position;
        Land->ProjectTerrain(spherePos, screen100, screen50);
    }

    float screenY;

    if (camera->CameraScale == 1)
    {
        sphere.ScreenX = (screen50.X - camera->ScreenUL50.X) + camera->HalfWidth;
        screenY = screen50.Y - camera->ScreenUL50.Y;
    }
    else
    {
        sphere.ScreenX = (screen100.X - camera->ScreenUL.X) + camera->HalfWidth;
        screenY = screen100.Y - camera->ScreenUL.Y;
    }

    ScreenPos.X = sphere.ScreenX;
    sphere.ScreenY = screenY + camera->HalfHeight;
    ScreenPos.Y = sphere.ScreenY;

    if (sphere.Shape == nullptr)
    {
        return 0;
    }

    // On screen when the frame's box overlaps the view.
    if (std::memcmp(sphere.Shape, "1.10", 4) != 0)
    {
        Fatal(0, " BAD VFX Shape ");
    }

    const int32_t count = VfxShapeCount(sphere.Shape);

    if (count <= sphere.Frame)
    {
        sphere.Frame = count - 1;
    }

    const int32_t minXY = VfxShapeMinxy(sphere.Shape, sphere.Frame);
    const int32_t resolution = VfxShapeResolution(sphere.Shape, sphere.Frame);
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
        return 1;
    }

    return 0;
}

auto MCSmoke::Update() -> int32_t
{
    const auto* smokeType = static_cast<MCSmokeType*>(ObjType);

    if (JustStarted != 0)
    {
        JustStarted = 0;
        const int32_t durationMs = smokeType->Duration * 1000;
        NextSphereTime = 0;
        EndTime = MCPort::Milliseconds() + static_cast<uint32_t>(durationMs);
    }

    if (Spheres == nullptr)
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
    for (int32_t i = 0; i < NumSpheres; i++)
    {
        MCSmokeSphere& sphere = Spheres[i];

        if (sphere.Active == 0)
        {
            continue;
        }

        const float stepY = FrameLength * sphere.Velocity.Y;
        const float stepZ = FrameLength * sphere.Velocity.Z;
        sphere.Position.X =
            static_cast<float>(static_cast<double>(sphere.Velocity.X) * FrameLength + sphere.Position.X);
        sphere.Position.Y = stepY + sphere.Position.Y;
        sphere.Position.Z = stepZ + sphere.Position.Z;

        if (sphere.OnGround == 0 && smokeType->HasRotation == 0)
        {
            const float elevation = Land->GetTerrainElevation(sphere.Position);

            if (sphere.Position.Z < elevation)
            {
                sphere.Velocity.X = RandomBounce(sphere.Velocity.Z);
                sphere.Velocity.Y = RandomBounce(sphere.Velocity.Z);
                sphere.Velocity.Z = 0.1f;
                sphere.Position.Z = elevation;
                sphere.OnGround = 1;
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

    int32_t result = 0;

    for (uint32_t i = 0; i < smokeType->MaxSmokeSpheres; i++)
    {
        if (Spheres[i].Active != 0)
        {
            result = 1;
        }
    }

    return result;
}

auto MCSmoke::Render() -> void
{
    if (GamePaused != 0)
    {
        OnScreen();
    }

    if (JustStarted != 0 || Spheres == nullptr)
    {
        return;
    }

    ElementList->OpenGroup(static_cast<int32_t>(static_cast<float>(DepthBias) - ScreenPos.Y), 1);
    const auto* smokeType = static_cast<MCSmokeType*>(ObjType);

    for (int32_t i = 0; i < NumSpheres; i++)
    {
        MCSmokeSphere& sphere = Spheres[i];

        if (sphere.Active == 0)
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
                int32_t lastFrame = VfxShapeCount(sphere.Shape);

                if (smokeType->HasRotation != 0)
                {
                    lastFrame /= smokeType->NumRotations;
                }

                if (lastFrame <= sphere.Frame)
                {
                    sphere.Active = 0;
                }
            }
        }

        if (sphere.Active == 0)
        {
            continue;
        }

        MCVfxElement* element;

        if (smokeType->HasRotation == 0)
        {
            element = MCElementPool::Make<MCVfxElement>(sphere.Shape, ScreenPos.X, ScreenPos.Y, sphere.Frame, 0,
                                                        nullptr, 0, 0);
            std::strcpy(element->Name, "smoke2");
        }
        else
        {
            // Rotated smoke picks its facing's set of frames.
            const int32_t rotation = RotationIndex(sphere.Velocity);
            const int32_t framesPerRotation = VfxShapeCount(sphere.Shape) / smokeType->NumRotations;
            element = MCElementPool::Make<MCVfxElement>(sphere.Shape, ScreenPos.X, ScreenPos.Y,
                                                        framesPerRotation * rotation + sphere.Frame, 0, nullptr, 0, 0);
            std::strcpy(element->Name, "smoke1");
        }

        ElementList->Add(element);
    }
}

auto MCSmoke::Destroy() -> void
{
    if (OwnerPosition != nullptr)
    {
        delete OwnerPosition;
        OwnerPosition = nullptr;
    }

    if (OwnerVelocity != nullptr)
    {
        delete OwnerVelocity;
        OwnerVelocity = nullptr;
    }

    SmokeManager->FreeSpheres(Spheres, NumSpheres);
    Spheres = nullptr;
}

auto MCSmoke::NewSmokeSphere() -> void
{
    if (Spheres == nullptr)
    {
        return;
    }

    auto* source = static_cast<MCGameObject*>(Owner);

    if (source != nullptr)
    {
        SetOwnerPosition(source->GetPositionFromHS(OwnerHotSpot));
    }

    if (OwnerPosition == nullptr)
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

    if (OwnerVelocity == nullptr)
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
    sphere.Active = 1;
    sphere.Frame = 0;
    sphere.FrameCount = 0;
    sphere.FrameTime = 0.0f;
    sphere.OnGround = 0;
    NextSphere++;

    if (NextSphere == NumSpheres)
    {
        NextSphere = 0;
    }
}

auto MCSmoke::Init(MCObjectType* objType) -> int32_t
{
    int32_t result = MCGameObject::Init(objType);

    if (result != 0)
    {
        return result;
    }

    JustStarted = 1;
    const auto* smokeType = static_cast<MCSmokeType*>(this->ObjType);
    NumSpheres = static_cast<int32_t>(smokeType->MaxSmokeSpheres);
    Spheres = SmokeManager->GetSpheres(NumSpheres);

    if (Spheres == nullptr)
    {
        return static_cast<int32_t>(0xdcdc000d);
    }

    for (int32_t i = 0; i < NumSpheres; i++)
    {
        MCSmokeSphere& sphere = Spheres[i];
        sphere.Position.Z = 0.0f;
        sphere.Position.Y = 0.0f;
        sphere.Position.X = 0.0f;
        sphere.Velocity.Z = 0.0f;
        sphere.Velocity.Y = 0.0f;
        sphere.Velocity.X = 0.0f;
        sphere.Active = 0;
        sphere.Shape = smokeType->SmokeShape;
        sphere.Frame = 0;
        sphere.FrameCount = 0;
        sphere.FrameTime = 0.0f;
    }

    ObjectClass = SMOKE;
    DepthBias = -200;
    return 0;
}

auto MCSmoke::SetOwner(MCBaseObject* owner) -> void
{
    this->Owner = owner;
}

auto MCSmoke::SetOwnerPosition(MCVector3D position) -> void
{
    if (OwnerPosition == nullptr)
    {
        OwnerPosition = new MCVector3D;
    }

    *OwnerPosition = position;
}

auto MCSmoke::SetOwnerVelocity(MCVector3D velocity) -> void
{
    if (OwnerVelocity == nullptr)
    {
        OwnerVelocity = new MCVector3D;
    }

    *OwnerVelocity = velocity;
}
