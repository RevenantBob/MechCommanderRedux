#include "stdafx.h"
#include "object/explode.h"
#include "appear/MCAppearanceType.h"
#include "appear/MCAppearanceTypeList.h"
#include "camera/MCCamera.h"
#include "camera/MCCameraList.h"
#include "gui/asystem.h"
#include "lib/MCIDString.h"
#include "lib/MCFrameOfRef.h"
#include "lib/MCVector2D.h"
#include "lib/MCDice.h"
#include "lib/MCFile.h"
#include "lib/MCFitIniFile.h"
#include "main/main.h"
#include "mission/scenario.h"
#include "network/multplyr.h"
#include "object/MCCollisionSystem.h"
#include "object/gate.h"
#include "object/MCObjectSystem.h"
#include "object/MCObjectQueue.h"
#include "object/turret.h"
#include "sound/soundsys.h"
#include "sprite/MCVfxAppearance.h"
#include "terrain/MCTerrain.h"
#include "object/MCObjectType.h"
#include "object/MCWeaponShotInfo.h"

namespace
{
    /// <summary>Runs a collision check between the explosion and every object of the list.</summary>
    void CollideWithList(MCExplosion* explosion, MCObjectList* list)
    {
        if (list == nullptr)
        {
            return;
        }

        // Port fix (OB-015): every object is checked; the original only stepped to the next one after an
        // object with a type, so one without hung the game.
        for (MCBaseObject* object : *list)
        {
            auto* other = static_cast<MCGameObject*>(object);

            if (other->GetObjectType() != nullptr)
            {
                // The block and vertex are fetched but never used.
                int32_t otherBlock = -1;
                int32_t otherVertex = -1;

                switch (other->ObjectClass)
                {
                    case MCObjectClass::Building:
                    case MCObjectClass::Tree:
                    case MCObjectClass::TerrainObject:
                    case MCObjectClass::MiscTerrainObject:
                    case MCObjectClass::TreeBuilding:
                    case MCObjectClass::CameraDrone:
                        other->GetBlockAndVertexNumber(otherBlock, otherVertex);
                        break;
                    default:
                        break;
                }

                CollisionSystem()->DetectStaticCollision(explosion, other);
            }
        }
    }

    /// <summary>
    /// For turrets and gates: false when the explosion's radius doesn't reach the collider's extent (measured
    /// centre to centre).
    /// </summary>
    bool ReachesExtent(MCGameObject* explosion, MCGameObject* collider, float extent)
    {
        const MCVector3D colliderPos = collider->GetPosition();
        const MCVector3D explosionPos = explosion->GetPosition();
        const double dx = static_cast<double>(colliderPos.X) - explosionPos.X;
        const double dy = static_cast<double>(colliderPos.Y) - explosionPos.Y;
        const float dz = colliderPos.Z - explosionPos.Z;
        const auto distance = static_cast<float>(std::sqrt((dx * dx + dy * dy) + static_cast<double>(dz) * dz));
        return !(extent < distance &&
                 static_cast<double>(explosion->GetExtentRadius()) < static_cast<double>(distance) - extent);
    }
} // namespace

//---------------------------------------------------------------------------
// ExplosionType
//---------------------------------------------------------------------------

MCExplosionType::MCExplosionType()
{
    DmgLevel = 0;
    SoundEffectId = 0xffffffff;
    ExplosionRadius = 0;
    DamageChunkSize = 0.0f;
}

auto MCExplosionType::CreateInstance() -> std::unique_ptr<MCBaseObject>
{
    auto newExplosion = std::make_unique<MCExplosion>();

    if (newExplosion == nullptr)
    {
        return nullptr;
    }

    if (newExplosion->Init(this) != 0)
    {
        return nullptr;
    }

    newExplosion->IdNumber = NextIdNumber++;
    return newExplosion;
}

auto MCExplosionType::Destroy() -> void
{
}

