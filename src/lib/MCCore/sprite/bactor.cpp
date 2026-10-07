#include "stdafx.h"
#include "sprite/bactor.h"
#include "camera/camera.h"
#include "engine/ceglist.h"
#include "engine/cevfx.h"
#include "lib/MCFitIniFile.h"
#include "main/main.h"
#include "object/bldng.h"
#include "object/team.h"
#include "sprite/sprtmgr.h"
#include "sprite/vfxshape.h"
#include "terrain/terrain.h"
#include "vfx/vfxfuncs.h"

int DynamicFrameTiming = 1;

namespace
{
    /// <summary>The first building frame's packet (packets 0..9 are the tiles under buildings).</summary>
    constexpr uint32_t FIRST_FRAME_PACKET = 0xc;
    /// <summary>The number of tile packets.</summary>
    constexpr uint32_t NUM_TILE_SHAPES = 10;

    /// <summary>Adds a VFX element of frame <paramref name="frame"/> of <paramref name="shapeTable"/>.</summary>
    auto AddShape(uint8_t* shapeTable, float x, float y, int32_t frame, uint8_t* fadeTable, const char* name) -> void
    {
        auto* element = MCElementPool::Make<MCVfxElement>(shapeTable, x, y, frame, 0, fadeTable, 1, 0);

        // Port fix: the original copies the debug name through a null element too.
        if (element != nullptr)
        {
            strcpy(element->Name, name);
        }

        ElementList->Add(element);
    }
}

//---------------------------------------------------------------------------
// VFXBuildingAppearanceType
//---------------------------------------------------------------------------

auto MCVfxBuildingAppearanceType::Init(MCFile* apprFile, uint32_t fileSize, uint32_t loadFlags) -> int32_t
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
        return static_cast<int32_t>(0xbead0002);
    }

    for (uint32_t i = 0; i < NumPackets; i++)
    {
        ShapeList[i] = nullptr;
    }

    return 0;
}

auto MCVfxBuildingAppearanceType::RemoveShape(MCShape* shape) -> void
{
    for (int32_t i = 0; i < static_cast<int32_t>(NumPackets); i++)
    {
        if (ShapeList[i] == shape)
        {
            ShapeList[i] = nullptr;
        }
    }

    for (MCAppearanceUser* user = UserList; user != nullptr; user = user->Next)
    {
        auto* appearance = static_cast<MCVfxBuildingAppearance*>(user->User);

        if (appearance->CurrentShape == shape)
        {
            appearance->CurrentShape = nullptr;
        }

        if (appearance->TileShape == shape)
        {
            appearance->TileShape = nullptr;
        }
    }
}

auto MCVfxBuildingAppearanceType::LoadIniFile(MCFile* apprFile, uint32_t fileSize) -> int32_t
{
    MCFitIniFile iniFile;
    int32_t result = iniFile.Open(apprFile, fileSize);

    if (result != 0)
    {
        return result;
    }

    if ((result = iniFile.SeekBlock("Main Info")) != 0)
    {
        return result;
    }

    if ((result = iniFile.ReadIdULong("NumFrames", NumFrames)) != 0)
    {
        return result;
    }

    if (iniFile.SeekBlock("AnimationInfo") == 0)
    {
        if ((result = iniFile.ReadIdULong("NumAnimStates", NumAnimStates)) != 0)
        {
            return result;
        }

        const uint32_t count = NumAnimStates;
        AnimStates =
            static_cast<MCBuildingAnimState*>(SpriteManager->MallocDataRam(count * sizeof(MCBuildingAnimState)));

        if (AnimStates == nullptr)
        {
            return static_cast<int32_t>(0xbead0001);
        }

        for (int32_t i = 0; i < static_cast<int32_t>(count); i++)
        {
            char blockName[20];
            sprintf(blockName, "AnimState%d", i);

            if ((result = iniFile.SeekBlock(blockName)) != 0)
            {
                return result;
            }

            if ((result = iniFile.ReadIdULong("numFrames", AnimStates[i].NumFrames)) != 0)
            {
                return result;
            }

            if ((result = iniFile.ReadIdFloat("frameRate", AnimStates[i].FrameRate)) != 0)
            {
                return result;
            }
        }
    }
    else
    {
        NumAnimStates = 0;
        AnimStates = nullptr;
    }

    iniFile.Close();
    return 0;
}

