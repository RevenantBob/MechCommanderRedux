#include "stdafx.h"
#include "sprite/actor.h"
#include "camera/camera.h"
#include "engine/ceglist.h"
#include "engine/cepane.h"
#include "engine/cepoly.h"
#include "engine/cevfx.h"
#include "lib/aerror.h"
#include "lib/inifile.h"
#include "lib/routines.h"
#include "main/main.h"
#include "object/gameobj.h"
#include "object/tbldng.h"
#include "object/team.h"
#include "object/terrobj.h"
#include "object/tree.h"
#include "sprite/bactor.h"
#include "sprite/sprtmgr.h"
#include "sprite/vfxshape.h"
#include "terrain/terrain.h"
#include "vfx/vfxfuncs.h"

namespace
{
    constexpr int32_t NO_DATA_RAM = static_cast<int32_t>(0xbead0002);

    /// <summary>
    /// The pixel offset of the terrain-placed classes (tree, terrain object, the unused class 0x17, tree building),
    /// all at +0x8c/+0x90. False for any other class.
    /// </summary>
    auto PixelOffset(MCGameObject* obj, int32_t& offsetX, int32_t& offsetY) -> bool
    {
        switch (static_cast<int32_t>(obj->ObjectClass))
        {
            case TREE:
            {
                offsetX = static_cast<MCTree*>(obj)->PixelOffsetX;
                offsetY = static_cast<MCTree*>(obj)->PixelOffsetY;
                return true;
            }
            case TERRAINOBJECT:
            case 0x17: // No class sets 0x17; the original reads the same offsets.
            {
                offsetX = static_cast<MCTerrainObject*>(obj)->PixelOffsetX;
                offsetY = static_cast<MCTerrainObject*>(obj)->PixelOffsetY;
                return true;
            }
            case TREEBUILDING:
            {
                offsetX = static_cast<MCTreeBuilding*>(obj)->PixelOffsetX;
                offsetY = static_cast<MCTreeBuilding*>(obj)->PixelOffsetY;
                return true;
            }
            default:
                return false;
        }
    }

    /// <summary>0.5 when <paramref name="cam"/> is zoomed out, else 1.</summary>
    auto ZoomScale(const MCCamera* cam) -> float
    {
        return cam->CameraScale == 1 ? 0.5f : 1.0f;
    }

    /// <summary>Reads the fields shared by states and sub-states.</summary>
    auto ReadStateData(MCFitIniFile& iniFile, MCActorData& data) -> int32_t
    {
        int32_t result = iniFile.ReadIdULong("NumFrames", data.NumFrames);

        if (result == 0)
        {
            result = iniFile.ReadIdFloat("FrameRate", data.FrameRate);
        }

        if (result == 0)
        {
            result = iniFile.ReadIdULong("BasePacketNumber", data.BasePacketNumber);
        }

        if (result == 0)
        {
            result = iniFile.ReadIdUChar("NumRotations", data.NumRotations);
        }

        if (result == 0)
        {
            result = iniFile.ReadIdUChar("Symmetrical", data.Symmetrical);
        }

        return result;
    }
}

//---------------------------------------------------------------------------
// VFXAppearanceType
//---------------------------------------------------------------------------

auto MCVfxAppearanceType::Init(MCFile* apprFile, uint32_t fileSize, uint32_t loadFlags) -> int32_t
{
    const int32_t result = LoadIniFile(apprFile, fileSize);

    if (result != 0)
    {
        return result;
    }

    const uint32_t packetFile = AppearanceNum & 0xffffff;
    NumPackets = static_cast<int16_t>(SpriteManager->GetNumShapes(packetFile));
    const uint32_t listSize = static_cast<uint32_t>(NumPackets) * sizeof(MCShape*);
    ShapeList = static_cast<MCShape**>(SpriteManager->MallocDataRam(listSize));

    if (ShapeList == nullptr)
    {
        return NO_DATA_RAM;
    }

    Memclear(ShapeList, static_cast<int>(listSize));
    KeepLoaded = static_cast<int32_t>(loadFlags);

    if (loadFlags != 0)
    {
        // Load the first (full size) shape now.
        const uint32_t first = Scaled != 0 ? 1 : 0;
        ShapeList[first] = SpriteManager->GetShapeData(packetFile, first, 1, this, 0);
    }

    return 0;
}

