#include "stdafx.h"
#include "object/MCEffectSystem.h"
#include "lib/MCFitIniFile.h"
#include "main/MCGameContext.h"
#include "object/MCFire.h"
#include "object/MCObjectSystem.h"
#include "object/MCObjectTypeManager.h"

namespace
{
    /// <summary>The smoke object types the game makes on its own (trails, damage and wreck smoke), kept loaded.</summary>
    constexpr std::array<int32_t, 7> PreloadedSmokeTypes = {0xb, 0x1c2, 0x1c5, 0x28c, 0x293, 0x2bd, 0x296};
}

MCEffectSystem::MCEffectSystem(int32_t maxFiresBurning, float maxFireBurnTime)
    : _FireRing(static_cast<size_t>(std::max(maxFiresBurning, 0))), _MaxFireBurnTime(maxFireBurnTime)
{
}

auto MCEffectSystem::Create(MCFitIniFile& scenarioFile, int32_t maxFiresBurning, float maxFireBurnTime)
    -> std::expected<std::unique_ptr<MCEffectSystem>, MCFitError>
{
    if (const int32_t result = scenarioFile.SeekBlock("Smoke Manager"); result != 0)
    {
        return std::unexpected(static_cast<MCFitError>(result));
    }

    // Read but not used: the smoke types and sphere heap were sized by them.
    MCFitReader read(scenarioFile);
    int32_t numSmokeTypes = 0;
    int32_t maxSmokesPerType = 0;
    uint32_t sphereHeapSize = 0;
    read.Value("NumSmokeTypes", numSmokeTypes);
    read.Value("MaxSmokesPerType", maxSmokesPerType);
    read.Value("SmokeSphereHeapSize", sphereHeapSize);

    if (read.Failed())
    {
        return std::unexpected(read.Error());
    }

    for (const int32_t smokeType : PreloadedSmokeTypes)
    {
        ObjectTypeManager()->Load(smokeType, 1);
    }

    return std::make_unique<MCEffectSystem>(maxFiresBurning, maxFireBurnTime);
}

auto MCEffectSystem::StartFire(MCFire* fire) -> void
{
    if (_FireRing.empty())
    {
        return;
    }

    _CurrentFire = (_CurrentFire + 1) % _FireRing.size();

    if (MCFire* oldest = _FireRing[_CurrentFire]; oldest != nullptr)
    {
        oldest->FinishFireNow();
    }

    _FireRing[_CurrentFire] = fire;
}

auto MCEffectSystem::EndFire(const MCFire* fire) -> void
{
    std::ranges::replace(_FireRing, const_cast<MCFire*>(fire), nullptr);
}

auto EffectSystem() -> MCEffectSystem*
{
    return MCGameContext::Current().EffectSystem();
}
