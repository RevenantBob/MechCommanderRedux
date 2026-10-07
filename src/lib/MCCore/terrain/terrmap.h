#pragma once

#include "gui/abutton.h"
#include "gui/asystem.h"
#include "object/objwtch.h"

class MCGuiChatWindow;
class MCGuiEvent;
class MCGuiPort;
class MCGuiScrollTextObject;
class MCGameObject;
class MCMechWarrior;
class MCVector3D;
struct tagRECT;

/// <summary>What the tactical map's MFD shows (<see cref="MCTacticalMap::SetDisplayType"/>).</summary>
enum MCTacmapDisplayTypes
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
class MCToolPalButton : public MCGuiToolButton
{
public:
    ~MCToolPalButton() override = default;

    /// <summary>Puts <see cref="HelpText"/> in the tactical map's status line (unless a button is held).</summary>
    void Enter() override;

    /// <summary>Clears the status line (unless a button is held).</summary>
    void Leave() override;

    /// <summary>The interface mode the button selects (an <c>IntMode</c> from <see cref="ButtonActions"/>, or 0x35).</summary>
    int32_t Action = 0;
    /// <summary>Help text shown in the status line.</summary>
    char HelpText[0x31] = {};
};

/// <summary>
/// One of the four support (artillery/airstrike) buttons of the tactical map: shows how many strikes of its kind
/// the home commander has left, and while armed, targets the next map click.
/// </summary>
/// <remarks>Original source: <c>terrain\terrmap.cpp</c>, 0x508 bytes.</remarks>
class MCArtilleryButton : public MCGuiButton
{
public:
    ~MCArtilleryButton() override = default;

    /// <summary>aButton::init, then clears the armed flags.</summary>
    int32_t Init(int32_t xPos, int32_t yPos, int32_t w, int32_t h, char* fileName) override;

    /// <summary>Grays out when none are left, then draws the count.</summary>
    void Draw() override;

    /// <summary>Arms/disarms the strike and handles the targeting click.</summary>
    void HandleEvent(MCGuiEvent* event) override;

    void Enter() override;

    void Leave() override;

    /// <summary>The command id (0xf8, 0xf9, 0xfa or 0x204), which also picks the commander's strike count.</summary>
    int32_t CommandId = 0;
    /// <summary>Help text shown in the status line.</summary>
    char HelpText[0x31] = {};
    /// <summary>Nonzero while armed by <see cref="MCTacticalMap::ActivateArtillery"/> (a hotkey) rather than a click.</summary>
    int32_t KeyArmed = 0;
    /// <summary>Nonzero while armed (waiting for the target click).</summary>
    int32_t Armed = 0;
};

/// <summary>
/// The small video window of the tactical map that shows the pilot speaking on the radio, with a marker blinking at
/// the pilot's unit on the map.
/// </summary>
/// <remarks>Original source: <c>terrain\terrmap.cpp</c>, 0x4c8 bytes.</remarks>
class MCVideoWindow : public MCGuiObject
{
public:
    ~MCVideoWindow() override = default;

    int32_t Init(int32_t xPos, int32_t yPos, int32_t w, int32_t h, char* fileName) override;

    /// <summary>
    /// Draws the speaking pilot's name and tracks the unit's map position (or the idle picture). Port: draws the
    /// picture and the speaking pilot's name each frame; <see cref="Update"/> does the rest.
    /// </summary>
    void Draw() override;

    /// <summary>Port: the window draws itself each frame (its port is a view).</summary>
    bool DrawsLive() override { return true; }

    /// <summary>
    /// Port: the original draw's work besides painting, which ran when <see cref="SetStar"/> painted: the mech bar
    /// blink and the unit's place on the map.
    /// </summary>
    void Update();

    /// <summary>
    /// Port: where the speaking pilot's unit is on the tactical map (<paramref name="mapX"/>, <paramref name="mapY"/>
    /// on the screen), or the window's bottom centre when it is off the map area.
    /// </summary>
    void TrackStar(float& mapX, float& mapY);

    /// <summary>
    /// Draws the line from the window to the unit on the map, then the window. Port: the line follows the unit.
    /// </summary>
    void Display() override;

    /// <summary>Starts (or with null ends) showing <paramref name="star"/>.</summary>
    void SetStar(MCMechWarrior* star);

