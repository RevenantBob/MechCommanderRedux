#include "stdafx.h"
#include "object/smokmgr.h"
#include "lib/inifile.h"
#include "object/object.h"
#include "object/objtype.h"

int32_t TotalSmokeSpheres = 0;
int32_t TotalSmokeShapeSize = 0;

auto MCSmokeManager::Init(MCFitIniFile* scenarioFile) -> int32_t
{
    int32_t result = scenarioFile->SeekBlock("Smoke Manager");

    if (result != 0)
    {
        return result;
    }

    if ((result = scenarioFile->ReadIdLong("NumSmokeTypes", NumSmokeTypes)) != 0)
    {
        return result;
    }

    if ((result = scenarioFile->ReadIdLong("MaxSmokesPerType", MaxSmokesPerType)) != 0)
    {
        return result;
    }

    // Read but not used: the sphere heap is sized from the scenario's smoke totals.
    uint32_t sphereHeapSize = 0;

    if ((result = scenarioFile->ReadIdULong("SmokeSphereHeapSize", sphereHeapSize)) != 0)
    {
        return result;
    }

    // Preload the smoke types the game makes on its own.
    ObjectTypeManager->Load(0xb, 1);
    ObjectTypeManager->Load(0x1c2, 1);
    ObjectTypeManager->Load(0x1c5, 1);
    ObjectTypeManager->Load(0x28c, 1);
    ObjectTypeManager->Load(0x293, 1);
    ObjectTypeManager->Load(0x2bd, 1);
    ObjectTypeManager->Load(0x296, 1);
    NumFreeSpheres = TotalSmokeSpheres;
    return 0;
}

auto MCSmokeManager::Destroy() -> void
{
    SphereBlocks.Clear();
}

auto MCSmokeManager::GetSpheres(int32_t& numSpheres) -> MCSmokeSphere*
{
    const int32_t available = NumFreeSpheres;
    const int32_t wanted = numSpheres;

    if (wanted < available)
    {
        // Port fix: the original allocates wanted * 0x38; SmokeSphere holds a pointer and is 0x40 on x64.
        auto* spheres = SphereBlocks.AllocateArray<MCSmokeSphere>(static_cast<size_t>(wanted));
        NumFreeSpheres = available - wanted;
        return spheres;
    }

    numSpheres = 0;
    return nullptr;
}

auto MCSmokeManager::FreeSpheres(MCSmokeSphere* spheres, int32_t numSpheres) -> void
{
    if (spheres == nullptr)
    {
        return;
    }

    NumFreeSpheres += numSpheres;
    SphereBlocks.Free(spheres);
}
