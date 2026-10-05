#pragma once

#include "gui/asystem.h"
#include "gui/aport.h"
#include "lib/cvmath.h"

class aFloatHelp;
class aMainWindow;
class BaseObject;
class GameObject;
class Mover;
class MoverGroup;
class Parser;
class TacticalMap;
class Team;
class vector_2d;
class vector_3d;
class FriendlyMechIcon;
class LanceIcon;

/// <summary>
/// The damage diagram of one mover in the tactical interface: a small window drawing the mover's damage shape
/// (<c>.shp</c>) with each body part tinted by how damaged it is, refreshed at most every 500 ms.
/// </summary>
/// <remarks>Original source: <c>iface\iface.cpp</c>. Base of <see cref="FriendlyMechIcon"/>; 0x518 bytes.</remarks>
class aMechIcon : public aObject
{
public:
    /// <remarks>MCX.EXE @ 0x006d0570 (vector deleting destructor)</remarks>
    ~aMechIcon() override = default;

    /// <summary>Makes the window and its offscreen damage port; every part starts undrawn.</summary>
    /// <remarks>MCX.EXE @ 0x006c8c60</remarks>
    int32_t init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* bitmapName) override;
    /// <summary>Frees the damage port and the damage shapes.</summary>
    /// <remarks>MCX.EXE @ 0x006c8d50</remarks>
    void destroy() override;
    /// <summary>Recolours the parts from the mover's damage and draws them (or the "destroyed" image).</summary>
    /// <remarks>MCX.EXE @ 0x006c8db0</remarks>
    void draw() override;
    /// <summary>Marks this mover as the one the mech bar highlights.</summary>
    /// <remarks>MCX.EXE @ 0x006c8e10</remarks>
    void enter() override;
    /// <summary>Hides the floating tag, clears the mech bar's highlight and resets the cursor.</summary>
    /// <remarks>MCX.EXE @ 0x006c8e40</remarks>
    void leave() override;
    /// <summary>Redraws (at most every 500 ms) while shown.</summary>
    /// <remarks>MCX.EXE @ 0x006c9310</remarks>
    void display() override;

    /// <summary>Attaches the icon to the mover (or salvage craft) with part id <paramref name="partId"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006c9360</remarks>
    void SetID(int32_t partId);
    /// <summary>Marks every body part as needing (or not needing) a redraw.</summary>
    /// <remarks>MCX.EXE @ 0x006c9430</remarks>
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
    /// mission results screen copies the icon). What <see cref="draw"/> drew into the icon's picture.
    /// </summary>
    virtual void DrawIcon(aPort* target);

protected:
    /// <summary>Draws each body part's shape in its colour (translucent unless undamaged) into <paramref name="target"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006c8ea0</remarks>
    void DrawParts(aPort* target);
    /// <summary>Picks each body part's colour from its armour left (green, yellow, orange, red, destroyed).</summary>
    /// <remarks>MCX.EXE @ 0x006c9150</remarks>
    void GetColors();

public:
    /// <summary>The part id of the mover shown.</summary>
    int32_t partId = 0; // +0x4ac
    /// <summary>The mover shown (a Mover, or the salvage craft's object).</summary>
    BaseObject* mover = nullptr; // +0x4b0
    /// <summary>Nonzero to draw parts whose <see cref="partDamaged"/> is set in the flash colour (0x10).</summary>
    int32_t flashDamage = 0; // +0x4b4
    /// <summary>
    /// Each body part's colour code: 0x0b undamaged, 0xf2 light, 0xeb heavy, 0xef critical, 0x19 destroyed, 0xff
    /// not yet set.
    /// </summary>
    uint8_t partColor[8] = {}; // +0x4b8
    /// <summary>Set when a part's armour changed since it was last drawn.</summary>
    int32_t partDamaged[8] = {}; // +0x4c0
    /// <summary>Set when a part's colour changed and it must be redrawn.</summary>
    int32_t partDirty[8] = {}; // +0x4e0
    /// <summary>The number of body parts (the mover's location count).</summary>
    int32_t numParts = 0; // +0x500
    /// <summary><c>GetTickCount</c> of the last redraw.</summary>
    uint32_t lastUpdateTime = 0; // +0x504
    /// <summary>Where the damage diagram is drawn in the window.</summary>
    int32_t diagramX = 0; // +0x508
    int32_t diagramY = 0; // +0x50c
    /// <summary>The "destroyed" image, copied over the diagram once the mover is dead.</summary>
    aPort* deadImage = nullptr; // +0x510
    /// <summary>The damage shapes (one per body part), loaded from <c>artPath</c> (new[]'d, registered with the renderers).</summary>
    void* damageShapes = nullptr; // +0x514
};

