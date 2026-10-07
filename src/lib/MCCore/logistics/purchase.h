#pragma once

#include "logistics/lport.h"

class MCGuiEvent;
class MCInventoryList;
class MCMechPurchaseBlock;
class MCPilotPurchaseBlock;
class MCVehiclePurchaseBlock;
struct MCLogInventoryItem;

/// <summary>The maximum and current armor of one body location of a mech for sale.</summary>
struct MCPurArmorLocation
{
    uint8_t MaxArmor = 0;
    uint8_t CurArmor = 0;
};

/// <summary>One critical slot of a body location of a mech for sale: the component and its damage.</summary>
struct MCPurCriticalSlot
{
    /// <summary>Master component ID, 0xff when empty.</summary>
    uint8_t MasterId = 0;
    /// <summary>Hits taken (passed to <c>InventoryList::hitItem</c>).</summary>
    uint8_t Damage = 0;
};

/// <summary>
/// One variant of a mech offered for sale, read from its profile (<c>&lt;profilePath&gt;&lt;name&gt;.fit</c>) by
/// <see cref="MCPurMechList::addMech(PurMech*, char*, int32_t)"/>.
/// </summary>
/// <remarks>Original source: <c>logistics\purchase.cpp</c>, 0x148 bytes (allocated on the logistics heap).</remarks>
class MCPurMechData
{
public:
    /// <summary>Loads description <paramref name="descIndex"/> of the object description file into <see cref="Description"/>.</summary>
    void LoadDescription(int32_t descIndex);

    /// <summary>Recomputes <see cref="BattleRating"/> from <see cref="ChassisBR"/> and the inventory.</summary>
    int32_t CalcBR();

    /// <summary>The profile's file name (without extension).</summary>
    char FileName[12] = {};
    /// <summary>The display name (string table entry <c>descIndex + 300</c>).</summary>
    char Name[41] = {};
    float CurTonnage = 0.0f;
    int32_t NameIndex = 0;
    /// <summary>Price in resource points: the file's ResourcePoints plus armor and components.</summary>
    int32_t Cost = 0;
    uint8_t MaxRunSpeed = 0;
    /// <summary>The Armor block's Tonnage.</summary>
    float ArmorTonnage = 0.0f;
    /// <summary>Head, CenterTorso, LeftTorso, RightTorso, LeftArm, RightArm, LeftLeg, RightLeg, then the three rear torsos.</summary>
    MCPurArmorLocation Armor[11] = {};
    uint8_t NumOther = 0;
    uint8_t NumWeapons = 0;
    uint8_t NumAmmo = 0;
    /// <summary>Current internal structure of the 8 body locations.</summary>
    uint8_t CurInternalStructure[8] = {};
    /// <summary>Critical slots of the 8 body locations (the count per location comes from a global table).</summary>
    MCPurCriticalSlot CriticalSlots[8][12] = {};
    /// <summary>How many of this variant the shop has.</summary>
    int32_t NumAvailable = 0;
    int32_t BattleRating = 0;
    /// <summary>The file's ChassisBR (100 when missing).</summary>
    int32_t ChassisBR = 0;
    /// <summary>The components the mech carries.</summary>
    MCInventoryList* Inventory = nullptr;
    /// <summary>The description text, a logistics block.</summary>
    char* Description = nullptr;
    int32_t DescIndex = -1;
};

/// <summary>A mech type for sale: up to three variants and the block that shows them.</summary>
/// <remarks>Original source: <c>logistics\purchase.cpp</c>, 0x14 bytes.</remarks>
class MCPurMech
{
public:
    MCPurMechData* Variants[3] = {};
    MCMechPurchaseBlock* Block = nullptr;
    MCPurMech* Next = nullptr;
};

/// <summary>The mechs of the shop, as a singly linked list (newest first).</summary>
/// <remarks>Original source: <c>logistics\purchase.cpp</c>, 8 bytes.</remarks>
class MCPurMechList
{
public:
    MCPurMechList();

    void Init();

    /// <summary>Frees every mech, its variants and its block.</summary>
    void Destroy();

