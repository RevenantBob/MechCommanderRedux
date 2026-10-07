#pragma once

#include "gui/asystem.h"
#include "gui/aport.h"
#include "lib/cvmath.h"

class MCFloatHelp;
class MCMainWindow;
class MCBaseObject;
class MCGameObject;
class MCMover;
class MCMoverGroup;
class MCParser;
class MCTacticalMap;
class MCTeam;
class MCVector2D;
class MCVector3D;
class MCFriendlyMechIcon;
class MCLanceIcon;

/// <summary>
/// The damage diagram of one mover in the tactical interface: a small window drawing the mover's damage shape
/// (<c>.shp</c>) with each body part tinted by how damaged it is, refreshed at most every 500 ms.
/// </summary>
/// <remarks>Original source: <c>iface\iface.cpp</c>. Base of <see cref="MCFriendlyMechIcon"/>; 0x518 bytes.</remarks>
class MCMechIcon : public MCGuiObject
{
public:
    ~MCMechIcon() override = default;

    /// <summary>Makes the window and its offscreen damage port; every part starts undrawn.</summary>
    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* bitmapName) override;
    /// <summary>Frees the damage port and the damage shapes.</summary>
    void Destroy() override;
    /// <summary>Recolours the parts from the mover's damage and draws them (or the "destroyed" image).</summary>
    void Draw() override;
    /// <summary>Marks this mover as the one the mech bar highlights.</summary>
    void Enter() override;
    /// <summary>Hides the floating tag, clears the mech bar's highlight and resets the cursor.</summary>
    void Leave() override;
    /// <summary>Redraws (at most every 500 ms) while shown.</summary>
    void Display() override;

    /// <summary>Attaches the icon to the mover (or salvage craft) with part id <paramref name="partId"/>.</summary>
    void SetID(int32_t partId);
    /// <summary>Marks every body part as needing (or not needing) a redraw.</summary>
    void SetFullUpdate(int fullUpdate);

    /// <summary>Port: icons draw themselves each frame (see <see cref="DrawIcon"/>).</summary>
    bool DrawsLive() override { return true; }

    /// <summary>
    /// Port: brings what the icon shows up to date with its mover: the part colours (and, for a friendly icon, the
    /// wounded or dead portrait). The original did this as it drew, every 500 ms; it runs every frame now.
    /// </summary>
    virtual void UpdateModel();

    /// <summary>
    /// Port: draws the icon into <paramref name="target"/>: its port's view in the frame pass, or a picture (the
    /// mission results screen copies the icon). What <see cref="Draw"/> drew into the icon's picture.
    /// </summary>
    virtual void DrawIcon(MCGuiPort* target);

protected:
    /// <summary>Draws each body part's shape in its colour (translucent unless undamaged) into <paramref name="target"/>.</summary>
    void DrawParts(MCGuiPort* target);
    /// <summary>Picks each body part's colour from its armour left (green, yellow, orange, red, destroyed).</summary>
    void GetColors();

public:
    /// <summary>The part id of the mover shown.</summary>
    int32_t PartId = 0;
    /// <summary>The mover shown (a Mover, or the salvage craft's object).</summary>
    MCBaseObject* Mover = nullptr;
    /// <summary>Nonzero to draw parts whose <see cref="PartDamaged"/> is set in the flash colour (0x10).</summary>
    int32_t FlashDamage = 0;
    /// <summary>
    /// Each body part's colour code: 0x0b undamaged, 0xf2 light, 0xeb heavy, 0xef critical, 0x19 destroyed, 0xff
    /// not yet set.
    /// </summary>
    uint8_t PartColor[8] = {};
    /// <summary>Set when a part's armour changed since it was last drawn.</summary>
    int32_t PartDamaged[8] = {};
    /// <summary>Set when a part's colour changed and it must be redrawn.</summary>
    int32_t PartDirty[8] = {};
    /// <summary>The number of body parts (the mover's location count).</summary>
    int32_t NumParts = 0;
    /// <summary><c>GetTickCount</c> of the last redraw.</summary>
    uint32_t LastUpdateTime = 0;
    /// <summary>Where the damage diagram is drawn in the window.</summary>
    int32_t DiagramX = 0;
    int32_t DiagramY = 0;
    /// <summary>The "destroyed" image, copied over the diagram once the mover is dead.</summary>
    MCGuiPort* DeadImage = nullptr;
    /// <summary>The damage shapes (one per body part), loaded from <c>artPath</c> (new[]'d, registered with the renderers).</summary>
    void* DamageShapes = nullptr;
};

