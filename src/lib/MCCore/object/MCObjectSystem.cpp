#include "stdafx.h"
#include "object/MCObjectSystem.h"
#include "lib/MCFatal.h"
#include "main/MCGameContext.h"
#include "object/MCGameObject.h"

MCObjectSystem::MCObjectSystem(std::unique_ptr<MCObjectTypeManager> types)
    : Types(std::move(types)), Lists(std::make_unique<MCObjectQueue>())
{
    ClanMechs = Lists->FindOrAddList(ClanMechListName);
    InnerSphereMechs = Lists->FindOrAddList(InnerSphereMechListName);
    Icons = Lists->FindOrAddList(IconListName);
    Weapons = Lists->FindOrAddList(WeaponListName);
}

MCObjectSystem::~MCObjectSystem()
{
    Unload();
}

auto MCObjectSystem::Unload() -> void
{
    // The lists are reachable while their objects go, as the original's globals were.
    Lists.reset();
    ClanMechs = nullptr;
    InnerSphereMechs = nullptr;
    Icons = nullptr;
    Weapons = nullptr;
}

auto MCObjectSystem::Start(std::string_view objectFileName) -> void
{
    std::expected<std::unique_ptr<MCObjectTypeManager>, std::string> types =
        MCObjectTypeManager::Create(objectFileName);

    if (!types)
    {
        Fatal(0, std::format(" could not Start ObjectSystem: {} ", types.error()));
    }

    MCGameContext::Current().SetObjectSystem(std::make_unique<MCObjectSystem>(std::move(*types)));
}

auto MCObjectSystem::Stop() -> void
{
    if (MCObjectSystem* system = ObjectSystem())
    {
        system->Unload();
    }

    MCGameContext::Current().SetObjectSystem(nullptr);
}

auto ObjectSystem() -> MCObjectSystem*
{
    return MCGameContext::Current().ObjectSystem();
}

auto ObjectList() -> MCObjectQueue*
{
    MCObjectSystem* system = ObjectSystem();
    return system != nullptr ? system->Lists.get() : nullptr;
}

auto ClanMechList() -> MCObjectList*
{
    MCObjectSystem* system = ObjectSystem();
    return system != nullptr ? system->ClanMechs : nullptr;
}

auto InnerSphereMechList() -> MCObjectList*
{
    MCObjectSystem* system = ObjectSystem();
    return system != nullptr ? system->InnerSphereMechs : nullptr;
}

auto IconList() -> MCObjectList*
{
    MCObjectSystem* system = ObjectSystem();
    return system != nullptr ? system->Icons : nullptr;
}

auto WeaponList() -> MCObjectList*
{
    MCObjectSystem* system = ObjectSystem();
    return system != nullptr ? system->Weapons : nullptr;
}

auto ObjectTypeManager() -> MCObjectTypeManager*
{
    MCObjectSystem* system = ObjectSystem();
    return system != nullptr ? system->Types.get() : nullptr;
}

auto ObjectWatchers() -> MCObjectWatcherList*
{
    MCObjectSystem* system = ObjectSystem();
    return system != nullptr ? &system->Watchers : nullptr;
}

auto CreateObject(int32_t typeId) -> std::unique_ptr<MCGameObject>
{
    if (typeId < 0)
    {
        return nullptr;
    }

    return std::unique_ptr<MCGameObject>(static_cast<MCGameObject*>(ObjectTypeManager()->Get(typeId).release()));
}

auto AddToDefaultList(std::unique_ptr<MCGameObject> object) -> MCGameObject*
{
    return static_cast<MCGameObject*>(ObjectList()->DefaultList().Add(std::move(object)));
}

auto DestroyObject(MCGameObject* object) -> void
{
    if (static_cast<uint32_t>(object->Kill()) == 0xbeaddead)
    {
        ObjectList()->Remove(object);
    }
}
