#include "stdafx.h"
#include "object/tree.h"
#include "ai/move.h"
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
#include "lib/MCFile.h"
#include "lib/MCFitIniFile.h"
#include "logistics/logmain.h"
#include "main/main.h"
#include "network/multplyr.h"
#include "object/bldng.h"
#include "object/collsn.h"
#include "object/fire.h"
#include "object/object.h"
#include "object/objevnt.h"
#include "object/team.h"
#include "sound/soundsys.h"
#include "sprite/MCVfxAppearance.h"
#include "terrain/terrain.h"
#include "terrain/terrmap.h"
#include "vfx/MCVfxFunctions.h"
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

    /// <summary>The map row and column of a tree's terrain vertex.</summary>
    void VertexRowCol(const MCTree* tree, uint32_t& row, uint32_t& col)
    {
        col = static_cast<uint32_t>((tree->BlockNumber % MCTerrain::BlocksMapSide) * MCTerrain::VerticesBlockSide +
                                    tree->VertexNumber % MCTerrain::VerticesBlockSide);
        row = static_cast<uint32_t>((tree->BlockNumber / MCTerrain::BlocksMapSide) * MCTerrain::VerticesBlockSide +
                                    tree->VertexNumber / MCTerrain::VerticesBlockSide);
    }
} // namespace

//---------------------------------------------------------------------------
// TreeType
//---------------------------------------------------------------------------

MCTreeType::MCTreeType()
{
    DmgLevel = 0;
    NormalShadow = nullptr;
    DestroyedShadow = nullptr;
}

auto MCTreeType::CreateInstance() -> MCBaseObject*
{
    auto* newTree = new MCTree;

    if (newTree == nullptr)
    {
        return nullptr;
    }

    if (newTree->Init(this) != 0)
    {
        return nullptr;
    }

    newTree->IdNumber = NextIdNumber++;
    return newTree;
}

auto MCTreeType::Destroy() -> void
{
    MCObjectTypeManager::ObjectTypeCache.Free(NormalShadow);
    NormalShadow = nullptr;
    MCObjectTypeManager::ObjectTypeCache.Free(DestroyedShadow);
    DestroyedShadow = nullptr;
}

auto MCTreeType::Init(MCFile* objFile, uint32_t fileSize) -> int32_t
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

    if (treeFile.ReadIdFloat("ExplosionRadius", ExplosionRadius) != 0)
    {
        ExplosionRadius = 0.0f;
    }

    if (treeFile.ReadIdFloat("ExplosionDamage", ExplosionDamage) != 0)
    {
        ExplosionDamage = 0.0f;
    }

    if ((result = LoadShadow(treeFile, "NormalShadow", NormalShadow)) != 0)
    {
        return result;
    }

    if ((result = LoadShadow(treeFile, "DestroyedShadow", DestroyedShadow)) != 0)
    {
        return result;
    }

    return MCObjectType::Init(&treeFile);
}

