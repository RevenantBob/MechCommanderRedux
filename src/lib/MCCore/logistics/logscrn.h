#pragma once

#include "logistics/lport.h"

class aEvent;
class lChatInput;
class ScrollPane;

/// <summary>
/// The common base of the purchase and repair screens: the inventory pane on the left, with tabs for mechs,
/// pilots, components and vehicles, filled from the logistics inventory lists.
/// </summary>
/// <remarks>
/// Original source: <c>logistics\logscrn.cpp</c>, 0x4cc bytes. It has no virtual functions of its own (its vtable
/// was never emitted), so it shows up only through the methods <see cref="PurchaseScreen"/> and
/// <see cref="RepairScreen"/> inherit. Several methods only use <c>globalLogPtr</c>.
/// </remarks>
class LogInvScreen : public lObject
{
public:
    /// <summary>Rebuilds the vehicle pane port and blocks.</summary>
    /// <remarks>MCX.EXE @ 0x00708e40</remarks>
    void createVehiclePane();

    /// <summary>Rebuilds the vehicle pane with the vehicles for sale.</summary>
    /// <remarks>MCX.EXE @ 0x00709040</remarks>
    void createPurVehiclePane(int redraw);

    /// <summary>Makes the port of mech inventory blocks and draws each block into it.</summary>
    /// <remarks>MCX.EXE @ 0x00709660</remarks>
    void createMechInvBlock();

    /// <summary>Makes the port of vehicle inventory blocks and draws each block into it.</summary>
    /// <remarks>MCX.EXE @ 0x00709750</remarks>
    void createVhclInvBlock();

    /// <summary>Makes the port of pilot inventory blocks and draws each block into it.</summary>
    /// <remarks>MCX.EXE @ 0x00709840</remarks>
    void createPilotInvBlock();

    /// <summary>Makes the port of component inventory blocks (after re-indexing the inventory) and draws each into it.</summary>
    /// <remarks>MCX.EXE @ 0x00709a50 (unnamed in the symbols: the name is inferred from its siblings; it uses no <c>this</c>).</remarks>
    void createCompInvBlock();

    /// <summary>Clears the info box under the inventory for tab <paramref name="tab"/> (negative = the current one).</summary>
    /// <remarks>MCX.EXE @ 0x00709950</remarks>
    void drawBlankInvInfoBlock(int32_t tab);

    /// <summary>
    /// Shows the mech tab: puts the mech blocks in the inventory pane at scroll <paramref name="scrollPos"/>;
    /// <paramref name="redrawTabs"/> redraws the tab art.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00709b20</remarks>
    void setUpMechInv(int scrollPos, int redrawTabs);

    /// <summary>Shows the mechs for sale in the purchase pane.</summary>
    /// <remarks>MCX.EXE @ 0x00709de0</remarks>
    void setUpMechPurchase();

    /// <summary>Shows the pilot tab.</summary>
    /// <remarks>MCX.EXE @ 0x00709f60</remarks>
    void setUpPilotInv(int scrollPos, int redrawTabs);

    /// <summary>Shows the component tab.</summary>
    /// <remarks>MCX.EXE @ 0x0070a230</remarks>
    void setUpCompInv(int scrollPos, int redrawTabs);

    /// <summary>Shows the vehicle tab.</summary>
    /// <remarks>MCX.EXE @ 0x0070a500</remarks>
    void setUpVhclInv(int scrollPos, int redrawTabs);

    /// <summary>Shows the vehicles for sale in the purchase pane.</summary>
    /// <remarks>MCX.EXE @ 0x0070a7c0</remarks>
    void setUpVehiclePurchase();

    /// <summary>Shows the pilots for hire in the purchase pane.</summary>
    /// <remarks>MCX.EXE @ 0x0070a8f0</remarks>
    void setUpPilotPurchase();

    /// <summary>Renumbers the component blocks after one was added or removed.</summary>
    /// <remarks>MCX.EXE @ 0x0070aa20</remarks>
    void reIndexComponents();

    /// <summary>Shows the components for sale in the purchase pane.</summary>
    /// <remarks>MCX.EXE @ 0x0070aaa0</remarks>
    void setUpCompPurchase();

    /// <summary>Removes pilot <paramref name="pilotIndex"/> from the inventory and its block.</summary>
    /// <remarks>MCX.EXE @ 0x0070abc0</remarks>
    void removePilot(int32_t pilotIndex);

    /// <summary>Set by the screens' <c>init</c> (0 on the purchase screen, -1 on the repair screen); use not seen.</summary>
    int32_t unknown4BC = 0; // +0x4bc
    /// <summary>
    /// Set while the screen's chat button blinks (multiplayer; timer 7 on the purchase screen, 8 on the repair
    /// screen); <c>BriefingScreen::setUpOperation</c> stops it.
    /// </summary>
    int32_t chatBlinking = 0; // +0x4c0
    /// <summary>The inventory pane (left).</summary>
    ScrollPane* inventoryPane = nullptr; // +0x4c4
    /// <summary>The unit pane: the store on the purchase screen, the vehicles on the repair screen.</summary>
    ScrollPane* unitPane = nullptr; // +0x4c8
};

/// <summary>
/// The multiplayer chat panel of the logistics screens: a scrolling history and an input line with a team/all
/// toggle.
/// </summary>
/// <remarks>Original source: <c>logistics\logscrn.cpp</c>, 0x4d0 bytes.</remarks>
class LogChatWindow : public lObject
{
public:
    /// <summary>
    /// Places the window; <paramref name="historySize"/> is the history's size in pixels of the pane's width
    /// (so its height is <c>historySize / width</c>).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0070aed0</remarks>
    void init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, int32_t historySize);

    /// <summary>Calls <see cref="destroy"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0070b1b0 (vector deleting destructor 0x006f0530)</remarks>
    ~LogChatWindow() override;

    /// <remarks>MCX.EXE @ 0x0070b1e0</remarks>
    void ShowGUIWindow(int show) override;

    /// <remarks>MCX.EXE @ 0x0070b1f0</remarks>
    void destroy() override;

    /// <summary>A chat message from the network: the text is at +9 and the team flag at +8 of <paramref name="message"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0070b270</remarks>
    void handleNetworkMessage(uint32_t fromPlayerId, void* message);

    /// <summary>
    /// Adds "<c>name: text</c>" to the history in the sender's colour and <paramref name="textColor"/> (-1 = 6),
    /// scrolling the old lines up.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0070b2a0</remarks>
    void processChatString(uint32_t fromPlayerId, char* string, int32_t textColor);

    /// <summary>Passes key presses on to the parent screen.</summary>
    /// <remarks>MCX.EXE @ 0x0070b460</remarks>
    void handleEvent(aEvent* event) override;

    /// <summary>Changes the height, keeping the history.</summary>
    /// <remarks>MCX.EXE @ 0x0070b490</remarks>
    void resize(int32_t height);

    /// <summary>Clears the history and the input line.</summary>
    /// <remarks>MCX.EXE @ 0x0070b640</remarks>
    void reset();

    /// <summary>The history pane.</summary>
    ScrollPane* historyPane = nullptr; // +0x4bc
    /// <summary>The frame picture along the bottom (<c>lsbdw04</c>).</summary>
    lPort* framePort = nullptr; // +0x4c0
    /// <summary>The input line.</summary>
    lChatInput* chatInput = nullptr; // +0x4c4
    /// <summary>Only cleared by <see cref="init"/>.</summary>
    int32_t unknown4C8 = 0; // +0x4c8
    /// <summary>The history size given to <see cref="init"/>.</summary>
    int32_t historySize = 0; // +0x4cc
};
