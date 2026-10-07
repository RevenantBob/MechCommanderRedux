#pragma once

#include "logistics/lport.h"

class MCGuiEvent;
class MCGuiFont;
class MCLogMech;
class MCLogVehicle;
class MCLogWarrior;
struct MCLogInventoryItem;

/// <summary>
/// The icon that follows the mouse while a mech, pilot, vehicle or component is dragged between the logistics
/// panes. The dragging block fills its port with the item's picture.
/// </summary>
/// <remarks>Original source: <c>logistics\invblock.cpp</c>, 0x4bc bytes (no fields of its own).</remarks>
class MCDragIcon : public MCLogObject
{
public:
    ~MCDragIcon() override = default;

    /// <summary>Copies the icon's port to the screen at its current position.</summary>
    void Display() override;

    /// <summary>
    /// Port: makes the icon at (<paramref name="xPos"/>, <paramref name="yPos"/>), <paramref name="width"/> x
    /// <paramref name="height"/>: its surface starts as colour 0 (the heap's fill), <paramref name="render"/> draws
    /// the dragged item into it once (the item's <c>OnBeginDrag</c>), and its edge is outlined in colour 0xea. The
    /// surface then stays as it is for the whole drag; <see cref="Display"/> only draws it.
    /// </summary>
    void Begin(int32_t xPos, int32_t yPos, int32_t width, int32_t height,
               const std::function<void(MCLogPort* surface)>& render);

    /// <summary>
    /// Port: calls <paramref name="draw"/> with <paramref name="surface"/> moved so that what it draws at
    /// (<paramref name="xPos"/>, <paramref name="yPos"/>) lands at the surface's (0, 0); the surface clips the rest.
    /// For an item that draws itself at its place in a list.
    /// </summary>
    static void DrawFrom(MCLogPort* surface, int32_t xPos, int32_t yPos,
                         const std::function<void(MCLogPort* port)>& draw);
};

/// <summary>
/// One row of an inventory list on the logistics inventory screen (a mech, pilot, vehicle or component the player
/// owns): a 0xad x 0x2b <see cref="MCLogObject"/> drawn into the inventory screen's port at row <see cref="ListIndex"/>.
/// </summary>
/// <remarks>Original source: <c>logistics\invblock.cpp</c>, 0x4c4 bytes.</remarks>
class MCInventoryBlock : public MCLogObject
{
public:
    ~MCInventoryBlock() override;

    /// <summary>Makes the row object at (<paramref name="xPos"/>, <paramref name="yPos"/>) in <paramref name="port"/>; enables it.</summary>
    void Init(int32_t xPos, int32_t yPos, MCLogPort* port);

    /// <summary>Nothing of its own: the base <see cref="MCLogObject::Destroy"/> does the work.</summary>
    void Destroy() override;

    /// <summary>Copies the blank row art (<c>Logistics</c> +0x10ec) into <paramref name="port"/> at this row.</summary>
    void DrawBackground(MCLogPort* port);

    /// <summary>Draws the row greyed out. Empty in the original.</summary>
    void DrawDisabled();

    /// <summary>Sets <see cref="Enabled"/> and redraws.</summary>
    void SetEnabled(int32_t enable);

    /// <summary>
    /// Port: draws the row into <paramref name="port"/> with its top at <paramref name="top"/>: what the original's
    /// <c>drawBackground</c> painted into the inventory tab's picture (the tab is now drawn from its rows each frame,
    /// and <c>drawBackground</c> keeps only what it did besides painting). The base draws the blank row art.
    /// </summary>
    virtual void DrawRow(MCLogPort* port, int32_t top);

    /// <summary>
    /// Port: draws the row's details into the info box under the inventory pane (see <c>InvInfoBox</c>); a
    /// component's are kept in the box itself.
    /// </summary>
    virtual void DrawInfo(MCLogPort* port) {}