/// <summary>
/// One of the player's movers on the mech bar: its damage diagram, pilot portrait, pilot health and weapon
/// cooldown bars, and its lance colour. Clicking selects the mover (see <c>mechIconHandleEvent</c>).
/// </summary>
/// <remarks>Original source: <c>iface\iface.cpp</c>, 0x538 bytes.</remarks>
class FriendlyMechIcon : public aMechIcon
{
public:
    /// <remarks>MCX.EXE @ 0x006d0530 (vector deleting destructor)</remarks>
    ~FriendlyMechIcon() override = default;

    /// <remarks>MCX.EXE @ 0x006c9450</remarks>
    int32_t init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* bitmapName) override;
    /// <remarks>MCX.EXE @ 0x006c94f0</remarks>
    void destroy() override;
    /// <summary>Shows the pilot/mech tag, highlights the mover and sets the cursor for the current command.</summary>
    /// <remarks>MCX.EXE @ 0x006c9530</remarks>
    void enter() override;
    /// <summary>Weapon bar, damage diagram, lance colour strip, then the pilot (or the mover's name).</summary>
    /// <remarks>MCX.EXE @ 0x006c97b0</remarks>
    void draw() override;
    /// <summary>Only while the mover is active.</summary>
    /// <remarks>MCX.EXE @ 0x006c9830</remarks>
    void display() override;
    using aObject::drawBox;
    /// <summary>Draws the selection frame, unless the mech bar is shuffling its buttons.</summary>
    /// <remarks>MCX.EXE @ 0x006c9840</remarks>
    void drawBox(uint8_t color, int32_t left, int32_t top, int32_t right, int32_t bottom) override;

    /// <summary>
    /// Attaches the icon to the mover with part id <paramref name="partId"/>: loads its damage shapes
    /// (<c>mi%02i.shp</c> for mechs, <c>vi%i.shp</c> for vehicles) and its pilot portrait.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006c9bf0</remarks>
    void SetID(int32_t partId);

    /// <summary>Port: the part colours, then the portrait switched to the wounded or dead image (once each).</summary>
    void UpdateModel() override;
    /// <summary>
    /// Port: the background (<see cref="iconBackground"/>), weapon bar, damage diagram, lance colour strip, then the
    /// pilot (or the mover's name).
    /// </summary>
    void DrawIcon(aPort* target) override;

protected:
    /// <summary>The pilot portrait, health bar and name, into <paramref name="target"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006c9920</remarks>
    void DrawPilot(aPort* target);
    /// <summary>The weapon recycle bar, into <paramref name="target"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006c9a90</remarks>
    void DrawWeapon(aPort* target);

public:
    /// <summary>Nonzero once the mover is in play (the icon is shown and counts in its lance).</summary>
    int32_t active = 0; // +0x518
    /// <summary>The lance (0-3) the mover belongs to, or 5 for none; indexes <c>lanceColorArray</c>.</summary>
    int32_t lance = 5; // +0x51c
    /// <summary>The x the icon moves to when the mech bar shuffles its buttons.</summary>
    int32_t targetX = 0; // +0x520
    /// <summary>The x step per frame of that shuffle.</summary>
    int32_t shuffleStep = 0; // +0x524
    /// <summary>Nonzero when the mover is its lance's point (leader).</summary>
    int32_t isPoint = 0; // +0x528
    /// <summary>The pilot portrait.</summary>
    aPort* pilotImage = nullptr; // +0x52c
    /// <summary>Set once the portrait was switched to the dead pilot image.</summary>
    int32_t showingDeadPilot = 0; // +0x530
    /// <summary>Set once the portrait was switched to the wounded pilot image.</summary>
    int32_t showingWoundedPilot = 0; // +0x534
    /// <summary>
    /// Port: the icon's background (<c>guiub00.tga</c>), which the original loaded into the icon's own picture and
    /// drew over.
    /// </summary>
    aPort* iconBackground = nullptr;
};

