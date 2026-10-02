#include "stdafx.h"
#include "sprite/armactor.h"
#include "camera/camera.h"
#include "color/color.h"
#include "engine/ceglist.h"
#include "engine/cevfx.h"
#include "lib/inifile.h"
#include "main/main.h"
#include "object/gameobj.h"
#include "object/objtype.h"
#include "sprite/bactor.h"
#include "sprite/sprtmgr.h"
#include "sprite/vfxshape.h"
#include "terrain/terrain.h"
#include "vfx/vfxfuncs.h"

//---------------------------------------------------------------------------
// ArmAppearanceType
//---------------------------------------------------------------------------

auto ArmAppearanceType::init(File* apprFile, uint32_t fileSize, uint32_t loadFlags) -> int32_t
{
    const int32_t result = loadIniFile(apprFile, fileSize);

    if (result != 0)
    {
        return result;
    }

    numPackets = spriteManager->getNumShapes(appearanceNum & 0xffffff);
    // Port fix: sized by the port's pointer size (the original: count * 4).
    shapeList = static_cast<Shape**>(
        spriteManager->mallocDataRAM(static_cast<uint32_t>(numPackets) * static_cast<uint32_t>(sizeof(Shape*))));

    if (shapeList == nullptr)
    {
        return static_cast<int32_t>(0xeada0016);
    }

    for (int32_t i = 0; i < numPackets; i++)
    {
        shapeList[i] = nullptr;
    }

    keepLoaded = static_cast<int32_t>(loadFlags);

    if (loadFlags != 0)
    {
        preloadGestures();
    }

    return 0;
}

auto ArmAppearanceType::removeShape(Shape* shape) -> void
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
        auto* appearance = static_cast<ArmAppearance*>(user->user);

        if (appearance->currentShape == shape)
        {
            appearance->currentShape = nullptr;
        }
    }
}

auto ArmAppearanceType::preloadGestures() -> void
{
    for (int32_t i = 0; i < numPackets; i++)
    {
        shapeList[i] = spriteManager->getShapeData(appearanceNum & 0xffffff, static_cast<uint32_t>(i), 0, this, 0);
    }
}

auto ArmAppearanceType::loadIniFile(File* apprFile, uint32_t fileSize) -> int32_t
{
    FitIniFile iniFile;
    int32_t result = iniFile.open(apprFile, fileSize, 50);

    if (result != 0)
    {
        return result;
    }

    actorData = static_cast<ArmActorData*>(spriteManager->mallocDataRAM(sizeof(ArmActorData)));

    if (actorData == nullptr)
    {
        return static_cast<int32_t>(0xeada0016);
    }

    if ((result = iniFile.seekBlock("State")) != 0)
    {
        return result;
    }

    if ((result = iniFile.readIdULong("NumFrames", actorData->numFrames)) != 0)
    {
        return result;
    }

    if ((result = iniFile.readIdFloat("FrameRate", actorData->frameRate)) != 0)
    {
        return result;
    }

    if ((result = iniFile.readIdULong("BasePacketNumber", actorData->basePacketNumber)) != 0)
    {
        return result;
    }

    if ((result = iniFile.readIdUChar("NumRotations", actorData->numRotations)) != 0)
    {
        return result;
    }

    if ((result = iniFile.readIdULong("Symmetrical", actorData->symmetrical)) != 0)
    {
        return result;
    }

    if (iniFile.readIdUChar("CheckForHeader", checkForHeader) != 0)
    {
        checkForHeader = 1;
    }

    iniFile.close();
    return 0;
}

auto ArmAppearanceType::getShape(int32_t rotation, int32_t, float& frameRate, int& reverse) -> Shape*
{
    const ArmActorData& data = *actorData;

    if (data.numFrames == 0)
    {
        return nullptr;
    }

    if (rotation > 180)
    {
        rotation -= 360;
    }
    else if (rotation < -180)
    {
        rotation += 360;
    }

    const bool symmetrical = data.symmetrical != 0;

    if (rotation < 0 && symmetrical)
    {
        rotation = -rotation;
        // Original behaviour (OB-053): reverse is only ever set, never cleared.
        reverse = 1;
    }
    else if (rotation < 0 && !symmetrical)
    {
        rotation += 360;
    }

    frameRate = data.frameRate;

    int32_t rotationIndex = static_cast<int16_t>(
        static_cast<int32_t>(std::floor(static_cast<double>((data.numRotations + 1) * rotation) * (1.0 / 360.0))));

    if (rotationIndex > 0x1f && rotation > 180 && !symmetrical)
    {
        rotationIndex = 0x1f;
    }

    float zoom = 1.0f;

    if (eye != nullptr && eye->cameraScale == 1)
    {
        zoom = 0.5f;
    }

    const uint32_t packet = data.basePacketNumber + static_cast<uint32_t>(rotationIndex);

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

auto ArmAppearanceType::destroy() -> void
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
    spriteManager->freeDataRAM(actorData);
    actorData = nullptr;
}

