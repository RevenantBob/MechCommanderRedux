#include "stdafx.h"
#include "object/jet.h"
#include "appear/apprtype.h"
#include "camera/camera.h"
#include "camera/camlist.h"
#include "lib/MCFrameOfRef.h"
#include "lib/MCVector2D.h"
#include "lib/MCDice.h"
#include "lib/MCFile.h"
#include "lib/MCFitIniFile.h"
#include "main/main.h"
#include "object/mech.h"
#include "object/object.h"
#include "object/smoke.h"
#include "sound/soundsys.h"
#include "sprite/armactor.h"
#include "sprite/mactor.h"
#include "terrain/terrain.h"

namespace
{
    /// <summary>Radians to degrees, as MCX.EXE stores it.</summary>
    constexpr double RADIANS_TO_DEGREES = 0x1.ca5dc1a6402aap+5;
} // namespace

//---------------------------------------------------------------------------
// JetType
//---------------------------------------------------------------------------

MCJetType::MCJetType()
{
    SoundEffectId = 0xffffffff;
    SmokeObjectId = 0xffffffff;
    GroundObjectId = 0xffffffff;
}

auto MCJetType::CreateInstance() -> MCBaseObject*
{
    auto* newJet = new MCJet;

    if (newJet == nullptr)
    {
        return nullptr;
    }

    if (newJet->Init(this) != 0)
    {
        return nullptr;
    }

    newJet->IdNumber = NextIdNumber++;
    return newJet;
}

auto MCJetType::Destroy() -> void
{
}

auto MCJetType::Init(MCFile* objFile, uint32_t fileSize) -> int32_t
{
    MCFitIniFile jetFile;
    int32_t result = jetFile.Open(objFile, fileSize);

    if (result != 0)
    {
        return result;
    }

    if (jetFile.SeekBlock("JetData") == 0)
    {
        if ((result = jetFile.ReadIdULong("SoundEffectId", SoundEffectId)) != 0)
        {
            return result;
        }

        if ((result = jetFile.ReadIdULong("SmokeObjectId", SmokeObjectId)) != 0)
        {
            return result;
        }
    }

    return MCObjectType::Init(&jetFile);
}

auto MCJetType::HandleCollision(MCGameObject*, MCGameObject*) -> int
{
    return 0;
}

auto MCJetType::HandleDestruction(MCGameObject*, MCGameObject*) -> int
{
    return 0;
}

//---------------------------------------------------------------------------
// Jet
//---------------------------------------------------------------------------

MCJet::MCJet()
{
    JetNumber = 0;
    JustCreated = 1;
    Appearance = nullptr;
    Smoke = nullptr;
    Owner = nullptr;
    GroundObject = nullptr;
    DrawRotation = 0;
}

auto MCJet::Init() -> void
{
}

auto MCJet::IsVisible() -> int
{
    MCCamera* camera = CameraList->FindCameraFromIDNumber(1);

    if (camera == nullptr || camera->Active == 0)
    {
        return 0;
    }

    MCVector2D screen100;
    MCVector2D screen50;

    if (Land != nullptr)
    {
        Land->ProjectTerrain(Position, screen100, screen50);
    }

    float screenY;

    if (camera->CameraScale == 1)
    {
        ScreenPos.X = (screen50.X - camera->ScreenUL50.X) + camera->HalfWidth;
        screenY = screen50.Y - camera->ScreenUL50.Y;
    }
    else
    {
        ScreenPos.X = (screen100.X - camera->ScreenUL.X) + camera->HalfWidth;
        screenY = screen100.Y - camera->ScreenUL.Y;
    }

    ScreenPos.Y = screenY + camera->HalfHeight;

    if (Appearance != nullptr && Appearance->RecalcBounds(camera) != 0)
    {
        WindowsVisible = Turn;
        return 1;
    }

    return 0;
}

