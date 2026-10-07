#include "stdafx.h"
#include "sprite/armactor.h"
#include "camera/camera.h"
#include "color/color.h"
#include "engine/ceglist.h"
#include "engine/cevfx.h"
#include "lib/MCFitIniFile.h"
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

auto MCArmAppearanceType::Init(MCFile* apprFile, uint32_t fileSize, uint32_t loadFlags) -> int32_t
{
    const int32_t result = LoadIniFile(apprFile, fileSize);

    if (result != 0)
    {
        return result;
    }

    NumPackets = SpriteManager->GetNumShapes(AppearanceNum & 0xffffff);
    // Port fix: sized by the port's pointer size (the original: count * 4).
    ShapeList = static_cast<MCShape**>(
        SpriteManager->MallocDataRam(static_cast<uint32_t>(NumPackets) * static_cast<uint32_t>(sizeof(MCShape*))));

    if (ShapeList == nullptr)
    {
        return static_cast<int32_t>(0xeada0016);
    }

    for (int32_t i = 0; i < NumPackets; i++)
    {
        ShapeList[i] = nullptr;
    }

    KeepLoaded = static_cast<int32_t>(loadFlags);

    if (loadFlags != 0)
    {
        PreloadGestures();
    }

    return 0;
}

auto MCArmAppearanceType::RemoveShape(MCShape* shape) -> void
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
        auto* appearance = static_cast<MCArmAppearance*>(user->User);

        if (appearance->CurrentShape == shape)
        {
            appearance->CurrentShape = nullptr;
        }
    }
}

auto MCArmAppearanceType::PreloadGestures() -> void
{
    for (int32_t i = 0; i < NumPackets; i++)
    {
        ShapeList[i] = SpriteManager->GetShapeData(AppearanceNum & 0xffffff, static_cast<uint32_t>(i), 0, this, 0);
    }
}

auto MCArmAppearanceType::LoadIniFile(MCFile* apprFile, uint32_t fileSize) -> int32_t
{
    MCFitIniFile iniFile;
    int32_t result = iniFile.Open(apprFile, fileSize);

    if (result != 0)
    {
        return result;
    }

    ActorData = static_cast<MCArmActorData*>(SpriteManager->MallocDataRam(sizeof(MCArmActorData)));

    if (ActorData == nullptr)
    {
        return static_cast<int32_t>(0xeada0016);
    }

    if ((result = iniFile.SeekBlock("State")) != 0)
    {
        return result;
    }

    if ((result = iniFile.ReadIdULong("NumFrames", ActorData->NumFrames)) != 0)
    {
        return result;
    }

    if ((result = iniFile.ReadIdFloat("FrameRate", ActorData->FrameRate)) != 0)
    {
        return result;
    }

    if ((result = iniFile.ReadIdULong("BasePacketNumber", ActorData->BasePacketNumber)) != 0)
    {
        return result;
    }

    if ((result = iniFile.ReadIdUChar("NumRotations", ActorData->NumRotations)) != 0)
    {
        return result;
    }

    if ((result = iniFile.ReadIdULong("Symmetrical", ActorData->Symmetrical)) != 0)
    {
        return result;
    }

    if (iniFile.ReadIdUChar("CheckForHeader", CheckForHeader) != 0)
    {
        CheckForHeader = 1;
    }

    iniFile.Close();
    return 0;
}

