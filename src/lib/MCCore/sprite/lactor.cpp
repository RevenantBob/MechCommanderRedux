#include "stdafx.h"
#include "sprite/lactor.h"
#include "camera/camera.h"
#include "color/color.h"
#include "engine/ceglist.h"
#include "engine/cevfx.h"
#include "main/main.h"
#include "object/gameobj.h"
#include "object/team.h"
#include "sprite/vfxshape.h"
#include "terrain/terrain.h"
#include "vfx/vfxfuncs.h"

namespace
{
    /// <summary>The jump gesture.</summary>
    constexpr int32_t GESTURE_JUMP = 2;

    /// <summary>The facing of <paramref name="obj"/> in degrees, negative to the right.</summary>
    auto objectFacing(GameObject* obj) -> float
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

        double facing = acosMatherr(static_cast<double>(cosFacing)) * 0x1.ca5dc1a6402aap+5;

        if (frame.i.y < 0.0)
        {
            facing = -facing;
        }

        return static_cast<float>(facing);
    }
}

auto ElementalActor::getNumFramesInGesture(uint32_t gesture) -> float
{
    // Port fix: the original tests numGestures < gesture, so gesture == numGestures reads past the table.
    if (appearType->numGestures <= gesture)
    {
        return 0.0f;
    }

    return static_cast<float>(appearType->gestures[gesture].numFrames);
}

auto ElementalActor::getVelocityOfGesture(uint32_t gesture) -> float
{
    // Port fix: as in getNumFramesInGesture.
    if (appearType->numGestures <= gesture)
    {
        return 0.0f;
    }

    return appearType->gestures[gesture].velocity;
}

auto ElementalActor::preloadGestures(int32_t gesture, float preloadRotation) -> void
{
    appearType->preloadGestures(gesture, preloadRotation);
}

auto ElementalActor::setGestureGoal(int32_t goal) -> int32_t
{
    if (goalPending != 0)
    {
        return static_cast<int32_t>(0xeadd0005);
    }

    if (goal == GESTURE_JUMP)
    {
        if (jumpSetup == 0)
        {
            return static_cast<int32_t>(0xeadd0006);
        }
    }
    else if (goal < 0)
    {
        return static_cast<int32_t>(0xeadd0003);
    }

    if (static_cast<int32_t>(appearType->numGestures) < goal)
    {
        return static_cast<int32_t>(0xeadd0003);
    }

    gestureGoal = goal;
    goalPending = 1;
    return 0;
}

auto ElementalActor::init(AppearanceType* tree, GameObject* obj) -> int32_t
{
    unknown04 = 0x70000000;
    visible = 0;
    owner = obj;
    appearType = static_cast<ElementalTree*>(tree);

    if (tree != nullptr)
    {
        tree->addUsers(this);
    }

    visible = 0;
    fadeTableIndex = -1;
    currentFrame = -1;
    unknown90[3] = 0;
    shapeMinY = -15.0f;
    shapeMinX = -15.0f;
    currentShape = nullptr;
    unknown44 = 0;
    currentTime = 0.0f;
    lastFrame = 0;
    velocity = 0.0f;
    unknown54 = 0;
    goalPending = 0;
    currentGesture = 0;
    oldGesture = 0;
    jumping = 0;
    jumpSetup = 0;
    unknown74[3] = 0;
    unknown74[2] = 0;
    unknown74[1] = 0;
    unknown74[0] = 0;
    unknown90[0] = 0;
    unknown90[1] = 0;
    unknown90[2] = 0;
    inView = 0;
    frameRate = 15.0f;
    velocityPercentage = 1.0f;
    unknown8C = 1;
    shapeMaxY = 30.0f;
    shapeMaxX = 30.0f;
    return 0;
}

auto ElementalActor::setJumpParameters(float jumpDistance) -> int32_t
{
    if (jumping != 0)
    {
        return static_cast<int32_t>(0xeadd0007);
    }

    gestureGoal = GESTURE_JUMP;
    jumpSetup = 1;
    // Cover the distance in the jump gesture's time.
    const ElementalGestureData& jump = appearType->gestures[GESTURE_JUMP];
    velocity = jumpDistance / (static_cast<float>(jump.numFrames) / jump.frameRate);
    return 0;
}

auto ElementalActor::getVelocityMagnitude() -> float
{
    if (jumping == 0 && jumpSetup == 0)
    {
        velocity = appearType->gestures[currentGesture].velocity;
    }

    return velocity;
}

auto ElementalActor::setVelocityPercentage(float percent) -> void
{
    // Keep the animation at the same frame when the speed changes.
    if (percent != velocityPercentage)
    {
        currentTime = (velocityPercentage * currentTime * frameRate) / (percent * frameRate);
    }

    velocityPercentage = percent;
}