/// <summary>
/// One of the player's movers on the mech bar: its damage diagram, pilot portrait, pilot health and weapon
/// cooldown bars, and its lance colour. Clicking selects the mover (see <c>mechIconHandleEvent</c>).
/// </summary>
/// <remarks>Original source: <c>iface\iface.cpp</c>, 0x538 bytes.</remarks>
class MCFriendlyMechIcon : public MCMechIcon
{
public:
    ~MCFriendlyMechIcon() override = default;

    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* bitmapName) override;
    void Destroy() override;
    /// <summary>Shows the pilot/mech tag, highlights the mover and sets the cursor for the current command.</summary>
    void Enter() override;
    /// <summary>Weapon bar, damage diagram, lance colour strip, then the pilot (or the mover's name).</summary>
    void Draw() override;
    /// <summary>Only while the mover is active.</summary>
    void Display() override;
    using MCGuiObject::DrawBox;
    /// <summary>Draws the selection frame, unless the mech bar is shuffling its buttons.</summary>
    void DrawBox(uint8_t color, int32_t left, int32_t top, int32_t right, int32_t bottom) override;

    /// <summary>
    /// Attaches the icon to the mover with part id <paramref name="partId"/>: loads its damage shapes
    /// (<c>mi%02i.shp</c> for mechs, <c>vi%i.shp</c> for vehicles) and its pilot portrait.
    /// </summary>
    void SetID(int32_t partId);

    /// <summary>Port: the part colours, then the portrait switched to the wounded or dead image (once each).</summary>
    void UpdateModel() override;
    /// <summary>
    /// Port: the background (<see cref="IconBackground"/>), weapon bar, damage diagram, lance colour strip, then the
    /// pilot (or the mover's name).
    /// </summary>
    void DrawIcon(MCGuiPort* target) override;

protected:
    /// <summary>The pilot portrait, health bar and name, into <paramref name="target"/>.</summary>
    void DrawPilot(MCGuiPort* target);
    /// <summary>The weapon recycle bar, into <paramref name="target"/>.</summary>
    void DrawWeapon(MCGuiPort* target);

public:
    /// <summary>Nonzero once the mover is in play (the icon is shown and counts in its lance).</summary>
    int32_t Active = 0;
    /// <summary>The lance (0-3) the mover belongs to, or 5 for none; indexes <c>lanceColorArray</c>.</summary>
    int32_t Lance = 5;
    /// <summary>The x the icon moves to when the mech bar shuffles its buttons.</summary>
    int32_t TargetX = 0;
    /// <summary>The x step per frame of that shuffle.</summary>
    int32_t ShuffleStep = 0;
    /// <summary>Nonzero when the mover is its lance's point (leader).</summary>
    int32_t IsPoint = 0;
    /// <summary>The pilot portrait.</summary>
    MCGuiPort* PilotImage = nullptr;
    /// <summary>Set once the portrait was switched to the dead pilot image.</summary>
    int32_t ShowingDeadPilot = 0;
    /// <summary>Set once the portrait was switched to the wounded pilot image.</summary>
    int32_t ShowingWoundedPilot = 0;
    /// <summary>
    /// Port: the icon's background (<c>guiub00.tga</c>), which the original loaded into the icon's own picture and
    /// drew over.
    /// </summary>
    MCGuiPort* IconBackground = nullptr;
};

/// <summary>A salvage craft's marker: draws every object in its list at the object's screen position.</summary>
/// <remarks>Original source: <c>iface\iface.cpp</c>. Only <see cref="Display"/> is its own.</remarks>
class MCSalvageIcon : public MCGuiObject
{
public:
    /// <summary>A node of the list of objects marked (<see cref="MCInterfaceObject::AddSalvageIcon"/>).</summary>
    struct SalvageNode
    {
        MCGameObject* Object = nullptr;
        SalvageNode* Next = nullptr;
    };

