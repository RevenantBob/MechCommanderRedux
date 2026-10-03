#pragma once

// Original source: mcx\logistics.cpp, the logistics (between-missions) layer: the player's mechs, vehicles, pilots
// and components, the screens that buy, repair and deploy them, campaign loading and saving, and the multiplayer
// force exchange.

#include "linkup/linkedlist.h"
#include "logistics/lport.h"

class aEvent;
class aObject;
class BriefingBox;
class BriefingScreen;
class CompInventoryBlock;
class CompPurchaseBlock;
class DragIcon;
class FIDPMessage;
class FIMessageHeader;
class FitIniFile;
class LogChatWindow;
class LogInvScreen;
class MCSplashScreen;
class MechBriefBlock;
class MechInventoryBlock;
class MechRepairBlock;
class PacketFile;
class PilotInventoryBlock;
class PurchaseDlg;
class PurchaseScreen;
class PurMechList;
class PurPilotList;
class PurVehicleList;
class RefitDialog;
class RepairScreen;
class ReusableDialog;
class SessionScreen;
class Ticker;
class UserHeap;
class VehicleInventoryBlock;
class VehicleRepairBlock;
class LogWarrior;

/// <summary>
/// One copy of a component in an inventory: where it sits and its state. The copies of one component type form a
/// list under their <see cref="_LogInventoryItem"/>, sorted by <see cref="itemNum"/>.
/// </summary>
/// <remarks>Original source: <c>logistics.cpp</c>, 0x1c bytes (<see cref="InventoryList::createStat"/>).</remarks>
struct _LogInventoryStat
{
    /// <summary>A number unique within the inventory (<see cref="InventoryList::nextStatID"/>).</summary>
    uint8_t statID; // +0x0
    /// <summary>Its damage (<see cref="InventoryList::hitItem"/>).</summary>
    uint8_t hits; // +0x1
    /// <summary>Set from <see cref="InventoryList::createStat"/>'s third argument; meaning unknown.</summary>
    int32_t unknown04;    // +0x4
    uint8_t unknown08[4]; // +0x8
    /// <summary>Nonzero when the weapon faces forward (the mech file's FacesForward).</summary>
    uint8_t facing; // +0xc
    /// <summary>Set from <see cref="InventoryList::createStat"/>'s fifth argument; meaning unknown.</summary>
    int16_t unknown0E; // +0xe
    /// <summary>The amount (ammunition count, or 1).</summary>
    int16_t amount; // +0x10
    /// <summary>The body location it is mounted in (0xff = none; <see cref="InventoryList::setStatLoc"/>).</summary>
    uint8_t location; // +0x12
    /// <summary>The item number in the mech/vehicle file; the list is sorted by it.</summary>
    int32_t itemNum;         // +0x14
    _LogInventoryStat* next; // +0x18
};

/// <summary>
/// A component type in an inventory: its master component, how many copies there are (the <see cref="stats"/>
/// list) and the widgets that show it on the purchase and inventory screens.
/// </summary>
/// <remarks>Original source: <c>logistics.cpp</c>, 0x48 bytes (<see cref="InventoryList::addItem"/>).</remarks>
struct _LogInventoryItem
{
    /// <summary>The index into <c>MasterComponentList</c>.</summary>
    uint8_t masterID; // +0x0
    /// <summary>A copy of the master component's first field (+0x0).</summary>
    int32_t masterValue; // +0x4
    /// <summary>The component's place in the logistics sort order (<see cref="Logistics::componentSort"/>).</summary>
    int32_t sortOrder; // +0x8
    /// <summary>The master component's name (28 characters and a terminator).</summary>
    char name[29]; // +0xc
    /// <summary>How many copies there are.</summary>
    int32_t count; // +0x2c
    /// <summary>The index of the component in <see cref="Logistics::rangeSortList"/>.</summary>
    int32_t rangeIndex; // +0x30
    /// <summary>The description text (loaded on demand by <see cref="InventoryList::loadDescription"/>).</summary>
    char* description;                  // +0x34
    _LogInventoryStat* stats;           // +0x38
    CompPurchaseBlock* purchaseBlock;   // +0x3c
    CompInventoryBlock* inventoryBlock; // +0x40
    _LogInventoryItem* next;            // +0x44
};

/// <summary>A list of components, sorted by master id, each with its copies.</summary>
/// <remarks>Original source: <c>logistics.cpp</c>, 0xc bytes.</remarks>
class InventoryList
{
public:
    /// <remarks>MCX.EXE @ 0x006e7980</remarks>
    InventoryList();

    /// <remarks>MCX.EXE @ 0x006e7c00</remarks>
    static void* operator new(size_t size) noexcept;
    /// <remarks>MCX.EXE @ 0x006e7c20</remarks>
    static void operator delete(void* ptr);

    /// <summary>Loads the description of item <paramref name="index"/> (or <paramref name="item"/>) from the object description file.</summary>
    /// <remarks>MCX.EXE @ 0x006e7990</remarks>
    void loadDescription(int32_t index, _LogInventoryItem* item);

    /// <summary>Makes a copy record numbered with the next <see cref="nextStatID"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006e7af0</remarks>
    _LogInventoryStat* createStat(uint8_t itemNum, uint8_t hits, int unknown, uint8_t facing, int16_t unknown2,
                                  int16_t amount, uint8_t location);

    /// <summary>The item at list position <paramref name="index"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006e7b80</remarks>
    _LogInventoryItem* getItemInfo(int32_t index);

    /// <summary>Adds <paramref name="count"/> to the count of the item with master id <paramref name="masterID"/> (not below 0).</summary>
    /// <remarks>MCX.EXE @ 0x006e7bb0</remarks>
    void addCountToItem(int32_t count, int32_t masterID);

    /// <summary>Frees every item, its copies and its widgets.</summary>
    /// <remarks>MCX.EXE @ 0x006e7c40</remarks>
    void destroy();

    /// <summary>
    /// Adds a copy of component <paramref name="masterID"/>; a new item gets purchase and inventory widgets unless
    /// <paramref name="widgets"/> is -1.
    /// </summary>
    /// <returns>The copy's stat id.</returns>
    /// <remarks>MCX.EXE @ 0x006e7ce0</remarks>
    int32_t addItem(uint8_t masterID, _LogInventoryStat* stat, int32_t widgets);

    /// <summary>Removes copy <paramref name="statID"/> of component <paramref name="masterID"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006e80a0</remarks>
    int32_t removeItem(uint8_t masterID, int32_t statID);

    /// <summary>The item holding the copy with stat id <paramref name="statID"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006e81d0</remarks>
    _LogInventoryItem* getItemStatIndex(int32_t statID);

    /// <summary>The stat id of copy <paramref name="copy"/> of component <paramref name="masterID"/>, or -1.</summary>
    /// <remarks>MCX.EXE @ 0x006e8210</remarks>
    int32_t getItemStatID(uint8_t masterID, int32_t copy);

    /// <summary>The list position of component <paramref name="masterID"/>, or -1.</summary>
    /// <remarks>MCX.EXE @ 0x006e8260</remarks>
    int32_t getIndexFromMasterID(uint8_t masterID);

    /// <summary>The master id of the item at list position <paramref name="index"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006e8290</remarks>
    int32_t getMasterIDFromIndex(int32_t index);

    /// <summary>The master id of the item holding copy <paramref name="statID"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006e82c0</remarks>
    uint8_t getMasterID(uint8_t statID);

    /// <summary>The facing of copy <paramref name="statID"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006e8320</remarks>
    uint8_t getFacing(uint8_t statID);

    /// <summary>The amount of copy <paramref name="statID"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006e8380</remarks>
    int32_t getAmount(uint8_t statID);

    /// <summary>The name of the item at list position <paramref name="masterID"/> (a position, despite the name).</summary>
    /// <remarks>MCX.EXE @ 0x006e83f0</remarks>
    char* getItemName(uint8_t masterID);

    /// <summary>How many copies of component <paramref name="masterID"/> there are.</summary>
    /// <remarks>MCX.EXE @ 0x006e8420</remarks>
    int32_t getItemCount(uint8_t masterID);

    /// <summary>Sets the damage of copy <paramref name="statID"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006e8450</remarks>
    int32_t hitItem(uint8_t statID, uint8_t hits);

    /// <summary>Sets the location of copy <paramref name="statID"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006e84c0</remarks>
    int32_t setStatLoc(uint8_t statID, int32_t location);

