#include "stdafx.h"
#include "terrain/terrmap.h"
#include "camera/camera.h"
#include "color/color.h"
#include "engine/bitflag.h"
#include "engine/font.h"
#include "gui/afont.h"
#include "gui/aport.h"
#include "gui/atextbox.h"
#include "gui/awindow.h"
#include "iface/iface.h"
#include "lib/aerror.h"
#include "lib/cident.h"
#include "lib/cvmath.h"
#include "lib/file.h"
#include "logistics/logbri.h"
#include "logistics/logmain.h"
#include "main/main.h"
#include "mission/mission.h"
#include "mission/scenario.h"
#include "network/multplyr.h"
#include "object/artlry.h"
#include "object/bldng.h"
#include "object/cmponent.h"
#include "object/comndr.h"
#include "object/contact.h"
#include "object/object.h"
#include "object/objque.h"
#include "object/tbldng.h"
#include "object/gameobj.h"
#include "object/gvehicl.h"
#include "object/mover.h"
#include "object/team.h"
#include "object/warrior.h"
#include "platform/MCRenderer.h"
#include "platform/MCWin32Defs.h"
#include "sound/radio.h"
#include "sound/soundsys.h"
#include "terrain/terrain.h"
#include "vfx/vfx.h"
#include "vfx/vfxfuncs.h"

int32_t ButtonActions[8] = {15, 14, 13, 12, 19, 17, 3, 53};
int16_t RangeColorArray[4] = {0x0e, 0xe5, 0xee, 0x14};
char CallingText[64] = {};
std::string StatusString[4];
std::string TypeString[5];
MCVector3D TacMapCenter;
int32_t RealSalvageCount = 0;
float TacFrameLength = 0.0f;
int OnNow = 0;

namespace
{
    /// <summary>The event types of aEvent::type the tactical map handles.</summary>
    constexpr int32_t EVENT_LEFT_DOWN = 1;
    constexpr int32_t EVENT_LEFT_UP = 4;
    constexpr int32_t EVENT_MOUSE_MOVE = 7;
    constexpr int32_t EVENT_KEY_UP = 8;
    constexpr int32_t EVENT_TIMER = 0x13;
    constexpr int32_t EVENT_CALLBACK = 0x14;
    constexpr int32_t EVENT_ZOOM_IN = 0x1a;
    constexpr int32_t EVENT_ZOOM_OUT = 0x1b;

    /// <summary>The auto-repeat timers of the scroll buttons: the first delay, then the repeat.</summary>
    constexpr int16_t SCROLL_START_TIMER = 4;
    constexpr int16_t SCROLL_REPEAT_TIMER = 5;

    /// <summary>The support command ids (and the button's strike count).</summary>
    constexpr int32_t STRIKE_SMALL = 0xf9;
    constexpr int32_t STRIKE_LARGE = 0xf8;
    constexpr int32_t STRIKE_SENSOR = 0xfa;
    constexpr int32_t STRIKE_CAMERA_DRONE = 0x204;

    /// <summary>The palette button's action that toggles the zoom instead of choosing a mode.</summary>
    constexpr int32_t ACTION_TOGGLE_ZOOM = 0x35;

    /// <summary>The number of the command palette's mode buttons (the eighth toggles the zoom).</summary>
    constexpr int32_t NUM_MODE_BUTTONS = 7;

    /// <summary>The tactical map's centre in MFD pixels (the floats at 0x0078426c and 0x00784268).</summary>
    constexpr float MAP_CENTER_X = 71.0f;
    constexpr float MAP_CENTER_Y = 99.0f;
    /// <summary>1/260: meters per pixel at zoom 1 from the map's diagonal (0x00784274).</summary>
    constexpr float DIAGONAL_TO_PIXELS = 1.0f / 260.0f;
    /// <summary>1/130: the map picture's pixels per MFD pixel at zoom 1 (0x00784270).</summary>
    constexpr float PICTURE_TO_PIXELS = 1.0f / 130.0f;
    /// <summary>cos 45 degrees (0x0077f0cc; 0x0077f0c4 holds its negative).</summary>
    constexpr float MAP_ROTATION = 0.70710677f;
    /// <summary>Half the map area's side, and the side (0x007849f8, 0x007849fc).</summary>
    constexpr float MAP_HALF_SIDE = 65.0f;
    constexpr float MAP_PICTURE_SIDE = 130.0f;
    /// <summary>The map area's top-left corner in MFD pixels (0x0077a538, 0x0078427c).</summary>
    constexpr float MAP_LEFT = 6.0f;
    constexpr float MAP_TOP = 34.0f;

    /// <summary>Draws the map page: the timer, the map, the fog of war, the camera views and the objects.</summary>
    /// <remarks>Part of TacticalMap::display.</remarks>
    void DrawMapPage(MCTacticalMap* map);

    /// <summary>A weapon of the info page's list (drawWeapons builds an array of them; 4 bytes).</summary>
    struct MCWeaponEntry
    {
        /// <summary>The weapon's MasterComponentList index.</summary>
        uint8_t MasterID;
        /// <summary>0xff damaged, 0 no shots left, 1 ready.</summary>
        uint8_t State;
        /// <summary>The range bracket (0, 10000, 20000) plus the damage: the sort key.</summary>
        int16_t SortKey;
    };

    /// <summary>The contacts drawObjects fetches from the home team (the unnamed 0x00809f78, 0x400 bytes).</summary>
    MCGameObject* ContactList[256] = {};

    /// <summary>A mech, vehicle, elemental or other mover (the classes that have a pilot and a sensor).</summary>
    bool IsMoverClass(MCGameObject* obj)
    {
        return obj->ObjectClass == BATTLEMECH || obj->ObjectClass == GROUNDVEHICLE || obj->ObjectClass == ELEMENTAL ||
               obj->ObjectClass == MOVER;
    }

    /// <summary>
    /// Draws a contact's sensor range around its dot: red, or white for the home side's alignment, or yellow while
    /// the sensor is weakened.
    /// </summary>
    void DrawSensorRange(MCTacticalMap* map, MCGameObject* obj, int32_t xPos, int32_t yPos, int32_t homeAlignment)
    {
        MCSensorSystem* sensor = static_cast<MCMover*>(obj)->SensorSystem;
        float range = -1.0f;

        if (sensor != nullptr && sensor->Enabled() != 0)
        {
            range = (sensor->GetSkilledRange() * WorldUnitsPerMeter) / map->MetersPerPixel;
        }

        if (range <= 0.0)
        {
            return;
        }

        uint8_t color = 0xef;

        if (obj->GetAlignment() == homeAlignment)
        {
            color = 0x1f;
        }

        if (sensor->Multiplier < 1.0)
        {
            color = 0xf2;
        }

        const auto radius = static_cast<int32_t>(range);
        AGEllipseDraw(&map->MapPane, xPos, yPos, radius, radius, color);
    }

    /// <summary>
    /// Whether a scroll position keeps the zoomed view's both edges on the map picture (<paramref name="side"/>
    /// pixels along that axis).
    /// </summary>
    bool ScrollInPicture(int32_t scroll, int32_t side, int32_t zoom)
    {
        const int32_t half = side >> 1;
        const int32_t zoomedHalf = half / zoom;
        const int32_t low = (scroll - zoomedHalf) + half;
        const int32_t high = (zoomedHalf - half) + side + scroll;
        return low >= 0 && low <= side - 1 && high >= 0 && high <= side - 1;
    }

    /// <summary>
    /// Places a text page's click areas along the right edge (the scroll-up button from <paramref name="upTop"/>,
    /// the scroll-down one from <paramref name="downTop"/>, the track between) and moves the markers there.
    /// </summary>
    void SetPageRects(MCTacticalMap* map, int32_t upTop, int32_t upBottom, int32_t downTop, int32_t downBottom)
    {
        int32_t (&rects)[3][4] = map->PageRects;
        rects[0][0] = 0x7d;
        rects[0][1] = upTop;
        rects[0][2] = 0x88;
        rects[0][3] = upBottom;
        map->ScrollUpMarker->MoveTo(0x7d, upTop, 0);
        rects[1][0] = 0x7d;
        rects[1][1] = downTop;
        rects[1][2] = 0x88;
        rects[1][3] = downBottom;
        map->ScrollDownMarker->MoveTo(0x7d, downTop, 0);
        rects[2][0] = 0x7d;
        rects[2][1] = upBottom;
        rects[2][2] = 0x88;
        rects[2][3] = downTop;
    }

    /// <summary>The part diagram's colour of a location from what is left of it: green, yellow, orange or red.</summary>
    uint8_t DamageColor(float current, uint8_t maximum)
    {
        const auto percent = static_cast<int16_t>(std::floor((current / static_cast<float>(maximum)) * 100.0f));

        if (percent >= 0x4c)
        {
            return 0xb;
        }

        if (percent >= 0x33)
        {
            return 0xf2;
        }

        if (percent >= 0x1a)
        {
            return 0xeb;
        }

        return 0xef;
    }

    /// <summary>
    /// The table the part diagram's shape is drawn through for a <see cref="DamageColor"/> colour: rows of the fade
    /// palettes past the haze levels, or for a destroyed location (0x19) the map's colour remap.
    /// </summary>
    uint8_t* PartColorTable(MCTacticalMap* map, uint8_t color)
    {
        const int32_t row = GamePalette->NumBitmapHazeLevels;
        uint8_t* fades = GamePalette->FadePalettes.get();

        switch (color)
        {
            case 0xb:
                return fades + (row + 10) * 0x200;
            case 0x19:
                return map->ColorRemap;
            case 0xeb:
                return fades + row * 0x200 + 0x1500;
            case 0xef:
                return fades + row * 0x200 + 0x1700;
            case 0xf2:
                return fades + (row + 11) * 0x200;
            default:
                return fades + (row + 12) * 0x200;
        }
    }

    /// <summary>Destroys and deletes a child object, and clears the pointer.</summary>
    template <typename Object> void DestroyChild(Object*& obj)
    {
        if (obj == nullptr)
        {
            return;
        }

        obj->Destroy();
        delete obj;
        obj = nullptr;
    }

    /// <summary>Destroys and deletes a port, and clears the pointer.</summary>
    void DestroyPort(MCGuiPort*& port)
    {
        if (port == nullptr)
        {
            return;
        }

        port->Destroy();
        delete port;
        port = nullptr;
    }

    MCTacticalMap* TacMap()
    {
        return MCTerrain::TerrainTacticalMap;
    }

    /// <summary>A command palette mode button, or null out of range (the binary checks each index).</summary>
    MCToolPalButton* ModeButton(int32_t index)
    {
        return (index < 0 || index > 7) ? nullptr : TacMap()->ToolButtons[index];
    }

    /// <summary>The support button's commander strike count.</summary>
    int32_t StrikesLeft(int32_t commandId, bool& known)
    {
        known = true;

        switch (commandId)
        {
            case STRIKE_SENSOR:
                return HomeCommander->NumSensorStrikes;
            case STRIKE_LARGE:
                return HomeCommander->NumLargeStrikes;
            case STRIKE_SMALL:
                return HomeCommander->NumSmallStrikes;
            case STRIKE_CAMERA_DRONE:
                return HomeCommander->NumCameraDrones;
            default:
            {
                known = false;
                return 0;
            }
        }
    }

    /// <summary>Lets a support button give up the status line and the targeting cursor.</summary>
    void ReleaseStatusLine()
    {
        MCTacticalMap* map = TacMap();
        map->StatusLocked = 0;
        map->StatusDirty = -1;
        map->StatusText = nullptr;
        Application->CursorHidden = 0;
        Application->SetCurrentCursor(static_cast<MCCursorType>(0));
    }

    /// <summary>
    /// Shared by the four scroll buttons: scroll once on press, then after the interface's scrollStart delay
    /// repeat five times as fast until release.
    /// </summary>
    void ScrollButtonEvent(MCGuiObject* obj, MCGuiEvent* event, int32_t dx, int32_t dy)
    {
        if (event->Type == EVENT_LEFT_DOWN)
        {
            Application->AddTimer(obj, SCROLL_START_TIMER, TheInterface->ScrollStart, 0, 0, 0);
            TacMap()->ScrollMap(dx, dy);
        }
        else if (event->Type == EVENT_LEFT_UP)
        {
            Application->RemoveTimer(obj, SCROLL_START_TIMER);
            Application->RemoveTimer(obj, SCROLL_REPEAT_TIMER);
        }
        else if (event->Type == EVENT_TIMER)
        {
            TacMap()->ScrollMap(dx, dy);

            if (event->Data == SCROLL_START_TIMER)
            {
                Application->RemoveTimer(obj, SCROLL_START_TIMER);
                Application->AddTimer(obj, SCROLL_REPEAT_TIMER, TheInterface->ScrollStart / 5, 0, 0, 0);
            }
        }
    }

    /// <summary>After a zoom, recentres the map on the main camera (unless at 1x) and redraws.</summary>
    void RecentreAfterZoom(MCTacticalMap* map)
    {
        map->MetersPerPixel = (map->MapDiagonal * DIAGONAL_TO_PIXELS) / static_cast<float>(map->Zoom);

        if (Eye == nullptr)
        {
            return;
        }

        MCVector3D center = Eye->Position;
        map->ScrollY = 0;
        map->ScrollX = 0;
        const float zoom = static_cast<float>(map->Zoom);
        const float scaleX = (static_cast<float>(map->MapWidth) * PICTURE_TO_PIXELS) / zoom;
        const float scaleY = (static_cast<float>(map->MapHeight) * PICTURE_TO_PIXELS) / zoom;

        if (map->Zoom == 1)
        {
            map->ScrollY = 0;
            map->ScrollX = 0;
            return;
        }

        map->WorldToTacMap(center, -1);
        center.X = (center.X - MAP_CENTER_X) * scaleX;
        center.Y = (center.Y - MAP_CENTER_Y) * scaleY;
        TacMap()->SetScrollMapPosition(static_cast<int32_t>(center.X), static_cast<int32_t>(center.Y));
    }

    /// <summary>Stops the pilot video (and its radio movie) when switching away from the map page.</summary>
    void StopVideo()
    {
        if (TacMap()->VideoWindow->Star == nullptr)
        {
            return;
        }

        MCRadioData* message = SoundSystem->CurrentMessage;

        if (message->MovieWindow != nullptr)
        {
            message->MovieWindow->EndSmackerMovie();
            delete message->MovieWindow;
            message->MovieWindow = nullptr;
            message->Movie = nullptr;
        }

        TacMap()->VideoWindow->SetStar(nullptr);
    }

    /// <summary>String <paramref name="id"/> of the string table.</summary>
    std::string LoadHeapString(uint32_t id)
    {
        char buffer[256];
        CLoadString(ThisInstance, id, buffer, 0xfe);
        return buffer;
    }

    /// <summary>Loads string <paramref name="id"/> into a help text (0x31 characters, as strncpy).</summary>
    void LoadHelpText(char* helpText, uint32_t id)
    {
        char buffer[256];
        CLoadString(ThisInstance, id, buffer, 0xfe);
        std::strncpy(helpText, buffer, 0x31);
    }

    /// <summary>
    /// A plain aObject child showing a picture. (The original loaded the picture into the object's own port; it is
    /// the object's background now, and the object draws itself.)
    /// </summary>
    MCGuiObject* MakePicture(MCGuiObject* parent, int32_t x, int32_t y, int32_t w, int32_t h, const char* picture)
    {
        auto* obj = new MCGuiObject;
        obj->SetDrawsLive();
        obj->Init(x, y, w, h, nullptr);
        obj->SetBackground(const_cast<char*>(picture));
        return obj;
    }

    /// <summary>A button with its up, down and gray pictures.</summary>
    template <typename Button>
    Button* MakeButton(int32_t x, int32_t y, int32_t w, int32_t h, const char* up, const char* down, const char* gray)
    {
        auto* button = new Button;
        button->Init(x, y, w, h, nullptr);
        button->SetUpPicture(const_cast<char*>(up));
        button->SetDownPicture(const_cast<char*>(down));

        if (gray != nullptr)
        {
            button->SetGrayPicture(const_cast<char*>(gray));
        }

        return button;
    }

    /// <summary>Loads a background port for one of the MFD pages.</summary>
    MCGuiPort* LoadBackground(const char* fileName, const char* error)
    {
        auto* background = new MCGuiPort;
        const int32_t result = background->Init(const_cast<char*>(fileName));
        Assert(result == 0, static_cast<uint32_t>(result), error);
        return background;
    }
}

