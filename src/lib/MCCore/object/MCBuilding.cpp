#include "stdafx.h"
#include "object/MCBuilding.h"
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
#include "object/MCBuildingType.h"
#include "object/MCCollisionSystem.h"
#include "object/MCContactSystem.h"
#include "object/MCForces.h"
#include "object/MCMover.h"
#include "object/MCObjectDrawing.h"
#include "object/MCObjectEvent.h"
#include "object/MCObjectSystem.h"
#include "object/MCSensorSystem.h"
#include "object/MCTeam.h"
#include "object/MCVertexPlacement.h"
#include "object/MCWeaponShotInfo.h"
#include "sound/soundsys.h"
#include "sprite/MCVfxBuildingAppearance.h"
#include "terrain/MCTacticalMap.h"
#include "terrain/MCTerrain.h"
#include "vfx/MCVfxFunctions.h"

MCBuilding::MCBuilding() = default;

MCBuilding::~MCBuilding()
{
    if (SensorSystem != nullptr)
    {
        SensorSystemManager()->FreeSensor(SensorSystem);
    }
}

auto MCBuilding::GetAppearance() -> MCAppearance*
{
    return Appearance.get();
}

auto MCBuilding::IsPrison() -> int
{
    return std::ranges::any_of(PrisonSlots, [](const MCMechWarrior* slot) { return slot != nullptr; }) ? 1 : 0;
}

auto MCBuilding::SetTerrainPosition(MCVector2D& offset, MCVector2D& numbers) -> void
{
    PixelOffsetX = static_cast<int32_t>(offset.X);
    PixelOffsetY = static_cast<int32_t>(offset.Y);

    if (TileNum != 10)
    {
        const auto* type = static_cast<MCBuildingType*>(ObjType);

        if (type->BasePixelOffsetX != 0)
        {
            PixelOffsetX = type->BasePixelOffsetX;
        }

        if (type->BasePixelOffsetY != 0)
        {
            PixelOffsetY = type->BasePixelOffsetY;
        }
    }

    VertexNumber = static_cast<int32_t>(numbers.X);
    BlockNumber = static_cast<int32_t>(numbers.Y);
}

auto MCBuilding::IsVisible(MCCamera* cam) -> bool
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

    if (visible == 0)
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

auto MCBuilding::Update() -> int32_t
{
    if (!JustCreated)
    {
        return 1;
    }

    // Set the building on its vertex. The type's collision offsets, when set, replace the pixel offsets.
    auto* type = static_cast<MCBuildingType*>(ObjType);
    JustCreated = false;
    const int32_t offsetX = type->CollisionOffsetX != 0 ? type->CollisionOffsetX : PixelOffsetX;
    const int32_t offsetY = type->CollisionOffsetY != 0 ? type->CollisionOffsetY : PixelOffsetY;
    Position = PlaceOnVertex(Position, BlockNumber, VertexNumber, offsetX, offsetY);
    const MCVertexCell cell = MCVertexCell::Of(BlockNumber, VertexNumber);
    CellColumn = cell.Col;
    VertexWorldX = cell.WorldX();
    CellRow = cell.Row;
    VertexWorldY = cell.WorldY();
    CellElevation = cell.Elevation(" bldng MapTile Out of Bounds ");

    // No extent radius in the FIT: measure it from the appearance's bounds.
    if (type->ExtentRadius < 0.0)
    {
        Appearance->Visible = 1;
        Appearance->Update();
        Appearance->CalcCollideBounds();
        const float dx = Appearance->UpperLeft.X - Appearance->LowerRight.X;
        const float dy = Appearance->UpperLeft.Y - Appearance->LowerRight.Y;
        const float radius = std::sqrt(dx * dx + dy * dy) / WorldUnitsPerMeter * 1.25f;

        if (static_cast<float>(CollisionSystem()->GridRadius()) < radius)
        {
            Fatal(static_cast<int32_t>(std::floor(static_cast<double>(radius))), " Object extent radius TOO large ");
        }

        type->ExtentRadius = radius;
    }

    if (type->ExtentRadius != 0.0)
    {
        CollisionsOn = 1;
    }

    return 1;
}

