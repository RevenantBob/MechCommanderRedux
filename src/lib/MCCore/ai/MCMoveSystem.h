#pragma once

#include "ai/MCGlobalMap.h"
#include "ai/MCMoveChunk.h"
#include "ai/MCMoveGeometry.h"
#include "ai/MCMoveMap.h"
#include "ai/MCMovePath.h"
#include "ai/MCMovePathManager.h"
#include "ai/MCObjectMap.h"
#include "ai/MCScenarioMap.h"
#include "lib/MCPriorityQueue.h"

/// <summary>
/// A mission's movement maps: the scenario map, which objects stand where, the long-range (door) map, the cell path
/// finder and the queue of path requests. A game system of <see cref="MCGameContext"/> (MoveSystem()), installed by
/// the scenario when it loads its terrain and removed when it ends.
/// </summary>
/// <remarks>The pieces were globals in the original (gameMap, GameObjectMap, GlobalMoveMap, the path finder,
/// PathManager, openList); a test can install a system with only the pieces it needs.</remarks>
class MCMoveSystem
{
public:
    /// <summary>Items the open list holds at first (the original's fixed capacity; it grows for a larger search).</summary>
    static constexpr int32_t OpenListItems = 5000;
    /// <summary>The open list's sentinel key, below every real key.</summary>
    static constexpr int32_t OpenListKeyMinimum = -2000000;

    MCMoveSystem();
    ~MCMoveSystem();
    MCMoveSystem(const MCMoveSystem&) = delete;
    MCMoveSystem& operator=(const MCMoveSystem&) = delete;

    /// <summary>
    /// Loads a scenario's maps: <paramref name="mapFile"/> (the terrain's .dat) and <paramref name="globalMapFile"/>
    /// (its .gmm), with a path finder window of <paramref name="pathWindow"/> tiles a side.
    /// </summary>
    static std::unique_ptr<MCMoveSystem> Load(MCFile& mapFile, MCFile& globalMapFile, int32_t pathWindow);

    /// <summary>
    /// The A* open list the path finder and the global map share. Declared first: the searches that point at it go
    /// before it.
    /// </summary>
    MCPriorityQueue OpenList{OpenListItems, OpenListKeyMinimum};
    std::unique_ptr<MCScenarioMap> Map;
    std::unique_ptr<MCObjectMap> ObjectMap;
    std::unique_ptr<MCMovePathManager> PathManager;
    std::unique_ptr<MCMoveMap> PathFinder;
    std::unique_ptr<MCGlobalMap> GlobalMap;
};

/// <summary>The mission's scenario map (null without one).</summary>
MCScenarioMap* GameMap();
/// <summary>The mission's object map (null without one).</summary>
MCObjectMap* GameObjectMap();
/// <summary>The mission's long-range map (null without one).</summary>
MCGlobalMap* GlobalMoveMap();
/// <summary>The mission's cell path finder (null without one).</summary>
MCMoveMap* PathFindMap();
/// <summary>The mission's path request queue (null without one).</summary>
MCMovePathManager* PathManager();