auto MCExplosionType::Init(MCFile* objFile, uint32_t fileSize) -> int32_t
{
    MCFitIniFile explFile;
    int32_t result = explFile.Open(objFile, fileSize);

    if (result != 0)
    {
        return result;
    }

    if ((result = explFile.SeekBlock("ExplosionData")) != 0)
    {
        return result;
    }

    if ((result = explFile.ReadIdULong("DmgLevel", DmgLevel)) != 0)
    {
        return result;
    }

    if ((result = explFile.ReadIdULong("SoundEffectId", SoundEffectId)) != 0)
    {
        return result;
    }

    if (explFile.ReadIdLong("ExplosionRadius", ExplosionRadius) != 0)
    {
        ExplosionRadius = 0;
    }

    if (explFile.ReadIdULong("LightObjectId", LightObjectId) != 0)
    {
        LightObjectId = 0xffffffff;
    }

    if (explFile.ReadIdFloat("DamageChunkSize", DamageChunkSize) != 0)
    {
        DamageChunkSize = 5.0f;
    }

    return MCObjectType::Init(&explFile);
}

auto MCExplosionType::HandleCollision(MCGameObject* collidee, MCGameObject* collider) -> int
{
    // Only the server deals explosion damage in multiplayer.
    if (MPlayer != nullptr && MPlayer->IsServer == 0)
    {
        return 0;
    }

    const float damage = collidee->GetExplDmg();

    if (damage == 0.0f)
    {
        return 0;
    }

    const float chunk = DamageChunkSize < damage ? DamageChunkSize : damage;
    const int multiplayer = MPlayer != nullptr ? 1 : 0;
    MCWeaponShotInfo shot;

    switch (collider->ObjectClass)
    {
        case MCObjectClass::BattleMech:
        case MCObjectClass::GroundVehicle:
        case MCObjectClass::Elemental:
        case MCObjectClass::Mover:
        {
            // Movers take the damage in chunks, each on a location of its own.
            shot.Init(nullptr, -1, chunk, 0, 0.0f);

            for (float remaining = damage; 0.0f < remaining; remaining -= DamageChunkSize)
            {
                shot.HitLocation = collider->CalcHitLocation(collidee, -1, 4, 0);
                collider->HandleWeaponHit(&shot, multiplayer);
            }

            return 0;
        }

        case MCObjectClass::Turret:
        {
            if (!ReachesExtent(collidee, collider, static_cast<MCTurretType*>(collider->ObjType)->LittleExtent))
            {
                return 0;
            }

            float remaining = collidee->GetExplDmg();
            shot.Init(nullptr, -1, chunk, 0, 0.0f);

            for (; 0.0f < remaining; remaining -= DamageChunkSize)
            {
                shot.HitLocation = 0;
                collider->HandleWeaponHit(&shot, multiplayer);
            }

            return 0;
        }

        case MCObjectClass::Gate:
        {
            if (!ReachesExtent(collidee, collider, static_cast<MCGateType*>(collider->ObjType)->LittleExtent))
            {
                return 0;
            }

            shot.Init(nullptr, -1, chunk, 0, 0.0f);

            for (float remaining = damage; 0.0f < remaining; remaining -= DamageChunkSize)
            {
                shot.HitLocation = 0;
                collider->HandleWeaponHit(&shot, multiplayer);
            }

            return 0;
        }

        default:
        {
            // Anything else takes it all at once.
            shot.Init(nullptr, -1, collidee->GetExplDmg(), 0, 0.0f);
            collider->HandleWeaponHit(&shot, multiplayer);
            return 0;
        }
    }
}

auto MCExplosionType::HandleDestruction(MCGameObject*, MCGameObject*) -> int
{
    return 0;
}

//---------------------------------------------------------------------------
// Explosion
//---------------------------------------------------------------------------

auto MCExplosion::Init() -> void
{
    JustCreated = 1;
    Appearance = nullptr;
    CollisionChecked = 0;
    TimeAlive = 0.0f;
    DamageChunkSize = 0.0f;
    Light = nullptr;
}

auto MCExplosion::GetExtentRadius() -> float
{
    return ExplRadius;
}

auto MCExplosion::SetExtentRadius(float newRadius) -> void
{
    ExplRadius = newRadius;
}

auto MCExplosion::OnScreen() -> int
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

