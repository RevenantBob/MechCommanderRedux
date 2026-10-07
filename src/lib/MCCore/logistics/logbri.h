#pragma once

#include "logistics/lport.h"

class aEvent;
class aSmackerWindow;
class BriefingBox;
class LogMech;
class LogVehicle;
class MechBriefBlock;
class ScrollPane;
struct SmackTag;

/// <summary>
/// The logistics briefing screen: the operation briefing (a Smacker movie, or the operation picture in
/// multiplayer), the mission briefing text, and the deploy area where the player drags mechs and vehicles into the
/// drop slots under the tonnage limit.
/// </summary>
/// <remarks>Original source: <c>logistics\logbri.cpp</c>, 0x5cc bytes (<c>Logistics</c> +0x4e0).</remarks>
class BriefingScreen : public lObject
{
public:
    /// <remarks>MCX.EXE @ 0x006f0310 (vector deleting destructor)</remarks>
    ~BriefingScreen() override { destroy(); }

    /// <summary>Makes the full-screen object, its scroll panes and ports, and the drop-slot rectangles.</summary>
    /// <remarks>MCX.EXE @ 0x006da4b0</remarks>
    void init();

    /// <summary>Draws the deploy area: the slots, their occupants and the tonnage.</summary>
    /// <remarks>MCX.EXE @ 0x006da9c0</remarks>
    void drawBackground();

    /// <summary>Displays the screen; starts or restarts the operation movie when it is due, or shows the picture after it.</summary>
    /// <remarks>MCX.EXE @ 0x006db5b0</remarks>
    void display() override;

    /// <summary>
    /// Port: draws the screen from its state (<see cref="PaintLook"/>) and the shared places
    /// (<see cref="LogScreenChrome"/>) in the frame pass. The original's draw did nothing (the screen was drawn
    /// piecewise into its picture).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006db6d0</remarks>
    void draw() override;

    /// <summary>Port: the screen draws itself each frame (its port is a view).</summary>
    bool DrawsLive() override { return true; }

    /// <summary>Port: see <see cref="LogScreenChrome"/>.</summary>
    LogScreenChrome* Chrome() override { return &chrome; }

    /// <remarks>MCX.EXE @ 0x006db6e0</remarks>
    void destroy() override;

    /// <summary>The multiplayer version of <see cref="calcTonnages"/> (the team's limits).</summary>
    /// <remarks>MCX.EXE @ 0x006db880</remarks>
    void mpCalcTonnages();

    /// <summary>Adds up <c>curDeployTonnage</c> from the mechs and vehicles in the drop slots.</summary>
    /// <remarks>MCX.EXE @ 0x006dbb10</remarks>
    void calcTonnages();

    /// <summary>Draws the tonnage bar against <c>maxDeployTonnage</c>.</summary>
    /// <remarks>MCX.EXE @ 0x006dbee0</remarks>
    void drawTonnageBar();

    /// <summary>The tabs (operation, mission, deploy), the launch and screen buttons, drops, cheat keys and timers.</summary>
    /// <remarks>MCX.EXE @ 0x006dc270</remarks>
    void handleEvent(aEvent* event) override;

    /// <remarks>MCX.EXE @ 0x006dcef0</remarks>
    void ShowGUIWindow(int show) override;

    /// <summary>Shows the operation tab and starts its movie (or, in multiplayer, the chat).</summary>
    /// <remarks>MCX.EXE @ 0x006dcf30</remarks>
    void setUpOperation();

    /// <summary>Shows the mission tab with its briefing text.</summary>
    /// <remarks>MCX.EXE @ 0x006dd290</remarks>
    void setUpMission();

    /// <summary>Stops and frees the operation movie.</summary>
    /// <remarks>MCX.EXE @ 0x006dd3b0</remarks>
    void StopSmackerMovies();

    /// <summary>Refills the deploy scroll pane with a block for every undeployed mech and vehicle.</summary>
    /// <remarks>MCX.EXE @ 0x006dd410</remarks>
    void setUpDeploy();

    /// <summary>Port: takes <paramref name="child"/> off the screen: a unit block leaves its drop slot, a box the box area.</summary>
    void removeChild(aObject* child) override;

    /// <summary>
    /// Port: draws the screen from its state into <paramref name="target"/> (screen coordinates): the art, the map, the
    /// lance labels, the tonnage bar and launch button, the tab, the operation area, the drop slots with their units
    /// and the briefing box. The original painted these into the screen's picture as events changed them.
    /// </summary>
    void PaintLook(PANE* target);

