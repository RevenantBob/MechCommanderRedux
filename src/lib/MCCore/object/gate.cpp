#include "stdafx.h"
#include "object/gate.h"
#include "ai/move.h"
#include "appear/MCAppearanceType.h"
#include "appear/MCAppearanceTypeList.h"
#include "camera/camera.h"
#include "color/MCPalette.h"
#include "engine/MCByteFlag.h"
#include "engine/MCElementBuffer.h"
#include "engine/MCEllipseElement.h"
#include "gui/asystem.h"
#include "lib/MCFatal.h"
#include "lib/MCFrameOfRef.h"
#include "lib/MCVector2D.h"
#include "lib/MCDice.h"
#include "lib/MCFile.h"
#include "lib/MCFitIniFile.h"
#include "logistics/logmain.h"
#include "main/main.h"
#include "network/multplyr.h"
#include "object/bldng.h"
#include "object/fire.h"
#include "object/mover.h"
#include "object/object.h"
#include "object/objevnt.h"
#include "object/objque.h"
#include "object/team.h"
#include "sound/soundsys.h"
#include "sprite/MCPUAppearance.h"
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

    /// <summary>The two shapes of gate: their object type ids pick which overlay types they set on the map.</summary>
    enum class MCGateKind
    {
        None,
        /// <summary>Types 0x284, 0x287, 0x2b2, 0x2b3, 0x403, 0x405.</summary>
        KindA,
        /// <summary>Types 0x285, 0x286, 0x2b4, 0x2b5, 0x404, 0x406.</summary>
        KindB
    };

    MCGateKind GateKind(int32_t objTypeNum)
    {
        switch (objTypeNum)
        {
            case 0x284:
            case 0x287:
            case 0x2b2:
            case 0x2b3:
            case 0x403:
            case 0x405:
                return MCGateKind::KindA;
            case 0x285:
            case 0x286:
            case 0x2b4:
            case 0x2b5:
            case 0x404:
            case 0x406:
                return MCGateKind::KindB;
            default:
                return MCGateKind::None;
        }
    }

    /// <summary>The map row and column of a gate's terrain vertex (used as its tile).</summary>
    void VertexRowCol(const MCGate* gate, int32_t& row, int32_t& col)
    {
        col = (gate->BlockNumber % MCTerrain::BlocksMapSide) * MCTerrain::VerticesBlockSide +
              gate->VertexNumber % MCTerrain::VerticesBlockSide;
        row = (gate->BlockNumber / MCTerrain::BlocksMapSide) * MCTerrain::VerticesBlockSide +
              gate->VertexNumber / MCTerrain::VerticesBlockSide;
    }

    /// <summary>
    /// Sets the gate tile's overlay type (the masked form MCX.EXE writes) and each of its nine cells' passable and
    /// line-of-sight bits.
    /// </summary>
    void SetGateTile(int32_t row, int32_t col, uint32_t keepMask, uint32_t overlayBits, uint32_t lineOfSight,
                     uint32_t passable)
    {
        MCMapTile& tile = GameMap->Map[GameMap->Width * row + col];
        tile.Overlay = (tile.Overlay & keepMask) | overlayBits;

        for (uint32_t shift = 0; shift < 0x12; shift += 2)
        {
            tile.Cells = (lineOfSight << (shift + 0xf)) | (~(0x8000u << shift) & tile.Cells);
            tile.Cells = (~(0x4000u << shift) & tile.Cells) | (passable << (shift + 0xe));
        }
    }

    /// <summary>
    /// Takes the pop-up appearance's answer to a combat mode request: open (2), opening (1, with the opening sound
    /// when it was closed), closing (3, with the closing sound when it was open) or closed (0).
    /// </summary>
    /// <returns>True when the gate is now closed.</returns>
    bool TakeGateState(MCGate* gate, int32_t state)
    {
        switch (state)
        {
            case 2:
            {
                gate->IsOpen = 1;
                gate->IsOpening = 0;
                gate->IsClosing = 0;
                gate->IsClosed = 0;
                return false;
            }
            case 1:
            {
                if (gate->IsClosed != 0)
                {
                    SoundSystem->PlayDigitalSample(0x2a, 1, gate, 0, 0);
                }

                gate->IsOpening = 1;
                gate->IsOpen = 0;
                gate->IsClosing = 0;
                gate->IsClosed = 0;
                return false;
            }
            case 3:
            {
                if (gate->IsOpen != 0)
                {
                    SoundSystem->PlayDigitalSample(0x2d, 1, gate, 0, 0);
                }

                gate->IsClosing = 1;
                gate->IsOpen = 0;
                gate->IsOpening = 0;
                gate->IsClosed = 0;
                return false;
            }
            case 0:
            {
                gate->IsClosed = 1;
                gate->IsOpen = 0;
                gate->IsOpening = 0;
                gate->IsClosing = 0;
                return true;
            }
            default:
                return false;
        }
    }

    /// <summary>Makes the gate's fire (the type's blown effect); anything that isn't a fire goes in the object list.</summary>
    void StartFire(MCGate* gate, bool listNonFire)
    {
        MCGameObject* effect =
            CreateObject(static_cast<int32_t>(static_cast<MCGateType*>(gate->ObjType)->BlownEffectId));

        if (effect == nullptr)
        {
            return;
        }

        effect->SetPosition(gate->Position);

        if (effect->ObjectClass == FIRE)
        {
            gate->FireObject = static_cast<MCFire*>(effect);
            effect->SetPotentialContact(3);
            gate->FireObject->BurningObject = gate;
            gate->FireObject->SetTonnage(40.0f);

            if (listNonFire)
            {
                gate->FireObject->Update();
                gate->FireStarted = 1;
            }
        }
        else if (listNonFire)
        {
            if (ObjectList->Head != nullptr)
            {
                ObjectList->Head->AddNode(effect);
            }
        }
        else
        {
            DestroyObject(effect);
        }
    }
} // namespace