/// <summary>A salvage craft's marker: draws every object in its list at the object's screen position.</summary>
/// <remarks>Original source: <c>iface\iface.cpp</c>. Only <see cref="display"/> is its own.</remarks>
class aSalvageIcon : public aObject
{
public:
    /// <summary>A node of the list of objects marked (<see cref="InterfaceObject::AddSalvageIcon"/>).</summary>
    struct SalvageNode
    {
        GameObject* object = nullptr; // +0x0
        SalvageNode* next = nullptr;  // +0x4
    };

    /// <summary>Copies the icon's image to the screen position of each visible object in the list.</summary>
    /// <remarks>MCX.EXE @ 0x006c9e10</remarks>
    void display() override;

    /// <summary>The objects marked (newest first).</summary>
    SalvageNode* objects = nullptr; // +0x4ac
};

/// <summary>A lance's badge on the mech bar: its number and one link per active mover, in the lance colour.</summary>
/// <remarks>Original source: <c>iface\iface.cpp</c>, 0x4d0 bytes.</remarks>
class LanceIcon : public aObject
{
public:
    /// <remarks>MCX.EXE @ 0x006caee0 (vector deleting destructor)</remarks>
    ~LanceIcon() override { destroy(); }

    /// <summary>Makes the badge for lance <paramref name="lanceNumber"/> and loads its five images.</summary>
    /// <remarks>MCX.EXE @ 0x006caf90</remarks>
    int32_t init(int16_t lanceNumber);
    /// <summary>Frees the five images.</summary>
    /// <remarks>MCX.EXE @ 0x006cb1d0</remarks>
    void destroy() override;
    /// <summary>Nothing: everything is drawn in <see cref="display"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006caed0</remarks>
    void draw() override {}
    /// <summary>The number, then one link per active mover, then a bar in the lance colour.</summary>
    /// <remarks>MCX.EXE @ 0x006cb2c0</remarks>
    void display() override;
    /// <summary>A click selects the lance (shift toggles it) or gives it the current order.</summary>
    /// <remarks>MCX.EXE @ 0x006cb400</remarks>
    void handleEvent(aEvent* event) override;
    /// <summary>Shows a floating tag over each active mover of the lance.</summary>
    /// <remarks>MCX.EXE @ 0x006cb640</remarks>
    void enter() override;
    /// <summary>Hides the floating tags.</summary>
    /// <remarks>MCX.EXE @ 0x006cb770</remarks>
    void leave() override;

    /// <summary>Shown only while the lance has an active mover.</summary>
    /// <remarks>MCX.EXE @ 0x006cb790</remarks>
    void ShowTest();
    /// <summary>The mech bar icons in this lance that are active.</summary>
    /// <remarks>MCX.EXE @ 0x006cb7d0</remarks>
    int32_t getNumActiveMovers();

    /// <summary>The lance's group (<c>HomeCommander</c>'s group of the lance number).</summary>
    MoverGroup* group = nullptr; // +0x4ac
    /// <summary>The group's id, which is the lance number compared with <see cref="FriendlyMechIcon::lance"/>.</summary>
    int32_t lanceId = -1; // +0x4b0
    /// <summary>The active movers counted when the mech bar last shuffled.</summary>
    int32_t numActiveMovers = 0; // +0x4b4
    /// <summary>The lance number (<c>guiubf%i.tga</c>).</summary>
    aPort* numberImage = nullptr; // +0x4b8
    /// <summary>The first link (<c>guiub01.tga</c>).</summary>
    aPort* firstLinkImage = nullptr; // +0x4bc
    /// <summary>The short link (<c>guiub02.tga</c>).</summary>
    aPort* shortLinkImage = nullptr; // +0x4c0
    /// <summary>The long link, once per further mover (<c>guiub03.tga</c>).</summary>
    aPort* longLinkImage = nullptr; // +0x4c4
    /// <summary>The end link (<c>guiub04.tga</c>).</summary>
    aPort* lastLinkImage = nullptr; // +0x4c8
    /// <summary>Cleared by InterfaceObject::RemoveMech, set by SelectLance and setUnit; meaning unconfirmed.</summary>
    int32_t unknown4CC = 0; // +0x4cc
};

