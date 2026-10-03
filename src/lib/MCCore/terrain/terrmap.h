#pragma once

#include "gui/abutton.h"
#include "gui/asystem.h"
#include "object/objwtch.h"

class aChatWindow;
class aEvent;
class aPort;
class aScrollTextObject;
class GameObject;
class MechWarrior;
class vector_3d;
struct tagRECT;

/// <summary>What the tactical map's MFD shows (<see cref="TacticalMap::SetDisplayType"/>).</summary>
enum TacmapDisplayTypes
{
    /// <summary>The map (MapSwitch).</summary>
    TACMAP_MAP = 0,
    /// <summary>The selected unit's data (InfoSwitch).</summary>
    TACMAP_INFO = 1,
    /// <summary>The mission objectives (MissionSwitch).</summary>
    TACMAP_MISSION = 2,
    /// <summary>The salvage list, or the chat window in multiplayer (SalvageSwitch).</summary>
    TACMAP_SALVAGE = 3
};

/// <summary>
/// A button of the tactical map's command palette: shows its help text in the MFD's status line while the mouse is
/// over it.
/// </summary>
/// <remarks>Original source: <c>terrain\terrmap.cpp</c>, 0x510 bytes.</remarks>
class ToolPalButton : public aToolButton
{
public:
    /// <remarks>MCX.EXE @ 0x007420c0 (vector deleting destructor); slot 0</remarks>
    ~ToolPalButton() override = default;

    /// <summary>Puts <see cref="helpText"/> in the tactical map's status line (unless a button is held).</summary>
    /// <remarks>MCX.EXE @ 0x0073f550; slot 41</remarks>
    void enter() override;

    /// <summary>Clears the status line (unless a button is held).</summary>
    /// <remarks>MCX.EXE @ 0x0073f580; slot 42</remarks>
    void leave() override;

    /// <summary>The interface mode the button selects (an <c>IntMode</c> from <see cref="buttonActions"/>, or 0x35).</summary>
    int32_t action = 0; // +0x4d8
    /// <summary>Help text shown in the status line.</summary>
    char helpText[0x31] = {}; // +0x4dc
    uint8_t unknown50D = 0;   // +0x50d (cleared by TacticalMap::init)
};

/// <summary>
/// One of the four support (artillery/airstrike) buttons of the tactical map: shows how many strikes of its kind
/// the home commander has left, and while armed, targets the next map click.
/// </summary>
/// <remarks>Original source: <c>terrain\terrmap.cpp</c>, 0x508 bytes.</remarks>
class ArtilleryButton : public aButton
{
public:
    /// <remarks>MCX.EXE @ 0x00742090 (vector deleting destructor); slot 0</remarks>
    ~ArtilleryButton() override = default;

    /// <summary>aButton::init, then clears the armed flags.</summary>
    /// <remarks>MCX.EXE @ 0x0073f5b0; slot 1</remarks>
    int32_t init(int32_t xPos, int32_t yPos, int32_t w, int32_t h, char* fileName) override;

    /// <summary>Grays out when none are left, then draws the count.</summary>
    /// <remarks>MCX.EXE @ 0x0073f5f0; slot 16</remarks>
    void draw() override;

    /// <summary>Arms/disarms the strike and handles the targeting click.</summary>
    /// <remarks>MCX.EXE @ 0x0073f790; slot 21</remarks>
    void handleEvent(aEvent* event) override;

    /// <remarks>MCX.EXE @ 0x0073fbc0; slot 41</remarks>
    void enter() override;

    /// <remarks>MCX.EXE @ 0x0073fbf0; slot 42</remarks>
    void leave() override;

    /// <summary>The command id (0xf8, 0xf9, 0xfa or 0x204), which also picks the commander's strike count.</summary>
    int32_t commandId = 0; // +0x4c8
    /// <summary>Help text shown in the status line.</summary>
    char helpText[0x31] = {}; // +0x4cc
    uint8_t unknown4FD = 0;   // +0x4fd (cleared by TacticalMap::init)
    /// <summary>Nonzero while armed by <see cref="TacticalMap::activateArtillery"/> (a hotkey) rather than a click.</summary>
    int32_t keyArmed = 0; // +0x500
    /// <summary>Nonzero while armed (waiting for the target click).</summary>
    int32_t armed = 0; // +0x504
};

