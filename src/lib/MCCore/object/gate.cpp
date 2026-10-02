#include "stdafx.h"
#include "object/gate.h"
#include "ai/move.h"
#include "appear/apprtype.h"
#include "camera/camera.h"
#include "color/color.h"
#include "engine/bitflag.h"
#include "engine/ceglist.h"
#include "engine/cellip.h"
#include "gui/asystem.h"
#include "lib/aerror.h"
#include "lib/cvmath.h"
#include "lib/file.h"
#include "lib/heap.h"
#include "lib/inifile.h"
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
#include "sprite/puactor.h"
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
    enum class GateKind
    {
        None,
        /// <summary>Types 0x284, 0x287, 0x2b2, 0x2b3, 0x403, 0x405.</summary>
        KindA,
        /// <summary>Types 0x285, 0x286, 0x2b4, 0x2b5, 0x404, 0x406.</summary>
        KindB
    };

    GateKind gateKind(int32_t objTypeNum)
    {
        switch (objTypeNum)
        {
            case 0x284:
            case 0x287:
            case 0x2b2:
            case 0x2b3:
            case 0x403:
            case 0x405:
                return GateKind::KindA;
            case 0x285:
            case 0x286:
            case 0x2b4:
            case 0x2b5:
            case 0x404:
            case 0x406:
                return GateKind::KindB;
            default:
                return GateKind::None;
        }
    }

    /// <summary>The map row and column of a gate's terrain vertex (used as its tile).</summary>
    void vertexRowCol(const Gate* gate, int32_t& row, int32_t& col)
    {
        col = (gate->blockNumber % Terrain::blocksMapSide) * Terrain::verticesBlockSide +
              gate->vertexNumber % Terrain::verticesBlockSide;
        row = (gate->blockNumber / Terrain::blocksMapSide) * Terrain::verticesBlockSide +
              gate->vertexNumber / Terrain::verticesBlockSide;
    }

    /// <summary>
    /// Sets the gate tile's overlay type (the masked form MCX.EXE writes) and each of its nine cells' passable and
    /// line-of-sight bits.
    /// </summary>
    void setGateTile(int32_t row, int32_t col, uint32_t keepMask, uint32_t overlayBits, uint32_t lineOfSight,
                     uint32_t passable)
    {
        MapTile& tile = GameMap->map[GameMap->width * row + col];
        tile.overlay = (tile.overlay & keepMask) | overlayBits;

        for (uint32_t shift = 0; shift < 0x12; shift += 2)
        {
            tile.cells = (lineOfSight << (shift + 0xf)) | (~(0x8000u << shift) & tile.cells);
            tile.cells = (~(0x4000u << shift) & tile.cells) | (passable << (shift + 0xe));
        }
    }

    /// <summary>
    /// Takes the pop-up appearance's answer to a combat mode request: open (2), opening (1, with the opening sound
    /// when it was closed), closing (3, with the closing sound when it was open) or closed (0).
    /// </summary>
    /// <returns>True when the gate is now closed.</returns>
    bool takeGateState(Gate* gate, int32_t state)
    {
        switch (state)
        {
            case 2:
            {
                gate->isOpen = 1;
                gate->isOpening = 0;
                gate->isClosing = 0;
                gate->isClosed = 0;
                return false;
            }
            case 1:
            {
                if (gate->isClosed != 0)
                {
                    soundSystem->playDigitalSample(0x2a, 1, gate, 0, 0);
                }

                gate->isOpening = 1;
                gate->isOpen = 0;
                gate->isClosing = 0;
                gate->isClosed = 0;
                return false;
            }
            case 3:
            {
                if (gate->isOpen != 0)
                {
                    soundSystem->playDigitalSample(0x2d, 1, gate, 0, 0);
                }

                gate->isClosing = 1;
                gate->isOpen = 0;
                gate->isOpening = 0;
                gate->isClosed = 0;
                return false;
            }
            case 0:
            {
                gate->isClosed = 1;
                gate->isOpen = 0;
                gate->isOpening = 0;
                gate->isClosing = 0;
                return true;
            }
            default:
                return false;
        }
    }

    /// <summary>Makes the gate's fire (the type's blown effect); anything that isn't a fire goes in the object list.</summary>
    void startFire(Gate* gate, bool listNonFire)
    {
        GameObject* effect = createObject(static_cast<int32_t>(static_cast<GateType*>(gate->objType)->blownEffectId));

        if (effect == nullptr)
        {
            return;
        }

        effect->setPosition(gate->position);

        if (effect->objectClass == FIRE)
        {
            gate->fireObject = static_cast<Fire*>(effect);
            effect->setPotentialContact(3);
            gate->fireObject->burningObject = gate;
            gate->fireObject->setTonnage(40.0f);

            if (listNonFire)
            {
                gate->fireObject->update();
                gate->fireStarted = 1;
            }
        }
        else if (listNonFire)
        {
            if (objectList->head != nullptr)
            {
                objectList->head->addNode(effect);
            }
        }
        else
        {
            destroyObject(effect);
        }
    }
} // namespace

