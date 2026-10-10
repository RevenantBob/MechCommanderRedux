#include "stdafx.h"
#include "logistics/MCBriefingScreen.h"
#include "gui/MCGuiFont.h"
#include "gui/MCGuiSmackerWindow.h"
#include "gui/MCScrollPane.h"
#include "gui/MCUpdateDisplay.h"
#include "lib/MCFatal.h"
#include "lib/MCFitIniFile.h"
#include "linkup/MCFidpPlayer.h"
#include "linkup/MCSessionManager.h"
#include "logistics/MCBriefingBox.h"
#include "logistics/MCLogChatWindow.h"
#include "logistics/MCLogRows.h"
#include "logistics/MCMainMenu.h"
#include "logistics/MCMechBriefBlock.h"
#include "logistics/MCPurchaseScreen.h"
#include "logistics/MCRepairScreen.h"
#include "logistics/MCTicker.h"
#include "logistics/MCUnitLimits.h"
#include "main/MCGamePaths.h"
#include "main/MCLogistics.h"
#include "main/MCGameStrings.h"
#include "mission/MCMission.h"
#include "network/MCMultiPlayer.h"
#include "platform/MCInput.h"
#include "platform/MCSmacker.h"
#include "sound/MCSoundSystem.h"
#include "vfx/MCVfxFunctions.h"

int32_t CurDeployTonnage = 0;
int32_t MaxDeployTonnage = 0;

namespace
{
    /// <summary>The colour of the drop zone markers and their leader lines on the briefing map.</summary>
    constexpr uint32_t DropZoneColor = 0x1f;

    /// <summary>The most units (mechs and vehicles) the player may own when dropping: a save holds no more (a game rule).</summary>
    constexpr int32_t MaxOwnedUnits = 50;

    /// <summary>The briefing screen's timers: the chat button's blink and the operation movie's start delay.</summary>
    constexpr int32_t ChatBlinkTimer = 5;
    constexpr int32_t MovieDelayTimer = 6;

    /// <summary>The tops of the drop slots' rows; each lance is two rows of two.</summary>
    constexpr std::array<int32_t, 6> SlotTops = {0x28, 0x58, 0x98, 0xc8, 0x108, 0x138};

    /// <summary>Puts string <paramref name="id"/> on the ticker.</summary>
    void ShowHelp(uint32_t id)
    {
        GlobalLogPtr->Ticker->SetString(LoadGameString(id, 0xfe));
    }

    /// <summary>
    /// Marks drop zone <paramref name="zone"/> on the briefing map (whose tac map picture fills (0x159, 0x1a) ..
    /// (0x273, 0x134), rotated 45 degrees): a leader line from the zone's label at height <paramref name="lineY"/> to
    /// a small cross at the zone. <paramref name="scale"/> is world units per map pixel.
    /// </summary>
    void DrawDropZoneMarker(MCPane* pane, const MCLogistics::DropZonePosition& zone, float scale, int32_t lineY)
    {
        constexpr float r = 0.70710677f;
        const float x = zone.X;
        const float yR = zone.Y * r;
        const auto diff = static_cast<float>(yR - static_cast<double>(x) * r);
        const double across = (static_cast<double>(x) * r + yR) / scale;
        const auto down = static_cast<float>(diff / scale);
        const auto screenX = static_cast<float>(across + 490.0);
        const auto screenY = static_cast<float>(141.0 - down + 31.0);

        const auto bend = static_cast<int32_t>((screenX - 351.0f) * 0.5f + 351.0f);
        VfxLineDraw(pane, 0x157, lineY, bend, lineY, DropZoneColor);
        const auto pointX = static_cast<int32_t>(screenX);
        const auto pointY = static_cast<int32_t>(screenY);
        VfxLineDraw(pane, bend, lineY, pointX, pointY, DropZoneColor);
        VfxLineDraw(pane, static_cast<int32_t>(screenX - 2.0f), pointY, static_cast<int32_t>(screenX + 2.0f), pointY,
                    DropZoneColor);
        VfxLineDraw(pane, pointX, static_cast<int32_t>(screenY - 2.0f), pointX, static_cast<int32_t>(screenY + 2.0f),
                    DropZoneColor);
        const auto top = static_cast<int32_t>(screenY - 1.0f);
        const auto left = static_cast<int32_t>(screenX - 1.0f);
        AGPixelWrite(pane, left, top, DropZoneColor);
        const auto right = static_cast<int32_t>(screenX + 1.0f);
        AGPixelWrite(pane, right, top, DropZoneColor);
        const auto bottom = static_cast<int32_t>(screenY + 1.0f);
        AGPixelWrite(pane, right, bottom, DropZoneColor);
        AGPixelWrite(pane, left, bottom, DropZoneColor);
    }