    /// <summary>Copies the icon's image to the screen position of each visible object in the list.</summary>
    void Display() override;

    /// <summary>The objects marked (newest first).</summary>
    SalvageNode* Objects = nullptr;
};

/// <summary>A lance's badge on the mech bar: its number and one link per active mover, in the lance colour.</summary>
/// <remarks>Original source: <c>iface\iface.cpp</c>, 0x4d0 bytes.</remarks>
class MCLanceIcon : public MCGuiObject
{
public:
    ~MCLanceIcon() override { Destroy(); }

    /// <summary>Makes the badge for lance <paramref name="lanceNumber"/> and loads its five images.</summary>
    int32_t Init(int16_t lanceNumber);
    /// <summary>Frees the five images.</summary>
    void Destroy() override;
    /// <summary>Nothing: everything is drawn in <see cref="Display"/>.</summary>
    void Draw() override {}
    /// <summary>The number, then one link per active mover, then a bar in the lance colour.</summary>
    void Display() override;
    /// <summary>A click selects the lance (shift toggles it) or gives it the current order.</summary>
    void HandleEvent(MCGuiEvent* event) override;
    /// <summary>Shows a floating tag over each active mover of the lance.</summary>
    void Enter() override;
    /// <summary>Hides the floating tags.</summary>
    void Leave() override;

    /// <summary>Shown only while the lance has an active mover.</summary>
    void ShowTest();
    /// <summary>The mech bar icons in this lance that are active.</summary>
    int32_t GetNumActiveMovers();

    /// <summary>The lance's group (<c>HomeCommander</c>'s group of the lance number).</summary>
    MCMoverGroup* Group = nullptr;
    /// <summary>The group's id, which is the lance number compared with <see cref="MCFriendlyMechIcon::Lance"/>.</summary>
    int32_t LanceId = -1;
    /// <summary>The active movers counted when the mech bar last shuffled.</summary>
    int32_t NumActiveMovers = 0;
    /// <summary>The lance number (<c>guiubf%i.tga</c>).</summary>
    MCGuiPort* NumberImage = nullptr;
    /// <summary>The first link (<c>guiub01.tga</c>).</summary>
    MCGuiPort* FirstLinkImage = nullptr;
    /// <summary>The short link (<c>guiub02.tga</c>).</summary>
    MCGuiPort* ShortLinkImage = nullptr;
    /// <summary>The long link, once per further mover (<c>guiub03.tga</c>).</summary>
    MCGuiPort* LongLinkImage = nullptr;
    /// <summary>The end link (<c>guiub04.tga</c>).</summary>
    MCGuiPort* LastLinkImage = nullptr;
    /// <summary>
    /// Set when <see cref="MCInterfaceObject::SetUnit"/> linked the lance, cleared when its point died
    /// (<see cref="MCInterfaceObject::RemoveMech"/>). <see cref="MCInterfaceObject::SelectLance"/> selects a linked
    /// lance as a whole and an unlinked one mover by mover.
    /// </summary>
    int32_t Linked = 0;
};

/// <summary>
/// The layout of the mech bar's buttons. In the original this was a virtual base of <see cref="MCMechBar"/> (reached
/// through the vbtable pointer at +0x4ac, placed at +0x4f4); its class name was lost. The port embeds it.
/// </summary>
struct MCMechBarLayout
{
    /// <summary>How many buttons the bar holds (12).</summary>
    int16_t MaxButtons = 12; // (aMechBar +0x4f4)
    /// <summary>How many buttons it holds now.</summary>
    int32_t NumButtons = 0; // (aMechBar +0x4f8)
    /// <summary>The part id of the mover whose icon the mouse is over (-1 = none).</summary>
    int32_t HighlightId = -1; // (aMechBar +0x4fc)
    /// <summary>The part id of the mover whose pilot is on the video window (-1 = none).</summary>
    int32_t VideoId = -1; // (aMechBar +0x500)
    /// <summary>The gap between buttons.</summary>
    int32_t SpacingX = 0; // (aMechBar +0x508)
    int32_t SpacingY = 0; // (aMechBar +0x50c)

