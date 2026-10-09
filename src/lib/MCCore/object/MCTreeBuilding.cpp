#include "stdafx.h"
#include "object/MCTreeBuilding.h"
#include "appear/MCAppearanceType.h"
#include "appear/MCAppearanceTypeList.h"
#include "camera/MCCamera.h"
#include "engine/MCElementBuffer.h"
#include "engine/MCVfxElement.h"
#include "lib/MCFatal.h"
#include "lib/MCDice.h"
#include "main/main.h"
#include "mission/scenario.h"
#include "network/multplyr.h"
#include "object/MCBuildingMarines.h"
#include "object/MCContactSystem.h"
#include "object/MCForces.h"
#include "object/MCMechWarrior.h"
#include "object/MCMover.h"
#include "object/MCObjectDrawing.h"
#include "object/MCObjectEvent.h"
#include "object/MCObjectSystem.h"
#include "object/MCSensorSystem.h"
#include "object/MCTeam.h"
#include "object/MCTreeBuildingType.h"
#include "object/MCVertexPlacement.h"
#include "object/MCWeaponShotInfo.h"
#include "sound/MCSoundSystem.h"
#include "sprite/MCVfxAppearance.h"
#include "terrain/MCTacticalMap.h"
#include "terrain/MCTerrain.h"
#include "vfx/MCVfxFunctions.h"

MCTreeBuilding::MCTreeBuilding()
{
    Frame.ResetToWorldFrame();
}

MCTreeBuilding::~MCTreeBuilding()
{
    if (SensorSystem != nullptr)
    {
        SensorSystemManager()->FreeSensor(SensorSystem);
    }
}

auto MCTreeBuilding::GetAppearance() -> MCAppearance*
{
    return Appearance.get();
}

auto MCTreeBuilding::SetTerrainPosition(MCVector2D& offset, MCVector2D& numbers) -> void
{
    PixelOffsetX = static_cast<int32_t>(offset.X);
    PixelOffsetY = static_cast<int32_t>(offset.Y);
    VertexNumber = static_cast<int32_t>(numbers.X);
    BlockNumber = static_cast<int32_t>(numbers.Y);
}

auto MCTreeBuilding::IsPrison() -> int
{
    return std::ranges::any_of(PrisonSlots, [](const MCMechWarrior* slot) { return slot != nullptr; }) ? 1 : 0;
}

auto MCTreeBuilding::GetRefitPoints() -> float
{
    if (!CanRefit)
    {
        return 0.0f;
    }

    return static_cast<float>(static_cast<int32_t>(static_cast<MCTreeBuildingType*>(ObjType)->DmgLevel)) - Damage;
}

auto MCTreeBuilding::BurnRefitPoints(float points) -> int
{
    if (!CanRefit)
    {
        return 0;
    }

    // Spent refit points count as damage; never more than are left.
    DamageObject(points < GetRefitPoints() ? points : GetRefitPoints());
    return 1;
}

auto MCTreeBuilding::IsVisible(MCCamera* cam) -> bool
{
    if (cam == nullptr || cam->Active == 0)
    {
        return false;
    }

    int visible = cam->VertexProject(BlockNumber, VertexNumber, ScreenPos);

    if (Appearance != nullptr)
    {
        visible = Appearance->RecalcBounds(cam);
    }

    // The shadow can stick out past the building: on screen when any of its box is.
    bool shadowOnScreen = false;

    if (uint8_t* shadow = static_cast<MCTreeBuildingType*>(ObjType)->NormalShadow.Data(); shadow != nullptr)
    {
        const float scale = cam->CameraScale != 1 ? 1.0f : 0.5f;
        const int32_t minXY = VfxShapeMinxy(shadow, 0);
        const float left = static_cast<float>(minXY >> 16) * scale + ScreenPos.X;
        const float top = static_cast<float>(static_cast<int16_t>(minXY)) * scale + ScreenPos.Y;
        const int32_t resolution = VfxShapeResolution(shadow, 0);

        if (0.0f <= static_cast<float>(resolution >> 16) * scale + left &&
            0.0f <= scale * static_cast<float>(static_cast<int16_t>(resolution)) + top)
        {
            const auto viewRight = static_cast<int16_t>(std::floor(static_cast<double>(cam->ViewWidth)));
            const auto viewBottom = static_cast<int16_t>(std::floor(static_cast<double>(cam->ViewHeight)));
            shadowOnScreen = left <= static_cast<float>(viewRight) && top <= static_cast<float>(viewBottom);
        }
    }

    if (!shadowOnScreen && visible == 0)
    {
        return false;
    }

    // Back on screen after a gap: the looping sound is started afresh.
    if (WindowsVisible < Turn - 2)
    {
        SoundHandle = 0xffffffff;
    }

    WindowsVisible = Turn;
    return true;
}

