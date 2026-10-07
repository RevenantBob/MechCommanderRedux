#pragma once

#include "gui/scrlpane.h"
#include "logistics/lport.h"

class MCGuiEvent;
class MCGuiFont;
class MCLogMech;
class MCLogVehicle;
struct MCLogInventoryItem;
struct MCLogInventoryStat;

/// <summary>
/// A mech's panel on the repair (refit) screen (0x19a x 0x70): damage diagram, battle rating, pilot, the three
/// repair sliders (armor, internal structure, engine) and the weapon lists in a scroll pane.
/// </summary>
/// <remarks>Original source: <c>logistics\mrblock.cpp</c>, 0x584 bytes.</remarks>
class MCMechRepairBlock : public MCLogObject
{
public:
    ~MCMechRepairBlock() override;

    /// <summary>
    /// Shows <paramref name="logMech"/>: makes the inventory scroll pane and slider art, and records the mech's
    /// armor and internal structure as the sliders' starting points.
    /// </summary>
    void Init(MCLogMech* logMech);

    /// <summary>Frees the scroll pane, the slider art and the weapon lists.</summary>
    void Destroy() override;

    /// <summary>
    /// Port: draws the mech's details into the repair screen's info box (see <c>InvInfoBox</c>): its diagram,
    /// tonnage, classes, speed and description.
    /// </summary>
    void DrawInfo(MCLogPort* port);

    /// <summary>Takes the mech out of its drop slot.</summary>
    void UndeployMech();

    /// <summary>Sliders, repair buttons, pilot and item drag and drop.</summary>
    void HandleEvent(MCGuiEvent* event) override;

    /// <summary>
    /// Draws the panel. <paramref name="row"/> &lt; 0 draws into <paramref name="port"/> (the briefing box);
    /// otherwise into the repair screen's scroll pane at that row. Port: the pane's rows are drawn each frame by
    /// <see cref="DrawRow"/>; for them this ends a pilot drag or a held repair button and does the rest of what the
    /// paint did (the battle rating, the weapon lists).
    /// </summary>
    void DrawBackground(int32_t row, MCLogPort* port);

    /// <summary>
    /// Port: draws the row into <paramref name="port"/> (the repair screen's unit pane) with its top at
    /// <paramref name="top"/>, from the state: framed while selected, darkened while another mech is.
    /// </summary>
    void DrawRow(MCLogPort* port, int32_t top);

    /// <summary>
    /// Port: draws the picked-up pilot's drag icon into <paramref name="surface"/> (0x20 square): the row's square at
    /// (5, 0x25), the portrait, over the unit list's colour 0xff. Before <see cref="ClearPilot"/>.
    /// </summary>
    void OnBeginDragPilot(MCLogPort* surface);

    /// <summary>
    /// Port: draws the picked-up mech's drag icon into <paramref name="surface"/> (0x20 square): its body diagram at
    /// (2, 1) over colour 0x10.
    /// </summary>
    void OnBeginDragMech(MCLogPort* surface);

    /// <summary>
    /// Port: draws the drag icon of <paramref name="item"/>, picked out of the weapon list, into
    /// <paramref name="surface"/> (0x20 square): its picture (<c>lscicc</c>) at (1, 1).
    /// </summary>
    void OnBeginDragItem(MCLogPort* surface, MCLogInventoryItem* item);

    /// <summary>Draws the repair buttons, enabled when there is something to repair.</summary>
    void DrawButtons(MCLogPort* port);

    void DrawDamageDiagram(MCLogPort* port);

    /// <summary>Draws the battle rating.</summary>
    void DrawBR(MCLogPort* port);

    /// <summary><see cref="DrawSlider"/> 0.</summary>
    void DrawArmorSlider(MCLogPort* port);

    /// <summary>Draws slider <paramref name="slider"/> (0 armor, 1 internal structure, 2 engine).</summary>
    void DrawSlider(MCLogPort* port, int32_t slider);

    /// <summary><see cref="DrawSlider"/> 1.</summary>
    void DrawInternalSlider(MCLogPort* port);

    /// <summary><see cref="DrawSlider"/> 2.</summary>
    void DrawEngineSlider(MCLogPort* port);

    void Draw() override;

    void DrawInventory(MCLogPort* port);

