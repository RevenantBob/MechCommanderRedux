#pragma once

#include "logistics/MCLogObject.h"

class MCCompInventoryBlock;
class MCScrollPane;

/// <summary>
/// Port: what an inventory screen shows in its info box under the inventory pane (at (2, 0x18a)) and in the column
/// header over the pane (at (0xc4, 0x65)); the screen draws it from here each frame. The original painted the blank
/// box, the details of the row under the mouse and the header into the screen's picture, where they stayed until
/// painted over.
/// </summary>
struct MCInvInfoBox
{
    /// <summary>Whose details the box shows.</summary>
    enum class Kind
    {
        /// <summary>None: the blank box only.</summary>
        None,
        /// <summary>A <c>MechInventoryBlock</c>'s mech.</summary>
        Mech,
        /// <summary>A <c>PilotInventoryBlock</c>'s pilot.</summary>
        Pilot,
        /// <summary>A <c>VehicleInventoryBlock</c>'s vehicle.</summary>
        Vehicle,
        /// <summary>A <c>CompInventoryBlock</c>'s component (a row of the component tab).</summary>
        Component,
        /// <summary>A <c>CompInventoryBlock</c>'s component in a mech's weapon list on the repair screen.</summary>
        RepairItem,
        /// <summary>A <c>MechRepairBlock</c>'s mech on the repair screen.</summary>
        RepairMech
    };

    /// <summary>The blank box shown (<c>Logistics::InventoryIconPorts</c>), or -1: the screen's picture shows.</summary>
    int32_t Art = -1;
    Kind InfoKind = Kind::None;
    /// <summary>The block whose details are shown (not a component's); it takes itself out when it goes.</summary>
    MCLogObject* Source = nullptr;
    /// <summary>
    /// A component's details, as they were when shown (the item can go while they are, as when it is dragged off a
    /// mech): its picture (<c>lscicc&lt;n&gt;</c>), its block's texts and its description.
    /// </summary>
    int32_t ComponentPicture = 0;
    std::string RangeText;
    std::string DamageText;
    std::string RecycleText;
    std::string Description;
    /// <summary>The tab whose column header is shown (0..3), or -1: the screen's picture shows.</summary>
    int32_t Header = -1;

    /// <summary>The screen's background art was painted over all of it.</summary>
    void Clear() { *this = MCInvInfoBox{}; }

    /// <summary>The blank box of tab <paramref name="tab"/> was painted over the box.</summary>
    void Blank(int32_t tab)
    {
        Art = tab;
        InfoKind = Kind::None;
        Source = nullptr;
    }
};

/// <summary>
/// The common base of the purchase and repair screens: the inventory pane on the left, with tabs for mechs, pilots,
/// components and vehicles, filled from the logistics inventory lists, and the unit pane on the right.
/// </summary>
/// <remarks>
/// Original source: <c>logistics\logscrn.cpp</c> (<c>LogInvScreen</c>). It had no virtual functions of its own (its
/// vtable was never emitted), so it shows up only through the methods the purchase and repair screens inherit.
/// Several methods only use <c>GlobalLogPtr</c>.
/// </remarks>
class MCLogInvScreen : public MCLogObject
{
public:
    /// <summary>The height of a unit block in the unit and store panes.</summary>
    static constexpr int32_t UnitBlockHeight = 0x70;
    /// <summary>The height of an inventory block.</summary>
    static constexpr int32_t InvBlockHeight = 0x2b;

    /// <summary>Forgets the screen (see <see cref="ForgetInfoSource"/>).</summary>
    ~MCLogInvScreen() override;

    /// <summary>Puts the force's repair blocks in the repair screen's unit pane, a row each (mechs, then vehicles).</summary>
    void CreateVehiclePane();

    /// <summary>
    /// Sets up the store's tabs (mechs, vehicles, components) unless <paramref name="pilotsOnly"/>, and the pilots
    /// for hire (always rebuilt).
    /// </summary>
    static void CreatePurVehiclePane(bool pilotsOnly);

    /// <summary>Makes the mech tab's view and numbers its blocks.</summary>
    static void CreateMechInvBlock();

    /// <summary>Makes the vehicle tab's view and numbers its blocks.</summary>
    static void CreateVhclInvBlock();

    /// <summary>Makes the pilot tab's view and numbers its blocks.</summary>
    void CreatePilotInvBlock() const;

    /// <summary>Makes the component tab's view (after re-indexing the inventory).</summary>
    static void CreateCompInvBlock();

    /// <summary>Clears the info box under the inventory for tab <paramref name="tab"/> (negative = the current one).</summary>
    static void DrawBlankInvInfoBlock(int32_t tab);

