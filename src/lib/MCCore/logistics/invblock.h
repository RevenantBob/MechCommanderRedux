#pragma once

#include "logistics/lport.h"

class aEvent;
class aFont;
class LogMech;
class LogVehicle;
class LogWarrior;
struct _LogInventoryItem;

/// <summary>
/// The icon that follows the mouse while a mech, pilot, vehicle or component is dragged between the logistics
/// panes. The dragging block fills its port with the item's picture.
/// </summary>
/// <remarks>Original source: <c>logistics\invblock.cpp</c>, 0x4bc bytes (no fields of its own).</remarks>
class DragIcon : public lObject
{
public:
    /// <remarks>MCX.EXE @ 0x006d5f30 (vector deleting destructor)</remarks>
    ~DragIcon() override = default;

    /// <summary>Copies the icon's port to the screen at its current position.</summary>
    /// <remarks>MCX.EXE @ 0x006fc7d0</remarks>
    void display() override;
};

/// <summary>
/// One row of an inventory list on the logistics inventory screen (a mech, pilot, vehicle or component the player
/// owns): a 0xad x 0x2b <see cref="lObject"/> drawn into the inventory screen's port at row <see cref="listIndex"/>.
/// </summary>
/// <remarks>Original source: <c>logistics\invblock.cpp</c>, 0x4c4 bytes.</remarks>
class InventoryBlock : public lObject
{
public:
    /// <remarks>MCX.EXE @ 0x006d4400 (vector deleting destructor)</remarks>
    ~InventoryBlock() override;

    /// <summary>Makes the row object at (<paramref name="xPos"/>, <paramref name="yPos"/>) in <paramref name="port"/>; enables it.</summary>
    /// <remarks>MCX.EXE @ 0x006d4660</remarks>
    void init(int32_t xPos, int32_t yPos, lPort* port);

    /// <summary>Nothing of its own: the base <see cref="lObject::destroy"/> does the work.</summary>
    /// <remarks>MCX.EXE @ 0x006d46a0</remarks>
    void destroy() override;

    /// <summary>Copies the blank row art (<c>Logistics</c> +0x10ec) into <paramref name="port"/> at this row.</summary>
    /// <remarks>MCX.EXE @ 0x006d46b0</remarks>
    void drawBackground(lPort* port);

    /// <summary>Draws the row greyed out. Empty in the original.</summary>
    /// <remarks>MCX.EXE @ 0x006d46f0</remarks>
    void drawDisabled();

    /// <summary>Sets <see cref="enabled"/> and redraws.</summary>
    /// <remarks>
    /// MCX.EXE @ 0x006d4700 (FUN_006d4700: unnamed in the binary; the name is the port's). Probably an inline
    /// setter from invblock.h.
    /// </remarks>
    void setEnabled(int32_t enable);

    // Fields are public: the inventory screen and the callbacks read and set them directly.
    /// <summary>Nonzero when the row is usable; zero makes <c>draw</c> call <see cref="drawDisabled"/>.</summary>
    int32_t enabled = 0; // +0x4bc
    /// <summary>The row of the list this block is drawn in (its y is <c>listIndex * height</c>).</summary>
    int32_t listIndex = 0; // +0x4c0
};

/// <summary>An owned mech in the inventory list: name, tonnage and a small damage diagram.</summary>
/// <remarks>Original source: <c>logistics\invblock.cpp</c>, 0x4cc bytes.</remarks>
class MechInventoryBlock : public InventoryBlock
{
public:
    /// <remarks>MCX.EXE @ 0x006ed410 (vector deleting destructor)</remarks>
    ~MechInventoryBlock() override;

    /// <summary>Shows <paramref name="logMech"/>; takes its list position from the mech.</summary>
    /// <remarks>MCX.EXE @ 0x006d4720</remarks>
    void init(LogMech* logMech);

    /// <summary>Frees the diagram port.</summary>
    /// <remarks>MCX.EXE @ 0x006d4770</remarks>
    void destroy() override;

    /// <summary>Only draws the disabled state (the row itself is drawn by <see cref="drawBackground"/>).</summary>
    /// <remarks>MCX.EXE @ 0x006d47c0</remarks>
    void draw() override;

    /// <summary>Draws the row: mech name, tonnage/class line and the damage diagram.</summary>
    /// <remarks>MCX.EXE @ 0x006d47d0</remarks>
    void drawBackground();

    /// <summary>Frees the cached damage diagram so the next draw rebuilds it.</summary>
    /// <remarks>MCX.EXE @ 0x006d49a0</remarks>
    void deleteDiagram();

    /// <summary>Selection, drag to a drop slot / the sell area, and the info display.</summary>
    /// <remarks>MCX.EXE @ 0x006d4ba0</remarks>
    void handleEvent(aEvent* event) override;