auto TogglePalette() -> void
{
    MCTacticalMap* map = TacMap();

    if (map->IsShowing() == 0)
    {
        return;
    }

    MCGuiObject* frame = map->PaletteFrame;
    frame->ShowGuiWindow(frame->IsShowing() == 0);

    if (map->PaletteFrame->IsShowing() == 0)
    {
        SoundSystem->PlayDigitalSample(0x41, 1, nullptr, 0, 0);
    }
    else
    {
        // Opening the palette drops the chosen mode.
        for (int32_t i = 0; i < NUM_MODE_BUTTONS; i++)
        {
            if (ModeButton(i)->Pushed != 0)
            {
                ModeButton(i)->Pushed = 0;
                TheInterface->CurrentCommand = 0;
                TheInterface->CommandOneShot = 0;
            }
        }

        SoundSystem->PlayDigitalSample(0x40, 1, nullptr, 0, 0);
    }

    TacMap()->PaletteButton->Pushed = TacMap()->PaletteFrame->IsShowing();
}

auto BlinkerHandleEvent(MCGuiObject* obj, MCGuiEvent* event) -> void
{
    // The port's resize broadcast (0x12, see MCFollowWindowSize) already reaches every object, and has no position:
    // passed to what lies under (0, 0), the tactical map, it would come back here forever.
    if (event->Type == 0x12)
    {
        return;
    }

    // Hides itself to find what lies under it, and passes the event there.
    obj->ShowGuiWindow(0);
    MCGuiObject* under = ScreenWindow->FindObject(event->X, event->Y);
    obj->ShowGuiWindow(-1);
    under->HandleEvent(event);
}

auto TmcUp(MCGuiObject* obj, MCGuiEvent* event) -> void
{
    ScrollButtonEvent(obj, event, 0, -TheInterface->TacScrollSpeed);
}

auto TmcLeft(MCGuiObject* obj, MCGuiEvent* event) -> void
{
    ScrollButtonEvent(obj, event, TheInterface->TacScrollSpeed, 0);
}

auto TmcDown(MCGuiObject* obj, MCGuiEvent* event) -> void
{
    ScrollButtonEvent(obj, event, 0, TheInterface->TacScrollSpeed);
}

auto TmcRight(MCGuiObject* obj, MCGuiEvent* event) -> void
{
    ScrollButtonEvent(obj, event, -TheInterface->TacScrollSpeed, 0);
}

auto TmcZoomIn() -> void
{
    MCTacticalMap* map = TacMap();
    const int32_t oldZoom = map->Zoom;
    const int32_t mapSide = map->MapVertexSide;
    map->Zoom = oldZoom * 2;

    if (oldZoom * 2 < 9)
    {
        SoundSystem->PlayDigitalSample(0x44, 1, nullptr, 0, 0);
        map = TacMap();
        map->ZoomOffset += (mapSide >> 1) / map->Zoom;
    }
    else
    {
        map->Zoom = 8;
    }

    RecentreAfterZoom(map);
    map = TacMap();

    if (map->Zoom == 8)
    {
        MCGuiButton* zoomIn = map->ScrollButtons[4];
        zoomIn->Disabled = -1;
        map = TacMap();
    }

    MCGuiButton* zoomOut = map->ScrollButtons[5];
    zoomOut->Disabled = 0;

    // Zoomed in, the map can scroll.
    for (int32_t i = 0; i < 4; i++)
    {
        MCGuiButton* scroll = TacMap()->ScrollButtons[i];
        scroll->Disabled = 0;
    }

    TacMap()->RefreshPage();
}

auto TmcZoomOut() -> void
{
    MCTacticalMap* map = TacMap();
    map->ZoomOffset -= (map->MapVertexSide >> 1) / map->Zoom;

    if (map->ZoomOffset < 0)
    {
        map->ZoomOffset = 0;
    }

    const int32_t oldZoom = map->Zoom;
    map->Zoom = oldZoom >> 1;

    if ((oldZoom >> 1) == 0)
    {
        map->Zoom = 1;
    }
    else
    {
        SoundSystem->PlayDigitalSample(0x45, 1, nullptr, 0, 0);
        map = TacMap();
    }

    if (map->Zoom == 1)
    {
        // At 1x the whole map shows: no zooming out or scrolling.
        MCGuiButton* zoomOut = map->ScrollButtons[5];
        zoomOut->Disabled = -1;

        for (int32_t i = 0; i < 4; i++)
        {
            MCGuiButton* scroll = TacMap()->ScrollButtons[i];
            scroll->Disabled = -1;
            map = TacMap();
        }
    }

    MCGuiButton* zoomIn = map->ScrollButtons[4];
    zoomIn->Disabled = 0;
    map = TacMap();
    RecentreAfterZoom(map);
    map->RefreshPage();
}

auto ArmorFrontButton() -> void
{
    TacMap()->SetDataDisplayMode(0, 0);
}

auto PayloadButton() -> void
{
    TacMap()->SetDataDisplayMode(2, 0);
}

auto RearButton() -> void
{
    TacMap()->SetDataDisplayMode(1, 0);
}

auto MCToolPalButton::Enter() -> void
{
    MCTacticalMap* map = TacMap();

    if (map->StatusLocked == 0)
    {
        map->StatusDirty = -1;
        map->StatusText = HelpText;
    }
}

auto MCToolPalButton::Leave() -> void
{
    MCTacticalMap* map = TacMap();

    if (map->StatusLocked == 0)
    {
        map->StatusDirty = -1;
        map->StatusText = nullptr;
    }
}

auto MCArtilleryButton::Init(int32_t xPos, int32_t yPos, int32_t w, int32_t h, char* fileName) -> int32_t
{
    const int32_t result = MCGuiButton::Init(xPos, yPos, w, h, fileName);
    Armed = 0;
    KeyArmed = 0;
    Disabled = 0;
    return result;
}

auto MCArtilleryButton::Draw() -> void
{
    bool known = false;
    const int32_t before = StrikesLeft(CommandId, known);

    if (known)
    {
        Disabled = before < 1 ? -1 : 0;
    }

    MCGuiButton::Draw();

    // The count, in the button's corner (the buffer is the original's 4-byte local).
    char count[16] = {};
    const int32_t left = StrikesLeft(CommandId, known);

    if (known)
    {
        std::snprintf(count, sizeof(count), "%02i", left);
    }

    if (Disabled != 0)
    {
        GreyFont->WriteString(DisplayPort->Frame(), 0x13, 9, reinterpret_cast<uint8_t*>(count), -1);
        return;
    }

    if (Application->GrabbedObject() == this && Application->CurrentObject() == this)
    {
        WhiteFont->WriteString(DisplayPort->Frame(), 0x13, 9, reinterpret_cast<uint8_t*>(count), -1);
        return;
    }

    BlueFont->WriteString(DisplayPort->Frame(), 0x13, 9, reinterpret_cast<uint8_t*>(count), -1);
}

auto MCArtilleryButton::HandleEvent(MCGuiEvent* event) -> void
{
    if (Disabled != 0)
    {
        if (Application->GrabbedObject() == this)
        {
            Application->Release();
        }

        if (event->Type == EVENT_LEFT_DOWN)
        {
            SoundSystem->PlayDigitalSample(0x46, 1, nullptr, 0, 0);
        }

        return;
    }

    if (event->Type == EVENT_LEFT_DOWN)
    {
        if (Armed == 0)
        {
            Application->Grab(this);
            Draw();
        }

        return;
    }

    if (event->Type == EVENT_LEFT_UP)
    {
        if (Armed == 0)
        {
            // Armed: "Calling <strike>..." holds the status line until the target click.
            char format[256];
            CLoadString(ThisInstance, 0x98, format, 0xfe);
            std::snprintf(CallingText, sizeof(CallingText), format, HelpText);
            MCTacticalMap* map = TacMap();

            if (map->StatusLocked == 0)
            {
                map->StatusDirty = -1;
                map->StatusText = CallingText;
            }

            map->StatusLocked = -1;
            Armed = -1;
            Application->SetCurrentCursor(static_cast<MCCursorType>(9));
            Application->CursorHidden = -1;
            Draw();
            return;
        }

        const int32_t screenX = event->X;
        const int32_t screenY = event->Y;
        POINT inMap;
        inMap.x = screenX - TacMap()->GlobalX();
        inMap.y = screenY - TacMap()->GlobalY();
        Armed = 0;
        Application->Release();
        Draw();
        const RECT* mapArea = reinterpret_cast<const RECT*>(TacMap()->MapRect);

        if (PtInRect(mapArea, inMap) == 0 || TacMap()->DisplayType != TACMAP_MAP)
        {
            // Outside the tactical map: the click must land in the active view.
            MCGuiObject* target = ScreenWindow->FindObject(screenX, screenY);

            if (target != MainHolder->GetActivePane() && target != TheInterface->MechBar)
            {
                Armed = 0;
                Application->Release();
                ReleaseStatusLine();
                Draw();
                return;
            }

            MCGuiObject* pane = MainHolder->GetActivePane()->PointInside(screenX, screenY) == 0
                                    ? MainHolder->GetInactivePane()
                                    : MainHolder->GetActivePane();

            if (pane == nullptr)
            {
                return;
            }

            MCCamera* camera = pane->GetCamera();

            if (camera == nullptr)
            {
                return;
            }

            // Port: on the view's world surface, through the zoom.
            MCVector2D screenPos = MCWindowPoint(pane, screenX, screenY);
            MCVector3D target3d;
            camera->InverseProject(screenPos, target3d);
            TheInterface->CallStrike(CommandId, &target3d, nullptr, -1, 0, -1.0f);
        }
        else
        {
            MCVector3D target3d(static_cast<float>(screenX - TacMap()->GlobalX()),
                                static_cast<float>(screenY - TacMap()->GlobalY()), 0.0f);
            TacMap()->TacMapToWorld(target3d, -1);
            TheInterface->CallStrike(CommandId, &target3d, nullptr, -1, 0, -1.0f);
        }

        MCTacticalMap* map = TacMap();
        map->StatusLocked = 0;
        map->StatusDirty = -1;
        map->StatusText = nullptr;
        Application->CursorHidden = 0;
        Application->SetCurrentCursor(static_cast<MCCursorType>(0));
        return;
    }

    if (event->Type == EVENT_KEY_UP)
    {
        // Backspace or Escape disarms.
        if (event->Key == 8 || event->Key == 0x1b)
        {
            Application->Release();
            Armed = 0;
            ReleaseStatusLine();
            Draw();
        }

        return;
    }

    // The port's resize broadcast (0x12) has no position: passed to the tactical map under (0, 0), it would come back
    // here forever (see BlinkerHandleEvent).
    if (event->Type == 0x12)
    {
        return;
    }

    // Anything else goes to what lies under the mouse.
    MCGuiObject* under = ScreenWindow->FindObject(event->X, event->Y);

    if (under != this && under != nullptr)
    {
        event->Target = under;
        under->HandleEvent(event);
    }
}

auto MCArtilleryButton::Enter() -> void
{
    MCTacticalMap* map = TacMap();

    if (map->StatusLocked == 0)
    {
        map->StatusDirty = -1;
        map->StatusText = HelpText;
    }
}

auto MCArtilleryButton::Leave() -> void
{
    MCTacticalMap* map = TacMap();

    if (map->StatusLocked == 0)
    {
        map->StatusDirty = -1;
        map->StatusText = nullptr;
    }
}

auto MapSwitch() -> void
{
    TacMap()->HideMe(0);

    if (TacMap()->DisplayType != TACMAP_MAP)
    {
        TacMap()->SetDisplayType(TACMAP_MAP);
    }
}

auto SalvageSwitch() -> void
{
    TacMap()->HideMe(0);

    if (TacMap()->DisplayType != TACMAP_SALVAGE)
    {
        TacMap()->SetDisplayType(TACMAP_SALVAGE);
        StopVideo();
    }
}

auto InfoSwitch() -> void
{
    TacMap()->HideMe(0);

    if (TacMap()->DisplayType != TACMAP_INFO)
    {
        TacMap()->SetDisplayType(TACMAP_INFO);
        StopVideo();
    }
}

auto MissionSwitch() -> void
{
    TacMap()->HideMe(0);

    if (TacMap()->DisplayType != TACMAP_MISSION)
    {
        TacMap()->SetDisplayType(TACMAP_MISSION);
        StopVideo();
    }
}

auto ToolPaletteButtonEvent(MCGuiObject* obj, MCGuiEvent* event) -> void
{
    if (event->Type != EVENT_CALLBACK)
    {
        return;
    }

    auto* button = static_cast<MCToolPalButton*>(obj);
    const int32_t action = button->Action;

    if (action == ACTION_TOGGLE_ZOOM)
    {
        SoundSystem->PlayDigitalSample(0x2f, 1, nullptr, 0, 0);
        ToggleZoom();
        return;
    }

    if (button->Pushed != 0)
    {
        // One mode at a time.
        SoundSystem->PlayDigitalSample(0x35, 1, nullptr, 0, 0);

        for (int32_t i = 0; i < NUM_MODE_BUTTONS; i++)
        {
            MCToolPalButton* other = ModeButton(i);

            if (other != button && other->Pushed != 0)
            {
                other->Pushed = 0;
            }
        }

        TheInterface->CurrentCommand = action;
        TheInterface->CommandOneShot = -1;
        return;
    }

    SoundSystem->PlayDigitalSample(0x34, 1, nullptr, 0, 0);
    TheInterface->CurrentCommand = 0;
    TheInterface->CommandOneShot = 0;
}

auto TabStripEvent(MCGuiObject* obj, MCGuiEvent* event) -> void
{
    if (event->Type == EVENT_LEFT_DOWN)
    {
        // The strip's four tabs, top to bottom; clicking the open page's tab while shown does nothing.
        const int32_t offset = event->Y - obj->GlobalY();

        if (offset > 0x1b)
        {
            if (offset < 0x4c)
            {
                if (TacMap()->DisplayType == TACMAP_MAP && TacMap()->IsHidden() == 0)
                {
                    return;
                }

                MapSwitch();
            }
            else if (offset < 0x74)
            {
                if (TacMap()->DisplayType == TACMAP_INFO && TacMap()->IsHidden() == 0)
                {
                    return;
                }

                InfoSwitch();
            }
            else if (offset < 0xae)
            {
                if (TacMap()->DisplayType == TACMAP_MISSION && TacMap()->IsHidden() == 0)
                {
                    return;
                }

                MissionSwitch();
            }
            else
            {
                if (offset > 0xe2)
                {
                    return;
                }

                if (TacMap()->DisplayType == TACMAP_SALVAGE && TacMap()->IsHidden() == 0)
                {
                    return;
                }

                SalvageSwitch();
            }

            SoundSystem->PlayDigitalSample(0x36, 1, nullptr, 0, 0);
            return;
        }

        // The strip's top toggles the MFD.
        Application->Grab(obj);
        TacMap()->HideMe(TacMap()->IsHidden() == 0);
    }
    else if (event->Type == EVENT_LEFT_UP)
    {
        Application->Release();
    }
    else if (event->Type == EVENT_MOUSE_MOVE)
    {
        Application->SetCurrentCursor(static_cast<MCCursorType>(0));
    }
}

auto TabTopEvent(MCGuiObject* /*obj*/, MCGuiEvent* event) -> void
{
    if (event->Type == EVENT_LEFT_DOWN)
    {
        TacMap()->HideMe(TacMap()->IsHidden() == 0);
    }
    else if (event->Type == EVENT_MOUSE_MOVE)
    {
        Application->SetCurrentCursor(static_cast<MCCursorType>(0));
    }
}

auto TabBottomEvent(MCGuiObject* /*obj*/, MCGuiEvent* event) -> void
{
    if (event->Type == EVENT_LEFT_DOWN)
    {
        if (TacMap()->DisplayType != TACMAP_SALVAGE)
        {
            SoundSystem->PlayDigitalSample(0x36, 1, nullptr, 0, 0);
            SalvageSwitch();
        }
    }
    else if (event->Type == EVENT_MOUSE_MOVE)
    {
        Application->SetCurrentCursor(static_cast<MCCursorType>(0));
    }
}

auto MCVideoWindow::Init(int32_t xPos, int32_t yPos, int32_t w, int32_t h, char* fileName) -> int32_t
{
    Star = nullptr;
    return MCGuiObject::Init(xPos, yPos, w, h, fileName);
}

auto MCVideoWindow::Draw() -> void
{
    // The picture (the original painted it when no pilot spoke, and a name stayed over it until then).
    if (BackgroundPort != nullptr)
    {
        BackgroundPort->CopyTo(DisplayPort->Frame(), 0, 0, -1);
    }

    if (Star == nullptr)
    {
        return;
    }

    // The pilot's name.
    LineFont->Scaled = 0;
    LineFont->Scale = 1.0f;
    FillBox(1, 1, static_cast<int16_t>(Width() - 2), 0xb, 0x10);
    LineFont->Print(3, 3, Star->Callsign, 0xe3, DisplayPort->Frame());
    LineFont->Scale = 2.0f;
    LineFont->Scaled = 1;
}

