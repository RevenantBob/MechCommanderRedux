#include "stdafx.h"
#include "object/bldng.h"
#include "appear/apprtype.h"
#include "camera/camera.h"
#include "color/color.h"
#include "engine/bitflag.h"
#include "engine/ceglist.h"
#include "engine/cellip.h"
#include "engine/cevfx.h"
#include "lib/aerror.h"
#include "lib/cident.h"
#include "lib/cvmath.h"
#include "lib/heap.h"
#include "lib/inifile.h"
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
#include "sprite/bactor.h"
#include "sprite/lactor.h"
#include "terrain/terrain.h"
#include "terrain/terrmap.h"
#include "vfx/vfxfuncs.h"

/// <remarks>MCX.EXE @ 0x0078fcfc</remarks>
int32_t DefaultPilotId = 0x28d;
/// <remarks>MCX.EXE @ 0x0078fd00</remarks>
char marineProfileName[80] = "PEM00001";
int drawExtents = 0;
/// <remarks>MCX.EXE @ 0x007de524</remarks>
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
    GameObject* createBlownEffect(Building* building, int32_t effectId)
    {
        GameObject* effect = createObject(effectId);

        if (effect == nullptr)
        {
            return nullptr;
        }

        effect->setPosition(building->position);

        if (effect->objectClass == FIRE)
        {
            building->fireObject = static_cast<Fire*>(effect);
            building->fireObject->setPotentialContact(3);
            building->fireObject->burningObject = building;
            building->fireObject->setTonnage(40.0f);
        }

        return effect;
    }
}

//---------------------------------------------------------------------------
// BuildingType
//---------------------------------------------------------------------------

auto BuildingType::init() -> void
{
    typeClass = -1;
    destroyedObject = -1;
    explosionObject = -1;
    unknown18 = 0;
    appearName = 0;
    extentRadius = 0.0f;
    keepMe = 0;
    iconNumber = -1;
    dmgLevel = 0;
    blownEffectId = 0xffffffff;
    normalEffectId = 0xffffffff;
    damageEffectId = 0xffffffff;
    sensorRange = -1.0f;
    teamId = -1;
    explRad = 0.0f;
    explDmg = 0.0f;
    baseTonnage = 0.0f;
    timeToBurnDamage = 0.0f;
    burnDamagePerTime = 0.0f;
    damageLvlForBurn = 0.0f;
    buildingName = 0;
    battleRating = 0;
    numMarines = 0;
}

auto BuildingType::createInstance() -> BaseObject*
{
    auto* newBuilding = new Building;

    if (newBuilding == nullptr)
    {
        return nullptr;
    }

    if (newBuilding->init(this) != 0)
    {
        return nullptr;
    }

    newBuilding->idNumber = NextIdNumber++;
    return newBuilding;
}

auto BuildingType::destroy() -> void
{
}