auto MCVfxBuildingAppearanceType::GetShape(uint32_t frame) -> MCShape*
{
    const uint32_t packet = frame + FIRST_FRAME_PACKET;

    if (NumPackets <= packet)
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
    shape = SpriteManager->GetShapeData(AppearanceNum & 0xffffff, packet, Turn, this, 0);
    ShapeList[packet] = shape;
    return shape;
}

auto MCVfxBuildingAppearanceType::GetTileShape(uint32_t tileNum) -> MCShape*
{
    if (tileNum >= NUM_TILE_SHAPES || NumPackets <= tileNum)
    {
        return nullptr;
    }

    MCShape* shape = ShapeList[tileNum];

    if (shape != nullptr)
    {
        shape->LastTurnUsed = Turn;
        return shape;
    }

    DynamicFrameTiming = 0;
    shape = SpriteManager->GetShapeData(AppearanceNum & 0xffffff, tileNum, Turn, this, 0);
    ShapeList[tileNum] = shape;
    return shape;
}

auto MCVfxBuildingAppearanceType::Destroy() -> void
{
    // The shapes stay in the sprite manager's cache, ownerless.
    for (int32_t i = 0; i < static_cast<int32_t>(NumPackets); i++)
    {
        if (ShapeList[i] != nullptr)
        {
            ShapeList[i]->Owner = nullptr;
        }
    }

    // Faithful: the animation states are not freed (they go with the data heap).
    SpriteManager->FreeDataRam(ShapeList);
    ShapeList = nullptr;
}

//---------------------------------------------------------------------------
// VFXBuildingAppearance
//---------------------------------------------------------------------------

auto MCVfxBuildingAppearance::Init(MCAppearanceType* tree, MCGameObject* obj) -> int32_t
{
    FadeTable = nullptr;
    DamageSet = 0;
    DamageLevel = 0;
    InView = 0;
    AnimState = -1;
    const int32_t result = MCVfxAppearance::Init(tree, obj);
    BuildType = static_cast<MCVfxBuildingAppearanceType*>(tree);
    return result;
}

auto MCVfxBuildingAppearance::Update() -> int32_t
{
    if (Visible != 0 && Owner->IsCaptured() != 0 && Highlighted == 0 && Highlighting == 0)
    {
        Highlighting = 1;
        HighlightTime = 3.0f;
    }

    return 1;
}

auto MCVfxBuildingAppearance::SetDamageLvl(uint32_t newDamageLevel) -> void
{
    DamageSet = 1;
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
        uint8_t* shapeTable = CurrentShape->FrameList;
        result = VfxShapeMinxy(shapeTable, 0);
        ShapeMinX = static_cast<float>(result >> 16);
        ShapeMinY = static_cast<float>(static_cast<int16_t>(result));
        result = VfxShapeResolution(shapeTable, 0);
        ShapeMaxX = static_cast<float>(result >> 16);
        ShapeMaxY = static_cast<float>(static_cast<int16_t>(result));

        if (TileShape != nullptr && TileShape->FrameList != nullptr)
        {
            uint8_t* tileTable = TileShape->FrameList;
            result = VfxShapeMinxy(tileTable, 0);
            tileMinX = static_cast<float>(result >> 16);
            tileMinY = static_cast<float>(static_cast<int16_t>(result));
            result = VfxShapeResolution(tileTable, 0);
            tileMaxX = static_cast<float>(result >> 16);
            tileMaxY = static_cast<float>(static_cast<int16_t>(result));
        }

        InView = 1;
    }

    if (Eye == nullptr)
    {
        return result;
    }

    // Faithful: the camera passed in is ignored; the eye's zoom and screen position 0 are used.
    MCBuilding* building = Owner->ObjectClass == BUILDING ? static_cast<MCBuilding*>(Owner) : nullptr;
    const float scale = Eye->CameraScale == 1 ? 0.5f : 1.0f;
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
    MCGameObject* obj = Owner;

    if (obj->ObjectClass == BUILDING && obj != nullptr)
    {
        offsetX = static_cast<float>(static_cast<MCBuilding*>(obj)->PixelOffsetX);
        offsetY = static_cast<float>(static_cast<MCBuilding*>(obj)->PixelOffsetY);
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

    const int32_t minXY = VfxShapeMinxy(shapeTable, frame);
    UpperLeft.X = static_cast<float>(minXY >> 16) + offsetX;
    UpperLeft.Y = static_cast<float>(static_cast<int16_t>(minXY)) + offsetY;
    const int32_t size = VfxShapeResolution(shapeTable, frame);
    LowerRight.X = static_cast<float>(size >> 16) + UpperLeft.X;
    LowerRight.Y = static_cast<float>(static_cast<int16_t>(size)) + UpperLeft.Y;
}