auto MCExplosion::HandleStaticCollision() -> void
{
    if (CollisionsOn == 0)
    {
        return;
    }

    int32_t blockNumber = 0;
    int32_t vertexNumber = 0;
    GetBlockAndVertexNumber(blockNumber, vertexNumber);

    // The terrain objects of the 3x3 terrain blocks around it.
    const int32_t firstBlock = blockNumber - MCTerrain::BlocksMapSide - 1;

    for (int32_t row = 0; row < 3; row++)
    {
        int32_t block = row * MCTerrain::BlocksMapSide + firstBlock;

        for (int32_t col = 0; col < 3; col++, block++)
        {
            char listName[12];
            std::sprintf(listName, "TBlk%d", block);
            CollideWithList(this, ObjectList()->FindList(listName));
            std::sprintf(listName, "RBlk%d", block);
            CollideWithList(this, ObjectList()->FindList(listName));
        }
    }
}

auto MCExplosion::Update() -> int32_t
{
    const int visibleNow = OnScreen();

    if (JustCreated != 0)
    {
        JustCreated = 0;
        CollisionsOn = 0;
        const uint32_t soundId = static_cast<MCExplosionType*>(ObjType)->SoundEffectId;

        if (soundId != 0xffffffff)
        {
            SoundSystem->PlayDigitalSample(soundId, 1, this, 0, 0);
        }
    }

    // Collisions are on for the one frame after the explosion is half a second old.
    if (CollisionChecked != 0)
    {
        CollisionsOn = 0;
    }

    const double aliveSum = static_cast<double>(FrameLength) + TimeAlive;
    TimeAlive = static_cast<float>(aliveSum);

    if (0.5 < aliveSum && CollisionChecked == 0)
    {
        CollisionChecked = 1;
        CollisionsOn = 1;
    }

    if (Light != nullptr)
    {
        MCVector3D lightPos = Position;
        Light->SetPosition(lightPos);
        Light->Update();
    }

    Appearance->Visible = visibleNow;
    return Appearance->Update();
}

auto MCExplosion::Render() -> void
{
    if (GamePaused != 0)
    {
        OnScreen();
    }

    if (JustCreated == 0 && WindowsVisible == Turn)
    {
        Appearance->Render(-150);
    }

    if (Light != nullptr)
    {
        Light->Render();
    }
}

auto MCExplosion::Destroy() -> void
{
    delete Appearance;
    Appearance = nullptr;
    delete Light;
    Light = nullptr;
}

auto MCExplosion::Init(MCObjectType* objType) -> int32_t
{
    int32_t result = MCGameObject::Init(objType);

    if (result != 0)
    {
        return result;
    }

    JustCreated = 1;
    CollisionsOn = 0;
    CollisionChecked = 0;
    TimeAlive = 0.0f;
    MCAppearanceType* apprType = AppearanceTypeList()->GetAppearance(objType->AppearName);

    if (apprType == nullptr)
    {
        return static_cast<int32_t>(0xdcdc0003);
    }

    if ((apprType->AppearanceNum & 0xff000000) != 0x2000000)
    {
        return static_cast<int32_t>(0xdcdc0005);
    }

    auto* vfxAppearance = new MCVfxAppearance;
    Appearance = vfxAppearance;

    if (vfxAppearance == nullptr)
    {
        return static_cast<int32_t>(0xdcdc0004);
    }

    vfxAppearance->Init(nullptr, nullptr);

    if ((result = vfxAppearance->Init(apprType, this)) != 0)
    {
        return result;
    }

    ObjectClass = MCObjectClass::Explosion;
    const auto* explType = static_cast<MCExplosionType*>(objType);

    if (explType->ExplosionRadius != 0)
    {
        SetExtentRadius(static_cast<float>(explType->ExplosionRadius));
        SetExplDmg(static_cast<float>(explType->DmgLevel));
    }

    if (static_cast<int32_t>(explType->LightObjectId) != -1)
    {
        Light = CreateObject(static_cast<int32_t>(explType->LightObjectId)).release();
    }

    DamageChunkSize = explType->DamageChunkSize;
    return 0;
}

void CreateExplosion(int32_t objectTypeId, MCVector3D& position, float damage, float radius)
{
    if (objectTypeId == -1)
    {
        return;
    }

    std::unique_ptr<MCGameObject> explosion = CreateObject(objectTypeId);

    if (explosion == nullptr)
    {
        return;
    }

    explosion->SetPosition(position);

    if (radius != 0.0f)
    {
        explosion->SetExtentRadius(radius);
        explosion->SetExplDmg(damage);
    }

    AddToDefaultList(std::move(explosion));
}
