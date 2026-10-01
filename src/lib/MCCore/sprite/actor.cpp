#include "stdafx.h"
#include "sprite/actor.h"
#include "camera/camera.h"
#include "engine/ceglist.h"
#include "engine/cepane.h"
#include "engine/cepoly.h"
#include "engine/cevfx.h"
#include "lib/aerror.h"
#include "lib/inifile.h"
#include "lib/routines.h"
#include "main/main.h"
#include "object/gameobj.h"
#include "object/tbldng.h"
#include "object/team.h"
#include "object/terrobj.h"
#include "object/tree.h"
#include "sprite/bactor.h"
#include "sprite/sprtmgr.h"
#include "sprite/vfxshape.h"
#include "terrain/terrain.h"
#include "vfx/vfxfuncs.h"

namespace
{
    constexpr int32_t NO_DATA_RAM = static_cast<int32_t>(0xbead0002);

    /// <summary>
    /// The pixel offset of the terrain-placed classes (tree, terrain object, the unused class 0x17, tree building),
    /// all at +0x8c/+0x90. False for any other class.
    /// </summary>
    auto pixelOffset(GameObject* obj, int32_t& offsetX, int32_t& offsetY) -> bool
    {
        switch (static_cast<int32_t>(obj->objectClass))
        {
            case TREE:
            {
                offsetX = static_cast<Tree*>(obj)->pixelOffsetX;
                offsetY = static_cast<Tree*>(obj)->pixelOffsetY;
                return true;
            }
            case TERRAINOBJECT:
            case 0x17: // No class sets 0x17; the original reads the same offsets.
            {
                offsetX = static_cast<TerrainObject*>(obj)->pixelOffsetX;
                offsetY = static_cast<TerrainObject*>(obj)->pixelOffsetY;
                return true;
            }
            case TREEBUILDING:
            {
                offsetX = static_cast<TreeBuilding*>(obj)->pixelOffsetX;
                offsetY = static_cast<TreeBuilding*>(obj)->pixelOffsetY;
                return true;
            }
            default:
                return false;
        }
    }

    /// <summary>0.5 when <paramref name="cam"/> is zoomed out, else 1.</summary>
    auto zoomScale(const Camera* cam) -> float
    {
        return cam->cameraScale == 1 ? 0.5f : 1.0f;
    }

    /// <summary>Reads the fields shared by states and sub-states.</summary>
    auto readStateData(FitIniFile& iniFile, ActorData& data) -> int32_t
    {
        int32_t result = iniFile.readIdULong("NumFrames", data.numFrames);

        if (result == 0)
        {
            result = iniFile.readIdFloat("FrameRate", data.frameRate);
        }

        if (result == 0)
        {
            result = iniFile.readIdULong("BasePacketNumber", data.basePacketNumber);
        }

        if (result == 0)
        {
            result = iniFile.readIdUChar("NumRotations", data.numRotations);
        }

        if (result == 0)
        {
            result = iniFile.readIdUChar("Symmetrical", data.symmetrical);
        }

        return result;
    }
}

//---------------------------------------------------------------------------
// VFXAppearanceType
//---------------------------------------------------------------------------

auto VFXAppearanceType::init(File* apprFile, uint32_t fileSize, uint32_t loadFlags) -> int32_t
{
    const int32_t result = loadIniFile(apprFile, fileSize);

    if (result != 0)
    {
        return result;
    }

    const uint32_t packetFile = appearanceNum & 0xffffff;
    numPackets = static_cast<int16_t>(spriteManager->getNumShapes(packetFile));
    const uint32_t listSize = static_cast<uint32_t>(numPackets) * sizeof(Shape*);
    shapeList = static_cast<Shape**>(spriteManager->mallocDataRAM(listSize));

    if (shapeList == nullptr)
    {
        return NO_DATA_RAM;
    }

    memclear(shapeList, static_cast<int>(listSize));
    keepLoaded = static_cast<int32_t>(loadFlags);

    if (loadFlags != 0)
    {
        // Load the first (full size) shape now.
        const uint32_t first = scaled != 0 ? 1 : 0;
        shapeList[first] = spriteManager->getShapeData(packetFile, first, 1, this, 0);
    }

    return 0;
}

