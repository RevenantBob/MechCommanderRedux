#include "stdafx.h"
#include "object/laser.h"
#include "camera/camera.h"
#include "engine/ceglist.h"
#include "engine/cepoly.h"
#include "engine/crater.h"
#include "lib/MCIDString.h"
#include "lib/MCFrameOfRef.h"
#include "lib/MCVector2D.h"
#include "lib/MCDice.h"
#include "lib/MCFile.h"
#include "platform/MCRenderer.h"
#include "lib/MCFitIniFile.h"
#include "logistics/logmain.h"
#include "main/main.h"
#include "network/multplyr.h"
#include "object/object.h"
#include "object/objque.h"
#include "sound/soundsys.h"
#include "terrain/terrain.h"
#include "vfx/vfx.h"
#include "vfx/vfxfuncs.h"

namespace
{
    /// <summary>Radians to degrees, as MCX.EXE stores it.</summary>
    constexpr double RADIANS_TO_DEGREES = 0x1.ca5dc1a6402aap+5;

    /// <summary>
    /// Sets the beam's duration and colours from stage <paramref name="stage"/> of the friendly set, or of the enemy
    /// set (the second half of the arrays) when the shooter is missing or not on side 1. Past the last stage the
    /// duration is -1 and the colours 0.
    /// </summary>
    void SetStage(MCLaser* laser, const MCLaserType* type, uint32_t stage)
    {
        auto* shooter = static_cast<MCGameObject*>(laser->Source.Object);
        const uint32_t numStages = type->NumStages;
        const uint32_t base = (shooter == nullptr || shooter->GetAlignment() != 1) ? numStages : 0;
        laser->StageTimeLeft = stage < numStages ? type->StageDuration[base + stage] : -1.0f;
        laser->HotColor = stage < numStages ? type->StageHot[base + stage] : 0;
        laser->CoolColor = stage < numStages ? type->StageCool[base + stage] : 0;
    }

    /// <summary>Applies the laser's shot to its target (the server's job in multiplayer) and marks it applied.</summary>
    void ApplyShot(MCLaser* laser)
    {
        auto* victim = static_cast<MCGameObject*>(laser->Target.Object);

        if (MPlayer == nullptr)
        {
            victim->HandleWeaponHit(&laser->ShotInfo, 0);
        }
        else if (MPlayer->IsServer != 0)
        {
            victim->HandleWeaponHit(&laser->ShotInfo, 1);
        }

        laser->DamageApplied = 1;
    }

    /// <summary>
    /// Pulls the beam's end in from the target to <paramref name="fraction"/> of the way from
    /// <paramref name="start"/>, so it grows out over its first stage (or PPC frame).
    /// </summary>
    void ExtendBeam(MCVector3D& end, const MCVector3D& start, float fraction)
    {
        float dx = end.X - start.X;
        float dy = end.Y - start.Y;
        float dz = end.Z - start.Z;
        const float length = std::sqrt(dx * dx + dy * dy + dz * dz);

        if (length != 0.0f)
        {
            dx /= length;
            dy /= length;
            dz /= length;
        }

        const float reach = fraction * length;
        end.X = dx * reach + start.X;
        end.Y = start.Y + dy * reach;
        end.Z = start.Z + dz * reach;
    }

    /// <summary>Creates the hit (or, without a target, the miss) effect at the beam's end; a miss leaves a crater.</summary>
    void CreateHitEffect(MCLaser* laser, const MCLaserType* type, MCGameObject* victim)
    {
        laser->HitEffectCreated = 1;
        MCGameObject* effect =
            CreateObject(static_cast<int32_t>(victim == nullptr ? type->LaserMissEffect : type->LaserHitEffect));

        if (effect == nullptr)
        {
            return;
        }

        if (laser->TargetPosition != nullptr)
        {
            effect->SetPosition(*laser->TargetPosition);
        }

        if (ObjectList->Head != nullptr)
        {
            ObjectList->Head->AddNode(effect);
        }

        if (victim == nullptr && laser->TargetPosition != nullptr)
        {
            CraterManager->AddCrater(7, *laser->TargetPosition, 1);
        }
    }

