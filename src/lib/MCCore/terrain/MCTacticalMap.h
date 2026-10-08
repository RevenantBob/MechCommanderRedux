#pragma once

#include "gui/abutton.h"
#include "gui/asystem.h"
#include "gui/MCGuiOwned.h"
#include "object/MCObjectWatcher.h"
#include "platform/MCRegisteredBlock.h"
#include "platform/MCWin32Defs.h"
#include "terrain/MCArtilleryButton.h"
#include "terrain/MCToolPalButton.h"
#include "terrain/MCVideoWindow.h"

class MCGuiChatWindow;
class MCGuiEvent;
class MCGuiPort;
class MCGuiScrollTextObject;
class MCGameObject;
class MCMechWarrior;
class MCVector3D;

/// <summary>What the tactical map's MFD shows (<see cref="MCTacticalMap::SetDisplayType"/>).</summary>
enum class MCTacmapPage : int32_t
{
    /// <summary>The map.</summary>
    Map = 0,
    /// <summary>The selected unit's data.</summary>
    Info = 1,
    /// <summary>The mission objectives.</summary>
    Mission = 2,
    /// <summary>The salvage list, or the chat window in multiplayer.</summary>
    Salvage = 3
};

/// <summary>
/// The tactical map's projection: how a world position maps to the map's pixels at a zoom and scroll. The map is
/// drawn turned 45 degrees (north-east up), the whole map's diagonal across the 130-pixel map area at 1x.
/// </summary>
struct MCTacmapProjection
{
    /// <summary>Meters per map pixel at the current zoom.</summary>
    float MetersPerPixel = 0.0f;
    /// <summary>Zoom factor: 1, 2, 4 or 8.</summary>
    int32_t Zoom = 1;
    /// <summary>The map picture's size (from its TGA).</summary>
    int32_t MapWidth = 0;
    int32_t MapHeight = 0;
    /// <summary>The map picture's scroll position.</summary>
    int32_t ScrollX = 0;
    int32_t ScrollY = 0;

    /// <summary>
    /// Converts a world position to map pixels: with <paramref name="scrolled"/>, MFD pixels at the zoom and scroll;
    /// without, pixels of the whole map's 130-pixel square, whatever the zoom. Z is scaled the same way.
    /// </summary>
    void WorldToMap(MCVector3D& pos, bool scrolled) const;

    /// <summary>The inverse of <see cref="WorldToMap"/> on the ground plane (Z becomes 0).</summary>
    void MapToWorld(MCVector3D& pos, bool scrolled) const;
};

/// <summary>The tactical map's salvage list: the salvageable objects, in the order they were added.</summary>
class MCSalvageList
{
public:
    /// <summary>Adds <paramref name="obj"/> unless it is listed.</summary>
    /// <returns>Whether it was added.</returns>
    bool Add(MCGameObject* obj);

    /// <summary>Removes <paramref name="obj"/>.</summary>
    /// <returns>Whether it was listed.</returns>
    bool Remove(MCGameObject* obj);

    /// <summary>Whether <paramref name="obj"/> is listed.</summary>
    bool Contains(const MCGameObject* obj) const;

    /// <summary>Removes the objects <paramref name="drop"/> picks.</summary>
    template <typename Predicate> void RemoveIf(Predicate drop) { std::erase_if(_Objects, drop); }

    size_t size() const { return _Objects.size(); }
    MCGameObject* operator[](size_t index) const { return _Objects[index]; }
    auto begin() const { return _Objects.begin(); }
    auto end() const { return _Objects.end(); }

private:
    std::vector<MCGameObject*> _Objects;
};

/// <summary>
/// The tactical map: the MFD in the corner of the mission screen, with the revealed map, the units, the command
/// palette, the support buttons, and the info/mission/salvage (or chat) pages.
/// </summary>
/// <remarks>
/// Made by <see cref="MCTerrain::Load"/> (<see cref="TacticalMap"/>). Rectangles are (left, top, right, bottom) in the
/// MFD's coordinates.
/// </remarks>
class MCTacticalMap : public MCGuiObject
{
public:
    MCTacticalMap();
    /// <summary>Frees the watcher.</summary>
    ~MCTacticalMap() override;

    /// <summary>Destroys every child, port and string table.</summary>
    void Destroy() override;

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

