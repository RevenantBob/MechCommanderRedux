#include "stdafx.h"
#include "sprite/MCGVAppearance.h"
#include "appear/MCAppearanceTypeList.h"
#include "camera/camera.h"
#include "color/MCPalette.h"
#include "engine/MCElementBuffer.h"
#include "engine/MCPolygonElement.h"
#include "engine/MCVfxElement.h"
#include "main/main.h"
#include "object/artlry.h"
#include "object/gvehicl.h"
#include "object/team.h"
#include "object/train.h"
#include "object/turret.h"
#include "sprite/MCShape.h"
#include "sprite/MCSpriteMath.h"
#include "terrain/terrain.h"
#include "vfx/MCVfxFunctions.h"

MCGVAppearance::~MCGVAppearance()
{
    if (AppearType != nullptr)
    {
        AppearType->RemoveUser(this);
        AppearanceTypeList()->RemoveAppearance(AppearType);
    }
}

auto MCGVAppearance::Init(MCAppearanceType* tree, MCGameObject* obj) -> int32_t
{
    Owner = obj;
    AppearType = static_cast<MCGVAppearanceType*>(tree);

    if (tree != nullptr)
    {
        NumParts = static_cast<int32_t>(AppearType->NumParts);
        TurretOffset = AppearType->TurretOffset;
        tree->AddUser(this);
    }

    CurrentShape.fill(nullptr);
    CurrentFrame.fill(-1);
    Visible = false;
    ShapeMinY = -25.0f;
    ShapeMinX = -25.0f;
    CurrentTime = 0.0f;
    LastFrame = 0;
    CurrentState = MCGVActorState::Normal;
    InView = false;
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
        const float camScale = MCZoomScale(cam);
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
        const MCFrameBounds bounds = MCShapeFrameBounds(shapeTable, MCClampShapeFrame(shapeTable, CurrentFrame[0]));
        MCGrowBounds(bounds, ShapeMinX, ShapeMinY, ShapeMaxX, ShapeMaxY);
        InView = true;
    }

    // Faithful: the zoom is the eye's, the screen limits the camera's (not rounded down here).
    const float scale = MCZoomScale(Eye);
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
    const float scale = MCZoomScale(Eye);

    if (objectClass == TURRET)
    {
        ScreenPos.X = static_cast<float>(static_cast<MCTurret*>(obj)->TileOffsetX) * scale + ScreenPos.X;
        ScreenPos.Y = static_cast<float>(static_cast<MCTurret*>(obj)->TileOffsetY) * scale + ScreenPos.Y;
    }

    // The body's and the turret's facings.
    if (objectClass == GROUNDVEHICLE)
    {
        const double body = MCActorFacing(obj) + 5.0;
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
        BodyRotation = static_cast<float>(MCActorFacing(obj) + 10.0);
        TurretRotation = 0.0f;
    }
    else if (objectClass == CAMERADRONE)
    {
        TurretRotation = 0.0f;
        // Faithful: 45 and -134 swap (-135 is left as it is).
        auto whole = static_cast<int32_t>(MCActorFacing(obj));

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
    const int32_t numRotations = AppearType->StateData(state).NumRotations + 1;
    const int32_t rotationIndex = static_cast<int16_t>(
        static_cast<int32_t>(std::floor(static_cast<double>(numRotations) * BodyRotation * (1.0 / 360.0))));
    BodyRotation = static_cast<float>(360.0 / numRotations * rotationIndex);
    CurrentShape[0] = AppearType->GetShape(state, static_cast<int32_t>(BodyRotation), 0, FrameRate);

    if (static_cast<uint32_t>(NumParts) < 2 || CurrentState == MCGVActorState::Destroyed)
    {
        CurrentShape[1] = nullptr;
    }
    else
    {
        CurrentShape[1] = AppearType->GetShape(CurrentState, static_cast<int32_t>(TurretRotation), 1, FrameRate);
    }

    ElementList()->OpenGroup(static_cast<int32_t>(static_cast<float>(depthFixup) - ScreenPos.Y), 0);

    for (int32_t part = 0; part < NumParts; part++)
    {
        if (CurrentShape[part] == nullptr || CurrentShape[part]->FrameList == nullptr)
        {
            continue;
        }

        float offsetX = 0.0f;
        float offsetY = 0.0f;

        if (part == 1)
        {
            // The turret sits turretOffset meters forward of the body's centre.
            const double distance = static_cast<double>(MCZoomScale(Eye)) * TurretOffset * WorldUnitsPerMeter;
            offsetX = static_cast<float>(distance * std::sin(BodyRotation * 0x1.1df46a2526c7ap-6));
            offsetY = static_cast<float>(distance * std::cos(BodyRotation * 0x1.1df46a2526c7ap-6) * 0.5);
        }

        uint8_t* fadeTable = nullptr;

        if (FadeTableIndex >= 0)
        {
            fadeTable = GamePalette()->GetFadeTable(FadeTableIndex);
        }

        ElementList()->Add(ElementList()->Make<MCVfxElement>(CurrentShape[part]->FrameList, ScreenPos.X - offsetX,
                                                             ScreenPos.Y - offsetY, CurrentFrame[part], 0, fadeTable,
                                                             0));
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

auto MCGVAppearance::Update() -> int32_t
{
    for (int32_t i = 0; i < NumParts; i++)
    {
        if (CurrentFrame[i] == -1)
        {
            CurrentFrame[i] = 0;
        }
    }

    if (Visible && Owner->IsCaptured() != 0 && !Highlighted && !Highlighting)
    {
        Highlighting = true;
        HighlightTime = 3.0f;
    }

    CurrentTime = FrameLength + CurrentTime;
    const auto wholeFrames = static_cast<int32_t>(std::floor(static_cast<double>(CurrentTime * FrameRate)));

    if (LastFrame < wholeFrames)
    {
        const int32_t played = LastFrame;
        LastFrame = wholeFrames;
        const int32_t advanced = wholeFrames - played;

        if (advanced != 0)
        {
            // The turret shows the body's frame.
            const auto frame = static_cast<uint32_t>(CurrentFrame[0] + advanced);
            CurrentFrame[0] = static_cast<int32_t>(frame);
            const uint32_t numFrames = AppearType->StateData(CurrentState).NumFrames;

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

auto MCGVAppearance::StateExists(MCGVActorState state) -> int32_t
{
    const int32_t numStates = (AppearType->HasExtraState ? 1 : 0) + 3;

    if (static_cast<int32_t>(state) < numStates && static_cast<int32_t>(state) >= 0)
    {
        return static_cast<int32_t>(AppearType->StateData(state).NumFrames);
    }

    return 0;
}

auto MCGVAppearance::DrawBars() -> void
{
    // Port: the bar is an overlay, on the screen over the view: it follows the sprite through the zoom, its size
    // doesn't change.
    const float scale = MCZoomScale(Eye);
    const float barWidth = scale * 38.0f;
    const float barHeight = scale * 4.0f;
    const float barY = (MCOverlayY(UpperLeft.Y) - scale * 6.0f) - barHeight;
    const auto barX = static_cast<float>(std::floor(static_cast<double>(MCOverlayX(ScreenPos.X) - barWidth * 0.5f)));

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
            const auto capacity = static_cast<float>(vehicle->Armor[GROUNDVEHICLE_LOCATION_TURRET].MaxArmor);
            health = static_cast<float>(vehicle->GetRefitPoints() / capacity * health);
        }
    }
    else if (objectClass == TURRET)
    {
        int32_t damage = static_cast<int32_t>(obj->GetDamage());
        const auto dmgLevel = static_cast<int32_t>(static_cast<MCTurretType*>(obj->GetObjectType())->DmgLevel);

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

    ElementList()->OpenGroup(-50000, 1);
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
        ElementList()->Add(ElementList()->Make<MCPolygonElement>(data, -50000));
    }
}
