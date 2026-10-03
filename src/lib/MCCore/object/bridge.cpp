#include "stdafx.h"
#include "object/bridge.h"
#include "ai/move.h"
#include "camera/camera.h"
#include "color/color.h"
#include "engine/bitflag.h"
#include "engine/ceglist.h"
#include "engine/cepoly.h"
#include "engine/cevfx.h"
#include "lib/aerror.h"
#include "lib/cident.h"
#include "lib/cvmath.h"
#include "lib/file.h"
#include "lib/heap.h"
#include "lib/inifile.h"
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

namespace
{
    /// <summary>Degrees to radians, as MCX.EXE stores it (a hair under pi / 180).</summary>
    constexpr double DEGREES_TO_RADIANS = 0x1.1df46a2526c7ap-6;
    /// <summary>Radians to degrees, as MCX.EXE stores it.</summary>
    constexpr double RADIANS_TO_DEGREES = 0x1.ca5dc1a6402aap+5;
    /// <summary>Sixty degrees in radians, as MCX.EXE stores it.</summary>
    constexpr double SIXTY_DEGREES = 0x1.0c152382d45b2p+0;

    /// <summary>What a misc terrain object is (terrainObjectKind, set by the placement code).</summary>
    enum MiscTerrainKind : int32_t
    {
        MISC_BRIDGE = 5,
        MISC_FOREST = 6,
        MISC_WALL = 7,
        MISC_MEDIUM_WALL = 8,
        MISC_LIGHT_WALL = 9
    };

    /// <summary>The map tile under the object's position.</summary>
    MapTile& tileAt(MiscTerrainObject* object, int32_t& tileR, int32_t& tileC)
    {
        int32_t cellR = 0;
        int32_t cellC = 0;
        tileR = 0;
        tileC = 0;
        GameMap->worldToMapPos(object->position, tileR, tileC, cellR, cellC);
        return GameMap->map[GameMap->width * tileR + tileC];
    }

    /// <summary>How many of the tile's nine cells are passable.</summary>
    int32_t countPassable(const MapTile& tile)
    {
        int32_t count = 0;

        for (uint32_t shift = 0; shift < 0x12; shift += 2)
        {
            count += static_cast<int32_t>((tile.cells & (0x4000u << shift)) >> (shift + 0xe));
        }

        return count;
    }

    /// <summary>Sets every cell of the tile: passable to <paramref name="passable"/>, see-through.</summary>
    void setAllCells(MapTile& tile, uint32_t passable)
    {
        for (uint32_t shift = 0; shift < 0x12; shift += 2)
        {
            tile.cells = (passable << (shift + 0xe)) | (~(0x4000u << shift) & tile.cells);
            tile.cells = (~(0x8000u << shift) & tile.cells) | (1u << (shift + 0xf));
        }
    }

    /// <summary>
    /// Makes every cell of a burnt forest tile passable (unless its terrain is type 0x29 or 0x2a) and see-through.
    /// </summary>
    void clearForestCells(MapTile& tile)
    {
        for (uint32_t shift = 0; shift < 0x12; shift += 2)
        {
            if ((tile.cells & 0x7f) != 0x29 && (tile.cells & 0x7f) != 0x2a)
            {
                tile.cells = (1u << (shift + 0xe)) | (~(0x4000u << shift) & tile.cells);
            }

            tile.cells = (1u << (shift + 0xf)) | (~(0x8000u << shift) & tile.cells);
        }
    }

    /// <summary>The damage level that destroys a misc terrain object of <paramref name="kind"/>, or 0.</summary>
    int32_t dmgLevelFor(const MiscTerrainObjectType* type, int32_t kind)
    {
        switch (kind)
        {
            case MISC_BRIDGE:
                return static_cast<int32_t>(type->bridgeDmgLevel);
            case MISC_FOREST:
                return static_cast<int32_t>(type->forestDmgLevel);
            case MISC_WALL:
                return static_cast<int32_t>(type->wallDmgLevel);
            case MISC_MEDIUM_WALL:
                return static_cast<int32_t>(type->mediumWallDmgLevel);
            case MISC_LIGHT_WALL:
                return static_cast<int32_t>(type->lightWallDmgLevel);
            default:
                return 0;
        }
    }
} // namespace

