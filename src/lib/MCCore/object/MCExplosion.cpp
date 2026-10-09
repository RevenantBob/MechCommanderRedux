#include "stdafx.h"
#include "object/MCExplosion.h"
#include "appear/MCAppearanceType.h"
#include "appear/MCAppearanceTypeList.h"
#include "camera/MCCamera.h"
#include "gui/MCGuiSystem.h"
#include "main/main.h"
#include "object/MCCollisionSystem.h"
#include "object/MCExplosionType.h"
#include "object/MCObjectDrawing.h"
#include "object/MCObjectSystem.h"
#include "sound/MCSoundSystem.h"
#include "sprite/MCVfxAppearance.h"
#include "terrain/MCTerrain.h"

MCExplosion::MCExplosion() = default;

MCExplosion::~MCExplosion() = default;

auto MCExplosion::OnScreen() -> int
{
    MCCamera* camera = ActiveMainCamera();

    if (camera == nullptr)
    {
        return 0;
    }

    ScreenPos = ProjectToScreen(Position, *camera);

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
        for (int32_t col = 0; col < 3; col++)
        {
            CollisionSystem()->DetectBlockCollisions(this, row * MCTerrain::BlocksMapSide + firstBlock + col);
        }
    }
}

auto MCExplosion::Update() -> int32_t
{
    const int visibleNow = OnScreen();

    if (JustCreated)
    {
        JustCreated = false;
        CollisionsOn = 0;
        const uint32_t soundId = static_cast<MCExplosionType*>(ObjType)->SoundEffectId;

        if (soundId != 0xffffffff)
        {
            SoundSystem()->PlayDigitalSample(soundId, 1, this, 0, 0);
        }
    }

    // Collisions are on for the one frame after the explosion is half a second old.
    if (CollisionChecked)
    {
        CollisionsOn = 0;
    }

    const double aliveSum = static_cast<double>(FrameLength) + TimeAlive;
    TimeAlive = static_cast<float>(aliveSum);

    if (0.5 < aliveSum && !CollisionChecked)
    {
        CollisionChecked = true;
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

    if (!JustCreated && WindowsVisible == Turn)
    {
        Appearance->Render(-150);
    }

    if (Light != nullptr)
    {
        Light->Render();
    }
}

auto MCExplosion::Init(MCObjectType* objType) -> int32_t
{
    if (const int32_t result = MCGameObject::Init(objType); result != 0)
    {
        return result;
    }

    JustCreated = true;
    CollisionsOn = 0;
    CollisionChecked = false;
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

    Appearance = std::make_unique<MCVfxAppearance>();
    Appearance->Init(nullptr, nullptr);

    if (const int32_t result = Appearance->Init(apprType, this); result != 0)
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
        Light = CreateObject(static_cast<int32_t>(explType->LightObjectId));
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
