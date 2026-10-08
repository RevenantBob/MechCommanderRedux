#include "stdafx.h"
#include "sprite/MCArmAppearance.h"
#include "appear/MCAppearanceTypeList.h"
#include "camera/MCCamera.h"
#include "color/MCPalette.h"
#include "engine/MCElementBuffer.h"
#include "engine/MCVfxElement.h"
#include "main/main.h"
#include "object/MCBigGameObject.h"
#include "sprite/MCShape.h"
#include "sprite/MCSpriteMath.h"
#include "terrain/MCTerrain.h"

MCArmAppearance::~MCArmAppearance()
{
    if (AppearType != nullptr)
    {
        AppearType->RemoveUser(this);
        AppearanceTypeList()->RemoveAppearance(AppearType);
    }
}

auto MCArmAppearance::Init(MCAppearanceType* tree, MCGameObject* obj) -> int32_t
{
    Owner = obj;
    AppearType = static_cast<MCArmAppearanceType*>(tree);

    if (tree != nullptr)
    {
        tree->AddUser(this);
    }

    CurrentShape = nullptr;
    CurrentFrame = -1;
    FadeTableIndex = -1;
    ShapeMinY = -15.0f;
    ShapeMinX = -15.0f;
    Visible = false;
    CurrentTime = 0.0f;
    LastFrame = 0;
    Reverse = false;
    InView = false;
    Rotation = 0.0f;
    ShapeMaxY = 15.0f;
    ShapeMaxX = 15.0f;
    return 0;
}

auto MCArmAppearance::RecalcBounds(MCCamera* cam) -> int
{
    if (cam == nullptr)
    {
        return 0;
    }

    const MCVector2D pos = Owner->GetScreenPos(cam->CameraId - 1);
    UpperLeft = pos;
    LowerRight = pos;

    if (CurrentShape != nullptr && CurrentShape->FrameList != nullptr)
    {
        // The bounds only ever grow: they cover every frame drawn so far.
        uint8_t* shapeTable = CurrentShape->FrameList;
        MCGrowBounds(MCShapeFrameBounds(shapeTable, MCClampShapeFrame(shapeTable, CurrentFrame)), ShapeMinX, ShapeMinY,
                     ShapeMaxX, ShapeMaxY);
        InView = true;
    }

    // Faithful: the zoom is the eye's, the screen limits the camera's.
    const float scale = MCZoomScale(Eye);
    UpperLeft.X = scale * ShapeMinX + pos.X;
    UpperLeft.Y = scale * ShapeMinY + pos.Y;
    LowerRight.X = scale * ShapeMaxX + UpperLeft.X;
    LowerRight.Y = scale * ShapeMaxY + UpperLeft.Y;

    if (0.0f <= LowerRight.X && 0.0f <= LowerRight.Y &&
        UpperLeft.X <= static_cast<float>(static_cast<int32_t>(std::floor(cam->ViewWidth))) &&
        UpperLeft.Y <= static_cast<float>(static_cast<int32_t>(std::floor(cam->ViewHeight))))
    {
        return 1;
    }

    return 0;
}

auto MCArmAppearance::Render(int32_t depthFixup) -> int32_t
{
    if (Owner->Selected != 0)
    {
        RecalcBounds(Eye);
        DrawSelectBox(0xfd);
    }

    MCGameObject* obj = Owner;
    ScreenPos = obj->GetScreenPos(Eye->CameraId - 1);
    Rotation = static_cast<float>(MCActorFacing(obj));
    CurrentShape = AppearType->GetShape(static_cast<int32_t>(Rotation), FrameRate, Reverse);

    if (DrawTerrainGrid != 0)
    {
        DrawSelectBox(0xfd);
    }

    uint8_t* fadeTable = nullptr;

    if (FadeTableIndex >= 0)
    {
        fadeTable = GamePalette()->GetFadeTable(FadeTableIndex);
    }

    ElementList()->OpenGroup(static_cast<int32_t>(static_cast<float>(depthFixup) - ScreenPos.Y), 1);

    if (CurrentShape == nullptr || CurrentShape->FrameList == nullptr)
    {
        return 0;
    }

    ElementList()->Add(ElementList()->Make<MCVfxElement>(CurrentShape->FrameList, ScreenPos.X, ScreenPos.Y,
                                                         CurrentFrame, Reverse ? 1 : 0, fadeTable, 0));
    return 0;
}

auto MCArmAppearance::Update() -> int32_t
{
    if (CurrentFrame == -1)
    {
        CurrentFrame = 0;
    }

    const int32_t played = LastFrame;
    const MCArmActorData& data = AppearType->ActorData;
    FrameRate = data.FrameRate;
    CurrentTime = FrameLength + CurrentTime;
    const auto wholeFrames = static_cast<int32_t>(std::floor(static_cast<double>(CurrentTime * FrameRate)));

    if (played < wholeFrames)
    {
        LastFrame = wholeFrames;
        const int32_t advanced = wholeFrames - played;

        if (advanced != 0)
        {
            CurrentFrame += advanced;

            if (data.NumFrames <= static_cast<uint32_t>(CurrentFrame))
            {
                CurrentFrame = static_cast<int32_t>(data.NumFrames - 1);
                return 0;
            }
        }
    }

    return 1;
}