    /// <summary>
    /// Writes the inventory into <paramref name="data"/> (null only measures it): the item count, then per item its
    /// master id (1 byte), copy count (4 bytes) and each copy's 0x1c-byte record.
    /// </summary>
    /// <returns>The size of the data when measuring; 4 when writing.</returns>
    /// <remarks>MCX.EXE @ 0x006e8530</remarks>
    int32_t getBinaryData(void* data);

    /// <summary>The item positions sorted by the components' range.</summary>
    /// <remarks>MCX.EXE @ 0x006e85e0</remarks>
    int32_t* sortRange();

    /// <summary>The item positions sorted by name.</summary>
    /// <remarks>MCX.EXE @ 0x006e8a70</remarks>
    int32_t* sortName();

    _LogInventoryItem* items; // +0x0
    int32_t numItems;         // +0x4
    /// <summary>The stat id the next copy gets.</summary>
    uint8_t nextStatID; // +0x8
};

/// <summary>
/// What mechs and vehicles have in common in logistics: identity, tonnage, cost, status flags, inventory and the
/// briefing box. <see cref="partType"/> tells which one a part is.
/// </summary>
/// <remarks>
/// Original source: <c>logistics.cpp</c> (no methods of its own). The records were written to and read from save
/// files and network messages as raw 32-bit images (<see cref="binarySize"/> bytes: the record, then the name and
/// icon strings, then the inventory); the port reads and writes them field by field.
/// </remarks>
class LogPart
{
public:
    /// <summary>1 for a <see cref="LogMech"/>, 2 for a <see cref="LogVehicle"/>.</summary>
    int32_t partType; // +0x0
    /// <summary>The name of the profile last written for the part ("tpak<i>n</i>", by the starting fit and save writers).</summary>
    char profileName[12]; // +0x4
    /// <summary>The weight class name (string table, from the tonnage).</summary>
    char* weightClassName; // +0x10
    /// <summary>The chassis class name (string table, from the chassis tonnage).</summary>
    char* chassisClassName; // +0x14
    /// <summary>
    /// The display name: string <c>DescIndex</c> + 300 (mechs) or + 700 (vehicles) of the string table. The original
    /// calls it the file name (it is saved as a profile's MechType).
    /// </summary>
    char* fileName; // +0x18
    /// <summary>The string table index of the name.</summary>
    int32_t nameIndex; // +0x1c
    /// <summary>The size of the saved record with its strings and inventory.</summary>
    uint32_t binarySize; // +0x20
    float curTonnage;    // +0x24
    /// <summary>The icon file name.</summary>
    char* iconName; // +0x28
    char status;    // +0x2c
    /// <summary>The packet of the chassis in the object packet file.</summary>
    uint32_t chassis; // +0x30
    /// <summary>The current value in resource points (a mech's; vehicles keep theirs at +0xb8).</summary>
    int32_t resourcePoints; // +0x34
    /// <summary>The value of the bare chassis.</summary>
    int32_t baseResourcePoints; // +0x38
    /// <summary>Not accessed in logistics.cpp.</summary>
    int32_t unknown3C; // +0x3c
    /// <summary>The description's index in the object description file (-1 = none).</summary>
    int32_t descIndex; // +0x40
    char* description; // +0x44
    /// <summary>The engine's tonnage.</summary>
    float engineTonnage; // +0x48
    /// <summary>The engine rating.</summary>
    uint32_t engineRating; // +0x4c
    /// <summary>The armor type (a profile's Armor Type).</summary>
    uint8_t armorType; // +0x50
    /// <summary>The armor's tonnage (a profile's Armor Tonnage).</summary>
    float armorTonnage;    // +0x54
    uint8_t numOther;      // +0x58
    uint8_t numWeapons;    // +0x59
    uint8_t numAmmo;       // +0x5a
    uint8_t unknown5B[13]; // +0x5b
    /// <summary>Cleared when the part is loaded.</summary>
    uint8_t unknown68;    // +0x68
    uint8_t unknown69[3]; // +0x69
    /// <summary>The battle rating (<see cref="LogMech::calcBR"/>).</summary>
    int32_t battleRating; // +0x6c
    int32_t unknown70;    // +0x70
    /// <summary>Nonzero when assigned to the force.</summary>
    int32_t assigned; // +0x74
    int32_t deployed; // +0x78
    /// <summary>Nonzero when the mission requires this part.</summary>
    int32_t required; // +0x7c
    /// <summary>Nonzero until the part is really the player's (salvage not yet taken).</summary>
    int32_t notMineYet; // +0x80
    /// <summary>1 for the local player's parts, 0 for ones received over the network.</summary>
    int32_t localPart; // +0x84
    /// <summary>The commander (player) id of a multiplayer part.</summary>
    int32_t commanderID;      // +0x88
    InventoryList* inventory; // +0x8c
    BriefingBox* briefingBox; // +0x90
    /// <summary>The multiplayer drop slot's lance.</summary>
    uint32_t dropLance; // +0x94
    /// <summary>The multiplayer drop slot within its lance.</summary>
    uint32_t dropSlot; // +0x98
};

/// <summary>A mech in logistics: armor, internals, the critical-slot layout, its pilot and its screen widgets.</summary>
/// <remarks>Original source: <c>logistics.cpp</c>, 600 (0x258) bytes.</remarks>
class LogMech : public LogPart
{
public:
    /// <summary>Armor points of a body location: the maximum and the current.</summary>
    struct ArmorPoints
    {
        uint8_t maxArmor;
        uint8_t curArmor;
    };

    /// <summary>
    /// A critical slot's occupant, as a profile's <c>Component:n</c> pair: <see cref="row"/> is the copy's item number
    /// (0xff = empty), <see cref="column"/> its damage, then the component.
    /// </summary>
    struct ItemSlot
    {
        uint8_t row;
        uint8_t column;
        uint8_t masterID;
    };

    /// <summary>The pilot modifier: the pilot's rank against the mech's weight class.</summary>
    /// <remarks>MCX.EXE @ 0x006eaee0</remarks>
    int32_t calcPilotModifier();

    /// <summary>Recomputes <see cref="LogPart::resourcePoints"/> from the chassis, the components and the damage.</summary>
    /// <remarks>MCX.EXE @ 0x006eafe0</remarks>
    void calcMechCost(int repaired);

    /// <summary>Recomputes the battle rating from the components.</summary>
    /// <remarks>MCX.EXE @ 0x006eb140</remarks>
    int32_t calcBR();

    /// <summary>
    /// Puts copy <paramref name="itemNum"/> (damage <paramref name="hits"/>) of component <paramref name="masterID"/>
    /// in the first free critical slot of the location its form goes to.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006eb1a0</remarks>
    void placeItem(uint8_t masterID, int32_t itemNum, int32_t hits);

    /// <summary>Whether weapon <paramref name="masterID"/> takes a large slot.</summary>
    /// <remarks>MCX.EXE @ 0x006eb5b0</remarks>
    int32_t getWeaponLarge(uint8_t masterID);

    /// <summary>The large weapons in body location <paramref name="location"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006eb610</remarks>
    int32_t getLargeWeaponCount(int32_t location);

    /// <summary>The small weapons in body location <paramref name="location"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006eb650</remarks>
    int32_t getSmallWeaponCount(int32_t location);

    /// <summary>Loads description <paramref name="descIndex"/> from the object description file.</summary>
    /// <remarks>MCX.EXE @ 0x006ed790</remarks>
    void loadDescription(int32_t descIndex);

    /// <summary>The mech's condition (0..1) from armor, internals and pilot.</summary>
    /// <remarks>MCX.EXE @ 0x0071aa20</remarks>
    float calcStatus();

