#include "stdafx.h"
#include "object/prjlase.h"
#include "appear/MCAppearanceType.h"
#include "appear/MCAppearanceTypeList.h"
#include "camera/MCCamera.h"
#include "camera/MCCameraList.h"
#include "engine/MCElementBuffer.h"
#include "engine/MCPolygonElement.h"
#include "engine/MCCraterManager.h"
#include "gui/asystem.h"
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
#include "object/MCObjectSystem.h"
#include "object/MCObjectQueue.h"
#include "object/smoke.h"
#include "sound/soundsys.h"
#include "sprite/MCArmAppearance.h"
#include "terrain/MCTerrain.h"
#include "terrain/MCTacticalMap.h"
#include "vfx/MCVfx.h"
#include "object/MCObjectType.h"
#include "object/MCWeaponShotInfo.h"

namespace
{
    /// <summary>Radians to degrees, as MCX.EXE stores it.</summary>
    constexpr double RADIANS_TO_DEGREES = 0x1.ca5dc1a6402aap+5;

    /// <summary>
    /// The camera's isometric projection of <paramref name="point"/> (inlined five times in render): the offset from
    /// the eye, halved at the 50% scale, turned by the view angle and dropped by its height.
    /// </summary>
    void ProjectPoint(const MCVector3D& point, int32_t& screenX, int32_t& screenY)
    {
        const float scale = Eye->CameraScale == 1 ? 0.5f : 1.0f;
        const float dx = (point.X - Eye->Position.X) * scale;
        const float dy = (point.Y - Eye->Position.Y) * scale;
        const float dz = (point.Z - Eye->Position.Z) * scale;
        const float x = dy * Eye->CosAngle + dx * Eye->CosAngle + Eye->HalfWidth;
        const float y = ((dx * Eye->SinAngle + Eye->HalfHeight) - dy * Eye->SinAngle) - dz;
        screenX = static_cast<int32_t>(x);
        screenY = static_cast<int32_t>(y);
    }

    /// <summary>A screen vertex in palette colour <paramref name="color"/> (u, v and w zero).</summary>
    MCScreenVertex ColorVertex(const MCVector3D& point, uint8_t color)
    {
        MCScreenVertex vertex;
        ProjectPoint(point, vertex.X, vertex.Y);
        vertex.C = static_cast<MCFixed16>(color) << 16;
        vertex.U = 0;
        vertex.V = 0;
        vertex.W = 0;
        return vertex;
    }
} // namespace

//---------------------------------------------------------------------------
// ProjectileLaserType
//---------------------------------------------------------------------------

MCProjectileLaserType::MCProjectileLaserType()
{
    SoundEffectId = 0xffffffff;
    ProjectileHitEffect = 0xffffffff;
    ProjectileMissEffect = 0xffffffff;
    BulgeWidth = 0.0f;
    BulgeLength = 0.0f;
    ProjectileLength = 0.0f;

    for (int32_t i = 0; i < 4; i++)
    {
        EColor[i] = 0;
        FColor[i] = 0;
    }
}

auto MCProjectileLaserType::CreateInstance() -> std::unique_ptr<MCBaseObject>
{
    auto newLaser = std::make_unique<MCProjectileLaser>();

    if (newLaser == nullptr)
    {
        return nullptr;
    }

    if (newLaser->Init(this) != 0)
    {
        return nullptr;
    }

    newLaser->IdNumber = NextIdNumber++;
    return newLaser;
}

auto MCProjectileLaserType::Destroy() -> void
{
}

