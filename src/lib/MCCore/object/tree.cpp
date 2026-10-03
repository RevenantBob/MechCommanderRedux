#include "stdafx.h"
#include "object/tree.h"
#include "ai/move.h"
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
#include "network/multplyr.h"
#include "object/bldng.h"
#include "object/collsn.h"
#include "object/fire.h"
#include "object/object.h"
#include "object/objevnt.h"
#include "object/team.h"
#include "sound/soundsys.h"
#include "sprite/actor.h"
#include "terrain/terrain.h"
#include "terrain/terrmap.h"
#include "vfx/vfxfuncs.h"

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
        shadowFile.close();
        return 0;
    }

    /// <summary>The map row and column of a tree's terrain vertex.</summary>
    void vertexRowCol(const Tree* tree, uint32_t& row, uint32_t& col)
    {
        col = static_cast<uint32_t>((tree->blockNumber % Terrain::blocksMapSide) * Terrain::verticesBlockSide +
                                    tree->vertexNumber % Terrain::verticesBlockSide);
        row = static_cast<uint32_t>((tree->blockNumber / Terrain::blocksMapSide) * Terrain::verticesBlockSide +
                                    tree->vertexNumber / Terrain::verticesBlockSide);
    }
} // namespace

//---------------------------------------------------------------------------
// TreeType
//---------------------------------------------------------------------------

TreeType::TreeType()
{
    dmgLevel = 0;
    normalShadow = nullptr;
    destroyedShadow = nullptr;
}

auto TreeType::createInstance() -> BaseObject*
{
    auto* newTree = new Tree;

    if (newTree == nullptr)
    {
        return nullptr;
    }

    if (newTree->init(this) != 0)
    {
        return nullptr;
    }

    newTree->idNumber = NextIdNumber++;
    return newTree;
}

auto TreeType::destroy() -> void
{
    ObjectTypeManager::objectTypeCache->free(normalShadow);
    normalShadow = nullptr;
    ObjectTypeManager::objectTypeCache->free(destroyedShadow);
    destroyedShadow = nullptr;
}

auto TreeType::init(File* objFile, uint32_t fileSize) -> int32_t
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

    if (treeFile.readIdFloat("ExplosionRadius", explosionRadius) != 0)
    {
        explosionRadius = 0.0f;
    }

    if (treeFile.readIdFloat("ExplosionDamage", explosionDamage) != 0)
    {
        explosionDamage = 0.0f;
    }

    if ((result = loadShadow(treeFile, "NormalShadow", normalShadow)) != 0)
    {
        return result;
    }

    if ((result = loadShadow(treeFile, "DestroyedShadow", destroyedShadow)) != 0)
    {
        return result;
    }

    return ObjectType::init(&treeFile);
}

auto TreeType::handleCollision(GameObject* collidee, GameObject* collider) -> int
{
    // A mover (not artillery or fire) knocks a standing tree over, away from itself.
    if (MOVER <= collider->objectClass || collider->objectClass == ARTILLERY || collider->objectClass == FIRE)
    {
        return 1;
    }

    auto* tree = static_cast<Tree*>(collidee);

    if (tree->fallen != 0 || tree->falling != 0)
    {
        return 1;
    }

    tree->falling = 1;
    const vector_3d colliderPos = collider->getPosition();
    const auto facing = static_cast<float>(tree->relFacingTo(colliderPos, -1));
    frame_of_ref frame = tree->getFrame();
    const auto s = static_cast<float>(std::sin(facing * DEGREES_TO_RADIANS));
    const auto c = static_cast<float>(std::cos(facing * DEGREES_TO_RADIANS));
    const vector_3d oldI = frame.i;
    frame.i = frame.i * c + frame.j * s;
    frame.j = frame.j * c - oldI * s;
    tree->setFrame(frame);

    // Fall (state 1), or a burnt tree crumble (state 4); a fall with frames to show makes a sound.
    auto* treeAppearance = static_cast<VFXAppearance*>(tree->appearance);
    tree->collisionsOn = 0;
    uint32_t numFrames = 0;

    if (tree->burnt == 0)
    {
        treeAppearance->setTypeId(ACTOR_STATE_BLOWING_UP1, 0xff);

        if (1 < treeAppearance->appearType->numStates)
        {
            numFrames = treeAppearance->appearType->actorStateData[1].numFrames;
        }
    }
    else
    {
        treeAppearance->setTypeId(static_cast<ActorState>(4), 0xff);

        if (4 < treeAppearance->appearType->numStates)
        {
            numFrames = treeAppearance->appearType->actorStateData[4].numFrames;
        }
    }

    if (useSound != 0 && soundSystem != nullptr && 1 < static_cast<int32_t>(numFrames))
    {
        soundSystem->playDigitalSample(0xe, 1, tree, 0, 0);
    }

    return 1;
}