/// <summary>
/// The small video window of the tactical map that shows the pilot speaking on the radio, with a marker blinking at
/// the pilot's unit on the map.
/// </summary>
/// <remarks>Original source: <c>terrain\terrmap.cpp</c>, 0x4c8 bytes.</remarks>
class VideoWindow : public aObject
{
public:
    /// <remarks>MCX.EXE @ 0x007420f0 (vector deleting destructor); slot 0</remarks>
    ~VideoWindow() override = default;

    /// <remarks>MCX.EXE @ 0x00740140; slot 1</remarks>
    int32_t init(int32_t xPos, int32_t yPos, int32_t w, int32_t h, char* fileName) override;

    /// <summary>Draws the speaking pilot's name and tracks the unit's map position (or the idle picture).</summary>
    /// <remarks>MCX.EXE @ 0x00740170; slot 16</remarks>
    void draw() override;

    /// <summary>Draws the line from the window to the unit on the map, then the window.</summary>
    /// <remarks>MCX.EXE @ 0x00740390; slot 50</remarks>
    void display() override;

    /// <summary>Starts (or with null ends) showing <paramref name="star"/>.</summary>
    /// <remarks>MCX.EXE @ 0x007403f0</remarks>
    void SetStar(MechWarrior* star);

    /// <summary>The pilot shown.</summary>
    MechWarrior* star = nullptr; // +0x4ac
    /// <summary>The unit's position on the tactical map.</summary>
    float starMapX = 0.0f; // +0x4b0
    float starMapY = 0.0f; // +0x4b4
    /// <summary>The window's anchor on the tactical map (bottom-centre).</summary>
    float anchorX = 0.0f; // +0x4b8
    float anchorY = 0.0f; // +0x4bc
    /// <summary>scenarioTime of the last blink toggle.</summary>
    float blinkTime = 0.0f; // +0x4c0
    /// <summary>Nonzero while the unit's marker is lit.</summary>
    int32_t blinkOn = 0; // +0x4c4
};

/// <summary>
/// The tactical map: the MFD in the corner of the mission screen, with the revealed map, the units, the command
/// palette, the support buttons, and the info/mission/salvage (or chat) pages.
/// </summary>
/// <remarks>
/// Original source: <c>terrain\terrmap.cpp</c> and <c>terrain\terrmap.h</c> (inline enter/leave/resize), 0x918
/// bytes. Created by <c>Terrain::init</c> as <see cref="Terrain::terrainTacticalMap"/>. Rectangles are
/// (left, top, right, bottom) in the MFD's coordinates.
/// </remarks>
class TacticalMap : public aObject
{
public:
    /// <remarks>MCX.EXE @ 0x007404d0</remarks>
    TacticalMap();
    /// <summary>Frees the watcher.</summary>
    /// <remarks>MCX.EXE @ 0x007405c0 (vector deleting destructor); slot 0</remarks>
    ~TacticalMap() override;

    /// <summary>Destroys every child, port and string table.</summary>
    /// <remarks>MCX.EXE @ 0x00742120; slot 2</remarks>
    void destroy() override;

    /// <summary>Does nothing (the MFD has a fixed size).</summary>
    /// <remarks>MCX.EXE @ 0x00740590; slot 8</remarks>
    void resize(int32_t w, int32_t h) override {}

    /// <summary>
    /// Port: draws the MFD from its state: the page's background, then its contents (the map, fog, view boxes and
    /// units; or the info page's unit), then the status line. The original's draw rebuilt the info and mission
    /// pages and drew them into the MFD's picture, over what earlier draws had left; the rebuilding is
    /// <see cref="RefreshPage"/> now, and the MFD draws itself whole each frame.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x007438d0; slot 16</remarks>
    void draw() override;
    /// <summary>Port: the MFD draws itself each frame.</summary>
    bool DrawsLive() override { return true; }

