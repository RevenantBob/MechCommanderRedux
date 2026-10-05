#include "stdafx.h"
#include "object/tbldng.h"
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
#include "lib/file.h"
#include "lib/heap.h"
#include "lib/inifile.h"
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
    int32_t loadShadow(FitIniFile& typeFile, const char* entry, uint8_t*& shadow)
    {
        char shadowName[80];

        if (typeFile.readIdString(entry, shadowName, 79) != 0)
        {
            return 0;
        }

        FullPathFileName shadowPath;
        shadowPath.init(spritePath, shadowName, ".shp");
        File shadowFile;
        const int32_t result = shadowFile.open(shadowPath, READ, 50);

        if (result != 0)
        {
            return result;
        }

        const uint32_t size = shadowFile.fileSize();
        shadow = static_cast<uint8_t*>(ObjectTypeManager::objectTypeCache->malloc(size));
        shadowFile.read(shadow, static_cast<int32_t>(size));
        MCRenderer::RegisterData(shadow, size, MCDataKind::Shapes);
        shadowFile.close();
        return 0;
    }

    /// <summary>The map row and column of a tree building's terrain vertex.</summary>
    void vertexRowCol(const TreeBuilding* building, uint32_t& row, uint32_t& col)
    {
        col = static_cast<uint32_t>((building->blockNumber % Terrain::blocksMapSide) * Terrain::verticesBlockSide +
                                    building->vertexNumber % Terrain::verticesBlockSide);
        row = static_cast<uint32_t>((building->blockNumber / Terrain::blocksMapSide) * Terrain::verticesBlockSide +
                                    building->vertexNumber / Terrain::verticesBlockSide);
    }
}

//---------------------------------------------------------------------------
// TreeBuildingType
//---------------------------------------------------------------------------

auto TreeBuildingType::init() -> void
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
    normalShadow = nullptr;
    destroyedShadow = nullptr;
    battleRating = 0;
    numMarines = 0;
    canRefit = 0;
    mechBay = 0;
}