    /// <summary>A second string table name (from the description index).</summary>
    char* extraName1; // +0x9c
    /// <summary>A third string table name.</summary>
    char* extraName2; // +0xa0
    /// <summary>The name from the mech file.</summary>
    char* mechName; // +0xa4
    /// <summary>The tonnage used by the chassis, engine and components.</summary>
    float usedTonnage; // +0xa8
    /// <summary>The tonnage left for components.</summary>
    float freeTonnage; // +0xac
    /// <summary>The tonnage of the weapons and ammunition.</summary>
    float weaponTonnage; // +0xb0
    /// <summary>The pilot's index in the warrior list (-1 = none).</summary>
    int32_t pilotIndex; // +0xb4
    int32_t unknownB8;  // +0xb8
    /// <summary>The name variant (0..2; picks the sort key and the multiplayer variant).</summary>
    int32_t nameVariant; // +0xbc
    int32_t sellValue;   // +0xc0
    /// <summary>The key the mech list is sorted by (<c>mechSort[nameIndex] * 3 + variant</c>).</summary>
    int32_t sortKey;     // +0xc4
    uint8_t maxRunSpeed; // +0xc8
    /// <summary>Head, center/left/right torso, left/right arm, left/right leg, rear center/left/right torso.</summary>
    ArmorPoints armor[11]; // +0xc9
    uint8_t unknownDF;     // +0xdf
    /// <summary>Nonzero where a location has CASE.</summary>
    int32_t hasCASE[8]; // +0xe0
    /// <summary>Internal structure of the eight locations: the chassis maximum and the current.</summary>
    ArmorPoints internals[8]; // +0x100
    /// <summary>The critical slot grid of the eight locations (0xff = empty).</summary>
    ItemSlot itemSlots[8][12]; // +0x110
    /// <summary>The hot spot of each location on the damage diagram.</summary>
    uint8_t hotSpotNumber[8]; // +0x230
    /// <summary>The chassis battle rating.</summary>
    int32_t chassisBR;     // +0x238
    int32_t pilotModifier; // +0x23c
    /// <summary>The condition from <see cref="calcStatus"/>.</summary>
    float statusValue;                  // +0x240
    MechRepairBlock* repairBlock;       // +0x244
    MechInventoryBlock* inventoryBlock; // +0x248
    MechBriefBlock* briefBlock;         // +0x24c
    /// <summary>The pilot of a multiplayer mech received over the network.</summary>
    LogWarrior* networkPilot; // +0x250
    LogMech* next;            // +0x254
};

/// <summary>A vehicle in logistics.</summary>
/// <remarks>Original source: <c>logistics.cpp</c>, 0xd0 bytes.</remarks>
class LogVehicle : public LogPart
{
public:
    /// <summary>The cost: the base value plus the components'.</summary>
    /// <remarks>MCX.EXE @ 0x006eaf90</remarks>
    void calcVehicleCost();

    /// <summary>Loads description <paramref name="descIndex"/> from the object description file.</summary>
    /// <remarks>MCX.EXE @ 0x006fd470</remarks>
    void loadDescription(int32_t descIndex);

    /// <summary>The crew (the vehicle file's Crew).</summary>
    char crew[9];         // +0x9c
    uint8_t maxMoveSpeed; // +0xa5
    /// <summary>The current internal structure of the five locations.</summary>
    uint8_t curInternalStructure[5]; // +0xa6
    uint8_t maxArmorPoints[5];       // +0xab
    uint8_t curArmorPoints[5];       // +0xb0
    uint8_t unknownB5[3];            // +0xb5
    /// <summary>The current value.</summary>
    int32_t vehicleResourcePoints; // +0xb8
    /// <summary>The value of the bare vehicle.</summary>
    int32_t baseVehicleResourcePoints;     // +0xbc
    VehicleRepairBlock* repairBlock;       // +0xc0
    VehicleInventoryBlock* inventoryBlock; // +0xc4
    MechBriefBlock* briefBlock;            // +0xc8
    LogVehicle* next;                      // +0xcc
};

/// <summary>A MechWarrior in logistics: names, portrait, skills, wounds and status.</summary>
/// <remarks>
/// Original source: <c>logistics.cpp</c>, 300 (0x12c) bytes. Saved as a raw 32-bit image followed by its six
/// strings (<see cref="binarySize"/> bytes); the port reads and writes it field by field.
/// </remarks>
class LogWarrior
{
public:
    /// <summary>The rank from the skills, weighted by <c>SkillWeightings</c> and cut by <c>WarriorRankScale</c>.</summary>
    /// <remarks>MCX.EXE @ 0x006fd290</remarks>
    void calcRank();

    /// <summary>Loads description <paramref name="descIndex"/> from the object description file.</summary>
    /// <remarks>MCX.EXE @ 0x006fd300</remarks>
    void loadDescription(int32_t descIndex);

    /// <summary>The profile file's base name.</summary>
    char fileName[12]; // +0x0
    LogWarrior* next;  // +0xc
    /// <summary>The size of the saved record with its strings.</summary>
    uint32_t binarySize; // +0x10
    char* name;          // +0x14
    /// <summary>A unique id (<see cref="Logistics::nextWarriorID"/>).</summary>
    int32_t id;       // +0x18
    char* callsign;   // +0x1c
    char* picture;    // +0x20
    char* pilotVideo; // +0x24
    char* pilotAudio; // +0x28
    /// <summary>The ABL brain file.</summary>
    char* brain;         // +0x2c
    int32_t paintScheme; // +0x30
    /// <summary>0 green .. 3 elite.</summary>
    int32_t rank;      // +0x34
    int32_t nameIndex; // +0x38
    int32_t descIndex; // +0x3c
    char* description; // +0x40
    int32_t unknown44; // +0x44
    /// <summary>Professionalism, decorum, aggressiveness, courage.</summary>
    char personality[4]; // +0x48
    /// <summary>Piloting, jumping, sensors, gunnery.</summary>
    char skills[4];         // +0x4c
    char originalSkills[4]; // +0x50
    char startingSkills[4]; // +0x54
    /// <summary>Skill points earned towards the next level of each skill.</summary>
    float skillPoints[4]; // +0x58
    char mechClass;       // +0x68
    char mechType;        // +0x69
    char weaponClass;     // +0x6a
    char weaponTypes[2];  // +0x6b
    uint8_t unknown6D[3]; // +0x6d
    float wounds;         // +0x70
    /// <summary>6 minus the wounds (0 = dead).</summary>
    float health; // +0x74
    /// <summary>4 = killed.</summary>
    int32_t warriorStatus; // +0x78
    int32_t unknown7C;     // +0x7c
    /// <summary>The lance of the drop slot the pilot's mech is in (-1 = none).</summary>
    int32_t dropLance; // +0x80
    /// <summary>The slot in <see cref="dropLance"/> (-1 = none).</summary>
    int32_t dropSlot;  // +0x84
    uint8_t unknown88; // +0x88
    int32_t assigned;  // +0x8c
    /// <summary>Set while the pilot's mech is in a drop slot.</summary>
    int32_t deployed;   // +0x90
    int32_t sold;       // +0x94
    int32_t notMineYet; // +0x98
    int32_t ejected;    // +0x9c
    /// <summary>Not accessed in logistics.cpp.</summary>
    uint8_t unknownA0[0x88];             // +0xa0
    PilotInventoryBlock* inventoryBlock; // +0x128
};

/// <summary>A linked list of mechs, sorted by <see cref="LogMech::sortKey"/> or by tonnage.</summary>
/// <remarks>Original source: <c>logistics.cpp</c>, 0xc bytes.</remarks>
class LogMechList
{
public:
    /// <remarks>MCX.EXE @ 0x006eae30</remarks>
    LogMechList();

    /// <remarks>MCX.EXE @ 0x006eae40</remarks>
    static void* operator new(size_t size) noexcept;
    /// <remarks>MCX.EXE @ 0x006eae60</remarks>
    static void operator delete(void* ptr);

    /// <summary>Removes every mech.</summary>
    /// <remarks>MCX.EXE @ 0x006eae80</remarks>
    void destroy();

    /// <summary>The position of <paramref name="mech"/>, or -1.</summary>
    /// <remarks>MCX.EXE @ 0x006eaeb0</remarks>
    int32_t getMechIndex(LogMech* mech);

    /// <summary>Adds the mech in file <paramref name="fileName"/> under <c>profilePath</c>.</summary>
    /// <remarks>MCX.EXE @ 0x006eb6c0</remarks>
    LogMech* addMech(char* fileName, int required, int sorted, int widgets);

    /// <summary>Replaces the mech at <paramref name="index"/> with one read from a save's packet.</summary>
    /// <remarks>MCX.EXE @ 0x006eb760</remarks>
    int32_t replaceMech(PacketFile* file, int32_t index);

    /// <summary>Adds a mech read from packet <paramref name="packet"/> of a save.</summary>
    /// <remarks>MCX.EXE @ 0x006eb920</remarks>
    LogMech* addMech(PacketFile* file, int32_t packet);

    /// <summary>
    /// Reads a mech from its profile (or a raw save record when the file has no [General] block) and adds it, with
    /// its repair, inventory and briefing widgets when <paramref name="widgets"/> is set.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006eb9b0</remarks>
    LogMech* addMech(FitIniFile* file, int required, int sorted, int widgets);