    void Enter() override { MouseInside = true; }

    void Leave() override { MouseInside = false; }

    void Display() override;

    void HideMe(int hide) override;

    /// <summary>Draws <c>&lt;terrainPath&gt;&lt;name&gt;.gif</c> (the pre-revealed parts) into the fog of war.</summary>
    void SetRevealedBitmap(std::string_view fileName);

    /// <summary>
    /// Builds the MFD at (<paramref name="xPos"/>, <paramref name="yPos"/>): the map ports (the map picture, and
    /// the fog-of-war port that aliases the visible bits), the backgrounds, tabs, buttons, pages and string tables.
    /// </summary>
    int32_t Init(int32_t xPos, int32_t yPos);

    /// <summary>
    /// Converts a world position to tactical-map pixels (rotated 45 degrees): with <paramref name="scrolled"/>, MFD
    /// pixels at the current zoom and scroll; without, pixels of the whole map's 130-pixel square
    /// (<see cref="MCTacmapProjection::WorldToMap"/>).
    /// </summary>
    void WorldToTacMap(MCVector3D& pos, bool scrolled);

    /// <summary>The inverse of <see cref="WorldToTacMap"/>, standing the point on the ground.</summary>
    void TacMapToWorld(MCVector3D& pos, bool scrolled);

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
    void SetDisplayType(MCTacmapPage type);

    void CenterOnObject(MCGameObject* obj);

    /// <summary>Scrolls the map picture; each axis only moves if the zoomed view stays on the picture.</summary>
    void ScrollMap(int32_t dx, int32_t dy);

    /// <summary>
    /// Jumps the scroll to (<paramref name="x"/>, <paramref name="y"/>), then steps back towards the old position (by
    /// the scroll speed) until the view is on the picture.
    /// </summary>
    void SetScrollMapPosition(int32_t x, int32_t y);

    /// <summary>The screen area the video window's picture occupies (below its name line).</summary>
    tagRECT GetVideoRect();

    /// <summary>Adds a salvageable object to the salvage page.</summary>
    /// <returns>Nonzero (-1) when it is (now) listed.</returns>
    int AddSalvage(MCGameObject* obj);

    /// <summary>Removes an object from the salvage list (and rebuilds the page if <paramref name="refresh"/>).</summary>
    /// <returns>Nonzero (-1) when it was listed.</returns>
    int RemoveSalvage(MCGameObject* obj, int refresh);

    /// <summary>Drops salvage that has been destroyed, then rebuilds the page.</summary>
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

    /// <summary>The map's projection at its zoom and scroll.</summary>
    MCTacmapProjection Projection() const
    {
        return MCTacmapProjection{MetersPerPixel, Zoom, MapWidth, MapHeight, ScrollX, ScrollY};
    }

    /// <summary>
    /// How far <paramref name="pos"/> lies off the tactical map: <paramref name="pos"/> minus the nearest point on it
    /// (in world units), or zero when it is on the map.
    /// </summary>
    MCVector3D PositionOnMap(MCVector3D pos);

    /// <summary>Passes a chat message to the chat window and blinks the chat tab.</summary>
    void HandleChatMessage(uint32_t fromID, const void* message);

    /// <summary>Arms (or disarms) support button <paramref name="button"/> as if clicked.</summary>
    void ActivateArtillery(int32_t button, int arm);

    /// <summary>Shows or hides the command palette.</summary>
    void TogglePalette();

    /// <summary>Puts <paramref name="text"/> in the status line, unless a support button holds it.</summary>
    void ShowStatus(const std::string* text);

    /// <summary>Lets a support button give up the status line and the targeting cursor.</summary>
    void ReleaseStatusLine();

