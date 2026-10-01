#pragma once

// The object system's entry points and lists (original source: object\object.cpp).

class GameObject;
class ObjectQueue;
class ObjectTypeManager;
struct ObjectQueueNode;

/// <summary>Makes an object of type <paramref name="typeId"/> (null for a negative id).</summary>
/// <remarks>MCX.EXE @ 0x0068dea0</remarks>
GameObject* createObject(int32_t typeId);
/// <summary>Kills <paramref name="object"/>; when kill answers 0xBEADDEAD (delete me), removes it from whichever
/// list holds it and deletes it.</summary>
/// <remarks>MCX.EXE @ 0x0068dec0</remarks>
void destroyObject(GameObject* object);
/// <summary>
/// Starts the object system: the type manager (from <paramref name="objectFileName"/>, with at least 0x17ffff
/// bytes of type heap), the object list with its DEFAULT, clan mech, Inner Sphere mech, icon and weapon lists,
/// and the object watchers. Returns 0 or an error code.
/// </summary>
/// <remarks>MCX.EXE @ 0x0068df00</remarks>
int32_t startObjects(char* objectFileName, int32_t typeCacheSize, int32_t objectCacheSize, int32_t maxWatchers);
/// <summary>Deletes every object list and the object watchers (the original's name is lost; called from
/// Scenario::destroy).</summary>
/// <remarks>MCX.EXE @ 0x0068e320</remarks>
void stopObjects();

/// <summary>Name of the first list, "DEFAULT".</summary>
extern char DEFAULT_LIST_ID[];
/// <summary>Name of the clan mechs' list, "CLANMEC".</summary>
extern char CLANMECH_LIST_ID[];
/// <summary>Name of the Inner Sphere mechs' list, "ISMECH".</summary>
extern char ISMECH_LIST_ID[];
/// <summary>Name of the icons' list, "ICONS".</summary>
extern char ICON_LIST_ID[];
/// <summary>Name of the weapons' list, "WEAPON".</summary>
extern char WEAPON_LIST_ID[];

/// <summary>Every object list.</summary>
extern ObjectQueue* objectList;
/// <summary>The clan's mechs.</summary>
extern ObjectQueueNode* clanMechList;
/// <summary>The Inner Sphere's mechs.</summary>
extern ObjectQueueNode* innerSphereMechList;
/// <summary>The icons' list (the original's name is lost).</summary>
extern ObjectQueueNode* iconList;
/// <summary>The weapons (bullets, lasers, missiles) in flight.</summary>
extern ObjectQueueNode* weaponList;
/// <summary>The object type manager.</summary>
extern ObjectTypeManager* objectTypeManager;