    /// <summary>Links <paramref name="mech"/> in (by tonnage when <paramref name="sorted"/>).</summary>
    /// <remarks>MCX.EXE @ 0x006ed460</remarks>
    int32_t addMech(LogMech* mech, int sorted);

    /// <summary>Unlinks the mech at <paramref name="index"/> without freeing it.</summary>
    /// <remarks>MCX.EXE @ 0x006ed4c0</remarks>
    int32_t extractMech(int32_t index, LogMech*& mech);

    /// <summary>Deletes the mech at <paramref name="index"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006ed530</remarks>
    int32_t removeMech(uint8_t index);

    /// <summary>Deletes <paramref name="mech"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006ed570</remarks>
    int32_t removeMech(LogMech* mech);

    /// <summary>Unlinks <paramref name="mech"/> (after <paramref name="previous"/>) and frees it and its widgets.</summary>
    /// <remarks>MCX.EXE @ 0x006ed5b0</remarks>
    int32_t deleteMech(LogMech* mech, LogMech* previous);

    /// <remarks>MCX.EXE @ 0x006ed900</remarks>
    int32_t getMechCount();

    /// <summary>The saved size of the mech at <paramref name="index"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006ed910</remarks>
    int32_t getMechSize(uint32_t index);

    /// <summary>The pilot index of the mech at <paramref name="index"/>, or -1.</summary>
    /// <remarks>MCX.EXE @ 0x006ed950</remarks>
    int32_t getMechPilotIndex(int32_t index);

    /// <remarks>MCX.EXE @ 0x006ed990</remarks>
    int32_t getMechInfo(int32_t index, LogMech*& mech);

    /// <summary>Writes the saved form of the mech at <paramref name="index"/> into <paramref name="data"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006ed9d0</remarks>
    int32_t getBinaryData(uint32_t index, void* data);

    /// <summary>Writes the mech at <paramref name="index"/> as a profile text file.</summary>
    /// <remarks>MCX.EXE @ 0x006eda80</remarks>
    int32_t saveMechText(char* fileName, int32_t index);

    /// <summary>Writes the mech at <paramref name="index"/> in its binary saved form.</summary>
    /// <remarks>MCX.EXE @ 0x006edac0</remarks>
    int32_t saveMechBinary(char* fileName, int32_t index);

    LogMech* mechs;   // +0x0
    int32_t numMechs; // +0x4
    /// <summary>The multiplayer player whose mechs these are (set by <see cref="Logistics::initializeMultiplayer"/>).</summary>
    uint32_t playerID; // +0x8
};

/// <summary>A linked list of vehicles.</summary>
/// <remarks>Original source: <c>logistics.cpp</c>, 0xc bytes.</remarks>
class LogVehicleList
{
public:
    /// <remarks>MCX.EXE @ 0x006fb520</remarks>
    LogVehicleList();

    /// <remarks>MCX.EXE @ 0x006fb530</remarks>
    static void* operator new(size_t size) noexcept;
    /// <remarks>MCX.EXE @ 0x006fb550</remarks>
    static void operator delete(void* ptr);

    /// <remarks>MCX.EXE @ 0x006fb570</remarks>
    void destroy();

    /// <remarks>MCX.EXE @ 0x006fb5a0</remarks>
    int32_t getVehicleIndex(LogVehicle* vehicle);

    /// <remarks>MCX.EXE @ 0x006fb5d0</remarks>
    LogVehicle* addVehicle(char* fileName, int required, int sorted, int widgets);

    /// <summary>Does nothing (vehicles are not replaced from saves).</summary>
    /// <remarks>MCX.EXE @ 0x006fb670</remarks>
    int32_t replaceVehicle(PacketFile* file, int32_t index);

    /// <remarks>MCX.EXE @ 0x006fb680</remarks>
    LogVehicle* addVehicle(PacketFile* file, int32_t packet);

    /// <summary>Reads a vehicle from its profile (or a raw save record) and adds it.</summary>
    /// <remarks>MCX.EXE @ 0x006fb710</remarks>
    LogVehicle* addVehicle(FitIniFile* file, int required, int sorted, int widgets);

    /// <remarks>MCX.EXE @ 0x006fc380</remarks>
    int32_t removeVehicle(uint8_t index);

    /// <remarks>MCX.EXE @ 0x006fc3c0</remarks>
    int32_t removeVehicle(LogVehicle* vehicle);

    /// <remarks>MCX.EXE @ 0x006fc400</remarks>
    int32_t deleteVehicle(LogVehicle* vehicle, LogVehicle* previous);

    /// <remarks>MCX.EXE @ 0x006fc520</remarks>
    int32_t getVehicleInfo(int32_t index, LogVehicle*& vehicle);

    /// <remarks>MCX.EXE @ 0x006fc560</remarks>
    int32_t getVehicleCount();

    /// <remarks>MCX.EXE @ 0x006fc570</remarks>
    int32_t getVehicleSize(uint32_t index);

    /// <remarks>MCX.EXE @ 0x006fc5b0</remarks>
    int32_t getBinaryData(uint32_t index, void* data);

    /// <remarks>MCX.EXE @ 0x006fc660</remarks>
    int32_t saveVehicleText(char* fileName, int32_t index);

    /// <remarks>MCX.EXE @ 0x006fc6a0</remarks>
    int32_t saveVehicleBinary(char* fileName, int32_t index);

    LogVehicle* vehicles; // +0x0
    int32_t numVehicles;  // +0x4
    /// <summary>The multiplayer player whose vehicles these are (set by <see cref="Logistics::initializeMultiplayer"/>).</summary>
    uint32_t playerID; // +0x8
};

/// <summary>A linked list of MechWarriors.</summary>
/// <remarks>Original source: <c>logistics.cpp</c>, 8 bytes.</remarks>
class LogWarriorList
{
public:
    /// <remarks>MCX.EXE @ 0x006e8fc0</remarks>
    LogWarriorList();

    /// <remarks>MCX.EXE @ 0x006e8fd0</remarks>
    void destroy();

    /// <summary>Adds the warrior in profile <paramref name="fileName"/> under <c>warriorPath</c>.</summary>
    /// <remarks>MCX.EXE @ 0x006e9000</remarks>
    int32_t addWarrior(char* fileName, int sorted);

    /// <summary>Replaces the warrior at <paramref name="index"/> with one read from a save's packet.</summary>
    /// <remarks>MCX.EXE @ 0x006e9080</remarks>
    int32_t replaceWarrior(PacketFile* file, int32_t index);

    /// <remarks>MCX.EXE @ 0x006e9620</remarks>
    int32_t addWarrior(PacketFile* file, int32_t packet, int sorted);

    /// <summary>Reads a warrior from its profile (or a raw save record) and adds it with its inventory widget.</summary>
    /// <remarks>MCX.EXE @ 0x006e96b0</remarks>
    int32_t addWarrior(FitIniFile* file, int sorted);

    /// <summary>Links <paramref name="warrior"/> in (by rank when <paramref name="sorted"/>).</summary>
    /// <remarks>MCX.EXE @ 0x006ea4a0</remarks>
    int32_t addWarrior(LogWarrior* warrior, int sorted);

    /// <remarks>MCX.EXE @ 0x006ea570</remarks>
    int32_t extractWarrior(int32_t index, LogWarrior*& warrior);

    /// <summary>Takes <paramref name="amount"/> wounds off every living, unsold warrior (between missions).</summary>
    /// <remarks>MCX.EXE @ 0x006ea5d0</remarks>
    void heal(int32_t amount);

    /// <remarks>MCX.EXE @ 0x006ea630</remarks>
    int32_t removeWarriorAtIndex(int32_t index);

    /// <remarks>MCX.EXE @ 0x006ea660</remarks>
    int32_t removeWarrior(uint8_t index);

    /// <remarks>MCX.EXE @ 0x006ea690</remarks>
    int32_t deleteWarrior(LogWarrior* warrior, LogWarrior* previous);

    /// <remarks>MCX.EXE @ 0x006ea7c0</remarks>
    int32_t getWarriorCount();

    /// <remarks>MCX.EXE @ 0x006ea7d0</remarks>
    int32_t getWarriorSize(uint32_t index);

    /// <summary>Copies the profile file name of the warrior at <paramref name="index"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006ea800</remarks>
    int32_t getWarriorProfile(uint32_t index, char* dest);