    /// <summary>Sets the gap between buttons.</summary>
    void SetSpacing(int32_t x, int32_t y)
    {
        SpacingX = x;
        SpacingY = y;
    }
};

/// <summary>
/// The bar along the bottom of the tactical screen: one <see cref="MCFriendlyMechIcon"/> per player mover, grouped
/// behind four <see cref="MCLanceIcon"/>s. When lances change the buttons "dance" into their new places.
/// </summary>
/// <remarks>Original source: <c>iface\iface.cpp</c>, 0x514 bytes (including the virtual base, see
/// <see cref="MCMechBarLayout"/>).</remarks>
class MCMechBar : public MCGuiObject
{
public:
    MCMechBar();
    ~MCMechBar() override = default;

    /// <summary>A bar with no bitmap of its own (it draws on its parent).</summary>
    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* bitmapName) override;
    void Destroy() override;
    /// <summary>Draws the children, the lance separators and each button's frame colour.</summary>
    void Display() override;
    /// <summary>Keeps the bar at the bottom of the screen and passes events to the interface.</summary>
    void HandleEvent(MCGuiEvent* event) override;
    void Resize(int32_t width, int32_t height) override;

    /// <summary>Removes every button and lance icon.</summary>
    void CleanUp();
    /// <summary>Appends a button.</summary>
    /// <returns>0, or 0xEEEE0001 (negative) when the bar is full.</returns>
    int32_t AddButton(MCFriendlyMechIcon* button);
    /// <summary>Deletes button <paramref name="index"/> and closes the gap.</summary>
    /// <returns>0, or 0xEEEE0003 (negative) when there is no such button.</returns>
    int32_t RemoveButton(int16_t index);
    /// <summary>Deletes the button of the mover with part id <paramref name="partId"/>.</summary>
    int32_t RemoveButton(uint32_t partId);
    /// <summary>The button of the mover with part id <paramref name="partId"/>, or null.</summary>
    MCFriendlyMechIcon* GetButtonFromID(int32_t partId);
    /// <summary>
    /// Sorts the buttons by lance and places them behind their lance icons; with <paramref name="animate"/> they
    /// move there over several frames (<c>DancingButtons</c>).
    /// </summary>
    void PlaceButtons(int animate);
    /// <summary>Makes the four lance icons.</summary>
    int32_t InitLances();
    void DestroyLances();
    /// <summary>The icon of lance <paramref name="lanceId"/>, or null.</summary>
    MCLanceIcon* GetLanceIconFromID(int32_t lanceId);

    /// <summary>Button <paramref name="index"/>, or null out of range.</summary>
    MCFriendlyMechIcon* GetButton(int16_t index)
    {
        if (index > -1 && index < Layout.MaxButtons)
        {
            return Buttons[index];
        }

        return nullptr;
    }

    // The original had a vbtable pointer here (virtual base aMechBarLayout); not kept.
    /// <summary>Nonzero while the buttons are dancing into place.</summary>
    int32_t Dancing = 0;
    MCFriendlyMechIcon* Buttons[12] = {};
    MCLanceIcon* LanceIcons[4] = {};
    /// <summary>The original's virtual base.</summary>
    MCMechBarLayout Layout;
};

/// <summary>
/// The player's command interface in a mission: the mech bar, the current selection of movers and lances, the
/// command mode, the key bindings and the mouse state over the map. There is one, <c>theInterface</c>.
/// </summary>
/// <remarks>
/// Original source: <c>iface\iface.cpp</c>, 0x28c bytes, no vtable, allocated on the GUI heap.
/// <para>
/// Each key binding in <see cref="Keys"/> packs a key code in bits 0-15 and the modifiers the event must carry:
/// bit 16 shift, bit 20 ctrl, bit 24 alt (from <c>aEvent</c> +0xd, +0xc, +0xb). Assigning a key code keeps bits 16,
/// 20 and 24 (<c>&amp; 0x1110000</c>), which suggests a bitfield struct in the original.
/// </para>
/// </remarks>
class MCInterfaceObject
{
public:
    /// <summary>The key bindings, by their slot in <see cref="Keys"/> (offset - 0x118) / 4.</summary>
    static constexpr int NUM_KEYS = 77;

