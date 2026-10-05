#pragma once

#include "object/MCGameObject.h"
#include "object/MCObjectQueue.h"
#include "object/MCObjectTypeManager.h"

/// <summary>
/// The mission's objects: the types they are made from and the lists that own them. A game system of
/// <see cref="MCGameContext"/> (ObjectSystem()), started by the scenario and stopped at its end.
/// </summary>
/// <remarks>
/// Original source: <c>object\object.cpp</c> (startObjects, the stop the scenario inlines, and the globals objectList,
/// clanMechList, ISMechList, the icon and weapon lists, ObjectTypeManager and objectWatchers). The watchers, which
/// nulled the pointers to an object as it was deleted, are the <c>std::weak_ptr</c>s that refer to objects now.
/// </remarks>
class MCObjectSystem
{
public:
    /// <summary>The clan mechs' list.</summary>
    static constexpr std::string_view ClanMechListName = "CLANMEC";
    /// <summary>The Inner Sphere mechs' list.</summary>
    static constexpr std::string_view InnerSphereMechListName = "ISMECH";
    /// <summary>The icons' list.</summary>
    static constexpr std::string_view IconListName = "ICONS";
    /// <summary>The weapons' list.</summary>
    static constexpr std::string_view WeaponListName = "WEAPON";

    /// <summary>
    /// A system with the types of <paramref name="types"/> and the lists DEFAULT, CLANMEC, ISMECH, ICONS and WEAPON.
    /// </summary>
    explicit MCObjectSystem(std::unique_ptr<MCObjectTypeManager> types);
    ~MCObjectSystem();
    MCObjectSystem(const MCObjectSystem&) = delete;
    MCObjectSystem& operator=(const MCObjectSystem&) = delete;

    /// <summary>
    /// Installs a system whose types come from the object packet file <paramref name="objectFileName"/>; a failure
    /// is Fatal.
    /// </summary>
    static void Start(std::string_view objectFileName);
    /// <summary>
    /// Takes the installed system's lists and their objects down while it is still installed (the objects reach for
    /// it as they go), then removes it with its types.
    /// </summary>
    static void Stop();
    /// <summary>Deletes the lists and their objects, first to last.</summary>
    void Unload();

    /// <summary>The types. Declared first: they go after the objects made from them.</summary>
    std::unique_ptr<MCObjectTypeManager> Types;
    /// <summary>The object lists.</summary>
    std::shared_ptr<MCObjectQueue> Lists;
    /// <summary>The clan mechs' list (one of <see cref="Lists"/>).</summary>
    std::shared_ptr<MCObjectList> ClanMechs;
    /// <summary>The Inner Sphere mechs' list (one of <see cref="Lists"/>).</summary>
    std::shared_ptr<MCObjectList> InnerSphereMechs;
    /// <summary>The icons' list (one of <see cref="Lists"/>).</summary>
    std::shared_ptr<MCObjectList> Icons;
    /// <summary>The weapons' list (one of <see cref="Lists"/>).</summary>
    std::shared_ptr<MCObjectList> Weapons;
};

/// <summary>The mission's object system (null outside a mission).</summary>
std::shared_ptr<MCObjectSystem> ObjectSystem();
/// <summary>Every object list (null without an object system).</summary>
std::shared_ptr<MCObjectQueue> ObjectList();
/// <summary>The clan's mechs (null without an object system).</summary>
std::shared_ptr<MCObjectList> ClanMechList();
/// <summary>The Inner Sphere's mechs (null without an object system).</summary>
std::shared_ptr<MCObjectList> InnerSphereMechList();
/// <summary>The weapons (bullets, lasers, missiles) in flight (null without an object system).</summary>
std::shared_ptr<MCObjectList> WeaponList();
/// <summary>The object types (asserts an object system).</summary>
MCObjectTypeManager& ObjectTypeManager();
/// <summary>Makes an object of type <paramref name="typeId"/> (null for a negative id).</summary>
std::shared_ptr<MCGameObject> CreateObject(int32_t typeId);
/// <summary>Makes an object of type <paramref name="typeId"/> as <typeparamref name="T"/>.</summary>
template <typename T> std::shared_ptr<T> CreateObjectAs(int32_t typeId)
{
    return std::static_pointer_cast<T>(CreateObject(typeId));
}

/// <summary>
/// Puts <paramref name="object"/> at the end of the first object list (the effects' and loose objects'); returns it.
/// </summary>
std::shared_ptr<MCGameObject> AddToDefaultList(std::shared_ptr<MCGameObject> object);
/// <summary>Kills <paramref name="object"/>; when kill answers 0xBEADDEAD (delete me), the list that holds it
/// lets it go.</summary>
void DestroyObject(MCGameObject& object);
