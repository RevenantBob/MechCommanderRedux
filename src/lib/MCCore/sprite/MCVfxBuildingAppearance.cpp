#include "stdafx.h"
#include "sprite/MCVfxBuildingAppearance.h"
#include "camera/MCCamera.h"
#include "engine/MCElementBuffer.h"
#include "engine/MCVfxElement.h"
#include "main/MCMissionGlobals.h"
#include "object/MCBuilding.h"
#include "object/MCBuildingType.h"
#include "object/MCBuildingMarines.h"
#include "object/MCObjectDrawing.h"
#include "object/MCForces.h"
#include "sprite/MCShape.h"
#include "sprite/MCSpriteMath.h"
#include "terrain/MCTerrain.h"
#include "vfx/MCVfxFunctions.h"

namespace
{
    /// <summary>Adds a VFX element of frame <paramref name="frame"/> of <paramref name="shapeTable"/>, unscaled.</summary>
    auto AddShape(uint8_t* shapeTable, float x, float y, int32_t frame, uint8_t* fadeTable) -> void
    {
        ElementList()->Add(ElementList()->Make<MCVfxElement>(shapeTable, x, y, frame, 0, fadeTable, 1));
    }

    /// <summary>The building <paramref name="obj"/> is, or null for any other class.</summary>
    auto AsBuilding(MCGameObject* obj) -> MCBuilding*
    {
        return obj != nullptr && obj->ObjectClass == MCObjectClass::Building ? static_cast<MCBuilding*>(obj) : nullptr;
    }
}

auto MCVfxBuildingAppearance::Init(MCAppearanceType* tree, MCGameObject* obj) -> int32_t
{
    FadeTable = nullptr;
    DamageSet = false;
    DamageLevel = 0;
    InView = false;
    AnimState = -1;
    const int32_t result = MCVfxAppearance::Init(tree, obj);
    BuildType = static_cast<MCVfxBuildingAppearanceType*>(tree);
    return result;
}

auto MCVfxBuildingAppearance::Update() -> int32_t
{
    if (Visible && Owner->IsCaptured() != 0 && !Highlighted && !Highlighting)
    {
        Highlighting = true;
        HighlightTime = 3.0f;
    }

    return 1;
}

auto MCVfxBuildingAppearance::SetDamageLvl(uint32_t newDamageLevel) -> void
{
    DamageSet = true;
    const uint32_t level = newDamageLevel & 0xf;
    DamageLevel = level;

    if (level != 0 && BuildType->NumFrames <= level)
    {
        DamageLevel = BuildType->NumFrames - 1;
    }

    if (TileNum > 9)
    {
        TileNum = 0;
    }
}

auto MCVfxBuildingAppearance::RecalcBounds(MCCamera*) -> int
{
    // Faithful: without a tile shape the visibility test takes in the previous shape's bounds.
    float tileMinX = ShapeMinX;
    float tileMinY = ShapeMinY;
    float tileMaxX = ShapeMaxX;
    float tileMaxY = ShapeMaxY;
    int result = 0;

    if (CurrentShape != nullptr && CurrentShape->FrameList != nullptr)
    {
        const MCFrameBounds bounds = MCShapeFrameBounds(CurrentShape->FrameList, 0);
        ShapeMinX = bounds.MinX;
        ShapeMinY = bounds.MinY;
        ShapeMaxX = bounds.Width;
        ShapeMaxY = bounds.Height;
        // Faithful: the last shape size read is what an off-screen building returns.
        result = VfxShapeResolution(CurrentShape->FrameList, 0);

        if (TileShape != nullptr && TileShape->FrameList != nullptr)
        {
            const MCFrameBounds tile = MCShapeFrameBounds(TileShape->FrameList, 0);
            tileMinX = tile.MinX;
            tileMinY = tile.MinY;
            tileMaxX = tile.Width;
            tileMaxY = tile.Height;
            result = VfxShapeResolution(TileShape->FrameList, 0);
        }

        InView = true;
    }

    if (Eye == nullptr)
    {
        return result;
    }

    // Faithful: the camera passed in is ignored; the eye's zoom and screen position 0 are used.
    MCBuilding* building = AsBuilding(Owner);
    const float scale = MCZoomScale(Eye);
    const MCVector2D pos = Owner->GetScreenPos(0);
    float x = pos.X;
    float y = pos.Y;

    if (building != nullptr)
    {
        x = static_cast<float>(building->PixelOffsetX) * scale + x;
        y = static_cast<float>(building->PixelOffsetY) * scale + y;
    }

    UpperLeft.X = scale * ShapeMinX + x;
    UpperLeft.Y = scale * ShapeMinY + y;
    LowerRight.X = scale * ShapeMaxX + UpperLeft.X;
    LowerRight.Y = scale * ShapeMaxY + UpperLeft.Y;

    // On screen when the building or its tile is.
    const float minX = ShapeMinX > tileMinX ? tileMinX : ShapeMinX;
    const float minY = ShapeMinY > tileMinY ? tileMinY : ShapeMinY;
    const float maxX = ShapeMaxX < tileMaxX ? tileMaxX : ShapeMaxX;
    const float maxY = ShapeMaxY < tileMaxY ? tileMaxY : ShapeMaxY;
    const float left = minX * scale + x;
    const float top = minY * scale + y;
    const float bottom = maxY * scale + top;

    if (0.0f <= maxX * scale + left && 0.0f <= bottom &&
        left <= static_cast<float>(static_cast<int32_t>(std::floor(Eye->ViewWidth))) &&
        top <= static_cast<float>(static_cast<int32_t>(std::floor(Eye->ViewHeight))))
    {
        return 1;
    }

    return result;
}

