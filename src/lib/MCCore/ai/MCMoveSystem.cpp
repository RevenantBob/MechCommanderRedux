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
    /// <summary>The installed move system, null when there is none.</summary>
    MCMoveSystem* System()
    {
        return MCGameContext::Current().MoveSystem();
    }
}

auto GameMap() -> MCScenarioMap*
{
    MCMoveSystem* system = System();
    return system != nullptr ? system->Map.get() : nullptr;
}

auto GameObjectMap() -> MCObjectMap*
{
    MCMoveSystem* system = System();
    return system != nullptr ? system->ObjectMap.get() : nullptr;
}

auto GlobalMoveMap() -> MCGlobalMap*
{
    MCMoveSystem* system = System();
    return system != nullptr ? system->GlobalMap.get() : nullptr;
}

auto PathFindMap() -> MCMoveMap*
{
    MCMoveSystem* system = System();
    return system != nullptr ? system->PathFinder.get() : nullptr;
}

auto PathManager() -> MCMovePathManager*
{
    MCMoveSystem* system = System();
    return system != nullptr ? system->PathManager.get() : nullptr;
}