    /// <summary>
    /// Port: draws the drag icon's picture into <paramref name="surface"/> (0x20 square): the row's square at (2, 1)
    /// over colour 0x10.
    /// </summary>
    void OnBeginDrag(MCLogPort* surface);

    // Fields are public: the inventory screen and the callbacks read and set them directly.
    /// <summary>Nonzero when the row is usable; zero makes <c>draw</c> call <see cref="DrawDisabled"/>.</summary>
    int32_t Enabled = 0;
    /// <summary>The row of the list this block is drawn in (its y is <c>listIndex * height</c>).</summary>
    int32_t ListIndex = 0;
};

/// <summary>An owned mech in the inventory list: name, tonnage and a small damage diagram.</summary>
/// <remarks>Original source: <c>logistics\invblock.cpp</c>, 0x4cc bytes.</remarks>
class MCMechInventoryBlock : public MCInventoryBlock
{
public:
    ~MCMechInventoryBlock() override;

    /// <summary>Shows <paramref name="logMech"/>; takes its list position from the mech.</summary>
    void Init(MCLogMech* logMech);

    /// <summary>Frees the diagram port.</summary>
    void Destroy() override;

    /// <summary>Only draws the disabled state (the row itself is drawn by <see cref="DrawBackground"/>).</summary>
    void Draw() override;

    /// <summary>
    /// Draws the row: mech name, tonnage/class line and the damage diagram (made here when missing). Port: the row is
    /// drawn by <see cref="DrawRow"/>; this makes the diagram.
    /// </summary>
    void DrawBackground();

    /// <summary>Port: see <see cref="MCInventoryBlock::DrawRow"/>.</summary>
    void DrawRow(MCLogPort* port, int32_t top) override;

    /// <summary>Port: the diagram, tonnage, classes, speed and description.</summary>
    void DrawInfo(MCLogPort* port) override;

    /// <summary>Frees the cached damage diagram so the next draw rebuilds it.</summary>
    void DeleteDiagram();

    /// <summary>Selection, drag to a drop slot / the sell area, and the info display.</summary>
    void HandleEvent(MCGuiEvent* event) override;

    // Fields are public: the inventory screen and the callbacks read and set them directly.
    /// <summary>The cached small damage diagram (0x1c x 0x1e).</summary>
    MCLogPort* DiagramPort = nullptr;
    MCLogMech* Mech = nullptr;
};

/// <summary>An owned MechWarrior in the inventory list: portrait, callsign, rank and skills.</summary>
/// <remarks>Original source: <c>logistics\invblock.cpp</c>, 0x4d8 bytes.</remarks>
class MCPilotInventoryBlock : public MCInventoryBlock
{
public:
    ~MCPilotInventoryBlock() override;

    /// <summary>Shows <paramref name="logWarrior"/>; takes the next free row and loads the portrait.</summary>
    void Init(MCLogWarrior* logWarrior);

    /// <summary>Frees the portrait port.</summary>
    void Destroy() override;

    /// <summary>Only draws the disabled state.</summary>
    void Draw() override;

    /// <summary>
    /// Draws portrait, callsign, rank and skill bars; darkens the row when <see cref="GreyedOut"/>. Port: the row is
    /// drawn by <see cref="DrawRow"/>; this sets <see cref="GreyedOut"/> and moves the block to its row.
    /// </summary>
    void DrawBackground();

    /// <summary>Port: see <see cref="MCInventoryBlock::DrawRow"/>.</summary>
    void DrawRow(MCLogPort* port, int32_t top) override;

    /// <summary>Port: the skills, portrait, rank, health and description.</summary>
    void DrawInfo(MCLogPort* port) override;

    /// <summary>Selection, drag onto a mech, and the info display.</summary>
    void HandleEvent(MCGuiEvent* event) override;

