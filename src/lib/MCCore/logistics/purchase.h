#pragma once

#include "logistics/lport.h"

class aEvent;
class InventoryList;
class MechPurchaseBlock;
class PilotPurchaseBlock;
class VehiclePurchaseBlock;
struct _LogInventoryItem;

/// <summary>The maximum and current armor of one body location of a mech for sale.</summary>
struct PurArmorLocation
{
    uint8_t maxArmor = 0; // +0x0
    uint8_t curArmor = 0; // +0x1
};

/// <summary>One critical slot of a body location of a mech for sale: the component and its damage.</summary>
struct PurCriticalSlot
{
    /// <summary>Master component ID, 0xff when empty.</summary>
    uint8_t masterId = 0; // +0x0
    /// <summary>Hits taken (passed to <c>InventoryList::hitItem</c>).</summary>
    uint8_t damage = 0; // +0x1
};

/// <summary>
/// One variant of a mech offered for sale, read from its profile (<c>&lt;profilePath&gt;&lt;name&gt;.fit</c>) by
/// <see cref="PurMechList::addMech(PurMech*, char*, int32_t)"/>.
/// </summary>
/// <remarks>Original source: <c>logistics\purchase.cpp</c>, 0x148 bytes (allocated on the logistics heap).</remarks>
class PurMechData
{
public:
    /// <summary>Loads description <paramref name="descIndex"/> of the object description file into <see cref="description"/>.</summary>
    /// <remarks>MCX.EXE @ 0x00726f90</remarks>
    void loadDescription(int32_t descIndex);

    /// <summary>Recomputes <see cref="battleRating"/> from <see cref="chassisBR"/> and the inventory.</summary>
    /// <remarks>MCX.EXE @ 0x00727140</remarks>
    int32_t calcBR();

    /// <summary>The profile's file name (without extension).</summary>
    char fileName[12] = {}; // +0x0
    /// <summary>The display name (string table entry <c>descIndex + 300</c>).</summary>
    char name[41] = {};      // +0xc
    float curTonnage = 0.0f; // +0x38
    int32_t nameIndex = 0;   // +0x3c
    /// <summary>Price in resource points: the file's ResourcePoints plus armor and components.</summary>
    int32_t cost = 0;        // +0x40
    uint8_t maxRunSpeed = 0; // +0x44
    /// <summary>The Armor block's Tonnage.</summary>
    float armorTonnage = 0.0f; // +0x48
    /// <summary>Head, CenterTorso, LeftTorso, RightTorso, LeftArm, RightArm, LeftLeg, RightLeg, then the three rear torsos.</summary>
    PurArmorLocation armor[11] = {}; // +0x4c
    uint8_t numOther = 0;            // +0x62
    uint8_t numWeapons = 0;          // +0x63
    uint8_t numAmmo = 0;             // +0x64
    /// <summary>Current internal structure of the 8 body locations.</summary>
    uint8_t curInternalStructure[8] = {}; // +0x65
    /// <summary>Critical slots of the 8 body locations (the count per location comes from a global table).</summary>
    PurCriticalSlot criticalSlots[8][12] = {}; // +0x6d
    /// <summary>How many of this variant the shop has.</summary>
    int32_t numAvailable = 0; // +0x130
    int32_t battleRating = 0; // +0x134
    /// <summary>The file's ChassisBR (100 when missing).</summary>
    int32_t chassisBR = 0; // +0x138
    /// <summary>The components the mech carries.</summary>
    InventoryList* inventory = nullptr; // +0x13c
    /// <summary>The description text, on the logistics heap.</summary>
    char* description = nullptr; // +0x140
    int32_t descIndex = -1;      // +0x144
};

/// <summary>A mech type for sale: up to three variants and the block that shows them.</summary>
/// <remarks>Original source: <c>logistics\purchase.cpp</c>, 0x14 bytes.</remarks>
class PurMech
{
public:
    PurMechData* variants[3] = {};      // +0x0
    MechPurchaseBlock* block = nullptr; // +0xc
    PurMech* next = nullptr;            // +0x10
};

/// <summary>The mechs of the shop, as a singly linked list (newest first).</summary>
/// <remarks>Original source: <c>logistics\purchase.cpp</c>, 8 bytes.</remarks>
class PurMechList
{
public:
    /// <remarks>MCX.EXE @ 0x0071faf0</remarks>
    PurMechList();

    /// <remarks>MCX.EXE @ 0x0071fb00</remarks>
    void init();

    /// <summary>Frees every mech, its variants and its block.</summary>
    /// <remarks>MCX.EXE @ 0x0071fb10</remarks>
    void destroy();