auto VFXAppearanceType::removeShape(Shape* shape) -> void
{
    for (int32_t i = 0; i < numPackets; i++)
    {
        if (shapeList[i] == shape)
        {
            shapeList[i] = nullptr;
        }
    }

    for (AppearanceUser* user = userList; user != nullptr; user = user->next)
    {
        auto* appearance = static_cast<VFXAppearance*>(user->user);

        if (appearance->currentShape == shape)
        {
            appearance->currentShape = nullptr;
        }
    }
}

auto VFXAppearanceType::loadIniFile(File* apprFile, uint32_t fileSize) -> int32_t
{
    FitIniFile iniFile;
    int32_t result = iniFile.open(apprFile, fileSize, 50);

    if (result != 0)
    {
        return result;
    }

    result = iniFile.seekBlock("Main Info");

    if (result != 0)
    {
        return result;
    }

    int32_t deltaValue = 0;

    if (iniFile.readIdLong("delta", deltaValue) != 0)
    {
        deltaValue = 0;
    }

    delta = deltaValue;

    result = iniFile.seekBlock("States");

    if (result != 0)
    {
        return result;
    }

    result = iniFile.readIdUChar("NumStates", numStates);

    if (result != 0)
    {
        return result;
    }

    if (iniFile.readIdUChar("Scaled", scaled) != 0)
    {
        scaled = 0;
    }

    constexpr uint32_t STATE_TABLE_SIZE = MAX_ACTOR_STATES * sizeof(ActorData);
    actorStateData = static_cast<ActorData*>(spriteManager->mallocDataRAM(STATE_TABLE_SIZE));

    if (actorStateData == nullptr)
    {
        return NO_DATA_RAM;
    }

    memclear(actorStateData, STATE_TABLE_SIZE);

    for (int32_t i = 0; i < numStates; i++)
    {
        ActorData& data = actorStateData[i];
        char blockName[20];
        sprintf(blockName, "State%d", i);
        result = iniFile.seekBlock(blockName);
        uint8_t stateValue = 0;

        if (result == 0)
        {
            result = iniFile.readIdUChar("State", stateValue);
        }

        if (result != 0)
        {
            return result;
        }

        data.state = static_cast<ActorState>(stateValue);

        uint8_t numSubStates = 0;

        if (iniFile.readIdUChar("SubStates", numSubStates) != 0)
        {
            numSubStates = 0;
        }

        result = readStateData(iniFile, data);

        if (result != 0)
        {
            return result;
        }

        // Only the first state with sub-states gets them (in practice state 0).
        if (numSubStates == 0 || actorSubStateData != nullptr)
        {
            continue;
        }

        const uint32_t subTableSize = numSubStates * sizeof(ActorData);
        actorSubStateData = static_cast<ActorData*>(spriteManager->mallocDataRAM(subTableSize));

        if (actorSubStateData == nullptr)
        {
            return NO_DATA_RAM;
        }

        memclear(actorSubStateData, static_cast<int>(subTableSize));

        for (int32_t j = 0; j < numSubStates; j++)
        {
            ActorData& subData = actorSubStateData[j];
            char subBlockName[20];
            sprintf(subBlockName, "Sub%dState%d", j, i);
            result = iniFile.seekBlock(subBlockName);
            uint8_t subStateValue = 0;

            if (result == 0)
            {
                result = iniFile.readIdUChar("State", subStateValue);
            }

            if (result != 0)
            {
                return result;
            }

            subData.state = static_cast<ActorState>(subStateValue);
            result = iniFile.readIdUChar("Sub", subData.subState);

            if (result == 0)
            {
                result = readStateData(iniFile, subData);
            }

            if (result == 0)
            {
                result = iniFile.readIdUChar("Loop", subData.loop);
            }

            if (result != 0)
            {
                return result;
            }
        }
    }

    iniFile.close();
    return 0;
}