    /// <summary>Reads profile <paramref name="fileName"/> into variant <paramref name="variant"/> of <paramref name="purMech"/>.</summary>
    int32_t AddMech(MCPurMech* purMech, char* fileName, int32_t variant);

    /// <summary>
    /// Adds a mech type with its three variants and how many of each are for sale, makes its block and shows
    /// the first variant with stock.
    /// </summary>
    int32_t AddMech(char* fileName0, int32_t count0, char* fileName2, int32_t count2, char* fileName1, int32_t count1);

    /// <summary>Adds to the stock of the mech type whose first variant is <paramref name="fileName"/> (never below 0).</summary>
    /// <returns>0, or -1 when there is none.</returns>
    int32_t ModMech(char* fileName, int32_t delta0, int32_t delta2, int32_t delta1);

    /// <summary>Does nothing.</summary>
    int32_t RemoveMech(uint8_t index);

    /// <summary>The <paramref name="index"/>th mech.</summary>
    /// <returns>0, or -1 past the end.</returns>
    int32_t GetMechInfo(int32_t index, MCPurMech*& purMech);

    int32_t GetMechCount();

    MCPurMech* First = nullptr;
    int32_t Count = 0;
};

/// <summary>A MechWarrior for hire, read from <c>&lt;warriorPath&gt;&lt;name&gt;.fit</c>.</summary>
/// <remarks>Original source: <c>logistics\purchase.cpp</c>, 0x148 bytes.</remarks>
class MCPurPilotData
{
public:
    /// <summary>Sets <see cref="Rank"/> from the weighted skills (<c>SkillWeightings</c>, <c>WarriorRankScale</c>).</summary>
    void CalcRank();

    /// <summary>Loads description <paramref name="descIndex"/> into <see cref="Description"/> (once).</summary>
    void LoadDescription(int32_t descIndex);

    char FileName[12] = {};
    char Callsign[21] = {};
    /// <summary>The pilot's speech sample set (pilotAudio).</summary>
    char PilotAudio[255] = {};
    int32_t NameIndex = 0;
    char Piloting = 0;
    char Gunnery = 0;
    char Jumping = 0;
    char Sensors = 0;
    /// <summary>6 minus the file's Wounds.</summary>
    char Health = 0;
    int32_t Rank = 0;
    /// <summary>Hiring price: the logistics price of <see cref="Rank"/>.</summary>
    int32_t Cost = 0;
    /// <summary>The DescIndex, also used as the pilot's ID (<see cref="MCPurPilotList::SetPilotStatus"/>).</summary>
    int32_t DescIndex = -1;
    char* Description = nullptr;
    /// <summary>0 = for hire (visible); 1 = hired; 3 = sold back.</summary>
    int32_t Status = 0;
    MCPilotPurchaseBlock* Block = nullptr;
    MCPurPilotData* Next = nullptr;
};

/// <summary>The pilots for hire, sorted by rank then callsign.</summary>
/// <remarks>Original source: <c>logistics\purchase.cpp</c>, 8 bytes. No constructor in the binary.</remarks>
class MCPurPilotList
{
public:
    /// <summary>Frees every pilot and its block.</summary>
    void Destroy();

    /// <summary>Reads pilot <paramref name="fileName"/>, makes its block, and inserts it in rank order.</summary>
    int32_t AddPilot(char* fileName, int32_t status);

    int32_t RemovePilot(int32_t index);

    /// <summary>Sets the <see cref="MCPurPilotData::Status"/> of the pilot whose ID is <paramref name="pilotId"/>.</summary>
    void SetPilotStatus(int32_t pilotId, int32_t status);

    /// <returns>0, or -1 past the end.</returns>
    int32_t GetPilotInfo(int32_t index, MCPurPilotData*& pilot);

    /// <summary>How many pilots are still for hire (status 0).</summary>
    int32_t GetVisiblePilotCount();

    MCPurPilotData* First = nullptr;
    int32_t Count = 0;
};

/// <summary>A vehicle type for sale, read from its profile.</summary>
/// <remarks>Original source: <c>logistics\purchase.cpp</c>, 0x3c bytes.</remarks>
class MCPurVehicleData
{
public:
    /// <summary>Loads description <paramref name="descIndex"/> into <see cref="Description"/> (once).</summary>
    void LoadDescription(int32_t descIndex);

