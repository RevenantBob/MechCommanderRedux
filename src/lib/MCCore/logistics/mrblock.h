#pragma once

#include "gui/scrlpane.h"
#include "logistics/lport.h"

class aEvent;
class aFont;
class LogMech;
class LogVehicle;
struct _LogInventoryItem;
struct _LogInventoryStat;

/// <summary>
/// A mech's panel on the repair (refit) screen (0x19a x 0x70): damage diagram, battle rating, pilot, the three
/// repair sliders (armor, internal structure, engine) and the weapon lists in a scroll pane.
/// </summary>
/// <remarks>Original source: <c>logistics\mrblock.cpp</c>, 0x584 bytes.</remarks>
class MechRepairBlock : public lObject
{
public:
    /// <remarks>MCX.EXE @ 0x006ed390 (vector deleting destructor)</remarks>
    ~MechRepairBlock() override;

    /// <summary>
    /// Shows <paramref name="logMech"/>: makes the inventory scroll pane and slider art, and records the mech's
    /// armor and internal structure as the sliders' starting points.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00715f20</remarks>
    void init(LogMech* logMech);

    /// <summary>Frees the scroll pane, the slider art and the weapon lists.</summary>
    /// <remarks>MCX.EXE @ 0x00716180</remarks>
    void destroy() override;

    /// <summary>
    /// Port: draws the mech's details into the repair screen's info box (see <c>InvInfoBox</c>): its diagram,
    /// tonnage, classes, speed and description.
    /// </summary>
    void DrawInfo(lPort* port);

    /// <summary>Takes the mech out of its drop slot.</summary>
    /// <remarks>MCX.EXE @ 0x007162a0</remarks>
    void undeployMech();

    /// <summary>Sliders, repair buttons, pilot and item drag and drop.</summary>
    /// <remarks>MCX.EXE @ 0x00716350</remarks>
    void handleEvent(aEvent* event) override;

    /// <summary>
    /// Draws the panel. <paramref name="row"/> &lt; 0 draws into <paramref name="port"/> (the briefing box);
    /// otherwise into the repair screen's scroll pane at that row. Port: the pane's rows are drawn each frame by
    /// <see cref="DrawRow"/>; for them this ends a pilot drag or a held repair button and does the rest of what the
    /// paint did (the battle rating, the weapon lists).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x007191b0</remarks>
    void drawBackground(int32_t row, lPort* port);

    /// <summary>
    /// Port: draws the row into <paramref name="port"/> (the repair screen's unit pane) with its top at
    /// <paramref name="top"/>, from the state: framed while selected, darkened while another mech is.
    /// </summary>
    void DrawRow(lPort* port, int32_t top);

    /// <summary>
    /// Port: draws the picked-up pilot's drag icon into <paramref name="surface"/> (0x20 square): the row's square at
    /// (5, 0x25), the portrait, over the unit list's colour 0xff. Before <see cref="clearPilot"/>.
    /// </summary>
    void OnBeginDragPilot(lPort* surface);

    /// <summary>
    /// Port: draws the picked-up mech's drag icon into <paramref name="surface"/> (0x20 square): its body diagram at
    /// (2, 1) over colour 0x10.
    /// </summary>
    void OnBeginDragMech(lPort* surface);

    /// <summary>
    /// Port: draws the drag icon of <paramref name="item"/>, picked out of the weapon list, into
    /// <paramref name="surface"/> (0x20 square): its picture (<c>lscicc</c>) at (1, 1).
    /// </summary>
    void OnBeginDragItem(lPort* surface, _LogInventoryItem* item);

    /// <summary>Draws the repair buttons, enabled when there is something to repair.</summary>
    /// <remarks>MCX.EXE @ 0x00719630</remarks>
    void drawButtons(lPort* port);

    /// <remarks>MCX.EXE @ 0x00719a50</remarks>
    void drawDamageDiagram(lPort* port);

    /// <summary>Draws the battle rating.</summary>
    /// <remarks>MCX.EXE @ 0x00719db0</remarks>
    void drawBR(lPort* port);

    /// <summary><see cref="drawSlider"/> 0.</summary>
    /// <remarks>MCX.EXE @ 0x0071a370</remarks>
    void drawArmorSlider(lPort* port);