    // Fields are public: the inventory screen and the callbacks read and set them directly.
    MCLogWarrior* Warrior = nullptr;
    /// <summary>
    /// Set by <see cref="DrawBackground"/> on the repair screen when the selected mech can't take a pilot; blocks
    /// dragging.
    /// </summary>
    int32_t GreyedOut = 0;
    /// <summary>The pilot's portrait (<c>logart\&lt;n&gt;</c>).</summary>
    MCLogPort* PortraitPort = nullptr;
    /// <summary>The mech the pilot was last dropped on (repair screen); cleared by <see cref="Init"/>, never read.</summary>
    MCLogMech* Mech = nullptr;
    /// <summary>
    /// The vehicle the pilot was last dropped on (<c>VehicleRepairBlock::handleEvent</c>); cleared by
    /// <see cref="Init"/>, never read.
    /// </summary>
    MCLogVehicle* Vehicle = nullptr;
};

/// <summary>An owned vehicle in the inventory list.</summary>
/// <remarks>Original source: <c>logistics\invblock.cpp</c>, 0x4d4 bytes.</remarks>
class MCVehicleInventoryBlock : public MCInventoryBlock
{
public:
    ~MCVehicleInventoryBlock() override;

    /// <summary>Shows <paramref name="logVehicle"/>; builds its weight-class and armor texts.</summary>
    void Init(MCLogVehicle* logVehicle);

    /// <summary>Frees the texts.</summary>
    void Destroy() override;

    /// <summary>Empty (a bare <c>ret</c> the export has no function for).</summary>
    void Draw() override;

    /// <summary>Selection, drag and the info display.</summary>
    void HandleEvent(MCGuiEvent* event) override;

    /// <summary>
    /// Draws the row: name, class texts and picture (made here when missing). Port: the row is drawn by
    /// <see cref="DrawRow"/>; this makes the picture.
    /// </summary>
    void DrawBackground();

    /// <summary>Port: see <see cref="MCInventoryBlock::DrawRow"/>.</summary>
    void DrawRow(MCLogPort* port, int32_t top) override;

    /// <summary>Port: the picture, tonnage, classes, speed and description.</summary>
    void DrawInfo(MCLogPort* port) override;

    // Fields are public: the inventory screen and the callbacks read and set them directly.
    /// <summary>The cached vehicle picture (0x1c x 0x1e).</summary>
    MCLogPort* PicturePort = nullptr;
    MCLogVehicle* Vehicle = nullptr;
    /// <summary>Weight class (string table 0x4f..0x52 by tonnage), a logistics block.</summary>
    char* WeightClassText = nullptr;
    /// <summary>Armor rating (string table 0x4f/0x51/0x64..0x66 by armor tonnage), a logistics block.</summary>
    char* ArmorText = nullptr;
};

/// <summary>A stack of an owned component (weapon, ammo, equipment) in the inventory list.</summary>
/// <remarks>Original source: <c>logistics\invblock.cpp</c>, 0x508 bytes.</remarks>
class MCCompInventoryBlock : public MCInventoryBlock
{
public:
    ~MCCompInventoryBlock() override;

    /// <summary>Shows <paramref name="item"/>; builds its stat texts and icon.</summary>
    void Init(MCLogInventoryItem* item);

    /// <summary>Frees the icon and own ports.</summary>
    void Destroy() override;

    /// <summary>Selection, drag onto a mech / the sell area, and the info display.</summary>
    void HandleEvent(MCGuiEvent* event) override;

    /// <summary>
    /// Draws the row; darkens it when the item can't fit on the selected mech. Port: the row is drawn by
    /// <see cref="DrawRow"/>; this shows or hides the block and sets <see cref="CantMount"/>.
    /// </summary>
    void DrawBackground();

    /// <summary>Port: see <see cref="MCInventoryBlock::DrawRow"/>.</summary>
    void DrawRow(MCLogPort* port, int32_t top) override;

