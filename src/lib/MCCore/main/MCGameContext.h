#pragma once

#include "platform/MCServices.h"

class MCAblRuntime;
class MCAblSymbolTable;
class MCAppearanceTypeList;
class MCCameraList;
class MCCollisionSystem;
class MCContactSystem;
class MCCraterManager;
class MCElementBuffer;
class MCFastFileSet;
class MCForces;
class MCMoveSystem;
class MCObjectSystem;
class MCPalette;
class MCSpriteManager;
class MCTerrain;

/// <summary>
/// Everything the game reaches for that a test may want to replace: the port services (clock, dice, files, sound
/// output, network) and, as the modernization moves them here, the game's own systems. Game code asks
/// <see cref="Current"/> for them instead of reading a global.
/// </summary>
/// <remarks>
/// <para>A context owns the services it was given. A service it wasn't given comes from the context it was installed
/// over (<see cref="MCTestContextScope"/>), so a test replaces only what it needs. The first context, made on first
/// use, holds the real services and lives until the process ends.</para>
/// <para>The game's own systems (the map, the scenario, the object manager, ...) get their slots here as they move
/// out of globals.</para>
/// </remarks>
class MCGameContext
{
public:
    /// <summary>The context the game runs in now.</summary>
    static MCGameContext& Current();

    /// <summary>A context with no services of its own: each comes from <paramref name="parent"/>.</summary>
    explicit MCGameContext(MCGameContext* parent);

    ~MCGameContext();
    MCGameContext(const MCGameContext&) = delete;
    MCGameContext& operator=(const MCGameContext&) = delete;

    /// <summary>The clocks.</summary>
    MCClock& Clock() const;

    /// <summary>The dice.</summary>
    MCRandom& Random() const;

    /// <summary>Where the game's files come from.</summary>
    MCFileSource& Files() const;

    /// <summary>The sound output.</summary>
    MCAudioDevice& Audio() const;

    /// <summary>The network.</summary>
    MCNetTransport& Net() const;

    /// <summary>The FastFiles the game has open.</summary>
    MCFastFileSet& FastFiles() const;

    /// <summary>The palette the game shows (null before the interface starts).</summary>
    MCPalette* Palette() const;

    /// <summary>The frame's draw list (null outside a mission).</summary>
    MCElementBuffer* ElementList() const;

    /// <summary>The mission's craters (null outside a mission).</summary>
    MCCraterManager* CraterManager() const;

    /// <summary>The mission's sprite cache (null outside a mission).</summary>
    MCSpriteManager* SpriteManager() const;

    /// <summary>The mission's appearance types (null outside a mission).</summary>
    MCAppearanceTypeList* AppearanceTypeList() const;

    /// <summary>The mission's terrain (null outside a mission).</summary>
    MCTerrain* Terrain() const;

    /// <summary>The mission's cameras (null outside a mission).</summary>
    MCCameraList* CameraList() const;

    /// <summary>The mission's movement maps and path finders (null outside a mission).</summary>
    MCMoveSystem* MoveSystem() const;

    /// <summary>The mission's sides: its teams and commanders (null outside a mission).</summary>
    MCForces* Forces() const;

    /// <summary>The mission's sensors and potential contacts (null outside a mission).</summary>
    MCContactSystem* ContactSystem() const;

    /// <summary>The mission's collision system (null outside a mission).</summary>
    MCCollisionSystem* CollisionSystem() const;

    /// <summary>The mission's objects: their types, lists and watchers (null outside a mission).</summary>
    MCObjectSystem* ObjectSystem() const;

    /// <summary>ABL's symbols and types (null outside AblInit .. AblClose).</summary>
    MCAblSymbolTable* AblSymbols() const;

    /// <summary>ABL's runtime: its modules, stack and interpreter (null outside AblInit .. AblClose).</summary>
    MCAblRuntime* AblRuntime() const;

