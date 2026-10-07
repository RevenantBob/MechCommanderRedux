#pragma once

#include "logistics/lport.h"

class MCGuiEvent;
class MCGuiSmackerWindow;
class MCBriefingBox;
class MCLogMech;
class MCLogVehicle;
class MCMechBriefBlock;
class MCScrollPane;
struct MCSmackTag;

/// <summary>
/// The logistics briefing screen: the operation briefing (a Smacker movie, or the operation picture in
/// multiplayer), the mission briefing text, and the deploy area where the player drags mechs and vehicles into the
/// drop slots under the tonnage limit.
/// </summary>
/// <remarks>Original source: <c>logistics\logbri.cpp</c>, 0x5cc bytes (<c>Logistics</c> +0x4e0).</remarks>
class MCBriefingScreen : public MCLogObject
{
public:
    ~MCBriefingScreen() override { Destroy(); }

    /// <summary>Makes the full-screen object, its scroll panes and ports, and the drop-slot rectangles.</summary>
    void Init();

    /// <summary>Draws the deploy area: the slots, their occupants and the tonnage.</summary>
    void DrawBackground();

    /// <summary>Displays the screen; starts or restarts the operation movie when it is due, or shows the picture after it.</summary>
    void Display() override;

    /// <summary>
    /// Port: draws the screen from its state (<see cref="PaintLook"/>) and the shared places
    /// (<see cref="MCLogScreenChrome"/>) in the frame pass. The original's draw did nothing (the screen was drawn
    /// piecewise into its picture).
    /// </summary>
    void Draw() override;

    /// <summary>Port: the screen draws itself each frame (its port is a view).</summary>
    bool DrawsLive() override { return true; }

    /// <summary>Port: see <see cref="MCLogScreenChrome"/>.</summary>
    MCLogScreenChrome* Chrome() override { return &ScreenChrome; }

    void Destroy() override;

    /// <summary>The multiplayer version of <see cref="CalcTonnages"/> (the team's limits).</summary>
    void MpCalcTonnages();

    /// <summary>Adds up <c>curDeployTonnage</c> from the mechs and vehicles in the drop slots.</summary>
    void CalcTonnages();

    /// <summary>Draws the tonnage bar against <c>maxDeployTonnage</c>.</summary>
    void DrawTonnageBar();

    /// <summary>The tabs (operation, mission, deploy), the launch and screen buttons, drops, cheat keys and timers.</summary>
    void HandleEvent(MCGuiEvent* event) override;

    void ShowGuiWindow(int show) override;

    /// <summary>Shows the operation tab and starts its movie (or, in multiplayer, the chat).</summary>
    void SetUpOperation();

    /// <summary>Shows the mission tab with its briefing text.</summary>
    void SetUpMission();

    /// <summary>Stops and frees the operation movie.</summary>
    void StopSmackerMovies();

    /// <summary>Refills the deploy scroll pane with a block for every undeployed mech and vehicle.</summary>
    void SetUpDeploy();

    /// <summary>Port: takes <paramref name="child"/> off the screen: a unit block leaves its drop slot, a box the box area.</summary>
    void RemoveChild(MCGuiObject* child) override;

    /// <summary>
    /// Port: draws the screen from its state into <paramref name="target"/> (screen coordinates): the art, the map, the
    /// lance labels, the tonnage bar and launch button, the tab, the operation area, the drop slots with their units
    /// and the briefing box. The original painted these into the screen's picture as events changed them.
    /// </summary>
    void PaintLook(MCPane* target);

    /// <summary>Port: a new screen-sized picture holding <see cref="PaintLook"/> (for transitions).</summary>
    MCLogPort* NewLookPicture();

    /// <summary>Port: the drop slot whose top left corner is (<paramref name="xPos"/>, <paramref name="yPos"/>), or -1.</summary>
    int32_t SlotAt(int32_t xPos, int32_t yPos) const;

    /// <summary>Port: <paramref name="block"/> is in the drop slot at its corner: drawn there from now on.</summary>
    void PlaceInSlot(MCMechBriefBlock* block);