    /// <summary>
    /// Adds up the tonnage of the units in <paramref name="lance"/>'s drop slots (the original's running
    /// <c>__ftol</c> of an int plus the float tonnage).
    /// </summary>
    int32_t LanceTonnage(int32_t lance)
    {
        int32_t tons = 0;

        for (const auto& slot : GlobalLogPtr->DeploySlots[lance])
        {
            if (slot.Unit >= 0)
            {
                MCLogMech* mech = nullptr;
                GlobalLogPtr->ForceMechList->GetMechInfo(slot.Unit, mech);
                tons = static_cast<int32_t>(static_cast<float>(tons) + mech->CurTonnage);
            }
            else if (slot.Vehicle >= 0)
            {
                MCLogVehicle* vehicle = nullptr;
                GlobalLogPtr->ForceVehicleList->GetVehicleInfo(slot.Vehicle, vehicle);
                tons = static_cast<int32_t>(static_cast<float>(tons) + vehicle->CurTonnage);
            }
        }

        return tons;
    }

    /// <summary>Fills a <paramref name="width"/> by <paramref name="height"/> box of <paramref name="target"/> with <paramref name="color"/>.</summary>
    void FillArea(MCPane* target, int32_t xPos, int32_t yPos, int32_t width, int32_t height, int32_t color)
    {
        MCLogBlockPort box(target, xPos, yPos, width, height, false);
        VfxPaneWipe(box.Frame(), color);
    }

    /// <summary>Reads entry <paramref name="name"/> of the terrain file's TerrainData block; a missing one is reported.</summary>
    template <MCFitValue T> T ReadTerrainEntry(MCFitIniFile& file, std::string_view name)
    {
        const MCFitResult<T> value = file.Read<T>(name);
        Assert(value.has_value(), value.has_value() ? 0 : static_cast<uint32_t>(std::to_underlying(value.error())),
               std::format("Could not find variable {} in terrain .FIT file", name));
        return value.value_or(T{});
    }

    /// <summary>Whether (<paramref name="point"/>) is in the rectangle (<paramref name="left"/>, <paramref name="top"/>) .. (<paramref name="right"/>, <paramref name="bottom"/>), right and bottom out.</summary>
    bool Inside(POINT point, int32_t left, int32_t top, int32_t right, int32_t bottom)
    {
        const RECT area{left, top, right, bottom};
        return PtInRect(&area, point) != 0;
    }
}

auto MCBriefingScreen::FitsTonnage(const MCLogPart* part) -> bool
{
    return !(static_cast<float>(MaxDeployTonnage) < static_cast<float>(CurDeployTonnage) + part->CurTonnage);
}

auto MCBriefingScreen::Init() -> void
{
    CurrentTab = OperationTab;
    CurDeployTonnage = 0;
    BriefingBox = nullptr;
    PlayMovie = false;
    ButtonsLocked = false;
    ChatBlinking = false;
    ChatBlinkOn = false;
    ChatTimerOn = false;
    const int32_t result = MCLogObject::Init(0, 0, 0x280, 0x1e0);
    Assert(result == 0, result, "Unable to init briefing screen");
    // The original loaded the background (lsbbk00) as the screen's picture; the screen draws it each frame.
    MissionPane = MCMakeGui<MCScrollPane>();
    MissionPane->Init(0xb9, 0xdb, 7, 0x6b, std::format("{}logart\\lsbbk01.tga", ArtPath).c_str());
    MissionPane->ClearDisplayPort();
    DeployPane = MCMakeGui<MCScrollPane>();
    DeployPane->Init(0xc4, 0x7a, 7, 0x15e, static_cast<char*>(nullptr));
    MissionPort = std::make_unique<MCLogPort>();
    MissionPort->Init(0xad, 0xdd);
    VfxPaneWipe(MissionPort->Frame(), 0x10);
    ChatBlinkPort = std::make_unique<MCLogPort>();
    ChatBlinkPort->Load(std::format("{}logart\\lsbdw08.tga", ArtPath));
    ChatRegularPort = std::make_unique<MCLogPort>();
    ChatRegularPort->Load(std::format("{}logart\\lsbdw03.tga", ArtPath));
    OperationPicture.reset();
    UndeployedMechs.clear();
    AddChild(MissionPane.get());
    AddChild(DeployPane.get());
    ShowGuiWindow(false);
    ScreenWindow()->AddChild(this);

    // Two columns of six slots; each lance is two rows.
    for (size_t i = 0; i < NumDropSlots; i++)
    {
        const int32_t left = (i & 1) != 0 ? 0x115 : 0xdf;
        const int32_t top = SlotTops[i >> 1];
        SlotRects[i] = {left, top, left + 0x34, top + 0x2e};
    }

    SmackerWindow.reset();
    EmptySlot.reset();
}

