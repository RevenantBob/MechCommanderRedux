#include "stdafx.h"
#include "sprite/puactor.h"
#include "camera/camera.h"
#include "color/color.h"
#include "engine/ceglist.h"
#include "engine/cepoly.h"
#include "engine/cevfx.h"
#include "lib/MCFitIniFile.h"
#include "main/main.h"
#include "object/gate.h"
#include "object/gvehicl.h"
#include "object/team.h"
#include "object/turret.h"
#include "sprite/bactor.h"
#include "sprite/sprtmgr.h"
#include "sprite/vfxshape.h"
#include "terrain/terrain.h"
#include "vfx/vfxfuncs.h"

namespace
{
    constexpr int32_t NO_DATA_RAM = static_cast<int32_t>(0xeada000c);

    /// <summary>0.5 when the camera is zoomed out, else 1.</summary>
    auto ZoomScale(const MCCamera* cam) -> float
    {
        return cam->CameraScale == 1 ? 0.5f : 1.0f;
    }

    /// <summary>The pixel offset of a turret or gate (both at +0x8c/+0x90); false for other classes.</summary>
    auto PixelOffset(MCGameObject* obj, int32_t& offsetX, int32_t& offsetY) -> bool
    {
        if (obj->ObjectClass == TURRET)
        {
            offsetX = static_cast<MCTurret*>(obj)->TileOffsetX;
            offsetY = static_cast<MCTurret*>(obj)->TileOffsetY;
            return true;
        }

        if (obj->ObjectClass == GATE)
        {
            offsetX = static_cast<MCGate*>(obj)->PixelOffsetX;
            offsetY = static_cast<MCGate*>(obj)->PixelOffsetY;
            return true;
        }

        return false;
    }
}

//---------------------------------------------------------------------------
// PUAppearanceType
//---------------------------------------------------------------------------

auto MCPUAppearanceType::Init(MCFile* apprFile, uint32_t fileSize, uint32_t loadFlags) -> int32_t
{
    const int32_t result = LoadIniFile(apprFile, fileSize);

    if (result != 0)
    {
        return result;
    }

    KeepLoaded = static_cast<int32_t>(loadFlags);
    NumPackets = SpriteManager->GetNumShapes(AppearanceNum & 0xffffff);
    // Port fix: sized by the port's pointer size (the original: count * 4).
    ShapeList = static_cast<MCShape**>(
        SpriteManager->MallocDataRam(static_cast<uint32_t>(NumPackets) * static_cast<uint32_t>(sizeof(MCShape*))));

    if (ShapeList == nullptr)
    {
        return NO_DATA_RAM;
    }

    for (int32_t i = 0; i < NumPackets; i++)
    {
        ShapeList[i] = nullptr;
    }

    return 0;
}

auto MCPUAppearanceType::RemoveShape(MCShape* shape) -> void
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
        auto* appearance = static_cast<MCPUAppearance*>(user->User);

        if (appearance->CurrentShape == shape)
        {
            appearance->CurrentShape = nullptr;
        }
    }
}