    /// <summary>Port: <paramref name="block"/> was picked up out of its slot (which shows empty again).</summary>
    void LiftFromSlot(MCMechBriefBlock* block);

    /// <summary>Port: <paramref name="box"/> shows in the briefing box area.</summary>
    void ShowBox(MCBriefingBox* box);

    /// <summary>Port: the briefing box area shows blank (colour 0x10).</summary>
    void BlankBox();

    /// <summary>Port: lance <paramref name="lance"/>'s tonnage as its label shows it (what calcTonnages counts).</summary>
    int32_t LanceTons(int32_t lance) const;

    /// <summary>How many mechs are waiting to be deployed.</summary>
    int32_t NumUndeployed = 0;
    /// <summary>Their indices in the mech list (<see cref="NumUndeployed"/> entries).</summary>
    int32_t* UndeployedMechs = nullptr;
    /// <summary>Set when the operation movie should (re)start; cleared once it has.</summary>
    int32_t PlayMovie = 0;
    /// <summary>While set, the screen and tab buttons are ignored (multiplayer launch in progress).</summary>
    int32_t ButtonsLocked = 0;
    /// <summary>Set while the chat button blinks (timer 5; multiplayer).</summary>
    int32_t ChatBlinking = 0;
    /// <summary>The chat button's blink phase.</summary>
    int32_t ChatBlinkOn = 0;
    /// <summary>Set while the chat blink timer (id 5) runs (started by <c>Logistics::setUpBriefingScreen</c>).</summary>
    int32_t ChatTimerOn = 0;
    /// <summary>The mission briefing text pane.</summary>
    MCScrollPane* MissionPane = nullptr;
    /// <summary>The port the mission text is formatted into.</summary>
    MCLogPort* MissionPort = nullptr;
    /// <summary>The operation picture (shown when the movie is over).</summary>
    MCLogPort* OperationPicture = nullptr;
    /// <summary>A copy of an empty drop slot, to erase slots with.</summary>
    MCLogPort* EmptySlot = nullptr;
    /// <summary>The chat button's lit picture (<c>lsbdw08</c>).</summary>
    MCLogPort* ChatBlinkPort = nullptr;
    /// <summary>The chat button's normal picture (<c>lsbdw03</c>).</summary>
    MCLogPort* ChatRegularPort = nullptr;
    /// <summary>The operation movie.</summary>
    MCSmackTag* Smacker = nullptr;
    /// <summary>The window the operation movie plays in.</summary>
    MCGuiSmackerWindow* SmackerWindow = nullptr;
    /// <summary>
    /// The briefing box being shown (cleared by <see cref="Init"/>); <c>MechRepairBlock::setInventory</c> in
    /// multiplayer only fills the weapon list of a mech outside the force when it is this one's.
    /// </summary>
    MCBriefingBox* BriefingBox = nullptr;
    /// <summary>The 12 drop slots on the screen, two columns of six (left, top, right, bottom).</summary>
    RECT SlotRects[12] = {};
    /// <summary>The pane of undeployed mech and vehicle blocks.</summary>
    MCScrollPane* DeployPane = nullptr;
    /// <summary>The tab shown: 1 operation, 2 mission, 3 deploy.</summary>
    int32_t CurrentTab = 0;
    /// <summary>Set once the operation movie has been started by <see cref="Display"/>.</summary>
    int32_t MovieStarted = 0;

    /// <summary>Port: what the screen shows of the shared places.</summary>
    MCLogScreenChrome ScreenChrome;

    // Port: the screen's own state that PaintLook draws (the rest is the game's: the drop slots, the tab, the chat
    // blink, the tonnages). drawBackground (a new mission) resets it.