auto MCJet::Update() -> int32_t
{
    if (JustCreated != 0)
    {
        JustCreated = 0;
        CollisionsOn = 0;

        if (Owner != nullptr)
        {
            Position = Owner->GetJumpPosition(JetNumber);
        }

        LastAltitude = Position.Z;
        const uint32_t soundId = static_cast<MCJetType*>(ObjType)->SoundEffectId;

        if (soundId != 0xffffffff)
        {
            SoundSystem->PlayDigitalSample(soundId, 1, this, 0, 0);
        }
    }

    MCBattleMech* mech = Owner;

    // Port fix: the original reads the owner's appearance and frame without checking it for null.
    if (mech == nullptr)
    {
        return 1;
    }

    Position = mech->GetJumpPosition(JetNumber);

    // Coming down: the jets cut out and the smoke stops.
    if (static_cast<MCMechActor*>(mech->Appearance)->Airborne != 0 && Position.Z < LastAltitude)
    {
        Landing = 1;
    }

    LastAltitude = Position.Z;
    const MCVector3D velocity = mech->GetVelocity();
    const int visibleNow = IsVisible();
    Appearance->Visible = visibleNow;
    Appearance->Update();

    // The flame points away from the mech's facing.
    const MCFrameOfRef frame = mech->GetFrame();
    float cosFacing = UnitX.X * frame.I.X + UnitX.Y * frame.I.Y + UnitX.Z * frame.I.Z;

    if (cosFacing < -1.0f)
    {
        cosFacing = -1.0f;
    }

    if (1.0f < cosFacing)
    {
        cosFacing = 1.0f;
    }

    double facing = AcosMatherr(static_cast<double>(cosFacing)) * RADIANS_TO_DEGREES;

    if (frame.I.Y < 0.0f)
    {
        facing = -facing;
    }

    DrawRotation = std::abs(facing) <= 90.0 ? 150 : -150;

    if (Smoke != nullptr)
    {
        Smoke->SetOwnerPosition(Position);
        Smoke->SetOwnerVelocity(velocity);

        if (Landing != 0)
        {
            Smoke->StopSmoking();
        }

        Smoke->Update();
    }

    if (GroundObject != nullptr)
    {
        MCVector3D groundPos = Position;
        groundPos.Z = Land->GetTerrainElevation(Position);
        GroundObject->SetPosition(groundPos);
        GroundObject->Update();
    }

    return 1;
}

auto MCJet::Render() -> void
{
    if (JustCreated != 0)
    {
        return;
    }

    if (Landing == 0)
    {
        Appearance->Render(DrawRotation);
    }

    if (Smoke != nullptr)
    {
        // Faithful: the smoke's depth bias takes the flame's draw rotation.
        Smoke->DepthBias = DrawRotation;
        Smoke->Render();
    }

    if (GroundObject != nullptr)
    {
        GroundObject->Render();
    }
}

auto MCJet::Destroy() -> void
{
    delete Appearance;
    Appearance = nullptr;

    if (Smoke != nullptr)
    {
        delete Smoke;
        Smoke = nullptr;
    }

    delete GroundObject;
    GroundObject = nullptr;
}

auto MCJet::Init(MCObjectType* objType) -> int32_t
{
    int32_t result = MCGameObject::Init(objType);

    if (result != 0)
    {
        return result;
    }

    JustCreated = 1;
    MCAppearanceType* apprType = AppearanceTypeList->GetAppearance(objType->AppearName, 0);

    if (apprType == nullptr)
    {
        return static_cast<int32_t>(0xdcdc005e);
    }

    if ((apprType->AppearanceNum & 0xff000000) != 0x6000000)
    {
        return static_cast<int32_t>(0xdcdc0050);
    }

    auto* armAppearance = new MCArmAppearance;
    Appearance = armAppearance;

    if (armAppearance == nullptr)
    {
        return static_cast<int32_t>(0xdcdc005f);
    }

    armAppearance->Init(nullptr, nullptr);
    armAppearance->OwnerObject = nullptr;

    if ((result = armAppearance->Init(apprType, this)) != 0)
    {
        return result;
    }

    const auto* jetType = static_cast<MCJetType*>(objType);

    if (static_cast<int32_t>(jetType->SmokeObjectId) != -1)
    {
        Smoke = static_cast<MCSmoke*>(CreateObject(static_cast<int32_t>(jetType->SmokeObjectId)));
    }

    if (static_cast<int32_t>(jetType->GroundObjectId) != -1)
    {
        GroundObject = CreateObject(static_cast<int32_t>(jetType->GroundObjectId));
    }

    ObjectClass = JET;
    Landing = 0;
    return 0;
}

auto MCJet::SetOwner(MCBaseObject* newOwner) -> void
{
    Owner = static_cast<MCBattleMech*>(newOwner);
}