auto MCTreeBuilding::IsCaptureable() -> int
{
    if (MPlayer == nullptr)
    {
        return Captureable && IsCaptured() == 0 && IsDestroyed() == 0 ? 1 : 0;
    }

    return Captureable && IsDestroyed() == 0 ? 1 : 0;
}

auto MCTreeBuilding::Update() -> int32_t
{
    if (!JustCreated)
    {
        return 1;
    }

    // Set the building on its vertex.
    JustCreated = false;
    Position = PlaceOnVertex(Position, BlockNumber, VertexNumber, PixelOffsetX, PixelOffsetY);
    const MCVertexCell cell = MCVertexCell::Of(BlockNumber, VertexNumber);
    CellColumn = cell.Col;
    VertexWorldX = cell.WorldX();
    CellRow = cell.Row;
    VertexWorldY = cell.WorldY();
    CellElevation = cell.Elevation(" tbldg MapTile Out of Bounds ");
    Appearance->Visible = 1;
    Appearance->Update();
    Appearance->RecalcBounds(Eye);

    if (CanRefit)
    {
        Appearance->SetTypeId(MCActorState::Normal, 0);
    }

    return 1;
}

auto MCTreeBuilding::SetAlignment(int32_t align) -> void
{
    MCBigGameObject::SetAlignment(align);

    if (IsDestroyed() != 0 || SensorSystem == nullptr)
    {
        return;
    }

    if (Alignment == -1)
    {
        SensorSystem->SetTeam(ClanTeam());
    }
    else if (Alignment == 1)
    {
        SensorSystem->SetTeam(InnerSphereTeam());
    }
    else if (Alignment == 0)
    {
        SensorSystem->SetTeam(AlliedTeam());
    }
}

auto MCTreeBuilding::HandleEvent(MCObjectEvent* event) -> int32_t
{
    if (event->Type == 0)
    {
        if (event->Id == 0x1c)
        {
            Selected = 1;
        }
        else if (event->Id == 0x1d)
        {
            Selected = 0;
        }
    }

    return 0;
}

auto MCTreeBuilding::LightOnFire(float timeToBurn) -> void
{
    const auto* type = static_cast<MCTreeBuildingType*>(ObjType);

    if (type->BlownEffectId == 0xffffffff)
    {
        // Nothing to burn: a point of damage instead.
        MCWeaponShotInfo shot;
        shot.Init(nullptr, -1, 1.0f, 0, 0.0f);

        if (MPlayer == nullptr)
        {
            HandleWeaponHit(&shot, 0);
        }
        else if (MPlayer->IsServer != 0)
        {
            HandleWeaponHit(&shot, 1);
        }

        return;
    }

    if (FireObject == nullptr)
    {
        std::unique_ptr<MCGameObject> newFire = CreateObject(static_cast<int32_t>(type->BlownEffectId));

        if (newFire != nullptr)
        {
            newFire->SetPosition(Position);

            if (newFire->ObjectClass == MCObjectClass::Fire)
            {
                // Original behaviour (OB-154): nothing updates or draws this fire.
                MCFire* fire = FireObject.Light(std::unique_ptr<MCFire>(static_cast<MCFire*>(newFire.release())));
                fire->SetPotentialContact(3);
                fire->BurningObject = this;
                fire->SetTonnage(40.0f);
            }
            else
            {
                DestroyObject(newFire.get());
            }
        }
    }

    if (FireObject != nullptr)
    {
        FireObject->AddTimeLeftToBurn(timeToBurn);
        Burning = true;
    }
}