auto MCPUAppearanceType::LoadIniFile(MCFile* apprFile, uint32_t fileSize) -> int32_t
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

    ActorStateData =
        static_cast<MCPUActorData*>(SpriteManager->MallocDataRam(MAX_PU_ACTOR_STATES * sizeof(MCPUActorData)));

    if (ActorStateData == nullptr)
    {
        return NO_DATA_RAM;
    }

    if ((result = iniFile.SeekBlock("States")) != 0)
    {
        return result;
    }

    uint8_t numStates = 0;

    if ((result = iniFile.ReadIdUChar("NumStates", numStates)) != 0)
    {
        return result;
    }

    if (numStates != MAX_PU_ACTOR_STATES)
    {
        return static_cast<int32_t>(0xeada000e);
    }

    if (iniFile.ReadIdUChar("Scaled", Scaled) != 0)
    {
        Scaled = 0;
    }

    for (int32_t i = 0; i < MAX_PU_ACTOR_STATES; i++)
    {
        char blockName[20];
        sprintf(blockName, "State%d", i);

        if ((result = iniFile.SeekBlock(blockName)) != 0)
        {
            return result;
        }

        uint8_t state = 0;

        if ((result = iniFile.ReadIdUChar("State", state)) != 0)
        {
            return result;
        }

        MCPUActorData& data = ActorStateData[i];
        data.State = static_cast<MCPUActorState>(state);

        if ((result = iniFile.ReadIdULong("NumFrames", data.NumFrames)) != 0)
        {
            return result;
        }

        if ((result = iniFile.ReadIdFloat("FrameRate", data.FrameRate)) != 0)
        {
            return result;
        }

        if ((result = iniFile.ReadIdULong("BasePacketNumber", data.BasePacketNumber)) != 0)
        {
            return result;
        }

        if ((result = iniFile.ReadIdUChar("NumRotations", data.NumRotations)) != 0)
        {
            return result;
        }
    }

    iniFile.Close();
    return 0;
}

