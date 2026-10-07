#include "stdafx.h"
#include "object/terrobj.h"
#include "ai/move.h"
#include "appear/apprtype.h"
#include "camera/camera.h"
#include "color/MCPalette.h"
#include "engine/MCByteFlag.h"
#include "engine/MCElementBuffer.h"
#include "engine/MCEllipseElement.h"
#include "lib/MCFatal.h"
#include "lib/MCFrameOfRef.h"
#include "lib/MCVector2D.h"
#include "lib/MCDice.h"
#include "lib/MCFile.h"
#include "lib/MCFitIniFile.h"
#include "main/main.h"
#include "network/multplyr.h"
#include "object/bldng.h"
#include "object/collsn.h"
#include "object/fire.h"
#include "object/object.h"
#include "object/objevnt.h"
#include "object/team.h"
#include "sprite/actor.h"
#include "terrain/terrain.h"
#include "terrain/terrmap.h"

namespace
{
    /// <summary>Degrees to radians, as MCX.EXE stores it (a hair under pi / 180).</summary>
    constexpr double DEGREES_TO_RADIANS = 0x1.1df46a2526c7ap-6;
    /// <summary>Radians to degrees, as MCX.EXE stores it.</summary>
    constexpr double RADIANS_TO_DEGREES = 0x1.ca5dc1a6402aap+5;
    /// <summary>Sixty degrees in radians, as MCX.EXE stores it.</summary>
    constexpr double SIXTY_DEGREES = 0x1.0c152382d45b2p+0;
} // namespace

//---------------------------------------------------------------------------
// TerrainObjectType
//---------------------------------------------------------------------------

auto MCTerrainObjectType::Init() -> void
{
    MCObjectType::Init();
    DmgLevel = 0;
    CollisionOffsetY = 0;
    CollisionOffsetX = 0;
    BasePixelOffsetY = 0;
    BasePixelOffsetX = 0;
    SetImpassable = 0;
    YImpasse = 0;
    XImpasse = 0;
    ExplDmg = 0.0f;
    ExplRad = 0.0f;
}

auto MCTerrainObjectType::CreateInstance() -> MCBaseObject*
{
    auto* newObject = new MCTerrainObject;

    if (newObject == nullptr)
    {
        return nullptr;
    }

    if (newObject->Init(this) != 0)
    {
        return nullptr;
    }

    newObject->IdNumber = NextIdNumber++;
    return newObject;
}

auto MCTerrainObjectType::Destroy() -> void
{
}

auto MCTerrainObjectType::Init(MCFile* objFile, uint32_t fileSize) -> int32_t
{
    MCFitIniFile objectFile;
    int32_t result = objectFile.Open(objFile, fileSize);

    if (result != 0)
    {
        return result;
    }

    if ((result = objectFile.SeekBlock("TerrainObjectData")) != 0)
    {
        return result;
    }

    if ((result = objectFile.ReadIdULong("DmgLevel", DmgLevel)) != 0)
    {
        return result;
    }

    if (objectFile.ReadIdLong("BasePixelOffsetX", BasePixelOffsetX) != 0)
    {
        BasePixelOffsetX = 0;
    }

    if (objectFile.ReadIdLong("BasePixelOffsetY", BasePixelOffsetY) != 0)
    {
        BasePixelOffsetY = 0;
    }

    if (objectFile.ReadIdLong("CollisionOffsetX", CollisionOffsetX) != 0)
    {
        CollisionOffsetX = 0;
    }

    if (objectFile.ReadIdLong("CollisionOffsetY", CollisionOffsetY) != 0)
    {
        CollisionOffsetY = 0;
    }

    if (objectFile.ReadIdLong("SetImpassable", SetImpassable) != 0)
    {
        SetImpassable = 0;
    }

    if (objectFile.ReadIdLong("XImpasse", XImpasse) != 0)
    {
        XImpasse = 0;
    }

    if (objectFile.ReadIdLong("YImpasse", YImpasse) != 0)
    {
        YImpasse = 0;
    }

    // No ExtentRadius: -1, measured from the appearance by the first object's update.
    float radius = 0.0f;

    if (objectFile.ReadIdFloat("ExtentRadius", radius) != 0)
    {
        radius = -1.0f;
    }

    if (objectFile.ReadIdFloat("ExplosionRadius", ExplRad) != 0)
    {
        ExplRad = 0.0f;
    }

    if (objectFile.ReadIdFloat("ExplosionDamage", ExplDmg) != 0)
    {
        ExplDmg = 0.0f;
    }

    result = MCObjectType::Init(&objectFile);
    ExtentRadius = radius;
    return result;
}

