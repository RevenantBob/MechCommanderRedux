#pragma once

// Original source: mcx\logistics.cpp, the logistics (between-missions) layer: the player's mechs, vehicles, pilots
// and components, the screens that buy, repair and deploy them, campaign loading and saving, and the multiplayer
// force exchange.

#include "gui/MCGuiOwned.h"
#include "linkup/linkedlist.h"
#include "logistics/MCDragIcon.h"
#include "logistics/MCLogObject.h"
#include "platform/MCBlockStore.h"

class MCGuiEvent;
class MCGuiObject;
class MCBriefingBox;
class MCBriefingScreen;
class MCCompInventoryBlock;
class MCCompPurchaseBlock;
class MCFidpMessage;
class MCFIMessageHeader;
class MCFitIniFile;
class MCLogChatWindow;
class MCLogInvScreen;
class MCSplashScreen;
class MCMechBriefBlock;
class MCMechInventoryBlock;
class MCMechRepairBlock;
class MCPacketFile;
class MCPilotInventoryBlock;
class MCPurchaseDlg;
class MCPurchaseScreen;
class MCPurMechList;
class MCPurPilotList;
class MCPurVehicleList;
class MCRefitDialog;
class MCRepairScreen;
class MCReusableDialog;
class MCSessionScreen;
class MCTicker;

class MCVehicleInventoryBlock;
class MCVehicleRepairBlock;
class MCLogWarrior;

/// <summary>
/// One copy of a component in an inventory: where it sits and its state. The copies of one component type form a
/// list under their <see cref="MCLogInventoryItem"/>, sorted by <see cref="ItemNum"/>.
/// </summary>
/// <remarks>Original source: <c>logistics.cpp</c>, 0x1c bytes (<see cref="MCInventoryList::CreateStat"/>).</remarks>
struct MCLogInventoryStat
{
    /// <summary>A number unique within the inventory (<see cref="MCInventoryList::NextStatID"/>).</summary>
    uint8_t StatID = 0;
    /// <summary>Its damage (<see cref="MCInventoryList::HitItem"/>).</summary>
    uint8_t Hits = 0;
    /// <summary>Nonzero when the weapon faces forward (the mech file's FacesForward).</summary>
    uint8_t Facing = 0;
    /// <summary>The amount (ammunition count, or 1).</summary>
    int16_t Amount = 0;
    /// <summary>The body location it is mounted in (0xff = none; <see cref="MCInventoryList::SetStatLoc"/>).</summary>
    uint8_t Location = 0;
    /// <summary>The item number in the mech/vehicle file; the list is sorted by it.</summary>
    int32_t ItemNum = 0;
    MCLogInventoryStat* Next = nullptr;
};

/// <summary>
/// A component type in an inventory: its master component, how many copies there are (the <see cref="Stats"/>
/// list) and the widgets that show it on the purchase and inventory screens.
/// </summary>
/// <remarks>Original source: <c>logistics.cpp</c>, 0x48 bytes (<see cref="MCInventoryList::AddItem"/>).</remarks>
struct MCLogInventoryItem
{
    /// <summary>The index into <c>MasterComponentList</c>.</summary>
    uint8_t MasterID = 0;
    /// <summary>A copy of the master component's first field (+0x0).</summary>
    int32_t MasterValue = 0;
    /// <summary>The component's place in the logistics sort order (<see cref="MCLogistics::ComponentSort"/>).</summary>
    int32_t SortOrder = 0;
    /// <summary>The master component's name (28 characters and a terminator).</summary>
    char Name[29]{};
    /// <summary>How many copies there are.</summary>
    int32_t Count = 0;
    /// <summary>The index of the component in <see cref="MCLogistics::RangeSortList"/>.</summary>
    int32_t RangeIndex = 0;
    /// <summary>The description text (loaded on demand by <see cref="MCInventoryList::LoadDescription"/>).</summary>
    char* Description = nullptr;
    MCLogInventoryStat* Stats = nullptr;
    MCCompPurchaseBlock* PurchaseBlock = nullptr;
    MCCompInventoryBlock* InventoryBlock = nullptr;
    MCLogInventoryItem* Next = nullptr;
};

/// <summary>A list of components, sorted by master id, each with its copies.</summary>
/// <remarks>Original source: <c>logistics.cpp</c>, 0xc bytes.</remarks>
class MCInventoryList
{
public:
    MCInventoryList();
    /// <summary>Frees the items (<see cref="Destroy"/>).</summary>
    ~MCInventoryList();
    MCInventoryList(const MCInventoryList&) = delete;
    MCInventoryList& operator=(const MCInventoryList&) = delete;

    /// <summary>Gives <paramref name="item"/> a new inventory row (owned by the item), and returns it.</summary>
    static MCCompInventoryBlock* MakeInventoryBlock(MCLogInventoryItem* item);

    /// <summary>Loads the description of item <paramref name="index"/> (or <paramref name="item"/>) from the object description file.</summary>
    void LoadDescription(int32_t index, MCLogInventoryItem* item);

    /// <summary>Makes a copy record numbered with the next <see cref="NextStatID"/>.</summary>
    MCLogInventoryStat* CreateStat(uint8_t itemNum, uint8_t hits, uint8_t facing, int16_t amount, uint8_t location);

    /// <summary>The item at list position <paramref name="index"/>.</summary>
    MCLogInventoryItem* GetItemInfo(int32_t index);

    /// <summary>Adds <paramref name="count"/> to the count of the item with master id <paramref name="masterID"/> (not below 0).</summary>
    void AddCountToItem(int32_t count, int32_t masterID);

    /// <summary>Frees every item, its copies and its widgets.</summary>
    void Destroy();

    /// <summary>
    /// Adds a copy of component <paramref name="masterID"/>; a new item gets purchase and inventory widgets unless
    /// <paramref name="widgets"/> is -1.
    /// </summary>
    /// <returns>The copy's stat id.</returns>
    int32_t AddItem(uint8_t masterID, MCLogInventoryStat* stat, int32_t widgets);

    /// <summary>Removes copy <paramref name="statID"/> of component <paramref name="masterID"/>.</summary>
    int32_t RemoveItem(uint8_t masterID, int32_t statID);

    /// <summary>The item holding the copy with stat id <paramref name="statID"/>.</summary>
    MCLogInventoryItem* GetItemStatIndex(int32_t statID);

    /// <summary>The stat id of copy <paramref name="copy"/> of component <paramref name="masterID"/>, or -1.</summary>
    int32_t GetItemStatID(uint8_t masterID, int32_t copy);

    /// <summary>The list position of component <paramref name="masterID"/>, or -1.</summary>
    int32_t GetIndexFromMasterID(uint8_t masterID);

    /// <summary>The master id of the item at list position <paramref name="index"/>.</summary>
    int32_t GetMasterIDFromIndex(int32_t index);

    /// <summary>The master id of the item holding copy <paramref name="statID"/>.</summary>
    uint8_t GetMasterID(uint8_t statID);

    /// <summary>The facing of copy <paramref name="statID"/>.</summary>
    uint8_t GetFacing(uint8_t statID);