auto MCArmAppearanceType::GetShape(int32_t rotation, int32_t, float& frameRate, int& reverse) -> MCShape*
{
    const MCArmActorData& data = *ActorData;

    if (data.NumFrames == 0)
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

    const bool symmetrical = data.Symmetrical != 0;

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

    frameRate = data.FrameRate;

    int32_t rotationIndex = static_cast<int16_t>(
        static_cast<int32_t>(std::floor(static_cast<double>((data.NumRotations + 1) * rotation) * (1.0 / 360.0))));

    if (rotationIndex > 0x1f && rotation > 180 && !symmetrical)
    {
        rotationIndex = 0x1f;
    }

    float zoom = 1.0f;

    if (Eye != nullptr && Eye->CameraScale == 1)
    {
        zoom = 0.5f;
    }

    const uint32_t packet = data.BasePacketNumber + static_cast<uint32_t>(rotationIndex);

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

auto MCArmAppearanceType::Destroy() -> void
{
    // The shapes stay in the sprite manager's cache, ownerless.
    for (int32_t i = 0; i < NumPackets; i++)
    {
        if (ShapeList[i] != nullptr)
        {
            ShapeList[i]->Owner = nullptr;
        }
    }

    SpriteManager->FreeDataRam(ShapeList);
    ShapeList = nullptr;
    SpriteManager->FreeDataRam(ActorData);
    ActorData = nullptr;
}

//---------------------------------------------------------------------------
// ArmAppearance
//---------------------------------------------------------------------------

auto MCArmAppearance::Init(MCAppearanceType* tree, MCGameObject* obj) -> int32_t
{
    Visible = 0;
    Owner = obj;
    AppearType = static_cast<MCArmAppearanceType*>(tree);

    if (tree != nullptr)
    {
        tree->AddUsers(this);
    }

    CurrentShape = nullptr;
    CurrentFrame = -1;
    FadeTableIndex = -1;
    ShapeMinY = -15.0f;
    ShapeMinX = -15.0f;
    Visible = 0;
    CurrentTime = 0.0f;
    LastFrame = 0;
    Reverse = 0;
    InView = 0;
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
    UpperLeft.X = pos.X;
    UpperLeft.Y = pos.Y;
    LowerRight.Y = pos.Y;
    LowerRight.X = pos.X;

    if (CurrentShape != nullptr && CurrentShape->FrameList != nullptr)
    {
        // The bounds only ever grow: they cover every frame drawn so far.
        uint8_t* shapeTable = CurrentShape->FrameList;
        int32_t frame = CurrentFrame;

        if (frame < 0)
        {
            frame = 0;
        }

        const int32_t numShapeFrames = VfxShapeCount(shapeTable);

        if (numShapeFrames <= frame)
        {
            frame = numShapeFrames - 1;
        }

        const int32_t minXY = VfxShapeMinxy(shapeTable, frame);

        if (static_cast<float>(minXY >> 16) < ShapeMinX)
        {
            ShapeMinX = static_cast<float>(minXY >> 16);
        }

        if (static_cast<float>(static_cast<int16_t>(minXY)) < ShapeMinY)
        {
            ShapeMinY = static_cast<float>(static_cast<int16_t>(minXY));
        }

        const int32_t size = VfxShapeResolution(shapeTable, frame);

        if (ShapeMaxX < static_cast<float>(size >> 16))
        {
            ShapeMaxX = static_cast<float>(size >> 16);
        }

        if (ShapeMaxY < static_cast<float>(static_cast<int16_t>(size)))
        {
            ShapeMaxY = static_cast<float>(static_cast<int16_t>(size));
        }

        InView = 1;
    }

    // Faithful: the zoom is the eye's, the screen limits the camera's.
    const float scale = Eye->CameraScale == 1 ? 0.5f : 1.0f;
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

    // The facing in degrees, negative to the right.
    const MCFrameOfRef frame = obj->GetFrame();
    float cosFacing = UnitX.Y * frame.I.Y + UnitX.X * frame.I.X + UnitX.Z * frame.I.Z;

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

    Rotation = static_cast<float>(facing);
    CurrentShape = AppearType->GetShape(static_cast<int32_t>(Rotation), 0, FrameRate, Reverse);

    if (DrawTerrainGrid != 0)
    {
        DrawSelectBox(0xfd);
    }

    uint8_t* fadeTable = nullptr;

    if (FadeTableIndex != -1 && FadeTableIndex >= 0)
    {
        fadeTable = GamePalette->FadePalettes.get() + (FadeTableIndex + GamePalette->NumBitmapHazeLevels * 2) * 0x100;
    }

    ElementList->OpenGroup(static_cast<int32_t>(static_cast<float>(depthFixup) - ScreenPos.Y), 1);

    if (CurrentShape == nullptr || CurrentShape->FrameList == nullptr)
    {
        return 0;
    }

    auto* element = MCElementPool::Make<MCVfxElement>(CurrentShape->FrameList, ScreenPos.X, ScreenPos.Y, CurrentFrame,
                                                      Reverse, fadeTable, 0, 0);

    // Port fix: the original writes the debug names through a null element too, and "%i" of a type number
    // can overrun name2.
    if (element != nullptr)
    {
        strcpy(element->Name, "armweap");

        if (OwnerObject == nullptr)
        {
            strcpy(element->Name2, "unknown");
        }
        else
        {
            snprintf(element->Name2, sizeof(element->Name2), "%i", OwnerObject->GetObjectType()->ObjTypeNum);
        }
    }

    ElementList->Add(element);
    return 0;
}

auto MCArmAppearance::Update() -> int32_t
{
    if (CurrentFrame == -1)
    {
        CurrentFrame = 0;
    }

    const int32_t played = LastFrame;
    const MCArmActorData& data = *AppearType->ActorData;
    FrameRate = data.FrameRate;
    CurrentTime = FrameLength + CurrentTime;
    const int32_t wholeFrames = static_cast<int32_t>(std::floor(static_cast<double>(CurrentTime * FrameRate)));

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

auto MCArmAppearance::Destroy() -> void
{
    AppearType->RemoveUsers(this);
    AppearanceTypeList->RemoveAppearance(AppearType);
}
