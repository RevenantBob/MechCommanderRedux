#include "stdafx.h"
#include "appear/lineappr.h"
#include "camera/camera.h"
#include "color/color.h"
#include "engine/ceglist.h"
#include "engine/celine.h"
#include "lib/inifile.h"
#include "object/gameobj.h"

auto LineAppearanceType::init(File* apprFile, uint32_t fileSize) -> int32_t
{
    return loadIniFile(apprFile, fileSize);
}

auto LineAppearanceType::loadIniFile(File* apprFile, uint32_t fileSize) -> int32_t
{
    FitIniFile iniFile;
    int32_t result = iniFile.open(apprFile, fileSize, 0x32);

    if (result != 0)
    {
        return result;
    }

    result = iniFile.seekBlock("Main Info");

    if (result != 0)
    {
        return result;
    }

    char name[12];
    result = iniFile.readIdString("Name", name, 9);

    if (result != 0)
    {
        return result;
    }

    // The type's heap size: the states lived in a heap of this size, so a size of 0 fails as the original did.
    uint32_t heapSize = 0;
    result = iniFile.readIdULong("HeapSize", heapSize);

    if (result != 0)
    {
        return result;
    }

    if (heapSize == 0)
    {
        return -0x4152fff6;
    }

    states.assign(NUM_LINE_STATES, LineStateData{});
    LineStateData* state = states.data();

    result = iniFile.seekBlock("States");

    if (result != 0)
    {
        return result;
    }

    uint8_t numStates;
    result = iniFile.readIdUChar("NumStates", numStates);

    if (result != 0)
    {
        return result;
    }

    if (numStates != NUM_LINE_STATES)
    {
        return -0x4152fff5;
    }

    for (int32_t i = 0; i < NUM_LINE_STATES; i++, state++)
    {
        char blockName[20];
        std::snprintf(blockName, sizeof(blockName), "State%d", i);
        result = iniFile.seekBlock(blockName);

        if (result != 0)
        {
            return result;
        }

        uint8_t stateNum;
        result = iniFile.readIdUChar("State", stateNum);

        if (result != 0)
        {
            return result;
        }

        state->state = stateNum;
        result = iniFile.readIdLong("StartColor", state->startColor);

        if (result != 0)
        {
            return result;
        }

        result = iniFile.readIdLong("EndColor", state->endColor);

        if (result != 0)
        {
            return result;
        }

        result = iniFile.readIdLong("FadeTable", state->fadeTable);

        if (result != 0)
        {
            return result;
        }

        result = iniFile.readIdLong("SingleColor", state->singleColor);

        if (result != 0)
        {
            return result;
        }
    }

    iniFile.close();
    return 0;
}

auto LineAppearanceType::destroy() -> void
{
    states.clear();
}

auto LineAppearance::init(AppearanceType* tree, GameObject* obj) -> int32_t
{
    owner = obj;
    visible = 0;
    appearType = static_cast<LineAppearanceType*>(tree);
    currentState = LINE_STATE_0;
    return 0;
}

auto LineAppearance::recalcBounds(Camera* cam) -> int
{
    if (cam == nullptr)
    {
        return 0;
    }

    // Both ends are projected through the game's camera; cam only gives the view size.
    Camera* camera = eye;
    float scale = camera->getScaleFactor();
    vector_3d offset = startPos - camera->position;
    offset *= scale;
    screenStart.x = offset.y * camera->cosAngle + offset.x * camera->cosAngle + camera->halfWidth;
    screenStart.y = ((offset.x * camera->sinAngle + camera->halfHeight) - offset.y * camera->sinAngle) - offset.z;
    camera = eye;
    scale = camera->getScaleFactor();
    offset = endPos - camera->position;
    offset *= scale;
    screenEnd.x = offset.y * camera->cosAngle + offset.x * camera->cosAngle + camera->halfWidth;
    screenEnd.y = ((offset.x * camera->sinAngle + camera->halfHeight) - offset.y * camera->sinAngle) - offset.z;

    if (screenEnd.x <= screenStart.x)
    {
        upperLeft = screenEnd;
        lowerRight = screenStart;
    }
    else
    {
        upperLeft = screenStart;
        lowerRight = screenEnd;
    }

    if (0.0f <= lowerRight.x && 0.0f <= lowerRight.y)
    {
        const int32_t viewWidth =
            static_cast<int16_t>(static_cast<int32_t>(std::floor(static_cast<double>(cam->viewWidth))));

        if (upperLeft.x <= static_cast<float>(viewWidth))
        {
            const int32_t viewHeight =
                static_cast<int16_t>(static_cast<int32_t>(std::floor(static_cast<double>(cam->viewHeight))));

            if (upperLeft.y <= static_cast<float>(viewHeight))
            {
                return 1;
            }
        }
    }

    return 0;
}

auto LineAppearance::render() -> int32_t
{
    if (visible == 0)
    {
        return 0;
    }

    const auto depth = static_cast<int32_t>(screenStart.y);
    ElementList->openGroup(depth, 1);
    LineStateData& state = appearType->states[currentState];
    uint8_t* fadeTable = nullptr;

    if (state.fadeTable != -1 && state.fadeTable > -1)
    {
        fadeTable = gamePalette->fadePalettes.get() + (state.fadeTable + gamePalette->numBitmapHazeLevels * 2) * 0x100;
    }

    ElementList->add(
        ElementPool::Make<LineElement>(screenStart, screenEnd, state.startColor, fadeTable, depth, state.endColor));

    if (owner != nullptr && owner->selected != 0)
    {
        recalcBounds(eye);
    }

    return 0;
}

auto LineAppearance::update() -> int32_t
{
    return 1;
}

auto LineAppearance::destroy() -> void
{
    appearanceTypeList->removeAppearance(appearType);
}

auto LineAppearance::stateExists(LineState state) -> int32_t
{
    if (state < NUM_LINE_STATES && state > -1)
    {
        return 1;
    }

    return 0;
}