auto MCBuilding::SetAlignment(int32_t align) -> void
{
    MCBigGameObject::SetAlignment(align);

    if (SensorSystem == nullptr)
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

auto MCBuilding::HandleEvent(MCObjectEvent* event) -> int32_t
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

auto MCBuilding::LightOnFire(float timeToBurn) -> void
{
    const auto* type = static_cast<MCBuildingType*>(ObjType);

    if (type->BlownEffectId == 0xffffffff)
    {
        return;
    }

    if (FireObject == nullptr)
    {
        std::unique_ptr<MCGameObject> effect = CreateObject(static_cast<int32_t>(type->BlownEffectId));

        if (effect != nullptr)
        {
            effect->SetPosition(Position);

            if (effect->ObjectClass == MCObjectClass::Fire)
            {
                // Original behaviour (OB-154): nothing updates or draws this fire, so it burns (and the building
                // takes its burn damage) until the building is destroyed.
                MCFire* fire = FireObject.Light(std::unique_ptr<MCFire>(static_cast<MCFire*>(effect.release())));
                fire->SetPotentialContact(3);
                fire->BurningObject = this;
                fire->SetTonnage(40.0f);
            }
            else
            {
                DestroyObject(effect.get());
            }
        }
    }

    if (FireObject != nullptr)
    {
        FireObject->AddTimeLeftToBurn(timeToBurn);
        Burning = true;
    }
}

auto MCBuilding::IsCaptureable() -> int
{
    if (MPlayer == nullptr)
    {
        return Captureable && IsCaptured() == 0 && IsDestroyed() == 0 ? 1 : 0;
    }

    return Captureable && IsDestroyed() == 0 ? 1 : 0;
}

auto MCBuilding::IsRevealed() -> int
{
    return MCVertexCell::Of(BlockNumber, VertexNumber).AnyCornerVisible() ? 1 : 0;
}

auto MCBuilding::Render() -> void
{
    if (JustCreated)
    {
        return;
    }

    const bool visible = IsVisible(Eye);

    if (Appearance != nullptr)
    {
        Appearance->Visible = visible ? 1 : 0;
        Appearance->TileNum = TileNum;
        Appearance->Update();
    }

    // Burning: the type's burn damage every TimeToBurnDamage seconds.
    if (FireObject == nullptr)
    {
        Burning = false;
    }
    else
    {
        const double burnSum = static_cast<double>(FrameLength) + BurnTime;
        BurnTime = static_cast<float>(burnSum);
        const auto* type = static_cast<MCBuildingType*>(ObjType);

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

    if (GetContactType(HomeTeam()->Id) == 2)
    {
        // A sensor contact: a blip sized by tonnage.
        if (uint8_t* shape = SensorBlipShape(GetTonnage()); shape != nullptr)
        {
            if (VfxShapeCount(shape) < BlipFrame)
            {
                if (SoundSystem != nullptr && UseSound != 0)
                {
                    SoundSystem->PlayDigitalSample(0x14, 1, this, 0, 1);
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
            SoundSystem->StopDigitalSample(SoundHandle);
            SoundHandle = 0xffffffff;
        }

        return;
    }

    // Hazed by how many corners of its vertex square the home team sees; drawn when any is. (The original also reads
    // each corner's seen bit and drops it.)
    const int32_t numVisible = MCVertexCell{CellRow, CellColumn}.VisibleCorners();
    uint8_t* hazePalette = nullptr;

    if (numVisible != 4 && Eye->HazeLevel != 0x7fff)
    {
        hazePalette = HazePaletteFor(numVisible);
    }

    Appearance->FadeTable = hazePalette;

    if (numVisible == 0 || JustCreated)
    {
        if (SoundHandle != 0xffffffff)
        {
            SoundSystem->StopDigitalSample(SoundHandle);
            SoundHandle = 0xffffffff;
        }
    }
    else
    {
        Appearance->Render(0);
        const auto* type = static_cast<MCBuildingType*>(ObjType);

        if (SoundHandle == 0xffffffff && type->NormalEffectId != 0xffffffff)
        {
            SoundHandle = static_cast<uint32_t>(SoundSystem->PlayDigitalSample(type->NormalEffectId, 0, this, 1, 0));
        }
    }

    if (DrawExtents)
    {
        // Debug: the extent radius as an ellipse.
        const float diameter = ObjType->ExtentRadius + ObjType->ExtentRadius;
        DrawExtentEllipse(Position, MCVector2D(Eye->CosAngle * diameter, Eye->SinAngle * diameter));
    }
}

auto MCBuilding::SetDamage(float newDamage) -> void
{
    Damage = newDamage;
    // Sixteen damage frames across the damage level.
    const auto* type = static_cast<MCBuildingType*>(ObjType);
    int32_t frame =
        static_cast<int32_t>(newDamage / (static_cast<double>(static_cast<int32_t>(type->DmgLevel)) * 0.0625));

    if (frame > 15)
    {
        frame = 15;
    }

    Appearance->SetDamageLvl(static_cast<uint32_t>(frame));
}

auto MCBuilding::SetSensorData(MCTeam* newTeam, float range, bool setTeam) -> void
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

auto MCBuilding::Init(MCObjectType* objType) -> int32_t
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

    Appearance = std::make_unique<MCVfxBuildingAppearance>();
    Appearance->Init(nullptr, nullptr);

    if ((apprType->AppearanceNum & 0xff000000) != 0x7000000)
    {
        return static_cast<int32_t>(0xdcdc0009);
    }

    if (const int32_t result = Appearance->Init(apprType, this); result != 0)
    {
        return result;
    }

    const auto* type = static_cast<MCBuildingType*>(ObjType);
    ObjectClass = MCObjectClass::Building;
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
    Name = LoadGameString(static_cast<uint32_t>(type->BuildingName), 0xfe);

    // Original behaviour (OB-016): a building with no team (TeamID -1) reads TeamTable[-1], the global before it in
    // MCX.EXE: homeTeam.
    Team = type->TeamId == -1 ? HomeTeam() : TeamById(type->TeamId);
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

    return 0;
}

auto MCBuilding::CreateBuildingMarines() -> void
{
    // Somewhere within the extent radius of the building.
    LetOutBuildingMarines(*this, static_cast<MCBuildingType*>(ObjType)->NumMarines,
                          [this](MCMover& marine)
                          {
                              const float extentX = ObjType->ExtentRadius;
                              const float extentY = ObjType->ExtentRadius;
                              const float offsetX =
                                  static_cast<float>(RandomNumber(static_cast<int32_t>(extentX + extentX))) - extentX;
                              const float offsetY =
                                  static_cast<float>(RandomNumber(static_cast<int32_t>(extentY + extentY))) - extentY;
                              const float offsetZ = static_cast<float>(RandomNumber(0));
                              MCVector3D marinePosition;
                              marinePosition.X = offsetX + Position.X;
                              marinePosition.Y = offsetY + Position.Y;
                              marinePosition.Z = offsetZ + Position.Z;
                              marine.SetPosition(marinePosition);
                              GameObjectMap()->AddObject(&marine);
                              marine.BounceToAdjCell();
                              marine.BounceToAdjCell();
                          });
}

auto MCBuilding::HandleWeaponHit(MCWeaponShotInfo* shotInfo, int addMultiplayChunk) -> int32_t
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

    float newDamage = GetDamage() + shotInfo->Damage;
    auto* type = static_cast<MCBuildingType*>(ObjType);
    const auto destroyLevel = static_cast<float>(static_cast<int32_t>(type->DmgLevel));

    if (destroyLevel <= newDamage)
    {
        // Destroyed: sensor off, a fire (or more burn time), the explosion, salvage gone and the marines out.
        if (SensorSystem != nullptr)
        {
            SensorSystem->Disable();
        }

        if (!Burning)
        {
            if (type->BlownEffectId != 0xffffffff)
            {
                std::unique_ptr<MCGameObject> effect = CreateObject(static_cast<int32_t>(type->BlownEffectId));

                if (effect != nullptr)
                {
                    effect->SetPosition(Position);

                    if (effect->ObjectClass == MCObjectClass::Fire)
                    {
                        MCFire* fire =
                            FireObject.Light(std::unique_ptr<MCFire>(static_cast<MCFire*>(effect.release())));
                        fire->SetPotentialContact(3);
                        fire->BurningObject = this;
                        fire->SetTonnage(40.0f);
                        FireObject.Burn();
                        Burning = true;
                    }
                    else
                    {
                        AddToDefaultList(std::move(effect));
                    }
                }
            }
        }
        else if (FireObject != nullptr)
        {
            FireObject->AddTimeLeftToBurn(2.0f);
        }

        type->CreateExplosion(Position, ExplDamage, ExplRadius);
        Status = 2;

        if (IsCaptured() != 0)
        {
            TacticalMap()->RemoveSalvage(this, 1);
        }

        newDamage = destroyLevel;

        if (MPlayer == nullptr)
        {
            CreateBuildingMarines();
        }
    }

    SetDamage(newDamage);
    return 0;
}