    /// <summary>
    /// Port-only: over the weapon list the mouse wheel scrolls it, as its arrows do. The list's pane is hidden (the
    /// block draws it), so the wheel finds the block, which would otherwise pass it on to the unit list.
    /// </summary>
    bool MouseWheel(int32_t steps, int32_t xPos, int32_t yPos) override;

    /// <summary>
    /// Draws the mech's status bar (colour by <c>LogMech::calcStatus</c>) into <paramref name="port"/>, or into the
    /// repair scroll pane at <see cref="SlotIndex"/> when it is null.
    /// </summary>
    void DrawStatusBar(MCLogPort* port = nullptr);

    /// <summary>With <paramref name="value"/> &lt; 0, puts the engine slider at the engine's damage level.</summary>
    void SetEngineSlider(int32_t value);

    /// <summary>With <paramref name="value"/> &lt; 0, puts the internal-structure slider at the current total.</summary>
    void SetInternalSlider(int32_t value);

    /// <summary>With <paramref name="value"/> &lt; 0, puts the armor slider at the current total.</summary>
    void SetArmorSlider(int32_t value);

    void ClearPilot();

    void SetPilotStats(MCLogPort* port);

    void SetPilotHealth(MCLogPort* port);

    /// <summary>Empty.</summary>
    void SetMechStats();

    /// <summary>
    /// Rebuilds the short-, medium- and long-range weapon lists and the equipment list from the mech's inventory,
    /// and sorts them.
    /// </summary>
    void SetWeaponLists();

    /// <summary>Sorts <paramref name="count"/> entries of <paramref name="list"/> (one of the weapon lists).</summary>
    void SortWeaponList(int32_t* list, int32_t count);

    /// <summary>
    /// The inventory item of entry <paramref name="index"/> of <paramref name="list"/>. The entry's copy is found by
    /// counting the equal entries before it; that copy's <c>itemNum</c> goes to <paramref name="itemNum"/>.
    /// </summary>
    MCLogInventoryItem* GetInvItem(int32_t* list, int32_t index, uint8_t* itemNum);

    /// <summary>Repairs armor up to slider position <paramref name="sliderPos"/>, paying for it.</summary>
    void RepairArmor(int32_t sliderPos);

    /// <summary>Repairs internal structure up to slider position <paramref name="sliderPos"/>.</summary>
    void RepairInternal(int32_t sliderPos);

    /// <summary>Fills <paramref name="pane"/> with the weapon lists (headings and items).</summary>
    void SetInventory(MCScrollPane* pane);

    /// <summary>
    /// Port: draws the weapon lists (headings and items) into <paramref name="content"/>, a weapon-list pane's view:
    /// what <see cref="SetInventory"/> painted into the pane's picture.
    /// </summary>
    void DrawWeaponList(MCLogPort* content);

    /// <summary>The item on line <paramref name="line"/> of <paramref name="pane"/> (null on a heading).</summary>
    MCLogInventoryItem* GetItemFromScrollPane(MCScrollPane* pane, int32_t line, uint8_t* itemNum);

    /// <summary>
    /// Starts dragging the copy <paramref name="itemNum"/> of <paramref name="item"/> out of the mech: makes the drag
    /// icon, shows the item's info, takes it (and a weapon's ammo) out of the mech's inventory. The item's inventory
    /// index goes to <paramref name="inventoryIndex"/>, the copy's damage to <paramref name="hits"/>.
    /// </summary>
    void SetUpItemDragIcon(MCLogInventoryItem* item, uint8_t itemNum, MCGuiEvent* event, int32_t* inventoryIndex,
                           int32_t* hits);

    /// <summary>Returns 0.</summary>
    int DebugFunction1(int32_t arg1, int32_t arg2);

