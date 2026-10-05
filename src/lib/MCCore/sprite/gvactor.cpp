#include "stdafx.h"
#include "sprite/gvactor.h"
#include "camera/camera.h"
#include "color/color.h"
#include "engine/ceglist.h"
#include "engine/cepoly.h"
#include "engine/cevfx.h"
#include "lib/inifile.h"
#include "main/main.h"
#include "object/artlry.h"
#include "object/gvehicl.h"
#include "object/team.h"
#include "object/train.h"
#include "object/turret.h"
#include "sprite/bactor.h"
#include "sprite/sprtmgr.h"
#include "sprite/vfxshape.h"
#include "terrain/terrain.h"
#include "vfx/vfxfuncs.h"

namespace
{
    constexpr int32_t NO_DATA_RAM = static_cast<int32_t>(0xeada000c);

    /// <summary>The facing of <paramref name="obj"/> in degrees, negative to the right.</summary>
    auto objectFacing(GameObject* obj) -> double
    {
        const frame_of_ref frame = obj->getFrame();
        float cosFacing = UnitX.z * frame.i.z + UnitX.x * frame.i.x + UnitX.y * frame.i.y;

        if (cosFacing < -1.0)
        {
            cosFacing = -1.0f;
        }

        if (cosFacing > 1.0)
        {
            cosFacing = 1.0f;
        }

        double facing = acosMatherr(static_cast<double>(cosFacing)) * 0x1.ca5dc1a6402aap+5;

        if (frame.i.y < 0.0)
        {
            facing = -facing;
        }

        return facing;
    }

    /// <summary>0.5 when the eye is zoomed out, else 1.</summary>
    auto eyeScale() -> float
    {
        return eye->cameraScale == 1 ? 0.5f : 1.0f;
    }
}

//---------------------------------------------------------------------------
// GVAppearanceType
//---------------------------------------------------------------------------

auto GVAppearanceType::init(File* apprFile, uint32_t fileSize, uint32_t loadFlags) -> int32_t
{
    const int32_t result = loadIniFile(apprFile, fileSize);

    if (result != 0)
    {
        return result;
    }

    keepLoaded = static_cast<int32_t>(loadFlags);
    numPackets = spriteManager->getNumShapes(appearanceNum & 0xffffff);
    // Port fix: sized by the port's pointer size (the original: count * 4).
    shapeList = static_cast<Shape**>(
        spriteManager->mallocDataRAM(static_cast<uint32_t>(numPackets) * static_cast<uint32_t>(sizeof(Shape*))));

    if (shapeList == nullptr)
    {
        return NO_DATA_RAM;
    }

    for (int32_t i = 0; i < numPackets; i++)
    {
        shapeList[i] = nullptr;
    }

    return 0;
}

auto GVAppearanceType::removeShape(Shape* shape) -> void
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
        auto* appearance = static_cast<GVAppearance*>(user->user);

        if (appearance->currentShape[0] == shape)
        {
            appearance->currentShape[0] = nullptr;
        }

        if (appearance->currentShape[1] == shape)
        {
            appearance->currentShape[1] = nullptr;
        }
    }
}

auto GVAppearanceType::loadIniFile(File* apprFile, uint32_t fileSize) -> int32_t
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

    if ((result = iniFile.readIdULong("NumParts", numParts)) != 0)
    {
        return result;
    }

    if ((result = iniFile.readIdFloat("TurretOffset", turretOffset)) != 0)
    {
        return result;
    }

    actorStateData = static_cast<GVActorData*>(spriteManager->mallocDataRAM(MAX_GV_ACTOR_STATES * sizeof(GVActorData)));

    if (actorStateData == nullptr)
    {
        return NO_DATA_RAM;
    }

    if ((result = iniFile.seekBlock("States")) != 0)
    {
        return result;
    }

    uint8_t numStates = 0;

    if ((result = iniFile.readIdUChar("NumStates", numStates)) != 0)
    {
        return result;
    }

    // Normal, damaged and destroyed, and optionally an extra state.
    if (numStates == 4)
    {
        hasExtraState = 1;
    }
    else if (numStates == 3)
    {
        hasExtraState = 0;
    }
    else
    {
        return static_cast<int32_t>(0xeada000e);
    }

    for (int32_t i = 0; i < numStates; i++)
    {
        char blockName[20];
        sprintf(blockName, "State%d", i);

        if ((result = iniFile.seekBlock(blockName)) != 0)
        {
            return result;
        }

        GVActorData& data = actorStateData[i];
        uint8_t state = 0;

        if ((result = iniFile.readIdUChar("State", state)) != 0)
        {
            return result;
        }

        data.state = static_cast<GVActorState>(state);

        if ((result = iniFile.readIdULong("NumFrames", data.numFrames)) != 0)
        {
            return result;
        }

        if ((result = iniFile.readIdFloat("FrameRate", data.frameRate)) != 0)
        {
            return result;
        }

        if ((result = iniFile.readIdULong("BasePacketNumber", data.basePacketNumber)) != 0)
        {
            return result;
        }

        if ((result = iniFile.readIdUChar("NumRotations", data.numRotations)) != 0)
        {
            return result;
        }
    }

    iniFile.close();
    return 0;
}