    /// <summary>
    /// Port: what the original's draw did besides drawing: the info page's colours, weapon list and text, the
    /// mission page's objectives. Run where the original drew the MFD (page and unit changes, and every 500 ms).
    /// </summary>
    void RefreshPage();

    /// <remarks>MCX.EXE @ 0x00742600; slot 21</remarks>
    void handleEvent(aEvent* event) override;
    /// <summary>
    /// Port-only: on the map page the mouse wheel zooms as the zoom buttons do (up in, down out); on the info,
    /// mission and salvage pages it scrolls the page's text as the page's arrows do.
    /// </summary>
    bool MouseWheel(int32_t steps, int32_t xPos, int32_t yPos) override;

    /// <remarks>MCX.EXE @ 0x007405a0; slot 41</remarks>
    void enter() override { mouseInside = -1; }

    /// <remarks>MCX.EXE @ 0x007405b0; slot 42</remarks>
    void leave() override { mouseInside = 0; }

    /// <remarks>MCX.EXE @ 0x00742ca0; slot 50</remarks>
    void display() override;

    /// <remarks>MCX.EXE @ 0x00745d50; slot 66</remarks>
    void HideMe(int hide) override;

    /// <summary>Draws <c>&lt;terrainPath&gt;&lt;name&gt;.gif</c> (the pre-revealed parts) into the map port.</summary>
    /// <remarks>MCX.EXE @ 0x007405f0</remarks>
    void setRevealedBitmap(char* fileName);

    /// <summary>
    /// Builds the MFD at (<paramref name="xPos"/>, <paramref name="yPos"/>): the map ports (the map picture, and
    /// the fog-of-war port that aliases the visible bits), the backgrounds, tabs, buttons, pages and string tables.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x007406c0</remarks>
    int32_t init(int32_t xPos, int32_t yPos);

    /// <summary>
    /// Converts a world position to tactical-map pixels (rotated 45 degrees); with <paramref name="scrolled"/>
    /// zero, unscrolled at the current zoom.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x007436d0</remarks>
    void worldToTacMap(vector_3d& pos, int scrolled);

    /// <summary>The inverse of <see cref="worldToTacMap"/>.</summary>
    /// <remarks>MCX.EXE @ 0x007437d0</remarks>
    void tacMapToWorld(vector_3d& pos, int scrolled);

    /// <summary>Draws the objectives, units, contacts and sensor ranges on the map.</summary>
    /// <remarks>MCX.EXE @ 0x00744010</remarks>
    void drawObjects();

    /// <summary>
    /// Port: the map page's per-frame state, which the original updated as it drew the page: the mission timer's
    /// text (once a second), the markers' blink, and the first reveal of the objectives' areas in the fog of war.
    /// </summary>
    void UpdateMapPage();

    /// <summary>
    /// Port: once a pending objective with a position exists, reveals every objective's area in the fog of war (the
    /// visible bits game logic reads). The original did it as it drew the map's units, after the fog; the MFD runs it
    /// after drawing the map page, at the same point of the frame.
    /// </summary>
    void RevealObjectives();

    /// <summary>Port: draws the info page's unit (what the original's draw drew of it).</summary>
    void DrawInfoPage();

    /// <summary>Switches the MFD page.</summary>
    /// <remarks>MCX.EXE @ 0x00744e10</remarks>
    void SetDisplayType(TacmapDisplayTypes type);

    /// <remarks>MCX.EXE @ 0x007454c0</remarks>
    void centerOnObject(GameObject* obj);

    /// <remarks>MCX.EXE @ 0x00745500</remarks>
    void scrollMap(int32_t dx, int32_t dy);

    /// <remarks>MCX.EXE @ 0x00745600</remarks>
    void setScrollMapPosition(int32_t x, int32_t y);

    /// <summary>The screen area the video window's picture occupies (below its name line).</summary>
    /// <remarks>MCX.EXE @ 0x00745770</remarks>
    tagRECT GetVideoRect();