auto MCBriefingScreen::DrawBackground() -> void
{
    // Port: the screen is drawn each frame (PaintLook) from its state; the slots that can't be filled show covered.
    MCLogPort* back = LogScreenArt("lsbbk00.tga");
    ClearLook();
    ScreenChrome.Clear();

    if (EmptySlot == nullptr && back != nullptr)
    {
        EmptySlot = std::make_unique<MCLogPort>();
        EmptySlot->Init(0x34, 0x2e);
        VfxPaneWipe(EmptySlot->Frame(), 0xff);
        VfxPaneCopy(back->Frame(), SlotRects[0].left, SlotRects[0].top, EmptySlot->Frame(), 0, 0, -1);
    }

    // The mission's tac map picture, turned 45 degrees onto the map area.
    MCFitIniFile terrainFile;
    int32_t result = terrainFile.Open(GamePath(TerrainPath, GlobalLogPtr->MissionFileName, ".fit"));
    Assert(result == 0, result, "Could not find terrain file from .TGA ");
    result = terrainFile.SeekBlock("TerrainData");
    Assert(result == 0, result, "Could not find TerrainData block in terrain .FIT file");
    const auto verticesBlockSide = ReadTerrainEntry<int32_t>(terrainFile, "VerticesBlockSide");
    const auto blocksMapSide = ReadTerrainEntry<int32_t>(terrainFile, "BlocksMapSide");
    const auto metersPerVertex = ReadTerrainEntry<float>(terrainFile, "MetersPerVertex");
    const int32_t mapSide = blocksMapSide * verticesBlockSide;
    terrainFile.Close();
    MapPicture = std::make_unique<MCLogPort>();
    // A map picture that can't be loaded is fatal.
    MapPicture->Load(GamePath(TerrainPath, GlobalLogPtr->MissionFileName, ".log.tga"));

    if (MultiPlayer() != nullptr)
    {
        // Each lance's label has a line to its drop zone on the map.
        const auto side = static_cast<double>(mapSide);
        MarkerScale = static_cast<float>(std::sqrt(side * side + side * side) * metersPerVertex *
                                         static_cast<double>(0.0017667845f));
        MarkedZone = MultiPlayer()->HomeTeam == 1 ? 3 : 0;
    }

    SetUpMission();
}

auto MCBriefingScreen::ClearLook() -> void
{
    MarkedZone = -1;
    LaunchPressed = false;
    OperationShown = false;
    OperationPictureShown = false;
    SlotBlocks.fill(nullptr);
    BoxShown = nullptr;
}

auto MCBriefingScreen::SlotAt(int32_t xPos, int32_t yPos) const -> int32_t
{
    for (size_t i = 0; i < NumDropSlots; i++)
    {
        if (SlotRects[i].left == xPos && SlotRects[i].top == yPos)
        {
            return static_cast<int32_t>(i);
        }
    }

    return -1;
}

auto MCBriefingScreen::PlaceInSlot(MCMechBriefBlock* block) -> void
{
    const int32_t slot = SlotAt(block->X(), block->Y());

    if (slot < 0)
    {
        return;
    }

    // A block left in any other slot has moved here.
    LiftFromSlot(block);
    SlotBlocks[static_cast<size_t>(slot)] = block;
}

auto MCBriefingScreen::LiftFromSlot(MCMechBriefBlock* block) -> void
{
    std::ranges::replace(SlotBlocks, block, nullptr);
}

auto MCBriefingScreen::ShowBox(MCBriefingBox* box) -> void
{
    BoxShown = box;
}

auto MCBriefingScreen::BlankBox() -> void
{
    BoxShown = nullptr;
}

auto MCBriefingScreen::RemoveChild(MCGuiObject* child) -> void
{
    // OB-133 (fixed): the original left a removed block's or box's picture where it was, until something painted over
    // it; the slot shows empty again, the box area blank.
    if (child != nullptr)
    {
        std::ranges::replace_if(SlotBlocks, [child](const MCGuiObject* block) { return block == child; }, nullptr);

        if (child == BoxShown)
        {
            BoxShown = nullptr;
        }
    }

    MCLogObject::RemoveChild(child);
}

auto MCBriefingScreen::LanceTons(int32_t lance) -> int32_t
{
    if (MultiPlayer() == nullptr)
    {
        return LanceTonnage(lance);
    }

    // Multiplayer: a lance counts once one of its slots holds a unit (the second lance always with two players).
    bool occupied = lance == 1 && static_cast<int32_t>(MultiPlayer()->PlayersOnHomeTeam()->size()) == 2;

    for (int32_t slot = 0; slot < 4 && !occupied; slot++)
    {
        const auto& deploy = GlobalLogPtr->DeploySlots[lance][slot];

        if (deploy.Unit >= 0)
        {
            MCLogMech* mech = nullptr;
            GlobalLogPtr->ForceMechList->GetMechInfo(deploy.Unit, mech);
            occupied = mech != nullptr;
        }
        else if (deploy.Vehicle >= 0)
        {
            // Original behaviour (OB-082): the vehicle looked up is lance 0's, whatever the lance.
            MCLogVehicle* vehicle = nullptr;
            GlobalLogPtr->ForceVehicleList->GetVehicleInfo(GlobalLogPtr->DeploySlots[0][slot].Vehicle, vehicle);
            occupied = vehicle != nullptr;
        }
    }

    return occupied ? LanceTonnage(lance) : 0;
}