    /// <summary>The amount of copy <paramref name="statID"/>.</summary>
    int32_t GetAmount(uint8_t statID);

    /// <summary>The name of the item at list position <paramref name="masterID"/> (a position, despite the name).</summary>
    char* GetItemName(uint8_t masterID);

    /// <summary>How many copies of component <paramref name="masterID"/> there are.</summary>
    int32_t GetItemCount(uint8_t masterID);

    /// <summary>Sets the damage of copy <paramref name="statID"/>.</summary>
    int32_t HitItem(uint8_t statID, uint8_t hits);

    /// <summary>Sets the location of copy <paramref name="statID"/>.</summary>
    int32_t SetStatLoc(uint8_t statID, int32_t location);

    /// <summary>
    /// Writes the inventory into <paramref name="data"/> (null only measures it): the item count, then per item its
    /// master id (1 byte), copy count (4 bytes) and each copy's 0x1c-byte record.
    /// </summary>
    /// <returns>The size of the data when measuring; 4 when writing.</returns>
    int32_t GetBinaryData(void* data);

    /// <summary>The item positions sorted by the components' range.</summary>
    int32_t* SortRange();

    /// <summary>The item positions sorted by name.</summary>
    int32_t* SortName();

    MCLogInventoryItem* Items = nullptr;
    int32_t NumItems = 0;
    /// <summary>The stat id the next copy gets.</summary>
    uint8_t NextStatID = 0;
};

/// <summary>
/// What mechs and vehicles have in common in logistics: identity, tonnage, cost, status flags, inventory and the
/// briefing box. <see cref="PartType"/> tells which one a part is.
/// </summary>
/// <remarks>
/// Original source: <c>logistics.cpp</c> (no methods of its own). The records were written to and read from save
/// files and network messages as raw 32-bit images (<see cref="BinarySize"/> bytes: the record, then the name and
/// icon strings, then the inventory); the port reads and writes them field by field.
/// </remarks>
class MCLogPart
{
public:
    /// <summary>1 for a <see cref="MCLogMech"/>, 2 for a <see cref="MCLogVehicle"/>.</summary>
    int32_t PartType = 0;
    /// <summary>The name of the profile last written for the part ("tpak<i>n</i>", by the starting fit and save writers).</summary>
    char ProfileName[12]{};
    /// <summary>The weight class name (string table, from the tonnage).</summary>
    char* WeightClassName = nullptr;
    /// <summary>The chassis class name (string table, from the chassis tonnage).</summary>
    char* ChassisClassName = nullptr;
    /// <summary>
    /// The display name: string <c>DescIndex</c> + 300 (mechs) or + 700 (vehicles) of the string table. The original
    /// calls it the file name (it is saved as a profile's MechType).
    /// </summary>
    char* FileName = nullptr;
    /// <summary>The string table index of the name.</summary>
    int32_t NameIndex = 0;
    /// <summary>The size of the saved record with its strings and inventory.</summary>
    uint32_t BinarySize = 0;
    float CurTonnage = 0;
    /// <summary>The icon file name.</summary>
    char* IconName = nullptr;
    char Status = 0;
    /// <summary>The packet of the chassis in the object packet file.</summary>
    uint32_t Chassis = 0;
    /// <summary>The current value in resource points (a mech's; vehicles keep theirs at +0xb8).</summary>
    int32_t ResourcePoints = 0;
    /// <summary>The value of the bare chassis.</summary>
    int32_t BaseResourcePoints = 0;
    /// <summary>The part's number in the scenario file the force is written to (its Mates entry).</summary>
    int32_t PartNumber = 0;
    /// <summary>The description's index in the object description file (-1 = none).</summary>
    int32_t DescIndex = 0;
    char* Description = nullptr;
    /// <summary>The engine's tonnage.</summary>
    float EngineTonnage = 0;
    /// <summary>The engine rating.</summary>
    uint32_t EngineRating = 0;
    /// <summary>The armor type (a profile's Armor Type).</summary>
    uint8_t ArmorType = 0;
    /// <summary>The armor's tonnage (a profile's Armor Tonnage).</summary>
    float ArmorTonnage = 0;
    uint8_t NumOther = 0;
    uint8_t NumWeapons = 0;
    uint8_t NumAmmo = 0;
    /// <summary>The battle rating (<see cref="MCLogMech::CalcBR"/>).</summary>
    int32_t BattleRating = 0;
    /// <summary>Nonzero when assigned to the force.</summary>
    int32_t Assigned = 0;
    int32_t Deployed = 0;
    /// <summary>Nonzero when the mission requires this part.</summary>
    int32_t Required = 0;
    /// <summary>Nonzero until the part is really the player's (salvage not yet taken).</summary>
    int32_t NotMineYet = 0;
    /// <summary>1 for the local player's parts, 0 for ones received over the network.</summary>
    int32_t LocalPart = 0;
    /// <summary>The commander (player) id of a multiplayer part.</summary>
    int32_t CommanderID = 0;
    MCInventoryList* Inventory = nullptr;
    MCBriefingBox* BriefingBox = nullptr;
    /// <summary>The multiplayer drop slot's lance.</summary>
    uint32_t DropLance = 0;
    /// <summary>The multiplayer drop slot within its lance.</summary>
    uint32_t DropSlot = 0;
};

/// <summary>A mech in logistics: armor, internals, the critical-slot layout, its pilot and its screen widgets.</summary>
/// <remarks>Original source: <c>logistics.cpp</c>, 600 (0x258) bytes.</remarks>
class MCLogMech : public MCLogPart
{
public:
    /// <summary>Armor points of a body location: the maximum and the current.</summary>
    struct ArmorPoints
    {
        uint8_t MaxArmor = 0;
        uint8_t CurArmor = 0;
    };

    /// <summary>
    /// A critical slot's occupant, as a profile's <c>Component:n</c> pair: <see cref="Row"/> is the copy's item number
    /// (0xff = empty), <see cref="Column"/> its damage, then the component.
    /// </summary>
    struct ItemSlot
    {
        uint8_t Row = 0;
        uint8_t Column = 0;
        uint8_t MasterID = 0;
    };

    /// <summary>The pilot modifier: the pilot's rank against the mech's weight class.</summary>
    int32_t CalcPilotModifier();

    /// <summary>Recomputes <see cref="MCLogPart::ResourcePoints"/> from the chassis, the components and the damage.</summary>
    void CalcMechCost(int repaired);

    /// <summary>Recomputes the battle rating from the components.</summary>
    int32_t CalcBR();

    /// <summary>
    /// Puts copy <paramref name="itemNum"/> (damage <paramref name="hits"/>) of component <paramref name="masterID"/>
    /// in the first free critical slot of the location its form goes to.
    /// </summary>
    void PlaceItem(uint8_t masterID, int32_t itemNum, int32_t hits);

    /// <summary>Whether weapon <paramref name="masterID"/> takes a large slot.</summary>
    int32_t GetWeaponLarge(uint8_t masterID);

    /// <summary>The large weapons in body location <paramref name="location"/>.</summary>
    int32_t GetLargeWeaponCount(int32_t location);