    /// <summary>Draws slider <paramref name="slider"/> (0 armor, 1 internal structure, 2 engine).</summary>
    /// <remarks>MCX.EXE @ 0x0071a390</remarks>
    void drawSlider(lPort* port, int32_t slider);

    /// <summary><see cref="drawSlider"/> 1.</summary>
    /// <remarks>MCX.EXE @ 0x0071a840</remarks>
    void drawInternalSlider(lPort* port);

    /// <summary><see cref="drawSlider"/> 2.</summary>
    /// <remarks>MCX.EXE @ 0x0071a860</remarks>
    void drawEngineSlider(lPort* port);

    /// <remarks>MCX.EXE @ 0x0071a880</remarks>
    void draw() override;

    /// <remarks>MCX.EXE @ 0x0071a8b0</remarks>
    void drawInventory(lPort* port);

    /// <summary>
    /// Port-only: over the weapon list the mouse wheel scrolls it, as its arrows do. The list's pane is hidden (the
    /// block draws it), so the wheel finds the block, which would otherwise pass it on to the unit list.
    /// </summary>
    bool MouseWheel(int32_t steps, int32_t xPos, int32_t yPos) override;

    /// <summary>
    /// Draws the mech's status bar (colour by <c>LogMech::calcStatus</c>) into <paramref name="port"/>, or into the
    /// repair scroll pane at <see cref="slotIndex"/> when it is null.
    /// </summary>
    /// <remarks>
    /// MCX.EXE @ 0x0071ad30 (FUN_0071ad30: unnamed in the binary; the name is the port's, after
    /// VehicleRepairBlock::setBar).
    /// </remarks>
    void drawStatusBar(lPort* port = nullptr);

    /// <summary>With <paramref name="value"/> &lt; 0, puts the engine slider at the engine's damage level.</summary>
    /// <remarks>MCX.EXE @ 0x0071b0a0</remarks>
    void setEngineSlider(int32_t value);

    /// <summary>With <paramref name="value"/> &lt; 0, puts the internal-structure slider at the current total.</summary>
    /// <remarks>MCX.EXE @ 0x0071b170</remarks>
    void setInternalSlider(int32_t value);

    /// <summary>With <paramref name="value"/> &lt; 0, puts the armor slider at the current total.</summary>
    /// <remarks>MCX.EXE @ 0x0071b1e0</remarks>
    void setArmorSlider(int32_t value);

    /// <remarks>MCX.EXE @ 0x0071b300</remarks>
    void clearPilot();

    /// <remarks>MCX.EXE @ 0x0071b3e0</remarks>
    void setPilotStats(lPort* port);

    /// <remarks>MCX.EXE @ 0x0071b940</remarks>
    void setPilotHealth(lPort* port);

    /// <summary>Empty.</summary>
    /// <remarks>MCX.EXE @ 0x0071baa0</remarks>
    void setMechStats();

    /// <summary>
    /// Rebuilds the short-, medium- and long-range weapon lists and the equipment list from the mech's inventory,
    /// and sorts them.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0071bab0</remarks>
    void setWeaponLists();

    /// <summary>Sorts <paramref name="count"/> entries of <paramref name="list"/> (one of the weapon lists).</summary>
    /// <remarks>MCX.EXE @ 0x0071bf50</remarks>
    void sortWeaponList(int32_t* list, int32_t count);

    /// <summary>
    /// The inventory item of entry <paramref name="index"/> of <paramref name="list"/>. The entry's copy is found by
    /// counting the equal entries before it; that copy's <c>itemNum</c> goes to <paramref name="itemNum"/>.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0071c090</remarks>
    _LogInventoryItem* getInvItem(int32_t* list, int32_t index, uint8_t* itemNum);

    /// <summary>Repairs armor up to slider position <paramref name="sliderPos"/>, paying for it.</summary>
    /// <remarks>MCX.EXE @ 0x0071c0f0</remarks>
    void repairArmor(int32_t sliderPos);

    /// <summary>Repairs internal structure up to slider position <paramref name="sliderPos"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0071c510</remarks>
    void repairInternal(int32_t sliderPos);

    /// <summary>Fills <paramref name="pane"/> with the weapon lists (headings and items).</summary>
    /// <remarks>MCX.EXE @ 0x0071c8b0</remarks>
    void setInventory(ScrollPane* pane);