auto VFXAppearanceType::getShape(ActorState state, uint8_t subState, int32_t rotation, int32_t, float& frameRate,
                                 int& reverse) -> Shape*
{
    const ActorData& data = actorStateData[state];
    reverse = 0;

    if (data.numFrames == 0)
    {
        return nullptr;
    }

    // Symmetrical states mirror the negative facings; the others wrap them.
    int isReversed = 0;

    if (rotation < 0 && data.symmetrical != 0)
    {
        isReversed = 1;
        rotation = -rotation;
    }
    else if (rotation < 0 && data.symmetrical == 0)
    {
        rotation += 360;
    }

    reverse = isReversed;

    int32_t rotationIndex = static_cast<int32_t>(
        std::floor(static_cast<double>(static_cast<int32_t>(data.numRotations) * rotation) * (1.0 / 360.0)));
    frameRate = data.frameRate;
    uint32_t basePacket = data.basePacketNumber;

    if (subState != 0xff && actorSubStateData != nullptr)
    {
        basePacket = actorSubStateData[subState].basePacketNumber;
        frameRate = actorSubStateData[subState].frameRate;
    }

    float zoom = 1.0f;

    if (eye != nullptr && eye->cameraScale == 1)
    {
        zoom = 0.5f;
    }

    // Scaled types alternate full size and zoomed out packets.
    if (scaled != 0)
    {
        rotationIndex <<= 1;
    }

    uint32_t packet = basePacket + static_cast<uint32_t>(rotationIndex);

    if (zoom != 1.0f && scaled != 0)
    {
        packet++;
    }

    if (numPackets <= static_cast<int32_t>(packet))
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
    shape = spriteManager->getShapeData(appearanceNum & 0xffffff, packet, turn, this, zoom != 1.0f ? 1 : 0);
    shapeList[packet] = shape;
    return shape;
}

auto VFXAppearanceType::destroy() -> void
{
    // The shapes stay in the sprite manager's cache, ownerless.
    for (int32_t i = 0; i < numPackets; i++)
    {
        if (shapeList[i] != nullptr)
        {
            shapeList[i]->owner = nullptr;
        }
    }

    spriteManager->freeDataRAM(userList);
    userList = nullptr;
    spriteManager->freeDataRAM(shapeList);
    shapeList = nullptr;
    spriteManager->freeDataRAM(actorStateData);
    actorStateData = nullptr;
}

//---------------------------------------------------------------------------
// VFXAppearance
//---------------------------------------------------------------------------

auto VFXAppearance::init(AppearanceType* tree, GameObject* obj) -> int32_t
{
    unknown04 = 0x70000000;
    visible = 0;
    owner = obj;
    appearType = static_cast<VFXAppearanceType*>(tree);

    if (tree != nullptr)
    {
        tree->addUsers(this);
    }

    currentShape = nullptr;
    currentFrame = -1;
    loopEnd = -1;
    loopStart = -1;
    shapeMinY = -15.0f;
    shapeMinX = -15.0f;
    unknown44 = 0;
    visible = 0;
    unknown48 = 0;
    currentTime = 0.0f;
    lastFrame = 0;
    fadeTable = nullptr;
    currentState = ACTOR_STATE_NORMAL;
    inView = 0;
    currentSubState = 0xff;
    shapeMaxY = 15.0f;
    shapeMaxX = 15.0f;
    typeChanged = 1;
    return 0;
}

auto VFXAppearance::recalcBounds(Camera* cam) -> int
{
    int result = 0;

    if (currentShape != nullptr && currentShape->frameList != nullptr)
    {
        uint8_t* shapeTable = currentShape->frameList;
        const int32_t minXY = VFX_shape_minxy(shapeTable, 0);
        shapeMinX = static_cast<float>(minXY >> 16);
        shapeMinY = static_cast<float>(static_cast<int16_t>(minXY));
        result = VFX_shape_resolution(shapeTable, 0);
        shapeMaxX = static_cast<float>(result >> 16);
        inView = 1;
        shapeMaxY = static_cast<float>(static_cast<int16_t>(result));
    }

    if (cam == nullptr)
    {
        return result;
    }

    const vector_2d pos = owner->getScreenPos(cam->cameraId - 1);
    float x = pos.x;
    float y = pos.y;
    float scale = zoomScale(cam);
    int32_t offsetX = 0;
    int32_t offsetY = 0;

    if (pixelOffset(owner, offsetX, offsetY))
    {
        x += static_cast<float>(offsetX) * scale;
        y += static_cast<float>(offsetY) * scale;

        // Faithful: trees and tree buildings then take their bounds at full size.
        if (owner->objectClass == TREE || owner->objectClass == TREEBUILDING)
        {
            scale = 1.0f;
        }
    }

    upperLeft.x = scale * shapeMinX + x;
    upperLeft.y = scale * shapeMinY + y;
    lowerRight.x = scale * shapeMaxX + upperLeft.x;
    lowerRight.y = scale * shapeMaxY + upperLeft.y;

    if (0.0f <= lowerRight.x && 0.0f <= lowerRight.y &&
        upperLeft.x <= static_cast<float>(static_cast<int32_t>(std::floor(cam->viewWidth))) &&
        upperLeft.y <= static_cast<float>(static_cast<int32_t>(std::floor(cam->viewHeight))))
    {
        return 1;
    }

    return result;
}

