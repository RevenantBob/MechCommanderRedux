#include "stdafx.h"
#include "object/MCContactSystem.h"
#include "lib/MCFitIniFile.h"
#include "main/MCGameContext.h"

auto MCContactSystem::Create(MCFitIniFile& scenarioFile) -> std::expected<std::unique_ptr<MCContactSystem>, std::string>
{
    if (scenarioFile.SeekBlock("PotentialContactManager") != 0)
    {
        return std::unexpected("no PotentialContactManager block");
    }

    const MCFitResult<int32_t> maxContacts = scenarioFile.Read<int32_t>("MaxPotentialContacts");

    if (!maxContacts)
    {
        return std::unexpected("no MaxPotentialContacts in the PotentialContactManager block");
    }

    if (*maxContacts < 2)
    {
        return std::unexpected("Way too few contacts in Potential Contact Manager!");
    }

    return std::make_unique<MCContactSystem>(*maxContacts);
}

auto ContactSystem() -> MCContactSystem*
{
    return MCGameContext::Current().ContactSystem();
}

auto SensorSystemManager() -> MCSensorSystemManager*
{
    MCContactSystem* system = ContactSystem();
    return system != nullptr ? &system->Sensors : nullptr;
}

auto PotentialContactManager() -> MCPotentialContactManager*
{
    MCContactSystem* system = ContactSystem();
    return system != nullptr ? &system->Contacts : nullptr;
}