    MCInterfaceObject();

    /// <summary>
    /// Makes the mech bar and the twelve floating tags, reads <c>iface.fit</c> (drag distance, scroll speeds,
    /// shuffle frames) and sets the default key bindings.
    /// </summary>
    /// <returns>0, 3 when out of memory, or the ini error.</returns>
    int32_t Init();
    void Destroy();

    /// <summary>
    /// Every tactical-screen event: keys (commands, map modes, zoom, groups), clicks and drags on the map and
    /// the order they give, and part-object creation/destruction messages.
    /// </summary>
    void HandleEvent(MCGuiEvent* event);
    /// <summary>
    /// The zoom-in key: the camera's view shows <paramref name="factor"/> times fewer lines of the world (eased), down
    /// to the closest zoom, with the original's sound; the tactical map's zoom button follows. Inline in
    /// <see cref="HandleEvent"/> in the original (which switched the camera between scales 100 and 1); split out so
    /// the mouse wheel can share it.
    /// </summary>
    void ZoomIn(float factor = ZoomKeyStep, bool sound = true);
    /// <summary>The zoom-out key, as <see cref="ZoomIn"/>: <paramref name="factor"/> times more lines. The wheel
    /// zooms without the sound.</summary>
    void ZoomOut(float factor = ZoomKeyStep, bool sound = true);
    /// <summary>How far a zoom key press zooms (about four presses from one end to the other).</summary>
    static constexpr float ZoomKeyStep = 1.5f;
    /// <summary>How far a notch of the mouse wheel zooms.</summary>
    static constexpr float ZoomWheelStep = 1.1f;

    /// <summary>Makes the command parser, starts the scroll callback and the lance icons.</summary>
    int32_t StartScenario();
    /// <summary>Drops the selection, parser, buttons, lance icons and salvage icon.</summary>
    void EndScenario();

    /// <summary>
    /// Makes an icon for the mover with part id <paramref name="partId"/>: on the mech bar in lance
    /// <paramref name="lance"/> when <paramref name="onBar"/>, else kept aside in <see cref="ReserveIcons"/>.
    /// </summary>
    int32_t AddMech(int32_t partId, int32_t lance, int active, int onBar);
    /// <summary>Brings a mover's icon into play and re-places the buttons.</summary>
    void ActivateMech(int32_t partId);
    /// <summary>Drops a (dead) mover from the selection, its lance's selection and the parser.</summary>
    void RemoveMech(int32_t partId);
    /// <summary>Redraws the mech bar.</summary>
    void UpdateInterface();

    int IsSelected(int32_t partId);
    int IsSelected(MCMoverGroup* group);
    void SelectMech(int32_t partId);
    /// <summary>Selects every player mover on screen.</summary>
    void SelectVisibleMechs();
    void DeselectMech(int32_t partId);
    void SelectEnemy(int32_t partId);
    void DeselectEnemy();
    void SelectLance(MCMoverGroup* group);
    void DeselectLance(MCMoverGroup* group);
    void ClearMechSelection();
    /// <summary>Whether part id <paramref name="partId"/> is one of the player's movers.</summary>
    int IsOurs(int16_t partId);
    /// <summary>Nothing (an empty hook).</summary>
    void ObjectAttacked(int32_t partId);

    /// <summary>
    /// Works out what the mouse is over (icon, object, terrain), whether it can be attacked, and sets the cursor
    /// for the current command. <paramref name="event"/> may be null (the per-frame callback).
    /// </summary>
    void UpdateMouseState(MCGuiEvent* event);