/// <summary>
/// The layout of the mech bar's buttons. In the original this was a virtual base of <see cref="aMechBar"/> (reached
/// through the vbtable pointer at +0x4ac, placed at +0x4f4); its class name was lost. The port embeds it.
/// </summary>
struct aMechBarLayout
{
    /// <summary>How many buttons the bar holds (12).</summary>
    int16_t maxButtons = 12; // +0x0 (aMechBar +0x4f4)
    /// <summary>How many buttons it holds now.</summary>
    int32_t numButtons = 0; // +0x4 (aMechBar +0x4f8)
    /// <summary>The part id of the mover whose icon the mouse is over (-1 = none).</summary>
    int32_t highlightId = -1; // +0x8 (aMechBar +0x4fc)
    /// <summary>The part id of the mover whose pilot is on the video window (-1 = none).</summary>
    int32_t videoId = -1; // +0xc (aMechBar +0x500)
    /// <summary>Set to -1 by the constructor; no other use found.</summary>
    int32_t unknown10 = -1; // +0x10 (aMechBar +0x504)
    /// <summary>The gap between buttons.</summary>
    int32_t spacingX = 0; // +0x14 (aMechBar +0x508)
    int32_t spacingY = 0; // +0x18 (aMechBar +0x50c)
    /// <summary>Set to 1 by the constructor; no other use found.</summary>
    int32_t unknown1C = 1; // +0x1c (aMechBar +0x510)

    /// <summary>Sets the gap between buttons.</summary>
    /// <remarks>MCX.EXE @ 0x006c9e90 (an inline whose name was lost)</remarks>
    void setSpacing(int32_t x, int32_t y)
    {
        spacingX = x;
        spacingY = y;
    }
};

/// <summary>
/// The bar along the bottom of the tactical screen: one <see cref="FriendlyMechIcon"/> per player mover, grouped
/// behind four <see cref="LanceIcon"/>s. When lances change the buttons "dance" into their new places.
/// </summary>
/// <remarks>Original source: <c>iface\iface.cpp</c>, 0x514 bytes (including the virtual base, see
/// <see cref="aMechBarLayout"/>).</remarks>
class aMechBar : public aObject
{
public:
    /// <remarks>MCX.EXE @ 0x006c9eb0</remarks>
    aMechBar();
    /// <remarks>MCX.EXE @ 0x006c9fb0 (vector deleting destructor)</remarks>
    ~aMechBar() override = default;

    /// <summary>A bar with no bitmap of its own (it draws on its parent).</summary>
    /// <remarks>MCX.EXE @ 0x006c9fe0</remarks>
    int32_t init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* bitmapName) override;
    /// <remarks>MCX.EXE @ 0x006ca070</remarks>
    void destroy() override;
    /// <summary>Draws the children, the lance separators and each button's frame colour.</summary>
    /// <remarks>MCX.EXE @ 0x006ca090</remarks>
    void display() override;
    /// <summary>Keeps the bar at the bottom of the screen and passes events to the interface.</summary>
    /// <remarks>MCX.EXE @ 0x006ca290</remarks>
    void handleEvent(aEvent* event) override;
    /// <remarks>MCX.EXE @ 0x006ca2f0</remarks>
    void resize(int32_t width, int32_t height) override;

    /// <summary>Removes every button and lance icon.</summary>
    /// <remarks>MCX.EXE @ 0x006ca330</remarks>
    void cleanUp();
    /// <summary>Appends a button.</summary>
    /// <returns>0, or 0xEEEE0001 (negative) when the bar is full.</returns>
    /// <remarks>MCX.EXE @ 0x006ca390</remarks>
    int32_t AddButton(FriendlyMechIcon* button);
    /// <summary>Deletes button <paramref name="index"/> and closes the gap.</summary>
    /// <returns>0, or 0xEEEE0003 (negative) when there is no such button.</returns>
    /// <remarks>MCX.EXE @ 0x006ca420</remarks>
    int32_t RemoveButton(int16_t index);
    /// <summary>Deletes the button of the mover with part id <paramref name="partId"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006ca4e0</remarks>
    int32_t RemoveButton(uint32_t partId);
    /// <summary>The button of the mover with part id <paramref name="partId"/>, or null.</summary>
    /// <remarks>MCX.EXE @ 0x006ca590</remarks>
    FriendlyMechIcon* GetButtonFromID(int32_t partId);
    /// <summary>
    /// Sorts the buttons by lance and places them behind their lance icons; with <paramref name="animate"/> they
    /// move there over several frames (<c>DancingButtons</c>).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006caba0</remarks>
    void PlaceButtons(int animate);
    /// <summary>Makes the four lance icons.</summary>
    /// <remarks>MCX.EXE @ 0x006cae40</remarks>
    int32_t InitLances();
    /// <remarks>MCX.EXE @ 0x006caf10</remarks>
    void DestroyLances();
    /// <summary>The icon of lance <paramref name="lanceId"/>, or null.</summary>
    /// <remarks>MCX.EXE @ 0x006caf50</remarks>
    LanceIcon* GetLanceIconFromID(int32_t lanceId);

    /// <summary>Button <paramref name="index"/>, or null out of range.</summary>
    /// <remarks>MCX.EXE @ 0x006ca550 (an inline whose name was lost)</remarks>
    FriendlyMechIcon* getButton(int16_t index)
    {
        if (index > -1 && index < layout.maxButtons)
        {
            return buttons[index];
        }

        return nullptr;
    }

    // +0x4ac: the original's vbtable pointer (virtual base aMechBarLayout); not kept.
    /// <summary>Nonzero while the buttons are dancing into place.</summary>
    int32_t dancing = 0;                // +0x4b0
    FriendlyMechIcon* buttons[12] = {}; // +0x4b4
    LanceIcon* lanceIcons[4] = {};      // +0x4e4
    /// <summary>The original's virtual base.</summary>
    aMechBarLayout layout; // +0x4f4
};

