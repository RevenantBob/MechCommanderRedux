#include "stdafx.h"
#include "sprite/puactor.h"
#include "camera/camera.h"
#include "color/color.h"
#include "engine/ceglist.h"
#include "engine/cepoly.h"
#include "engine/cevfx.h"
#include "lib/inifile.h"
#include "main/main.h"
#include "object/gate.h"
#include "object/gvehicl.h"
#include "object/team.h"
#include "object/turret.h"
#include "sprite/bactor.h"
#include "sprite/sprtmgr.h"
#include "sprite/vfxshape.h"
#include "terrain/terrain.h"
#include "vfx/vfxfuncs.h"

namespace
{
    constexpr int32_t NO_DATA_RAM = static_cast<int32_t>(0xeada000c);

    /// <summary>0.5 when the camera is zoomed out, else 1.</summary>
    auto zoomScale(const Camera* cam) -> float
    {
        return cam->cameraScale == 1 ? 0.5f : 1.0f;
    }

    /// <summary>The pixel offset of a turret or gate (both at +0x8c/+0x90); false for other classes.</summary>
    auto pixelOffset(GameObject* obj, int32_t& offsetX, int32_t& offsetY) -> bool
    {
        if (obj->objectClass == TURRET)
        {
            offsetX = static_cast<Turret*>(obj)->tileOffsetX;
            offsetY = static_cast<Turret*>(obj)->tileOffsetY;
            return true;
        }

        if (obj->objectClass == GATE)
        {
            offsetX = static_cast<Gate*>(obj)->pixelOffsetX;
            offsetY = static_cast<Gate*>(obj)->pixelOffsetY;
            return true;
        }

        return false;
    }
}

//---------------------------------------------------------------------------
// PUAppearanceType
//---------------------------------------------------------------------------

auto PUAppearanceType::init(File* apprFile, uint32_t fileSize, uint32_t loadFlags) -> int32_t
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

auto PUAppearanceType::removeShape(Shape* shape) -> void
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
        auto* appearance = static_cast<PUAppearance*>(user->user);

        if (appearance->currentShape == shape)
        {
            appearance->currentShape = nullptr;
        }
    }
}