auto MCVfxAppearanceType::RemoveShape(MCShape* shape) -> void
{
    for (int32_t i = 0; i < NumPackets; i++)
    {
        if (ShapeList[i] == shape)
        {
            ShapeList[i] = nullptr;
        }
    }

    for (MCAppearanceUser* user = UserList; user != nullptr; user = user->Next)
    {
        auto* appearance = static_cast<MCVfxAppearance*>(user->User);

        if (appearance->CurrentShape == shape)
        {
            appearance->CurrentShape = nullptr;
        }
    }
}

auto MCVfxAppearanceType::LoadIniFile(MCFile* apprFile, uint32_t fileSize) -> int32_t
{
    MCFitIniFile iniFile;
    int32_t result = iniFile.Open(apprFile, fileSize, 50);

    if (result != 0)
    {
        return result;
    }

    result = iniFile.SeekBlock("Main Info");

    if (result != 0)
    {
        return result;
    }

    int32_t deltaValue = 0;

    if (iniFile.ReadIdLong("delta", deltaValue) != 0)
    {
        deltaValue = 0;
    }

    Delta = deltaValue;

    result = iniFile.SeekBlock("States");

    if (result != 0)
    {
        return result;
    }

    result = iniFile.ReadIdUChar("NumStates", NumStates);

    if (result != 0)
    {
        return result;
    }

    if (iniFile.ReadIdUChar("Scaled", Scaled) != 0)
    {
        Scaled = 0;
    }

    constexpr uint32_t stateTableSize = MAX_ACTOR_STATES * sizeof(MCActorData);
    ActorStateData = static_cast<MCActorData*>(SpriteManager->MallocDataRam(stateTableSize));

    if (ActorStateData == nullptr)
    {
        return NO_DATA_RAM;
    }

    Memclear(ActorStateData, stateTableSize);

    for (int32_t i = 0; i < NumStates; i++)
    {
        MCActorData& data = ActorStateData[i];
        char blockName[20];
        sprintf(blockName, "State%d", i);
        result = iniFile.SeekBlock(blockName);
        uint8_t stateValue = 0;

        if (result == 0)
        {
            result = iniFile.ReadIdUChar("State", stateValue);
        }

        if (result != 0)
        {
            return result;
        }

        data.State = static_cast<MCActorState>(stateValue);

        uint8_t numSubStates = 0;

        if (iniFile.ReadIdUChar("SubStates", numSubStates) != 0)
        {
            numSubStates = 0;
        }

        result = ReadStateData(iniFile, data);

        if (result != 0)
        {
            return result;
        }

        // Only the first state with sub-states gets them (in practice state 0).
        if (numSubStates == 0 || ActorSubStateData != nullptr)
        {
            continue;
        }

        const uint32_t subTableSize = numSubStates * sizeof(MCActorData);
        ActorSubStateData = static_cast<MCActorData*>(SpriteManager->MallocDataRam(subTableSize));

        if (ActorSubStateData == nullptr)
        {
            return NO_DATA_RAM;
        }

        Memclear(ActorSubStateData, static_cast<int>(subTableSize));

        for (int32_t j = 0; j < numSubStates; j++)
        {
            MCActorData& subData = ActorSubStateData[j];
            char subBlockName[20];
            sprintf(subBlockName, "Sub%dState%d", j, i);
            result = iniFile.SeekBlock(subBlockName);
            uint8_t subStateValue = 0;

            if (result == 0)
            {
                result = iniFile.ReadIdUChar("State", subStateValue);
            }

            if (result != 0)
            {
                return result;
            }

            subData.State = static_cast<MCActorState>(subStateValue);
            result = iniFile.ReadIdUChar("Sub", subData.SubState);

            if (result == 0)
            {
                result = ReadStateData(iniFile, subData);
            }

            if (result == 0)
            {
                result = iniFile.ReadIdUChar("Loop", subData.Loop);
            }

            if (result != 0)
            {
                return result;
            }
        }
    }

    iniFile.Close();
    return 0;
}