    /// <summary>Port: a new screen-sized picture holding <see cref="PaintLook"/> (for transitions).</summary>
    lPort* NewLookPicture();

    /// <summary>Port: the drop slot whose top left corner is (<paramref name="xPos"/>, <paramref name="yPos"/>), or -1.</summary>
    int32_t SlotAt(int32_t xPos, int32_t yPos) const;

    /// <summary>Port: <paramref name="block"/> is in the drop slot at its corner: drawn there from now on.</summary>
    void PlaceInSlot(MechBriefBlock* block);

    /// <summary>Port: <paramref name="block"/> was picked up out of its slot (which shows empty again).</summary>
    void LiftFromSlot(MechBriefBlock* block);

    /// <summary>Port: <paramref name="box"/> shows in the briefing box area.</summary>
    void ShowBox(BriefingBox* box);

    /// <summary>Port: the briefing box area shows blank (colour 0x10).</summary>
    void BlankBox();

    /// <summary>Port: lance <paramref name="lance"/>'s tonnage as its label shows it (what calcTonnages counts).</summary>
    int32_t LanceTons(int32_t lance) const;

    /// <summary>How many mechs are waiting to be deployed.</summary>
    int32_t numUndeployed = 0; // +0x4bc
    /// <summary>Their indices in the mech list (<see cref="numUndeployed"/> entries).</summary>
    int32_t* undeployedMechs = nullptr; // +0x4c0
    /// <summary>Set when the operation movie should (re)start; cleared once it has.</summary>
    int32_t playMovie = 0; // +0x4c4
    /// <summary>While set, the screen and tab buttons are ignored (multiplayer launch in progress).</summary>
    int32_t buttonsLocked = 0; // +0x4c8
    /// <summary>Set while the chat button blinks (timer 5; multiplayer).</summary>
    int32_t chatBlinking = 0; // +0x4cc
    /// <summary>The chat button's blink phase.</summary>
    int32_t chatBlinkOn = 0; // +0x4d0
    /// <summary>Set while the chat blink timer (id 5) runs (started by <c>Logistics::setUpBriefingScreen</c>).</summary>
    int32_t chatTimerOn = 0; // +0x4d4
    /// <summary>The mission briefing text pane.</summary>
    ScrollPane* missionPane = nullptr; // +0x4d8
    /// <summary>The port the mission text is formatted into.</summary>
    lPort* missionPort = nullptr; // +0x4dc
    /// <summary>The operation picture (shown when the movie is over).</summary>
    lPort* operationPicture = nullptr; // +0x4e0
    /// <summary>A copy of an empty drop slot, to erase slots with.</summary>
    lPort* emptySlot = nullptr; // +0x4e8
    /// <summary>The chat button's lit picture (<c>lsbdw08</c>).</summary>
    lPort* chatBlinkPort = nullptr; // +0x4ec
    /// <summary>The chat button's normal picture (<c>lsbdw03</c>).</summary>
    lPort* chatRegularPort = nullptr; // +0x4f0
    /// <summary>The operation movie.</summary>
    SmackTag* smacker = nullptr; // +0x4f4
    /// <summary>The window the operation movie plays in.</summary>
    aSmackerWindow* smackerWindow = nullptr; // +0x4f8
    /// <summary>
    /// The briefing box being shown (cleared by <see cref="init"/>); <c>MechRepairBlock::setInventory</c> in
    /// multiplayer only fills the weapon list of a mech outside the force when it is this one's.
    /// </summary>
    BriefingBox* briefingBox = nullptr; // +0x4fc
    /// <summary>The 12 drop slots on the screen, two columns of six (left, top, right, bottom).</summary>
    RECT slotRects[12] = {}; // +0x500
    /// <summary>The pane of undeployed mech and vehicle blocks.</summary>
    ScrollPane* deployPane = nullptr; // +0x5c0
    /// <summary>The tab shown: 1 operation, 2 mission, 3 deploy.</summary>
    int32_t currentTab = 0; // +0x5c4
    /// <summary>Set once the operation movie has been started by <see cref="display"/>.</summary>
    int32_t movieStarted = 0; // +0x5c8

    /// <summary>Port: what the screen shows of the shared places.</summary>
    LogScreenChrome chrome;

    // Port: the screen's own state that PaintLook draws (the rest is the game's: the drop slots, the tab, the chat
    // blink, the tonnages). drawBackground (a new mission) resets it.