auto BuildingType::init(File* objFile, uint32_t fileSize) -> int32_t
{
    FitIniFile bldgFile;
    int32_t result = bldgFile.open(objFile, fileSize, 50);

    if (result != 0)
    {
        return result;
    }

    if ((result = bldgFile.seekBlock("BuildingData")) != 0)
    {
        return result;
    }

    if ((result = bldgFile.readIdULong("DmgLevel", dmgLevel)) != 0)
    {
        return result;
    }

    if ((result = bldgFile.readIdULong("BlownEffectId", blownEffectId)) != 0)
    {
        return result;
    }

    if (bldgFile.readIdULong("NormalEffectId", normalEffectId) != 0)
    {
        normalEffectId = 0xffffffff;
    }

    if ((result = bldgFile.readIdULong("DamageEffectId", damageEffectId)) != 0)
    {
        return result;
    }

    if (bldgFile.readIdLong("BasePixelOffsetX", basePixelOffsetX) != 0)
    {
        basePixelOffsetX = 0;
    }

    if (bldgFile.readIdLong("BasePixelOffsetY", basePixelOffsetY) != 0)
    {
        basePixelOffsetY = 0;
    }

    if (bldgFile.readIdLong("CollisionOffsetX", collisionOffsetX) != 0)
    {
        collisionOffsetX = 0;
    }

    if (bldgFile.readIdLong("CollisionOffsetY", collisionOffsetY) != 0)
    {
        collisionOffsetY = 0;
    }

    // -1 has update measure the extent from the appearance.
    float fitExtentRadius = 0.0f;

    if (bldgFile.readIdFloat("ExtentRadius", fitExtentRadius) != 0)
    {
        fitExtentRadius = -1.0f;
    }

    if (bldgFile.readIdFloat("Tonnage", baseTonnage) != 0)
    {
        baseTonnage = 20.0f;
    }

    if (bldgFile.readIdLong("BattleRating", battleRating) != 0)
    {
        battleRating = 20;
    }

    if (bldgFile.readIdLong("NumMarines", numMarines) != 0)
    {
        numMarines = 0;
    }

    if (bldgFile.readIdFloat("ExplosionRadius", explRad) != 0)
    {
        explRad = 0.0f;
    }

    if (bldgFile.readIdFloat("ExplosionDamage", explDmg) != 0)
    {
        explDmg = 0.0f;
    }

    if (bldgFile.readIdFloat("TimeToBurnDamage", timeToBurnDamage) != 0)
    {
        timeToBurnDamage = 5.0f;
    }

    if (bldgFile.readIdFloat("BurnDamagePerTime", burnDamagePerTime) != 0)
    {
        burnDamagePerTime = 1.0f;
    }

    if (bldgFile.readIdFloat("DamageLvlForBurn", damageLvlForBurn) != 0)
    {
        damageLvlForBurn = static_cast<float>(dmgLevel);
    }

    int32_t potentialContact = 0;
    bldgFile.readIdLong("PotentialContact", potentialContact);
    unknown18 = potentialContact == 1 ? 1 : 0;

    if (bldgFile.readIdLong("TeamID", teamId) != 0)
    {
        teamId = -1;
    }

    if (bldgFile.readIdFloat("SensorRange", sensorRange) != 0)
    {
        sensorRange = -1.0f;
    }

    if (bldgFile.readIdLong("BuildingName", buildingName) != 0)
    {
        buildingName = 0xa3;
    }

    result = ObjectType::init(&bldgFile);
    extentRadius = fitExtentRadius;
    return result;
}

auto BuildingType::handleCollision(GameObject* collidee, GameObject* collider) -> int
{
    if (MPlayer != nullptr && MPlayer->isServer == 0)
    {
        return 1;
    }

    // Movers (not artillery) that run into it do 10 points of damage.
    if (collider->objectClass < 8 && collider->objectClass != 7)
    {
        _WeaponShotInfo shot;
        shot.init(nullptr, -1, 10.0f, 0, 0.0f);

        if (scenarioTime <= collider->getCollisionFreeTime())
        {
            collidee->handleWeaponHit(&shot, MPlayer != nullptr ? 1 : 0);
        }
    }

    return 1;
}

auto BuildingType::handleDestruction(GameObject*, GameObject*) -> int
{
    return 0;
}

//---------------------------------------------------------------------------
// Building
//---------------------------------------------------------------------------

Building::Building()
{
    init();
    justCreated = 1;
    appearance = nullptr;
    pixelOffsetY = 0;
    pixelOffsetX = 0;
    vertexNumber = 0;
    blockNumber = 0;
    burning = 0;
    unknown9C = 0;
    unknownA0 = 500000;
    tileNum = 0;
    burnTime = 0.0f;
    captureable = 0;
    unknownE4 = 0;
    commanderId = static_cast<char>(0xff);
    name = nullptr;
    soundHandle = 0xffffffff;
    team = nullptr;
    fireObject = nullptr;
}

auto Building::init() -> void
{
    sensorSystem = nullptr;

    for (MechWarrior*& slot : prisonSlots)
    {
        slot = nullptr;
    }
}

auto Building::isPrison() -> int
{
    for (const MechWarrior* slot : prisonSlots)
    {
        if (slot != nullptr)
        {
            return 1;
        }
    }

    return 0;
}