int32_t MiscTerrainObject::cellArray[9] = {};

//---------------------------------------------------------------------------
// MiscTerrainObjectType
//---------------------------------------------------------------------------

auto MiscTerrainObjectType::init() -> void
{
    ObjectType::init();
    forestDmgLevel = 0;
    lightWallDmgLevel = 0;
    mediumWallDmgLevel = 0;
    wallDmgLevel = 0;
    bridgeDmgLevel = 0;
    wallFireFX = 0xffffffff;
    bridgeFireFX = 0xffffffff;
    forestFireFX = 0xffffffff;
    blownEffectId = 0xffffffff;
    normalEffectId = 0xffffffff;
    damageEffectId = 0xffffffff;
    forestEdgeShapes = nullptr;
}

auto MiscTerrainObjectType::createInstance() -> BaseObject*
{
    auto* newObject = new MiscTerrainObject;

    if (newObject == nullptr)
    {
        return nullptr;
    }

    if (newObject->init(this) != 0)
    {
        return nullptr;
    }

    newObject->idNumber = NextIdNumber++;
    return newObject;
}

auto MiscTerrainObjectType::destroy() -> void
{
}

auto MiscTerrainObjectType::init(File* objFile, uint32_t fileSize) -> int32_t
{
    FitIniFile bridgeFile;
    int32_t result = bridgeFile.open(objFile, fileSize, 50);

    if (result != 0)
    {
        return result;
    }

    if ((result = bridgeFile.seekBlock("BridgeData")) != 0)
    {
        return result;
    }

    if ((result = bridgeFile.readIdULong("WallDmgLevel", wallDmgLevel)) != 0)
    {
        return result;
    }

    if (bridgeFile.readIdULong("MediumWallDmgLevel", mediumWallDmgLevel) != 0)
    {
        mediumWallDmgLevel = wallDmgLevel >> 1;
    }

    // Original behaviour (OB-021): the light wall default halves the light wall level itself (0 when unread),
    // not the wall's.
    if (bridgeFile.readIdULong("LightWallDmgLevel", lightWallDmgLevel) != 0)
    {
        lightWallDmgLevel = lightWallDmgLevel >> 1;
    }

    if ((result = bridgeFile.readIdULong("BridgeDmgLevel", bridgeDmgLevel)) != 0)
    {
        return result;
    }

    if ((result = bridgeFile.readIdULong("ForestDmgLevel", forestDmgLevel)) != 0)
    {
        return result;
    }

    if ((result = bridgeFile.readIdULong("WallFireFX", wallFireFX)) != 0)
    {
        return result;
    }

    if ((result = bridgeFile.readIdULong("BridgeFireFX", bridgeFireFX)) != 0)
    {
        return result;
    }

    if ((result = bridgeFile.readIdULong("ForestFireFX", forestFireFX)) != 0)
    {
        return result;
    }

    if ((result = bridgeFile.readIdULong("BlownEffectId", blownEffectId)) != 0)
    {
        return result;
    }

    if ((result = bridgeFile.readIdULong("NormalEffectId", normalEffectId)) != 0)
    {
        return result;
    }

    if ((result = bridgeFile.readIdULong("DamageEffectId", damageEffectId)) != 0)
    {
        return result;
    }

    // The forest edge shapes (the "x" set for a custom tile set).
    char edgesName[250];

    if ((result = bridgeFile.readIdString("ForestEdges", edgesName, 0xf9)) != 0)
    {
        return result;
    }

    if (terrainTiles->customTileSet == 1)
    {
        std::strcat(edgesName, "x");
    }

    FullPathFileName edgesPath;
    edgesPath.init(spritePath, edgesName, ".shp");
    File edgesFile;

    if ((result = edgesFile.open(edgesPath, READ, 50)) != 0)
    {
        return result;
    }

    uint32_t size = edgesFile.fileSize();
    forestEdgeShapes = static_cast<uint8_t*>(ObjectTypeManager::objectTypeCache->malloc(size));
    Assert(forestEdgeShapes != nullptr ? 1u : 0u, 0, " No RAM for Forest Edge Shapes ");
    size = edgesFile.fileSize();
    edgesFile.read(forestEdgeShapes, static_cast<int32_t>(size));
    edgesFile.close();
    return ObjectType::init(&bridgeFile);
}