    char FileName[12] = {};
    /// <summary>The display name (string table entry <c>descIndex + 700</c>), a logistics block.</summary>
    char* Name = nullptr;
    float CurTonnage = 0.0f;
    int32_t NameIndex = 0;
    /// <summary>The file's ResourcePoints (100 when missing).</summary>
    int32_t BaseCost = 0;
    /// <summary><see cref="BaseCost"/> plus the components (<see cref="MCPurVehicle::CalcVehicleCost"/>).</summary>
    int32_t Cost = 0;
    int32_t DescIndex = -1;
    char* Description = nullptr;
    uint8_t MaxMoveSpeed = 0;
    /// <summary>The Armor block's Tonnage.</summary>
    float ArmorTonnage = 0.0f;
    uint8_t NumOther = 0;
    uint8_t NumWeapons = 0;
    uint8_t NumAmmo = 0;
    int32_t NumAvailable = 0;
    MCInventoryList* Inventory = nullptr;
};

/// <summary>A vehicle type for sale and its block.</summary>
/// <remarks>Original source: <c>logistics\purchase.cpp</c>, 0xc bytes.</remarks>
class MCPurVehicle
{
public:
    /// <summary>Sets <see cref="MCPurVehicleData::Cost"/> from the base cost and the components.</summary>
    void CalcVehicleCost();

    MCPurVehicleData* Data = nullptr;
    MCVehiclePurchaseBlock* Block = nullptr;
    MCPurVehicle* Next = nullptr;
};

/// <summary>The vehicles of the shop, sorted by tonnage.</summary>
/// <remarks>Original source: <c>logistics\purchase.cpp</c>, 8 bytes.</remarks>
class MCPurVehicleList
{
public:
    MCPurVehicleList();

    void Init();

    /// <summary>Frees every vehicle, its data and its block.</summary>
    void Destroy();

    /// <summary>Adds <paramref name="delta"/> to the stock of vehicle <paramref name="fileName"/> (never below 0).</summary>
    /// <returns>0, or -1 when there is none.</returns>
    int32_t ModVehicle(char* fileName, int32_t delta);

    /// <summary>
    /// Reads vehicle profile <paramref name="fileName"/> with <paramref name="count"/> for sale, makes its block
    /// and inserts it in tonnage order.
    /// </summary>
    /// <remarks>
    /// When the file
    /// has no General block the original falls back to reading raw 0xd0-byte records into the 0xc-byte
    /// <see cref="MCPurVehicle"/>, which overruns it.
    /// </remarks>
    int32_t AddVehicle(char* fileName, int32_t count);

    /// <summary>Does nothing.</summary>
    int32_t RemoveVehicle(uint8_t index);

    /// <returns>0, or -1 past the end.</returns>
    int32_t GetVehicleInfo(int32_t index, MCPurVehicle*& vehicle);

    int32_t GetVehicleCount();

    MCPurVehicle* First = nullptr;
    int32_t Count = 0;
};

/// <summary>A row of the mech shop (0x19a x 0x70): picture, diagram, class texts, variants and price.</summary>
/// <remarks>Original source: <c>logistics\purchase.cpp</c>, 0x4e0 bytes.</remarks>
class MCMechPurchaseBlock : public MCLogObject
{
public:
    ~MCMechPurchaseBlock() override;

    /// <summary>Shows <paramref name="purMech"/> (variant <see cref="CurVariant"/>, set by the caller first).</summary>
    void Init(MCPurMech* purMech);

    /// <summary>Frees the ports and texts.</summary>
    void Destroy() override;

    /// <summary>Variant buttons, buy (drag to the inventory), and the info display.</summary>
    void HandleEvent(MCGuiEvent* event) override;

    /// <summary>Empty: the row is drawn by <see cref="DrawBackground"/>.</summary>
    void Draw() override;

    /// <summary>
    /// Draws the row at list position <paramref name="row"/>. Port: the row is drawn by <see cref="DrawRow"/>; this
    /// makes the picture and diagram when missing, the class texts and the description.
    /// </summary>
    void DrawBackground(int32_t row);