auto MCBriefingScreen::PaintLook(MCPane* target) -> void
{
    if (MCLogPort* back = LogScreenArt("lsbbk00.tga"))
    {
        VfxPaneCopy(back->Frame(), 0, 0, target, 0, 0, -1);
    }

    if (MapPicture != nullptr)
    {
        MCWindow* map = MapPicture->Frame()->Window;
        std::array<MCScreenVertex, 4> vertices = {};
        vertices[0] = {0x159, 0x1a, 0, 0, 0, 0};
        vertices[1] = {0x273, 0x1a, 0, map->XMax << 16, 0, 0};
        vertices[2] = {0x273, 0x134, 0, map->XMax << 16, map->YMax << 16, 0};
        vertices[3] = {0x159, 0x134, 0, 0, map->YMax << 16, 0};
        VfxMapPolygon(target, vertices, map, VfxMapTransparent);
    }

    // The lance labels, with their tonnage: the first lance's always, the others in multiplayer or when the mission
    // gives the player their slots. (The original measured the first lance's figure in the black font and the
    // others' in the blue one, and wrote them all in black.)
    static constexpr std::array<int32_t, 3> labelTops = {0x18, 0x88, 0xf8};
    const std::array<bool, MCLogistics::NumDropSlots>& local = GlobalLogPtr->LocalDropSlot;
    const std::array<bool, 3> lanceShown = {true, MultiPlayer() != nullptr || local[4],
                                            MultiPlayer() != nullptr || (local[4] && local[8])};

    for (int32_t lance = 0; lance < 3; lance++)
    {
        if (!lanceShown[static_cast<size_t>(lance)])
        {
            continue;
        }

        const int32_t top = labelTops[static_cast<size_t>(lance)];

        if (MCLogPort* art = LogScreenArt(std::format("lsbdf0{}.tga", lance + 2)))
        {
            art->CopyTo(target, 0xd6, top, true);
        }

        const std::string text = std::to_string(LanceTons(lance));
        const int32_t textWidth =
            lance == 0 && MultiPlayer() == nullptr ? BlackFont->Width(text.c_str()) : BlueFont->Width(text.c_str());
        BlackFont->WriteString(target, 0x13f - textWidth, top + 4, text.c_str(), -1);
    }

    if (MarkedZone >= 0)
    {
        static constexpr std::array<int32_t, 3> markerLines = {0x24, 0x94, 0x104};

        for (int32_t zone = 0; zone < 3; zone++)
        {
            DrawDropZoneMarker(target, GlobalLogPtr->DropZonePositions[MarkedZone + zone], MarkerScale,
                               markerLines[static_cast<size_t>(zone)]);
        }
    }

    // The tonnage bar and the launch button: pressed while clicked or locked (multiplayer launch), lit when the force
    // can drop.
    PaintTonnageBar(target, MaxDeployTonnage, CurDeployTonnage, GlobalLogPtr->HammerDown != 0);
    std::string_view launchName = "lsbdf01.tga";

    if (!LaunchPressed && !ButtonsLocked)
    {
        bool ready = CurDeployTonnage >= 1;

        if (ready && (MaxDeployTonnage < CurDeployTonnage || GlobalLogPtr->RequiredAssigned() == 0))
        {
            ready = GlobalLogPtr->HammerDown != 0;
        }

        launchName = ready ? "lsbdf00.tga" : "lsbdf00a.tga";
    }

    if (MCLogPort* art = LogScreenArt(launchName))
    {
        art->CopyTo(target, 0x20d, 0x148, true);
    }

    // The tab, and over it the chat button while it blinks (multiplayer).
    if (CurrentTab == OperationTab || CurrentTab == MissionTab)
    {
        const std::string_view tabName = CurrentTab == OperationTab
                                             ? (MultiPlayer() == nullptr ? "lsbdw00.tga" : "lsbdw02.tga")
                                             : (MultiPlayer() == nullptr ? "lsbdw01.tga" : "lsbdw03.tga");

        if (MCLogPort* art = LogScreenArt(tabName))
        {
            art->CopyTo(target, 0xc4, 0x65, false);
        }
    }

    if (ChatTimerOn)
    {
        if (ChatBlinkOn)
        {
            ChatBlinkPort->CopyTo(target, 0xc5, 0x65, true);
        }
        else
        {
            ChatRegularPort->CopyTo(target, 0xc4, 0x65, true);
        }
    }

    // The operation area: its art, the operation picture over it before the movie, the closing art after the movie.
    if (OperationShown)
    {
        if (MCLogPort* art = LogScreenArt("lsb_op0.tga"))
        {
            art->CopyTo(target, 0xc, 0x6f, true);
        }
    }

    if (OperationPictureShown && OperationPicture != nullptr)
    {
        OperationPicture->CopyTo(target, 0xc, 0x6f, true);
    }

    if (MovieOver != 0 && InDemo == 0)
    {
        if (MCLogPort* art = LogScreenArt("lsb_op6.tga"))
        {
            art->CopyTo(target, 0xc, 0x6f, true);
        }
    }

    // The drop slots: covered when the player can't fill them, else blank (colour 0x10 inside the slot's border, as
    // the screen's setup painted them), and the unit placed in it.
    for (size_t i = 0; i < NumDropSlots; i++)
    {
        const RECT& area = SlotRects[i];

        if (GlobalLogPtr->LocalDropSlot[i] == 0)
        {
            if (MCLogPort* art = LogScreenArt("lsbdf06.tga"))
            {
                art->CopyTo(target, area.left + 1, area.top + 1, true);
            }
        }
        else
        {
            FillArea(target, area.left, area.top, 0x32, 0x2c, 0x10);
        }

        if (SlotBlocks[i] != nullptr)
        {
            SlotBlocks[i]->PaintBlock(target, area.left, area.top, true);
        }
    }

    // The briefing box area: the box shown, or blank.
    if (BoxShown != nullptr)
    {
        BoxShown->PaintBox(target, 0xd3, 0x16f);
    }
    else
    {
        FillArea(target, 0xd3, 0x16f, 0x1aa, 0x6e, 0x10);
    }
}