    /// <summary>Reads profile <paramref name="fileName"/> into variant <paramref name="variant"/> of <paramref name="purMech"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0071fc10</remarks>
    int32_t addMech(PurMech* purMech, char* fileName, int32_t variant);

    /// <summary>
    /// Adds a mech type with its three variants and how many of each are for sale, makes its block and shows
    /// the first variant with stock.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x007209e0</remarks>
    int32_t addMech(char* fileName0, int32_t count0, char* fileName2, int32_t count2, char* fileName1, int32_t count1);

    /// <summary>Adds to the stock of the mech type whose first variant is <paramref name="fileName"/> (never below 0).</summary>
    /// <returns>0, or -1 when there is none.</returns>
    /// <remarks>MCX.EXE @ 0x00720b40</remarks>
    int32_t modMech(char* fileName, int32_t delta0, int32_t delta2, int32_t delta1);

    /// <summary>Does nothing.</summary>
    /// <remarks>MCX.EXE @ 0x00720c40</remarks>
    int32_t removeMech(uint8_t index);

    /// <summary>The <paramref name="index"/>th mech.</summary>
    /// <returns>0, or -1 past the end.</returns>
    /// <remarks>MCX.EXE @ 0x00720c50</remarks>
    int32_t getMechInfo(int32_t index, PurMech*& purMech);

    /// <remarks>MCX.EXE @ 0x00720c80</remarks>
    int32_t getMechCount();

    PurMech* first = nullptr; // +0x0
    int32_t count = 0;        // +0x4
};

/// <summary>A MechWarrior for hire, read from <c>&lt;warriorPath&gt;&lt;name&gt;.fit</c>.</summary>
/// <remarks>Original source: <c>logistics\purchase.cpp</c>, 0x148 bytes.</remarks>
class PurPilotData
{
public:
    /// <summary>Sets <see cref="rank"/> from the weighted skills (<c>SkillWeightings</c>, <c>WarriorRankScale</c>).</summary>
    /// <remarks>MCX.EXE @ 0x007271a0</remarks>
    void calcRank();

    /// <summary>Loads description <paramref name="descIndex"/> into <see cref="description"/> (once).</summary>
    /// <remarks>MCX.EXE @ 0x00727250</remarks>
    void loadDescription(int32_t descIndex);

    char fileName[12] = {}; // +0x0
    char callsign[21] = {}; // +0xc
    /// <summary>The pilot's speech sample set (pilotAudio).</summary>
    char pilotAudio[255] = {}; // +0x21
    int32_t nameIndex = 0;     // +0x120
    char piloting = 0;         // +0x124
    char gunnery = 0;          // +0x125
    char jumping = 0;          // +0x126
    char sensors = 0;          // +0x127
    /// <summary>6 minus the file's Wounds.</summary>
    char health = 0;  // +0x128
    int32_t rank = 0; // +0x12c
    /// <summary>Hiring price: the logistics price of <see cref="rank"/>.</summary>
    int32_t cost = 0; // +0x130
    /// <summary>The DescIndex, also used as the pilot's ID (<see cref="PurPilotList::setPilotStatus"/>).</summary>
    int32_t descIndex = -1;      // +0x134
    char* description = nullptr; // +0x138
    /// <summary>0 = for hire (visible); 1 = hired; 3 = sold back.</summary>
    int32_t status = 0;                  // +0x13c
    PilotPurchaseBlock* block = nullptr; // +0x140
    PurPilotData* next = nullptr;        // +0x144
};

/// <summary>The pilots for hire, sorted by rank then callsign.</summary>
/// <remarks>Original source: <c>logistics\purchase.cpp</c>, 8 bytes. No constructor in the binary.</remarks>
class PurPilotList
{
public:
    /// <summary>Frees every pilot and its block.</summary>
    /// <remarks>MCX.EXE @ 0x00726960</remarks>
    void destroy();

    /// <summary>Reads pilot <paramref name="fileName"/>, makes its block, and inserts it in rank order.</summary>
    /// <remarks>MCX.EXE @ 0x007269d0</remarks>
    int32_t addPilot(char* fileName, int32_t status);

    /// <remarks>MCX.EXE @ 0x00726ea0</remarks>
    int32_t removePilot(int32_t index);

    /// <summary>Sets the <see cref="PurPilotData::status"/> of the pilot whose ID is <paramref name="pilotId"/>.</summary>
    /// <remarks>MCX.EXE @ 0x00726f00</remarks>
    void setPilotStatus(int32_t pilotId, int32_t status);

    /// <returns>0, or -1 past the end.</returns>
    /// <remarks>MCX.EXE @ 0x00726f40</remarks>
    int32_t getPilotInfo(int32_t index, PurPilotData*& pilot);

