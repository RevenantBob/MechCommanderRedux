#include "stdafx.h"
#include "object/bldng.h"
#include "appear/MCAppearanceType.h"
#include "appear/MCAppearanceTypeList.h"
#include "camera/camera.h"
#include "color/MCPalette.h"
#include "engine/MCByteFlag.h"
#include "engine/MCElementBuffer.h"
#include "engine/MCEllipseElement.h"
#include "engine/MCVfxElement.h"
#include "lib/MCFatal.h"
#include "lib/MCIDString.h"
#include "lib/MCFrameOfRef.h"
#include "lib/MCVector2D.h"
#include "lib/MCDice.h"
#include "lib/MCFitIniFile.h"
#include "logistics/logmain.h"
#include "main/main.h"
#include "mission/scenario.h"
#include "network/multplyr.h"
#include "object/collsn.h"
#include "object/contact.h"
#include "object/fire.h"
#include "object/mover.h"
#include "object/object.h"
#include "object/objevnt.h"
#include "object/objque.h"
#include "object/team.h"
#include "object/warrior.h"
#include "sound/soundsys.h"
#include "sprite/MCVfxBuildingAppearance.h"
#include "sprite/MCElementalActor.h"
#include "terrain/terrain.h"
#include "terrain/terrmap.h"
#include "vfx/MCVfxFunctions.h"

int32_t DefaultPilotId = 0x28d;
char MarineProfileName[80] = "PEM00001";
int DrawExtents = 0;
int32_t NumMarines = 0;

namespace
{
    /// <summary>Degrees to radians, as MCX.EXE stores it (a hair under pi / 180).</summary>
    constexpr double DEGREES_TO_RADIANS = 0x1.1df46a2526c7ap-6;
    /// <summary>Radians to degrees, as MCX.EXE stores it.</summary>
    constexpr double RADIANS_TO_DEGREES = 0x1.ca5dc1a6402aap+5;
    /// <summary>Sixty degrees in radians, as MCX.EXE stores it.</summary>
    constexpr double SIXTY_DEGREES = 0x1.0c152382d45b2p+0;

    /// <summary>Makes the type's blown effect (a fire) at the building; anything else it makes is returned as is.</summary>
    MCGameObject* CreateBlownEffect(MCBuilding* building, int32_t effectId)
    {
        MCGameObject* effect = CreateObject(effectId);

        if (effect == nullptr)
        {
            return nullptr;
        }

        effect->SetPosition(building->Position);

        if (effect->ObjectClass == FIRE)
        {
            building->FireObject = static_cast<MCFire*>(effect);
            building->FireObject->SetPotentialContact(3);
            building->FireObject->BurningObject = building;
            building->FireObject->SetTonnage(40.0f);
        }

        return effect;
    }
}

//---------------------------------------------------------------------------
// BuildingType
//---------------------------------------------------------------------------

auto MCBuildingType::Init() -> void
{
    TypeClass = -1;
    DestroyedObject = -1;
    ExplosionObject = -1;
    AppearName = 0;
    ExtentRadius = 0.0f;
    KeepMe = 0;
    IconNumber = -1;
    DmgLevel = 0;
    BlownEffectId = 0xffffffff;
    NormalEffectId = 0xffffffff;
    DamageEffectId = 0xffffffff;
    SensorRange = -1.0f;
    TeamId = -1;
    ExplRad = 0.0f;
    ExplDmg = 0.0f;
    BaseTonnage = 0.0f;
    TimeToBurnDamage = 0.0f;
    BurnDamagePerTime = 0.0f;
    DamageLvlForBurn = 0.0f;
    BuildingName = 0;
    BattleRating = 0;
    NumMarines = 0;
}

auto MCBuildingType::CreateInstance() -> MCBaseObject*
{
    auto* newBuilding = new MCBuilding;

    if (newBuilding == nullptr)
    {
        return nullptr;
    }

    if (newBuilding->Init(this) != 0)
    {
        return nullptr;
    }

    newBuilding->IdNumber = NextIdNumber++;
    return newBuilding;
}