auto MCTerrainObjectType::HandleCollision(MCGameObject* collidee, MCGameObject* collider) -> int
{
    // A mover (not artillery) running into it deals it 10 points; the server's job in multiplayer.
    if ((MPlayer == nullptr || MPlayer->IsServer != 0) && collider->ObjectClass < MOVER &&
        collider->ObjectClass != ARTILLERY)
    {
        MCWeaponShotInfo shot;
        shot.Init(nullptr, -1, 10.0f, 0, 0.0f);
        collidee->HandleWeaponHit(&shot, MPlayer != nullptr ? 1 : 0);
    }

    return 1;
}

auto MCTerrainObjectType::HandleDestruction(MCGameObject*, MCGameObject*) -> int
{
    return 0;
}

//---------------------------------------------------------------------------
// TerrainObject
//---------------------------------------------------------------------------

MCTerrainObject::MCTerrainObject()
{
    JustCreated = 1;
    Appearance = nullptr;
    PixelOffsetY = 0;
    PixelOffsetX = 0;
    VertexNumber = 0;
    BlockNumber = 0;
    FireObject = nullptr;
    Burning = 0;
}

auto MCTerrainObject::Init() -> void
{
}

auto MCTerrainObject::SetTerrainPosition(MCVector2D& offset, MCVector2D& numbers) -> void
{
    PixelOffsetX = static_cast<int32_t>(offset.X);
    PixelOffsetY = static_cast<int32_t>(offset.Y);
    const auto* objectType = static_cast<MCTerrainObjectType*>(ObjType);

    if (objectType->BasePixelOffsetX != 0)
    {
        PixelOffsetX = objectType->BasePixelOffsetX;
    }

    if (objectType->BasePixelOffsetY != 0)
    {
        PixelOffsetY = objectType->BasePixelOffsetY;
    }

    VertexNumber = static_cast<int32_t>(numbers.X);
    BlockNumber = static_cast<int32_t>(numbers.Y);
}

auto MCTerrainObject::GetBlockAndVertexNumber(int32_t& blockNum, int32_t& vertexNum) -> void
{
    blockNum = BlockNumber;
    vertexNum = VertexNumber;
}

auto MCTerrainObject::IsVisible(MCCamera* cam) -> int
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

    WindowsVisible = Turn;
    return 1;
}

