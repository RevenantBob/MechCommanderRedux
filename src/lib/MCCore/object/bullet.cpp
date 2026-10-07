#include "stdafx.h"
#include "object/bullet.h"
#include "appear/apprtype.h"
#include "camera/camera.h"
#include "camera/camlist.h"
#include "engine/MCCraterManager.h"
#include "lib/MCFrameOfRef.h"
#include "lib/MCVector2D.h"
#include "lib/MCDice.h"
#include "lib/MCFile.h"
#include "lib/MCFitIniFile.h"
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
    void ProjectToScreen(MCBigGameObject* object, MCCamera* camera)
    {
        MCVector2D screen100;
        MCVector2D screen50;

        if (Land != nullptr)
        {
            Land->ProjectTerrain(object->Position, screen100, screen50);
        }

        float screenY;

        if (camera->CameraScale == 1)
        {
            object->ScreenPos.X = (screen50.X - camera->ScreenUL50.X) + camera->HalfWidth;
            screenY = screen50.Y - camera->ScreenUL50.Y;
        }
        else
        {
            object->ScreenPos.X = (screen100.X - camera->ScreenUL.X) + camera->HalfWidth;
            screenY = screen100.Y - camera->ScreenUL.Y;
        }

        object->ScreenPos.Y = screenY + camera->HalfHeight;
    }
} // namespace

//---------------------------------------------------------------------------
// BulletType
//---------------------------------------------------------------------------

MCBulletType::MCBulletType()
{
    SoundEffectId = 0xffffffff;
    BulletHitEffect = 0xffffffff;
    BulletMissEffect = 0xffffffff;
    SmokeObjectId = 0xffffffff;
}

auto MCBulletType::CreateInstance() -> MCBaseObject*
{
    auto* newBullet = new MCBullet;

    if (newBullet == nullptr)
    {
        return nullptr;
    }

    if (newBullet->Init(this) != 0)
    {
        return nullptr;
    }

    newBullet->IdNumber = NextIdNumber++;
    return newBullet;
}

auto MCBulletType::Destroy() -> void
{
}

auto MCBulletType::Init(MCFile* objFile, uint32_t fileSize) -> int32_t
{
    MCFitIniFile bulletFile;
    int32_t result = bulletFile.Open(objFile, fileSize);

    if (result != 0)
    {
        return result;
    }

    if (bulletFile.SeekBlock("BulletData") == 0)
    {
        if ((result = bulletFile.ReadIdULong("SoundEffectId", SoundEffectId)) != 0)
        {
            return result;
        }

        if ((result = bulletFile.ReadIdULong("BulletHitEffect", BulletHitEffect)) != 0)
        {
            return result;
        }

        if ((result = bulletFile.ReadIdULong("BulletMissEffect", BulletMissEffect)) != 0)
        {
            return result;
        }

        if ((result = bulletFile.ReadIdFloat("Velocity", Velocity)) != 0)
        {
            return result;
        }

        if ((result = bulletFile.ReadIdFloat("CloseDistance", CloseDistance)) != 0)
        {
            return result;
        }

        if ((result = bulletFile.ReadIdULong("SmokeObjectId", SmokeObjectId)) != 0)
        {
            return result;
        }

        if (bulletFile.ReadIdULong("LightObjectId", LightObjectId) != 0)
        {
            LightObjectId = 0xffffffff;
        }
    }

    result = MCObjectType::Init(&bulletFile);
    ObjectTypeManager->Load(static_cast<int32_t>(BulletHitEffect), 1);
    ObjectTypeManager->Load(static_cast<int32_t>(BulletMissEffect), 1);
    ObjectTypeManager->Load(static_cast<int32_t>(SmokeObjectId), 1);
    return result;
}

auto MCBulletType::HandleCollision(MCGameObject*, MCGameObject*) -> int
{
    return 0;
}

auto MCBulletType::HandleDestruction(MCGameObject*, MCGameObject*) -> int
{
    return 0;
}

//---------------------------------------------------------------------------
// Bullet
//---------------------------------------------------------------------------

MCBullet::MCBullet()
{
    OwnerHotSpot = 0;
    TargetHotSpot = 0;
    TargetPosition = nullptr;
    JustCreated = 1;
    Appearance = nullptr;
    NumShots = 0;
    Smoke = nullptr;
    ClosestDistanceSq = 0.0f;
    Target = nullptr;
    Owner = nullptr;
    Light = nullptr;
    DrawRotation = 0;
}

auto MCBullet::Init() -> void
{
}

auto MCBullet::IsVisible() -> int
{
    MCCamera* camera = CameraList->FindCameraFromIDNumber(1);

    if (camera == nullptr || camera->Active == 0)
    {
        return 0;
    }

    ProjectToScreen(this, camera);

    if (Appearance != nullptr && Appearance->RecalcBounds(camera) == 0)
    {
        return 0;
    }

    WindowsVisible = Turn;
    return 1;
}