    /// <summary>A screen vertex (colour, u and v as given; w zero).</summary>
    MCScreenVertex ScreenVertex(int32_t x, int32_t y, MCFixed16 c, MCFixed16 u, MCFixed16 v)
    {
        MCScreenVertex vertex;
        vertex.X = x;
        vertex.Y = y;
        vertex.C = c;
        vertex.U = u;
        vertex.V = v;
        vertex.W = 0;
        return vertex;
    }
} // namespace

std::unique_ptr<uint8_t[]> LaserEffectBuffer;
std::unique_ptr<MCPane> LaserPane;
std::unique_ptr<MCWindow> LaserWindow;

namespace
{
    /// <summary>
    /// The texture the PPC beam's polygons map: laserEffectBuffer as they read it (laserWindow's x_max bytes a row,
    /// one fewer than the window draws it with).
    /// </summary>
    MCTexture* LaserTexture = nullptr;
}

//---------------------------------------------------------------------------
// LaserType
//---------------------------------------------------------------------------

auto MCLaserType::Init() -> void
{
    MCObjectType::Init();
    StageDuration = nullptr;
    StageHot = nullptr;
    StageCool = nullptr;
    DmgLevel = 0;
    NumStages = 0;
    PixelWidth = 0;
    LaserHitEffect = 0xffffffff;
    LaserMissEffect = 0xffffffff;
    LaserEffectShape = nullptr;
    HitPpc = 0;
    NumPpcFrames = 0;
    Bppc = 0;
    Rppc = 0;
    Tppc = 0;
    Lppc = 0;
    LengthPpc = 0.0f;
    AnimPpc = 0.0f;
}

auto MCLaserType::CreateInstance() -> MCBaseObject*
{
    auto* newLaser = new MCLaser;

    if (newLaser == nullptr)
    {
        return nullptr;
    }

    if (newLaser->Init(this) != 0)
    {
        return nullptr;
    }

    newLaser->IdNumber = NextIdNumber++;
    return newLaser;
}

auto MCLaserType::Destroy() -> void
{
    MCBlockStore& cache = MCObjectTypeManager::ObjectTypeCache;
    cache.Free(StageDuration);
    StageDuration = nullptr;
    cache.Free(StageCool);
    StageCool = nullptr;
    cache.Free(StageHot);
    StageHot = nullptr;
}