    /// <summary>
    /// Port: draws the row into <paramref name="port"/> (the store's view) with its top at <paramref name="top"/>:
    /// what the original's <c>drawBackground</c> painted into the store's picture.
    /// </summary>
    void DrawRow(MCLogPort* port, int32_t top);

    /// <summary>
    /// Port: draws the drag icon's picture into <paramref name="surface"/>: a 0x20 square of the row, over the
    /// store's background.
    /// </summary>
    void OnBeginDrag(MCLogPort* surface);

    /// <summary>Empty.</summary>
    void SetBar();

    /// <summary>The list position the row is drawn at.</summary>
    int32_t Row = 0;
    MCPurMech* PurMech = nullptr;
    /// <summary>The shown variant's name index.</summary>
    int32_t NameIndex = 0;
    /// <summary>The variant shown (0..2), -1 before <see cref="MCPurMechList::AddMech"/> picks one.</summary>
    int32_t CurVariant = -1;
    /// <summary>The mech's picture (0x4b x 100), built by the first <see cref="DrawBackground"/>.</summary>
    MCLogPort* PicturePort = nullptr;
    /// <summary>The small body diagram (0x1e x 0x1e), built on first draw.</summary>
    MCLogPort* DiagramPort = nullptr;
    /// <summary>Weight class by current tonnage, a logistics block.</summary>
    char* WeightClassText = nullptr;
    /// <summary>Armor rating by armor tonnage.</summary>
    char* ArmorText = nullptr;
    /// <summary>Internal structure rating by the total internal structure.</summary>
    char* InternalText = nullptr;
};

/// <summary>A row of the vehicle shop.</summary>
/// <remarks>Original source: <c>logistics\purchase.cpp</c>, 0x4d4 bytes.</remarks>
class MCVehiclePurchaseBlock : public MCLogObject
{
public:
    ~MCVehiclePurchaseBlock() override;

    /// <summary>Shows <paramref name="purVehicle"/>; builds its class texts.</summary>
    void Init(MCPurVehicle* purVehicle);

    void Destroy() override;

    void HandleEvent(MCGuiEvent* event) override;

    /// <summary>
    /// Draws the row at list position <paramref name="row"/>. Port: the row is drawn by <see cref="DrawRow"/>; this
    /// makes the diagram the purchase dialog shows.
    /// </summary>
    void DrawBackground(int32_t row);

    /// <summary>
    /// Port: draws the row into <paramref name="port"/> (the store's view) with its top at <paramref name="top"/>:
    /// what the original's <c>drawBackground</c> painted into the store's picture.
    /// </summary>
    void DrawRow(MCLogPort* port, int32_t top);

    /// <summary>
    /// Port: draws the drag icon's picture into <paramref name="surface"/>: a 0x20 square of the row, over the
    /// store's background.
    /// </summary>
    void OnBeginDrag(MCLogPort* surface);

    /// <summary>Empty.</summary>
    void SetBar();

    int32_t Row = 0;
    MCPurVehicle* PurVehicle = nullptr;
    int32_t NameIndex = 0;
    MCLogPort* PicturePort = nullptr;
    char* WeightClassText = nullptr;
    char* ArmorText = nullptr;
};

/// <summary>A row of the component shop.</summary>
/// <remarks>Original source: <c>logistics\purchase.cpp</c>, 0x594 bytes.</remarks>
class MCCompPurchaseBlock : public MCLogObject
{
public:
    ~MCCompPurchaseBlock() override;

    /// <summary>Shows <paramref name="item"/>; builds its stat texts.</summary>
    void Init(MCLogInventoryItem* item);

    /// <summary>Nothing of its own (the inlined <see cref="MCLogObject::Destroy"/>).</summary>
    void Destroy() override;

    void HandleEvent(MCGuiEvent* event) override;

    /// <summary>
    /// Draws the row at list position <paramref name="row"/>. Port: the row is drawn by <see cref="DrawRow"/>; this
    /// readies the description.
    /// </summary>
    void DrawBackground(int32_t row, int32_t unused);