//---------------------------------------------------------------------------
// ArmAppearance
//---------------------------------------------------------------------------

auto ArmAppearance::init(AppearanceType* tree, GameObject* obj) -> int32_t
{
    unknown04 = 0x70000000;
    visible = 0;
    owner = obj;
    appearType = static_cast<ArmAppearanceType*>(tree);

    if (tree != nullptr)
    {
        tree->addUsers(this);
    }

    currentShape = nullptr;
    currentFrame = -1;
    fadeTableIndex = -1;
    unknown48 = 0;
    shapeMinY = -15.0f;
    shapeMinX = -15.0f;
    visible = 0;
    unknown50 = 0;
    currentTime = 0.0f;
    lastFrame = 0;
    reverse = 0;
    inView = 0;
    rotation = 0.0f;
    shapeMaxY = 15.0f;
    shapeMaxX = 15.0f;
    return 0;
}

auto ArmAppearance::recalcBounds(Camera* cam) -> int
{
    if (cam == nullptr)
    {
        return 0;
    }

    const vector_2d pos = owner->getScreenPos(cam->cameraId - 1);
    upperLeft.x = pos.x;
    upperLeft.y = pos.y;
    lowerRight.y = pos.y;
    lowerRight.x = pos.x;

    if (currentShape != nullptr && currentShape->frameList != nullptr)
    {
        // The bounds only ever grow: they cover every frame drawn so far.
        uint8_t* shapeTable = currentShape->frameList;
        int32_t frame = currentFrame;

        if (frame < 0)
        {
            frame = 0;
        }

        const int32_t numShapeFrames = VFX_shape_count(shapeTable);

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

    // Faithful: the zoom is the eye's, the screen limits the camera's.
    const float scale = eye->cameraScale == 1 ? 0.5f : 1.0f;
    upperLeft.x = scale * shapeMinX + pos.x;
    upperLeft.y = scale * shapeMinY + pos.y;
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

auto ArmAppearance::render(int32_t depthFixup) -> int32_t
{
    if (owner->selected != 0)
    {
        recalcBounds(eye);
        drawSelectBox(0xfd);
    }

    GameObject* obj = owner;
    screenPos = obj->getScreenPos(eye->cameraId - 1);

    // The facing in degrees, negative to the right.
    const frame_of_ref frame = obj->getFrame();
    float cosFacing = UnitX.y * frame.i.y + UnitX.x * frame.i.x + UnitX.z * frame.i.z;

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

    rotation = static_cast<float>(facing);
    currentShape = appearType->getShape(static_cast<int32_t>(rotation), 0, frameRate, reverse);

    if (drawTerrainGrid != 0)
    {
        drawSelectBox(0xfd);
    }

    uint8_t* fadeTable = nullptr;

    if (fadeTableIndex != -1 && fadeTableIndex >= 0)
    {
        fadeTable = gamePalette->fadePalettes + (fadeTableIndex + gamePalette->numBitmapHazeLevels * 2) * 0x100;
    }

    ElementList->openGroup(static_cast<int32_t>(static_cast<float>(depthFixup) - screenPos.y), 1);

    if (currentShape == nullptr || currentShape->frameList == nullptr)
    {
        return 0;
    }

    auto* element =
        new VFXElement(currentShape->frameList, screenPos.x, screenPos.y, currentFrame, reverse, fadeTable, 0, 0);

    // Port fix: the original writes the debug names through a null element too, and "%i" of a type number
    // can overrun name2.
    if (element != nullptr)
    {
        strcpy(element->name, "armweap");

        if (ownerObject == nullptr)
        {
            strcpy(element->name2, "unknown");
        }
        else
        {
            snprintf(element->name2, sizeof(element->name2), "%i", ownerObject->getObjectType()->objTypeNum);
        }
    }

    ElementList->add(element);
    return 0;
}

auto ArmAppearance::update() -> int32_t
{
    if (currentFrame == -1)
    {
        currentFrame = 0;
    }

    const int32_t played = lastFrame;
    const ArmActorData& data = *appearType->actorData;
    frameRate = data.frameRate;
    currentTime = frameLength + currentTime;
    const int32_t wholeFrames = static_cast<int32_t>(std::floor(static_cast<double>(currentTime * frameRate)));

    if (played < wholeFrames)
    {
        lastFrame = wholeFrames;
        const int32_t advanced = wholeFrames - played;

        if (advanced != 0)
        {
            currentFrame += advanced;

            if (data.numFrames <= static_cast<uint32_t>(currentFrame))
            {
                currentFrame = static_cast<int32_t>(data.numFrames - 1);
                return 0;
            }
        }
    }

    return 1;
}

auto ArmAppearance::destroy() -> void
{
    appearType->removeUsers(this);
    appearanceTypeList->removeAppearance(appearType);
}