auto TreeType::handleDestruction(GameObject*, GameObject*) -> int
{
    return 0;
}

//---------------------------------------------------------------------------
// Tree
//---------------------------------------------------------------------------

Tree::Tree()
{
    treeFrame.i = UnitX;
    treeFrame.j = UnitY;
    treeFrame.k = UnitZ;
    justCreated = 1;
    appearance = nullptr;
    vertexNumber = 0;
    blockNumber = 0;
    fireStarted = 0;
    unknown9C = 0;
    unknownA0 = 500000;
    burnt = 0;
    fallen = 0;
    falling = 0;
    fireObject = nullptr;
}

auto Tree::init() -> void
{
}

auto Tree::killFireObject() -> void
{
    fireObject = nullptr;
}

auto Tree::setTerrainPosition(vector_2d& pixelOffset, vector_2d& blockVertex) -> void
{
    pixelOffsetX = static_cast<int32_t>(pixelOffset.x);
    pixelOffsetY = static_cast<int32_t>(pixelOffset.y);
    vertexNumber = static_cast<int32_t>(blockVertex.x);
    blockNumber = static_cast<int32_t>(blockVertex.y);
}

auto Tree::setFrame(frame_of_ref& newFrame) -> void
{
    treeFrame = newFrame;
}

auto Tree::getBlockAndVertexNumber(int32_t& blockNum, int32_t& vertexNum) -> void
{
    blockNum = blockNumber;
    vertexNum = vertexNumber;
}

auto Tree::isVisible(Camera* cam) -> int
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

    // The shadow can stick out past the tree: on screen when any of its box is.
    uint8_t* shadow = static_cast<TreeType*>(objType)->normalShadow;

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

            if (left <= static_cast<float>(viewRight) && top <= static_cast<float>(viewBottom))
            {
                windowsVisible = turn;
                return 1;
            }
        }
    }

    if (visible == 0)
    {
        return 0;
    }

    windowsVisible = turn;
    return 1;
}

auto Tree::update() -> int32_t
{
    if (justCreated == 0)
    {
        return 1;
    }

    // Set the tree on its vertex: the block's corner, the vertex within it, then the pixel offset within the tile
    // (turned into the isometric grid's 60-degree axes).
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
    Assert(inBounds(), 0, " tree MapTile Out of Bounds ");
    Assert(inBounds(), 0, " Map Tile out of bounds ");
    const MapTile& tile = GameMap->map[GameMap->width * tileRow + tileCol];
    const int32_t elevationLevel = static_cast<int32_t>((tile.cells >> 7) & 0x3f) + GameMap->baseElevation;
    appearance->visible = 1;
    tileElevation = static_cast<float>(elevationLevel) * Terrain::metersPerElevLevel;

    // Every tree measures the type's extent radius from its appearance's diagonal.
    appearance->update();
    appearance->recalcBounds(eye);
    const double dx = static_cast<double>(appearance->upperLeft.x) - appearance->lowerRight.x;
    const double dy = static_cast<double>(appearance->upperLeft.y) - appearance->lowerRight.y;
    const auto radius = static_cast<float>(std::sqrt(dy * dy + dx * dx) / worldUnitsPerMeter);

    if (static_cast<float>(CollisionSystem::gridRadius) < radius)
    {
        Fatal(static_cast<int32_t>(std::floor(static_cast<double>(radius))), " Object extent radius TOO large ");
    }

    objType->extentRadius = radius;
    collisionsOn = 1;
    return 1;
}

auto Tree::handleEvent(ObjectEvent* event) -> int32_t
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