auto GVAppearanceType::getShape(GVActorState state, int32_t rotation, int32_t part, float& frameRate) -> Shape*
{
    if (static_cast<int32_t>(numParts) <= part)
    {
        return nullptr;
    }

    const GVActorData& data = actorStateData[state];

    if (data.numFrames == 0)
    {
        return nullptr;
    }

    // Into 0..360 (the original's unsigned divisions; the operands are positive).
    if (rotation > 180)
    {
        rotation -= static_cast<int32_t>((static_cast<uint32_t>(rotation) + 179u) / 360u) * 360;
    }

    if (rotation < -180)
    {
        rotation += static_cast<int32_t>((179u - static_cast<uint32_t>(rotation)) / 360u) * 360;
    }

    if (rotation < 0)
    {
        rotation += 360;
    }

    const uint32_t numRotations = data.numRotations;
    frameRate = data.frameRate;
    const int32_t rotationIndex = static_cast<int16_t>(static_cast<int32_t>(std::floor(
        static_cast<double>(static_cast<int32_t>(numRotations * static_cast<uint32_t>(rotation))) * (1.0 / 360.0))));
    // The turret's rotations follow the body's.
    uint32_t basePacket = data.basePacketNumber;

    if (part > 0)
    {
        basePacket += numRotations * static_cast<uint32_t>(part);
    }

    const uint32_t packet = basePacket + static_cast<uint32_t>(rotationIndex);

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
    // Faithful: always asks for the zoomed out version (the sprite manager ignores it).
    shape = spriteManager->getShapeData(appearanceNum & 0xffffff, packet, turn, this, 1);
    shapeList[packet] = shape;
    return shape;
}

auto GVAppearanceType::destroy() -> void
{
    // The shapes stay in the sprite manager's cache, ownerless.
    for (int32_t i = 0; i < numPackets; i++)
    {
        if (shapeList[i] != nullptr)
        {
            shapeList[i]->owner = nullptr;
        }
    }

    spriteManager->freeDataRAM(shapeList);
    shapeList = nullptr;
    spriteManager->freeDataRAM(actorStateData);
    actorStateData = nullptr;
}

//---------------------------------------------------------------------------
// GVAppearance
//---------------------------------------------------------------------------

auto GVAppearance::init(AppearanceType* tree, GameObject* obj) -> int32_t
{
    visible = 0;
    owner = obj;
    appearType = static_cast<GVAppearanceType*>(tree);

    if (tree != nullptr)
    {
        numParts = static_cast<int32_t>(appearType->numParts);
        turretOffset = appearType->turretOffset;
        tree->addUsers(this);
    }

    for (int32_t i = 0; i < 2; i++)
    {
        currentShape[i] = nullptr;
        currentFrame[i] = -1;
    }

    visible = 0;
    shapeMinY = -25.0f;
    shapeMinX = -25.0f;
    currentTime = 0.0f;
    lastFrame = 0;
    currentState = GV_ACTOR_STATE_NORMAL;
    inView = 0;
    bodyRotation = 0.0f;
    shapeMaxY = 50.0f;
    shapeMaxX = 50.0f;
    frameRate = 15.0f;
    fadeTableIndex = -1;
    return 0;
}