//---------------------------------------------------------------------------
// GateType
//---------------------------------------------------------------------------

auto MCGateType::Init() -> void
{
    MCObjectType::Init();
    DmgLevel = 0;
    BlownEffectId = 0xffffffff;
    NormalEffectId = 0xffffffff;
    DamageEffectId = 0xffffffff;
    ExplosionRadius = 0.0f;
    ExplosionDamage = 0.0f;
    BuildingName = 0;
}

auto MCGateType::CreateInstance() -> MCBaseObject*
{
    auto* newGate = new MCGate;

    if (newGate == nullptr)
    {
        return nullptr;
    }

    if (newGate->Init(this) != 0)
    {
        return nullptr;
    }

    newGate->IdNumber = NextIdNumber++;
    return newGate;
}

auto MCGateType::Destroy() -> void
{
}

auto MCGateType::Init(MCFile* objFile, uint32_t fileSize) -> int32_t
{
    MCFitIniFile gateFile;
    int32_t result = gateFile.Open(objFile, fileSize);

    if (result != 0)
    {
        return result;
    }

    if ((result = gateFile.SeekBlock("GateData")) != 0)
    {
        return result;
    }

    if ((result = gateFile.ReadIdULong("DmgLevel", DmgLevel)) != 0)
    {
        return result;
    }

    gateFile.ReadIdULong("BlownEffectId", BlownEffectId);
    gateFile.ReadIdULong("NormalEffectId", NormalEffectId);
    gateFile.ReadIdULong("DamageEffectId", DamageEffectId);

    if (gateFile.ReadIdLong("BasePixelOffsetX", BasePixelOffsetX) != 0)
    {
        BasePixelOffsetX = 0;
    }

    if (gateFile.ReadIdLong("BasePixelOffsetY", BasePixelOffsetY) != 0)
    {
        BasePixelOffsetY = 0;
    }

    if (gateFile.ReadIdFloat("ExplosionRadius", ExplosionRadius) != 0)
    {
        ExplosionRadius = 0.0f;
    }

    if (gateFile.ReadIdFloat("ExplosionDamage", ExplosionDamage) != 0)
    {
        ExplosionDamage = 0.0f;
    }

    if ((result = gateFile.ReadIdFloat("OpenRadius", OpenRadius)) != 0)
    {
        return result;
    }

    if (gateFile.ReadIdFloat("LittleExtent", LittleExtent) != 0)
    {
        LittleExtent = 20.0f;
    }

    if (gateFile.ReadIdLong("BuildingName", BuildingName) != 0)
    {
        BuildingName = 0xa5;
    }

    if (gateFile.ReadIdBoolean("BlocksLineOfFire", BlocksLineOfFire) != 0)
    {
        BlocksLineOfFire = 0;
    }

    return MCObjectType::Init(&gateFile);
}