/// <summary>
/// The player's command interface in a mission: the mech bar, the current selection of movers and lances, the
/// command mode, the key bindings and the mouse state over the map. There is one, <c>theInterface</c>.
/// </summary>
/// <remarks>
/// Original source: <c>iface\iface.cpp</c>, 0x28c bytes, no vtable, allocated on the GUI heap.
/// <para>
/// Each key binding in <see cref="keys"/> packs a key code in bits 0-15 and the modifiers the event must carry:
/// bit 16 shift, bit 20 ctrl, bit 24 alt (from <c>aEvent</c> +0xd, +0xc, +0xb). Assigning a key code keeps bits 16,
/// 20 and 24 (<c>&amp; 0x1110000</c>), which suggests a bitfield struct in the original.
/// </para>
/// </remarks>
class InterfaceObject
{
public:
    /// <summary>The key bindings, by their slot in <see cref="keys"/> (offset - 0x118) / 4.</summary>
    static constexpr int NUM_KEYS = 77;

    /// <remarks>MCX.EXE @ 0x006cb820</remarks>
    InterfaceObject();

    /// <summary>
    /// Makes the mech bar and the twelve floating tags, reads <c>iface.fit</c> (drag distance, scroll speeds,
    /// shuffle frames) and sets the default key bindings.
    /// </summary>
    /// <returns>0, 3 when out of memory, or the ini error.</returns>
    /// <remarks>MCX.EXE @ 0x006cb920</remarks>
    int32_t init();
    /// <remarks>MCX.EXE @ 0x006cc450</remarks>
    void destroy();

    /// <summary>
    /// Every tactical-screen event: keys (commands, map modes, zoom, groups), clicks and drags on the map and
    /// the order they give, and part-object creation/destruction messages.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006cc4c0</remarks>
    void handleEvent(aEvent* event);
    /// <summary>
    /// The zoom-in key: the camera's view shows <paramref name="factor"/> times fewer lines of the world (eased), down
    /// to the closest zoom, with the original's sound; the tactical map's zoom button follows. Inline in
    /// <see cref="handleEvent"/> in the original (which switched the camera between scales 100 and 1); split out so
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
    /// <remarks>MCX.EXE @ 0x006d01c0</remarks>
    int32_t StartScenario();
    /// <summary>Drops the selection, parser, buttons, lance icons and salvage icon.</summary>
    /// <remarks>MCX.EXE @ 0x006d0320</remarks>
    void EndScenario();