    /// <summary>Copies the brain file name of the warrior at <paramref name="index"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006ea850</remarks>
    int32_t getWarriorBrain(uint32_t index, char* dest);

    /// <summary>The <see cref="LogWarrior::id"/> of the warrior at <paramref name="index"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006ea8a0</remarks>
    int32_t getID(int32_t index);

    /// <remarks>MCX.EXE @ 0x006ea8d0</remarks>
    int32_t getBinaryData(uint32_t index, void* data);

    /// <remarks>MCX.EXE @ 0x006ea9f0</remarks>
    int32_t saveWarriorText(char* fileName, int32_t index);

    /// <remarks>MCX.EXE @ 0x006eaa30</remarks>
    int32_t getWarriorInfo(int32_t index, LogWarrior*& warrior);

    /// <remarks>MCX.EXE @ 0x006eaa70</remarks>
    int32_t getWarriorIndex(LogWarrior* warrior);

    /// <summary>Whether a warrior whose callsign is <paramref name="fileName"/> is in the list.</summary>
    /// <remarks>MCX.EXE @ 0x006eaaa0</remarks>
    int exists(char* fileName);

    /// <remarks>MCX.EXE @ 0x006eab00</remarks>
    int32_t saveWarriorBinary(char* fileName, int32_t index);

    /// <summary>Marks the warrior at <paramref name="index"/> deployed or not.</summary>
    /// <remarks>MCX.EXE @ 0x006eac50</remarks>
    void setDeployed(int32_t index, int deployed);

    /// <summary>Re-sorts the list (assigned first, then by rank) and fixes the mechs' pilot indexes.</summary>
    /// <remarks>MCX.EXE @ 0x006eac90</remarks>
    void reorder();

    LogWarrior* warriors; // +0x0
    int32_t numWarriors;  // +0x4
};

/// <summary>A multiplayer drop slot: its lance and position, and the mech or vehicle placed there.</summary>
/// <remarks>Original source: <c>logistics.cpp</c>, 0xc bytes.</remarks>
class DropSlot
{
public:
    /// <remarks>MCX.EXE @ 0x006ffa10</remarks>
    static void* operator new(size_t size) noexcept;
    /// <remarks>MCX.EXE @ 0x006ffa30</remarks>
    static void operator delete(void* ptr);

    int32_t lance; // +0x0
    int32_t slot;  // +0x4
    LogPart* part; // +0x8
};

/// <summary>The row of lights on the multiplayer screens, one per player, showing whether each is ready.</summary>
/// <remarks>Original source: <c>logistics.cpp</c>, 0x50c bytes.</remarks>
class MPPlayerLights : public lObject
{
public:
    static constexpr int32_t MAX_PLAYERS = 6;

    /// <remarks>MCX.EXE @ 0x0070e670 (vector deleting destructor)</remarks>
    ~MPPlayerLights() override;

    /// <summary>Places the lights and loads their pictures.</summary>
    /// <remarks>MCX.EXE @ 0x00700070</remarks>
    void init();

    /// <summary>Frees the pictures and stops the blink timer.</summary>
    /// <remarks>MCX.EXE @ 0x00700220</remarks>
    void destroy() override;

    /// <summary>Sets how many lights there are (and the width).</summary>
    /// <remarks>MCX.EXE @ 0x007002e0</remarks>
    void setNumPlayers(int32_t count);

    /// <summary>Sets the player shown by light <paramref name="light"/>.</summary>
    /// <remarks>MCX.EXE @ 0x00700310</remarks>
    void setPlayerID(int32_t light, uint32_t playerID);

    /// <summary>Sets a player's status (0..2); status 2 starts the blink timer.</summary>
    /// <remarks>MCX.EXE @ 0x00700330</remarks>
    void setPlayerStatus(uint32_t playerID, int32_t status);

    /// <summary>
    /// Paints the lights: each player's numbered light, lit (<c>lsc_ph</c>) or blinking over it, and the backing
    /// (<c>lsc_p0</c>) into the parent. Port: draws the lights from the state each frame; the parent screen draws the
    /// backing (<see cref="Logistics::drawScreenChrome"/>).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x007003b0</remarks>
    void draw() override;

    /// <summary>Port: the lights draw themselves each frame (their port is a view).</summary>
    bool DrawsLive() override { return true; }

    /// <summary>The blink timer toggles the blink; pointing at a light shows the player's name on the ticker.</summary>
    /// <remarks>MCX.EXE @ 0x00700620</remarks>
    void handleEvent(aEvent* event) override;

    int32_t numPlayers = 0;                 // +0x4bc
    uint32_t playerIDs[MAX_PLAYERS] = {};   // +0x4c0
    int32_t playerStatus[MAX_PLAYERS] = {}; // +0x4d8
    /// <summary>The width of one light.</summary>
    int32_t lightWidth = 0; // +0x4f0
    /// <summary>
    /// <see cref="draw"/> paints the background into the parent when this differs from the parent. Cleared by
    /// <see cref="init"/> and never set, so the background is painted on every draw once there is a parent.
    /// </summary>
    aObject* backgroundParent = nullptr; // +0x4f4
    /// <summary>Nonzero while the blink timer runs.</summary>
    int32_t timerRunning = 0; // +0x4f8
    /// <summary>The blink phase.</summary>
    int32_t blinkOn = 0; // +0x4fc
    /// <summary>The unlit lights (<c>lsc_pn.tga</c>).</summary>
    lPort* lightsPort = nullptr; // +0x500
    /// <summary>The ready light (<c>lsc_pg.tga</c>).</summary>
    lPort* readyPort = nullptr; // +0x504
    /// <summary>The blinking light (<c>lsc_pg1.tga</c>).</summary>
    lPort* blinkPort = nullptr; // +0x508
};

/// <summary>
/// The logistics layer: owns the logistics heap, every screen (main menu, multiplayer, load/save, preferences,
/// briefing, purchase, repair, session), the player's force and inventory, the art they share, and runs campaign
/// loading/saving and the multiplayer force exchange. One instance, <c>globalLogPtr</c>.
/// </summary>
/// <remarks>Original source: <c>logistics.cpp</c>, 0x1908 bytes (allocated in <c>mission.cpp</c>).</remarks>
class Logistics
{
public:
    /// <summary>Draws the damage state of location <paramref name="location"/> of a mech into <paramref name="port"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006d49e0</remarks>
    void drawMechBodyLoc(LogMech* mech, int32_t location, lPort* port, int32_t xPos, int32_t yPos);

    /// <summary>Draws the damage state of location <paramref name="location"/> of a vehicle into <paramref name="port"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006d8780</remarks>
    void drawVehicleBodyLoc(LogVehicle* vehicle, int32_t location, lPort* port, int32_t xPos, int32_t yPos);

    /// <summary>
    /// Makes the logistics heap, the lists, every screen and dialog, loads the shared art, shapes and sort tables,
    /// and shows the main screen.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006edcc0</remarks>
    void init();

    /// <summary>Makes the multiplayer lists, drop slots and message buffer, and reads the net mech/warrior/vehicle lists.</summary>
    /// <remarks>MCX.EXE @ 0x006f05a0</remarks>
    void initializeMultiplayer();

    /// <summary>Frees what <see cref="initializeMultiplayer"/> made.</summary>
    /// <remarks>MCX.EXE @ 0x006f0b60 (original name lost)</remarks>
    void destroyMultiplayer();

    /// <summary>Frees everything <see cref="init"/> made.</summary>
    /// <remarks>MCX.EXE @ 0x006f0d90</remarks>
    void destroy();

    /// <summary>Shows or hides the current logistics screen.</summary>
    /// <remarks>MCX.EXE @ 0x006f1a20</remarks>
    void showLogScreen(int show, int redraw);

    /// <remarks>MCX.EXE @ 0x006f1a80</remarks>
    int32_t setUpMainScreen(int mode);

    /// <summary>Reads the campaign's purchase file name and costs from packet file <paramref name="file"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006f1bc0</remarks>
    char* setUpCampaignPurchasing(char* purchaseFile, PacketFile* file);

    /// <summary>Sets up what can be bought in a multiplayer game.</summary>
    /// <remarks>MCX.EXE @ 0x006f2080</remarks>
    void setUpMPPurchasing(char* purchaseFile);

    /// <summary>Sets up what can be bought from a campaign's purchase packet.</summary>
    /// <remarks>MCX.EXE @ 0x006f28a0</remarks>
    void setUpPurchasing(PacketFile* file);