auto MCVfxBuildingAppearance::Render(int32_t) -> int32_t
{
    MCBuilding* building = nullptr;

    if (Owner->ObjectClass == BUILDING && Owner != nullptr)
    {
        building = static_cast<MCBuilding*>(Owner);
        ScreenPos = building->GetScreenPos(Eye->CameraId - 1);
    }

    MCVfxBuildingAppearanceType* type = BuildType;
    MCShape* shape = type->GetShape(DamageLevel);
    CurrentShape = shape;
    MCShape* tile = type->GetTileShape(TileNum);
    TileShape = tile;
    const float scale = Eye->CameraScale == 1 ? 0.5f : 1.0f;
    const bool fullSize = scale == 1.0;

    // The tile goes under everything; zoomed out uses the table's second (small) frame.
    ElementList->OpenGroup(20000000, 1);

    if (tile != nullptr && tile->FrameList != nullptr)
    {
        AddShape(tile->FrameList, ScreenPos.X, ScreenPos.Y, fullSize ? 0 : 1, FadeTable,
                 fullSize ? "bactor1" : "bactor2");
    }

    if (building != nullptr)
    {
        ScreenPos.X = static_cast<float>(building->PixelOffsetX) * scale + ScreenPos.X;
        ScreenPos.Y = static_cast<float>(building->PixelOffsetY) * scale + ScreenPos.Y;
    }

    if (shape != nullptr && shape->FrameList != nullptr)
    {
        if (AnimState == -1)
        {
            ElementList->OpenGroup(static_cast<int32_t>(-ScreenPos.Y), 1);
            AddShape(shape->FrameList, ScreenPos.X, ScreenPos.Y, fullSize ? 0 : 1, FadeTable,
                     fullSize ? "bactor4" : "bactor5");
        }
        else
        {
            const int32_t numShapeFrames = VfxShapeCount(shape->FrameList);

            if (numShapeFrames <= CurrentFrame)
            {
                CurrentFrame = numShapeFrames - 1;
            }

            ElementList->OpenGroup(static_cast<int32_t>(-ScreenPos.Y), 1);
            AddShape(shape->FrameList, ScreenPos.X, ScreenPos.Y, CurrentFrame, FadeTable, "bactor3");
        }
    }

    if (Owner->Selected == 1 || Owner->GetNumAttackers() > 0)
    {
        DrawBars();
    }

    MCGameObject* obj = Owner;

    if (obj != nullptr && obj->Selected != 0)
    {
        const int32_t homeAlignment = HomeTeam->Alignment;

        if (obj->GetAlignment() != homeAlignment && obj->GetAlignment() != 0 && static_cast<uint8_t>(obj->Status) != 2)
        {
            RecalcBounds(Eye);
            DrawSelectBrackets(0xcf);
        }
        else
        {
            RecalcBounds(Eye);
            DrawSelectBox(0xe1);
        }
    }

    if (DrawTerrainGrid != 0)
    {
        DrawSelectBox(0xe1);
    }

    if (Highlighting != 0)
    {
        if (HighlightTime > 0.0f)
        {
            HighlightTime -= FrameLength;
            DrawSelectBox(0xfc);
            return 0;
        }

        Highlighted = 1;
    }

    return 0;
}

auto MCVfxBuildingAppearance::Destroy() -> void
{
    AppearType->RemoveUsers(this);
}

auto MCVfxBuildingAppearance::DrawBars() -> void
{
    MCDrawDamageBar(this, GetAppearanceType(), Owner);
}