    /// <summary>
    /// Makes an icon for the mover with part id <paramref name="partId"/>: on the mech bar in lance
    /// <paramref name="lance"/> when <paramref name="onBar"/>, else kept aside in <see cref="reserveIcons"/>.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006d0450</remarks>
    int32_t AddMech(int32_t partId, int32_t lance, int active, int onBar);
    /// <summary>Brings a mover's icon into play and re-places the buttons.</summary>
    /// <remarks>MCX.EXE @ 0x006d05a0</remarks>
    void ActivateMech(int32_t partId);
    /// <summary>Drops a (dead) mover from the selection, its lance's selection and the parser.</summary>
    /// <remarks>MCX.EXE @ 0x006d05f0</remarks>
    void RemoveMech(int32_t partId);
    /// <summary>Redraws the mech bar.</summary>
    /// <remarks>MCX.EXE @ 0x006d0780</remarks>
    void UpdateInterface();

    /// <remarks>MCX.EXE @ 0x006d0790</remarks>
    int IsSelected(int32_t partId);
    /// <remarks>MCX.EXE @ 0x006d07d0</remarks>
    int IsSelected(MoverGroup* group);
    /// <remarks>MCX.EXE @ 0x006d0810</remarks>
    void SelectMech(int32_t partId);
    /// <summary>Selects every player mover on screen.</summary>
    /// <remarks>MCX.EXE @ 0x006d08a0</remarks>
    void SelectVisibleMechs();
    /// <remarks>MCX.EXE @ 0x006d09c0</remarks>
    void DeselectMech(int32_t partId);
    /// <remarks>MCX.EXE @ 0x006d0b10</remarks>
    void SelectEnemy(int32_t partId);
    /// <remarks>MCX.EXE @ 0x006d0b70</remarks>
    void DeselectEnemy();
    /// <remarks>MCX.EXE @ 0x006d0bb0</remarks>
    void SelectLance(MoverGroup* group);
    /// <remarks>MCX.EXE @ 0x006d0cc0</remarks>
    void DeselectLance(MoverGroup* group);
    /// <remarks>MCX.EXE @ 0x006d0d70</remarks>
    void ClearMechSelection();
    /// <summary>Whether part id <paramref name="partId"/> is one of the player's movers.</summary>
    /// <remarks>MCX.EXE @ 0x006d0e30</remarks>
    int IsOurs(int16_t partId);
    /// <summary>Nothing (an empty hook).</summary>
    /// <remarks>MCX.EXE @ 0x006d0ea0</remarks>
    void ObjectAttacked(int32_t partId);

    /// <summary>
    /// Works out what the mouse is over (icon, object, terrain), whether it can be attacked, and sets the cursor
    /// for the current command. <paramref name="event"/> may be null (the per-frame callback).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006d0eb0</remarks>
    void UpdateMouseState(aEvent* event);

    /// <summary>
    /// Calls an artillery strike of <paramref name="strikeType"/> at <paramref name="position"/> (or at
    /// <paramref name="target"/>'s): for the home commander's team when <paramref name="forCommander"/>, else for
    /// the Inner Sphere (or the clans with <paramref name="forClans"/>); in multiplayer it goes over the network.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006d2990</remarks>
    void CallStrike(int strikeType, vector_3d* position, GameObject* target, int forCommander, int forClans,
                    float delay);
    /// <summary>Adds <paramref name="object"/> to the salvage icon's list.</summary>
    /// <remarks>MCX.EXE @ 0x006d2b60</remarks>
    void AddSalvageIcon(GameObject* object);
    /// <summary>Hides the twelve floating tags.</summary>
    /// <remarks>MCX.EXE @ 0x006d2ba0</remarks>
    void HideTags();
    /// <summary>Hides the floating tags for good (their objects may be gone).</summary>
    /// <remarks>MCX.EXE @ 0x006d2bd0</remarks>
    void WhackTags();
    /// <summary>
    /// Whether anything is selected; with <paramref name="needsCommand"/>, whether a selected mover can take the
    /// current command.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006d2f70</remarks>
    int AnySelected(int needsCommand);
    /// <summary>Makes <paramref name="numMovers"/> movers into group <paramref name="groupId"/> and relinks their icons.</summary>
    /// <remarks>MCX.EXE @ 0x006d3070</remarks>
    void setUnit(int32_t groupId, int32_t numMovers, GameObject** movers, int32_t pointIndex);
    /// <summary>Marks (or unmarks) a mover's icon as its lance's point.</summary>
    /// <remarks>MCX.EXE @ 0x006d3180</remarks>
    void setPoint(int32_t partId, int isPoint);
    /// <summary>Picks the cursor's direction (<see cref="cursorOffset"/>) from the selection's centre to <paramref name="screenPos"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006d31b0</remarks>
    void setCursorOffset(vector_2d screenPos);
    /// <summary>Whether the one selected mover, a live refit vehicle, can refit <paramref name="target"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006d3330</remarks>
    int refitCheck(GameObject* target);
    /// <summary>
    /// Whether <paramref name="target"/>, a live repair bay (object type 0x1b) of the player's, can fix the one
    /// selected mover (mech bays fix mechs, vehicle bays vehicles) within 100 units.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006d33c0</remarks>
    int getFixedCheck(GameObject* target);
    /// <summary>The mech bar icon of the mover with part id <paramref name="partId"/>, or null.</summary>
    /// <remarks>MCX.EXE @ 0x006d34d0</remarks>
    FriendlyMechIcon* GetMechIconFromID(int32_t partId);