    /// <summary>How many pilots are still for hire (status 0).</summary>
    /// <remarks>MCX.EXE @ 0x00726f70</remarks>
    int32_t getVisiblePilotCount();

    PurPilotData* first = nullptr; // +0x0
    int32_t count = 0;             // +0x4
};

/// <summary>A vehicle type for sale, read from its profile.</summary>
/// <remarks>Original source: <c>logistics\purchase.cpp</c>, 0x3c bytes.</remarks>
class PurVehicleData
{
public:
    /// <summary>Loads description <paramref name="descIndex"/> into <see cref="description"/> (once).</summary>
    /// <remarks>MCX.EXE @ 0x00727410</remarks>
    void loadDescription(int32_t descIndex);

    char fileName[12] = {}; // +0x0
    /// <summary>The display name (string table entry <c>descIndex + 700</c>), on the logistics heap.</summary>
    char* name = nullptr;    // +0xc
    float curTonnage = 0.0f; // +0x10
    int32_t nameIndex = 0;   // +0x14
    /// <summary>The file's ResourcePoints (100 when missing).</summary>
    int32_t baseCost = 0; // +0x18
    /// <summary><see cref="baseCost"/> plus the components (<see cref="PurVehicle::calcVehicleCost"/>).</summary>
    int32_t cost = 0;            // +0x1c
    int32_t descIndex = -1;      // +0x20
    char* description = nullptr; // +0x24
    uint8_t maxMoveSpeed = 0;    // +0x28
    /// <summary>The Armor block's Tonnage.</summary>
    float armorTonnage = 0.0f;          // +0x2c
    uint8_t numOther = 0;               // +0x30
    uint8_t numWeapons = 0;             // +0x31
    uint8_t numAmmo = 0;                // +0x32
    int32_t numAvailable = 0;           // +0x34
    InventoryList* inventory = nullptr; // +0x38
};

/// <summary>A vehicle type for sale and its block.</summary>
/// <remarks>Original source: <c>logistics\purchase.cpp</c>, 0xc bytes.</remarks>
class PurVehicle
{
public:
    /// <summary>Sets <see cref="PurVehicleData::cost"/> from the base cost and the components.</summary>
    /// <remarks>MCX.EXE @ 0x007273d0</remarks>
    void calcVehicleCost();

    PurVehicleData* data = nullptr;        // +0x0
    VehiclePurchaseBlock* block = nullptr; // +0x4
    PurVehicle* next = nullptr;            // +0x8
};

/// <summary>The vehicles of the shop, sorted by tonnage.</summary>
/// <remarks>Original source: <c>logistics\purchase.cpp</c>, 8 bytes.</remarks>
class PurVehicleList
{
public:
    /// <remarks>MCX.EXE @ 0x00722d60</remarks>
    PurVehicleList();

    /// <remarks>MCX.EXE @ 0x00722d70</remarks>
    void init();

    /// <summary>Frees every vehicle, its data and its block.</summary>
    /// <remarks>MCX.EXE @ 0x00722d80</remarks>
    void destroy();

    /// <summary>Adds <paramref name="delta"/> to the stock of vehicle <paramref name="fileName"/> (never below 0).</summary>
    /// <returns>0, or -1 when there is none.</returns>
    /// <remarks>MCX.EXE @ 0x00722e60 (FUN_00722e60: unnamed in the binary; named after PurMechList::modMech)</remarks>
    int32_t modVehicle(char* fileName, int32_t delta);

    /// <summary>
    /// Reads vehicle profile <paramref name="fileName"/> with <paramref name="count"/> for sale, makes its block
    /// and inserts it in tonnage order.
    /// </summary>
    /// <remarks>
    /// MCX.EXE @ 0x00722f00 (FUN_00722f00: unnamed in the binary; named after PurMechList::addMech). When the file
    /// has no General block the original falls back to reading raw 0xd0-byte records into the 0xc-byte
    /// <see cref="PurVehicle"/>, which overruns it.
    /// </remarks>
    int32_t addVehicle(char* fileName, int32_t count);

    /// <summary>Does nothing.</summary>
    /// <remarks>MCX.EXE @ 0x007237a0</remarks>
    int32_t removeVehicle(uint8_t index);

    /// <returns>0, or -1 past the end.</returns>
    /// <remarks>MCX.EXE @ 0x007237b0</remarks>
    int32_t getVehicleInfo(int32_t index, PurVehicle*& vehicle);

    /// <remarks>MCX.EXE @ 0x007237e0</remarks>
    int32_t getVehicleCount();