    /// <summary>The small weapons in body location <paramref name="location"/>.</summary>
    int32_t GetSmallWeaponCount(int32_t location);

    /// <summary>Loads description <paramref name="descIndex"/> from the object description file.</summary>
    void LoadDescription(int32_t descIndex);

    /// <summary>The mech's condition (0..1) from armor, internals and pilot.</summary>
    float CalcStatus();

    /// <summary>A second string table name (from the description index).</summary>
    char* ExtraName1 = nullptr;
    /// <summary>A third string table name.</summary>
    char* ExtraName2 = nullptr;
    /// <summary>The name from the mech file.</summary>
    char* MechName = nullptr;
    /// <summary>The tonnage used by the chassis, engine and components.</summary>
    float UsedTonnage = 0;
    /// <summary>The tonnage left for components.</summary>
    float FreeTonnage = 0;
    /// <summary>The tonnage of the weapons and ammunition.</summary>
    float WeaponTonnage = 0;
    /// <summary>The pilot's index in the warrior list (-1 = none).</summary>
    int32_t PilotIndex = 0;
    /// <summary>The name variant (0..2; picks the sort key and the multiplayer variant).</summary>
    int32_t NameVariant = 0;
    int32_t SellValue = 0;
    /// <summary>The key the mech list is sorted by (<c>mechSort[nameIndex] * 3 + variant</c>).</summary>
    int32_t SortKey = 0;
    uint8_t MaxRunSpeed = 0;
    /// <summary>Head, center/left/right torso, left/right arm, left/right leg, rear center/left/right torso.</summary>
    ArmorPoints Armor[11]{};
    /// <summary>Nonzero where a location has CASE.</summary>
    int32_t HasCase[8]{};
    /// <summary>Internal structure of the eight locations: the chassis maximum and the current.</summary>
    ArmorPoints Internals[8]{};
    /// <summary>The critical slot grid of the eight locations (0xff = empty).</summary>
    ItemSlot ItemSlots[8][12]{};
    /// <summary>The hot spot of each location on the damage diagram.</summary>
    uint8_t HotSpotNumber[8]{};
    /// <summary>The chassis battle rating.</summary>
    int32_t ChassisBR = 0;
    int32_t PilotModifier = 0;
    /// <summary>The condition from <see cref="CalcStatus"/>.</summary>
    float StatusValue = 0;
    MCMechRepairBlock* RepairBlock = nullptr;
    MCMechInventoryBlock* InventoryBlock = nullptr;
    MCMechBriefBlock* BriefBlock = nullptr;
    /// <summary>The pilot of a multiplayer mech received over the network.</summary>
    MCLogWarrior* NetworkPilot = nullptr;
    MCLogMech* Next = nullptr;
};

/// <summary>A vehicle in logistics.</summary>
/// <remarks>Original source: <c>logistics.cpp</c>, 0xd0 bytes.</remarks>
class MCLogVehicle : public MCLogPart
{
public:
    /// <summary>The cost: the base value plus the components'.</summary>
    void CalcVehicleCost();

    /// <summary>Loads description <paramref name="descIndex"/> from the object description file.</summary>
    void LoadDescription(int32_t descIndex);

    /// <summary>The crew (the vehicle file's Crew).</summary>
    char Crew[9]{};
    uint8_t MaxMoveSpeed = 0;
    /// <summary>The current internal structure of the five locations.</summary>
    uint8_t CurInternalStructure[5]{};
    uint8_t MaxArmorPoints[5]{};
    uint8_t CurArmorPoints[5]{};
    /// <summary>The current value.</summary>
    int32_t VehicleResourcePoints = 0;
    /// <summary>The value of the bare vehicle.</summary>
    int32_t BaseVehicleResourcePoints = 0;
    MCVehicleRepairBlock* RepairBlock = nullptr;
    MCVehicleInventoryBlock* InventoryBlock = nullptr;
    MCMechBriefBlock* BriefBlock = nullptr;
    MCLogVehicle* Next = nullptr;
};

/// <summary>A MechWarrior in logistics: names, portrait, skills, wounds and status.</summary>
/// <remarks>
/// Original source: <c>logistics.cpp</c>, 300 (0x12c) bytes. Saved as a raw 32-bit image followed by its six
/// strings (<see cref="BinarySize"/> bytes); the port reads and writes it field by field.
/// </remarks>
class MCLogWarrior
{
public:
    /// <summary>The rank from the skills, weighted by <c>SkillWeightings</c> and cut by <c>WarriorRankScale</c>.</summary>
    void CalcRank();

    /// <summary>Loads description <paramref name="descIndex"/> from the object description file.</summary>
    void LoadDescription(int32_t descIndex);

    /// <summary>The profile file's base name.</summary>
    char FileName[12]{};
    MCLogWarrior* Next = nullptr;
    /// <summary>The size of the saved record with its strings.</summary>
    uint32_t BinarySize = 0;
    char* Name = nullptr;
    /// <summary>A unique id (<see cref="MCLogistics::NextWarriorID"/>).</summary>
    int32_t Id = 0;
    char* Callsign = nullptr;
    char* Picture = nullptr;
    char* PilotVideo = nullptr;
    char* PilotAudio = nullptr;
    /// <summary>The ABL brain file.</summary>
    char* Brain = nullptr;
    int32_t PaintScheme = 0;
    /// <summary>0 green .. 3 elite.</summary>
    int32_t Rank = 0;
    int32_t NameIndex = 0;
    int32_t DescIndex = 0;
    char* Description = nullptr;
    /// <summary>Professionalism, decorum, aggressiveness, courage.</summary>
    char Personality[4]{};
    /// <summary>Piloting, jumping, sensors, gunnery.</summary>
    char Skills[4]{};
    char OriginalSkills[4]{};
    char StartingSkills[4]{};
    /// <summary>Skill points earned towards the next level of each skill.</summary>
    float SkillPoints[4]{};
    char MechClass = 0;
    char MechType = 0;
    char WeaponClass = 0;
    char WeaponTypes[2]{};
    float Wounds = 0;
    /// <summary>6 minus the wounds (0 = dead).</summary>
    float Health = 0;
    /// <summary>4 = killed.</summary>
    int32_t WarriorStatus = 0;
    /// <summary>The lance of the drop slot the pilot's mech is in (-1 = none).</summary>
    int32_t DropLance = 0;
    /// <summary>The slot in <see cref="DropLance"/> (-1 = none).</summary>
    int32_t DropSlot = 0;
    int32_t Assigned = 0;
    /// <summary>Set while the pilot's mech is in a drop slot.</summary>
    int32_t Deployed = 0;
    int32_t Sold = 0;
    int32_t NotMineYet = 0;
    int32_t Ejected = 0;
    MCPilotInventoryBlock* InventoryBlock = nullptr;
};

/// <summary>A linked list of mechs, sorted by <see cref="MCLogMech::SortKey"/> or by tonnage.</summary>
/// <remarks>Original source: <c>logistics.cpp</c>, 0xc bytes.</remarks>
class MCLogMechList
{
public:
    MCLogMechList();

