#include "stdafx.h"
#include "object/tbldng.h"
#include "appear/apprtype.h"
#include "camera/camera.h"
#include "color/color.h"
#include "engine/bitflag.h"
#include "engine/ceglist.h"
#include "engine/cellip.h"
#include "engine/cevfx.h"
#include "lib/MCFatal.h"
#include "lib/MCIDString.h"
#include "lib/MCFrameOfRef.h"
#include "lib/MCVector2D.h"
#include "lib/MCDice.h"
#include "lib/MCFile.h"
#include "lib/MCFitIniFile.h"
#include "logistics/logmain.h"
#include "main/main.h"
#include "mission/scenario.h"
#include "network/multplyr.h"
#include "object/bldng.h"
#include "object/contact.h"
#include "object/fire.h"
#include "object/mover.h"
#include "object/object.h"
#include "object/objevnt.h"
#include "object/objque.h"
#include "object/team.h"
#include "object/warrior.h"
#include "sound/soundsys.h"
#include "sprite/actor.h"
#include "sprite/lactor.h"
#include "terrain/terrain.h"
#include "terrain/terrmap.h"
#include "vfx/vfxfuncs.h"
#include "platform/MCRenderer.h"

namespace
{
    /// <summary>Degrees to radians, as MCX.EXE stores it (a hair under pi / 180).</summary>
    constexpr double DEGREES_TO_RADIANS = 0x1.1df46a2526c7ap-6;
    /// <summary>Radians to degrees, as MCX.EXE stores it.</summary>
    constexpr double RADIANS_TO_DEGREES = 0x1.ca5dc1a6402aap+5;
    /// <summary>Sixty degrees in radians, as MCX.EXE stores it.</summary>
    constexpr double SIXTY_DEGREES = 0x1.0c152382d45b2p+0;

    /// <summary>
    /// Loads the shadow shape named by FIT entry <paramref name="entry"/> (a .shp in spritePath) into the object type
    /// cache. No entry leaves <paramref name="shadow"/> alone and succeeds; a file that won't open returns its error.
    /// </summary>
    int32_t LoadShadow(MCFitIniFile& typeFile, const char* entry, uint8_t*& shadow)
    {
        char shadowName[80];

        if (typeFile.ReadIdString(entry, shadowName, 79) != 0)
        {
            return 0;
        }

        std::string shadowPath;
        shadowPath = GamePath(SpritePath, shadowName, ".shp");
        MCFile shadowFile;
        const int32_t result = shadowFile.Open(shadowPath);

        if (result != 0)
        {
            return result;
        }

        const uint32_t size = shadowFile.FileSize();
        shadow = static_cast<uint8_t*>(MCObjectTypeManager::ObjectTypeCache.Allocate(size));
        shadowFile.Read(shadow, static_cast<int32_t>(size));
        MCRenderer::RegisterData(shadow, size, MCDataKind::Shapes);
        shadowFile.Close();
        return 0;
    }

    /// <summary>The map row and column of a tree building's terrain vertex.</summary>
    void VertexRowCol(const MCTreeBuilding* building, uint32_t& row, uint32_t& col)
    {
        col = static_cast<uint32_t>((building->BlockNumber % MCTerrain::BlocksMapSide) * MCTerrain::VerticesBlockSide +
                                    building->VertexNumber % MCTerrain::VerticesBlockSide);
        row = static_cast<uint32_t>((building->BlockNumber / MCTerrain::BlocksMapSide) * MCTerrain::VerticesBlockSide +
                                    building->VertexNumber / MCTerrain::VerticesBlockSide);
    }
}

//---------------------------------------------------------------------------
// TreeBuildingType
//---------------------------------------------------------------------------

auto MCTreeBuildingType::Init() -> void
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
    NormalShadow = nullptr;
    DestroyedShadow = nullptr;
    BattleRating = 0;
    NumMarines = 0;
    CanRefit = 0;
    MechBay = 0;
}