auto TreeBuildingType::createInstance() -> BaseObject*
{
    auto* newBuilding = new TreeBuilding;

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

auto TreeBuildingType::destroy() -> void
{
    ObjectTypeManager::objectTypeCache->free(normalShadow);
    normalShadow = nullptr;
    ObjectTypeManager::objectTypeCache->free(destroyedShadow);
    destroyedShadow = nullptr;
}

auto TreeBuildingType::init(File* objFile, uint32_t fileSize) -> int32_t
{
    FitIniFile treeFile;
    int32_t result = treeFile.open(objFile, fileSize, 50);

    if (result != 0)
    {
        return result;
    }

    if ((result = treeFile.seekBlock("TreeData")) != 0)
    {
        return result;
    }

    if ((result = treeFile.readIdULong("DmgLevel", dmgLevel)) != 0)
    {
        return result;
    }

    treeFile.readIdULong("NormalEffectId", normalEffectId);
    treeFile.readIdULong("BlownEffectId", blownEffectId);
    treeFile.readIdULong("DamageEffectId", damageEffectId);

    if (treeFile.readIdBoolean("CanRefit", canRefit) != 0)
    {
        canRefit = 0;
    }

    if (canRefit != 0 && treeFile.readIdBoolean("MechBay", mechBay) != 0)
    {
        mechBay = 0;
    }

    if ((result = loadShadow(treeFile, "NormalShadow", normalShadow)) != 0)
    {
        return result;
    }

    if ((result = loadShadow(treeFile, "DestroyedShadow", destroyedShadow)) != 0)
    {
        return result;
    }

    if (treeFile.readIdFloat("ExplosionRadius", explRad) != 0)
    {
        explRad = 0.0f;
    }

    if (treeFile.readIdFloat("ExplosionDamage", explDmg) != 0)
    {
        explDmg = 0.0f;
    }

    if (treeFile.readIdFloat("TimeToBurnDamage", timeToBurnDamage) != 0)
    {
        timeToBurnDamage = 5.0f;
    }

    if (treeFile.readIdFloat("BurnDamagePerTime", burnDamagePerTime) != 0)
    {
        burnDamagePerTime = 1.0f;
    }

    if (treeFile.readIdFloat("DamageLvlForBurn", damageLvlForBurn) != 0)
    {
        damageLvlForBurn = static_cast<float>(dmgLevel);
    }

    // The team is only read for a building with a sensor.
    if (treeFile.readIdFloat("SensorRange", sensorRange) == 0)
    {
        if (treeFile.readIdLong("TeamID", teamId) != 0)
        {
            teamId = -1;
        }
    }
    else
    {
        sensorRange = -1.0f;
    }

    if (treeFile.readIdFloat("Tonnage", baseTonnage) != 0)
    {
        baseTonnage = 20.0f;
    }

    if (treeFile.readIdLong("BattleRating", battleRating) != 0)
    {
        battleRating = 20;
    }

    if (treeFile.readIdLong("NumMarines", numMarines) != 0)
    {
        numMarines = 0;
    }

    if (treeFile.readIdLong("BuildingName", buildingName) != 0)
    {
        buildingName = 0xa3;
    }

    return ObjectType::init(&treeFile);
}

auto TreeBuildingType::handleCollision(GameObject*, GameObject*) -> int
{
    return 1;
}

auto TreeBuildingType::handleDestruction(GameObject*, GameObject*) -> int
{
    return 0;
}

//---------------------------------------------------------------------------
// TreeBuilding
//---------------------------------------------------------------------------

TreeBuilding::TreeBuilding()
{
    frame.reset_to_world_frame();
    init();
    justCreated = 1;
    appearance = nullptr;
    vertexNumber = 0;
    blockNumber = 0;
    burning = 0;
    unknown9C = 0;
    unknownA0 = 500000;
    hitOnce = 0;
    collapsed = 0;
    collapsing = 0;
    burnTime = 0.0f;
    name = nullptr;
    fireObject = nullptr;
    sensorSystem = nullptr;
    soundHandle = 0xffffffff;
    commanderId = static_cast<char>(0xff);
    canRefit = 0;
    mechBay = 0;
}

auto TreeBuilding::init() -> void
{
    for (MechWarrior*& slot : prisonSlots)
    {
        slot = nullptr;
    }
}

auto TreeBuilding::setTerrainPosition(vector_2d& offset, vector_2d& numbers) -> void
{
    pixelOffsetX = static_cast<int32_t>(offset.x);
    pixelOffsetY = static_cast<int32_t>(offset.y);
    vertexNumber = static_cast<int32_t>(numbers.x);
    blockNumber = static_cast<int32_t>(numbers.y);
}

auto TreeBuilding::getFrame() -> frame_of_ref
{
    return frame;
}

auto TreeBuilding::setFrame(frame_of_ref& newFrame) -> void
{
    frame = newFrame;
}

auto TreeBuilding::isPrison() -> int
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

auto TreeBuilding::getBlockAndVertexNumber(int32_t& blockNum, int32_t& vertexNum) -> void
{
    blockNum = blockNumber;
    vertexNum = vertexNumber;
}

auto TreeBuilding::getRefitPoints() -> float
{
    if (canRefit == 0)
    {
        return 0.0f;
    }

    return static_cast<float>(static_cast<int32_t>(static_cast<TreeBuildingType*>(objType)->dmgLevel)) - damage;
}

auto TreeBuilding::burnRefitPoints(float points) -> int
{
    if (canRefit == 0)
    {
        return 0;
    }

    // Spent refit points count as damage; never more than are left.
    if (points < getRefitPoints())
    {
        damageObject(points);
    }
    else
    {
        damageObject(getRefitPoints());
    }

    return 1;
}

auto TreeBuilding::isVisible(Camera* cam) -> int
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

    // The shadow can stick out past the building: on screen when any of its box is.
    bool shadowOnScreen = false;
    uint8_t* shadow = static_cast<TreeBuildingType*>(objType)->normalShadow;

    if (shadow != nullptr)
    {
        const float scale = cam->cameraScale != 1 ? 1.0f : 0.5f;
        const int32_t minXY = VFX_shape_minxy(shadow, 0);
        const float left = static_cast<float>(minXY >> 16) * scale + screenPos.x;
        const float top = static_cast<float>(static_cast<int16_t>(minXY)) * scale + screenPos.y;
        const int32_t resolution = VFX_shape_resolution(shadow, 0);

        if (0.0f <= static_cast<float>(resolution >> 16) * scale + left &&
            0.0f <= scale * static_cast<float>(static_cast<int16_t>(resolution)) + top)
        {
            const auto viewRight = static_cast<int16_t>(std::floor(static_cast<double>(cam->viewWidth)));
            const auto viewBottom = static_cast<int16_t>(std::floor(static_cast<double>(cam->viewHeight)));
            shadowOnScreen = left <= static_cast<float>(viewRight) && top <= static_cast<float>(viewBottom);
        }
    }

    if (!shadowOnScreen && visible == 0)
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

auto TreeBuilding::isCaptureable() -> int
{
    if (MPlayer == nullptr)
    {
        return captureable != 0 && isCaptured() == 0 && isDestroyed() == 0 ? 1 : 0;
    }

    return captureable != 0 && isDestroyed() == 0 ? 1 : 0;
}

auto TreeBuilding::update() -> int32_t
{
    if (justCreated == 0)
    {
        return 1;
    }

    // Set the building on its vertex: the block's corner, the vertex within it, then the pixel offset within the
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

    cellColumn = (blockNumber % Terrain::blocksMapSide) * verticesBlockSide + vertexNumber % verticesBlockSide;
    const int32_t halfMap = (verticesBlockSide * Terrain::blocksMapSide) >> 1;
    vertexWorldX = static_cast<float>(cellColumn - halfMap) * Terrain::metersPerVertex;
    cellRow = vertexNumber / verticesBlockSide + (blockNumber / Terrain::blocksMapSide) * verticesBlockSide;
    vertexWorldY = static_cast<float>(halfMap - cellRow) * Terrain::metersPerVertex;
    const auto inBounds = [&]
    { return cellRow < 0 || GameMap->height <= cellRow || cellColumn < 0 || GameMap->width <= cellColumn ? 0u : 1u; };
    Assert(inBounds(), 0, " tbldg MapTile Out of Bounds ");
    Assert(inBounds(), 0, " Map Tile out of bounds ");
    const MapTile& tile = GameMap->map[GameMap->width * cellRow + cellColumn];
    const int32_t elevationLevel = static_cast<int32_t>((tile.cells >> 7) & 0x3f) + GameMap->baseElevation;
    auto* treeAppearance = static_cast<VFXAppearance*>(appearance);
    treeAppearance->visible = 1;
    cellElevation = static_cast<float>(elevationLevel) * Terrain::metersPerElevLevel;
    treeAppearance->update();
    treeAppearance->recalcBounds(eye);

    if (canRefit != 0)
    {
        treeAppearance->setTypeId(ACTOR_STATE_NORMAL, 0);
    }

    return 1;
}

auto TreeBuilding::setAlignment(int32_t align) -> void
{
    BigGameObject::setAlignment(align);

    if (isDestroyed() != 0 || sensorSystem == nullptr)
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

auto TreeBuilding::setCommanderId(int32_t id) -> void
{
    commanderId = static_cast<char>(id);
}

auto TreeBuilding::handleEvent(ObjectEvent* event) -> int32_t
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

auto TreeBuilding::lightOnFire(float timeToBurn) -> void
{
    auto* type = static_cast<TreeBuildingType*>(objType);

    if (type->blownEffectId == 0xffffffff)
    {
        // Nothing to burn: a point of damage instead.
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
        GameObject* newFire = createObject(static_cast<int32_t>(type->blownEffectId));

        if (newFire != nullptr)
        {
            newFire->setPosition(position);

            if (newFire->objectClass == FIRE)
            {
                fireObject = static_cast<Fire*>(newFire);
                fireObject->setPotentialContact(3);
                fireObject->burningObject = this;
                fireObject->setTonnage(40.0f);
            }
            else
            {
                destroyObject(newFire);
            }
        }
    }

    if (fireObject != nullptr)
    {
        fireObject->addTimeLeftToBurn(timeToBurn);
        burning = 1;
    }
}

auto TreeBuilding::isRevealed() -> int
{
    ByteFlag* visibleBits = homeTeam->alignment == -1 ? Terrain::ClanVisibleBits : Terrain::terrainVisibleBits;
    uint32_t row;
    uint32_t col;
    vertexRowCol(this, row, col);

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

auto TreeBuilding::render() -> void
{
    if (justCreated != 0)
    {
        return;
    }

    auto* type = static_cast<TreeBuildingType*>(objType);

    // Burning: the type's burn damage every TimeToBurnDamage seconds.
    if (fireObject == nullptr)
    {
        burning = 0;
    }
    else
    {
        const double burnSum = static_cast<double>(frameLength) + burnTime;
        burnTime = static_cast<float>(burnSum);

        if (type->timeToBurnDamage < burnSum)
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

    auto* treeAppearance = static_cast<VFXAppearance*>(appearance);

    if (treeAppearance != nullptr)
    {
        treeAppearance->visible = isVisible(eye);

        // When the collapse animation ends, settle on the matching rubble state.
        if (treeAppearance->update() == 0 && collapsing != 0)
        {
            const ActorState state = treeAppearance->currentState;
            collapsing = 0;
            collapsed = 1;

            if (state == ACTOR_STATE_BLOWING_UP1)
            {
                treeAppearance->setTypeId(ACTOR_STATE_DAMAGED, 0xff);
            }
            else if (state == ACTOR_STATE_DESTROYED)
            {
                treeAppearance->setTypeId(ACTOR_STATE_FALLEN_DMG, 0xff);
            }

            treeAppearance->update();
        }
    }

    if (getContactType(homeTeam->id) == 2)
    {
        // A sensor contact: a blip sized by tonnage.
        uint8_t* shape;

        if (50.0f < getTonnage())
        {
            shape = scenario->sensorContactShapes[0];
        }
        else if (35.0f < getTonnage())
        {
            shape = scenario->sensorContactShapes[2];
        }
        else
        {
            shape = scenario->sensorContactShapes[4];
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
            ElementList->add(
                ElementPool::Make<VFXElement>(shape, screenPos.x, screenPos.y, blipFrame, 0, nullptr, 0, 0));
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

    // Hazed by how many corners of its vertex square the home team sees; drawn (with its shadow) when any is. (The
    // original also reads each corner's seen bit and drops it.)
    uint32_t row;
    uint32_t col;
    vertexRowCol(this, row, col);
    ByteFlag* visibleBits = homeTeam->alignment == -1 ? Terrain::ClanVisibleBits : Terrain::terrainVisibleBits;
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

    treeAppearance->fadeTable = hazePalette;

    if (numVisible == 0)
    {
        if (soundHandle != 0xffffffff)
        {
            soundSystem->stopDigitalSample(soundHandle);
            soundHandle = 0xffffffff;
        }
    }
    else
    {
        // Standing, it sorts with the terrain; fallen or burning down, by its screen row.
        const bool standing = treeAppearance->currentState == ACTOR_STATE_NORMAL;
        treeAppearance->render(standing ? 0 : static_cast<int32_t>(screenPos.y));
        uint8_t* shadow = standing ? type->normalShadow : type->destroyedShadow;

        if (shadow != nullptr)
        {
            ElementList->openGroup(static_cast<int32_t>(screenPos.y), 1);
            ElementList->add(ElementPool::Make<VFXElement>(shadow, screenPos.x, screenPos.y, 0, 0, hazePalette, 0, 0));
        }

        if (soundHandle == 0xffffffff && type->normalEffectId != 0xffffffff)
        {
            soundHandle = static_cast<uint32_t>(soundSystem->playDigitalSample(type->normalEffectId, 0, this, 1, 0));
        }
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
        // Port: an overlay, on the screen over the view: it follows the object through the zoom.
        center = MCOverlayPoint(center);
        size.x *= MCOverlay.ScaleX;
        size.y *= MCOverlay.ScaleY;
        ElementList->add(ElementPool::Make<EllipseElement>(center, size, 0xfe, -50000));
    }
}

auto TreeBuilding::destroy() -> void
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

auto TreeBuilding::setSensorData(Team* newTeam, float range, int setTeam) -> void
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

auto TreeBuilding::init(ObjectType* objType) -> int32_t
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

    auto* treeAppearance = new VFXAppearance;

    if (treeAppearance != nullptr)
    {
        treeAppearance->init(nullptr, nullptr);
    }

    appearance = treeAppearance;

    if (treeAppearance == nullptr)
    {
        return static_cast<int32_t>(0xdcdc0008);
    }

    if ((apprType->appearanceNum & 0xff000000) != 0x2000000)
    {
        return static_cast<int32_t>(0xdcdc0009);
    }

    if ((result = treeAppearance->init(apprType, this)) != 0)
    {
        return result;
    }

    auto* type = static_cast<TreeBuildingType*>(this->objType);
    objectClass = TREEBUILDING;
    hitOnce = 0;
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
    canRefit = type->canRefit;
    mechBay = type->mechBay;
    char nameBuffer[256];
    cLoadString(thisInstance, static_cast<uint32_t>(type->buildingName), nameBuffer, 0xfe);
    name = static_cast<char*>(systemHeap->malloc(static_cast<uint32_t>(std::strlen(nameBuffer) + 1)));
    std::strcpy(name, nameBuffer);

    // Original behaviour (OB-016): with no team (TeamID -1) this reads TeamTable[-1], which in MCX.EXE is homeTeam.
    typeTeam = type->teamId == -1 ? homeTeam : TeamTable[type->teamId];
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

    captureable = 0;
    refitBuddy = nullptr;

    // Damage level 0: already rubble.
    if (type->dmgLevel == 0)
    {
        collisionsOn = 0;
        status = 2;
        hitOnce = 1;
    }

    return 0;
}

auto TreeBuilding::createBuildingMarines() -> void
{
    auto* type = static_cast<TreeBuildingType*>(objType);
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
        // A random direction, set 1.5 extent radii out on the ground (z stays the unscaled unit component).
        const float extent = objType->extentRadius;
        vector_3d offset;
        offset.x = static_cast<float>(RandomNumber(static_cast<int32_t>(extent + extent))) - extent;
        offset.y = static_cast<float>(RandomNumber(static_cast<int32_t>(extent + extent))) - extent;
        offset.z = static_cast<float>(RandomNumber(0)) - 0.0f;
        const double length =
            std::sqrt(static_cast<double>(offset.z) * offset.z + static_cast<double>(offset.y) * offset.y +
                      static_cast<double>(offset.x) * offset.x);

        if (length != 0.0)
        {
            offset.x = static_cast<float>(offset.x / length);
            offset.y = static_cast<float>(offset.y / length);
            offset.z = static_cast<float>(offset.z / length);
        }

        offset.x = static_cast<float>(static_cast<double>(extent) * offset.x * 1.5);
        offset.y = static_cast<float>(static_cast<double>(extent) * offset.y * 1.5);
        vector_3d marinePosition;
        marinePosition.x = offset.x + position.x;
        marinePosition.y = offset.y + position.y;
        marinePosition.z = offset.z + position.z;
        marine->setPosition(marinePosition);
        marine->setLastValidPosition(position + offset);
        GameObjectMap->addObject(marine);
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

auto TreeBuilding::handleWeaponHit(_WeaponShotInfo* shotInfo, int addMultiplayChunk) -> int32_t
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

    const float newDamage = getDamage() + shotInfo->damage;
    setDamage(newDamage);
    hitOnce = 1;
    auto* type = static_cast<TreeBuildingType*>(objType);

    if (newDamage < static_cast<float>(static_cast<int32_t>(type->dmgLevel)) || collapsed != 0 || collapsing != 0)
    {
        return 0;
    }

    // Collapses: the shooter's pilot is alarmed, the marines come out, and the sensor, fire and explosion follow.
    collapsing = 1;
    collisionsOn = 0;
    status = 2;

    if (isCaptured() != 0)
    {
        Terrain::terrainTacticalMap->RemoveSalvage(this, 1);
    }

    GameObject* attacker = shotInfo->attacker;

    if (attacker != nullptr && (attacker->objectClass == BATTLEMECH || attacker->objectClass == GROUNDVEHICLE ||
                                attacker->objectClass == ELEMENTAL || attacker->objectClass == MOVER))
    {
        attacker->getPilot()->triggerAlarm(12, static_cast<uint32_t>(partId));
    }

    if (MPlayer == nullptr)
    {
        createBuildingMarines();
    }

    if (type->damageEffectId != 0xffffffff && 5 < turn)
    {
        soundSystem->playDigitalSample(type->damageEffectId, 1, this, 1, 0);
    }

    if (sensorSystem != nullptr)
    {
        sensorSystem->disable();
    }

    // Original behaviour: hitOnce was set just above, so the collapse always plays the destroyed state (4), never 1.
    auto* treeAppearance = static_cast<VFXAppearance*>(appearance);
    treeAppearance->setTypeId(hitOnce == 0 ? ACTOR_STATE_BLOWING_UP1 : ACTOR_STATE_DESTROYED, 0xff);

    if (burning == 0)
    {
        if (type->blownEffectId != 0xffffffff)
        {
            GameObject* newFire = createObject(static_cast<int32_t>(type->blownEffectId));

            if (newFire != nullptr)
            {
                newFire->setPosition(position);

                if (newFire->objectClass == FIRE)
                {
                    fireObject = static_cast<Fire*>(newFire);
                    fireObject->setPotentialContact(3);
                    fireObject->burningObject = this;
                    fireObject->setTonnage(40.0f);
                    fireObject->update();
                    burning = 1;
                }
                else if (objectList->head != nullptr)
                {
                    objectList->head->addNode(newFire);
                }
            }
        }
    }
    else if (fireObject != nullptr)
    {
        fireObject->addTimeLeftToBurn(2.0f);
    }

    type->createExplosion(position, explDamage, explRadius);
    return 0;
}