auto MCBuildingType::Destroy() -> void
{
}

auto MCBuildingType::Init(MCFile* objFile, uint32_t fileSize) -> int32_t
{
    MCFitIniFile bldgFile;
    int32_t result = bldgFile.Open(objFile, fileSize);

    if (result != 0)
    {
        return result;
    }

    if ((result = bldgFile.SeekBlock("BuildingData")) != 0)
    {
        return result;
    }

    if ((result = bldgFile.ReadIdULong("DmgLevel", DmgLevel)) != 0)
    {
        return result;
    }

    if ((result = bldgFile.ReadIdULong("BlownEffectId", BlownEffectId)) != 0)
    {
        return result;
    }

    if (bldgFile.ReadIdULong("NormalEffectId", NormalEffectId) != 0)
    {
        NormalEffectId = 0xffffffff;
    }

    if ((result = bldgFile.ReadIdULong("DamageEffectId", DamageEffectId)) != 0)
    {
        return result;
    }

    if (bldgFile.ReadIdLong("BasePixelOffsetX", BasePixelOffsetX) != 0)
    {
        BasePixelOffsetX = 0;
    }

    if (bldgFile.ReadIdLong("BasePixelOffsetY", BasePixelOffsetY) != 0)
    {
        BasePixelOffsetY = 0;
    }

    if (bldgFile.ReadIdLong("CollisionOffsetX", CollisionOffsetX) != 0)
    {
        CollisionOffsetX = 0;
    }

    if (bldgFile.ReadIdLong("CollisionOffsetY", CollisionOffsetY) != 0)
    {
        CollisionOffsetY = 0;
    }

    // -1 has update measure the extent from the appearance.
    float fitExtentRadius = 0.0f;

    if (bldgFile.ReadIdFloat("ExtentRadius", fitExtentRadius) != 0)
    {
        fitExtentRadius = -1.0f;
    }

    if (bldgFile.ReadIdFloat("Tonnage", BaseTonnage) != 0)
    {
        BaseTonnage = 20.0f;
    }

    if (bldgFile.ReadIdLong("BattleRating", BattleRating) != 0)
    {
        BattleRating = 20;
    }

    if (bldgFile.ReadIdLong("NumMarines", NumMarines) != 0)
    {
        NumMarines = 0;
    }

    if (bldgFile.ReadIdFloat("ExplosionRadius", ExplRad) != 0)
    {
        ExplRad = 0.0f;
    }

    if (bldgFile.ReadIdFloat("ExplosionDamage", ExplDmg) != 0)
    {
        ExplDmg = 0.0f;
    }

    if (bldgFile.ReadIdFloat("TimeToBurnDamage", TimeToBurnDamage) != 0)
    {
        TimeToBurnDamage = 5.0f;
    }

    if (bldgFile.ReadIdFloat("BurnDamagePerTime", BurnDamagePerTime) != 0)
    {
        BurnDamagePerTime = 1.0f;
    }

    if (bldgFile.ReadIdFloat("DamageLvlForBurn", DamageLvlForBurn) != 0)
    {
        DamageLvlForBurn = static_cast<float>(DmgLevel);
    }

    int32_t potentialContact = 0;
    bldgFile.ReadIdLong("PotentialContact", potentialContact);

    if (bldgFile.ReadIdLong("TeamID", TeamId) != 0)
    {
        TeamId = -1;
    }

    if (bldgFile.ReadIdFloat("SensorRange", SensorRange) != 0)
    {
        SensorRange = -1.0f;
    }

    if (bldgFile.ReadIdLong("BuildingName", BuildingName) != 0)
    {
        BuildingName = 0xa3;
    }

    result = MCObjectType::Init(&bldgFile);
    ExtentRadius = fitExtentRadius;
    return result;
}