    /// <summary>The pilot shown.</summary>
    MCMechWarrior* Star = nullptr;
    /// <summary>The unit's position on the tactical map.</summary>
    float StarMapX = 0.0f;
    float StarMapY = 0.0f;
    /// <summary>The window's anchor on the tactical map (bottom-centre).</summary>
    float AnchorX = 0.0f;
    float AnchorY = 0.0f;
    /// <summary>scenarioTime of the last blink toggle.</summary>
    float BlinkTime = 0.0f;
    /// <summary>Nonzero while the unit's marker is lit.</summary>
    int32_t BlinkOn = 0;
};

/// <summary>
/// The tactical map: the MFD in the corner of the mission screen, with the revealed map, the units, the command
/// palette, the support buttons, and the info/mission/salvage (or chat) pages.
/// </summary>
/// <remarks>
/// Original source: <c>terrain\terrmap.cpp</c> and <c>terrain\terrmap.h</c> (inline enter/leave/resize), 0x918
/// bytes. Created by <c>Terrain::init</c> as <see cref="MCTerrain::TerrainTacticalMap"/>. Rectangles are
/// (left, top, right, bottom) in the MFD's coordinates.
/// </remarks>
class MCTacticalMap : public MCGuiObject
{
public:
    MCTacticalMap();
    /// <summary>Frees the watcher.</summary>
    ~MCTacticalMap() override;

    /// <summary>Destroys every child, port and string table.</summary>
    void Destroy() override;

    /// <summary>Unregisters and frees <see cref="PartShapes"/>.</summary>
    void FreePartShapes();

    /// <summary>Does nothing (the MFD has a fixed size).</summary>
    void Resize(int32_t w, int32_t h) override {}

    /// <summary>
    /// Port: draws the MFD from its state: the page's background, then its contents (the map, fog, view boxes and
    /// units; or the info page's unit), then the status line. The original's draw rebuilt the info and mission
    /// pages and drew them into the MFD's picture, over what earlier draws had left; the rebuilding is
    /// <see cref="RefreshPage"/> now, and the MFD draws itself whole each frame.
    /// </summary>
    void Draw() override;
    /// <summary>Port: the MFD draws itself each frame.</summary>
    bool DrawsLive() override { return true; }

    /// <summary>
    /// Port: what the original's draw did besides drawing: the info page's colours, weapon list and text, the
    /// mission page's objectives. Run where the original drew the MFD (page and unit changes, and every 500 ms).
    /// </summary>
    void RefreshPage();

    void HandleEvent(MCGuiEvent* event) override;
    /// <summary>
    /// Port-only: on the map page the mouse wheel zooms as the zoom buttons do (up in, down out); on the info,
    /// mission and salvage pages it scrolls the page's text as the page's arrows do.
    /// </summary>
    bool MouseWheel(int32_t steps, int32_t xPos, int32_t yPos) override;

    void Enter() override { MouseInside = -1; }

    void Leave() override { MouseInside = 0; }

    void Display() override;

    void HideMe(int hide) override;

    /// <summary>Draws <c>&lt;terrainPath&gt;&lt;name&gt;.gif</c> (the pre-revealed parts) into the map port.</summary>
    void SetRevealedBitmap(char* fileName);

    /// <summary>
    /// Builds the MFD at (<paramref name="xPos"/>, <paramref name="yPos"/>): the map ports (the map picture, and
    /// the fog-of-war port that aliases the visible bits), the backgrounds, tabs, buttons, pages and string tables.
    /// </summary>
    int32_t Init(int32_t xPos, int32_t yPos);

    /// <summary>
    /// Converts a world position to tactical-map pixels (rotated 45 degrees); with <paramref name="scrolled"/>
    /// zero, unscrolled at the current zoom.
    /// </summary>
    void WorldToTacMap(MCVector3D& pos, int scrolled);

    /// <summary>The inverse of <see cref="WorldToTacMap"/>.</summary>
    void TacMapToWorld(MCVector3D& pos, int scrolled);

    /// <summary>Draws the objectives, units, contacts and sensor ranges on the map.</summary>
    void DrawObjects();

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
    void SetDisplayType(MCTacmapDisplayTypes type);