auto MCBriefingScreen::NewLookPicture() -> std::unique_ptr<MCLogPort>
{
    auto picture = std::make_unique<MCLogPort>();
    picture->Init(Width(), Height());
    PaintLook(picture->Frame());
    GlobalLogPtr->DrawScreenChrome(this, picture->Frame());
    return picture;
}

auto MCBriefingScreen::Display() -> void
{
    if (ShowWindow == 0)
    {
        return;
    }

    MCLogObject::Display();

    if (MovieOver == 0)
    {
        if (GlobalLogPtr->AutoPlayMovie != 0)
        {
            if (MovieStarted)
            {
                SetUpOperation();
                return;
            }

            MovieStarted = true;
        }
    }
    else if (InDemo == 0)
    {
        // The movie is over: its window goes, and the closing art shows (PaintLook).
        PlayMovie = false;
        RemoveChild(SmackerWindow.get());
    }
}

auto MCBriefingScreen::Draw() -> void
{
    // The original drew nothing here; the screen now draws its picture and the shared places in the frame pass.
    if (Lport()->ViewOpen())
    {
        PaintLook(Lport()->Frame());
        GlobalLogPtr->DrawScreenChrome(this, Lport()->Frame());
    }
}

auto MCBriefingScreen::Destroy() -> void
{
    StopSmackerMovies();
    ScreenWindow()->RemoveChild(this);
    ClearLook();
    MapPicture.reset();
    ChatBlinkPort.reset();
    ChatRegularPort.reset();
    EmptySlot.reset();
    UndeployedMechs.clear();
    MissionPort.reset();
    SmackerWindow.reset();
    DeployPane.reset();

    if (MissionPane != nullptr)
    {
        MissionPane->ClearDisplayPort();
        MissionPane.reset();
    }

    OperationPicture.reset();
    MCLogObject::Destroy();
}

auto MCBriefingScreen::MpCalcTonnages() -> void
{
    // A lance counts once one of its slots holds a unit (LanceTons); the labels show each (PaintLook).
    CurDeployTonnage = 0;

    for (int32_t lance = 0; lance < 3; lance++)
    {
        CurDeployTonnage += LanceTons(lance);
    }

    DrawTonnageBar();
}

auto MCBriefingScreen::CalcTonnages() -> void
{
    if (MultiPlayer() != nullptr)
    {
        MpCalcTonnages();
        return;
    }

    // The labels show each lance's tonnage (PaintLook).
    CurDeployTonnage = LanceTons(0);

    if (GlobalLogPtr->LocalDropSlot[4] != 0)
    {
        CurDeployTonnage += LanceTons(1);

        if (GlobalLogPtr->LocalDropSlot[8] != 0)
        {
            CurDeployTonnage += LanceTons(2);
        }
    }

    DrawTonnageBar();
}

auto MCBriefingScreen::DrawTonnageBar() -> void
{
    // Port: the bar and the launch button are drawn each frame (PaintLook) with the figures of now; a new figure ends
    // a press shown on the launch button (the original painted it up again).
    if (!ButtonsLocked)
    {
        LaunchPressed = false;
    }
}

auto MCBriefingScreen::PaintTonnageBar(MCPane* target, int32_t maxTons, int32_t tons, bool hammerDown) -> void
{
    if (MCLogPort* art = LogScreenArt("lsbdf07.tga"))
    {
        art->CopyTo(target, 0x157, 0x13f, false);
    }

    std::string text = std::to_string(maxTons);
    int32_t width = BlueFont->Width(text.c_str());
    MCGuiFont* font = !hammerDown ? YellowDropFont : RedFont;
    font->WriteString(target, 0x1d9 - width, 0x13f, text.c_str(), -1);
    text = std::to_string(tons);

    if (maxTons < tons)
    {
        width = RedFont->Width(text.c_str());
        font = RedFont;
    }
    else
    {
        width = BlueFont->Width(text.c_str());
        font = YellowDropFont;
    }

    font->WriteString(target, 0x1d9 - width, 0x149, text.c_str(), -1);

    // Port fix: with no limit the original's bar width was the x87 integer indefinite (a negative port width).
    int32_t barWidth = 0;

    if (maxTons != 0)
    {
        barWidth = static_cast<int32_t>(static_cast<double>(tons) / maxTons * 149.0);
    }

    if (barWidth != 0)
    {
        barWidth = std::min(barWidth, 0x95);
        MCLogBlockPort bar(target, 0x15b, 0x155, barWidth, 0xe, true);

        if (MCLogPort* art = LogScreenArt("lsbdf05.tga"))
        {
            art->CopyTo(bar.Frame(), 0, 0, false);
        }
    }
}

