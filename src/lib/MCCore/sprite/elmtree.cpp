#include "stdafx.h"
#include "sprite/elmtree.h"
#include "camera/camera.h"
#include "lib/inifile.h"
#include "main/main.h"
#include "sprite/bactor.h"
#include "sprite/sprtmgr.h"
#include "sprite/vfxshape.h"

namespace
{
    /// <summary>
    /// Wraps <paramref name="rotation"/> into 0..360 and returns the gesture's frame rate, made positive.
    /// </summary>
    auto wrapFacing(const ElementalGestureData& data, float& rotation) -> float
    {
        if (rotation > 180.0)
        {
            rotation = (rotation - 180.0f) - 180.0f;
        }
        else if (rotation < -180.0)
        {
            rotation = rotation + 180.0f + 180.0f;
        }

        if (rotation < 0.0)
        {
            rotation = static_cast<float>(rotation + 360.0);
        }

        float frameRate = data.frameRate;

        if (frameRate < 0.0)
        {
            frameRate = -frameRate;
        }

        return frameRate;
    }
}

auto ElementalTree::init(File* apprFile, uint32_t fileSize, uint32_t loadFlags) -> int32_t
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
        return static_cast<int32_t>(0xbeef000a);
    }

    for (uint32_t i = 0; i < numPackets; i++)
    {
        shapeList[i] = nullptr;
    }

    return 0;
}

auto ElementalTree::removeShape(Shape* shape) -> void
{
    for (int32_t i = 0; i < static_cast<int32_t>(numPackets); i++)
    {
        if (shapeList[i] == shape)
        {
            shapeList[i] = nullptr;
            // Port fix: the original returns here, leaving the users holding the freed shape.
            break;
        }
    }

    // The users are elemental appearances, whose shape is at +0x3c.
    for (AppearanceUser* user = userList; user != nullptr; user = user->next)
    {
        auto** userShape = reinterpret_cast<Shape**>(static_cast<uint8_t*>(user->user) + 0x3c);

        if (*userShape == shape)
        {
            *userShape = nullptr;
        }
    }
}

auto ElementalTree::preloadGestures(int32_t, float) -> void
{
}

auto ElementalTree::loadIniFile(File* apprFile, uint32_t fileSize) -> int32_t
{
    FitIniFile iniFile;
    int32_t result = iniFile.open(apprFile, fileSize, 50);

    if (result != 0)
    {
        return result;
    }

    jumpMaxDistance = static_cast<float*>(spriteManager->mallocDataRAM(sizeof(float)));

    if (jumpMaxDistance == nullptr)
    {
        return static_cast<int32_t>(0xeadd0009);
    }

    if ((result = iniFile.seekBlock("SpecialInfo")) != 0)
    {
        return result;
    }

    if ((result = iniFile.readIdFloat("jumpMaxDistance", *jumpMaxDistance)) != 0)
    {
        return result;
    }

    if ((result = iniFile.seekBlock("Gestures")) != 0)
    {
        return result;
    }

    if ((result = iniFile.readIdULong("NumGestures", numGestures)) != 0)
    {
        return result;
    }

    const int32_t count = static_cast<int32_t>(numGestures);
    gestures =
        static_cast<ElementalGestureData*>(spriteManager->mallocDataRAM(numGestures * sizeof(ElementalGestureData)));

    if (gestures == nullptr)
    {
        return static_cast<int32_t>(0xbeef000a);
    }

    for (int32_t i = 0; i < count; i++)
    {
        char blockName[20];
        sprintf(blockName, "Gestures%d", i);

        if ((result = iniFile.seekBlock(blockName)) != 0)
        {
            return result;
        }

        ElementalGestureData& data = gestures[i];

        if ((result = iniFile.readIdUChar("State", data.state)) != 0)
        {
            return result;
        }

        uint32_t numFrames = 0;

        if ((result = iniFile.readIdULong("NumFrames", numFrames)) != 0)
        {
            return result;
        }

        data.numFrames = numFrames;
        float frameRate = 0.0f;

        if ((result = iniFile.readIdFloat("FrameRate", frameRate)) != 0)
        {
            return result;
        }

        data.frameRate = frameRate;
        uint32_t basePacketNumber = 0;

        if ((result = iniFile.readIdULong("BasePacketNumber", basePacketNumber)) != 0)
        {
            return result;
        }

        data.basePacketNumber = basePacketNumber;

        if ((result = iniFile.readIdUChar("NumRotations", data.numRotations)) != 0)
        {
            return result;
        }

        float velocity = 0.0f;

        if ((result = iniFile.readIdFloat("Velocity", velocity)) != 0)
        {
            return result;
        }

        data.velocity = velocity;
    }

    iniFile.close();
    return 0;
}

auto ElementalTree::setGesture(int32_t gesture, float rotation, float& frameRate) -> void
{
    frameRate = 0.0f;

    if (gesture < 0 || gesture >= static_cast<int32_t>(numGestures) || gestures[gesture].numFrames == 0)
    {
        return;
    }

    // The original also works out the rotation index here, then drops it.
    frameRate = wrapFacing(gestures[gesture], rotation);
}

auto ElementalTree::getGesture(int32_t gesture, float rotation, float& frameRate, int) -> Shape*
{
    frameRate = 0.0f;

    if (gesture < 0 || gesture >= static_cast<int32_t>(numGestures) || gestures[gesture].numFrames == 0)
    {
        return nullptr;
    }

    const ElementalGestureData& data = gestures[gesture];
    frameRate = wrapFacing(data, rotation);

    const int32_t numRotations = data.numRotations;
    int32_t rotationIndex = static_cast<int16_t>(static_cast<int32_t>(
        std::floor(static_cast<double>(rotation * static_cast<float>(numRotations + 1)) * (1.0 / 360.0))));

    if (numRotations <= rotationIndex)
    {
        rotationIndex = numRotations - 1;
    }

    float zoom = 1.0f;

    if (eye != nullptr && eye->cameraScale == 1)
    {
        zoom = 0.5f;
    }

    // Full size and zoomed out packets alternate.
    uint32_t packet = data.basePacketNumber + static_cast<uint32_t>(rotationIndex * 2);

    if (zoom != 1.0f)
    {
        packet++;
    }

    if (packet >= numPackets)
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

auto ElementalTree::destroy() -> void
{
    // The shapes stay in the sprite manager's cache, ownerless.
    for (int32_t i = 0; i < static_cast<int32_t>(numPackets); i++)
    {
        if (shapeList[i] != nullptr)
        {
            shapeList[i]->owner = nullptr;
        }
    }

    spriteManager->freeDataRAM(shapeList);
    shapeList = nullptr;
    spriteManager->freeDataRAM(jumpMaxDistance);
    jumpMaxDistance = nullptr;
    spriteManager->freeDataRAM(gestures);
    gestures = nullptr;
}