    /// <summary>Removes every mech.</summary>
    void Destroy();

    /// <summary>The position of <paramref name="mech"/>, or -1.</summary>
    int32_t GetMechIndex(MCLogMech* mech);

    /// <summary>Adds the mech in file <paramref name="fileName"/> under <c>profilePath</c>.</summary>
    MCLogMech* AddMech(char* fileName, int required, int sorted, int widgets);

    /// <summary>Replaces the mech at <paramref name="index"/> with one read from a save's packet.</summary>
    int32_t ReplaceMech(MCPacketFile* file, int32_t index);

    /// <summary>Adds a mech read from packet <paramref name="packet"/> of a save.</summary>
    MCLogMech* AddMech(MCPacketFile* file, int32_t packet);

    /// <summary>
    /// Reads a mech from its profile (or a raw save record when the file has no [General] block) and adds it, with
    /// its repair, inventory and briefing widgets when <paramref name="widgets"/> is set.
    /// </summary>
    MCLogMech* AddMech(MCFitIniFile* file, int required, int sorted, int widgets);

    /// <summary>Links <paramref name="mech"/> in (by tonnage when <paramref name="sorted"/>).</summary>
    int32_t AddMech(MCLogMech* mech, int sorted);

    /// <summary>Unlinks the mech at <paramref name="index"/> without freeing it.</summary>
    int32_t ExtractMech(int32_t index, MCLogMech*& mech);

    /// <summary>Deletes the mech at <paramref name="index"/>.</summary>
    int32_t RemoveMech(uint8_t index);

    /// <summary>Deletes <paramref name="mech"/>.</summary>
    int32_t RemoveMech(MCLogMech* mech);

    /// <summary>Unlinks <paramref name="mech"/> (after <paramref name="previous"/>) and frees it and its widgets.</summary>
    int32_t DeleteMech(MCLogMech* mech, MCLogMech* previous);

    int32_t GetMechCount();

    /// <summary>The saved size of the mech at <paramref name="index"/>.</summary>
    int32_t GetMechSize(uint32_t index);

    /// <summary>The pilot index of the mech at <paramref name="index"/>, or -1.</summary>
    int32_t GetMechPilotIndex(int32_t index);

    int32_t GetMechInfo(int32_t index, MCLogMech*& mech);

    /// <summary>Writes the saved form of the mech at <paramref name="index"/> into <paramref name="data"/>.</summary>
    int32_t GetBinaryData(uint32_t index, void* data);

    /// <summary>Writes the mech at <paramref name="index"/> as a profile text file.</summary>
    int32_t SaveMechText(char* fileName, int32_t index);

    /// <summary>Writes the mech at <paramref name="index"/> in its binary saved form.</summary>
    int32_t SaveMechBinary(char* fileName, int32_t index);

    MCLogMech* Mechs = nullptr;
    int32_t NumMechs = 0;
    /// <summary>The multiplayer player whose mechs these are (set by <see cref="MCLogistics::InitializeMultiplayer"/>).</summary>
    uint32_t PlayerID = 0;
};

/// <summary>A linked list of vehicles.</summary>
/// <remarks>Original source: <c>logistics.cpp</c>, 0xc bytes.</remarks>
class MCLogVehicleList
{
public:
    MCLogVehicleList();

    void Destroy();

    int32_t GetVehicleIndex(MCLogVehicle* vehicle);

    MCLogVehicle* AddVehicle(char* fileName, int required, int sorted, int widgets);

    /// <summary>Does nothing (vehicles are not replaced from saves).</summary>
    int32_t ReplaceVehicle(MCPacketFile* file, int32_t index);

    MCLogVehicle* AddVehicle(MCPacketFile* file, int32_t packet);

    /// <summary>Reads a vehicle from its profile (or a raw save record) and adds it.</summary>
    MCLogVehicle* AddVehicle(MCFitIniFile* file, int required, int sorted, int widgets);

    int32_t RemoveVehicle(uint8_t index);

    int32_t RemoveVehicle(MCLogVehicle* vehicle);

    int32_t DeleteVehicle(MCLogVehicle* vehicle, MCLogVehicle* previous);

    int32_t GetVehicleInfo(int32_t index, MCLogVehicle*& vehicle);

    int32_t GetVehicleCount();

    int32_t GetVehicleSize(uint32_t index);

    int32_t GetBinaryData(uint32_t index, void* data);

    int32_t SaveVehicleText(char* fileName, int32_t index);

    int32_t SaveVehicleBinary(char* fileName, int32_t index);

    MCLogVehicle* Vehicles = nullptr;
    int32_t NumVehicles = 0;
    /// <summary>The multiplayer player whose vehicles these are (set by <see cref="MCLogistics::InitializeMultiplayer"/>).</summary>
    uint32_t PlayerID = 0;
};

/// <summary>A linked list of MechWarriors.</summary>
/// <remarks>Original source: <c>logistics.cpp</c>, 8 bytes.</remarks>
class MCLogWarriorList
{
public:
    MCLogWarriorList();

    void Destroy();

    /// <summary>Adds the warrior in profile <paramref name="fileName"/> under <c>warriorPath</c>.</summary>
    int32_t AddWarrior(char* fileName, int sorted);

    /// <summary>Replaces the warrior at <paramref name="index"/> with one read from a save's packet.</summary>
    int32_t ReplaceWarrior(MCPacketFile* file, int32_t index);

    int32_t AddWarrior(MCPacketFile* file, int32_t packet, int sorted);

    /// <summary>Reads a warrior from its profile (or a raw save record) and adds it with its inventory widget.</summary>
    int32_t AddWarrior(MCFitIniFile* file, int sorted);

    /// <summary>Links <paramref name="warrior"/> in (by rank when <paramref name="sorted"/>).</summary>
    int32_t AddWarrior(MCLogWarrior* warrior, int sorted);

    int32_t ExtractWarrior(int32_t index, MCLogWarrior*& warrior);

    /// <summary>Takes <paramref name="amount"/> wounds off every living, unsold warrior (between missions).</summary>
    void Heal(int32_t amount);

    int32_t RemoveWarriorAtIndex(int32_t index);

    int32_t RemoveWarrior(uint8_t index);

    int32_t DeleteWarrior(MCLogWarrior* warrior, MCLogWarrior* previous);

    int32_t GetWarriorCount();

    int32_t GetWarriorSize(uint32_t index);

    /// <summary>Copies the profile file name of the warrior at <paramref name="index"/>.</summary>
    int32_t GetWarriorProfile(uint32_t index, char* dest);

    /// <summary>Copies the brain file name of the warrior at <paramref name="index"/>.</summary>
    int32_t GetWarriorBrain(uint32_t index, char* dest);

    /// <summary>The <see cref="MCLogWarrior::Id"/> of the warrior at <paramref name="index"/>.</summary>
    int32_t GetID(int32_t index);

    int32_t GetBinaryData(uint32_t index, void* data);

    int32_t SaveWarriorText(char* fileName, int32_t index);

    int32_t GetWarriorInfo(int32_t index, MCLogWarrior*& warrior);