auto MCGateType::HandleCollision(MCGameObject* collidee, MCGameObject* collider) -> int
{
    // Only mechs, vehicles and elementals open gates or get caught in them.
    if (collider->ObjectClass < BATTLEMECH || EXPLOSION <= collider->ObjectClass)
    {
        return 1;
    }

    auto* gate = static_cast<MCGate*>(collidee);
    const MCVector3D colliderPos = collider->GetPosition();
    const MCVector3D gatePos = gate->GetPosition();
    const float distanceSq = (gatePos.X - colliderPos.X) * (gatePos.X - colliderPos.X) +
                             (gatePos.Y - colliderPos.Y) * (gatePos.Y - colliderPos.Y);

    // A friendly (or, for a neutral gate, any) live unit within openRadius asks it to open.
    if ((collider->GetAlignment() == gate->GetAlignment() || gate->GetAlignment() == 0) &&
        collider->IsDisabled() == 0 && collider->IsDestroyed() == 0 && distanceSq < OpenRadius * OpenRadius)
    {
        gate->OpenRequested = 1;
    }

    // A live unit standing in it (not jumping) is what a closing gate crushes.
    if (distanceSq < 1.2e8f && collider->IsDisabled() == 0 && collider->IsDestroyed() == 0 &&
        static_cast<MCMover*>(collider)->IsJumping(nullptr) == 0)
    {
        gate->OffendingObject = collider;
    }

    return 1;
}

auto MCGateType::HandleDestruction(MCGameObject*, MCGameObject*) -> int
{
    return 0;
}

//---------------------------------------------------------------------------
// Gate
//---------------------------------------------------------------------------

MCGate::MCGate()
{
    JustCreated = 1;
    OpenRequested = 1;
    IsClosed = 1;
    Appearance = nullptr;
    VertexNumber = 0;
    BlockNumber = 0;
    FireStarted = 0;
    Destroyed = 0;
    FireObject = nullptr;
    Name.clear();
    LockedClosed = 0;
    BlownOpen = 0;
    IsOpen = 0;
    IsOpening = 0;
    IsClosing = 0;
    Destroying = 0;
    OffendingObject = nullptr;
}

auto MCGate::Init() -> void
{
}

auto MCGate::KillFireObject() -> void
{
    FireObject = nullptr;
}

auto MCGate::SetTerrainPosition(MCVector2D& pixelOffset, MCVector2D& blockVertex) -> void
{
    PixelOffsetX = static_cast<int32_t>(pixelOffset.X);
    PixelOffsetY = static_cast<int32_t>(pixelOffset.Y);
    const auto* gateType = static_cast<MCGateType*>(ObjType);

    if (gateType->BasePixelOffsetX != 0 || gateType->BasePixelOffsetY != 0)
    {
        PixelOffsetX = gateType->BasePixelOffsetX;
        PixelOffsetY = gateType->BasePixelOffsetY;
    }

    VertexNumber = static_cast<int32_t>(blockVertex.X);
    BlockNumber = static_cast<int32_t>(blockVertex.Y);
}

auto MCGate::GetBlockAndVertexNumber(int32_t& blockNum, int32_t& vertexNum) -> void
{
    blockNum = BlockNumber;
    vertexNum = VertexNumber;
}

auto MCGate::IsVisible(MCCamera* cam) -> int
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