    /// <summary>Gives this context its own clock.</summary>
    /// <returns>The clock, still reachable as its own type.</returns>
    template <std::derived_from<MCClock> T> T& SetClock(std::unique_ptr<T> clock)
    {
        return Install(_Clock, std::move(clock));
    }

    /// <summary>Gives this context its own dice.</summary>
    template <std::derived_from<MCRandom> T> T& SetRandom(std::unique_ptr<T> random)
    {
        return Install(_Random, std::move(random));
    }

    /// <summary>Gives this context its own file source.</summary>
    template <std::derived_from<MCFileSource> T> T& SetFiles(std::unique_ptr<T> files)
    {
        return Install(_Files, std::move(files));
    }

    /// <summary>Gives this context its own sound output.</summary>
    template <std::derived_from<MCAudioDevice> T> T& SetAudio(std::unique_ptr<T> audio)
    {
        return Install(_Audio, std::move(audio));
    }

    /// <summary>Gives this context its own network.</summary>
    template <std::derived_from<MCNetTransport> T> T& SetNet(std::unique_ptr<T> net)
    {
        return Install(_Net, std::move(net));
    }

    /// <summary>Gives this context its own set of FastFiles.</summary>
    MCFastFileSet& SetFastFiles(std::unique_ptr<MCFastFileSet> fastFiles);

    /// <summary>Gives this context its own palette (null: the one it was installed over, if any).</summary>
    /// <returns>The palette this context had.</returns>
    std::unique_ptr<MCPalette> SetPalette(std::unique_ptr<MCPalette> palette);

    /// <summary>Gives this context its own draw list (null: the one it was installed over, if any).</summary>
    /// <returns>The draw list this context had.</returns>
    std::unique_ptr<MCElementBuffer> SetElementList(std::unique_ptr<MCElementBuffer> elementList);

    /// <summary>Gives this context its own craters (null: the ones it was installed over, if any).</summary>
    /// <returns>The craters this context had.</returns>
    std::unique_ptr<MCCraterManager> SetCraterManager(std::unique_ptr<MCCraterManager> craterManager);

    /// <summary>Gives this context its own sprite cache (null: the one it was installed over, if any).</summary>
    /// <returns>The sprite cache this context had.</returns>
    std::unique_ptr<MCSpriteManager> SetSpriteManager(std::unique_ptr<MCSpriteManager> spriteManager);

    /// <summary>Gives this context its own appearance types (null: the ones it was installed over, if any).</summary>
    /// <returns>The appearance types this context had.</returns>
    std::unique_ptr<MCAppearanceTypeList> SetAppearanceTypeList(std::unique_ptr<MCAppearanceTypeList> typeList);

    /// <summary>Gives this context its own terrain (null: the one it was installed over, if any).</summary>
    /// <returns>The terrain this context had.</returns>
    std::unique_ptr<MCTerrain> SetTerrain(std::unique_ptr<MCTerrain> terrain);

    /// <summary>Gives this context its own cameras (null: the ones it was installed over, if any).</summary>
    /// <returns>The cameras this context had.</returns>
    std::unique_ptr<MCCameraList> SetCameraList(std::unique_ptr<MCCameraList> cameraList);

    /// <summary>Gives this context its own movement maps (null: the ones it was installed over, if any).</summary>
    /// <returns>The movement maps this context had.</returns>
    std::unique_ptr<MCMoveSystem> SetMoveSystem(std::unique_ptr<MCMoveSystem> moveSystem);

    /// <summary>Gives this context its own forces (null: the ones it was installed over, if any).</summary>
    /// <returns>The forces this context had.</returns>
    std::unique_ptr<MCForces> SetForces(std::unique_ptr<MCForces> forces);

    /// <summary>Gives this context its own sensors and contacts (null: the ones it was installed over, if any).</summary>
    /// <returns>The contact system this context had.</returns>
    std::unique_ptr<MCContactSystem> SetContactSystem(std::unique_ptr<MCContactSystem> contactSystem);