    int32_t GetWarriorIndex(MCLogWarrior* warrior);

    /// <summary>Whether a warrior whose callsign is <paramref name="fileName"/> is in the list.</summary>
    int Exists(char* fileName);

    int32_t SaveWarriorBinary(char* fileName, int32_t index);

    /// <summary>Marks the warrior at <paramref name="index"/> deployed or not.</summary>
    void SetDeployed(int32_t index, int deployed);

    /// <summary>Re-sorts the list (assigned first, then by rank) and fixes the mechs' pilot indexes.</summary>
    void Reorder();

    MCLogWarrior* Warriors = nullptr;
    int32_t NumWarriors = 0;
};

/// <summary>A multiplayer drop slot: its lance and position, and the mech or vehicle placed there.</summary>
/// <remarks>Original source: <c>logistics.cpp</c>, 0xc bytes.</remarks>
class MCDropSlot
{
public:
    int32_t Lance = 0;
    int32_t Slot = 0;
    MCLogPart* Part = nullptr;
};

/// <summary>The row of lights on the multiplayer screens, one per player, showing whether each is ready.</summary>
/// <remarks>Original source: <c>logistics.cpp</c>, 0x50c bytes.</remarks>
class MCMPPlayerLights : public MCLogObject
{
public:
    static constexpr int32_t MAX_PLAYERS = 6;

    ~MCMPPlayerLights() override;

    /// <summary>Places the lights and loads their pictures.</summary>
    void Init();

    /// <summary>Frees the pictures and stops the blink timer.</summary>
    void Destroy() override;

    /// <summary>Sets how many lights there are (and the width).</summary>
    void SetNumPlayers(int32_t count);

    /// <summary>Sets the player shown by light <paramref name="light"/>.</summary>
    void SetPlayerID(int32_t light, uint32_t playerID);

    /// <summary>Sets a player's status (0..2); status 2 starts the blink timer.</summary>
    void SetPlayerStatus(uint32_t playerID, int32_t status);

    /// <summary>
    /// Paints the lights: each player's numbered light, lit (<c>lsc_ph</c>) or blinking over it, and the backing
    /// (<c>lsc_p0</c>) into the parent. Port: draws the lights from the state each frame; the parent screen draws the
    /// backing (<see cref="MCLogistics::DrawScreenChrome"/>).
    /// </summary>
    void Draw() override;

    /// <summary>Port: the lights draw themselves each frame (their port is a view).</summary>
    bool DrawsLive() override { return true; }

    /// <summary>The blink timer toggles the blink; pointing at a light shows the player's name on the ticker.</summary>
    void HandleEvent(MCGuiEvent* event) override;

    int32_t NumPlayers = 0;
    uint32_t PlayerIDs[MAX_PLAYERS] = {};
    int32_t PlayerStatus[MAX_PLAYERS] = {};
    /// <summary>The width of one light.</summary>
    int32_t LightWidth = 0;
    /// <summary>
    /// <see cref="Draw"/> paints the background into the parent when this differs from the parent. Cleared by
    /// <see cref="Init"/> and never set, so the background is painted on every draw once there is a parent.
    /// </summary>
    MCGuiObject* BackgroundParent = nullptr;
    /// <summary>Nonzero while the blink timer runs.</summary>
    int32_t TimerRunning = 0;
    /// <summary>The blink phase.</summary>
    int32_t BlinkOn = 0;
    /// <summary>The unlit lights (<c>lsc_pn.tga</c>).</summary>
    MCLogPort* LightsPort = nullptr;
    /// <summary>The ready light (<c>lsc_pg.tga</c>).</summary>
    MCLogPort* ReadyPort = nullptr;
    /// <summary>The blinking light (<c>lsc_pg1.tga</c>).</summary>
    MCLogPort* BlinkPort = nullptr;
};

/// <summary>
/// The logistics layer: owns the logistics blocks, every screen (main menu, multiplayer, load/save, preferences,
/// briefing, purchase, repair, session), the player's force and inventory, the art they share, and runs campaign
/// loading/saving and the multiplayer force exchange. One instance, <c>globalLogPtr</c>.
/// </summary>
/// <remarks>Original source: <c>logistics.cpp</c>, 0x1908 bytes (allocated in <c>mission.cpp</c>).</remarks>
class MCLogistics
{
public:
    /// <summary>Draws the damage state of location <paramref name="location"/> of a mech into <paramref name="port"/>.</summary>
    void DrawMechBodyLoc(MCLogMech* mech, int32_t location, MCLogPort* port, int32_t xPos, int32_t yPos);

    /// <summary>Draws the damage state of location <paramref name="location"/> of a vehicle into <paramref name="port"/>.</summary>
    void DrawVehicleBodyLoc(MCLogVehicle* vehicle, int32_t location, MCLogPort* port, int32_t xPos, int32_t yPos);

    /// <summary>
    /// Makes the logistics block store, the lists, every screen and dialog, loads the shared art, shapes and sort tables,
    /// and shows the main screen.
    /// </summary>
    void Init();

    /// <summary>Makes the multiplayer lists, drop slots and message buffer, and reads the net mech/warrior/vehicle lists.</summary>
    void InitializeMultiplayer();

    /// <summary>Frees what <see cref="InitializeMultiplayer"/> made.</summary>
    void DestroyMultiplayer();

    /// <summary>Frees everything <see cref="Init"/> made.</summary>
    void Destroy();

    /// <summary>Shows or hides the current logistics screen.</summary>
    void ShowLogScreen(int show, int redraw);

    int32_t SetUpMainScreen(int mode);

    /// <summary>Reads the campaign's purchase file name and costs from packet file <paramref name="file"/>.</summary>
    char* SetUpCampaignPurchasing(char* purchaseFile, MCPacketFile* file);

    /// <summary>Sets up what can be bought in a multiplayer game.</summary>
    void SetUpMPPurchasing(char* purchaseFile);

    /// <summary>Sets up what can be bought from a campaign's purchase packet.</summary>
    void SetUpPurchasing(MCPacketFile* file);

    /// <summary>Sets up what can be bought from an old-style purchase file.</summary>
    void SetUpOldPurchasing(char* purchaseFile);

    int32_t SetUpPurchaseScreen(int mode);

    /// <summary>
    /// Painted the screen switch buttons on the current screen: each normal, the current screen's grayed, none lit.
    /// Port: the buttons are drawn each frame (<see cref="DrawScreenChrome"/>); this puts out the lit one.
    /// </summary>
    void DrawScreenButtons();

    /// <summary>
    /// Port: screen button <paramref name="button"/> of <paramref name="screen"/> is under the mouse and shows lit
    /// (the original copied the lit picture over it).
    /// </summary>
    void HoverScreenButton(MCLogObject* screen, int32_t button);

    /// <summary>
    /// Port: draws the shared places of <paramref name="screen"/> from the state (<see cref="MCLogScreenChrome"/>): the
    /// multiplayer lights' backing, the screen buttons, the ticker line, the resource points and the clock.
    /// </summary>
    void DrawScreenChrome(MCLogObject* screen, MCPane* target);

