#pragma once

#include "logistics/lport.h"

class aEvent;
class aSmackerWindow;
class BriefingBox;
class LogMech;
class LogVehicle;
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

    /// <summary>Nothing: the screen is drawn piecewise.</summary>
    /// <remarks>MCX.EXE @ 0x006db6d0</remarks>
    void draw() override;

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
    /// <summary>Not seen used.</summary>
    int32_t unknown4E4 = 0; // +0x4e4
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
