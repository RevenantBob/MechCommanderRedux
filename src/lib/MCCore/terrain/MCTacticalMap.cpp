#include "stdafx.h"
#include "terrain/MCTacticalMap.h"
#include "camera/MCCamera.h"
#include "camera/MCMainWindow.h"
#include "engine/MCByteFlag.h"
#include "engine/MCFont.h"
#include "gui/MCGuiFont.h"
#include "gui/MCGuiPort.h"
#include "gui/MCGuiScrollTextObject.h"
#include "gui/MCGuiChatWindow.h"
#include "iface/MCTacticalInterface.h"
#include "lib/MCFatal.h"
#include "lib/MCFile.h"
#include "main/MCGamePaths.h"
#include "logistics/MCPreferencesMenu.h"
#include "terrain/MCMapBlockManager.h"
#include "main/MCMissionGlobals.h"
#include "main/MCGameStrings.h"
#include "mission/MCScenario.h"
#include "network/multplyr.h"
#include "object/MCBigGameObject.h"
#include "object/MCGroundVehicle.h"
#include "object/MCGroundVehicleType.h"
#include "object/MCGroundVehicleGameSystem.h"
#include "object/MCMover.h"
#include "object/MCMoverGameSystem.h"
#include "object/MCObjectType.h"
#include "object/MCObjectSystem.h"
#include "object/MCObjectQueue.h"
#include "object/MCForces.h"
#include "object/MCMechWarrior.h"
#include "platform/MCRenderer.h"
#include "sound/MCRadio.h"
#include "sound/MCSoundSystem.h"
#include "terrain/MCTacticalMapLayout.h"
#include "terrain/MCTerrain.h"
#include "vfx/MCVfx.h"
#include "vfx/MCVfxFunctions.h"

using namespace MCTacmapLayout;

bool MCTacticalMap::MarkersLit = false;
float MCTacticalMap::MarkerBlinkTime = 0.0f;

namespace
{
    /// <summary>The auto-repeat timers of the scroll buttons: the first delay, then the repeat.</summary>
    constexpr int16_t ScrollStartTimer = 4;
    constexpr int16_t ScrollRepeatTimer = 5;
    /// <summary>The timer that blinks the chat tab.</summary>
    constexpr int16_t ChatBlinkTimer = 1;

    /// <summary>The support command ids (and the button's strike count).</summary>
    constexpr int32_t StrikeSmall = 0xf9;
    constexpr int32_t StrikeLarge = 0xf8;
    constexpr int32_t StrikeSensor = 0xfa;
    constexpr int32_t StrikeCameraDrone = 0x204;

    /// <summary>The palette button's action that toggles the zoom instead of choosing a mode.</summary>
    constexpr MCInterfaceMode ActionToggleZoom = MCInterfaceMode::ToggleZoom;
    /// <summary>The palette button's action of the jump mode (enabled only when the whole selection can jump).</summary>
    constexpr MCInterfaceMode ActionJump = MCInterfaceMode::Jump;

    /// <summary>The number of the command palette's mode buttons (the eighth toggles the zoom).</summary>
    constexpr size_t NumModeButtons = 7;

    /// <summary>The interface mode of each command palette button (<c>IntMode</c> values).</summary>
    constexpr std::array<MCInterfaceMode, 8> ButtonActions = {MCInterfaceMode::AttackFromPosition,
                                                              MCInterfaceMode::AttackShortRange,
                                                              MCInterfaceMode::AttackMediumRange,
                                                              MCInterfaceMode::AttackLongRange,
                                                              MCInterfaceMode::Guard,
                                                              MCInterfaceMode::Jump,
                                                              MCInterfaceMode::Run,
                                                              ActionToggleZoom};