    /// <summary>Range text ("%.1f m", or a short/medium/long word, or a label for equipment).</summary>
    char RangeText[12] = {};
    /// <summary>Weight text ("%.1f tons").</summary>
    char WeightText[12] = {};
    /// <summary>"%.2f s": MasterComponent +0x5c, probably the recycle time.</summary>
    char RecycleText[12] = {};
    /// <summary>"%.2f": MasterComponent +0x58 (doubled for one weapon kind), probably the damage.</summary>
    char DamageText[12] = {};
    /// <summary>
    /// Nonzero when the item can't go on the mech selected on the repair screen (too heavy, equipment it already
    /// has, or a restricted component); the row is drawn darkened.
    /// </summary>
    int32_t CantMount = 0;
    MCLogInventoryItem* Item = nullptr;
    /// <summary>The component's icon.</summary>
    MCLogPort* IconPort = nullptr;
    /// <summary>Tonnage (plus its ammo's for weapons of kind 8/9).</summary>
    float Tonnage = 0.0f;
    /// <summary>The item's index in the owner's inventory list (set by whoever creates the block).</summary>
    int32_t InventoryIndex = 0;
};

/// <summary>Sell-confirm callback of a mech: moves its components to the inventory and removes it.</summary>
/// <param name="confirmed">Nonzero when the player confirmed.</param>
/// <param name="quantity">Unused for mechs.</param>
void MechSellCallback(int confirmed, int32_t quantity);

/// <summary>Sell-confirm callback of a pilot.</summary>
void PilotSellCallback(int confirmed, int32_t quantity);

/// <summary>Sell-confirm callback of <paramref name="quantity"/> components (<see cref="GlobalCompPtr"/>).</summary>
void CompSellCallback(int confirmed, int32_t quantity);

/// <summary>Sell-confirm callback of a vehicle (<see cref="GlobalVehicle"/>).</summary>
void VehicleSellCallback(int confirmed, int32_t quantity);

/// <summary>
/// Port: a description in the info box: formatted by the text formatter into a <paramref name="width"/> x
/// <paramref name="height"/> picture and drawn keyed at (<paramref name="xPos"/>, <paramref name="yPos"/>) of
/// <paramref name="port"/>; nothing without one.
/// </summary>
void DrawInfoDescription(MCLogPort* port, int32_t width, int32_t height, char* description, int32_t xPos, int32_t yPos);

/// <summary>
/// Port: readies a description for the info box as the original did when it showed it: its fourth character (a
/// colour code's digit) becomes '9'.
/// </summary>
void PrepareInfoDescription(char* description);

/// <summary>How many entries of <see cref="RestrictedComps"/> are used (5).</summary>
extern int32_t NumRestrictedComponents;
/// <summary>
/// Master component IDs ({15, 37, 38, 42, 43}) that a mech can only mount when its LogMech +0x1c is 5, 0xe or 0x10
/// (checked by <see cref="MCCompInventoryBlock::DrawBackground"/>).
/// </summary>
extern int32_t RestrictedComps[5];
/// <summary>The yellow and blue fonts of the inventory rows.</summary>
extern MCGuiFont* YellowDropFont;
extern MCGuiFont* BlueDropFont;
/// <summary>The mech a sell dialog is open for (<see cref="MechSellCallback"/>).</summary>
extern MCLogMech* GlobalMechPtr;
/// <summary>The component a sell dialog is open for (<see cref="CompSellCallback"/>).</summary>
extern MCLogInventoryItem* GlobalCompPtr;
/// <summary>The vehicle a sell dialog is open for (<see cref="VehicleSellCallback"/>).</summary>
extern MCLogVehicle* GlobalVehicle;
/// <summary>Nonzero in a single-player (solo) campaign.</summary>
extern int Solo;
/// <summary>
/// The damage colours of the mech/vehicle diagrams, per state (armor left: 0 under 76%, 1 under 51%, 2 under 26%, 3
/// armor and structure gone; above that the shape keeps its own colours): the colours
/// that replace the diagram shapes' pixels 0xea, 0xe8 and 0xe7 (the low byte of each entry).
/// </summary>
extern int32_t IconFade[4][3];
