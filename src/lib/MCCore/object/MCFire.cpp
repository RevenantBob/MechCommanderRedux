#include "stdafx.h"
#include "object/MCFire.h"
#include "ai/MCMoveSystem.h"
#include "ai/MCScenarioMap.h"
#include "appear/MCAppearanceType.h"
#include "appear/MCAppearanceTypeList.h"
#include "camera/MCCamera.h"
#include "engine/MCElementBuffer.h"
#include "engine/MCVfxElement.h"
#include "lib/MCDice.h"
#include "main/main.h"
#include "mission/mission.h"
#include "network/multplyr.h"
#include "object/MCCollisionSystem.h"
#include "object/MCEffectSystem.h"
#include "object/MCFireType.h"
#include "object/MCForces.h"
#include "object/MCMiscTerrainObject.h"
#include "object/MCMiscTerrainObjectType.h"
#include "object/MCObjectDrawing.h"
#include "object/MCObjectQueue.h"
#include "object/MCObjectSystem.h"
#include "object/MCVertexPlacement.h"
#include "object/MCWeaponShotInfo.h"
#include "sound/soundsys.h"
#include "sprite/MCVfxAppearance.h"
#include "terrain/MCTerrain.h"
#include "vfx/MCVfxFunctions.h"

namespace
{
    /// <summary>How long a flame burns when no mission has read gamesys.fit's MaxFireBurnTime (its first value).</summary>
    constexpr float DefaultMaxFireBurnTime = 5.0f;

    /// <summary>The most seconds a flame burns: the mission's MaxFireBurnTime.</summary>
    float MaxFireBurnTime()
    {
        const MCEffectSystem* effects = EffectSystem();
        return effects != nullptr ? effects->MaxFireBurnTime() : DefaultMaxFireBurnTime;
    }

    /// <summary>A random offset of up to <paramref name="range"/>, added or (on a coin flip) taken away.</summary>
    float Scatter(float base, int32_t range)
    {
        const auto offset = static_cast<float>(RandomNumber(range));

        if (!RollDice(50))
        {
            return base - offset;
        }

        return offset + base;
    }
} // namespace

MCFireFlame::MCFireFlame() = default;

MCFireFlame::~MCFireFlame() = default;

MCFireFlame::MCFireFlame(MCFireFlame&&) noexcept = default;

auto MCFireFlame::operator=(MCFireFlame&&) noexcept -> MCFireFlame& = default;

MCFire::MCFire() = default;

MCFire::~MCFire()
{
    if (PotentialContact != nullptr)
    {
        SetPotentialContact(0);
    }

    // Port fix: the original left a deleted fire in the ring, for the next fire to finish through a stale pointer.
    if (MCEffectSystem* effects = EffectSystem(); effects != nullptr)
    {
        effects->EndFire(this);
    }

    // Port fix: an object whose fire goes before it burns out (at a mission's end) forgets it.
    if (BurningObject != nullptr)
    {
        BurningObject->KillFireObject();
    }
}

auto MCFire::HandleStaticCollision() -> void
{
    if (CollisionsOn == 0)
    {
        return;
    }

    int32_t blockNumber = 0;
    int32_t vertexNumber = 0;
    GetBlockAndVertexNumber(blockNumber, vertexNumber);

    // The terrain objects of the terrain blocks around it.
    // Original behaviour (OB-151): the second row starts where the first did, so the row above is checked twice and
    // the row below never.
    const int32_t firstBlock = blockNumber - MCTerrain::BlocksMapSide - 1;

    for (int32_t row = 0; row < 3; row++)
    {
        const int32_t rowStart = row == 0 ? firstBlock : (row - 1) * MCTerrain::BlocksMapSide + firstBlock;

        for (int32_t col = 0; col < 3; col++)
        {
            CollisionSystem()->DetectBlockCollisions(this, rowStart + col);
        }
    }
}

auto MCFire::IsVisible(size_t flameIndex) -> bool
{
    bool onScreenNow = false;
    MCCamera* camera = ActiveMainCamera();

    if (camera != nullptr)
    {
        const MCFireFlame& flame = Flames[flameIndex];
        MCVector3D flamePosition;
        flamePosition.X = Position.X + flame.Offset.X;
        flamePosition.Y = Position.Y + flame.Offset.Y;
        flamePosition.Z = Position.Z + flame.Offset.Z;
        ScreenPos = ProjectToScreen(flamePosition, *camera);

        if (flame.Appearance != nullptr)
        {
            onScreenNow = flame.Appearance->RecalcBounds(camera) != 0;

            if (onScreenNow)
            {
                LastVisibleTurn = Turn;
            }
        }
    }

    // In multiplayer fires always count as visible.
    if (MPlayer == nullptr && !onScreenNow)
    {
        return false;
    }

    WindowsVisible = Turn;
    return true;
}

auto MCFire::FinishFireNow() -> void
{
    for (MCFireFlame& flame : Flames)
    {
        flame.LoopsLeft = 2;
    }

    BurningOut = true;

    for (MCFireFlame& flame : Flames)
    {
        flame.StartDelay = 0.0f;
        flame.TimeLeftToBurn = 0.0f;
    }
}