    /// <summary>The zoom-in button's click area, which takes the event whole.</summary>
    tagRECT ZoomInRect{};
    /// <summary>The zoom-out button's click area, which takes the event whole.</summary>
    tagRECT ZoomOutRect{};
    /// <summary>The map area (6, 0x22, 0x87, 0xa3).</summary>
    tagRECT MapRect{};
    /// <summary>A text page's click areas, placed by SetDisplayType: the scroll-up button, the scroll-down one, the
    /// track between.</summary>
    std::array<tagRECT, 3> PageRects{};
    /// <summary>Meters per map pixel at the current zoom.</summary>
    float MetersPerPixel = 0.0f;
    /// <summary>Vertices along the map's side (verticesBlockSide * blocksMapSide).</summary>
    int32_t MapVertexSide = 0;
    /// <summary>The map's diagonal in meters.</summary>
    float MapDiagonal = 0.0f;
    /// <summary>Zoom factor: 1, 2, 4 or 8.</summary>
    int32_t Zoom = 1;
    /// <summary>The map's scroll position.</summary>
    int32_t ScrollX = 0;
    int32_t ScrollY = 0;
    /// <summary>Accumulated by the zoom buttons (half the map side over the zoom).</summary>
    int32_t ZoomOffset = 0;
    /// <summary>The map picture's size (from its TGA).</summary>
    int32_t MapWidth = 0;
    int32_t MapHeight = 0;
    /// <summary>Draw sensor/range circles around the units.</summary>
    bool ShowRanges = true;
    /// <summary>The map picture (<c>&lt;terrainName&gt;.tga</c>).</summary>
    MCGuiOwned<MCGuiPort> MapPort;
    /// <summary>The fog-of-war port: its bitmap is the visible bits of the home side.</summary>
    MCGuiOwned<MCGuiPort> VisibilityPort;
    /// <summary>Background of the map page (mfdmwn00.tga).</summary>
    MCGuiOwned<MCGuiPort> MapBackground;
    /// <summary>Background of the info page (mfddwn00.tga).</summary>
    MCGuiOwned<MCGuiPort> InfoBackground;
    /// <summary>Background of the mission page (mfdbwn00.tga).</summary>
    MCGuiOwned<MCGuiPort> MissionBackground;
    /// <summary>Background of the salvage page: mfdswn01.tga in multiplayer, the mission one in single player.</summary>
    MCGuiPort* SalvageBackground = nullptr;
    /// <summary>Ports of the info page's pilot/passenger pictures.</summary>
    std::array<MCGuiOwned<MCGuiPort>, 4> InfoPorts;
    /// <summary>The tab strip.</summary>
    MCGuiOwned<MCGuiObject> TabStrip;
    /// <summary>The top tab.</summary>
    MCGuiOwned<MCGuiObject> TabTop;
    /// <summary>The bottom tab.</summary>
    MCGuiOwned<MCGuiObject> TabBottom;
    /// <summary>The bottom tab shows its highlighted picture.</summary>
    bool TabHighlighted = false;
    /// <summary>The map area as a pane on the MFD's window.</summary>
    MCPane MapPane = {};
    /// <summary>The salvage objects (no limit: the original's list held 100, OB-052).</summary>
    MCSalvageList Salvage;
    /// <summary>The salvage page.</summary>
    MCGuiOwned<MCGuiScrollTextObject> SalvageText;
    /// <summary>The info/mission page text.</summary>
    MCGuiOwned<MCGuiScrollTextObject> InfoText;
    /// <summary>The pilot video window.</summary>
    MCGuiOwned<MCVideoWindow> VideoWindow;
    /// <summary>The unit shown on the info page.</summary>
    MCGameObject* InfoObject = nullptr;
    /// <summary>The support buttons.</summary>
    std::array<MCGuiOwned<MCArtilleryButton>, 4> ArtilleryButtons;
    /// <summary>The palette toggle button.</summary>
    MCGuiOwned<MCGuiToolButton> PaletteButton;
    /// <summary>The command palette (the last one is the zoom/palette-toggle button).</summary>
    std::array<MCGuiOwned<MCToolPalButton>, 8> ToolButtons;
    /// <summary>The scroll buttons (up, left, down, right) and zoom in/out.</summary>
    std::array<MCGuiOwned<MCGuiButton>, 6> ScrollButtons;
    /// <summary>The command palette frame.</summary>
    MCGuiOwned<MCGuiObject> PaletteFrame;
    /// <summary>The command palette's lower half.</summary>
    MCGuiOwned<MCGuiObject> PaletteBottom;
    /// <summary>The info page's data buttons (armor front, rear, payload).</summary>
    std::array<MCGuiOwned<MCGuiToolButton>, 3> DataButtons;
    /// <summary>Info page scroll markers.</summary>
    MCGuiOwned<MCGuiObject> ScrollUpMarker;
    MCGuiOwned<MCGuiObject> ScrollDownMarker;
    /// <summary>The chat tab's blinkers (multiplayer).</summary>
    MCGuiOwned<MCGuiObject> ChatBlinkerOn;
    MCGuiOwned<MCGuiObject> ChatBlinkerOff;
    /// <summary>The chat window (multiplayer).</summary>
    MCGuiOwned<MCGuiChatWindow> ChatWindow;
    /// <summary>A chat message is unread (the tab blinks).</summary>
    bool ChatPending = false;
    /// <summary>The current page.</summary>
    MCTacmapPage DisplayType = MCTacmapPage::Map;
    /// <summary>The part diagram shapes of the info page.</summary>
    MCRegisteredBlock PartShapes;
    /// <summary>The part diagram's colour of each armor location (GetColors).</summary>
    std::array<uint8_t, 11> ArmorColors{};
    /// <summary>The part diagram's colour of each internal structure (body) location (GetColors).</summary>
    std::array<uint8_t, 13> BodyColors{};
    /// <summary>Watches <see cref="InfoObject"/> so it is cleared when the object goes away.</summary>
    MCBaseObjectWatcher InfoWatcher;
    /// <summary>MCPort::Milliseconds of the last page refresh.</summary>
    uint32_t LastRefreshTime = 0;
    /// <summary>Time of the last timer update (-999 initially).</summary>
    float LastMapTime = -999.0f;
    /// <summary>The status line's text (a hovered button's help), or null for the default text.</summary>
    const std::string* StatusText = nullptr;
    /// <summary>A support button holds the status line.</summary>
    bool StatusLocked = false;
    /// <summary>The info page's data view (<see cref="SetDataDisplayMode"/>).</summary>
    char DataDisplayMode = 0;
    /// <summary>The mouse is over the MFD.</summary>
    bool MouseInside = false;
    /// <summary>The map is being dragged.</summary>
    bool MapDragging = false;
    /// <summary>Set once the objectives' areas have been revealed in the fog of war.</summary>
    bool ObjectivesRevealed = false;
    /// <summary>Colour remap for the destroyed parts of the diagram (0xff = unchanged; 0xe6 and 0xe8 map to 0x13).</summary>
    MCRegisteredBlock ColorRemap;
    /// <summary>"Calling ..." text of an armed support button.</summary>
    std::string CallingText;
    /// <summary>The unit status strings of the info page (string table 0x78-0x7b).</summary>
    std::array<std::string, 4> StatusStrings;
    /// <summary>The unit type strings of the info page (string table 0x7c-0x80).</summary>
    std::array<std::string, 5> TypeStrings;
    /// <summary>The markers' blink: lit, and the time since the last toggle (kept from mission to mission, as the
    /// original's globals were).</summary>
    static bool MarkersLit;
    static float MarkerBlinkTime;

