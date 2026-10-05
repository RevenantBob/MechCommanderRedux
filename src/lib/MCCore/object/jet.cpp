#include "stdafx.h"
#include "object/jet.h"
#include "appear/apprtype.h"
#include "camera/camera.h"
#include "camera/camlist.h"
#include "lib/cvmath.h"
#include "lib/file.h"
#include "lib/inifile.h"
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

JetType::JetType()
{
    soundEffectId = 0xffffffff;
    smokeObjectId = 0xffffffff;
    groundObjectId = 0xffffffff;
}

auto JetType::createInstance() -> BaseObject*
{
    auto* newJet = new Jet;

    if (newJet == nullptr)
    {
        return nullptr;
    }

    if (newJet->init(this) != 0)
    {
        return nullptr;
    }

    newJet->idNumber = NextIdNumber++;
    return newJet;
}

auto JetType::destroy() -> void
{
}

auto JetType::init(File* objFile, uint32_t fileSize) -> int32_t
{
    FitIniFile jetFile;
    int32_t result = jetFile.open(objFile, fileSize, 50);

    if (result != 0)
    {
        return result;
    }

    if (jetFile.seekBlock("JetData") == 0)
    {
        if ((result = jetFile.readIdULong("SoundEffectId", soundEffectId)) != 0)
        {
            return result;
        }

        if ((result = jetFile.readIdULong("SmokeObjectId", smokeObjectId)) != 0)
        {
            return result;
        }
    }

    return ObjectType::init(&jetFile);
}

auto JetType::handleCollision(GameObject*, GameObject*) -> int
{
    return 0;
}

auto JetType::handleDestruction(GameObject*, GameObject*) -> int
{
    return 0;
}

//---------------------------------------------------------------------------
// Jet
//---------------------------------------------------------------------------

Jet::Jet()
{
    jetNumber = 0;
    justCreated = 1;
    appearance = nullptr;
    smoke = nullptr;
    owner = nullptr;
    groundObject = nullptr;
    drawRotation = 0;
}

auto Jet::init() -> void
{
}

auto Jet::isVisible() -> int
{
    Camera* camera = cameraList->findCameraFromIDNumber(1);

    if (camera == nullptr || camera->active == 0)
    {
        return 0;
    }

    vector_2d screen100;
    vector_2d screen50;

    if (land != nullptr)
    {
        land->projectTerrain(position, screen100, screen50);
    }

    float screenY;

    if (camera->cameraScale == 1)
    {
        screenPos.x = (screen50.x - camera->screenUL50.x) + camera->halfWidth;
        screenY = screen50.y - camera->screenUL50.y;
    }
    else
    {
        screenPos.x = (screen100.x - camera->screenUL.x) + camera->halfWidth;
        screenY = screen100.y - camera->screenUL.y;
    }

    screenPos.y = screenY + camera->halfHeight;

    if (appearance != nullptr && appearance->recalcBounds(camera) != 0)
    {
        windowsVisible = turn;
        return 1;
    }

    return 0;
}

auto Jet::update() -> int32_t
{
    if (justCreated != 0)
    {
        justCreated = 0;
        collisionsOn = 0;

        if (owner != nullptr)
        {
            position = owner->getJumpPosition(jetNumber);
        }

        lastAltitude = position.z;
        const uint32_t soundId = static_cast<JetType*>(objType)->soundEffectId;

        if (soundId != 0xffffffff)
        {
            soundSystem->playDigitalSample(soundId, 1, this, 0, 0);
        }
    }

    BattleMech* mech = owner;

    // Port fix: the original reads the owner's appearance and frame without checking it for null.
    if (mech == nullptr)
    {
        return 1;
    }

    position = mech->getJumpPosition(jetNumber);

    // Coming down: the jets cut out and the smoke stops.
    if (static_cast<MechActor*>(mech->appearance)->airborne != 0 && position.z < lastAltitude)
    {
        landing = 1;
    }

    lastAltitude = position.z;
    const vector_3d velocity = mech->getVelocity();
    const int visibleNow = isVisible();
    appearance->visible = visibleNow;
    appearance->update();

    // The flame points away from the mech's facing.
    const frame_of_ref frame = mech->getFrame();
    float cosFacing = UnitX.x * frame.i.x + UnitX.y * frame.i.y + UnitX.z * frame.i.z;

    if (cosFacing < -1.0f)
    {
        cosFacing = -1.0f;
    }

    if (1.0f < cosFacing)
    {
        cosFacing = 1.0f;
    }

    double facing = acosMatherr(static_cast<double>(cosFacing)) * RADIANS_TO_DEGREES;

    if (frame.i.y < 0.0f)
    {
        facing = -facing;
    }

    drawRotation = std::abs(facing) <= 90.0 ? 150 : -150;

    if (smoke != nullptr)
    {
        smoke->setOwnerPosition(position);
        smoke->setOwnerVelocity(velocity);

        if (landing != 0)
        {
            smoke->stopSmoking();
        }

        smoke->update();
    }

    if (groundObject != nullptr)
    {
        vector_3d groundPos = position;
        groundPos.z = land->getTerrainElevation(position);
        groundObject->setPosition(groundPos);
        groundObject->update();
    }

    return 1;
}

auto Jet::render() -> void
{
    if (justCreated != 0)
    {
        return;
    }

    if (landing == 0)
    {
        appearance->render(drawRotation);
    }

    if (smoke != nullptr)
    {
        // Faithful: the smoke's depth bias takes the flame's draw rotation.
        smoke->unknownB0 = drawRotation;
        smoke->render();
    }

    if (groundObject != nullptr)
    {
        groundObject->render();
    }
}

auto Jet::destroy() -> void
{
    delete appearance;
    appearance = nullptr;

    if (smoke != nullptr)
    {
        delete smoke;
        smoke = nullptr;
    }

    delete groundObject;
    groundObject = nullptr;
}

auto Jet::init(ObjectType* objType) -> int32_t
{
    int32_t result = GameObject::init(objType);

    if (result != 0)
    {
        return result;
    }

    justCreated = 1;
    AppearanceType* apprType = appearanceTypeList->getAppearance(objType->appearName, 0);

    if (apprType == nullptr)
    {
        return static_cast<int32_t>(0xdcdc005e);
    }

    if ((apprType->appearanceNum & 0xff000000) != 0x6000000)
    {
        return static_cast<int32_t>(0xdcdc0050);
    }

    auto* armAppearance = new ArmAppearance;
    appearance = armAppearance;

    if (armAppearance == nullptr)
    {
        return static_cast<int32_t>(0xdcdc005f);
    }

    armAppearance->init(nullptr, nullptr);
    armAppearance->ownerObject = nullptr;

    if ((result = armAppearance->init(apprType, this)) != 0)
    {
        return result;
    }

    const auto* jetType = static_cast<JetType*>(objType);

    if (static_cast<int32_t>(jetType->smokeObjectId) != -1)
    {
        smoke = static_cast<Smoke*>(createObject(static_cast<int32_t>(jetType->smokeObjectId)));
    }

    if (static_cast<int32_t>(jetType->groundObjectId) != -1)
    {
        groundObject = createObject(static_cast<int32_t>(jetType->groundObjectId));
    }

    objectClass = JET;
    landing = 0;
    return 0;
}

auto Jet::setOwner(BaseObject* newOwner) -> void
{
    owner = static_cast<BattleMech*>(newOwner);
}