    /// <summary>Sets up what can be bought from an old-style purchase file.</summary>
    /// <remarks>MCX.EXE @ 0x006f31c0</remarks>
    void setUpOldPurchasing(char* purchaseFile);

    /// <remarks>MCX.EXE @ 0x006f3780</remarks>
    int32_t setUpPurchaseScreen(int mode);

    /// <summary>
    /// Painted the screen switch buttons on the current screen: each normal, the current screen's grayed, none lit.
    /// Port: the buttons are drawn each frame (<see cref="drawScreenChrome"/>); this puts out the lit one.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006f3c30</remarks>
    void drawScreenButtons();

    /// <summary>
    /// Port: screen button <paramref name="button"/> of <paramref name="screen"/> is under the mouse and shows lit
    /// (the original copied the lit picture over it).
    /// </summary>
    void hoverScreenButton(lObject* screen, int32_t button);

    /// <summary>
    /// Port: draws the shared places of <paramref name="screen"/> from the state (<see cref="LogScreenChrome"/>): the
    /// multiplayer lights' backing, the screen buttons, the ticker line, the resource points and the clock.
    /// </summary>
    void drawScreenChrome(lObject* screen, _pane* target);

    /// <summary>Switches to the briefing screen.</summary>
    /// <remarks>MCX.EXE @ 0x006f3dc0 (original name lost)</remarks>
    int32_t setUpBriefingScreen(int mode);

    /// <remarks>MCX.EXE @ 0x006f4390</remarks>
    int32_t setUpSessionScreen();

    /// <remarks>MCX.EXE @ 0x006f43f0</remarks>
    int32_t setUpRepairScreen(int mode);

    /// <summary>Loads a quick-start force (mechs, pilots, vehicles) from <paramref name="file"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006f48c0</remarks>
    void loadQuickStart(FitIniFile* file);

    /// <remarks>MCX.EXE @ 0x006f4dc0</remarks>
    int32_t saveCampaign(char* fileName);

    /// <summary>Loads a campaign (or a saved game) and its force.</summary>
    /// <remarks>MCX.EXE @ 0x006f4de0</remarks>
    int32_t loadCampaign(char* campaignFile, char* saveFile, int newCampaign, int loadForce);

    /// <summary>Writes the deployed force into the mission's start files and starts the mission.</summary>
    /// <remarks>MCX.EXE @ 0x006f6b60</remarks>
    int32_t prepareScenario(char* scenarioName, char* startFile);

    /// <summary>Gives mech <paramref name="mechIndex"/> pilot <paramref name="pilotIndex"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006fae20</remarks>
    void setPilot(int32_t mechIndex, int32_t pilotIndex);

    /// <remarks>MCX.EXE @ 0x006faf10</remarks>
    void reorderMechs();

    /// <remarks>MCX.EXE @ 0x006fb080</remarks>
    void reorderVehicles();

    /// <remarks>MCX.EXE @ 0x006fb1f0</remarks>
    void reorderWarriors();

    /// <summary>Shifts the pilot indexes after <paramref name="from"/> by <paramref name="amount"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006fb3e0</remarks>
    void shiftPilots(int32_t from, int32_t amount);

    /// <summary>Whether every required mech has a pilot.</summary>
    /// <remarks>MCX.EXE @ 0x006fb420</remarks>
    int requiredAssigned();

    /// <summary>Reads the current mission's name and settings from the campaign.</summary>
    /// <remarks>MCX.EXE @ 0x006fc810</remarks>
    void getCurrentMission();

    /// <summary>Wipes from <paramref name="from"/> to <paramref name="to"/> (a screen change).</summary>
    /// <remarks>MCX.EXE @ 0x006fce70</remarks>
    void transition(lPort* from, lPort* to, int direction);

    /// <summary>Darkens <paramref name="port"/> through the fade table.</summary>
    /// <remarks>MCX.EXE @ 0x006fd090</remarks>
    void darken(int32_t amount, char* fadeTable, lPort* port);

    /// <summary>
    /// Port: <see cref="darken"/> of a <paramref name="width"/> x <paramref name="height"/> block of
    /// <paramref name="port"/> at (<paramref name="xPos"/>, <paramref name="yPos"/>), drawn in place: the fade
    /// table's translate polygon over the block (the original copied the block out, translated it and copied it back,
    /// which comes out the same). On the GPU it is one blended quad.
    /// </summary>
    static void DarkenRect(lPort* port, int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* fadeTable);

    /// <summary>Renumbers the component inventory's copies.</summary>
    /// <remarks>MCX.EXE @ 0x006fd200</remarks>
    int32_t reIndexInventory();

    /// <summary>Drops pilot indexes above <paramref name="removedPilot"/> by one along the mech list from <paramref name="mech"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006fd900 (name and signature inferred; no callers)</remarks>
    void removeReorderPilotIndexes(LogMech* mech, int32_t removedPilot);

    /// <summary>Handles a multiplayer "deploy force" message: adds the other player's mech or vehicle to its drop slot.</summary>
    /// <remarks>MCX.EXE @ 0x006fdbd0</remarks>
    void HandleDeployForceMessage(uint32_t playerID, const void* message);

    /// <remarks>MCX.EXE @ 0x006fde30</remarks>
    void HandleRemoveForceMessage(uint32_t playerID, const void* message);

    /// <remarks>MCX.EXE @ 0x006fe070</remarks>
    void HandleChatMessage(uint32_t playerID, const void* message);

    /// <remarks>MCX.EXE @ 0x006fe160</remarks>
    void SendRemoveForceMessage(int lance, int slot);

    /// <remarks>MCX.EXE @ 0x006fe1c0</remarks>
    void SendAddMechMessage(LogMech* mech, int lance, int slot);

    /// <remarks>MCX.EXE @ 0x006fe330</remarks>
    void SendAddVehicleMessage(LogVehicle* vehicle, int lance, int slot);

    /// <summary>A player left: removes their forces and tells the player.</summary>
    /// <remarks>MCX.EXE @ 0x006fe570</remarks>
    void handleLostPlayer(uint32_t playerID, int hostLeft);

    /// <remarks>MCX.EXE @ 0x006fe670</remarks>
    void handlePrepareScenarioMessage();

    /// <remarks>MCX.EXE @ 0x006fe6a0</remarks>
    int32_t prepareMultiplayerScenario(char* scenarioName, char* startFile);

    /// <summary>Feeds a typed character to the cheat code matcher.</summary>
    /// <remarks>MCX.EXE @ 0x006ffa50</remarks>
    void processCheatCode(int16_t key);

    /// <summary>Draws the bar of the pilot's skill <paramref name="skill"/> (an index into its skills).</summary>
    /// <remarks>MCX.EXE @ 0x0071b6d0</remarks>
    void drawPilotSkillBar(LogWarrior* warrior, int32_t skill, int32_t xPos, int32_t yPos, int32_t row, int32_t width,
                           int32_t rowHeight, lPort* port);

    /// <summary>
    /// Draws a 4-pixel skill bar <paramref name="width"/> wide at (<paramref name="xPos"/>, <paramref name="yPos"/> +
    /// <paramref name="row"/> * <paramref name="rowHeight"/>), filled in proportion to <paramref name="value"/>
    /// between <c>MinPilotSkill</c> and <c>MaxPilotSkill</c>.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0071b700</remarks>
    void drawPilotSkillBar(int32_t value, int32_t xPos, int32_t yPos, int32_t row, int32_t width, int32_t rowHeight,
                           lPort* port);

protected:
    /// <summary>Marks which of the 12 drop slots are the local player's.</summary>
    /// <remarks>MCX.EXE @ 0x006f0b10</remarks>
    void SetupSlotsForMultiplayer(int playerIndex, int numPlayers);

    /// <summary>The mech list of player <paramref name="playerID"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006fd5e0</remarks>
    LogMechList* FindMPMechList(uint32_t playerID, int teammate, int* listIndex);

    /// <remarks>MCX.EXE @ 0x006fd870</remarks>
    LogVehicleList* FindMPVehicleList(uint32_t playerID, int teammate);

    /// <summary>Bumps the pilot indexes along the mech list from <paramref name="mech"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006fd8d0</remarks>
    void addReorderPilotIndexes(LogMech* mech);

    /// <summary>Does nothing.</summary>
    /// <remarks>MCX.EXE @ 0x006fd930</remarks>
    void addReorderPilotIndexes(LogVehicle* vehicle);