auto MCVideoWindow::Update() -> void
{
    if (Star == nullptr)
    {
        return;
    }

    // Blink the pilot's unit on the mech bar every half second.
    if (BlinkTime + 0.5 < ScenarioTime)
    {
        if (BlinkOn == 0)
        {
            BlinkOn = -1;
            TheInterface->MechBar->Layout.VideoId = Star->Vehicle->PartId;
        }
        else
        {
            BlinkOn = 0;
            TheInterface->MechBar->Layout.VideoId = -1;
        }
    }

    // Track the unit on the tactical map.
    TrackStar(StarMapX, StarMapY);
    AnchorX = static_cast<float>(Width() / 2 + GlobalX());
    AnchorY = static_cast<float>(GlobalY());
}

auto MCVideoWindow::TrackStar(float& mapX, float& mapY) -> void
{
    // The unit on the tactical map, or the window's anchor (its bottom centre) when it is off the map area.
    MCVector3D position = Star->Vehicle->GetPosition();
    TacMap()->WorldToTacMap(position, -1);
    mapX = static_cast<float>(TacMap()->GlobalX()) + position.X;
    mapY = static_cast<float>(TacMap()->GlobalY()) + position.Y;

    if (mapX < 6.0f || mapX > 136.0f || mapY < 34.0f || mapY > 164.0f)
    {
        mapX = static_cast<float>(Width() / 2 + GlobalX());
        mapY = static_cast<float>(GlobalY());
    }
}

auto MCVideoWindow::Display() -> void
{
    // The line from the window to the unit, which it follows. (The original's line ran to where the unit was when
    // the window last painted, and from where the window was the paint before.)
    if (Star != nullptr)
    {
        float mapX = 0.0f;
        float mapY = 0.0f;
        TrackStar(mapX, mapY);
        VfxLineDraw(TacMap()->Frame(), Width() / 2 + GlobalX(), GlobalY(), static_cast<int32_t>(mapX),
                    static_cast<int32_t>(mapY), 0, 0x1f);
    }

    MCGuiObject::Display();
}

auto MCVideoWindow::SetStar(MCMechWarrior* newStar) -> void
{
    if (newStar != nullptr)
    {
        BlinkOn = 0;
        Star = newStar;
        BlinkTime = static_cast<float>(ScenarioTime - 0.5);
        TheInterface->MechBar->Layout.VideoId = newStar->Vehicle->PartId;
        Update();
        return;
    }

    if (Star != nullptr)
    {
        TheInterface->MechBar->Layout.VideoId = -1;
        MCGameObject* vehicle = Star->Vehicle;

        if (TheInterface->IsSelected(vehicle->PartId) != 0)
        {
            vehicle->SetSelected(1);
            Star = nullptr;
            Update();
            return;
        }

        vehicle->SetSelected(0);
    }

    Star = nullptr;
    Update();
}

MCTacticalMap::MCTacticalMap()
{
    InfoWatcher = {};
}

MCTacticalMap::~MCTacticalMap()
{
    InfoWatcher.Free();
}

auto MCTacticalMap::SetRevealedBitmap(char* fileName) -> void
{
    MCFile gifFile;
    MCFullPathFileName gifName;
    gifName.Init(TerrainPath, fileName, ".gif");

    if (FileExists(gifName) == 0)
    {
        return;
    }

    gifFile.Open(gifName, READ, 50);
    const uint32_t size = gifFile.FileSize();

    if (size == 0)
    {
        return;
    }

    auto* gif = static_cast<uint8_t*>(std::malloc(size));

    if (gif == nullptr)
    {
        return;
    }

    gifFile.Read(gif, size);
    gifFile.Close();
    VfxGifResolution(gif);
    void* work = std::malloc(VFX_GIF_BUFFER_SIZE);
    VfxGifDraw(VisibilityPort->Frame(), gif, work);
    std::free(gif);
    std::free(work);
}

auto MCTacticalMap::Init(int32_t xPos, int32_t yPos) -> int32_t
{
    Zoom = 1;
    InfoObject = nullptr;
    ZoomOffset = 0;
    ObjectivesRevealed = 0;
    FreePartShapes();
    TacMapCenter = MCVector3D(0.0f, 0.0f, 0.0f);
    MapVertexSide = MCTerrain::VerticesBlockSide * MCTerrain::BlocksMapSide;

    // The map picture.
    MapPort = new MCGuiPort;

    if (MapPort == nullptr)
    {
        Fatal(-1, "No RAM for TacMap");
    }

    MCFullPathFileName pictureName;
    pictureName.Init(TerrainPath, MCTerrain::TerrainName, ".tga");
    int32_t result = MapPort->Init(pictureName);
    Assert(result == 0, static_cast<uint32_t>(result), " could not start tacticalMap ");
    MapWidth = MapPort->Frame()->Window->XMax;
    MapHeight = MapPort->Frame()->Window->YMax;

    // The fog of war: a port whose pixels are the home side's visible bits.
    VisibilityPort = new MCGuiPort;

    if (MapPort == nullptr)
    {
        Fatal(-1, "No RAM for TacMap");
    }

    VisibilityPort->Init(MapVertexSide, MapVertexSide);

    if (result != 0)
    {
        Fatal(result, " Unable to create Port for TacMap ");
    }

    MCRenderer::DestroyTexture(VisibilityPort->Frame()->Window);
    MCGuiPort::FreePixels(VisibilityPort->Frame()->Window->Buffer);
    MCByteFlag* visibleBits = HomeTeam->Alignment == -1 ? MCTerrain::ClanVisibleBits : MCTerrain::TerrainVisibleBits;
    VisibilityPort->Frame()->Window->Buffer = visibleBits->FlagData.data();
    // Port: the fog of war is a kept frame surface: the reveals draw it on the GPU as well as in the flags the game
    // reads, and the map page samples the GPU's copy instead of uploading the flags whenever they change.
    MCRenderer::AddFrameSurface(VisibilityPort->Frame()->Window, true);

    const float side = static_cast<float>(MapVertexSide) * static_cast<float>(MapVertexSide);
    MapDiagonal = std::sqrt(side + side) * MCTerrain::MetersPerVertex;
    MetersPerPixel = (MapDiagonal * DIAGONAL_TO_PIXELS) / static_cast<float>(Zoom);

    MapBackground = LoadBackground("mfdmwn00.tga", "Error reading tacmap MFD background");
    InfoBackground = LoadBackground("mfddwn00.tga", "Error reading info MFD background");
    MissionBackground = LoadBackground("mfdbwn00.tga", "Error reading mission MFD background");

    // Port: the info page's data view backgrounds, which the original loaded each time it drew one (and skipped
    // when one failed to load).
    static const char* const viewBackgroundNames[3] = {"mfddwn01.tga", "mfddwn02.tga", "mfddwn03.tga"};

    for (int32_t i = 0; i < 3; i++)
    {
        InfoViewBackgrounds[i] = new MCGuiPort;

        if (InfoViewBackgrounds[i]->Init(const_cast<char*>(viewBackgroundNames[i])) != 0)
        {
            DestroyPort(InfoViewBackgrounds[i]);
        }
    }

    result = MCGuiObject::Init(xPos, yPos, 0x8c, 0xef, nullptr);

    if (result != 0)
    {
        return result;
    }

    if (MPlayer == nullptr)
    {
        SalvageBackground = MissionBackground;
        result = 0;
    }
    else
    {
        SalvageBackground = LoadBackground("mfdswn01.tga", "Error reading salvage MFD background");
        ChatWindow = new MCGuiChatWindow;
        result = ChatWindow->Init(6, 0x22, 0x82, 0x99, nullptr);
        AddChild(ChatWindow);
        Assert(result == 0, static_cast<uint32_t>(result), "Error initing chat object");
    }

    ChatPending = 0;

    // The tabs along the MFD's right edge.
    TabTop = MakePicture(this, Width(), 0, 0xc, 4, "mfdmts00.tga");
    TabTop->SetEventRoutine(TabTopEvent);
    TabTop->SetTransparent(-1);
    AddChild(TabTop);

    TabStrip = new MCGuiObject;
    TabStrip->SetDrawsLive();
    TabStrip->Init(Width(), 4, 0xc, 0xe7, nullptr);

    if (MPlayer == nullptr)
    {
        TabStrip->SetBackground(const_cast<char*>("mfdmts01.tga"));
    }
    else
    {
        const int32_t blinkerTop = TabStrip->Bottom() - 0x3a;
        TabStrip->SetBackground(const_cast<char*>("mfdmts03.tga"));

        ChatBlinkerOff = new MCGuiObject;
        ChatBlinkerOff->SetDrawsLive();
        result = ChatBlinkerOff->Init(Width() + 2, blinkerTop, 10, 0x3a, nullptr);
        Assert(result == 0, static_cast<uint32_t>(result), "Error initing chat blinker object");
        ChatBlinkerOff->SetBackground(const_cast<char*>("mfdsts05.tga"));
        ChatBlinkerOff->SetDepth(10);
        AddChild(ChatBlinkerOff);
        ChatBlinkerOff->ShowGuiWindow(0);
        ChatBlinkerOff->SetEventRoutine(BlinkerHandleEvent);

        ChatBlinkerOn = new MCGuiObject;
        ChatBlinkerOn->SetDrawsLive();
        result = ChatBlinkerOn->Init(Width(), blinkerTop, 10, 0x3a, nullptr);
        Assert(result == 0, static_cast<uint32_t>(result), "Error initing chat blinker object");
        ChatBlinkerOn->SetBackground(const_cast<char*>("mfdsts04.tga"));
        ChatBlinkerOn->SetDepth(10);
        AddChild(ChatBlinkerOn);
        ChatBlinkerOn->ShowGuiWindow(0);
        ChatBlinkerOn->SetEventRoutine(BlinkerHandleEvent);
    }

    TabStrip->SetEventRoutine(TabStripEvent);
    AddChild(TabStrip);

    TabBottom = MakePicture(this, Width(), 0xeb, 0xc, 4, "mfdmts02.tga");
    TabBottom->SetEventRoutine(TabBottomEvent);
    TabBottom->SetTransparent(-1);
    AddChild(TabBottom);

    ObjectType = 6;
    ShowGuiWindow(0);
    SetBackColor(0x10);
    SetHideDirection(static_cast<MCDirection>(0));

    // The support buttons.
    struct SupportButton
    {
        int32_t X;
        int32_t CommandId;
        uint32_t HelpId;
        const char* Pictures[3];
    };

    static const SupportButton supportButtons[4] = {
        {6, STRIKE_SMALL, 0x93, {"mfdtbh00.tga", "mfdtbg00.tga", "mfdtbn00.tga"}},
        {0x26, STRIKE_LARGE, 0x94, {"mfdtbh01.tga", "mfdtbg01.tga", "mfdtbn01.tga"}},
        {0x47, STRIKE_SENSOR, 0x95, {"mfdtbh02.tga", "mfdtbg02.tga", "mfdtbn02.tga"}},
        {0x68, STRIKE_CAMERA_DRONE, 0x96, {"mfdtbh03.tga", "mfdtbg03.tga", "mfdtbn03.tga"}},
    };

    for (int32_t i = 0; i < 4; i++)
    {
        const SupportButton& spec = supportButtons[i];
        MCArtilleryButton* button =
            MakeButton<MCArtilleryButton>(spec.X, 6, 0x1f, 0x16, spec.Pictures[0], spec.Pictures[1], spec.Pictures[2]);
        ArtilleryButtons[i] = button;
        button->CommandId = spec.CommandId;
        LoadHelpText(button->HelpText, spec.HelpId);
        AddChild(button);
    }

    // The info page's data buttons.
    struct DataButton
    {
        int32_t Index;
        int32_t X;
        void (*Callback)();
        const char* Pictures[3];
    };

    static const DataButton dataButtonSpecs[3] = {
        {0, 0xf, ArmorFrontButton, {"mfddbh01.tga", "mfddbg01.tga", "mfddbn01.tga"}},
        {2, 0x56, PayloadButton, {"mfddbh02.tga", "mfddbg02.tga", "mfddbn02.tga"}},
        {1, 0x35, RearButton, {"mfddbh03.tga", "mfddbg03.tga", "mfddbn03.tga"}},
    };

    for (const DataButton& spec : dataButtonSpecs)
    {
        MCGuiToolButton* button =
            MakeButton<MCGuiToolButton>(spec.X, 0xcc, 0x20, 0xc, spec.Pictures[0], spec.Pictures[1], spec.Pictures[2]);
        DataButtons[spec.Index] = button;
        button->Framed = 0;
        button->Callback()->SetExec(spec.Callback);
        AddChild(button);
    }

    // The scroll buttons (disabled at 1x) and the zoom buttons.
    struct ScrollButton
    {
        int32_t X;
        int32_t Y;
        void (*Routine)(MCGuiObject*, MCGuiEvent*);
        const char* Pictures[3];
    };

    static const ScrollButton scrollSpecs[4] = {
        {0x4b, 0xb5, TmcUp, {"mfdmbh00.tga", "mfdmbg00.tga", "mfdmbn00.tga"}},
        {0x59, 0xc0, TmcLeft, {"mfdmbh01.tga", "mfdmbg01.tga", "mfdmbn01.tga"}},
        {0x4b, 0xcd, TmcDown, {"mfdmbh02.tga", "mfdmbg02.tga", "mfdmbn02.tga"}},
        {0x3f, 0xc0, TmcRight, {"mfdmbh03.tga", "mfdmbg03.tga", "mfdmbn03.tga"}},
    };

    for (int32_t i = 0; i < 4; i++)
    {
        const ScrollButton& spec = scrollSpecs[i];
        MCGuiButton* button =
            MakeButton<MCGuiButton>(spec.X, spec.Y, 0xd, 0xb, spec.Pictures[0], spec.Pictures[1], spec.Pictures[2]);
        ScrollButtons[i] = button;
        button->SetEventRoutine(spec.Routine);
        AddChild(button);
    }

    MCGuiButton* zoomOutButton =
        MakeButton<MCGuiButton>(0x74, 0xca, 0xd, 0xd, "mfdmbh04.tga", "mfdmbg04.tga", "mfdmbn04.tga");
    ScrollButtons[5] = zoomOutButton;
    zoomOutButton->Callback()->SetExec(TmcZoomOut);
    zoomOutButton->Disabled = -1;
    AddChild(zoomOutButton);
    MCGuiButton* zoomInButton =
        MakeButton<MCGuiButton>(0x74, 0xb6, 0xd, 0xd, "mfdmbh05.tga", "mfdmbg05.tga", "mfdmbn05.tga");
    ScrollButtons[4] = zoomInButton;
    zoomInButton->Callback()->SetExec(TmcZoomIn);
    AddChild(zoomInButton);

    for (int32_t i = 0; i < 4; i++)
    {
        MCGuiButton* button = TacMap()->ScrollButtons[i];
        button->Disabled = -1;
    }

    // The command palette, hidden until the palette button opens it.
    PaletteFrame = new MCGuiObject;
    PaletteFrame->SetDrawsLive();
    PaletteFrame->Init(0, 0xe9, 0x8c, 0x35, nullptr);
    PaletteFrame->SetDepth(10);
    PaletteFrame->SetBackground(const_cast<char*>("mfdcwn00.tga"));
    AddChild(PaletteFrame);
    PaletteFrame->ShowGuiWindow(0);
    PaletteBottom = new MCGuiObject;
    PaletteBottom->SetDrawsLive();
    PaletteBottom->Init(PaletteFrame->Width(), 0, 2, 0x35, nullptr);
    PaletteBottom->SetBackground(const_cast<char*>("mfdcwn01.tga"));
    PaletteBottom->SetTransparent(-1);
    PaletteFrame->AddChild(PaletteBottom);

    PaletteButton = MakeButton<MCGuiToolButton>(6, 0xe0, 9, 9, "mfdcwn02.tga", "mfdcwn03.tga", nullptr);
    PaletteButton->Framed = 0;
    PaletteButton->Callback()->SetExec(TogglePalette);
    AddChild(PaletteButton);

    ScrollUpMarker = MakePicture(this, 0, 0, 0xb, 0xb, "mfddbg04.tga");
    ScrollUpMarker->ShowGuiWindow(0);
    AddChild(ScrollUpMarker);
    ScrollDownMarker = MakePicture(this, 0, 0, 0xb, 0xb, "mfddbg05.tga");
    ScrollDownMarker->ShowGuiWindow(0);
    AddChild(ScrollDownMarker);

    LastMapTime = -999.0f;

    // The palette's mode buttons, two rows of four.
    int32_t buttonX = 6;
    int32_t buttonY = 2;

    for (int16_t i = 0; i < 8; i++)
    {
        auto* button = new MCToolPalButton;
        ToolButtons[i] = button;
        button->Init(buttonX, buttonY, 0x1f, 0x16, nullptr);
        button->Callback()->SetMessage(button, EVENT_CALLBACK);
        button->SetEventRoutine(ToolPaletteButtonEvent);
        char pictureName[32];
        std::snprintf(pictureName, sizeof(pictureName), i == 7 ? "mfdcbn%02i.tga" : "mfdcbh%02i.tga", i);
        button->SetUpPicture(pictureName);
        std::snprintf(pictureName, sizeof(pictureName), i == 7 ? "mfdcbh%02i.tga" : "mfdcbg%02i.tga", i);
        button->SetDownPicture(pictureName);
        std::snprintf(pictureName, sizeof(pictureName), "mfdcbn%02i.tga", i);
        button->SetGrayPicture(pictureName);
        button->Framed = 0;

        if (i == 7)
        {
            // The original also disabled zoom in multiplayer; the port allows it.
            if (Only45Pixel == 0)
            {
                button->Action = ACTION_TOGGLE_ZOOM;
                LoadHelpText(button->HelpText, 0x91);
            }
            else
            {
                // No zoom: only the 45-pixel art is loaded.
                button->SetGrayPicture(const_cast<char*>("mfdcbn07a.tga"));
                button->Disabled = -1;
                LoadHelpText(button->HelpText, 0x92);
            }
        }
        else
        {
            button->Action = ButtonActions[i];
            LoadHelpText(button->HelpText, 0x8a + static_cast<uint32_t>(i));
        }

        PaletteFrame->AddChild(button);
        buttonX += 1 + button->Width();

        if (i == 3)
        {
            buttonX = 6;
            buttonY = 0x19;
        }
    }

    VideoWindow = new MCVideoWindow;
    VideoWindow->Init(6, 0xaa, 0x30, 0x30, nullptr);
    VideoWindow->SetBackground(const_cast<char*>("mfdmwn01.tga"));
    AddChild(VideoWindow);

    InfoText = new MCGuiScrollTextObject;
    Assert(InfoText != nullptr, 0, "Not enough memory for text view object");
    InfoText->Init(5, 0x22, 0x76, 0xb8, nullptr);
    AddChild(InfoText);
    InfoText->ShowGuiWindow(0);
    SalvageText = new MCGuiScrollTextObject;
    Assert(SalvageText != nullptr, 0, "Not enough memory for salvage view object");
    SalvageText->Init(5, 0x22, 0x76, 0xb8, nullptr);
    AddChild(SalvageText);
    SalvageText->ShowGuiWindow(0);

    NumSalvage = 0;
    ScrollX = 0;
    ScrollY = 0;
    SetDisplayType(TACMAP_MAP);

    // The map area, as rectangles and as a pane on the MFD's window.
    MapPane.Window = DisplayPort->Frame()->Window;
    MapRect[0] = 6;
    MapPane.X0 = 6;
    MapRect[1] = 0x22;
    MapPane.Y0 = 0x22;
    ZoomInRect[0] = 0x70;
    MapRect[2] = 0x87;
    MapPane.X1 = 0x87;
    ZoomInRect[1] = 0xb2;
    MapRect[3] = 0xa3;
    MapPane.Y1 = 0xa3;
    ZoomInRect[2] = 0x89;
    ZoomInRect[3] = 199;
    ZoomOutRect[0] = 0x70;
    ZoomOutRect[1] = 199;
    ZoomOutRect[2] = 0xb2;
    ZoomOutRect[3] = 0xdb;

    for (MCGuiPort*& port : InfoPorts)
    {
        port = new MCGuiPort;
    }

    SetDataDisplayMode(0, -1);
    LastRefreshTime = 0;
    RefreshPage();

    TypeString[0] = LoadHeapString(0x7c);
    TypeString[1] = LoadHeapString(0x7d);
    TypeString[2] = LoadHeapString(0x7e);
    TypeString[3] = LoadHeapString(0x7f);
    TypeString[4] = LoadHeapString(0x80);
    StatusString[0] = LoadHeapString(0x78);
    StatusString[1] = LoadHeapString(0x79);
    StatusString[2] = LoadHeapString(0x7a);
    StatusString[3] = LoadHeapString(0x7b);

    std::memset(ColorRemap, 0xff, sizeof(ColorRemap));
    ColorRemap[0xe6] = 0x13;
    ColorRemap[0xe8] = 0x13;
    MCRenderer::RegisterData(ColorRemap, sizeof(ColorRemap), MCDataKind::Tables);

    for (MCGameObject*& item : Salvage)
    {
        item = nullptr;
    }

    return result;
}