auto MCTerrainObject::Update() -> int32_t
{
    if (JustCreated == 0)
    {
        return 1;
    }

    // Set the object on its vertex: the block's corner, the vertex within it, then the pixel offset within the
    // tile (turned into the isometric grid's 60-degree axes). The type's collision offset replaces the pixel offset.
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
    const auto* objectType = static_cast<MCTerrainObjectType*>(ObjType);
    int32_t offsetXPixels = PixelOffsetX;

    if (objectType->CollisionOffsetX != 0)
    {
        offsetXPixels = objectType->CollisionOffsetX;
    }

    int32_t offsetYPixels = PixelOffsetY;

    if (objectType->CollisionOffsetY != 0)
    {
        offsetYPixels = objectType->CollisionOffsetY;
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
    Assert(inBounds(), 0, " terrobj MapTile Out of Bounds ");
    Assert(inBounds(), 0, " Map Tile out of bounds ");
    const MCMapTile& tile = GameMap->Map[GameMap->Width * CellRow + CellColumn];
    const int32_t elevationLevel = static_cast<int32_t>((tile.Cells >> 7) & 0x3f) + GameMap->BaseElevation;
    CellElevation = static_cast<float>(elevationLevel) * MCTerrain::MetersPerElevLevel;

    // No extent radius in the FIT: measure it (twice the appearance's diagonal) for the whole type.
    if (ObjType->ExtentRadius < 0.0f)
    {
        Appearance->Visible = 1;
        Appearance->Update();
        Appearance->RecalcBounds(Eye);
        const float dx = Appearance->UpperLeft.X - Appearance->LowerRight.X;
        const float dy = Appearance->UpperLeft.Y - Appearance->LowerRight.Y;
        float radius = std::sqrt(dx * dx + dy * dy) / WorldUnitsPerMeter;
        radius = radius + radius;

        if (static_cast<float>(MCCollisionSystem::GridRadius) < radius)
        {
            Fatal(static_cast<int32_t>(std::floor(static_cast<double>(radius))), " Object extent radius TOO large ");
        }

        ObjType->ExtentRadius = radius;
    }

    return 1;
}

auto MCTerrainObject::HandleEvent(MCObjectEvent* event) -> int32_t
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

auto MCTerrainObject::Render() -> void
{
    if (JustCreated != 0)
    {
        return;
    }

    const int visibleNow = IsVisible(Eye);

    if (Appearance != nullptr)
    {
        Appearance->Visible = visibleNow;
        Appearance->Update();
    }

    if (WindowsVisible != Turn)
    {
        return;
    }

    // Hazed by how many corners of its vertex square the home team sees; drawn when any is. (The original also
    // reads each corner's seen bit and drops it.)
    const auto row = static_cast<uint32_t>(CellRow);
    const auto col = static_cast<uint32_t>(CellColumn);
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

    static_cast<MCVfxAppearance*>(Appearance)->FadeTable = hazePalette;

    if (JustCreated == 0 && numVisible != 0)
    {
        Appearance->Render(0);

        if (FireObject != nullptr)
        {
            FireObject->Render();
        }
    }

    if (DrawExtents != 0)
    {
        // Debug: the extent radius as an ellipse.
        MCVector2D size;
        size.X = Eye->CosAngle * ObjType->ExtentRadius;
        size.Y = Eye->SinAngle * ObjType->ExtentRadius;

        if (Eye->CameraScale == 1)
        {
            size.X *= 0.5f;
            size.Y *= 0.5f;
        }

        const float scale = Eye->CameraScale != 1 ? 1.0f : 0.5f;
        const float sx = (Position.X - Eye->Position.X) * scale;
        const float sy = (Position.Y - Eye->Position.Y) * scale;
        MCVector2D center;
        center.X = sx * Eye->CosAngle + sy * Eye->CosAngle + Eye->HalfWidth;
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

auto MCTerrainObject::Destroy() -> void
{
    delete Appearance;
    Appearance = nullptr;
}

auto MCTerrainObject::SetDamage(int32_t newDamage) -> void
{
    Damage = static_cast<float>(newDamage);
}

auto MCTerrainObject::Init(MCObjectType* objType) -> int32_t
{
    int32_t result = MCGameObject::Init(objType);

    if (result != 0)
    {
        return result;
    }

    JustCreated = 1;
    MCAppearanceType* apprType = AppearanceTypeList->GetAppearance(objType->AppearName, 0);

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

    ObjectClass = TERRAINOBJECT;

    if (0.0f < this->ObjType->ExtentRadius)
    {
        CollisionsOn = 1;
    }

    // A DmgLevel of 0 means it starts destroyed.
    if (static_cast<MCTerrainObjectType*>(this->ObjType)->DmgLevel == 0)
    {
        CollisionsOn = 0;
        Status = 2;
    }

    return 0;
}

auto MCTerrainObject::LightOnFire(float timeToBurn) -> void
{
    if (FireObject == nullptr && ObjType->ExplosionObject != -1)
    {
        auto* fire = static_cast<MCFire*>(CreateObject(ObjType->ExplosionObject));

        if (fire != nullptr)
        {
            FireObject = fire;
            fire->SetPotentialContact(3);
            FireObject->BurningObject = this;
            FireObject->SetTonnage(40.0f);
            FireObject->SetPosition(Position);
        }
    }

    if (FireObject != nullptr)
    {
        FireObject->AddTimeLeftToBurn(timeToBurn);
        Burning = 1;
    }
}

auto MCTerrainObject::HandleWeaponHit(MCWeaponShotInfo* shotInfo, int addMultiplayChunk) -> int32_t
{
    if (shotInfo == nullptr)
    {
        return 0;
    }

    if (addMultiplayChunk != 0)
    {
        MPlayer->AddWeaponHitChunk(this, shotInfo, 0);
    }

    double newDamage = static_cast<double>(GetDamage()) + shotInfo->Damage;
    const auto maxDamage =
        static_cast<float>(static_cast<int32_t>(static_cast<MCTerrainObjectType*>(ObjType)->DmgLevel));

    if (maxDamage < newDamage)
    {
        newDamage = maxDamage;
    }

    SetDamage(static_cast<int32_t>(newDamage));

    // A hit sets it burning (10 seconds), or keeps a fire going (2 more).
    if (Burning == 0)
    {
        if (ObjType->ExplosionObject != -1)
        {
            LightOnFire(10.0f);
        }
    }
    else
    {
        LightOnFire(2.0f);
    }

    return 0;
}