auto MCLaserType::Init(MCFile* objFile, uint32_t fileSize) -> int32_t
{
    MCFitIniFile laserFile;
    int32_t result = laserFile.Open(objFile, fileSize);

    if (result != 0)
    {
        return result;
    }

    if ((result = laserFile.SeekBlock("LaserData")) != 0)
    {
        return result;
    }

    if ((result = laserFile.ReadIdUChar("PixelWidth", PixelWidth)) != 0)
    {
        return result;
    }

    if ((result = laserFile.ReadIdUChar("NumStages", NumStages)) != 0)
    {
        return result;
    }

    if ((result = laserFile.ReadIdULong("DmgLevel", DmgLevel)) != 0)
    {
        return result;
    }

    if ((result = laserFile.ReadIdULong("SoundEffectId", SoundEffectId)) != 0)
    {
        return result;
    }

    if ((result = laserFile.ReadIdULong("LaserHitEffect", LaserHitEffect)) != 0)
    {
        return result;
    }

    if ((result = laserFile.ReadIdULong("LaserMissEffect", LaserMissEffect)) != 0)
    {
        return result;
    }

    // A PPC: the effect shape and its frame data.
    char shapeName[80];

    if (laserFile.ReadIdString("LaserEffectShape", shapeName, 79) == 0)
    {
        std::string shapePath;
        shapePath = GamePath(SpritePath, shapeName, ".shp");
        MCFile shapeFile;

        if ((result = shapeFile.Open(shapePath)) != 0)
        {
            return result;
        }

        const uint32_t size = shapeFile.FileSize();
        LaserEffectShape = static_cast<uint8_t*>(MCObjectTypeManager::ObjectTypeCache.Allocate(size));

        if (LaserEffectShape == nullptr)
        {
            return static_cast<int32_t>(0xdcdc0001);
        }

        shapeFile.Read(LaserEffectShape, static_cast<int32_t>(size));
        MCRenderer::RegisterData(LaserEffectShape, size, MCDataKind::Shapes);
        shapeFile.Close();

        if ((result = laserFile.SeekBlock("PPCData")) != 0)
        {
            return result;
        }

        if ((result = laserFile.ReadIdULong("numPPCFrames", NumPpcFrames)) != 0)
        {
            return result;
        }

        if ((result = laserFile.ReadIdULong("tPPC", Tppc)) != 0)
        {
            return result;
        }

        if ((result = laserFile.ReadIdULong("lPPC", Lppc)) != 0)
        {
            return result;
        }

        if ((result = laserFile.ReadIdULong("bPPC", Bppc)) != 0)
        {
            return result;
        }

        if ((result = laserFile.ReadIdULong("rPPC", Rppc)) != 0)
        {
            return result;
        }

        if ((result = laserFile.ReadIdULong("hitPPC", HitPpc)) != 0)
        {
            return result;
        }

        if ((result = laserFile.ReadIdFloat("lengthPPC", LengthPpc)) != 0)
        {
            return result;
        }

        if ((result = laserFile.ReadIdFloat("animPPC", AnimPpc)) != 0)
        {
            return result;
        }
    }
    else
    {
        LaserEffectShape = nullptr;
    }

    // The stage arrays: numStages friendly stages, then numStages enemy ones.
    {
        MCBlockStore& cache = MCObjectTypeManager::ObjectTypeCache;
        const size_t count = NumStages;
        StageDuration = cache.AllocateArray<float>(count * 2);
        StageCool = cache.AllocateArray<uint8_t>(count * 2);
        StageHot = cache.AllocateArray<uint8_t>(count * 2);
    }

    const int32_t count = NumStages;
    char blockName[20];

    for (int32_t i = 0; i < count; i++)
    {
        std::sprintf(blockName, "FLaser%d", i);

        if ((result = laserFile.SeekBlock(blockName)) != 0)
        {
            return result;
        }

        if ((result = laserFile.ReadIdFloat("StageDuration", StageDuration[i])) != 0)
        {
            return result;
        }

        if ((result = laserFile.ReadIdUChar("StageCool", StageCool[i])) != 0)
        {
            return result;
        }

        if ((result = laserFile.ReadIdUChar("StageHot", StageHot[i])) != 0)
        {
            return result;
        }
    }

    for (int32_t i = 0; i < count; i++)
    {
        std::sprintf(blockName, "ELaser%d", i);

        if ((result = laserFile.SeekBlock(blockName)) != 0)
        {
            return result;
        }

        if ((result = laserFile.ReadIdFloat("StageDuration", StageDuration[count + i])) != 0)
        {
            return result;
        }

        if ((result = laserFile.ReadIdUChar("StageCool", StageCool[count + i])) != 0)
        {
            return result;
        }

        if ((result = laserFile.ReadIdUChar("StageHot", StageHot[count + i])) != 0)
        {
            return result;
        }
    }

    result = MCObjectType::Init(&laserFile);
    ObjectTypeManager->Load(static_cast<int32_t>(LaserHitEffect), 1);
    ObjectTypeManager->Load(static_cast<int32_t>(LaserMissEffect), 1);
    return result;
}

auto MCLaserType::HandleCollision(MCGameObject*, MCGameObject*) -> int
{
    return 0;
}

auto MCLaserType::HandleDestruction(MCGameObject*, MCGameObject*) -> int
{
    return 0;
}

//---------------------------------------------------------------------------
// Laser
//---------------------------------------------------------------------------

auto MCLaser::Init() -> void
{
    MCGameObject::Init();
    CurrentStage = 0xff;
    StageTimeLeft = 0.0f;
    CoolColor = 0;
    HotColor = 0;
    Source.SetWatcher(nullptr);
    Target.SetWatcher(nullptr);
    ShotInfo.Damage = 0.0f;
    TargetHotSpot = 0;
    SourceHotSpot = 0;
    TargetPosition = nullptr;
    HitEffectCreated = 0;
    DamageApplied = 0;
    ShotInfo.MasterId = -1;
    ShotInfo.HitLocation = -1;
    JustCreated = 1;
}

