#include "stdafx.h"
#include "object/MCCameraDroneType.h"
#include "lib/MCFitIniFile.h"
#include "object/MCCameraDrone.h"

auto MCCameraDroneType::CreateInstance() -> std::unique_ptr<MCBaseObject>
{
    auto newDrone = std::make_unique<MCCameraDrone>();

    if (newDrone->Init(this) != 0)
    {
        return nullptr;
    }

    newDrone->IdNumber = NextIdNumber++;
    return newDrone;
}

auto MCCameraDroneType::Init(MCFile* objFile, uint32_t fileSize) -> int32_t
{
    MCFitIniFile droneFile;

    if (const int32_t result = droneFile.Open(objFile, fileSize); result != 0)
    {
        return result;
    }

    if (const int32_t result = droneFile.SeekBlock("General"); result != 0)
    {
        return result;
    }

    MCFitReader read(droneFile);
    read.Value("maxVelocity", MaxVelocity);
    read.Value("maxDamage", MaxDamage);

    if (read.Failed())
    {
        return std::to_underlying(read.Error());
    }

    BrValue = droneFile.Read<int32_t>("BRValue").value_or(0);
    const int32_t result = MCObjectType::Init(&droneFile);
    ExtentRadius = -1.0f;
    return result;
}

auto MCCameraDroneType::HandleDestruction(MCGameObject* collidee, MCGameObject* /*collider*/) -> int
{
    collidee->Status = 2;
    return 1;
}