auto MCProjectileLaserType::Init(MCFile* objFile, uint32_t fileSize) -> int32_t
{
    MCFitIniFile laserFile;
    int32_t result = laserFile.Open(objFile, fileSize);

    if (result != 0)
    {
        return result;
    }

    if (laserFile.SeekBlock("ProjectileLaserData") == 0)
    {
        if ((result = laserFile.ReadIdULong("SoundEffectId", SoundEffectId)) != 0)
        {
            return result;
        }

        if ((result = laserFile.ReadIdULong("ProjectileHitEffect", ProjectileHitEffect)) != 0)
        {
            return result;
        }

        if ((result = laserFile.ReadIdULong("ProjectileMissEffect", ProjectileMissEffect)) != 0)
        {
            return result;
        }

        if ((result = laserFile.ReadIdFloat("Velocity", Velocity)) != 0)
        {
            return result;
        }

        if ((result = laserFile.ReadIdFloat("CloseDistance", CloseDistance)) != 0)
        {
            return result;
        }

        if ((result = laserFile.ReadIdFloat("ProjectileLength", ProjectileLength)) != 0)
        {
            return result;
        }

        if ((result = laserFile.ReadIdFloat("BulgeLength", BulgeLength)) != 0)
        {
            return result;
        }

        if ((result = laserFile.ReadIdFloat("BulgeWidth", BulgeWidth)) != 0)
        {
            return result;
        }

        if ((result = laserFile.ReadIdUChar("e0Color", EColor[0])) != 0)
        {
            return result;
        }

        if ((result = laserFile.ReadIdUChar("e1Color", EColor[1])) != 0)
        {
            return result;
        }

        if ((result = laserFile.ReadIdUChar("e2Color", EColor[2])) != 0)
        {
            return result;
        }

        if ((result = laserFile.ReadIdUChar("e3Color", EColor[3])) != 0)
        {
            return result;
        }

        if ((result = laserFile.ReadIdUChar("f0Color", FColor[0])) != 0)
        {
            return result;
        }

        if ((result = laserFile.ReadIdUChar("f1Color", FColor[1])) != 0)
        {
            return result;
        }

        if ((result = laserFile.ReadIdUChar("f2Color", FColor[2])) != 0)
        {
            return result;
        }

        if ((result = laserFile.ReadIdUChar("f3Color", FColor[3])) != 0)
        {
            return result;
        }

        if (laserFile.ReadIdULong("SmokeObjectId", SmokeObjectId) != 0)
        {
            SmokeObjectId = 0xffffffff;
        }

        if (laserFile.ReadIdULong("LightObjectId", LightObjectId) != 0)
        {
            LightObjectId = 0xffffffff;
        }
    }

    result = MCObjectType::Init(&laserFile);
    ObjectTypeManager()->Load(static_cast<int32_t>(ProjectileHitEffect), 1);
    ObjectTypeManager()->Load(static_cast<int32_t>(ProjectileMissEffect), 1);
    return result;
}

auto MCProjectileLaserType::HandleCollision(MCGameObject*, MCGameObject*) -> int
{
    return 0;
}

auto MCProjectileLaserType::HandleDestruction(MCGameObject*, MCGameObject*) -> int
{
    return 0;
}

//---------------------------------------------------------------------------
// ProjectileLaser
//---------------------------------------------------------------------------

MCProjectileLaser::MCProjectileLaser()
{
    Frame.ResetToWorldFrame();
    SmokeDisplacement.X = 0.0f;
    SmokeDisplacement.Y = 0.0f;
    SmokeDisplacement.Z = 0.0f;
    Frame.I = UnitX;
    Frame.J = UnitY;
    Frame.K = UnitZ;
    OwnerHotSpot = 0;
    TargetHotSpot = 0;
    TargetPosition = nullptr;
    JustCreated = 1;
    Appearance = nullptr;
    Smoke = nullptr;
    ClosestDistanceSq = 0.0f;
    Target = nullptr;
    Owner = nullptr;
    Light = nullptr;
    DrawRotation = 0;
}

auto MCProjectileLaser::Init() -> void
{
}