auto PUAppearanceType::loadIniFile(File* apprFile, uint32_t fileSize) -> int32_t
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

    actorStateData = static_cast<PUActorData*>(spriteManager->mallocDataRAM(MAX_PU_ACTOR_STATES * sizeof(PUActorData)));

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

    if (numStates != MAX_PU_ACTOR_STATES)
    {
        return static_cast<int32_t>(0xeada000e);
    }

    if (iniFile.readIdUChar("Scaled", scaled) != 0)
    {
        scaled = 0;
    }

    for (int32_t i = 0; i < MAX_PU_ACTOR_STATES; i++)
    {
        char blockName[20];
        sprintf(blockName, "State%d", i);

        if ((result = iniFile.seekBlock(blockName)) != 0)
        {
            return result;
        }

        uint8_t state = 0;

        if ((result = iniFile.readIdUChar("State", state)) != 0)
        {
            return result;
        }

        PUActorData& data = actorStateData[i];
        data.state = static_cast<PUActorState>(state);

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

auto PUAppearanceType::getShape(PUActorState state, int32_t rotation, int32_t part, float& frameRate) -> Shape*
{
    const PUActorData& data = actorStateData[state];

    if (data.numFrames == 0)
    {
        return nullptr;
    }

    if (rotation < 0)
    {
        rotation += 360;
    }

    const uint32_t numRotations = data.numRotations;
    frameRate = data.frameRate;
    const int32_t rotationIndex = static_cast<int16_t>(static_cast<int32_t>(std::floor(
        static_cast<double>(static_cast<int32_t>(numRotations * static_cast<uint32_t>(rotation))) * (1.0 / 360.0))));
    uint32_t basePacket = data.basePacketNumber;

    if (part > 0)
    {
        basePacket += numRotations * static_cast<uint32_t>(part);
    }

    uint32_t packet = basePacket + static_cast<uint32_t>(rotationIndex);
    float zoom = 1.0f;

    if (eye != nullptr && eye->cameraScale == 1)
    {
        zoom = 0.5f;
    }

    // Scaled types keep their zoomed out rotations after the full size ones.
    if (zoom != 1.0f && scaled != 0)
    {
        packet += numRotations;
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

auto PUAppearanceType::destroy() -> void
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
// PUAppearance
//---------------------------------------------------------------------------

auto PUAppearance::init(AppearanceType* tree, GameObject* obj) -> int32_t
{
    unknown04 = 0x70000000;
    visible = 0;
    owner = obj;
    appearType = static_cast<PUAppearanceType*>(tree);

    if (tree != nullptr)
    {
        tree->addUsers(this);
    }

    currentShape = nullptr;
    shapeMinY = -25.0f;
    shapeMinX = -25.0f;
    unknown44 = 0;
    visible = 0;
    unknown48 = 0;
    currentTime = 0.0f;
    lastFrame = 0;
    currentState = PU_ACTOR_STATE_CLOSED;
    inView = 0;
    rotation = 0.0f;
    currentFrame = -1;
    shapeMaxY = 50.0f;
    shapeMaxX = 50.0f;
    frameRate = 15.0f;
    return 0;
}

auto PUAppearance::recalcBounds(Camera* cam) -> int
{
    if (cam == nullptr)
    {
        return 0;
    }

    GameObject* obj = owner;
    const vector_2d pos = obj->getScreenPos(cam->cameraId - 1);
    float x = pos.x;
    float y = pos.y;
    int32_t offsetX = 0;
    int32_t offsetY = 0;

    if (pixelOffset(obj, offsetX, offsetY))
    {
        const float camScale = zoomScale(cam);
        x = static_cast<float>(offsetX) * camScale + x;
        y = static_cast<float>(offsetY) * camScale + y;
    }

    upperLeft.x = x;
    upperLeft.y = y;
    lowerRight.y = y;
    lowerRight.x = x;

    // The shape's bounds are taken once, from the first shape seen.
    if (currentShape != nullptr && currentShape->frameList != nullptr && inView == 0)
    {
        uint8_t* shapeTable = currentShape->frameList;
        int32_t frame = currentFrame;

        if (frame < 0)
        {
            frame = 0;
        }

        const int32_t numShapeFrames = VFX_shape_count(shapeTable);

        if (numShapeFrames <= frame)
        {
            currentFrame = numShapeFrames - 1;
            // Port fix: the original clamps only the object's frame and measures past the shape table.
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

    const float scale = zoomScale(eye);
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

    return 0;
}

auto PUAppearance::render(int32_t depthFixup) -> int32_t
{
    GameObject* obj = owner;
    screenPos = obj->getScreenPos(eye->cameraId - 1);
    const float scale = zoomScale(eye);
    int32_t offsetX = 0;
    int32_t offsetY = 0;

    if (pixelOffset(obj, offsetX, offsetY))
    {
        screenPos.x = static_cast<float>(offsetX) * scale + screenPos.x;
        screenPos.y = static_cast<float>(offsetY) * scale + screenPos.y;
    }

    if (obj->objectClass == GROUNDVEHICLE)
    {
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

        rotation = static_cast<float>(facing + static_cast<GroundVehicle*>(obj)->turretRotation);
    }
    else if (obj->objectClass == TURRET)
    {
        rotation = static_cast<Turret*>(obj)->turretRotation;
    }

    if (rotation < 0.0)
    {
        rotation = static_cast<float>(rotation + 360.0);
    }

    // Snap to the nearest drawn rotation.
    const PUActorState state = currentState;
    const int32_t numRotations = appearType->actorStateData[state].numRotations + 1;
    const int32_t rotationIndex = static_cast<int16_t>(static_cast<int32_t>(
        std::floor(static_cast<double>(static_cast<float>(numRotations) * rotation) * (1.0 / 360.0))));
    rotation = static_cast<float>(360.0 / numRotations) * static_cast<float>(rotationIndex);
    currentShape = appearType->getShape(state, static_cast<int32_t>(rotation), 0, frameRate);

    ElementList->openGroup(static_cast<int32_t>(static_cast<float>(depthFixup) - screenPos.y), 0);

    if (drawTerrainGrid != 0)
    {
        drawSelectBox(0xfd);
    }

    if (currentShape != nullptr && currentShape->frameList != nullptr)
    {
        uint8_t* fadeTable = nullptr;

        if (fadeTableIndex != -1 && fadeTableIndex >= 0)
        {
            fadeTable = gamePalette->fadePalettes + (fadeTableIndex + gamePalette->numBitmapHazeLevels * 2) * 0x100;
        }

        ElementList->add(
            new VFXElement(currentShape->frameList, screenPos.x, screenPos.y, currentFrame, 0, fadeTable, 1, 0));
    }

    const int32_t selected = owner->selected;
    bool showBars = selected == -1 || selected == 1;

    if (!showBars)
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

        showBars = owner->getNumAttackers() >= 1;
    }

    if (showBars)
    {
        recalcBounds(eye);
        drawBars();
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

auto PUAppearance::setDestroyed() -> void
{
    // Destroyed closed from closed, destroyed open from any other state.
    currentState =
        currentState != PU_ACTOR_STATE_CLOSED ? PU_ACTOR_STATE_DESTROYED_OPEN : PU_ACTOR_STATE_DESTROYED_CLOSED;
}

auto PUAppearance::setCombatMode(int combatMode) -> int32_t
{
    const PUActorData* states = appearType->actorStateData;
    const int32_t lastOpeningFrame = static_cast<int32_t>(states[PU_ACTOR_STATE_OPENING].numFrames) - 1;
    const int32_t lastClosingFrame = static_cast<int32_t>(states[PU_ACTOR_STATE_CLOSING].numFrames) - 1;

    // Finish an opening or closing that has played out.
    if (currentState == PU_ACTOR_STATE_OPENING || currentState == PU_ACTOR_STATE_CLOSING)
    {
        if (currentState == PU_ACTOR_STATE_CLOSING && currentFrame == lastClosingFrame)
        {
            currentState = PU_ACTOR_STATE_CLOSED;
            currentFrame = -1;
        }

        if (currentState == PU_ACTOR_STATE_OPENING && currentFrame == lastOpeningFrame)
        {
            currentState = PU_ACTOR_STATE_OPEN;
            currentFrame = -1;
        }
    }

    if (combatMode == 0)
    {
        if (currentState == PU_ACTOR_STATE_OPEN)
        {
            currentState = PU_ACTOR_STATE_CLOSING;
        }

        if (currentState == PU_ACTOR_STATE_CLOSING && currentFrame == lastClosingFrame)
        {
            currentState = PU_ACTOR_STATE_CLOSED;
            currentFrame = -1;
        }
    }
    else
    {
        if (currentState == PU_ACTOR_STATE_CLOSED)
        {
            currentState = PU_ACTOR_STATE_OPENING;
        }

        if (currentState == PU_ACTOR_STATE_OPENING && currentFrame == lastOpeningFrame)
        {
            currentState = PU_ACTOR_STATE_OPEN;
            currentFrame = -1;
        }

        if (currentState == PU_ACTOR_STATE_CLOSING)
        {
            // Reopen from the matching point of the opening.
            const double closedShare =
                static_cast<double>(static_cast<uint32_t>(currentFrame)) / static_cast<double>(lastClosingFrame);
            currentFrame = static_cast<int32_t>((1.0 - closedShare) *
                                                static_cast<double>(states[PU_ACTOR_STATE_OPENING].numFrames));
            currentState = PU_ACTOR_STATE_OPENING;
            return 1;
        }
    }

    return currentState;
}

auto PUAppearance::update() -> int32_t
{
    if (currentFrame == -1)
    {
        currentFrame = 0;
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
            const uint32_t frame = static_cast<uint32_t>(currentFrame + advanced);
            currentFrame = static_cast<int32_t>(frame);
            const uint32_t numFrames = appearType->actorStateData[currentState].numFrames;

            if (numFrames <= frame)
            {
                currentFrame = static_cast<int32_t>(numFrames - 1);
                return 0;
            }
        }
    }

    return 1;
}

auto PUAppearance::destroy() -> void
{
    appearType->removeUsers(this);
    appearanceTypeList->removeAppearance(appearType);
}

auto PUAppearance::stateExists(PUActorState state) -> int32_t
{
    if (static_cast<int32_t>(state) < MAX_PU_ACTOR_STATES && state >= 0)
    {
        return static_cast<int32_t>(appearType->actorStateData[state].numFrames);
    }

    return 0;
}

auto PUAppearance::drawBars() -> void
{
    const float scale = zoomScale(eye);
    const float barWidth = scale * 38.0f;
    const float barHeight = scale * 4.0f;
    const float barY = (upperLeft.y - scale * 7.0f) - barHeight;
    const float barX = static_cast<float>(std::floor(static_cast<double>(screenPos.x - barWidth * 0.5f)));

    // A turret shows its bar only with its weapon deployed.
    GameObject* obj = owner;

    if (obj->objectClass == TURRET && static_cast<Turret*>(obj)->weaponDeployed == 0)
    {
        return;
    }

    double health = 0.0; // Port fix: the original leaves this unset for other classes.
    const int32_t objectClass = obj->objectClass;

    if (objectClass == BATTLEMECH || objectClass == GROUNDVEHICLE || objectClass == ELEMENTAL || objectClass == MOVER)
    {
        auto* mover = static_cast<Mover*>(obj);

        if (mover == nullptr)
        {
            return;
        }

        if (mover->weaponEffectiveness < 0.0f || mover->maxWeaponEffectiveness < mover->weaponEffectiveness)
        {
            return;
        }

        health = mover->getTotalEffectiveness();
    }
    else if (objectClass == TURRET || objectClass == GATE)
    {
        int32_t damage = static_cast<int32_t>(obj->getDamage());
        // Turret and gate types both keep their damage level at +0x30.
        const int32_t dmgLevel = static_cast<int32_t>(static_cast<TurretType*>(obj->getObjectType())->dmgLevel);

        if (dmgLevel < damage)
        {
            damage = dmgLevel;
        }

        health = 1.0 - static_cast<double>(damage) / static_cast<double>(dmgLevel);
    }

    // Green, then yellow below half, red at a fifth (palette colours here).
    int32_t barColor;

    if (health < 0.5)
    {
        barColor = health <= 0.2 ? 0xef : 0xf2;
    }
    else
    {
        barColor = 0x0b;
    }

    float barLength = static_cast<float>(health * barWidth);

    // A unit that isn't quite dead shows at least one pixel.
    if (health * barWidth < 1.0 && health > 0.001)
    {
        barLength = 1.0f;
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
    data.barColor = barColor;
    data.vertices[0].x = static_cast<int32_t>(barX - 1.0f);
    data.vertices[0].y = static_cast<int32_t>(barY - 1.0f);
    data.vertices[1].x = static_cast<int32_t>(barX + barWidth + 1.0f);
    data.vertices[1].y = static_cast<int32_t>(barY + barHeight + 1.0f);
    data.barPercent = static_cast<int32_t>(barLength);

    if (data.barPercent > 0)
    {
        ElementList->add(new PolygonElement(&data, -50000));
    }
}