    /// <summary>
    /// Port: draws the weapon lists (headings and items) into <paramref name="content"/>, a weapon-list pane's view:
    /// what <see cref="setInventory"/> painted into the pane's picture.
    /// </summary>
    void DrawWeaponList(lPort* content);

    /// <summary>The item on line <paramref name="line"/> of <paramref name="pane"/> (null on a heading).</summary>
    /// <remarks>MCX.EXE @ 0x0071ed40</remarks>
    _LogInventoryItem* getItemFromScrollPane(ScrollPane* pane, int32_t line, uint8_t* itemNum);

    /// <summary>
    /// Starts dragging the copy <paramref name="itemNum"/> of <paramref name="item"/> out of the mech: makes the drag
    /// icon, shows the item's info, takes it (and a weapon's ammo) out of the mech's inventory. The item's inventory
    /// index goes to <paramref name="inventoryIndex"/>, the copy's damage to <paramref name="hits"/>.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0071ee40</remarks>
    void setUpItemDragIcon(_LogInventoryItem* item, uint8_t itemNum, aEvent* event, int32_t* inventoryIndex,
                           int32_t* hits);

    /// <summary>Returns 0.</summary>
    /// <remarks>MCX.EXE @ 0x0071f3e0</remarks>
    int DebugFunction1(int32_t arg1, int32_t arg2);

    /// <summary>The drop slot / row the block stands for (set by the inventory screen).</summary>
    int32_t slotIndex = 0;   // +0x4bc
    LogMech* mech = nullptr; // +0x4c0
    /// <summary>The weapon lists.</summary>
    ScrollPane* inventoryPane = nullptr; // +0x4c4
    /// <summary>A copy of LogMech +0x1c (also the inventory row of <c>MechInventoryBlock</c>).</summary>
    int32_t listPosition = 0; // +0x4c8
    /// <summary>Inventory indices of the short-range weapons (range &lt; the first threshold), on the logistics heap.</summary>
    int32_t* shortRangeWeapons = nullptr;  // +0x4cc
    int32_t numShortRangeWeapons = 0;      // +0x4d0
    int32_t* mediumRangeWeapons = nullptr; // +0x4d4
    int32_t numMediumRangeWeapons = 0;     // +0x4d8
    int32_t* longRangeWeapons = nullptr;   // +0x4dc
    int32_t numLongRangeWeapons = 0;       // +0x4e0
    /// <summary>Equipment (component kinds 2, 0x10, 0x11).</summary>
    int32_t* equipment = nullptr; // +0x4e4
    int32_t numEquipment = 0;     // +0x4e8
    /// <summary>The damage (the copy's <c>hits</c>) of every entry of the four lists, in list order.</summary>
    int32_t* itemHits = nullptr; // +0x4ec
    int32_t numItems = 0;        // +0x4f0
    /// <summary>The picture of an item being dragged (0x1c x 0x1e).</summary>
    lPort* dragPort = nullptr; // +0x4f4
    /// <summary>Set to -1 by <see cref="init"/>, otherwise unused.</summary>
    int32_t unknown4F8 = -1; // +0x4f8
    /// <summary>The slider art (<c>logart\lsrupm05.tga</c>).</summary>
    lPort* sliderArtPort = nullptr; // +0x4fc
    /// <summary>Nonzero when armor, internal structure or the engine is damaged (repair button enabled).</summary>
    int32_t canRepairStructure = 0; // +0x500
    /// <summary>Nonzero when a weapon or piece of equipment is damaged.</summary>
    int32_t canRepairItems = 0; // +0x504
    /// <summary>Armor slider position (pixels).</summary>
    int32_t armorSliderPos = 0; // +0x508
    /// <summary>Slider pixels per armor point (a constant over the total maximum armor).</summary>
    float armorPixelScale = 0.0f; // +0x50c
    /// <summary>Armor of the 11 locations when the block was made.</summary>
    int32_t startArmor[11] = {}; // +0x510
    /// <summary>Armor slider position when the block was made.</summary>
    int32_t armorSliderStart = 0;    // +0x53c
    int32_t internalSliderPos = 0;   // +0x540
    float internalPixelScale = 0.0f; // +0x544
    /// <summary>Internal structure when the block was made (only the first 8 locations are used).</summary>
    int32_t startInternal[11] = {};  // +0x548
    int32_t internalSliderStart = 0; // +0x574
    /// <summary>Engine slider position: 0x127, 0x113, 0xff or 0xeb for engine damage 0..3.</summary>
    int32_t engineSliderPos = 0;   // +0x578
    int32_t engineSliderStart = 0; // +0x57c
    /// <summary>The engine's inventory stat (its second byte is the damage level).</summary>
    _LogInventoryStat* engineStat = nullptr; // +0x580

private:
    // Port: the pieces of the panel, drawn into port with its top at top (row 0 of the briefing box's picture, or
    // a row of the repair screen's unit pane).