    /// <summary>Does nothing.</summary>
    /// <remarks>MCX.EXE @ 0x006fd940</remarks>
    void removeReorderPilotIndexes(LogVehicle* vehicle, LogVehicle* previous);

    /// <summary>Builds a mech (with its pilot and inventory) from a "deploy force" message.</summary>
    /// <remarks>MCX.EXE @ 0x006fd950</remarks>
    LogPart* AddMechFromNetworkMessage(LogMechList* list, FIMessageHeader* message);

    /// <remarks>MCX.EXE @ 0x006fdac0</remarks>
    LogPart* AddVehicleFromNetworkMessage(LogVehicleList* list, FIMessageHeader* message);

    /// <summary>Empties drop slot <paramref name="slot"/> of the player's or the opponents' table.</summary>
    /// <remarks>MCX.EXE @ 0x006fde70</remarks>
    int RemoveForceAtDropSlot(int32_t slot, uint32_t playerID, int teamTable);

public:
    /// <summary>The name ticker on the main screen.</summary>
    Ticker* ticker; // +0x0
    /// <summary>The multiplayer ready lights.</summary>
    MPPlayerLights* playerLights; // +0x4
    /// <summary>Cleared by <see cref="init"/>.</summary>
    int32_t unknown08; // +0x8
    /// <summary>The current mission's number in the campaign (-1 = none).</summary>
    int32_t currentMission; // +0xc
    /// <summary>The campaign's purchase file (the save's "purchaseFile"; written to starting fits as PurchaseFile).</summary>
    char purchaseFile[0x80];  // +0x10
    uint8_t unknown90[0x12c]; // +0x90
    /// <summary>Every pilot the player has.</summary>
    LogWarriorList* warriorList; // +0x1bc
    /// <summary>The pilots assigned to mechs.</summary>
    LogWarriorList* assignedWarriorList; // +0x1c0
    /// <summary>Every mech the player has.</summary>
    LogMechList* mechList; // +0x1c4
    /// <summary>Every vehicle the player has.</summary>
    LogVehicleList* vehicleList; // +0x1c8
    /// <summary>The mechs in the force (repair and briefing screens).</summary>
    LogMechList* forceMechList; // +0x1cc
    /// <summary>The vehicles in the force.</summary>
    LogVehicleList* forceVehicleList; // +0x1d0
    /// <summary>The other multiplayer players' mechs, per team side and player (<see cref="FindMPMechList"/>).</summary>
    LogMechList* mpMechLists[2][3]; // +0x1d4
    /// <summary>The other multiplayer players' vehicles.</summary>
    LogVehicleList* mpVehicleLists[2][3]; // +0x1ec
    /// <summary>The pilots of mechs received over the network.</summary>
    LogWarriorList* mpWarriorList;  // +0x204
    PurMechList* purMechList;       // +0x208
    PurVehicleList* purVehicleList; // +0x20c
    PurPilotList* purPilotList;     // +0x210
    /// <summary>The player's spare components.</summary>
    InventoryList* componentInventory; // +0x214
    /// <summary>The components for sale.</summary>
    InventoryList* purchaseComponents; // +0x218
    /// <summary>The number of entries in <see cref="rangeSortList"/>.</summary>
    int32_t numRangeSorted; // +0x21c
    /// <summary>Component master ids in range order (<c>logart\comp.rsp</c>).</summary>
    uint32_t* rangeSortList; // +0x220
    /// <summary>Which screen or sub-screen logistics is on (1 main, 2.. the others, as the callbacks set it).</summary>
    int32_t logisticsState; // +0x224
    /// <summary>
    /// The <see cref="logisticsState"/> before the main menu opened over it: the menu's return and a finished save
    /// go back to the purchase (2) or repair (4) screen, else the briefing.
    /// </summary>
    int32_t previousState; // +0x228
    /// <summary>Passed to a CRT call in <c>logistics.cpp</c> by address; not otherwise known.</summary>
    /// <summary>The clock text ("HH:MM:SS", from the CRT's <c>_strtime</c>), written by <c>RepairScreen::display</c>.</summary>
    char timeString[0xc]; // +0x22c
    /// <summary>Cleared by <see cref="init"/>.</summary>
    int32_t unknown238; // +0x238
    /// <summary>The logistics heap every logistics object allocates from.</summary>
    UserHeap* logisticsHeap; // +0x23c
    /// <summary>
    /// Which of the 12 drop slots (three lances of four) may be filled: the local player's in multiplayer; in single
    /// player the briefing screen shows the lances whose slots are set.
    /// </summary>
    int32_t localDropSlot[12]; // +0x240
    /// <summary>A world position (x, y) per drop zone: three for side 0, then three for side 1 (MultiPlayer homeTeam 1).</summary>
    struct DropZonePosition
    {
        float x;
        float y;
    } dropZonePositions[6]; // +0x270
    /// <summary>The drop zone: per lance and slot, the index of the mech or vehicle placed there (-1 = none).</summary>
    struct DeploySlot
    {
        /// <summary>The mech's index in the force mech list, or -1.</summary>
        int32_t unit;
        /// <summary>The vehicle's index in the force vehicle list, or -1 (only read when <see cref="unit"/> is -1).</summary>
        int32_t vehicle;
    } deploySlots[3][4]; // +0x2a0
    /// <summary>
    /// Per drop zone and slot, the unit's place relative to the zone (the mission's <c>OffsetX</c>, <c>OffsetY</c>
    /// and <c>Rotation</c>, read by <see cref="getCurrentMission"/>): the six zones of <see cref="dropZonePositions"/>.
    /// <see cref="init"/> clears only the first three.
    /// </summary>
    struct DeploySlotInfo
    {
        float offsetX;
        float offsetY;
        float rotation;
    } deploySlotInfo[6][4]; // +0x300
    /// <summary>The drop slots every force is placed in (multiplayer).</summary>
    DropSlot* dropSlots[3][4]; // +0x420
    /// <summary>The drop slots of the players not on the local player's team (multiplayer).</summary>
    DropSlot* opponentDropSlots[3][4]; // +0x450
    /// <summary>The current mission's name (freed by <see cref="destroy"/>).</summary>
    char* missionFileName;   // +0x480
    uint8_t unknown484[0xc]; // +0x484
    /// <summary>The inventory tab shown: 0 mechs, 1 pilots, 2 components, 3 vehicles.</summary>
    int32_t currentInvTab; // +0x490
    /// <summary>The cost of an armor point (the purchase file's PurchaseCosts).</summary>
    int32_t armorCost; // +0x494
    /// <summary>The cost of an internal structure point.</summary>
    int32_t internalCost; // +0x498
    /// <summary>The cost of engine work.</summary>
    int32_t engineCost; // +0x49c
    /// <summary>The cost of a green, regular, veteran and elite pilot.</summary>
    int32_t pilotCosts[4]; // +0x4a0
    /// <summary>The price factor of clan technology.</summary>
    float clanCostFactor; // +0x4b0
    /// <summary>The screen being shown.</summary>
    lObject* currentScreen;       // +0x4b4
    SessionScreen* sessionScreen; // +0x4b8
    MCSplashScreen* serialScreen; // +0x4bc
    /// <summary>The multiplayer connection screen (the one after the protocol choice).</summary>
    MCSplashScreen* connectScreen;     // +0x4c0
    MCSplashScreen* modemScreen;       // +0x4c4
    MCSplashScreen* lanScreen;         // +0x4c8
    MCSplashScreen* mainScreen;        // +0x4cc
    MCSplashScreen* multiplayerScreen; // +0x4d0
    MCSplashScreen* loadScreen;        // +0x4d4
    MCSplashScreen* saveScreen;        // +0x4d8
    MCSplashScreen* prefScreen;        // +0x4dc
    BriefingScreen* briefingScreen;    // +0x4e0
    PurchaseScreen* purchaseScreen;    // +0x4e4
    RepairScreen* repairScreen;        // +0x4e8
    /// <summary>The preferences as they were when the preferences screen opened (restored by CancelPrefs).</summary>
    int32_t savedPrefs0;  // +0x4ec
    int32_t savedPrefs1;  // +0x4f0
    int32_t savedPrefs2;  // +0x4f4
    uint32_t savedPrefs3; // +0x4f8
    uint32_t savedPrefs4; // +0x4fc
    uint32_t savedPrefs5; // +0x500
    int32_t savedPrefs6;  // +0x504
    /// <summary>
    /// The current mission's operation number (the campaign's Operation; picks the briefing's operation picture),
    /// 0 when it has none.
    /// </summary>
    int32_t operation;         // +0x508
    LogChatWindow* chatWindow; // +0x50c
    /// <summary>The mech repair screen shapes (<c>mechrep##.shp</c>).</summary>
    void* mechRepShapes[24]; // +0x510
    /// <summary>The vehicle repair screen shapes (<c>vr1_##.shp</c>).</summary>
    void* vehicleRepShapes[35]; // +0x570
    /// <summary>The mech icon shapes (<c>mi##.shp</c>).</summary>
    void* mechIconShapes[24]; // +0x5fc
    /// <summary>The vehicle icon shapes (<c>vi1_##.shp</c>).</summary>
    void* vehicleIconShapes[35]; // +0x65c
    /// <summary>Color remap tables for drawing shapes (<c>VFX_shape_lookaside</c>).</summary>
    uint8_t shapeLookaside[10][256]; // +0x6e8
    /// <summary>The repair screen's mech picture background (<c>lsrupm00.tga</c>).</summary>
    lPort* repairBackPort; // +0x10e8
    /// <summary>The inventory block background (<c>invblock.tga</c>).</summary>
    lPort* invBlockPort; // +0x10ec
    /// <summary>
    /// The inventory pane's contents per tab (mechs, pilots, components, vehicles), made by the
    /// <c>LogInvScreen::create*InvBlock</c> functions.
    /// </summary>
    lPort* invTabPorts[4]; // +0x10f0
    /// <summary>The box behind the resource figure at the top right (<c>RepairScreen::display</c>).</summary>
    lPort* resourceBackPort; // +0x1100
    /// <summary>The box behind the clock at the top right (<c>RepairScreen::display</c>).</summary>
    lPort* clockBackPort; // +0x1104
    /// <summary>The repair screen pieces (<c>lsrupm01..07.tga</c>).</summary>
    lPort* repairPorts[6]; // +0x1108
    /// <summary>The purchase screen pieces (<c>lspcb05..09.tga</c>).</summary>
    lPort* purchasePorts[4]; // +0x1120
    /// <summary>The two full-screen work ports (0x1ab x 0x1ce) the screens draw into.</summary>
    lPort* workPort1; // +0x1130
    lPort* workPort0; // +0x1134
    /// <summary>The screen switch buttons' pictures: button 0, exit, buttons 1..3; normal, highlighted, gray.</summary>
    lPort* screenButtonPorts[5][3]; // +0x1138
    /// <summary>The inventory tab icons: mechs, pilots, components, vehicles (<c>lscii?.tga</c>).</summary>
    lPort* inventoryIconPorts[4]; // +0x1174
    /// <summary>Not accessed in logistics.cpp (0x300 bytes: probably a palette).</summary>
    uint8_t unknown1184[0x300]; // +0x1184
    /// <summary>
    /// The chat text colour of each player number (1, 3, 4, 2, 6, 5), used as <c>%fc</c> codes by
    /// <c>LogChatWindow::processChatString</c>.
    /// </summary>
    int32_t playerColors[6]; // +0x1484
    /// <summary>Each component's place in the logistics sort order (<c>objsort.rsp</c>).</summary>
    int32_t componentSort[256]; // +0x149c
    /// <summary>The icon following the mouse while an inventory row is dragged (made and freed by the rows' <c>handleEvent</c>).</summary>
    DragIcon* dragIcon; // +0x189c
    /// <summary>The id the next <see cref="LogWarrior"/> gets.</summary>
    int32_t nextWarriorID;       // +0x18a0
    PurchaseDlg* purchaseDialog; // +0x18a4
    /// <summary>The one-button (or yes/no) message dialog.</summary>
    ReusableDialog* messageDialog; // +0x18a8
    /// <summary>The yes/no dialog.</summary>
    ReusableDialog* questionDialog; // +0x18ac
    RefitDialog* refitDialog;       // +0x18b0
    /// <summary>The campaign's CampaignBriefing Filename (0x29 bytes, logistics heap; freed by <see cref="destroy"/>).</summary>
    char* campaignBriefingName; // +0x18b4
    /// <summary>
    /// The mission's OperationCinema: the briefing movie played in <c>data\movies\</c> (0x29 bytes, logistics heap;
    /// null when the mission has none).
    /// </summary>
    char* operationCinema; // +0x18b8
    /// <summary>Set when the mission file has a HammerDown1 block: the drop tonnage limit is not enforced.</summary>
    int32_t hammerDown; // +0x18bc
    /// <summary>The mission's AutoPlay: the briefing screen starts the operation movie when first shown.</summary>
    int32_t autoPlayMovie; // +0x18c0
    /// <summary>The mechs allowed in multiplayer (<c>netmechs.rsp</c>).</summary>
    FLinkedList<char> netMechNames; // +0x18c4
    /// <summary>The pilots allowed in multiplayer (<c>netwars.rsp</c>).</summary>
    FLinkedList<char> netWarriorNames; // +0x18d4
    /// <summary>The vehicles allowed in multiplayer (<c>netvhcls.rsp</c>).</summary>
    FLinkedList<char> netVehicleNames; // +0x18e4
    /// <summary>Nonzero once <see cref="initializeMultiplayer"/> ran.</summary>
    int32_t multiplayerInitialized; // +0x18f4
    /// <summary>The buffer outgoing force messages are built in.</summary>
    FIMessageHeader* messageBuffer; // +0x18f8
    /// <summary>The default multiplayer planning time in seconds (240).</summary>
    uint32_t defaultPlanningTime; // +0x18fc
    /// <summary>The multiplayer planning time: a save's PlanningTime, else <see cref="defaultPlanningTime"/>.</summary>
    uint32_t planningTime; // +0x1900
    /// <summary>The multiplayer mission name (0x80 bytes).</summary>
    char* mpMissionName; // +0x1904
};