auto Tree::lightOnFire(float timeToBurn) -> void
{
    // A fire does 25 points to the tree (the server's job in multiplayer), and burns on.
    _WeaponShotInfo shot;
    shot.init(nullptr, -1, 25.0f, 0, 0.0f);

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
        fireStarted = 1;
    }
}

auto Tree::render() -> void
{
    if (justCreated != 0)
    {
        return;
    }

    if (fireObject == nullptr)
    {
        fireStarted = 0;
    }

    auto* treeAppearance = static_cast<VFXAppearance*>(appearance);

    if (treeAppearance != nullptr)
    {
        treeAppearance->visible = isVisible(eye);

        // A falling tree comes to rest: fallen (2), or fallen burnt (5).
        if (treeAppearance->update() == 0 && falling != 0)
        {
            falling = 0;
            collisionsOn = 0;
            fallen = 1;

            if (treeAppearance->currentState == ACTOR_STATE_BLOWING_UP1)
            {
                treeAppearance->setTypeId(ACTOR_STATE_DAMAGED, 0xff);
            }
            else if (treeAppearance->currentState == static_cast<ActorState>(4))
            {
                treeAppearance->setTypeId(static_cast<ActorState>(5), 0xff);
            }

            treeAppearance->update();
        }
    }

    if (windowsVisible != turn)
    {
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

    treeAppearance->fadeTable = hazePalette;

    if (numVisible != 0)
    {
        // Standing, it sorts with the terrain; fallen, by its screen row.
        const auto* type = static_cast<TreeType*>(objType);
        treeAppearance->render(treeAppearance->currentState != ACTOR_STATE_NORMAL ? static_cast<int32_t>(screenPos.y)
                                                                                  : 0);

        if (treeAppearance->currentState != ACTOR_STATE_NORMAL)
        {
            if (type->destroyedShadow != nullptr)
            {
                ElementList->openGroup(static_cast<int32_t>(screenPos.y), 1);
                ElementList->add(
                    new VFXElement(type->destroyedShadow, screenPos.x, screenPos.y, 0, 0, hazePalette, 0, 0));
            }
        }
        else if (type->normalShadow != nullptr)
        {
            ElementList->openGroup(static_cast<int32_t>(-screenPos.y), 1);
            ElementList->add(new VFXElement(type->normalShadow, screenPos.x, screenPos.y, 0, 0, hazePalette, 0, 0));
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
        ElementList->add(new EllipseElement(center, size, 0xfe, -50000));
    }
}

auto Tree::destroy() -> void
{
    delete appearance;
    appearance = nullptr;
}

auto Tree::init(ObjectType* objType) -> int32_t
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

    objectClass = TREE;
    burnt = 0;
    return 0;
}

auto Tree::handleWeaponHit(_WeaponShotInfo* shotInfo, int addMultiplayChunk) -> int32_t
{
    if (shotInfo == nullptr)
    {
        return 0;
    }

    if (addMultiplayChunk != 0)
    {
        MPlayer->addWeaponHitChunk(this, shotInfo, 0);
    }

    // Any hit burns the tree: standing (0) to burnt (3), fallen (2) to fallen burnt (5).
    setDamage(getDamage() + 1.0f);
    burnt = 1;

    if (falling == 0)
    {
        auto* treeAppearance = static_cast<VFXAppearance*>(appearance);

        if (treeAppearance->currentState == ACTOR_STATE_NORMAL)
        {
            treeAppearance->setTypeId(ACTOR_STATE_BLOWING_UP2, 0xff);
        }
        else if (treeAppearance->currentState == ACTOR_STATE_DAMAGED)
        {
            treeAppearance->setTypeId(static_cast<ActorState>(5), 0xff);
        }
    }

    status = 2;

    // The first hit sets it alight; later ones keep the fire going 2 more seconds.
    if (fireStarted == 0)
    {
        if (objType->explosionObject != -1)
        {
            auto* fire = static_cast<Fire*>(createObject(objType->explosionObject));

            if (fire != nullptr)
            {
                fire->burningObject = this;
                fire->setTonnage(40.0f);
                fire->setPosition(position);
                fire->update();
                fireObject = fire;
            }
        }

        fireStarted = 1;
        return 0;
    }

    if (fireObject != nullptr)
    {
        fireObject->addTimeLeftToBurn(2.0f);
    }

    return 0;
}

auto Tree::isRevealed() -> int
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