    /// <summary>A mech, vehicle, elemental or other mover (the classes that have a pilot and a sensor).</summary>
    bool IsMoverClass(const MCGameObject* obj)
    {
        return obj->ObjectClass == MCObjectClass::BattleMech || obj->ObjectClass == MCObjectClass::GroundVehicle ||
               obj->ObjectClass == MCObjectClass::Elemental || obj->ObjectClass == MCObjectClass::Mover;
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
    void SetPageRects(MCTacticalMap& map, int32_t upTop, int32_t upBottom, int32_t downTop, int32_t downBottom)
    {
        map.PageRects[0] = {0x7d, upTop, 0x88, upBottom};
        map.ScrollUpMarker->MoveTo(0x7d, upTop, 0);
        map.PageRects[1] = {0x7d, downTop, 0x88, downBottom};
        map.ScrollDownMarker->MoveTo(0x7d, downTop, 0);
        map.PageRects[2] = {0x7d, upBottom, 0x88, downTop};
    }

    /// <summary>String <paramref name="id"/> of the string table.</summary>
    std::string TableString(uint32_t id)
    {
        return LoadGameString(id, 0xfe);
    }

    /// <summary>Loads string <paramref name="id"/> as a help text (at most 0x31 characters, as the original's).</summary>
    std::string LoadHelpText(uint32_t id)
    {
        std::string text = TableString(id);

        if (text.size() > 0x31)
        {
            text.resize(0x31);
        }

        return text;
    }

    /// <summary>
    /// A plain aObject showing a picture. (The original loaded the picture into the object's own port; it is the
    /// object's background now, and the object draws itself.)
    /// </summary>
    MCGuiOwned<MCGuiObject> MakePicture(int32_t x, int32_t y, int32_t w, int32_t h, const char* picture)
    {
        auto obj = MCMakeGui<MCGuiObject>();
        obj->SetDrawsLive();
        obj->Init(x, y, w, h, nullptr);
        obj->SetBackground(const_cast<char*>(picture));
        return obj;
    }

    /// <summary>A button with its up, down and gray pictures.</summary>
    template <typename Button>
    MCGuiOwned<Button> MakeButton(int32_t x, int32_t y, int32_t w, int32_t h, const char* up, const char* down,
                                  const char* gray)
    {
        auto button = MCMakeGui<Button>();
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
    MCGuiOwned<MCGuiPort> LoadBackground(const char* fileName, std::string_view error)
    {
        auto background = MCMakeGui<MCGuiPort>();
        const int32_t result = background->Init(const_cast<char*>(fileName));
        Assert(result == 0, static_cast<uint32_t>(result), error);
        return background;
    }

    /// <summary>Stops the pilot video (and its radio movie) when switching away from the map page.</summary>
    void StopVideo()
    {
        MCTacticalMap* map = TacticalMap();

        if (map->VideoWindow->Star == nullptr)
        {
            return;
        }

        SoundSystem()->CurrentMessage->CloseMovie();
        map->VideoWindow->SetStar(nullptr);
    }

    /// <summary>After a zoom, recentres the map on the main camera (unless at 1x).</summary>
    void RecentreAfterZoom(MCTacticalMap& map)
    {
        map.MetersPerPixel = (map.MapDiagonal * DiagonalToPixels) / static_cast<float>(map.Zoom);

        if (Eye == nullptr)
        {
            return;
        }

        MCVector3D center = Eye->Position;
        map.ScrollY = 0;
        map.ScrollX = 0;
        const auto zoom = static_cast<float>(map.Zoom);
        const float scaleX = (static_cast<float>(map.MapWidth) * PictureToPixels) / zoom;
        const float scaleY = (static_cast<float>(map.MapHeight) * PictureToPixels) / zoom;

        if (map.Zoom == 1)
        {
            return;
        }

        map.WorldToTacMap(center, true);
        center.X = (center.X - MapCenterX) * scaleX;
        center.Y = (center.Y - MapCenterY) * scaleY;
        map.SetScrollMapPosition(static_cast<int32_t>(center.X), static_cast<int32_t>(center.Y));
    }

    /// <summary>Callback of the zoom-in button (at most 8x).</summary>
    void ZoomIn()
    {
        MCTacticalMap& map = *TacticalMap();
        const int32_t oldZoom = map.Zoom;
        map.Zoom = oldZoom * 2;

        if (map.Zoom < 9)
        {
            SoundSystem()->PlayDigitalSample(0x44, 1, nullptr, 0, 0);
            map.ZoomOffset += (map.MapVertexSide >> 1) / map.Zoom;
        }
        else
        {
            map.Zoom = 8;
        }

        RecentreAfterZoom(map);

        if (map.Zoom == 8)
        {
            map.ScrollButtons[4]->Disabled = true;
        }

        map.ScrollButtons[5]->Disabled = 0;

        // Zoomed in, the map can scroll.
        for (size_t i = 0; i < 4; i++)
        {
            map.ScrollButtons[i]->Disabled = 0;
        }

        map.RefreshPage();
    }

    /// <summary>Callback of the zoom-out button.</summary>
    void ZoomOut()
    {
        MCTacticalMap& map = *TacticalMap();
        map.ZoomOffset = std::max(map.ZoomOffset - (map.MapVertexSide >> 1) / map.Zoom, 0);
        map.Zoom >>= 1;

        if (map.Zoom == 0)
        {
            map.Zoom = 1;
        }
        else
        {
            SoundSystem()->PlayDigitalSample(0x45, 1, nullptr, 0, 0);
        }

        if (map.Zoom == 1)
        {
            // At 1x the whole map shows: no zooming out or scrolling.
            map.ScrollButtons[5]->Disabled = true;

            for (size_t i = 0; i < 4; i++)
            {
                map.ScrollButtons[i]->Disabled = true;
            }
        }

        map.ScrollButtons[4]->Disabled = 0;
        RecentreAfterZoom(map);
        map.RefreshPage();
    }

    /// <summary>
    /// Shared by the four scroll buttons: scroll once on press, then after the interface's scrollStart delay repeat
    /// five times as fast until release.
    /// </summary>
    void ScrollButtonEvent(MCGuiObject* obj, MCGuiEvent* event, int32_t dx, int32_t dy)
    {
        if (event->Type == EventLeftDown)
        {
            GuiSystem()->AddTimer(obj, ScrollStartTimer, TacticalInterface()->ScrollStart, 0, 0, 0);
            TacticalMap()->ScrollMap(dx, dy);
        }
        else if (event->Type == EventLeftUp)
        {
            GuiSystem()->RemoveTimer(obj, ScrollStartTimer);
            GuiSystem()->RemoveTimer(obj, ScrollRepeatTimer);
        }
        else if (event->Type == EventTimer)
        {
            TacticalMap()->ScrollMap(dx, dy);

            if (event->Data == ScrollStartTimer)
            {
                GuiSystem()->RemoveTimer(obj, ScrollStartTimer);
                GuiSystem()->AddTimer(obj, ScrollRepeatTimer, TacticalInterface()->ScrollStart / 5, 0, 0, 0);
            }
        }
    }

    /// <summary>Event routines of the map scroll buttons.</summary>
    void ScrollUp(MCGuiObject* obj, MCGuiEvent* event)
    {
        ScrollButtonEvent(obj, event, 0, -TacticalInterface()->TacScrollSpeed);
    }

    void ScrollLeft(MCGuiObject* obj, MCGuiEvent* event)
    {
        ScrollButtonEvent(obj, event, TacticalInterface()->TacScrollSpeed, 0);
    }

    void ScrollDown(MCGuiObject* obj, MCGuiEvent* event)
    {
        ScrollButtonEvent(obj, event, 0, TacticalInterface()->TacScrollSpeed);
    }

    void ScrollRight(MCGuiObject* obj, MCGuiEvent* event)
    {
        ScrollButtonEvent(obj, event, -TacticalInterface()->TacScrollSpeed, 0);
    }

    /// <summary>Callbacks of the info page's data buttons.</summary>
    void ArmorFrontButton()
    {
        TacticalMap()->SetDataDisplayMode(0, 0);
    }

    void PayloadButton()
    {
        TacticalMap()->SetDataDisplayMode(2, 0);
    }

    void RearButton()
    {
        TacticalMap()->SetDataDisplayMode(1, 0);
    }

    /// <summary>Shows the MFD on <paramref name="page"/>; a page switch away from the map stops the pilot video.</summary>
    void SwitchPage(MCTacmapPage page)
    {
        MCTacticalMap* map = TacticalMap();
        map->HideMe(0);

        if (map->DisplayType != page)
        {
            map->SetDisplayType(page);

            if (page != MCTacmapPage::Map)
            {
                StopVideo();
            }
        }
    }

    /// <summary>Event routine of the chat tab blinkers: passes the event to what lies under the blinker.</summary>
    void BlinkerHandleEvent(MCGuiObject* obj, MCGuiEvent* event)
    {
        // The port's resize broadcast (see MCFollowWindowSize) already reaches every object, and has no position:
        // passed to what lies under (0, 0), the tactical map, it would come back here forever.
        if (event->Type == EventResize)
        {
            return;
        }

        // Hides itself to find what lies under it, and passes the event there.
        obj->ShowGuiWindow(0);
        MCGuiObject* under = ScreenWindow()->FindObject(event->X, event->Y);
        obj->ShowGuiWindow(true);
        under->HandleEvent(event);
    }

    /// <summary>Event routine of the command palette buttons.</summary>
    void ToolPaletteButtonEvent(MCGuiObject* obj, MCGuiEvent* event)
    {
        if (event->Type != EventCallback)
        {
            return;
        }

        auto* button = static_cast<MCToolPalButton*>(obj);
        const MCInterfaceMode action = button->Action;

        if (action == ActionToggleZoom)
        {
            SoundSystem()->PlayDigitalSample(0x2f, 1, nullptr, 0, 0);
            ToggleZoom();
            return;
        }

        if (button->Pushed != 0)
        {
            // One mode at a time.
            SoundSystem()->PlayDigitalSample(0x35, 1, nullptr, 0, 0);

            for (size_t i = 0; i < NumModeButtons; i++)
            {
                MCToolPalButton* other = TacticalMap()->ToolButtons[i].get();

                if (other != button && other->Pushed != 0)
                {
                    other->Pushed = 0;
                }
            }

            TacticalInterface()->CurrentMode = action;
            TacticalInterface()->OneShotMode = true;
            return;
        }

        SoundSystem()->PlayDigitalSample(0x34, 1, nullptr, 0, 0);
        TacticalInterface()->SetMode(MCInterfaceMode::None);
    }

    /// <summary>Event routine of the tab strip: its four tabs switch pages; its top toggles the MFD.</summary>
    void TabStripEvent(MCGuiObject* obj, MCGuiEvent* event)
    {
        MCTacticalMap* map = TacticalMap();

        if (event->Type == EventLeftDown)
        {
            // The strip's four tabs, top to bottom; clicking the open page's tab while shown does nothing.
            const int32_t offset = event->Y - obj->GlobalY();

            if (offset > 0x1b)
            {
                MCTacmapPage page = MCTacmapPage::Salvage;

                if (offset < 0x4c)
                {
                    page = MCTacmapPage::Map;
                }
                else if (offset < 0x74)
                {
                    page = MCTacmapPage::Info;
                }
                else if (offset < 0xae)
                {
                    page = MCTacmapPage::Mission;
                }
                else if (offset > 0xe2)
                {
                    return;
                }

                if (map->DisplayType == page && map->IsHidden() == 0)
                {
                    return;
                }

                SwitchPage(page);
                SoundSystem()->PlayDigitalSample(0x36, 1, nullptr, 0, 0);
                return;
            }

            GuiSystem()->Grab(obj);
            map->HideMe(map->IsHidden() == 0);
        }
        else if (event->Type == EventLeftUp)
        {
            GuiSystem()->Release();
        }
        else if (event->Type == EventMouseMove)
        {
            GuiSystem()->SetCurrentCursor(static_cast<MCCursorType>(0));
        }
    }

    /// <summary>Event routine of the top tab: toggles the MFD.</summary>
    void TabTopEvent(MCGuiObject* /*obj*/, MCGuiEvent* event)
    {
        if (event->Type == EventLeftDown)
        {
            TacticalMap()->HideMe(TacticalMap()->IsHidden() == 0);
        }
        else if (event->Type == EventMouseMove)
        {
            GuiSystem()->SetCurrentCursor(static_cast<MCCursorType>(0));
        }
    }

    /// <summary>Event routine of the bottom tab: the salvage (or chat) page.</summary>
    void TabBottomEvent(MCGuiObject* /*obj*/, MCGuiEvent* event)
    {
        if (event->Type == EventLeftDown)
        {
            if (TacticalMap()->DisplayType != MCTacmapPage::Salvage)
            {
                SoundSystem()->PlayDigitalSample(0x36, 1, nullptr, 0, 0);
                SwitchPage(MCTacmapPage::Salvage);
            }
        }
        else if (event->Type == EventMouseMove)
        {
            GuiSystem()->SetCurrentCursor(static_cast<MCCursorType>(0));
        }
    }
}

auto MCSalvageList::Add(MCGameObject* obj) -> bool
{
    if (Contains(obj))
    {
        return false;
    }

    _Objects.push_back(obj);
    return true;
}

auto MCSalvageList::Remove(MCGameObject* obj) -> bool
{
    const auto found = std::ranges::find(_Objects, obj);

    if (found == _Objects.end())
    {
        return false;
    }

    _Objects.erase(found);
    return true;
}

auto MCSalvageList::Contains(const MCGameObject* obj) const -> bool
{
    return std::ranges::contains(_Objects, obj);
}

auto TogglePalette() -> void
{
    TacticalMap()->TogglePalette();
}

MCTacticalMap::MCTacticalMap() = default;

MCTacticalMap::~MCTacticalMap()
{
    InfoWatcher.Free();
}

auto MCTacticalMap::TogglePalette() -> void
{
    if (IsShowing() == 0)
    {
        return;
    }

    PaletteFrame->ShowGuiWindow(PaletteFrame->IsShowing() == 0);

    if (PaletteFrame->IsShowing() == 0)
    {
        SoundSystem()->PlayDigitalSample(0x41, 1, nullptr, 0, 0);
    }
    else
    {
        // Opening the palette drops the chosen mode.
        for (size_t i = 0; i < NumModeButtons; i++)
        {
            if (ToolButtons[i]->Pushed != 0)
            {
                ToolButtons[i]->Pushed = 0;
                TacticalInterface()->SetMode(MCInterfaceMode::None);
            }
        }

        SoundSystem()->PlayDigitalSample(0x40, 1, nullptr, 0, 0);
    }

    PaletteButton->Pushed = PaletteFrame->IsShowing();
}

auto MCTacticalMap::ShowStatus(const std::string* text) -> void
{
    if (!StatusLocked)
    {
        StatusText = text;
    }
}

auto MCTacticalMap::ReleaseStatusLine() -> void
{
    StatusLocked = false;
    StatusText = nullptr;
    GuiSystem()->CursorHidden = 0;
    GuiSystem()->SetCurrentCursor(static_cast<MCCursorType>(0));
}

auto MCTacticalMap::SetRevealedBitmap(std::string_view fileName) -> void
{
    const std::string gifName = GamePath(TerrainPath, fileName, ".gif");

    if (!FileExists(gifName))
    {
        return;
    }

    MCFile gifFile;
    gifFile.Open(gifName);
    const uint32_t size = gifFile.FileSize();

    if (size == 0)
    {
        return;
    }

    std::vector<uint8_t> gif(size);
    gifFile.Read(gif.data(), size);
    gifFile.Close();
    VfxGifResolution(gif.data());
    std::vector<uint8_t> work(VfxGifBufferSize);
    VfxGifDraw(VisibilityPort->Frame(), gif.data(), work.data());
}

auto MCTacticalMap::Init(int32_t xPos, int32_t yPos) -> int32_t
{
    Zoom = 1;
    InfoObject = nullptr;
    ZoomOffset = 0;
    ObjectivesRevealed = false;
    PartShapes = {};
    MapVertexSide = MCTerrain::VerticesBlockSide * MCTerrain::BlocksMapSide;

    // The map picture.
    MapPort = MCMakeGui<MCGuiPort>();
    std::string pictureName = GamePath(TerrainPath, Terrain()->Name, ".tga");
    int32_t result = MapPort->Init(pictureName.data());
    Assert(result == 0, static_cast<uint32_t>(result), " could not start tacticalMap ");
    MapWidth = MapPort->Frame()->Window->XMax;
    MapHeight = MapPort->Frame()->Window->YMax;

    // The fog of war: a port whose pixels are the home side's visible bits.
    VisibilityPort = MCMakeGui<MCGuiPort>();
    VisibilityPort->Init(MapVertexSide, MapVertexSide);
    MCRenderer::DestroyTexture(VisibilityPort->Frame()->Window);
    MCGuiPort::FreePixels(VisibilityPort->Frame()->Window->Buffer);
    VisibilityPort->Frame()->Window->Buffer = Terrain()->HomeVisibleBits()->Data();
    // Port: the fog of war is a kept frame surface: the reveals draw it on the GPU as well as in the flags the game
    // reads, and the map page samples the GPU's copy instead of uploading the flags whenever they change.
    MCRenderer::AddFrameSurface(VisibilityPort->Frame()->Window, true);

    const float side = static_cast<float>(MapVertexSide) * static_cast<float>(MapVertexSide);
    MapDiagonal = std::sqrt(side + side) * MCTerrain::MetersPerVertex;
    MetersPerPixel = (MapDiagonal * DiagonalToPixels) / static_cast<float>(Zoom);

    MapBackground = LoadBackground("mfdmwn00.tga", "Error reading tacmap MFD background");
    InfoBackground = LoadBackground("mfddwn00.tga", "Error reading info MFD background");
    MissionBackground = LoadBackground("mfdbwn00.tga", "Error reading mission MFD background");

    // Port: the info page's data view backgrounds, which the original loaded each time it drew one (and skipped
    // when one failed to load).
    static constexpr std::array<const char*, 3> viewBackgroundNames = {"mfddwn01.tga", "mfddwn02.tga", "mfddwn03.tga"};

    for (size_t i = 0; i < viewBackgroundNames.size(); i++)
    {
        InfoViewBackgrounds[i] = MCMakeGui<MCGuiPort>();

        if (InfoViewBackgrounds[i]->Init(const_cast<char*>(viewBackgroundNames[i])) != 0)
        {
            InfoViewBackgrounds[i].reset();
        }
    }

    result = MCGuiObject::Init(xPos, yPos, 0x8c, 0xef, nullptr);

    if (result != 0)
    {
        return result;
    }

    if (MPlayer == nullptr)
    {
        SalvageBackground = MissionBackground.get();
    }
    else
    {
        _OwnedSalvageBackground = LoadBackground("mfdswn01.tga", "Error reading salvage MFD background");
        SalvageBackground = _OwnedSalvageBackground.get();
        ChatWindow = MCMakeGui<MCGuiChatWindow>();
        result = ChatWindow->Init(6, 0x22, 0x82, 0x99, nullptr);
        AddChild(ChatWindow.get());
        Assert(result == 0, static_cast<uint32_t>(result), "Error initing chat object");
        result = 0;
    }

    ChatPending = false;

    // The tabs along the MFD's right edge.
    TabTop = MakePicture(Width(), 0, 0xc, 4, "mfdmts00.tga");
    TabTop->SetEventRoutine(TabTopEvent);
    TabTop->SetTransparent(true);
    AddChild(TabTop.get());

    TabStrip = MCMakeGui<MCGuiObject>();
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

        ChatBlinkerOff = MCMakeGui<MCGuiObject>();
        ChatBlinkerOff->SetDrawsLive();
        result = ChatBlinkerOff->Init(Width() + 2, blinkerTop, 10, 0x3a, nullptr);
        Assert(result == 0, static_cast<uint32_t>(result), "Error initing chat blinker object");
        ChatBlinkerOff->SetBackground(const_cast<char*>("mfdsts05.tga"));
        ChatBlinkerOff->SetDepth(10);
        AddChild(ChatBlinkerOff.get());
        ChatBlinkerOff->ShowGuiWindow(0);
        ChatBlinkerOff->SetEventRoutine(BlinkerHandleEvent);

        ChatBlinkerOn = MCMakeGui<MCGuiObject>();
        ChatBlinkerOn->SetDrawsLive();
        result = ChatBlinkerOn->Init(Width(), blinkerTop, 10, 0x3a, nullptr);
        Assert(result == 0, static_cast<uint32_t>(result), "Error initing chat blinker object");
        ChatBlinkerOn->SetBackground(const_cast<char*>("mfdsts04.tga"));
        ChatBlinkerOn->SetDepth(10);
        AddChild(ChatBlinkerOn.get());
        ChatBlinkerOn->ShowGuiWindow(0);
        ChatBlinkerOn->SetEventRoutine(BlinkerHandleEvent);
    }

    TabStrip->SetEventRoutine(TabStripEvent);
    AddChild(TabStrip.get());

    TabBottom = MakePicture(Width(), 0xeb, 0xc, 4, "mfdmts02.tga");
    TabBottom->SetEventRoutine(TabBottomEvent);
    TabBottom->SetTransparent(true);
    AddChild(TabBottom.get());

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
        std::array<const char*, 3> Pictures;
    };