auto GVAppearance::recalcBounds(Camera* cam) -> int
{
    if (cam == nullptr)
    {
        return 0;
    }

    GameObject* obj = owner;
    const vector_2d pos = obj->getScreenPos(cam->cameraId - 1);
    float x = pos.x;
    float y = pos.y;

    if (obj->objectClass == TURRET)
    {
        const float camScale = cam->cameraScale == 1 ? 0.5f : 1.0f;
        x = static_cast<float>(static_cast<Turret*>(obj)->tileOffsetX) * camScale + x;
        y = static_cast<float>(static_cast<Turret*>(obj)->tileOffsetY) * camScale + y;
    }

    upperLeft.x = x;
    upperLeft.y = y;
    lowerRight.y = y;
    lowerRight.x = x;

    if (currentShape[0] != nullptr && currentShape[0]->frameList != nullptr)
    {
        // The bounds only ever grow: they cover every body frame drawn so far.
        uint8_t* shapeTable = currentShape[0]->frameList;
        const int32_t numShapeFrames = VFX_shape_count(shapeTable);
        int32_t frame = currentFrame[0];

        if (frame < 0)
        {
            frame = 0;
        }

        if (numShapeFrames <= frame)
        {
            frame = numShapeFrames - 1;
        }

        const int32_t minXY = VFX_shape_minxy(shapeTable, frame);

        if (static_cast<float>(minXY >> 16) < shapeMinX)
        {
            shapeMinX = static_cast<float>(minXY >> 16);
        }

        if (static_cast<float>(static_cast<int16_t>(minXY)) < shapeMinY)
        {
            shapeMinY = static_cast<float>(static_cast<int16_t>(minXY));
        }

        const int32_t size = VFX_shape_resolution(shapeTable, frame);

        if (shapeMaxX < static_cast<float>(size >> 16))
        {
            shapeMaxX = static_cast<float>(size >> 16);
        }

        if (shapeMaxY < static_cast<float>(static_cast<int16_t>(size)))
        {
            shapeMaxY = static_cast<float>(static_cast<int16_t>(size));
        }

        inView = 1;
    }

    // Faithful: the zoom is the eye's, the screen limits the camera's (not rounded down here).
    const float scale = eyeScale();
    upperLeft.x = scale * shapeMinX + x;
    upperLeft.y = scale * shapeMinY + y;
    lowerRight.x = scale * shapeMaxX + upperLeft.x;
    lowerRight.y = scale * shapeMaxY + upperLeft.y;

    if (0.0f <= lowerRight.x && 0.0f <= lowerRight.y && upperLeft.x <= cam->viewWidth && upperLeft.y <= cam->viewHeight)
    {
        return 1;
    }

    return 0;
}