    /// <summary>Port: the mission's tac map picture, turned onto the map area (null before drawBackground).</summary>
    MCLogPort* MapPicture = nullptr;
    /// <summary>Port: the first drop zone marked on the map (multiplayer), or -1.</summary>
    int32_t MarkedZone = -1;
    /// <summary>Port: the map's world units per pixel, for the drop zone markers.</summary>
    float MarkerScale = 0.0f;
    /// <summary>Port: the launch button was clicked: it shows pressed until the release (or while locked).</summary>
    bool LaunchPressed = false;
    /// <summary>
    /// Port: how far the operation area has got: its art shown (<see cref="SetUpOperation"/>), and the operation
    /// picture over it (before the movie). The movie's closing art shows over both once the movie is over.
    /// </summary>
    bool OperationShown = false;
    bool OperationPictureShown = false;
    /// <summary>Port: the block in each drop slot, if any.</summary>
    MCMechBriefBlock* SlotBlocks[12] = {};
    /// <summary>Port: the box shown in the briefing box area (null: the area is blank).</summary>
    MCBriefingBox* BoxShown = nullptr;

private:
    /// <summary>Port: back to the screen's art alone: no units in the slots, no box, no operation art.</summary>
    void ClearLook();

    /// <summary>Port: draws the tonnage bar (what <see cref="DrawTonnageBar"/> painted) into <paramref name="target"/>.</summary>
    static void PaintTonnageBar(MCPane* target, int32_t maxTons, int32_t tons, bool hammerDown);
};

/// <summary>
/// One mech or vehicle in the briefing screen's deploy pane: its picture, name and tonnage; it can be dragged into
/// a drop slot.
/// </summary>
/// <remarks>Original source: <c>logistics\logbri.cpp</c>, 0x4c4 bytes.</remarks>
class MCMechBriefBlock : public MCLogObject
{
public:
    MCMechBriefBlock();
    ~MCMechBriefBlock() override { Destroy(); }

    /// <summary>A block for <paramref name="mech"/> at (<paramref name="xPos"/>, <paramref name="yPos"/>) in <paramref name="parent"/>.</summary>
    void Init(MCLogMech* mech, MCLogObject* parent, int32_t xPos, int32_t yPos);

    /// <summary>A block for <paramref name="vehicle"/> at (<paramref name="xPos"/>, <paramref name="yPos"/>) in <paramref name="parent"/>.</summary>
    void Init(MCLogVehicle* vehicle, MCLogObject* parent, int32_t xPos, int32_t yPos);

    void Destroy() override;

    /// <summary>Drags the block and drops it into a slot (or back), updating the tonnage.</summary>
    void HandleEvent(MCGuiEvent* event) override;

    /// <summary>Draws the unit's picture, name and tonnage.</summary>
    void DrawBackground();

    /// <summary>
    /// Port: draws the block's picture (the unit's picture and name, darkened when another player's, with a bevelled
    /// frame when <paramref name="framed"/>) at (<paramref name="xPos"/>, <paramref name="yPos"/>) in
    /// <paramref name="target"/>: what <see cref="DrawBackground"/> painted into its parent's picture.
    /// </summary>
    void PaintBlock(MCPane* target, int32_t xPos, int32_t yPos, bool framed);

    /// <summary>
    /// Port: draws the drag icon's picture into <paramref name="surface"/> (0x34 x 0x2e): the block framed over the
    /// empty slot when it is in a drop slot, unframed over colour 0x10 in the deploy pane.
    /// </summary>
    void OnBeginDrag(MCLogPort* surface);

    /// <summary>The mech shown, or null for a vehicle.</summary>
    MCLogMech* Mech = nullptr;
    /// <summary>The vehicle shown, or null for a mech.</summary>
    MCLogVehicle* Vehicle = nullptr;
};

/// <summary>The logistics art folder (<c>data\art\</c>...), prefixed to every image name.</summary>
extern char ArtPath[];

/// <summary>Nonzero once the briefing movie (or an intro movie window) has finished.</summary>
extern int MovieOver;

/// <summary>The tonnage of the units in the drop slots.</summary>
extern int32_t CurDeployTonnage;

/// <summary>The mission's drop tonnage limit.</summary>
extern int32_t MaxDeployTonnage;