auto MCBuildingType::HandleCollision(MCGameObject* collidee, MCGameObject* collider) -> int
{
    if (MPlayer != nullptr && MPlayer->IsServer == 0)
    {
        return 1;
    }

    // Movers (not artillery) that run into it do 10 points of damage.
    if (collider->ObjectClass < 8 && collider->ObjectClass != 7)
    {
        MCWeaponShotInfo shot;
        shot.Init(nullptr, -1, 10.0f, 0, 0.0f);

        if (ScenarioTime <= collider->GetCollisionFreeTime())
        {
            collidee->HandleWeaponHit(&shot, MPlayer != nullptr ? 1 : 0);
        }
    }

    return 1;
}

auto MCBuildingType::HandleDestruction(MCGameObject*, MCGameObject*) -> int
{
    return 0;
}

//---------------------------------------------------------------------------
// Building
//---------------------------------------------------------------------------

MCBuilding::MCBuilding()
{
    Init();
    JustCreated = 1;
    Appearance = nullptr;
    PixelOffsetY = 0;
    PixelOffsetX = 0;
    VertexNumber = 0;
    BlockNumber = 0;
    Burning = 0;
    TileNum = 0;
    BurnTime = 0.0f;
    Captureable = 0;
    CommanderId = static_cast<char>(0xff);
    Name.clear();
    SoundHandle = 0xffffffff;
    Team = nullptr;
    FireObject = nullptr;
}

auto MCBuilding::Init() -> void
{
    SensorSystem = nullptr;

    for (MCMechWarrior*& slot : PrisonSlots)
    {
        slot = nullptr;
    }
}

auto MCBuilding::IsPrison() -> int
{
    for (const MCMechWarrior* slot : PrisonSlots)
    {
        if (slot != nullptr)
        {
            return 1;
        }
    }

    return 0;
}

auto MCBuilding::GetBlockAndVertexNumber(int32_t& blockNum, int32_t& vertexNum) -> void
{
    blockNum = BlockNumber;
    vertexNum = VertexNumber;
}