auto MiscTerrainObjectType::handleCollision(GameObject* collidee, GameObject* collider) -> int
{
    // A mech or vehicle running into a light wall knocks it down (250 points, the server's job in multiplayer).
    auto* object = static_cast<MiscTerrainObject*>(collidee);

    if (object->terrainObjectKind == MISC_LIGHT_WALL && BATTLEMECH <= collider->objectClass &&
        collider->objectClass < ELEMENTAL)
    {
        _WeaponShotInfo shot;
        shot.init(collider, -1, 250.0f, 0, 0.0f);

        if (MPlayer == nullptr)
        {
            collidee->handleWeaponHit(&shot, 0);
        }
        else if (MPlayer->isServer != 0)
        {
            collidee->handleWeaponHit(&shot, 1);
        }
    }

    return 1;
}

auto MiscTerrainObjectType::handleDestruction(GameObject*, GameObject*) -> int
{
    return 0;
}

//---------------------------------------------------------------------------
// MiscTerrainObject
//---------------------------------------------------------------------------

MiscTerrainObject::MiscTerrainObject()
{
    vertexNumber = 0;
    blockNumber = 0;
    destroyed = 0;
    fireObject = nullptr;
    terrainObjectKind = -1;
    justCreated = 1;
}

auto MiscTerrainObject::init() -> void
{
}

auto MiscTerrainObject::killFireObject() -> void
{
    fireObject = nullptr;
}

auto MiscTerrainObject::getBlockAndVertexNumber(int32_t& blockNum, int32_t& vertexNum) -> void
{
    blockNum = blockNumber;
    vertexNum = vertexNumber;
}

auto MiscTerrainObject::update() -> int32_t
{
    if (justCreated == 0)
    {
        return 1;
    }

    // Set the object on its vertex, 60 pixels down the tile (turned into the isometric grid's 60-degree axes).
    const int32_t verticesBlockSide = Terrain::verticesBlockSide;
    justCreated = 0;
    overlayDestroyed = 0;
    collisionsOn = 1;
    const int32_t blocksMapSide = Terrain::blocksMapSide;
    float blockX = static_cast<float>(blockNumber % blocksMapSide - blocksMapSide / 2) * Terrain::metersBlockSide;
    float blockY = static_cast<float>(blocksMapSide / 2 - blockNumber / blocksMapSide) * Terrain::metersBlockSide;

    if ((blocksMapSide & 1) != 0)
    {
        blockX = blockX - Terrain::metersBlockSide * 0.5f;
        blockY = Terrain::metersBlockSide * 0.5f + blockY;
    }

    const float vertexX = static_cast<float>(vertexNumber % verticesBlockSide) * Terrain::metersPerVertex;
    const double offsetAngle = std::atan(0.0 / 60.0) * RADIANS_TO_DEGREES;
    position.y = blockY - static_cast<float>(vertexNumber / verticesBlockSide) * Terrain::metersPerVertex;
    const float offsetDistance = 3600.0f;
    const double axisAngle = (60.0 - offsetAngle) * DEGREES_TO_RADIANS;
    const auto alongAxis =
        static_cast<float>(std::sin(axisAngle) * std::sqrt(offsetDistance) / std::sin(SIXTY_DEGREES));
    position.x = vertexX + blockX;
    const float elevation = land->getTerrainElevation(position);
    position.x = static_cast<float>(std::cos(SIXTY_DEGREES) * alongAxis +
                                    std::cos(axisAngle) * std::sqrt(offsetDistance) + position.x);
    position.y = position.y - alongAxis;
    position.z = elevation;

    // An object whose tile already shows it broken (a blocked bridge; an open wall or forest) starts destroyed.
    int32_t tileR;
    int32_t tileC;
    const MapTile& tile = tileAt(this, tileR, tileC);
    const auto* type = static_cast<MiscTerrainObjectType*>(objType);

    switch (terrainObjectKind)
    {
        case MISC_BRIDGE:
        {
            for (uint32_t shift = 0; shift < 0x12; shift += 2)
            {
                if (((0x4000u << shift) & tile.cells) >> (shift + 0xe) == 0)
                {
                    status = 2;
                    damage = static_cast<float>(static_cast<int32_t>(type->bridgeDmgLevel));
                }
            }
            break;
        }
        case MISC_FOREST:
        {
            if (countPassable(tile) == 9)
            {
                damage = static_cast<float>(static_cast<int32_t>(type->forestDmgLevel));
                status = 2;
            }
            break;
        }
        case MISC_WALL:
        case MISC_MEDIUM_WALL:
        case MISC_LIGHT_WALL:
        {
            if (countPassable(tile) == 9)
            {
                damage = static_cast<float>(static_cast<int32_t>(type->wallDmgLevel));
                status = 2;
            }
            break;
        }
        default:
            break;
    }

    return 1;
}