auto MCBriefingScreen::ShowHoverHelp(int32_t xPos, int32_t yPos) -> void
{
    GuiSystem()->SetCurrentCursor(static_cast<MCCursorType>(0));
    GlobalLogPtr->DrawScreenButtons();
    const POINT point{xPos, yPos};

    // The help for the drop slot under the mouse (empty or filled), if any.
    auto slotHelp = [&point, this]() -> std::optional<uint32_t>
    {
        for (int32_t lance = 0; lance < 3; lance++)
        {
            for (int32_t slot = 0; slot < 4; slot++)
            {
                if (GlobalLogPtr->LocalDropSlot[lance * 4 + slot] == 0)
                {
                    break;
                }

                if (PtInRect(&SlotRects[static_cast<size_t>(lance * 4 + slot)], point) != 0)
                {
                    const auto& deploy = GlobalLogPtr->DeploySlots[lance][slot];
                    return deploy.Unit < 0 && deploy.Vehicle < 0 ? 0x20u : 0x21u;
                }
            }
        }

        return std::nullopt;
    };

    if (Inside(point, 2, 2, 0xd0, 0xd))
    {
        ShowHelp(0x1d);
    }
    else if (Inside(point, 2, 0x10, 0xd0, 0x21))
    {
        ShowHelp(0x286);
        MCLogistics::HoverScreenButton(this, 0);
    }
    else if (Inside(point, 2, 0x22, 0xd0, 0x33))
    {
        ShowHelp(0x1e);
    }
    else if (Inside(point, 2, 0x34, 0xd0, 0x45))
    {
        ShowHelp(0x41);

        if (!ButtonsLocked)
        {
            MCLogistics::HoverScreenButton(this, 2);
        }
    }
    else if (Inside(point, 2, 0x46, 0xd0, 0x57))
    {
        ShowHelp(0x42);

        if (!ButtonsLocked)
        {
            MCLogistics::HoverScreenButton(this, 3);
        }
    }
    else if (Inside(point, 0x20c, 2, 0x24d, 0xd))
    {
        ShowHelp(0x1f);
    }
    else if (const std::optional<uint32_t> slotText =
                 Inside(point, 0xde, 0x1a, 0x14a, 0x164) ? slotHelp() : std::nullopt)
    {
        ShowHelp(*slotText);
    }
    else if (Inside(point, 0x20d, 0x148, 0x26a, 0x15a))
    {
        ShowHelp(0x22);
    }
    else if (Inside(point, 0xd3, 0x16f, 0x27f, 0x1df))
    {
        ShowHelp(0x23);
    }
    else if (Inside(point, 7, 0x15d, 0xcb, 0x1d8))
    {
        ShowHelp(0x24);
    }
    else if (Inside(point, 0xc2, 0x67, 0xcf, 0xde))
    {
        ShowHelp(CurrentTab == OperationTab ? 0x26 : 0x27);
    }
    else if (Inside(point, 0xc2, 0xe0, 0xcf, 0x14a))
    {
        if (CurrentTab != MissionTab)
        {
            ShowHelp(0x28);
        }
    }
    else
    {
        GlobalLogPtr->Ticker->SetString({});
    }
}

auto MCBriefingScreen::HandleClick(int32_t xPos, int32_t yPos) -> void
{
    const POINT point{xPos, yPos};

    if (!ButtonsLocked && Inside(point, 2, 0x34, 0xd1, 0x45))
    {
        GlobalLogPtr->SetUpPurchaseScreen(true);
        return;
    }

    if (!ButtonsLocked && Inside(point, 2, 0x46, 0xd1, 0x57))
    {
        GlobalLogPtr->SetUpRepairScreen(true);
        return;
    }

    if (Inside(point, 2, 0x10, 0xd1, 0x21))
    {
        if (MultiPlayer() == nullptr)
        {
            StopSmackerMovies();
            SoundSystem()->PlayDigitalSample(0x36, 1, nullptr, false, false);
            GlobalLogPtr->SetUpMainScreen(false);
            return;
        }

        CheckExit();
        return;
    }

    // The tabs.
    if (Inside(point, 0xc2, 0x67, 0xd1, 0xde))
    {
        SoundSystem()->PlayDigitalSample(0x36, 1, nullptr, false, false);
        SetUpOperation();
        return;
    }

    if (Inside(point, 0xc2, 0xdf, 0xd1, 0x14a))
    {
        SoundSystem()->PlayDigitalSample(0x36, 1, nullptr, false, false);
        SetUpMission();
        return;
    }

    if (!ButtonsLocked && Inside(point, 0x205, 0x14a, 0x270, 0x15c))
    {
        Launch();
    }
}