auto MCTreeBuildingType::CreateInstance() -> MCBaseObject*
{
    auto* newBuilding = new MCTreeBuilding;

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

auto MCTreeBuildingType::Destroy() -> void
{
    MCObjectTypeManager::ObjectTypeCache.Free(NormalShadow);
    NormalShadow = nullptr;
    MCObjectTypeManager::ObjectTypeCache.Free(DestroyedShadow);
    DestroyedShadow = nullptr;
}

auto MCTreeBuildingType::Init(MCFile* objFile, uint32_t fileSize) -> int32_t
{
    MCFitIniFile treeFile;
    int32_t result = treeFile.Open(objFile, fileSize);

    if (result != 0)
    {
        return result;
    }

    if ((result = treeFile.SeekBlock("TreeData")) != 0)
    {
        return result;
    }

    if ((result = treeFile.ReadIdULong("DmgLevel", DmgLevel)) != 0)
    {
        return result;
    }

    treeFile.ReadIdULong("NormalEffectId", NormalEffectId);
    treeFile.ReadIdULong("BlownEffectId", BlownEffectId);
    treeFile.ReadIdULong("DamageEffectId", DamageEffectId);

    if (treeFile.ReadIdBoolean("CanRefit", CanRefit) != 0)
    {
        CanRefit = 0;
    }

    if (CanRefit != 0 && treeFile.ReadIdBoolean("MechBay", MechBay) != 0)
    {
        MechBay = 0;
    }

    if ((result = LoadShadow(treeFile, "NormalShadow", NormalShadow)) != 0)
    {
        return result;
    }

    if ((result = LoadShadow(treeFile, "DestroyedShadow", DestroyedShadow)) != 0)
    {
        return result;
    }

    if (treeFile.ReadIdFloat("ExplosionRadius", ExplRad) != 0)
    {
        ExplRad = 0.0f;
    }

    if (treeFile.ReadIdFloat("ExplosionDamage", ExplDmg) != 0)
    {
        ExplDmg = 0.0f;
    }

    if (treeFile.ReadIdFloat("TimeToBurnDamage", TimeToBurnDamage) != 0)
    {
        TimeToBurnDamage = 5.0f;
    }

    if (treeFile.ReadIdFloat("BurnDamagePerTime", BurnDamagePerTime) != 0)
    {
        BurnDamagePerTime = 1.0f;
    }

    if (treeFile.ReadIdFloat("DamageLvlForBurn", DamageLvlForBurn) != 0)
    {
        DamageLvlForBurn = static_cast<float>(DmgLevel);
    }

    // The team is only read for a building with a sensor.
    if (treeFile.ReadIdFloat("SensorRange", SensorRange) == 0)
    {
        if (treeFile.ReadIdLong("TeamID", TeamId) != 0)
        {
            TeamId = -1;
        }
    }
    else
    {
        SensorRange = -1.0f;
    }

    if (treeFile.ReadIdFloat("Tonnage", BaseTonnage) != 0)
    {
        BaseTonnage = 20.0f;
    }

    if (treeFile.ReadIdLong("BattleRating", BattleRating) != 0)
    {
        BattleRating = 20;
    }

    if (treeFile.ReadIdLong("NumMarines", NumMarines) != 0)
    {
        NumMarines = 0;
    }

    if (treeFile.ReadIdLong("BuildingName", BuildingName) != 0)
    {
        BuildingName = 0xa3;
    }

    return MCObjectType::Init(&treeFile);
}

auto MCTreeBuildingType::HandleCollision(MCGameObject*, MCGameObject*) -> int
{
    return 1;
}

auto MCTreeBuildingType::HandleDestruction(MCGameObject*, MCGameObject*) -> int
{
    return 0;
}

//---------------------------------------------------------------------------
// TreeBuilding
//---------------------------------------------------------------------------

MCTreeBuilding::MCTreeBuilding()
{
    Frame.ResetToWorldFrame();
    Init();
    JustCreated = 1;
    Appearance = nullptr;
    VertexNumber = 0;
    BlockNumber = 0;
    Burning = 0;
    HitOnce = 0;
    Collapsed = 0;
    Collapsing = 0;
    BurnTime = 0.0f;
    Name.clear();
    FireObject = nullptr;
    SensorSystem = nullptr;
    SoundHandle = 0xffffffff;
    CommanderId = static_cast<char>(0xff);
    CanRefit = 0;
    MechBay = 0;
}

auto MCTreeBuilding::Init() -> void
{
    for (MCMechWarrior*& slot : PrisonSlots)
    {
        slot = nullptr;
    }
}

auto MCTreeBuilding::SetTerrainPosition(MCVector2D& offset, MCVector2D& numbers) -> void
{
    PixelOffsetX = static_cast<int32_t>(offset.X);
    PixelOffsetY = static_cast<int32_t>(offset.Y);
    VertexNumber = static_cast<int32_t>(numbers.X);
    BlockNumber = static_cast<int32_t>(numbers.Y);
}

auto MCTreeBuilding::GetFrame() -> MCFrameOfRef
{
    return Frame;
}

auto MCTreeBuilding::SetFrame(MCFrameOfRef& newFrame) -> void
{
    Frame = newFrame;
}

auto MCTreeBuilding::IsPrison() -> int
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

auto MCTreeBuilding::GetBlockAndVertexNumber(int32_t& blockNum, int32_t& vertexNum) -> void
{
    blockNum = BlockNumber;
    vertexNum = VertexNumber;
}

auto MCTreeBuilding::GetRefitPoints() -> float
{
    if (CanRefit == 0)
    {
        return 0.0f;
    }

    return static_cast<float>(static_cast<int32_t>(static_cast<MCTreeBuildingType*>(ObjType)->DmgLevel)) - Damage;
}

auto MCTreeBuilding::BurnRefitPoints(float points) -> int
{
    if (CanRefit == 0)
    {
        return 0;
    }

    // Spent refit points count as damage; never more than are left.
    if (points < GetRefitPoints())
    {
        DamageObject(points);
    }
    else
    {
        DamageObject(GetRefitPoints());
    }

    return 1;
}

auto MCTreeBuilding::IsVisible(MCCamera* cam) -> int
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

    // The shadow can stick out past the building: on screen when any of its box is.
    bool shadowOnScreen = false;
    uint8_t* shadow = static_cast<MCTreeBuildingType*>(ObjType)->NormalShadow;

    if (shadow != nullptr)
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

auto MCTreeBuilding::IsCaptureable() -> int
{
    if (MPlayer == nullptr)
    {
        return Captureable != 0 && IsCaptured() == 0 && IsDestroyed() == 0 ? 1 : 0;
    }

    return Captureable != 0 && IsDestroyed() == 0 ? 1 : 0;
}

auto MCTreeBuilding::Update() -> int32_t
{
    if (JustCreated == 0)
    {
        return 1;
    }

    // Set the building on its vertex: the block's corner, the vertex within it, then the pixel offset within the
    // tile (turned into the isometric grid's 60-degree axes).
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
    const double offsetY = static_cast<double>(PixelOffsetY);
    const double offsetX = static_cast<double>(PixelOffsetX);
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
    Assert(inBounds(), 0, " tbldg MapTile Out of Bounds ");
    Assert(inBounds(), 0, " Map Tile out of bounds ");
    const MCMapTile& tile = GameMap->Map[GameMap->Width * CellRow + CellColumn];
    const int32_t elevationLevel = static_cast<int32_t>((tile.Cells >> 7) & 0x3f) + GameMap->BaseElevation;
    auto* treeAppearance = static_cast<MCVfxAppearance*>(Appearance);
    treeAppearance->Visible = 1;
    CellElevation = static_cast<float>(elevationLevel) * MCTerrain::MetersPerElevLevel;
    treeAppearance->Update();
    treeAppearance->RecalcBounds(Eye);

    if (CanRefit != 0)
    {
        treeAppearance->SetTypeId(ACTOR_STATE_NORMAL, 0);
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

auto MCTreeBuilding::SetCommanderId(int32_t id) -> void
{
    CommanderId = static_cast<char>(id);
}

auto MCTreeBuilding::HandleEvent(MCObjectEvent* event) -> int32_t
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

auto MCTreeBuilding::LightOnFire(float timeToBurn) -> void
{
    auto* type = static_cast<MCTreeBuildingType*>(ObjType);

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
        MCGameObject* newFire = CreateObject(static_cast<int32_t>(type->BlownEffectId));

        if (newFire != nullptr)
        {
            newFire->SetPosition(Position);

            if (newFire->ObjectClass == FIRE)
            {
                FireObject = static_cast<MCFire*>(newFire);
                FireObject->SetPotentialContact(3);
                FireObject->BurningObject = this;
                FireObject->SetTonnage(40.0f);
            }
            else
            {
                DestroyObject(newFire);
            }
        }
    }

    if (FireObject != nullptr)
    {
        FireObject->AddTimeLeftToBurn(timeToBurn);
        Burning = 1;
    }
}

auto MCTreeBuilding::IsRevealed() -> int
{
    MCByteFlag* visibleBits = HomeTeam->Alignment == -1 ? MCTerrain::ClanVisibleBits : MCTerrain::TerrainVisibleBits;
    uint32_t row;
    uint32_t col;
    VertexRowCol(this, row, col);

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

auto MCTreeBuilding::Render() -> void
{
    if (JustCreated != 0)
    {
        return;
    }

    auto* type = static_cast<MCTreeBuildingType*>(ObjType);

    // Burning: the type's burn damage every TimeToBurnDamage seconds.
    if (FireObject == nullptr)
    {
        Burning = 0;
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

    auto* treeAppearance = static_cast<MCVfxAppearance*>(Appearance);

    if (treeAppearance != nullptr)
    {
        treeAppearance->Visible = IsVisible(Eye);

        // When the collapse animation ends, settle on the matching rubble state.
        if (treeAppearance->Update() == 0 && Collapsing != 0)
        {
            const MCActorState state = treeAppearance->CurrentState;
            Collapsing = 0;
            Collapsed = 1;

            if (state == ACTOR_STATE_BLOWING_UP1)
            {
                treeAppearance->SetTypeId(ACTOR_STATE_DAMAGED, 0xff);
            }
            else if (state == ACTOR_STATE_DESTROYED)
            {
                treeAppearance->SetTypeId(ACTOR_STATE_FALLEN_DMG, 0xff);
            }

            treeAppearance->Update();
        }
    }

    if (GetContactType(HomeTeam->Id) == 2)
    {
        // A sensor contact: a blip sized by tonnage.
        uint8_t* shape;

        if (50.0f < GetTonnage())
        {
            shape = Scenario->SensorContactShapes[0];
        }
        else if (35.0f < GetTonnage())
        {
            shape = Scenario->SensorContactShapes[2];
        }
        else
        {
            shape = Scenario->SensorContactShapes[4];
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

            ElementList->OpenGroup(-100000, 1);
            ElementList->Add(
                MCElementPool::Make<MCVfxElement>(shape, ScreenPos.X, ScreenPos.Y, BlipFrame, 0, nullptr, 0, 0));
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

    // Hazed by how many corners of its vertex square the home team sees; drawn (with its shadow) when any is. (The
    // original also reads each corner's seen bit and drops it.)
    uint32_t row;
    uint32_t col;
    VertexRowCol(this, row, col);
    MCByteFlag* visibleBits = HomeTeam->Alignment == -1 ? MCTerrain::ClanVisibleBits : MCTerrain::TerrainVisibleBits;
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

    if (numVisible != 0 && numVisible != 4 && hazeLevel != 0x7fff)
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

        hazePalette = GamePalette->GetHazePalette(level);
    }

    treeAppearance->FadeTable = hazePalette;

    if (numVisible == 0)
    {
        if (SoundHandle != 0xffffffff)
        {
            SoundSystem->StopDigitalSample(SoundHandle);
            SoundHandle = 0xffffffff;
        }
    }
    else
    {
        // Standing, it sorts with the terrain; fallen or burning down, by its screen row.
        const bool standing = treeAppearance->CurrentState == ACTOR_STATE_NORMAL;
        treeAppearance->Render(standing ? 0 : static_cast<int32_t>(ScreenPos.Y));
        uint8_t* shadow = standing ? type->NormalShadow : type->DestroyedShadow;

        if (shadow != nullptr)
        {
            ElementList->OpenGroup(static_cast<int32_t>(ScreenPos.Y), 1);
            ElementList->Add(
                MCElementPool::Make<MCVfxElement>(shadow, ScreenPos.X, ScreenPos.Y, 0, 0, hazePalette, 0, 0));
        }

        if (SoundHandle == 0xffffffff && type->NormalEffectId != 0xffffffff)
        {
            SoundHandle = static_cast<uint32_t>(SoundSystem->PlayDigitalSample(type->NormalEffectId, 0, this, 1, 0));
        }
    }

    if (DrawExtents != 0)
    {
        // Debug: the extent radius as an ellipse.
        float radius = ObjType->ExtentRadius;

        if (Eye->CameraScale == 1)
        {
            radius *= 0.5f;
        }

        const float scale = Eye->CameraScale != 1 ? 1.0f : 0.5f;
        const float sx = (Position.X - Eye->Position.X) * scale;
        const float sy = (Position.Y - Eye->Position.Y) * scale;
        MCVector2D center;
        center.X = sx * Eye->CosAngle + sy * Eye->CosAngle + Eye->HalfWidth;
        center.Y =
            ((sx * Eye->SinAngle + Eye->HalfHeight) - sy * Eye->SinAngle) - scale * (Position.Z - Eye->Position.Z);
        MCVector2D size(radius, radius);
        ElementList->OpenGroup(-50000, 1);
        // Port: an overlay, on the screen over the view: it follows the object through the zoom.
        center = MCOverlayPoint(center);
        size.X *= MCOverlay.ScaleX;
        size.Y *= MCOverlay.ScaleY;
        ElementList->Add(MCElementPool::Make<MCEllipseElement>(center, size, 0xfe, -50000));
    }
}

auto MCTreeBuilding::Destroy() -> void
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

auto MCTreeBuilding::SetSensorData(MCTeam* newTeam, float range, int setTeam) -> void
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

auto MCTreeBuilding::Init(MCObjectType* objType) -> int32_t
{
    int32_t result = MCGameObject::Init(objType);

    if (result != 0)
    {
        return result;
    }

    SetExists(1);
    const uint32_t appearId = objType->AppearName;
    JustCreated = 1;
    MCAppearanceType* apprType = AppearanceTypeList->GetAppearance(appearId, 0);

    if (apprType == nullptr)
    {
        return static_cast<int32_t>(0xdcdc0007);
    }

    auto* treeAppearance = new MCVfxAppearance;

    if (treeAppearance != nullptr)
    {
        treeAppearance->Init(nullptr, nullptr);
    }

    Appearance = treeAppearance;

    if (treeAppearance == nullptr)
    {
        return static_cast<int32_t>(0xdcdc0008);
    }

    if ((apprType->AppearanceNum & 0xff000000) != 0x2000000)
    {
        return static_cast<int32_t>(0xdcdc0009);
    }

    if ((result = treeAppearance->Init(apprType, this)) != 0)
    {
        return result;
    }

    auto* type = static_cast<MCTreeBuildingType*>(this->ObjType);
    ObjectClass = TREEBUILDING;
    HitOnce = 0;
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
    char nameBuffer[256];
    CLoadString(ThisInstance, static_cast<uint32_t>(type->BuildingName), nameBuffer, 0xfe);
    Name = nameBuffer;

    // Original behaviour (OB-016): with no team (TeamID -1) this reads TeamTable[-1], which in MCX.EXE is homeTeam.
    TypeTeam = type->TeamId == -1 ? HomeTeam : TeamTable[type->TeamId];
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

    Captureable = 0;
    RefitBuddy = nullptr;

    // Damage level 0: already rubble.
    if (type->DmgLevel == 0)
    {
        CollisionsOn = 0;
        Status = 2;
        HitOnce = 1;
    }

    return 0;
}

auto MCTreeBuilding::CreateBuildingMarines() -> void
{
    auto* type = static_cast<MCTreeBuildingType*>(ObjType);
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
        // A random direction, set 1.5 extent radii out on the ground (z stays the unscaled unit component).
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
        marine->SetPosition(marinePosition);
        marine->SetLastValidPosition(Position + offset);
        GameObjectMap->AddObject(marine);
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
    HitOnce = 1;
    auto* type = static_cast<MCTreeBuildingType*>(ObjType);

    if (newDamage < static_cast<float>(static_cast<int32_t>(type->DmgLevel)) || Collapsed != 0 || Collapsing != 0)
    {
        return 0;
    }

    // Collapses: the shooter's pilot is alarmed, the marines come out, and the sensor, fire and explosion follow.
    Collapsing = 1;
    CollisionsOn = 0;
    Status = 2;

    if (IsCaptured() != 0)
    {
        MCTerrain::TerrainTacticalMap->RemoveSalvage(this, 1);
    }

    MCGameObject* attacker = shotInfo->Attacker;

    if (attacker != nullptr && (attacker->ObjectClass == BATTLEMECH || attacker->ObjectClass == GROUNDVEHICLE ||
                                attacker->ObjectClass == ELEMENTAL || attacker->ObjectClass == MOVER))
    {
        attacker->GetPilot()->TriggerAlarm(12, static_cast<uint32_t>(PartId));
    }

    if (MPlayer == nullptr)
    {
        CreateBuildingMarines();
    }

    if (type->DamageEffectId != 0xffffffff && 5 < Turn)
    {
        SoundSystem->PlayDigitalSample(type->DamageEffectId, 1, this, 1, 0);
    }

    if (SensorSystem != nullptr)
    {
        SensorSystem->Disable();
    }

    // Original behaviour: hitOnce was set just above, so the collapse always plays the destroyed state (4), never 1.
    auto* treeAppearance = static_cast<MCVfxAppearance*>(Appearance);
    treeAppearance->SetTypeId(HitOnce == 0 ? ACTOR_STATE_BLOWING_UP1 : ACTOR_STATE_DESTROYED, 0xff);

    if (Burning == 0)
    {
        if (type->BlownEffectId != 0xffffffff)
        {
            MCGameObject* newFire = CreateObject(static_cast<int32_t>(type->BlownEffectId));

            if (newFire != nullptr)
            {
                newFire->SetPosition(Position);

                if (newFire->ObjectClass == FIRE)
                {
                    FireObject = static_cast<MCFire*>(newFire);
                    FireObject->SetPotentialContact(3);
                    FireObject->BurningObject = this;
                    FireObject->SetTonnage(40.0f);
                    FireObject->Update();
                    Burning = 1;
                }
                else if (ObjectList->Head != nullptr)
                {
                    ObjectList->Head->AddNode(newFire);
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