auto MiscTerrainObject::handleEvent(ObjectEvent* event) -> int32_t
{
    if (event->type == 0)
    {
        switch (event->id)
        {
            case 0x1c:
                selected = 1;
                break;
            case 0x1d:
                selected = 0;
                break;
            case 0x1e:
                unknown2C = 1;
                break;
            case 0x1f:
                unknown2C = 0;
                break;
            default:
                break;
        }
    }

    return 0;
}

auto MiscTerrainObject::getScreenPos() -> vector_2d
{
    vector_2d screenPos;
    eye->vertexProject(blockNumber, vertexNumber, screenPos);
    return screenPos;
}

auto MiscTerrainObject::render() -> void
{
    // Once the fire is out, a destroyed object's tile is opened up (a destroyed bridge stays impassable).
    if (fireObject == nullptr && destroyed != 0)
    {
        int32_t tileR;
        int32_t tileC;
        MapTile& tile = tileAt(this, tileR, tileC);

        switch (terrainObjectKind)
        {
            case MISC_BRIDGE:
                setAllCells(tile, 0);
                break;
            case MISC_FOREST:
                clearForestCells(tile);
                break;
            case MISC_WALL:
            case MISC_MEDIUM_WALL:
            case MISC_LIGHT_WALL:
                setAllCells(tile, 1);
                break;
            default:
                break;
        }
    }

    vector_2d screenPos;

    if (eye->vertexProject(blockNumber, vertexNumber, screenPos) == 0 || isRevealed() == 0)
    {
        return;
    }

    if ((selected == -1 || selected == 1) && static_cast<uint8_t>(status) != 2)
    {
        drawBars(screenPos);
    }

    if (terrainObjectKind != MISC_FOREST)
    {
        return;
    }

    // A forest draws the edge shape that matches its overlay tile, hazed by how many corners the home team sees.
    const int32_t overlayTile = land->getOverlayTile(blockNumber, vertexNumber);
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

    const auto row = static_cast<uint32_t>((blockNumber / Terrain::blocksMapSide) * Terrain::verticesBlockSide +
                                           vertexNumber / Terrain::verticesBlockSide);
    const auto col = static_cast<uint32_t>((blockNumber % Terrain::blocksMapSide) * Terrain::verticesBlockSide +
                                           vertexNumber % Terrain::verticesBlockSide);
    ByteFlag* visibleBits = homeTeam->alignment == -1 ? Terrain::ClanVisibleBits : Terrain::terrainVisibleBits;
    int32_t numVisible = 0;

    if (visibleBits->getFlag(row, col) != 0)
    {
        numVisible = 1;
    }

    if (visibleBits->getFlag(row, col + 1) != 0)
    {
        numVisible++;
    }

    if (visibleBits->getFlag(row + 1, col + 1) != 0)
    {
        numVisible++;
    }

    if (visibleBits->getFlag(row + 1, col) != 0)
    {
        numVisible++;
    }

    uint8_t* hazePalette = nullptr;

    if (numVisible != 4)
    {
        const int32_t hazeLevel = eye->hazeLevel;
        int32_t level;

        if (hazeLevel < 0 && 0 < hazeLevel + eye->hazeInc * numVisible)
        {
            level = 0;
        }
        else
        {
            level = hazeLevel + eye->hazeInc * numVisible;
        }

        hazePalette = gamePalette->getHazePalette(level);
    }

    int32_t frame = edge * 2;
    int32_t depthOffset = 0x3a;

    if (eye->cameraScale == 1)
    {
        frame++;
        depthOffset = 0x1d;

        if (terrainTiles->customTileSet == 0)
        {
            screenPos.x = screenPos.x + 79.0f;
            screenPos.y = screenPos.y + 29.0f;
        }
    }

    ElementList->openGroup(static_cast<int32_t>(-screenPos.y - static_cast<float>(depthOffset)), 1);
    auto* element = new VFXElement(static_cast<MiscTerrainObjectType*>(objType)->forestEdgeShapes, screenPos.x,
                                   screenPos.y, frame, 0, hazePalette, 1, 0);
    std::strcpy(element->name, "terobj");
    ElementList->add(element);
}