//---------------------------------------------------------------------------
// GateType
//---------------------------------------------------------------------------

auto GateType::init() -> void
{
    ObjectType::init();
    dmgLevel = 0;
    blownEffectId = 0xffffffff;
    normalEffectId = 0xffffffff;
    damageEffectId = 0xffffffff;
    explosionRadius = 0.0f;
    explosionDamage = 0.0f;
    unknown40 = 0;
    buildingName = 0;
}

auto GateType::createInstance() -> BaseObject*
{
    auto* newGate = new Gate;

    if (newGate == nullptr)
    {
        return nullptr;
    }

    if (newGate->init(this) != 0)
    {
        return nullptr;
    }

    newGate->idNumber = NextIdNumber++;
    return newGate;
}

auto GateType::destroy() -> void
{
}

auto GateType::init(File* objFile, uint32_t fileSize) -> int32_t
{
    FitIniFile gateFile;
    int32_t result = gateFile.open(objFile, fileSize, 50);

    if (result != 0)
    {
        return result;
    }

    if ((result = gateFile.seekBlock("GateData")) != 0)
    {
        return result;
    }

    if ((result = gateFile.readIdULong("DmgLevel", dmgLevel)) != 0)
    {
        return result;
    }

    gateFile.readIdULong("BlownEffectId", blownEffectId);
    gateFile.readIdULong("NormalEffectId", normalEffectId);
    gateFile.readIdULong("DamageEffectId", damageEffectId);

    if (gateFile.readIdLong("BasePixelOffsetX", basePixelOffsetX) != 0)
    {
        basePixelOffsetX = 0;
    }

    if (gateFile.readIdLong("BasePixelOffsetY", basePixelOffsetY) != 0)
    {
        basePixelOffsetY = 0;
    }

    if (gateFile.readIdFloat("ExplosionRadius", explosionRadius) != 0)
    {
        explosionRadius = 0.0f;
    }

    if (gateFile.readIdFloat("ExplosionDamage", explosionDamage) != 0)
    {
        explosionDamage = 0.0f;
    }

    if ((result = gateFile.readIdFloat("OpenRadius", openRadius)) != 0)
    {
        return result;
    }

    if (gateFile.readIdFloat("LittleExtent", littleExtent) != 0)
    {
        littleExtent = 20.0f;
    }

    if (gateFile.readIdLong("BuildingName", buildingName) != 0)
    {
        buildingName = 0xa5;
    }

    if (gateFile.readIdBoolean("BlocksLineOfFire", blocksLineOfFire) != 0)
    {
        blocksLineOfFire = 0;
    }

    return ObjectType::init(&gateFile);
}