auto MCGate::Update() -> int32_t
{
    if (JustCreated != 0)
    {
        // Set the gate on its vertex: the block's corner, the vertex within it, then the pixel offset within the
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

        TileCol = (BlockNumber % MCTerrain::BlocksMapSide) * verticesBlockSide + VertexNumber % verticesBlockSide;
        const int32_t halfMap = (verticesBlockSide * MCTerrain::BlocksMapSide) >> 1;
        TileWorldX = static_cast<float>(TileCol - halfMap) * MCTerrain::MetersPerVertex;
        TileRow = VertexNumber / verticesBlockSide + (BlockNumber / MCTerrain::BlocksMapSide) * verticesBlockSide;
        TileWorldY = static_cast<float>(halfMap - TileRow) * MCTerrain::MetersPerVertex;
        const auto inBounds = [&]
        { return TileRow < 0 || GameMap->Height <= TileRow || TileCol < 0 || GameMap->Width <= TileCol ? 0u : 1u; };
        Assert(inBounds(), 0, " tbldg MapTile Out of Bounds ");
        Assert(inBounds(), 0, " Map Tile out of bounds ");
        const MCMapTile& tile = GameMap->Map[GameMap->Width * TileRow + TileCol];
        const int32_t elevationLevel = static_cast<int32_t>((tile.Cells >> 7) & 0x3f) + GameMap->BaseElevation;
        Appearance->Visible = 1;
        TileElevation = static_cast<float>(elevationLevel) * MCTerrain::MetersPerElevLevel;
        Appearance->Update();
        Appearance->RecalcBounds(Eye);
    }

    if (Destroyed == 0)
    {
        OpenGate();
        OffendingObject = nullptr;
    }

    return 1;
}

auto MCGate::BlowAnyOffendingObject() -> void
{
    // Whatever is caught in a gate as it shuts takes 10 hits of 250, and the gate is destroyed.
    if (MPlayer != nullptr && MPlayer->IsServer == 0)
    {
        return;
    }

    MCGameObject* offender = OffendingObject;

    if (offender == nullptr)
    {
        return;
    }

    const MCVector3D offenderPos = offender->GetPosition();
    const MCVector3D gatePos = GetPosition();
    const float dx = gatePos.X - offenderPos.X;
    const float dy = gatePos.Y - offenderPos.Y;
    const double reach =
        static_cast<double>(static_cast<MCGateType*>(ObjType)->LittleExtent) + offender->GetObjectType()->ExtentRadius;

    if (reach * reach <= static_cast<double>(dy) * dy + static_cast<double>(dx) * dx)
    {
        return;
    }

    const int multiplayer = MPlayer != nullptr ? 1 : 0;
    MCWeaponShotInfo shot;
    shot.Init(nullptr, -3, 250.0f, 0, 0.0f);

    for (int32_t i = 0; i < 10; i++)
    {
        shot.HitLocation = offender->CalcHitLocation(nullptr, -1, 4, 0);
        offender->HandleWeaponHit(&shot, multiplayer);
    }

    const auto gateDamage = static_cast<float>(static_cast<int32_t>(static_cast<MCGateType*>(ObjType)->DmgLevel + 5));
    shot.Init(nullptr, -3, gateDamage, 0, 0.0f);
    HandleWeaponHit(&shot, multiplayer);
}

auto MCGate::OpenGate() -> void
{
    if (Destroyed != 0)
    {
        return;
    }

    int32_t row;
    int32_t col;
    VertexRowCol(this, row, col);

    // Ask the pop-up appearance to open or shut: locked (or forced) gates shut; blown open or neutral gates open;
    // others follow openRequested. A gate that has just shut crushes whatever is in it (unless blown or neutral).
    if (LockedClosed == 0 && ForceGatesClosed == 0)
    {
        if (BlownOpen == 0 && Alignment != 0)
        {
            if (TakeGateState(this, static_cast<MCPUAppearance*>(Appearance)->SetCombatMode(OpenRequested)))
            {
                BlowAnyOffendingObject();
            }
        }
        else
        {
            TakeGateState(this, static_cast<MCPUAppearance*>(Appearance)->SetCombatMode(1));
        }
    }
    else if (TakeGateState(this, static_cast<MCPUAppearance*>(Appearance)->SetCombatMode(0)))
    {
        BlowAnyOffendingObject();
    }

    // Open: the tile is passable and see-through. Shut: blocked (unless locked open) and see-through only when the
    // type doesn't block line of fire. The overlay type depends on the gate's shape and whose it is.
    const MCGateKind kind = GateKind(GetObjectType()->ObjTypeNum);

    if (IsOpen != 0)
    {
        const bool clan = Alignment != 1 && Alignment != 0;

        if (kind == MCGateKind::KindA)
        {
            if (clan)
            {
                SetGateTile(row, col, 0xffffffc5, 0x45, 1, 1);
            }
            else
            {
                SetGateTile(row, col, 0xffffffc9, 0x49, 1, 1);
            }
        }
        else if (kind == MCGateKind::KindB)
        {
            if (clan)
            {
                SetGateTile(row, col, 0xffffffc3, 0x43, 1, 1);
            }
            else
            {
                SetGateTile(row, col, 0xffffffc7, 0x47, 1, 1);
            }
        }

        OpenRequested = 0;
        return;
    }

    const uint32_t lineOfSight = static_cast<MCGateType*>(ObjType)->BlocksLineOfFire == 0 ? 1 : 0;
    const uint32_t passable = LockedClosed == 0 ? 1 : 0;

    if (kind == MCGateKind::KindA)
    {
        if (Alignment == 1)
        {
            SetGateTile(row, col, 0xffffffca, 0x4a, lineOfSight, passable);
        }
        else
        {
            SetGateTile(row, col, 0xffffffc6, 0x46, lineOfSight, passable);
        }
    }
    else if (kind == MCGateKind::KindB)
    {
        if (Alignment == 1)
        {
            SetGateTile(row, col, 0xffffffc8, 0x48, lineOfSight, passable);
        }
        else
        {
            SetGateTile(row, col, 0xffffffc4, 0x44, lineOfSight, passable);
        }
    }

    OpenRequested = 0;
}