    // Fields are public: the inventory screen and the callbacks read and set them directly.
    /// <summary>The cached small damage diagram (0x1c x 0x1e).</summary>
    lPort* diagramPort = nullptr; // +0x4c4
    LogMech* mech = nullptr;      // +0x4c8
};

/// <summary>An owned MechWarrior in the inventory list: portrait, callsign, rank and skills.</summary>
/// <remarks>Original source: <c>logistics\invblock.cpp</c>, 0x4d8 bytes.</remarks>
class PilotInventoryBlock : public InventoryBlock
{
public:
    /// <remarks>MCX.EXE @ 0x006ea450 (vector deleting destructor)</remarks>
    ~PilotInventoryBlock() override;

    /// <summary>Shows <paramref name="logWarrior"/>; takes the next free row and loads the portrait.</summary>
    /// <remarks>MCX.EXE @ 0x006d5f60</remarks>
    void init(LogWarrior* logWarrior);

    /// <summary>Frees the portrait port.</summary>
    /// <remarks>MCX.EXE @ 0x006d6020</remarks>
    void destroy() override;

    /// <summary>Only draws the disabled state.</summary>
    /// <remarks>MCX.EXE @ 0x006d6070</remarks>
    void draw() override;

    /// <summary>Draws portrait, callsign, rank and skill bars; darkens the row when <see cref="greyedOut"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006d6080</remarks>
    void drawBackground();

    /// <summary>Selection, drag onto a mech, and the info display.</summary>
    /// <remarks>MCX.EXE @ 0x006d6340</remarks>
    void handleEvent(aEvent* event) override;

    // Fields are public: the inventory screen and the callbacks read and set them directly.
    LogWarrior* warrior = nullptr; // +0x4c4
    /// <summary>
    /// Set by <see cref="drawBackground"/> on the repair screen when the selected mech can't take a pilot; blocks
    /// dragging.
    /// </summary>
    int32_t greyedOut = 0; // +0x4c8
    /// <summary>The pilot's portrait (<c>logart\&lt;n&gt;</c>).</summary>
    lPort* portraitPort = nullptr; // +0x4cc
    /// <summary>The mech the pilot was last dropped on (repair screen); cleared by <see cref="init"/>, never read.</summary>
    LogMech* mech = nullptr; // +0x4d0
    /// <summary>
    /// The vehicle the pilot was last dropped on (<c>VehicleRepairBlock::handleEvent</c>); cleared by
    /// <see cref="init"/>, never read.
    /// </summary>
    LogVehicle* vehicle = nullptr; // +0x4d4
};

/// <summary>An owned vehicle in the inventory list.</summary>
/// <remarks>Original source: <c>logistics\invblock.cpp</c>, 0x4d4 bytes.</remarks>
class VehicleInventoryBlock : public InventoryBlock
{
public:
    /// <remarks>MCX.EXE @ 0x006fc330 (vector deleting destructor)</remarks>
    ~VehicleInventoryBlock() override;

    /// <summary>Shows <paramref name="logVehicle"/>; builds its weight-class and armor texts.</summary>
    /// <remarks>MCX.EXE @ 0x006d7410</remarks>
    void init(LogVehicle* logVehicle);

    /// <summary>Frees the texts.</summary>
    /// <remarks>MCX.EXE @ 0x006d7620</remarks>
    void destroy() override;

    /// <summary>Empty (a bare <c>ret</c> the export has no function for).</summary>
    /// <remarks>MCX.EXE @ 0x006d85a0</remarks>
    void draw() override;

    /// <summary>Selection, drag and the info display.</summary>
    /// <remarks>MCX.EXE @ 0x006d7680</remarks>
    void handleEvent(aEvent* event) override;

    /// <summary>Draws the row: name, class texts and picture.</summary>
    /// <remarks>MCX.EXE @ 0x006d85b0</remarks>
    void drawBackground();

    // Fields are public: the inventory screen and the callbacks read and set them directly.
    /// <summary>The cached vehicle picture (0x1c x 0x1e).</summary>
    lPort* picturePort = nullptr;  // +0x4c4
    LogVehicle* vehicle = nullptr; // +0x4c8
    /// <summary>Weight class (string table 0x4f..0x52 by tonnage), on the logistics heap.</summary>
    char* weightClassText = nullptr; // +0x4cc
    /// <summary>Armor rating (string table 0x4f/0x51/0x64..0x66 by armor tonnage), on the logistics heap.</summary>
    char* armorText = nullptr; // +0x4d0
};

/// <summary>A stack of an owned component (weapon, ammo, equipment) in the inventory list.</summary>
/// <remarks>Original source: <c>logistics\invblock.cpp</c>, 0x508 bytes.</remarks>
class CompInventoryBlock : public InventoryBlock
{
public:
    /// <remarks>MCX.EXE @ 0x006d4380 (vector deleting destructor)</remarks>
    ~CompInventoryBlock() override;