auto GateType::handleCollision(GameObject* collidee, GameObject* collider) -> int
{
    // Only mechs, vehicles and elementals open gates or get caught in them.
    if (collider->objectClass < BATTLEMECH || EXPLOSION <= collider->objectClass)
    {
        return 1;
    }

    auto* gate = static_cast<Gate*>(collidee);
    const vector_3d colliderPos = collider->getPosition();
    const vector_3d gatePos = gate->getPosition();
    const float distanceSq = (gatePos.x - colliderPos.x) * (gatePos.x - colliderPos.x) +
                             (gatePos.y - colliderPos.y) * (gatePos.y - colliderPos.y);

    // A friendly (or, for a neutral gate, any) live unit within openRadius asks it to open.
    if ((collider->getAlignment() == gate->getAlignment() || gate->getAlignment() == 0) &&
        collider->isDisabled() == 0 && collider->isDestroyed() == 0 && distanceSq < openRadius * openRadius)
    {
        gate->openRequested = 1;
    }

    // A live unit standing in it (not jumping) is what a closing gate crushes.
    if (distanceSq < 1.2e8f && collider->isDisabled() == 0 && collider->isDestroyed() == 0 &&
        static_cast<Mover*>(collider)->isJumping(nullptr) == 0)
    {
        gate->offendingObject = collider;
    }

    return 1;
}

auto GateType::handleDestruction(GameObject*, GameObject*) -> int
{
    return 0;
}

//---------------------------------------------------------------------------
// Gate
//---------------------------------------------------------------------------

Gate::Gate()
{
    justCreated = 1;
    openRequested = 1;
    isClosed = 1;
    appearance = nullptr;
    vertexNumber = 0;
    blockNumber = 0;
    fireStarted = 0;
    unknown9C = 0;
    unknownA0 = 500000;
    destroyed = 0;
    unknownD0 = 0;
    fireObject = nullptr;
    name = nullptr;
    lockedClosed = 0;
    blownOpen = 0;
    isOpen = 0;
    isOpening = 0;
    isClosing = 0;
    destroying = 0;
    offendingObject = nullptr;
}

auto Gate::init() -> void
{
}

auto Gate::killFireObject() -> void
{
    fireObject = nullptr;
}

auto Gate::setTerrainPosition(vector_2d& pixelOffset, vector_2d& blockVertex) -> void
{
    pixelOffsetX = static_cast<int32_t>(pixelOffset.x);
    pixelOffsetY = static_cast<int32_t>(pixelOffset.y);
    const auto* gateType = static_cast<GateType*>(objType);

    if (gateType->basePixelOffsetX != 0 || gateType->basePixelOffsetY != 0)
    {
        pixelOffsetX = gateType->basePixelOffsetX;
        pixelOffsetY = gateType->basePixelOffsetY;
    }

    vertexNumber = static_cast<int32_t>(blockVertex.x);
    blockNumber = static_cast<int32_t>(blockVertex.y);
}

auto Gate::getBlockAndVertexNumber(int32_t& blockNum, int32_t& vertexNum) -> void
{
    blockNum = blockNumber;
    vertexNum = vertexNumber;
}

auto Gate::isVisible(Camera* cam) -> int
{
    if (cam == nullptr || cam->active == 0)
    {
        return 0;
    }

    int visible = cam->vertexProject(blockNumber, vertexNumber, screenPos);

    if (appearance != nullptr)
    {
        visible = appearance->recalcBounds(cam);
    }

    if (visible == 0)
    {
        return 0;
    }

    windowsVisible = turn;
    return 1;
}