auto MCBriefingScreen::Launch() -> void
{
    if (CurDeployTonnage == 0 || ((MaxDeployTonnage < CurDeployTonnage || GlobalLogPtr->RequiredAssigned() == 0) &&
                                  GlobalLogPtr->HammerDown == 0))
    {
        SoundSystem()->PlayBettySample(0x1b);
        return;
    }

    LaunchPressed = true;
    UpdateDisplay(false, false, 0, false, 0);

    if (MultiPlayer() == nullptr)
    {
        // Everything the player owns must fit the save.
        const int32_t units = GlobalLogPtr->ForceVehicleList->GetVehicleCount() +
                              GlobalLogPtr->ForceMechList->GetMechCount() +
                              GlobalLogPtr->VehicleList->GetVehicleCount() + GlobalLogPtr->MechList->GetMechCount();

        if (units <= MaxOwnedUnits)
        {
            SoundSystem()->PlayDigitalSample(0x3a, 1, nullptr, false, false);
            Mission()->StartScenario(Mission()->Scenarios[Mission()->CurrentScenario].data());
            return;
        }

        SoundSystem()->PlayDigitalSample(0x33, 1, nullptr, false, false);
        ShowLogMessage(
            MCFormatPrintf(LoadGameString(0x373, 0xfe).c_str(), units, MaxOwnedUnits, units - MaxOwnedUnits));
        return;
    }

    ButtonsLocked = true;
    MultiPlayer()->SendReadyForBattle();
    std::string text =
        MCFormatPrintf(LoadGameString(0x379, 199).c_str(), MultiPlayer()->SessionManager->MyPlayer->Name.c_str());
    // The original's 304-byte buffer.
    text.resize(std::min<size_t>(text.size(), 303));
    MultiPlayer()->SendChat(0, text.data());
}

auto MCBriefingScreen::HandleEvent(MCGuiEvent* event) -> void
{
    if (GlobalLogPtr->CurrentScreen != this)
    {
        return;
    }

    if (event->Key == 0 && event->Type != 0x13)
    {
        ShowHoverHelp(event->X, event->Y);
    }

    if (event->Type == 9)
    {
        // Ctrl+Alt+= : a thousand resource points.
        const bool ctrlAlt = MCInput::GetAsyncKeyState(VK_CONTROL) != 0 && MCInput::GetAsyncKeyState(VK_MENU) != 0;

        if (MultiPlayer() == nullptr && CheatsOn != 0 && event->Key == 0xbb && ctrlAlt)
        {
            ResourcePoints += 1000;
        }
    }

    switch (event->Type)
    {
        case 1:
        {
            HandleClick(event->X - GlobalX(), event->Y - GlobalY());
            return;
        }
        case 4:
        {
            // The launch button shows up again once let go (unless a multiplayer launch locked it).
            LaunchPressed = false;
            return;
        }
        case 8:
        {
            if (event->Key == 0x1b && MovieOver == 0)
            {
                StopSmackerMovies();
                return;
            }

            GlobalLogPtr->ProcessCheatCode(event->ScanCode);
            return;
        }
        case 0x13:
        {
            if (event->Data == MovieDelayTimer)
            {
                // The operation movie's start delay.
                PlayMovie = true;
                GuiSystem()->RemoveTimer(this, MovieDelayTimer);
                SetUpOperation();
                return;
            }

            if (event->Data == ChatBlinkTimer)
            {
                // The chat button blinks (PaintLook draws it).
                ChatBlinkOn = !ChatBlinkOn;
            }
            break;
        }
        default:
            break;
    }
}

auto MCBriefingScreen::ShowGuiWindow(bool show) -> void
{
    const bool paneShown = MovieOver != 0;
    ShowWindow = show;

    if (paneShown)
    {
        MissionPane->ShowGuiWindow(show);
    }

    DeployPane->ShowGuiWindow(show);
}

auto MCBriefingScreen::SetUpOperation() -> void
{
    GlobalLogPtr->AutoPlayMovie = false;
    MovieOver = 0;

    if (MultiPlayer() == nullptr)
    {
        OperationShown = true;

        if (!PlayMovie)
        {
            GuiSystem()->AddTimer(this, MovieDelayTimer, 500, 0, 0, false);
        }
    }
    else
    {
        // Multiplayer: the tab is the chat, which stops every chat button blinking.
        MovieOver = -1;

        if (ChatBlinking)
        {
            GuiSystem()->RemoveTimer(this, ChatBlinkTimer);
            ChatTimerOn = false;
            ChatBlinking = false;
        }

        if (GlobalLogPtr->PurchaseScreen->ChatBlinking)
        {
            GuiSystem()->RemoveTimer(GlobalLogPtr->PurchaseScreen.get(), 7);
            GlobalLogPtr->PurchaseScreen->ChatBlinking = false;
        }

        if (GlobalLogPtr->RepairScreen->ChatBlinking)
        {
            GuiSystem()->RemoveTimer(GlobalLogPtr->RepairScreen.get(), 8);
            GlobalLogPtr->RepairScreen->ChatBlinking = false;
        }

        ChatBlinkOn = false;
        GlobalLogPtr->ChatWindow->ShowGuiWindow(true);
    }

    MissionPane->ShowGuiWindow(false);
    StopSmackerMovies();
    CurrentTab = OperationTab;

    if (!PlayMovie || MultiPlayer() != nullptr)
    {
        return;
    }

    if (OperationPicture != nullptr)
    {
        OperationPictureShown = true;
        SoundSystem()->PlayBettySample(0x1a);

        while (SoundSystem()->IsChannelPlaying(0xe) != 0)
        {
            UpdateDisplay(false, false, 0, false, 0);
        }

        if (MultiPlayer() != nullptr)
        {
            return;
        }
    }

    if (!GlobalLogPtr->OperationCinema.empty())
    {
        std::unique_ptr<MCSmackTag> movie =
            SmackOpen(GamePath(MoviePath, GlobalLogPtr->OperationCinema, ".smk").c_str(), 0xfe000, -1);

        // Port fix: a missing movie is skipped (the original read the null handle's size).
        if (movie == nullptr)
        {
            return;
        }

        SmackerWindow = MCMakeGui<MCGuiSmackerWindow>();
        const RECT area{0xc, 0x6f, movie->Player->Width(), movie->Player->Height()};
        SmackerWindow->Init(area, nullptr);
        AddChild(SmackerWindow.get());
        SmackerWindow->StartSmackerMovie(std::move(movie), false);
        SmackerWindow->Draw();
    }
}