    void CenterOnObject(MCGameObject* obj);

    void ScrollMap(int32_t dx, int32_t dy);

    void SetScrollMapPosition(int32_t x, int32_t y);

    /// <summary>The screen area the video window's picture occupies (below its name line).</summary>
    tagRECT GetVideoRect();

    /// <summary>Adds a salvageable object to the salvage page.</summary>
    /// <returns>Nonzero (-1) when it is (now) listed.</returns>
    int AddSalvage(MCGameObject* obj);

    /// <summary>Removes an object from the salvage list (and rebuilds the page if <paramref name="refresh"/>).</summary>
    int RemoveSalvage(MCGameObject* obj, int refresh);

    /// <summary>Drops salvage that has been destroyed or picked up, then rebuilds the page.</summary>
    void UpdateSalvage();

    /// <summary>Rebuilds the salvage page's text from the list, keeping the scroll position.</summary>
    void RefreshSalvageList();

    /// <summary>Shows the unit with part id <paramref name="partId"/> on the info page.</summary>
    void SetID(int32_t partId);

    /// <summary>Makes the command palette match the interface's current mode.</summary>
    void UpdateOrderPalette();

    /// <summary>Selects the info page's data view (0 armor front, 1 rear, 2 payload).</summary>
    void SetDataDisplayMode(char mode, int silent);

    /// <summary>Toggles the zoom palette button.</summary>
    void ToggleZoom();

    /// <summary>
    /// How far <paramref name="pos"/> lies off the tactical map: <paramref name="pos"/> minus the nearest point on it
    /// (in world units), or zero when it is on the map.
    /// </summary>
    MCVector3D PositionOnMap(MCVector3D pos);

    /// <summary>Passes a chat message to the chat window and blinks the chat tab.</summary>
    void HandleChatMessage(uint32_t fromID, const void* message);

    /// <summary>Arms (or disarms) support button <paramref name="button"/> as if clicked.</summary>
    void ActivateArtillery(int32_t button, int arm);

protected:
    /// <summary>Appends the salvage line of <paramref name="obj"/> to the salvage page.</summary>
    void AddSalvageString(MCGameObject* obj);

    void DrawBar();

    /// <summary>Draws the selected unit's armor/payload diagram on the info page.</summary>
    void DrawParts();

    /// <summary>Picks the colours of the part diagram from the unit's damage.</summary>
    void GetColors();

    void DrawPilot(MCMechWarrior* pilot);