auto Gate::update() -> int32_t
{
    if (justCreated != 0)
    {
        // Set the gate on its vertex: the block's corner, the vertex within it, then the pixel offset within the
        // tile (turned into the isometric grid's 60-degree axes).
        const int32_t blocksMapSide = Terrain::blocksMapSide;
        const int32_t verticesBlockSide = Terrain::verticesBlockSide;
        justCreated = 0;
        float blockX = static_cast<float>(blockNumber % blocksMapSide - blocksMapSide / 2) * Terrain::metersBlockSide;
        float blockY = static_cast<float>(blocksMapSide / 2 - blockNumber / blocksMapSide) * Terrain::metersBlockSide;

        if ((blocksMapSide & 1) != 0)
        {
            blockX = blockX - Terrain::metersBlockSide * 0.5f;
            blockY = Terrain::metersBlockSide * 0.5f + blockY;
        }

        const float vertexX = static_cast<float>(vertexNumber % verticesBlockSide) * Terrain::metersPerVertex;
        const double offsetY = static_cast<double>(pixelOffsetY);
        const double offsetX = static_cast<double>(pixelOffsetX);
        double offsetAngle;

        if (offsetY == 0.0)
        {
            offsetAngle = 90.0;
        }
        else
        {
            offsetAngle = std::atan(offsetX / offsetY) * RADIANS_TO_DEGREES;
        }

        position.y = blockY - static_cast<float>(vertexNumber / verticesBlockSide) * Terrain::metersPerVertex;
        const auto offsetDistance = static_cast<float>(std::sqrt(offsetY * offsetY + offsetX * offsetX));
        const double axisAngle = (60.0 - offsetAngle) * DEGREES_TO_RADIANS;
        const auto alongAxis = static_cast<float>(std::sin(axisAngle) * offsetDistance / std::sin(SIXTY_DEGREES));
        position.x = vertexX + blockX;
        const float elevation = land->getTerrainElevation(position);
        position.x =
            static_cast<float>(std::cos(SIXTY_DEGREES) * alongAxis + std::cos(axisAngle) * offsetDistance + position.x);
        position.y = position.y - alongAxis;
        position.z = elevation;

        tileCol = (blockNumber % Terrain::blocksMapSide) * verticesBlockSide + vertexNumber % verticesBlockSide;
        const int32_t halfMap = (verticesBlockSide * Terrain::blocksMapSide) >> 1;
        tileWorldX = static_cast<float>(tileCol - halfMap) * Terrain::metersPerVertex;
        tileRow = vertexNumber / verticesBlockSide + (blockNumber / Terrain::blocksMapSide) * verticesBlockSide;
        tileWorldY = static_cast<float>(halfMap - tileRow) * Terrain::metersPerVertex;
        const auto inBounds = [&]
        { return tileRow < 0 || GameMap->height <= tileRow || tileCol < 0 || GameMap->width <= tileCol ? 0u : 1u; };
        Assert(inBounds(), 0, " tbldg MapTile Out of Bounds ");
        Assert(inBounds(), 0, " Map Tile out of bounds ");
        const MapTile& tile = GameMap->map[GameMap->width * tileRow + tileCol];
        const int32_t elevationLevel = static_cast<int32_t>((tile.cells >> 7) & 0x3f) + GameMap->baseElevation;
        appearance->visible = 1;
        tileElevation = static_cast<float>(elevationLevel) * Terrain::metersPerElevLevel;
        appearance->update();
        appearance->recalcBounds(eye);
    }

    if (destroyed == 0)
    {
        openGate();
        offendingObject = nullptr;
    }

    return 1;
}

auto Gate::blowAnyOffendingObject() -> void
{
    // Whatever is caught in a gate as it shuts takes 10 hits of 250, and the gate is destroyed.
    if (MPlayer != nullptr && MPlayer->isServer == 0)
    {
        return;
    }

    GameObject* offender = offendingObject;

    if (offender == nullptr)
    {
        return;
    }

    const vector_3d offenderPos = offender->getPosition();
    const vector_3d gatePos = getPosition();
    const float dx = gatePos.x - offenderPos.x;
    const float dy = gatePos.y - offenderPos.y;
    const double reach =
        static_cast<double>(static_cast<GateType*>(objType)->littleExtent) + offender->getObjectType()->extentRadius;

    if (reach * reach <= static_cast<double>(dy) * dy + static_cast<double>(dx) * dx)
    {
        return;
    }

    const int multiplayer = MPlayer != nullptr ? 1 : 0;
    _WeaponShotInfo shot;
    shot.init(nullptr, -3, 250.0f, 0, 0.0f);

    for (int32_t i = 0; i < 10; i++)
    {
        shot.hitLocation = offender->calcHitLocation(nullptr, -1, 4, 0);
        offender->handleWeaponHit(&shot, multiplayer);
    }

    const auto gateDamage = static_cast<float>(static_cast<int32_t>(static_cast<GateType*>(objType)->dmgLevel + 5));
    shot.init(nullptr, -3, gateDamage, 0, 0.0f);
    handleWeaponHit(&shot, multiplayer);
}