auto Building::getBlockAndVertexNumber(int32_t& blockNum, int32_t& vertexNum) -> void
{
    blockNum = blockNumber;
    vertexNum = vertexNumber;
}

auto Building::setTerrainPosition(vector_2d& offset, vector_2d& numbers) -> void
{
    pixelOffsetX = static_cast<int32_t>(offset.x);
    pixelOffsetY = static_cast<int32_t>(offset.y);

    if (tileNum != 10)
    {
        auto* type = static_cast<BuildingType*>(objType);

        if (type->basePixelOffsetX != 0)
        {
            pixelOffsetX = type->basePixelOffsetX;
        }

        if (type->basePixelOffsetY != 0)
        {
            pixelOffsetY = type->basePixelOffsetY;
        }
    }

    vertexNumber = static_cast<int32_t>(numbers.x);
    blockNumber = static_cast<int32_t>(numbers.y);
}

auto Building::isVisible(Camera* cam) -> int
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

    // Back on screen after a gap: the looping sound is started afresh.
    if (windowsVisible < turn - 2)
    {
        soundHandle = 0xffffffff;
    }

    windowsVisible = turn;
    return 1;
}

auto Building::update() -> int32_t
{
    if (justCreated == 0)
    {
        return 1;
    }

    // Set the building on its vertex: the block's corner, the vertex within it, then the offset within the tile
    // (turned into the isometric grid's 60-degree axes). The type's collision offsets, when set, replace the pixel
    // offsets.
    auto* type = static_cast<BuildingType*>(objType);
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
    int32_t offsetXPixels = pixelOffsetX;

    if (static_cast<double>(type->collisionOffsetX) != 0.0)
    {
        offsetXPixels = type->collisionOffsetX;
    }

    int32_t offsetYPixels = pixelOffsetY;

    if (static_cast<float>(type->collisionOffsetY) != 0.0f)
    {
        offsetYPixels = type->collisionOffsetY;
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
    Assert(inBounds(), 0, " bldng MapTile Out of Bounds ");
    Assert(inBounds(), 0, " Map Tile out of bounds ");
    const MapTile& tile = GameMap->map[GameMap->width * cellRow + cellColumn];
    const int32_t elevationLevel = static_cast<int32_t>((tile.cells >> 7) & 0x3f) + GameMap->baseElevation;
    cellElevation = static_cast<float>(elevationLevel) * Terrain::metersPerElevLevel;

    // No extent radius in the FIT: measure it from the appearance's bounds.
    if (type->extentRadius < 0.0)
    {
        auto* buildingAppearance = static_cast<VFXBuildingAppearance*>(appearance);
        buildingAppearance->visible = 1;
        buildingAppearance->update();
        buildingAppearance->calcCollideBounds();
        const float dx = buildingAppearance->upperLeft.x - buildingAppearance->lowerRight.x;
        const float dy = buildingAppearance->upperLeft.y - buildingAppearance->lowerRight.y;
        const float radius = std::sqrt(dx * dx + dy * dy) / worldUnitsPerMeter * 1.25f;

        if (static_cast<float>(CollisionSystem::gridRadius) < radius)
        {
            Fatal(static_cast<int32_t>(std::floor(static_cast<double>(radius))), " Object extent radius TOO large ");
        }

        type->extentRadius = radius;
    }

    if (type->extentRadius != 0.0)
    {
        collisionsOn = 1;
    }

    return 1;
}

auto Building::setAlignment(int32_t align) -> void
{
    BigGameObject::setAlignment(align);

    if (sensorSystem == nullptr)
    {
        return;
    }

    if (alignment == -1)
    {
        sensorSystem->setTeam(clanTeam);
    }
    else if (alignment == 1)
    {
        sensorSystem->setTeam(innerSphereTeam);
    }
    else if (alignment == 0)
    {
        sensorSystem->setTeam(alliedTeam);
    }
}

auto Building::handleEvent(ObjectEvent* event) -> int32_t
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
        }
    }

    return 0;
}

