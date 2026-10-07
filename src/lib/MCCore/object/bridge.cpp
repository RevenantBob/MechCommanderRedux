#include "stdafx.h"
#include "object/bridge.h"
#include "ai/move.h"
#include "camera/camera.h"
#include "color/color.h"
#include "engine/bitflag.h"
#include "engine/ceglist.h"
#include "engine/cepoly.h"
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
#include "network/multplyr.h"
#include "object/fire.h"
#include "object/object.h"
#include "object/objevnt.h"
#include "object/team.h"
#include "sound/soundsys.h"
#include "terrain/terrain.h"
#include "terrain/terrmap.h"
#include "terrain/terrtxm.h"
#include "platform/MCRenderer.h"

namespace
{
    /// <summary>Degrees to radians, as MCX.EXE stores it (a hair under pi / 180).</summary>
    constexpr double DEGREES_TO_RADIANS = 0x1.1df46a2526c7ap-6;
    /// <summary>Radians to degrees, as MCX.EXE stores it.</summary>
    constexpr double RADIANS_TO_DEGREES = 0x1.ca5dc1a6402aap+5;
    /// <summary>Sixty degrees in radians, as MCX.EXE stores it.</summary>
    constexpr double SIXTY_DEGREES = 0x1.0c152382d45b2p+0;

    /// <summary>What a misc terrain object is (terrainObjectKind, set by the placement code).</summary>
    enum MCMiscTerrainKind : int32_t
    {
        MISC_BRIDGE = 5,
        MISC_FOREST = 6,
        MISC_WALL = 7,
        MISC_MEDIUM_WALL = 8,
        MISC_LIGHT_WALL = 9
    };

    /// <summary>The map tile under the object's position.</summary>
    MCMapTile& TileAt(MCMiscTerrainObject* object, int32_t& tileR, int32_t& tileC)
    {
        int32_t cellR = 0;
        int32_t cellC = 0;
        tileR = 0;
        tileC = 0;
        GameMap->WorldToMapPos(object->Position, tileR, tileC, cellR, cellC);
        return GameMap->Map[GameMap->Width * tileR + tileC];
    }

    /// <summary>How many of the tile's nine cells are passable.</summary>
    int32_t CountPassable(const MCMapTile& tile)
    {
        int32_t count = 0;

        for (uint32_t shift = 0; shift < 0x12; shift += 2)
        {
            count += static_cast<int32_t>((tile.Cells & (0x4000u << shift)) >> (shift + 0xe));
        }

        return count;
    }

    /// <summary>Sets every cell of the tile: passable to <paramref name="passable"/>, see-through.</summary>
    void SetAllCells(MCMapTile& tile, uint32_t passable)
    {
        for (uint32_t shift = 0; shift < 0x12; shift += 2)
        {
            tile.Cells = (passable << (shift + 0xe)) | (~(0x4000u << shift) & tile.Cells);
            tile.Cells = (~(0x8000u << shift) & tile.Cells) | (1u << (shift + 0xf));
        }
    }

    /// <summary>
    /// Makes every cell of a burnt forest tile passable (unless its terrain is type 0x29 or 0x2a) and see-through.
    /// </summary>
    void ClearForestCells(MCMapTile& tile)
    {
        for (uint32_t shift = 0; shift < 0x12; shift += 2)
        {
            if ((tile.Cells & 0x7f) != 0x29 && (tile.Cells & 0x7f) != 0x2a)
            {
                tile.Cells = (1u << (shift + 0xe)) | (~(0x4000u << shift) & tile.Cells);
            }

            tile.Cells = (1u << (shift + 0xf)) | (~(0x8000u << shift) & tile.Cells);
        }
    }

    /// <summary>The damage level that destroys a misc terrain object of <paramref name="kind"/>, or 0.</summary>
    int32_t DmgLevelFor(const MCMiscTerrainObjectType* type, int32_t kind)
    {
        switch (kind)
        {
            case MISC_BRIDGE:
                return static_cast<int32_t>(type->BridgeDmgLevel);
            case MISC_FOREST:
                return static_cast<int32_t>(type->ForestDmgLevel);
            case MISC_WALL:
                return static_cast<int32_t>(type->WallDmgLevel);
            case MISC_MEDIUM_WALL:
                return static_cast<int32_t>(type->MediumWallDmgLevel);
            case MISC_LIGHT_WALL:
                return static_cast<int32_t>(type->LightWallDmgLevel);
            default:
                return 0;
        }
    }
} // namespace