auto MCTacticalMap::FreePartShapes() -> void
{
    if (PartShapes != nullptr)
    {
        MCRenderer::UnregisterData(PartShapes.get());
        PartShapes.reset();
    }
}

auto MCTacticalMap::Destroy() -> void
{
    if (MapPort != nullptr)
    {
        MapPort->Destroy();
        delete MapPort;
        MapPort = nullptr;
    }

    if (VisibilityPort != nullptr)
    {
        // The bitmap's pixels are the visible bits' heap; the original clears the pane's window first.
        MCRenderer::RemoveFrameSurface(VisibilityPort->Frame()->Window);
        VisibilityPort->Frame()->Window = nullptr;
        VisibilityPort->Destroy();
        delete VisibilityPort;
        VisibilityPort = nullptr;
    }

    TheInterface->TacticalMap = nullptr;

    for (MCArtilleryButton*& button : ArtilleryButtons)
    {
        DestroyChild(button);
    }

    for (MCGuiButton*& button : ScrollButtons)
    {
        DestroyChild(button);
    }

    for (MCToolPalButton*& button : ToolButtons)
    {
        DestroyChild(button);
    }

    for (MCGuiToolButton*& button : DataButtons)
    {
        DestroyChild(button);
    }

    DestroyChild(PaletteButton);
    DestroyChild(PaletteBottom);
    DestroyChild(PaletteFrame);
    DestroyChild(SalvageText);
    DestroyChild(InfoText);
    DestroyChild(VideoWindow);

    for (MCGuiPort*& port : InfoPorts)
    {
        DestroyPort(port);
    }

    DestroyPort(MapBackground);
    DestroyPort(InfoBackground);

    for (MCGuiPort*& background : InfoViewBackgrounds)
    {
        DestroyPort(background);
    }

    if (MissionBackground != nullptr)
    {
        MissionBackground->Destroy();
        delete MissionBackground;
        MissionBackground = nullptr;

        // In single player the salvage page shares it.
        if (MPlayer == nullptr)
        {
            SalvageBackground = nullptr;
        }
    }

    DestroyPort(SalvageBackground);
    DestroyChild(TabTop);
    DestroyChild(TabStrip);
    DestroyChild(TabBottom);
    DestroyChild(ScrollUpMarker);
    DestroyChild(ScrollDownMarker);
    DestroyChild(ChatWindow);
    DestroyChild(ChatBlinkerOff);
    DestroyChild(ChatBlinkerOn);
    FreePartShapes();
    MouseInside = 0;
    MCGuiObject::Destroy();

    for (std::string& text : TypeString)
    {
        text.clear();
    }

    for (std::string& text : StatusString)
    {
        text.clear();
    }
}

auto MCTacticalMap::RefreshPage() -> void
{
    if (IsShowing() == 0)
    {
        return;
    }

    if (DisplayType == TACMAP_INFO)
    {
        MCGuiScrollTextObject* text = InfoText;
        MCGameObject* obj = InfoObject;
        const int32_t firstPixel = text->FirstPixel;

        if (obj == nullptr || (obj->ObjectClass != BATTLEMECH && obj->ObjectClass != GROUNDVEHICLE &&
                               obj->ObjectClass != ELEMENTAL && obj->ObjectClass != MOVER))
        {
            InfoText->ShowGuiWindow(0);
            return;
        }

        if (DataDisplayMode == 2)
        {
            // Payload: the weapon list.
            InfoDirty = 0;
            text->ShowGuiWindow(-1);
            DrawWeapons();
        }
        else
        {
            // Armor: the part diagram's colours.
            GetColors();
            InfoDirty = 0;
            InfoText->ShowGuiWindow(0);
        }

        if (obj->ObjectClass == BATTLEMECH)
        {
            MCGuiScrollTextObject* list = InfoText;
            list->FirstPixel = firstPixel;
            list->PositionScrollTab();
        }
        else if (obj->ObjectClass == GROUNDVEHICLE)
        {
            if (DataDisplayMode == 1)
            {
                SetDataDisplayMode(0, 0);
            }

            MCGuiScrollTextObject* list = InfoText;
            list->FirstPixel = firstPixel;
            list->PositionScrollTab();
        }

        return;
    }

    if (DisplayType == TACMAP_MISSION)
    {
        // The home side's objectives, each with its type and its timer or status.
        MCGuiScrollTextObject* text = InfoText;
        const int32_t firstPixel = text->FirstPixel;
        text->Clear();

        if (Turn > 1)
        {
            const int32_t count = static_cast<int32_t>(HomeTeam->NumObjectives);
            int32_t objectiveNum = HomeTeam->FirstObjective;
            char line[256];

            for (int32_t i = 0; i < count; i++, objectiveNum++)
            {
                MCScenarioObjective* objective = &Scenario->Objectives[objectiveNum];
                uint8_t color = 0;

                if (objective->Status == 0)
                {
                    color = 0xf2;
                }
                else if (objective->Status == 1)
                {
                    color = 0xb;
                }
                else if (objective->Status == 2)
                {
                    color = 0xef;
                }

                std::snprintf(line, sizeof(line), "%d--%s", i + 1, objective->Name);
                text->PrintWrapped(line, color, -1);
                const uint32_t type = objective->Type + 1 > 3 ? 4 : objective->Type + 1;
                std::snprintf(line, sizeof(line), "      %s", TypeString[type].c_str());
                text->PrintWrapped(line, color, -1);
                const float timeLeft = Scenario->CheckObjectiveTimer(objectiveNum);

                if (timeLeft > 0.0)
                {
                    const auto seconds =
                        static_cast<int32_t>(std::floor(std::fmod(static_cast<double>(timeLeft), 60.0)));
                    const auto minutes = static_cast<int32_t>(timeLeft * (1.0 / 60.0));
                    std::snprintf(line, sizeof(line), "%02d:%02d", minutes, seconds);
                }
                else
                {
                    const uint32_t status = objective->Status > 2 ? 3 : objective->Status;
                    std::snprintf(line, sizeof(line), "      %s", StatusString[status].c_str());
                }

                text->PrintWrapped(line, color, -1);
                text->Print(nullptr, 0x1f);
            }
        }

        text->FirstPixel = firstPixel;
        text->ResetPortSize();
        text->PositionScrollTab();
    }
}

auto MCTacticalMap::Draw() -> void
{
    if (IsShowing() == 0)
    {
        return;
    }

    // The page's background, drawn with its holes (colour 0xff) open.
    switch (DisplayType)
    {
        case TACMAP_MAP:
        {
            MapBackground->CopyTo(DisplayPort->Frame(), 0, 0, -1);
            DrawMapPage(this);
            break;
        }
        case TACMAP_INFO:
        {
            if (InfoObject == nullptr)
            {
                VfxPaneWipe(DisplayPort->Frame(), 0x10);
            }

            InfoBackground->CopyTo(DisplayPort->Frame(), 0, 0, -1);
            DrawInfoPage();
            break;
        }
        case TACMAP_MISSION:
        {
            MissionBackground->CopyTo(DisplayPort->Frame(), 0, 0, -1);
            break;
        }
        case TACMAP_SALVAGE:
        {
            SalvageBackground->CopyTo(DisplayPort->Frame(), 0, 0, -1);
            break;
        }
    }

    // The status line: a button's help, or the default text.
    FillBox(0x15, 0xe2, 0x87, 0xe8, 0x10);
    char buffer[256];
    char* text = StatusText;

    if (text == nullptr)
    {
        CLoadString(ThisInstance, 0x97, buffer, 0xfe);
        text = buffer;
    }

    BlueFont->WriteString(Port()->Frame(), 0x15, 0xe2, reinterpret_cast<uint8_t*>(text), -1);
    MCGuiObject::Draw();
}

auto MCTacticalMap::DrawInfoPage() -> void
{
    MCGameObject* obj = InfoObject;

    if (obj == nullptr || (obj->ObjectClass != BATTLEMECH && obj->ObjectClass != GROUNDVEHICLE &&
                           obj->ObjectClass != ELEMENTAL && obj->ObjectClass != MOVER))
    {
        FillBox(0xe, 0x2a, 0x2c, 0x4c, 0x10);
        return;
    }

    MCMechWarrior* pilot = obj->GetPilot();
    auto* mover = static_cast<MCMover*>(obj);
    const bool showPilot = mover->NetPlayerId >= 0;

    if (DataDisplayMode == 2)
    {
        // Payload: the weapon list's background (the list is the info text).
        if (InfoViewBackgrounds[1] != nullptr)
        {
            VfxPaneCopy(InfoViewBackgrounds[1]->Frame(), 0, 0, DisplayPort->Frame(), 6, 0x5d, -1);
        }
    }
    else
    {
        // Armor: the part diagram, over the home side's or the enemy's background.
        MCGuiPort* background = InfoViewBackgrounds[obj->GetTeam() == HomeTeam ? 0 : 2];

        if (background != nullptr)
        {
            VfxPaneCopy(background->Frame(), 0, 0, DisplayPort->Frame(), 6, 0x5d, -1);
        }

        const int32_t shape = obj->ObjectClass == BATTLEMECH ? mover->NumArmorLocations + 1 + mover->NumBodyLocations
                                                             : mover->NumBodyLocations;
        AGShapeDraw(Port()->Frame(), PartShapes.get(), shape, 0x22, 0x65);
        DrawParts();
    }

    DrawBar();
    char line[64];

    if (obj->ObjectClass == BATTLEMECH)
    {
        if (showPilot)
        {
            DrawPilot(pilot);
        }
        else
        {
            FillBox(6, 0x2a, 0x88, 0x4c, 0x10);
        }

        // "<name> <weight class> <tons>".
        const int32_t tonnage = static_cast<int32_t>(obj->GetTonnage());
        uint32_t classId = 0x85;

        if (tonnage < 0x28)
        {
            classId = 0x82;
        }
        else if (tonnage < 0x3c)
        {
            classId = 0x83;
        }
        else if (tonnage < 0x50)
        {
            classId = 0x84;
        }

        char buffer[256];
        CLoadString(ThisInstance, classId, buffer, 0xfe);
        char weightClass[12];
        std::strncpy(weightClass, buffer, 9);
        weightClass[9] = 0;
        CLoadString(ThisInstance, 0x81, buffer, 0xfe);
        std::snprintf(line, sizeof(line), buffer, mover->GetIfaceName(), weightClass, tonnage);
        const int32_t lineWidth = BlueFont->Width(reinterpret_cast<uint8_t*>(line));
        BlueFont->WriteString(Port()->Frame(), 0x47 - lineWidth / 2, 0x54, reinterpret_cast<uint8_t*>(line), -1);
    }
    else if (obj->ObjectClass == GROUNDVEHICLE)
    {
        FillBox(6, 0x2a, 0x88, 0x4c, 0x10);
        std::snprintf(line, sizeof(line), "%s", mover->GetIfaceName());
        // Measured in greenFont, written in blueFont (as the original).
        const int32_t lineWidth = GreenFont->Width(reinterpret_cast<uint8_t*>(line));
        BlueFont->WriteString(Port()->Frame(), 0x47 - lineWidth / 2, 0x54, reinterpret_cast<uint8_t*>(line), -1);

        // The passengers, with their pictures.
        auto* vehicle = static_cast<MCGroundVehicle*>(InfoObject);
        int32_t shown = 0;
        int32_t xPos = 10;

        for (int32_t seat = 0; seat < vehicle->Seats; seat++)
        {
            MCMechWarrior* passenger = vehicle->Passengers[seat];

            if (passenger == nullptr)
            {
                continue;
            }

            VfxPaneCopy(InfoPorts[shown]->Frame(), 0, 0, DisplayPort->Frame(), xPos, 0xc4, 0xfff);
            const int32_t yPos = 0xa6 - (GreenFont->Height() + 2) * shown;
            GreenFont->WriteString(Port()->Frame(), xPos, yPos, reinterpret_cast<uint8_t*>(passenger->Name), -1);
            shown++;
            xPos += 0x23;
        }
    }
}