    /// <summary>Switches to the briefing screen.</summary>
    int32_t SetUpBriefingScreen(int mode);

    int32_t SetUpSessionScreen();

    int32_t SetUpRepairScreen(int mode);

    /// <summary>Loads a quick-start force (mechs, pilots, vehicles) from <paramref name="file"/>.</summary>
    void LoadQuickStart(MCFitIniFile* file);

    int32_t SaveCampaign(char* fileName);

    /// <summary>Loads a campaign (or a saved game) and its force.</summary>
    int32_t LoadCampaign(char* campaignFile, char* saveFile, int newCampaign, int loadForce);

    /// <summary>Writes the deployed force into the mission's start files and starts the mission.</summary>
    int32_t PrepareScenario(char* scenarioName, char* startFile);

    /// <summary>Gives mech <paramref name="mechIndex"/> pilot <paramref name="pilotIndex"/>.</summary>
    void SetPilot(int32_t mechIndex, int32_t pilotIndex);

    void ReorderMechs();

    void ReorderVehicles();

    void ReorderWarriors();

    /// <summary>Shifts the pilot indexes after <paramref name="from"/> by <paramref name="amount"/>.</summary>
    void ShiftPilots(int32_t from, int32_t amount);

    /// <summary>Whether every required mech has a pilot.</summary>
    int RequiredAssigned();

    /// <summary>Reads the current mission's name and settings from the campaign.</summary>
    void GetCurrentMission();

    /// <summary>Wipes from <paramref name="from"/> to <paramref name="to"/> (a screen change).</summary>
    void Transition(MCLogPort* from, MCLogPort* to, int direction);

    /// <summary>Darkens <paramref name="port"/> through the fade table.</summary>
    void Darken(int32_t amount, char* fadeTable, MCLogPort* port);

    /// <summary>
    /// Port: <see cref="Darken"/> of a <paramref name="width"/> x <paramref name="height"/> block of
    /// <paramref name="port"/> at (<paramref name="xPos"/>, <paramref name="yPos"/>), drawn in place: the fade
    /// table's translate polygon over the block (the original copied the block out, translated it and copied it back,
    /// which comes out the same). On the GPU it is one blended quad.
    /// </summary>
    static void DarkenRect(MCLogPort* port, int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* fadeTable);

    /// <summary>Renumbers the component inventory's copies.</summary>
    int32_t ReIndexInventory();

    /// <summary>Drops pilot indexes above <paramref name="removedPilot"/> by one along the mech list from <paramref name="mech"/>.</summary>
    void RemoveReorderPilotIndexes(MCLogMech* mech, int32_t removedPilot);

    /// <summary>Handles a multiplayer "deploy force" message: adds the other player's mech or vehicle to its drop slot.</summary>
    void HandleDeployForceMessage(uint32_t playerID, const void* message);

    void HandleRemoveForceMessage(uint32_t playerID, const void* message);

    void HandleChatMessage(uint32_t playerID, const void* message);

    void SendRemoveForceMessage(int lance, int slot);

    void SendAddMechMessage(MCLogMech* mech, int lance, int slot);

    void SendAddVehicleMessage(MCLogVehicle* vehicle, int lance, int slot);

    /// <summary>A player left: removes their forces and tells the player.</summary>
    void HandleLostPlayer(uint32_t playerID, int hostLeft);

    void HandlePrepareScenarioMessage();

    int32_t PrepareMultiplayerScenario(char* scenarioName, char* startFile);

    /// <summary>Feeds a typed character to the cheat code matcher.</summary>
    void ProcessCheatCode(int16_t key);

    /// <summary>Draws the bar of the pilot's skill <paramref name="skill"/> (an index into its skills).</summary>
    void DrawPilotSkillBar(MCLogWarrior* warrior, int32_t skill, int32_t xPos, int32_t yPos, int32_t row, int32_t width,
                           int32_t rowHeight, MCLogPort* port);

    /// <summary>
    /// Draws a 4-pixel skill bar <paramref name="width"/> wide at (<paramref name="xPos"/>, <paramref name="yPos"/> +
    /// <paramref name="row"/> * <paramref name="rowHeight"/>), filled in proportion to <paramref name="value"/>
    /// between <c>MinPilotSkill</c> and <c>MaxPilotSkill</c>.
    /// </summary>
    void DrawPilotSkillBar(int32_t value, int32_t xPos, int32_t yPos, int32_t row, int32_t width, int32_t rowHeight,
                           MCLogPort* port);

protected:
    /// <summary>Marks which of the 12 drop slots are the local player's.</summary>
    void SetupSlotsForMultiplayer(int playerIndex, int numPlayers);

    /// <summary>The mech list of player <paramref name="playerID"/>.</summary>
    MCLogMechList* FindMPMechList(uint32_t playerID, int teammate, int* listIndex);

    MCLogVehicleList* FindMPVehicleList(uint32_t playerID, int teammate);

    /// <summary>Bumps the pilot indexes along the mech list from <paramref name="mech"/>.</summary>
    void AddReorderPilotIndexes(MCLogMech* mech);

    /// <summary>Does nothing.</summary>
    void AddReorderPilotIndexes(MCLogVehicle* vehicle);

    /// <summary>Does nothing.</summary>
    void RemoveReorderPilotIndexes(MCLogVehicle* vehicle, MCLogVehicle* previous);

    /// <summary>Builds a mech (with its pilot and inventory) from a "deploy force" message.</summary>
    MCLogPart* AddMechFromNetworkMessage(MCLogMechList* list, MCFIMessageHeader* message);

    MCLogPart* AddVehicleFromNetworkMessage(MCLogVehicleList* list, MCFIMessageHeader* message);