int32_t MCMiscTerrainObject::CellArray[9] = {};

//---------------------------------------------------------------------------
// MiscTerrainObjectType
//---------------------------------------------------------------------------

auto MCMiscTerrainObjectType::Init() -> void
{
    MCObjectType::Init();
    ForestDmgLevel = 0;
    LightWallDmgLevel = 0;
    MediumWallDmgLevel = 0;
    WallDmgLevel = 0;
    BridgeDmgLevel = 0;
    WallFireFX = 0xffffffff;
    BridgeFireFX = 0xffffffff;
    ForestFireFX = 0xffffffff;
    BlownEffectId = 0xffffffff;
    NormalEffectId = 0xffffffff;
    DamageEffectId = 0xffffffff;
    ForestEdgeShapes = nullptr;
}

auto MCMiscTerrainObjectType::CreateInstance() -> MCBaseObject*
{
    auto* newObject = new MCMiscTerrainObject;

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

auto MCMiscTerrainObjectType::Destroy() -> void
{
}

auto MCMiscTerrainObjectType::Init(MCFile* objFile, uint32_t fileSize) -> int32_t
{
    MCFitIniFile bridgeFile;
    int32_t result = bridgeFile.Open(objFile, fileSize);

    if (result != 0)
    {
        return result;
    }

    if ((result = bridgeFile.SeekBlock("BridgeData")) != 0)
    {
        return result;
    }

    if ((result = bridgeFile.ReadIdULong("WallDmgLevel", WallDmgLevel)) != 0)
    {
        return result;
    }

    if (bridgeFile.ReadIdULong("MediumWallDmgLevel", MediumWallDmgLevel) != 0)
    {
        MediumWallDmgLevel = WallDmgLevel >> 1;
    }

    // Original behaviour (OB-021): the light wall default halves the light wall level itself (0 when unread),
    // not the wall's.
    if (bridgeFile.ReadIdULong("LightWallDmgLevel", LightWallDmgLevel) != 0)
    {
        LightWallDmgLevel = LightWallDmgLevel >> 1;
    }

    if ((result = bridgeFile.ReadIdULong("BridgeDmgLevel", BridgeDmgLevel)) != 0)
    {
        return result;
    }

    if ((result = bridgeFile.ReadIdULong("ForestDmgLevel", ForestDmgLevel)) != 0)
    {
        return result;
    }

    if ((result = bridgeFile.ReadIdULong("WallFireFX", WallFireFX)) != 0)
    {
        return result;
    }

    if ((result = bridgeFile.ReadIdULong("BridgeFireFX", BridgeFireFX)) != 0)
    {
        return result;
    }

    if ((result = bridgeFile.ReadIdULong("ForestFireFX", ForestFireFX)) != 0)
    {
        return result;
    }

    if ((result = bridgeFile.ReadIdULong("BlownEffectId", BlownEffectId)) != 0)
    {
        return result;
    }

    if ((result = bridgeFile.ReadIdULong("NormalEffectId", NormalEffectId)) != 0)
    {
        return result;
    }

    if ((result = bridgeFile.ReadIdULong("DamageEffectId", DamageEffectId)) != 0)
    {
        return result;
    }

    // The forest edge shapes (the "x" set for a custom tile set).
    char edgesName[250];

    if ((result = bridgeFile.ReadIdString("ForestEdges", edgesName, 0xf9)) != 0)
    {
        return result;
    }

    if (TerrainTiles->CustomTileSet == 1)
    {
        std::strcat(edgesName, "x");
    }

    std::string edgesPath;
    edgesPath = GamePath(SpritePath, edgesName, ".shp");
    MCFile edgesFile;

    if ((result = edgesFile.Open(edgesPath)) != 0)
    {
        return result;
    }

    uint32_t size = edgesFile.FileSize();
    ForestEdgeShapes = static_cast<uint8_t*>(MCObjectTypeManager::ObjectTypeCache.Allocate(size));
    size = edgesFile.FileSize();
    edgesFile.Read(ForestEdgeShapes, static_cast<int32_t>(size));
    MCRenderer::RegisterData(ForestEdgeShapes, size, MCDataKind::Shapes);
    edgesFile.Close();
    return MCObjectType::Init(&bridgeFile);
}

auto MCMiscTerrainObjectType::HandleCollision(MCGameObject* collidee, MCGameObject* collider) -> int
{
    // A mech or vehicle running into a light wall knocks it down (250 points, the server's job in multiplayer).
    auto* object = static_cast<MCMiscTerrainObject*>(collidee);

    if (object->TerrainObjectKind == MISC_LIGHT_WALL && BATTLEMECH <= collider->ObjectClass &&
        collider->ObjectClass < ELEMENTAL)
    {
        MCWeaponShotInfo shot;
        shot.Init(collider, -1, 250.0f, 0, 0.0f);

        if (MPlayer == nullptr)
        {
            collidee->HandleWeaponHit(&shot, 0);
        }
        else if (MPlayer->IsServer != 0)
        {
            collidee->HandleWeaponHit(&shot, 1);
        }
    }

    return 1;
}

auto MCMiscTerrainObjectType::HandleDestruction(MCGameObject*, MCGameObject*) -> int
{
    return 0;
}

//---------------------------------------------------------------------------
// MiscTerrainObject
//---------------------------------------------------------------------------

MCMiscTerrainObject::MCMiscTerrainObject()
{
    VertexNumber = 0;
    BlockNumber = 0;
    Destroyed = 0;
    FireObject = nullptr;
    TerrainObjectKind = -1;
    JustCreated = 1;
}

auto MCMiscTerrainObject::Init() -> void
{
}

auto MCMiscTerrainObject::KillFireObject() -> void
{
    FireObject = nullptr;
}

auto MCMiscTerrainObject::GetBlockAndVertexNumber(int32_t& blockNum, int32_t& vertexNum) -> void
{
    blockNum = BlockNumber;
    vertexNum = VertexNumber;
}

auto MCMiscTerrainObject::Update() -> int32_t
{
    if (JustCreated == 0)
    {
        return 1;
    }

    // Set the object on its vertex, 60 pixels down the tile (turned into the isometric grid's 60-degree axes).
    const int32_t verticesBlockSide = MCTerrain::VerticesBlockSide;
    JustCreated = 0;
    OverlayDestroyed = 0;
    CollisionsOn = 1;
    const int32_t blocksMapSide = MCTerrain::BlocksMapSide;
    float blockX = static_cast<float>(BlockNumber % blocksMapSide - blocksMapSide / 2) * MCTerrain::MetersBlockSide;
    float blockY = static_cast<float>(blocksMapSide / 2 - BlockNumber / blocksMapSide) * MCTerrain::MetersBlockSide;

    if ((blocksMapSide & 1) != 0)
    {
        blockX = blockX - MCTerrain::MetersBlockSide * 0.5f;
        blockY = MCTerrain::MetersBlockSide * 0.5f + blockY;
    }

    const float vertexX = static_cast<float>(VertexNumber % verticesBlockSide) * MCTerrain::MetersPerVertex;
    const double offsetAngle = std::atan(0.0 / 60.0) * RADIANS_TO_DEGREES;
    Position.Y = blockY - static_cast<float>(VertexNumber / verticesBlockSide) * MCTerrain::MetersPerVertex;
    const float offsetDistance = 3600.0f;
    const double axisAngle = (60.0 - offsetAngle) * DEGREES_TO_RADIANS;
    const auto alongAxis =
        static_cast<float>(std::sin(axisAngle) * std::sqrt(offsetDistance) / std::sin(SIXTY_DEGREES));
    Position.X = vertexX + blockX;
    const float elevation = Land->GetTerrainElevation(Position);
    Position.X = static_cast<float>(std::cos(SIXTY_DEGREES) * alongAxis +
                                    std::cos(axisAngle) * std::sqrt(offsetDistance) + Position.X);
    Position.Y = Position.Y - alongAxis;
    Position.Z = elevation;

    // An object whose tile already shows it broken (a blocked bridge; an open wall or forest) starts destroyed.
    int32_t tileR;
    int32_t tileC;
    const MCMapTile& tile = TileAt(this, tileR, tileC);
    const auto* type = static_cast<MCMiscTerrainObjectType*>(ObjType);

    switch (TerrainObjectKind)
    {
        case MISC_BRIDGE:
        {
            for (uint32_t shift = 0; shift < 0x12; shift += 2)
            {
                if (((0x4000u << shift) & tile.Cells) >> (shift + 0xe) == 0)
                {
                    Status = 2;
                    Damage = static_cast<float>(static_cast<int32_t>(type->BridgeDmgLevel));
                }
            }
            break;
        }
        case MISC_FOREST:
        {
            if (CountPassable(tile) == 9)
            {
                Damage = static_cast<float>(static_cast<int32_t>(type->ForestDmgLevel));
                Status = 2;
            }
            break;
        }
        case MISC_WALL:
        case MISC_MEDIUM_WALL:
        case MISC_LIGHT_WALL:
        {
            if (CountPassable(tile) == 9)
            {
                Damage = static_cast<float>(static_cast<int32_t>(type->WallDmgLevel));
                Status = 2;
            }
            break;
        }
        default:
            break;
    }

    return 1;
}

auto MCMiscTerrainObject::HandleEvent(MCObjectEvent* event) -> int32_t
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

auto MCMiscTerrainObject::GetScreenPos() -> MCVector2D
{
    MCVector2D screenPos;
    Eye->VertexProject(BlockNumber, VertexNumber, screenPos);
    return screenPos;
}

auto MCMiscTerrainObject::Render() -> void
{
    // Once the fire is out, a destroyed object's tile is opened up (a destroyed bridge stays impassable).
    if (FireObject == nullptr && Destroyed != 0)
    {
        int32_t tileR;
        int32_t tileC;
        MCMapTile& tile = TileAt(this, tileR, tileC);

        switch (TerrainObjectKind)
        {
            case MISC_BRIDGE:
                SetAllCells(tile, 0);
                break;
            case MISC_FOREST:
                ClearForestCells(tile);
                break;
            case MISC_WALL:
            case MISC_MEDIUM_WALL:
            case MISC_LIGHT_WALL:
                SetAllCells(tile, 1);
                break;
            default:
                break;
        }
    }

    MCVector2D screenPos;

    if (Eye->VertexProject(BlockNumber, VertexNumber, screenPos) == 0 || IsRevealed() == 0)
    {
        return;
    }

    if ((Selected == -1 || Selected == 1) && static_cast<uint8_t>(Status) != 2)
    {
        DrawBars(screenPos);
    }

    if (TerrainObjectKind != MISC_FOREST)
    {
        return;
    }

    // A forest draws the edge shape that matches its overlay tile, hazed by how many corners the home team sees.
    const int32_t overlayTile = Land->GetOverlayTile(BlockNumber, VertexNumber);
    int32_t edge;

    if (overlayTile < 0xd0e)
    {
        edge = overlayTile - 0xd0a;
    }
    else
    {
        if (0xd6d < overlayTile || overlayTile - 0xd66 < 0)
        {
            return;
        }

        edge = overlayTile - 0xd62;
    }

    if (edge < 0)
    {
        return;
    }

    const auto row = static_cast<uint32_t>((BlockNumber / MCTerrain::BlocksMapSide) * MCTerrain::VerticesBlockSide +
                                           VertexNumber / MCTerrain::VerticesBlockSide);
    const auto col = static_cast<uint32_t>((BlockNumber % MCTerrain::BlocksMapSide) * MCTerrain::VerticesBlockSide +
                                           VertexNumber % MCTerrain::VerticesBlockSide);
    MCByteFlag* visibleBits = HomeTeam->Alignment == -1 ? MCTerrain::ClanVisibleBits : MCTerrain::TerrainVisibleBits;
    int32_t numVisible = 0;

    if (visibleBits->GetFlag(row, col) != 0)
    {
        numVisible = 1;
    }

    if (visibleBits->GetFlag(row, col + 1) != 0)
    {
        numVisible++;
    }

    if (visibleBits->GetFlag(row + 1, col + 1) != 0)
    {
        numVisible++;
    }

    if (visibleBits->GetFlag(row + 1, col) != 0)
    {
        numVisible++;
    }

    uint8_t* hazePalette = nullptr;

    if (numVisible != 4)
    {
        const int32_t hazeLevel = Eye->HazeLevel;
        int32_t level;

        if (hazeLevel < 0 && 0 < hazeLevel + Eye->HazeInc * numVisible)
        {
            level = 0;
        }
        else
        {
            level = hazeLevel + Eye->HazeInc * numVisible;
        }

        hazePalette = GamePalette->GetHazePalette(level);
    }

    int32_t frame = edge * 2;
    int32_t depthOffset = 0x3a;

    if (Eye->CameraScale == 1)
    {
        frame++;
        depthOffset = 0x1d;

        if (TerrainTiles->CustomTileSet == 0)
        {
            screenPos.X = screenPos.X + 79.0f;
            screenPos.Y = screenPos.Y + 29.0f;
        }
    }

    ElementList->OpenGroup(static_cast<int32_t>(-screenPos.Y - static_cast<float>(depthOffset)), 1);
    auto* element = MCElementPool::Make<MCVfxElement>(static_cast<MCMiscTerrainObjectType*>(ObjType)->ForestEdgeShapes,
                                                      screenPos.X, screenPos.Y, frame, 0, hazePalette, 1, 0);
    std::strcpy(element->Name, "terobj");
    ElementList->Add(element);
}

auto MCMiscTerrainObject::DrawBars(MCVector2D screenPos) -> void
{
    // The damage bar over a selected wall, bridge or forest: green, then yellow under half, red at a fifth.
    // Port: an overlay, on the screen over the view: it follows the object through the zoom, its size doesn't change.
    screenPos = MCOverlayPoint(screenPos);
    MCPolyElementData data;
    data.Init();
    const float scale = Eye->CameraScale != 1 ? 1.0f : 0.5f;
    const float barWidth = (Eye->CameraScale != 1 ? 1.0f : 0.5f) * 38.0f;
    const float barHeight = (Eye->CameraScale != 1 ? 1.0f : 0.5f) * 4.0f;
    const float top = (screenPos.Y - scale * 6.0f) - barHeight;
    const auto left = static_cast<float>(std::floor(static_cast<double>(screenPos.X - barWidth * 0.5f)));
    int32_t damageTaken = static_cast<int16_t>(static_cast<int32_t>(std::floor(static_cast<double>(GetDamage()))));
    const int32_t maxDamage = DmgLevelFor(static_cast<MCMiscTerrainObjectType*>(ObjType), TerrainObjectKind);

    if (maxDamage < damageTaken)
    {
        damageTaken = maxDamage;
    }

    // Faithful: an object of no known kind divides by zero here.
    const float health = 1.0f - static_cast<float>(damageTaken) / static_cast<float>(maxDamage);
    int32_t barColor;

    if (health < 0.5f)
    {
        barColor = health <= 0.2f ? 0x103 : 0x102;
    }
    else
    {
        barColor = 0x101;
    }

    float barLength = health * barWidth;

    if (barLength < 1.0f && 0.001f < health)
    {
        barLength = 1.0f;
    }

    ElementList->OpenGroup(-50000, 1);
    data.NumVertices = 0;
    data.TextureMapOff = 0;
    data.Texture = nullptr;
    data.TextureWidth = 0;
    data.TextureHeight = 0;
    data.FadeTable = nullptr;
    data.Translate = 0;
    data.StatusBar = 1;
    data.BarColor = barColor;
    data.Vertices[0].X = static_cast<int16_t>(static_cast<int32_t>(std::floor(static_cast<double>(left - 1.0f))));
    data.Vertices[0].Y = static_cast<int16_t>(static_cast<int32_t>(std::floor(static_cast<double>(top - 1.0f))));
    data.Vertices[1].X =
        static_cast<int16_t>(static_cast<int32_t>(std::floor(static_cast<double>(left + barWidth + 1.0f))));
    data.Vertices[1].Y =
        static_cast<int16_t>(static_cast<int32_t>(std::floor(static_cast<double>(top + barHeight + 1.0f))));
    data.BarPercent = static_cast<int16_t>(static_cast<int32_t>(std::floor(static_cast<double>(barLength))));

    if (0 < data.BarPercent)
    {
        ElementList->Add(MCElementPool::Make<MCPolygonElement>(&data, -50000));
    }
}

auto MCMiscTerrainObject::Destroy() -> void
{
}

auto MCMiscTerrainObject::SetDamage(float newDamage) -> void
{
    if (newDamage != Damage)
    {
        Damage = newDamage;
    }

    const auto* type = static_cast<MCMiscTerrainObjectType*>(ObjType);
    int32_t tileR;
    int32_t tileC;

    switch (TerrainObjectKind)
    {
        case MISC_BRIDGE:
        {
            // A destroyed bridge: the broken overlay, its span closed to movement (and its global map area shut).
            if (Damage < static_cast<float>(static_cast<int32_t>(type->BridgeDmgLevel)) || OverlayDestroyed != 0)
            {
                return;
            }

            Land->SetOverlayTile(BlockNumber, VertexNumber, 0xf);
            MCTerrain::ForceRedraw = 1;
            OverlayDestroyed = 1;
            Destroyed = 1;
            Status = 2;
            MCMapTile& tile = TileAt(this, tileR, tileC);

            switch (tile.Overlay & 0x7f)
            {
                case 0x25:
                    tile.Overlay = (tile.Overlay & 0xffffffa6) | 0x26;
                    break;
                case 0x27:
                    tile.Overlay = (tile.Overlay & 0xffffffa8) | 0x28;
                    break;
                case 0x37:
                    tile.Overlay = (tile.Overlay & 0xffffffb8) | 0x38;
                    break;
                case 0x39:
                    tile.Overlay = (tile.Overlay & 0xffffffba) | 0x3a;
                    break;
                default:
                    break;
            }

            const int32_t area = GlobalMoveMap->CalcArea(tileR, tileC);

            if (area < 0)
            {
                Fatal(0, "Bad Global Area to close");
            }
            else
            {
                GlobalMoveMap->CloseArea(area);
            }

            SetAllCells(tile, 0);
            return;
        }

        case MISC_FOREST:
        {
            // A burnt forest: the cleared overlay, passable.
            if (Damage < static_cast<float>(static_cast<int32_t>(type->ForestDmgLevel)) || OverlayDestroyed != 0)
            {
                return;
            }

            const int32_t overlayTile = Land->GetOverlayTile(BlockNumber, VertexNumber);
            Land->SetOverlayTile(BlockNumber, VertexNumber, overlayTile < 0xd0a || 0xd0d < overlayTile ? 8 : 4);
            MCTerrain::ForceRedraw = 1;
            OverlayDestroyed = 1;
            Destroyed = 1;
            Status = 2;
            MCMapTile& tile = TileAt(this, tileR, tileC);
            tile.Overlay = (tile.Overlay & 0xffffffbf) | 0x3f;
            ClearForestCells(tile);
            return;
        }

        case MISC_WALL:
        case MISC_MEDIUM_WALL:
        case MISC_LIGHT_WALL:
        {
            // A knocked-down wall: the rubble overlay, passable once any fire is out.
            const int32_t level = DmgLevelFor(type, TerrainObjectKind);

            if (Damage < static_cast<float>(level) || OverlayDestroyed != 0)
            {
                return;
            }

            Land->GetOverlayTile(BlockNumber, VertexNumber);
            Land->SetOverlayTile(BlockNumber, VertexNumber, 0x13);
            MCTerrain::ForceRedraw = 1;
            OverlayDestroyed = 1;
            Destroyed = 1;
            Status = 2;
            MCMapTile& tile = TileAt(this, tileR, tileC);
            tile.Overlay = (tile.Overlay & 0xffffffbd) | 0x3d;

            if (FireObject == nullptr)
            {
                SetAllCells(tile, 1);
            }

            return;
        }

        default:
            return;
    }
}

auto MCMiscTerrainObject::Init(MCObjectType* objType) -> int32_t
{
    const int32_t result = MCGameObject::Init(objType);

    if (result != 0)
    {
        return result;
    }

    JustCreated = 1;
    CollisionsOn = 0;
    ObjectClass = MISCTERRAINOBJECT;
    Damage = 0.0f;
    Alignment = 0;
    return 0;
}

auto MCMiscTerrainObject::LightOnFire(float timeToBurn) -> void
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

    if (FireObject != nullptr)
    {
        FireObject->AddTimeLeftToBurn(timeToBurn);
    }
}