    /// <summary>Adds a salvageable object to the salvage page.</summary>
    /// <returns>Nonzero (-1) when it is (now) listed.</returns>
    /// <remarks>MCX.EXE @ 0x00745b00</remarks>
    int AddSalvage(GameObject* obj);

    /// <summary>Removes an object from the salvage list (and rebuilds the page if <paramref name="refresh"/>).</summary>
    /// <remarks>MCX.EXE @ 0x00745be0</remarks>
    int RemoveSalvage(GameObject* obj, int refresh);

    /// <summary>Drops salvage that has been destroyed or picked up, then rebuilds the page.</summary>
    /// <remarks>MCX.EXE @ 0x00745c60</remarks>
    void UpdateSalvage();

    /// <summary>Rebuilds the salvage page's text from the list, keeping the scroll position.</summary>
    /// <remarks>MCX.EXE @ 0x00745cd0. Its name wasn't kept (no symbol); this is the port's name.</remarks>
    void refreshSalvageList();

    /// <summary>Shows the unit with part id <paramref name="partId"/> on the info page.</summary>
    /// <remarks>MCX.EXE @ 0x007465e0</remarks>
    void SetID(int32_t partId);

    /// <summary>Makes the command palette match the interface's current mode.</summary>
    /// <remarks>MCX.EXE @ 0x00747410</remarks>
    void updateOrderPalette();

    /// <summary>Selects the info page's data view (0 armor front, 1 rear, 2 payload).</summary>
    /// <remarks>MCX.EXE @ 0x00747570</remarks>
    void SetDataDisplayMode(char mode, int silent);

    /// <summary>Toggles the zoom palette button.</summary>
    /// <remarks>MCX.EXE @ 0x007475e0</remarks>
    void toggleZoom();

    /// <summary>
    /// How far <paramref name="pos"/> lies off the tactical map: <paramref name="pos"/> minus the nearest point on it
    /// (in world units), or zero when it is on the map.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00747600</remarks>
    vector_3d positionOnMap(vector_3d pos);

    /// <summary>Passes a chat message to the chat window and blinks the chat tab.</summary>
    /// <remarks>MCX.EXE @ 0x007476e0 (the line tables credit terrtxm.cpp)</remarks>
    void handleChatMessage(uint32_t fromID, const void* message);

    /// <summary>Arms (or disarms) support button <paramref name="button"/> as if clicked.</summary>
    /// <remarks>MCX.EXE @ 0x00747790</remarks>
    void activateArtillery(int32_t button, int arm);

protected:
    /// <summary>Appends the salvage line of <paramref name="obj"/> to the salvage page.</summary>
    /// <remarks>MCX.EXE @ 0x00745830</remarks>
    void AddSalvageString(GameObject* obj);

    /// <remarks>MCX.EXE @ 0x00745ec0</remarks>
    void DrawBar();

    /// <summary>Draws the selected unit's armor/payload diagram on the info page.</summary>
    /// <remarks>MCX.EXE @ 0x00745f50</remarks>
    void DrawParts();

    /// <summary>Picks the colours of the part diagram from the unit's damage.</summary>
    /// <remarks>MCX.EXE @ 0x007463e0</remarks>
    void GetColors();

    /// <remarks>MCX.EXE @ 0x00746910</remarks>
    void drawPilot(MechWarrior* pilot);