    /// <summary>Empties drop slot <paramref name="slot"/> of the player's or the opponents' table.</summary>
    int RemoveForceAtDropSlot(int32_t slot, uint32_t playerID, int teamTable);

public:
    /// <summary>The name ticker on the main screen.</summary>
    MCTicker* Ticker = nullptr;
    /// <summary>The multiplayer ready lights.</summary>
    MCGuiOwned<MCMPPlayerLights> PlayerLights;
    /// <summary>The current mission's number in the campaign (-1 = none).</summary>
    int32_t CurrentMission = 0;
    /// <summary>The campaign's purchase file (the save's "purchaseFile"; written to starting fits as PurchaseFile).</summary>
    char PurchaseFile[0x80]{};
    /// <summary>Every pilot the player has.</summary>
    MCLogWarriorList* WarriorList = nullptr;
    /// <summary>The pilots assigned to mechs.</summary>
    MCLogWarriorList* AssignedWarriorList = nullptr;
    /// <summary>Every mech the player has.</summary>
    MCLogMechList* MechList = nullptr;
    /// <summary>Every vehicle the player has.</summary>
    MCLogVehicleList* VehicleList = nullptr;
    /// <summary>The mechs in the force (repair and briefing screens).</summary>
    MCLogMechList* ForceMechList = nullptr;
    /// <summary>The vehicles in the force.</summary>
    MCLogVehicleList* ForceVehicleList = nullptr;
    /// <summary>The other multiplayer players' mechs, per team side and player (<see cref="FindMPMechList"/>).</summary>
    MCLogMechList* MpMechLists[2][3]{};
    /// <summary>The other multiplayer players' vehicles.</summary>
    MCLogVehicleList* MpVehicleLists[2][3]{};
    /// <summary>The pilots of mechs received over the network.</summary>
    MCLogWarriorList* MpWarriorList = nullptr;
    MCPurMechList* PurMechList = nullptr;
    MCPurVehicleList* PurVehicleList = nullptr;
    MCPurPilotList* PurPilotList = nullptr;
    /// <summary>The player's spare components.</summary>
    MCInventoryList* ComponentInventory = nullptr;
    /// <summary>The components for sale.</summary>
    MCInventoryList* PurchaseComponents = nullptr;
    /// <summary>The number of entries in <see cref="RangeSortList"/>.</summary>
    int32_t NumRangeSorted = 0;
    /// <summary>Component master ids in range order (<c>logart\comp.rsp</c>).</summary>
    uint32_t* RangeSortList = nullptr;
    /// <summary>Which screen or sub-screen logistics is on (1 main, 2.. the others, as the callbacks set it).</summary>
    int32_t LogisticsState = 0;
    /// <summary>
    /// The <see cref="LogisticsState"/> before the main menu opened over it: the menu's return and a finished save
    /// go back to the purchase (2) or repair (4) screen, else the briefing.
    /// </summary>
    int32_t PreviousState = 0;
    /// <summary>The clock text ("HH:MM:SS", from the CRT's <c>_strtime</c>), written by <c>RepairScreen::display</c>.</summary>
    char TimeString[0xc]{};
    /// <summary>
    /// The blocks of logistics data (strings, tables, shapes, port bitmaps) that have no single owner yet (the
    /// logistics heap's in the original); made by <see cref="Init"/> and cleared by <see cref="Destroy"/>.
    /// </summary>
    std::unique_ptr<MCBlockStore> LogisticsBlocks;
    /// <summary>
    /// Which of the 12 drop slots (three lances of four) may be filled: the local player's in multiplayer; in single
    /// player the briefing screen shows the lances whose slots are set.
    /// </summary>
    int32_t LocalDropSlot[12]{};
    /// <summary>A world position (x, y) per drop zone: three for side 0, then three for side 1 (MultiPlayer homeTeam 1).</summary>
    struct DropZonePosition
    {
        float X = 0;
        float Y = 0;
    } DropZonePositions[6]{};
    /// <summary>The drop zone: per lance and slot, the index of the mech or vehicle placed there (-1 = none).</summary>
    struct DeploySlot
    {
        /// <summary>The mech's index in the force mech list, or -1.</summary>
        int32_t Unit = 0;
        /// <summary>The vehicle's index in the force vehicle list, or -1 (only read when <see cref="Unit"/> is -1).</summary>
        int32_t Vehicle = 0;
    } DeploySlots[3][4]{};
    /// <summary>
    /// Per drop zone and slot, the unit's place relative to the zone (the mission's <c>OffsetX</c>, <c>OffsetY</c>
    /// and <c>Rotation</c>, read by <see cref="GetCurrentMission"/>): the six zones of <see cref="DropZonePositions"/>.
    /// <see cref="Init"/> clears only the first three.
    /// </summary>
    struct DeploySlotInfo
    {
        float OffsetX = 0;
        float OffsetY = 0;
        float Rotation = 0;
    } DeploySlotPlacements[6][4]{};
    /// <summary>The drop slots every force is placed in (multiplayer).</summary>
    MCDropSlot* DropSlots[3][4]{};
    /// <summary>The drop slots of the players not on the local player's team (multiplayer).</summary>
    MCDropSlot* OpponentDropSlots[3][4]{};
    /// <summary>The current mission's name (freed by <see cref="Destroy"/>).</summary>
    char* MissionFileName = nullptr;
    /// <summary>The inventory tab shown: 0 mechs, 1 pilots, 2 components, 3 vehicles.</summary>
    int32_t CurrentInvTab = 0;
    /// <summary>The cost of an armor point (the purchase file's PurchaseCosts).</summary>
    int32_t ArmorCost = 0;
    /// <summary>The cost of an internal structure point.</summary>
    int32_t InternalCost = 0;
    /// <summary>The cost of engine work.</summary>
    int32_t EngineCost = 0;
    /// <summary>The cost of a green, regular, veteran and elite pilot.</summary>
    int32_t PilotCosts[4]{};
    /// <summary>The price factor of clan technology.</summary>
    float ClanCostFactor = 0;
    /// <summary>The screen being shown.</summary>
    MCLogObject* CurrentScreen = nullptr;
    MCSessionScreen* SessionScreen = nullptr;
    MCSplashScreen* SerialScreen = nullptr;
    /// <summary>The multiplayer connection screen (the one after the protocol choice).</summary>
    MCSplashScreen* ConnectScreen = nullptr;
    MCSplashScreen* ModemScreen = nullptr;
    MCSplashScreen* LanScreen = nullptr;
    MCSplashScreen* MainScreen = nullptr;
    MCSplashScreen* MultiplayerScreen = nullptr;
    MCSplashScreen* LoadScreen = nullptr;
    MCSplashScreen* SaveScreen = nullptr;
    MCSplashScreen* PrefScreen = nullptr;
    MCBriefingScreen* BriefingScreen = nullptr;
    MCPurchaseScreen* PurchaseScreen = nullptr;
    MCRepairScreen* RepairScreen = nullptr;
    /// <summary>The preferences as they were when the preferences screen opened (restored by CancelPrefs).</summary>
    int32_t SavedPrefs0 = 0;
    int32_t SavedPrefs1 = 0;
    int32_t SavedPrefs2 = 0;
    uint32_t SavedPrefs3 = 0;
    uint32_t SavedPrefs4 = 0;
    uint32_t SavedPrefs5 = 0;
    int32_t SavedPrefs6 = 0;
    /// <summary>
    /// The current mission's operation number (the campaign's Operation; picks the briefing's operation picture),
    /// 0 when it has none.
    /// </summary>
    int32_t Operation = 0;
    MCLogChatWindow* ChatWindow = nullptr;
    /// <summary>The mech repair screen shapes (<c>mechrep##.shp</c>).</summary>
    void* MechRepShapes[24]{};
    /// <summary>The vehicle repair screen shapes (<c>vr1_##.shp</c>).</summary>
    void* VehicleRepShapes[35]{};
    /// <summary>The mech icon shapes (<c>mi##.shp</c>).</summary>
    void* MechIconShapes[24]{};
    /// <summary>The vehicle icon shapes (<c>vi1_##.shp</c>).</summary>
    void* VehicleIconShapes[35]{};
    /// <summary>Color remap tables for drawing shapes (<c>VFX_shape_lookaside</c>).</summary>
    uint8_t ShapeLookaside[10][256]{};
    /// <summary>The repair screen's mech picture background (<c>lsrupm00.tga</c>).</summary>
    MCLogPort* RepairBackPort = nullptr;
    /// <summary>The inventory block background (<c>invblock.tga</c>).</summary>
    MCLogPort* InvBlockPort = nullptr;
    /// <summary>
    /// The inventory pane's contents per tab (mechs, pilots, components, vehicles), made by the
    /// <c>MCLogInvScreen::Create*InvBlock</c> functions: views each tab's rows are drawn into.
    /// </summary>
    std::array<std::unique_ptr<MCLogPort>, 4> InvTabPorts;
    /// <summary>The box behind the resource figure at the top right (<c>RepairScreen::display</c>).</summary>
    MCLogPort* ResourceBackPort = nullptr;
    /// <summary>The box behind the clock at the top right (<c>RepairScreen::display</c>).</summary>
    MCLogPort* ClockBackPort = nullptr;
    /// <summary>The repair screen pieces (<c>lsrupm01..07.tga</c>).</summary>
    MCLogPort* RepairPorts[6]{};
    /// <summary>The purchase screen pieces (<c>lspcb05..09.tga</c>).</summary>
    MCLogPort* PurchasePorts[4]{};
    /// <summary>The two full-screen work ports (0x1ab x 0x1ce) the screens draw into.</summary>
    MCLogPort* WorkPort1 = nullptr;
    MCLogPort* WorkPort0 = nullptr;
    /// <summary>The screen switch buttons' pictures: button 0, exit, buttons 1..3; normal, highlighted, gray.</summary>
    MCLogPort* ScreenButtonPorts[5][3]{};
    /// <summary>The inventory tab icons: mechs, pilots, components, vehicles (<c>lscii?.tga</c>).</summary>
    MCLogPort* InventoryIconPorts[4]{};
    /// <summary>
    /// The chat text colour of each player number (1, 3, 4, 2, 6, 5), used as <c>%fc</c> codes by
    /// <c>LogChatWindow::processChatString</c>.
    /// </summary>
    int32_t PlayerColors[6]{};
    /// <summary>Each component's place in the logistics sort order (<c>objsort.rsp</c>).</summary>
    int32_t ComponentSort[256]{};
    /// <summary>The icon following the mouse while an inventory row is dragged (<see cref="MCDragIcon::Create"/>, <see cref="MCDragIcon::Remove"/>).</summary>
    MCGuiOwned<MCDragIcon> DragIcon;
    /// <summary>The id the next <see cref="MCLogWarrior"/> gets.</summary>
    int32_t NextWarriorID = 0;
    MCPurchaseDlg* PurchaseDialog = nullptr;
    /// <summary>The one-button (or yes/no) message dialog.</summary>
    MCReusableDialog* MessageDialog = nullptr;
    /// <summary>The yes/no dialog.</summary>
    MCReusableDialog* QuestionDialog = nullptr;
    MCRefitDialog* RefitDialog = nullptr;
    /// <summary>The campaign's CampaignBriefing Filename (0x29 bytes, a logistics block; freed by <see cref="Destroy"/>).</summary>
    char* CampaignBriefingName = nullptr;
    /// <summary>
    /// The mission's OperationCinema: the briefing movie played in <c>data\movies\</c> (0x29 bytes, a logistics block;
    /// null when the mission has none).
    /// </summary>
    char* OperationCinema = nullptr;
    /// <summary>Set when the mission file has a HammerDown1 block: the drop tonnage limit is not enforced.</summary>
    int32_t HammerDown = 0;
    /// <summary>The mission's AutoPlay: the briefing screen starts the operation movie when first shown.</summary>
    int32_t AutoPlayMovie = 0;
    /// <summary>The mechs allowed in multiplayer (<c>netmechs.rsp</c>).</summary>
    MCFLinkedList<char> NetMechNames;
    /// <summary>The pilots allowed in multiplayer (<c>netwars.rsp</c>).</summary>
    MCFLinkedList<char> NetWarriorNames;
    /// <summary>The vehicles allowed in multiplayer (<c>netvhcls.rsp</c>).</summary>
    MCFLinkedList<char> NetVehicleNames;
    /// <summary>Nonzero once <see cref="InitializeMultiplayer"/> ran.</summary>
    int32_t MultiplayerInitialized = 0;
    /// <summary>The buffer outgoing force messages are built in.</summary>
    MCFIMessageHeader* MessageBuffer = nullptr;
    /// <summary>The default multiplayer planning time in seconds (240).</summary>
    uint32_t DefaultPlanningTime = 0;
    /// <summary>The multiplayer planning time: a save's PlanningTime, else <see cref="DefaultPlanningTime"/>.</summary>
    uint32_t PlanningTime = 0;
    /// <summary>The multiplayer mission name (0x80 bytes).</summary>
    char* MpMissionName = nullptr;
};

