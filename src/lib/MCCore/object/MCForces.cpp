#include "stdafx.h"
#include "object/MCForces.h"
#include "main/MCGameContext.h"

MCForces::MCForces(bool allied)
{
    // The order the scenario made them in: clan, allied, Inner Sphere.
    Teams[ClanId] = std::make_shared<MCTeam>(ClanId, -1);

    if (allied)
    {
        Teams[AlliedId] = std::make_shared<MCTeam>(AlliedId, 1);
    }

    Teams[InnerSphereId] = std::make_shared<MCTeam>(InnerSphereId, 1);
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
        Commanders.push_back(std::make_shared<MCCommander>(i));
    }
}

auto Forces() -> MCForces&
{
    return MCGameContext::Current().Forces();
}

auto ClanTeam() -> std::shared_ptr<MCTeam>
{
    return TeamById(MCForces::ClanId);
}

auto AlliedTeam() -> std::shared_ptr<MCTeam>
{
    return TeamById(MCForces::AlliedId);
}

auto InnerSphereTeam() -> std::shared_ptr<MCTeam>
{
    return TeamById(MCForces::InnerSphereId);
}

auto HomeTeam() -> std::shared_ptr<MCTeam>
{
    const MCGameContext& context = MCGameContext::Current();
    return context.HasForces() ? context.Forces().PlayerTeam.lock() : nullptr;
}

auto TeamById(int32_t id) -> std::shared_ptr<MCTeam>
{
    const MCGameContext& context = MCGameContext::Current();
    return context.HasForces() ? context.Forces().Team(id) : nullptr;
}

auto NumCommanders() -> int32_t
{
    const MCGameContext& context = MCGameContext::Current();
    return context.HasForces() ? static_cast<int32_t>(context.Forces().Commanders.size()) : 0;
}

auto CommanderById(int32_t id) -> std::shared_ptr<MCCommander>
{
    // The original's table held six entries, null past the commanders in play.
    const MCGameContext& context = MCGameContext::Current();

    if (!context.HasForces() || id < 0 || id >= static_cast<int32_t>(context.Forces().Commanders.size()))
    {
        return nullptr;
    }

    return context.Forces().Commanders[id];
}

auto HomeCommander() -> std::shared_ptr<MCCommander>
{
    const MCGameContext& context = MCGameContext::Current();
    return context.HasForces() ? context.Forces().PlayerCommander.lock() : nullptr;
}