    /// <summary>Gives this context its own collision system (null: the one it was installed over, if any).</summary>
    /// <returns>The collision system this context had.</returns>
    std::unique_ptr<MCCollisionSystem> SetCollisionSystem(std::unique_ptr<MCCollisionSystem> collisionSystem);

    /// <summary>Gives this context its own object system (null: the one it was installed over, if any).</summary>
    /// <returns>The object system this context had.</returns>
    std::unique_ptr<MCObjectSystem> SetObjectSystem(std::unique_ptr<MCObjectSystem> objectSystem);

    /// <summary>Gives this context its own ABL symbol table (null: the one it was installed over, if any).</summary>
    /// <returns>The symbol table this context had.</returns>
    std::unique_ptr<MCAblSymbolTable> SetAblSymbols(std::unique_ptr<MCAblSymbolTable> symbols);

    /// <summary>Installs ABL's runtime (AblInit); returns the one it replaces.</summary>
    std::unique_ptr<MCAblRuntime> SetAblRuntime(std::unique_ptr<MCAblRuntime> runtime);

private:
    friend class MCTestContextScope;

    /// <summary>Stores <paramref name="service"/> in <paramref name="slot"/> and returns it.</summary>
    template <typename Base, typename T> static T& Install(std::unique_ptr<Base>& slot, std::unique_ptr<T> service)
    {
        T& installed = *service;
        slot = std::move(service);
        return installed;
    }

    /// <summary>Makes <paramref name="context"/> the current one.</summary>
    static void SetCurrent(MCGameContext* context);

    /// <summary>Where the services this context doesn't own come from; null only for the first context.</summary>
    MCGameContext* _Parent = nullptr;
    std::unique_ptr<MCClock> _Clock;
    std::unique_ptr<MCRandom> _Random;
    std::unique_ptr<MCFileSource> _Files;
    std::unique_ptr<MCAudioDevice> _Audio;
    std::unique_ptr<MCNetTransport> _Net;
    std::unique_ptr<MCFastFileSet> _FastFiles;
    std::unique_ptr<MCPalette> _Palette;
    std::unique_ptr<MCElementBuffer> _ElementList;
    std::unique_ptr<MCCraterManager> _CraterManager;
    std::unique_ptr<MCSpriteManager> _SpriteManager;
    std::unique_ptr<MCAppearanceTypeList> _AppearanceTypeList;
    std::unique_ptr<MCTerrain> _Terrain;
    std::unique_ptr<MCCameraList> _CameraList;
    std::unique_ptr<MCMoveSystem> _MoveSystem;
    std::unique_ptr<MCForces> _Forces;
    std::unique_ptr<MCContactSystem> _ContactSystem;
    std::unique_ptr<MCCollisionSystem> _CollisionSystem;
    /// <summary>After the systems its objects reach for as they go: it goes before them.</summary>
    std::unique_ptr<MCObjectSystem> _ObjectSystem;
    std::unique_ptr<MCAblSymbolTable> _AblSymbols;
    /// <summary>Declared after the symbols: it goes first (its modules' watches point into the symbols).</summary>
    std::unique_ptr<MCAblRuntime> _AblRuntime;
};

/// <summary>
/// Installs a fresh context over the current one for its lifetime, then puts the previous one back. Its services
/// come from the previous context until the test gives it its own (<see cref="Context"/>). Scopes nest, and must end
/// in the reverse order they began.
/// </summary>
class MCTestContextScope
{
public:
    MCTestContextScope();
    ~MCTestContextScope();
    MCTestContextScope(const MCTestContextScope&) = delete;
    MCTestContextScope& operator=(const MCTestContextScope&) = delete;

    /// <summary>The installed context, to give it services.</summary>
    MCGameContext& Context() { return *_Context; }

private:
    MCGameContext* _Previous = nullptr;
    /// <summary>Deleted while still current: the objects of its systems reach for them as they go.</summary>
    std::unique_ptr<MCGameContext> _Context;
};