    PurVehicle* first = nullptr; // +0x0
    int32_t count = 0;           // +0x4
};

/// <summary>A row of the mech shop (0x19a x 0x70): picture, diagram, class texts, variants and price.</summary>
/// <remarks>Original source: <c>logistics\purchase.cpp</c>, 0x4e0 bytes.</remarks>
class MechPurchaseBlock : public lObject
{
public:
    /// <remarks>MCX.EXE @ 0x00720b00 (vector deleting destructor)</remarks>
    ~MechPurchaseBlock() override;

    /// <summary>Shows <paramref name="purMech"/> (variant <see cref="curVariant"/>, set by the caller first).</summary>
    /// <remarks>MCX.EXE @ 0x00720c90</remarks>
    void init(PurMech* purMech);

    /// <summary>Frees the ports and texts.</summary>
    /// <remarks>MCX.EXE @ 0x00720d00</remarks>
    void destroy() override;

    /// <summary>Variant buttons, buy (drag to the inventory), and the info display.</summary>
    /// <remarks>MCX.EXE @ 0x00720dd0</remarks>
    void handleEvent(aEvent* event) override;

    /// <summary>Empty: the row is drawn by <see cref="drawBackground"/>.</summary>
    /// <remarks>MCX.EXE @ 0x00721800</remarks>
    void draw() override;

    /// <summary>Draws the row at list position <paramref name="row"/>.</summary>
    /// <remarks>MCX.EXE @ 0x00721810</remarks>
    void drawBackground(int32_t row);

    /// <summary>Empty.</summary>
    /// <remarks>MCX.EXE @ 0x00722d50</remarks>
    void setBar();

    /// <summary>The list position the row is drawn at.</summary>
    int32_t row = 0;            // +0x4bc
    PurMech* purMech = nullptr; // +0x4c0
    /// <summary>The shown variant's name index.</summary>
    int32_t nameIndex = 0; // +0x4c4
    /// <summary>The variant shown (0..2), -1 before <see cref="PurMechList::addMech"/> picks one.</summary>
    int32_t curVariant = -1; // +0x4c8
    /// <summary>The mech's picture (0x4b x 100), built on first draw.</summary>
    lPort* picturePort = nullptr; // +0x4cc
    /// <summary>The small body diagram (0x1e x 0x1e), built on first draw.</summary>
    lPort* diagramPort = nullptr; // +0x4d0
    /// <summary>Weight class by current tonnage, on the logistics heap.</summary>
    char* weightClassText = nullptr; // +0x4d4
    /// <summary>Armor rating by armor tonnage.</summary>
    char* armorText = nullptr; // +0x4d8
    /// <summary>Internal structure rating by the total internal structure.</summary>
    char* internalText = nullptr; // +0x4dc
};

/// <summary>A row of the vehicle shop.</summary>
/// <remarks>Original source: <c>logistics\purchase.cpp</c>, 0x4d4 bytes.</remarks>
class VehiclePurchaseBlock : public lObject
{
public:
    /// <remarks>MCX.EXE @ 0x00723760 (vector deleting destructor)</remarks>
    ~VehiclePurchaseBlock() override;

    /// <summary>Shows <paramref name="purVehicle"/>; builds its class texts.</summary>
    /// <remarks>MCX.EXE @ 0x007237f0</remarks>
    void init(PurVehicle* purVehicle);

    /// <remarks>MCX.EXE @ 0x00723a10</remarks>
    void destroy() override;

    /// <remarks>MCX.EXE @ 0x00723aa0</remarks>
    void handleEvent(aEvent* event) override;

    /// <summary>Draws the row at list position <paramref name="row"/>.</summary>
    /// <remarks>MCX.EXE @ 0x00724270</remarks>
    void drawBackground(int32_t row);

    /// <summary>Empty.</summary>
    /// <remarks>MCX.EXE @ 0x00724bc0</remarks>
    void setBar();

    int32_t row = 0;                  // +0x4bc
    PurVehicle* purVehicle = nullptr; // +0x4c0
    int32_t nameIndex = 0;            // +0x4c4
    lPort* picturePort = nullptr;     // +0x4c8
    char* weightClassText = nullptr;  // +0x4cc
    char* armorText = nullptr;        // +0x4d0
};

/// <summary>A row of the component shop.</summary>
/// <remarks>Original source: <c>logistics\purchase.cpp</c>, 0x594 bytes.</remarks>
class CompPurchaseBlock : public lObject
{
public:
    /// <remarks>MCX.EXE @ 0x006e8060 (vector deleting destructor)</remarks>
    ~CompPurchaseBlock() override;