auto MCBriefingScreen::SetUpMission() -> void
{
    // The tab's art is drawn from currentTab (PaintLook).
    CurrentTab = MissionTab;

    if (MultiPlayer() == nullptr)
    {
        StopSmackerMovies();
    }
    else
    {
        GlobalLogPtr->ChatWindow->ShowGuiWindow(false);
    }

    MissionPane->SetDisplayPort(MissionPort.get(), false);
    MissionPane->ShowGuiWindow(true);
}

auto MCBriefingScreen::StopSmackerMovies() -> void
{
    if (SmackerWindow == nullptr)
    {
        return;
    }

    RemoveChild(SmackerWindow.get());
    SmackerWindow->EndSmackerMovie();
    SmackerWindow.reset();
    PlayMovie = false;
}

auto MCBriefingScreen::SetUpDeploy() -> void
{
    MCScrollPane* pane = DeployPane.get();

    for (int32_t count = pane->NumberOfChildren(); count > 0; count--)
    {
        pane->RemoveChild(pane->Child(0));
    }

    // Blocks of units still waiting are rebuilt; placed units keep theirs (redrawn).
    UndeployedMechs.clear();
    int32_t index = 0;

    for (const std::unique_ptr<MCLogMech>& mech : GlobalLogPtr->ForceMechList->Mechs)
    {
        const int32_t mechIndex = index++;

        if (mech->PilotIndex < 0)
        {
            continue;
        }

        if (!mech->Deployed)
        {
            MCMechBriefBlock::Discard(mech->BriefBlock);
            UndeployedMechs.push_back(mechIndex);
        }
        else
        {
            mech->BriefBlock->DrawBackground();
        }
    }

    for (const std::unique_ptr<MCLogVehicle>& vehicle : GlobalLogPtr->ForceVehicleList->Vehicles)
    {
        if (vehicle->Deployed == 0)
        {
            MCMechBriefBlock::Discard(vehicle->BriefBlock);
        }
        else
        {
            vehicle->BriefBlock->DrawBackground();
        }
    }

    // Three blocks a row.
    const auto mechCount = static_cast<int32_t>(UndeployedMechs.size());
    const int32_t blocks = GlobalLogPtr->ForceVehicleList->GetVehicleCount() + mechCount;
    int32_t height = pane->Height();

    if (blocks >= 7)
    {
        height = (blocks / 3 + 1) * 0x34;

        if (blocks % 3 == 0)
        {
            height -= 0x34;
        }
    }

    // Port: the pane's blocks are drawn into it each frame (the original painted each into this picture).
    auto port = std::make_unique<MCLogPort>();
    port->InitView(0xc5, height);
    port->DrawContent = [pane](MCGuiPort* view)
    {
        VfxPaneWipe(view->Frame(), 0x10);
        const int32_t scroll = pane->GetScrollOffset();

        for (int32_t i = 0; i < pane->NumberOfChildren(); i++)
        {
            auto* brief = static_cast<MCMechBriefBlock*>(pane->Child(i));
            brief->PaintBlock(view->Frame(), brief->X(), brief->Y() + scroll, false);
        }
    };

    const int32_t sliderPos = pane->SliderPos;
    pane->SetDisplayPort(std::move(port), true);
    int32_t block = 0;

    // The blocks' places: three a row.
    auto blockX = [&block]() { return (block % 3) * 0x38 + 4; };
    auto blockY = [&block]() { return (block / 3) * 0x34 + 3; };

    for (; block < mechCount; block++)
    {
        MCLogMech* mech = nullptr;
        GlobalLogPtr->ForceMechList->GetMechInfo(UndeployedMechs[static_cast<size_t>(block)], mech);
        MCMechBriefBlock::Create(mech, pane, blockX(), blockY());
    }

    for (const std::unique_ptr<MCLogVehicle>& vehicle : GlobalLogPtr->ForceVehicleList->Vehicles)
    {
        if (vehicle->Deployed != 0)
        {
            continue;
        }

        MCMechBriefBlock::Create(vehicle.get(), pane, blockX(), blockY());
        ++block;
    }

    pane->SetSliderPos(sliderPos);
}
