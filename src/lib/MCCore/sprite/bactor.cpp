#include "stdafx.h"
#include "sprite/bactor.h"
#include "camera/camera.h"
#include "engine/ceglist.h"
#include "engine/cevfx.h"
#include "lib/inifile.h"
#include "main/main.h"
#include "object/bldng.h"
#include "object/team.h"
#include "sprite/sprtmgr.h"
#include "sprite/vfxshape.h"
#include "terrain/terrain.h"
#include "vfx/vfxfuncs.h"

int dynamicFrameTiming = 1;

namespace
{
    /// <summary>The first building frame's packet (packets 0..9 are the tiles under buildings).</summary>
    constexpr uint32_t FIRST_FRAME_PACKET = 0xc;
    /// <summary>The number of tile packets.</summary>
    constexpr uint32_t NUM_TILE_SHAPES = 10;

    /// <summary>Adds a VFX element of frame <paramref name="frame"/> of <paramref name="shapeTable"/>.</summary>
    auto addShape(uint8_t* shapeTable, float x, float y, int32_t frame, uint8_t* fadeTable, const char* name) -> void
    {
        auto* element = ElementPool::Make<VFXElement>(shapeTable, x, y, frame, 0, fadeTable, 1, 0);

        // Port fix: the original copies the debug name through a null element too.
        if (element != nullptr)
        {
            strcpy(element->name, name);
        }

        ElementList->add(element);
    }
}

//---------------------------------------------------------------------------
// VFXBuildingAppearanceType
//---------------------------------------------------------------------------

auto VFXBuildingAppearanceType::init(File* apprFile, uint32_t fileSize, uint32_t loadFlags) -> int32_t
{
    const int32_t result = loadIniFile(apprFile, fileSize);

    if (result != 0)
    {
        return result;
    }

    keepLoaded = static_cast<int32_t>(loadFlags);
    numPackets = static_cast<uint32_t>(spriteManager->getNumShapes(appearanceNum & 0xffffff));
    // Port fix: sized by the port's pointer size (the original: count * 4).
    shapeList = static_cast<Shape**>(spriteManager->mallocDataRAM(numPackets * static_cast<uint32_t>(sizeof(Shape*))));

    if (shapeList == nullptr)
    {
        return static_cast<int32_t>(0xbead0002);
    }

    for (uint32_t i = 0; i < numPackets; i++)
    {
        shapeList[i] = nullptr;
    }

    return 0;
}

auto VFXBuildingAppearanceType::removeShape(Shape* shape) -> void
{
    for (int32_t i = 0; i < static_cast<int32_t>(numPackets); i++)
    {
        if (shapeList[i] == shape)
        {
            shapeList[i] = nullptr;
        }
    }

    for (AppearanceUser* user = userList; user != nullptr; user = user->next)
    {
        auto* appearance = static_cast<VFXBuildingAppearance*>(user->user);

        if (appearance->currentShape == shape)
        {
            appearance->currentShape = nullptr;
        }

        if (appearance->tileShape == shape)
        {
            appearance->tileShape = nullptr;
        }
    }
}

auto VFXBuildingAppearanceType::loadIniFile(File* apprFile, uint32_t fileSize) -> int32_t
{
    FitIniFile iniFile;
    int32_t result = iniFile.open(apprFile, fileSize, 50);

    if (result != 0)
    {
        return result;
    }

    if ((result = iniFile.seekBlock("Main Info")) != 0)
    {
        return result;
    }

    if ((result = iniFile.readIdULong("NumFrames", numFrames)) != 0)
    {
        return result;
    }

    if (iniFile.seekBlock("AnimationInfo") == 0)
    {
        if ((result = iniFile.readIdULong("NumAnimStates", numAnimStates)) != 0)
        {
            return result;
        }

        const uint32_t count = numAnimStates;
        animStates = static_cast<BuildingAnimState*>(spriteManager->mallocDataRAM(count * sizeof(BuildingAnimState)));

        if (animStates == nullptr)
        {
            return static_cast<int32_t>(0xbead0001);
        }

        for (int32_t i = 0; i < static_cast<int32_t>(count); i++)
        {
            char blockName[20];
            sprintf(blockName, "AnimState%d", i);

            if ((result = iniFile.seekBlock(blockName)) != 0)
            {
                return result;
            }

            if ((result = iniFile.readIdULong("numFrames", animStates[i].numFrames)) != 0)
            {
                return result;
            }

            if ((result = iniFile.readIdFloat("frameRate", animStates[i].frameRate)) != 0)
            {
                return result;
            }
        }
    }
    else
    {
        numAnimStates = 0;
        animStates = nullptr;
    }

    iniFile.close();
    return 0;
}

auto VFXBuildingAppearanceType::getShape(uint32_t frame) -> Shape*
{
    const uint32_t packet = frame + FIRST_FRAME_PACKET;

    if (numPackets <= packet)
    {
        return nullptr;
    }

    Shape* shape = shapeList[packet];

    if (shape != nullptr)
    {
        shape->lastTurnUsed = turn;
        return shape;
    }

    dynamicFrameTiming = 0;
    shape = spriteManager->getShapeData(appearanceNum & 0xffffff, packet, turn, this, 0);
    shapeList[packet] = shape;
    return shape;
}