    /// <summary>The drop slot / row the block stands for (set by the inventory screen).</summary>
    int32_t SlotIndex = 0;
    MCLogMech* Mech = nullptr;
    /// <summary>The weapon lists.</summary>
    MCScrollPane* InventoryPane = nullptr;
    /// <summary>A copy of LogMech +0x1c (also the inventory row of <c>MechInventoryBlock</c>).</summary>
    int32_t ListPosition = 0;
    /// <summary>Inventory indices of the short-range weapons (range &lt; the first threshold), a logistics block.</summary>
    int32_t* ShortRangeWeapons = nullptr;
    int32_t NumShortRangeWeapons = 0;
    int32_t* MediumRangeWeapons = nullptr;
    int32_t NumMediumRangeWeapons = 0;
    int32_t* LongRangeWeapons = nullptr;
    int32_t NumLongRangeWeapons = 0;
    /// <summary>Equipment (component kinds 2, 0x10, 0x11).</summary>
    int32_t* Equipment = nullptr;
    int32_t NumEquipment = 0;
    /// <summary>The damage (the copy's <c>hits</c>) of every entry of the four lists, in list order.</summary>
    int32_t* ItemHits = nullptr;
    int32_t NumItems = 0;
    /// <summary>The picture of an item being dragged (0x1c x 0x1e).</summary>
    MCLogPort* DragPort = nullptr;
    /// <summary>The slider art (<c>logart\lsrupm05.tga</c>).</summary>
    MCLogPort* SliderArtPort = nullptr;
    /// <summary>Nonzero when armor, internal structure or the engine is damaged (repair button enabled).</summary>
    int32_t CanRepairStructure = 0;
    /// <summary>Nonzero when a weapon or piece of equipment is damaged.</summary>
    int32_t CanRepairItems = 0;
    /// <summary>Armor slider position (pixels).</summary>
    int32_t ArmorSliderPos = 0;
    /// <summary>Slider pixels per armor point (a constant over the total maximum armor).</summary>
    float ArmorPixelScale = 0.0f;
    /// <summary>Armor of the 11 locations when the block was made.</summary>
    int32_t StartArmor[11] = {};
    /// <summary>Armor slider position when the block was made.</summary>
    int32_t ArmorSliderStart = 0;
    int32_t InternalSliderPos = 0;
    float InternalPixelScale = 0.0f;
    /// <summary>Internal structure when the block was made (only the first 8 locations are used).</summary>
    int32_t StartInternal[11] = {};
    int32_t InternalSliderStart = 0;
    /// <summary>Engine slider position: 0x127, 0x113, 0xff or 0xeb for engine damage 0..3.</summary>
    int32_t EngineSliderPos = 0;
    int32_t EngineSliderStart = 0;
    /// <summary>The engine's inventory stat (its second byte is the damage level).</summary>
    MCLogInventoryStat* EngineStat = nullptr;

private:
    // Port: the pieces of the panel, drawn into port with its top at top (row 0 of the briefing box's picture, or
    // a row of the repair screen's unit pane).

    /// <summary>The background, the name art, the tonnage line and (when <paramref name="framed"/>) the selection frame.</summary>
    void PaintBase(MCLogPort* port, int32_t top, bool briefing, bool framed);
    /// <summary>The repair buttons, live when <paramref name="items"/> / <paramref name="structure"/>.</summary>
    void PaintButtons(MCLogPort* port, int32_t top, bool onRows, int32_t items, int32_t structure);
    void PaintDiagram(MCLogPort* port, int32_t top, int32_t xPos);
    void PaintBR(MCLogPort* port, int32_t top);
    void PaintSlider(MCLogPort* port, int32_t top, int32_t slider, bool briefing);
    void PaintStatusBar(MCLogPort* port, int32_t top, float status, bool repairLayout);
    /// <summary>The status bar, the pilot's portrait, callsign, rank, skills and health.</summary>
    void PaintPilot(MCLogPort* port, int32_t top, float status, bool repairLayout);
    /// <summary>The weapon list (its pane, as scrolled) and its slider column.</summary>
    void PaintInventory(MCLogPort* port, int32_t top);
    /// <summary>The weapons' weight against the free weight.</summary>
    void PaintTonnage(MCLogPort* port, int32_t top);

    /// <summary>Port: a weapon or piece of equipment is damaged (the item repair button is live).</summary>
    bool ItemsDamaged() const;
    /// <summary>Port: the engine, internal structure or armor is damaged (the structure repair button is live).</summary>
    bool StructureDamaged() const;
    /// <summary>Port: the block lists the mech's weapons (in multiplayer only the player's own mechs, and the boxed one).</summary>
    bool ShowsInventory() const;

    /// <summary>Port: the pilot is being dragged out of the row (its portrait's place blank until the drop).</summary>
    bool _PilotLifted = false;
    /// <summary>Port: the repair button held while its repair runs: 0 none, 1 items, 2 structure.</summary>
    int32_t _PressedButton = 0;
};

