#include "stdafx.h"
#include "sprite/gvactor.h"
#include "camera/camera.h"
#include "color/color.h"
#include "engine/ceglist.h"
#include "engine/cepoly.h"
#include "engine/cevfx.h"
#include "lib/inifile.h"
#include "main/main.h"
#include "object/artlry.h"
#include "object/gvehicl.h"
#include "object/team.h"
#include "object/train.h"
#include "object/turret.h"
#include "sprite/bactor.h"
#include "sprite/sprtmgr.h"
#include "sprite/vfxshape.h"
#include "terrain/terrain.h"
#include "vfx/vfxfuncs.h"

namespace
{
    constexpr int32_t NO_DATA_RAM = static_cast<int32_t>(0xeada000c);

    /// <summary>The facing of <paramref name="obj"/> in degrees, negative to the right.</summary>
    auto ObjectFacing(MCGameObject* obj) -> double
    {
        const MCFrameOfRef frame = obj->GetFrame();
        float cosFacing = UnitX.Z * frame.I.Z + UnitX.X * frame.I.X + UnitX.Y * frame.I.Y;

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

        return facing;
    }

    /// <summary>0.5 when the eye is zoomed out, else 1.</summary>
    auto EyeScale() -> float
    {
        return Eye->CameraScale == 1 ? 0.5f : 1.0f;
    }
}

//---------------------------------------------------------------------------
// GVAppearanceType
//---------------------------------------------------------------------------

auto MCGVAppearanceType::Init(MCFile* apprFile, uint32_t fileSize, uint32_t loadFlags) -> int32_t
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

auto MCGVAppearanceType::RemoveShape(MCShape* shape) -> void
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
        auto* appearance = static_cast<MCGVAppearance*>(user->User);

        if (appearance->CurrentShape[0] == shape)
        {
            appearance->CurrentShape[0] = nullptr;
        }

        if (appearance->CurrentShape[1] == shape)
        {
            appearance->CurrentShape[1] = nullptr;
        }
    }
}