    static constexpr std::array<SupportButton, 4> supportButtons = {{
        {6, StrikeSmall, 0x93, {"mfdtbh00.tga", "mfdtbg00.tga", "mfdtbn00.tga"}},
        {0x26, StrikeLarge, 0x94, {"mfdtbh01.tga", "mfdtbg01.tga", "mfdtbn01.tga"}},
        {0x47, StrikeSensor, 0x95, {"mfdtbh02.tga", "mfdtbg02.tga", "mfdtbn02.tga"}},
        {0x68, StrikeCameraDrone, 0x96, {"mfdtbh03.tga", "mfdtbg03.tga", "mfdtbn03.tga"}},
    }};

    for (size_t i = 0; i < supportButtons.size(); i++)
    {
        const SupportButton& spec = supportButtons[i];
        ArtilleryButtons[i] =
            MakeButton<MCArtilleryButton>(spec.X, 6, 0x1f, 0x16, spec.Pictures[0], spec.Pictures[1], spec.Pictures[2]);
        ArtilleryButtons[i]->CommandId = spec.CommandId;
        ArtilleryButtons[i]->HelpText = LoadHelpText(spec.HelpId);
        AddChild(ArtilleryButtons[i].get());
    }

    // The info page's data buttons.
    struct DataButton
    {
        size_t Index;
        int32_t X;
        void (*Callback)();
        std::array<const char*, 3> Pictures;
    };