auto VFXAppearance::render(int32_t depthFixup) -> int32_t
{
    GameObject* obj = owner;
    screenPos = obj->getScreenPos(eye->cameraId - 1);

    // The facing in degrees, negative to the right.
    const frame_of_ref frame = obj->getFrame();
    float cosFacing = UnitX.x * frame.i.x + UnitX.y * frame.i.y + UnitX.z * frame.i.z;

    if (cosFacing < -1.0)
    {
        cosFacing = -1.0f;
    }

    if (cosFacing > 1.0)
    {
        cosFacing = 1.0f;
    }

    double facing = std::acos(static_cast<double>(cosFacing)) * 57.29577951308232;

    if (frame.i.y < 0.0)
    {
        facing = -facing;
    }

    const uint8_t subState = currentSubState;
    float frameRate = 0.0f;
    int reverse = 0;
    Shape* shape =
        appearType->getShape(currentState, subState, static_cast<int32_t>(facing), currentFrame, frameRate, reverse);
    currentShape = shape;

    if (obj->isCaptured() != 0 && highlighted == 0 && highlighting == 0)
    {
        highlighting = 1;
        highlightTime = 3.0f;
    }

    const float scale = zoomScale(eye);
    int32_t offsetX = 0;
    int32_t offsetY = 0;

    if (pixelOffset(obj, offsetX, offsetY))
    {
        screenPos.x = static_cast<float>(offsetX) * scale + screenPos.x;
        screenPos.y = static_cast<float>(offsetY) * scale + screenPos.y;
    }

    VFXAppearanceType* type = appearType;
    uint32_t numFrames = type->actorStateData[currentState].numFrames;

    if (subState != 0xff)
    {
        numFrames = type->actorSubStateData[subState].numFrames;
    }

    const int32_t depth = static_cast<int32_t>(static_cast<float>(depthFixup) - screenPos.y);

    if (type->delta != 0 && static_cast<int32_t>(numFrames) >= 2)
    {
        // Delta-compressed animations draw unscaled (only when the type is scaled).
        if (shape != nullptr && shape->frameList != nullptr && type->scaled != 0)
        {
            ElementList->openGroup(depth, 1);
            auto* element = new DeltaElement(shape->frameList, static_cast<int32_t>(screenPos.x),
                                             static_cast<int32_t>(screenPos.y), currentFrame, 0, fadeTable, 1, 0);
            ElementList->add(element);
        }
    }
    else if (shape != nullptr && shape->frameList != nullptr)
    {
        ElementList->openGroup(depth, 1);
        // Scaled types have their own zoomed out shapes, so they draw unscaled.
        const int noScaleDraw = type->scaled != 0 ? 1 : 0;
        auto* element =
            new VFXElement(shape->frameList, screenPos.x, screenPos.y, currentFrame, 0, fadeTable, noScaleDraw, 0);

        // Port fix: the original copies the debug name through a null element too.
        if (element != nullptr)
        {
            strcpy(element->name, noScaleDraw != 0 ? "actor1" : "actor2");
        }

        ElementList->add(element);
    }

    if (owner->selected == 1 || owner->getNumAttackers() > 0)
    {
        drawBars();
    }

    if (highlighting != 0)
    {
        if (highlightTime <= 0.0f)
        {
            highlighted = 1;
        }
        else
        {
            highlightTime -= frameLength;
            drawSelectBox(0xfc);
        }
    }

    obj = owner;

    if (obj->selected != 0)
    {
        const int32_t homeAlignment = homeTeam->alignment;

        if (obj->getAlignment() == homeAlignment || obj->getAlignment() == 0 || static_cast<uint8_t>(obj->status) == 2)
        {
            drawSelectBox(0xe1);
        }
        else
        {
            drawSelectBrackets(0xcf);
        }
    }

    if (drawTerrainGrid != 0)
    {
        recalcBounds(eye);
        drawSelectBox(0xe1);
    }

    return 0;
}