    /// <summary>The background, the name art, the tonnage line and (when <paramref name="framed"/>) the selection frame.</summary>
    void PaintBase(lPort* port, int32_t top, bool briefing, bool framed);
    /// <summary>The repair buttons, live when <paramref name="items"/> / <paramref name="structure"/>.</summary>
    void PaintButtons(lPort* port, int32_t top, bool onRows, int32_t items, int32_t structure);
    void PaintDiagram(lPort* port, int32_t top, int32_t xPos);
    void PaintBR(lPort* port, int32_t top);
    void PaintSlider(lPort* port, int32_t top, int32_t slider, bool briefing);
    void PaintStatusBar(lPort* port, int32_t top, float status, bool repairLayout);
    /// <summary>The status bar, the pilot's portrait, callsign, rank, skills and health.</summary>
    void PaintPilot(lPort* port, int32_t top, float status, bool repairLayout);
    /// <summary>The weapon list (its pane, as scrolled) and its slider column.</summary>
    void PaintInventory(lPort* port, int32_t top);
    /// <summary>The weapons' weight against the free weight.</summary>
    void PaintTonnage(lPort* port, int32_t top);

    /// <summary>Port: a weapon or piece of equipment is damaged (the item repair button is live).</summary>
    bool ItemsDamaged() const;
    /// <summary>Port: the engine, internal structure or armor is damaged (the structure repair button is live).</summary>
    bool StructureDamaged() const;
    /// <summary>Port: the block lists the mech's weapons (in multiplayer only the player's own mechs, and the boxed one).</summary>
    bool ShowsInventory() const;

    /// <summary>Port: the pilot is being dragged out of the row (its portrait's place blank until the drop).</summary>
    bool pilotLifted = false;
    /// <summary>Port: the repair button held while its repair runs: 0 none, 1 items, 2 structure.</summary>
    int32_t pressedButton = 0;
};

/// <summary>A vehicle's panel on the repair screen (vehicles can't be refitted: mostly a status bar).</summary>
/// <remarks>Original source: <c>logistics\mrblock.cpp</c>, 0x4e0 bytes.</remarks>
class VehicleRepairBlock : public lObject
{
public:
    /// <remarks>MCX.EXE @ 0x006fc2f0 (vector deleting destructor)</remarks>
    ~VehicleRepairBlock() override;

    /// <remarks>MCX.EXE @ 0x0071d4d0</remarks>
    void init(LogVehicle* logVehicle);

    /// <summary>Nothing of its own (the inlined <see cref="lObject::destroy"/>).</summary>
    /// <remarks>MCX.EXE @ 0x0071d520</remarks>
    void destroy() override;

    /// <remarks>MCX.EXE @ 0x0071d530</remarks>
    void handleEvent(aEvent* event) override;

    /// <remarks>MCX.EXE @ 0x0071ddc0</remarks>
    void drawDamageDiagram(lPort* port);

    /// <summary>As <see cref="MechRepairBlock::drawBackground"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0071df00</remarks>
    void drawBackground(int32_t row, lPort* port);

    /// <summary>Port: as <see cref="MechRepairBlock::DrawRow"/>.</summary>
    void DrawRow(lPort* port, int32_t top);

    /// <summary>
    /// Port: draws the picked-up vehicle's drag icon into <paramref name="surface"/> (0x1e square): its body diagram
    /// at (2, 0) over colour 0x10.
    /// </summary>
    void OnBeginDrag(lPort* surface);