    /// <summary>Shows <paramref name="item"/>; builds its stat texts and icon.</summary>
    /// <remarks>MCX.EXE @ 0x006d8920</remarks>
    void init(_LogInventoryItem* item);

    /// <summary>Frees the icon and own ports.</summary>
    /// <remarks>MCX.EXE @ 0x006d8f30</remarks>
    void destroy() override;

    /// <summary>Selection, drag onto a mech / the sell area, and the info display.</summary>
    /// <remarks>MCX.EXE @ 0x006d8fb0</remarks>
    void handleEvent(aEvent* event) override;

    /// <summary>Draws the row; darkens it when the item can't fit on the selected mech.</summary>
    /// <remarks>MCX.EXE @ 0x006da2c0</remarks>
    void drawBackground();

    /// <summary>Range text ("%.1f m", or a short/medium/long word, or a label for equipment).</summary>
    char rangeText[12] = {}; // +0x4c4
    /// <summary>Weight text ("%.1f tons").</summary>
    char weightText[12] = {}; // +0x4d0
    /// <summary>"%.2f s": MasterComponent +0x5c, probably the recycle time.</summary>
    char recycleText[12] = {}; // +0x4dc
    /// <summary>"%.2f": MasterComponent +0x58 (doubled for one weapon kind), probably the damage.</summary>
    char damageText[12] = {}; // +0x4e8
    /// <summary>
    /// Nonzero when the item can't go on the mech selected on the repair screen (too heavy, equipment it already
    /// has, or a restricted component); the row is drawn darkened.
    /// </summary>
    int32_t cantMount = 0;             // +0x4f4
    _LogInventoryItem* item = nullptr; // +0x4f8
    /// <summary>The component's icon.</summary>
    lPort* iconPort = nullptr; // +0x4fc
    /// <summary>Tonnage (plus its ammo's for weapons of kind 8/9).</summary>
    float tonnage = 0.0f; // +0x500
    /// <summary>The item's index in the owner's inventory list (set by whoever creates the block).</summary>
    int32_t inventoryIndex = 0; // +0x504
};

/// <summary>Sell-confirm callback of a mech: moves its components to the inventory and removes it.</summary>
/// <param name="confirmed">Nonzero when the player confirmed.</param>
/// <param name="quantity">Unused for mechs.</param>
/// <remarks>MCX.EXE @ 0x006d4150</remarks>
void MechSellCallback(int confirmed, int32_t quantity);

/// <summary>Sell-confirm callback of a pilot.</summary>
/// <remarks>MCX.EXE @ 0x006d4440</remarks>
void PilotSellCallback(int confirmed, int32_t quantity);

/// <summary>Sell-confirm callback of <paramref name="quantity"/> components (<see cref="globalCompPtr"/>).</summary>
/// <remarks>MCX.EXE @ 0x006d4550</remarks>
void CompSellCallback(int confirmed, int32_t quantity);

/// <summary>Sell-confirm callback of a vehicle (<see cref="globalVehicle"/>).</summary>
/// <remarks>MCX.EXE @ 0x006d45c0</remarks>
void VehicleSellCallback(int confirmed, int32_t quantity);

/// <summary>How many entries of <see cref="restrictedComps"/> are used (5).</summary>
extern int32_t numRestrictedComponents;
/// <summary>
/// Master component IDs ({15, 37, 38, 42, 43}) that a mech can only mount when its LogMech +0x1c is 5, 0xe or 0x10
/// (checked by <see cref="CompInventoryBlock::drawBackground"/>).
/// </summary>
extern int32_t restrictedComps[5];
/// <summary>The yellow and blue fonts of the inventory rows.</summary>
extern aFont* yellowDropFont;
extern aFont* blueDropFont;
/// <summary>The mech a sell dialog is open for (<see cref="MechSellCallback"/>).</summary>
extern LogMech* globalMechPtr;
/// <summary>The component a sell dialog is open for (<see cref="CompSellCallback"/>).</summary>
extern _LogInventoryItem* globalCompPtr;
/// <summary>The vehicle a sell dialog is open for (<see cref="VehicleSellCallback"/>).</summary>
extern LogVehicle* globalVehicle;
/// <summary>Nonzero in a single-player (solo) campaign.</summary>
extern int Solo;
/// <summary>
/// The damage colours of the mech/vehicle diagrams, per state (armor left: 0 under 76%, 1 under 51%, 2 under 26%, 3
/// armor and structure gone; above that the shape keeps its own colours): the colours
/// that replace the diagram shapes' pixels 0xea, 0xe8 and 0xe7 (the low byte of each entry).
/// </summary>
extern int32_t iconFade[4][3];
