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
    auto WrapFacing(const MCElementalGestureData& data, float& rotation) -> float
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

        float frameRate = data.FrameRate;

        if (frameRate < 0.0)
        {
            frameRate = -frameRate;
        }

        return frameRate;
    }
}

auto MCElementalTree::Init(MCFile* apprFile, uint32_t fileSize, uint32_t loadFlags) -> int32_t
{
    const int32_t result = LoadIniFile(apprFile, fileSize);

    if (result != 0)
    {
        return result;
    }

    KeepLoaded = static_cast<int32_t>(loadFlags);
    NumPackets = static_cast<uint32_t>(SpriteManager->GetNumShapes(AppearanceNum & 0xffffff));
    // Port fix: sized by the port's pointer size (the original: count * 4).
    ShapeList =
        static_cast<MCShape**>(SpriteManager->MallocDataRam(NumPackets * static_cast<uint32_t>(sizeof(MCShape*))));

    if (ShapeList == nullptr)
    {
        return static_cast<int32_t>(0xbeef000a);
    }

    for (uint32_t i = 0; i < NumPackets; i++)
    {
        ShapeList[i] = nullptr;
    }

    return 0;
}

auto MCElementalTree::RemoveShape(MCShape* shape) -> void
{
    for (int32_t i = 0; i < static_cast<int32_t>(NumPackets); i++)
    {
        if (ShapeList[i] == shape)
        {
            ShapeList[i] = nullptr;
            // Port fix: the original returns here, leaving the users holding the freed shape.
            break;
        }
    }

    // The users are elemental appearances, whose shape is at +0x3c.
    for (MCAppearanceUser* user = UserList; user != nullptr; user = user->Next)
    {
        auto** userShape = reinterpret_cast<MCShape**>(static_cast<uint8_t*>(user->User) + 0x3c);

        if (*userShape == shape)
        {
            *userShape = nullptr;
        }
    }
}

auto MCElementalTree::PreloadGestures(int32_t, float) -> void
{
}

auto MCElementalTree::LoadIniFile(MCFile* apprFile, uint32_t fileSize) -> int32_t
{
    MCFitIniFile iniFile;
    int32_t result = iniFile.Open(apprFile, fileSize, 50);

    if (result != 0)
    {
        return result;
    }

    JumpMaxDistance = static_cast<float*>(SpriteManager->MallocDataRam(sizeof(float)));

    if (JumpMaxDistance == nullptr)
    {
        return static_cast<int32_t>(0xeadd0009);
    }

    if ((result = iniFile.SeekBlock("SpecialInfo")) != 0)
    {
        return result;
    }

    if ((result = iniFile.ReadIdFloat("jumpMaxDistance", *JumpMaxDistance)) != 0)
    {
        return result;
    }

    if ((result = iniFile.SeekBlock("Gestures")) != 0)
    {
        return result;
    }

    if ((result = iniFile.ReadIdULong("NumGestures", NumGestures)) != 0)
    {
        return result;
    }

    const int32_t count = static_cast<int32_t>(NumGestures);
    Gestures = static_cast<MCElementalGestureData*>(
        SpriteManager->MallocDataRam(NumGestures * sizeof(MCElementalGestureData)));

    if (Gestures == nullptr)
    {
        return static_cast<int32_t>(0xbeef000a);
    }

    for (int32_t i = 0; i < count; i++)
    {
        char blockName[20];
        sprintf(blockName, "Gestures%d", i);

        if ((result = iniFile.SeekBlock(blockName)) != 0)
        {
            return result;
        }

        MCElementalGestureData& data = Gestures[i];

        if ((result = iniFile.ReadIdUChar("State", data.State)) != 0)
        {
            return result;
        }

        uint32_t numFrames = 0;

        if ((result = iniFile.ReadIdULong("NumFrames", numFrames)) != 0)
        {
            return result;
        }

        data.NumFrames = numFrames;
        float frameRate = 0.0f;

        if ((result = iniFile.ReadIdFloat("FrameRate", frameRate)) != 0)
        {
            return result;
        }

        data.FrameRate = frameRate;
        uint32_t basePacketNumber = 0;

        if ((result = iniFile.ReadIdULong("BasePacketNumber", basePacketNumber)) != 0)
        {
            return result;
        }

        data.BasePacketNumber = basePacketNumber;

        if ((result = iniFile.ReadIdUChar("NumRotations", data.NumRotations)) != 0)
        {
            return result;
        }

        float velocity = 0.0f;

        if ((result = iniFile.ReadIdFloat("Velocity", velocity)) != 0)
        {
            return result;
        }

        data.Velocity = velocity;
    }

    iniFile.Close();
    return 0;
}

auto MCElementalTree::SetGesture(int32_t gesture, float rotation, float& frameRate) -> void
{
    frameRate = 0.0f;

    if (gesture < 0 || gesture >= static_cast<int32_t>(NumGestures) || Gestures[gesture].NumFrames == 0)
    {
        return;
    }

    // The original also works out the rotation index here, then drops it.
    frameRate = WrapFacing(Gestures[gesture], rotation);
}

auto MCElementalTree::GetGesture(int32_t gesture, float rotation, float& frameRate, int) -> MCShape*
{
    frameRate = 0.0f;

    if (gesture < 0 || gesture >= static_cast<int32_t>(NumGestures) || Gestures[gesture].NumFrames == 0)
    {
        return nullptr;
    }

    const MCElementalGestureData& data = Gestures[gesture];
    frameRate = WrapFacing(data, rotation);

    const int32_t numRotations = data.NumRotations;
    int32_t rotationIndex = static_cast<int16_t>(static_cast<int32_t>(
        std::floor(static_cast<double>(rotation * static_cast<float>(numRotations + 1)) * (1.0 / 360.0))));

    if (numRotations <= rotationIndex)
    {
        rotationIndex = numRotations - 1;
    }

    float zoom = 1.0f;

    if (Eye != nullptr && Eye->CameraScale == 1)
    {
        zoom = 0.5f;
    }

    // Full size and zoomed out packets alternate.
    uint32_t packet = data.BasePacketNumber + static_cast<uint32_t>(rotationIndex * 2);

    if (zoom != 1.0f)
    {
        packet++;
    }

    if (packet >= NumPackets)
    {
        return nullptr;
    }

    MCShape* shape = ShapeList[packet];

    if (shape != nullptr)
    {
        shape->LastTurnUsed = Turn;
        return shape;
    }

    DynamicFrameTiming = 0;
    shape = SpriteManager->GetShapeData(AppearanceNum & 0xffffff, packet, Turn, this, zoom != 1.0f ? 1 : 0);
    ShapeList[packet] = shape;
    return shape;
}

auto MCElementalTree::Destroy() -> void
{
    // The shapes stay in the sprite manager's cache, ownerless.
    for (int32_t i = 0; i < static_cast<int32_t>(NumPackets); i++)
    {
        if (ShapeList[i] != nullptr)
        {
            ShapeList[i]->Owner = nullptr;
        }
    }

    SpriteManager->FreeDataRam(ShapeList);
    ShapeList = nullptr;
    SpriteManager->FreeDataRam(JumpMaxDistance);
    JumpMaxDistance = nullptr;
    SpriteManager->FreeDataRam(Gestures);
    Gestures = nullptr;
}