auto VFXBuildingAppearanceType::getTileShape(uint32_t tileNum) -> Shape*
{
    if (tileNum >= NUM_TILE_SHAPES || numPackets <= tileNum)
    {
        return nullptr;
    }

    Shape* shape = shapeList[tileNum];

    if (shape != nullptr)
    {
        shape->lastTurnUsed = turn;
        return shape;
    }

    dynamicFrameTiming = 0;
    shape = spriteManager->getShapeData(appearanceNum & 0xffffff, tileNum, turn, this, 0);
    shapeList[tileNum] = shape;
    return shape;
}

auto VFXBuildingAppearanceType::destroy() -> void
{
    // The shapes stay in the sprite manager's cache, ownerless.
    for (int32_t i = 0; i < static_cast<int32_t>(numPackets); i++)
    {
        if (shapeList[i] != nullptr)
        {
            shapeList[i]->owner = nullptr;
        }
    }

    // Faithful: the animation states are not freed (they go with the data heap).
    spriteManager->freeDataRAM(shapeList);
    shapeList = nullptr;
}

//---------------------------------------------------------------------------
// VFXBuildingAppearance
//---------------------------------------------------------------------------

auto VFXBuildingAppearance::init(AppearanceType* tree, GameObject* obj) -> int32_t
{
    fadeTable = nullptr;
    damageSet = 0;
    damageLevel = 0;
    inView = 0;
    animState = -1;
    const int32_t result = VFXAppearance::init(tree, obj);
    buildType = static_cast<VFXBuildingAppearanceType*>(tree);
    return result;
}

auto VFXBuildingAppearance::update() -> int32_t
{
    if (visible != 0 && owner->isCaptured() != 0 && highlighted == 0 && highlighting == 0)
    {
        highlighting = 1;
        highlightTime = 3.0f;
    }

    return 1;
}

auto VFXBuildingAppearance::setDamageLvl(uint32_t newDamageLevel) -> void
{
    damageSet = 1;
    const uint32_t level = newDamageLevel & 0xf;
    damageLevel = level;

    if (level != 0 && buildType->numFrames <= level)
    {
        damageLevel = buildType->numFrames - 1;
    }

    if (tileNum > 9)
    {
        tileNum = 0;
    }
}

auto VFXBuildingAppearance::recalcBounds(Camera*) -> int
{
    // Faithful: without a tile shape the visibility test takes in the previous shape's bounds.
    float tileMinX = shapeMinX;
    float tileMinY = shapeMinY;
    float tileMaxX = shapeMaxX;
    float tileMaxY = shapeMaxY;
    int result = 0;

    if (currentShape != nullptr && currentShape->frameList != nullptr)
    {
        uint8_t* shapeTable = currentShape->frameList;
        result = VFX_shape_minxy(shapeTable, 0);
        shapeMinX = static_cast<float>(result >> 16);
        shapeMinY = static_cast<float>(static_cast<int16_t>(result));
        result = VFX_shape_resolution(shapeTable, 0);
        shapeMaxX = static_cast<float>(result >> 16);
        shapeMaxY = static_cast<float>(static_cast<int16_t>(result));

        if (tileShape != nullptr && tileShape->frameList != nullptr)
        {
            uint8_t* tileTable = tileShape->frameList;
            result = VFX_shape_minxy(tileTable, 0);
            tileMinX = static_cast<float>(result >> 16);
            tileMinY = static_cast<float>(static_cast<int16_t>(result));
            result = VFX_shape_resolution(tileTable, 0);
            tileMaxX = static_cast<float>(result >> 16);
            tileMaxY = static_cast<float>(static_cast<int16_t>(result));
        }

        inView = 1;
    }

    if (eye == nullptr)
    {
        return result;
    }

    // Faithful: the camera passed in is ignored; the eye's zoom and screen position 0 are used.
    Building* building = owner->objectClass == BUILDING ? static_cast<Building*>(owner) : nullptr;
    const float scale = eye->cameraScale == 1 ? 0.5f : 1.0f;
    const vector_2d pos = owner->getScreenPos(0);
    float x = pos.x;
    float y = pos.y;

    if (building != nullptr)
    {
        x = static_cast<float>(building->pixelOffsetX) * scale + x;
        y = static_cast<float>(building->pixelOffsetY) * scale + y;
    }

    upperLeft.x = scale * shapeMinX + x;
    upperLeft.y = scale * shapeMinY + y;
    lowerRight.x = scale * shapeMaxX + upperLeft.x;
    lowerRight.y = scale * shapeMaxY + upperLeft.y;

    // On screen when the building or its tile is.
    const float minX = shapeMinX > tileMinX ? tileMinX : shapeMinX;
    const float minY = shapeMinY > tileMinY ? tileMinY : shapeMinY;
    const float maxX = shapeMaxX < tileMaxX ? tileMaxX : shapeMaxX;
    const float maxY = shapeMaxY < tileMaxY ? tileMaxY : shapeMaxY;
    const float left = minX * scale + x;
    const float top = minY * scale + y;
    const float bottom = maxY * scale + top;

    if (0.0f <= maxX * scale + left && 0.0f <= bottom &&
        left <= static_cast<float>(static_cast<int32_t>(std::floor(eye->viewWidth))) &&
        top <= static_cast<float>(static_cast<int32_t>(std::floor(eye->viewHeight))))
    {
        return 1;
    }

    return result;
}

