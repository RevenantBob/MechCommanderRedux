#include "stdafx.h"
#include "object/smokmgr.h"
#include "lib/heap.h"
#include "lib/inifile.h"
#include "object/object.h"
#include "object/objtype.h"

int32_t totalSmokeSpheres = 0;
int32_t totalSmokeShapeSize = 0;

auto SmokeManager::init(FitIniFile* scenarioFile) -> int32_t
{
    int32_t result = scenarioFile->seekBlock("Smoke Manager");

    if (result != 0)
    {
        return result;
    }

    if ((result = scenarioFile->readIdLong("NumSmokeTypes", numSmokeTypes)) != 0)
    {
        return result;
    }

    if ((result = scenarioFile->readIdLong("MaxSmokesPerType", maxSmokesPerType)) != 0)
    {
        return result;
    }

    // Read but not used: the sphere heap is sized from the scenario's smoke totals.
    uint32_t sphereHeapSize = 0;

    if ((result = scenarioFile->readIdULong("SmokeSphereHeapSize", sphereHeapSize)) != 0)
    {
        return result;
    }

    sphereHeap = new UserHeap;

    if (sphereHeap == nullptr)
    {
        return static_cast<int32_t>(0xdcdc0017);
    }

    if ((result = sphereHeap->init(static_cast<uint32_t>(totalSmokeSpheres * 0x40 + totalSmokeShapeSize))) != 0)
    {
        return result;
    }

    sphereHeap->unknown2C = 0;

    const int32_t numTypes = numSmokeTypes;
    const int32_t perType = maxSmokesPerType;
    smokeHeap = new UserHeap;

    if (smokeHeap == nullptr)
    {
        return static_cast<int32_t>(0xdcdc0017);
    }

    if ((result = smokeHeap->init(static_cast<uint32_t>(numTypes * 0x24 + 0x50 + (numTypes + 1) * perType * 0xbc))) !=
        0)
    {
        return result;
    }

    smokeHeap->unknown2C = 0;

    // Preload the smoke types the game makes on its own.
    objectTypeManager->load(0xb, 1);
    objectTypeManager->load(0x1c2, 1);
    objectTypeManager->load(0x1c5, 1);
    objectTypeManager->load(0x28c, 1);
    objectTypeManager->load(0x293, 1);
    objectTypeManager->load(0x2bd, 1);
    objectTypeManager->load(0x296, 1);
    numFreeSpheres = totalSmokeSpheres;
    return 0;
}

auto SmokeManager::destroy() -> void
{
    delete smokeHeap;
    smokeHeap = nullptr;
    delete sphereHeap;
    sphereHeap = nullptr;
}

auto SmokeManager::getSpheres(int32_t& numSpheres) -> SmokeSphere*
{
    const int32_t available = numFreeSpheres;
    const int32_t wanted = numSpheres;

    if (wanted < available)
    {
        // Port fix: the original allocates wanted * 0x38; SmokeSphere holds a pointer and is 0x40 on x64.
        auto* spheres =
            static_cast<SmokeSphere*>(sphereHeap->malloc(static_cast<uint32_t>(wanted * sizeof(SmokeSphere))));
        numFreeSpheres = available - wanted;
        return spheres;
    }

    numSpheres = 0;
    return nullptr;
}

auto SmokeManager::freeSpheres(SmokeSphere* spheres, int32_t numSpheres) -> void
{
    if (spheres == nullptr)
    {
        return;
    }

    numFreeSpheres += numSpheres;
    sphereHeap->free(spheres);
}