auto Building::lightOnFire(float timeToBurn) -> void
{
    auto* type = static_cast<BuildingType*>(objType);

    if (type->blownEffectId == 0xffffffff)
    {
        return;
    }

    if (fireObject == nullptr)
    {
        GameObject* effect = createBlownEffect(this, static_cast<int32_t>(type->blownEffectId));

        if (effect != nullptr && effect->objectClass != FIRE)
        {
            destroyObject(effect);
        }
    }

    if (fireObject != nullptr)
    {
        fireObject->addTimeLeftToBurn(timeToBurn);
        burning = 1;
    }
}

auto Building::isCaptureable() -> int
{
    if (MPlayer == nullptr)
    {
        return captureable != 0 && isCaptured() == 0 && isDestroyed() == 0 ? 1 : 0;
    }

    return captureable != 0 && isDestroyed() == 0 ? 1 : 0;
}

auto Building::setCommanderId(int32_t id) -> void
{
    commanderId = static_cast<char>(id);
}

auto Building::isRevealed() -> int
{
    ByteFlag* visibleBits = homeTeam->alignment == -1 ? Terrain::ClanVisibleBits : Terrain::terrainVisibleBits;
    const auto col = static_cast<uint32_t>((blockNumber % Terrain::blocksMapSide) * Terrain::verticesBlockSide +
                                           vertexNumber % Terrain::verticesBlockSide);
    const auto row = static_cast<uint32_t>((blockNumber / Terrain::blocksMapSide) * Terrain::verticesBlockSide +
                                           vertexNumber / Terrain::verticesBlockSide);

    if (visibleBits->getFlag(row, col) != 0)
    {
        return 1;
    }

    if (visibleBits->getFlag(row + 1, col) != 0)
    {
        return 1;
    }

    if (visibleBits->getFlag(row + 1, col + 1) != 0)
    {
        return 1;
    }

    return visibleBits->getFlag(row, col + 1) != 0 ? 1 : 0;
}

