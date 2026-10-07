#include "stdafx.h"
#include "appear/lineappr.h"
#include "camera/camera.h"
#include "color/MCPalette.h"
#include "engine/MCElementBuffer.h"
#include "engine/MCLineElement.h"
#include "lib/MCFitIniFile.h"
#include "object/gameobj.h"

auto MCLineAppearanceType::Init(MCFile* apprFile, uint32_t fileSize) -> int32_t
{
    return LoadIniFile(apprFile, fileSize);
}

auto MCLineAppearanceType::LoadIniFile(MCFile* apprFile, uint32_t fileSize) -> int32_t
{
    MCFitIniFile iniFile;
    int32_t result = iniFile.Open(apprFile, fileSize);

    if (result != 0)
    {
        return result;
    }

    result = iniFile.SeekBlock("Main Info");

    if (result != 0)
    {
        return result;
    }

    char name[12];
    result = iniFile.ReadIdString("Name", name, 9);

    if (result != 0)
    {
        return result;
    }

    // The type's heap size: the states lived in a heap of this size, so a size of 0 fails as the original did.
    uint32_t heapSize = 0;
    result = iniFile.ReadIdULong("HeapSize", heapSize);

    if (result != 0)
    {
        return result;
    }

    if (heapSize == 0)
    {
        return -0x4152fff6;
    }

    States.assign(NUM_LINE_STATES, MCLineStateData{});
    MCLineStateData* state = States.data();

    result = iniFile.SeekBlock("States");

    if (result != 0)
    {
        return result;
    }

    uint8_t numStates;
    result = iniFile.ReadIdUChar("NumStates", numStates);

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
        result = iniFile.SeekBlock(blockName);

        if (result != 0)
        {
            return result;
        }

        uint8_t stateNum;
        result = iniFile.ReadIdUChar("State", stateNum);

        if (result != 0)
        {
            return result;
        }

        state->State = stateNum;
        result = iniFile.ReadIdLong("StartColor", state->StartColor);

        if (result != 0)
        {
            return result;
        }

        result = iniFile.ReadIdLong("EndColor", state->EndColor);

        if (result != 0)
        {
            return result;
        }

        result = iniFile.ReadIdLong("FadeTable", state->FadeTable);

        if (result != 0)
        {
            return result;
        }

        result = iniFile.ReadIdLong("SingleColor", state->SingleColor);

        if (result != 0)
        {
            return result;
        }
    }

    iniFile.Close();
    return 0;
}

auto MCLineAppearanceType::Destroy() -> void
{
    States.clear();
}

auto MCLineAppearance::Init(MCAppearanceType* tree, MCGameObject* obj) -> int32_t
{
    Owner = obj;
    Visible = 0;
    AppearType = static_cast<MCLineAppearanceType*>(tree);
    CurrentState = LINE_STATE_0;
    return 0;
}

auto MCLineAppearance::RecalcBounds(MCCamera* cam) -> int
{
    if (cam == nullptr)
    {
        return 0;
    }

    // Both ends are projected through the game's camera; cam only gives the view size.
    MCCamera* camera = Eye;
    float scale = camera->GetScaleFactor();
    MCVector3D offset = StartPos - camera->Position;
    offset *= scale;
    ScreenStart.X = offset.Y * camera->CosAngle + offset.X * camera->CosAngle + camera->HalfWidth;
    ScreenStart.Y = ((offset.X * camera->SinAngle + camera->HalfHeight) - offset.Y * camera->SinAngle) - offset.Z;
    camera = Eye;
    scale = camera->GetScaleFactor();
    offset = EndPos - camera->Position;
    offset *= scale;
    ScreenEnd.X = offset.Y * camera->CosAngle + offset.X * camera->CosAngle + camera->HalfWidth;
    ScreenEnd.Y = ((offset.X * camera->SinAngle + camera->HalfHeight) - offset.Y * camera->SinAngle) - offset.Z;

    if (ScreenEnd.X <= ScreenStart.X)
    {
        UpperLeft = ScreenEnd;
        LowerRight = ScreenStart;
    }
    else
    {
        UpperLeft = ScreenStart;
        LowerRight = ScreenEnd;
    }

    if (0.0f <= LowerRight.X && 0.0f <= LowerRight.Y)
    {
        const int32_t viewWidth =
            static_cast<int16_t>(static_cast<int32_t>(std::floor(static_cast<double>(cam->ViewWidth))));

        if (UpperLeft.X <= static_cast<float>(viewWidth))
        {
            const int32_t viewHeight =
                static_cast<int16_t>(static_cast<int32_t>(std::floor(static_cast<double>(cam->ViewHeight))));

            if (UpperLeft.Y <= static_cast<float>(viewHeight))
            {
                return 1;
            }
        }
    }

    return 0;
}

auto MCLineAppearance::Render() -> int32_t
{
    if (Visible == 0)
    {
        return 0;
    }

    const auto depth = static_cast<int32_t>(ScreenStart.Y);
    ElementList()->OpenGroup(depth, 1);
    MCLineStateData& state = AppearType->States[CurrentState];
    uint8_t* fadeTable = nullptr;

    if (state.FadeTable != -1 && state.FadeTable > -1)
    {
        fadeTable = GamePalette()->GetFadeTable(state.FadeTable);
    }

    ElementList()->Add(
        ElementList()->Make<MCLineElement>(ScreenStart, ScreenEnd, state.StartColor, fadeTable, depth, state.EndColor));

    if (Owner != nullptr && Owner->Selected != 0)
    {
        RecalcBounds(Eye);
    }

    return 0;
}

auto MCLineAppearance::Update() -> int32_t
{
    return 1;
}

auto MCLineAppearance::Destroy() -> void
{
    AppearanceTypeList->RemoveAppearance(AppearType);
}

auto MCLineAppearance::StateExists(MCLineState state) -> int32_t
{
    if (state < NUM_LINE_STATES && state > -1)
    {
        return 1;
    }

    return 0;
}