    static constexpr std::array<DataButton, 3> dataButtonSpecs = {{
        {0, 0xf, ArmorFrontButton, {"mfddbh01.tga", "mfddbg01.tga", "mfddbn01.tga"}},
        {2, 0x56, PayloadButton, {"mfddbh02.tga", "mfddbg02.tga", "mfddbn02.tga"}},
        {1, 0x35, RearButton, {"mfddbh03.tga", "mfddbg03.tga", "mfddbn03.tga"}},
    }};

    for (const DataButton& spec : dataButtonSpecs)
    {
        MCGuiOwned<MCGuiToolButton>& button = DataButtons[spec.Index];
        button =
            MakeButton<MCGuiToolButton>(spec.X, 0xcc, 0x20, 0xc, spec.Pictures[0], spec.Pictures[1], spec.Pictures[2]);
        button->Framed = 0;
        button->Callback()->SetExec(spec.Callback);
        AddChild(button.get());
    }

    // The scroll buttons (disabled at 1x) and the zoom buttons.
    struct ScrollButton
    {
        int32_t X;
        int32_t Y;
        void (*Routine)(MCGuiObject*, MCGuiEvent*);
        std::array<const char*, 3> Pictures;
    };

    static constexpr std::array<ScrollButton, 4> scrollSpecs = {{
        {0x4b, 0xb5, ScrollUp, {"mfdmbh00.tga", "mfdmbg00.tga", "mfdmbn00.tga"}},
        {0x59, 0xc0, ScrollLeft, {"mfdmbh01.tga", "mfdmbg01.tga", "mfdmbn01.tga"}},
        {0x4b, 0xcd, ScrollDown, {"mfdmbh02.tga", "mfdmbg02.tga", "mfdmbn02.tga"}},
        {0x3f, 0xc0, ScrollRight, {"mfdmbh03.tga", "mfdmbg03.tga", "mfdmbn03.tga"}},
    }};

    for (size_t i = 0; i < scrollSpecs.size(); i++)
    {
        const ScrollButton& spec = scrollSpecs[i];
        ScrollButtons[i] =
            MakeButton<MCGuiButton>(spec.X, spec.Y, 0xd, 0xb, spec.Pictures[0], spec.Pictures[1], spec.Pictures[2]);
        ScrollButtons[i]->SetEventRoutine(spec.Routine);
        AddChild(ScrollButtons[i].get());
    }

    ScrollButtons[5] = MakeButton<MCGuiButton>(0x74, 0xca, 0xd, 0xd, "mfdmbh04.tga", "mfdmbg04.tga", "mfdmbn04.tga");
    ScrollButtons[5]->Callback()->SetExec(ZoomOut);
    ScrollButtons[5]->Disabled = true;
    AddChild(ScrollButtons[5].get());
    ScrollButtons[4] = MakeButton<MCGuiButton>(0x74, 0xb6, 0xd, 0xd, "mfdmbh05.tga", "mfdmbg05.tga", "mfdmbn05.tga");
    ScrollButtons[4]->Callback()->SetExec(ZoomIn);
    AddChild(ScrollButtons[4].get());

    for (size_t i = 0; i < 4; i++)
    {
        ScrollButtons[i]->Disabled = true;
    }

    // The command palette, hidden until the palette button opens it.
    PaletteFrame = MCMakeGui<MCGuiObject>();
    PaletteFrame->SetDrawsLive();
    PaletteFrame->Init(0, 0xe9, 0x8c, 0x35, nullptr);
    PaletteFrame->SetDepth(10);
    PaletteFrame->SetBackground(const_cast<char*>("mfdcwn00.tga"));
    AddChild(PaletteFrame.get());
    PaletteFrame->ShowGuiWindow(0);
    PaletteBottom = MCMakeGui<MCGuiObject>();
    PaletteBottom->SetDrawsLive();
    PaletteBottom->Init(PaletteFrame->Width(), 0, 2, 0x35, nullptr);
    PaletteBottom->SetBackground(const_cast<char*>("mfdcwn01.tga"));
    PaletteBottom->SetTransparent(true);
    PaletteFrame->AddChild(PaletteBottom.get());

    PaletteButton = MakeButton<MCGuiToolButton>(6, 0xe0, 9, 9, "mfdcwn02.tga", "mfdcwn03.tga", nullptr);
    PaletteButton->Framed = 0;
    PaletteButton->Callback()->SetExec(::TogglePalette);
    AddChild(PaletteButton.get());

    ScrollUpMarker = MakePicture(0, 0, 0xb, 0xb, "mfddbg04.tga");
    ScrollUpMarker->ShowGuiWindow(0);
    AddChild(ScrollUpMarker.get());
    ScrollDownMarker = MakePicture(0, 0, 0xb, 0xb, "mfddbg05.tga");
    ScrollDownMarker->ShowGuiWindow(0);
    AddChild(ScrollDownMarker.get());

    LastMapTime = -999.0f;

    // The palette's mode buttons, two rows of four.
    int32_t buttonX = 6;
    int32_t buttonY = 2;

    for (size_t i = 0; i < ToolButtons.size(); i++)
    {
        ToolButtons[i] = MCMakeGui<MCToolPalButton>();
        MCToolPalButton& button = *ToolButtons[i];
        button.Init(buttonX, buttonY, 0x1f, 0x16, nullptr);
        button.Callback()->SetMessage(&button, EventCallback);
        button.SetEventRoutine(ToolPaletteButtonEvent);
        const bool zoomButton = i == 7;
        std::string pictureName = std::format("{}{:02}.tga", zoomButton ? "mfdcbn" : "mfdcbh", i);
        button.SetUpPicture(pictureName.data());
        pictureName = std::format("{}{:02}.tga", zoomButton ? "mfdcbh" : "mfdcbg", i);
        button.SetDownPicture(pictureName.data());
        pictureName = std::format("mfdcbn{:02}.tga", i);
        button.SetGrayPicture(pictureName.data());
        button.Framed = 0;

        if (zoomButton)
        {
            // The original also disabled zoom in multiplayer; the port allows it.
            if (!Only45Pixel)
            {
                button.Action = ActionToggleZoom;
                button.HelpText = LoadHelpText(0x91);
            }
            else
            {
                // No zoom: only the 45-pixel art is loaded.
                button.SetGrayPicture(const_cast<char*>("mfdcbn07a.tga"));
                button.Disabled = true;
                button.HelpText = LoadHelpText(0x92);
            }
        }
        else
        {
            button.Action = ButtonActions[i];
            button.HelpText = LoadHelpText(0x8a + static_cast<uint32_t>(i));
        }

        PaletteFrame->AddChild(&button);
        buttonX += 1 + button.Width();

        if (i == 3)
        {
            buttonX = 6;
            buttonY = 0x19;
        }
    }

    VideoWindow = MCMakeGui<MCVideoWindow>();
    VideoWindow->Init(6, 0xaa, 0x30, 0x30, nullptr);
    VideoWindow->SetBackground(const_cast<char*>("mfdmwn01.tga"));
    AddChild(VideoWindow.get());

    InfoText = MCMakeGui<MCGuiScrollTextObject>();
    InfoText->Init(5, 0x22, 0x76, 0xb8, nullptr);
    AddChild(InfoText.get());
    InfoText->ShowGuiWindow(0);
    SalvageText = MCMakeGui<MCGuiScrollTextObject>();
    SalvageText->Init(5, 0x22, 0x76, 0xb8, nullptr);
    AddChild(SalvageText.get());
    SalvageText->ShowGuiWindow(0);

    Salvage = {};
    ScrollX = 0;
    ScrollY = 0;
    SetDisplayType(MCTacmapPage::Map);

    // The map area, as rectangles and as a pane on the MFD's window.
    MapPane.Window = DisplayPort->Frame()->Window;
    MapRect = {6, 0x22, 0x87, 0xa3};
    MapPane.X0 = 6;
    MapPane.Y0 = 0x22;
    MapPane.X1 = 0x87;
    MapPane.Y1 = 0xa3;
    ZoomInRect = {0x70, 0xb2, 0x89, 199};
    ZoomOutRect = {0x70, 199, 0xb2, 0xdb};

    for (MCGuiOwned<MCGuiPort>& port : InfoPorts)
    {
        port = MCMakeGui<MCGuiPort>();
    }

    SetDataDisplayMode(0, -1);
    LastRefreshTime = 0;
    RefreshPage();