auto MCVfxAppearanceType::GetShape(MCActorState state, uint8_t subState, int32_t rotation, int32_t, float& frameRate,
                                   int& reverse) -> MCShape*
{
    const MCActorData& data = ActorStateData[state];
    reverse = 0;

    if (data.NumFrames == 0)
    {
        return nullptr;
    }

    // Symmetrical states mirror the negative facings; the others wrap them.
    int isReversed = 0;

    if (rotation < 0 && data.Symmetrical != 0)
    {
        isReversed = 1;
        rotation = -rotation;
    }
    else if (rotation < 0 && data.Symmetrical == 0)
    {
        rotation += 360;
    }

    reverse = isReversed;

    int32_t rotationIndex = static_cast<int32_t>(
        std::floor(static_cast<double>(static_cast<int32_t>(data.NumRotations) * rotation) * (1.0 / 360.0)));
    frameRate = data.FrameRate;
    uint32_t basePacket = data.BasePacketNumber;

    if (subState != 0xff && ActorSubStateData != nullptr)
    {
        basePacket = ActorSubStateData[subState].BasePacketNumber;
        frameRate = ActorSubStateData[subState].FrameRate;
    }

    float zoom = 1.0f;

    if (Eye != nullptr && Eye->CameraScale == 1)
    {
        zoom = 0.5f;
    }

    // Scaled types alternate full size and zoomed out packets.
    if (Scaled != 0)
    {
        rotationIndex <<= 1;
    }

    uint32_t packet = basePacket + static_cast<uint32_t>(rotationIndex);

    if (zoom != 1.0f && Scaled != 0)
    {
        packet++;
    }

    if (NumPackets <= static_cast<int32_t>(packet))
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

auto MCVfxAppearanceType::Destroy() -> void
{
    // The shapes stay in the sprite manager's cache, ownerless.
    for (int32_t i = 0; i < NumPackets; i++)
    {
        if (ShapeList[i] != nullptr)
        {
            ShapeList[i]->Owner = nullptr;
        }
    }

    SpriteManager->FreeDataRam(UserList);
    UserList = nullptr;
    SpriteManager->FreeDataRam(ShapeList);
    ShapeList = nullptr;
    SpriteManager->FreeDataRam(ActorStateData);
    ActorStateData = nullptr;
}

//---------------------------------------------------------------------------
// VFXAppearance
//---------------------------------------------------------------------------

auto MCVfxAppearance::Init(MCAppearanceType* tree, MCGameObject* obj) -> int32_t
{
    Visible = 0;
    Owner = obj;
    AppearType = static_cast<MCVfxAppearanceType*>(tree);

    if (tree != nullptr)
    {
        tree->AddUsers(this);
    }

    CurrentShape = nullptr;
    CurrentFrame = -1;
    LoopEnd = -1;
    LoopStart = -1;
    ShapeMinY = -15.0f;
    ShapeMinX = -15.0f;
    Visible = 0;
    CurrentTime = 0.0f;
    LastFrame = 0;
    FadeTable = nullptr;
    CurrentState = ACTOR_STATE_NORMAL;
    InView = 0;
    CurrentSubState = 0xff;
    ShapeMaxY = 15.0f;
    ShapeMaxX = 15.0f;
    TypeChanged = 1;
    return 0;
}

auto MCVfxAppearance::RecalcBounds(MCCamera* cam) -> int
{
    int result = 0;

    if (CurrentShape != nullptr && CurrentShape->FrameList != nullptr)
    {
        uint8_t* shapeTable = CurrentShape->FrameList;
        const int32_t minXY = VfxShapeMinxy(shapeTable, 0);
        ShapeMinX = static_cast<float>(minXY >> 16);
        ShapeMinY = static_cast<float>(static_cast<int16_t>(minXY));
        result = VfxShapeResolution(shapeTable, 0);
        ShapeMaxX = static_cast<float>(result >> 16);
        InView = 1;
        ShapeMaxY = static_cast<float>(static_cast<int16_t>(result));
    }

    if (cam == nullptr)
    {
        return result;
    }

    const MCVector2D pos = Owner->GetScreenPos(cam->CameraId - 1);
    float x = pos.X;
    float y = pos.Y;
    float scale = ZoomScale(cam);
    int32_t offsetX = 0;
    int32_t offsetY = 0;

    if (PixelOffset(Owner, offsetX, offsetY))
    {
        x += static_cast<float>(offsetX) * scale;
        y += static_cast<float>(offsetY) * scale;

        // Faithful: trees and tree buildings then take their bounds at full size.
        if (Owner->ObjectClass == TREE || Owner->ObjectClass == TREEBUILDING)
        {
            scale = 1.0f;
        }
    }

    UpperLeft.X = scale * ShapeMinX + x;
    UpperLeft.Y = scale * ShapeMinY + y;
    LowerRight.X = scale * ShapeMaxX + UpperLeft.X;
    LowerRight.Y = scale * ShapeMaxY + UpperLeft.Y;

    if (0.0f <= LowerRight.X && 0.0f <= LowerRight.Y &&
        UpperLeft.X <= static_cast<float>(static_cast<int32_t>(std::floor(cam->ViewWidth))) &&
        UpperLeft.Y <= static_cast<float>(static_cast<int32_t>(std::floor(cam->ViewHeight))))
    {
        return 1;
    }

    return result;
}

auto MCVfxAppearance::Render(int32_t depthFixup) -> int32_t
{
    MCGameObject* obj = Owner;
    ScreenPos = obj->GetScreenPos(Eye->CameraId - 1);

    // The facing in degrees, negative to the right.
    const MCFrameOfRef frame = obj->GetFrame();
    float cosFacing = UnitX.X * frame.I.X + UnitX.Y * frame.I.Y + UnitX.Z * frame.I.Z;

    if (cosFacing < -1.0)
    {
        cosFacing = -1.0f;
    }

    if (cosFacing > 1.0)
    {
        cosFacing = 1.0f;
    }

    double facing = AcosMatherr(static_cast<double>(cosFacing)) * 0x1.ca5dc1a6402aap+5;

    if (frame.I.Y < 0.0)
    {
        facing = -facing;
    }

    const uint8_t subState = CurrentSubState;
    float frameRate = 0.0f;
    int reverse = 0;
    MCShape* shape =
        AppearType->GetShape(CurrentState, subState, static_cast<int32_t>(facing), CurrentFrame, frameRate, reverse);
    CurrentShape = shape;

    if (obj->IsCaptured() != 0 && Highlighted == 0 && Highlighting == 0)
    {
        Highlighting = 1;
        HighlightTime = 3.0f;
    }

    const float scale = ZoomScale(Eye);
    int32_t offsetX = 0;
    int32_t offsetY = 0;

    if (PixelOffset(obj, offsetX, offsetY))
    {
        ScreenPos.X = static_cast<float>(offsetX) * scale + ScreenPos.X;
        ScreenPos.Y = static_cast<float>(offsetY) * scale + ScreenPos.Y;
    }

    MCVfxAppearanceType* type = AppearType;
    uint32_t numFrames = type->ActorStateData[CurrentState].NumFrames;

    if (subState != 0xff)
    {
        numFrames = type->ActorSubStateData[subState].NumFrames;
    }

    const int32_t depth = static_cast<int32_t>(static_cast<float>(depthFixup) - ScreenPos.Y);

    if (type->Delta != 0 && static_cast<int32_t>(numFrames) >= 2)
    {
        // Delta-compressed animations draw unscaled (only when the type is scaled).
        if (shape != nullptr && shape->FrameList != nullptr && type->Scaled != 0)
        {
            ElementList->OpenGroup(depth, 1);
            auto* element = MCElementPool::Make<MCDeltaElement>(shape->FrameList, static_cast<int32_t>(ScreenPos.X),
                                                                static_cast<int32_t>(ScreenPos.Y), CurrentFrame, 0,
                                                                FadeTable, 1, 0);
            ElementList->Add(element);
        }
    }
    else if (shape != nullptr && shape->FrameList != nullptr)
    {
        ElementList->OpenGroup(depth, 1);
        // Scaled types have their own zoomed out shapes, so they draw unscaled.
        const int noScaleDraw = type->Scaled != 0 ? 1 : 0;
        auto* element = MCElementPool::Make<MCVfxElement>(shape->FrameList, ScreenPos.X, ScreenPos.Y, CurrentFrame, 0,
                                                          FadeTable, noScaleDraw, 0);

        // Port fix: the original copies the debug name through a null element too.
        if (element != nullptr)
        {
            strcpy(element->Name, noScaleDraw != 0 ? "actor1" : "actor2");
        }

        ElementList->Add(element);
    }

    if (Owner->Selected == 1 || Owner->GetNumAttackers() > 0)
    {
        DrawBars();
    }

    if (Highlighting != 0)
    {
        if (HighlightTime <= 0.0f)
        {
            Highlighted = 1;
        }
        else
        {
            HighlightTime -= FrameLength;
            DrawSelectBox(0xfc);
        }
    }

    obj = Owner;

    if (obj->Selected != 0)
    {
        const int32_t homeAlignment = HomeTeam->Alignment;

        if (obj->GetAlignment() == homeAlignment || obj->GetAlignment() == 0 || static_cast<uint8_t>(obj->Status) == 2)
        {
            DrawSelectBox(0xe1);
        }
        else
        {
            DrawSelectBrackets(0xcf);
        }
    }

    if (DrawTerrainGrid != 0)
    {
        RecalcBounds(Eye);
        DrawSelectBox(0xe1);
    }

    return 0;
}

auto MCVfxAppearance::SetDamageLvl(uint32_t damageLevel) -> void
{
    DamageSet = 1;

    if (damageLevel == 0)
    {
        SetTypeId(ACTOR_STATE_NORMAL, 0xff);
        return;
    }

    const uint32_t blowUp1Frames = AppearType->ActorStateData[ACTOR_STATE_BLOWING_UP1].NumFrames;

    if (AppearType->ActorStateData[ACTOR_STATE_BLOWING_UP2].NumFrames + blowUp1Frames <= damageLevel)
    {
        SetTypeId(ACTOR_STATE_DESTROYED, 0xff);
        return;
    }

    if (blowUp1Frames <= damageLevel)
    {
        SetTypeId(ACTOR_STATE_BLOWING_UP2, 0xff);
        CurrentFrame = static_cast<int32_t>(damageLevel - blowUp1Frames);
        return;
    }

    SetTypeId(ACTOR_STATE_BLOWING_UP1, 0xff);
    CurrentFrame = static_cast<int32_t>(damageLevel);
}

auto MCVfxAppearance::Update() -> int32_t
{
    const int32_t startFrame = CurrentFrame;

    if (startFrame == -1)
    {
        CurrentFrame = 0;
    }

    MCVfxAppearanceType* type = AppearType;
    const uint8_t subState = CurrentSubState;
    FramesAdvanced = 0.0f;
    const MCActorData& data = type->ActorStateData[CurrentState];
    uint32_t numFrames = data.NumFrames;

    if (subState != 0xff)
    {
        numFrames = type->ActorSubStateData[subState].NumFrames;
    }

    if (numFrames <= 1)
    {
        return 1;
    }

    // Faithful: the state's frame rate, even in a sub-state.
    CurrentTime = FrameLength + CurrentTime;
    const float frames = CurrentTime * data.FrameRate;
    const int32_t played = LastFrame;

    if (frames < static_cast<float>(played))
    {
        Fatal(played, " Frame Count wrong", nullptr);
    }

    const int32_t wholeFrames = static_cast<int32_t>(std::floor(static_cast<double>(frames)));

    if (played < wholeFrames)
    {
        FramesAdvanced = static_cast<float>(wholeFrames - played);
        LastFrame = wholeFrames;
    }

    if (FramesAdvanced == 0.0f)
    {
        return 1;
    }

    CurrentFrame = static_cast<int32_t>(static_cast<double>(static_cast<uint32_t>(CurrentFrame)) + FramesAdvanced);

    if (startFrame == -1 && type->Delta != 0)
    {
        CurrentFrame = 0;
    }

    uint32_t stateFrames = data.NumFrames;
    uint8_t loop = 1;

    if (subState != 0xff)
    {
        loop = type->ActorSubStateData[subState].Loop;
        stateFrames = type->ActorSubStateData[subState].NumFrames;
    }

    const uint32_t frameNum = static_cast<uint32_t>(CurrentFrame);

    if (stateFrames <= frameNum)
    {
        if (loop != 0 && LoopEnd == -1)
        {
            CurrentFrame = static_cast<int32_t>(frameNum % stateFrames);
            return 0;
        }

        if (loop == 0)
        {
            CurrentFrame = static_cast<int32_t>(stateFrames - 1);
            return 1;
        }
    }

    if (static_cast<uint32_t>(LoopEnd) <= frameNum)
    {
        CurrentFrame = LoopStart;
        return 0;
    }

    return 1;
}

auto MCVfxAppearance::Destroy() -> void
{
    AppearType->RemoveUsers(this);
    AppearanceTypeList->RemoveAppearance(AppearType);
}

auto MCVfxAppearance::StateExists(MCActorState state) -> int32_t
{
    if (static_cast<int32_t>(state) < static_cast<int32_t>(AppearType->NumStates) && state >= 0)
    {
        return static_cast<int32_t>(AppearType->ActorStateData[state].NumFrames);
    }

    return 0;
}

auto MCVfxAppearance::DrawBars() -> void
{
    // Only tree buildings get a damage bar here.
    MCGameObject* obj = Owner;
    const bool hasBar = obj->ObjectClass == TREEBUILDING && obj != nullptr;
    MCDrawDamageBar(this, GetAppearanceType(), hasBar ? obj : nullptr);
}

auto MCDrawDamageBar(MCAppearance* appearance, MCAppearanceType* type, MCGameObject* obj) -> void
{
    // The bar sits centred above the type's bounds (or the shape's, when the type has none).
    const MCVector2D& screenPos = appearance->ScreenPos;
    const MCVector2D& upperLeft = appearance->UpperLeft;
    const MCVector2D& lowerRight = appearance->LowerRight;
    float left;
    float top;
    float right;

    if (type == nullptr || (type->BoundsUpperLeftX == 0 && type->BoundsUpperLeftY == 0 &&
                            type->BoundsLowerRightX == 0 && type->BoundsLowerRightY == 0))
    {
        left = upperLeft.X;
        top = upperLeft.Y;
        right = lowerRight.X;
    }
    else
    {
        const int shift = Eye->CameraScale == 1 ? 1 : 0;
        left = static_cast<float>(type->BoundsUpperLeftX >> shift) + screenPos.X;
        top = static_cast<float>(type->BoundsUpperLeftY >> shift) + screenPos.Y;
        right = static_cast<float>(type->BoundsLowerRightX >> shift) + screenPos.X;
    }

    // Port: the bar is an overlay, on the screen over the view: it follows the sprite through the zoom, its size
    // doesn't change.
    left = MCOverlayX(left);
    top = MCOverlayY(top);
    right = MCOverlayX(right);
    const float scale = ZoomScale(Eye);
    const float gap = scale * 5.0f;
    const float barWidth = scale * 38.0f;
    const float barHeight = scale * 4.0f;
    const float barY = top - gap - barHeight - gap - barHeight;
    const float barX = (left + (right - left) * 0.5f) - barWidth * 0.5f;

    if (obj == nullptr)
    {
        return;
    }

    ElementList->OpenGroup(-50000, 1);
    MCPolyElementData data;
    data.NumVertices = 0;
    data.TextureMapOff = 0;
    data.Texture = nullptr;
    data.TextureWidth = 0;
    data.TextureHeight = 0;
    data.FadeTable = nullptr;
    data.Translate = 0;
    data.StatusBar = 1;
    data.BarColor = 0x101;
    data.Vertices[0].X = static_cast<int32_t>(barX - 1.0f);
    data.Vertices[0].Y = static_cast<int32_t>(barY - 1.0f);
    data.Vertices[1].X = static_cast<int32_t>(barX + barWidth + 1.0f);
    data.Vertices[1].Y = static_cast<int32_t>(barY + barHeight + 1.0f);

    // Tree building and building types both keep their damage level at +0x30.
    auto typeDmgLevel = [obj]() -> float
    {
        return static_cast<float>(
            static_cast<int32_t>(static_cast<MCTreeBuildingType*>(obj->GetObjectType())->DmgLevel));
    };

    float remaining = typeDmgLevel() - obj->GetDamage();

    if (remaining < 0.0f)
    {
        remaining = 0.0f;
    }

    const float ratio = remaining / typeDmgLevel();
    float barLength = ratio * barWidth;

    // A building that isn't quite destroyed shows at least one pixel.
    if (barLength < 1.0 && ratio > 0.001)
    {
        barLength = 1.0f;
    }

    data.BarPercent = static_cast<int32_t>(barLength);

    if (data.BarPercent > 0)
    {
        ElementList->Add(MCElementPool::Make<MCPolygonElement>(&data, -50000));
    }
}

auto MCVfxAppearance::SetTypeId(MCActorState state, uint8_t subState) -> void
{
    if (static_cast<int32_t>(state) < static_cast<int32_t>(AppearType->NumStates) && state >= 0)
    {
        CurrentState = state;
        CurrentFrame = -1;
        CurrentTime = 0.0f;
        LastFrame = 0;
        CurrentSubState = 0xff;

        if (state != ACTOR_STATE_NORMAL)
        {
            TypeChanged = 1;
            return;
        }
    }

    if (subState != 0xff && state == ACTOR_STATE_NORMAL && AppearType->ActorSubStateData != nullptr)
    {
        CurrentSubState = subState;
    }

    TypeChanged = 1;
}