    void DrawWeapons();

public:
    /// <summary>The zoom-in button's click area (left, top, right, bottom), which takes the event whole.</summary>
    int32_t ZoomInRect[4] = {};
    /// <summary>The zoom-out button's click area (left, top, right, bottom), which takes the event whole.</summary>
    int32_t ZoomOutRect[4] = {};
    /// <summary>The map area (6, 0x22, 0x87, 0xa3).</summary>
    int32_t MapRect[4] = {};
    /// <summary>The page's click areas, placed by SetDisplayType.</summary>
    int32_t PageRects[3][4] = {};
    /// <summary>Meters per map pixel at the current zoom.</summary>
    float MetersPerPixel = 0.0f;
    /// <summary>Vertices along the map's side (verticesBlockSide * blocksMapSide).</summary>
    int32_t MapVertexSide = 0;
    /// <summary>The map's diagonal in meters.</summary>
    float MapDiagonal = 0.0f;
    /// <summary>Zoom factor: 1, 2, 4 or 8.</summary>
    int32_t Zoom = 0;
    /// <summary>The map's scroll position.</summary>
    int32_t ScrollX = 0;
    int32_t ScrollY = 0;
    /// <summary>Accumulated by the zoom buttons (half the map side over the zoom).</summary>
    int32_t ZoomOffset = 0;
    /// <summary>The map picture's size (from its TGA).</summary>
    int32_t MapWidth = 0;
    int32_t MapHeight = 0;
    /// <summary>Nonzero to draw sensor/range circles around the units.</summary>
    int32_t ShowRanges = -1;
    /// <summary>The map picture (<c>&lt;terrainName&gt;.tga</c>).</summary>
    MCGuiPort* MapPort = nullptr;
    /// <summary>The fog-of-war port: its bitmap is the visible bits of the home side.</summary>
    MCGuiPort* VisibilityPort = nullptr;
    /// <summary>Background of the map page (mfdmwn00.tga).</summary>
    MCGuiPort* MapBackground = nullptr;
    /// <summary>Background of the info page (mfddwn00.tga).</summary>
    MCGuiPort* InfoBackground = nullptr;
    /// <summary>Background of the mission page (mfdbwn00.tga).</summary>
    MCGuiPort* MissionBackground = nullptr;
    /// <summary>Background of the salvage page (mfdswn01.tga; the mission one in single player).</summary>
    MCGuiPort* SalvageBackground = nullptr;
    /// <summary>Ports of the info page's pilot/part pictures.</summary>
    MCGuiPort* InfoPorts[4] = {};
    /// <summary>The tab strip.</summary>
    MCGuiObject* TabStrip = nullptr;
    /// <summary>The top tab.</summary>
    MCGuiObject* TabTop = nullptr;
    /// <summary>The bottom tab.</summary>
    MCGuiObject* TabBottom = nullptr;
    /// <summary>Nonzero while the bottom tab shows its highlighted picture.</summary>
    int32_t TabHighlighted = 0;
    /// <summary>The map area as a pane on the MFD's window.</summary>
    MCPane MapPane = {};
    /// <summary>Salvage objects listed.</summary>
    int32_t NumSalvage = 0;
    /// <summary>The salvage objects.</summary>
    MCGameObject* Salvage[100] = {};
    /// <summary>The salvage page.</summary>
    MCGuiScrollTextObject* SalvageText = nullptr;
    /// <summary>The info/mission page text.</summary>
    MCGuiScrollTextObject* InfoText = nullptr;
    /// <summary>The pilot video window.</summary>
    MCVideoWindow* VideoWindow = nullptr;
    /// <summary>The unit shown on the info page.</summary>
    MCGameObject* InfoObject = nullptr;
    /// <summary>The support buttons.</summary>
    MCArtilleryButton* ArtilleryButtons[4] = {};
    /// <summary>The palette toggle button.</summary>
    MCGuiToolButton* PaletteButton = nullptr;
    /// <summary>The command palette (the last one is the zoom/palette-toggle button).</summary>
    MCToolPalButton* ToolButtons[8] = {};
    /// <summary>The scroll buttons (up, left, down, right) and zoom in/out.</summary>
    MCGuiButton* ScrollButtons[6] = {};
    /// <summary>The command palette frame.</summary>
    MCGuiObject* PaletteFrame = nullptr;
    /// <summary>The command palette's lower half.</summary>
    MCGuiObject* PaletteBottom = nullptr;
    /// <summary>The info page's data buttons (armor front, rear, payload).</summary>
    MCGuiToolButton* DataButtons[3] = {};
    /// <summary>Info page scroll markers.</summary>
    MCGuiObject* ScrollUpMarker = nullptr;
    MCGuiObject* ScrollDownMarker = nullptr;
    /// <summary>The chat tab's blinkers (multiplayer).</summary>
    MCGuiObject* ChatBlinkerOn = nullptr;
    MCGuiObject* ChatBlinkerOff = nullptr;
    /// <summary>The chat window (multiplayer).</summary>
    MCGuiChatWindow* ChatWindow = nullptr;
    /// <summary>Nonzero while a chat message is unread (the tab blinks).</summary>
    int32_t ChatPending = 0;
    /// <summary>The current page.</summary>
    MCTacmapDisplayTypes DisplayType = TACMAP_MAP;
    /// <summary>The part diagram shapes of the info page.</summary>
    std::unique_ptr<uint8_t[]> PartShapes;
    /// <summary>The part diagram's colour of each armor location (GetColors).</summary>
    uint8_t ArmorColors[11] = {};
    /// <summary>The part diagram's colour of each internal structure (body) location (GetColors).</summary>
    uint8_t BodyColors[13] = {};
    /// <summary>Watches <see cref="InfoObject"/> so it is cleared when the object goes away.</summary>
    MCBaseObjectWatcher InfoWatcher;
    /// <summary>GetTickCount of the last page refresh.</summary>
    uint32_t LastRefreshTime = 0;
    /// <summary>Time of the last map redraw (-999 initially).</summary>
    float LastMapTime = 0.0f;
    /// <summary>The status line's text (a hovered button's help), or null.</summary>
    char* StatusText = nullptr;
    /// <summary>Nonzero when the status line must be redrawn.</summary>
    int32_t StatusDirty = -1;
    /// <summary>Nonzero while a support button holds the status line.</summary>
    int32_t StatusLocked = 0;
    /// <summary>The info page's data view (<see cref="SetDataDisplayMode"/>).</summary>
    char DataDisplayMode = 0;
    /// <summary>Nonzero when the info page must be redrawn.</summary>
    int32_t InfoDirty = 0;
    /// <summary>-1 while the mouse is over the MFD.</summary>
    int32_t MouseInside = 0;
    /// <summary>Nonzero while the map is being dragged.</summary>
    int32_t MapDragging = 0;
    /// <summary>Set once drawObjects has revealed the objectives' areas in the fog of war.</summary>
    int32_t ObjectivesRevealed = 0;
    /// <summary>Colour remap for the map picture (0xff = unchanged; entries 0xe6 and 0xe8 map to 0x13).</summary>
    uint8_t ColorRemap[256] = {};

