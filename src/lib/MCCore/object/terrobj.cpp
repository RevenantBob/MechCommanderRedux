#include "stdafx.h"
#include "object/terrobj.h"
#include "ai/move.h"
#include "appear/apprtype.h"
#include "camera/camera.h"
#include "color/color.h"
#include "engine/bitflag.h"
#include "engine/ceglist.h"
#include "engine/cellip.h"
#include "lib/aerror.h"
#include "lib/cvmath.h"
#include "lib/file.h"
#include "lib/inifile.h"
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

auto TerrainObjectType::init() -> void
{
    ObjectType::init();
    dmgLevel = 0;
    collisionOffsetY = 0;
    collisionOffsetX = 0;
    basePixelOffsetY = 0;
    basePixelOffsetX = 0;
    setImpassable = 0;
    yImpasse = 0;
    xImpasse = 0;
    explDmg = 0.0f;
    explRad = 0.0f;
}

auto TerrainObjectType::createInstance() -> BaseObject*
{
    auto* newObject = new TerrainObject;

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

auto TerrainObjectType::destroy() -> void
{
}

auto TerrainObjectType::init(File* objFile, uint32_t fileSize) -> int32_t
{
    FitIniFile objectFile;
    int32_t result = objectFile.open(objFile, fileSize, 50);

    if (result != 0)
    {
        return result;
    }

    if ((result = objectFile.seekBlock("TerrainObjectData")) != 0)
    {
        return result;
    }

    if ((result = objectFile.readIdULong("DmgLevel", dmgLevel)) != 0)
    {
        return result;
    }

    if (objectFile.readIdLong("BasePixelOffsetX", basePixelOffsetX) != 0)
    {
        basePixelOffsetX = 0;
    }

    if (objectFile.readIdLong("BasePixelOffsetY", basePixelOffsetY) != 0)
    {
        basePixelOffsetY = 0;
    }

    if (objectFile.readIdLong("CollisionOffsetX", collisionOffsetX) != 0)
    {
        collisionOffsetX = 0;
    }

    if (objectFile.readIdLong("CollisionOffsetY", collisionOffsetY) != 0)
    {
        collisionOffsetY = 0;
    }

    if (objectFile.readIdLong("SetImpassable", setImpassable) != 0)
    {
        setImpassable = 0;
    }

    if (objectFile.readIdLong("XImpasse", xImpasse) != 0)
    {
        xImpasse = 0;
    }

    if (objectFile.readIdLong("YImpasse", yImpasse) != 0)
    {
        yImpasse = 0;
    }

    // No ExtentRadius: -1, measured from the appearance by the first object's update.
    float radius = 0.0f;

    if (objectFile.readIdFloat("ExtentRadius", radius) != 0)
    {
        radius = -1.0f;
    }

    if (objectFile.readIdFloat("ExplosionRadius", explRad) != 0)
    {
        explRad = 0.0f;
    }

    if (objectFile.readIdFloat("ExplosionDamage", explDmg) != 0)
    {
        explDmg = 0.0f;
    }

    result = ObjectType::init(&objectFile);
    extentRadius = radius;
    return result;
}

auto TerrainObjectType::handleCollision(GameObject* collidee, GameObject* collider) -> int
{
    // A mover (not artillery) running into it deals it 10 points; the server's job in multiplayer.
    if ((MPlayer == nullptr || MPlayer->isServer != 0) && collider->objectClass < MOVER &&
        collider->objectClass != ARTILLERY)
    {
        _WeaponShotInfo shot;
        shot.init(nullptr, -1, 10.0f, 0, 0.0f);
        collidee->handleWeaponHit(&shot, MPlayer != nullptr ? 1 : 0);
    }

    return 1;
}

auto TerrainObjectType::handleDestruction(GameObject*, GameObject*) -> int
{
    return 0;
}

//---------------------------------------------------------------------------
// TerrainObject
//---------------------------------------------------------------------------

TerrainObject::TerrainObject()
{
    justCreated = 1;
    appearance = nullptr;
    pixelOffsetY = 0;
    pixelOffsetX = 0;
    vertexNumber = 0;
    blockNumber = 0;
    fireObject = nullptr;
    unknown9C = 0;
    burning = 0;
    unknownA0 = 500000;
}

auto TerrainObject::init() -> void
{
}

auto TerrainObject::setTerrainPosition(vector_2d& offset, vector_2d& numbers) -> void
{
    pixelOffsetX = static_cast<int32_t>(offset.x);
    pixelOffsetY = static_cast<int32_t>(offset.y);
    const auto* objectType = static_cast<TerrainObjectType*>(objType);

    if (objectType->basePixelOffsetX != 0)
    {
        pixelOffsetX = objectType->basePixelOffsetX;
    }

    if (objectType->basePixelOffsetY != 0)
    {
        pixelOffsetY = objectType->basePixelOffsetY;
    }

    vertexNumber = static_cast<int32_t>(numbers.x);
    blockNumber = static_cast<int32_t>(numbers.y);
}

auto TerrainObject::getBlockAndVertexNumber(int32_t& blockNum, int32_t& vertexNum) -> void
{
    blockNum = blockNumber;
    vertexNum = vertexNumber;
}

auto TerrainObject::isVisible(Camera* cam) -> int
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

auto TerrainObject::update() -> int32_t
{
    if (justCreated == 0)
    {
        return 1;
    }

    // Set the object on its vertex: the block's corner, the vertex within it, then the pixel offset within the
    // tile (turned into the isometric grid's 60-degree axes). The type's collision offset replaces the pixel offset.
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
    const auto* objectType = static_cast<TerrainObjectType*>(objType);
    int32_t offsetXPixels = pixelOffsetX;

    if (objectType->collisionOffsetX != 0)
    {
        offsetXPixels = objectType->collisionOffsetX;
    }

    int32_t offsetYPixels = pixelOffsetY;

    if (objectType->collisionOffsetY != 0)
    {
        offsetYPixels = objectType->collisionOffsetY;
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

    cellColumn = (blockNumber % Terrain::blocksMapSide) * verticesBlockSide + vertexNumber % verticesBlockSide;
    const int32_t halfMap = (verticesBlockSide * Terrain::blocksMapSide) >> 1;
    vertexWorldX = static_cast<float>(cellColumn - halfMap) * Terrain::metersPerVertex;
    cellRow = vertexNumber / verticesBlockSide + (blockNumber / Terrain::blocksMapSide) * verticesBlockSide;
    vertexWorldY = static_cast<float>(halfMap - cellRow) * Terrain::metersPerVertex;
    const auto inBounds = [&]
    { return cellRow < 0 || GameMap->height <= cellRow || cellColumn < 0 || GameMap->width <= cellColumn ? 0u : 1u; };
    Assert(inBounds(), 0, " terrobj MapTile Out of Bounds ");
    Assert(inBounds(), 0, " Map Tile out of bounds ");
    const MapTile& tile = GameMap->map[GameMap->width * cellRow + cellColumn];
    const int32_t elevationLevel = static_cast<int32_t>((tile.cells >> 7) & 0x3f) + GameMap->baseElevation;
    cellElevation = static_cast<float>(elevationLevel) * Terrain::metersPerElevLevel;

    // No extent radius in the FIT: measure it (twice the appearance's diagonal) for the whole type.
    if (objType->extentRadius < 0.0f)
    {
        appearance->visible = 1;
        appearance->update();
        appearance->recalcBounds(eye);
        const float dx = appearance->upperLeft.x - appearance->lowerRight.x;
        const float dy = appearance->upperLeft.y - appearance->lowerRight.y;
        float radius = std::sqrt(dx * dx + dy * dy) / worldUnitsPerMeter;
        radius = radius + radius;

        if (static_cast<float>(CollisionSystem::gridRadius) < radius)
        {
            Fatal(static_cast<int32_t>(std::floor(static_cast<double>(radius))), " Object extent radius TOO large ");
        }

        objType->extentRadius = radius;
    }

    return 1;
}

auto TerrainObject::handleEvent(ObjectEvent* event) -> int32_t
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

auto TerrainObject::render() -> void
{
    if (justCreated != 0)
    {
        return;
    }

    const int visibleNow = isVisible(eye);

    if (appearance != nullptr)
    {
        appearance->visible = visibleNow;
        appearance->update();
    }

    if (windowsVisible != turn)
    {
        return;
    }

    // Hazed by how many corners of its vertex square the home team sees; drawn when any is. (The original also
    // reads each corner's seen bit and drops it.)
    const auto row = static_cast<uint32_t>(cellRow);
    const auto col = static_cast<uint32_t>(cellColumn);
    ByteFlag* visibleBits = homeTeam->alignment == -1 ? Terrain::ClanVisibleBits : Terrain::terrainVisibleBits;
    int32_t numVisible = 0;

    if (visibleBits->getFlag(row, col) != 0)
    {
        numVisible = 1;
    }

    if (visibleBits->getFlag(row + 1, col) != 0)
    {
        numVisible++;
    }

    if (visibleBits->getFlag(row + 1, col + 1) != 0)
    {
        numVisible++;
    }

    if (visibleBits->getFlag(row, col + 1) != 0)
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

    static_cast<VFXAppearance*>(appearance)->fadeTable = hazePalette;

    if (justCreated == 0 && numVisible != 0)
    {
        appearance->render(0);

        if (fireObject != nullptr)
        {
            fireObject->render();
        }
    }

    if (drawExtents != 0)
    {
        // Debug: the extent radius as an ellipse.
        vector_2d size;
        size.x = eye->cosAngle * objType->extentRadius;
        size.y = eye->sinAngle * objType->extentRadius;

        if (eye->cameraScale == 1)
        {
            size.x *= 0.5f;
            size.y *= 0.5f;
        }

        const float scale = eye->cameraScale != 1 ? 1.0f : 0.5f;
        const float sx = (position.x - eye->position.x) * scale;
        const float sy = (position.y - eye->position.y) * scale;
        vector_2d center;
        center.x = sx * eye->cosAngle + sy * eye->cosAngle + eye->halfWidth;
        center.y =
            ((sx * eye->sinAngle + eye->halfHeight) - sy * eye->sinAngle) - scale * (position.z - eye->position.z);
        ElementList->openGroup(-50000, 1);
        ElementList->add(new EllipseElement(center, size, 0xfe, -50000));
    }
}

auto TerrainObject::destroy() -> void
{
    delete appearance;
    appearance = nullptr;
}

auto TerrainObject::setDamage(int32_t newDamage) -> void
{
    damage = static_cast<float>(newDamage);
}

auto TerrainObject::init(ObjectType* objType) -> int32_t
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

    auto* vfxAppearance = new VFXAppearance;
    appearance = vfxAppearance;

    if (vfxAppearance == nullptr)
    {
        return static_cast<int32_t>(0xdcdc0008);
    }

    vfxAppearance->init(nullptr, nullptr);

    if ((apprType->appearanceNum & 0xff000000) != 0x2000000)
    {
        return static_cast<int32_t>(0xdcdc0009);
    }

    if ((result = vfxAppearance->init(apprType, this)) != 0)
    {
        return result;
    }

    objectClass = TERRAINOBJECT;

    if (0.0f < this->objType->extentRadius)
    {
        collisionsOn = 1;
    }

    // A DmgLevel of 0 means it starts destroyed.
    if (static_cast<TerrainObjectType*>(this->objType)->dmgLevel == 0)
    {
        collisionsOn = 0;
        status = 2;
    }

    return 0;
}

auto TerrainObject::lightOnFire(float timeToBurn) -> void
{
    if (fireObject == nullptr && objType->explosionObject != -1)
    {
        auto* fire = static_cast<Fire*>(createObject(objType->explosionObject));

        if (fire != nullptr)
        {
            fireObject = fire;
            fire->setPotentialContact(3);
            fireObject->burningObject = this;
            fireObject->setTonnage(40.0f);
            fireObject->setPosition(position);
        }
    }

    if (fireObject != nullptr)
    {
        fireObject->addTimeLeftToBurn(timeToBurn);
        burning = 1;
    }
}

auto TerrainObject::handleWeaponHit(_WeaponShotInfo* shotInfo, int addMultiplayChunk) -> int32_t
{
    if (shotInfo == nullptr)
    {
        return 0;
    }

    if (addMultiplayChunk != 0)
    {
        MPlayer->addWeaponHitChunk(this, shotInfo, 0);
    }

    float newDamage = getDamage() + shotInfo->damage;
    const auto maxDamage = static_cast<float>(static_cast<int32_t>(static_cast<TerrainObjectType*>(objType)->dmgLevel));

    if (maxDamage < newDamage)
    {
        newDamage = maxDamage;
    }

    setDamage(static_cast<int32_t>(newDamage));

    // A hit sets it burning (10 seconds), or keeps a fire going (2 more).
    if (burning == 0)
    {
        if (objType->explosionObject != -1)
        {
            lightOnFire(10.0f);
        }
    }
    else
    {
        lightOnFire(2.0f);
    }

    return 0;
}