/// <summary>The logistics screens' default button callback (does nothing).</summary>
void LogisticsCallback();

/// <summary><c>qsort</c> comparer of mechs by id.</summary>
int CompareLogMechIDs(const void* a, const void* b);

/// <summary><c>qsort</c> comparer of vehicles by id.</summary>
int CompareLogVehicleIDs(const void* a, const void* b);

/// <summary>The player's user name (the multiplayer default name).</summary>
int MyGetUserName(char* name, uint32_t* size);

/// <summary>A dialog's answer: cancels.</summary>
void CancelBool(int32_t answer);

/// <summary>Returns to the multiplayer session screen.</summary>
void BackToSession();

/// <summary>A dialog's answer: returns to the session screen.</summary>
void BackToSessionBool(int32_t answer);

/// <summary>The "player left" dialog's answer.</summary>
void LostPlayerHandler(int32_t answer);

/// <summary>The multiplayer chat handler while in logistics.</summary>
void LogisticsChatCallback(MCFidpMessage* message, void* data);

/// <summary>How much each skill weighs in a pilot's rank.</summary>
extern float SkillWeightings[4];
/// <summary>The weighted skill each rank ends at.</summary>
extern float WarriorRankScale[4];
/// <summary>Each mech name index's place in the mech sort order.</summary>
extern int32_t MechSort[];
/// <summary>The object packet file under <c>objectPath</c> the chassis profiles are read from ("object2.pak").</summary>
extern char ObjectPakName[20];
extern char MissionName[];
/// <summary>The current planet (campaign setting).</summary>
extern int32_t CurPlanet;
/// <summary>The logistics screens while they exist (a view: <c>Mission()-&gt;Logistics</c> owns them).</summary>
extern MCLogistics* GlobalLogPtr;
/// <summary>The logistics state last left for a mission (shared with <c>mission.cpp</c>).</summary>
extern int32_t LastLogisticsMissionState;
extern char HoldString[256];
/// <summary>Which logistics cheat codes are on.</summary>
extern int LogCheatActive[];
/// <summary>The multiplayer players' colors.</summary>
extern int32_t MultiPlayerColors[];
/// <summary>How many characters of a cheat code have been typed.</summary>
extern int32_t LogCurCheatChar;
/// <summary>Nonzero in the demo version.</summary>
extern int InDemo;
/// <summary>
/// A single (non-campaign) mission is being played: units sell back at full price (as in multiplayer) rather than
/// half.
/// </summary>
extern bool Solo;