    /// <summary>
    /// Port: the info page's data view backgrounds (mfddwn01.tga home armor, mfddwn02.tga payload, mfddwn03.tga
    /// enemy armor), which the original loaded each time it drew one.
    /// </summary>
    MCGuiPort* InfoViewBackgrounds[3] = {};
    /// <summary>Port: the mission timer as last written (once a second), and whether it is red (time up).</summary>
    char MapTimeText[16] = {};
    bool MapTimeRed = false;
    /// <summary>Port: set once the timer has been written.</summary>
    bool MapTimeShown = false;
};

/// <summary>Shows or hides the command palette.</summary>
void TogglePalette();
/// <summary>Event routine of the chat tab blinkers.</summary>
void BlinkerHandleEvent(MCGuiObject* obj, MCGuiEvent* event);
/// <summary>Event routines of the map scroll buttons.</summary>
void TmcUp(MCGuiObject* obj, MCGuiEvent* event);
void TmcLeft(MCGuiObject* obj, MCGuiEvent* event);
void TmcDown(MCGuiObject* obj, MCGuiEvent* event);
void TmcRight(MCGuiObject* obj, MCGuiEvent* event);
/// <summary>Callback of the zoom-in button (at most 8x).</summary>
void TmcZoomIn();
/// <summary>Callback of the zoom-out button.</summary>
void TmcZoomOut();
/// <summary>Callbacks of the info page's data buttons.</summary>
void ArmorFrontButton();
void PayloadButton();
void RearButton();
/// <summary>Page switches.</summary>
void MapSwitch();
void SalvageSwitch();
void InfoSwitch();
void MissionSwitch();
/// <summary>Event routine of the command palette buttons.</summary>
void ToolPaletteButtonEvent(MCGuiObject* obj, MCGuiEvent* event);
/// <summary>Event routines of the page tabs.</summary>
void TabStripEvent(MCGuiObject* obj, MCGuiEvent* event);
void TabTopEvent(MCGuiObject* obj, MCGuiEvent* event);
void TabBottomEvent(MCGuiObject* obj, MCGuiEvent* event);
/// <summary>qsort comparison of the info page's weapon list.</summary>
int CompareWeapons(const void* a, const void* b);

/// <summary>The interface mode of each command palette button (<c>IntMode</c> values: 15, 14, 13, 12, 19, 17, 3, 53).</summary>
extern int32_t ButtonActions[8];
/// <summary>Colours of the range circles.</summary>
extern int16_t RangeColorArray[4];
/// <summary>"Calling ..." text of the video window.</summary>
extern char CallingText[64];
/// <summary>The unit status strings of the info page (string table 0x78-0x7b).</summary>
extern std::string StatusString[4];
/// <summary>The unit type strings of the info page (string table 0x7c-0x80).</summary>
extern std::string TypeString[5];
/// <summary>The world point at the centre of the tactical map.</summary>
extern MCVector3D TacMapCenter;
/// <summary>Salvage objects listed (mirrors TacticalMap::numSalvage).</summary>
extern int32_t RealSalvageCount;
extern float TacFrameLength;
extern int OnNow;