auto MCBullet::Update() -> int32_t
{
    if (JustCreated != 0)
    {
        JustCreated = 0;
        CollisionsOn = 0;

        if (Owner != nullptr)
        {
            Position = Owner->GetPositionFromHS(static_cast<uint32_t>(OwnerHotSpot));
        }

        const uint32_t soundId = static_cast<MCBulletType*>(ObjType)->SoundEffectId;

        if (soundId != 0xffffffff)
        {
            SoundSystem->PlayDigitalSample(soundId, 1, this, 0, 0);
        }

        BulletPosition = Position;
        ClosestDistanceSq = 1.0e8f;
    }

    MCGameObject* shooter = Owner;

    if (shooter != nullptr)
    {
        Position = shooter->GetPositionFromHS(static_cast<uint32_t>(OwnerHotSpot));
    }

    const int visibleNow = IsVisible();

    if (Appearance != nullptr)
    {
        Appearance->Visible = visibleNow;
        Appearance->Update();
    }

    // Fly toward the target (following it if it moves); once the ground distance stops shrinking, we're there.
    MCVector3D from = BulletPosition;

    if (Target != nullptr)
    {
        // Port fix (OB-017): follow the hot spot the hit effect plays at (the original used ownerHotSpot).
        const uint32_t hotSpot = Target->ObjectClass == TURRET ? 0xffffffff : static_cast<uint32_t>(TargetHotSpot);
        SetTargetPosition(Target->GetPositionFromHS(hotSpot));
    }

    // Port fix: the original leaves the destination uninitialised when there is no target position.
    MCVector3D to;
    to.X = 0.0f;
    to.Y = 0.0f;
    to.Z = 0.0f;

    if (TargetPosition != nullptr)
    {
        to = *TargetPosition;
    }

    const double step = static_cast<double>(static_cast<MCBulletType*>(ObjType)->Velocity) * FrameLength;
    const double dxWide = static_cast<double>(to.X) - from.X;
    const auto dx = static_cast<float>(dxWide);
    const float dy = to.Y - from.Y;
    const float dz = to.Z - from.Z;
    const double dxSq = static_cast<double>(dx) * dx;
    const double dySq = static_cast<double>(dy) * dy;
    const double groundDistanceSq = dxSq + dySq;
    const bool arrived = ClosestDistanceSq <= groundDistanceSq;

    if (!arrived)
    {
        ClosestDistanceSq = static_cast<float>(groundDistanceSq);
    }

    int32_t result = arrived ? 0 : 1;

    MCVector3D velocity;
    double unitX = dxWide;
    velocity.Y = dy;
    velocity.Z = dz;
    const double length = std::sqrt(static_cast<double>(dz) * dz + dxSq + dySq);
    const auto lengthF = static_cast<float>(length);

    if (length != 0.0)
    {
        unitX = static_cast<double>(dx) / lengthF;
        velocity.Y = static_cast<float>(static_cast<double>(dy) / lengthF);
        velocity.Z = static_cast<float>(static_cast<double>(dz) / lengthF);
    }

    velocity.X = static_cast<float>(unitX * step);
    velocity.Y = static_cast<float>(velocity.Y * step);
    velocity.Z = static_cast<float>(velocity.Z * step);
    BulletPosition.X += velocity.X;
    BulletPosition.Y += velocity.Y;
    BulletPosition.Z += velocity.Z;

    if (Smoke != nullptr)
    {
        result = 1;
        Smoke->SetOwnerPosition(BulletPosition);
        Smoke->SetOwnerVelocity(velocity);
        Smoke->Update();

        if (arrived)
        {
            Smoke->StopSmoking();
            result = 0;
        }
    }

    if (Light != nullptr)
    {
        MCVector3D lightPos = BulletPosition;
        Light->SetPosition(lightPos);
        Light->Update();
    }

    // A mech facing the other way draws the bullet mirrored.
    DrawRotation = -150;

    // Port fix: the original reads the owner's class without checking it for null.
    if (shooter != nullptr && shooter->ObjectClass == BATTLEMECH)
    {
        const MCFrameOfRef frame = shooter->GetFrame();
        float cosFacing = UnitX.Y * frame.I.Y + UnitX.X * frame.I.X + UnitX.Z * frame.I.Z;

        if (cosFacing < -1.0f)
        {
            cosFacing = -1.0f;
        }

        if (1.0f < cosFacing)
        {
            cosFacing = 1.0f;
        }

        double facing = AcosMatherr(static_cast<double>(cosFacing)) * RADIANS_TO_DEGREES;

        if (frame.I.Y < 0.0f)
        {
            facing = -facing;
        }

        DrawRotation = std::abs(facing) <= 90.0 ? -150 : 150;
    }

    if (result != 0)
    {
        return result;
    }

    // Arrived: apply the shots (in multiplayer only the server does, and sends them on).
    if (Target != nullptr)
    {
        if (MPlayer == nullptr)
        {
            for (int32_t i = 0; i < NumShots; i++)
            {
                Target->HandleWeaponHit(&ShotInfo[i], 0);
            }
        }
        else if (MPlayer->IsServer != 0)
        {
            for (int32_t i = 0; i < NumShots; i++)
            {
                Target->HandleWeaponHit(&ShotInfo[i], 1);
            }
        }
    }

    const MCBulletType* bulletType = static_cast<MCBulletType*>(ObjType);
    MCGameObject* effect = CreateObject(
        static_cast<int32_t>(Target == nullptr ? bulletType->BulletMissEffect : bulletType->BulletHitEffect));

    if (effect == nullptr)
    {
        return result;
    }

    if (Target != nullptr)
    {
        const uint32_t hotSpot = Target->ObjectClass == TURRET ? 0xffffffff : static_cast<uint32_t>(TargetHotSpot);
        MCVector3D hitPos = Target->GetPositionFromHS(hotSpot);
        effect->SetPosition(hitPos);
    }
    else if (TargetPosition != nullptr)
    {
        effect->SetPosition(*TargetPosition);
    }

    if (ObjectList->Head != nullptr)
    {
        ObjectList->Head->AddNode(effect);
    }

    // A miss leaves a crater and sets off a live mine where it lands.
    if (Target == nullptr && TargetPosition != nullptr)
    {
        CraterManager()->AddCrater(6, *TargetPosition, 1);

        int32_t tileR = 0;
        int32_t tileC = 0;
        int32_t cellR = 0;
        int32_t cellC = 0;
        GameMap->WorldToMapPos(*TargetPosition, tileR, tileC, cellR, cellC);

        // Port fix: a miss can land off the map, where the original reads (and writes) outside it.
        if (!GameMap->OnMap(tileR, tileC))
        {
            return result;
        }

        MCMapTile& tile = GameMap->Map[GameMap->Width * tileR + tileC];

        if ((tile.Overlay & 0x1800) == 0x1000 || (tile.Overlay & 0x6000) == 0x4000)
        {
            CreateExplosion(MineExplosion, *TargetPosition, MineSplashDamage, WorldUnitsPerMeter * MineSplashRange);
            tile.Overlay |= 0x1800;
            tile.Overlay |= 0x6000;
        }
    }

    return result;
}