    for (size_t i = 0; i < TypeStrings.size(); i++)
    {
        TypeStrings[i] = TableString(0x7c + static_cast<uint32_t>(i));
    }

    for (size_t i = 0; i < StatusStrings.size(); i++)
    {
        StatusStrings[i] = TableString(0x78 + static_cast<uint32_t>(i));
    }

    ColorRemap = MCRegisteredBlock(256, MCDataKind::Tables);
    std::ranges::fill(ColorRemap.Bytes(), 0xff);
    ColorRemap.Data()[0xe6] = 0x13;
    ColorRemap.Data()[0xe8] = 0x13;
    return result;
}

auto MCTacticalMap::Destroy() -> void
{
    MapPort.reset();

    if (VisibilityPort != nullptr)
    {
        // The bitmap's pixels are the visible bits'; the original clears the pane's window first.
        MCRenderer::RemoveFrameSurface(VisibilityPort->Frame()->Window);
        VisibilityPort->Frame()->Window = nullptr;
        VisibilityPort.reset();
    }

    // A context going down takes its interface down before the terrain.
    if (TacticalInterface() != nullptr)
    {
        TacticalInterface()->TacticalMap = nullptr;
    }

    for (MCGuiOwned<MCArtilleryButton>& button : ArtilleryButtons)
    {
        button.reset();
    }

    for (MCGuiOwned<MCGuiButton>& button : ScrollButtons)
    {
        button.reset();
    }

    for (MCGuiOwned<MCToolPalButton>& button : ToolButtons)
    {
        button.reset();
    }

    for (MCGuiOwned<MCGuiToolButton>& button : DataButtons)
    {
        button.reset();
    }

    PaletteButton.reset();
    PaletteBottom.reset();
    PaletteFrame.reset();
    SalvageText.reset();
    InfoText.reset();
    VideoWindow.reset();

    for (MCGuiOwned<MCGuiPort>& port : InfoPorts)
    {
        port.reset();
    }

    MapBackground.reset();
    InfoBackground.reset();

    for (MCGuiOwned<MCGuiPort>& background : InfoViewBackgrounds)
    {
        background.reset();
    }

    SalvageBackground = nullptr;
    MissionBackground.reset();
    _OwnedSalvageBackground.reset();
    TabTop.reset();
    TabStrip.reset();
    TabBottom.reset();
    ScrollUpMarker.reset();
    ScrollDownMarker.reset();
    ChatWindow.reset();
    ChatBlinkerOff.reset();
    ChatBlinkerOn.reset();
    PartShapes = {};
    ColorRemap = {};
    MouseInside = false;
    MCGuiObject::Destroy();

    for (std::string& text : TypeStrings)
    {
        text.clear();
    }

    for (std::string& text : StatusStrings)
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

    if (DisplayType == MCTacmapPage::Info)
    {
        MCGameObject* obj = InfoObject;
        const int32_t firstPixel = InfoText->FirstPixel;

        if (obj == nullptr || !IsMoverClass(obj))
        {
            InfoText->ShowGuiWindow(0);
            return;
        }

        if (DataDisplayMode == 2)
        {
            // Payload: the weapon list.
            InfoText->ShowGuiWindow(true);
            DrawWeapons();
        }
        else
        {
            // Armor: the part diagram's colours.
            GetColors();
            InfoText->ShowGuiWindow(0);
        }

        if (obj->ObjectClass == MCObjectClass::BattleMech)
        {
            InfoText->FirstPixel = firstPixel;
            InfoText->PositionScrollTab();
        }
        else if (obj->ObjectClass == MCObjectClass::GroundVehicle)
        {
            if (DataDisplayMode == 1)
            {
                SetDataDisplayMode(0, 0);
            }

            InfoText->FirstPixel = firstPixel;
            InfoText->PositionScrollTab();
        }

        return;
    }

    if (DisplayType == MCTacmapPage::Mission)
    {
        // The home side's objectives, each with its type and its timer or status.
        MCGuiScrollTextObject* text = InfoText.get();
        const int32_t firstPixel = text->FirstPixel;
        text->Clear();

        if (Turn > 1)
        {
            const auto count = static_cast<int32_t>(HomeTeam()->NumObjectives);
            int32_t objectiveNum = HomeTeam()->FirstObjective;

            for (int32_t i = 0; i < count; i++, objectiveNum++)
            {
                MCScenarioObjective* objective = &Scenario()->Objectives[objectiveNum];
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

                std::string line = std::format("{}--{}", i + 1, objective->Name);
                text->PrintWrapped(line.data(), color, -1);
                const uint32_t type = objective->Type + 1 > 3 ? 4 : objective->Type + 1;
                line = std::format("      {}", TypeStrings[type]);
                text->PrintWrapped(line.data(), color, -1);
                const float timeLeft = Scenario()->CheckObjectiveTimer(objectiveNum);

                if (timeLeft > 0.0)
                {
                    const auto seconds =
                        static_cast<int32_t>(std::floor(std::fmod(static_cast<double>(timeLeft), 60.0)));
                    const auto minutes = static_cast<int32_t>(timeLeft * (1.0 / 60.0));
                    line = std::format("{:02}:{:02}", minutes, seconds);
                }
                else
                {
                    const uint32_t status = objective->Status > 2 ? 3 : objective->Status;
                    line = std::format("      {}", StatusStrings[status]);
                }

                text->PrintWrapped(line.data(), color, -1);
                text->PrintBlank(0x1f);
            }
        }

        text->FirstPixel = firstPixel;
        text->ResetPortSize();
        text->PositionScrollTab();
    }
}

auto MCTacticalMap::HandleEvent(MCGuiEvent* event) -> void
{
    const int32_t screenX = event->X;
    const int32_t screenY = event->Y;
    const POINT local{screenX - GlobalX(), screenY - GlobalY()};

    // The zoom buttons' areas take the event whole.
    if (DisplayType == MCTacmapPage::Map)
    {
        if (PtInRect(&ZoomInRect, local) != 0)
        {
            ScrollButtons[4]->HandleEvent(event);
            return;
        }

        if (PtInRect(&ZoomOutRect, local) != 0)
        {
            ScrollButtons[5]->HandleEvent(event);
            return;
        }
    }

    const bool textPage = DisplayType != MCTacmapPage::Map;
    // The info and mission pages scroll InfoText, the salvage page SalvageText.
    MCGuiScrollTextObject* text = DisplayType == MCTacmapPage::Salvage ? SalvageText.get() : InfoText.get();

    switch (event->Type)
    {
        case EventLeftDown:
        {
            if (DisplayType == MCTacmapPage::Map && PtInRect(&MapRect, local) != 0)
            {
                MapDragging = true;
            }

            if (local.y > 0xe0)
            {
                // Below the pages: the palette toggle.
                TogglePalette();
                break;
            }

            if (textPage)
            {
                // Press and hold repeats.
                if (PtInRect(&PageRects[0], local) != 0)
                {
                    GuiSystem()->Grab(this);
                    ScrollUpMarker->ShowGuiWindow(true);
                    GuiSystem()->AddTimer(this, ScrollStartTimer, TacticalInterface()->ScrollStart, 0, 0, 0);
                    text->ReceiveClick(-1, 0);
                }
                else if (PtInRect(&PageRects[1], local) != 0)
                {
                    GuiSystem()->Grab(this);
                    ScrollDownMarker->ShowGuiWindow(true);
                    GuiSystem()->AddTimer(this, ScrollStartTimer, TacticalInterface()->ScrollStart, 0, 0, 0);
                    text->ReceiveClick(1, 0);
                }
                else if (PtInRect(&PageRects[2], local) != 0)
                {
                    // The info pages measure the click from InfoText's screen top, the salvage page from the track.
                    const int32_t yPos =
                        DisplayType == MCTacmapPage::Salvage ? local.y - PageRects[2].top : local.y - text->GlobalY();
                    text->ReceiveClick(0, yPos);
                }
            }
            break;
        }

        case EventLeftUp:
        {
            if (DisplayType == MCTacmapPage::Map && MapDragging)
            {
                // A click on the map moves the active camera there.
                MapDragging = false;

                if (PtInRect(&MapRect, local) != 0)
                {
                    MCVector3D target(static_cast<float>(local.x), static_cast<float>(local.y), 0.0f);
                    TacMapToWorld(target, true);
                    MCCamera* camera = MainHolder()->GetActivePane()->GetCamera();
                    camera->ChangeTarget(static_cast<MCBaseObject*>(nullptr), false);
                    camera->SetPosition(target);
                }
            }

            GuiSystem()->RemoveTimer(this, ScrollStartTimer);
            GuiSystem()->RemoveTimer(this, ScrollRepeatTimer);

            if (GuiSystem()->GrabbedObject() == this)
            {
                // Released on the top-right corner: toggles the MFD.
                if (local.x > 0x8c && local.y < 0x1c)
                {
                    HideMe(IsHidden() == 0);
                }
            }

            GuiSystem()->Release();
            ScrollUpMarker->ShowGuiWindow(0);
            ScrollDownMarker->ShowGuiWindow(0);
            break;
        }

        case EventTimer:
        {
            if (event->Data == ChatBlinkTimer)
            {
                // The chat tab blinks while a message is unread.
                if (!ChatPending)
                {
                    MCGuiObject* blinker =
                        DisplayType == MCTacmapPage::Salvage ? ChatBlinkerOn.get() : ChatBlinkerOff.get();

                    if (blinker != nullptr)
                    {
                        blinker->ShowGuiWindow(true);
                    }

                    ChatPending = true;
                }
                else
                {
                    for (MCGuiObject* blinker : {ChatBlinkerOff.get(), ChatBlinkerOn.get()})
                    {
                        if (blinker != nullptr)
                        {
                            blinker->ShowGuiWindow(0);
                        }
                    }

                    ChatPending = false;
                }
                break;
            }

            if (event->Data == ScrollStartTimer)
            {
                GuiSystem()->RemoveTimer(this, ScrollStartTimer);
                GuiSystem()->AddTimer(this, ScrollRepeatTimer, TacticalInterface()->ScrollStart / 5, 0, 0, 0);
            }
            else if (event->Data != ScrollRepeatTimer)
            {
                break;
            }

            if (textPage)
            {
                if (PtInRect(&PageRects[0], local) != 0)
                {
                    text->ReceiveClick(-1, 0);
                }
                else if (PtInRect(&PageRects[1], local) != 0)
                {
                    text->ReceiveClick(1, 0);
                }
            }
            break;
        }

        case EventZoomIn:
        {
            ZoomIn();
            break;
        }

        case EventZoomOut:
        {
            ZoomOut();
            break;
        }
    }

    MCGuiObject::HandleEvent(event);
}