    /// <summary>
    /// Shows the mech tab: puts the mech blocks in the inventory panes (<paramref name="resetScroll"/> scrolls them
    /// to the top); <paramref name="redrawTabs"/> blanks the info box.
    /// </summary>
    void SetUpMechInv(bool resetScroll, bool redrawTabs);

    /// <summary>Shows the mechs for sale in the store (on the purchase screen only).</summary>
    void SetUpMechPurchase() const;

    /// <summary>Shows the pilot tab (the assigned pilots, last in the list, aren't shown).</summary>
    void SetUpPilotInv(bool resetScroll, bool redrawTabs);

    /// <summary>Shows the component tab.</summary>
    void SetUpCompInv(bool resetScroll, bool redrawTabs);

    /// <summary>Shows the vehicle tab.</summary>
    void SetUpVhclInv(bool resetScroll, bool redrawTabs);

    /// <summary>Shows the vehicles for sale in the store.</summary>
    void SetUpVehiclePurchase() const;

    /// <summary>Shows the pilots for hire in the store.</summary>
    void SetUpPilotPurchase() const;

    /// <summary>Numbers the store's component blocks by their sort order.</summary>
    static void ReIndexComponents();

    /// <summary>Shows the components for sale in the store.</summary>
    void SetUpCompPurchase() const;

    /// <summary>Takes the pilot of row <paramref name="pilotIndex"/> out of the store: the rows below move up.</summary>
    static void RemovePilot(int32_t pilotIndex);

    /// <summary>
    /// Port: draws the screen: its background art, the info box and column header (<see cref="Info"/>), then the
    /// shared places (<see cref="MCLogScreenChrome"/>). Outside the frame pass it refreshes the children, as the
    /// original's paint.
    /// </summary>
    void Draw() override;

    /// <summary>Port: the screen draws itself each frame (its port is a view).</summary>
    bool DrawsLive() override { return true; }

    /// <summary>Port: see <see cref="MCLogScreenChrome"/>.</summary>
    MCLogScreenChrome* Chrome() override { return &ScreenChrome; }

    /// <summary>
    /// Port: sets the screen's background art (<paramref name="artName"/> in <c>logart</c>, which the original loaded as
    /// the screen's picture) and lists the screen for <see cref="Of"/>.
    /// </summary>
    void InitLive(std::string_view artName);

    /// <summary>
    /// Port: shows the details of <paramref name="source"/> in the info box, over the blank box last shown (the
    /// original painted them there right after it).
    /// </summary>
    void ShowInfo(MCInvInfoBox::Kind kind, MCLogObject* source);

    /// <summary>
    /// Port: shows the details of <paramref name="block"/>'s component in the info box (as <see cref="ShowInfo"/>),
    /// from a mech's weapon list when <paramref name="repairItem"/>.
    /// </summary>
    void ShowComponentInfo(MCCompInventoryBlock* block, bool repairItem);

    /// <summary>Port: draws <see cref="Info"/> into <paramref name="port"/> (the screen's view).</summary>
    void DrawInfo(MCLogPort* port);

    /// <summary>Port: the inventory screen <paramref name="screen"/> is, or null for another screen.</summary>
    static MCLogInvScreen* Of(MCGuiObject* screen);

    /// <summary>Port: <paramref name="source"/> is going: no inventory screen shows its details any more.</summary>
    static void ForgetInfoSource(MCLogObject* source);

    /// <summary>Port: see <see cref="MCInvInfoBox"/>.</summary>
    MCInvInfoBox Info;

    /// <summary>
    /// Set while the screen's chat button blinks (multiplayer; timer 7 on the purchase screen, 8 on the repair
    /// screen); <c>BriefingScreen::SetUpOperation</c> stops it.
    /// </summary>
    bool ChatBlinking = false;
    /// <summary>The inventory pane (left).</summary>
    MCScrollPane* InventoryPane = nullptr;
    /// <summary>The unit pane: the store on the purchase screen, the force on the repair screen.</summary>
    MCScrollPane* UnitPane = nullptr;

    /// <summary>Port: what the screen shows of the shared places.</summary>
    MCLogScreenChrome ScreenChrome;
    /// <summary>Port: the background art's file name in <c>logart</c> (<see cref="InitLive"/>).</summary>
    std::string BackgroundArt;

protected:
    /// <summary>
    /// Makes the inventory pane (empty) and the unit pane (backed by <c>lsrbk01</c>; emptied when
    /// <paramref name="emptyUnitPane"/>), owned by the screen and added as its children.
    /// </summary>
    void MakePanes(bool emptyUnitPane);

    /// <summary>Frees the panes (their content is the logistics screen's, not theirs).</summary>
    void FreePanes();

private:
    MCGuiOwned<MCScrollPane> _OwnedInventoryPane;
    MCGuiOwned<MCScrollPane> _OwnedUnitPane;
};