auto MCTreeType::HandleCollision(MCGameObject* collidee, MCGameObject* collider) -> int
{
    // A mover (not artillery or fire) knocks a standing tree over, away from itself.
    if (MOVER <= collider->ObjectClass || collider->ObjectClass == ARTILLERY || collider->ObjectClass == FIRE)
    {
        return 1;
    }

    auto* tree = static_cast<MCTree*>(collidee);

    if (tree->Fallen != 0 || tree->Falling != 0)
    {
        return 1;
    }

    tree->Falling = 1;
    const MCVector3D colliderPos = collider->GetPosition();
    const auto facing = static_cast<float>(tree->RelFacingTo(colliderPos, -1));
    MCFrameOfRef frame = tree->GetFrame();
    const auto s = static_cast<float>(std::sin(facing * DEGREES_TO_RADIANS));
    const auto c = static_cast<float>(std::cos(facing * DEGREES_TO_RADIANS));
    const MCVector3D oldI = frame.I;
    frame.I = frame.I * c + frame.J * s;
    frame.J = frame.J * c - oldI * s;
    tree->SetFrame(frame);

    // Fall (state 1), or a burnt tree crumble (state 4); a fall with frames to show makes a sound.
    auto* treeAppearance = static_cast<MCVfxAppearance*>(tree->Appearance);
    tree->CollisionsOn = 0;
    uint32_t numFrames = 0;

    if (tree->Burnt == 0)
    {
        treeAppearance->SetTypeId(MCActorState::BlowingUp1, 0xff);

        if (1 < treeAppearance->AppearType->NumStates)
        {
            numFrames = treeAppearance->AppearType->States[1].NumFrames;
        }
    }
    else
    {
        treeAppearance->SetTypeId(MCActorState::Destroyed, NoSubState);

        if (4 < treeAppearance->AppearType->NumStates)
        {
            numFrames = treeAppearance->AppearType->States[4].NumFrames;
        }
    }

    if (UseSound != 0 && SoundSystem != nullptr && 1 < static_cast<int32_t>(numFrames))
    {
        SoundSystem->PlayDigitalSample(0xe, 1, tree, 0, 0);
    }

    return 1;
}

auto MCTreeType::HandleDestruction(MCGameObject*, MCGameObject*) -> int
{
    return 0;
}

//---------------------------------------------------------------------------
// Tree
//---------------------------------------------------------------------------

MCTree::MCTree()
{
    TreeFrame.I = UnitX;
    TreeFrame.J = UnitY;
    TreeFrame.K = UnitZ;
    JustCreated = 1;
    Appearance = nullptr;
    VertexNumber = 0;
    BlockNumber = 0;
    FireStarted = 0;
    Burnt = 0;
    Fallen = 0;
    Falling = 0;
    FireObject = nullptr;
}

auto MCTree::Init() -> void
{
}

auto MCTree::KillFireObject() -> void
{
    FireObject = nullptr;
}

auto MCTree::SetTerrainPosition(MCVector2D& pixelOffset, MCVector2D& blockVertex) -> void
{
    PixelOffsetX = static_cast<int32_t>(pixelOffset.X);
    PixelOffsetY = static_cast<int32_t>(pixelOffset.Y);
    VertexNumber = static_cast<int32_t>(blockVertex.X);
    BlockNumber = static_cast<int32_t>(blockVertex.Y);
}

auto MCTree::SetFrame(MCFrameOfRef& newFrame) -> void
{
    TreeFrame = newFrame;
}

auto MCTree::GetBlockAndVertexNumber(int32_t& blockNum, int32_t& vertexNum) -> void
{
    blockNum = BlockNumber;
    vertexNum = VertexNumber;
}

auto MCTree::IsVisible(MCCamera* cam) -> int
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

    // The shadow can stick out past the tree: on screen when any of its box is.
    uint8_t* shadow = static_cast<MCTreeType*>(ObjType)->NormalShadow;

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

            if (left <= static_cast<float>(viewRight) && top <= static_cast<float>(viewBottom))
            {
                WindowsVisible = Turn;
                return 1;
            }
        }
    }

    if (visible == 0)
    {
        return 0;
    }

    WindowsVisible = Turn;
    return 1;
}