    /// <remarks>MCX.EXE @ 0x006c87e0</remarks>
    Parser* GetCommandParser() { return commandParser; }

    /// <summary>
    /// Whether every selected mover can jump (TacticalMap::updateOrderPalette calls it too). The original name was
    /// lost.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006d2c00</remarks>
    int canSelectionJump();

private:
    /// <summary>
    /// Whether the selection (movers and lances) can jump to <paramref name="position"/>: passable, not onto a
    /// friendly mover, and within every mover's jump range. The original name was lost.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006d2c60</remarks>
    int canSelectionJumpTo(vector_3d position, GameObject* target, int fromWayPoint);

public:
    /// <summary>
    /// The world point under the last map click, which <see cref="handleEvent"/> unprojects into (the original
    /// passes <c>this</c> as the output vector).
    /// </summary>
    vector_3d mouseWorldPos; // +0x0
    /// <summary>How many movers are selected (<see cref="selectedMechs"/>).</summary>
    int16_t numSelectedMechs = 0; // +0xc
    /// <summary>The key that rotates the camera while held (0x38, alt).</summary>
    int16_t rotateKey = 0; // +0xe
    /// <summary>The direction the arrow keys scroll the map (0, 2, 4, 6; -1 = none).</summary>
    int32_t scrollDirection = -1; // +0x10
    /// <summary>The direction the ctrl+arrow keys scroll the tactical map (-1 = none).</summary>
    int32_t tacScrollDirection = -1; // +0x14
    /// <summary>
    /// Nonzero when the current command was picked for one order only: the parser resets
    /// <see cref="currentCommand"/> after sending it. Cleared whenever a key picks a command.
    /// </summary>
    int32_t commandOneShot = 0; // +0x18
    /// <summary>How many frames the mech bar's buttons take to dance into place (<c>Shuffle Frames</c>, 15).</summary>
    int16_t shuffleFrames = 0; // +0x1c
    /// <summary>
    /// Nonzero when the object under the mouse can be captured: capturable, in sight, not the player's, and the
    /// command is none or move (<see cref="UpdateMouseState"/>).
    /// </summary>
    int32_t canCapture = 0; // +0x20
    /// <summary>Nonzero when that capture is blocked (the object has a capture blocker against the player).</summary>
    int32_t captureBlocked = 0; // +0x24
    /// <summary>The cursor's direction variant (0-6), from <see cref="setCursorOffset"/>.</summary>
    int32_t cursorOffset = 0;        // +0x28
    Parser* commandParser = nullptr; // +0x2c
    aMechBar* mechBar = nullptr;     // +0x30
    /// <summary>Icons of movers not on the mech bar (<see cref="AddMech"/> with onBar 0).</summary>
    FriendlyMechIcon* reserveIcons[24] = {}; // +0x34
    int32_t numReserveIcons = 0;             // +0x94
    /// <summary>Set to 0 by the constructor; no other use found.</summary>
    int32_t unknown98 = 0;              // +0x98
    TacticalMap* tacticalMap = nullptr; // +0x9c
    /// <summary>The current command mode (0 none, 1 move, 2 attack, 3 ..., 0x15 jump, 0x16 ..., 0x33 ...).</summary>
    int32_t currentCommand = -1; // +0xa0
    /// <summary>Cleared by the constructor and StartScenario; no other use found.</summary>
    int32_t unknownA4 = 0; // +0xa4
    /// <summary>How far the mouse must move with a button down to start a drag (<c>Drag Distance</c>).</summary>
    int16_t dragDistance = 10; // +0xa8
    /// <summary><c>Scroll Speed</c>.</summary>
    int16_t scrollSpeed = 4; // +0xaa
    /// <summary><c>Tac Scroll Speed</c>.</summary>
    int16_t tacScrollSpeed = 1; // +0xac
    /// <summary><c>Scroll Start</c>: the delay before edge scrolling starts.</summary>
    int16_t scrollStart = 500; // +0xae
    /// <summary>Set to 0 by the constructor; no other use found.</summary>
    int32_t unknownB0 = 0; // +0xb0
    int32_t unknownB4 = 0; // +0xb4
    /// <summary>Part ids of the selected movers.</summary>
    int32_t selectedMechs[12] = {}; // +0xb8
    /// <summary>The selected lances.</summary>
    MoverGroup* selectedLances[4] = {}; // +0xe8
    int16_t numSelectedLances = 0;      // +0xf8
    /// <summary>Where the left button went down (screen), to tell a click from a drag.</summary>
    float mouseDownX = 0.0f; // +0xfc
    float mouseDownY = 0.0f; // +0x100
    /// <summary>Nonzero while the left button is down on the map.</summary>
    int32_t mouseDown = 0; // +0x104
    /// <summary>Set to 1 by the constructor; no other use found.</summary>
    int32_t unknown108 = 1; // +0x108
    /// <summary>What the mouse is over: 0 an icon, 1 an enemy, ..., 7 nothing (-1 before the first update).</summary>
    int32_t mouseObjectType = -1; // +0x10c
    /// <summary>The object (or icon's mover) under the mouse.</summary>
    BaseObject* mouseObject = nullptr; // +0x110
    /// <summary>The enemy selected as the target.</summary>
    GameObject* selectedEnemy = nullptr; // +0x114
    /// <summary>The key bindings, packed as described in the class remarks; slot 0 (+0x118) is unused.</summary>
    uint32_t keys[NUM_KEYS] = {}; // +0x118
    /// <summary>Nonzero while the tactical map's large view is up.</summary>
    int32_t tacMapShown = 0; // +0x24c
    /// <summary>The salvage icon (created elsewhere, deleted by EndScenario).</summary>
    aSalvageIcon* salvageIcon = nullptr; // +0x250
    /// <summary>The floating tags shown over movers.</summary>
    aFloatHelp* floatingTags[12] = {}; // +0x254
    /// <summary>
    /// Set while one of the F9-F12 (or ctrl) keys is held: the next click gives a forced order. Meaning inferred.
    /// </summary>
    int32_t forceOrderActive = 0; // +0x284
    /// <summary>Which forced order (-1 none, 0, 1, 2 = move along a way path). Meaning inferred.</summary>
    int32_t forceOrderType = -1; // +0x288
};