auto VFXAppearance::setDamageLvl(uint32_t damageLevel) -> void
{
    damageSet = 1;

    if (damageLevel == 0)
    {
        setTypeId(ACTOR_STATE_NORMAL, 0xff);
        return;
    }

    const uint32_t blowUp1Frames = appearType->actorStateData[ACTOR_STATE_BLOWING_UP1].numFrames;

    if (appearType->actorStateData[ACTOR_STATE_BLOWING_UP2].numFrames + blowUp1Frames <= damageLevel)
    {
        setTypeId(ACTOR_STATE_DESTROYED, 0xff);
        return;
    }

    if (blowUp1Frames <= damageLevel)
    {
        setTypeId(ACTOR_STATE_BLOWING_UP2, 0xff);
        currentFrame = static_cast<int32_t>(damageLevel - blowUp1Frames);
        return;
    }

    setTypeId(ACTOR_STATE_BLOWING_UP1, 0xff);
    currentFrame = static_cast<int32_t>(damageLevel);
}

auto VFXAppearance::update() -> int32_t
{
    const int32_t startFrame = currentFrame;

    if (startFrame == -1)
    {
        currentFrame = 0;
    }

    VFXAppearanceType* type = appearType;
    const uint8_t subState = currentSubState;
    framesAdvanced = 0.0f;
    const ActorData& data = type->actorStateData[currentState];
    uint32_t numFrames = data.numFrames;

    if (subState != 0xff)
    {
        numFrames = type->actorSubStateData[subState].numFrames;
    }

    if (numFrames <= 1)
    {
        return 1;
    }

    // Faithful: the state's frame rate, even in a sub-state.
    currentTime = frameLength + currentTime;
    const float frames = currentTime * data.frameRate;
    const int32_t played = lastFrame;

    if (frames < static_cast<float>(played))
    {
        Fatal(played, " Frame Count wrong", nullptr);
    }

    const int32_t wholeFrames = static_cast<int32_t>(std::floor(static_cast<double>(frames)));

    if (played < wholeFrames)
    {
        framesAdvanced = static_cast<float>(wholeFrames - played);
        lastFrame = wholeFrames;
    }

    if (framesAdvanced == 0.0f)
    {
        return 1;
    }

    currentFrame = static_cast<int32_t>(static_cast<double>(static_cast<uint32_t>(currentFrame)) + framesAdvanced);

    if (startFrame == -1 && type->delta != 0)
    {
        currentFrame = 0;
    }

    uint32_t stateFrames = data.numFrames;
    uint8_t loop = 1;

    if (subState != 0xff)
    {
        loop = type->actorSubStateData[subState].loop;
        stateFrames = type->actorSubStateData[subState].numFrames;
    }

    const uint32_t frameNum = static_cast<uint32_t>(currentFrame);

    if (stateFrames <= frameNum)
    {
        if (loop != 0 && loopEnd == -1)
        {
            currentFrame = static_cast<int32_t>(frameNum % stateFrames);
            return 0;
        }

        if (loop == 0)
        {
            currentFrame = static_cast<int32_t>(stateFrames - 1);
            return 1;
        }
    }

    if (static_cast<uint32_t>(loopEnd) <= frameNum)
    {
        currentFrame = loopStart;
        return 0;
    }

    return 1;
}

auto VFXAppearance::destroy() -> void
{
    appearType->removeUsers(this);
    appearanceTypeList->removeAppearance(appearType);
}

