#pragma once

#include "logistics/lport.h"

class aEvent;
class CompInventoryBlock;
class lChatInput;
class ScrollPane;

/// <summary>
/// Port: what an inventory screen shows in its info box under the inventory pane (at (2, 0x18a)) and in the column
/// header over the pane (at (0xc4, 0x65)); the screen draws it from here each frame. The original painted the blank
/// box, the details of the row under the mouse and the header into the screen's picture, where they stayed until
/// painted over.
/// </summary>
struct InvInfoBox
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

    /// <summary>The blank box shown (<c>Logistics::inventoryIconPorts</c>), or -1: the screen's picture shows.</summary>
    int32_t art = -1;
    Kind kind = Kind::None;
    /// <summary>The block whose details are shown (not a component's); it takes itself out when it goes.</summary>
    lObject* source = nullptr;
    /// <summary>
    /// A component's details, as they were when shown (the item can go while they are, as when it is dragged off a
    /// mech): its picture (<c>lscicc&lt;n&gt;</c>), its block's texts and its description.
    /// </summary>
    int32_t componentPicture = 0;
    char rangeText[12] = {};
    char damageText[12] = {};
    char recycleText[12] = {};
    std::string description;
    /// <summary>The tab whose column header is shown (0..3), or -1: the screen's picture shows.</summary>
    int32_t header = -1;

    /// <summary>The screen's background art was painted over all of it.</summary>
    void Clear() { *this = InvInfoBox{}; }

    /// <summary>The blank box of tab <paramref name="tab"/> was painted over the box.</summary>
    void Blank(int32_t tab)
    {
        art = tab;
        kind = Kind::None;
        source = nullptr;
    }
};

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

    /// <summary>
    /// Port: draws the screen: its background art, the info box and column header (<see cref="info"/>), then the
    /// shared places (<see cref="LogScreenChrome"/>). Outside the frame pass it refreshes the children, as the
    /// original's paint.
    /// </summary>
    void draw() override;

    /// <summary>Port: the screen draws itself each frame (its port is a view).</summary>
    bool DrawsLive() override { return true; }

    /// <summary>Port: see <see cref="LogScreenChrome"/>.</summary>
    LogScreenChrome* Chrome() override { return &chrome; }

    /// <summary>
    /// Port: sets the screen's background art (<paramref name="artName"/> in <c>logart</c>, which the original loaded as
    /// the screen's picture) and lists the screen for <see cref="Of"/>.
    /// </summary>
    void initLive(const char* artName);

    /// <summary>Port: forgets the screen (see <see cref="ForgetInfoSource"/>).</summary>
    ~LogInvScreen() override;

    /// <summary>
    /// Port: shows the details of <paramref name="source"/> in the info box, over the blank box last shown
    /// (the original painted them there right after it).
    /// </summary>
    void ShowInfo(InvInfoBox::Kind kind, lObject* source);

    /// <summary>
    /// Port: shows the details of <paramref name="block"/>'s component in the info box (as <see cref="ShowInfo"/>),
    /// from a mech's weapon list when <paramref name="repairItem"/>.
    /// </summary>
    void ShowComponentInfo(CompInventoryBlock* block, bool repairItem);

    /// <summary>Port: draws <see cref="info"/> into <paramref name="port"/> (the screen's view).</summary>
    void DrawInfo(lPort* port);

    /// <summary>Port: the inventory screen <paramref name="screen"/> is, or null for another screen.</summary>
    static LogInvScreen* Of(aObject* screen);

    /// <summary>Port: <paramref name="source"/> is going: no inventory screen shows its details any more.</summary>
    static void ForgetInfoSource(lObject* source);

    /// <summary>Port: see <see cref="InvInfoBox"/>.</summary>
    InvInfoBox info;

    /// <summary>
    /// Set while the screen's chat button blinks (multiplayer; timer 7 on the purchase screen, 8 on the repair
    /// screen); <c>BriefingScreen::setUpOperation</c> stops it.
    /// </summary>
    int32_t chatBlinking = 0; // +0x4c0
    /// <summary>The inventory pane (left).</summary>
    ScrollPane* inventoryPane = nullptr; // +0x4c4
    /// <summary>The unit pane: the store on the purchase screen, the vehicles on the repair screen.</summary>
    ScrollPane* unitPane = nullptr; // +0x4c8

    /// <summary>Port: what the screen shows of the shared places.</summary>
    LogScreenChrome chrome;
    /// <summary>Port: the background art's file name in <c>logart</c> (<see cref="initLive"/>).</summary>
    const char* backgroundArt = nullptr;
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

    /// <summary>Port: draws the frame along the bottom (the history pane and the input line draw themselves).</summary>
    void draw() override;

    /// <summary>Port: the window draws itself each frame (its port is a view).</summary>
    bool DrawsLive() override { return true; }

    /// <summary>
    /// Port: adds <paramref name="line"/> (text with <c>SMUTI</c> codes) to the bottom of the history, the older lines
    /// moving up by its height (the second half of <see cref="processChatString"/>).
    /// </summary>
    void AddLine(const char* line);

    /// <summary>A line of the history, and the height the text formatter gave it.</summary>
    struct HistoryLine
    {
        std::string Text;
        int32_t Used = 0;
    };

    /// <summary>
    /// Port: draws <paramref name="lines"/> (oldest first) into <paramref name="port"/> (the history's view, open) as
    /// the original's history picture held them: each line was written along the bottom over a wiped strip one row
    /// taller than its text, after the picture had moved up by its height, so a line's bottom row is wiped by the next.
    /// </summary>
    static void DrawHistory(aPort* port, const std::vector<HistoryLine>& lines);

    /// <summary>Port: makes the history pane's content, a view that draws <see cref="lines"/>.</summary>
    lPort* NewHistoryView(int32_t width, int32_t height);

    /// <summary>The history pane.</summary>
    ScrollPane* historyPane = nullptr; // +0x4bc
    /// <summary>The frame picture along the bottom (<c>lsbdw04</c>).</summary>
    lPort* framePort = nullptr; // +0x4c0
    /// <summary>The input line.</summary>
    lChatInput* chatInput = nullptr; // +0x4c4
    /// <summary>The history size given to <see cref="init"/>.</summary>
    int32_t historySize = 0; // +0x4cc

    /// <summary>
    /// Port: the history, oldest first (the original kept it as the pixels of the pane's picture). Lines that moved
    /// off the top are dropped.
    /// </summary>
    std::vector<HistoryLine> lines;
};