auto MiscTerrainObject::drawBars(vector_2d screenPos) -> void
{
    // The damage bar over a selected wall, bridge or forest: green, then yellow under half, red at a fifth.
    // Port: an overlay, on the screen over the view: it follows the object through the zoom, its size doesn't change.
    screenPos = MCOverlayPoint(screenPos);
    PolyElementData data;
    data.init();
    const float scale = eye->cameraScale != 1 ? 1.0f : 0.5f;
    const float barWidth = (eye->cameraScale != 1 ? 1.0f : 0.5f) * 38.0f;
    const float barHeight = (eye->cameraScale != 1 ? 1.0f : 0.5f) * 4.0f;
    const float top = (screenPos.y - scale * 6.0f) - barHeight;
    const auto left = static_cast<float>(std::floor(static_cast<double>(screenPos.x - barWidth * 0.5f)));
    int32_t damageTaken = static_cast<int16_t>(static_cast<int32_t>(std::floor(static_cast<double>(getDamage()))));
    const int32_t maxDamage = dmgLevelFor(static_cast<MiscTerrainObjectType*>(objType), terrainObjectKind);

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

    ElementList->openGroup(-50000, 1);
    data.numVertices = 0;
    data.textureMapOff = 0;
    data.unknown98 = 0;
    data.texture = nullptr;
    data.textureWidth = 0;
    data.textureHeight = 0;
    data.fadeTable = nullptr;
    data.translate = 0;
    data.statusBar = 1;
    data.barColor = barColor;
    data.vertices[0].x = static_cast<int16_t>(static_cast<int32_t>(std::floor(static_cast<double>(left - 1.0f))));
    data.vertices[0].y = static_cast<int16_t>(static_cast<int32_t>(std::floor(static_cast<double>(top - 1.0f))));
    data.vertices[1].x =
        static_cast<int16_t>(static_cast<int32_t>(std::floor(static_cast<double>(left + barWidth + 1.0f))));
    data.vertices[1].y =
        static_cast<int16_t>(static_cast<int32_t>(std::floor(static_cast<double>(top + barHeight + 1.0f))));
    data.barPercent = static_cast<int16_t>(static_cast<int32_t>(std::floor(static_cast<double>(barLength))));

    if (0 < data.barPercent)
    {
        ElementList->add(new PolygonElement(&data, -50000));
    }
}

auto MiscTerrainObject::destroy() -> void
{
}

