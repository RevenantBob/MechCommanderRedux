#include "stdafx.h"
#include "object/MCForces.h"
#include "main/MCGameContext.h"

MCForces::MCForces(bool allied)
{
    // The order the scenario made them in: clan, allied, Inner Sphere.
    Teams[ClanId] = std::make_unique<MCTeam>(ClanId, -1);

    if (allied)
    {
        Teams[AlliedId] = std::make_unique<MCTeam>(AlliedId, 1);
    }

    Teams[InnerSphereId] = std::make_unique<MCTeam>(InnerSphereId, 1);
}

MCForces::~MCForces()
{
    // The commanders went before the teams.
    Commanders.clear();
}

auto MCForces::MakeCommanders(int32_t count) -> void
{
    Commanders.clear();

    for (int32_t i = 0; i < count; i++)
    {
        Commanders.push_back(std::make_unique<MCCommander>(i));
    }
}

auto Forces() -> MCForces*
{
    return MCGameContext::Current().Forces();
}

auto ClanTeam() -> MCTeam*
{
    return TeamById(MCForces::ClanId);
}

auto AlliedTeam() -> MCTeam*
{
    return TeamById(MCForces::AlliedId);
}

auto InnerSphereTeam() -> MCTeam*
{
    return TeamById(MCForces::InnerSphereId);
}

auto HomeTeam() -> MCTeam*
{
    MCForces* forces = Forces();
    return forces != nullptr ? forces->PlayerTeam : nullptr;
}

auto TeamById(int32_t id) -> MCTeam*
{
    MCForces* forces = Forces();
    return forces != nullptr ? forces->Team(id) : nullptr;
}

auto NumCommanders() -> int32_t
{
    MCForces* forces = Forces();
    return forces != nullptr ? static_cast<int32_t>(forces->Commanders.size()) : 0;
}

auto CommanderById(int32_t id) -> MCCommander*
{
    // The original's table held six entries, null past the commanders in play.
    MCForces* forces = Forces();

    if (forces == nullptr || id < 0 || id >= static_cast<int32_t>(forces->Commanders.size()))
    {
        return nullptr;
    }

    return forces->Commanders[id].get();
}

auto HomeCommander() -> MCCommander*
{
    MCForces* forces = Forces();
    return forces != nullptr ? forces->PlayerCommander : nullptr;
}