auto MCPUAppearanceType::GetShape(MCPUActorState state, int32_t rotation, int32_t part, float& frameRate) -> MCShape*
{
    const MCPUActorData& data = ActorStateData[state];

    if (data.NumFrames == 0)
    {
        return nullptr;
    }

    if (rotation < 0)
    {
        rotation += 360;
    }

    const uint32_t numRotations = data.NumRotations;
    frameRate = data.FrameRate;
    const int32_t rotationIndex = static_cast<int16_t>(static_cast<int32_t>(std::floor(
        static_cast<double>(static_cast<int32_t>(numRotations * static_cast<uint32_t>(rotation))) * (1.0 / 360.0))));
    uint32_t basePacket = data.BasePacketNumber;

    if (part > 0)
    {
        basePacket += numRotations * static_cast<uint32_t>(part);
    }

    uint32_t packet = basePacket + static_cast<uint32_t>(rotationIndex);
    float zoom = 1.0f;

    if (Eye != nullptr && Eye->CameraScale == 1)
    {
        zoom = 0.5f;
    }

    // Scaled types keep their zoomed out rotations after the full size ones.
    if (zoom != 1.0f && Scaled != 0)
    {
        packet += numRotations;
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

auto MCPUAppearanceType::Destroy() -> void
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
    SpriteManager->FreeDataRam(ActorStateData);
    ActorStateData = nullptr;
}

//---------------------------------------------------------------------------
// PUAppearance
//---------------------------------------------------------------------------

auto MCPUAppearance::Init(MCAppearanceType* tree, MCGameObject* obj) -> int32_t
{
    Visible = 0;
    Owner = obj;
    AppearType = static_cast<MCPUAppearanceType*>(tree);

    if (tree != nullptr)
    {
        tree->AddUsers(this);
    }

    CurrentShape = nullptr;
    ShapeMinY = -25.0f;
    ShapeMinX = -25.0f;
    Visible = 0;
    CurrentTime = 0.0f;
    LastFrame = 0;
    CurrentState = PU_ACTOR_STATE_CLOSED;
    InView = 0;
    Rotation = 0.0f;
    CurrentFrame = -1;
    ShapeMaxY = 50.0f;
    ShapeMaxX = 50.0f;
    FrameRate = 15.0f;
    return 0;
}

auto MCPUAppearance::RecalcBounds(MCCamera* cam) -> int
{
    if (cam == nullptr)
    {
        return 0;
    }

    MCGameObject* obj = Owner;
    const MCVector2D pos = obj->GetScreenPos(cam->CameraId - 1);
    float x = pos.X;
    float y = pos.Y;
    int32_t offsetX = 0;
    int32_t offsetY = 0;

    if (PixelOffset(obj, offsetX, offsetY))
    {
        const float camScale = ZoomScale(cam);
        x = static_cast<float>(offsetX) * camScale + x;
        y = static_cast<float>(offsetY) * camScale + y;
    }

    UpperLeft.X = x;
    UpperLeft.Y = y;
    LowerRight.Y = y;
    LowerRight.X = x;

    // The shape's bounds are taken once, from the first shape seen.
    if (CurrentShape != nullptr && CurrentShape->FrameList != nullptr && InView == 0)
    {
        uint8_t* shapeTable = CurrentShape->FrameList;
        int32_t frame = CurrentFrame;

        if (frame < 0)
        {
            frame = 0;
        }

        const int32_t numShapeFrames = VfxShapeCount(shapeTable);

        if (numShapeFrames <= frame)
        {
            CurrentFrame = numShapeFrames - 1;
            // Port fix: the original clamps only the object's frame and measures past the shape table.
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

    const float scale = ZoomScale(Eye);
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

    return 0;
}

auto MCPUAppearance::Render(int32_t depthFixup) -> int32_t
{
    MCGameObject* obj = Owner;
    ScreenPos = obj->GetScreenPos(Eye->CameraId - 1);
    const float scale = ZoomScale(Eye);
    int32_t offsetX = 0;
    int32_t offsetY = 0;

    if (PixelOffset(obj, offsetX, offsetY))
    {
        ScreenPos.X = static_cast<float>(offsetX) * scale + ScreenPos.X;
        ScreenPos.Y = static_cast<float>(offsetY) * scale + ScreenPos.Y;
    }

    if (obj->ObjectClass == GROUNDVEHICLE)
    {
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

        Rotation = static_cast<float>(facing + static_cast<MCGroundVehicle*>(obj)->TurretRotation);
    }
    else if (obj->ObjectClass == TURRET)
    {
        Rotation = static_cast<MCTurret*>(obj)->TurretRotation;
    }

    if (Rotation < 0.0)
    {
        Rotation = static_cast<float>(Rotation + 360.0);
    }

    // Snap to the nearest drawn rotation.
    const MCPUActorState state = CurrentState;
    const int32_t numRotations = AppearType->ActorStateData[state].NumRotations + 1;
    const int32_t rotationIndex = static_cast<int16_t>(static_cast<int32_t>(
        std::floor(static_cast<double>(static_cast<float>(numRotations) * Rotation) * (1.0 / 360.0))));
    Rotation = static_cast<float>(360.0 / numRotations) * static_cast<float>(rotationIndex);
    CurrentShape = AppearType->GetShape(state, static_cast<int32_t>(Rotation), 0, FrameRate);

    ElementList->OpenGroup(static_cast<int32_t>(static_cast<float>(depthFixup) - ScreenPos.Y), 0);

    if (DrawTerrainGrid != 0)
    {
        DrawSelectBox(0xfd);
    }

    if (CurrentShape != nullptr && CurrentShape->FrameList != nullptr)
    {
        uint8_t* fadeTable = nullptr;

        if (FadeTableIndex != -1 && FadeTableIndex >= 0)
        {
            fadeTable =
                GamePalette->FadePalettes.get() + (FadeTableIndex + GamePalette->NumBitmapHazeLevels * 2) * 0x100;
        }

        ElementList->Add(MCElementPool::Make<MCVfxElement>(CurrentShape->FrameList, ScreenPos.X, ScreenPos.Y,
                                                           CurrentFrame, 0, fadeTable, 1, 0));
    }

    const int32_t selected = Owner->Selected;
    bool showBars = selected == -1 || selected == 1;

    if (!showBars)
    {
        if (selected == 2)
        {
            RecalcBounds(Eye);
            MCGameObject* selectedObj = Owner;
            const int32_t alignment = selectedObj->GetAlignment();

            if (alignment == -1)
            {
                DrawSelectBrackets(0xfd);
            }
            else if (alignment == 0)
            {
                DrawSelectBrackets(0xfe);
            }
            else if (alignment == 1)
            {
                DrawSelectBrackets(selectedObj->GetAlignment() == HomeTeam->Alignment ? 0xfc : 0xfb);
            }
        }

        showBars = Owner->GetNumAttackers() >= 1;
    }

    if (showBars)
    {
        RecalcBounds(Eye);
        DrawBars();
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

auto MCPUAppearance::SetDestroyed() -> void
{
    // Destroyed closed from closed, destroyed open from any other state.
    CurrentState =
        CurrentState != PU_ACTOR_STATE_CLOSED ? PU_ACTOR_STATE_DESTROYED_OPEN : PU_ACTOR_STATE_DESTROYED_CLOSED;
}

auto MCPUAppearance::SetCombatMode(int combatMode) -> int32_t
{
    const MCPUActorData* states = AppearType->ActorStateData;
    const int32_t lastOpeningFrame = static_cast<int32_t>(states[PU_ACTOR_STATE_OPENING].NumFrames) - 1;
    const int32_t lastClosingFrame = static_cast<int32_t>(states[PU_ACTOR_STATE_CLOSING].NumFrames) - 1;

    // Finish an opening or closing that has played out.
    if (CurrentState == PU_ACTOR_STATE_OPENING || CurrentState == PU_ACTOR_STATE_CLOSING)
    {
        if (CurrentState == PU_ACTOR_STATE_CLOSING && CurrentFrame == lastClosingFrame)
        {
            CurrentState = PU_ACTOR_STATE_CLOSED;
            CurrentFrame = -1;
        }

        if (CurrentState == PU_ACTOR_STATE_OPENING && CurrentFrame == lastOpeningFrame)
        {
            CurrentState = PU_ACTOR_STATE_OPEN;
            CurrentFrame = -1;
        }
    }

    if (combatMode == 0)
    {
        if (CurrentState == PU_ACTOR_STATE_OPEN)
        {
            CurrentState = PU_ACTOR_STATE_CLOSING;
        }

        if (CurrentState == PU_ACTOR_STATE_CLOSING && CurrentFrame == lastClosingFrame)
        {
            CurrentState = PU_ACTOR_STATE_CLOSED;
            CurrentFrame = -1;
        }
    }
    else
    {
        if (CurrentState == PU_ACTOR_STATE_CLOSED)
        {
            CurrentState = PU_ACTOR_STATE_OPENING;
        }

        if (CurrentState == PU_ACTOR_STATE_OPENING && CurrentFrame == lastOpeningFrame)
        {
            CurrentState = PU_ACTOR_STATE_OPEN;
            CurrentFrame = -1;
        }

        if (CurrentState == PU_ACTOR_STATE_CLOSING)
        {
            // Reopen from the matching point of the opening.
            const double closedShare =
                static_cast<double>(static_cast<uint32_t>(CurrentFrame)) / static_cast<double>(lastClosingFrame);
            CurrentFrame = static_cast<int32_t>((1.0 - closedShare) *
                                                static_cast<double>(states[PU_ACTOR_STATE_OPENING].NumFrames));
            CurrentState = PU_ACTOR_STATE_OPENING;
            return 1;
        }
    }

    return CurrentState;
}

auto MCPUAppearance::Update() -> int32_t
{
    if (CurrentFrame == -1)
    {
        CurrentFrame = 0;
    }

    if (Visible != 0 && Owner->IsCaptured() != 0 && Highlighted == 0 && Highlighting == 0)
    {
        Highlighting = 1;
        HighlightTime = 3.0f;
    }

    CurrentTime = FrameLength + CurrentTime;
    const int32_t wholeFrames = static_cast<int32_t>(std::floor(static_cast<double>(CurrentTime * FrameRate)));

    if (LastFrame < wholeFrames)
    {
        const int32_t played = LastFrame;
        LastFrame = wholeFrames;
        const int32_t advanced = wholeFrames - played;

        if (advanced != 0)
        {
            const uint32_t frame = static_cast<uint32_t>(CurrentFrame + advanced);
            CurrentFrame = static_cast<int32_t>(frame);
            const uint32_t numFrames = AppearType->ActorStateData[CurrentState].NumFrames;

            if (numFrames <= frame)
            {
                CurrentFrame = static_cast<int32_t>(numFrames - 1);
                return 0;
            }
        }
    }

    return 1;
}

auto MCPUAppearance::Destroy() -> void
{
    AppearType->RemoveUsers(this);
    AppearanceTypeList->RemoveAppearance(AppearType);
}

auto MCPUAppearance::StateExists(MCPUActorState state) -> int32_t
{
    if (static_cast<int32_t>(state) < MAX_PU_ACTOR_STATES && state >= 0)
    {
        return static_cast<int32_t>(AppearType->ActorStateData[state].NumFrames);
    }

    return 0;
}

auto MCPUAppearance::DrawBars() -> void
{
    // Port: the bar is an overlay, on the screen over the view: it follows the sprite through the zoom, its size
    // doesn't change.
    const float scale = ZoomScale(Eye);
    const float barWidth = scale * 38.0f;
    const float barHeight = scale * 4.0f;
    const float barY = (MCOverlayY(UpperLeft.Y) - scale * 7.0f) - barHeight;
    const float barX = static_cast<float>(std::floor(static_cast<double>(MCOverlayX(ScreenPos.X) - barWidth * 0.5f)));

    // A turret shows its bar only with its weapon deployed.
    MCGameObject* obj = Owner;

    if (obj->ObjectClass == TURRET && static_cast<MCTurret*>(obj)->WeaponDeployed == 0)
    {
        return;
    }

    double health = 0.0; // Port fix: the original leaves this unset for other classes.
    const int32_t objectClass = obj->ObjectClass;

    if (objectClass == BATTLEMECH || objectClass == GROUNDVEHICLE || objectClass == ELEMENTAL || objectClass == MOVER)
    {
        auto* mover = static_cast<MCMover*>(obj);

        if (mover == nullptr)
        {
            return;
        }

        if (mover->WeaponEffectiveness < 0.0f || mover->MaxWeaponEffectiveness < mover->WeaponEffectiveness)
        {
            return;
        }

        health = mover->GetTotalEffectiveness();
    }
    else if (objectClass == TURRET || objectClass == GATE)
    {
        int32_t damage = static_cast<int32_t>(obj->GetDamage());
        // Turret and gate types both keep their damage level at +0x30.
        const int32_t dmgLevel = static_cast<int32_t>(static_cast<MCTurretType*>(obj->GetObjectType())->DmgLevel);

        if (dmgLevel < damage)
        {
            damage = dmgLevel;
        }

        health = 1.0 - static_cast<double>(damage) / static_cast<double>(dmgLevel);
    }

    // Green, then yellow below half, red at a fifth (palette colours here).
    int32_t barColor;

    if (health < 0.5)
    {
        barColor = health <= 0.2 ? 0xef : 0xf2;
    }
    else
    {
        barColor = 0x0b;
    }

    float barLength = static_cast<float>(health * barWidth);

    // A unit that isn't quite dead shows at least one pixel.
    if (health * barWidth < 1.0 && health > 0.001)
    {
        barLength = 1.0f;
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
    data.BarColor = barColor;
    data.Vertices[0].X = static_cast<int32_t>(barX - 1.0f);
    data.Vertices[0].Y = static_cast<int32_t>(barY - 1.0f);
    data.Vertices[1].X = static_cast<int32_t>(barX + barWidth + 1.0f);
    data.Vertices[1].Y = static_cast<int32_t>(barY + barHeight + 1.0f);
    data.BarPercent = static_cast<int32_t>(barLength);

    if (data.BarPercent > 0)
    {
        ElementList->Add(MCElementPool::Make<MCPolygonElement>(&data, -50000));
    }
}