auto MCFire::AddTimeLeftToBurn(float extraTime) -> void
{
    for (MCFireFlame& flame : Flames)
    {
        if (extraTime + flame.TimeLeftToBurn < MaxFireBurnTime())
        {
            flame.TimeLeftToBurn = extraTime + flame.TimeLeftToBurn;
        }
    }
}

auto MCFire::IsRevealed() -> int
{
    int32_t tileR = 0;
    int32_t tileC = 0;
    int32_t cellR = 0;
    int32_t cellC = 0;
    GameMap()->WorldToMapPos(Position, tileR, tileC, cellR, cellC);
    // Faithful: tile coordinates are looked up in the vertex-resolution visibility bits.
    return MCVertexCell{tileR, tileC}.AnyCornerVisible() ? 1 : 0;
}

auto MCFire::Update() -> int32_t
{
    if (JustCreated)
    {
        // The object that lit the fire, or whatever made it, has put it on the object lists by now (MCFireLink::Burn).
        JustCreated = false;
        SetPotentialContact(3);
        SDL_assert(ObjectList()->FindIf([this](MCBaseObject* object) { return object == this; }) != nullptr);
    }

    if (BurningOut)
    {
        CollisionsOn = 0;
    }

    // Each flame waits out its start delay, then burns until its time runs out; a flame burnt out while still
    // looping sets the fire burning out, and collisions on for a frame at the full extent.
    const auto* fireType = static_cast<MCFireType*>(ObjType);

    for (MCFireFlame& flame : Flames)
    {
        if (flame.StartDelay <= 0.0f)
        {
            if (flame.LoopsLeft != 0)
            {
                const float timeLeft = flame.TimeLeftToBurn - FrameLength;
                flame.TimeLeftToBurn = timeLeft;

                if (0.0f < timeLeft || static_cast<uint32_t>(flame.LoopsLeft) < 3 ||
                    flame.Appearance->CurrentState == MCActorState::Damaged)
                {
                    if (0.0f < timeLeft)
                    {
                        flame.LoopsLeft = 999;
                    }
                }
                else
                {
                    CollisionsOn = 1;
                    ExtentRadius = fireType->MaxExtentRadius;
                    BurningOut = true;
                    flame.LoopsLeft = 2;
                }
            }
        }
        else
        {
            flame.StartDelay -= FrameLength;
        }
    }

    // Done once burning out and (if anyone can see it) every flame has finished.
    bool done = false;

    if (BurningOut)
    {
        done = true;

        if (IsRevealed() != 0 || MPlayer != nullptr)
        {
            done = std::ranges::none_of(Flames, [](const MCFireFlame& flame) { return flame.LoopsLeft != 0; });
        }
    }

    if (Light != nullptr)
    {
        MCVector3D lightPos = Position;
        Light->SetPosition(lightPos);
        Light->Update();
    }

    if (!done)
    {
        return 1;
    }

    // Put the burning object out; a burnt forest takes its damage.
    if (BurningObject != nullptr)
    {
        if (BurningObject->ObjectClass == MCObjectClass::MiscTerrainObject &&
            static_cast<MCMiscTerrainObject*>(BurningObject)->Kind == MCMiscTerrainKind::Forest)
        {
            const auto* forestType = static_cast<MCMiscTerrainObjectType*>(BurningObject->GetObjectType());
            MCWeaponShotInfo shot;
            shot.Init(nullptr, -3, static_cast<float>(static_cast<int32_t>(forestType->ForestDmgLevel)), 0, 0.0f);
            BurningObject->HandleWeaponHit(&shot, MPlayer != nullptr ? 1 : 0);
        }

        BurningObject->KillFireObject();
        BurningObject = nullptr;
    }

    if (MCEffectSystem* effects = EffectSystem(); effects != nullptr)
    {
        effects->EndFire(this);
    }

    return 0;
}