auto MCBuilding::SetTerrainPosition(MCVector2D& offset, MCVector2D& numbers) -> void
{
    PixelOffsetX = static_cast<int32_t>(offset.X);
    PixelOffsetY = static_cast<int32_t>(offset.Y);

    if (TileNum != 10)
    {
        auto* type = static_cast<MCBuildingType*>(ObjType);

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

auto MCBuilding::IsVisible(MCCamera* cam) -> int
{
    if (cam == nullptr || cam->Active == 0)
    {
        return 0;
    }

    int visible = cam->VertexProject(BlockNumber, VertexNumber, ScreenPos);

    if (Appearance != nullptr)
    {
        visible = Appearance->RecalcBounds(cam);
    }

    if (visible == 0)
    {
        return 0;
    }

    // Back on screen after a gap: the looping sound is started afresh.
    if (WindowsVisible < Turn - 2)
    {
        SoundHandle = 0xffffffff;
    }

    WindowsVisible = Turn;
    return 1;
}

auto MCBuilding::Update() -> int32_t
{
    if (JustCreated == 0)
    {
        return 1;
    }

    // Set the building on its vertex: the block's corner, the vertex within it, then the offset within the tile
    // (turned into the isometric grid's 60-degree axes). The type's collision offsets, when set, replace the pixel
    // offsets.
    auto* type = static_cast<MCBuildingType*>(ObjType);
    const int32_t blocksMapSide = MCTerrain::BlocksMapSide;
    const int32_t verticesBlockSide = MCTerrain::VerticesBlockSide;
    JustCreated = 0;
    float blockX = static_cast<float>(BlockNumber % blocksMapSide - blocksMapSide / 2) * MCTerrain::MetersBlockSide;
    float blockY = static_cast<float>(blocksMapSide / 2 - BlockNumber / blocksMapSide) * MCTerrain::MetersBlockSide;

    if ((blocksMapSide & 1) != 0)
    {
        blockX = blockX - MCTerrain::MetersBlockSide * 0.5f;
        blockY = MCTerrain::MetersBlockSide * 0.5f + blockY;
    }

    const float vertexX = static_cast<float>(VertexNumber % verticesBlockSide) * MCTerrain::MetersPerVertex;
    int32_t offsetXPixels = PixelOffsetX;

    if (static_cast<double>(type->CollisionOffsetX) != 0.0)
    {
        offsetXPixels = type->CollisionOffsetX;
    }

    int32_t offsetYPixels = PixelOffsetY;

    if (static_cast<float>(type->CollisionOffsetY) != 0.0f)
    {
        offsetYPixels = type->CollisionOffsetY;
    }

    const double offsetY = static_cast<double>(offsetYPixels);
    const double offsetX = static_cast<double>(offsetXPixels);
    double offsetAngle;

    if (offsetY == 0.0)
    {
        offsetAngle = 90.0;
    }
    else
    {
        offsetAngle = std::atan(offsetX / offsetY) * RADIANS_TO_DEGREES;
    }

    Position.Y = blockY - static_cast<float>(VertexNumber / verticesBlockSide) * MCTerrain::MetersPerVertex;
    const auto offsetDistance = static_cast<float>(std::sqrt(offsetY * offsetY + offsetX * offsetX));
    const double axisAngle = (60.0 - offsetAngle) * DEGREES_TO_RADIANS;
    const auto alongAxis = static_cast<float>(std::sin(axisAngle) * offsetDistance / std::sin(SIXTY_DEGREES));
    Position.X = vertexX + blockX;
    const float elevation = Land->GetTerrainElevation(Position);
    Position.X =
        static_cast<float>(std::cos(SIXTY_DEGREES) * alongAxis + std::cos(axisAngle) * offsetDistance + Position.X);
    Position.Y = Position.Y - alongAxis;
    Position.Z = elevation;

    CellColumn = (BlockNumber % MCTerrain::BlocksMapSide) * verticesBlockSide + VertexNumber % verticesBlockSide;
    const int32_t halfMap = (verticesBlockSide * MCTerrain::BlocksMapSide) >> 1;
    VertexWorldX = static_cast<float>(CellColumn - halfMap) * MCTerrain::MetersPerVertex;
    CellRow = VertexNumber / verticesBlockSide + (BlockNumber / MCTerrain::BlocksMapSide) * verticesBlockSide;
    VertexWorldY = static_cast<float>(halfMap - CellRow) * MCTerrain::MetersPerVertex;
    const auto inBounds = [&]
    { return CellRow < 0 || GameMap->Height <= CellRow || CellColumn < 0 || GameMap->Width <= CellColumn ? 0u : 1u; };
    Assert(inBounds(), 0, " bldng MapTile Out of Bounds ");
    Assert(inBounds(), 0, " Map Tile out of bounds ");
    const MCMapTile& tile = GameMap->Map[GameMap->Width * CellRow + CellColumn];
    const int32_t elevationLevel = static_cast<int32_t>((tile.Cells >> 7) & 0x3f) + GameMap->BaseElevation;
    CellElevation = static_cast<float>(elevationLevel) * MCTerrain::MetersPerElevLevel;

    // No extent radius in the FIT: measure it from the appearance's bounds.
    if (type->ExtentRadius < 0.0)
    {
        auto* buildingAppearance = static_cast<MCVfxBuildingAppearance*>(Appearance);
        buildingAppearance->Visible = 1;
        buildingAppearance->Update();
        buildingAppearance->CalcCollideBounds();
        const float dx = buildingAppearance->UpperLeft.X - buildingAppearance->LowerRight.X;
        const float dy = buildingAppearance->UpperLeft.Y - buildingAppearance->LowerRight.Y;
        const float radius = std::sqrt(dx * dx + dy * dy) / WorldUnitsPerMeter * 1.25f;

        if (static_cast<float>(MCCollisionSystem::GridRadius) < radius)
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
        SensorSystem->SetTeam(ClanTeam);
    }
    else if (Alignment == 1)
    {
        SensorSystem->SetTeam(InnerSphereTeam);
    }
    else if (Alignment == 0)
    {
        SensorSystem->SetTeam(AlliedTeam);
    }
}

auto MCBuilding::HandleEvent(MCObjectEvent* event) -> int32_t
{
    if (event->Type == 0)
    {
        switch (event->Id)
        {
            case 0x1c:
                Selected = 1;
                break;
            case 0x1d:
                Selected = 0;
                break;
        }
    }

    return 0;
}

auto MCBuilding::LightOnFire(float timeToBurn) -> void
{
    auto* type = static_cast<MCBuildingType*>(ObjType);

    if (type->BlownEffectId == 0xffffffff)
    {
        return;
    }

    if (FireObject == nullptr)
    {
        MCGameObject* effect = CreateBlownEffect(this, static_cast<int32_t>(type->BlownEffectId));

        if (effect != nullptr && effect->ObjectClass != FIRE)
        {
            DestroyObject(effect);
        }
    }

    if (FireObject != nullptr)
    {
        FireObject->AddTimeLeftToBurn(timeToBurn);
        Burning = 1;
    }
}

auto MCBuilding::IsCaptureable() -> int
{
    if (MPlayer == nullptr)
    {
        return Captureable != 0 && IsCaptured() == 0 && IsDestroyed() == 0 ? 1 : 0;
    }

    return Captureable != 0 && IsDestroyed() == 0 ? 1 : 0;
}

auto MCBuilding::SetCommanderId(int32_t id) -> void
{
    CommanderId = static_cast<char>(id);
}

auto MCBuilding::IsRevealed() -> int
{
    MCByteFlag* visibleBits = HomeTeam->Alignment == -1 ? MCTerrain::ClanVisibleBits : MCTerrain::TerrainVisibleBits;
    const auto col = static_cast<uint32_t>((BlockNumber % MCTerrain::BlocksMapSide) * MCTerrain::VerticesBlockSide +
                                           VertexNumber % MCTerrain::VerticesBlockSide);
    const auto row = static_cast<uint32_t>((BlockNumber / MCTerrain::BlocksMapSide) * MCTerrain::VerticesBlockSide +
                                           VertexNumber / MCTerrain::VerticesBlockSide);

    if (visibleBits->GetFlag(row, col) != 0)
    {
        return 1;
    }

    if (visibleBits->GetFlag(row + 1, col) != 0)
    {
        return 1;
    }

    if (visibleBits->GetFlag(row + 1, col + 1) != 0)
    {
        return 1;
    }

    return visibleBits->GetFlag(row, col + 1) != 0 ? 1 : 0;
}

auto MCBuilding::Render() -> void
{
    if (JustCreated != 0)
    {
        return;
    }

    auto* buildingAppearance = static_cast<MCVfxBuildingAppearance*>(Appearance);
    const int visible = IsVisible(Eye);

    if (buildingAppearance != nullptr)
    {
        buildingAppearance->Visible = visible;
        buildingAppearance->TileNum = TileNum;
        buildingAppearance->Update();
    }

    // Burning: the type's burn damage every TimeToBurnDamage seconds.
    if (FireObject == nullptr)
    {
        Burning = 0;
    }
    else
    {
        const double burnSum = static_cast<double>(FrameLength) + BurnTime;
        BurnTime = static_cast<float>(burnSum);
        auto* type = static_cast<MCBuildingType*>(ObjType);

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

    if (GetContactType(HomeTeam->Id) == 2)
    {
        // A sensor contact: a blip sized by tonnage.
        uint8_t* shape;
        const char* shapeName;

        if (50.0f < GetTonnage())
        {
            shape = Scenario->SensorContactShapes[0];
            shapeName = "blip1";
        }
        else if (35.0f < GetTonnage())
        {
            shape = Scenario->SensorContactShapes[2];
            shapeName = "blip2";
        }
        else
        {
            shape = Scenario->SensorContactShapes[4];
            shapeName = "blip3";
        }

        if (shape != nullptr)
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
            auto* element =
                ElementList()->Make<MCVfxElement>(shape, ScreenPos.X, ScreenPos.Y, BlipFrame, 0, nullptr, 0);
            ElementList()->Add(element);
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
    MCByteFlag* visibleBits = HomeTeam->Alignment == -1 ? MCTerrain::ClanVisibleBits : MCTerrain::TerrainVisibleBits;
    const auto col = static_cast<uint32_t>(CellColumn);
    const auto row = static_cast<uint32_t>(CellRow);
    int32_t numVisible = 0;

    if (visibleBits->GetFlag(row, col) != 0)
    {
        numVisible++;
    }

    if (visibleBits->GetFlag(row + 1, col) != 0)
    {
        numVisible++;
    }

    if (visibleBits->GetFlag(row + 1, col + 1) != 0)
    {
        numVisible++;
    }

    if (visibleBits->GetFlag(row, col + 1) != 0)
    {
        numVisible++;
    }

    uint8_t* hazePalette = nullptr;
    const int32_t hazeLevel = Eye->HazeLevel;

    if (numVisible != 4 && hazeLevel != 0x7fff)
    {
        int32_t level;

        if (hazeLevel < 0 && 0 < Eye->HazeInc * numVisible + hazeLevel)
        {
            level = 0;
        }
        else
        {
            level = hazeLevel + Eye->HazeInc * numVisible;
        }

        hazePalette = GamePalette()->GetHazePalette(level);
    }

    buildingAppearance->FadeTable = hazePalette;

    if (numVisible == 0 || JustCreated != 0)
    {
        if (SoundHandle != 0xffffffff)
        {
            SoundSystem->StopDigitalSample(SoundHandle);
            SoundHandle = 0xffffffff;
        }
    }
    else
    {
        buildingAppearance->Render(0);
        auto* type = static_cast<MCBuildingType*>(ObjType);

        if (SoundHandle == 0xffffffff && type->NormalEffectId != 0xffffffff)
        {
            SoundHandle = static_cast<uint32_t>(SoundSystem->PlayDigitalSample(type->NormalEffectId, 0, this, 1, 0));
        }
    }

    if (DrawExtents != 0)
    {
        // Debug: the extent radius as an ellipse.
        const float diameter = ObjType->ExtentRadius + ObjType->ExtentRadius;
        MCVector2D size;
        size.X = Eye->CosAngle * diameter;
        size.Y = Eye->SinAngle * diameter;

        if (Eye->CameraScale == 1)
        {
            size.X *= 0.5f;
            size.Y *= 0.5f;
        }

        const float scale = Eye->CameraScale != 1 ? 1.0f : 0.5f;
        const float sx = (Position.X - Eye->Position.X) * scale;
        const float sy = (Position.Y - Eye->Position.Y) * scale;
        MCVector2D center;
        center.X = sy * Eye->CosAngle + sx * Eye->CosAngle + Eye->HalfWidth;
        center.Y =
            ((sx * Eye->SinAngle + Eye->HalfHeight) - sy * Eye->SinAngle) - scale * (Position.Z - Eye->Position.Z);
        ElementList()->OpenGroup(-50000, 1);
        // Port: an overlay, on the screen over the view: it follows the object through the zoom.
        center = MCOverlayPoint(center);
        size.X *= MCOverlay.ScaleX;
        size.Y *= MCOverlay.ScaleY;
        ElementList()->Add(ElementList()->Make<MCEllipseElement>(center, size, 0xfe, -50000));
    }
}

auto MCBuilding::Destroy() -> void
{
    delete Appearance;
    Appearance = nullptr;

    if (SensorSystem != nullptr)
    {
        SensorSystemManager->FreeSensor(SensorSystem);
        SensorSystem = nullptr;
    }

    Name.clear();
}

auto MCBuilding::SetDamage(float newDamage) -> void
{
    Damage = newDamage;
    // Sixteen damage frames across the damage level.
    auto* type = static_cast<MCBuildingType*>(ObjType);
    int32_t frame =
        static_cast<int32_t>(newDamage / (static_cast<double>(static_cast<int32_t>(type->DmgLevel)) * 0.0625));

    if (frame > 15)
    {
        frame = 15;
    }

    static_cast<MCVfxBuildingAppearance*>(Appearance)->SetDamageLvl(static_cast<uint32_t>(frame));
}

auto MCBuilding::SetSensorData(MCTeam* newTeam, float range, int setTeam) -> void
{
    if (!(-1.0 < range))
    {
        return;
    }

    if (SensorSystem == nullptr)
    {
        SensorSystem = SensorSystemManager->NewSensor();

        if (SensorSystem == nullptr)
        {
            Fatal(0, " No RAM for Sensor System ");
        }
    }

    SensorSystem->Owner = this;

    if (setTeam != 0)
    {
        SensorSystem->SetTeam(newTeam);
    }

    SensorSystem->SetRange(range);
}

auto MCBuilding::Init(MCObjectType* objType) -> int32_t
{
    int32_t result = MCGameObject::Init(objType);

    if (result != 0)
    {
        return result;
    }

    SetExists(1);
    const uint32_t appearId = objType->AppearName;
    JustCreated = 1;
    MCAppearanceType* apprType = AppearanceTypeList()->GetAppearance(appearId);

    if (apprType == nullptr)
    {
        return static_cast<int32_t>(0xdcdc0007);
    }

    auto* buildingAppearance = new MCVfxBuildingAppearance;

    if (buildingAppearance != nullptr)
    {
        buildingAppearance->Init(nullptr, nullptr);
    }

    Appearance = buildingAppearance;

    if (buildingAppearance == nullptr)
    {
        return static_cast<int32_t>(0xdcdc0008);
    }

    if ((apprType->AppearanceNum & 0xff000000) != 0x7000000)
    {
        return static_cast<int32_t>(0xdcdc0009);
    }

    if ((result = buildingAppearance->Init(apprType, this)) != 0)
    {
        return result;
    }

    auto* type = static_cast<MCBuildingType*>(this->ObjType);
    ObjectClass = BUILDING;
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
    char nameBuffer[256];
    CLoadString(ThisInstance, static_cast<uint32_t>(type->BuildingName), nameBuffer, 0xfe);
    Name = nameBuffer;

    // Original behaviour (OB-016): a building with no team (TeamID -1) reads TeamTable[-1], the global before it in
    // MCX.EXE: homeTeam.
    Team = type->TeamId == -1 ? HomeTeam : TeamTable[type->TeamId];
    const float range = type->SensorRange;

    if (-1.0 < range)
    {
        switch (type->TeamId)
        {
            case 0:
            {
                SetSensorData(InnerSphereTeam, range, 0);
                SetAlignment(1);
                break;
            }
            case 1:
            {
                SetSensorData(ClanTeam, range, 0);
                SetAlignment(-1);
                break;
            }
            case 2:
            {
                SetSensorData(AlliedTeam, range, 0);
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
    auto* type = static_cast<MCBuildingType*>(ObjType);
    const int32_t marinesWanted = type->NumMarines;

    if (marinesWanted == 0)
    {
        return;
    }

    int32_t marinesMade = 0;
    const auto numWarriors = static_cast<int32_t>(Scenario->NumWarriors);

    // Each marine is piloted by an enemy warrior with no working vehicle (none, disabled or destroyed); warrior 0 is
    // never used.
    for (int32_t i = 0; i < numWarriors; i++)
    {
        if (i <= 0 || static_cast<uint32_t>(i) > Scenario->NumWarriors)
        {
            continue;
        }

        MCMechWarrior* warrior = Scenario->Warriors[i];

        if (warrior == nullptr || warrior->Alignment == HomeTeam->Alignment)
        {
            continue;
        }

        if (warrior->Vehicle != nullptr)
        {
            const auto vehicleStatus = static_cast<int8_t>(warrior->Vehicle->Status);

            if (vehicleStatus != 2 && vehicleStatus != 1)
            {
                continue;
            }
        }

        auto* marine = static_cast<MCMover*>(CreateObject(DefaultPilotId));

        if (marine == nullptr)
        {
            Fatal(-1, " Couldnt create Marine for Building ");
        }

        marine->SetAwake(1);
        std::string profileName;
        profileName = GamePath(ProfilePath, MarineProfileName, ".fit");
        MCFitIniFile profileFile;
        const int32_t result = profileFile.Open(profileName);

        if (result != 0)
        {
            Fatal(result, " Unable to open Vehicle Marine Profile ");
        }

        if (marine->Init(&profileFile) != 0)
        {
            Fatal(-1, " Bad Vehicle Marine Profile File ");
        }

        profileFile.Close();

        marine->SetPilot(warrior);
        warrior->SetVehicle(marine);
        warrior->Lobotomy();
        marine->SetControl(2, 3, -1);
        marine->SetTeam(ClanTeam);
        // Somewhere within the extent radius of the building.
        const float extentX = ObjType->ExtentRadius;
        const float extentY = ObjType->ExtentRadius;
        const float offsetX = static_cast<float>(RandomNumber(static_cast<int32_t>(extentX + extentX))) - extentX;
        const float offsetY = static_cast<float>(RandomNumber(static_cast<int32_t>(extentY + extentY))) - extentY;
        const float offsetZ = static_cast<float>(RandomNumber(0));
        MCVector3D marinePosition;
        marinePosition.X = offsetX + Position.X;
        marinePosition.Y = offsetY + Position.Y;
        marinePosition.Z = offsetZ + Position.Z;
        marine->SetPosition(marinePosition);
        GameObjectMap->AddObject(marine);
        marine->BounceToAdjCell();
        marine->BounceToAdjCell();
        auto* marineAppearance = static_cast<MCElementalActor*>(marine->GetAppearance());

        if (marineAppearance != nullptr)
        {
            marineAppearance->SetGesture(0);
            marineAppearance->FadeTableIndex = GetAlignment() == -1 ? 0x1c : 0x12;
        }

        marine->IdNumber = 2500000;
        marine->SetPartId(0xfff - NumMarines++);
        marine->SetAlignment(GetAlignment());
        MCObjectQueueNode* list = GetAlignment() == -1 ? ClanMechList : InnerSphereMechList;

        if (list != nullptr)
        {
            list->AddNode(marine);
        }

        marine->SetPotentialContact(0);
        marine->SetExists(1);
        warrior->ClearAttackOrders();
        warrior->ClearMoveOrders();
        warrior->OrderMoveToPoint(0, 1, 0, MCVector3D(0.0f, 0.0f, 0.0f), -1, 1);

        if (++marinesMade == marinesWanted)
        {
            return;
        }
    }
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

        if (Burning == 0)
        {
            if (type->BlownEffectId != 0xffffffff)
            {
                MCGameObject* effect = CreateBlownEffect(this, static_cast<int32_t>(type->BlownEffectId));

                if (effect != nullptr)
                {
                    if (effect->ObjectClass == FIRE)
                    {
                        FireObject->Update();
                        Burning = 1;
                    }
                    else if (ObjectList->Head != nullptr)
                    {
                        ObjectList->Head->AddNode(effect);
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
            MCTerrain::TerrainTacticalMap->RemoveSalvage(this, 1);
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