auto MCVfxBuildingAppearance::CalcCollideBounds() -> void
{
    float offsetX = 0.0f;
    float offsetY = 0.0f;

    if (MCBuilding* building = AsBuilding(Owner))
    {
        offsetX = static_cast<float>(building->PixelOffsetX);
        offsetY = static_cast<float>(building->PixelOffsetY);
    }

    UpperLeft.X = offsetX;
    UpperLeft.Y = offsetY;
    LowerRight.Y = offsetY;
    LowerRight.X = offsetX;

    if (CurrentShape == nullptr || CurrentShape->FrameList == nullptr)
    {
        return;
    }

    uint8_t* shapeTable = CurrentShape->FrameList;
    int32_t frame = CurrentFrame;
    const int32_t numShapeFrames = VfxShapeCount(shapeTable);

    if (frame == -1)
    {
        frame = 0;
    }

    if (numShapeFrames <= frame)
    {
        frame = numShapeFrames - 1;
    }

    const MCFrameBounds bounds = MCShapeFrameBounds(shapeTable, frame);
    UpperLeft.X = bounds.MinX + offsetX;
    UpperLeft.Y = bounds.MinY + offsetY;
    LowerRight.X = bounds.Width + UpperLeft.X;
    LowerRight.Y = bounds.Height + UpperLeft.Y;
}

auto MCVfxBuildingAppearance::Render(int32_t) -> int32_t
{
    MCBuilding* building = AsBuilding(Owner);

    if (building != nullptr)
    {
        ScreenPos = building->GetScreenPos(Eye->CameraId - 1);
    }

    MCVfxBuildingAppearanceType* type = BuildType;
    MCShape* shape = type->GetShape(DamageLevel);
    CurrentShape = shape;
    MCShape* tile = type->GetTileShape(TileNum);
    TileShape = tile;
    const float scale = MCZoomScale(Eye);
    const bool fullSize = scale == 1.0;

    // The tile goes under everything; zoomed out uses the table's second (small) frame.
    ElementList()->OpenGroup(20000000, 1);

    if (tile != nullptr && tile->FrameList != nullptr)
    {
        AddShape(tile->FrameList, ScreenPos.X, ScreenPos.Y, fullSize ? 0 : 1, FadeTable);
    }

    if (building != nullptr)
    {
        ScreenPos.X = static_cast<float>(building->PixelOffsetX) * scale + ScreenPos.X;
        ScreenPos.Y = static_cast<float>(building->PixelOffsetY) * scale + ScreenPos.Y;
    }

    if (shape != nullptr && shape->FrameList != nullptr)
    {
        int32_t frame = fullSize ? 0 : 1;

        if (AnimState != -1)
        {
            const int32_t numShapeFrames = VfxShapeCount(shape->FrameList);

            if (numShapeFrames <= CurrentFrame)
            {
                CurrentFrame = numShapeFrames - 1;
            }

            frame = CurrentFrame;
        }

        ElementList()->OpenGroup(static_cast<int32_t>(-ScreenPos.Y), 1);
        AddShape(shape->FrameList, ScreenPos.X, ScreenPos.Y, frame, FadeTable);
    }

    if (Owner->Selected == 1 || Owner->GetNumAttackers() > 0)
    {
        DrawBars();
    }

    MCGameObject* obj = Owner;

    if (obj != nullptr && obj->Selected != 0)
    {
        const int32_t homeAlignment = HomeTeam()->Alignment;
        RecalcBounds(Eye);

        if (obj->GetAlignment() != homeAlignment && obj->GetAlignment() != 0 && static_cast<uint8_t>(obj->Status) != 2)
        {
            DrawSelectBrackets(0xcf);
        }
        else
        {
            DrawSelectBox(0xe1);
        }
    }

    if (DrawTerrainGrid != 0)
    {
        DrawSelectBox(0xe1);
    }

    if (Highlighting)
    {
        if (HighlightTime > 0.0f)
        {
            HighlightTime -= FrameLength;
            DrawSelectBox(0xfc);
            return 0;
        }

        Highlighted = true;
    }

    return 0;
}

auto MCVfxBuildingAppearance::DrawBars() -> void
{
    MCDrawDamageBar(this, GetAppearanceType(), Owner);
}