/// <summary>A vehicle's panel on the repair screen (vehicles can't be refitted: mostly a status bar).</summary>
/// <remarks>Original source: <c>logistics\mrblock.cpp</c>, 0x4e0 bytes.</remarks>
class MCVehicleRepairBlock : public MCLogObject
{
public:
    ~MCVehicleRepairBlock() override;

    void Init(MCLogVehicle* logVehicle);

    /// <summary>Nothing of its own (the inlined <see cref="MCLogObject::Destroy"/>).</summary>
    void Destroy() override;

    void HandleEvent(MCGuiEvent* event) override;

    void DrawDamageDiagram(MCLogPort* port);

    /// <summary>As <see cref="MCMechRepairBlock::DrawBackground"/>.</summary>
    void DrawBackground(int32_t row, MCLogPort* port);

    /// <summary>Port: as <see cref="MCMechRepairBlock::DrawRow"/>.</summary>
    void DrawRow(MCLogPort* port, int32_t top);

    /// <summary>
    /// Port: draws the picked-up vehicle's drag icon into <paramref name="surface"/> (0x1e square): its body diagram
    /// at (2, 0) over colour 0x10.
    /// </summary>
    void OnBeginDrag(MCLogPort* surface);

    /// <summary>Draws the status bar into <paramref name="port"/> at row <paramref name="row"/>.</summary>
    void SetBar(MCLogPort* port, int32_t row);

    /// <summary>Empty.</summary>
    void SetPilotStats();

    /// <summary>Empty.</summary>
    void SetPilotHealth(int32_t health, MCLogPort* port);

    /// <summary>Empty.</summary>
    void ClearPilot();

    int32_t SlotIndex = 0;
    MCLogVehicle* Vehicle = nullptr;

private:
    /// <summary>Port: the panel into <paramref name="port"/> with its top at <paramref name="top"/>.</summary>
    void PaintRow(MCLogPort* port, int32_t top, bool briefing, bool framed);
};

/// <summary>The panel of the briefing screen showing the selected mech (its repair block) or vehicle.</summary>
/// <remarks>Original source: <c>logistics\mrblock.cpp</c>, 0x4c8 bytes.</remarks>
class MCBriefingBox : public MCLogObject
{
public:
    ~MCBriefingBox() override;

    /// <summary>Shows <paramref name="logMech"/> (with a weapon-list pane) or <paramref name="logVehicle"/>.</summary>
    void Init(MCLogMech* logMech, MCLogVehicle* logVehicle);

    void Destroy() override;

    /// <summary>Draws the mech's or vehicle's repair block into the box.</summary>
    void DrawBackground();

    /// <summary>
    /// Port: draws the box (the unit's repair block, its weapon list and tonnage bar, darkened) at
    /// (<paramref name="xPos"/>, <paramref name="yPos"/>) in <paramref name="target"/>: what
    /// <see cref="DrawBackground"/> painted into the briefing screen's picture.
    /// </summary>
    void PaintBox(MCPane* target, int32_t xPos, int32_t yPos);

    /// <summary>Empty.</summary>
    void DrawVehicleBackground();

    void HandleEvent(MCGuiEvent* event) override;

    /// <summary>
    /// Port-only: over the weapon list the mouse wheel scrolls it, as its arrows do (the list's pane is hidden, so the
    /// wheel finds the box).
    /// </summary>
    bool MouseWheel(int32_t steps, int32_t xPos, int32_t yPos) override;

    void Draw() override;

    /// <summary>Empty (a bare <c>ret</c> the export has no function for).</summary>
    void Display() override;

    MCLogMech* Mech = nullptr;
    MCLogVehicle* Vehicle = nullptr;
    /// <summary>The mech's weapon lists (null for a vehicle).</summary>
    MCScrollPane* InventoryPane = nullptr;
};

/// <summary>Callback of the refit dialog when an item is dropped on a mech.</summary>
void RefitItemCallback();

/// <summary>
/// The 256-entry colour table used to darken greyed-out rows (<c>Logistics::darken</c>): AlphaTable row 0x100
/// (0x008009d0).
/// </summary>
extern char* LogisticFadetable;

// mrblock.cpp also has its own `MechRepairBlock* globalMechPurchaseBlock` (0x00808694), clashing by name with
// purchase.cpp's MechPurchaseBlock* one: it is a file-static of mrblock.cpp in the port, not declared here.