auto MCTacticalMap::HandleEvent(MCGuiEvent* event) -> void
{
    const int32_t screenX = event->X;
    const int32_t screenY = event->Y;
    POINT local;
    local.x = screenX - GlobalX();
    local.y = screenY - GlobalY();

    // The zoom buttons' areas take the event whole.
    if (DisplayType == TACMAP_MAP)
    {
        if (PtInRect(reinterpret_cast<const RECT*>(ZoomInRect), local) != 0)
        {
            ScrollButtons[4]->HandleEvent(event);
            return;
        }

        if (DisplayType == TACMAP_MAP && PtInRect(reinterpret_cast<const RECT*>(ZoomOutRect), local) != 0)
        {
            ScrollButtons[5]->HandleEvent(event);
            return;
        }
    }

    const RECT* upArea = reinterpret_cast<const RECT*>(PageRects[0]);
    const RECT* downArea = reinterpret_cast<const RECT*>(PageRects[1]);
    const RECT* trackArea = reinterpret_cast<const RECT*>(PageRects[2]);

    switch (event->Type)
    {
        case EVENT_LEFT_DOWN:
        {
            if (DisplayType == TACMAP_MAP && PtInRect(reinterpret_cast<const RECT*>(MapRect), local) != 0)
            {
                MapDragging = -1;
            }

            if (local.y > 0xe0)
            {
                // Below the pages: the palette toggle.
                TogglePalette();
                break;
            }

            if (DisplayType > TACMAP_MAP && DisplayType <= TACMAP_SALVAGE)
            {
                // The info/mission pages scroll infoText, the salvage page salvageText; press and hold repeats.
                MCGuiScrollTextObject* text = DisplayType == TACMAP_SALVAGE ? SalvageText : InfoText;

                if (PtInRect(upArea, local) != 0)
                {
                    Application->Grab(this);
                    ScrollUpMarker->ShowGuiWindow(-1);
                    Application->AddTimer(this, SCROLL_START_TIMER, TheInterface->ScrollStart, 0, 0, 0);
                    text->ReceiveClick(-1, 0);
                }
                else if (PtInRect(downArea, local) != 0)
                {
                    Application->Grab(this);
                    ScrollDownMarker->ShowGuiWindow(-1);
                    Application->AddTimer(this, SCROLL_START_TIMER, TheInterface->ScrollStart, 0, 0, 0);
                    text->ReceiveClick(1, 0);
                }
                else if (PtInRect(trackArea, local) != 0)
                {
                    // The info pages measure the click from infoText's screen top, the salvage page from the track.
                    const int32_t yPos =
                        DisplayType == TACMAP_SALVAGE ? local.y - PageRects[2][1] : local.y - text->GlobalY();
                    text->ReceiveClick(0, yPos);
                }
            }
            break;
        }

        case EVENT_LEFT_UP:
        {
            if (DisplayType == TACMAP_MAP && MapDragging != 0)
            {
                // A click on the map moves the active camera there.
                MapDragging = 0;

                if (PtInRect(reinterpret_cast<const RECT*>(MapRect), local) != 0)
                {
                    MCVector3D target(static_cast<float>(local.x), static_cast<float>(local.y), 0.0f);
                    TacMapToWorld(target, -1);
                    MainHolder->GetActivePane()->GetCamera()->ChangeTarget(nullptr, 0);
                    MainHolder->GetActivePane()->GetCamera()->SetPosition(target);
                }
            }

            Application->RemoveTimer(this, SCROLL_START_TIMER);
            Application->RemoveTimer(this, SCROLL_REPEAT_TIMER);

            if (Application->GrabbedObject() == this)
            {
                // Released on the top-right corner: toggles the MFD.
                const int32_t xPos = screenX - GlobalX();
                const int32_t yPos = screenY - GlobalY();

                if (xPos > 0x8c && yPos < 0x1c)
                {
                    HideMe(MCTerrain::TerrainTacticalMap->IsHidden() == 0);
                }
            }

            Application->Release();
            ScrollUpMarker->ShowGuiWindow(0);
            ScrollDownMarker->ShowGuiWindow(0);
            break;
        }

        case EVENT_TIMER:
        {
            if (event->Data == 1)
            {
                // The chat tab blinks while a message is unread.
                if (ChatPending == 0)
                {
                    if (DisplayType == TACMAP_SALVAGE)
                    {
                        if (ChatBlinkerOn != nullptr)
                        {
                            ChatBlinkerOn->ShowGuiWindow(-1);
                        }
                    }
                    else if (ChatBlinkerOff != nullptr)
                    {
                        ChatBlinkerOff->ShowGuiWindow(-1);
                    }

                    ChatPending = -1;
                }
                else
                {
                    if (ChatBlinkerOff != nullptr)
                    {
                        ChatBlinkerOff->ShowGuiWindow(0);
                    }

                    if (ChatBlinkerOn != nullptr)
                    {
                        ChatBlinkerOn->ShowGuiWindow(0);
                    }

                    ChatPending = 0;
                }
                break;
            }

            if (event->Data == SCROLL_START_TIMER)
            {
                Application->RemoveTimer(this, SCROLL_START_TIMER);
                Application->AddTimer(this, SCROLL_REPEAT_TIMER, TheInterface->ScrollStart / 5, 0, 0, 0);
            }
            else if (event->Data != SCROLL_REPEAT_TIMER)
            {
                break;
            }

            if (DisplayType > TACMAP_MAP && DisplayType <= TACMAP_SALVAGE)
            {
                MCGuiScrollTextObject* text = DisplayType == TACMAP_SALVAGE ? SalvageText : InfoText;

                if (PtInRect(upArea, local) != 0)
                {
                    text->ReceiveClick(-1, 0);
                }
                else if (PtInRect(downArea, local) != 0)
                {
                    text->ReceiveClick(1, 0);
                }
            }
            break;
        }

        case EVENT_ZOOM_IN:
            TmcZoomIn();
            break;

        case EVENT_ZOOM_OUT:
            TmcZoomOut();
            break;
    }

    MCGuiObject::HandleEvent(event);
}

auto MCTacticalMap::MouseWheel(int32_t steps, int32_t xPos, int32_t yPos) -> bool
{
    if (DisplayType > TACMAP_MAP && DisplayType <= TACMAP_SALVAGE)
    {
        MCGuiScrollTextObject* text = DisplayType == TACMAP_SALVAGE ? SalvageText : InfoText;
        return text->MouseWheel(steps, xPos, yPos);
    }

    if (DisplayType == TACMAP_MAP)
    {
        // As clicking the zoom buttons (up in, down out), which are disabled at the ends.
        for (; steps != 0; steps += steps < 0 ? 1 : -1)
        {
            MCGuiButton* button = ScrollButtons[steps < 0 ? 4 : 5];

            if (button->Disabled != 0)
            {
                break;
            }

            if (steps < 0)
            {
                TmcZoomIn();
            }
            else
            {
                TmcZoomOut();
            }
        }

        return true;
    }

    return false;
}

auto MCTacticalMap::Display() -> void
{
    if (IsShowing() == 0)
    {
        return;
    }

    // Hidden and at rest the MFD lies off the screen (but for its tabs); the original showed its picture as it was,
    // without updating it.
    const bool atRest = IsHidden() != 0 && HideOffset == 0;

    if (!atRest && HideOffset != 0)
    {
        // Sliding: step, then stop once off screen (hiding) or back home (showing).
        MoveTo(X() + HideOffset, Y(), -1);

        if (Hidden != 0)
        {
            const tagRECT screen = {2, 0, Application->Width(), Application->Height()};

            if (RectIntersect(screen) == 0)
            {
                HideOffset = 0;
            }
        }
        else
        {
            const bool home = HideOffset < 0 ? (HomeX >= GlobalX() && HomeY >= GlobalY())
                                             : (HomeX <= GlobalX() && HomeY <= GlobalY());

            if (home)
            {
                MoveTo(HomeX - Parent->GlobalX(), HomeY - Parent->GlobalY(), -1);
                HideOffset = 0;
            }
        }
    }

    const bool updating = !atRest && (IsHidden() == 0 || HideOffset != 0);

    if (updating)
    {
        switch (DisplayType)
        {
            case TACMAP_MAP:
                UpdateMapPage();
                break;
            case TACMAP_INFO:
            case TACMAP_MISSION:
            {
                if (MCPort::Milliseconds() > LastRefreshTime + 500)
                {
                    LastRefreshTime = MCPort::Milliseconds();
                    RefreshPage();
                }
                break;
            }
            case TACMAP_SALVAGE:
            {
                if (MPlayer == nullptr && MCPort::Milliseconds() > LastRefreshTime + 500)
                {
                    LastRefreshTime = MCPort::Milliseconds();
                    UpdateSalvage();
                }
                break;
            }
        }

        // The status line is drawn each frame now.
        StatusDirty = 0;
    }

    if (MouseInside != 0)
    {
        Application->SetCurrentCursor(static_cast<MCCursorType>(0));
    }

    // Port: the MFD draws itself, then its children (the original copied its picture, then displayed them).
    DrawInFramePass(DisplayPort);

    // The original revealed the objectives' areas as it drew the map's units, after the fog of war.
    if (updating && DisplayType == TACMAP_MAP)
    {
        RevealObjectives();
    }
}

auto MCTacticalMap::UpdateMapPage() -> void
{
    // The mission timer, rewritten once a second.
    if (Scenario->TimeLimit >= 0 && LastMapTime + 1.0f < ActualTime)
    {
        LastMapTime = ActualTime;
        const float remaining = static_cast<float>(Scenario->TimeLimit) - ActualTime;

        if (remaining >= 0.0f)
        {
            const auto seconds = static_cast<int32_t>(std::fmod(static_cast<double>(remaining), 60.0));
            const auto minutes = static_cast<int32_t>(remaining * (1.0f / 60.0f));
            std::snprintf(MapTimeText, sizeof(MapTimeText), "%02i:%02i", minutes, seconds);
            MapTimeRed = false;
        }
        else
        {
            std::snprintf(MapTimeText, sizeof(MapTimeText), "00:00");
            MapTimeRed = true;
        }

        MapTimeShown = true;
    }

    // Markers blink five times a second.
    TacFrameLength = FrameLength + TacFrameLength;

    if (TacFrameLength > 0.2)
    {
        TacFrameLength = 0.0f;
        OnNow = ~OnNow;
    }
}

auto MCTacticalMap::RevealObjectives() -> void
{
    if (ObjectivesRevealed != 0)
    {
        return;
    }

    // Once a pending objective with a position is found, every objective's area is revealed in the fog of war.
    const int32_t numObjectives = static_cast<int32_t>(HomeTeam->NumObjectives);

    for (int32_t i = 0; i < numObjectives; i++)
    {
        MCScenarioObjective* objective = &Scenario->Objectives[HomeTeam->FirstObjective + i];

        if (objective->Position[0] == -99.0f || objective->Position[1] == -99.0f || objective->Position[2] == -99.0f ||
            objective->Status != 0)
        {
            continue;
        }

        for (int32_t j = 0; j < numObjectives; j++)
        {
            MCScenarioObjective* area = &Scenario->Objectives[HomeTeam->FirstObjective + j];

            if (area->Radius <= 0.0)
            {
                continue;
            }

            const float column = static_cast<float>(std::floor(static_cast<double>(
                MCTerrain::OneOvermetersPerVertex * (area->Position[0] - MCTerrain::MapTopLeft3d100.X))));
            const float row = static_cast<float>(std::floor(static_cast<double>(
                MCTerrain::OneOvermetersPerVertex * (MCTerrain::MapTopLeft3d100.Y - area->Position[1]))));
            const auto xc = static_cast<int32_t>(std::floor(static_cast<double>(column)));
            const auto yc = static_cast<int32_t>(std::floor(static_cast<double>(row)));
            const auto radius = static_cast<int32_t>(area->Radius / MetersPerPixel * WorldUnitsPerMeter);
            VfxEllipseFill(VisibilityPort->Frame(), xc, yc, radius, radius, 0x14);
        }

        ObjectivesRevealed = -1;
        return;
    }
}

namespace
{
    void DrawMapPage(MCTacticalMap* map)
    {
        // The mission timer (or the "no time limit" text), as UpdateMapPage last wrote it.
        char buffer[256];
        MCPane* page = map->Port()->Frame();

        if (Scenario->TimeLimit < 0)
        {
            CLoadString(ThisInstance, 0xbc, buffer, 0xfe);
            WhiteFont->WriteString(page, 0x3c, 0xab, reinterpret_cast<uint8_t*>(buffer), -1);
        }
        else if (map->MapTimeShown)
        {
            map->FillBox(0x37, 0xaa, 0x88, 0xb2, 0x12);
            MCGuiFont* font = map->MapTimeRed ? RedFont : WhiteFont;
            font->WriteString(page, 0x3c, 0xab, reinterpret_cast<uint8_t*>(map->MapTimeText), -1);
        }

        // The map picture, scrolled and zoomed, mapped onto the map area.
        const int32_t halfWidth = map->MapWidth >> 1;
        const int32_t halfHeight = map->MapHeight >> 1;
        const int32_t zoomedHalfWidth = halfWidth / map->Zoom;
        const int32_t zoomedHalfHeight = halfHeight / map->Zoom;
        const int32_t left = ((map->ScrollX - zoomedHalfWidth) + halfWidth) * 0x10000;
        const int32_t right = ((map->ScrollX - halfWidth) + zoomedHalfWidth + map->MapWidth) * 0x10000;
        const int32_t top = ((map->ScrollY - zoomedHalfHeight) + halfHeight) * 0x10000;
        const int32_t bottom = ((map->ScrollY - halfHeight) + zoomedHalfHeight + map->MapHeight) * 0x10000;
        MCScreenVertex vertices[4] = {};
        vertices[0] = {6, 0x22, 0, left, top, 0};
        vertices[1] = {0x88, 0x22, 0, right, top, 0};
        vertices[2] = {0x88, 0xa4, 0, right, bottom, 0};
        vertices[3] = {6, 0xa4, 0, left, bottom, 0};
        VfxMapPolygon(map->DisplayPort->Frame(), 4, vertices, map->MapPort->Frame()->Window, MP_XP);

        if (DrawRevealedTacMap == 0)
        {
            // The fog of war: the visible bits, one texel per vertex, over the same area.
            MCVector3D corners[4] = {MCVector3D(6.0f, 34.0f, 0.0f), MCVector3D(136.0f, 34.0f, 0.0f),
                                     MCVector3D(136.0f, 164.0f, 0.0f), MCVector3D(6.0f, 164.0f, 0.0f)};

            for (MCVector3D& corner : corners)
            {
                map->TacMapToWorld(corner, -1);
            }

            int32_t u[4];
            int32_t v[4];

            for (int32_t i = 0; i < 4; i++)
            {
                const float column = (corners[i].X - MCTerrain::MapTopLeft3d100.X) * MCTerrain::OneOvermetersPerVertex;
                const float row = (MCTerrain::MapTopLeft3d100.Y - corners[i].Y) * MCTerrain::OneOvermetersPerVertex;
                u[i] = static_cast<int32_t>(column * 65536.0 + 0.5);
                v[i] = static_cast<int32_t>(row * 65536.0 + 0.5);
            }

            vertices[0] = {6, 0x22, 0, u[0] + 0x20000, v[0] + 0x10000, 0};
            vertices[1] = {0x88, 0x22, 0, u[1] - 0x10000, v[1] + 0x10000, 0};
            vertices[2] = {0x88, 0xa4, 0, u[2] - 0x10000, v[2] - 0x10000, 0};
            vertices[3] = {6, 0xa4, 0, u[3] + 0x10000, v[3] - 0x20000, 0};

            if (DrawRevealedTacMap == 0)
            {
                VfxMapPolygon(map->DisplayPort->Frame(), 4, vertices, map->VisibilityPort->Frame()->Window, MP_XP);
            }
        }

        // What each camera window sees, as a rectangle in its colour.
        for (int32_t windowNum = 0; windowNum < 4; windowNum++)
        {
            MCTerrainWindow* window = Land->GetTerrainWindow(windowNum);

            if (window == nullptr || window->Camera == nullptr || window->Camera->Active == 0)
            {
                continue;
            }

            MCCamera* camera = window->Camera;
            MCViewWindow* view = camera->Window;
            MCVector2D screenTopLeft(0.0f, 0.0f);
            // Port: the corners of the world surface the view shows (its size follows the zoom).
            MCVector2D screenBottomRight(static_cast<float>(view->WorldWidth()),
                                         static_cast<float>(view->WorldHeight()));
            MCVector3D topLeft;
            MCVector3D bottomRight;
            camera->InverseProject(screenTopLeft, topLeft);
            camera->InverseProject(screenBottomRight, bottomRight);
            map->WorldToTacMap(topLeft, -1);
            map->WorldToTacMap(bottomRight, -1);
            uint8_t color = 0x1f;

            switch (camera->CameraId)
            {
                case 1:
                    color = 0xef;
                    break;
                case 2:
                    color = 0xb;
                    break;
                case 3:
                    color = 0xf2;
                    break;
                case 4:
                    color = 0xc;
                    break;
            }

            MCPane* pane = &map->MapPane;
            const auto paneLeft = static_cast<float>(pane->X0);
            const auto paneTop = static_cast<float>(pane->Y0);
            const auto x0 = static_cast<int32_t>(topLeft.X - paneLeft);
            const auto y0 = static_cast<int32_t>(topLeft.Y - paneTop);
            const auto x1 = static_cast<int32_t>(bottomRight.X - paneLeft);
            const auto y1 = static_cast<int32_t>(bottomRight.Y - paneTop);
            VfxLineDraw(pane, x0, y1, x1, y1, LD_DRAW, color);
            VfxLineDraw(pane, x1, y0, x1, y1, LD_DRAW, color);
            VfxLineDraw(pane, x1, y0, x0, y0, LD_DRAW, color);
            VfxLineDraw(pane, x0, y0, x0, y1, LD_DRAW, color);
        }

        map->DrawObjects();
    }
}