    /// <remarks>MCX.EXE @ 0x00746ce0</remarks>
    void drawWeapons();

public:
    /// <summary>The zoom buttons' area.</summary>
    int32_t zoomRect[4] = {}; // +0x4ac
    /// <summary>Checked by handleEvent; set only by init.</summary>
    int32_t unknown4BC[4] = {}; // +0x4bc
    /// <summary>The map area (6, 0x22, 0x87, 0xa3).</summary>
    int32_t mapRect[4] = {}; // +0x4cc
    /// <summary>The page's click areas, placed by SetDisplayType.</summary>
    int32_t pageRects[3][4] = {}; // +0x4dc
    /// <summary>Meters per map pixel at the current zoom.</summary>
    float metersPerPixel = 0.0f; // +0x50c
    /// <summary>Vertices along the map's side (verticesBlockSide * blocksMapSide).</summary>
    int32_t mapVertexSide = 0; // +0x510
    /// <summary>The map's diagonal in meters.</summary>
    float mapDiagonal = 0.0f; // +0x514
    /// <summary>Zoom factor: 1, 2, 4 or 8.</summary>
    int32_t zoom = 0;       // +0x518
    int32_t unknown51C = 0; // +0x51c
    /// <summary>The map's scroll position.</summary>
    int32_t scrollX = 0;    // +0x520
    int32_t scrollY = 0;    // +0x524
    int32_t unknown528 = 0; // +0x528
    int32_t unknown52C = 0; // +0x52c
    /// <summary>Accumulated by the zoom buttons (half the map side over the zoom).</summary>
    int32_t zoomOffset = 0; // +0x530
    /// <summary>The map picture's size (from its TGA).</summary>
    int32_t mapWidth = 0;    // +0x534
    int32_t mapHeight = 0;   // +0x538
    int32_t unknown53C = 0;  // +0x53c
    int32_t unknown540 = -1; // +0x540
    int32_t unknown544 = -1; // +0x544
    int32_t unknown548 = -1; // +0x548
    int32_t unknown54C = 0;  // +0x54c
    int32_t unknown550 = 0;  // +0x550
    int32_t unknown554 = 0;  // +0x554
    /// <summary>Nonzero to draw sensor/range circles around the units.</summary>
    int32_t showRanges = -1; // +0x558
    /// <summary>The map picture (<c>&lt;terrainName&gt;.tga</c>).</summary>
    aPort* mapPort = nullptr; // +0x55c
    /// <summary>The fog-of-war port: its bitmap is the visible bits of the home side.</summary>
    aPort* visibilityPort = nullptr; // +0x560
    /// <summary>Background of the map page (mfdmwn00.tga).</summary>
    aPort* mapBackground = nullptr; // +0x564
    /// <summary>Background of the info page (mfddwn00.tga).</summary>
    aPort* infoBackground = nullptr; // +0x568
    /// <summary>Background of the mission page (mfdbwn00.tga).</summary>
    aPort* missionBackground = nullptr; // +0x56c
    /// <summary>Background of the salvage page (mfdswn01.tga; the mission one in single player).</summary>
    aPort* salvageBackground = nullptr; // +0x570
    /// <summary>Ports of the info page's pilot/part pictures.</summary>
    aPort* infoPorts[4] = {}; // +0x574
    /// <summary>The tab strip.</summary>
    aObject* tabStrip = nullptr; // +0x584
    /// <summary>The top tab.</summary>
    aObject* tabTop = nullptr; // +0x588
    /// <summary>The bottom tab.</summary>
    aObject* tabBottom = nullptr; // +0x58c
    /// <summary>Nonzero while the bottom tab shows its highlighted picture.</summary>
    int32_t tabHighlighted = 0; // +0x590
    /// <summary>The map area as a pane on the MFD's window.</summary>
    _pane mapPane = {}; // +0x594
    /// <summary>Salvage objects listed.</summary>
    int32_t numSalvage = 0; // +0x5a8
    /// <summary>The salvage objects.</summary>
    GameObject* salvage[100] = {}; // +0x5ac
    /// <summary>The salvage page.</summary>
    aScrollTextObject* salvageText = nullptr; // +0x73c
    /// <summary>The info/mission page text.</summary>
    aScrollTextObject* infoText = nullptr; // +0x740
    /// <summary>The pilot video window.</summary>
    VideoWindow* videoWindow = nullptr; // +0x744
    /// <summary>The unit shown on the info page.</summary>
    GameObject* infoObject = nullptr; // +0x748
    /// <summary>The support buttons.</summary>
    ArtilleryButton* artilleryButtons[4] = {}; // +0x74c
    /// <summary>The palette toggle button.</summary>
    aToolButton* paletteButton = nullptr; // +0x75c
    /// <summary>The command palette (the last one is the zoom/palette-toggle button).</summary>
    ToolPalButton* toolButtons[8] = {}; // +0x760
    /// <summary>The scroll buttons (up, left, down, right) and zoom in/out.</summary>
    aButton* scrollButtons[6] = {}; // +0x780
    /// <summary>The command palette frame.</summary>
    aObject* paletteFrame = nullptr; // +0x798
    /// <summary>The command palette's lower half.</summary>
    aObject* paletteBottom = nullptr; // +0x79c
    /// <summary>The info page's data buttons (armor front, rear, payload).</summary>
    aToolButton* dataButtons[3] = {}; // +0x7a0
    /// <summary>Info page scroll markers.</summary>
    aObject* scrollUpMarker = nullptr;   // +0x7ac
    aObject* scrollDownMarker = nullptr; // +0x7b0
    /// <summary>The chat tab's blinkers (multiplayer).</summary>
    aObject* chatBlinkerOn = nullptr;  // +0x7b4
    aObject* chatBlinkerOff = nullptr; // +0x7b8
    /// <summary>The chat window (multiplayer).</summary>
    aChatWindow* chatWindow = nullptr; // +0x7bc
    /// <summary>Nonzero while a chat message is unread (the tab blinks).</summary>
    int32_t chatPending = 0; // +0x7c0
    /// <summary>The current page.</summary>
    TacmapDisplayTypes displayType = TACMAP_MAP; // +0x7c4
    int32_t unknown7C8 = 0;                      // +0x7c8
    /// <summary>The part diagram shapes of the info page (from guiHeap).</summary>
    uint8_t* partShapes = nullptr; // +0x7cc
    /// <summary>The part diagram's colour of each armor location (GetColors).</summary>
    uint8_t armorColors[11] = {}; // +0x7d0
    /// <summary>The part diagram's colour of each internal structure (body) location (GetColors).</summary>
    uint8_t bodyColors[13] = {}; // +0x7db
    /// <summary>Watches <see cref="infoObject"/> so it is cleared when the object goes away.</summary>
    BaseObjectWatcher infoWatcher; // +0x7e8
    /// <summary>GetTickCount of the last page refresh.</summary>
    uint32_t lastRefreshTime = 0; // +0x7ec
    /// <summary>Time of the last map redraw (-999 initially).</summary>
    float lastMapTime = 0.0f; // +0x7f0
    /// <summary>The status line's text (a hovered button's help), or null.</summary>
    char* statusText = nullptr; // +0x7f4
    /// <summary>Nonzero when the status line must be redrawn.</summary>
    int32_t statusDirty = -1; // +0x7f8
    /// <summary>Nonzero while a support button holds the status line.</summary>
    int32_t statusLocked = 0; // +0x7fc
    /// <summary>The info page's data view (<see cref="SetDataDisplayMode"/>).</summary>
    char dataDisplayMode = 0; // +0x800
    /// <summary>Nonzero when the info page must be redrawn.</summary>
    int32_t infoDirty = 0; // +0x804
    /// <summary>-1 while the mouse is over the MFD.</summary>
    int32_t mouseInside = 0; // +0x808
    /// <summary>Nonzero while the map is being dragged.</summary>
    int32_t mapDragging = 0; // +0x80c
    /// <summary>Set once drawObjects has revealed the objectives' areas in the fog of war.</summary>
    int32_t objectivesRevealed = 0; // +0x810
    /// <summary>Colour remap for the map picture (0xff = unchanged; entries 0xe6 and 0xe8 map to 0x13).</summary>
    uint8_t colorRemap[256] = {}; // +0x814
    int32_t unknown914 = 0;       // +0x914