auto MCTreeBuilding::IsRevealed() -> int
{
    return MCVertexCell::Of(BlockNumber, VertexNumber).AnyCornerVisible() ? 1 : 0;
}

auto MCTreeBuilding::Render() -> void
{
    if (JustCreated)
    {
        return;
    }

    const auto* type = static_cast<MCTreeBuildingType*>(ObjType);

    // Burning: the type's burn damage every TimeToBurnDamage seconds.
    if (FireObject == nullptr)
    {
        Burning = false;
    }
    else
    {
        const double burnSum = static_cast<double>(FrameLength) + BurnTime;
        BurnTime = static_cast<float>(burnSum);

        if (type->TimeToBurnDamage < burnSum)
        {
            BurnTime = 0.0f;
            MCWeaponShotInfo shot;
            shot.Init(nullptr, -1, type->BurnDamagePerTime, 0, 0.0f);

            if (MPlayer == nullptr)
            {
                HandleWeaponHit(&shot, 0);
            }
            else if (MPlayer->IsServer != 0)
            {
                HandleWeaponHit(&shot, 1);
            }
        }
    }

    if (Appearance != nullptr)
    {
        Appearance->Visible = IsVisible(Eye) ? 1 : 0;

        // When the collapse animation ends, settle on the matching rubble state.
        if (Appearance->Update() == 0 && Collapsing)
        {
            const MCActorState state = Appearance->CurrentState;
            Collapsing = false;
            Collapsed = true;

            if (state == MCActorState::BlowingUp1)
            {
                Appearance->SetTypeId(MCActorState::Damaged, 0xff);
            }
            else if (state == MCActorState::Destroyed)
            {
                Appearance->SetTypeId(MCActorState::FallenDamaged, 0xff);
            }

            Appearance->Update();
        }
    }

    if (GetContactType(HomeTeam()->Id) == 2)
    {
        // A sensor contact: a blip sized by tonnage.
        if (uint8_t* shape = SensorBlipShape(GetTonnage()); shape != nullptr)
        {
            if (VfxShapeCount(shape) < BlipFrame)
            {
                if (SoundSystem() != nullptr && UseSound != 0)
                {
                    SoundSystem()->PlayDigitalSample(0x14, 1, this, 0, 1);
                }

                BlipFrame = 0;
            }

            ElementList()->OpenGroup(-100000, 1);
            ElementList()->Add(
                ElementList()->Make<MCVfxElement>(shape, ScreenPos.X, ScreenPos.Y, BlipFrame, 0, nullptr, 0));
            BlipTime = FrameLength + BlipTime;

            if (0.067 < BlipTime)
            {
                BlipFrame = static_cast<int32_t>(BlipTime * (1.0 / 0.067) + BlipFrame + 0.5);
                BlipTime = 0.0f;
            }
        }
    }

    if (WindowsVisible != Turn)
    {
        if (SoundHandle != 0xffffffff)
        {
            SoundSystem()->StopDigitalSample(SoundHandle);
            SoundHandle = 0xffffffff;
        }

        return;
    }

    // Hazed by how many corners of its vertex square the home team sees; drawn (with its shadow) when any is. (The
    // original also reads each corner's seen bit and drops it.)
    const int32_t numVisible = MCVertexCell::Of(BlockNumber, VertexNumber).VisibleCorners();
    uint8_t* hazePalette = nullptr;

    if (numVisible != 0 && numVisible != 4 && Eye->HazeLevel != 0x7fff)
    {
        hazePalette = HazePaletteFor(numVisible);
    }

    Appearance->FadeTable = hazePalette;

    if (numVisible == 0)
    {
        if (SoundHandle != 0xffffffff)
        {
            SoundSystem()->StopDigitalSample(SoundHandle);
            SoundHandle = 0xffffffff;
        }
    }
    else
    {
        // Standing, it sorts with the terrain; fallen or burning down, by its screen row.
        const bool standing = Appearance->CurrentState == MCActorState::Normal;
        Appearance->Render(standing ? 0 : static_cast<int32_t>(ScreenPos.Y));
        uint8_t* shadow = standing ? type->NormalShadow.Data() : type->DestroyedShadow.Data();

        if (shadow != nullptr)
        {
            ElementList()->OpenGroup(static_cast<int32_t>(ScreenPos.Y), 1);
            ElementList()->Add(
                ElementList()->Make<MCVfxElement>(shadow, ScreenPos.X, ScreenPos.Y, 0, 0, hazePalette, 0));
        }

        if (SoundHandle == 0xffffffff && type->NormalEffectId != 0xffffffff)
        {
            SoundHandle = static_cast<uint32_t>(SoundSystem()->PlayDigitalSample(type->NormalEffectId, 0, this, 1, 0));
        }
    }

    if (DrawExtents)
    {
        // Debug: the extent radius as an ellipse.
        DrawExtentEllipse(Position, MCVector2D(ObjType->ExtentRadius, ObjType->ExtentRadius));
    }
}