auto VFXAppearance::stateExists(ActorState state) -> int32_t
{
    if (static_cast<int32_t>(state) < static_cast<int32_t>(appearType->numStates) && state >= 0)
    {
        return static_cast<int32_t>(appearType->actorStateData[state].numFrames);
    }

    return 0;
}

auto VFXAppearance::drawBars() -> void
{
    // Only tree buildings get a damage bar here.
    GameObject* obj = owner;
    const bool hasBar = obj->objectClass == TREEBUILDING && obj != nullptr;
    MCDrawDamageBar(this, getAppearanceType(), hasBar ? obj : nullptr);
}

auto MCDrawDamageBar(Appearance* appearance, AppearanceType* type, GameObject* obj) -> void
{
    // The bar sits centred above the type's bounds (or the shape's, when the type has none).
    const vector_2d& screenPos = appearance->screenPos;
    const vector_2d& upperLeft = appearance->upperLeft;
    const vector_2d& lowerRight = appearance->lowerRight;
    float left;
    float top;
    float right;

    if (type == nullptr || (type->boundsUpperLeftX == 0 && type->boundsUpperLeftY == 0 &&
                            type->boundsLowerRightX == 0 && type->boundsLowerRightY == 0))
    {
        left = upperLeft.x;
        top = upperLeft.y;
        right = lowerRight.x;
    }
    else
    {
        const int shift = eye->cameraScale == 1 ? 1 : 0;
        left = static_cast<float>(type->boundsUpperLeftX >> shift) + screenPos.x;
        top = static_cast<float>(type->boundsUpperLeftY >> shift) + screenPos.y;
        right = static_cast<float>(type->boundsLowerRightX >> shift) + screenPos.x;
    }

    const float scale = zoomScale(eye);
    const float gap = scale * 5.0f;
    const float barWidth = scale * 38.0f;
    const float barHeight = scale * 4.0f;
    const float barY = top - gap - barHeight - gap - barHeight;
    const float barX = (left + (right - left) * 0.5f) - barWidth * 0.5f;

    if (obj == nullptr)
    {
        return;
    }

    ElementList->openGroup(-50000, 1);
    PolyElementData data;
    data.numVertices = 0;
    data.textureMapOff = 0;
    data.unknown98 = 0;
    data.texture = nullptr;
    data.textureWidth = 0;
    data.textureHeight = 0;
    data.fadeTable = nullptr;
    data.translate = 0;
    data.statusBar = 1;
    data.barColor = 0x101;
    data.vertices[0].x = static_cast<int32_t>(barX - 1.0f);
    data.vertices[0].y = static_cast<int32_t>(barY - 1.0f);
    data.vertices[1].x = static_cast<int32_t>(barX + barWidth + 1.0f);
    data.vertices[1].y = static_cast<int32_t>(barY + barHeight + 1.0f);

    // Tree building and building types both keep their damage level at +0x30.
    auto typeDmgLevel = [obj]() -> float
    {
        return static_cast<float>(static_cast<int32_t>(static_cast<TreeBuildingType*>(obj->getObjectType())->dmgLevel));
    };

    float remaining = typeDmgLevel() - obj->getDamage();

    if (remaining < 0.0f)
    {
        remaining = 0.0f;
    }

    const float ratio = remaining / typeDmgLevel();
    float barLength = ratio * barWidth;

    // A building that isn't quite destroyed shows at least one pixel.
    if (barLength < 1.0 && ratio > 0.001)
    {
        barLength = 1.0f;
    }

    data.barPercent = static_cast<int32_t>(barLength);

    if (data.barPercent > 0)
    {
        ElementList->add(new PolygonElement(&data, -50000));
    }
}

auto VFXAppearance::setTypeId(ActorState state, uint8_t subState) -> void
{
    if (static_cast<int32_t>(state) < static_cast<int32_t>(appearType->numStates) && state >= 0)
    {
        currentState = state;
        currentFrame = -1;
        currentTime = 0.0f;
        lastFrame = 0;
        currentSubState = 0xff;

        if (state != ACTOR_STATE_NORMAL)
        {
            typeChanged = 1;
            return;
        }
    }

    if (subState != 0xff && state == ACTOR_STATE_NORMAL && appearType->actorSubStateData != nullptr)
    {
        currentSubState = subState;
    }

    typeChanged = 1;
}