    /// <summary>
    /// Port: the info page's data view backgrounds (mfddwn01.tga home armor, mfddwn02.tga payload, mfddwn03.tga
    /// enemy armor), which the original loaded each time it drew one.
    /// </summary>
    aPort* infoViewBackgrounds[3] = {};
    /// <summary>Port: the mission timer as last written (once a second), and whether it is red (time up).</summary>
    char mapTimeText[16] = {};
    bool mapTimeRed = false;
    /// <summary>Port: set once the timer has been written.</summary>
    bool mapTimeShown = false;
};

/// <summary>Shows or hides the command palette.</summary>
/// <remarks>MCX.EXE @ 0x0073ec90</remarks>
void TogglePalette();
/// <summary>Event routine of the chat tab blinkers.</summary>
/// <remarks>MCX.EXE @ 0x0073eda0</remarks>
void BlinkerHandleEvent(aObject* obj, aEvent* event);
/// <summary>Event routines of the map scroll buttons.</summary>
/// <remarks>MCX.EXE @ 0x0073edf0</remarks>
void TMCUp(aObject* obj, aEvent* event);
/// <remarks>MCX.EXE @ 0x0073eed0</remarks>
void TMCLeft(aObject* obj, aEvent* event);
/// <remarks>MCX.EXE @ 0x0073efb0</remarks>
void TMCDown(aObject* obj, aEvent* event);
/// <remarks>MCX.EXE @ 0x0073f090</remarks>
void TMCRight(aObject* obj, aEvent* event);
/// <summary>Callback of the zoom-in button (at most 8x).</summary>
/// <remarks>MCX.EXE @ 0x0073f170</remarks>
void TMCZoomIn();
/// <summary>Callback of the zoom-out button.</summary>
/// <remarks>MCX.EXE @ 0x0073f340</remarks>
void TMCZoomOut();
/// <summary>Callbacks of the info page's data buttons.</summary>
/// <remarks>MCX.EXE @ 0x0073f520</remarks>
void ArmorFrontButton();
/// <remarks>MCX.EXE @ 0x0073f530</remarks>
void PayloadButton();
/// <remarks>MCX.EXE @ 0x0073f540</remarks>
void RearButton();
/// <summary>Page switches.</summary>
/// <remarks>MCX.EXE @ 0x0073fc20</remarks>
void MapSwitch();
/// <remarks>MCX.EXE @ 0x0073fc50</remarks>
void SalvageSwitch();
/// <remarks>MCX.EXE @ 0x0073fd00</remarks>
void InfoSwitch();
/// <remarks>MCX.EXE @ 0x0073fdb0</remarks>
void MissionSwitch();
/// <summary>Event routine of the command palette buttons.</summary>
/// <remarks>MCX.EXE @ 0x0073fe60</remarks>
void ToolPaletteButtonEvent(aObject* obj, aEvent* event);
/// <summary>Event routines of the page tabs.</summary>
/// <remarks>MCX.EXE @ 0x0073ff50</remarks>
void TabStripEvent(aObject* obj, aEvent* event);
/// <remarks>MCX.EXE @ 0x007400a0</remarks>
void TabTopEvent(aObject* obj, aEvent* event);
/// <remarks>MCX.EXE @ 0x007400f0</remarks>
void TabBottomEvent(aObject* obj, aEvent* event);
/// <summary>qsort comparison of the info page's weapon list.</summary>
/// <remarks>MCX.EXE @ 0x00746c90</remarks>
int CompareWeapons(const void* a, const void* b);

/// <summary>The interface mode of each command palette button (<c>IntMode</c> values: 15, 14, 13, 12, 19, 17, 3, 53).</summary>
extern int32_t buttonActions[8];
/// <summary>Colours of the range circles.</summary>
extern int16_t RangeColorArray[4];
/// <summary>"Calling ..." text of the video window.</summary>
extern char callingText[64];
/// <summary>The unit status strings of the info page (string table 0x78-0x7b).</summary>
extern char* statusString[4];
/// <summary>The unit type strings of the info page (string table 0x7c-0x80).</summary>
extern char* typeString[5];
/// <summary>The world point at the centre of the tactical map.</summary>
extern vector_3d tacMapCenter;
/// <summary>Salvage objects listed (mirrors TacticalMap::numSalvage).</summary>
extern int32_t realSalvageCount;
extern float tacFrameLength;
extern int onNow;