auto MiscTerrainObject::setDamage(float newDamage) -> void
{
    if (newDamage != damage)
    {
        damage = newDamage;
    }

    const auto* type = static_cast<MiscTerrainObjectType*>(objType);
    int32_t tileR;
    int32_t tileC;

    switch (terrainObjectKind)
    {
        case MISC_BRIDGE:
        {
            // A destroyed bridge: the broken overlay, its span closed to movement (and its global map area shut).
            if (damage < static_cast<float>(static_cast<int32_t>(type->bridgeDmgLevel)) || overlayDestroyed != 0)
            {
                return;
            }

            land->setOverlayTile(blockNumber, vertexNumber, 0xf);
            Terrain::forceRedraw = 1;
            overlayDestroyed = 1;
            destroyed = 1;
            status = 2;
            MapTile& tile = tileAt(this, tileR, tileC);

            switch (tile.overlay & 0x7f)
            {
                case 0x25:
                    tile.overlay = (tile.overlay & 0xffffffa6) | 0x26;
                    break;
                case 0x27:
                    tile.overlay = (tile.overlay & 0xffffffa8) | 0x28;
                    break;
                case 0x37:
                    tile.overlay = (tile.overlay & 0xffffffb8) | 0x38;
                    break;
                case 0x39:
                    tile.overlay = (tile.overlay & 0xffffffba) | 0x3a;
                    break;
                default:
                    break;
            }

            const int32_t area = GlobalMoveMap->calcArea(tileR, tileC);

            if (area < 0)
            {
                Fatal(0, "Bad Global Area to close");
            }
            else
            {
                GlobalMoveMap->closeArea(area);
            }

            setAllCells(tile, 0);
            return;
        }

        case MISC_FOREST:
        {
            // A burnt forest: the cleared overlay, passable.
            if (damage < static_cast<float>(static_cast<int32_t>(type->forestDmgLevel)) || overlayDestroyed != 0)
            {
                return;
            }

            const int32_t overlayTile = land->getOverlayTile(blockNumber, vertexNumber);
            land->setOverlayTile(blockNumber, vertexNumber, overlayTile < 0xd0a || 0xd0d < overlayTile ? 8 : 4);
            Terrain::forceRedraw = 1;
            overlayDestroyed = 1;
            destroyed = 1;
            status = 2;
            MapTile& tile = tileAt(this, tileR, tileC);
            tile.overlay = (tile.overlay & 0xffffffbf) | 0x3f;
            clearForestCells(tile);
            return;
        }

        case MISC_WALL:
        case MISC_MEDIUM_WALL:
        case MISC_LIGHT_WALL:
        {
            // A knocked-down wall: the rubble overlay, passable once any fire is out.
            const int32_t level = dmgLevelFor(type, terrainObjectKind);

            if (damage < static_cast<float>(level) || overlayDestroyed != 0)
            {
                return;
            }

            land->getOverlayTile(blockNumber, vertexNumber);
            land->setOverlayTile(blockNumber, vertexNumber, 0x13);
            Terrain::forceRedraw = 1;
            overlayDestroyed = 1;
            destroyed = 1;
            status = 2;
            MapTile& tile = tileAt(this, tileR, tileC);
            tile.overlay = (tile.overlay & 0xffffffbd) | 0x3d;

            if (fireObject == nullptr)
            {
                setAllCells(tile, 1);
            }

            return;
        }

        default:
            return;
    }
}

auto MiscTerrainObject::init(ObjectType* objType) -> int32_t
{
    const int32_t result = GameObject::init(objType);

    if (result != 0)
    {
        return result;
    }

    justCreated = 1;
    collisionsOn = 0;
    objectClass = MISCTERRAINOBJECT;
    damage = 0.0f;
    alignment = 0;
    return 0;
}

auto MiscTerrainObject::lightOnFire(float timeToBurn) -> void
{
    _WeaponShotInfo shot;
    shot.init(nullptr, -1, 1.0f, 0, 0.0f);

    if (MPlayer == nullptr)
    {
        handleWeaponHit(&shot, 0);
    }
    else if (MPlayer->isServer != 0)
    {
        handleWeaponHit(&shot, 1);
    }

    if (fireObject != nullptr)
    {
        fireObject->addTimeLeftToBurn(timeToBurn);
    }
}

auto MiscTerrainObject::clearLineOfFire() -> void
{
    // Save each cell's line-of-sight bit and make it see-through (so a shot can pass while it is checked).
    int32_t tileR;
    int32_t tileC;
    MapTile& tile = tileAt(this, tileR, tileC);
    int32_t cell = 0;

    for (uint32_t shift = 0; shift < 0x12; shift += 2, cell++)
    {
        const uint32_t bit = 0x8000u << shift;
        cellArray[cell] = static_cast<int32_t>((tile.cells & bit) >> (shift + 0xf));
        tile.cells = (~bit & tile.cells) | (1u << (shift + 0xf));
    }
}