auto Gate::openGate() -> void
{
    if (destroyed != 0)
    {
        return;
    }

    int32_t row;
    int32_t col;
    vertexRowCol(this, row, col);

    // Ask the pop-up appearance to open or shut: locked (or forced) gates shut; blown open or neutral gates open;
    // others follow openRequested. A gate that has just shut crushes whatever is in it (unless blown or neutral).
    if (lockedClosed == 0 && forceGatesClosed == 0)
    {
        if (blownOpen == 0 && alignment != 0)
        {
            if (takeGateState(this, static_cast<PUAppearance*>(appearance)->setCombatMode(openRequested)))
            {
                blowAnyOffendingObject();
            }
        }
        else
        {
            takeGateState(this, static_cast<PUAppearance*>(appearance)->setCombatMode(1));
        }
    }
    else if (takeGateState(this, static_cast<PUAppearance*>(appearance)->setCombatMode(0)))
    {
        blowAnyOffendingObject();
    }

    // Open: the tile is passable and see-through. Shut: blocked (unless locked open) and see-through only when the
    // type doesn't block line of fire. The overlay type depends on the gate's shape and whose it is.
    const GateKind kind = gateKind(getObjectType()->objTypeNum);

    if (isOpen != 0)
    {
        const bool clan = alignment != 1 && alignment != 0;

        if (kind == GateKind::KindA)
        {
            if (clan)
            {
                setGateTile(row, col, 0xffffffc5, 0x45, 1, 1);
            }
            else
            {
                setGateTile(row, col, 0xffffffc9, 0x49, 1, 1);
            }
        }
        else if (kind == GateKind::KindB)
        {
            if (clan)
            {
                setGateTile(row, col, 0xffffffc3, 0x43, 1, 1);
            }
            else
            {
                setGateTile(row, col, 0xffffffc7, 0x47, 1, 1);
            }
        }

        openRequested = 0;
        return;
    }

    const uint32_t lineOfSight = static_cast<GateType*>(objType)->blocksLineOfFire == 0 ? 1 : 0;
    const uint32_t passable = lockedClosed == 0 ? 1 : 0;

    if (kind == GateKind::KindA)
    {
        if (alignment == 1)
        {
            setGateTile(row, col, 0xffffffca, 0x4a, lineOfSight, passable);
        }
        else
        {
            setGateTile(row, col, 0xffffffc6, 0x46, lineOfSight, passable);
        }
    }
    else if (kind == GateKind::KindB)
    {
        if (alignment == 1)
        {
            setGateTile(row, col, 0xffffffc8, 0x48, lineOfSight, passable);
        }
        else
        {
            setGateTile(row, col, 0xffffffc4, 0x44, lineOfSight, passable);
        }
    }

    openRequested = 0;
}

auto Gate::setAlignment(int32_t newAlignment) -> void
{
    BigGameObject::setAlignment(newAlignment);
}

auto Gate::handleEvent(ObjectEvent* event) -> int32_t
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

auto Gate::lightOnFire(float timeToBurn) -> void
{
    // A gate with no blown effect just takes a point of damage (the server's job in multiplayer).
    if (static_cast<int32_t>(static_cast<GateType*>(objType)->blownEffectId) == -1)
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

        return;
    }

    if (fireObject == nullptr)
    {
        startFire(this, false);
    }

    if (fireObject != nullptr)
    {
        fireObject->addTimeLeftToBurn(timeToBurn);
        fireStarted = 1;
    }
}