auto MCLaser::Init(MCObjectType* objType) -> int32_t
{
    Init();
    MCGameObject::Init(objType);
    CurrentStage = 0xff;
    ObjectClass = LASER;
    JustCreated = 1;
    return 0;
}

auto MCLaser::Destroy() -> void
{
    if (TargetPosition != nullptr)
    {
        delete TargetPosition;
        TargetPosition = nullptr;
    }
}

auto MCLaser::Update() -> int32_t
{
    const auto* type = static_cast<MCLaserType*>(ObjType);

    // A PPC runs through the frames of its effect shape and hits on frame hitPPC.
    if (type->LaserEffectShape != nullptr)
    {
        if (JustCreated == 0)
        {
            PpcAnimTimeLeft -= FrameLength;

            if (PpcAnimTimeLeft < 0.0f)
            {
                PpcAnimTimeLeft = type->AnimPpc;
            }

            PpcFrameTimeLeft -= FrameLength;

            if (PpcFrameTimeLeft < 0.0f)
            {
                PpcFrameTimeLeft = type->LengthPpc;
                PpcFrame++;

                if (PpcFrame == static_cast<int32_t>(type->HitPpc) && Target.Object != nullptr && DamageApplied == 0)
                {
                    ApplyShot(this);
                }

                if (PpcFrame == static_cast<int32_t>(type->NumPpcFrames))
                {
                    return 0;
                }
            }
        }
        else
        {
            PpcFrame = 0;
            DamageApplied = 0;
            PpcFrameTimeLeft = type->LengthPpc;
            PpcAnimTimeLeft = type->AnimPpc;
            SoundSystem->PlayDigitalSample(type->SoundEffectId, 1, this, 0, 0);
        }

        JustCreated = 0;
        return 1;
    }

    // A beam steps through its colour stages, then hits.
    if (CurrentStage == 0xff)
    {
        CurrentStage = 0;
        SetStage(this, type, 0);
        JustCreated = 0;
        return 1;
    }

    if (0.0f <= StageTimeLeft)
    {
        StageTimeLeft -= FrameLength;
        JustCreated = 0;
        return 1;
    }

    CurrentStage++;

    if (CurrentStage < type->NumStages)
    {
        SetStage(this, type, CurrentStage);
        JustCreated = 0;
        return 1;
    }

    if (Target.Object == nullptr || DamageApplied != 0)
    {
        return 0;
    }

    ApplyShot(this);
    return 0;
}