auto MiscTerrainObject::restoreLineOfFire() -> void
{
    int32_t tileR;
    int32_t tileC;
    MapTile& tile = tileAt(this, tileR, tileC);
    int32_t cell = 0;

    for (uint32_t shift = 0; shift < 0x12; shift += 2, cell++)
    {
        tile.cells = (static_cast<uint32_t>(cellArray[cell]) << (shift + 0xf)) | (~(0x8000u << shift) & tile.cells);
    }
}

auto MiscTerrainObject::handleWeaponHit(_WeaponShotInfo* shotInfo, int addMultiplayChunk) -> int32_t
{
    if (shotInfo == nullptr)
    {
        return 0;
    }

    if (addMultiplayChunk != 0)
    {
        MPlayer->addWeaponHitChunk(this, shotInfo, 0);
    }

    if (destroyed != 0)
    {
        return 0;
    }

    const float newDamage = getDamage() + shotInfo->damage;
    const auto* type = static_cast<MiscTerrainObjectType*>(objType);

    switch (terrainObjectKind)
    {
        case MISC_BRIDGE:
        case MISC_WALL:
        case MISC_MEDIUM_WALL:
        {
            if (static_cast<float>(dmgLevelFor(type, terrainObjectKind)) <= newDamage)
            {
                objType->createExplosion(position, 0.0f, 0.0f);
                status = 2;
            }
            break;
        }
        case MISC_FOREST:
        {
            // A forest catches fire on any hit (the test is always true) and burns a second longer per hit.
            const auto threshold = static_cast<float>(static_cast<int32_t>(type->forestDmgLevel));

            if (threshold <= newDamage)
            {
                objType->createExplosion(position, 0.0f, 0.0f);
                status = 2;
            }

            if (fireObject == nullptr &&
                (newDamage < static_cast<float>(static_cast<int32_t>(type->forestDmgLevel)) || threshold <= newDamage))
            {
                GameObject* fire = createObject(static_cast<int32_t>(type->forestFireFX));

                if (fire != nullptr)
                {
                    fire->setPosition(position);
                    fireObject = static_cast<Fire*>(fire);
                    fire->setPotentialContact(3);
                    fireObject->burningObject = this;
                    fireObject->setTonnage(40.0f);
                    fireObject->update();
                }
            }

            if (fireObject != nullptr)
            {
                fireObject->addTimeLeftToBurn(1.0f);
            }
            break;
        }

        case MISC_LIGHT_WALL:
        {
            if (static_cast<float>(static_cast<int32_t>(type->lightWallDmgLevel)) <= newDamage)
            {
                objType->createExplosion(position, 0.0f, 0.0f);
                status = 2;
                soundSystem->playDigitalSample(0x48, 1, this, 0, 0);
            }
            break;
        }
        default:
            break;
    }

    setDamage(newDamage);
    return 0;
}

auto MiscTerrainObject::isRevealed() -> int
{
    const auto col = static_cast<uint32_t>((blockNumber % Terrain::blocksMapSide) * Terrain::verticesBlockSide +
                                           vertexNumber % Terrain::verticesBlockSide);
    const auto row = static_cast<uint32_t>((blockNumber / Terrain::blocksMapSide) * Terrain::verticesBlockSide +
                                           vertexNumber / Terrain::verticesBlockSide);
    ByteFlag* visibleBits = homeTeam->alignment == -1 ? Terrain::ClanVisibleBits : Terrain::terrainVisibleBits;

    if (visibleBits->getFlag(row, col) != 0)
    {
        return 1;
    }

    if (visibleBits->getFlag(row, col + 1) != 0)
    {
        return 1;
    }

    if (visibleBits->getFlag(row + 1, col + 1) != 0)
    {
        return 1;
    }

    return visibleBits->getFlag(row + 1, col) != 0 ? 1 : 0;
}