auto MCProjectileLaser::IsVisible() -> int
{
    MCCamera* camera = CameraList()->FindCameraFromIDNumber(1);

    if (camera == nullptr || camera->Active == 0)
    {
        return 0;
    }

    MCVector2D screen100;
    MCVector2D screen50;

    if (Terrain() != nullptr)
    {
        Terrain()->ProjectTerrain(Position, screen100, screen50);
    }

    float screenY;

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

    if (Appearance != nullptr && Appearance->RecalcBounds(camera) != 0)
    {
        WindowsVisible = Turn;
        return 1;
    }

    return 0;
}

auto MCProjectileLaser::Update() -> int32_t
{
    const auto* laserType = static_cast<MCProjectileLaserType*>(ObjType);

    if (JustCreated != 0)
    {
        // Start at the owner's hot spot, facing its way, with the head one projectile length toward the target.
        MCGameObject* shooter = Owner;
        JustCreated = 0;
        CollisionsOn = 0;

        if (shooter != nullptr)
        {
            Position = shooter->GetPositionFromHS(static_cast<uint32_t>(OwnerHotSpot));
            Frame = shooter->GetFrame();
        }

        if (laserType->SoundEffectId != 0xffffffff)
        {
            SoundSystem->PlayDigitalSample(laserType->SoundEffectId, 1, this, 0, 0);
        }

        HeadPosition = Position;
        ClosestDistanceSq = 1.0e8f;

        if (Target != nullptr)
        {
            // Port fix (OB-017): the original aimed at the target's hot spot numbered like the owner's (ownerHotSpot).
            const uint32_t hotSpot =
                Target->ObjectClass == MCObjectClass::Turret ? 0xffffffff : static_cast<uint32_t>(TargetHotSpot);
            SetTargetPosition(Target->GetPositionFromHS(hotSpot));
        }

        // Port fix: the original reads an unset destination when there is no target position.
        MCVector3D to = HeadPosition;

        if (TargetPosition != nullptr)
        {
            to = *TargetPosition;
        }

        float dx = to.X - HeadPosition.X;
        float dy = to.Y - HeadPosition.Y;
        float dz = to.Z - HeadPosition.Z;
        const float length = std::sqrt(dx * dx + dy * dy + dz * dz);

        if (length != 0.0f)
        {
            dx /= length;
            dy /= length;
            dz /= length;
        }

        HeadPosition.X = dx * laserType->ProjectileLength + HeadPosition.X;
        HeadPosition.Y = dy * laserType->ProjectileLength + HeadPosition.Y;
        HeadPosition.Z = dz * laserType->ProjectileLength + HeadPosition.Z;
    }

    MCGameObject* shooter = Owner;

    if (shooter != nullptr)
    {
        Position = shooter->GetPositionFromHS(static_cast<uint32_t>(OwnerHotSpot));
    }

    int arrived = 0;
    const int visibleNow = IsVisible();

    if (Appearance != nullptr)
    {
        Appearance->Visible = visibleNow;
        Appearance->Update();
    }

    // A mech facing the other way draws the bolt mirrored.
    DrawRotation = -150;

    // Port fix: the original reads the owner's class without checking it for null.
    if (shooter != nullptr && shooter->ObjectClass == MCObjectClass::BattleMech)
    {
        MCFrameOfRef ownerFrame = shooter->GetFrame();
        const float cosFacing = UnitX.Y * ownerFrame.I.Y + UnitX.X * ownerFrame.I.X + UnitX.Z * ownerFrame.I.Z;
        float facing = static_cast<float>(ownerFrame.MyAcos(cosFacing) * RADIANS_TO_DEGREES);

        if (ownerFrame.I.Y < 0.0f)
        {
            facing = -facing;
        }

        DrawRotation = std::abs(facing) <= 90.0f ? -150 : 150;
    }

    // Move the head toward the target; once its ground distance stops shrinking, it has arrived.
    const MCVector3D from = HeadPosition;

    if (Target != nullptr)
    {
        // Port fix (OB-017): aim at the hot spot the hit effect plays at (the original used ownerHotSpot).
        const uint32_t hotSpot =
            Target->ObjectClass == MCObjectClass::Turret ? 0xffffffff : static_cast<uint32_t>(TargetHotSpot);
        SetTargetPosition(Target->GetPositionFromHS(hotSpot));
    }

    MCVector3D to = from;

    if (TargetPosition != nullptr)
    {
        to = *TargetPosition;
    }

    const double step = static_cast<double>(laserType->Velocity) * FrameLength;
    const double dxWide = static_cast<double>(to.X) - from.X;
    const auto dx = static_cast<float>(dxWide);
    float dy = to.Y - from.Y;
    float dz = to.Z - from.Z;
    const double dxSq = static_cast<double>(dx) * dx;
    const double dySq = static_cast<double>(dy) * dy;
    const double groundDistanceSq = dxSq + dySq;

    if (ClosestDistanceSq <= groundDistanceSq)
    {
        arrived = 1;
    }
    else
    {
        ClosestDistanceSq = static_cast<float>(groundDistanceSq);
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
    HeadPosition.X = static_cast<float>(moveX + HeadPosition.X);
    HeadPosition.Y = moveY + HeadPosition.Y;
    HeadPosition.Z = moveZ + HeadPosition.Z;

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

    const float bulgeLength = laserType->BulgeLength;
    const float bulgeWidth = laserType->BulgeWidth;
    BulgeSide1.Y = backY * bulgeLength + HeadPosition.Y;
    BulgeSide1.X = static_cast<float>(backXWide * bulgeLength + HeadPosition.X);
    BulgeSide1.Z = backZ * bulgeLength + HeadPosition.Z;
    BulgeSide2 = BulgeSide1;
    BulgeCenter = BulgeSide1;
    const double backYWidth = static_cast<double>(backY) * bulgeWidth;
    const auto backXWidth = static_cast<float>(backXWide * bulgeWidth);
    BulgeSide2.X = static_cast<float>(backYWidth + BulgeSide2.X);
    BulgeSide2.Y -= backXWidth;
    BulgeSide1.X = static_cast<float>(BulgeSide1.X - backYWidth);
    BulgeSide1.Y += backXWidth;
    TailPosition.Y = backY * laserType->ProjectileLength + HeadPosition.Y;
    TailPosition.X = static_cast<float>(backXWide * laserType->ProjectileLength + HeadPosition.X);
    TailPosition.Z = backZ * laserType->ProjectileLength + HeadPosition.Z;

    // With a smoke trail, arrival is instead the tail coming within closeDistance (plus a step) of the target.
    if (Smoke != nullptr)
    {
        const MCVector3D tail = TailPosition;
        arrived = 0;

        if (Target != nullptr)
        {
            // Port fix (OB-017): see above.
            const uint32_t hotSpot =
                Target->ObjectClass == MCObjectClass::Turret ? 0xffffffff : static_cast<uint32_t>(TargetHotSpot);
            SetTargetPosition(Target->GetPositionFromHS(hotSpot));
        }

        MCVector3D smokeTo = tail;

        if (TargetPosition != nullptr)
        {
            smokeTo = *TargetPosition;
        }

        const float smokeStep = laserType->Velocity * FrameLength;
        float sx = smokeTo.X - tail.X;
        float sy = smokeTo.Y - tail.Y;
        float sz = smokeTo.Z - tail.Z;
        const double reach = static_cast<double>(smokeStep) + laserType->CloseDistance;
        const float sySq = sy * sy;
        const float sxSq = sx * sx;
        const double groundSq = static_cast<double>(sx) * sx + sySq;

        if (groundSq <= reach * reach)
        {
            Smoke->StopSmoking();
            arrived = 1;
        }

        const double smokeLength = std::sqrt(static_cast<double>(sz) * sz + sxSq + sySq);

        if (smokeLength != 0.0)
        {
            sx = static_cast<float>(sx / smokeLength);
            sy = static_cast<float>(sy / smokeLength);
            sz = static_cast<float>(sz / smokeLength);
        }

        MCVector3D smokeVelocity;
        smokeVelocity.X = sx * smokeStep;
        smokeVelocity.Y = sy * smokeStep;
        smokeVelocity.Z = sz * smokeStep;
        SmokeDisplacement.X += smokeVelocity.X;
        SmokeDisplacement.Y += smokeVelocity.Y;
        SmokeDisplacement.Z += smokeVelocity.Z;
        Smoke->SetOwnerPosition(TailPosition);
        Smoke->SetOwnerVelocity(smokeVelocity);
        Smoke->Update();
    }

    if (Light != nullptr)
    {
        MCVector3D lightPos = TailPosition;
        Light->SetPosition(lightPos);
        Light->Update();
    }

    if (arrived == 0)
    {
        return 1;
    }

    // Arrived: apply the shot (in multiplayer only the server does, and sends it on).
    if (Target != nullptr)
    {
        if (MPlayer == nullptr)
        {
            Target->HandleWeaponHit(&ShotInfo, 0);
        }
        else if (MPlayer->IsServer != 0)
        {
            Target->HandleWeaponHit(&ShotInfo, 1);
        }
    }

    std::unique_ptr<MCGameObject> effect = CreateObject(
        static_cast<int32_t>(Target == nullptr ? laserType->ProjectileMissEffect : laserType->ProjectileHitEffect));

    if (effect == nullptr)
    {
        return 0;
    }

    if (Target != nullptr)
    {
        const uint32_t hotSpot =
            Target->ObjectClass == MCObjectClass::Turret ? 0xffffffff : static_cast<uint32_t>(TargetHotSpot);
        MCVector3D hitPos = Target->GetPositionFromHS(hotSpot);
        effect->SetPosition(hitPos);
    }
    else if (TargetPosition != nullptr)
    {
        effect->SetPosition(*TargetPosition);
    }

    AddToDefaultList(std::move(effect));

    // A miss leaves a crater and sets off a live mine where it lands.
    if (Target == nullptr && TargetPosition != nullptr)
    {
        CraterManager()->AddCrater(7, *TargetPosition, 1);

        int32_t tileR = 0;
        int32_t tileC = 0;
        int32_t cellR = 0;
        int32_t cellC = 0;
        GameMap()->WorldToMapPos(*TargetPosition, tileR, tileC, cellR, cellC);

        // Port fix: a miss can land off the map, where the original reads (and writes) outside it.
        if (!GameMap()->OnMap(tileR, tileC))
        {
            return 0;
        }

        MCMapTile& tile = GameMap()->Map[GameMap()->Width * tileR + tileC];

        if ((tile.Overlay & 0x1800) == 0x1000 || (tile.Overlay & 0x6000) == 0x4000)
        {
            CreateExplosion(MineExplosion, *TargetPosition, MineSplashDamage, WorldUnitsPerMeter * MineSplashRange);
            tile.Overlay |= 0x1800;
            tile.Overlay |= 0x6000;
        }
    }

    return 0;
}

auto MCProjectileLaser::Render() -> void
{
    if (GamePaused != 0)
    {
        OnScreen();
    }

    if (JustCreated != 0)
    {
        return;
    }

    if (Appearance != nullptr)
    {
        Appearance->Render(DrawRotation);
    }

    // Two quads, head-bulge-tail-centre on each side, in the friendly or enemy colours.
    const auto* laserType = static_cast<MCProjectileLaserType*>(ObjType);
    const bool enemy = Owner == nullptr || Owner->GetAlignment() == -1;
    const uint8_t headColor = enemy ? laserType->EColor[0] : laserType->FColor[0];
    // Original behaviour (OB-020): the other corners test "no owner" and then ask that null owner for its
    // alignment, so with an owner they always take the enemy colours (without one, MCX.EXE crashes).
    const uint8_t* cornerColors = laserType->EColor;

    MCPolyElementData side1;
    side1.NumVertices = 4;
    side1.Vertices[0] = ColorVertex(HeadPosition, headColor);
    side1.Vertices[1] = ColorVertex(BulgeSide1, cornerColors[1]);
    side1.Vertices[2] = ColorVertex(TailPosition, cornerColors[2]);
    side1.Vertices[3] = ColorVertex(BulgeCenter, cornerColors[3]);
    MCPolyElementData side2;
    side2.NumVertices = 4;
    side2.Vertices[0] = side1.Vertices[0];
    side2.Vertices[1] = ColorVertex(BulgeSide2, cornerColors[1]);
    side2.Vertices[2] = side1.Vertices[2];
    side2.Vertices[3] = side1.Vertices[3];

    const int32_t depth = -side1.Vertices[0].Y;
    ElementList()->OpenGroup(depth, 1);
    ElementList()->Add(ElementList()->Make<MCPolygonElement>(side1, depth));
    ElementList()->Add(ElementList()->Make<MCPolygonElement>(side2, depth));

    if (Smoke != nullptr)
    {
        Smoke->Render();
    }

    if (Light != nullptr)
    {
        Light->Render();
    }
}

auto MCProjectileLaser::Destroy() -> void
{
    delete TargetPosition;
    TargetPosition = nullptr;
    delete Appearance;
    Appearance = nullptr;
    delete Smoke;
    Smoke = nullptr;
    delete Light;
    Light = nullptr;
}

auto MCProjectileLaser::Init(MCObjectType* objType) -> int32_t
{
    int32_t result = MCGameObject::Init(objType);

    if (result != 0)
    {
        return result;
    }

    const uint32_t appearId = objType->AppearName;
    JustCreated = 1;

    if (appearId != 0)
    {
        MCAppearanceType* apprType = AppearanceTypeList()->GetAppearance(appearId);

        if (apprType == nullptr)
        {
            return -0x2d;
        }

        if ((apprType->AppearanceNum & 0xff000000) != 0x6000000)
        {
            return -0x38;
        }

        auto* armAppearance = new MCArmAppearance;
        Appearance = armAppearance;

        if (armAppearance == nullptr)
        {
            return -0x2e;
        }

        armAppearance->Init(nullptr, nullptr);
        armAppearance->OwnerObject = nullptr;

        if ((result = armAppearance->Init(apprType, this)) != 0)
        {
            return result;
        }

        armAppearance->OwnerObject = this;
    }

    const auto* laserType = static_cast<MCProjectileLaserType*>(objType);

    if (static_cast<int32_t>(laserType->SmokeObjectId) != -1)
    {
        Smoke = CreateObjectAs<MCSmoke>(static_cast<int32_t>(laserType->SmokeObjectId)).release();
    }

    if (static_cast<int32_t>(laserType->LightObjectId) != -1)
    {
        Light = CreateObject(static_cast<int32_t>(laserType->LightObjectId)).release();
    }

    ObjectClass = MCObjectClass::ProjectileLaser;
    return 0;
}

auto MCProjectileLaser::SetOwner(MCBaseObject* newOwner) -> void
{
    Owner = static_cast<MCGameObject*>(newOwner);
}

auto MCProjectileLaser::SetTargetPosition(MCVector3D position) -> void
{
    if (TargetPosition == nullptr)
    {
        TargetPosition = new MCVector3D;
    }

    *TargetPosition = position;
}

auto MCProjectileLaser::Connect(MCGameObject* source, MCVector3D targetPos, MCWeaponShotInfo* shotInfo,
                                int32_t sourceHotSpot) -> void
{
    Owner = source;
    OwnerHotSpot = sourceHotSpot;
    SetTargetPosition(targetPos);

    if (shotInfo != nullptr)
    {
        const MCWeaponShotInfo shot = *shotInfo;
        this->ShotInfo.Init(shot.Attacker, shot.MasterId, shot.Damage, shot.HitLocation, shot.EntryAngle);
    }
}
