#pragma once

#include "object/MCPotentialContact.h"
#include "object/MCSensorSystem.h"

class MCFitIniFile;

/// <summary>
/// The mission's sensing: the pool of sensor systems the movers and buildings carry, and the potential contacts they
/// can find. A game system of <see cref="MCGameContext"/> (ContactSystem()), made by the scenario and removed at its
/// end.
/// </summary>
/// <remarks>The two managers were globals in the original (SensorSystemManager, PotentialContactManager).</remarks>
class MCContactSystem
{
public:
    /// <summary>The sensors, and a contact pool of <paramref name="initialContacts"/> to begin with.</summary>
    explicit MCContactSystem(int32_t initialContacts) : Contacts(initialContacts) {}

    /// <summary>
    /// A system sized by the scenario's "PotentialContactManager" block ("MaxPotentialContacts"); the error names the
    /// missing entry.
    /// </summary>
    static std::expected<std::unique_ptr<MCContactSystem>, std::string> Create(MCFitIniFile& scenarioFile);

    /// <summary>The sensors. Declared first: the contacts go before them.</summary>
    MCSensorSystemManager Sensors;
    MCPotentialContactManager Contacts;
};

/// <summary>The mission's contact system (null outside a mission).</summary>
MCContactSystem* ContactSystem();
/// <summary>The sensor systems (null without a contact system).</summary>
MCSensorSystemManager* SensorSystemManager();
/// <summary>The potential contacts (null without a contact system).</summary>
MCPotentialContactManager* PotentialContactManager();