auto MCTacticalMap::HideMe(int hide) -> void
{
    if (HideOffset != 0)
    {
        return;
    }

    if (hide == 0)
    {
        // Shown: the chat tab stops blinking.
        Application->RemoveTimer(this, 1);

        if (ChatBlinkerOn != nullptr)
        {
            ChatBlinkerOn->ShowGuiWindow(0);
        }

        if (ChatBlinkerOff != nullptr)
        {
            ChatBlinkerOff->ShowGuiWindow(0);
        }
    }
    else
    {
        // Port fix: stopVideo checks the movie window, which the original ends without checking.
        StopVideo();
    }

    if (Hidden == hide)
    {
        return;
    }

    if (Turn > 1)
    {
        SoundSystem->PlayDigitalSample(0x3b, 1, nullptr, 0, 0);
    }

    if (hide != 0)
    {
        // Slide off the left edge, from here.
        HomeX = GlobalX();
        HomeY = GlobalY();
        HideOffset = (2 - GlobalX()) - Width();
        Hidden = hide;
        return;
    }

    // Slide back home.
    if (HomeX != GlobalX())
    {
        Hidden = 0;
        HideOffset = HomeX - GlobalX();
        return;
    }

    Hidden = 0;
    HideOffset = HomeY - GlobalY();
}

auto MCTacticalMap::WorldToTacMap(MCVector3D& pos, int scrolled) -> void
{
    // Rotate 45 degrees (the map is drawn diamond-wise), then scale to pixels about the map's centre.
    const float worldX = pos.X;
    const float worldY = pos.Y;
    pos.X = worldX * MAP_ROTATION + worldY * MAP_ROTATION;
    const float rotatedY = worldY * MAP_ROTATION - worldX * MAP_ROTATION;
    pos.Y = rotatedY;

    if (scrolled != 0)
    {
        pos.X = pos.X / MetersPerPixel;
        pos.Y = rotatedY / MetersPerPixel;
        pos.Z = pos.Z / MetersPerPixel;
        const auto zoomF = static_cast<float>(Zoom);
        pos.X = (pos.X + MAP_CENTER_X) -
                (MAP_PICTURE_SIDE / static_cast<float>(MapWidth)) * zoomF * static_cast<float>(ScrollX);
        pos.Y = ((MAP_HALF_SIDE - pos.Y) + MAP_TOP) -
                (MAP_PICTURE_SIDE / static_cast<float>(MapHeight)) * zoomF * static_cast<float>(ScrollY);
        return;
    }

    const float scale = static_cast<float>(Zoom) * MetersPerPixel;
    const float pixelX = pos.X / scale;
    const float pixelY = rotatedY / scale;
    pos.Z = pos.Z / scale;
    pos.X = pixelX + MAP_HALF_SIDE;
    pos.Y = MAP_HALF_SIDE - pixelY;
}

auto MCTacticalMap::TacMapToWorld(MCVector3D& pos, int scrolled) -> void
{
    pos.Z = 0.0f;

    if (scrolled != 0)
    {
        const auto zoomF = static_cast<float>(Zoom);
        pos.X = ((MAP_PICTURE_SIDE / static_cast<float>(MapWidth)) * zoomF * static_cast<float>(ScrollX) + pos.X) -
                MAP_LEFT;
        pos.Y = ((MAP_PICTURE_SIDE / static_cast<float>(MapHeight)) * zoomF * static_cast<float>(ScrollY) + pos.Y) -
                MAP_TOP;
    }

    const float pixelX = pos.X - MAP_HALF_SIDE;
    const float pixelY = MAP_HALF_SIDE - pos.Y;

    if (scrolled != 0)
    {
        pos.X = pixelX * MetersPerPixel;
        pos.Y = pixelY * MetersPerPixel;
    }
    else
    {
        const float scale = static_cast<float>(Zoom) * MetersPerPixel;
        pos.X = pixelX * scale;
        pos.Y = pixelY * scale;
    }

    // Rotate back, and stand the point on the ground.
    const float rotatedX = pos.X;
    const float rotatedY = pos.Y;
    pos.X = rotatedX * MAP_ROTATION + rotatedY * -MAP_ROTATION;
    pos.Y = rotatedY * MAP_ROTATION - rotatedX * -MAP_ROTATION;
    pos.Z = Land->GetTerrainElevation(pos);
}

auto MCTacticalMap::DrawObjects() -> void
{
    // (The markers' blink is stepped by UpdateMapPage, and the objectives' areas revealed by RevealObjectives: the
    // original did both here.)

    // The home side's pending objectives that have a position: a numbered dot.
    const int32_t numObjectives = static_cast<int32_t>(HomeTeam->NumObjectives);

    if (numObjectives != 0)
    {
        for (int32_t i = 0; i < numObjectives; i++)
        {
            MCScenarioObjective* objective = &Scenario->Objectives[HomeTeam->FirstObjective + i];

            if (objective->Position[0] == -99.0f || objective->Position[1] == -99.0f ||
                objective->Position[2] == -99.0f || objective->Status != 0)
            {
                continue;
            }

            if (OnNow != 0)
            {
                MCVector3D pos(objective->Position[0], objective->Position[1], 0.0f);
                WorldToTacMap(pos, -1);
                const int32_t xPos = static_cast<int32_t>(std::floor(static_cast<double>(pos.X))) - MapPane.X0;
                const int32_t yPos = static_cast<int32_t>(std::floor(static_cast<double>(pos.Y))) - MapPane.Y0;
                AGEllipseFill(&MapPane, xPos, yPos, 2, 2, 0x1f);
                const uint32_t status = Scenario->Objectives[HomeTeam->FirstObjective + i].Status;
                uint8_t color = 0;

                if (status == 0)
                {
                    color = 0xf2;
                }
                else if (status == 1)
                {
                    color = 0xb;
                }
                else if (status == 2)
                {
                    color = 0xef;
                }

                char number[8];
                std::snprintf(number, sizeof(number), "%d", i + 1);
                LineFont->Print(xPos, yPos, number, color, &MapPane);
            }
        }
    }

    // The sensor contacts: a dot (dark when not identified), and with the ranges on, the unit's sensor range.
    const int32_t homeAlignment = HomeTeam->Alignment;
    int32_t numContacts = HomeTeam->GetSensorContacts(ContactList);

    for (int32_t i = 0; i < numContacts; i++)
    {
        MCGameObject* obj = ContactList[i];
        int tagged = 0;
        obj->GetContactType(HomeTeam->Id, tagged);

        if (obj->GetAwake() == 0 || obj->IsDisabled() != 0 || obj->InTransport() != 0)
        {
            continue;
        }

        const bool mover = IsMoverClass(obj);

        if (mover && obj->GetPilot()->Status == 2)
        {
            continue;
        }

        const MCVector3D position = obj->GetPosition();
        MCVector3D pos(position.X, obj->GetPosition().Y, 0.0f);
        WorldToTacMap(pos, -1);
        const int32_t xPos = static_cast<int32_t>(std::floor(static_cast<double>(pos.X))) - MapPane.X0;
        const int32_t yPos = static_cast<int32_t>(std::floor(static_cast<double>(pos.Y))) - MapPane.Y0;
        AGEllipseFill(&MapPane, xPos, yPos, 2, 2, tagged == 0 ? 10 : 0xcf);

        if (ShowRanges != 0 && mover)
        {
            DrawSensorRange(this, obj, xPos, yPos, homeAlignment);
        }
    }

    // The contacts in line of sight (mechs and vehicles).
    numContacts = HomeTeam->GetLosContacts(ContactList);

    for (int32_t i = 0; i < numContacts; i++)
    {
        MCGameObject* obj = ContactList[i];

        if (obj->ObjectClass <= 1 || obj->ObjectClass >= 4 || obj->GetAwake() == 0 || obj->IsDisabled() != 0 ||
            obj->InTransport() != 0 || obj->GetPilot()->Status == 2)
        {
            continue;
        }

        const MCVector3D position = obj->GetPosition();
        MCVector3D pos(position.X, position.Y, 0.0f);
        WorldToTacMap(pos, -1);
        const int32_t xPos = static_cast<int32_t>(std::floor(static_cast<double>(pos.X))) - MapPane.X0;
        const int32_t yPos = static_cast<int32_t>(std::floor(static_cast<double>(pos.Y))) - MapPane.Y0;
        AGEllipseFill(&MapPane, xPos, yPos, 2, 2, 0xcf);

        if (ShowRanges != 0)
        {
            DrawSensorRange(this, obj, xPos, yPos, homeAlignment);
        }
    }

    if (ShowRanges != 0)
    {
        // The home side's other sensors (not artillery's), then the enemy's revealed sensor buildings.
        for (int32_t i = 0; i < HomeTeam->NumSensors; i++)
        {
            MCSensorSystem* sensor = HomeTeam->Sensors[i];

            if (sensor->Enabled() == 0 || sensor->Owner->ObjectClass == ARTILLERY)
            {
                continue;
            }

            const float range = (sensor->GetSkilledRange() / MetersPerPixel) * WorldUnitsPerMeter;

            if (range <= 0.0)
            {
                continue;
            }

            const MCVector3D position = sensor->Owner->GetPosition();
            MCVector3D pos(position.X, position.Y, 0.0f);
            const uint8_t color = sensor->Multiplier < 1.0 ? 0xec : 0x1f;
            WorldToTacMap(pos, -1);
            const int32_t xPos = static_cast<int32_t>(std::floor(static_cast<double>(pos.X))) - MapPane.X0;
            const int32_t yPos = static_cast<int32_t>(std::floor(static_cast<double>(pos.Y))) - MapPane.Y0;
            const auto radius = static_cast<int32_t>(range);
            AGEllipseDraw(&MapPane, xPos, yPos, radius, radius, color);
        }

        MCTeam* enemy = HomeTeam == InnerSphereTeam ? ClanTeam : InnerSphereTeam;

        for (int32_t i = 0; i < enemy->NumSensors; i++)
        {
            MCSensorSystem* sensor = enemy->Sensors[i];

            if (sensor->Owner->IsBuilding() == 0 || sensor->Enabled() == 0 || sensor->Owner->IsRevealed() == 0)
            {
                continue;
            }

            const float range = (sensor->GetSkilledRange() / MetersPerPixel) * WorldUnitsPerMeter;

            if (range <= 0.0)
            {
                continue;
            }

            const MCVector3D position = sensor->Owner->GetPosition();
            MCVector3D pos(position.X, position.Y, 0.0f);
            WorldToTacMap(pos, -1);
            const int32_t xPos = static_cast<int32_t>(std::floor(static_cast<double>(pos.X))) - MapPane.X0;
            const int32_t yPos = static_cast<int32_t>(std::floor(static_cast<double>(pos.Y))) - MapPane.Y0;
            const uint8_t color = sensor->Multiplier < 1.0 ? 0xf2 : 0xef;
            const auto radius = static_cast<int32_t>(range);
            AGEllipseDraw(&MapPane, xPos, yPos, radius, radius, color);
        }
    }

    // The home side's mechs; the selected ones last, on top.
    MCObjectQueueNode* mechList = HomeTeam == InnerSphereTeam ? InnerSphereMechList : ClanMechList;
    std::vector<std::pair<int32_t, int32_t>> selected;

    for (MCBaseObject* node = mechList->Head; node != nullptr; node = node->Next)
    {
        auto* obj = static_cast<MCGameObject*>(node);

        if (obj->GetAwake() == 0 || obj->IsDisabled() != 0)
        {
            continue;
        }

        const MCVector3D position = obj->GetPosition();
        MCVector3D pos(position.X, obj->GetPosition().Y, 0.0f);
        WorldToTacMap(pos, -1);
        const int32_t xPos = static_cast<int32_t>(std::floor(static_cast<double>(pos.X))) - MapPane.X0;
        const int32_t yPos = static_cast<int32_t>(std::floor(static_cast<double>(pos.Y))) - MapPane.Y0;

        if (TheInterface->IsSelected(obj->PartId) == 0)
        {
            AGEllipseFill(&MapPane, xPos, yPos, 2, 2, 0xf);
        }
        else
        {
            selected.emplace_back(xPos, yPos); // Port: a vector (the original's stack arrays have no bound).
        }
    }

    for (const auto& [xPos, yPos] : selected)
    {
        AGEllipseFill(&MapPane, xPos, yPos, 2, 2, 0xb);
    }

    // Artillery strikes: the home side's, and the enemy's in their last 4 seconds, blinking.
    MCObjectQueueNode* defaultList = ObjectList->FindList(DefaultListId);

    if (defaultList == nullptr)
    {
        return;
    }

    for (MCBaseObject* node = defaultList->Head; node != nullptr; node = node->Next)
    {
        if (node->ObjectClass != ARTILLERY)
        {
            continue;
        }

        auto* strike = static_cast<MCArtillery*>(node);
        const bool ours = strike->GetAlignment() == HomeTeam->Alignment;

        if (!ours && strike->TimeToImpact >= 4.0)
        {
            continue;
        }

        const MCVector3D position = strike->GetPosition();
        MCVector3D pos(position.X, position.Y, 0.0f);
        WorldToTacMap(pos, -1);
        const int32_t xPos = static_cast<int32_t>(std::floor(static_cast<double>(pos.X))) - MapPane.X0;
        const int32_t yPos = static_cast<int32_t>(std::floor(static_cast<double>(pos.Y))) - MapPane.Y0;
        auto* type = static_cast<MCArtilleryType*>(strike->GetObjectType());
        const auto diameter = static_cast<int32_t>(static_cast<int32_t>(type->NominalMinorRange) * 2);
        int32_t dotSize = static_cast<int32_t>(static_cast<float>(diameter) / MetersPerPixel);
        int32_t ring = 0;

        if (dotSize == 0)
        {
            // Too small to see: a dot, and while the strike is live, its sensor range.
            dotSize = 2;

            if (strike->TimeToImpact < 0.0)
            {
                int32_t range = 0;

                if (strike->SensorSystem != nullptr)
                {
                    range = static_cast<int32_t>(strike->SensorSystem->GetSkilledRange());
                }

                range = static_cast<int32_t>(static_cast<float>(range) / MetersPerPixel);
                ring = static_cast<int32_t>(static_cast<float>(range) * WorldUnitsPerMeter);
            }
        }

        if (OnNow != 0)
        {
            continue;
        }

        uint8_t fill;

        if (strike->GetAlignment() == HomeTeam->Alignment)
        {
            if (ring > 0)
            {
                AGEllipseDraw(&MapPane, xPos, yPos, ring, ring, strike->SensorSystem->Multiplier < 1.0 ? 0xec : 0x1f);
            }

            fill = 0xf;
        }
        else
        {
            if (ring > 0)
            {
                AGEllipseDraw(&MapPane, xPos, yPos, ring, ring, strike->SensorSystem->Multiplier < 1.0 ? 0xf2 : 0xef);
            }

            fill = 0xcf;
        }

        AGEllipseFill(&MapPane, xPos, yPos, dotSize, dotSize, fill);
    }
}