auto Building::render() -> void
{
    if (justCreated != 0)
    {
        return;
    }

    auto* buildingAppearance = static_cast<VFXBuildingAppearance*>(appearance);
    const int visible = isVisible(eye);

    if (buildingAppearance != nullptr)
    {
        buildingAppearance->visible = visible;
        buildingAppearance->tileNum = tileNum;
        buildingAppearance->update();
    }

    // Burning: the type's burn damage every TimeToBurnDamage seconds.
    if (fireObject == nullptr)
    {
        burning = 0;
    }
    else
    {
        burnTime = frameLength + burnTime;
        auto* type = static_cast<BuildingType*>(objType);

        if (type->timeToBurnDamage < burnTime)
        {
            burnTime = 0.0f;
            _WeaponShotInfo shot;
            shot.init(nullptr, -1, type->burnDamagePerTime, 0, 0.0f);

            if (MPlayer == nullptr)
            {
                handleWeaponHit(&shot, 0);
            }
            else if (MPlayer->isServer != 0)
            {
                handleWeaponHit(&shot, 1);
            }
        }
    }

    if (getContactType(homeTeam->id) == 2)
    {
        // A sensor contact: a blip sized by tonnage.
        uint8_t* shape;
        const char* shapeName;

        if (100.0f < getTonnage())
        {
            shape = scenario->sensorContactShapes[0];
            shapeName = "blip1";
        }
        else if (35.0f < getTonnage())
        {
            shape = scenario->sensorContactShapes[2];
            shapeName = "blip2";
        }
        else
        {
            shape = scenario->sensorContactShapes[4];
            shapeName = "blip3";
        }

        if (shape != nullptr)
        {
            if (VFX_shape_count(shape) < blipFrame)
            {
                if (soundSystem != nullptr && useSound != 0)
                {
                    soundSystem->playDigitalSample(0x14, 1, this, 0, 1);
                }

                blipFrame = 0;
            }

            ElementList->openGroup(-100000, 1);
            auto* element = new VFXElement(shape, screenPos.x, screenPos.y, blipFrame, 0, nullptr, 0, 0);
            std::strcpy(element->name, shapeName);
            ElementList->add(element);
            blipTime = frameLength + blipTime;

            if (0.067 < blipTime)
            {
                blipFrame = static_cast<int32_t>(blipTime * (1.0 / 0.067) + blipFrame + 0.5);
                blipTime = 0.0f;
            }
        }
    }

    if (windowsVisible != turn)
    {
        if (soundHandle != 0xffffffff)
        {
            soundSystem->stopDigitalSample(soundHandle);
            soundHandle = 0xffffffff;
        }

        return;
    }

    // Hazed by how many corners of its vertex square the home team sees; drawn when any is. (The original also reads
    // each corner's seen bit and drops it.)
    ByteFlag* visibleBits = homeTeam->alignment == -1 ? Terrain::ClanVisibleBits : Terrain::terrainVisibleBits;
    const auto col = static_cast<uint32_t>(cellColumn);
    const auto row = static_cast<uint32_t>(cellRow);
    int32_t numVisible = 0;

    if (visibleBits->getFlag(row, col) != 0)
    {
        numVisible++;
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

    if (numVisible != 4 && hazeLevel != 0x7fff)
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

    buildingAppearance->fadeTable = hazePalette;

    if (numVisible == 0 || justCreated != 0)
    {
        if (soundHandle != 0xffffffff)
        {
            soundSystem->stopDigitalSample(soundHandle);
            soundHandle = 0xffffffff;
        }
    }
    else
    {
        buildingAppearance->render(0);
        auto* type = static_cast<BuildingType*>(objType);

        if (soundHandle == 0xffffffff && type->normalEffectId != 0xffffffff)
        {
            soundHandle = static_cast<uint32_t>(soundSystem->playDigitalSample(type->normalEffectId, 0, this, 1, 0));
        }
    }

    if (drawExtents != 0)
    {
        // Debug: the extent radius as an ellipse.
        const float diameter = objType->extentRadius + objType->extentRadius;
        vector_2d size;
        size.x = eye->cosAngle * diameter;
        size.y = eye->sinAngle * diameter;

        if (eye->cameraScale == 1)
        {
            size.x *= 0.5f;
            size.y *= 0.5f;
        }

        const float scale = eye->cameraScale != 1 ? 1.0f : 0.5f;
        const float sx = (position.x - eye->position.x) * scale;
        const float sy = (position.y - eye->position.y) * scale;
        vector_2d center;
        center.x = sy * eye->cosAngle + sx * eye->cosAngle + eye->halfWidth;
        center.y =
            ((sx * eye->sinAngle + eye->halfHeight) - sy * eye->sinAngle) - scale * (position.z - eye->position.z);
        ElementList->openGroup(-50000, 1);
        ElementList->add(new EllipseElement(center, size, 0xfe, -50000));
    }
}

auto Building::destroy() -> void
{
    delete appearance;
    appearance = nullptr;

    if (sensorSystem != nullptr)
    {
        sensorSystemManager->freeSensor(sensorSystem);
        sensorSystem = nullptr;
    }

    systemHeap->free(name);
    name = nullptr;
}

auto Building::setDamage(float newDamage) -> void
{
    damage = newDamage;
    // Sixteen damage frames across the damage level.
    auto* type = static_cast<BuildingType*>(objType);
    int32_t frame =
        static_cast<int32_t>(newDamage / (static_cast<double>(static_cast<int32_t>(type->dmgLevel)) * 0.0625));

    if (frame > 15)
    {
        frame = 15;
    }

    static_cast<VFXBuildingAppearance*>(appearance)->setDamageLvl(static_cast<uint32_t>(frame));
}

auto Building::setSensorData(Team* newTeam, float range, int setTeam) -> void
{
    if (!(-1.0 < range))
    {
        return;
    }

    if (sensorSystem == nullptr)
    {
        sensorSystem = sensorSystemManager->newSensor();

        if (sensorSystem == nullptr)
        {
            Fatal(0, " No RAM for Sensor System ");
        }
    }

    sensorSystem->owner = this;

    if (setTeam != 0)
    {
        sensorSystem->setTeam(newTeam);
    }

    sensorSystem->setRange(range);
}

auto Building::init(ObjectType* objType) -> int32_t
{
    int32_t result = GameObject::init(objType);

    if (result != 0)
    {
        return result;
    }

    setExists(1);
    const uint32_t appearId = objType->appearName;
    justCreated = 1;
    AppearanceType* apprType = appearanceTypeList->getAppearance(appearId, 0);

    if (apprType == nullptr)
    {
        return static_cast<int32_t>(0xdcdc0007);
    }

    auto* buildingAppearance = new VFXBuildingAppearance;

    if (buildingAppearance != nullptr)
    {
        buildingAppearance->init(nullptr, nullptr);
    }

    appearance = buildingAppearance;

    if (buildingAppearance == nullptr)
    {
        return static_cast<int32_t>(0xdcdc0008);
    }

    if ((apprType->appearanceNum & 0xff000000) != 0x7000000)
    {
        return static_cast<int32_t>(0xdcdc0009);
    }

    if ((result = buildingAppearance->init(apprType, this)) != 0)
    {
        return result;
    }

    auto* type = static_cast<BuildingType*>(this->objType);
    objectClass = BUILDING;
    soundHandle = 0xffffffff;

    if (0.0 < type->extentRadius)
    {
        collisionsOn = 1;
    }

    tonnage = type->baseTonnage;
    explRadius = type->explRad;
    explDamage = type->explDmg;
    maxCV = type->battleRating;
    curCV = type->battleRating;
    char nameBuffer[256];
    cLoadString(thisInstance, static_cast<uint32_t>(type->buildingName), nameBuffer, 0xfe);
    name = static_cast<char*>(systemHeap->malloc(static_cast<uint32_t>(std::strlen(nameBuffer) + 1)));
    std::strcpy(name, nameBuffer);

    // Original behaviour (OB-016): a building with no team (TeamID -1) reads TeamTable[-1], the global before it in
    // MCX.EXE: homeTeam.
    team = type->teamId == -1 ? homeTeam : TeamTable[type->teamId];
    const float range = type->sensorRange;

    if (-1.0 < range)
    {
        switch (type->teamId)
        {
            case 0:
            {
                setSensorData(innerSphereTeam, range, 0);
                setAlignment(1);
                break;
            }
            case 1:
            {
                setSensorData(clanTeam, range, 0);
                setAlignment(-1);
                break;
            }
            case 2:
            {
                setSensorData(alliedTeam, range, 0);
                setAlignment(0);
                break;
            }
            default:
                break;
        }
    }

    return 0;
}

auto Building::createBuildingMarines() -> void
{
    auto* type = static_cast<BuildingType*>(objType);
    const int32_t marinesWanted = type->numMarines;

    if (marinesWanted == 0)
    {
        return;
    }

    int32_t marinesMade = 0;
    const auto numWarriors = static_cast<int32_t>(scenario->numWarriors);

    // Each marine is piloted by an enemy warrior with no working vehicle (none, disabled or destroyed); warrior 0 is
    // never used.
    for (int32_t i = 0; i < numWarriors; i++)
    {
        if (i <= 0 || static_cast<uint32_t>(i) > scenario->numWarriors)
        {
            continue;
        }

        MechWarrior* warrior = scenario->warriors[i];

        if (warrior == nullptr || warrior->alignment == homeTeam->alignment)
        {
            continue;
        }

        if (warrior->vehicle != nullptr)
        {
            const auto vehicleStatus = static_cast<int8_t>(warrior->vehicle->status);

            if (vehicleStatus != 2 && vehicleStatus != 1)
            {
                continue;
            }
        }

        auto* marine = static_cast<Mover*>(createObject(DefaultPilotId));

        if (marine == nullptr)
        {
            Fatal(-1, " Couldnt create Marine for Building ");
        }

        marine->setAwake(1);
        FullPathFileName profileName;
        profileName.init(profilePath, marineProfileName, ".fit");
        FitIniFile profileFile;
        const int32_t result = profileFile.open(profileName, READ, 50);

        if (result != 0)
        {
            Fatal(result, " Unable to open Vehicle Marine Profile ");
        }

        if (marine->init(&profileFile) != 0)
        {
            Fatal(-1, " Bad Vehicle Marine Profile File ");
        }

        profileFile.close();

        marine->setPilot(warrior);
        warrior->setVehicle(marine);
        warrior->lobotomy();
        marine->setControl(2, 3, -1);
        marine->setTeam(clanTeam);
        // Somewhere within the extent radius of the building.
        const float extentX = objType->extentRadius;
        const float extentY = objType->extentRadius;
        const float offsetX = static_cast<float>(RandomNumber(static_cast<int32_t>(extentX + extentX))) - extentX;
        const float offsetY = static_cast<float>(RandomNumber(static_cast<int32_t>(extentY + extentY))) - extentY;
        const float offsetZ = static_cast<float>(RandomNumber(0));
        vector_3d marinePosition;
        marinePosition.x = offsetX + position.x;
        marinePosition.y = offsetY + position.y;
        marinePosition.z = offsetZ + position.z;
        marine->setPosition(marinePosition);
        GameObjectMap->addObject(marine);
        marine->bounceToAdjCell();
        marine->bounceToAdjCell();
        auto* marineAppearance = static_cast<ElementalActor*>(marine->getAppearance());

        if (marineAppearance != nullptr)
        {
            marineAppearance->setGesture(0);
            marineAppearance->fadeTableIndex = getAlignment() == -1 ? 0x1c : 0x12;
        }

        marine->idNumber = 2500000;
        marine->setPartId(0xfff - NumMarines++);
        marine->setAlignment(getAlignment());
        ObjectQueueNode* list = getAlignment() == -1 ? clanMechList : innerSphereMechList;

        if (list != nullptr)
        {
            list->addNode(marine);
        }

        marine->setPotentialContact(0);
        marine->setExists(1);
        warrior->clearAttackOrders();
        warrior->clearMoveOrders();
        warrior->orderMoveToPoint(0, 1, 0, vector_3d(0.0f, 0.0f, 0.0f), -1, 1);

        if (++marinesMade == marinesWanted)
        {
            return;
        }
    }
}

auto Building::handleWeaponHit(_WeaponShotInfo* shotInfo, int addMultiplayChunk) -> int32_t
{
    if (shotInfo == nullptr)
    {
        return 0;
    }

    if (addMultiplayChunk != 0)
    {
        MPlayer->addWeaponHitChunk(this, shotInfo, 0);
    }

    if (isDestroyed() != 0)
    {
        return 0;
    }

    float newDamage = getDamage() + shotInfo->damage;
    auto* type = static_cast<BuildingType*>(objType);
    const auto destroyLevel = static_cast<float>(static_cast<int32_t>(type->dmgLevel));

    if (destroyLevel <= newDamage)
    {
        // Destroyed: sensor off, a fire (or more burn time), the explosion, salvage gone and the marines out.
        if (sensorSystem != nullptr)
        {
            sensorSystem->disable();
        }

        if (burning == 0)
        {
            if (type->blownEffectId != 0xffffffff)
            {
                GameObject* effect = createBlownEffect(this, static_cast<int32_t>(type->blownEffectId));

                if (effect != nullptr)
                {
                    if (effect->objectClass == FIRE)
                    {
                        fireObject->update();
                        burning = 1;
                    }
                    else if (objectList->head != nullptr)
                    {
                        objectList->head->addNode(effect);
                    }
                }
            }
        }
        else if (fireObject != nullptr)
        {
            fireObject->addTimeLeftToBurn(2.0f);
        }

        type->createExplosion(position, explDamage, explRadius);
        status = 2;

        if (isCaptured() != 0)
        {
            Terrain::terrainTacticalMap->RemoveSalvage(this, 1);
        }

        newDamage = destroyLevel;

        if (MPlayer == nullptr)
        {
            createBuildingMarines();
        }
    }

    setDamage(newDamage);
    return 0;
}