auto VFXBuildingAppearance::calcCollideBounds() -> void
{
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    GameObject* obj = owner;

    if (obj->objectClass == BUILDING && obj != nullptr)
    {
        offsetX = static_cast<float>(static_cast<Building*>(obj)->pixelOffsetX);
        offsetY = static_cast<float>(static_cast<Building*>(obj)->pixelOffsetY);
    }

    upperLeft.x = offsetX;
    upperLeft.y = offsetY;
    lowerRight.y = offsetY;
    lowerRight.x = offsetX;

    if (currentShape == nullptr || currentShape->frameList == nullptr)
    {
        return;
    }

    uint8_t* shapeTable = currentShape->frameList;
    int32_t frame = currentFrame;
    const int32_t numShapeFrames = VFX_shape_count(shapeTable);

    if (frame == -1)
    {
        frame = 0;
    }

    if (numShapeFrames <= frame)
    {
        frame = numShapeFrames - 1;
    }

    const int32_t minXY = VFX_shape_minxy(shapeTable, frame);
    upperLeft.x = static_cast<float>(minXY >> 16) + offsetX;
    upperLeft.y = static_cast<float>(static_cast<int16_t>(minXY)) + offsetY;
    const int32_t size = VFX_shape_resolution(shapeTable, frame);
    lowerRight.x = static_cast<float>(size >> 16) + upperLeft.x;
    lowerRight.y = static_cast<float>(static_cast<int16_t>(size)) + upperLeft.y;
}

auto VFXBuildingAppearance::render(int32_t) -> int32_t
{
    Building* building = nullptr;

    if (owner->objectClass == BUILDING && owner != nullptr)
    {
        building = static_cast<Building*>(owner);
        screenPos = building->getScreenPos(eye->cameraId - 1);
    }

    VFXBuildingAppearanceType* type = buildType;
    Shape* shape = type->getShape(damageLevel);
    currentShape = shape;
    Shape* tile = type->getTileShape(tileNum);
    tileShape = tile;
    const float scale = eye->cameraScale == 1 ? 0.5f : 1.0f;
    const bool fullSize = scale == 1.0;

    // The tile goes under everything; zoomed out uses the table's second (small) frame.
    ElementList->openGroup(20000000, 1);

    if (tile != nullptr && tile->frameList != nullptr)
    {
        addShape(tile->frameList, screenPos.x, screenPos.y, fullSize ? 0 : 1, fadeTable,
                 fullSize ? "bactor1" : "bactor2");
    }

    if (building != nullptr)
    {
        screenPos.x = static_cast<float>(building->pixelOffsetX) * scale + screenPos.x;
        screenPos.y = static_cast<float>(building->pixelOffsetY) * scale + screenPos.y;
    }

    if (shape != nullptr && shape->frameList != nullptr)
    {
        if (animState == -1)
        {
            ElementList->openGroup(static_cast<int32_t>(-screenPos.y), 1);
            addShape(shape->frameList, screenPos.x, screenPos.y, fullSize ? 0 : 1, fadeTable,
                     fullSize ? "bactor4" : "bactor5");
        }
        else
        {
            const int32_t numShapeFrames = VFX_shape_count(shape->frameList);

            if (numShapeFrames <= currentFrame)
            {
                currentFrame = numShapeFrames - 1;
            }

            ElementList->openGroup(static_cast<int32_t>(-screenPos.y), 1);
            addShape(shape->frameList, screenPos.x, screenPos.y, currentFrame, fadeTable, "bactor3");
        }
    }

    if (owner->selected == 1 || owner->getNumAttackers() > 0)
    {
        drawBars();
    }

    GameObject* obj = owner;

    if (obj != nullptr && obj->selected != 0)
    {
        const int32_t homeAlignment = homeTeam->alignment;

        if (obj->getAlignment() != homeAlignment && obj->getAlignment() != 0 && static_cast<uint8_t>(obj->status) != 2)
        {
            recalcBounds(eye);
            drawSelectBrackets(0xcf);
        }
        else
        {
            recalcBounds(eye);
            drawSelectBox(0xe1);
        }
    }

    if (drawTerrainGrid != 0)
    {
        drawSelectBox(0xe1);
    }

    if (highlighting != 0)
    {
        if (highlightTime > 0.0f)
        {
            highlightTime -= frameLength;
            drawSelectBox(0xfc);
            return 0;
        }

        highlighted = 1;
    }

    return 0;
}

auto VFXBuildingAppearance::destroy() -> void
{
    appearType->removeUsers(this);
}

auto VFXBuildingAppearance::drawBars() -> void
{
    MCDrawDamageBar(this, getAppearanceType(), owner);
}