    /// <summary>
    /// Calls an artillery strike of <paramref name="strikeType"/> at <paramref name="position"/> (or at
    /// <paramref name="target"/>'s): for the home commander's team when <paramref name="forCommander"/>, else for
    /// the Inner Sphere (or the clans with <paramref name="forClans"/>); in multiplayer it goes over the network.
    /// </summary>
    void CallStrike(int strikeType, MCVector3D* position, MCGameObject* target, int forCommander, int forClans,
                    float delay);
    /// <summary>Adds <paramref name="object"/> to the salvage icon's list.</summary>
    void AddSalvageIcon(MCGameObject* object);
    /// <summary>Hides the twelve floating tags.</summary>
    void HideTags();
    /// <summary>Hides the floating tags for good (their objects may be gone).</summary>
    void WhackTags();
    /// <summary>
    /// Whether anything is selected; with <paramref name="needsCommand"/>, whether a selected mover can take the
    /// current command.
    /// </summary>
    int AnySelected(int needsCommand);
    /// <summary>Makes <paramref name="numMovers"/> movers into group <paramref name="groupId"/> and relinks their icons.</summary>
    void SetUnit(int32_t groupId, int32_t numMovers, MCGameObject** movers, int32_t pointIndex);
    /// <summary>Marks (or unmarks) a mover's icon as its lance's point.</summary>
    void SetPoint(int32_t partId, int isPoint);
    /// <summary>Picks the cursor's direction (<see cref="CursorOffset"/>) from the selection's centre to <paramref name="screenPos"/>.</summary>
    void SetCursorOffset(MCVector2D screenPos);
    /// <summary>Whether the one selected mover, a live refit vehicle, can refit <paramref name="target"/>.</summary>
    int RefitCheck(MCGameObject* target);
    /// <summary>
    /// Whether <paramref name="target"/>, a live repair bay (object type 0x1b) of the player's, can fix the one
    /// selected mover (mech bays fix mechs, vehicle bays vehicles) within 100 units.
    /// </summary>
    int GetFixedCheck(MCGameObject* target);
    /// <summary>The mech bar icon of the mover with part id <paramref name="partId"/>, or null.</summary>
    MCFriendlyMechIcon* GetMechIconFromID(int32_t partId);

    MCParser* GetCommandParser() { return CommandParser; }