    /// <summary>Port: the mission's tac map picture, turned onto the map area (null before drawBackground).</summary>
    lPort* mapPicture = nullptr;
    /// <summary>Port: the first drop zone marked on the map (multiplayer), or -1.</summary>
    int32_t markedZone = -1;
    /// <summary>Port: the map's world units per pixel, for the drop zone markers.</summary>
    float markerScale = 0.0f;
    /// <summary>Port: the launch button was clicked: it shows pressed until the release (or while locked).</summary>
    bool launchPressed = false;
    /// <summary>
    /// Port: how far the operation area has got: its art shown (<see cref="setUpOperation"/>), and the operation
    /// picture over it (before the movie). The movie's closing art shows over both once the movie is over.
    /// </summary>
    bool operationShown = false;
    bool operationPictureShown = false;
    /// <summary>Port: the block in each drop slot, if any.</summary>
    MechBriefBlock* slotBlocks[12] = {};
    /// <summary>Port: the box shown in the briefing box area (null: the area is blank).</summary>
    BriefingBox* boxShown = nullptr;

private:
    /// <summary>Port: back to the screen's art alone: no units in the slots, no box, no operation art.</summary>
    void ClearLook();

    /// <summary>Port: draws the tonnage bar (what <see cref="drawTonnageBar"/> painted) into <paramref name="target"/>.</summary>
    static void PaintTonnageBar(PANE* target, int32_t maxTons, int32_t tons, bool hammerDown);
};

/// <summary>
/// One mech or vehicle in the briefing screen's deploy pane: its picture, name and tonnage; it can be dragged into
/// a drop slot.
/// </summary>
/// <remarks>Original source: <c>logistics\logbri.cpp</c>, 0x4c4 bytes.</remarks>
class MechBriefBlock : public lObject
{
public:
    /// <remarks>MCX.EXE @ 0x006dd760</remarks>
    MechBriefBlock();
    /// <remarks>MCX.EXE @ 0x006dd780 (vector deleting destructor)</remarks>
    ~MechBriefBlock() override { destroy(); }

    /// <summary>A block for <paramref name="mech"/> at (<paramref name="xPos"/>, <paramref name="yPos"/>) in <paramref name="parent"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006dd7c0</remarks>
    void init(LogMech* mech, lObject* parent, int32_t xPos, int32_t yPos);

    /// <summary>A block for <paramref name="vehicle"/> at (<paramref name="xPos"/>, <paramref name="yPos"/>) in <paramref name="parent"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006dd810</remarks>
    void init(LogVehicle* vehicle, lObject* parent, int32_t xPos, int32_t yPos);

    /// <remarks>MCX.EXE @ 0x006dd860</remarks>
    void destroy() override;

    /// <summary>Drags the block and drops it into a slot (or back), updating the tonnage.</summary>
    /// <remarks>MCX.EXE @ 0x006dd880 (unnamed in the symbols; it is vtable slot 21, <c>handleEvent</c>).</remarks>
    void handleEvent(aEvent* event) override;

    /// <summary>Draws the unit's picture, name and tonnage.</summary>
    /// <remarks>MCX.EXE @ 0x006deb90</remarks>
    void drawBackground();

    /// <summary>
    /// Port: draws the block's picture (the unit's picture and name, darkened when another player's, with a bevelled
    /// frame when <paramref name="framed"/>) at (<paramref name="xPos"/>, <paramref name="yPos"/>) in
    /// <paramref name="target"/>: what <see cref="drawBackground"/> painted into its parent's picture.
    /// </summary>
    void PaintBlock(PANE* target, int32_t xPos, int32_t yPos, bool framed);

    /// <summary>
    /// Port: draws the drag icon's picture into <paramref name="surface"/> (0x34 x 0x2e): the block framed over the
    /// empty slot when it is in a drop slot, unframed over colour 0x10 in the deploy pane.
    /// </summary>
    void OnBeginDrag(lPort* surface);

    /// <summary>The mech shown, or null for a vehicle.</summary>
    LogMech* mech = nullptr; // +0x4bc
    /// <summary>The vehicle shown, or null for a mech.</summary>
    LogVehicle* vehicle = nullptr; // +0x4c0
};

/// <summary>The logistics art folder (<c>data\art\</c>...), prefixed to every image name.</summary>
extern char artPath[];

/// <summary>Nonzero once the briefing movie (or an intro movie window) has finished.</summary>
extern int movieOver;

/// <summary>The tonnage of the units in the drop slots.</summary>
extern int32_t curDeployTonnage;

/// <summary>The mission's drop tonnage limit.</summary>
extern int32_t maxDeployTonnage;