auto MCGate::SetAlignment(int32_t newAlignment) -> void
{
    MCBigGameObject::SetAlignment(newAlignment);
}

auto MCGate::HandleEvent(MCObjectEvent* event) -> int32_t
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

auto MCGate::LightOnFire(float timeToBurn) -> void
{
    // A gate with no blown effect just takes a point of damage (the server's job in multiplayer).
    if (static_cast<int32_t>(static_cast<MCGateType*>(ObjType)->BlownEffectId) == -1)
    {
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
        StartFire(this, false);
    }

    if (FireObject != nullptr)
    {
        FireObject->AddTimeLeftToBurn(timeToBurn);
        FireStarted = 1;
    }
}

auto MCGate::IsRevealed() -> int
{
    MCByteFlag* visibleBits = HomeTeam->Alignment == -1 ? MCTerrain::ClanVisibleBits : MCTerrain::TerrainVisibleBits;
    int32_t row;
    int32_t col;
    VertexRowCol(this, row, col);
    const auto r = static_cast<uint32_t>(row);
    const auto c = static_cast<uint32_t>(col);

    if (visibleBits->GetFlag(r, c) != 0)
    {
        return 1;
    }

    if (visibleBits->GetFlag(r + 1, c) != 0)
    {
        return 1;
    }

    if (visibleBits->GetFlag(r + 1, c + 1) != 0)
    {
        return 1;
    }

    return visibleBits->GetFlag(r, c + 1) != 0 ? 1 : 0;
}