auto MCFire::Render() -> void
{
    int tagged = 0;
    const int32_t contactType = GetContactType(HomeTeam()->Id, tagged);
    const int revealed = BurningObject != nullptr ? BurningObject->IsRevealed() : IsRevealed();

    if (revealed != 0 || MPlayer != nullptr)
    {
        // Each burning flame plays its start (0), loop (1) and end (2) animations in turn.
        for (size_t i = 0; i < Flames.size(); i++)
        {
            MCFireFlame& flame = Flames[i];
            bool finished = false;

            if (flame.LoopsLeft == 0)
            {
                continue;
            }

            const bool visibleNow = IsVisible(i);
            MCVfxAppearance* flameAppearance = flame.Appearance.get();
            flameAppearance->Visible = visibleNow ? 1 : 0;

            if (0.0f < flame.StartDelay)
            {
                continue;
            }

            if (flameAppearance->Update() == 0)
            {
                if (flameAppearance->CurrentState == MCActorState::Damaged)
                {
                    finished = true;
                    flame.LoopsLeft = 0;
                }
                else
                {
                    if (flameAppearance->CurrentState == MCActorState::Normal)
                    {
                        flameAppearance->SetTypeId(MCActorState::BlowingUp1, 0xff);
                        flameAppearance->Update();
                    }

                    flame.LoopsLeft--;

                    if (flame.LoopsLeft == 1)
                    {
                        flameAppearance->SetTypeId(MCActorState::Damaged, 0xff);
                        flameAppearance->Update();
                    }
                }
            }

            if (!JustCreated && !finished && IsRevealed() != 0 && LastVisibleTurn == Turn)
            {
                SomethingOnFire = 1;
                flameAppearance->Render(-150);
            }
        }

        if (Light != nullptr)
        {
            Light->Render();
        }

        return;
    }

    // Unseen: a sensor contact shows a blip sized by tonnage.
    if (contactType != 2 || Flames.empty() || !IsVisible(0))
    {
        return;
    }

    uint8_t* shape = SensorBlipShape(GetTonnage());

    if (shape == nullptr)
    {
        return;
    }

    if (VfxShapeCount(shape) <= BlipFrame)
    {
        if (SoundSystem != nullptr && UseSound != 0)
        {
            SoundSystem->PlayDigitalSample(0x14, 1, this, 0, 1);
        }

        BlipFrame = 0;
    }

    ElementList()->OpenGroup(-100000, 1);
    ElementList()->Add(ElementList()->Make<MCVfxElement>(shape, ScreenPos.X, ScreenPos.Y, BlipFrame, 0, nullptr, 0));
    BlipTime = FrameLength + BlipTime;

    if (0.067 < BlipTime)
    {
        BlipFrame = static_cast<int32_t>(BlipTime * (1.0 / 0.067) + BlipFrame + 0.5);
        BlipTime = 0.0f;
    }
}

auto MCFire::Init(MCObjectType* objType) -> int32_t
{
    if (const int32_t result = MCGameObject::Init(objType); result != 0)
    {
        return result;
    }

    const auto* fireType = static_cast<MCFireType*>(objType);
    JustCreated = true;
    Flames.resize(fireType->Shapes.size());
    const uint32_t appearId = objType->AppearName;

    for (size_t i = 0; i < Flames.size(); i++)
    {
        MCFireFlame& flame = Flames[i];
        const MCFireShape& shape = fireType->Shapes[i];
        flame.LoopsLeft = static_cast<int32_t>(fireType->NumLoops);
        flame.TimeLeftToBurn = MaxFireBurnTime();
        MCAppearanceType* apprType = AppearanceTypeList()->GetAppearance(appearId);

        if (apprType == nullptr)
        {
            return static_cast<int32_t>(0xdcdc0003);
        }

        AppearanceClass = apprType->AppearanceNum >> 24;

        if (AppearanceClass != 2)
        {
            return static_cast<int32_t>(0xdcdc0005);
        }

        flame.Appearance = std::make_unique<MCVfxAppearance>();
        flame.Appearance->Init(nullptr, nullptr);

        if (const int32_t result = flame.Appearance->Init(apprType, this); result != 0)
        {
            return result;
        }

        flame.Appearance->SetTypeId(MCActorState::Normal, 0xff);

        // Place the flame at its offset, scattered, and delay its start (tenths of a second).
        flame.Offset.X = shape.OffsetX + flame.Offset.X;
        flame.Offset.Y = shape.OffsetY + flame.Offset.Y;
        flame.Offset.X = Scatter(flame.Offset.X, shape.RandomOffsetX);
        flame.Offset.Y = Scatter(flame.Offset.Y, shape.RandomOffsetY);
        const float delay = shape.Delay + flame.StartDelay;
        flame.StartDelay = delay;
        flame.StartDelay = static_cast<float>((static_cast<double>(RandomNumber(shape.RandomDelay)) + delay) * 0.1);
    }

    ObjectClass = MCObjectClass::Fire;
    CollisionsOn = 0;
    BurningOut = false;
    BlipFrame = 0;

    // Fires share a ring of slots; taking a slot finishes the fire that held it.
    if (MCEffectSystem* effects = EffectSystem(); effects != nullptr)
    {
        effects->StartFire(this);
    }

    if (static_cast<int32_t>(fireType->LightObjectId) != -1)
    {
        Light = CreateObject(static_cast<int32_t>(fireType->LightObjectId));
    }

    return 0;
}

MCFireLink::~MCFireLink()
{
    if (_Fire != nullptr)
    {
        _Fire->BurningObject = nullptr;
    }
}

auto MCFireLink::Light(std::unique_ptr<MCFire> fire) -> MCFire*
{
    _Owned = std::move(fire);
    _Fire = _Owned.get();
    return _Fire;
}

auto MCFireLink::Burn() -> void
{
    if (_Owned != nullptr)
    {
        AddToDefaultList(std::move(_Owned));
    }

    _Fire->Update();
}

auto MCFireLink::BurntOut() -> void
{
    SDL_assert(_Owned == nullptr);
    _Fire = nullptr;
}