auto Gate::isRevealed() -> int
{
    ByteFlag* visibleBits = homeTeam->alignment == -1 ? Terrain::ClanVisibleBits : Terrain::terrainVisibleBits;
    int32_t row;
    int32_t col;
    vertexRowCol(this, row, col);
    const auto r = static_cast<uint32_t>(row);
    const auto c = static_cast<uint32_t>(col);

    if (visibleBits->getFlag(r, c) != 0)
    {
        return 1;
    }

    if (visibleBits->getFlag(r + 1, c) != 0)
    {
        return 1;
    }

    if (visibleBits->getFlag(r + 1, c + 1) != 0)
    {
        return 1;
    }

    return visibleBits->getFlag(r, c + 1) != 0 ? 1 : 0;
}

auto Gate::render() -> void
{
    if (justCreated != 0)
    {
        return;
    }

    if (appearance != nullptr)
    {
        appearance->visible = isVisible(eye);
        appearance->update();
    }

    if (windowsVisible != turn)
    {
        return;
    }

    // Hazed by how many corners of its vertex square the home team sees; drawn when any is. (The original also
    // reads each corner's seen bit and drops it.)
    int32_t row;
    int32_t col;
    vertexRowCol(this, row, col);
    const auto r = static_cast<uint32_t>(row);
    const auto c = static_cast<uint32_t>(col);
    ByteFlag* visibleBits = homeTeam->alignment == -1 ? Terrain::ClanVisibleBits : Terrain::terrainVisibleBits;
    int32_t numVisible = 0;

    if (visibleBits->getFlag(r, c) != 0)
    {
        numVisible = 1;
    }

    if (visibleBits->getFlag(r + 1, c) != 0)
    {
        numVisible++;
    }

    if (visibleBits->getFlag(r + 1, c + 1) != 0)
    {
        numVisible++;
    }

    if (visibleBits->getFlag(r, c + 1) != 0)
    {
        numVisible++;
    }

    uint8_t* hazePalette = nullptr;
    const int32_t hazeLevel = eye->hazeLevel;

    if (numVisible != 0 && numVisible != 4 && hazeLevel != 0x7fff)
    {
        int32_t level;

        if (hazeLevel < 0 && 0 < eye->hazeInc * numVisible + hazeLevel)
        {
            level = 0;
        }
        else
        {
            level = hazeLevel + eye->hazeInc * numVisible;
        }

        hazePalette = gamePalette->getHazePalette(level);
    }

    static_cast<PUAppearance*>(appearance)->hazePalette = hazePalette;

    if (numVisible != 0)
    {
        appearance->render(0);
    }

    if (drawExtents != 0)
    {
        // Debug: the extent radius as an ellipse.
        float radius = objType->extentRadius;

        if (eye->cameraScale == 1)
        {
            radius *= 0.5f;
        }

        const float scale = eye->cameraScale != 1 ? 1.0f : 0.5f;
        const float sx = (position.x - eye->position.x) * scale;
        const float sy = (position.y - eye->position.y) * scale;
        vector_2d center;
        center.x = sx * eye->cosAngle + sy * eye->cosAngle + eye->halfWidth;
        center.y =
            ((sx * eye->sinAngle + eye->halfHeight) - sy * eye->sinAngle) - scale * (position.z - eye->position.z);
        vector_2d size(radius, radius);
        ElementList->openGroup(-50000, 1);
        ElementList->add(new EllipseElement(center, size, 0xfe, -50000));
    }
}

auto Gate::destroy() -> void
{
    delete appearance;
    appearance = nullptr;
    systemHeap->free(name);
    name = nullptr;
}