auto ElementalActor::recalcBounds(Camera* cam) -> int
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

    const float scale = cam->cameraScale == 1 ? 0.5f : 1.0f;
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

auto ElementalActor::render(int32_t depthFixup) -> int32_t
{
    GameObject* obj = owner;
    screenPos = obj->getScreenPos(eye->cameraId - 1);
    const float facing = objectFacing(obj);
    currentShape = appearType->getGesture(currentGesture, facing, frameRate, visible);
    recalcBounds(eye);
    ElementList->openGroup(static_cast<int32_t>(static_cast<float>(depthFixup) - screenPos.y), 1);

    if (drawTerrainGrid != 0)
    {
        drawSelectBox(0xff);
    }

    if (currentShape == nullptr || currentShape->frameList == nullptr)
    {
        return 0;
    }

    uint8_t* fadeTable = nullptr;

    if (fadeTableIndex != -1 && fadeTableIndex >= 0)
    {
        fadeTable = gamePalette->fadePalettes.get() + (fadeTableIndex + gamePalette->numBitmapHazeLevels * 2) * 0x100;
    }

    if (currentFrame < 0)
    {
        currentFrame = 0;
    }

    ElementList->add(ElementPool::Make<VFXElement>(currentShape->frameList, screenPos.x, screenPos.y, currentFrame, 0,
                                                   fadeTable, 1, 0));

    // Selection: -1 and 1 draw the bars, 2 the brackets in the owner's alignment colour.
    const int32_t selected = owner->selected;

    if (selected == -1 || selected == 1)
    {
        recalcBounds(eye);
        drawBars();
    }
    else if (selected == 2)
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

    return 0;
}

auto ElementalActor::update() -> int32_t
{
    int32_t gesture = currentGesture;
    goalPending = 0;

    if (gestureGoal != gesture)
    {
        gesture = gestureGoal;
        currentGesture = gesture;
        oldGesture = gesture;
        jumpSetup = 0;
        jumping = gesture == GESTURE_JUMP ? 1 : 0;

        if (owner->isDisabled() == 0)
        {
            owner->status = 0;
        }

        currentFrame = 0;
        // Keep the animation time consistent with the new gesture's rate.
        const float newRate = std::fabs(appearType->gestures[gesture].frameRate);

        if (newRate != frameRate || velocityPercentage != 1.0)
        {
            currentTime = (frameRate * currentTime) / (velocityPercentage * newRate);
        }
    }

    frameRate = appearType->gestures[gesture].frameRate;

    if (frameRate < 0.0)
    {
        frameRate = -frameRate;
    }

    appearType->setGesture(gesture, objectFacing(owner), frameRate);

    if (currentFrame == -1)
    {
        currentFrame = 0;
        return 1;
    }

    frameRate = velocityPercentage * frameRate;
    currentTime = frameLength + currentTime;
    const double frames = static_cast<double>(currentTime * frameRate);
    const int32_t wholeFrames = static_cast<int32_t>(std::floor(frames));

    if (lastFrame < wholeFrames)
    {
        const int32_t played = lastFrame;
        lastFrame = wholeFrames;
        const int32_t advanced = wholeFrames - played;
        // The jump gesture only animates while unknown8C is set.
        const int32_t current = currentGesture;

        if (advanced != 0 && (current != GESTURE_JUMP || unknown8C != 0))
        {
            const uint32_t frame = static_cast<uint32_t>(currentFrame + advanced);
            currentFrame = static_cast<int32_t>(frame);
            const uint32_t numFrames = appearType->gestures[current].numFrames;

            if (static_cast<int32_t>(numFrames) <= static_cast<int32_t>(frame))
            {
                currentFrame = static_cast<int32_t>(frame % numFrames);

                if (current == GESTURE_JUMP)
                {
                    // The jump is over: back to gesture 0.
                    setGestureGoal(0);
                    const int32_t goal = gestureGoal;
                    currentGesture = goal;
                    oldGesture = goal;
                    jumping = 0;
                    const float newRate = std::fabs(appearType->gestures[goal].frameRate);

                    if (newRate != frameRate || velocityPercentage != 1.0)
                    {
                        currentTime = (frameRate * currentTime) / (velocityPercentage * newRate);
                    }

                    currentFrame = 0;
                }
            }
        }
    }

    return 1;
}

auto ElementalActor::destroy() -> void
{
    appearType->removeUsers(this);
    appearanceTypeList->removeAppearance(appearType);
}

auto ElementalActor::drawBars() -> void
{
}
