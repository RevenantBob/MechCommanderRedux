#pragma once

// The object system's entry points and lists (original source: object\object.cpp).

class MCGameObject;
class MCObjectQueue;
class MCObjectTypeManager;
struct MCObjectQueueNode;

/// <summary>Makes an object of type <paramref name="typeId"/> (null for a negative id).</summary>
MCGameObject* CreateObject(int32_t typeId);
/// <summary>Kills <paramref name="object"/>; when kill answers 0xBEADDEAD (delete me), removes it from whichever
/// list holds it and deletes it.</summary>
void DestroyObject(MCGameObject* object);
/// <summary>
/// Starts the object system: the type manager (from <paramref name="objectFileName"/>, with at least 0x17ffff
/// bytes of type heap), the object list with its DEFAULT, clan mech, Inner Sphere mech, icon and weapon lists,
/// and the object watchers. Returns 0 or an error code.
/// </summary>
int32_t StartObjects(char* objectFileName, int32_t typeCacheSize, int32_t objectCacheSize, int32_t maxWatchers);
/// <summary>Deletes every object list and the object watchers (the original's name is lost; called from
/// Scenario::destroy).</summary>
void StopObjects();

/// <summary>Name of the first list, "DEFAULT".</summary>
extern char DefaultListId[];
/// <summary>Name of the clan mechs' list, "CLANMEC".</summary>
extern char ClanmechListId[];
/// <summary>Name of the Inner Sphere mechs' list, "ISMECH".</summary>
extern char IsmechListId[];
/// <summary>Name of the icons' list, "ICONS".</summary>
extern char IconListId[];
/// <summary>Name of the weapons' list, "WEAPON".</summary>
extern char WeaponListId[];

/// <summary>Every object list.</summary>
extern MCObjectQueue* ObjectList;
/// <summary>The clan's mechs.</summary>
extern MCObjectQueueNode* ClanMechList;
/// <summary>The Inner Sphere's mechs.</summary>
extern MCObjectQueueNode* InnerSphereMechList;
/// <summary>The icons' list (the original's name is lost).</summary>
extern MCObjectQueueNode* IconList;
/// <summary>The weapons (bullets, lasers, missiles) in flight.</summary>
extern MCObjectQueueNode* WeaponList;
/// <summary>The object type manager.</summary>
extern MCObjectTypeManager* ObjectTypeManager;
