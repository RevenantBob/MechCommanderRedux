#include "stdafx.h"
#include "object/MCJet.h"
#include "appear/MCAppearanceType.h"
#include "appear/MCAppearanceTypeList.h"
#include "camera/MCCamera.h"
#include "lib/MCFrameOfRef.h"
#include "main/MCMissionGlobals.h"
#include "object/MCBattleMech.h"
#include "object/MCJetType.h"
#include "object/MCObjectDrawing.h"
#include "object/MCObjectSystem.h"
#include "object/MCSmoke.h"
#include "sound/MCSoundSystem.h"
#include "sprite/MCArmAppearance.h"
#include "sprite/MCMechActor.h"
#include "terrain/MCTerrain.h"

namespace
{
    /// <summary>Radians to degrees, as MCX.EXE stores it.</summary>
    constexpr double RadiansToDegrees = 0x1.ca5dc1a6402aap+5;
} // namespace

MCJet::MCJet() = default;

MCJet::~MCJet() = default;

auto MCJet::IsVisible() -> bool
{
    MCCamera* camera = ActiveMainCamera();

    if (camera == nullptr)
    {
        return false;
    }

    ScreenPos = ProjectToScreen(Position, *camera);

    if (Appearance != nullptr && Appearance->RecalcBounds(camera) != 0)
    {
        WindowsVisible = Turn;
        return true;
    }

    return false;
}

auto MCJet::Update() -> int32_t
{
    if (JustCreated)
    {
        JustCreated = false;
        CollisionsOn = 0;

        if (Owner != nullptr)
        {
            Position = Owner->GetJumpPosition(JetNumber);
        }

        LastAltitude = Position.Z;
        const uint32_t soundId = static_cast<MCJetType*>(ObjType)->SoundEffectId;

        if (soundId != 0xffffffff)
        {
            SoundSystem()->PlayDigitalSample(soundId, 1, this, 0, 0);
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
    if (static_cast<MCMechActor*>(mech->Appearance.get())->Airborne != 0 && Position.Z < LastAltitude)
    {
        Landing = true;
    }

    LastAltitude = Position.Z;
    const MCVector3D velocity = mech->GetVelocity();
    const bool visibleNow = IsVisible();
    Appearance->Visible = visibleNow ? 1 : 0;
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

    double facing = AcosMatherr(static_cast<double>(cosFacing)) * RadiansToDegrees;

    if (frame.I.Y < 0.0f)
    {
        facing = -facing;
    }

    DrawRotation = std::abs(facing) <= 90.0 ? 150 : -150;

    if (Smoke != nullptr)
    {
        Smoke->SetOwnerPosition(Position);
        Smoke->SetOwnerVelocity(velocity);

        if (Landing)
        {
            Smoke->StopSmoking();
        }

        Smoke->Update();
    }

    if (GroundObject != nullptr)
    {
        MCVector3D groundPos = Position;
        groundPos.Z = Terrain()->GetTerrainElevation(Position);
        GroundObject->SetPosition(groundPos);
        GroundObject->Update();
    }

    return 1;
}

auto MCJet::Render() -> void
{
    if (JustCreated)
    {
        return;
    }

    if (!Landing)
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

auto MCJet::Init(MCObjectType* objType) -> int32_t
{
    if (const int32_t result = MCGameObject::Init(objType); result != 0)
    {
        return result;
    }

    JustCreated = true;
    MCAppearanceType* apprType = AppearanceTypeList()->GetAppearance(objType->AppearName);

    if (apprType == nullptr)
    {
        return static_cast<int32_t>(0xdcdc005e);
    }

    if ((apprType->AppearanceNum & 0xff000000) != 0x6000000)
    {
        return static_cast<int32_t>(0xdcdc0050);
    }

    Appearance = std::make_unique<MCArmAppearance>();
    Appearance->Init(nullptr, nullptr);
    Appearance->OwnerObject = nullptr;

    if (const int32_t result = Appearance->Init(apprType, this); result != 0)
    {
        return result;
    }

    const auto* jetType = static_cast<MCJetType*>(objType);

    if (static_cast<int32_t>(jetType->SmokeObjectId) != -1)
    {
        Smoke = CreateObjectAs<MCSmoke>(static_cast<int32_t>(jetType->SmokeObjectId));
    }

    if (static_cast<int32_t>(jetType->GroundObjectId) != -1)
    {
        GroundObject = CreateObject(static_cast<int32_t>(jetType->GroundObjectId));
    }

    ObjectClass = MCObjectClass::Jet;
    Landing = false;
    return 0;
}

auto MCJet::SetOwner(MCBaseObject* newOwner) -> void
{
    Owner = static_cast<MCBattleMech*>(newOwner);
}