auto Gate::init(ObjectType* objType) -> int32_t
{
    int32_t result = GameObject::init(objType);

    if (result != 0)
    {
        return result;
    }

    justCreated = 1;
    AppearanceType* apprType = appearanceTypeList->getAppearance(objType->appearName, 0);

    if (apprType == nullptr)
    {
        return static_cast<int32_t>(0xdcdc0007);
    }

    auto* popUpAppearance = new PUAppearance;
    appearance = popUpAppearance;

    if (popUpAppearance == nullptr)
    {
        return -0x2ffff;
    }

    popUpAppearance->init(nullptr, nullptr);

    if ((apprType->appearanceNum & 0xff000000) != 0x9000000)
    {
        return -0x2fff6;
    }

    if ((result = popUpAppearance->init(apprType, this)) != 0)
    {
        return result;
    }

    auto* gateType = static_cast<GateType*>(this->objType);
    objectClass = GATE;
    destroyed = 0;
    alignment = -1;

    // The open radius doubles as the extent the collision system checks.
    if (gateType->openRadius != 0.0f)
    {
        gateType->extentRadius = gateType->openRadius;
    }

    if (0.0f < gateType->extentRadius)
    {
        collisionsOn = 1;
    }

    explDamage = gateType->explosionDamage;
    explRadius = gateType->explosionRadius;
    char nameBuffer[256];
    cLoadString(thisInstance, static_cast<uint32_t>(gateType->buildingName), nameBuffer, 0xfe);
    name = static_cast<char*>(systemHeap->malloc(static_cast<uint32_t>(std::strlen(nameBuffer) + 1)));
    std::strcpy(name, nameBuffer);
    return 0;
}

auto Gate::handleWeaponHit(_WeaponShotInfo* shotInfo, int addMultiplayChunk) -> int32_t
{
    if (shotInfo == nullptr)
    {
        return 0;
    }

    if (addMultiplayChunk != 0)
    {
        MPlayer->addWeaponHitChunk(this, shotInfo, 0);
    }

    const float newDamage = getDamage() + shotInfo->damage;
    setDamage(newDamage);

    if (static_cast<float>(static_cast<int32_t>(static_cast<GateType*>(objType)->dmgLevel)) <= newDamage)
    {
        destroyGate(0);
    }

    return 0;
}

auto Gate::destroyGate(int fromNetwork) -> void
{
    destroyed = 1;
    destroying = 1;
    auto* popUpAppearance = static_cast<PUAppearance*>(appearance);

    if (popUpAppearance != nullptr)
    {
        popUpAppearance->visible = onScreen();
        popUpAppearance->update();
    }

    // Port fix: the original calls setDestroyed on a null appearance.
    if (popUpAppearance != nullptr)
    {
        popUpAppearance->setDestroyed();
    }

    // Blown open: the tile is passable and see-through for good (the network copy already got the map change).
    int32_t row;
    int32_t col;
    vertexRowCol(this, row, col);

    if (fromNetwork == 0)
    {
        const GateKind kind = gateKind(getObjectType()->objTypeNum);

        if (kind == GateKind::KindA)
        {
            if (alignment == 1)
            {
                setGateTile(row, col, 0xffffffc9, 0x49, 1, 1);
            }
            else
            {
                setGateTile(row, col, 0xffffffc5, 0x45, 1, 1);
            }
        }
        else if (kind == GateKind::KindB)
        {
            if (alignment == 1)
            {
                setGateTile(row, col, 0xffffffc7, 0x47, 1, 1);
            }
            else
            {
                setGateTile(row, col, 0xffffffc3, 0x43, 1, 1);
            }
        }
    }

    isOpen = 1;
    isOpening = 0;
    isClosing = 0;
    isClosed = 0;
    blownOpen = 1;
    destroying = 0;
    collisionsOn = 0;
    status = 2;

    // Set it burning (or keep a fire going) and blow it up.
    if (fireStarted == 0 && fromNetwork == 0)
    {
        if (static_cast<int32_t>(static_cast<GateType*>(objType)->blownEffectId) == -1)
        {
            if (fireObject != nullptr)
            {
                fireObject->addTimeLeftToBurn(2.0f);
            }
        }
        else
        {
            startFire(this, true);
        }

        objType->createExplosion(position, explDamage, explRadius);
    }
}