auto MCGVAppearanceType::LoadIniFile(MCFile* apprFile, uint32_t fileSize) -> int32_t
{
    MCFitIniFile iniFile;
    int32_t result = iniFile.Open(apprFile, fileSize, 50);

    if (result != 0)
    {
        return result;
    }

    if ((result = iniFile.SeekBlock("Main Info")) != 0)
    {
        return result;
    }

    if ((result = iniFile.ReadIdULong("NumParts", NumParts)) != 0)
    {
        return result;
    }

    if ((result = iniFile.ReadIdFloat("TurretOffset", TurretOffset)) != 0)
    {
        return result;
    }

    ActorStateData =
        static_cast<MCGVActorData*>(SpriteManager->MallocDataRam(MAX_GV_ACTOR_STATES * sizeof(MCGVActorData)));

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

    // Normal, damaged and destroyed, and optionally an extra state.
    if (numStates == 4)
    {
        HasExtraState = 1;
    }
    else if (numStates == 3)
    {
        HasExtraState = 0;
    }
    else
    {
        return static_cast<int32_t>(0xeada000e);
    }

    for (int32_t i = 0; i < numStates; i++)
    {
        char blockName[20];
        sprintf(blockName, "State%d", i);

        if ((result = iniFile.SeekBlock(blockName)) != 0)
        {
            return result;
        }

        MCGVActorData& data = ActorStateData[i];
        uint8_t state = 0;

        if ((result = iniFile.ReadIdUChar("State", state)) != 0)
        {
            return result;
        }

        data.State = static_cast<MCGVActorState>(state);

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

auto MCGVAppearanceType::GetShape(MCGVActorState state, int32_t rotation, int32_t part, float& frameRate) -> MCShape*
{
    if (static_cast<int32_t>(NumParts) <= part)
    {
        return nullptr;
    }

    const MCGVActorData& data = ActorStateData[state];

    if (data.NumFrames == 0)
    {
        return nullptr;
    }

    // Into 0..360 (the original's unsigned divisions; the operands are positive).
    if (rotation > 180)
    {
        rotation -= static_cast<int32_t>((static_cast<uint32_t>(rotation) + 179u) / 360u) * 360;
    }

    if (rotation < -180)
    {
        rotation += static_cast<int32_t>((179u - static_cast<uint32_t>(rotation)) / 360u) * 360;
    }

    if (rotation < 0)
    {
        rotation += 360;
    }

    const uint32_t numRotations = data.NumRotations;
    frameRate = data.FrameRate;
    const int32_t rotationIndex = static_cast<int16_t>(static_cast<int32_t>(std::floor(
        static_cast<double>(static_cast<int32_t>(numRotations * static_cast<uint32_t>(rotation))) * (1.0 / 360.0))));
    // The turret's rotations follow the body's.
    uint32_t basePacket = data.BasePacketNumber;

    if (part > 0)
    {
        basePacket += numRotations * static_cast<uint32_t>(part);
    }

    const uint32_t packet = basePacket + static_cast<uint32_t>(rotationIndex);

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
    // Faithful: always asks for the zoomed out version (the sprite manager ignores it).
    shape = SpriteManager->GetShapeData(AppearanceNum & 0xffffff, packet, Turn, this, 1);
    ShapeList[packet] = shape;
    return shape;
}

auto MCGVAppearanceType::Destroy() -> void
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
// GVAppearance
//---------------------------------------------------------------------------

auto MCGVAppearance::Init(MCAppearanceType* tree, MCGameObject* obj) -> int32_t
{
    Visible = 0;
    Owner = obj;
    AppearType = static_cast<MCGVAppearanceType*>(tree);

    if (tree != nullptr)
    {
        NumParts = static_cast<int32_t>(AppearType->NumParts);
        TurretOffset = AppearType->TurretOffset;
        tree->AddUsers(this);
    }

    for (int32_t i = 0; i < 2; i++)
    {
        CurrentShape[i] = nullptr;
        CurrentFrame[i] = -1;
    }

    Visible = 0;
    ShapeMinY = -25.0f;
    ShapeMinX = -25.0f;
    CurrentTime = 0.0f;
    LastFrame = 0;
    CurrentState = GV_ACTOR_STATE_NORMAL;
    InView = 0;
    BodyRotation = 0.0f;
    ShapeMaxY = 50.0f;
    ShapeMaxX = 50.0f;
    FrameRate = 15.0f;
    FadeTableIndex = -1;
    return 0;
}

auto MCGVAppearance::RecalcBounds(MCCamera* cam) -> int
{
    if (cam == nullptr)
    {
        return 0;
    }

    MCGameObject* obj = Owner;
    const MCVector2D pos = obj->GetScreenPos(cam->CameraId - 1);
    float x = pos.X;
    float y = pos.Y;

    if (obj->ObjectClass == TURRET)
    {
        const float camScale = cam->CameraScale == 1 ? 0.5f : 1.0f;
        x = static_cast<float>(static_cast<MCTurret*>(obj)->TileOffsetX) * camScale + x;
        y = static_cast<float>(static_cast<MCTurret*>(obj)->TileOffsetY) * camScale + y;
    }

    UpperLeft.X = x;
    UpperLeft.Y = y;
    LowerRight.Y = y;
    LowerRight.X = x;

    if (CurrentShape[0] != nullptr && CurrentShape[0]->FrameList != nullptr)
    {
        // The bounds only ever grow: they cover every body frame drawn so far.
        uint8_t* shapeTable = CurrentShape[0]->FrameList;
        const int32_t numShapeFrames = VfxShapeCount(shapeTable);
        int32_t frame = CurrentFrame[0];

        if (frame < 0)
        {
            frame = 0;
        }

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

    // Faithful: the zoom is the eye's, the screen limits the camera's (not rounded down here).
    const float scale = EyeScale();
    UpperLeft.X = scale * ShapeMinX + x;
    UpperLeft.Y = scale * ShapeMinY + y;
    LowerRight.X = scale * ShapeMaxX + UpperLeft.X;
    LowerRight.Y = scale * ShapeMaxY + UpperLeft.Y;

    if (0.0f <= LowerRight.X && 0.0f <= LowerRight.Y && UpperLeft.X <= cam->ViewWidth && UpperLeft.Y <= cam->ViewHeight)
    {
        return 1;
    }

    return 0;
}

auto MCGVAppearance::Render(int32_t depthFixup) -> int32_t
{
    MCGameObject* obj = Owner;
    ScreenPos = obj->GetScreenPos(Eye->CameraId - 1);
    const int32_t objectClass = obj->ObjectClass;
    const float scale = EyeScale();

    if (objectClass == TURRET)
    {
        ScreenPos.X = static_cast<float>(static_cast<MCTurret*>(obj)->TileOffsetX) * scale + ScreenPos.X;
        ScreenPos.Y = static_cast<float>(static_cast<MCTurret*>(obj)->TileOffsetY) * scale + ScreenPos.Y;
    }

    // The body's and the turret's facings.
    if (objectClass == GROUNDVEHICLE)
    {
        const double body = ObjectFacing(obj) + 5.0;
        BodyRotation = static_cast<float>(body);
        TurretRotation = static_cast<float>(body + static_cast<MCGroundVehicle*>(obj)->TurretRotation);
    }
    else if (objectClass == TURRET)
    {
        const float rotation = static_cast<MCTurret*>(obj)->TurretRotation;
        TurretRotation = rotation;
        BodyRotation = rotation;
    }
    else if (objectClass == TRAINCAR)
    {
        BodyRotation = static_cast<float>(ObjectFacing(obj) + 10.0);
        TurretRotation = 0.0f;
    }
    else if (objectClass == CAMERADRONE)
    {
        const double facing = ObjectFacing(obj);
        BodyRotation = static_cast<float>(facing);
        TurretRotation = 0.0f;
        // Faithful: 45 and -134 swap (-135 is left as it is).
        int32_t whole = static_cast<int32_t>(facing);

        if (whole == 45)
        {
            whole = -135;
        }
        else if (whole == -134)
        {
            whole = 45;
        }

        BodyRotation = static_cast<float>(whole);
    }
    while (TurretRotation > 180.0f)
    {
        TurretRotation -= 360.0f;
    }
    while (TurretRotation < -180.0f)
    {
        TurretRotation += 360.0f;
    }

    if (TurretRotation < 0.0f)
    {
        TurretRotation += 360.0f;
    }

    // The body snaps to its nearest drawn rotation.
    const MCGVActorState state = CurrentState;
    const int32_t numRotations = AppearType->ActorStateData[state].NumRotations + 1;
    const int32_t rotationIndex = static_cast<int16_t>(
        static_cast<int32_t>(std::floor(static_cast<double>(numRotations) * BodyRotation * (1.0 / 360.0))));
    BodyRotation = static_cast<float>(360.0 / numRotations * rotationIndex);
    CurrentShape[0] = AppearType->GetShape(state, static_cast<int32_t>(BodyRotation), 0, FrameRate);

    if (static_cast<uint32_t>(NumParts) < 2 || CurrentState == GV_ACTOR_STATE_DESTROYED)
    {
        CurrentShape[1] = nullptr;
    }
    else
    {
        CurrentShape[1] = AppearType->GetShape(CurrentState, static_cast<int32_t>(TurretRotation), 1, FrameRate);
    }

    ElementList->OpenGroup(static_cast<int32_t>(static_cast<float>(depthFixup) - ScreenPos.Y), 0);
    PartOrder[0] = 0;
    PartOrder[1] = 1;

    for (int32_t i = 0; i < NumParts; i++)
    {
        const int32_t part = PartOrder[i];

        if (CurrentShape[part] == nullptr || CurrentShape[part]->FrameList == nullptr)
        {
            continue;
        }

        float offsetX = 0.0f;
        float offsetY = 0.0f;

        if (part == 1)
        {
            // The turret sits turretOffset meters forward of the body's centre.
            const double distance = static_cast<double>(EyeScale()) * TurretOffset * WorldUnitsPerMeter;
            offsetX = static_cast<float>(distance * std::sin(BodyRotation * 0x1.1df46a2526c7ap-6));
            offsetY = static_cast<float>(distance * std::cos(BodyRotation * 0x1.1df46a2526c7ap-6) * 0.5);
        }

        uint8_t* fadeTable = nullptr;

        if (FadeTableIndex != -1 && FadeTableIndex >= 0)
        {
            fadeTable =
                GamePalette->FadePalettes.get() + (FadeTableIndex + GamePalette->NumBitmapHazeLevels * 2) * 0x100;
        }

        auto* element =
            MCElementPool::Make<MCVfxElement>(CurrentShape[part]->FrameList, ScreenPos.X - offsetX,
                                              ScreenPos.Y - offsetY, CurrentFrame[part], 0, fadeTable, 0, 0);

        // Port fix: the original copies the debug name through a null element too.
        if (element != nullptr)
        {
            strcpy(element->Name, "gvactor");
        }

        ElementList->Add(element);
    }

    const int32_t selected = Owner->Selected;

    if (selected == -1 || selected == 1)
    {
        RecalcBounds(Eye);
        DrawBars();
    }
    else
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

        if (Owner->GetNumAttackers() >= 1)
        {
            RecalcBounds(Eye);
            DrawBars();
        }
    }

    if (DrawTerrainGrid != 0)
    {
        RecalcBounds(Eye);
        DrawSelectBox(0xfd);
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

auto MCGVAppearance::Update() -> int32_t
{
    for (int32_t i = 0; i < NumParts; i++)
    {
        if (CurrentFrame[i] == -1)
        {
            CurrentFrame[i] = 0;
        }
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
            // The turret shows the body's frame.
            const uint32_t frame = static_cast<uint32_t>(CurrentFrame[0] + advanced);
            CurrentFrame[0] = static_cast<int32_t>(frame);
            const uint32_t numFrames = AppearType->ActorStateData[CurrentState].NumFrames;

            if (numFrames <= frame)
            {
                CurrentFrame[0] = static_cast<int32_t>(frame % numFrames);
                return 0;
            }

            CurrentFrame[1] = static_cast<int32_t>(frame);
        }
    }

    return 1;
}

auto MCGVAppearance::Destroy() -> void
{
    AppearType->RemoveUsers(this);
    AppearanceTypeList->RemoveAppearance(AppearType);
}

auto MCGVAppearance::StateExists(MCGVActorState state) -> int32_t
{
    const int32_t numStates = (AppearType->HasExtraState != 0 ? 1 : 0) + 3;

    if (static_cast<int32_t>(state) < numStates && state >= 0)
    {
        return static_cast<int32_t>(AppearType->ActorStateData[state].NumFrames);
    }

    return 0;
}

auto MCGVAppearance::DrawBars() -> void
{
    // Port: the bar is an overlay, on the screen over the view: it follows the sprite through the zoom, its size
    // doesn't change.
    const float scale = EyeScale();
    const float barWidth = scale * 38.0f;
    const float barHeight = scale * 4.0f;
    const float barY = (MCOverlayY(UpperLeft.Y) - scale * 6.0f) - barHeight;
    const float barX = static_cast<float>(std::floor(static_cast<double>(MCOverlayX(ScreenPos.X) - barWidth * 0.5f)));

    // How much of the unit is left, per class.
    MCGameObject* obj = Owner;
    float health = 0.0f; // Port fix: the original leaves this unset for other classes.
    const int32_t objectClass = obj->ObjectClass;

    if (objectClass == BATTLEMECH || objectClass == GROUNDVEHICLE || objectClass == ELEMENTAL || objectClass == MOVER)
    {
        health = static_cast<MCMover*>(obj)->GetTotalEffectiveness();
        auto* vehicle = static_cast<MCGroundVehicle*>(obj);

        if (obj->ObjectClass == GROUNDVEHICLE && vehicle->Refitter != 0)
        {
            // A refit vehicle shows the refit points left against the turret's full armor.
            const float capacity = vehicle->Refitter == 0
                                       ? 0.0f
                                       : static_cast<float>(vehicle->Armor[GROUNDVEHICLE_LOCATION_TURRET].MaxArmor);
            health = static_cast<float>(vehicle->GetRefitPoints() / capacity * health);
        }
    }
    else if (objectClass == TURRET)
    {
        int32_t damage = static_cast<int32_t>(obj->GetDamage());
        const int32_t dmgLevel = static_cast<int32_t>(static_cast<MCTurretType*>(obj->GetObjectType())->DmgLevel);

        if (dmgLevel < damage)
        {
            damage = dmgLevel;
        }

        health = 1.0f - static_cast<float>(damage) / static_cast<float>(dmgLevel);
    }
    else if (objectClass == TRAINCAR)
    {
        int32_t damage = static_cast<int32_t>(obj->GetDamage());
        const int32_t dmgLevel = static_cast<MCTrainCarType*>(obj->GetObjectType())->Damage;

        if (dmgLevel < damage)
        {
            damage = dmgLevel;
        }

        // Original behaviour (OB-054): the damage taken, not the health left.
        health = static_cast<float>(damage) / static_cast<float>(dmgLevel);
    }
    else if (objectClass == CAMERADRONE)
    {
        health = 1.0f;

        if (static_cast<MCCameraDrone*>(obj)->HitPoints < 1)
        {
            health = 0.0f;
        }
    }

    // Green, then yellow below half, red at a fifth.
    int32_t barColor;

    if (health < 0.5)
    {
        barColor = health <= 0.2 ? 0x103 : 0x102;
    }
    else
    {
        barColor = 0x101;
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
    float barLength = health * barWidth;

    // A unit that isn't quite dead shows at least one pixel.
    if (barLength < 1.0 && health > 0.001)
    {
        barLength = 1.0f;
    }

    data.BarPercent = static_cast<int32_t>(barLength);

    if (data.BarPercent > 0)
    {
        ElementList->Add(MCElementPool::Make<MCPolygonElement>(&data, -50000));
    }
}