auto MCMiscTerrainObject::ClearLineOfFire() -> void
{
    // Save each cell's line-of-sight bit and make it see-through (so a shot can pass while it is checked).
    int32_t tileR;
    int32_t tileC;
    MCMapTile& tile = TileAt(this, tileR, tileC);
    int32_t cell = 0;

    for (uint32_t shift = 0; shift < 0x12; shift += 2, cell++)
    {
        const uint32_t bit = 0x8000u << shift;
        CellArray[cell] = static_cast<int32_t>((tile.Cells & bit) >> (shift + 0xf));
        tile.Cells = (~bit & tile.Cells) | (1u << (shift + 0xf));
    }
}

auto MCMiscTerrainObject::RestoreLineOfFire() -> void
{
    int32_t tileR;
    int32_t tileC;
    MCMapTile& tile = TileAt(this, tileR, tileC);
    int32_t cell = 0;

    for (uint32_t shift = 0; shift < 0x12; shift += 2, cell++)
    {
        tile.Cells = (static_cast<uint32_t>(CellArray[cell]) << (shift + 0xf)) | (~(0x8000u << shift) & tile.Cells);
    }
}

auto MCMiscTerrainObject::HandleWeaponHit(MCWeaponShotInfo* shotInfo, int addMultiplayChunk) -> int32_t
{
    if (shotInfo == nullptr)
    {
        return 0;
    }

    if (addMultiplayChunk != 0)
    {
        MPlayer->AddWeaponHitChunk(this, shotInfo, 0);
    }

    if (Destroyed != 0)
    {
        return 0;
    }

    const float newDamage = GetDamage() + shotInfo->Damage;
    const auto* type = static_cast<MCMiscTerrainObjectType*>(ObjType);

    switch (TerrainObjectKind)
    {
        case MISC_BRIDGE:
        case MISC_WALL:
        case MISC_MEDIUM_WALL:
        {
            if (static_cast<float>(DmgLevelFor(type, TerrainObjectKind)) <= newDamage)
            {
                ObjType->CreateExplosion(Position, 0.0f, 0.0f);
                Status = 2;
            }
            break;
        }
        case MISC_FOREST:
        {
            // A forest catches fire on any hit (the test is always true) and burns a second longer per hit.
            const auto threshold = static_cast<float>(static_cast<int32_t>(type->ForestDmgLevel));

            if (threshold <= newDamage)
            {
                ObjType->CreateExplosion(Position, 0.0f, 0.0f);
                Status = 2;
            }

            if (FireObject == nullptr &&
                (newDamage < static_cast<float>(static_cast<int32_t>(type->ForestDmgLevel)) || threshold <= newDamage))
            {
                MCGameObject* fire = CreateObject(static_cast<int32_t>(type->ForestFireFX));

                if (fire != nullptr)
                {
                    fire->SetPosition(Position);
                    FireObject = static_cast<MCFire*>(fire);
                    fire->SetPotentialContact(3);
                    FireObject->BurningObject = this;
                    FireObject->SetTonnage(40.0f);
                    FireObject->Update();
                }
            }

            if (FireObject != nullptr)
            {
                FireObject->AddTimeLeftToBurn(1.0f);
            }
            break;
        }

        case MISC_LIGHT_WALL:
        {
            if (static_cast<float>(static_cast<int32_t>(type->LightWallDmgLevel)) <= newDamage)
            {
                ObjType->CreateExplosion(Position, 0.0f, 0.0f);
                Status = 2;
                SoundSystem->PlayDigitalSample(0x48, 1, this, 0, 0);
            }
            break;
        }
        default:
            break;
    }

    SetDamage(newDamage);
    return 0;
}

auto MCMiscTerrainObject::IsRevealed() -> int
{
    const auto col = static_cast<uint32_t>((BlockNumber % MCTerrain::BlocksMapSide) * MCTerrain::VerticesBlockSide +
                                           VertexNumber % MCTerrain::VerticesBlockSide);
    const auto row = static_cast<uint32_t>((BlockNumber / MCTerrain::BlocksMapSide) * MCTerrain::VerticesBlockSide +
                                           VertexNumber / MCTerrain::VerticesBlockSide);
    MCByteFlag* visibleBits = HomeTeam->Alignment == -1 ? MCTerrain::ClanVisibleBits : MCTerrain::TerrainVisibleBits;

    if (visibleBits->GetFlag(row, col) != 0)
    {
        return 1;
    }

    if (visibleBits->GetFlag(row, col + 1) != 0)
    {
        return 1;
    }

    if (visibleBits->GetFlag(row + 1, col + 1) != 0)
    {
        return 1;
    }

    return visibleBits->GetFlag(row + 1, col) != 0 ? 1 : 0;
}