auto MCTacticalMap::SetDisplayType(MCTacmapDisplayTypes type) -> void
{
    StatusDirty = -1;
    InfoDirty = -1;
    DisplayType = type;

    // Hide every page's parts, then show the new page's.
    SalvageText->ShowGuiWindow(0);

    if (MPlayer != nullptr)
    {
        ChatWindow->ShowGuiWindow(0);

        if (Application->TextObject() == ChatWindow->ChatInput)
        {
            Application->ReleaseText();
        }
    }

    InfoText->ShowGuiWindow(0);
    InfoText->Clear();

    for (MCGuiButton* button : ScrollButtons)
    {
        button->ShowGuiWindow(0);
    }

    VideoWindow->ShowGuiWindow(0);
    StopVideo();

    for (MCGuiToolButton* button : DataButtons)
    {
        button->ShowGuiWindow(0);
    }

    switch (type)
    {
        case TACMAP_MAP:
        {
            if (TabHighlighted != 0)
            {
                TabBottom->SetBackground(const_cast<char*>("mfdmts02.tga"));
            }

            TabStrip->SetBackground(const_cast<char*>(MPlayer == nullptr ? "mfdmts01.tga" : "mfdmts03.tga"));
            TabHighlighted = 0;
            // (The original copied the page's background into the MFD's picture here; draw shows it each frame.)

            for (MCGuiButton* button : ScrollButtons)
            {
                button->ShowGuiWindow(-1);
            }

            VideoWindow->ShowGuiWindow(-1);
            break;
        }

        case TACMAP_INFO:
        {
            for (MCGuiToolButton* button : DataButtons)
            {
                button->ShowGuiWindow(-1);
            }

            if (TabHighlighted != 0)
            {
                TabBottom->SetBackground(const_cast<char*>("mfddts02.tga"));
            }

            TabStrip->SetBackground(const_cast<char*>(MPlayer == nullptr ? "mfddts01.tga" : "mfddts03.tga"));
            TabHighlighted = 0;

            // (The original wiped the picture without a unit, and copied the page's background; draw does both.)
            if (InfoObject != nullptr)
            {
                SetID(InfoObject->PartId);
            }

            InfoText->MoveTo(7, 0x5e, 0);
            InfoText->Resize(0x74, 0x69);
            InfoText->ShowGuiWindow(-1);
            SetPageRects(this, 0x5d, 0x68, 0xbe, 0xc9);
            InfoText->FirstPixel = 0;
            RefreshPage();
            return;
        }

        case TACMAP_MISSION:
        {
            if (TabHighlighted != 0)
            {
                TabBottom->SetBackground(const_cast<char*>("mfdbts02.tga"));
            }

            TabStrip->SetBackground(const_cast<char*>(MPlayer == nullptr ? "mfdbts01.tga" : "mfdbts03.tga"));
            TabHighlighted = 0;
            InfoText->MoveTo(5, 0x22, 0);
            InfoText->Resize(0x76, 0xb8);
            InfoText->ShowGuiWindow(-1);
            SetPageRects(this, 0x22, 0x2d, 0xcf, 0xda);
            PageRects[2][3] = 0xce;
            RefreshPage();
            return;
        }

        case TACMAP_SALVAGE:
        {
            TabBottom->SetBackground(const_cast<char*>("mfdsts02.tga"));
            TabStrip->SetBackground(const_cast<char*>(MPlayer == nullptr ? "mfdsts01.tga" : "mfdsts03.tga"));
            TabHighlighted = -1;

            if (MPlayer == nullptr)
            {
                SalvageText->ShowGuiWindow(-1);
                RefreshSalvageList();
                SetPageRects(this, 0x22, 0x2d, 0xcf, 0xda);
                PageRects[2][3] = 0xce;
                RefreshPage();
                return;
            }

            // Multiplayer: the chat window, and the tab stops blinking.
            Application->RemoveTimer(this, 1);

            if (ChatBlinkerOn != nullptr)
            {
                ChatBlinkerOn->ShowGuiWindow(0);
            }

            if (ChatBlinkerOff != nullptr)
            {
                ChatBlinkerOff->ShowGuiWindow(0);
            }

            ChatWindow->ShowGuiWindow(-1);
            break;
        }
    }

    RefreshPage();
}

auto MCTacticalMap::CenterOnObject(MCGameObject* obj) -> void
{
    MCObjectPosition* position = obj->GetObjPosition();
    ScrollX = position->TileC - MapVertexSide;
    ScrollY = position->TileR - MapVertexSide;
}

auto MCTacticalMap::ScrollMap(int32_t dx, int32_t dy) -> void
{
    // Each axis only moves if the view stays on the picture.
    const int32_t oldX = ScrollX;
    const int32_t oldY = ScrollY;
    ScrollX = oldX + dx;
    ScrollY = dy + oldY;

    if (!ScrollInPicture(ScrollX, MapWidth, Zoom))
    {
        ScrollX = oldX;
    }

    if (!ScrollInPicture(ScrollY, MapHeight, Zoom))
    {
        ScrollY = oldY;
    }
}

auto MCTacticalMap::SetScrollMapPosition(int32_t x, int32_t y) -> void
{
    // Jump there, then step back towards the old position (by the scroll speed) until the view is on the picture.
    const int32_t oldX = ScrollX;
    const int32_t oldY = ScrollY;
    ScrollY = y;
    ScrollX = x;
    int32_t stepY = TheInterface->TacScrollSpeed;
    const int32_t stepX = oldX < x ? -stepY : stepY;

    if (oldY < y)
    {
        stepY = -stepY;
    }

    int doneX = 0;
    int doneY = 0;

    while (doneX == 0 || doneY == 0)
    {
        if (!ScrollInPicture(ScrollX, MapWidth, Zoom))
        {
            ScrollX += stepX;
        }
        else
        {
            doneX = -1;
        }

        if (!ScrollInPicture(ScrollY, MapHeight, Zoom))
        {
            ScrollY += stepY;
        }
        else
        {
            doneY = -1;
        }
    }
}

auto MCTacticalMap::GetVideoRect() -> tagRECT
{
    // The name line's height (unscaled), then lineFont back to its double scale.
    LineFont->Scaled = 0;
    LineFont->Scale = 1.0f;
    uint8_t lineHeight = LineFont->FontHeight;

    if (LineFont->Scaled != 0)
    {
        lineHeight = static_cast<uint8_t>(std::floor(static_cast<float>(lineHeight) * LineFont->Scale));
    }

    LineFont->Scale = 2.0f;
    LineFont->Scaled = 1;
    tagRECT rect;
    rect.left = VideoWindow->GlobalX();
    rect.top = VideoWindow->GlobalY() + lineHeight + 4;
    rect.right = VideoWindow->Width();
    rect.bottom = VideoWindow->Height() - (lineHeight + 4);
    return rect;
}

auto MCTacticalMap::AddSalvage(MCGameObject* obj) -> int
{
    const int32_t count = NumSalvage;

    if (count > 99)
    {
        // Original behaviour (OB-052): full, the count drops back to 99, forgetting the last entry.
        NumSalvage = 99;
        return 0;
    }

    if (obj->ObjectClass != BATTLEMECH && obj->ObjectClass != GROUNDVEHICLE && obj->IsBuilding() == 0)
    {
        return 0;
    }

    // A mech whose status byte is 2 isn't salvage.
    if (static_cast<uint8_t>(obj->Status) == 2 && obj->ObjectClass == BATTLEMECH)
    {
        return 0;
    }

    for (int32_t i = 0; i < count; i++)
    {
        if (Salvage[i] == obj)
        {
            return -1;
        }
    }

    if (MPlayer == nullptr && obj->IsBuilding() != 0)
    {
        SoundSystem->PlayBettySample(2);
    }

    Salvage[count] = obj;
    RealSalvageCount = NumSalvage + 1;
    NumSalvage = RealSalvageCount;
    AddSalvageString(obj);
    return -1;
}

auto MCTacticalMap::RemoveSalvage(MCGameObject* obj, int refresh) -> int
{
    const int32_t count = NumSalvage;
    int32_t i = 0;

    while (i < count && Salvage[i] != obj)
    {
        i++;
    }

    // Original behaviour (OB-051): not found, it still tests the entry just past the end, which after an earlier
    // removal holds a stale copy of the last one. (At 100 entries the original read the next field; never a match.)
    if (i >= 100 || Salvage[i] != obj)
    {
        return 0;
    }

    RealSalvageCount = count - 1;
    NumSalvage = RealSalvageCount;

    for (; i < NumSalvage; i++)
    {
        Salvage[i] = Salvage[i + 1];
    }

    if (refresh != 0)
    {
        RefreshSalvageList();
    }

    return -1;
}

auto MCTacticalMap::UpdateSalvage() -> void
{
    // Drop destroyed units, starting over after each.
    for (int32_t i = 0; i < NumSalvage; i++)
    {
        MCGameObject* obj = Salvage[i];

        if (obj != nullptr && IsMoverClass(obj) && obj->IsDestroyed() != 0)
        {
            RemoveSalvage(obj, -1);
            i = -1;
        }
    }

    RefreshSalvageList();
}

auto MCTacticalMap::RefreshSalvageList() -> void
{
    const int32_t firstPixel = SalvageText->FirstPixel;
    SalvageText->Clear();

    for (int32_t i = 0; i < NumSalvage; i++)
    {
        AddSalvageString(Salvage[i]);
    }

    MCGuiScrollTextObject* text = SalvageText;
    text->FirstPixel = firstPixel;
    text->ResetPortSize();
    text->PositionScrollTab();
}

auto MCTacticalMap::SetID(int32_t partId) -> void
{
    auto* obj = static_cast<MCGameObject*>(ObjectList->FindObjectFromPart(partId));

    if (DisplayType != TACMAP_INFO)
    {
        InfoObject = obj;
        return;
    }

    InfoDirty = -1;

    for (MCGuiPort* port : InfoPorts)
    {
        if (port != nullptr)
        {
            port->Destroy();
        }
    }

    if (obj == nullptr || !IsMoverClass(obj))
    {
        return;
    }

    // Port fix: the original leaves the shape file's name unset for elementals and other movers (whatever the stack
    // held); the port keeps the last one, starting with the generic vehicle's.
    static char shapeName[32] = "vr106";
    MCFile shapeFile;
    // (The original copied the info page's background into the MFD's picture here; draw shows it each frame.)
    StatusDirty = -1;

    if (obj->ObjectClass == BATTLEMECH)
    {
        // A mech: front, rear and payload views, and the pilot's picture.
        MCGuiToolButton* front = DataButtons[0];
        front->SetUpPicture(const_cast<char*>("mfddbh01.tga"));
        front->SetDownPicture(const_cast<char*>("mfddbg01.tga"));
        front->SetGrayPicture(const_cast<char*>("mfddbn01.tga"));
        front->MoveTo(0xf, 0xcc, 0);
        DataButtons[2]->MoveTo(0x56, 0xcc, 0);
        DataButtons[1]->ShowGuiWindow(-1);
        std::snprintf(shapeName, sizeof(shapeName), "mechrep%02i", obj->GetObjectType()->IconNumber);
        InfoPorts[0]->Init(obj->GetPilot()->Picture);
    }
    else if (obj->ObjectClass == GROUNDVEHICLE)
    {
        // A vehicle: no rear view; its passengers' pictures.
        MCGuiToolButton* front = DataButtons[0];
        front->SetUpPicture(const_cast<char*>("mfddbh00.tga"));
        front->SetDownPicture(const_cast<char*>("mfddbg00.tga"));
        front->SetGrayPicture(const_cast<char*>("mfddbn00.tga"));
        front->MoveTo(0x21, 0xcc, 0);
        DataButtons[2]->MoveTo(0x4b, 0xcc, 0);
        DataButtons[1]->ShowGuiWindow(0);

        if (DataDisplayMode == 1)
        {
            SetDataDisplayMode(0, 0);
        }

        auto* vehicle = static_cast<MCGroundVehicle*>(obj);

        // Original behaviour: the pictures go by seat, while draw shows the passengers packed (see draw).
        for (int32_t seat = 0; seat < vehicle->Seats; seat++)
        {
            if (vehicle->Passengers[seat] != nullptr)
            {
                InfoPorts[seat]->Init(vehicle->Passengers[seat]->Picture);
            }
        }

        if (obj->GetObjectType()->IconNumber == 0)
        {
            std::snprintf(shapeName, sizeof(shapeName), "vr106");
        }
        else
        {
            std::snprintf(shapeName, sizeof(shapeName), "vr%i", obj->GetObjectType()->IconNumber);
        }
    }

    // The part diagram's shapes.
    MCFullPathFileName shapePath;
    shapePath.Init(ArtPath, shapeName, ".shp");

    if (shapeFile.Open(shapePath, READ, 0x32) != 0)
    {
        Fatal(0, "Unable to open damage display shape file");
    }

    FreePartShapes();
    PartShapes = std::make_unique<uint8_t[]>(shapeFile.GetLength());
    shapeFile.Read(PartShapes.get(), static_cast<int32_t>(shapeFile.GetLength()));
    MCRenderer::RegisterData(PartShapes.get(), shapeFile.GetLength(), MCDataKind::Shapes);
    shapeFile.Close();
    InfoObject = obj;
    RefreshPage();
}

auto MCTacticalMap::UpdateOrderPalette() -> void
{
    // A mode chosen: only its button pushed.
    const int32_t command = TheInterface->CurrentCommand;

    if (command != 0)
    {
        for (MCToolPalButton* button : ToolButtons)
        {
            if (button->Action == command)
            {
                if (button->Pushed == 0)
                {
                    button->Pushed = -1;
                }
            }
            else if (button->Pushed != 0)
            {
                button->Pushed = 0;
            }
        }

        return;
    }

    if (TheInterface->AnySelected(0) == 0)
    {
        // Nothing selected: every mode button released and grayed.
        for (int32_t i = 0; i < NUM_MODE_BUTTONS; i++)
        {
            MCToolPalButton* button = ToolButtons[i];

            if (button->Pushed != 0)
            {
                button->Pushed = 0;
                TheInterface->CurrentCommand = 0;
                TheInterface->CommandOneShot = 0;
            }

            if (button->Disabled == 0)
            {
                button->Disabled = -1;
            }
        }

        return;
    }

    // A selection: released and enabled, the jump button only if every selected unit can jump.
    for (int32_t i = 0; i < NUM_MODE_BUTTONS; i++)
    {
        MCToolPalButton* button = ToolButtons[i];

        if (button->Pushed != 0)
        {
            button->Pushed = 0;
        }

        if (button->Action == 0x11)
        {
            const int cannotJump = TheInterface->CanSelectionJump() == 0 ? 1 : 0;

            if (button->Disabled != cannotJump)
            {
                button->Disabled = cannotJump;
            }
        }
        else if (button->Disabled != 0)
        {
            button->Disabled = 0;
        }
    }
}

auto MCTacticalMap::SetDataDisplayMode(char mode, int silent) -> void
{
    if (DataDisplayMode == mode)
    {
        return;
    }

    DataDisplayMode = mode;
    InfoDirty = -1;

    for (int32_t i = 0; i < 3; i++)
    {
        DataButtons[i]->Pushed = mode == i ? 1 : 0;
    }

    if (silent == 0)
    {
        SoundSystem->PlayDigitalSample(0x47, 1, nullptr, 0, 0);
    }
}

auto MCTacticalMap::ToggleZoom() -> void
{
    MCToolPalButton* button = ToolButtons[7];
    button->Pushed = button->Pushed == 0 ? 1 : 0;
}

auto MCTacticalMap::PositionOnMap(MCVector3D pos) -> MCVector3D
{
    // Clamp the unscrolled map position to the 130-pixel map; unclamped, the point is on it.
    MCVector3D onMap = pos;
    WorldToTacMap(onMap, 0);
    int clamped = 0;

    if (onMap.X < 0.0)
    {
        onMap.X = 0.0f;
        clamped = -1;
    }
    else if (onMap.X > MAP_PICTURE_SIDE)
    {
        onMap.X = MAP_PICTURE_SIDE;
        clamped = -1;
    }

    if (onMap.Y < 0.0)
    {
        onMap.Y = 0.0f;
    }
    else if (onMap.Y > MAP_PICTURE_SIDE)
    {
        onMap.Y = MAP_PICTURE_SIDE;
    }
    else if (clamped == 0)
    {
        return MCVector3D(0.0f, 0.0f, 0.0f);
    }

    TacMapToWorld(onMap, 0);
    return MCVector3D(pos.X - onMap.X, pos.Y - onMap.Y, 0.0f);
}