auto MCLaser::Render() -> void
{
    if (JustCreated != 0)
    {
        return;
    }

    auto* shooter = static_cast<MCGameObject*>(Source.Object);

    if (shooter == nullptr)
    {
        return;
    }

    const MCVector3D start = shooter->GetPositionFromHS(static_cast<uint32_t>(SourceHotSpot));
    auto* victim = static_cast<MCGameObject*>(Target.Object);

    if (victim != nullptr)
    {
        // Port fix (OB-017): end at the hot spot that was hit (the original used sourceHotSpot).
        const uint32_t hotSpot = victim->ObjectClass == TURRET ? 0xffffffff : static_cast<uint32_t>(TargetHotSpot);
        SetTargetPosition(victim->GetPositionFromHS(hotSpot));
    }

    // Grow the beam out to the target over the first stage (or PPC frame).
    const auto* type = static_cast<MCLaserType*>(ObjType);

    if (type->LaserEffectShape == nullptr)
    {
        if (CurrentStage == 0 && victim != nullptr && TargetPosition != nullptr)
        {
            // Faithful: the first stage's length is always read from the enemy set (index numStages).
            const float firstDuration = type->NumStages != 0 ? type->StageDuration[type->NumStages] : -1.0f;
            ExtendBeam(*TargetPosition, start, firstDuration - StageTimeLeft);
        }
    }
    else if (PpcFrame == 0 && victim != nullptr && TargetPosition != nullptr)
    {
        ExtendBeam(*TargetPosition, start, type->LengthPpc - PpcFrameTimeLeft);
    }

    // The hit (or miss) effect, once: when the beam reaches its second stage, or the PPC its hit frame.
    if (HitEffectCreated == 0)
    {
        if (type->LaserEffectShape == nullptr)
        {
            if (CurrentStage != 0)
            {
                CreateHitEffect(this, type, victim);
            }
        }
        else if (PpcFrame == static_cast<int32_t>(type->HitPpc))
        {
            CreateHitEffect(this, type, victim);
        }
    }

    // Port fix: the original goes on to draw from an unset end point when there is no target position.
    if (TargetPosition == nullptr)
    {
        return;
    }

    const MCVector3D end = *TargetPosition;
    MCVector2D start100;
    MCVector2D start50;
    MCVector2D end100;
    MCVector2D end50;

    if (Land != nullptr)
    {
        MCVector3D startPos = start;
        MCVector3D endPos = end;
        Land->ProjectTerrain(startPos, start100, start50);
        Land->ProjectTerrain(endPos, end100, end50);
    }

    float startX;
    float startY;
    float endX;
    float endY;

    if (Eye->CameraScale == 1)
    {
        startX = (start50.X - Eye->ScreenUL50.X) + Eye->HalfWidth;
        startY = (start50.Y - Eye->ScreenUL50.Y) + Eye->HalfHeight;
        endX = (end50.X - Eye->ScreenUL50.X) + Eye->HalfWidth;
        endY = (end50.Y - Eye->ScreenUL50.Y) + Eye->HalfHeight;
    }
    else
    {
        startX = (start100.X - Eye->ScreenUL.X) + Eye->HalfWidth;
        startY = (start100.Y - Eye->ScreenUL.Y) + Eye->HalfHeight;
        endX = (end100.X - Eye->ScreenUL.X) + Eye->HalfWidth;
        endY = (end100.Y - Eye->ScreenUL.Y) + Eye->HalfHeight;
    }

    // (The original then tests the leftmost end against the screen's left edge, but draws either way.)

    MCPolyElementData data;

    if (type->LaserEffectShape == nullptr)
    {
        // A beam: a quad pixelWidth wide across the facing, in the stage's outer colour.
        const float width = static_cast<float>(type->PixelWidth);
        // Original behaviour (OB-018): the facing is in degrees, and gets multiplied by 57.3 again before cos/sin.
        float crossX = static_cast<float>(std::cos(shooter->RelViewFacingTo(end) * RADIANS_TO_DEGREES) * width);
        float crossY = static_cast<float>(std::sin(shooter->RelViewFacingTo(end) * RADIANS_TO_DEGREES) * width);
        ElementList->OpenGroup(static_cast<int32_t>(startY), 1);
        const auto depth = static_cast<int32_t>((endY + startY) * 0.5f);
        const MCFixed16 color = static_cast<MCFixed16>(CoolColor << 16);
        data.NumVertices = 4;
        data.Vertices[0] =
            ScreenVertex(static_cast<int32_t>(crossX + startX), static_cast<int32_t>(startY + crossY), color, 0, 0);
        data.Vertices[3] =
            ScreenVertex(static_cast<int32_t>(startX - crossX), static_cast<int32_t>(startY - crossY), color, 0, 0);
        data.Vertices[2] =
            ScreenVertex(static_cast<int32_t>(endX - crossX), static_cast<int32_t>(endY - crossY), color, 0, 0);
        data.Vertices[1] =
            ScreenVertex(static_cast<int32_t>(crossX + endX), static_cast<int32_t>(endY + crossY), color, 0, 0);
        ElementList->Add(MCElementPool::Make<MCPolygonElement>(&data, depth));

        if (HotColor == CoolColor)
        {
            return;
        }

        // The core, half as wide.
        // Original behaviour (OB-019): the core is drawn in the outer (cool) colour too; the hot colour is never used.
        crossX = static_cast<float>(crossX * 0.5);
        crossY = static_cast<float>(crossY * 0.5);
        data.Vertices[0] =
            ScreenVertex(static_cast<int32_t>(crossX + startX), static_cast<int32_t>(startY + crossY), color, 0, 0);
        data.Vertices[3] =
            ScreenVertex(static_cast<int32_t>(startX - crossX), static_cast<int32_t>(startY - crossY), color, 0, 0);
        data.Vertices[2] =
            ScreenVertex(static_cast<int32_t>(endX - crossX), static_cast<int32_t>(endY - crossY), color, 0, 0);
        data.Vertices[1] =
            ScreenVertex(static_cast<int32_t>(crossX + endX), static_cast<int32_t>(endY + crossY), color, 0, 0);
        ElementList->Add(MCElementPool::Make<MCPolygonElement>(&data, depth));
        return;
    }

    // A PPC: the current frame of the effect shape, drawn into its own buffer and stretched along the beam.
    if (LaserEffectBuffer == nullptr)
    {
        LaserEffectBuffer = std::make_unique<uint8_t[]>(0x10000);
        LaserPane = std::make_unique<MCPane>();
        LaserWindow = std::make_unique<MCWindow>();
        const auto shapeWidth = static_cast<int32_t>(type->Rppc - type->Lppc);
        const auto shapeHeight = static_cast<int32_t>(type->Bppc - type->Tppc);
        LaserPane->X0 = 0;
        LaserPane->Y0 = 0;
        LaserPane->X1 = shapeWidth;
        LaserPane->Y1 = shapeHeight;
        LaserWindow->Buffer = LaserEffectBuffer.get();
        LaserWindow->XMax = shapeWidth + 1;
        LaserWindow->YMax = shapeHeight + 1;
        LaserPane->Window = LaserWindow.get();
        LaserTexture = MCRenderer::CreateTexture(LaserEffectBuffer.get(), LaserWindow->XMax, LaserWindow->YMax,
                                                 MCTextureUse::Dynamic);
    }

    AGShapeDraw(LaserPane.get(), type->LaserEffectShape, PpcFrame, 0, 0);
    MCRenderer::UnlockTexture(LaserTexture);
    const float width = static_cast<float>(type->PixelWidth);
    const auto top = static_cast<int32_t>(startY);
    ElementList->OpenGroup(top, 1);
    const auto left = static_cast<int32_t>(startX);
    const auto right = static_cast<int32_t>(endX);
    data.NumVertices = 4;
    data.Vertices[0] = ScreenVertex(left, top, 0, LaserPane->X0 << 16, LaserPane->Y0 << 16);
    data.Vertices[3] =
        ScreenVertex(left, static_cast<int32_t>(startY - width), 0, LaserPane->X0 << 16, LaserPane->Y1 << 16);
    data.Vertices[2] =
        ScreenVertex(right, static_cast<int32_t>(endY - width), 0, LaserPane->X1 << 16, LaserPane->Y1 << 16);
    data.Vertices[1] = ScreenVertex(right, static_cast<int32_t>(endY), 0, LaserPane->X1 << 16, LaserPane->Y0 << 16);
    data.Texture = LaserEffectBuffer.get();
    data.TextureWidth = LaserWindow->XMax;
    data.TextureHeight = LaserWindow->YMax;
    data.TextureHandle = LaserTexture;
    data.FadeTable = nullptr;
    ElementList->Add(MCElementPool::Make<MCPolygonElement>(&data, static_cast<int32_t>((endY + startY) * 0.5f)));
}

auto MCLaser::SetTargetPosition(MCVector3D position) -> void
{
    if (TargetPosition == nullptr)
    {
        TargetPosition = new MCVector3D;
    }

    *TargetPosition = position;
}

auto MCLaser::Connect(MCGameObject* source, MCVector3D targetPos, MCWeaponShotInfo* shotInfo, int32_t sourceHotSpot)
    -> void
{
    this->Source.SetWatcher(source);
    this->SourceHotSpot = sourceHotSpot;
    SetTargetPosition(targetPos);

    if (shotInfo != nullptr)
    {
        const MCWeaponShotInfo shot = *shotInfo;
        this->ShotInfo.Init(shot.Attacker, shot.MasterId, shot.Damage, shot.HitLocation, shot.EntryAngle);
    }
}