auto MCTacticalMap::MouseWheel(int32_t steps, int32_t xPos, int32_t yPos) -> bool
{
    if (DisplayType != MCTacmapPage::Map)
    {
        MCGuiScrollTextObject* text = DisplayType == MCTacmapPage::Salvage ? SalvageText.get() : InfoText.get();
        return text->MouseWheel(steps, xPos, yPos);
    }

    // As clicking the zoom buttons (up in, down out), which are disabled at the ends.
    for (; steps != 0; steps += steps < 0 ? 1 : -1)
    {
        if (ScrollButtons[steps < 0 ? 4 : 5]->Disabled != 0)
        {
            break;
        }

        if (steps < 0)
        {
            ZoomIn();
        }
        else
        {
            ZoomOut();
        }
    }

    return true;
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
        MoveTo(X() + HideOffset, Y(), true);

        if (Hidden != 0)
        {
            const tagRECT screen = {2, 0, GuiSystem()->Width(), GuiSystem()->Height()};

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
                MoveTo(HomeX - Parent->GlobalX(), HomeY - Parent->GlobalY(), true);
                HideOffset = 0;
            }
        }
    }

    const bool updating = !atRest && (IsHidden() == 0 || HideOffset != 0);

    if (updating)
    {
        switch (DisplayType)
        {
            case MCTacmapPage::Map:
            {
                UpdateMapPage();
                break;
            }
            case MCTacmapPage::Info:
            case MCTacmapPage::Mission:
            {
                if (MCPort::Milliseconds() > LastRefreshTime + 500)
                {
                    LastRefreshTime = MCPort::Milliseconds();
                    RefreshPage();
                }
                break;
            }
            case MCTacmapPage::Salvage:
            {
                if (MPlayer == nullptr && MCPort::Milliseconds() > LastRefreshTime + 500)
                {
                    LastRefreshTime = MCPort::Milliseconds();
                    UpdateSalvage();
                }
                break;
            }
        }
    }

    if (MouseInside)
    {
        GuiSystem()->SetCurrentCursor(static_cast<MCCursorType>(0));
    }

    // Port: the MFD draws itself, then its children (the original copied its picture, then displayed them).
    DrawInFramePass(DisplayPort.get());

    // The original revealed the objectives' areas as it drew the map's units, after the fog of war.
    if (updating && DisplayType == MCTacmapPage::Map)
    {
        RevealObjectives();
    }
}

auto MCTacticalMap::UpdateMapPage() -> void
{
    // The mission timer, rewritten once a second.
    if (Scenario()->TimeLimit >= 0 && LastMapTime + 1.0f < ActualTime)
    {
        LastMapTime = ActualTime;
        const float remaining = static_cast<float>(Scenario()->TimeLimit) - ActualTime;

        if (remaining >= 0.0f)
        {
            const auto seconds = static_cast<int32_t>(std::fmod(static_cast<double>(remaining), 60.0));
            const auto minutes = static_cast<int32_t>(remaining * (1.0f / 60.0f));
            MapTimeText = std::format("{:02}:{:02}", minutes, seconds);
            MapTimeRed = false;
        }
        else
        {
            MapTimeText = "00:00";
            MapTimeRed = true;
        }

        MapTimeShown = true;
    }

    // Markers blink five times a second.
    MarkerBlinkTime = FrameLength + MarkerBlinkTime;

    if (MarkerBlinkTime > 0.2)
    {
        MarkerBlinkTime = 0.0f;
        MarkersLit = !MarkersLit;
    }
}

auto MCTacticalMap::RevealObjectives() -> void
{
    if (ObjectivesRevealed)
    {
        return;
    }

    // Once a pending objective with a position is found, every objective's area is revealed in the fog of war.
    const auto numObjectives = static_cast<int32_t>(HomeTeam()->NumObjectives);

    for (int32_t i = 0; i < numObjectives; i++)
    {
        const MCScenarioObjective& objective = Scenario()->Objectives[HomeTeam()->FirstObjective + i];

        if (objective.Position[0] == -99.0f || objective.Position[1] == -99.0f || objective.Position[2] == -99.0f ||
            objective.Status != 0)
        {
            continue;
        }

        for (int32_t j = 0; j < numObjectives; j++)
        {
            const MCScenarioObjective& area = Scenario()->Objectives[HomeTeam()->FirstObjective + j];

            if (area.Radius <= 0.0)
            {
                continue;
            }

            const auto column = static_cast<float>(std::floor(static_cast<double>(
                MCTerrain::OneOvermetersPerVertex * (area.Position[0] - MCTerrain::MapTopLeft3d100.X))));
            const auto row = static_cast<float>(std::floor(static_cast<double>(
                MCTerrain::OneOvermetersPerVertex * (MCTerrain::MapTopLeft3d100.Y - area.Position[1]))));
            const auto xc = static_cast<int32_t>(std::floor(static_cast<double>(column)));
            const auto yc = static_cast<int32_t>(std::floor(static_cast<double>(row)));
            const auto radius = static_cast<int32_t>(area.Radius / MetersPerPixel * WorldUnitsPerMeter);
            VfxEllipseFill(VisibilityPort->Frame(), xc, yc, radius, radius, 0x14);
        }

        ObjectivesRevealed = true;
        return;
    }
}