auto MCTreeBuilding::SetSensorData(MCTeam* newTeam, float range, bool setTeam) -> void
{
    if (!(-1.0 < range))
    {
        return;
    }

    if (SensorSystem == nullptr)
    {
        SensorSystem = SensorSystemManager()->NewSensor();

        if (SensorSystem == nullptr)
        {
            Fatal(0, " No RAM for Sensor System ");
        }
    }

    SensorSystem->Owner = this;

    if (setTeam)
    {
        SensorSystem->SetTeam(newTeam);
    }

    SensorSystem->SetRange(range);
}

auto MCTreeBuilding::Init(MCObjectType* objType) -> int32_t
{
    if (const int32_t result = MCGameObject::Init(objType); result != 0)
    {
        return result;
    }

    SetExists(1);
    JustCreated = true;
    MCAppearanceType* apprType = AppearanceTypeList()->GetAppearance(objType->AppearName);

    if (apprType == nullptr)
    {
        return static_cast<int32_t>(0xdcdc0007);
    }

    Appearance = std::make_unique<MCVfxAppearance>();
    Appearance->Init(nullptr, nullptr);

    if ((apprType->AppearanceNum & 0xff000000) != 0x2000000)
    {
        return static_cast<int32_t>(0xdcdc0009);
    }

    if (const int32_t result = Appearance->Init(apprType, this); result != 0)
    {
        return result;
    }

    const auto* type = static_cast<MCTreeBuildingType*>(ObjType);
    ObjectClass = MCObjectClass::TreeBuilding;
    HitOnce = false;
    SoundHandle = 0xffffffff;

    if (0.0 < type->ExtentRadius)
    {
        CollisionsOn = 1;
    }

    Tonnage = type->BaseTonnage;
    ExplRadius = type->ExplRad;
    ExplDamage = type->ExplDmg;
    MaxCV = type->BattleRating;
    CurCV = type->BattleRating;
    CanRefit = type->CanRefit;
    MechBay = type->MechBay;
    Name = LoadGameString(static_cast<uint32_t>(type->BuildingName), 0xfe);

    // Original behaviour (OB-016): with no team (TeamID -1) this reads TeamTable[-1], which in MCX.EXE is homeTeam.
    TypeTeam = type->TeamId == -1 ? HomeTeam() : TeamById(type->TeamId);
    const float range = type->SensorRange;

    if (-1.0 < range)
    {
        switch (type->TeamId)
        {
            case 0:
            {
                SetSensorData(InnerSphereTeam(), range, false);
                SetAlignment(1);
                break;
            }

            case 1:
            {
                SetSensorData(ClanTeam(), range, false);
                SetAlignment(-1);
                break;
            }

            case 2:
            {
                SetSensorData(AlliedTeam(), range, false);
                SetAlignment(0);
                break;
            }

            default:
                break;
        }
    }

    Captureable = false;
    RefitBuddy = nullptr;

    // Damage level 0: already rubble.
    if (type->DmgLevel == 0)
    {
        CollisionsOn = 0;
        Status = 2;
        HitOnce = true;
    }

    return 0;
}