    /// <summary>Draws the status bar into <paramref name="port"/> at row <paramref name="row"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0071e410</remarks>
    void setBar(lPort* port, int32_t row);

    /// <summary>Empty.</summary>
    /// <remarks>MCX.EXE @ 0x0071e690</remarks>
    void setPilotStats();

    /// <summary>Empty.</summary>
    /// <remarks>MCX.EXE @ 0x0071e6a0</remarks>
    void setPilotHealth(int32_t health, lPort* port);

    /// <summary>Empty.</summary>
    /// <remarks>MCX.EXE @ 0x0071e6b0</remarks>
    void clearPilot();

    int32_t slotIndex = 0;         // +0x4bc
    LogVehicle* vehicle = nullptr; // +0x4c0
    /// <summary>Never touched by the code: the class is 0x4e0 bytes but ends its known fields at 0x4c4.</summary>
    int32_t unknown4C4[7] = {}; // +0x4c4

private:
    /// <summary>Port: the panel into <paramref name="port"/> with its top at <paramref name="top"/>.</summary>
    void PaintRow(lPort* port, int32_t top, bool briefing, bool framed);
};

/// <summary>The panel of the briefing screen showing the selected mech (its repair block) or vehicle.</summary>
/// <remarks>Original source: <c>logistics\mrblock.cpp</c>, 0x4c8 bytes.</remarks>
class BriefingBox : public lObject
{
public:
    /// <remarks>MCX.EXE @ 0x006ed3d0 (vector deleting destructor)</remarks>
    ~BriefingBox() override;

    /// <summary>Shows <paramref name="logMech"/> (with a weapon-list pane) or <paramref name="logVehicle"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0071e6c0</remarks>
    void init(LogMech* logMech, LogVehicle* logVehicle);

    /// <remarks>MCX.EXE @ 0x0071e780</remarks>
    void destroy() override;

    /// <summary>Draws the mech's or vehicle's repair block into the box.</summary>
    /// <remarks>MCX.EXE @ 0x0071e7b0</remarks>
    void drawBackground();

    /// <summary>
    /// Port: draws the box (the unit's repair block, its weapon list and tonnage bar, darkened) at
    /// (<paramref name="xPos"/>, <paramref name="yPos"/>) in <paramref name="target"/>: what
    /// <see cref="drawBackground"/> painted into the briefing screen's picture.
    /// </summary>
    void PaintBox(PANE* target, int32_t xPos, int32_t yPos);

    /// <summary>Empty.</summary>
    /// <remarks>MCX.EXE @ 0x0071eba0</remarks>
    void drawVehicleBackground();

    /// <remarks>MCX.EXE @ 0x0071ebc0</remarks>
    void handleEvent(aEvent* event) override;

    /// <summary>
    /// Port-only: over the weapon list the mouse wheel scrolls it, as its arrows do (the list's pane is hidden, so the
    /// wheel finds the box).
    /// </summary>
    bool MouseWheel(int32_t steps, int32_t xPos, int32_t yPos) override;

    /// <remarks>MCX.EXE @ 0x0071ec10</remarks>
    void draw() override;

    /// <summary>Empty (a bare <c>ret</c> the export has no function for).</summary>
    /// <remarks>MCX.EXE @ 0x0071ebb0</remarks>
    void display() override;

    LogMech* mech = nullptr;       // +0x4bc
    LogVehicle* vehicle = nullptr; // +0x4c0
    /// <summary>The mech's weapon lists (null for a vehicle).</summary>
    ScrollPane* inventoryPane = nullptr; // +0x4c4
};

/// <summary>Callback of the refit dialog when an item is dropped on a mech.</summary>
/// <remarks>MCX.EXE @ 0x00715d80</remarks>
void RefitItemCallback();

/// <summary>
/// The 256-entry colour table used to darken greyed-out rows (<c>Logistics::darken</c>): AlphaTable row 0x100
/// (0x008009d0).
/// </summary>
extern char* g_logistic_fadetable;

// mrblock.cpp also has its own `MechRepairBlock* globalMechPurchaseBlock` (0x00808694), clashing by name with
// purchase.cpp's MechPurchaseBlock* one: it is a file-static of mrblock.cpp in the port, not declared here.