    /// <summary>
    /// Whether every selected mover can jump (TacticalMap::updateOrderPalette calls it too). The original name was
    /// lost.
    /// </summary>
    int CanSelectionJump();

private:
    /// <summary>
    /// Whether the selection (movers and lances) can jump to <paramref name="position"/>: passable, not onto a
    /// friendly mover, and within every mover's jump range. The original name was lost.
    /// </summary>
    int CanSelectionJumpTo(MCVector3D position, MCGameObject* target, int fromWayPoint);

public:
    /// <summary>
    /// The world point under the last map click, which <see cref="HandleEvent"/> unprojects into (the original
    /// passes <c>this</c> as the output vector).
    /// </summary>
    MCVector3D MouseWorldPos;
    /// <summary>How many movers are selected (<see cref="SelectedMechs"/>).</summary>
    int16_t NumSelectedMechs = 0;
    /// <summary>The key that rotates the camera while held (0x38, alt).</summary>
    int16_t RotateKey = 0;
    /// <summary>The direction the arrow keys scroll the map (0, 2, 4, 6; -1 = none).</summary>
    int32_t ScrollDirection = -1;
    /// <summary>The direction the ctrl+arrow keys scroll the tactical map (-1 = none).</summary>
    int32_t TacScrollDirection = -1;
    /// <summary>
    /// Nonzero when the current command was picked for one order only: the parser resets
    /// <see cref="CurrentCommand"/> after sending it. Cleared whenever a key picks a command.
    /// </summary>
    int32_t CommandOneShot = 0;
    /// <summary>How many frames the mech bar's buttons take to dance into place (<c>Shuffle Frames</c>, 15).</summary>
    int16_t ShuffleFrames = 0;
    /// <summary>
    /// Nonzero when the object under the mouse can be captured: capturable, in sight, not the player's, and the
    /// command is none or move (<see cref="UpdateMouseState"/>).
    /// </summary>
    int32_t CanCapture = 0;
    /// <summary>Nonzero when that capture is blocked (the object has a capture blocker against the player).</summary>
    int32_t CaptureBlocked = 0;
    /// <summary>The cursor's direction variant (0-6), from <see cref="SetCursorOffset"/>.</summary>
    int32_t CursorOffset = 0;
    MCParser* CommandParser = nullptr;
    MCMechBar* MechBar = nullptr;
    /// <summary>Icons of movers not on the mech bar (<see cref="AddMech"/> with onBar 0).</summary>
    MCFriendlyMechIcon* ReserveIcons[24] = {};
    int32_t NumReserveIcons = 0;
    MCTacticalMap* TacticalMap = nullptr;
    /// <summary>The current command mode (0 none, 1 move, 2 attack, 3 ..., 0x15 jump, 0x16 ..., 0x33 ...).</summary>
    int32_t CurrentCommand = -1;
    /// <summary>How far the mouse must move with a button down to start a drag (<c>Drag Distance</c>).</summary>
    int16_t DragDistance = 10;
    /// <summary><c>Scroll Speed</c>.</summary>
    int16_t ScrollSpeed = 4;
    /// <summary><c>Tac Scroll Speed</c>.</summary>
    int16_t TacScrollSpeed = 1;
    /// <summary><c>Scroll Start</c>: the delay before edge scrolling starts.</summary>
    int16_t ScrollStart = 500;
    /// <summary>Part ids of the selected movers.</summary>
    int32_t SelectedMechs[12] = {};
    /// <summary>The selected lances.</summary>
    MCMoverGroup* SelectedLances[4] = {};
    int16_t NumSelectedLances = 0;
    /// <summary>Where the left button went down (screen), to tell a click from a drag.</summary>
    float MouseDownX = 0.0f;
    float MouseDownY = 0.0f;
    /// <summary>Nonzero while the left button is down on the map.</summary>
    int32_t MouseDown = 0;
    /// <summary>What the mouse is over: 0 an icon, 1 an enemy, ..., 7 nothing (-1 before the first update).</summary>
    int32_t MouseObjectType = -1;
    /// <summary>The object (or icon's mover) under the mouse.</summary>
    MCBaseObject* MouseObject = nullptr;
    /// <summary>The enemy selected as the target.</summary>
    MCGameObject* SelectedEnemy = nullptr;
    /// <summary>The key bindings, packed as described in the class remarks; slot 0 (+0x118) is unused.</summary>
    uint32_t Keys[NUM_KEYS] = {};
    /// <summary>Nonzero while the tactical map's large view is up.</summary>
    int32_t TacMapShown = 0;
    /// <summary>The salvage icon (created elsewhere, deleted by EndScenario).</summary>
    MCSalvageIcon* SalvageIcon = nullptr;
    /// <summary>The floating tags shown over movers.</summary>
    MCFloatHelp* FloatingTags[12] = {};
    /// <summary>
    /// Set while one of the F9-F12 (or ctrl) keys is held: the next click gives a forced order. Meaning inferred.
    /// </summary>
    int32_t ForceOrderActive = 0;
    /// <summary>Which forced order (-1 none, 0, 1, 2 = move along a way path). Meaning inferred.</summary>
    int32_t ForceOrderType = -1;
};

/// <summary>Moves the mech bar's buttons one frame toward their places (an <c>aCallback</c>).</summary>
void DancingButtons();

/// <summary>Each lance's colour, indexed by lance number (5 = no lance).</summary>
extern uint8_t LanceColorArray[];
/// <summary>The floating tag's offset from its mover (20, -10).</summary>
extern int32_t Sx;
extern int32_t Sy;
/// <summary>The slopes that split the cursor directions (<see cref="MCInterfaceObject::SetCursorOffset"/>), 8 floats.</summary>
extern float SlopeTest[];
/// <summary>The tactical screen's main window.</summary>
extern MCMainWindow* MainHolder;
/// <summary>The current phase (0-2) of the mech bar's dance.</summary>
extern uint8_t DanceStep;
/// <summary>Frames since the dance started.</summary>
extern int16_t DanceFrames;
/// <summary>The edge-scrolling callback (<c>ScrollScreen</c>), live during a mission.</summary>
extern MCGuiCallback* ScrollCallback;
/// <summary>The dance callback (<see cref="DancingButtons"/>), live while the buttons move.</summary>
extern MCGuiCallback* MoveCallback;
/// <summary>The object being dragged (a selection box or an icon).</summary>
extern MCGuiObject* DragTarget;
/// <summary>The tactical interface.</summary>
/// <remarks>Defined in iface.cpp by the port; globals_by_file.md places it in terrain\terrmap.cpp.</remarks>
extern MCInterfaceObject* TheInterface;