auto MCTree::Update() -> int32_t
{
    if (JustCreated == 0)
    {
        return 1;
    }

    // Set the tree on its vertex: the block's corner, the vertex within it, then the pixel offset within the tile
    // (turned into the isometric grid's 60-degree axes).
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

    TileCol = (BlockNumber % MCTerrain::BlocksMapSide) * verticesBlockSide + VertexNumber % verticesBlockSide;
    const int32_t halfMap = (verticesBlockSide * MCTerrain::BlocksMapSide) >> 1;
    TileWorldX = static_cast<float>(TileCol - halfMap) * MCTerrain::MetersPerVertex;
    TileRow = VertexNumber / verticesBlockSide + (BlockNumber / MCTerrain::BlocksMapSide) * verticesBlockSide;
    TileWorldY = static_cast<float>(halfMap - TileRow) * MCTerrain::MetersPerVertex;
    const auto inBounds = [&]
    { return TileRow < 0 || GameMap->Height <= TileRow || TileCol < 0 || GameMap->Width <= TileCol ? 0u : 1u; };
    Assert(inBounds(), 0, " tree MapTile Out of Bounds ");
    Assert(inBounds(), 0, " Map Tile out of bounds ");
    const MCMapTile& tile = GameMap->Map[GameMap->Width * TileRow + TileCol];
    const int32_t elevationLevel = static_cast<int32_t>((tile.Cells >> 7) & 0x3f) + GameMap->BaseElevation;
    Appearance->Visible = 1;
    TileElevation = static_cast<float>(elevationLevel) * MCTerrain::MetersPerElevLevel;

    // Every tree measures the type's extent radius from its appearance's diagonal.
    Appearance->Update();
    Appearance->RecalcBounds(Eye);
    const double dx = static_cast<double>(Appearance->UpperLeft.X) - Appearance->LowerRight.X;
    const double dy = static_cast<double>(Appearance->UpperLeft.Y) - Appearance->LowerRight.Y;
    const auto radius = static_cast<float>(std::sqrt(dy * dy + dx * dx) / WorldUnitsPerMeter);

    if (static_cast<float>(MCCollisionSystem::GridRadius) < radius)
    {
        Fatal(static_cast<int32_t>(std::floor(static_cast<double>(radius))), " Object extent radius TOO large ");
    }

    ObjType->ExtentRadius = radius;
    CollisionsOn = 1;
    return 1;
}

auto MCTree::HandleEvent(MCObjectEvent* event) -> int32_t
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
            default:
                break;
        }
    }

    return 0;
}

auto MCTree::LightOnFire(float timeToBurn) -> void
{
    // A fire does 25 points to the tree (the server's job in multiplayer), and burns on.
    MCWeaponShotInfo shot;
    shot.Init(nullptr, -1, 25.0f, 0, 0.0f);

    if (MPlayer == nullptr)
    {
        HandleWeaponHit(&shot, 0);
    }
    else if (MPlayer->IsServer != 0)
    {
        HandleWeaponHit(&shot, 1);
    }

    if (FireObject != nullptr)
    {
        FireObject->AddTimeLeftToBurn(timeToBurn);
        FireStarted = 1;
    }
}