auto MCTacticalMap::HandleChatMessage(uint32_t fromID, const void* message) -> void
{
    ChatWindow->HandleNetworkMessage(fromID, const_cast<void*>(message));

    if (IsHidden() != 0 || DisplayType != TACMAP_SALVAGE)
    {
        // Not on show: blink the chat tab.
        ChatPending = -1;
        Application->AddTimer(this, 1, 500, 0, 0, 0);

        if (DisplayType == TACMAP_SALVAGE)
        {
            if (ChatBlinkerOn != nullptr)
            {
                ChatBlinkerOn->ShowGuiWindow(-1);
            }
        }
        else if (ChatBlinkerOff != nullptr)
        {
            ChatBlinkerOff->ShowGuiWindow(-1);
        }
    }

    if (SoundSystem != nullptr)
    {
        SoundSystem->PlayDigitalSample(0x11, 1, nullptr, 0, 0);
    }
}

auto MCTacticalMap::ActivateArtillery(int32_t button, int arm) -> void
{
    if (button < 0 || button >= 4)
    {
        return;
    }

    MCArtilleryButton* strike = ArtilleryButtons[button];
    MCGuiEvent event;

    if (arm == 0)
    {
        // Disarm as if Escape were pressed.
        strike->KeyArmed = 0;
        event.Clear();
        event.Type = EVENT_KEY_UP;
        event.Key = 0x1b;
    }
    else
    {
        // Arm as if clicked (pressed, then released).
        if (strike->KeyArmed != 0)
        {
            return;
        }

        strike->KeyArmed = -1;
        event.Clear();
        event.Type = EVENT_LEFT_DOWN;
        strike->HandleEvent(&event);
        event.Type = EVENT_LEFT_UP;
    }

    strike->HandleEvent(&event);
}

auto MCTacticalMap::AddSalvageString(MCGameObject* obj) -> void
{
    char line[64];
    MCGuiScrollTextObject* text = SalvageText;

    if (obj->ObjectClass == BATTLEMECH)
    {
        // A mech: its name, its undamaged weapons, and its sensor if undamaged.
        auto* mech = static_cast<MCMover*>(obj);
        std::snprintf(line, sizeof(line), "%s", mech->GetIfaceName());
        text->Print(line, 0xb);
        const uint32_t first = mech->NumOther;
        const uint32_t end = mech->NumWeapons + first;

        for (uint32_t i = first; i < end; i++)
        {
            const MCInventoryItem& item = mech->Inventory[i];

            if (item.Health == static_cast<int8_t>(MasterComponentList[item.MasterID].Health))
            {
                std::snprintf(line, sizeof(line), "    %s", MasterComponentList[item.MasterID].Abbreviation);
                text->Print(line, 0xc);
            }
        }

        const MCInventoryItem& sensor = mech->Inventory[mech->Sensor];

        if (sensor.Health == static_cast<int8_t>(MasterComponentList[sensor.MasterID].Health))
        {
            std::snprintf(line, sizeof(line), "    %s", MasterComponentList[sensor.MasterID].Abbreviation);
            text->Print(line, 0x1f);
        }
    }
    else if (obj->ObjectClass == GROUNDVEHICLE)
    {
        // A vehicle: its name and its salvage.
        std::snprintf(line, sizeof(line), "%s", static_cast<MCMover*>(obj)->GetIfaceName());
        text->Print(line, 0xb);

        for (MCSalvageItem* item = obj->GetSalvage(); item != nullptr; item = item->Next)
        {
            std::snprintf(line, sizeof(line), "    %i %s", item->NumItems,
                          MasterComponentList[item->ItemId].Abbreviation);
            text->Print(line, 0x1f);
        }
    }
    else
    {
        if (obj->IsBuilding() == 0)
        {
            SalvageText->ResetPortSize();
            return;
        }

        // A building or tree building: its name, its salvage.
        MCSalvageItem* item = obj->GetSalvage();

        if (obj->ObjectClass == BUILDING)
        {
            std::snprintf(line, sizeof(line), "%s", static_cast<MCBuilding*>(obj)->Name.c_str());
        }
        else if (obj->ObjectClass == TREEBUILDING)
        {
            std::snprintf(line, sizeof(line), "%s", static_cast<MCTreeBuilding*>(obj)->Name.c_str());
        }
        else
        {
            line[0] = 0; // Port fix: isBuilding is only true for these two, but the buffer was uninitialised.
        }

        text->Print(line, 0xb);

        for (; item != nullptr; item = item->Next)
        {
            std::snprintf(line, sizeof(line), "    %i %s", item->NumItems,
                          MasterComponentList[item->ItemId].Abbreviation);
            text->Print(line, 0x1f);
        }
    }

    text->Print(nullptr, 0x1f);
    SalvageText->ResetPortSize();
}

auto MCTacticalMap::DrawBar() -> void
{
    // The unit's effectiveness: green, yellow below half, red at a fifth.
    if (InfoObject == nullptr)
    {
        return;
    }

    const float effectiveness = static_cast<MCMover*>(InfoObject)->GetTotalEffectiveness();
    uint8_t color;

    if (effectiveness >= 0.5)
    {
        color = 0xb;
    }
    else
    {
        color = effectiveness > 0.2 ? 0xf2 : 0xef;
    }

    FillBox(0x22, 0x5f, 0x6d, 0x61, 0x10);
    const auto length = static_cast<int32_t>(effectiveness * 75.0f);

    if (length != 0)
    {
        FillBox(0x22, 0x5f, static_cast<int16_t>(length + 0x22), 0x61, color);
    }
}

auto MCTacticalMap::DrawParts() -> void
{
    auto* mover = static_cast<MCMover*>(InfoObject);

    if (mover == nullptr)
    {
        return;
    }

    // The armor locations: a mech's front ones (0..7) or, in the rear view, its rear ones over the rear diagram;
    // other units all of theirs.
    int16_t first;
    int16_t end;

    if (DataDisplayMode == 1)
    {
        AGShapeDraw(Port()->Frame(), PartShapes.get(), mover->NumBodyLocations + mover->NumArmorLocations, 0x22, 0x65);
        first = 8;
        end = mover->NumArmorLocations;
    }
    else
    {
        first = 0;
        end = mover->ObjectClass == BATTLEMECH ? 8 : mover->NumArmorLocations;
    }

    for (int32_t i = first; i < end; i++)
    {
        AGShapeLookaside(PartColorTable(this, ArmorColors[i]));
        AGShapeTranslateDraw(Port()->Frame(), PartShapes.get(), i, 0x22, 0x65);
    }

    // A mech's front view also shows its internal structure.
    if (mover->ObjectClass == BATTLEMECH && DataDisplayMode == 0)
    {
        for (int16_t i = 0; i < mover->NumBodyLocations; i++)
        {
            const int8_t numArmor = mover->NumArmorLocations;
            AGShapeLookaside(PartColorTable(this, BodyColors[i]));
            AGShapeTranslateDraw(Port()->Frame(), PartShapes.get(), static_cast<int16_t>(numArmor + i), 0x22, 0x65);
        }
    }
}

auto MCTacticalMap::GetColors() -> void
{
    auto* mover = static_cast<MCMover*>(InfoObject);

    if (mover == nullptr)
    {
        return;
    }

    for (int32_t i = 0; i < mover->NumBodyLocations; i++)
    {
        const MCBodyLocation& location = mover->BodyAt(i);

        if (location.DamageState == 2)
        {
            BodyColors[i] = 0x19;
        }
        else
        {
            BodyColors[i] = DamageColor(location.CurInternalStructure, location.MaxInternalStructure);
        }
    }

    for (int32_t i = 0; i < mover->NumArmorLocations; i++)
    {
        const MCArmorLocation& location = mover->Armor[i];

        if (location.CurArmor == 0.0)
        {
            ArmorColors[i] = 0x19;
        }
        else
        {
            ArmorColors[i] = DamageColor(location.CurArmor, location.MaxArmor);
        }
    }
}

auto MCTacticalMap::DrawPilot(MCMechWarrior* pilot) -> void
{
    // The picture.
    static const int32_t skillOrder[4] = {3, 0, 1, 2};
    FillBox(10, 0x2e, 0x21, 0x4c, 0x10);

    if (static_cast<MCMover*>(pilot->Vehicle)->NetPlayerId >= 0)
    {
        VfxPaneCopy(InfoPorts[0]->Frame(), 0, 0, DisplayPort->Frame(), 10, 0x2e, 0xfff);
    }

    // Four skill bars, 55 pixels at the best skill, each an outlined, shaded bar.
    int32_t yPos = 0x2f;

    for (const int32_t skill : skillOrder)
    {
        const auto value = static_cast<float>(pilot->Skills[skill]);
        const auto length = static_cast<int32_t>(((value - MinPilotSkill) * 55.0f) / (MaxPilotSkill - MinPilotSkill));
        const int32_t barEnd = length + 0x4c;
        VfxLineDraw(Port()->Frame(), 0x4e, yPos, 0x4e, yPos + 1, LD_DRAW, 0xe3);
        VfxLineDraw(Port()->Frame(), 0x4f, yPos - 1, barEnd, yPos - 1, LD_DRAW, 0xe3);
        AGPixelWrite(Port()->Frame(), length + 0x4d, yPos - 1, 0x10);
        VfxLineDraw(Port()->Frame(), length + 0x4e, yPos - 1, length + 0x4e, yPos + 2, LD_DRAW, 0x10);
        AGPixelWrite(Port()->Frame(), length + 0x4d, yPos + 2, 0x10);
        VfxLineDraw(Port()->Frame(), length + 0x4d, yPos, length + 0x4d, yPos + 1, LD_DRAW, 0xe3);
        VfxLineDraw(Port()->Frame(), 0x4f, yPos + 2, barEnd, yPos + 2, LD_DRAW, 0xe5);
        VfxLineDraw(Port()->Frame(), 0x4f, yPos, barEnd, yPos, LD_DRAW, 0xe4);
        VfxLineDraw(Port()->Frame(), 0x4f, yPos + 1, barEnd, yPos + 1, LD_DRAW, 0xe4);
        yPos += 8;
    }

    // The wounds blank out health pips, right to left.
    int16_t pipX = 0x1b;

    for (int32_t i = 0; i < 6; i++)
    {
        if (pilot->Wounds <= static_cast<float>(i))
        {
            break;
        }

        FillBox(pipX, 0x2b, static_cast<int16_t>(pipX + 1), 0x2c, 0x10);
        pipX = static_cast<int16_t>(pipX - 3);
    }

    // The callsign and the rank.
    WhiteFont->WriteString(Port()->Frame(), 10, 0x23, reinterpret_cast<uint8_t*>(pilot->Callsign), -1);
    char rank[256] = {}; // Port fix: a rank above 3 printed the uninitialised buffer.
    if (pilot->Rank <= 3)
    {
        CLoadString(ThisInstance, 0x86 + pilot->Rank, rank, 0xfe);
    }

    WhiteFont->WriteString(Port()->Frame(), 0x33, 0x23, reinterpret_cast<uint8_t*>(rank), -1);
}

auto MCTacticalMap::DrawWeapons() -> void
{
    MCGuiScrollTextObject* text = InfoText;
    const int32_t firstPixel = text->FirstPixel;
    text->Clear();
    auto* mover = static_cast<MCMover*>(InfoObject);

    if (mover == nullptr || !IsMoverClass(mover))
    {
        return;
    }

    // The weapons, sorted by range bracket (up to 75, 150, beyond) and then damage.
    const int32_t numWeapons = mover->NumWeapons;
    std::vector<MCWeaponEntry> weapons(static_cast<size_t>(std::max(numWeapons, 0)));

    const int32_t firstWeapon = mover->NumOther;

    for (int32_t i = firstWeapon; i < mover->NumWeapons + firstWeapon; i++)
    {
        const MCInventoryItem& item = mover->Inventory[i];
        MCWeaponEntry& entry = weapons[i - firstWeapon];
        entry.MasterID = item.MasterID;
        const MCMasterComponent& component = MasterComponentList[item.MasterID];
        const float longRange = component.WeaponRange[3];

        if (longRange > 150.0f)
        {
            entry.SortKey = 20000;
        }
        else if (longRange > 75.0f)
        {
            entry.SortKey = 10000;
        }
        else
        {
            entry.SortKey = 0;
        }

        entry.SortKey = static_cast<int16_t>(component.Damage + static_cast<float>(entry.SortKey));

        if (static_cast<int32_t>(item.Health) < static_cast<int8_t>(component.Health))
        {
            entry.State = 0xff;
        }
        else
        {
            entry.State = mover->GetWeaponShots(i) == 0 ? 0 : 1;
        }
    }

    std::qsort(weapons.data(), static_cast<size_t>(numWeapons), sizeof(MCWeaponEntry), CompareWeapons);

    // Three sections under their headers: red when damaged, yellow without ammo, green ready. Clan weapons get the
    // bracket's clan icon.
    static const int16_t bracketLimits[3] = {0x4b, 0x96, 0xe1};
    static const uint32_t headerIds[3] = {0x55, 0x50, 0x6d};
    static const char* const bracketFormats[3] = {"{    %s", "|    %s", "}    %s"};
    int32_t sectionCounts[4] = {};
    int32_t next = 0;
    char header[256];
    char line[256];
    uint8_t color = 0;

    for (int32_t bracket = 0; bracket < 3; bracket++)
    {
        sectionCounts[bracket] = 0;
        CLoadString(ThisInstance, headerIds[bracket], header, 0xfe);
        text->Print(header, 0x1f);
        const auto limit = static_cast<float>(bracketLimits[bracket]);

        for (; next < numWeapons; next++)
        {
            const MCWeaponEntry& entry = weapons[next];
            const MCMasterComponent& component = MasterComponentList[entry.MasterID];

            if (component.WeaponRange[3] > limit && bracket != 2)
            {
                break;
            }

            if (entry.State == 0xff)
            {
                color = 0xef;
            }
            else if (entry.State == 0)
            {
                color = 0xf2;
            }
            else if (entry.State == 1)
            {
                color = 0xc;
            }

            std::snprintf(line, sizeof(line), bracketFormats[bracket], component.Abbreviation);

            if (component.TechBase == 1)
            {
                line[0] = static_cast<char>(0x1d + bracket);
            }

            text->Print(line, color);
            sectionCounts[bracket]++;
        }
    }

    // The equipment: sensor, ECM, jammer, probe; red when disabled or destroyed.
    CLoadString(ThisInstance, 0x37e, header, 0xfe);
    text->Print(header, 0x1f);
    const uint8_t equipment[4] = {mover->Sensor, mover->Ecm, mover->Jammer, mover->Probe};

    for (const uint8_t index : equipment)
    {
        if (index == 0xff)
        {
            continue;
        }

        const MCInventoryItem& item = mover->Inventory[index];
        color = (item.Disabled != 0 || item.Health == 0) ? 0xef : 0xc;
        std::snprintf(line, sizeof(line), "    %s", MasterComponentList[item.MasterID].Abbreviation);
        text->Print(line, color);
    }

    // The ammo: red when out, yellow under half.
    for (int32_t i = 0; i < mover->NumAmmoTypes; i++)
    {
        const MCAmmoTally& ammo = mover->AmmoTypeTotal[i];
        const int32_t amount = ammo.CurAmount;

        if (amount == 9999)
        {
            continue;
        }

        if (amount == 0)
        {
            color = 0xef;
        }
        else
        {
            color = ammo.StartAmount / 2 <= amount ? 0xc : 0xf2;
        }

        std::snprintf(line, sizeof(line), "  %s", MasterComponentList[ammo.MasterId].Abbreviation);
        text->Print(line, color);
        CLoadString(ThisInstance, 0x380, header, 0xfe);
        std::snprintf(line, sizeof(line), header, amount);
        text->Print(line, color);
    }

    // The weapon sections take the range colours.
    int32_t start = 0;

    for (int32_t i = 0; i < 4; i++)
    {
        text->SectionStarts[i] = start;
        start += 1 + sectionCounts[i];
        text->SectionColors[i] = static_cast<uint8_t>(RangeColorArray[i]);
    }

    text->FirstPixel = firstPixel;
    text->ResetPortSize();
    text->PositionScrollTab();
}

auto CompareWeapons(const void* a, const void* b) -> int
{
    const auto keyA = static_cast<float>(static_cast<const MCWeaponEntry*>(a)->SortKey);
    const auto keyB = static_cast<float>(static_cast<const MCWeaponEntry*>(b)->SortKey);

    if (keyA == keyB)
    {
        return 0;
    }

    return keyB < keyA ? 1 : -1;
}