    /// <summary>Shows <paramref name="item"/>; builds its stat texts.</summary>
    /// <remarks>MCX.EXE @ 0x00724bd0</remarks>
    void init(_LogInventoryItem* item);

    /// <summary>Nothing of its own (the inlined <see cref="lObject::destroy"/>).</summary>
    /// <remarks>MCX.EXE @ 0x00725040</remarks>
    void destroy() override;

    /// <remarks>MCX.EXE @ 0x00725050</remarks>
    void handleEvent(aEvent* event) override;

    /// <summary>Draws the row at list position <paramref name="row"/>.</summary>
    /// <remarks>MCX.EXE @ 0x00725860</remarks>
    void drawBackground(int32_t row, int32_t unused);

    int32_t row = 0; // +0x4bc
    /// <summary>The item's sort order (copied from <c>_LogInventoryItem::sortOrder</c> when the block is made).</summary>
    int32_t sortOrder = 0;    // +0x4c0
    char rangeText[51] = {};  // +0x4c4
    char weightText[51] = {}; // +0x4f7
    /// <summary>MasterComponent +0x5c with a rating word, probably the recycle time.</summary>
    char recycleText[51] = {}; // +0x52a
    /// <summary>MasterComponent +0x58 with a rating word, probably the damage.</summary>
    char damageText[51] = {};          // +0x55d
    _LogInventoryItem* item = nullptr; // +0x590
};

/// <summary>A row of the pilot hiring list.</summary>
/// <remarks>Original source: <c>logistics\purchase.cpp</c>, 0x4c4 bytes.</remarks>
class PilotPurchaseBlock : public lObject
{
public:
    /// <remarks>MCX.EXE @ 0x00726e60 (vector deleting destructor)</remarks>
    ~PilotPurchaseBlock() override;

    /// <remarks>MCX.EXE @ 0x00725cd0</remarks>
    void init(PurPilotData* pilot);

    /// <remarks>MCX.EXE @ 0x00725d10</remarks>
    void destroy() override;

    /// <remarks>MCX.EXE @ 0x00725d20</remarks>
    void handleEvent(aEvent* event) override;

    /// <summary>Draws the row at list position <paramref name="row"/>.</summary>
    /// <remarks>MCX.EXE @ 0x00726510</remarks>
    void drawBackground(int32_t row);

    int32_t row = 0;               // +0x4bc
    PurPilotData* pilot = nullptr; // +0x4c0
};

/// <summary>Warns (dialog) when the player owns 50 units or more.</summary>
/// <returns>1 when full.</returns>
/// <remarks>MCX.EXE @ 0x0071f3f0</remarks>
int checkMaxUnits();

/// <summary>Warns (dialog) when the player owns 40 units or more.</summary>
/// <remarks>MCX.EXE @ 0x0071f520</remarks>
void checkNumUnits();

/// <summary>Buy-confirm callback of <paramref name="quantity"/> mechs of <see cref="globalMechPurchaseBlock"/>.</summary>
/// <remarks>MCX.EXE @ 0x0071f670</remarks>
void MechPurchaseCallback(int confirmed, int32_t quantity);

/// <summary>Hire-confirm callback of the pilot of <see cref="globalPilotPurchaseBlock"/>.</summary>
/// <remarks>MCX.EXE @ 0x0071f760</remarks>
void PilotPurchaseCallback(int confirmed, int32_t quantity);

/// <summary>Buy-confirm callback of <paramref name="quantity"/> components (<see cref="globalItemPtr"/>).</summary>
/// <remarks>MCX.EXE @ 0x0071f8a0</remarks>
void CompPurchaseCallback(int confirmed, int32_t quantity);

/// <summary>Buy-confirm callback of the vehicle of <see cref="globalVehicleBlockPtr"/>.</summary>
/// <remarks>MCX.EXE @ 0x0071fa30</remarks>
void VehiclePurchaseCallback(int confirmed, int32_t quantity);

/// <summary>The player's resource points.</summary>
extern int32_t ResourcePoints;
/// <summary>The shop rows a buy dialog is open for.</summary>
extern MechPurchaseBlock* globalMechPurchaseBlock;
extern PilotPurchaseBlock* globalPilotPurchaseBlock;
extern _LogInventoryItem* globalItemPtr;
extern VehiclePurchaseBlock* globalVehicleBlockPtr;
/// <summary>The order the 8 body-location shapes of the mech diagram are drawn in ({7, 6, 4, 5, 0, 1, 2, 3}).</summary>
extern int32_t bodyTrans[8];
/// <summary>The object description file under <c>objectPath</c> ("desc.fit"; its Desc&lt;n&gt; blocks hold the texts).</summary>
extern char objectDesc[];