    /// <summary>
    /// Port: the info page's data view backgrounds (mfddwn01.tga home armor, mfddwn02.tga payload, mfddwn03.tga
    /// enemy armor), which the original loaded each time it drew one.
    /// </summary>
    std::array<MCGuiOwned<MCGuiPort>, 3> InfoViewBackgrounds;
    /// <summary>Port: the mission timer as last written (once a second), and whether it is red (time up).</summary>
    std::string MapTimeText;
    bool MapTimeRed = false;
    /// <summary>Port: set once the timer has been written.</summary>
    bool MapTimeShown = false;

private:
    /// <summary>Appends the salvage line of <paramref name="obj"/> to the salvage page.</summary>
    void AddSalvageString(MCGameObject* obj);

    /// <summary>Draws the selected unit's effectiveness bar.</summary>
    void DrawBar();

    /// <summary>Draws the selected unit's armor/payload diagram on the info page.</summary>
    void DrawParts();

    /// <summary>Picks the colours of the part diagram from the unit's damage.</summary>
    void GetColors();

    void DrawPilot(MCMechWarrior* pilot);

    void DrawWeapons();

    /// <summary>Draws the map page: the timer, the map, the fog of war, the camera views and the objects.</summary>
    void DrawMapPage();

    /// <summary>The background of the salvage page, owned in multiplayer.</summary>
    MCGuiOwned<MCGuiPort> _OwnedSalvageBackground;
};

/// <summary>Shows or hides the command palette.</summary>
void TogglePalette();
