#include "stdafx.h"
#include "object/MCBigGameObject.h"
#include "ai/MCMoveSystem.h"
#include "camera/MCCamera.h"
#include "camera/MCCameraList.h"
#include "lib/MCFatal.h"
#include "lib/MCFile.h"
#include "main/MCMissionGlobals.h"
#include "object/MCContactSystem.h"
#include "object/MCObjectSystem.h"
#include "object/MCObjectType.h"

MCBigGameObject::~MCBigGameObject()
{
    // The systems can be gone when an object outlives them (a test's, or one held outside the lists).
    if (PotentialContact != nullptr && PotentialContactManager() != nullptr)
    {
        PotentialContactManager()->Remove(PotentialContact);
    }

    if (ObjPosition != nullptr && GameObjectMap() != nullptr)
    {
        GameObjectMap()->RemoveObject(this);
    }
}

auto MCBigGameObject::Init(MCObjectType* type) -> int32_t
{
    ObjectClass = MCObjectClass::BigGameObject;
    ObjType = type;
    return 0;
}

auto MCBigGameObject::Kill() -> int32_t
{
    // Port fix: with no type the original used uninitialised ids.
    int32_t explosionId = -1;
    int32_t destroyedId = -1;

    if (GetObjectType() != nullptr)
    {
        explosionId = GetObjectType()->ExplosionObject;
        destroyedId = GetObjectType()->DestroyedObject;
    }

    if (explosionId != -1)
    {
        std::unique_ptr<MCGameObject> explosion = CreateObject(explosionId);
        MCVector3D here = GetPosition();
        explosion->SetPosition(here);
        MCVector3D velocity = GetVelocity();
        explosion->SetVelocity(velocity);
        AddToDefaultList(std::move(explosion));
    }

    if (destroyedId != -1)
    {
        std::unique_ptr<MCGameObject> wreck = CreateObject(destroyedId);
        MCVector3D here = GetPosition();
        wreck->SetPosition(here);
        MCVector3D velocity = GetVelocity();
        wreck->SetVelocity(velocity);
        MCFrameOfRef frame = GetFrame();
        wreck->SetFrame(frame);
        AddToDefaultList(std::move(wreck));
    }

    return static_cast<int32_t>(0xbeaddead);
}

auto MCBigGameObject::OnScreen() -> int
{
    MCCamera* camera = CameraList()->FindCameraFromIDNumber(1);

    if (camera == nullptr || camera->Active == 0)
    {
        return 0;
    }

    // At camera scale 1 everything is drawn at half size.
    const float scale = MCCamera::CameraScale != 1 ? 1.0f : 0.5f;
    const float dx = (Position.X - camera->Position.X) * scale;
    const float dy = (Position.Y - camera->Position.Y) * scale;
    const float dz = scale * Position.Z - camera->Position.Z;
    const float screenX = static_cast<float>(static_cast<double>(dy) * camera->CosAngle +
                                             static_cast<double>(dx) * camera->CosAngle + camera->HalfWidth);
    ScreenPos.X = screenX;
    const float screenY = static_cast<float>(((static_cast<double>(dx) * camera->SinAngle + camera->HalfHeight) -
                                              static_cast<double>(dy) * camera->SinAngle) -
                                             static_cast<double>(scale) * dz);
    ScreenPos.Y = screenY;

    if (screenX >= 0.0f && screenY >= 0.0f && screenX <= camera->ViewWidth && screenY <= camera->ViewHeight)
    {
        WindowsVisible = Turn;
        return 1;
    }

    return 0;
}

auto MCBigGameObject::SetPotentialContact(int32_t contactType) -> void
{
    // The contact list by alignment: the clan's for -1, the Inner Sphere's for 1, else the allied list.
    MCPotentialContactType listType;

    if (Alignment == -1)
    {
        listType = MCPotentialContactType::Clan;
    }
    else
    {
        listType = Alignment != 1 ? MCPotentialContactType::Allied : MCPotentialContactType::InnerSphere;
    }

    if (contactType == 0)
    {
        PotentialContactManager()->Remove(PotentialContact);
        PotentialContact = nullptr;
        return;
    }

    if (PotentialContact == nullptr)
    {
        PotentialContact = PotentialContactManager()->Add(listType, this, static_cast<int8_t>(contactType));
        return;
    }

    PotentialContactManager()->Move(PotentialContact, listType, static_cast<int8_t>(contactType));
}

auto MCBigGameObject::UpdateContactStatus(MCTeam* contactTeam) -> void
{
    if (PotentialContact != nullptr)
    {
        PotentialContact->UpdateStatus(contactTeam);
    }
}

auto MCBigGameObject::GetContactCount(int32_t teamId) -> int32_t
{
    Assert(teamId > -1, -1, " Bad Team Id ");

    if (PotentialContact != nullptr)
    {
        return static_cast<uint8_t>(PotentialContact->NumSensors[teamId]);
    }

    return 0;
}

// The "tagged" flag is the contact's LostVisual byte.

auto MCBigGameObject::SetContactTagged(int32_t teamId, int tagged) -> void
{
    Assert(teamId > -1, -1, " Bad Team Id ");
    Assert(PotentialContact != nullptr, -1, " Is Not Potential Contact ");

    if (PotentialContact != nullptr)
    {
        PotentialContact->LostVisual[teamId] = static_cast<uint8_t>(tagged);
    }
}

auto MCBigGameObject::GetContactTagged(int32_t teamId) -> int
{
    Assert(teamId > -1, -1, " Bad Team Id ");

    if (PotentialContact != nullptr)
    {
        return PotentialContact->LostVisual[teamId];
    }

    return 0;
}

auto MCBigGameObject::GetContactType(int32_t teamId, int& tagged) -> int32_t
{
    Assert(teamId > -1, -1, " Bad Team Id ");

    if (PotentialContact != nullptr)
    {
        tagged = PotentialContact->LostVisual[teamId];
        return static_cast<int32_t>(PotentialContact->ContactStatus[teamId]);
    }

    return 0;
}

auto MCBigGameObject::GetContactType(int32_t teamId) -> int32_t
{
    Assert(teamId > -1, -1, " Bad Team Id ");

    if (PotentialContact != nullptr)
    {
        return static_cast<int32_t>(PotentialContact->ContactStatus[teamId]);
    }

    return 0;
}

auto MCBigGameObject::GetScreenPos(int32_t) -> MCVector2D
{
    return ScreenPos;
}

auto MCBigGameObject::SetAlignment(int32_t newAlignment) -> void
{
    Alignment = newAlignment;

    if (PotentialContact != nullptr)
    {
        SetPotentialContact(PotentialContact->Visibility);
    }
}

auto MCBigGameObject::GetMechClass() -> MCMechClass
{
    if (ObjectClass != MCObjectClass::BattleMech)
    {
        return MCMechClass::None;
    }

    if (Tonnage < 35.0f)
    {
        return MCMechClass::Light;
    }

    if (Tonnage < 55.0f)
    {
        return MCMechClass::Medium;
    }

    if (Tonnage >= 75.0f)
    {
        return MCMechClass::Assault;
    }

    return MCMechClass::Heavy;
}

auto MCBigGameObject::DecrementAttackers() -> void
{
    Assert(NumAttackers > 0, 0);
    NumAttackers--;
}

auto MCBigGameObject::GetVitalInfo(void* vitalInfo) -> int32_t
{
    // The original only asks isCaptured and fills nothing.
    if (vitalInfo != nullptr)
    {
        IsCaptured();
    }

    return 0x19;
}