auto MCGate::Render() -> void
{
    if (JustCreated != 0)
    {
        return;
    }

    if (Appearance != nullptr)
    {
        Appearance->Visible = IsVisible(Eye);
        Appearance->Update();
    }

    if (WindowsVisible != Turn)
    {
        return;
    }

    // Hazed by how many corners of its vertex square the home team sees; drawn when any is. (The original also
    // reads each corner's seen bit and drops it.)
    int32_t row;
    int32_t col;
    VertexRowCol(this, row, col);
    const auto r = static_cast<uint32_t>(row);
    const auto c = static_cast<uint32_t>(col);
    MCByteFlag* visibleBits = HomeTeam->Alignment == -1 ? MCTerrain::ClanVisibleBits : MCTerrain::TerrainVisibleBits;
    int32_t numVisible = 0;

    if (visibleBits->GetFlag(r, c) != 0)
    {
        numVisible = 1;
    }

    if (visibleBits->GetFlag(r + 1, c) != 0)
    {
        numVisible++;
    }

    if (visibleBits->GetFlag(r + 1, c + 1) != 0)
    {
        numVisible++;
    }

    if (visibleBits->GetFlag(r, c + 1) != 0)
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

    static_cast<MCPUAppearance*>(Appearance)->HazePalette = hazePalette;

    if (numVisible != 0)
    {
        Appearance->Render(0);
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

auto MCGate::Destroy() -> void
{
    delete Appearance;
    Appearance = nullptr;
    Name.clear();
}

auto MCGate::Init(MCObjectType* objType) -> int32_t
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

    auto* popUpAppearance = new MCPUAppearance;
    Appearance = popUpAppearance;

    if (popUpAppearance == nullptr)
    {
        return -0x2ffff;
    }

    popUpAppearance->Init(nullptr, nullptr);

    if ((apprType->AppearanceNum & 0xff000000) != 0x9000000)
    {
        return -0x2fff6;
    }

    if ((result = popUpAppearance->Init(apprType, this)) != 0)
    {
        return result;
    }

    auto* gateType = static_cast<MCGateType*>(this->ObjType);
    ObjectClass = GATE;
    Destroyed = 0;
    Alignment = -1;

    // The open radius doubles as the extent the collision system checks.
    if (gateType->OpenRadius != 0.0f)
    {
        gateType->ExtentRadius = gateType->OpenRadius;
    }

    if (0.0f < gateType->ExtentRadius)
    {
        CollisionsOn = 1;
    }

    ExplDamage = gateType->ExplosionDamage;
    ExplRadius = gateType->ExplosionRadius;
    char nameBuffer[256];
    CLoadString(ThisInstance, static_cast<uint32_t>(gateType->BuildingName), nameBuffer, 0xfe);
    Name = nameBuffer;
    return 0;
}

auto MCGate::HandleWeaponHit(MCWeaponShotInfo* shotInfo, int addMultiplayChunk) -> int32_t
{
    if (shotInfo == nullptr)
    {
        return 0;
    }

    if (addMultiplayChunk != 0)
    {
        MPlayer->AddWeaponHitChunk(this, shotInfo, 0);
    }

    const float newDamage = GetDamage() + shotInfo->Damage;
    SetDamage(newDamage);

    if (static_cast<float>(static_cast<int32_t>(static_cast<MCGateType*>(ObjType)->DmgLevel)) <= newDamage)
    {
        DestroyGate(0);
    }

    return 0;
}

auto MCGate::DestroyGate(int fromNetwork) -> void
{
    Destroyed = 1;
    Destroying = 1;
    auto* popUpAppearance = static_cast<MCPUAppearance*>(Appearance);

    if (popUpAppearance != nullptr)
    {
        popUpAppearance->Visible = OnScreen();
        popUpAppearance->Update();
    }

    // Port fix: the original calls setDestroyed on a null appearance.
    if (popUpAppearance != nullptr)
    {
        popUpAppearance->SetDestroyed();
    }

    // Blown open: the tile is passable and see-through for good (the network copy already got the map change).
    int32_t row;
    int32_t col;
    VertexRowCol(this, row, col);

    if (fromNetwork == 0)
    {
        const MCGateKind kind = GateKind(GetObjectType()->ObjTypeNum);

        if (kind == MCGateKind::KindA)
        {
            if (Alignment == 1)
            {
                SetGateTile(row, col, 0xffffffc9, 0x49, 1, 1);
            }
            else
            {
                SetGateTile(row, col, 0xffffffc5, 0x45, 1, 1);
            }
        }
        else if (kind == MCGateKind::KindB)
        {
            if (Alignment == 1)
            {
                SetGateTile(row, col, 0xffffffc7, 0x47, 1, 1);
            }
            else
            {
                SetGateTile(row, col, 0xffffffc3, 0x43, 1, 1);
            }
        }
    }

    IsOpen = 1;
    IsOpening = 0;
    IsClosing = 0;
    IsClosed = 0;
    BlownOpen = 1;
    Destroying = 0;
    CollisionsOn = 0;
    Status = 2;

    // Set it burning (or keep a fire going) and blow it up.
    if (FireStarted == 0 && fromNetwork == 0)
    {
        if (static_cast<int32_t>(static_cast<MCGateType*>(ObjType)->BlownEffectId) == -1)
        {
            if (FireObject != nullptr)
            {
                FireObject->AddTimeLeftToBurn(2.0f);
            }
        }
        else
        {
            StartFire(this, true);
        }

        ObjType->CreateExplosion(Position, ExplDamage, ExplRadius);
    }
}
