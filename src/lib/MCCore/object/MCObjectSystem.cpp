#include "stdafx.h"
#include "object/MCObjectSystem.h"
#include "lib/MCFatal.h"
#include "main/MCGameContext.h"
#include "object/MCGameObject.h"

MCObjectSystem::MCObjectSystem(std::unique_ptr<MCObjectTypeManager> types)
    : Types(std::move(types)), Lists(std::make_shared<MCObjectQueue>())
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
    // The lists are reachable while their objects go, as the original's globals were: the queue empties them first
    // to last, then the named ones are let go.
    Lists.reset();
    ClanMechs.reset();
    InnerSphereMechs.reset();
    Icons.reset();
    Weapons.reset();
}

auto MCObjectSystem::Start(std::string_view objectFileName) -> void
{
    std::expected<std::unique_ptr<MCObjectTypeManager>, std::string> types =
        MCObjectTypeManager::Create(objectFileName);

    if (!types)
    {
        Fatal(0, std::format(" could not Start ObjectSystem: {} ", types.error()));
    }

    MCGameContext::Current().SetObjectSystem(std::make_shared<MCObjectSystem>(std::move(*types)));
}

auto MCObjectSystem::Stop() -> void
{
    if (std::shared_ptr<MCObjectSystem> system = ObjectSystem())
    {
        system->Unload();
    }

    MCGameContext::Current().SetObjectSystem(nullptr);
}

auto ObjectSystem() -> std::shared_ptr<MCObjectSystem>
{
    return MCGameContext::Current().ObjectSystem();
}

auto ObjectList() -> std::shared_ptr<MCObjectQueue>
{
    std::shared_ptr<MCObjectSystem> system = ObjectSystem();
    return system != nullptr ? system->Lists : nullptr;
}

auto ClanMechList() -> std::shared_ptr<MCObjectList>
{
    std::shared_ptr<MCObjectSystem> system = ObjectSystem();
    return system != nullptr ? system->ClanMechs : nullptr;
}

auto InnerSphereMechList() -> std::shared_ptr<MCObjectList>
{
    std::shared_ptr<MCObjectSystem> system = ObjectSystem();
    return system != nullptr ? system->InnerSphereMechs : nullptr;
}

auto WeaponList() -> std::shared_ptr<MCObjectList>
{
    std::shared_ptr<MCObjectSystem> system = ObjectSystem();
    return system != nullptr ? system->Weapons : nullptr;
}

auto ObjectTypeManager() -> MCObjectTypeManager&
{
    std::shared_ptr<MCObjectSystem> system = ObjectSystem();
    SDL_assert(system != nullptr);
    return *system->Types;
}

auto CreateObject(int32_t typeId) -> std::shared_ptr<MCGameObject>
{
    if (typeId < 0)
    {
        return nullptr;
    }

    return std::static_pointer_cast<MCGameObject>(ObjectTypeManager().Get(typeId));
}

auto AddToDefaultList(std::shared_ptr<MCGameObject> object) -> std::shared_ptr<MCGameObject>
{
    return std::static_pointer_cast<MCGameObject>(ObjectList()->DefaultList().Add(std::move(object)));
}

auto DestroyObject(MCGameObject& object) -> void
{
    if (static_cast<uint32_t>(object.Kill()) == 0xbeaddead)
    {
        ObjectList()->Remove(object);
    }
}
