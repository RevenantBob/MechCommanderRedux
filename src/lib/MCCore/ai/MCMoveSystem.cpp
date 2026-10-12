#include "stdafx.h"
#include "ai/MCMoveSystem.h"
#include "main/MCGameContext.h"

MCMoveSystem::MCMoveSystem() = default;

MCMoveSystem::~MCMoveSystem() = default;

auto MCMoveSystem::Load(MCFile& mapFile, MCFile& globalMapFile, int32_t pathWindow) -> std::unique_ptr<MCMoveSystem>
{
    auto system = std::make_unique<MCMoveSystem>();
    system->Map = std::make_unique<MCScenarioMap>(mapFile);
    system->ObjectMap = std::make_unique<MCObjectMap>(*system->Map);
    system->PathManager = std::make_unique<MCMovePathManager>();
    system->PathFinder = std::make_unique<MCMoveMap>(pathWindow, pathWindow, system->OpenList);
    system->GlobalMap = std::make_unique<MCGlobalMap>(globalMapFile, system->OpenList);
    return system;
}

namespace
{
    /// <summary>The installed move system (asserted).</summary>
    MCMoveSystem& System()
    {
        return MCGameContext::Current().MoveSystem();
    }
}

auto GameMap() -> MCScenarioMap&
{
    SDL_assert(System().Map != nullptr);
    return *System().Map;
}

auto GameObjectMap() -> MCObjectMap&
{
    SDL_assert(System().ObjectMap != nullptr);
    return *System().ObjectMap;
}

auto GlobalMoveMap() -> MCGlobalMap&
{
    SDL_assert(System().GlobalMap != nullptr);
    return *System().GlobalMap;
}

auto PathFindMap() -> MCMoveMap&
{
    SDL_assert(System().PathFinder != nullptr);
    return *System().PathFinder;
}

auto PathManager() -> MCMovePathManager&
{
    SDL_assert(System().PathManager != nullptr);
    return *System().PathManager;
}