/// <summary>The logistics screens' default button callback (does nothing).</summary>
/// <remarks>MCX.EXE @ 0x006e7910</remarks>
void logisticsCallback();

/// <summary><c>qsort</c> comparer of mechs by id.</summary>
/// <remarks>MCX.EXE @ 0x006e7920</remarks>
int CompareLogMechIDs(const void* a, const void* b);

/// <summary><c>qsort</c> comparer of vehicles by id.</summary>
/// <remarks>MCX.EXE @ 0x006e7950</remarks>
int CompareLogVehicleIDs(const void* a, const void* b);

/// <summary>The player's user name (the multiplayer default name).</summary>
/// <remarks>MCX.EXE @ 0x006edbf0</remarks>
int MyGetUserName(char* name, uint32_t* size);

/// <summary>A dialog's answer: cancels.</summary>
/// <remarks>MCX.EXE @ 0x006fe430</remarks>
void CancelBool(int32_t answer);

/// <summary>Returns to the multiplayer session screen.</summary>
/// <remarks>MCX.EXE @ 0x006fe450</remarks>
void BackToSession();

/// <summary>A dialog's answer: returns to the session screen.</summary>
/// <remarks>MCX.EXE @ 0x006fe460</remarks>
void BackToSessionBool(int32_t answer);

/// <summary>The "player left" dialog's answer.</summary>
/// <remarks>MCX.EXE @ 0x006fe470</remarks>
void LostPlayerHandler(int32_t answer);

/// <summary>The multiplayer chat handler while in logistics.</summary>
/// <remarks>MCX.EXE @ 0x006fe650</remarks>
void LogisticsChatCallback(FIDPMessage* message, void* data);

/// <summary>How much each skill weighs in a pilot's rank.</summary>
extern float SkillWeightings[4];
/// <summary>The weighted skill each rank ends at.</summary>
extern float WarriorRankScale[4];
/// <summary>Each mech name index's place in the mech sort order.</summary>
extern int32_t mechSort[];
/// <summary>The logistics heap size (from the system config).</summary>
extern uint32_t LogisticsHeapSize;
/// <summary>The object packet file under <c>objectPath</c> the chassis profiles are read from ("object2.pak").</summary>
extern char objectPakName[20];
extern char missionName[];
/// <summary>The current planet (campaign setting).</summary>
extern int32_t CurPlanet;
extern char holdString[256];
/// <summary>Which logistics cheat codes are on.</summary>
extern int LogCheatActive[];
/// <summary>The multiplayer players' colors.</summary>
extern int32_t multiPlayerColors[];
/// <summary>How many characters of a cheat code have been typed.</summary>
extern int32_t LogCurCheatChar;
/// <summary>Nonzero in the demo version.</summary>
extern int InDemo;