    /// <summary>
    /// Port: draws the row into <paramref name="port"/> (the store's view) with its top at <paramref name="top"/>:
    /// what the original's <c>drawBackground</c> painted into the store's picture.
    /// </summary>
    void DrawRow(MCLogPort* port, int32_t top);

    /// <summary>
    /// Port: draws the drag icon's picture into <paramref name="surface"/>: a 0x20 square of the row, over the
    /// store's background.
    /// </summary>
    void OnBeginDrag(MCLogPort* surface);

    int32_t Row = 0;
    /// <summary>The item's sort order (copied from <c>_LogInventoryItem::sortOrder</c> when the block is made).</summary>
    int32_t SortOrder = 0;
    char RangeText[51] = {};
    char WeightText[51] = {};
    /// <summary>MasterComponent +0x5c with a rating word, probably the recycle time.</summary>
    char RecycleText[51] = {};
    /// <summary>MasterComponent +0x58 with a rating word, probably the damage.</summary>
    char DamageText[51] = {};
    MCLogInventoryItem* Item = nullptr;
};

/// <summary>A row of the pilot hiring list.</summary>
/// <remarks>Original source: <c>logistics\purchase.cpp</c>, 0x4c4 bytes.</remarks>
class MCPilotPurchaseBlock : public MCLogObject
{
public:
    ~MCPilotPurchaseBlock() override;

    void Init(MCPurPilotData* pilot);

    void Destroy() override;

    void HandleEvent(MCGuiEvent* event) override;

    /// <summary>
    /// Draws the row at list position <paramref name="row"/>. Port: the row is drawn by <see cref="DrawRow"/>; this
    /// readies the description.
    /// </summary>
    void DrawBackground(int32_t row);

    /// <summary>
    /// Port: draws the row into <paramref name="port"/> (the store's view) with its top at <paramref name="top"/>:
    /// what the original's <c>drawBackground</c> painted into the store's picture.
    /// </summary>
    void DrawRow(MCLogPort* port, int32_t top);

    /// <summary>
    /// Port: draws the drag icon's picture into <paramref name="surface"/>: a 0x20 square of the row, over the
    /// store's background.
    /// </summary>
    void OnBeginDrag(MCLogPort* surface);

    int32_t Row = 0;
    MCPurPilotData* Pilot = nullptr;
};

/// <summary>Warns (dialog) when the player owns 50 units or more.</summary>
/// <returns>1 when full.</returns>
int CheckMaxUnits();

/// <summary>Warns (dialog) when the player owns 40 units or more.</summary>
void CheckNumUnits();

/// <summary>Buy-confirm callback of <paramref name="quantity"/> mechs of <see cref="GlobalMechPurchaseBlock"/>.</summary>
void MechPurchaseCallback(int confirmed, int32_t quantity);

/// <summary>Hire-confirm callback of the pilot of <see cref="GlobalPilotPurchaseBlock"/>.</summary>
void PilotPurchaseCallback(int confirmed, int32_t quantity);

/// <summary>Buy-confirm callback of <paramref name="quantity"/> components (<see cref="GlobalItemPtr"/>).</summary>
void CompPurchaseCallback(int confirmed, int32_t quantity);

/// <summary>Buy-confirm callback of the vehicle of <see cref="GlobalVehicleBlockPtr"/>.</summary>
void VehiclePurchaseCallback(int confirmed, int32_t quantity);

/// <summary>The player's resource points.</summary>
extern int32_t ResourcePoints;
/// <summary>The shop rows a buy dialog is open for.</summary>
extern MCMechPurchaseBlock* GlobalMechPurchaseBlock;
extern MCPilotPurchaseBlock* GlobalPilotPurchaseBlock;
extern MCLogInventoryItem* GlobalItemPtr;
extern MCVehiclePurchaseBlock* GlobalVehicleBlockPtr;
/// <summary>The order the 8 body-location shapes of the mech diagram are drawn in ({7, 6, 4, 5, 0, 1, 2, 3}).</summary>
extern int32_t BodyTrans[8];
/// <summary>The object description file under <c>objectPath</c> ("desc.fit"; its Desc&lt;n&gt; blocks hold the texts).</summary>
extern char ObjectDesc[];