auto MCTreeBuilding::CreateBuildingMarines() -> void
{
    // A random direction, set 1.5 extent radii out on the ground (z stays the unscaled unit component).
    LetOutBuildingMarines(
        *this, static_cast<MCTreeBuildingType*>(ObjType)->NumMarines,
        [this](MCMover& marine)
        {
            const float extent = ObjType->ExtentRadius;
            MCVector3D offset;
            offset.X = static_cast<float>(RandomNumber(static_cast<int32_t>(extent + extent))) - extent;
            offset.Y = static_cast<float>(RandomNumber(static_cast<int32_t>(extent + extent))) - extent;
            offset.Z = static_cast<float>(RandomNumber(0)) - 0.0f;
            const double length =
                std::sqrt(static_cast<double>(offset.Z) * offset.Z + static_cast<double>(offset.Y) * offset.Y +
                          static_cast<double>(offset.X) * offset.X);

            if (length != 0.0)
            {
                offset.X = static_cast<float>(offset.X / length);
                offset.Y = static_cast<float>(offset.Y / length);
                offset.Z = static_cast<float>(offset.Z / length);
            }

            offset.X = static_cast<float>(static_cast<double>(extent) * offset.X * 1.5);
            offset.Y = static_cast<float>(static_cast<double>(extent) * offset.Y * 1.5);
            MCVector3D marinePosition;
            marinePosition.X = offset.X + Position.X;
            marinePosition.Y = offset.Y + Position.Y;
            marinePosition.Z = offset.Z + Position.Z;
            marine.SetPosition(marinePosition);
            marine.SetLastValidPosition(Position + offset);
            GameObjectMap()->AddObject(&marine);
        });
}

auto MCTreeBuilding::HandleWeaponHit(MCWeaponShotInfo* shotInfo, int addMultiplayChunk) -> int32_t
{
    if (shotInfo == nullptr)
    {
        return 0;
    }

    if (addMultiplayChunk != 0)
    {
        MPlayer->AddWeaponHitChunk(this, shotInfo, 0);
    }

    if (IsDestroyed() != 0)
    {
        return 0;
    }

    const float newDamage = GetDamage() + shotInfo->Damage;
    SetDamage(newDamage);
    HitOnce = true;
    auto* type = static_cast<MCTreeBuildingType*>(ObjType);

    if (newDamage < static_cast<float>(static_cast<int32_t>(type->DmgLevel)) || Collapsed || Collapsing)
    {
        return 0;
    }

    // Collapses: the shooter's pilot is alarmed, the marines come out, and the sensor, fire and explosion follow.
    Collapsing = true;
    CollisionsOn = 0;
    Status = 2;

    if (IsCaptured() != 0)
    {
        TacticalMap()->RemoveSalvage(this, 1);
    }

    if (MCGameObject* attacker = shotInfo->Attacker; attacker != nullptr && IsMoverClass(attacker->ObjectClass))
    {
        attacker->GetPilot()->TriggerAlarm(MCPilotAlarmType::KilledTarget, static_cast<uint32_t>(PartId));
    }

    if (MPlayer == nullptr)
    {
        CreateBuildingMarines();
    }

    if (type->DamageEffectId != 0xffffffff && 5 < Turn)
    {
        SoundSystem()->PlayDigitalSample(type->DamageEffectId, 1, this, 1, 0);
    }

    if (SensorSystem != nullptr)
    {
        SensorSystem->Disable();
    }

    // Original behaviour: hitOnce was set just above, so the collapse always plays the destroyed state, never state 1.
    Appearance->SetTypeId(HitOnce ? MCActorState::Destroyed : MCActorState::BlowingUp1, 0xff);

    if (!Burning)
    {
        if (type->BlownEffectId != 0xffffffff)
        {
            std::unique_ptr<MCGameObject> newFire = CreateObject(static_cast<int32_t>(type->BlownEffectId));

            if (newFire != nullptr)
            {
                newFire->SetPosition(Position);

                if (newFire->ObjectClass == MCObjectClass::Fire)
                {
                    MCFire* fire = FireObject.Light(std::unique_ptr<MCFire>(static_cast<MCFire*>(newFire.release())));
                    fire->SetPotentialContact(3);
                    fire->BurningObject = this;
                    fire->SetTonnage(40.0f);
                    FireObject.Burn();
                    Burning = true;
                }
                else
                {
                    AddToDefaultList(std::move(newFire));
                }
            }
        }
    }
    else if (FireObject != nullptr)
    {
        FireObject->AddTimeLeftToBurn(2.0f);
    }

    type->CreateExplosion(Position, ExplDamage, ExplRadius);
    return 0;
}
