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

int32_t buttonActions[8] = {15, 14, 13, 12, 19, 17, 3, 53};
int16_t RangeColorArray[4] = {0x0e, 0xe5, 0xee, 0x14};
char callingText[64] = {};
std::string statusString[4];
std::string typeString[5];
vector_3d tacMapCenter;
int32_t realSalvageCount = 0;
float tacFrameLength = 0.0f;
int onNow = 0;

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
    /// <remarks>Part of TacticalMap::display (MCX.EXE @ 0x00742e36..0x743549).</remarks>
    void drawMapPage(TacticalMap* map);

    /// <summary>A weapon of the info page's list (drawWeapons builds an array of them; 4 bytes).</summary>
    struct WeaponEntry
    {
        /// <summary>The weapon's MasterComponentList index.</summary>
        uint8_t masterID; // +0x0
        /// <summary>0xff damaged, 0 no shots left, 1 ready.</summary>
        uint8_t state; // +0x1
        /// <summary>The range bracket (0, 10000, 20000) plus the damage: the sort key.</summary>
        int16_t sortKey; // +0x2
    };

    /// <summary>The contacts drawObjects fetches from the home team (the unnamed 0x00809f78, 0x400 bytes).</summary>
    GameObject* contactList[256] = {};

    /// <summary>A mech, vehicle, elemental or other mover (the classes that have a pilot and a sensor).</summary>
    bool isMoverClass(GameObject* obj)
    {
        return obj->objectClass == BATTLEMECH || obj->objectClass == GROUNDVEHICLE || obj->objectClass == ELEMENTAL ||
               obj->objectClass == MOVER;
    }

    /// <summary>
    /// Draws a contact's sensor range around its dot: red, or white for the home side's alignment, or yellow while
    /// the sensor is weakened.
    /// </summary>
    void drawSensorRange(TacticalMap* map, GameObject* obj, int32_t xPos, int32_t yPos, int32_t homeAlignment)
    {
        SensorSystem* sensor = static_cast<Mover*>(obj)->sensorSystem;
        float range = -1.0f;

        if (sensor != nullptr && sensor->enabled() != 0)
        {
            range = (sensor->getSkilledRange() * worldUnitsPerMeter) / map->metersPerPixel;
        }

        if (range <= 0.0)
        {
            return;
        }

        uint8_t color = 0xef;

        if (obj->getAlignment() == homeAlignment)
        {
            color = 0x1f;
        }

        if (sensor->multiplier < 1.0)
        {
            color = 0xf2;
        }

        const auto radius = static_cast<int32_t>(range);
        AG_ellipse_draw(&map->mapPane, xPos, yPos, radius, radius, color);
    }

    /// <summary>
    /// Whether a scroll position keeps the zoomed view's both edges on the map picture (<paramref name="side"/>
    /// pixels along that axis).
    /// </summary>
    bool scrollInPicture(int32_t scroll, int32_t side, int32_t zoom)
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
    void setPageRects(TacticalMap* map, int32_t upTop, int32_t upBottom, int32_t downTop, int32_t downBottom)
    {
        int32_t (&rects)[3][4] = map->pageRects;
        rects[0][0] = 0x7d;
        rects[0][1] = upTop;
        rects[0][2] = 0x88;
        rects[0][3] = upBottom;
        map->scrollUpMarker->moveTo(0x7d, upTop, 0);
        rects[1][0] = 0x7d;
        rects[1][1] = downTop;
        rects[1][2] = 0x88;
        rects[1][3] = downBottom;
        map->scrollDownMarker->moveTo(0x7d, downTop, 0);
        rects[2][0] = 0x7d;
        rects[2][1] = upBottom;
        rects[2][2] = 0x88;
        rects[2][3] = downTop;
    }

    /// <summary>The part diagram's colour of a location from what is left of it: green, yellow, orange or red.</summary>
    uint8_t damageColor(float current, uint8_t maximum)
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
    /// The table the part diagram's shape is drawn through for a <see cref="damageColor"/> colour: rows of the fade
    /// palettes past the haze levels, or for a destroyed location (0x19) the map's colour remap.
    /// </summary>
    uint8_t* partColorTable(TacticalMap* map, uint8_t color)
    {
        const int32_t row = gamePalette->numBitmapHazeLevels;
        uint8_t* fades = gamePalette->fadePalettes.get();

        switch (color)
        {
            case 0xb:
                return fades + (row + 10) * 0x200;
            case 0x19:
                return map->colorRemap;
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
    template <typename Object> void destroyChild(Object*& obj)
    {
        if (obj == nullptr)
        {
            return;
        }

        obj->destroy();
        delete obj;
        obj = nullptr;
    }

    /// <summary>Destroys and deletes a port, and clears the pointer.</summary>
    void destroyPort(aPort*& port)
    {
        if (port == nullptr)
        {
            return;
        }

        port->destroy();
        delete port;
        port = nullptr;
    }

    TacticalMap* tacMap()
    {
        return Terrain::terrainTacticalMap;
    }

    /// <summary>A command palette mode button, or null out of range (the binary checks each index).</summary>
    ToolPalButton* modeButton(int32_t index)
    {
        return (index < 0 || index > 7) ? nullptr : tacMap()->toolButtons[index];
    }

    /// <summary>The support button's commander strike count.</summary>
    int32_t strikesLeft(int32_t commandId, bool& known)
    {
        known = true;

        switch (commandId)
        {
            case STRIKE_SENSOR:
                return HomeCommander->numSensorStrikes;
            case STRIKE_LARGE:
                return HomeCommander->numLargeStrikes;
            case STRIKE_SMALL:
                return HomeCommander->numSmallStrikes;
            case STRIKE_CAMERA_DRONE:
                return HomeCommander->numCameraDrones;
            default:
            {
                known = false;
                return 0;
            }
        }
    }

    /// <summary>Lets a support button give up the status line and the targeting cursor.</summary>
    void releaseStatusLine()
    {
        TacticalMap* map = tacMap();
        map->statusLocked = 0;
        map->statusDirty = -1;
        map->statusText = nullptr;
        application->cursorHidden = 0;
        application->SetCurrentCursor(static_cast<CursorType>(0));
    }

    /// <summary>
    /// Shared by the four scroll buttons: scroll once on press, then after the interface's scrollStart delay
    /// repeat five times as fast until release.
    /// </summary>
    void scrollButtonEvent(aObject* obj, aEvent* event, int32_t dx, int32_t dy)
    {
        if (event->type == EVENT_LEFT_DOWN)
        {
            application->AddTimer(obj, SCROLL_START_TIMER, theInterface->scrollStart, 0, 0, 0);
            tacMap()->scrollMap(dx, dy);
        }
        else if (event->type == EVENT_LEFT_UP)
        {
            application->RemoveTimer(obj, SCROLL_START_TIMER);
            application->RemoveTimer(obj, SCROLL_REPEAT_TIMER);
        }
        else if (event->type == EVENT_TIMER)
        {
            tacMap()->scrollMap(dx, dy);

            if (event->data == SCROLL_START_TIMER)
            {
                application->RemoveTimer(obj, SCROLL_START_TIMER);
                application->AddTimer(obj, SCROLL_REPEAT_TIMER, theInterface->scrollStart / 5, 0, 0, 0);
            }
        }
    }

    /// <summary>After a zoom, recentres the map on the main camera (unless at 1x) and redraws.</summary>
    void recentreAfterZoom(TacticalMap* map)
    {
        map->metersPerPixel = (map->mapDiagonal * DIAGONAL_TO_PIXELS) / static_cast<float>(map->zoom);

        if (eye == nullptr)
        {
            return;
        }

        vector_3d center = eye->position;
        map->scrollY = 0;
        map->scrollX = 0;
        const float zoom = static_cast<float>(map->zoom);
        const float scaleX = (static_cast<float>(map->mapWidth) * PICTURE_TO_PIXELS) / zoom;
        const float scaleY = (static_cast<float>(map->mapHeight) * PICTURE_TO_PIXELS) / zoom;

        if (map->zoom == 1)
        {
            map->scrollY = 0;
            map->scrollX = 0;
            return;
        }

        map->worldToTacMap(center, -1);
        center.x = (center.x - MAP_CENTER_X) * scaleX;
        center.y = (center.y - MAP_CENTER_Y) * scaleY;
        tacMap()->setScrollMapPosition(static_cast<int32_t>(center.x), static_cast<int32_t>(center.y));
    }

    /// <summary>Stops the pilot video (and its radio movie) when switching away from the map page.</summary>
    void stopVideo()
    {
        if (tacMap()->videoWindow->star == nullptr)
        {
            return;
        }

        RadioData* message = soundSystem->currentMessage;

        if (message->movieWindow != nullptr)
        {
            message->movieWindow->endSmackerMovie();
            delete message->movieWindow;
            message->movieWindow = nullptr;
            message->movie = nullptr;
        }

        tacMap()->videoWindow->SetStar(nullptr);
    }

    /// <summary>String <paramref name="id"/> of the string table.</summary>
    std::string loadHeapString(uint32_t id)
    {
        char buffer[256];
        cLoadString(thisInstance, id, buffer, 0xfe);
        return buffer;
    }

    /// <summary>Loads string <paramref name="id"/> into a help text (0x31 characters, as strncpy).</summary>
    void loadHelpText(char* helpText, uint32_t id)
    {
        char buffer[256];
        cLoadString(thisInstance, id, buffer, 0xfe);
        std::strncpy(helpText, buffer, 0x31);
    }

    /// <summary>
    /// A plain aObject child showing a picture. (The original loaded the picture into the object's own port; it is
    /// the object's background now, and the object draws itself.)
    /// </summary>
    aObject* makePicture(aObject* parent, int32_t x, int32_t y, int32_t w, int32_t h, const char* picture)
    {
        auto* obj = new aObject;
        obj->SetDrawsLive();
        obj->init(x, y, w, h, nullptr);
        obj->setBackground(const_cast<char*>(picture));
        return obj;
    }

    /// <summary>A button with its up, down and gray pictures.</summary>
    template <typename Button>
    Button* makeButton(int32_t x, int32_t y, int32_t w, int32_t h, const char* up, const char* down, const char* gray)
    {
        auto* button = new Button;
        button->init(x, y, w, h, nullptr);
        button->setUpPicture(const_cast<char*>(up));
        button->setDownPicture(const_cast<char*>(down));

        if (gray != nullptr)
        {
            button->setGrayPicture(const_cast<char*>(gray));
        }

        return button;
    }

    /// <summary>Loads a background port for one of the MFD pages.</summary>
    aPort* loadBackground(const char* fileName, const char* error)
    {
        auto* background = new aPort;
        const int32_t result = background->init(const_cast<char*>(fileName));
        Assert(result == 0, static_cast<uint32_t>(result), error);
        return background;
    }
}

auto TogglePalette() -> void
{
    TacticalMap* map = tacMap();

    if (map->IsShowing() == 0)
    {
        return;
    }

    aObject* frame = map->paletteFrame;
    frame->ShowGUIWindow(frame->IsShowing() == 0);

    if (map->paletteFrame->IsShowing() == 0)
    {
        soundSystem->playDigitalSample(0x41, 1, nullptr, 0, 0);
    }
    else
    {
        // Opening the palette drops the chosen mode.
        for (int32_t i = 0; i < NUM_MODE_BUTTONS; i++)
        {
            if (modeButton(i)->pushed != 0)
            {
                modeButton(i)->pushed = 0;
                theInterface->currentCommand = 0;
                theInterface->commandOneShot = 0;
            }
        }

        soundSystem->playDigitalSample(0x40, 1, nullptr, 0, 0);
    }

    tacMap()->paletteButton->pushed = tacMap()->paletteFrame->IsShowing();
}

auto BlinkerHandleEvent(aObject* obj, aEvent* event) -> void
{
    // The port's resize broadcast (0x12, see MCFollowWindowSize) already reaches every object, and has no position:
    // passed to what lies under (0, 0), the tactical map, it would come back here forever.
    if (event->type == 0x12)
    {
        return;
    }

    // Hides itself to find what lies under it, and passes the event there.
    obj->ShowGUIWindow(0);
    aObject* under = screenWindow->findObject(event->x, event->y);
    obj->ShowGUIWindow(-1);
    under->handleEvent(event);
}

auto TMCUp(aObject* obj, aEvent* event) -> void
{
    scrollButtonEvent(obj, event, 0, -theInterface->tacScrollSpeed);
}

auto TMCLeft(aObject* obj, aEvent* event) -> void
{
    scrollButtonEvent(obj, event, theInterface->tacScrollSpeed, 0);
}

auto TMCDown(aObject* obj, aEvent* event) -> void
{
    scrollButtonEvent(obj, event, 0, theInterface->tacScrollSpeed);
}

auto TMCRight(aObject* obj, aEvent* event) -> void
{
    scrollButtonEvent(obj, event, -theInterface->tacScrollSpeed, 0);
}

auto TMCZoomIn() -> void
{
    TacticalMap* map = tacMap();
    const int32_t oldZoom = map->zoom;
    const int32_t mapSide = map->mapVertexSide;
    map->zoom = oldZoom * 2;

    if (oldZoom * 2 < 9)
    {
        soundSystem->playDigitalSample(0x44, 1, nullptr, 0, 0);
        map = tacMap();
        map->zoomOffset += (mapSide >> 1) / map->zoom;
    }
    else
    {
        map->zoom = 8;
    }

    recentreAfterZoom(map);
    map = tacMap();

    if (map->zoom == 8)
    {
        aButton* zoomIn = map->scrollButtons[4];
        zoomIn->disabled = -1;
        map = tacMap();
    }

    aButton* zoomOut = map->scrollButtons[5];
    zoomOut->disabled = 0;

    // Zoomed in, the map can scroll.
    for (int32_t i = 0; i < 4; i++)
    {
        aButton* scroll = tacMap()->scrollButtons[i];
        scroll->disabled = 0;
    }

    tacMap()->RefreshPage();
}

auto TMCZoomOut() -> void
{
    TacticalMap* map = tacMap();
    map->zoomOffset -= (map->mapVertexSide >> 1) / map->zoom;

    if (map->zoomOffset < 0)
    {
        map->zoomOffset = 0;
    }

    const int32_t oldZoom = map->zoom;
    map->zoom = oldZoom >> 1;

    if ((oldZoom >> 1) == 0)
    {
        map->zoom = 1;
    }
    else
    {
        soundSystem->playDigitalSample(0x45, 1, nullptr, 0, 0);
        map = tacMap();
    }

    if (map->zoom == 1)
    {
        // At 1x the whole map shows: no zooming out or scrolling.
        aButton* zoomOut = map->scrollButtons[5];
        zoomOut->disabled = -1;

        for (int32_t i = 0; i < 4; i++)
        {
            aButton* scroll = tacMap()->scrollButtons[i];
            scroll->disabled = -1;
            map = tacMap();
        }
    }

    aButton* zoomIn = map->scrollButtons[4];
    zoomIn->disabled = 0;
    map = tacMap();
    recentreAfterZoom(map);
    map->RefreshPage();
}

auto ArmorFrontButton() -> void
{
    tacMap()->SetDataDisplayMode(0, 0);
}

auto PayloadButton() -> void
{
    tacMap()->SetDataDisplayMode(2, 0);
}

auto RearButton() -> void
{
    tacMap()->SetDataDisplayMode(1, 0);
}

auto ToolPalButton::enter() -> void
{
    TacticalMap* map = tacMap();

    if (map->statusLocked == 0)
    {
        map->statusDirty = -1;
        map->statusText = helpText;
    }
}

auto ToolPalButton::leave() -> void
{
    TacticalMap* map = tacMap();

    if (map->statusLocked == 0)
    {
        map->statusDirty = -1;
        map->statusText = nullptr;
    }
}

auto ArtilleryButton::init(int32_t xPos, int32_t yPos, int32_t w, int32_t h, char* fileName) -> int32_t
{
    const int32_t result = aButton::init(xPos, yPos, w, h, fileName);
    armed = 0;
    keyArmed = 0;
    disabled = 0;
    return result;
}

auto ArtilleryButton::draw() -> void
{
    bool known = false;
    const int32_t before = strikesLeft(commandId, known);

    if (known)
    {
        disabled = before < 1 ? -1 : 0;
    }

    aButton::draw();

    // The count, in the button's corner (the buffer is the original's 4-byte local).
    char count[16] = {};
    const int32_t left = strikesLeft(commandId, known);

    if (known)
    {
        std::snprintf(count, sizeof(count), "%02i", left);
    }

    if (disabled != 0)
    {
        greyFont->writeString(displayPort->frame(), 0x13, 9, reinterpret_cast<uint8_t*>(count), -1);
        return;
    }

    if (application->grabbedObject() == this && application->currentObject() == this)
    {
        whiteFont->writeString(displayPort->frame(), 0x13, 9, reinterpret_cast<uint8_t*>(count), -1);
        return;
    }

    blueFont->writeString(displayPort->frame(), 0x13, 9, reinterpret_cast<uint8_t*>(count), -1);
}

auto ArtilleryButton::handleEvent(aEvent* event) -> void
{
    if (disabled != 0)
    {
        if (application->grabbedObject() == this)
        {
            application->release();
        }

        if (event->type == EVENT_LEFT_DOWN)
        {
            soundSystem->playDigitalSample(0x46, 1, nullptr, 0, 0);
        }

        return;
    }

    if (event->type == EVENT_LEFT_DOWN)
    {
        if (armed == 0)
        {
            application->grab(this);
            draw();
        }

        return;
    }

    if (event->type == EVENT_LEFT_UP)
    {
        if (armed == 0)
        {
            // Armed: "Calling <strike>..." holds the status line until the target click.
            char format[256];
            cLoadString(thisInstance, 0x98, format, 0xfe);
            std::snprintf(callingText, sizeof(callingText), format, helpText);
            TacticalMap* map = tacMap();

            if (map->statusLocked == 0)
            {
                map->statusDirty = -1;
                map->statusText = callingText;
            }

            map->statusLocked = -1;
            armed = -1;
            application->SetCurrentCursor(static_cast<CursorType>(9));
            application->cursorHidden = -1;
            draw();
            return;
        }

        const int32_t screenX = event->x;
        const int32_t screenY = event->y;
        POINT inMap;
        inMap.x = screenX - tacMap()->globalX();
        inMap.y = screenY - tacMap()->globalY();
        armed = 0;
        application->release();
        draw();
        const RECT* mapArea = reinterpret_cast<const RECT*>(tacMap()->mapRect);

        if (PtInRect(mapArea, inMap) == 0 || tacMap()->displayType != TACMAP_MAP)
        {
            // Outside the tactical map: the click must land in the active view.
            aObject* target = screenWindow->findObject(screenX, screenY);

            if (target != mainHolder->GetActivePane() && target != theInterface->mechBar)
            {
                armed = 0;
                application->release();
                releaseStatusLine();
                draw();
                return;
            }

            aObject* pane = mainHolder->GetActivePane()->pointInside(screenX, screenY) == 0
                                ? mainHolder->GetInactivePane()
                                : mainHolder->GetActivePane();

            if (pane == nullptr)
            {
                return;
            }

            Camera* camera = pane->GetCamera();

            if (camera == nullptr)
            {
                return;
            }

            // Port: on the view's world surface, through the zoom.
            vector_2d screenPos = MCWindowPoint(pane, screenX, screenY);
            vector_3d target3d;
            camera->inverseProject(screenPos, target3d);
            theInterface->CallStrike(commandId, &target3d, nullptr, -1, 0, -1.0f);
        }
        else
        {
            vector_3d target3d(static_cast<float>(screenX - tacMap()->globalX()),
                               static_cast<float>(screenY - tacMap()->globalY()), 0.0f);
            tacMap()->tacMapToWorld(target3d, -1);
            theInterface->CallStrike(commandId, &target3d, nullptr, -1, 0, -1.0f);
        }

        TacticalMap* map = tacMap();
        map->statusLocked = 0;
        map->statusDirty = -1;
        map->statusText = nullptr;
        application->cursorHidden = 0;
        application->SetCurrentCursor(static_cast<CursorType>(0));
        return;
    }

    if (event->type == EVENT_KEY_UP)
    {
        // Backspace or Escape disarms.
        if (event->key == 8 || event->key == 0x1b)
        {
            application->release();
            armed = 0;
            releaseStatusLine();
            draw();
        }

        return;
    }

    // The port's resize broadcast (0x12) has no position: passed to the tactical map under (0, 0), it would come back
    // here forever (see BlinkerHandleEvent).
    if (event->type == 0x12)
    {
        return;
    }

    // Anything else goes to what lies under the mouse.
    aObject* under = screenWindow->findObject(event->x, event->y);

    if (under != this && under != nullptr)
    {
        event->target = under;
        under->handleEvent(event);
    }
}

auto ArtilleryButton::enter() -> void
{
    TacticalMap* map = tacMap();

    if (map->statusLocked == 0)
    {
        map->statusDirty = -1;
        map->statusText = helpText;
    }
}

auto ArtilleryButton::leave() -> void
{
    TacticalMap* map = tacMap();

    if (map->statusLocked == 0)
    {
        map->statusDirty = -1;
        map->statusText = nullptr;
    }
}

auto MapSwitch() -> void
{
    tacMap()->HideMe(0);

    if (tacMap()->displayType != TACMAP_MAP)
    {
        tacMap()->SetDisplayType(TACMAP_MAP);
    }
}

auto SalvageSwitch() -> void
{
    tacMap()->HideMe(0);

    if (tacMap()->displayType != TACMAP_SALVAGE)
    {
        tacMap()->SetDisplayType(TACMAP_SALVAGE);
        stopVideo();
    }
}

auto InfoSwitch() -> void
{
    tacMap()->HideMe(0);

    if (tacMap()->displayType != TACMAP_INFO)
    {
        tacMap()->SetDisplayType(TACMAP_INFO);
        stopVideo();
    }
}

auto MissionSwitch() -> void
{
    tacMap()->HideMe(0);

    if (tacMap()->displayType != TACMAP_MISSION)
    {
        tacMap()->SetDisplayType(TACMAP_MISSION);
        stopVideo();
    }
}

auto ToolPaletteButtonEvent(aObject* obj, aEvent* event) -> void
{
    if (event->type != EVENT_CALLBACK)
    {
        return;
    }

    auto* button = static_cast<ToolPalButton*>(obj);
    const int32_t action = button->action;

    if (action == ACTION_TOGGLE_ZOOM)
    {
        soundSystem->playDigitalSample(0x2f, 1, nullptr, 0, 0);
        ToggleZoom();
        return;
    }

    if (button->pushed != 0)
    {
        // One mode at a time.
        soundSystem->playDigitalSample(0x35, 1, nullptr, 0, 0);

        for (int32_t i = 0; i < NUM_MODE_BUTTONS; i++)
        {
            ToolPalButton* other = modeButton(i);

            if (other != button && other->pushed != 0)
            {
                other->pushed = 0;
            }
        }

        theInterface->currentCommand = action;
        theInterface->commandOneShot = -1;
        return;
    }

    soundSystem->playDigitalSample(0x34, 1, nullptr, 0, 0);
    theInterface->currentCommand = 0;
    theInterface->commandOneShot = 0;
}

auto TabStripEvent(aObject* obj, aEvent* event) -> void
{
    if (event->type == EVENT_LEFT_DOWN)
    {
        // The strip's four tabs, top to bottom; clicking the open page's tab while shown does nothing.
        const int32_t offset = event->y - obj->globalY();

        if (offset > 0x1b)
        {
            if (offset < 0x4c)
            {
                if (tacMap()->displayType == TACMAP_MAP && tacMap()->IsHidden() == 0)
                {
                    return;
                }

                MapSwitch();
            }
            else if (offset < 0x74)
            {
                if (tacMap()->displayType == TACMAP_INFO && tacMap()->IsHidden() == 0)
                {
                    return;
                }

                InfoSwitch();
            }
            else if (offset < 0xae)
            {
                if (tacMap()->displayType == TACMAP_MISSION && tacMap()->IsHidden() == 0)
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

                if (tacMap()->displayType == TACMAP_SALVAGE && tacMap()->IsHidden() == 0)
                {
                    return;
                }

                SalvageSwitch();
            }

            soundSystem->playDigitalSample(0x36, 1, nullptr, 0, 0);
            return;
        }

        // The strip's top toggles the MFD.
        application->grab(obj);
        tacMap()->HideMe(tacMap()->IsHidden() == 0);
    }
    else if (event->type == EVENT_LEFT_UP)
    {
        application->release();
    }
    else if (event->type == EVENT_MOUSE_MOVE)
    {
        application->SetCurrentCursor(static_cast<CursorType>(0));
    }
}

auto TabTopEvent(aObject* /*obj*/, aEvent* event) -> void
{
    if (event->type == EVENT_LEFT_DOWN)
    {
        tacMap()->HideMe(tacMap()->IsHidden() == 0);
    }
    else if (event->type == EVENT_MOUSE_MOVE)
    {
        application->SetCurrentCursor(static_cast<CursorType>(0));
    }
}

auto TabBottomEvent(aObject* /*obj*/, aEvent* event) -> void
{
    if (event->type == EVENT_LEFT_DOWN)
    {
        if (tacMap()->displayType != TACMAP_SALVAGE)
        {
            soundSystem->playDigitalSample(0x36, 1, nullptr, 0, 0);
            SalvageSwitch();
        }
    }
    else if (event->type == EVENT_MOUSE_MOVE)
    {
        application->SetCurrentCursor(static_cast<CursorType>(0));
    }
}

auto VideoWindow::init(int32_t xPos, int32_t yPos, int32_t w, int32_t h, char* fileName) -> int32_t
{
    star = nullptr;
    return aObject::init(xPos, yPos, w, h, fileName);
}

auto VideoWindow::draw() -> void
{
    // The picture (the original painted it when no pilot spoke, and a name stayed over it until then).
    if (backgroundPort != nullptr)
    {
        backgroundPort->copyTo(displayPort->frame(), 0, 0, -1);
    }

    if (star == nullptr)
    {
        return;
    }

    // The pilot's name.
    lineFont->scaled = 0;
    lineFont->scale = 1.0f;
    FillBox(1, 1, static_cast<int16_t>(width() - 2), 0xb, 0x10);
    lineFont->print(3, 3, star->callsign, 0xe3, displayPort->frame());
    lineFont->scale = 2.0f;
    lineFont->scaled = 1;
}

auto VideoWindow::Update() -> void
{
    if (star == nullptr)
    {
        return;
    }

    // Blink the pilot's unit on the mech bar every half second.
    if (blinkTime + 0.5 < scenarioTime)
    {
        if (blinkOn == 0)
        {
            blinkOn = -1;
            theInterface->mechBar->layout.videoId = star->vehicle->partId;
        }
        else
        {
            blinkOn = 0;
            theInterface->mechBar->layout.videoId = -1;
        }
    }

    // Track the unit on the tactical map.
    TrackStar(starMapX, starMapY);
    anchorX = static_cast<float>(width() / 2 + globalX());
    anchorY = static_cast<float>(globalY());
}

auto VideoWindow::TrackStar(float& mapX, float& mapY) -> void
{
    // The unit on the tactical map, or the window's anchor (its bottom centre) when it is off the map area.
    vector_3d position = star->vehicle->getPosition();
    tacMap()->worldToTacMap(position, -1);
    mapX = static_cast<float>(tacMap()->globalX()) + position.x;
    mapY = static_cast<float>(tacMap()->globalY()) + position.y;

    if (mapX < 6.0f || mapX > 136.0f || mapY < 34.0f || mapY > 164.0f)
    {
        mapX = static_cast<float>(width() / 2 + globalX());
        mapY = static_cast<float>(globalY());
    }
}

auto VideoWindow::display() -> void
{
    // The line from the window to the unit, which it follows. (The original's line ran to where the unit was when
    // the window last painted, and from where the window was the paint before.)
    if (star != nullptr)
    {
        float mapX = 0.0f;
        float mapY = 0.0f;
        TrackStar(mapX, mapY);
        VFX_line_draw(tacMap()->frame(), width() / 2 + globalX(), globalY(), static_cast<int32_t>(mapX),
                      static_cast<int32_t>(mapY), 0, 0x1f);
    }

    aObject::display();
}

auto VideoWindow::SetStar(MechWarrior* newStar) -> void
{
    if (newStar != nullptr)
    {
        blinkOn = 0;
        star = newStar;
        blinkTime = static_cast<float>(scenarioTime - 0.5);
        theInterface->mechBar->layout.videoId = newStar->vehicle->partId;
        Update();
        return;
    }

    if (star != nullptr)
    {
        theInterface->mechBar->layout.videoId = -1;
        GameObject* vehicle = star->vehicle;

        if (theInterface->IsSelected(vehicle->partId) != 0)
        {
            vehicle->setSelected(1);
            star = nullptr;
            Update();
            return;
        }

        vehicle->setSelected(0);
    }

    star = nullptr;
    Update();
}

TacticalMap::TacticalMap()
{
    infoWatcher = {};
}

TacticalMap::~TacticalMap()
{
    infoWatcher.free();
}

auto TacticalMap::setRevealedBitmap(char* fileName) -> void
{
    File gifFile;
    FullPathFileName gifName;
    gifName.init(terrainPath, fileName, ".gif");

    if (fileExists(gifName) == 0)
    {
        return;
    }

    gifFile.open(gifName, READ, 50);
    const uint32_t size = gifFile.fileSize();

    if (size == 0)
    {
        return;
    }

    auto* gif = static_cast<uint8_t*>(std::malloc(size));

    if (gif == nullptr)
    {
        return;
    }

    gifFile.read(gif, size);
    gifFile.close();
    VFX_GIF_resolution(gif);
    void* work = std::malloc(VFX_GIF_BUFFER_SIZE);
    VFX_GIF_draw(visibilityPort->frame(), gif, work);
    std::free(gif);
    std::free(work);
}

auto TacticalMap::init(int32_t xPos, int32_t yPos) -> int32_t
{
    unknown544 = -1;
    unknown540 = -1;
    unknown548 = -1;
    zoom = 1;
    infoObject = nullptr;
    zoomOffset = 0;
    objectivesRevealed = 0;
    freePartShapes();
    tacMapCenter = vector_3d(0.0f, 0.0f, 0.0f);
    mapVertexSide = Terrain::verticesBlockSide * Terrain::blocksMapSide;
    unknown554 = 0;
    unknown550 = 0;

    // The map picture.
    mapPort = new aPort;

    if (mapPort == nullptr)
    {
        Fatal(-1, "No RAM for TacMap");
    }

    FullPathFileName pictureName;
    pictureName.init(terrainPath, Terrain::terrainName, ".tga");
    int32_t result = mapPort->init(pictureName);
    Assert(result == 0, static_cast<uint32_t>(result), " could not start tacticalMap ");
    mapWidth = mapPort->frame()->window->x_max;
    mapHeight = mapPort->frame()->window->y_max;

    // The fog of war: a port whose pixels are the home side's visible bits.
    visibilityPort = new aPort;

    if (mapPort == nullptr)
    {
        Fatal(-1, "No RAM for TacMap");
    }

    visibilityPort->init(mapVertexSide, mapVertexSide);

    if (result != 0)
    {
        Fatal(result, " Unable to create Port for TacMap ");
    }

    MCRenderer::DestroyTexture(visibilityPort->frame()->window);
    aPort::freePixels(visibilityPort->frame()->window->buffer);
    ByteFlag* visibleBits = homeTeam->alignment == -1 ? Terrain::ClanVisibleBits : Terrain::terrainVisibleBits;
    visibilityPort->frame()->window->buffer = visibleBits->flagData.data();
    // Port: the fog of war is a kept frame surface: the reveals draw it on the GPU as well as in the flags the game
    // reads, and the map page samples the GPU's copy instead of uploading the flags whenever they change.
    MCRenderer::AddFrameSurface(visibilityPort->frame()->window, true);

    const float side = static_cast<float>(mapVertexSide) * static_cast<float>(mapVertexSide);
    mapDiagonal = std::sqrt(side + side) * Terrain::metersPerVertex;
    metersPerPixel = (mapDiagonal * DIAGONAL_TO_PIXELS) / static_cast<float>(zoom);

    mapBackground = loadBackground("mfdmwn00.tga", "Error reading tacmap MFD background");
    infoBackground = loadBackground("mfddwn00.tga", "Error reading info MFD background");
    missionBackground = loadBackground("mfdbwn00.tga", "Error reading mission MFD background");

    // Port: the info page's data view backgrounds, which the original loaded each time it drew one (and skipped
    // when one failed to load).
    static const char* const viewBackgroundNames[3] = {"mfddwn01.tga", "mfddwn02.tga", "mfddwn03.tga"};

    for (int32_t i = 0; i < 3; i++)
    {
        infoViewBackgrounds[i] = new aPort;

        if (infoViewBackgrounds[i]->init(const_cast<char*>(viewBackgroundNames[i])) != 0)
        {
            destroyPort(infoViewBackgrounds[i]);
        }
    }

    result = aObject::init(xPos, yPos, 0x8c, 0xef, nullptr);

    if (result != 0)
    {
        return result;
    }

    if (MPlayer == nullptr)
    {
        salvageBackground = missionBackground;
        result = 0;
    }
    else
    {
        salvageBackground = loadBackground("mfdswn01.tga", "Error reading salvage MFD background");
        chatWindow = new aChatWindow;
        result = chatWindow->init(6, 0x22, 0x82, 0x99, nullptr);
        addChild(chatWindow);
        Assert(result == 0, static_cast<uint32_t>(result), "Error initing chat object");
    }

    chatPending = 0;

    // The tabs along the MFD's right edge.
    tabTop = makePicture(this, width(), 0, 0xc, 4, "mfdmts00.tga");
    tabTop->setEventRoutine(TabTopEvent);
    tabTop->SetTransparent(-1);
    addChild(tabTop);

    tabStrip = new aObject;
    tabStrip->SetDrawsLive();
    tabStrip->init(width(), 4, 0xc, 0xe7, nullptr);

    if (MPlayer == nullptr)
    {
        tabStrip->setBackground(const_cast<char*>("mfdmts01.tga"));
    }
    else
    {
        const int32_t blinkerTop = tabStrip->bottom() - 0x3a;
        tabStrip->setBackground(const_cast<char*>("mfdmts03.tga"));

        chatBlinkerOff = new aObject;
        chatBlinkerOff->SetDrawsLive();
        result = chatBlinkerOff->init(width() + 2, blinkerTop, 10, 0x3a, nullptr);
        Assert(result == 0, static_cast<uint32_t>(result), "Error initing chat blinker object");
        chatBlinkerOff->setBackground(const_cast<char*>("mfdsts05.tga"));
        chatBlinkerOff->setDepth(10);
        addChild(chatBlinkerOff);
        chatBlinkerOff->ShowGUIWindow(0);
        chatBlinkerOff->setEventRoutine(BlinkerHandleEvent);

        chatBlinkerOn = new aObject;
        chatBlinkerOn->SetDrawsLive();
        result = chatBlinkerOn->init(width(), blinkerTop, 10, 0x3a, nullptr);
        Assert(result == 0, static_cast<uint32_t>(result), "Error initing chat blinker object");
        chatBlinkerOn->setBackground(const_cast<char*>("mfdsts04.tga"));
        chatBlinkerOn->setDepth(10);
        addChild(chatBlinkerOn);
        chatBlinkerOn->ShowGUIWindow(0);
        chatBlinkerOn->setEventRoutine(BlinkerHandleEvent);
    }

    tabStrip->setEventRoutine(TabStripEvent);
    addChild(tabStrip);

    tabBottom = makePicture(this, width(), 0xeb, 0xc, 4, "mfdmts02.tga");
    tabBottom->setEventRoutine(TabBottomEvent);
    tabBottom->SetTransparent(-1);
    addChild(tabBottom);

    objectType = 6;
    ShowGUIWindow(0);
    setBackColor(0x10);
    SetHideDirection(static_cast<DIRECTION>(0));

    // The support buttons.
    struct SupportButton
    {
        int32_t x;
        int32_t commandId;
        uint32_t helpId;
        const char* pictures[3];
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
        ArtilleryButton* button =
            makeButton<ArtilleryButton>(spec.x, 6, 0x1f, 0x16, spec.pictures[0], spec.pictures[1], spec.pictures[2]);
        artilleryButtons[i] = button;
        button->commandId = spec.commandId;
        loadHelpText(button->helpText, spec.helpId);
        button->unknown4FD = 0;
        addChild(button);
    }

    // The info page's data buttons.
    struct DataButton
    {
        int32_t index;
        int32_t x;
        void (*callback)();
        const char* pictures[3];
    };

    static const DataButton dataButtonSpecs[3] = {
        {0, 0xf, ArmorFrontButton, {"mfddbh01.tga", "mfddbg01.tga", "mfddbn01.tga"}},
        {2, 0x56, PayloadButton, {"mfddbh02.tga", "mfddbg02.tga", "mfddbn02.tga"}},
        {1, 0x35, RearButton, {"mfddbh03.tga", "mfddbg03.tga", "mfddbn03.tga"}},
    };

    for (const DataButton& spec : dataButtonSpecs)
    {
        aToolButton* button =
            makeButton<aToolButton>(spec.x, 0xcc, 0x20, 0xc, spec.pictures[0], spec.pictures[1], spec.pictures[2]);
        dataButtons[spec.index] = button;
        button->framed = 0;
        button->callback()->setExec(spec.callback);
        addChild(button);
    }

    // The scroll buttons (disabled at 1x) and the zoom buttons.
    struct ScrollButton
    {
        int32_t x;
        int32_t y;
        void (*routine)(aObject*, aEvent*);
        const char* pictures[3];
    };

    static const ScrollButton scrollSpecs[4] = {
        {0x4b, 0xb5, TMCUp, {"mfdmbh00.tga", "mfdmbg00.tga", "mfdmbn00.tga"}},
        {0x59, 0xc0, TMCLeft, {"mfdmbh01.tga", "mfdmbg01.tga", "mfdmbn01.tga"}},
        {0x4b, 0xcd, TMCDown, {"mfdmbh02.tga", "mfdmbg02.tga", "mfdmbn02.tga"}},
        {0x3f, 0xc0, TMCRight, {"mfdmbh03.tga", "mfdmbg03.tga", "mfdmbn03.tga"}},
    };

    for (int32_t i = 0; i < 4; i++)
    {
        const ScrollButton& spec = scrollSpecs[i];
        aButton* button =
            makeButton<aButton>(spec.x, spec.y, 0xd, 0xb, spec.pictures[0], spec.pictures[1], spec.pictures[2]);
        scrollButtons[i] = button;
        button->setEventRoutine(spec.routine);
        addChild(button);
    }

    aButton* zoomOutButton = makeButton<aButton>(0x74, 0xca, 0xd, 0xd, "mfdmbh04.tga", "mfdmbg04.tga", "mfdmbn04.tga");
    scrollButtons[5] = zoomOutButton;
    zoomOutButton->callback()->setExec(TMCZoomOut);
    zoomOutButton->disabled = -1;
    addChild(zoomOutButton);
    aButton* zoomInButton = makeButton<aButton>(0x74, 0xb6, 0xd, 0xd, "mfdmbh05.tga", "mfdmbg05.tga", "mfdmbn05.tga");
    scrollButtons[4] = zoomInButton;
    zoomInButton->callback()->setExec(TMCZoomIn);
    addChild(zoomInButton);

    for (int32_t i = 0; i < 4; i++)
    {
        aButton* button = tacMap()->scrollButtons[i];
        button->disabled = -1;
    }

    // The command palette, hidden until the palette button opens it.
    paletteFrame = new aObject;
    paletteFrame->SetDrawsLive();
    paletteFrame->init(0, 0xe9, 0x8c, 0x35, nullptr);
    paletteFrame->setDepth(10);
    paletteFrame->setBackground(const_cast<char*>("mfdcwn00.tga"));
    addChild(paletteFrame);
    paletteFrame->ShowGUIWindow(0);
    paletteBottom = new aObject;
    paletteBottom->SetDrawsLive();
    paletteBottom->init(paletteFrame->width(), 0, 2, 0x35, nullptr);
    paletteBottom->setBackground(const_cast<char*>("mfdcwn01.tga"));
    paletteBottom->SetTransparent(-1);
    paletteFrame->addChild(paletteBottom);

    paletteButton = makeButton<aToolButton>(6, 0xe0, 9, 9, "mfdcwn02.tga", "mfdcwn03.tga", nullptr);
    paletteButton->framed = 0;
    paletteButton->callback()->setExec(TogglePalette);
    addChild(paletteButton);

    scrollUpMarker = makePicture(this, 0, 0, 0xb, 0xb, "mfddbg04.tga");
    scrollUpMarker->ShowGUIWindow(0);
    addChild(scrollUpMarker);
    scrollDownMarker = makePicture(this, 0, 0, 0xb, 0xb, "mfddbg05.tga");
    scrollDownMarker->ShowGUIWindow(0);
    addChild(scrollDownMarker);

    lastMapTime = -999.0f;

    // The palette's mode buttons, two rows of four.
    int32_t buttonX = 6;
    int32_t buttonY = 2;

    for (int16_t i = 0; i < 8; i++)
    {
        auto* button = new ToolPalButton;
        toolButtons[i] = button;
        button->init(buttonX, buttonY, 0x1f, 0x16, nullptr);
        button->callback()->setMessage(button, EVENT_CALLBACK);
        button->setEventRoutine(ToolPaletteButtonEvent);
        char pictureName[32];
        std::snprintf(pictureName, sizeof(pictureName), i == 7 ? "mfdcbn%02i.tga" : "mfdcbh%02i.tga", i);
        button->setUpPicture(pictureName);
        std::snprintf(pictureName, sizeof(pictureName), i == 7 ? "mfdcbh%02i.tga" : "mfdcbg%02i.tga", i);
        button->setDownPicture(pictureName);
        std::snprintf(pictureName, sizeof(pictureName), "mfdcbn%02i.tga", i);
        button->setGrayPicture(pictureName);
        button->framed = 0;

        if (i == 7)
        {
            // The original also disabled zoom in multiplayer; the port allows it.
            if (only45Pixel == 0)
            {
                button->action = ACTION_TOGGLE_ZOOM;
                loadHelpText(button->helpText, 0x91);
            }
            else
            {
                // No zoom: only the 45-pixel art is loaded.
                button->setGrayPicture(const_cast<char*>("mfdcbn07a.tga"));
                button->disabled = -1;
                loadHelpText(button->helpText, 0x92);
            }
        }
        else
        {
            button->action = buttonActions[i];
            loadHelpText(button->helpText, 0x8a + static_cast<uint32_t>(i));
        }

        button->unknown50D = 0;
        paletteFrame->addChild(button);
        buttonX += 1 + button->width();

        if (i == 3)
        {
            buttonX = 6;
            buttonY = 0x19;
        }
    }

    videoWindow = new VideoWindow;
    videoWindow->init(6, 0xaa, 0x30, 0x30, nullptr);
    videoWindow->setBackground(const_cast<char*>("mfdmwn01.tga"));
    addChild(videoWindow);

    infoText = new aScrollTextObject;
    Assert(infoText != nullptr, 0, "Not enough memory for text view object");
    infoText->init(5, 0x22, 0x76, 0xb8, nullptr);
    addChild(infoText);
    infoText->ShowGUIWindow(0);
    salvageText = new aScrollTextObject;
    Assert(salvageText != nullptr, 0, "Not enough memory for salvage view object");
    salvageText->init(5, 0x22, 0x76, 0xb8, nullptr);
    addChild(salvageText);
    salvageText->ShowGUIWindow(0);

    numSalvage = 0;
    scrollX = 0;
    scrollY = 0;
    unknown528 = 0;
    unknown52C = 0;
    SetDisplayType(TACMAP_MAP);

    // The map area, as rectangles and as a pane on the MFD's window.
    mapPane.window = displayPort->frame()->window;
    mapRect[0] = 6;
    mapPane.x0 = 6;
    mapRect[1] = 0x22;
    mapPane.y0 = 0x22;
    zoomRect[0] = 0x70;
    mapRect[2] = 0x87;
    mapPane.x1 = 0x87;
    zoomRect[1] = 0xb2;
    mapRect[3] = 0xa3;
    mapPane.y1 = 0xa3;
    zoomRect[2] = 0x89;
    zoomRect[3] = 199;
    unknown4BC[0] = 0x70;
    unknown4BC[1] = 199;
    unknown4BC[2] = 0xb2;
    unknown4BC[3] = 0xdb;

    for (aPort*& port : infoPorts)
    {
        port = new aPort;
    }

    SetDataDisplayMode(0, -1);
    lastRefreshTime = 0;
    RefreshPage();

    typeString[0] = loadHeapString(0x7c);
    typeString[1] = loadHeapString(0x7d);
    typeString[2] = loadHeapString(0x7e);
    typeString[3] = loadHeapString(0x7f);
    typeString[4] = loadHeapString(0x80);
    statusString[0] = loadHeapString(0x78);
    statusString[1] = loadHeapString(0x79);
    statusString[2] = loadHeapString(0x7a);
    statusString[3] = loadHeapString(0x7b);

    std::memset(colorRemap, 0xff, sizeof(colorRemap));
    colorRemap[0xe6] = 0x13;
    colorRemap[0xe8] = 0x13;
    MCRenderer::RegisterData(colorRemap, sizeof(colorRemap), MCDataKind::Tables);

    for (GameObject*& item : salvage)
    {
        item = nullptr;
    }

    return result;
}

auto TacticalMap::freePartShapes() -> void
{
    if (partShapes != nullptr)
    {
        MCRenderer::UnregisterData(partShapes.get());
        partShapes.reset();
    }
}

auto TacticalMap::destroy() -> void
{
    if (mapPort != nullptr)
    {
        mapPort->destroy();
        delete mapPort;
        mapPort = nullptr;
    }

    if (visibilityPort != nullptr)
    {
        // The bitmap's pixels are the visible bits' heap; the original clears the pane's window first.
        MCRenderer::RemoveFrameSurface(visibilityPort->frame()->window);
        visibilityPort->frame()->window = nullptr;
        visibilityPort->destroy();
        delete visibilityPort;
        visibilityPort = nullptr;
    }

    theInterface->tacticalMap = nullptr;

    for (ArtilleryButton*& button : artilleryButtons)
    {
        destroyChild(button);
    }

    for (aButton*& button : scrollButtons)
    {
        destroyChild(button);
    }

    for (ToolPalButton*& button : toolButtons)
    {
        destroyChild(button);
    }

    for (aToolButton*& button : dataButtons)
    {
        destroyChild(button);
    }

    destroyChild(paletteButton);
    destroyChild(paletteBottom);
    destroyChild(paletteFrame);
    destroyChild(salvageText);
    destroyChild(infoText);
    destroyChild(videoWindow);

    for (aPort*& port : infoPorts)
    {
        destroyPort(port);
    }

    destroyPort(mapBackground);
    destroyPort(infoBackground);

    for (aPort*& background : infoViewBackgrounds)
    {
        destroyPort(background);
    }

    if (missionBackground != nullptr)
    {
        missionBackground->destroy();
        delete missionBackground;
        missionBackground = nullptr;

        // In single player the salvage page shares it.
        if (MPlayer == nullptr)
        {
            salvageBackground = nullptr;
        }
    }

    destroyPort(salvageBackground);
    destroyChild(tabTop);
    destroyChild(tabStrip);
    destroyChild(tabBottom);
    destroyChild(scrollUpMarker);
    destroyChild(scrollDownMarker);
    destroyChild(chatWindow);
    destroyChild(chatBlinkerOff);
    destroyChild(chatBlinkerOn);
    freePartShapes();
    mouseInside = 0;
    aObject::destroy();

    for (std::string& text : typeString)
    {
        text.clear();
    }

    for (std::string& text : statusString)
    {
        text.clear();
    }
}

auto TacticalMap::RefreshPage() -> void
{
    if (IsShowing() == 0)
    {
        return;
    }

    if (displayType == TACMAP_INFO)
    {
        aScrollTextObject* text = infoText;
        GameObject* obj = infoObject;
        const int32_t firstPixel = text->firstPixel;

        if (obj == nullptr || (obj->objectClass != BATTLEMECH && obj->objectClass != GROUNDVEHICLE &&
                               obj->objectClass != ELEMENTAL && obj->objectClass != MOVER))
        {
            infoText->ShowGUIWindow(0);
            return;
        }

        if (dataDisplayMode == 2)
        {
            // Payload: the weapon list.
            infoDirty = 0;
            text->ShowGUIWindow(-1);
            drawWeapons();
        }
        else
        {
            // Armor: the part diagram's colours.
            GetColors();
            infoDirty = 0;
            infoText->ShowGUIWindow(0);
        }

        if (obj->objectClass == BATTLEMECH)
        {
            aScrollTextObject* list = infoText;
            list->firstPixel = firstPixel;
            list->PositionScrollTab();
        }
        else if (obj->objectClass == GROUNDVEHICLE)
        {
            if (dataDisplayMode == 1)
            {
                SetDataDisplayMode(0, 0);
            }

            aScrollTextObject* list = infoText;
            list->firstPixel = firstPixel;
            list->PositionScrollTab();
        }

        return;
    }

    if (displayType == TACMAP_MISSION)
    {
        // The home side's objectives, each with its type and its timer or status.
        aScrollTextObject* text = infoText;
        const int32_t firstPixel = text->firstPixel;
        text->Clear();

        if (turn > 1)
        {
            const int32_t count = static_cast<int32_t>(homeTeam->numObjectives);
            int32_t objectiveNum = homeTeam->firstObjective;
            char line[256];

            for (int32_t i = 0; i < count; i++, objectiveNum++)
            {
                ScenarioObjective* objective = &scenario->objectives[objectiveNum];
                uint8_t color = 0;

                if (objective->status == 0)
                {
                    color = 0xf2;
                }
                else if (objective->status == 1)
                {
                    color = 0xb;
                }
                else if (objective->status == 2)
                {
                    color = 0xef;
                }

                std::snprintf(line, sizeof(line), "%d--%s", i + 1, objective->name);
                text->PrintWrapped(line, color, -1);
                const uint32_t type = objective->type + 1 > 3 ? 4 : objective->type + 1;
                std::snprintf(line, sizeof(line), "      %s", typeString[type].c_str());
                text->PrintWrapped(line, color, -1);
                const float timeLeft = scenario->checkObjectiveTimer(objectiveNum);

                if (timeLeft > 0.0)
                {
                    const auto seconds =
                        static_cast<int32_t>(std::floor(std::fmod(static_cast<double>(timeLeft), 60.0)));
                    const auto minutes = static_cast<int32_t>(timeLeft * (1.0 / 60.0));
                    std::snprintf(line, sizeof(line), "%02d:%02d", minutes, seconds);
                }
                else
                {
                    const uint32_t status = objective->status > 2 ? 3 : objective->status;
                    std::snprintf(line, sizeof(line), "      %s", statusString[status].c_str());
                }

                text->PrintWrapped(line, color, -1);
                text->Print(nullptr, 0x1f);
            }
        }

        text->firstPixel = firstPixel;
        text->ResetPortSize();
        text->PositionScrollTab();
    }
}

auto TacticalMap::draw() -> void
{
    if (IsShowing() == 0)
    {
        return;
    }

    // The page's background, drawn with its holes (colour 0xff) open.
    switch (displayType)
    {
        case TACMAP_MAP:
        {
            mapBackground->copyTo(displayPort->frame(), 0, 0, -1);
            drawMapPage(this);
            break;
        }
        case TACMAP_INFO:
        {
            if (infoObject == nullptr)
            {
                VFX_pane_wipe(displayPort->frame(), 0x10);
            }

            infoBackground->copyTo(displayPort->frame(), 0, 0, -1);
            DrawInfoPage();
            break;
        }
        case TACMAP_MISSION:
        {
            missionBackground->copyTo(displayPort->frame(), 0, 0, -1);
            break;
        }
        case TACMAP_SALVAGE:
        {
            salvageBackground->copyTo(displayPort->frame(), 0, 0, -1);
            break;
        }
    }

    // The status line: a button's help, or the default text.
    FillBox(0x15, 0xe2, 0x87, 0xe8, 0x10);
    char buffer[256];
    char* text = statusText;

    if (text == nullptr)
    {
        cLoadString(thisInstance, 0x97, buffer, 0xfe);
        text = buffer;
    }

    blueFont->writeString(port()->frame(), 0x15, 0xe2, reinterpret_cast<uint8_t*>(text), -1);
    aObject::draw();
}

auto TacticalMap::DrawInfoPage() -> void
{
    GameObject* obj = infoObject;

    if (obj == nullptr || (obj->objectClass != BATTLEMECH && obj->objectClass != GROUNDVEHICLE &&
                           obj->objectClass != ELEMENTAL && obj->objectClass != MOVER))
    {
        FillBox(0xe, 0x2a, 0x2c, 0x4c, 0x10);
        return;
    }

    MechWarrior* pilot = obj->getPilot();
    auto* mover = static_cast<Mover*>(obj);
    const bool showPilot = mover->netPlayerId >= 0;

    if (dataDisplayMode == 2)
    {
        // Payload: the weapon list's background (the list is the info text).
        if (infoViewBackgrounds[1] != nullptr)
        {
            VFX_pane_copy(infoViewBackgrounds[1]->frame(), 0, 0, displayPort->frame(), 6, 0x5d, -1);
        }
    }
    else
    {
        // Armor: the part diagram, over the home side's or the enemy's background.
        aPort* background = infoViewBackgrounds[obj->getTeam() == homeTeam ? 0 : 2];

        if (background != nullptr)
        {
            VFX_pane_copy(background->frame(), 0, 0, displayPort->frame(), 6, 0x5d, -1);
        }

        const int32_t shape = obj->objectClass == BATTLEMECH ? mover->numArmorLocations + 1 + mover->numBodyLocations
                                                             : mover->numBodyLocations;
        AG_shape_draw(port()->frame(), partShapes.get(), shape, 0x22, 0x65);
        DrawParts();
    }

    DrawBar();
    char line[64];

    if (obj->objectClass == BATTLEMECH)
    {
        if (showPilot)
        {
            drawPilot(pilot);
        }
        else
        {
            FillBox(6, 0x2a, 0x88, 0x4c, 0x10);
        }

        // "<name> <weight class> <tons>".
        const int32_t tonnage = static_cast<int32_t>(obj->getTonnage());
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
        cLoadString(thisInstance, classId, buffer, 0xfe);
        char weightClass[12];
        std::strncpy(weightClass, buffer, 9);
        weightClass[9] = 0;
        cLoadString(thisInstance, 0x81, buffer, 0xfe);
        std::snprintf(line, sizeof(line), buffer, mover->getIfaceName(), weightClass, tonnage);
        const int32_t lineWidth = blueFont->width(reinterpret_cast<uint8_t*>(line));
        blueFont->writeString(port()->frame(), 0x47 - lineWidth / 2, 0x54, reinterpret_cast<uint8_t*>(line), -1);
    }
    else if (obj->objectClass == GROUNDVEHICLE)
    {
        FillBox(6, 0x2a, 0x88, 0x4c, 0x10);
        std::snprintf(line, sizeof(line), "%s", mover->getIfaceName());
        // Measured in greenFont, written in blueFont (as the original).
        const int32_t lineWidth = greenFont->width(reinterpret_cast<uint8_t*>(line));
        blueFont->writeString(port()->frame(), 0x47 - lineWidth / 2, 0x54, reinterpret_cast<uint8_t*>(line), -1);

        // The passengers, with their pictures.
        auto* vehicle = static_cast<GroundVehicle*>(infoObject);
        int32_t shown = 0;
        int32_t xPos = 10;

        for (int32_t seat = 0; seat < vehicle->seats; seat++)
        {
            MechWarrior* passenger = vehicle->passengers[seat];

            if (passenger == nullptr)
            {
                continue;
            }

            VFX_pane_copy(infoPorts[shown]->frame(), 0, 0, displayPort->frame(), xPos, 0xc4, 0xfff);
            const int32_t yPos = 0xa6 - (greenFont->height() + 2) * shown;
            greenFont->writeString(port()->frame(), xPos, yPos, reinterpret_cast<uint8_t*>(passenger->name), -1);
            shown++;
            xPos += 0x23;
        }
    }
}

auto TacticalMap::handleEvent(aEvent* event) -> void
{
    const int32_t screenX = event->x;
    const int32_t screenY = event->y;
    POINT local;
    local.x = screenX - globalX();
    local.y = screenY - globalY();

    // The zoom buttons' areas take the event whole.
    if (displayType == TACMAP_MAP)
    {
        if (PtInRect(reinterpret_cast<const RECT*>(zoomRect), local) != 0)
        {
            scrollButtons[4]->handleEvent(event);
            return;
        }

        if (displayType == TACMAP_MAP && PtInRect(reinterpret_cast<const RECT*>(unknown4BC), local) != 0)
        {
            scrollButtons[5]->handleEvent(event);
            return;
        }
    }

    const RECT* upArea = reinterpret_cast<const RECT*>(pageRects[0]);
    const RECT* downArea = reinterpret_cast<const RECT*>(pageRects[1]);
    const RECT* trackArea = reinterpret_cast<const RECT*>(pageRects[2]);

    switch (event->type)
    {
        case EVENT_LEFT_DOWN:
        {
            if (displayType == TACMAP_MAP && PtInRect(reinterpret_cast<const RECT*>(mapRect), local) != 0)
            {
                mapDragging = -1;
            }

            if (local.y > 0xe0)
            {
                // Below the pages: the palette toggle.
                TogglePalette();
                break;
            }

            if (displayType > TACMAP_MAP && displayType <= TACMAP_SALVAGE)
            {
                // The info/mission pages scroll infoText, the salvage page salvageText; press and hold repeats.
                aScrollTextObject* text = displayType == TACMAP_SALVAGE ? salvageText : infoText;

                if (PtInRect(upArea, local) != 0)
                {
                    application->grab(this);
                    scrollUpMarker->ShowGUIWindow(-1);
                    application->AddTimer(this, SCROLL_START_TIMER, theInterface->scrollStart, 0, 0, 0);
                    text->ReceiveClick(-1, 0);
                }
                else if (PtInRect(downArea, local) != 0)
                {
                    application->grab(this);
                    scrollDownMarker->ShowGUIWindow(-1);
                    application->AddTimer(this, SCROLL_START_TIMER, theInterface->scrollStart, 0, 0, 0);
                    text->ReceiveClick(1, 0);
                }
                else if (PtInRect(trackArea, local) != 0)
                {
                    // The info pages measure the click from infoText's screen top, the salvage page from the track.
                    const int32_t yPos =
                        displayType == TACMAP_SALVAGE ? local.y - pageRects[2][1] : local.y - text->globalY();
                    text->ReceiveClick(0, yPos);
                }
            }
            break;
        }

        case EVENT_LEFT_UP:
        {
            if (displayType == TACMAP_MAP && mapDragging != 0)
            {
                // A click on the map moves the active camera there.
                mapDragging = 0;

                if (PtInRect(reinterpret_cast<const RECT*>(mapRect), local) != 0)
                {
                    vector_3d target(static_cast<float>(local.x), static_cast<float>(local.y), 0.0f);
                    tacMapToWorld(target, -1);
                    mainHolder->GetActivePane()->GetCamera()->changeTarget(nullptr, 0);
                    mainHolder->GetActivePane()->GetCamera()->setPosition(target);
                }
            }

            application->RemoveTimer(this, SCROLL_START_TIMER);
            application->RemoveTimer(this, SCROLL_REPEAT_TIMER);

            if (application->grabbedObject() == this)
            {
                // Released on the top-right corner: toggles the MFD.
                const int32_t xPos = screenX - globalX();
                const int32_t yPos = screenY - globalY();

                if (xPos > 0x8c && yPos < 0x1c)
                {
                    HideMe(Terrain::terrainTacticalMap->IsHidden() == 0);
                }
            }

            application->release();
            scrollUpMarker->ShowGUIWindow(0);
            scrollDownMarker->ShowGUIWindow(0);
            break;
        }

        case EVENT_TIMER:
        {
            if (event->data == 1)
            {
                // The chat tab blinks while a message is unread.
                if (chatPending == 0)
                {
                    if (displayType == TACMAP_SALVAGE)
                    {
                        if (chatBlinkerOn != nullptr)
                        {
                            chatBlinkerOn->ShowGUIWindow(-1);
                        }
                    }
                    else if (chatBlinkerOff != nullptr)
                    {
                        chatBlinkerOff->ShowGUIWindow(-1);
                    }

                    chatPending = -1;
                }
                else
                {
                    if (chatBlinkerOff != nullptr)
                    {
                        chatBlinkerOff->ShowGUIWindow(0);
                    }

                    if (chatBlinkerOn != nullptr)
                    {
                        chatBlinkerOn->ShowGUIWindow(0);
                    }

                    chatPending = 0;
                }
                break;
            }

            if (event->data == SCROLL_START_TIMER)
            {
                application->RemoveTimer(this, SCROLL_START_TIMER);
                application->AddTimer(this, SCROLL_REPEAT_TIMER, theInterface->scrollStart / 5, 0, 0, 0);
            }
            else if (event->data != SCROLL_REPEAT_TIMER)
            {
                break;
            }

            if (displayType > TACMAP_MAP && displayType <= TACMAP_SALVAGE)
            {
                aScrollTextObject* text = displayType == TACMAP_SALVAGE ? salvageText : infoText;

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
            TMCZoomIn();
            break;

        case EVENT_ZOOM_OUT:
            TMCZoomOut();
            break;
    }

    aObject::handleEvent(event);
}

auto TacticalMap::MouseWheel(int32_t steps, int32_t xPos, int32_t yPos) -> bool
{
    if (displayType > TACMAP_MAP && displayType <= TACMAP_SALVAGE)
    {
        aScrollTextObject* text = displayType == TACMAP_SALVAGE ? salvageText : infoText;
        return text->MouseWheel(steps, xPos, yPos);
    }

    if (displayType == TACMAP_MAP)
    {
        // As clicking the zoom buttons (up in, down out), which are disabled at the ends.
        for (; steps != 0; steps += steps < 0 ? 1 : -1)
        {
            aButton* button = scrollButtons[steps < 0 ? 4 : 5];

            if (button->disabled != 0)
            {
                break;
            }

            if (steps < 0)
            {
                TMCZoomIn();
            }
            else
            {
                TMCZoomOut();
            }
        }

        return true;
    }

    return false;
}

auto TacticalMap::display() -> void
{
    if (IsShowing() == 0)
    {
        return;
    }

    // Hidden and at rest the MFD lies off the screen (but for its tabs); the original showed its picture as it was,
    // without updating it.
    const bool atRest = IsHidden() != 0 && hideOffset == 0;

    if (!atRest && hideOffset != 0)
    {
        // Sliding: step, then stop once off screen (hiding) or back home (showing).
        moveTo(x() + hideOffset, y(), -1);

        if (hidden != 0)
        {
            const tagRECT screen = {2, 0, application->width(), application->height()};

            if (rectIntersect(screen) == 0)
            {
                hideOffset = 0;
            }
        }
        else
        {
            const bool home = hideOffset < 0 ? (homeX >= globalX() && homeY >= globalY())
                                             : (homeX <= globalX() && homeY <= globalY());

            if (home)
            {
                moveTo(homeX - parent->globalX(), homeY - parent->globalY(), -1);
                hideOffset = 0;
            }
        }
    }

    const bool updating = !atRest && (IsHidden() == 0 || hideOffset != 0);

    if (updating)
    {
        switch (displayType)
        {
            case TACMAP_MAP:
                UpdateMapPage();
                break;
            case TACMAP_INFO:
            case TACMAP_MISSION:
            {
                if (MCPort::Milliseconds() > lastRefreshTime + 500)
                {
                    lastRefreshTime = MCPort::Milliseconds();
                    RefreshPage();
                }
                break;
            }
            case TACMAP_SALVAGE:
            {
                if (MPlayer == nullptr && MCPort::Milliseconds() > lastRefreshTime + 500)
                {
                    lastRefreshTime = MCPort::Milliseconds();
                    UpdateSalvage();
                }
                break;
            }
        }

        // The status line is drawn each frame now.
        statusDirty = 0;
    }

    if (mouseInside != 0)
    {
        application->SetCurrentCursor(static_cast<CursorType>(0));
    }

    // Port: the MFD draws itself, then its children (the original copied its picture, then displayed them).
    DrawInFramePass(displayPort);

    // The original revealed the objectives' areas as it drew the map's units, after the fog of war.
    if (updating && displayType == TACMAP_MAP)
    {
        RevealObjectives();
    }
}

auto TacticalMap::UpdateMapPage() -> void
{
    // The mission timer, rewritten once a second.
    if (scenario->timeLimit >= 0 && lastMapTime + 1.0f < actualTime)
    {
        lastMapTime = actualTime;
        const float remaining = static_cast<float>(scenario->timeLimit) - actualTime;

        if (remaining >= 0.0f)
        {
            const auto seconds = static_cast<int32_t>(std::fmod(static_cast<double>(remaining), 60.0));
            const auto minutes = static_cast<int32_t>(remaining * (1.0f / 60.0f));
            std::snprintf(mapTimeText, sizeof(mapTimeText), "%02i:%02i", minutes, seconds);
            mapTimeRed = false;
        }
        else
        {
            std::snprintf(mapTimeText, sizeof(mapTimeText), "00:00");
            mapTimeRed = true;
        }

        mapTimeShown = true;
    }

    // Markers blink five times a second.
    tacFrameLength = frameLength + tacFrameLength;

    if (tacFrameLength > 0.2)
    {
        tacFrameLength = 0.0f;
        onNow = ~onNow;
    }
}

auto TacticalMap::RevealObjectives() -> void
{
    if (objectivesRevealed != 0)
    {
        return;
    }

    // Once a pending objective with a position is found, every objective's area is revealed in the fog of war.
    const int32_t numObjectives = static_cast<int32_t>(homeTeam->numObjectives);

    for (int32_t i = 0; i < numObjectives; i++)
    {
        ScenarioObjective* objective = &scenario->objectives[homeTeam->firstObjective + i];

        if (objective->position[0] == -99.0f || objective->position[1] == -99.0f || objective->position[2] == -99.0f ||
            objective->status != 0)
        {
            continue;
        }

        for (int32_t j = 0; j < numObjectives; j++)
        {
            ScenarioObjective* area = &scenario->objectives[homeTeam->firstObjective + j];

            if (area->radius <= 0.0)
            {
                continue;
            }

            const float column = static_cast<float>(std::floor(static_cast<double>(
                Terrain::OneOvermetersPerVertex * (area->position[0] - Terrain::mapTopLeft3d100.x))));
            const float row = static_cast<float>(std::floor(static_cast<double>(
                Terrain::OneOvermetersPerVertex * (Terrain::mapTopLeft3d100.y - area->position[1]))));
            const auto xc = static_cast<int32_t>(std::floor(static_cast<double>(column)));
            const auto yc = static_cast<int32_t>(std::floor(static_cast<double>(row)));
            const auto radius = static_cast<int32_t>(area->radius / metersPerPixel * worldUnitsPerMeter);
            VFX_ellipse_fill(visibilityPort->frame(), xc, yc, radius, radius, 0x14);
        }

        objectivesRevealed = -1;
        return;
    }
}

namespace
{
    void drawMapPage(TacticalMap* map)
    {
        // The mission timer (or the "no time limit" text), as UpdateMapPage last wrote it.
        char buffer[256];
        _pane* page = map->port()->frame();

        if (scenario->timeLimit < 0)
        {
            cLoadString(thisInstance, 0xbc, buffer, 0xfe);
            whiteFont->writeString(page, 0x3c, 0xab, reinterpret_cast<uint8_t*>(buffer), -1);
        }
        else if (map->mapTimeShown)
        {
            map->FillBox(0x37, 0xaa, 0x88, 0xb2, 0x12);
            aFont* font = map->mapTimeRed ? redFont : whiteFont;
            font->writeString(page, 0x3c, 0xab, reinterpret_cast<uint8_t*>(map->mapTimeText), -1);
        }

        // The map picture, scrolled and zoomed, mapped onto the map area.
        const int32_t halfWidth = map->mapWidth >> 1;
        const int32_t halfHeight = map->mapHeight >> 1;
        const int32_t zoomedHalfWidth = halfWidth / map->zoom;
        const int32_t zoomedHalfHeight = halfHeight / map->zoom;
        const int32_t left = ((map->scrollX - zoomedHalfWidth) + halfWidth) * 0x10000;
        const int32_t right = ((map->scrollX - halfWidth) + zoomedHalfWidth + map->mapWidth) * 0x10000;
        const int32_t top = ((map->scrollY - zoomedHalfHeight) + halfHeight) * 0x10000;
        const int32_t bottom = ((map->scrollY - halfHeight) + zoomedHalfHeight + map->mapHeight) * 0x10000;
        SCRNVERTEX vertices[4] = {};
        vertices[0] = {6, 0x22, 0, left, top, 0};
        vertices[1] = {0x88, 0x22, 0, right, top, 0};
        vertices[2] = {0x88, 0xa4, 0, right, bottom, 0};
        vertices[3] = {6, 0xa4, 0, left, bottom, 0};
        VFX_map_polygon(map->displayPort->frame(), 4, vertices, map->mapPort->frame()->window, MP_XP);

        if (drawRevealedTacMap == 0)
        {
            // The fog of war: the visible bits, one texel per vertex, over the same area.
            vector_3d corners[4] = {vector_3d(6.0f, 34.0f, 0.0f), vector_3d(136.0f, 34.0f, 0.0f),
                                    vector_3d(136.0f, 164.0f, 0.0f), vector_3d(6.0f, 164.0f, 0.0f)};

            for (vector_3d& corner : corners)
            {
                map->tacMapToWorld(corner, -1);
            }

            int32_t u[4];
            int32_t v[4];

            for (int32_t i = 0; i < 4; i++)
            {
                const float column = (corners[i].x - Terrain::mapTopLeft3d100.x) * Terrain::OneOvermetersPerVertex;
                const float row = (Terrain::mapTopLeft3d100.y - corners[i].y) * Terrain::OneOvermetersPerVertex;
                u[i] = static_cast<int32_t>(column * 65536.0 + 0.5);
                v[i] = static_cast<int32_t>(row * 65536.0 + 0.5);
            }

            vertices[0] = {6, 0x22, 0, u[0] + 0x20000, v[0] + 0x10000, 0};
            vertices[1] = {0x88, 0x22, 0, u[1] - 0x10000, v[1] + 0x10000, 0};
            vertices[2] = {0x88, 0xa4, 0, u[2] - 0x10000, v[2] - 0x10000, 0};
            vertices[3] = {6, 0xa4, 0, u[3] + 0x10000, v[3] - 0x20000, 0};

            if (drawRevealedTacMap == 0)
            {
                VFX_map_polygon(map->displayPort->frame(), 4, vertices, map->visibilityPort->frame()->window, MP_XP);
            }
        }

        // What each camera window sees, as a rectangle in its colour.
        for (int32_t windowNum = 0; windowNum < 4; windowNum++)
        {
            TerrainWindow* window = land->getTerrainWindow(windowNum);

            if (window == nullptr || window->camera == nullptr || window->camera->active == 0)
            {
                continue;
            }

            Camera* camera = window->camera;
            viewWindow* view = camera->window;
            vector_2d screenTopLeft(0.0f, 0.0f);
            // Port: the corners of the world surface the view shows (its size follows the zoom).
            vector_2d screenBottomRight(static_cast<float>(view->WorldWidth()),
                                        static_cast<float>(view->WorldHeight()));
            vector_3d topLeft;
            vector_3d bottomRight;
            camera->inverseProject(screenTopLeft, topLeft);
            camera->inverseProject(screenBottomRight, bottomRight);
            map->worldToTacMap(topLeft, -1);
            map->worldToTacMap(bottomRight, -1);
            uint8_t color = 0x1f;

            switch (camera->cameraId)
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

            _pane* pane = &map->mapPane;
            const auto paneLeft = static_cast<float>(pane->x0);
            const auto paneTop = static_cast<float>(pane->y0);
            const auto x0 = static_cast<int32_t>(topLeft.x - paneLeft);
            const auto y0 = static_cast<int32_t>(topLeft.y - paneTop);
            const auto x1 = static_cast<int32_t>(bottomRight.x - paneLeft);
            const auto y1 = static_cast<int32_t>(bottomRight.y - paneTop);
            VFX_line_draw(pane, x0, y1, x1, y1, LD_DRAW, color);
            VFX_line_draw(pane, x1, y0, x1, y1, LD_DRAW, color);
            VFX_line_draw(pane, x1, y0, x0, y0, LD_DRAW, color);
            VFX_line_draw(pane, x0, y0, x0, y1, LD_DRAW, color);
        }

        map->drawObjects();
    }
}

auto TacticalMap::HideMe(int hide) -> void
{
    if (hideOffset != 0)
    {
        return;
    }

    if (hide == 0)
    {
        // Shown: the chat tab stops blinking.
        application->RemoveTimer(this, 1);

        if (chatBlinkerOn != nullptr)
        {
            chatBlinkerOn->ShowGUIWindow(0);
        }

        if (chatBlinkerOff != nullptr)
        {
            chatBlinkerOff->ShowGUIWindow(0);
        }
    }
    else
    {
        // Port fix: stopVideo checks the movie window, which the original ends without checking.
        stopVideo();
    }

    if (hidden == hide)
    {
        return;
    }

    if (turn > 1)
    {
        soundSystem->playDigitalSample(0x3b, 1, nullptr, 0, 0);
    }

    if (hide != 0)
    {
        // Slide off the left edge, from here.
        homeX = globalX();
        homeY = globalY();
        hideOffset = (2 - globalX()) - width();
        hidden = hide;
        return;
    }

    // Slide back home.
    if (homeX != globalX())
    {
        hidden = 0;
        hideOffset = homeX - globalX();
        return;
    }

    hidden = 0;
    hideOffset = homeY - globalY();
}

auto TacticalMap::worldToTacMap(vector_3d& pos, int scrolled) -> void
{
    // Rotate 45 degrees (the map is drawn diamond-wise), then scale to pixels about the map's centre.
    const float worldX = pos.x;
    const float worldY = pos.y;
    pos.x = worldX * MAP_ROTATION + worldY * MAP_ROTATION;
    const float rotatedY = worldY * MAP_ROTATION - worldX * MAP_ROTATION;
    pos.y = rotatedY;

    if (scrolled != 0)
    {
        pos.x = pos.x / metersPerPixel;
        pos.y = rotatedY / metersPerPixel;
        pos.z = pos.z / metersPerPixel;
        const auto zoomF = static_cast<float>(zoom);
        pos.x = (pos.x + MAP_CENTER_X) -
                (MAP_PICTURE_SIDE / static_cast<float>(mapWidth)) * zoomF * static_cast<float>(scrollX);
        pos.y = ((MAP_HALF_SIDE - pos.y) + MAP_TOP) -
                (MAP_PICTURE_SIDE / static_cast<float>(mapHeight)) * zoomF * static_cast<float>(scrollY);
        return;
    }

    const float scale = static_cast<float>(zoom) * metersPerPixel;
    const float pixelX = pos.x / scale;
    const float pixelY = rotatedY / scale;
    pos.z = pos.z / scale;
    pos.x = pixelX + MAP_HALF_SIDE;
    pos.y = MAP_HALF_SIDE - pixelY;
}

auto TacticalMap::tacMapToWorld(vector_3d& pos, int scrolled) -> void
{
    pos.z = 0.0f;

    if (scrolled != 0)
    {
        const auto zoomF = static_cast<float>(zoom);
        pos.x = ((MAP_PICTURE_SIDE / static_cast<float>(mapWidth)) * zoomF * static_cast<float>(scrollX) + pos.x) -
                MAP_LEFT;
        pos.y = ((MAP_PICTURE_SIDE / static_cast<float>(mapHeight)) * zoomF * static_cast<float>(scrollY) + pos.y) -
                MAP_TOP;
    }

    const float pixelX = pos.x - MAP_HALF_SIDE;
    const float pixelY = MAP_HALF_SIDE - pos.y;

    if (scrolled != 0)
    {
        pos.x = pixelX * metersPerPixel;
        pos.y = pixelY * metersPerPixel;
    }
    else
    {
        const float scale = static_cast<float>(zoom) * metersPerPixel;
        pos.x = pixelX * scale;
        pos.y = pixelY * scale;
    }

    // Rotate back, and stand the point on the ground.
    const float rotatedX = pos.x;
    const float rotatedY = pos.y;
    pos.x = rotatedX * MAP_ROTATION + rotatedY * -MAP_ROTATION;
    pos.y = rotatedY * MAP_ROTATION - rotatedX * -MAP_ROTATION;
    pos.z = land->getTerrainElevation(pos);
}

auto TacticalMap::drawObjects() -> void
{
    // (The markers' blink is stepped by UpdateMapPage, and the objectives' areas revealed by RevealObjectives: the
    // original did both here.)

    // The home side's pending objectives that have a position: a numbered dot.
    const int32_t numObjectives = static_cast<int32_t>(homeTeam->numObjectives);

    if (numObjectives != 0)
    {
        for (int32_t i = 0; i < numObjectives; i++)
        {
            ScenarioObjective* objective = &scenario->objectives[homeTeam->firstObjective + i];

            if (objective->position[0] == -99.0f || objective->position[1] == -99.0f ||
                objective->position[2] == -99.0f || objective->status != 0)
            {
                continue;
            }

            if (onNow != 0)
            {
                vector_3d pos(objective->position[0], objective->position[1], 0.0f);
                worldToTacMap(pos, -1);
                const int32_t xPos = static_cast<int32_t>(std::floor(static_cast<double>(pos.x))) - mapPane.x0;
                const int32_t yPos = static_cast<int32_t>(std::floor(static_cast<double>(pos.y))) - mapPane.y0;
                AG_ellipse_fill(&mapPane, xPos, yPos, 2, 2, 0x1f);
                const uint32_t status = scenario->objectives[homeTeam->firstObjective + i].status;
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
                lineFont->print(xPos, yPos, number, color, &mapPane);
            }
        }
    }

    // The sensor contacts: a dot (dark when not identified), and with the ranges on, the unit's sensor range.
    const int32_t homeAlignment = homeTeam->alignment;
    int32_t numContacts = homeTeam->getSensorContacts(contactList);

    for (int32_t i = 0; i < numContacts; i++)
    {
        GameObject* obj = contactList[i];
        int tagged = 0;
        obj->getContactType(homeTeam->id, tagged);

        if (obj->getAwake() == 0 || obj->isDisabled() != 0 || obj->inTransport() != 0)
        {
            continue;
        }

        const bool mover = isMoverClass(obj);

        if (mover && obj->getPilot()->status == 2)
        {
            continue;
        }

        const vector_3d position = obj->getPosition();
        vector_3d pos(position.x, obj->getPosition().y, 0.0f);
        worldToTacMap(pos, -1);
        const int32_t xPos = static_cast<int32_t>(std::floor(static_cast<double>(pos.x))) - mapPane.x0;
        const int32_t yPos = static_cast<int32_t>(std::floor(static_cast<double>(pos.y))) - mapPane.y0;
        AG_ellipse_fill(&mapPane, xPos, yPos, 2, 2, tagged == 0 ? 10 : 0xcf);

        if (showRanges != 0 && mover)
        {
            drawSensorRange(this, obj, xPos, yPos, homeAlignment);
        }
    }

    // The contacts in line of sight (mechs and vehicles).
    numContacts = homeTeam->getLOSContacts(contactList);

    for (int32_t i = 0; i < numContacts; i++)
    {
        GameObject* obj = contactList[i];

        if (obj->objectClass <= 1 || obj->objectClass >= 4 || obj->getAwake() == 0 || obj->isDisabled() != 0 ||
            obj->inTransport() != 0 || obj->getPilot()->status == 2)
        {
            continue;
        }

        const vector_3d position = obj->getPosition();
        vector_3d pos(position.x, position.y, 0.0f);
        worldToTacMap(pos, -1);
        const int32_t xPos = static_cast<int32_t>(std::floor(static_cast<double>(pos.x))) - mapPane.x0;
        const int32_t yPos = static_cast<int32_t>(std::floor(static_cast<double>(pos.y))) - mapPane.y0;
        AG_ellipse_fill(&mapPane, xPos, yPos, 2, 2, 0xcf);

        if (showRanges != 0)
        {
            drawSensorRange(this, obj, xPos, yPos, homeAlignment);
        }
    }

    if (showRanges != 0)
    {
        // The home side's other sensors (not artillery's), then the enemy's revealed sensor buildings.
        for (int32_t i = 0; i < homeTeam->numSensors; i++)
        {
            SensorSystem* sensor = homeTeam->sensors[i];

            if (sensor->enabled() == 0 || sensor->owner->objectClass == ARTILLERY)
            {
                continue;
            }

            const float range = (sensor->getSkilledRange() / metersPerPixel) * worldUnitsPerMeter;

            if (range <= 0.0)
            {
                continue;
            }

            const vector_3d position = sensor->owner->getPosition();
            vector_3d pos(position.x, position.y, 0.0f);
            const uint8_t color = sensor->multiplier < 1.0 ? 0xec : 0x1f;
            worldToTacMap(pos, -1);
            const int32_t xPos = static_cast<int32_t>(std::floor(static_cast<double>(pos.x))) - mapPane.x0;
            const int32_t yPos = static_cast<int32_t>(std::floor(static_cast<double>(pos.y))) - mapPane.y0;
            const auto radius = static_cast<int32_t>(range);
            AG_ellipse_draw(&mapPane, xPos, yPos, radius, radius, color);
        }

        Team* enemy = homeTeam == innerSphereTeam ? clanTeam : innerSphereTeam;

        for (int32_t i = 0; i < enemy->numSensors; i++)
        {
            SensorSystem* sensor = enemy->sensors[i];

            if (sensor->owner->isBuilding() == 0 || sensor->enabled() == 0 || sensor->owner->isRevealed() == 0)
            {
                continue;
            }

            const float range = (sensor->getSkilledRange() / metersPerPixel) * worldUnitsPerMeter;

            if (range <= 0.0)
            {
                continue;
            }

            const vector_3d position = sensor->owner->getPosition();
            vector_3d pos(position.x, position.y, 0.0f);
            worldToTacMap(pos, -1);
            const int32_t xPos = static_cast<int32_t>(std::floor(static_cast<double>(pos.x))) - mapPane.x0;
            const int32_t yPos = static_cast<int32_t>(std::floor(static_cast<double>(pos.y))) - mapPane.y0;
            const uint8_t color = sensor->multiplier < 1.0 ? 0xf2 : 0xef;
            const auto radius = static_cast<int32_t>(range);
            AG_ellipse_draw(&mapPane, xPos, yPos, radius, radius, color);
        }
    }

    // The home side's mechs; the selected ones last, on top.
    ObjectQueueNode* mechList = homeTeam == innerSphereTeam ? innerSphereMechList : clanMechList;
    std::vector<std::pair<int32_t, int32_t>> selected;

    for (BaseObject* node = mechList->head; node != nullptr; node = node->next)
    {
        auto* obj = static_cast<GameObject*>(node);

        if (obj->getAwake() == 0 || obj->isDisabled() != 0)
        {
            continue;
        }

        const vector_3d position = obj->getPosition();
        vector_3d pos(position.x, obj->getPosition().y, 0.0f);
        worldToTacMap(pos, -1);
        const int32_t xPos = static_cast<int32_t>(std::floor(static_cast<double>(pos.x))) - mapPane.x0;
        const int32_t yPos = static_cast<int32_t>(std::floor(static_cast<double>(pos.y))) - mapPane.y0;

        if (theInterface->IsSelected(obj->partId) == 0)
        {
            AG_ellipse_fill(&mapPane, xPos, yPos, 2, 2, 0xf);
        }
        else
        {
            selected.emplace_back(xPos, yPos); // Port: a vector (the original's stack arrays have no bound).
        }
    }

    for (const auto& [xPos, yPos] : selected)
    {
        AG_ellipse_fill(&mapPane, xPos, yPos, 2, 2, 0xb);
    }

    // Artillery strikes: the home side's, and the enemy's in their last 4 seconds, blinking.
    ObjectQueueNode* defaultList = objectList->findList(DEFAULT_LIST_ID);

    if (defaultList == nullptr)
    {
        return;
    }

    for (BaseObject* node = defaultList->head; node != nullptr; node = node->next)
    {
        if (node->objectClass != ARTILLERY)
        {
            continue;
        }

        auto* strike = static_cast<Artillery*>(node);
        const bool ours = strike->getAlignment() == homeTeam->alignment;

        if (!ours && strike->timeToImpact >= 4.0)
        {
            continue;
        }

        const vector_3d position = strike->getPosition();
        vector_3d pos(position.x, position.y, 0.0f);
        worldToTacMap(pos, -1);
        const int32_t xPos = static_cast<int32_t>(std::floor(static_cast<double>(pos.x))) - mapPane.x0;
        const int32_t yPos = static_cast<int32_t>(std::floor(static_cast<double>(pos.y))) - mapPane.y0;
        auto* type = static_cast<ArtilleryType*>(strike->getObjectType());
        const auto diameter = static_cast<int32_t>(static_cast<int32_t>(type->nominalMinorRange) * 2);
        int32_t dotSize = static_cast<int32_t>(static_cast<float>(diameter) / metersPerPixel);
        int32_t ring = 0;

        if (dotSize == 0)
        {
            // Too small to see: a dot, and while the strike is live, its sensor range.
            dotSize = 2;

            if (strike->timeToImpact < 0.0)
            {
                int32_t range = 0;

                if (strike->sensorSystem != nullptr)
                {
                    range = static_cast<int32_t>(strike->sensorSystem->getSkilledRange());
                }

                range = static_cast<int32_t>(static_cast<float>(range) / metersPerPixel);
                ring = static_cast<int32_t>(static_cast<float>(range) * worldUnitsPerMeter);
            }
        }

        if (onNow != 0)
        {
            continue;
        }

        uint8_t fill;

        if (strike->getAlignment() == homeTeam->alignment)
        {
            if (ring > 0)
            {
                AG_ellipse_draw(&mapPane, xPos, yPos, ring, ring, strike->sensorSystem->multiplier < 1.0 ? 0xec : 0x1f);
            }

            fill = 0xf;
        }
        else
        {
            if (ring > 0)
            {
                AG_ellipse_draw(&mapPane, xPos, yPos, ring, ring, strike->sensorSystem->multiplier < 1.0 ? 0xf2 : 0xef);
            }

            fill = 0xcf;
        }

        AG_ellipse_fill(&mapPane, xPos, yPos, dotSize, dotSize, fill);
    }
}

auto TacticalMap::SetDisplayType(TacmapDisplayTypes type) -> void
{
    statusDirty = -1;
    infoDirty = -1;
    displayType = type;

    // Hide every page's parts, then show the new page's.
    salvageText->ShowGUIWindow(0);

    if (MPlayer != nullptr)
    {
        chatWindow->ShowGUIWindow(0);

        if (application->textObject() == chatWindow->chatInput)
        {
            application->releaseText();
        }
    }

    infoText->ShowGUIWindow(0);
    infoText->Clear();

    for (aButton* button : scrollButtons)
    {
        button->ShowGUIWindow(0);
    }

    videoWindow->ShowGUIWindow(0);
    stopVideo();

    for (aToolButton* button : dataButtons)
    {
        button->ShowGUIWindow(0);
    }

    switch (type)
    {
        case TACMAP_MAP:
        {
            if (tabHighlighted != 0)
            {
                tabBottom->setBackground(const_cast<char*>("mfdmts02.tga"));
            }

            tabStrip->setBackground(const_cast<char*>(MPlayer == nullptr ? "mfdmts01.tga" : "mfdmts03.tga"));
            tabHighlighted = 0;
            // (The original copied the page's background into the MFD's picture here; draw shows it each frame.)

            for (aButton* button : scrollButtons)
            {
                button->ShowGUIWindow(-1);
            }

            videoWindow->ShowGUIWindow(-1);
            break;
        }

        case TACMAP_INFO:
        {
            for (aToolButton* button : dataButtons)
            {
                button->ShowGUIWindow(-1);
            }

            if (tabHighlighted != 0)
            {
                tabBottom->setBackground(const_cast<char*>("mfddts02.tga"));
            }

            tabStrip->setBackground(const_cast<char*>(MPlayer == nullptr ? "mfddts01.tga" : "mfddts03.tga"));
            tabHighlighted = 0;

            // (The original wiped the picture without a unit, and copied the page's background; draw does both.)
            if (infoObject != nullptr)
            {
                SetID(infoObject->partId);
            }

            infoText->moveTo(7, 0x5e, 0);
            infoText->resize(0x74, 0x69);
            infoText->ShowGUIWindow(-1);
            setPageRects(this, 0x5d, 0x68, 0xbe, 0xc9);
            infoText->firstPixel = 0;
            RefreshPage();
            return;
        }

        case TACMAP_MISSION:
        {
            if (tabHighlighted != 0)
            {
                tabBottom->setBackground(const_cast<char*>("mfdbts02.tga"));
            }

            tabStrip->setBackground(const_cast<char*>(MPlayer == nullptr ? "mfdbts01.tga" : "mfdbts03.tga"));
            tabHighlighted = 0;
            infoText->moveTo(5, 0x22, 0);
            infoText->resize(0x76, 0xb8);
            infoText->ShowGUIWindow(-1);
            setPageRects(this, 0x22, 0x2d, 0xcf, 0xda);
            pageRects[2][3] = 0xce;
            RefreshPage();
            return;
        }

        case TACMAP_SALVAGE:
        {
            tabBottom->setBackground(const_cast<char*>("mfdsts02.tga"));
            tabStrip->setBackground(const_cast<char*>(MPlayer == nullptr ? "mfdsts01.tga" : "mfdsts03.tga"));
            tabHighlighted = -1;

            if (MPlayer == nullptr)
            {
                salvageText->ShowGUIWindow(-1);
                refreshSalvageList();
                setPageRects(this, 0x22, 0x2d, 0xcf, 0xda);
                pageRects[2][3] = 0xce;
                RefreshPage();
                return;
            }

            // Multiplayer: the chat window, and the tab stops blinking.
            application->RemoveTimer(this, 1);

            if (chatBlinkerOn != nullptr)
            {
                chatBlinkerOn->ShowGUIWindow(0);
            }

            if (chatBlinkerOff != nullptr)
            {
                chatBlinkerOff->ShowGUIWindow(0);
            }

            chatWindow->ShowGUIWindow(-1);
            break;
        }
    }

    RefreshPage();
}

auto TacticalMap::centerOnObject(GameObject* obj) -> void
{
    ObjectPosition* position = obj->getObjPosition();
    scrollX = position->tileC - mapVertexSide;
    scrollY = position->tileR - mapVertexSide;
}

auto TacticalMap::scrollMap(int32_t dx, int32_t dy) -> void
{
    // Each axis only moves if the view stays on the picture.
    const int32_t oldX = scrollX;
    const int32_t oldY = scrollY;
    scrollX = oldX + dx;
    scrollY = dy + oldY;

    if (!scrollInPicture(scrollX, mapWidth, zoom))
    {
        scrollX = oldX;
    }

    if (!scrollInPicture(scrollY, mapHeight, zoom))
    {
        scrollY = oldY;
    }
}

auto TacticalMap::setScrollMapPosition(int32_t x, int32_t y) -> void
{
    // Jump there, then step back towards the old position (by the scroll speed) until the view is on the picture.
    const int32_t oldX = scrollX;
    const int32_t oldY = scrollY;
    scrollY = y;
    scrollX = x;
    int32_t stepY = theInterface->tacScrollSpeed;
    const int32_t stepX = oldX < x ? -stepY : stepY;

    if (oldY < y)
    {
        stepY = -stepY;
    }

    int doneX = 0;
    int doneY = 0;

    while (doneX == 0 || doneY == 0)
    {
        if (!scrollInPicture(scrollX, mapWidth, zoom))
        {
            scrollX += stepX;
        }
        else
        {
            doneX = -1;
        }

        if (!scrollInPicture(scrollY, mapHeight, zoom))
        {
            scrollY += stepY;
        }
        else
        {
            doneY = -1;
        }
    }
}

auto TacticalMap::GetVideoRect() -> tagRECT
{
    // The name line's height (unscaled), then lineFont back to its double scale.
    lineFont->scaled = 0;
    lineFont->scale = 1.0f;
    uint8_t lineHeight = lineFont->fontHeight;

    if (lineFont->scaled != 0)
    {
        lineHeight = static_cast<uint8_t>(std::floor(static_cast<float>(lineHeight) * lineFont->scale));
    }

    lineFont->scale = 2.0f;
    lineFont->scaled = 1;
    tagRECT rect;
    rect.left = videoWindow->globalX();
    rect.top = videoWindow->globalY() + lineHeight + 4;
    rect.right = videoWindow->width();
    rect.bottom = videoWindow->height() - (lineHeight + 4);
    return rect;
}

auto TacticalMap::AddSalvage(GameObject* obj) -> int
{
    const int32_t count = numSalvage;

    if (count > 99)
    {
        // Original behaviour (OB-052): full, the count drops back to 99, forgetting the last entry.
        numSalvage = 99;
        return 0;
    }

    if (obj->objectClass != BATTLEMECH && obj->objectClass != GROUNDVEHICLE && obj->isBuilding() == 0)
    {
        return 0;
    }

    // A mech whose status byte is 2 isn't salvage.
    if (static_cast<uint8_t>(obj->status) == 2 && obj->objectClass == BATTLEMECH)
    {
        return 0;
    }

    for (int32_t i = 0; i < count; i++)
    {
        if (salvage[i] == obj)
        {
            return -1;
        }
    }

    if (MPlayer == nullptr && obj->isBuilding() != 0)
    {
        soundSystem->playBettySample(2);
    }

    salvage[count] = obj;
    realSalvageCount = numSalvage + 1;
    numSalvage = realSalvageCount;
    AddSalvageString(obj);
    return -1;
}

auto TacticalMap::RemoveSalvage(GameObject* obj, int refresh) -> int
{
    const int32_t count = numSalvage;
    int32_t i = 0;

    while (i < count && salvage[i] != obj)
    {
        i++;
    }

    // Original behaviour (OB-051): not found, it still tests the entry just past the end, which after an earlier
    // removal holds a stale copy of the last one. (At 100 entries the original read the next field; never a match.)
    if (i >= 100 || salvage[i] != obj)
    {
        return 0;
    }

    realSalvageCount = count - 1;
    numSalvage = realSalvageCount;

    for (; i < numSalvage; i++)
    {
        salvage[i] = salvage[i + 1];
    }

    if (refresh != 0)
    {
        refreshSalvageList();
    }

    return -1;
}

auto TacticalMap::UpdateSalvage() -> void
{
    // Drop destroyed units, starting over after each.
    for (int32_t i = 0; i < numSalvage; i++)
    {
        GameObject* obj = salvage[i];

        if (obj != nullptr && isMoverClass(obj) && obj->isDestroyed() != 0)
        {
            RemoveSalvage(obj, -1);
            i = -1;
        }
    }

    refreshSalvageList();
}

auto TacticalMap::refreshSalvageList() -> void
{
    const int32_t firstPixel = salvageText->firstPixel;
    salvageText->Clear();

    for (int32_t i = 0; i < numSalvage; i++)
    {
        AddSalvageString(salvage[i]);
    }

    aScrollTextObject* text = salvageText;
    text->firstPixel = firstPixel;
    text->ResetPortSize();
    text->PositionScrollTab();
}

auto TacticalMap::SetID(int32_t partId) -> void
{
    auto* obj = static_cast<GameObject*>(objectList->findObjectFromPart(partId));

    if (displayType != TACMAP_INFO)
    {
        infoObject = obj;
        return;
    }

    infoDirty = -1;

    for (aPort* port : infoPorts)
    {
        if (port != nullptr)
        {
            port->destroy();
        }
    }

    if (obj == nullptr || !isMoverClass(obj))
    {
        return;
    }

    // Port fix: the original leaves the shape file's name unset for elementals and other movers (whatever the stack
    // held); the port keeps the last one, starting with the generic vehicle's.
    static char shapeName[32] = "vr106";
    File shapeFile;
    // (The original copied the info page's background into the MFD's picture here; draw shows it each frame.)
    statusDirty = -1;

    if (obj->objectClass == BATTLEMECH)
    {
        // A mech: front, rear and payload views, and the pilot's picture.
        aToolButton* front = dataButtons[0];
        front->setUpPicture(const_cast<char*>("mfddbh01.tga"));
        front->setDownPicture(const_cast<char*>("mfddbg01.tga"));
        front->setGrayPicture(const_cast<char*>("mfddbn01.tga"));
        front->moveTo(0xf, 0xcc, 0);
        dataButtons[2]->moveTo(0x56, 0xcc, 0);
        dataButtons[1]->ShowGUIWindow(-1);
        std::snprintf(shapeName, sizeof(shapeName), "mechrep%02i", obj->getObjectType()->iconNumber);
        infoPorts[0]->init(obj->getPilot()->picture);
    }
    else if (obj->objectClass == GROUNDVEHICLE)
    {
        // A vehicle: no rear view; its passengers' pictures.
        aToolButton* front = dataButtons[0];
        front->setUpPicture(const_cast<char*>("mfddbh00.tga"));
        front->setDownPicture(const_cast<char*>("mfddbg00.tga"));
        front->setGrayPicture(const_cast<char*>("mfddbn00.tga"));
        front->moveTo(0x21, 0xcc, 0);
        dataButtons[2]->moveTo(0x4b, 0xcc, 0);
        dataButtons[1]->ShowGUIWindow(0);

        if (dataDisplayMode == 1)
        {
            SetDataDisplayMode(0, 0);
        }

        auto* vehicle = static_cast<GroundVehicle*>(obj);

        // Original behaviour: the pictures go by seat, while draw shows the passengers packed (see draw).
        for (int32_t seat = 0; seat < vehicle->seats; seat++)
        {
            if (vehicle->passengers[seat] != nullptr)
            {
                infoPorts[seat]->init(vehicle->passengers[seat]->picture);
            }
        }

        if (obj->getObjectType()->iconNumber == 0)
        {
            std::snprintf(shapeName, sizeof(shapeName), "vr106");
        }
        else
        {
            std::snprintf(shapeName, sizeof(shapeName), "vr%i", obj->getObjectType()->iconNumber);
        }
    }

    // The part diagram's shapes.
    FullPathFileName shapePath;
    shapePath.init(artPath, shapeName, ".shp");

    if (shapeFile.open(shapePath, READ, 0x32) != 0)
    {
        Fatal(0, "Unable to open damage display shape file");
    }

    freePartShapes();
    partShapes = std::make_unique<uint8_t[]>(shapeFile.getLength());
    shapeFile.read(partShapes.get(), static_cast<int32_t>(shapeFile.getLength()));
    MCRenderer::RegisterData(partShapes.get(), shapeFile.getLength(), MCDataKind::Shapes);
    shapeFile.close();
    infoObject = obj;
    RefreshPage();
}

auto TacticalMap::updateOrderPalette() -> void
{
    // A mode chosen: only its button pushed.
    const int32_t command = theInterface->currentCommand;

    if (command != 0)
    {
        for (ToolPalButton* button : toolButtons)
        {
            if (button->action == command)
            {
                if (button->pushed == 0)
                {
                    button->pushed = -1;
                }
            }
            else if (button->pushed != 0)
            {
                button->pushed = 0;
            }
        }

        return;
    }

    if (theInterface->AnySelected(0) == 0)
    {
        // Nothing selected: every mode button released and grayed.
        for (int32_t i = 0; i < NUM_MODE_BUTTONS; i++)
        {
            ToolPalButton* button = toolButtons[i];

            if (button->pushed != 0)
            {
                button->pushed = 0;
                theInterface->currentCommand = 0;
                theInterface->commandOneShot = 0;
            }

            if (button->disabled == 0)
            {
                button->disabled = -1;
            }
        }

        return;
    }

    // A selection: released and enabled, the jump button only if every selected unit can jump.
    for (int32_t i = 0; i < NUM_MODE_BUTTONS; i++)
    {
        ToolPalButton* button = toolButtons[i];

        if (button->pushed != 0)
        {
            button->pushed = 0;
        }

        if (button->action == 0x11)
        {
            const int cannotJump = theInterface->canSelectionJump() == 0 ? 1 : 0;

            if (button->disabled != cannotJump)
            {
                button->disabled = cannotJump;
            }
        }
        else if (button->disabled != 0)
        {
            button->disabled = 0;
        }
    }
}

auto TacticalMap::SetDataDisplayMode(char mode, int silent) -> void
{
    if (dataDisplayMode == mode)
    {
        return;
    }

    dataDisplayMode = mode;
    infoDirty = -1;

    for (int32_t i = 0; i < 3; i++)
    {
        dataButtons[i]->pushed = mode == i ? 1 : 0;
    }

    if (silent == 0)
    {
        soundSystem->playDigitalSample(0x47, 1, nullptr, 0, 0);
    }
}

auto TacticalMap::toggleZoom() -> void
{
    ToolPalButton* button = toolButtons[7];
    button->pushed = button->pushed == 0 ? 1 : 0;
}

auto TacticalMap::positionOnMap(vector_3d pos) -> vector_3d
{
    // Clamp the unscrolled map position to the 130-pixel map; unclamped, the point is on it.
    vector_3d onMap = pos;
    worldToTacMap(onMap, 0);
    int clamped = 0;

    if (onMap.x < 0.0)
    {
        onMap.x = 0.0f;
        clamped = -1;
    }
    else if (onMap.x > MAP_PICTURE_SIDE)
    {
        onMap.x = MAP_PICTURE_SIDE;
        clamped = -1;
    }

    if (onMap.y < 0.0)
    {
        onMap.y = 0.0f;
    }
    else if (onMap.y > MAP_PICTURE_SIDE)
    {
        onMap.y = MAP_PICTURE_SIDE;
    }
    else if (clamped == 0)
    {
        return vector_3d(0.0f, 0.0f, 0.0f);
    }

    tacMapToWorld(onMap, 0);
    return vector_3d(pos.x - onMap.x, pos.y - onMap.y, 0.0f);
}

auto TacticalMap::handleChatMessage(uint32_t fromID, const void* message) -> void
{
    chatWindow->handleNetworkMessage(fromID, const_cast<void*>(message));

    if (IsHidden() != 0 || displayType != TACMAP_SALVAGE)
    {
        // Not on show: blink the chat tab.
        chatPending = -1;
        application->AddTimer(this, 1, 500, 0, 0, 0);

        if (displayType == TACMAP_SALVAGE)
        {
            if (chatBlinkerOn != nullptr)
            {
                chatBlinkerOn->ShowGUIWindow(-1);
            }
        }
        else if (chatBlinkerOff != nullptr)
        {
            chatBlinkerOff->ShowGUIWindow(-1);
        }
    }

    if (soundSystem != nullptr)
    {
        soundSystem->playDigitalSample(0x11, 1, nullptr, 0, 0);
    }
}

auto TacticalMap::activateArtillery(int32_t button, int arm) -> void
{
    if (button < 0 || button >= 4)
    {
        return;
    }

    ArtilleryButton* strike = artilleryButtons[button];
    aEvent event;

    if (arm == 0)
    {
        // Disarm as if Escape were pressed.
        strike->keyArmed = 0;
        event.clear();
        event.type = EVENT_KEY_UP;
        event.key = 0x1b;
    }
    else
    {
        // Arm as if clicked (pressed, then released).
        if (strike->keyArmed != 0)
        {
            return;
        }

        strike->keyArmed = -1;
        event.clear();
        event.type = EVENT_LEFT_DOWN;
        strike->handleEvent(&event);
        event.type = EVENT_LEFT_UP;
    }

    strike->handleEvent(&event);
}

auto TacticalMap::AddSalvageString(GameObject* obj) -> void
{
    char line[64];
    aScrollTextObject* text = salvageText;

    if (obj->objectClass == BATTLEMECH)
    {
        // A mech: its name, its undamaged weapons, and its sensor if undamaged.
        auto* mech = static_cast<Mover*>(obj);
        std::snprintf(line, sizeof(line), "%s", mech->getIfaceName());
        text->Print(line, 0xb);
        const uint32_t first = mech->numOther;
        const uint32_t end = mech->numWeapons + first;

        for (uint32_t i = first; i < end; i++)
        {
            const InventoryItem& item = mech->inventory[i];

            if (item.health == static_cast<int8_t>(MasterComponentList[item.masterID].health))
            {
                std::snprintf(line, sizeof(line), "    %s", MasterComponentList[item.masterID].abbreviation);
                text->Print(line, 0xc);
            }
        }

        const InventoryItem& sensor = mech->inventory[mech->sensor];

        if (sensor.health == static_cast<int8_t>(MasterComponentList[sensor.masterID].health))
        {
            std::snprintf(line, sizeof(line), "    %s", MasterComponentList[sensor.masterID].abbreviation);
            text->Print(line, 0x1f);
        }
    }
    else if (obj->objectClass == GROUNDVEHICLE)
    {
        // A vehicle: its name and its salvage.
        std::snprintf(line, sizeof(line), "%s", static_cast<Mover*>(obj)->getIfaceName());
        text->Print(line, 0xb);

        for (SalvageItem* item = obj->getSalvage(); item != nullptr; item = item->next)
        {
            std::snprintf(line, sizeof(line), "    %i %s", item->numItems,
                          MasterComponentList[item->itemId].abbreviation);
            text->Print(line, 0x1f);
        }
    }
    else
    {
        if (obj->isBuilding() == 0)
        {
            salvageText->ResetPortSize();
            return;
        }

        // A building or tree building: its name, its salvage.
        SalvageItem* item = obj->getSalvage();

        if (obj->objectClass == BUILDING)
        {
            std::snprintf(line, sizeof(line), "%s", static_cast<Building*>(obj)->name);
        }
        else if (obj->objectClass == TREEBUILDING)
        {
            std::snprintf(line, sizeof(line), "%s", static_cast<TreeBuilding*>(obj)->name);
        }
        else
        {
            line[0] = 0; // Port fix: isBuilding is only true for these two, but the buffer was uninitialised.
        }

        text->Print(line, 0xb);

        for (; item != nullptr; item = item->next)
        {
            std::snprintf(line, sizeof(line), "    %i %s", item->numItems,
                          MasterComponentList[item->itemId].abbreviation);
            text->Print(line, 0x1f);
        }
    }

    text->Print(nullptr, 0x1f);
    salvageText->ResetPortSize();
}

auto TacticalMap::DrawBar() -> void
{
    // The unit's effectiveness: green, yellow below half, red at a fifth.
    if (infoObject == nullptr)
    {
        return;
    }

    const float effectiveness = static_cast<Mover*>(infoObject)->getTotalEffectiveness();
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

auto TacticalMap::DrawParts() -> void
{
    auto* mover = static_cast<Mover*>(infoObject);

    if (mover == nullptr)
    {
        return;
    }

    // The armor locations: a mech's front ones (0..7) or, in the rear view, its rear ones over the rear diagram;
    // other units all of theirs.
    int16_t first;
    int16_t end;

    if (dataDisplayMode == 1)
    {
        AG_shape_draw(port()->frame(), partShapes.get(), mover->numBodyLocations + mover->numArmorLocations, 0x22,
                      0x65);
        first = 8;
        end = mover->numArmorLocations;
    }
    else
    {
        first = 0;
        end = mover->objectClass == BATTLEMECH ? 8 : mover->numArmorLocations;
    }

    for (int32_t i = first; i < end; i++)
    {
        AG_shape_lookaside(partColorTable(this, armorColors[i]));
        AG_shape_translate_draw(port()->frame(), partShapes.get(), i, 0x22, 0x65);
    }

    // A mech's front view also shows its internal structure.
    if (mover->objectClass == BATTLEMECH && dataDisplayMode == 0)
    {
        for (int16_t i = 0; i < mover->numBodyLocations; i++)
        {
            const int8_t numArmor = mover->numArmorLocations;
            AG_shape_lookaside(partColorTable(this, bodyColors[i]));
            AG_shape_translate_draw(port()->frame(), partShapes.get(), static_cast<int16_t>(numArmor + i), 0x22, 0x65);
        }
    }
}

auto TacticalMap::GetColors() -> void
{
    auto* mover = static_cast<Mover*>(infoObject);

    if (mover == nullptr)
    {
        return;
    }

    for (int32_t i = 0; i < mover->numBodyLocations; i++)
    {
        const BodyLocation& location = mover->bodyAt(i);

        if (location.damageState == 2)
        {
            bodyColors[i] = 0x19;
        }
        else
        {
            bodyColors[i] = damageColor(location.curInternalStructure, location.maxInternalStructure);
        }
    }

    for (int32_t i = 0; i < mover->numArmorLocations; i++)
    {
        const ArmorLocation& location = mover->armor[i];

        if (location.curArmor == 0.0)
        {
            armorColors[i] = 0x19;
        }
        else
        {
            armorColors[i] = damageColor(location.curArmor, location.maxArmor);
        }
    }
}

auto TacticalMap::drawPilot(MechWarrior* pilot) -> void
{
    // The picture.
    static const int32_t skillOrder[4] = {3, 0, 1, 2};
    FillBox(10, 0x2e, 0x21, 0x4c, 0x10);

    if (static_cast<Mover*>(pilot->vehicle)->netPlayerId >= 0)
    {
        VFX_pane_copy(infoPorts[0]->frame(), 0, 0, displayPort->frame(), 10, 0x2e, 0xfff);
    }

    // Four skill bars, 55 pixels at the best skill, each an outlined, shaded bar.
    int32_t yPos = 0x2f;

    for (const int32_t skill : skillOrder)
    {
        const auto value = static_cast<float>(pilot->skills[skill]);
        const auto length = static_cast<int32_t>(((value - MinPilotSkill) * 55.0f) / (MaxPilotSkill - MinPilotSkill));
        const int32_t barEnd = length + 0x4c;
        VFX_line_draw(port()->frame(), 0x4e, yPos, 0x4e, yPos + 1, LD_DRAW, 0xe3);
        VFX_line_draw(port()->frame(), 0x4f, yPos - 1, barEnd, yPos - 1, LD_DRAW, 0xe3);
        AG_pixel_write(port()->frame(), length + 0x4d, yPos - 1, 0x10);
        VFX_line_draw(port()->frame(), length + 0x4e, yPos - 1, length + 0x4e, yPos + 2, LD_DRAW, 0x10);
        AG_pixel_write(port()->frame(), length + 0x4d, yPos + 2, 0x10);
        VFX_line_draw(port()->frame(), length + 0x4d, yPos, length + 0x4d, yPos + 1, LD_DRAW, 0xe3);
        VFX_line_draw(port()->frame(), 0x4f, yPos + 2, barEnd, yPos + 2, LD_DRAW, 0xe5);
        VFX_line_draw(port()->frame(), 0x4f, yPos, barEnd, yPos, LD_DRAW, 0xe4);
        VFX_line_draw(port()->frame(), 0x4f, yPos + 1, barEnd, yPos + 1, LD_DRAW, 0xe4);
        yPos += 8;
    }

    // The wounds blank out health pips, right to left.
    int16_t pipX = 0x1b;

    for (int32_t i = 0; i < 6; i++)
    {
        if (pilot->wounds <= static_cast<float>(i))
        {
            break;
        }

        FillBox(pipX, 0x2b, static_cast<int16_t>(pipX + 1), 0x2c, 0x10);
        pipX = static_cast<int16_t>(pipX - 3);
    }

    // The callsign and the rank.
    whiteFont->writeString(port()->frame(), 10, 0x23, reinterpret_cast<uint8_t*>(pilot->callsign), -1);
    char rank[256] = {}; // Port fix: an unknown rank printed the uninitialised buffer.
    if (pilot->rank <= 3)
    {
        cLoadString(thisInstance, 0x86 + pilot->rank, rank, 0xfe);
    }

    whiteFont->writeString(port()->frame(), 0x33, 0x23, reinterpret_cast<uint8_t*>(rank), -1);
}

auto TacticalMap::drawWeapons() -> void
{
    aScrollTextObject* text = infoText;
    const int32_t firstPixel = text->firstPixel;
    text->Clear();
    auto* mover = static_cast<Mover*>(infoObject);

    if (mover == nullptr || !isMoverClass(mover))
    {
        return;
    }

    // The weapons, sorted by range bracket (up to 75, 150, beyond) and then damage.
    const int32_t numWeapons = mover->numWeapons;
    std::vector<WeaponEntry> weapons(static_cast<size_t>(std::max(numWeapons, 0)));

    const int32_t firstWeapon = mover->numOther;

    for (int32_t i = firstWeapon; i < mover->numWeapons + firstWeapon; i++)
    {
        const InventoryItem& item = mover->inventory[i];
        WeaponEntry& entry = weapons[i - firstWeapon];
        entry.masterID = item.masterID;
        const MasterComponent& component = MasterComponentList[item.masterID];
        const float longRange = component.weaponRange[3];

        if (longRange > 150.0f)
        {
            entry.sortKey = 20000;
        }
        else if (longRange > 75.0f)
        {
            entry.sortKey = 10000;
        }
        else
        {
            entry.sortKey = 0;
        }

        entry.sortKey = static_cast<int16_t>(component.damage + static_cast<float>(entry.sortKey));

        if (static_cast<int32_t>(item.health) < static_cast<int8_t>(component.health))
        {
            entry.state = 0xff;
        }
        else
        {
            entry.state = mover->getWeaponShots(i) == 0 ? 0 : 1;
        }
    }

    std::qsort(weapons.data(), static_cast<size_t>(numWeapons), sizeof(WeaponEntry), CompareWeapons);

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
        cLoadString(thisInstance, headerIds[bracket], header, 0xfe);
        text->Print(header, 0x1f);
        const auto limit = static_cast<float>(bracketLimits[bracket]);

        for (; next < numWeapons; next++)
        {
            const WeaponEntry& entry = weapons[next];
            const MasterComponent& component = MasterComponentList[entry.masterID];

            if (component.weaponRange[3] > limit && bracket != 2)
            {
                break;
            }

            if (entry.state == 0xff)
            {
                color = 0xef;
            }
            else if (entry.state == 0)
            {
                color = 0xf2;
            }
            else if (entry.state == 1)
            {
                color = 0xc;
            }

            std::snprintf(line, sizeof(line), bracketFormats[bracket], component.abbreviation);

            if (component.techBase == 1)
            {
                line[0] = static_cast<char>(0x1d + bracket);
            }

            text->Print(line, color);
            sectionCounts[bracket]++;
        }
    }

    // The equipment: sensor, ECM, jammer, probe; red when disabled or destroyed.
    cLoadString(thisInstance, 0x37e, header, 0xfe);
    text->Print(header, 0x1f);
    const uint8_t equipment[4] = {mover->sensor, mover->ecm, mover->jammer, mover->probe};

    for (const uint8_t index : equipment)
    {
        if (index == 0xff)
        {
            continue;
        }

        const InventoryItem& item = mover->inventory[index];
        color = (item.disabled != 0 || item.health == 0) ? 0xef : 0xc;
        std::snprintf(line, sizeof(line), "    %s", MasterComponentList[item.masterID].abbreviation);
        text->Print(line, color);
    }

    // The ammo: red when out, yellow under half.
    for (int32_t i = 0; i < mover->numAmmoTypes; i++)
    {
        const AmmoTally& ammo = mover->ammoTypeTotal[i];
        const int32_t amount = ammo.curAmount;

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
            color = ammo.startAmount / 2 <= amount ? 0xc : 0xf2;
        }

        std::snprintf(line, sizeof(line), "  %s", MasterComponentList[ammo.masterId].abbreviation);
        text->Print(line, color);
        cLoadString(thisInstance, 0x380, header, 0xfe);
        std::snprintf(line, sizeof(line), header, amount);
        text->Print(line, color);
    }

    // The weapon sections take the range colours.
    int32_t start = 0;

    for (int32_t i = 0; i < 4; i++)
    {
        text->sectionStarts[i] = start;
        start += 1 + sectionCounts[i];
        text->sectionColors[i] = static_cast<uint8_t>(RangeColorArray[i]);
    }

    text->firstPixel = firstPixel;
    text->ResetPortSize();
    text->PositionScrollTab();
}

auto CompareWeapons(const void* a, const void* b) -> int
{
    const auto keyA = static_cast<float>(static_cast<const WeaponEntry*>(a)->sortKey);
    const auto keyB = static_cast<float>(static_cast<const WeaponEntry*>(b)->sortKey);

    if (keyA == keyB)
    {
        return 0;
    }

    return keyB < keyA ? 1 : -1;
}