auto MCTree::Render() -> void
{
    if (JustCreated != 0)
    {
        return;
    }

    if (FireObject == nullptr)
    {
        FireStarted = 0;
    }

    auto* treeAppearance = static_cast<MCVfxAppearance*>(Appearance);

    if (treeAppearance != nullptr)
    {
        treeAppearance->Visible = IsVisible(Eye);

        // A falling tree comes to rest: fallen (2), or fallen burnt (5).
        if (treeAppearance->Update() == 0 && Falling != 0)
        {
            Falling = 0;
            CollisionsOn = 0;
            Fallen = 1;

            if (treeAppearance->CurrentState == MCActorState::BlowingUp1)
            {
                treeAppearance->SetTypeId(MCActorState::Damaged, 0xff);
            }
            else if (treeAppearance->CurrentState == static_cast<MCActorState>(4))
            {
                treeAppearance->SetTypeId(static_cast<MCActorState>(5), 0xff);
            }

            treeAppearance->Update();
        }
    }

    if (WindowsVisible != Turn)
    {
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
        numVisible = 1;
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

        hazePalette = GamePalette()->GetHazePalette(level);
    }

    treeAppearance->FadeTable = hazePalette;

    if (numVisible != 0)
    {
        // Standing, it sorts with the terrain; fallen, by its screen row.
        const auto* type = static_cast<MCTreeType*>(ObjType);
        treeAppearance->Render(treeAppearance->CurrentState != MCActorState::Normal ? static_cast<int32_t>(ScreenPos.Y)
                                                                                    : 0);

        if (treeAppearance->CurrentState != MCActorState::Normal)
        {
            if (type->DestroyedShadow != nullptr)
            {
                ElementList()->OpenGroup(static_cast<int32_t>(ScreenPos.Y), 1);
                ElementList()->Add(ElementList()->Make<MCVfxElement>(type->DestroyedShadow, ScreenPos.X, ScreenPos.Y, 0,
                                                                     0, hazePalette, 0));
            }
        }
        else if (type->NormalShadow != nullptr)
        {
            ElementList()->OpenGroup(static_cast<int32_t>(-ScreenPos.Y), 1);
            ElementList()->Add(
                ElementList()->Make<MCVfxElement>(type->NormalShadow, ScreenPos.X, ScreenPos.Y, 0, 0, hazePalette, 0));
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
        ElementList()->OpenGroup(-50000, 1);
        // Port: an overlay, on the screen over the view: it follows the object through the zoom.
        center = MCOverlayPoint(center);
        size.X *= MCOverlay.ScaleX;
        size.Y *= MCOverlay.ScaleY;
        ElementList()->Add(ElementList()->Make<MCEllipseElement>(center, size, 0xfe, -50000));
    }
}

auto MCTree::Destroy() -> void
{
    delete Appearance;
    Appearance = nullptr;
}

auto MCTree::Init(MCObjectType* objType) -> int32_t
{
    int32_t result = MCGameObject::Init(objType);

    if (result != 0)
    {
        return result;
    }

    JustCreated = 1;
    MCAppearanceType* apprType = AppearanceTypeList()->GetAppearance(objType->AppearName);

    if (apprType == nullptr)
    {
        return static_cast<int32_t>(0xdcdc0007);
    }

    auto* vfxAppearance = new MCVfxAppearance;
    Appearance = vfxAppearance;

    if (vfxAppearance == nullptr)
    {
        return static_cast<int32_t>(0xdcdc0008);
    }

    vfxAppearance->Init(nullptr, nullptr);

    if ((apprType->AppearanceNum & 0xff000000) != 0x2000000)
    {
        return static_cast<int32_t>(0xdcdc0009);
    }

    if ((result = vfxAppearance->Init(apprType, this)) != 0)
    {
        return result;
    }

    ObjectClass = TREE;
    Burnt = 0;
    return 0;
}

auto MCTree::HandleWeaponHit(MCWeaponShotInfo* shotInfo, int addMultiplayChunk) -> int32_t
{
    if (shotInfo == nullptr)
    {
        return 0;
    }

    if (addMultiplayChunk != 0)
    {
        MPlayer->AddWeaponHitChunk(this, shotInfo, 0);
    }

    // Any hit burns the tree: standing (0) to burnt (3), fallen (2) to fallen burnt (5).
    SetDamage(GetDamage() + 1.0f);
    Burnt = 1;

    if (Falling == 0)
    {
        auto* treeAppearance = static_cast<MCVfxAppearance*>(Appearance);

        if (treeAppearance->CurrentState == MCActorState::Normal)
        {
            treeAppearance->SetTypeId(MCActorState::BlowingUp2, 0xff);
        }
        else if (treeAppearance->CurrentState == MCActorState::Damaged)
        {
            treeAppearance->SetTypeId(static_cast<MCActorState>(5), 0xff);
        }
    }

    Status = 2;

    // The first hit sets it alight; later ones keep the fire going 2 more seconds.
    if (FireStarted == 0)
    {
        if (ObjType->ExplosionObject != -1)
        {
            auto* fire = static_cast<MCFire*>(CreateObject(ObjType->ExplosionObject));

            if (fire != nullptr)
            {
                fire->BurningObject = this;
                fire->SetTonnage(40.0f);
                fire->SetPosition(Position);
                fire->Update();
                FireObject = fire;
            }
        }

        FireStarted = 1;
        return 0;
    }

    if (FireObject != nullptr)
    {
        FireObject->AddTimeLeftToBurn(2.0f);
    }

    return 0;
}

auto MCTree::IsRevealed() -> int
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