auto GVAppearance::render(int32_t depthFixup) -> int32_t
{
    GameObject* obj = owner;
    screenPos = obj->getScreenPos(eye->cameraId - 1);
    const int32_t objectClass = obj->objectClass;
    const float scale = eyeScale();

    if (objectClass == TURRET)
    {
        screenPos.x = static_cast<float>(static_cast<Turret*>(obj)->tileOffsetX) * scale + screenPos.x;
        screenPos.y = static_cast<float>(static_cast<Turret*>(obj)->tileOffsetY) * scale + screenPos.y;
    }

    // The body's and the turret's facings.
    if (objectClass == GROUNDVEHICLE)
    {
        const double body = objectFacing(obj) + 5.0;
        bodyRotation = static_cast<float>(body);
        turretRotation = static_cast<float>(body + static_cast<GroundVehicle*>(obj)->turretRotation);
    }
    else if (objectClass == TURRET)
    {
        const float rotation = static_cast<Turret*>(obj)->turretRotation;
        turretRotation = rotation;
        bodyRotation = rotation;
    }
    else if (objectClass == TRAINCAR)
    {
        bodyRotation = static_cast<float>(objectFacing(obj) + 10.0);
        turretRotation = 0.0f;
    }
    else if (objectClass == CAMERADRONE)
    {
        const double facing = objectFacing(obj);
        bodyRotation = static_cast<float>(facing);
        turretRotation = 0.0f;
        // Faithful: 45 and -134 swap (-135 is left as it is).
        int32_t whole = static_cast<int32_t>(facing);

        if (whole == 45)
        {
            whole = -135;
        }
        else if (whole == -134)
        {
            whole = 45;
        }

        bodyRotation = static_cast<float>(whole);
    }
    while (turretRotation > 180.0f)
    {
        turretRotation -= 360.0f;
    }
    while (turretRotation < -180.0f)
    {
        turretRotation += 360.0f;
    }

    if (turretRotation < 0.0f)
    {
        turretRotation += 360.0f;
    }

    // The body snaps to its nearest drawn rotation.
    const GVActorState state = currentState;
    const int32_t numRotations = appearType->actorStateData[state].numRotations + 1;
    const int32_t rotationIndex = static_cast<int16_t>(
        static_cast<int32_t>(std::floor(static_cast<double>(numRotations) * bodyRotation * (1.0 / 360.0))));
    bodyRotation = static_cast<float>(360.0 / numRotations * rotationIndex);
    currentShape[0] = appearType->getShape(state, static_cast<int32_t>(bodyRotation), 0, frameRate);

    if (static_cast<uint32_t>(numParts) < 2 || currentState == GV_ACTOR_STATE_DESTROYED)
    {
        currentShape[1] = nullptr;
    }
    else
    {
        currentShape[1] = appearType->getShape(currentState, static_cast<int32_t>(turretRotation), 1, frameRate);
    }

    ElementList->openGroup(static_cast<int32_t>(static_cast<float>(depthFixup) - screenPos.y), 0);
    partOrder[0] = 0;
    partOrder[1] = 1;

    for (int32_t i = 0; i < numParts; i++)
    {
        const int32_t part = partOrder[i];

        if (currentShape[part] == nullptr || currentShape[part]->frameList == nullptr)
        {
            continue;
        }

        float offsetX = 0.0f;
        float offsetY = 0.0f;

        if (part == 1)
        {
            // The turret sits turretOffset meters forward of the body's centre.
            const double distance = static_cast<double>(eyeScale()) * turretOffset * worldUnitsPerMeter;
            offsetX = static_cast<float>(distance * std::sin(bodyRotation * 0x1.1df46a2526c7ap-6));
            offsetY = static_cast<float>(distance * std::cos(bodyRotation * 0x1.1df46a2526c7ap-6) * 0.5);
        }

        uint8_t* fadeTable = nullptr;

        if (fadeTableIndex != -1 && fadeTableIndex >= 0)
        {
            fadeTable =
                gamePalette->fadePalettes.get() + (fadeTableIndex + gamePalette->numBitmapHazeLevels * 2) * 0x100;
        }

        auto* element = ElementPool::Make<VFXElement>(currentShape[part]->frameList, screenPos.x - offsetX,
                                                      screenPos.y - offsetY, currentFrame[part], 0, fadeTable, 0, 0);

        // Port fix: the original copies the debug name through a null element too.
        if (element != nullptr)
        {
            strcpy(element->name, "gvactor");
        }

        ElementList->add(element);
    }

    const int32_t selected = owner->selected;

    if (selected == -1 || selected == 1)
    {
        recalcBounds(eye);
        drawBars();
    }
    else
    {
        if (selected == 2)
        {
            recalcBounds(eye);
            GameObject* selectedObj = owner;
            const int32_t alignment = selectedObj->getAlignment();

            if (alignment == -1)
            {
                drawSelectBrackets(0xfd);
            }
            else if (alignment == 0)
            {
                drawSelectBrackets(0xfe);
            }
            else if (alignment == 1)
            {
                drawSelectBrackets(selectedObj->getAlignment() == homeTeam->alignment ? 0xfc : 0xfb);
            }
        }

        if (owner->getNumAttackers() >= 1)
        {
            recalcBounds(eye);
            drawBars();
        }
    }

    if (drawTerrainGrid != 0)
    {
        recalcBounds(eye);
        drawSelectBox(0xfd);
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

auto GVAppearance::update() -> int32_t
{
    for (int32_t i = 0; i < numParts; i++)
    {
        if (currentFrame[i] == -1)
        {
            currentFrame[i] = 0;
        }
    }

    if (visible != 0 && owner->isCaptured() != 0 && highlighted == 0 && highlighting == 0)
    {
        highlighting = 1;
        highlightTime = 3.0f;
    }

    currentTime = frameLength + currentTime;
    const int32_t wholeFrames = static_cast<int32_t>(std::floor(static_cast<double>(currentTime * frameRate)));

    if (lastFrame < wholeFrames)
    {
        const int32_t played = lastFrame;
        lastFrame = wholeFrames;
        const int32_t advanced = wholeFrames - played;

        if (advanced != 0)
        {
            // The turret shows the body's frame.
            const uint32_t frame = static_cast<uint32_t>(currentFrame[0] + advanced);
            currentFrame[0] = static_cast<int32_t>(frame);
            const uint32_t numFrames = appearType->actorStateData[currentState].numFrames;

            if (numFrames <= frame)
            {
                currentFrame[0] = static_cast<int32_t>(frame % numFrames);
                return 0;
            }

            currentFrame[1] = static_cast<int32_t>(frame);
        }
    }

    return 1;
}

auto GVAppearance::destroy() -> void
{
    appearType->removeUsers(this);
    appearanceTypeList->removeAppearance(appearType);
}

auto GVAppearance::stateExists(GVActorState state) -> int32_t
{
    const int32_t numStates = (appearType->hasExtraState != 0 ? 1 : 0) + 3;

    if (static_cast<int32_t>(state) < numStates && state >= 0)
    {
        return static_cast<int32_t>(appearType->actorStateData[state].numFrames);
    }

    return 0;
}

auto GVAppearance::drawBars() -> void
{
    // Port: the bar is an overlay, on the screen over the view: it follows the sprite through the zoom, its size
    // doesn't change.
    const float scale = eyeScale();
    const float barWidth = scale * 38.0f;
    const float barHeight = scale * 4.0f;
    const float barY = (MCOverlayY(upperLeft.y) - scale * 6.0f) - barHeight;
    const float barX = static_cast<float>(std::floor(static_cast<double>(MCOverlayX(screenPos.x) - barWidth * 0.5f)));

    // How much of the unit is left, per class.
    GameObject* obj = owner;
    float health = 0.0f; // Port fix: the original leaves this unset for other classes.
    const int32_t objectClass = obj->objectClass;

    if (objectClass == BATTLEMECH || objectClass == GROUNDVEHICLE || objectClass == ELEMENTAL || objectClass == MOVER)
    {
        health = static_cast<Mover*>(obj)->getTotalEffectiveness();
        auto* vehicle = static_cast<GroundVehicle*>(obj);

        if (obj->objectClass == GROUNDVEHICLE && vehicle->refitter != 0)
        {
            // A refit vehicle shows the refit points left against the turret's full armor.
            const float capacity = vehicle->refitter == 0
                                       ? 0.0f
                                       : static_cast<float>(vehicle->armor[GROUNDVEHICLE_LOCATION_TURRET].maxArmor);
            health = static_cast<float>(vehicle->getRefitPoints() / capacity * health);
        }
    }
    else if (objectClass == TURRET)
    {
        int32_t damage = static_cast<int32_t>(obj->getDamage());
        const int32_t dmgLevel = static_cast<int32_t>(static_cast<TurretType*>(obj->getObjectType())->dmgLevel);

        if (dmgLevel < damage)
        {
            damage = dmgLevel;
        }

        health = 1.0f - static_cast<float>(damage) / static_cast<float>(dmgLevel);
    }
    else if (objectClass == TRAINCAR)
    {
        int32_t damage = static_cast<int32_t>(obj->getDamage());
        const int32_t dmgLevel = static_cast<TrainCarType*>(obj->getObjectType())->damage;

        if (dmgLevel < damage)
        {
            damage = dmgLevel;
        }

        // Original behaviour (OB-054): the damage taken, not the health left.
        health = static_cast<float>(damage) / static_cast<float>(dmgLevel);
    }
    else if (objectClass == CAMERADRONE)
    {
        health = 1.0f;

        if (static_cast<CameraDrone*>(obj)->hitPoints < 1)
        {
            health = 0.0f;
        }
    }

    // Green, then yellow below half, red at a fifth.
    int32_t barColor;

    if (health < 0.5)
    {
        barColor = health <= 0.2 ? 0x103 : 0x102;
    }
    else
    {
        barColor = 0x101;
    }

    ElementList->openGroup(-50000, 1);
    PolyElementData data;
    data.numVertices = 0;
    data.textureMapOff = 0;
    data.texture = nullptr;
    data.textureWidth = 0;
    data.textureHeight = 0;
    data.fadeTable = nullptr;
    data.translate = 0;
    data.statusBar = 1;
    data.barColor = barColor;
    data.vertices[0].x = static_cast<int32_t>(barX - 1.0f);
    data.vertices[0].y = static_cast<int32_t>(barY - 1.0f);
    data.vertices[1].x = static_cast<int32_t>(barX + barWidth + 1.0f);
    data.vertices[1].y = static_cast<int32_t>(barY + barHeight + 1.0f);
    float barLength = health * barWidth;

    // A unit that isn't quite dead shows at least one pixel.
    if (barLength < 1.0 && health > 0.001)
    {
        barLength = 1.0f;
    }

    data.barPercent = static_cast<int32_t>(barLength);

    if (data.barPercent > 0)
    {
        ElementList->add(ElementPool::Make<PolygonElement>(&data, -50000));
    }
}