auto MCBullet::Render() -> void
{
    const int32_t firstFrame = JustCreated;

    if (firstFrame == 0 && Appearance != nullptr)
    {
        Appearance->Render(DrawRotation);
    }

    if (Smoke != nullptr && firstFrame == 0)
    {
        Smoke->Render();
    }

    if (Light != nullptr)
    {
        Light->Render();
    }
}

auto MCBullet::Destroy() -> void
{
    delete TargetPosition;
    TargetPosition = nullptr;
    delete Appearance;
    Appearance = nullptr;

    if (Smoke != nullptr)
    {
        delete Smoke;
        Smoke = nullptr;
    }

    delete Light;
    Light = nullptr;
}

auto MCBullet::Init(MCObjectType* objType) -> int32_t
{
    int32_t result = MCGameObject::Init(objType);

    if (result != 0)
    {
        return result;
    }

    JustCreated = 1;
    MCAppearanceType* apprType = AppearanceTypeList->GetAppearance(objType->AppearName, 0);

    if (apprType != nullptr)
    {
        if ((apprType->AppearanceNum & 0xff000000) != 0x6000000)
        {
            return static_cast<int32_t>(0xdcdc0010);
        }

        auto* armAppearance = new MCArmAppearance;
        Appearance = armAppearance;

        if (armAppearance == nullptr)
        {
            return static_cast<int32_t>(0xdcdc000f);
        }

        armAppearance->Init(nullptr, nullptr);
        armAppearance->OwnerObject = nullptr;

        if ((result = armAppearance->Init(apprType, this)) != 0)
        {
            return result;
        }

        armAppearance->OwnerObject = this;
    }

    const auto* bulletType = static_cast<MCBulletType*>(objType);

    if (static_cast<int32_t>(bulletType->SmokeObjectId) != -1)
    {
        Smoke = static_cast<MCSmoke*>(CreateObject(static_cast<int32_t>(bulletType->SmokeObjectId)));
    }

    if (static_cast<int32_t>(bulletType->LightObjectId) != -1)
    {
        Light = CreateObject(static_cast<int32_t>(bulletType->LightObjectId));
    }

    ObjectClass = BULLET;
    return 0;
}

auto MCBullet::SetOwner(MCBaseObject* newOwner) -> void
{
    Owner = static_cast<MCGameObject*>(newOwner);
}

auto MCBullet::SetTarget(MCBaseObject* newTarget) -> void
{
    Target = static_cast<MCGameObject*>(newTarget);
}

auto MCBullet::SetTargetPosition(MCVector3D position) -> void
{
    if (TargetPosition == nullptr)
    {
        TargetPosition = new MCVector3D;
    }

    *TargetPosition = position;
}

auto MCBullet::Connect(MCGameObject* source, MCVector3D targetPos, int32_t sourceHotSpot) -> void
{
    Owner = source;
    OwnerHotSpot = sourceHotSpot;
    SetTargetPosition(targetPos);
}