/// <summary>Moves the mech bar's buttons one frame toward their places (an <c>aCallback</c>).</summary>
/// <remarks>MCX.EXE @ 0x006ca610</remarks>
void DancingButtons();

/// <summary>Each lance's colour, indexed by lance number (5 = no lance).</summary>
extern uint8_t lanceColorArray[];
/// <summary>The floating tag's offset from its mover (20, -10).</summary>
extern int32_t sx;
extern int32_t sy;
/// <summary>The slopes that split the cursor directions (<see cref="InterfaceObject::setCursorOffset"/>), 8 floats.</summary>
extern float slopeTest[];
/// <summary>The tactical screen's main window.</summary>
extern aMainWindow* mainHolder;
/// <summary>The current phase (0-2) of the mech bar's dance.</summary>
extern uint8_t danceStep;
/// <summary>Frames since the dance started.</summary>
extern int16_t danceFrames;
/// <summary>The edge-scrolling callback (<c>ScrollScreen</c>), live during a mission.</summary>
extern aCallback* scrollCallback;
/// <summary>The dance callback (<see cref="DancingButtons"/>), live while the buttons move.</summary>
extern aCallback* moveCallback;
/// <summary>The object being dragged (a selection box or an icon).</summary>
extern aObject* dragTarget;
/// <summary>The tactical interface.</summary>
/// <remarks>Defined in iface.cpp by the port; globals_by_file.md places it in terrain\terrmap.cpp.</remarks>
extern InterfaceObject* theInterface;