auto MCTacticalMap::HideMe(bool hide) -> void
{
    if (HideOffset != 0)
    {
        return;
    }

    if (hide == 0)
    {
        // Shown: the chat tab stops blinking.
        GuiSystem()->RemoveTimer(this, ChatBlinkTimer);

        for (MCGuiObject* blinker : {ChatBlinkerOn.get(), ChatBlinkerOff.get()})
        {
            if (blinker != nullptr)
            {
                blinker->ShowGuiWindow(0);
            }
        }
    }
    else
    {
        // Port fix: StopVideo checks the movie window, which the original ends without checking.
        StopVideo();
    }

    if (Hidden == hide)
    {
        return;
    }

    if (Turn > 1)
    {
        SoundSystem()->PlayDigitalSample(0x3b, 1, nullptr, 0, 0);
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
    Hidden = 0;
    HideOffset = HomeX != GlobalX() ? HomeX - GlobalX() : HomeY - GlobalY();
}

auto MCTacmapProjection::WorldToMap(MCVector3D& pos, bool scrolled) const -> void
{
    // Rotate 45 degrees (the map is drawn diamond-wise), then scale to pixels about the map's centre.
    const float worldX = pos.X;
    const float worldY = pos.Y;
    pos.X = worldX * MapRotation + worldY * MapRotation;
    const float rotatedY = worldY * MapRotation - worldX * MapRotation;
    pos.Y = rotatedY;

    if (scrolled)
    {
        pos.X = pos.X / MetersPerPixel;
        pos.Y = rotatedY / MetersPerPixel;
        pos.Z = pos.Z / MetersPerPixel;
        const auto zoom = static_cast<float>(Zoom);
        pos.X =
            (pos.X + MapCenterX) - (MapPictureSide / static_cast<float>(MapWidth)) * zoom * static_cast<float>(ScrollX);
        pos.Y = ((MapHalfSide - pos.Y) + MapTop) -
                (MapPictureSide / static_cast<float>(MapHeight)) * zoom * static_cast<float>(ScrollY);
        return;
    }

    const float scale = static_cast<float>(Zoom) * MetersPerPixel;
    const float pixelX = pos.X / scale;
    const float pixelY = rotatedY / scale;
    pos.Z = pos.Z / scale;
    pos.X = pixelX + MapHalfSide;
    pos.Y = MapHalfSide - pixelY;
}

auto MCTacmapProjection::MapToWorld(MCVector3D& pos, bool scrolled) const -> void
{
    pos.Z = 0.0f;

    if (scrolled)
    {
        const auto zoom = static_cast<float>(Zoom);
        pos.X =
            ((MapPictureSide / static_cast<float>(MapWidth)) * zoom * static_cast<float>(ScrollX) + pos.X) - MapLeft;
        pos.Y =
            ((MapPictureSide / static_cast<float>(MapHeight)) * zoom * static_cast<float>(ScrollY) + pos.Y) - MapTop;
    }

    const float pixelX = pos.X - MapHalfSide;
    const float pixelY = MapHalfSide - pos.Y;

    if (scrolled)
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

    // Rotate back.
    const float rotatedX = pos.X;
    const float rotatedY = pos.Y;
    pos.X = rotatedX * MapRotation + rotatedY * -MapRotation;
    pos.Y = rotatedY * MapRotation - rotatedX * -MapRotation;
}

auto MCTacticalMap::WorldToTacMap(MCVector3D& pos, bool scrolled) -> void
{
    Projection().WorldToMap(pos, scrolled);
}

auto MCTacticalMap::TacMapToWorld(MCVector3D& pos, bool scrolled) -> void
{
    // Back to the world, and stand the point on the ground.
    Projection().MapToWorld(pos, scrolled);
    pos.Z = TerrainElevationAt(pos);
}

auto MCTacticalMap::SetDisplayType(MCTacmapPage type) -> void
{
    DisplayType = type;

    // Hide every page's parts, then show the new page's.
    SalvageText->ShowGuiWindow(0);

    if (MPlayer != nullptr)
    {
        ChatWindow->ShowGuiWindow(0);

        if (GuiSystem()->TextObject() == ChatWindow->ChatInput.get())
        {
            GuiSystem()->ReleaseText();
        }
    }

    InfoText->ShowGuiWindow(0);
    InfoText->Clear();

    for (const MCGuiOwned<MCGuiButton>& button : ScrollButtons)
    {
        button->ShowGuiWindow(0);
    }

    VideoWindow->ShowGuiWindow(0);
    StopVideo();

    for (const MCGuiOwned<MCGuiToolButton>& button : DataButtons)
    {
        button->ShowGuiWindow(0);
    }

    switch (type)
    {
        case MCTacmapPage::Map:
        {
            if (TabHighlighted)
            {
                TabBottom->SetBackground(const_cast<char*>("mfdmts02.tga"));
            }

            TabStrip->SetBackground(const_cast<char*>(MPlayer == nullptr ? "mfdmts01.tga" : "mfdmts03.tga"));
            TabHighlighted = false;
            // (The original copied the page's background into the MFD's picture here; draw shows it each frame.)

            for (const MCGuiOwned<MCGuiButton>& button : ScrollButtons)
            {
                button->ShowGuiWindow(true);
            }

            VideoWindow->ShowGuiWindow(true);
            break;
        }

        case MCTacmapPage::Info:
        {
            for (const MCGuiOwned<MCGuiToolButton>& button : DataButtons)
            {
                button->ShowGuiWindow(true);
            }

            if (TabHighlighted)
            {
                TabBottom->SetBackground(const_cast<char*>("mfddts02.tga"));
            }

            TabStrip->SetBackground(const_cast<char*>(MPlayer == nullptr ? "mfddts01.tga" : "mfddts03.tga"));
            TabHighlighted = false;

            // (The original wiped the picture without a unit, and copied the page's background; draw does both.)
            if (InfoObject != nullptr)
            {
                SetID(InfoObject->PartId);
            }

            InfoText->MoveTo(7, 0x5e, 0);
            InfoText->Resize(0x74, 0x69);
            InfoText->ShowGuiWindow(true);
            SetPageRects(*this, 0x5d, 0x68, 0xbe, 0xc9);
            InfoText->FirstPixel = 0;
            RefreshPage();
            return;
        }

        case MCTacmapPage::Mission:
        {
            if (TabHighlighted)
            {
                TabBottom->SetBackground(const_cast<char*>("mfdbts02.tga"));
            }

            TabStrip->SetBackground(const_cast<char*>(MPlayer == nullptr ? "mfdbts01.tga" : "mfdbts03.tga"));
            TabHighlighted = false;
            InfoText->MoveTo(5, 0x22, 0);
            InfoText->Resize(0x76, 0xb8);
            InfoText->ShowGuiWindow(true);
            SetPageRects(*this, 0x22, 0x2d, 0xcf, 0xda);
            PageRects[2].bottom = 0xce;
            RefreshPage();
            return;
        }

        case MCTacmapPage::Salvage:
        {
            TabBottom->SetBackground(const_cast<char*>("mfdsts02.tga"));
            TabStrip->SetBackground(const_cast<char*>(MPlayer == nullptr ? "mfdsts01.tga" : "mfdsts03.tga"));
            TabHighlighted = true;

            if (MPlayer == nullptr)
            {
                SalvageText->ShowGuiWindow(true);
                RefreshSalvageList();
                SetPageRects(*this, 0x22, 0x2d, 0xcf, 0xda);
                PageRects[2].bottom = 0xce;
                RefreshPage();
                return;
            }

            // Multiplayer: the chat window, and the tab stops blinking.
            GuiSystem()->RemoveTimer(this, ChatBlinkTimer);

            for (MCGuiObject* blinker : {ChatBlinkerOn.get(), ChatBlinkerOff.get()})
            {
                if (blinker != nullptr)
                {
                    blinker->ShowGuiWindow(0);
                }
            }

            ChatWindow->ShowGuiWindow(true);
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
    if (ScrollInPicture(ScrollX + dx, MapWidth, Zoom))
    {
        ScrollX += dx;
    }

    if (ScrollInPicture(ScrollY + dy, MapHeight, Zoom))
    {
        ScrollY += dy;
    }
}

auto MCTacticalMap::SetScrollMapPosition(int32_t x, int32_t y) -> void
{
    const int32_t oldX = ScrollX;
    const int32_t oldY = ScrollY;
    ScrollY = y;
    ScrollX = x;
    const int32_t speed = TacticalInterface()->TacScrollSpeed;
    const int32_t stepX = oldX < x ? -speed : speed;
    const int32_t stepY = oldY < y ? -speed : speed;
    bool doneX = false;
    bool doneY = false;

    while (!doneX || !doneY)
    {
        if (!ScrollInPicture(ScrollX, MapWidth, Zoom))
        {
            ScrollX += stepX;
        }
        else
        {
            doneX = true;
        }

        if (!ScrollInPicture(ScrollY, MapHeight, Zoom))
        {
            ScrollY += stepY;
        }
        else
        {
            doneY = true;
        }
    }
}

auto MCTacticalMap::GetVideoRect() -> tagRECT
{
    // The name line's height (unscaled), then lineFont back to its double scale.
    LineFont()->Scaled = 0;
    LineFont()->Scale = 1.0f;
    const uint8_t lineHeight = LineFont()->FontHeight;
    LineFont()->Scale = 2.0f;
    LineFont()->Scaled = 1;
    tagRECT rect;
    rect.left = VideoWindow->GlobalX();
    rect.top = VideoWindow->GlobalY() + lineHeight + 4;
    rect.right = VideoWindow->Width();
    rect.bottom = VideoWindow->Height() - (lineHeight + 4);
    return rect;
}

auto MCTacticalMap::AddSalvage(MCGameObject* obj) -> int
{
    if (obj->ObjectClass != MCObjectClass::BattleMech && obj->ObjectClass != MCObjectClass::GroundVehicle &&
        obj->IsBuilding() == 0)
    {
        return 0;
    }

    // A mech whose status byte is 2 isn't salvage.
    if (static_cast<uint8_t>(obj->Status) == 2 && obj->ObjectClass == MCObjectClass::BattleMech)
    {
        return 0;
    }

    if (Salvage.Contains(obj))
    {
        return -1;
    }

    if (MPlayer == nullptr && obj->IsBuilding() != 0)
    {
        SoundSystem()->PlayBettySample(2);
    }

    Salvage.Add(obj);
    AddSalvageString(obj);
    return -1;
}

auto MCTacticalMap::RemoveSalvage(MCGameObject* obj, int refresh) -> int
{
    if (!Salvage.Remove(obj))
    {
        return 0;
    }

    if (refresh != 0)
    {
        RefreshSalvageList();
    }

    return -1;
}

auto MCTacticalMap::UpdateSalvage() -> void
{
    // Drop destroyed units.
    Salvage.RemoveIf([](MCGameObject* obj) { return obj != nullptr && IsMoverClass(obj) && obj->IsDestroyed() != 0; });
    RefreshSalvageList();
}

auto MCTacticalMap::RefreshSalvageList() -> void
{
    const int32_t firstPixel = SalvageText->FirstPixel;
    SalvageText->Clear();

    for (MCGameObject* obj : Salvage)
    {
        AddSalvageString(obj);
    }

    SalvageText->FirstPixel = firstPixel;
    SalvageText->ResetPortSize();
    SalvageText->PositionScrollTab();
}

auto MCTacticalMap::SetID(int32_t partId) -> void
{
    auto* obj = static_cast<MCGameObject*>(ObjectList()->FindObjectFromPart(partId));

    if (DisplayType != MCTacmapPage::Info)
    {
        InfoObject = obj;
        return;
    }

    for (const MCGuiOwned<MCGuiPort>& port : InfoPorts)
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
    static std::string shapeName = "vr106";

    if (obj->ObjectClass == MCObjectClass::BattleMech)
    {
        // A mech: front, rear and payload views, and the pilot's picture.
        MCGuiToolButton& front = *DataButtons[0];
        front.SetUpPicture(const_cast<char*>("mfddbh01.tga"));
        front.SetDownPicture(const_cast<char*>("mfddbg01.tga"));
        front.SetGrayPicture(const_cast<char*>("mfddbn01.tga"));
        front.MoveTo(0xf, 0xcc, 0);
        DataButtons[2]->MoveTo(0x56, 0xcc, 0);
        DataButtons[1]->ShowGuiWindow(true);
        shapeName = std::format("mechrep{:02}", static_cast<int32_t>(obj->GetObjectType()->IconNumber));
        InfoPorts[0]->Init(obj->GetPilot()->Picture.data());
    }
    else if (obj->ObjectClass == MCObjectClass::GroundVehicle)
    {
        // A vehicle: no rear view; its passengers' pictures.
        MCGuiToolButton& front = *DataButtons[0];
        front.SetUpPicture(const_cast<char*>("mfddbh00.tga"));
        front.SetDownPicture(const_cast<char*>("mfddbg00.tga"));
        front.SetGrayPicture(const_cast<char*>("mfddbn00.tga"));
        front.MoveTo(0x21, 0xcc, 0);
        DataButtons[2]->MoveTo(0x4b, 0xcc, 0);
        DataButtons[1]->ShowGuiWindow(0);

        if (DataDisplayMode == 1)
        {
            SetDataDisplayMode(0, 0);
        }

        auto* vehicle = static_cast<MCGroundVehicle*>(obj);

        // Original behaviour: the pictures go by seat, while the info page shows the passengers packed.
        for (int32_t seat = 0; seat < vehicle->Seats; seat++)
        {
            if (vehicle->Passengers[seat] != nullptr)
            {
                InfoPorts[static_cast<size_t>(seat)]->Init(vehicle->Passengers[seat]->Picture.data());
            }
        }

        const auto icon = static_cast<int32_t>(obj->GetObjectType()->IconNumber);
        shapeName = icon == 0 ? std::string("vr106") : std::format("vr{}", icon);
    }

    // The part diagram's shapes.
    MCFile shapeFile;

    if (shapeFile.Open(GamePath(ArtPath, shapeName, ".shp")) != 0)
    {
        Fatal(0, "Unable to open damage display shape file");
    }

    PartShapes = MCRegisteredBlock(shapeFile.GetLength(), MCDataKind::Shapes);
    shapeFile.Read(PartShapes.Data(), static_cast<int32_t>(shapeFile.GetLength()));
    shapeFile.Close();
    InfoObject = obj;
    RefreshPage();
}

auto MCTacticalMap::UpdateOrderPalette() -> void
{
    // A mode chosen: only its button pushed.
    const MCInterfaceMode mode = TacticalInterface()->CurrentMode;

    if (mode != MCInterfaceMode::None)
    {
        for (const MCGuiOwned<MCToolPalButton>& button : ToolButtons)
        {
            if (button->Action == mode)
            {
                if (button->Pushed == 0)
                {
                    button->Pushed = true;
                }
            }
            else if (button->Pushed != 0)
            {
                button->Pushed = 0;
            }
        }

        return;
    }

    if (!TacticalInterface()->AnySelected())
    {
        // Nothing selected: every mode button released and grayed.
        for (size_t i = 0; i < NumModeButtons; i++)
        {
            MCToolPalButton& button = *ToolButtons[i];

            if (button.Pushed != 0)
            {
                button.Pushed = 0;
                TacticalInterface()->SetMode(MCInterfaceMode::None);
            }

            if (button.Disabled == 0)
            {
                button.Disabled = true;
            }
        }

        return;
    }

    // A selection: released and enabled, the jump button only if every selected unit can jump.
    for (size_t i = 0; i < NumModeButtons; i++)
    {
        MCToolPalButton& button = *ToolButtons[i];
        button.Pushed = 0;

        if (button.Action == ActionJump)
        {
            button.Disabled = TacticalInterface()->CanSelectionJump() ? 0 : 1;
        }
        else
        {
            button.Disabled = 0;
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

    for (size_t i = 0; i < DataButtons.size(); i++)
    {
        DataButtons[i]->Pushed = static_cast<size_t>(mode) == i ? 1 : 0;
    }

    if (silent == 0)
    {
        SoundSystem()->PlayDigitalSample(0x47, 1, nullptr, 0, 0);
    }
}

auto MCTacticalMap::ToggleZoom() -> void
{
    MCToolPalButton& button = *ToolButtons[7];
    button.Pushed = button.Pushed == 0 ? 1 : 0;
}

auto MCTacticalMap::PositionOnMap(MCVector3D pos) -> MCVector3D
{
    // Clamp the unscrolled map position to the 130-pixel map; unclamped, the point is on it.
    MCVector3D onMap = pos;
    WorldToTacMap(onMap, false);
    bool clamped = false;

    if (onMap.X < 0.0)
    {
        onMap.X = 0.0f;
        clamped = true;
    }
    else if (onMap.X > MapPictureSide)
    {
        onMap.X = MapPictureSide;
        clamped = true;
    }

    if (onMap.Y < 0.0)
    {
        onMap.Y = 0.0f;
    }
    else if (onMap.Y > MapPictureSide)
    {
        onMap.Y = MapPictureSide;
    }
    else if (!clamped)
    {
        return MCVector3D(0.0f, 0.0f, 0.0f);
    }

    TacMapToWorld(onMap, false);
    return MCVector3D(pos.X - onMap.X, pos.Y - onMap.Y, 0.0f);
}

auto MCTacticalMap::HandleChatMessage(uint32_t fromID, const void* message) -> void
{
    ChatWindow->HandleNetworkMessage(fromID, const_cast<void*>(message));

    if (IsHidden() != 0 || DisplayType != MCTacmapPage::Salvage)
    {
        // Not on show: blink the chat tab.
        ChatPending = true;
        GuiSystem()->AddTimer(this, ChatBlinkTimer, 500, 0, 0, 0);
        MCGuiObject* blinker = DisplayType == MCTacmapPage::Salvage ? ChatBlinkerOn.get() : ChatBlinkerOff.get();

        if (blinker != nullptr)
        {
            blinker->ShowGuiWindow(true);
        }
    }

    if (SoundSystem() != nullptr)
    {
        SoundSystem()->PlayDigitalSample(0x11, 1, nullptr, 0, 0);
    }
}

auto MCTacticalMap::ActivateArtillery(int32_t button, int arm) -> void
{
    if (button < 0 || button >= 4)
    {
        return;
    }

    MCArtilleryButton& strike = *ArtilleryButtons[static_cast<size_t>(button)];
    MCGuiEvent event;

    if (arm == 0)
    {
        // Disarm as if Escape were pressed.
        strike.KeyArmed = false;
        event.Clear();
        event.Type = EventKeyUp;
        event.Key = 0x1b;
    }
    else
    {
        // Arm as if clicked (pressed, then released).
        if (strike.KeyArmed)
        {
            return;
        }

        strike.KeyArmed = true;
        event.Clear();
        event.Type = EventLeftDown;
        strike.HandleEvent(&event);
        event.Type = EventLeftUp;
    }

    strike.HandleEvent(&event);
}
