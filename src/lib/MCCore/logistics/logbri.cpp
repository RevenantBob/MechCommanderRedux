#include "stdafx.h"
#include "logistics/logbri.h"
#include "gui/afont.h"
#include "gui/awindow.h"
#include "gui/scrlpane.h"
#include "gui/updisp.h"
#include "lib/MCFatal.h"
#include "lib/MCIDString.h"
#include "lib/MCFitIniFile.h"
#include "linkup/dpplayer.h"
#include "linkup/sessionmanager.h"
#include "logistics/invblock.h"
#include "logistics/logdlg.h"
#include "logistics/logmain.h"
#include "logistics/logpur.h"
#include "logistics/logrep.h"
#include "logistics/mrblock.h"
#include "logistics/purchase.h"
#include "logistics/ticker.h"
#include "main/logistics.h"
#include "main/main.h"
#include "mission/mission.h"
#include "network/multplyr.h"
#include "object/MCMasterComponent.h"
#include "platform/MCInput.h"
#include "platform/MCSmacker.h"
#include "sound/MCSoundSystem.h"
#include "vfx/MCVfxFunctions.h"

char ArtPath[80] = {}; // 80 bytes (0x007ab080..0x007ab0d0); gui\asystem.cpp's RealWinMain sets it.
int32_t CurDeployTonnage = 0;
int32_t MaxDeployTonnage = 0;

namespace
{
    /// <summary>The colour of the drop zone markers and their leader lines on the briefing map.</summary>
    constexpr uint32_t DropZoneColor = 0x1f;

    /// <summary>Set while the left button is down on the briefing screen (0x008080b0); only written.</summary>
    int32_t MouseDown = 0;

    /// <summary>Set while a unit block is dragged with the left button (0x008080b4).</summary>
    int32_t LeftDragging = 0;
    /// <summary>Set while a unit block is dragged with the right button (0x008080b8).</summary>
    int32_t RightDragging = 0;
    /// <summary>The drag icon's position (0x008080bc / 0x008080c0).</summary>
    int32_t DragX = 0;
    int32_t DragY = 0;
    /// <summary>Set when the dragged block was picked up from a drop slot, not the deploy pane (0x008080c4).</summary>
    int32_t DraggedFromSlot = 0;

    /// <summary>Puts string <paramref name="id"/> on the ticker.</summary>
    void ShowHelp(uint32_t id)
    {
        char text[256];
        CLoadString(ThisInstance, id, text, 0xfe);
        GlobalLogPtr->Ticker->SetString(text);
    }

    /// <summary>Shows <paramref name="text"/> in the one-button message dialog.</summary>
    void ShowMessage(char* text)
    {
        GlobalLogPtr->MessageDialog->SetText(text);
        GlobalLogPtr->MessageDialog->SetTwoButton(0);
        MCReusableDialog* dialog = GlobalLogPtr->MessageDialog;
        dialog->Callback = nullptr;
        char upArt[] = "bh_okay.tga";
        char downArt[] = "bg_okay.tga";
        dialog->OkButton->SetUpPicture(upArt);
        GlobalLogPtr->MessageDialog->OkButton->SetDownPicture(downArt);
        MCLogDialogButton* button = GlobalLogPtr->MessageDialog->OkButton;
        button->Disabled = 0;
        GlobalLogPtr->MessageDialog->Activate();
    }

    /// <summary>Shows string <paramref name="id"/> in the one-button message dialog.</summary>
    void ShowMessage(uint32_t id)
    {
        char text[256];
        CLoadString(ThisInstance, id, text, 0xfe);
        ShowMessage(text);
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

    /// <summary>The unit's tonnage added to (or, with <paramref name="sign"/> -1, taken from) <c>curDeployTonnage</c>.</summary>
    int32_t DeployTonnageWith(const MCLogPart* part, float sign)
    {
        return static_cast<int32_t>(static_cast<float>(CurDeployTonnage) + sign * part->CurTonnage);
    }

    /// <summary>Fills a <paramref name="width"/> by <paramref name="height"/> box of <paramref name="target"/> with <paramref name="color"/>.</summary>
    void FillBox(MCPane* target, int32_t xPos, int32_t yPos, int32_t width, int32_t height, int32_t color)
    {
        MCLogBlockPort box(target, xPos, yPos, width, height, false);
        VfxPaneWipe(box.Frame(), color);
    }

    /// <summary>Whether <paramref name="part"/> fits under the drop tonnage limit.</summary>
    bool FitsTonnage(const MCLogPart* part)
    {
        return !(static_cast<float>(MaxDeployTonnage) < static_cast<float>(CurDeployTonnage) + part->CurTonnage);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// BriefingScreen
// ---------------------------------------------------------------------------------------------------------------------

auto MCBriefingScreen::Init() -> void
{
    CurrentTab = 1;
    CurDeployTonnage = 0;
    BriefingBox = nullptr;
    PlayMovie = 0;
    ButtonsLocked = 0;
    ChatBlinking = 0;
    ChatBlinkOn = 0;
    ChatTimerOn = 0;
    int32_t result = MCLogObject::Init(0, 0, 0x280, 0x1e0, nullptr, nullptr);
    Assert(result == 0, result, "Unable to init briefing screen");
    // The original loaded the background (lsbbk00) as the screen's picture; the screen draws it each frame.
    char fileName[256];

    auto* pane = new MCScrollPane;

    if (pane != nullptr)
    {
        pane->Init();
    }

    MissionPane = pane;
    Assert(pane != nullptr, 0, " Not enough memory for missionScroll ");
    std::snprintf(fileName, sizeof(fileName), "%slogart\\lsbbk01.tga", ArtPath);
    pane->Init(0xb9, 0xdb, 7, 0x6b, fileName);
    pane->SetDisplayPort(nullptr, -1, -1);

    auto* deploy = new MCScrollPane;

    if (deploy != nullptr)
    {
        deploy->Init();
    }

    DeployPane = deploy;
    Assert(deploy != nullptr, 0, " Not enough memory for deployScroll ");
    deploy->Init(0xc4, 0x7a, 7, 0x15e, static_cast<char*>(nullptr));

    MissionPort = new MCLogPort;
    Assert(MissionPort != nullptr, 0, " Not enough memory for missionPort ");
    MissionPort->Init(0xad, 0xdd, -1);
    VfxPaneWipe(MissionPort->Frame(), 0x10);

    char artName[256];
    ChatBlinkPort = new MCLogPort;
    Assert(ChatBlinkPort != nullptr, 0, " Not enough memory for chatBlinker ");
    std::snprintf(artName, sizeof(artName), "%slogart\\lsbdw08.tga", ArtPath);
    ChatBlinkPort->Init(artName);
    ChatRegularPort = new MCLogPort;
    Assert(ChatRegularPort != nullptr, 0, " Not enough memory for chatRegular ");
    std::snprintf(artName, sizeof(artName), "%slogart\\lsbdw03.tga", ArtPath);
    ChatRegularPort->Init(artName);

    OperationPicture = nullptr;
    UndeployedMechs = nullptr;
    AddChild(pane);
    AddChild(DeployPane);
    ShowGuiWindow(0);
    ScreenWindow->AddChild(this);

    // Two columns of six slots; each lance is two rows.
    static constexpr int32_t slotTops[6] = {0x28, 0x58, 0x98, 0xc8, 0x108, 0x138};

    for (int32_t i = 0; i < 12; i++)
    {
        const int32_t left = (i & 1) != 0 ? 0x115 : 0xdf;
        const int32_t top = slotTops[i >> 1];
        SlotRects[i] = {left, top, left + 0x34, top + 0x2e};
    }

    Smacker = nullptr;
    SmackerWindow = nullptr;
    EmptySlot = nullptr;
}

auto MCBriefingScreen::DrawBackground() -> void
{
    // Port: the screen is drawn each frame (PaintLook) from its state; the slots that can't be filled show covered.
    MCLogPort* back = LogArtf("%slogart\\lsbbk00.tga", ArtPath);
    ClearLook();
    ScreenChrome.Clear();

    if (EmptySlot == nullptr && back != nullptr)
    {
        EmptySlot = new MCLogPort;
        EmptySlot->Init(0x34, 0x2e, -1);
        VfxPaneWipe(EmptySlot->Frame(), 0xff);
        VfxPaneCopy(back->Frame(), SlotRects[0].left, SlotRects[0].top, EmptySlot->Frame(), 0, 0, -1);
    }

    // The mission's tac map picture, turned 45 degrees onto the map area.
    std::string mapName;
    mapName = GamePath(TerrainPath, GlobalLogPtr->MissionFileName, ".log.tga");
    std::string fitName;
    fitName = GamePath(TerrainPath, GlobalLogPtr->MissionFileName, ".fit");
    MCFitIniFile terrainFile;
    int32_t result = terrainFile.Open(fitName);
    Assert(result == 0, result, "Could not find terrain file from .TGA ");
    result = terrainFile.SeekBlock("TerrainData");
    Assert(result == 0, result, "Could not find TerrainData block in terrain .FIT file");
    int32_t verticesBlockSide = 0;
    result = terrainFile.ReadIdLong("VerticesBlockSide", verticesBlockSide);
    Assert(result == 0, result, "Could not find variable VerticesBlockSide in terrain .FIT file");
    int32_t blocksMapSide = 0;
    result = terrainFile.ReadIdLong("BlocksMapSide", blocksMapSide);
    Assert(result == 0, result, "Could not find variable BlocksMapSide in terrain .FIT file");
    float metersPerVertex = 0.0f;
    result = terrainFile.ReadIdFloat("MetersPerVertex", metersPerVertex);
    Assert(result == 0, result, "Could not find variable MetersPerVertex in terrain .FIT file");
    const int32_t mapSide = blocksMapSide * verticesBlockSide;
    terrainFile.Close();
    delete MapPicture;
    MapPicture = new MCLogPort;
    result = MapPicture->Init(mapName.data());

    if (result != 0)
    {
        Fatal(result, " Unable to create Port for TacMap ");
    }

    if (MPlayer != nullptr)
    {
        // Each lance's label has a line to its drop zone on the map.
        const double side = static_cast<double>(mapSide);
        MarkerScale = static_cast<float>(std::sqrt(side * side + side * side) * metersPerVertex *
                                         static_cast<double>(0.0017667845f));
        MarkedZone = MPlayer->HomeTeam == 1 ? 3 : 0;
    }

    SetUpMission();
}

auto MCBriefingScreen::ClearLook() -> void
{
    MarkedZone = -1;
    LaunchPressed = false;
    OperationShown = false;
    OperationPictureShown = false;

    for (MCMechBriefBlock*& block : SlotBlocks)
    {
        block = nullptr;
    }

    BoxShown = nullptr;
}

auto MCBriefingScreen::SlotAt(int32_t xPos, int32_t yPos) const -> int32_t
{
    for (int32_t i = 0; i < 12; i++)
    {
        if (SlotRects[i].left == xPos && SlotRects[i].top == yPos)
        {
            return i;
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
    for (MCMechBriefBlock*& other : SlotBlocks)
    {
        if (other == block)
        {
            other = nullptr;
        }
    }

    SlotBlocks[slot] = block;
}

auto MCBriefingScreen::LiftFromSlot(MCMechBriefBlock* block) -> void
{
    for (MCMechBriefBlock*& other : SlotBlocks)
    {
        if (other == block)
        {
            other = nullptr;
        }
    }
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
    for (MCMechBriefBlock*& block : SlotBlocks)
    {
        if (child != nullptr && block == child)
        {
            block = nullptr;
        }
    }

    if (child != nullptr && child == BoxShown)
    {
        BoxShown = nullptr;
    }

    MCLogObject::RemoveChild(child);
}

auto MCBriefingScreen::LanceTons(int32_t lance) const -> int32_t
{
    if (MPlayer == nullptr)
    {
        return LanceTonnage(lance);
    }

    // Multiplayer: a lance counts once one of its slots holds a unit (the second lance always with two players).
    bool occupied = lance == 1 && MPlayer->PlayersOnHomeTeam()->Count == 2;

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
    if (MCLogPort* back = LogArtf("%slogart\\lsbbk00.tga", ArtPath))
    {
        VfxPaneCopy(back->Frame(), 0, 0, target, 0, 0, -1);
    }

    if (MapPicture != nullptr)
    {
        MCWindow* map = MapPicture->Frame()->Window;
        MCScreenVertex vertices[4] = {};
        vertices[0] = {0x159, 0x1a, 0, 0, 0, 0};
        vertices[1] = {0x273, 0x1a, 0, map->XMax << 16, 0, 0};
        vertices[2] = {0x273, 0x134, 0, map->XMax << 16, map->YMax << 16, 0};
        vertices[3] = {0x159, 0x134, 0, 0, map->YMax << 16, 0};
        VfxMapPolygon(target, std::span(vertices, 4), map, VfxMapTransparent);
    }

    // The lance labels, with their tonnage: the first lance's always, the others in multiplayer or when the mission
    // gives the player their slots. (The original measured the first lance's figure in the black font and the
    // others' in the blue one, and wrote them all in black.)
    static constexpr int32_t labelTops[3] = {0x18, 0x88, 0xf8};
    const int32_t* local = GlobalLogPtr->LocalDropSlot;
    const bool lanceShown[3] = {true, MPlayer != nullptr || local[4] != 0,
                                MPlayer != nullptr || (local[4] != 0 && local[8] != 0)};

    for (int32_t lance = 0; lance < 3; lance++)
    {
        if (!lanceShown[lance])
        {
            continue;
        }

        if (MCLogPort* art = LogArtf("%slogart\\lsbdf0%d.tga", ArtPath, lance + 2))
        {
            art->CopyTo(target, 0xd6, labelTops[lance], -1);
        }

        char text[32];
        std::snprintf(text, sizeof(text), "%d", LanceTons(lance));
        auto* bytes = reinterpret_cast<uint8_t*>(text);
        const int32_t textWidth = lance == 0 && MPlayer == nullptr ? BlackFont->Width(bytes) : BlueFont->Width(bytes);
        BlackFont->WriteString(target, 0x13f - textWidth, labelTops[lance] + 4, bytes, -1);
    }

    if (MarkedZone >= 0)
    {
        static constexpr int32_t markerLines[3] = {0x24, 0x94, 0x104};

        for (int32_t zone = 0; zone < 3; zone++)
        {
            DrawDropZoneMarker(target, GlobalLogPtr->DropZonePositions[MarkedZone + zone], MarkerScale,
                               markerLines[zone]);
        }
    }

    // The tonnage bar and the launch button: pressed while clicked or locked (multiplayer launch), lit when the force
    // can drop.
    PaintTonnageBar(target, MaxDeployTonnage, CurDeployTonnage, GlobalLogPtr->HammerDown != 0);
    const char* launchName = "lsbdf01.tga";

    if (!LaunchPressed && ButtonsLocked == 0)
    {
        bool ready = CurDeployTonnage >= 1;

        if (ready && (MaxDeployTonnage < CurDeployTonnage || GlobalLogPtr->RequiredAssigned() == 0))
        {
            ready = GlobalLogPtr->HammerDown != 0;
        }

        launchName = ready ? "lsbdf00.tga" : "lsbdf00a.tga";
    }

    if (MCLogPort* art = LogArtf("%slogart\\%s", ArtPath, launchName))
    {
        art->CopyTo(target, 0x20d, 0x148, -1);
    }

    // The tab, and over it the chat button while it blinks (multiplayer).
    if (CurrentTab == 1 || CurrentTab == 2)
    {
        const char* tabName = CurrentTab == 1 ? (MPlayer == nullptr ? "lsbdw00.tga" : "lsbdw02.tga")
                                              : (MPlayer == nullptr ? "lsbdw01.tga" : "lsbdw03.tga");

        if (MCLogPort* art = LogArtf("%slogart\\%s", ArtPath, tabName))
        {
            art->CopyTo(target, 0xc4, 0x65, 0);
        }
    }

    if (ChatTimerOn != 0)
    {
        if (ChatBlinkOn != 0)
        {
            ChatBlinkPort->CopyTo(target, 0xc5, 0x65, -1);
        }
        else
        {
            ChatRegularPort->CopyTo(target, 0xc4, 0x65, -1);
        }
    }

    // The operation area: its art, the operation picture over it before the movie, the closing art after the movie.
    if (OperationShown)
    {
        if (MCLogPort* art = LogArtf("%slogart\\lsb_op0.tga", ArtPath))
        {
            art->CopyTo(target, 0xc, 0x6f, -1);
        }
    }

    if (OperationPictureShown && OperationPicture != nullptr)
    {
        OperationPicture->CopyTo(target, 0xc, 0x6f, -1);
    }

    if (MovieOver != 0 && InDemo == 0)
    {
        if (MCLogPort* art = LogArtf("%slogart\\lsb_op6.tga", ArtPath))
        {
            art->CopyTo(target, 0xc, 0x6f, -1);
        }
    }

    // The drop slots: covered when the player can't fill them, else blank (colour 0x10 inside the slot's border, as
    // the screen's setup painted them), and the unit placed in it.
    for (int32_t i = 0; i < 12; i++)
    {
        const RECT& area = SlotRects[i];

        if (GlobalLogPtr->LocalDropSlot[i] == 0)
        {
            if (MCLogPort* art = LogArtf("%slogart\\lsbdf06.tga", ArtPath))
            {
                art->CopyTo(target, area.left + 1, area.top + 1, -1);
            }
        }
        else
        {
            ::FillBox(target, area.left, area.top, 0x32, 0x2c, 0x10);
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
        ::FillBox(target, 0xd3, 0x16f, 0x1aa, 0x6e, 0x10);
    }
}

auto MCBriefingScreen::NewLookPicture() -> MCLogPort*
{
    auto* picture = new MCLogPort;
    picture->Init(Width(), Height(), -1);
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
            if (MovieStarted != 0)
            {
                SetUpOperation();
                return;
            }

            MovieStarted = 1;
        }
    }
    else if (InDemo == 0)
    {
        // The movie is over: its window goes, and the closing art shows (PaintLook).
        PlayMovie = 0;
        RemoveChild(SmackerWindow);
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
    ScreenWindow->RemoveChild(this);
    ClearLook();
    delete MapPicture;
    MapPicture = nullptr;
    delete ChatBlinkPort;
    ChatBlinkPort = nullptr;
    delete ChatRegularPort;
    ChatRegularPort = nullptr;
    delete EmptySlot;
    EmptySlot = nullptr;
    delete[] UndeployedMechs;
    UndeployedMechs = nullptr;
    delete MissionPort;
    MissionPort = nullptr;

    if (Smacker != nullptr)
    {
        // Port: the original freed the handle from the logistics heap; the port's handles come from SmackOpen.
        SmackClose(Smacker);
        Smacker = nullptr;
    }

    delete SmackerWindow;
    SmackerWindow = nullptr;
    delete DeployPane;
    DeployPane = nullptr;

    if (MissionPane != nullptr)
    {
        MissionPane->SetDisplayPort(nullptr, 0, -1);
        delete MissionPane;
        MissionPane = nullptr;
    }

    delete OperationPicture;
    OperationPicture = nullptr;
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
    if (MPlayer != nullptr)
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
    if (ButtonsLocked == 0)
    {
        LaunchPressed = false;
    }
}

auto MCBriefingScreen::PaintTonnageBar(MCPane* target, int32_t maxTons, int32_t tons, bool hammerDown) -> void
{
    if (MCLogPort* art = LogArtf("%slogart\\lsbdf07.tga", ArtPath))
    {
        art->CopyTo(target, 0x157, 0x13f, 0);
    }

    char text[256];
    std::snprintf(text, sizeof(text), "%d", maxTons);
    int32_t width = BlueFont->Width(reinterpret_cast<uint8_t*>(text));
    MCGuiFont* font = !hammerDown ? YellowDropFont : RedFont;
    font->WriteString(target, 0x1d9 - width, 0x13f, reinterpret_cast<uint8_t*>(text), -1);
    std::snprintf(text, sizeof(text), "%d", tons);

    if (maxTons < tons)
    {
        width = RedFont->Width(reinterpret_cast<uint8_t*>(text));
        font = RedFont;
    }
    else
    {
        width = BlueFont->Width(reinterpret_cast<uint8_t*>(text));
        font = YellowDropFont;
    }

    font->WriteString(target, 0x1d9 - width, 0x149, reinterpret_cast<uint8_t*>(text), -1);

    // Port fix: with no limit the original's bar width was the x87 integer indefinite (a negative port width).
    int32_t barWidth = 0;

    if (maxTons != 0)
    {
        barWidth = static_cast<int32_t>(static_cast<double>(tons) / maxTons * 149.0);
    }

    if (barWidth != 0)
    {
        if (barWidth > 0x95)
        {
            barWidth = 0x95;
        }

        MCLogBlockPort bar(target, 0x15b, 0x155, barWidth, 0xe, true);

        if (MCLogPort* art = LogArtf("%slogart\\lsbdf05.tga", ArtPath))
        {
            art->CopyTo(bar.Frame(), 0, 0, 0);
        }
    }
}

auto MCBriefingScreen::HandleEvent(MCGuiEvent* event) -> void
{
    if (GlobalLogPtr->CurrentScreen != this)
    {
        return;
    }

    const int32_t xPos = event->X;
    const int32_t yPos = event->Y;

    if (event->Key == 0 && event->Type != 0x13)
    {
        // The help line for whatever the mouse is over, and the highlighted screen button.
        Application->SetCurrentCursor(static_cast<MCCursorType>(0));
        GlobalLogPtr->DrawScreenButtons();
        POINT point{xPos, yPos};
        auto inside = [&point](int32_t left, int32_t top, int32_t right, int32_t bottom)
        {
            RECT area{left, top, right, bottom};
            return PtInRect(&area, point) != 0;
        };

        // The drop slots under the mouse, if any: -1 when none.
        auto slotHelp = [&point]() -> int32_t
        {
            for (int32_t lance = 0; lance < 3; lance++)
            {
                for (int32_t slot = 0; slot < 4; slot++)
                {
                    if (GlobalLogPtr->LocalDropSlot[lance * 4 + slot] == 0)
                    {
                        break;
                    }

                    if (PtInRect(&GlobalLogPtr->BriefingScreen->SlotRects[lance * 4 + slot], point) != 0)
                    {
                        const auto& deploy = GlobalLogPtr->DeploySlots[lance][slot];
                        return deploy.Unit < 0 && deploy.Vehicle < 0 ? 0x20 : 0x21;
                    }
                }
            }

            return -1;
        };

        if (inside(2, 2, 0xd0, 0xd))
        {
            ShowHelp(0x1d);
        }
        else if (inside(2, 0x10, 0xd0, 0x21))
        {
            ShowHelp(0x286);
            GlobalLogPtr->HoverScreenButton(this, 0);
        }
        else if (inside(2, 0x22, 0xd0, 0x33))
        {
            ShowHelp(0x1e);
        }
        else if (inside(2, 0x34, 0xd0, 0x45))
        {
            ShowHelp(0x41);

            if (ButtonsLocked == 0)
            {
                GlobalLogPtr->HoverScreenButton(this, 2);
            }
        }
        else if (inside(2, 0x46, 0xd0, 0x57))
        {
            ShowHelp(0x42);

            if (ButtonsLocked == 0)
            {
                GlobalLogPtr->HoverScreenButton(this, 3);
            }
        }
        else if (inside(0x20c, 2, 0x24d, 0xd))
        {
            ShowHelp(0x1f);
        }
        else
        {
            const int32_t slotText = inside(0xde, 0x1a, 0x14a, 0x164) ? slotHelp() : -1;

            if (slotText >= 0)
            {
                ShowHelp(static_cast<uint32_t>(slotText));
            }
            else if (inside(0x20d, 0x148, 0x26a, 0x15a))
            {
                ShowHelp(0x22);
            }
            else if (inside(0xd3, 0x16f, 0x27f, 0x1df))
            {
                ShowHelp(0x23);
            }
            else if (inside(7, 0x15d, 0xcb, 0x1d8))
            {
                ShowHelp(0x24);
            }
            else if (inside(0xc2, 0x67, 0xcf, 0xde))
            {
                ShowHelp(CurrentTab == 1 ? 0x26 : 0x27);
            }
            else if (inside(0xc2, 0xe0, 0xcf, 0x14a))
            {
                if (CurrentTab != 2)
                {
                    ShowHelp(0x28);
                }
            }
            else
            {
                GlobalLogPtr->Ticker->SetString(nullptr);
            }
        }
    }

    if (event->Type == 9)
    {
        // Ctrl+Alt+= : a thousand resource points.
        const bool ctrlAlt = MCInput::GetAsyncKeyState(VK_CONTROL) != 0 && MCInput::GetAsyncKeyState(VK_MENU) != 0;

        if (MPlayer == nullptr && CheatsOn != 0 && event->Key == 0xbb && ctrlAlt)
        {
            ResourcePoints += 1000;
        }
    }

    switch (event->Type)
    {
        case 1:
        {
            MouseDown = -1;
            POINT point{xPos - GlobalX(), yPos - GlobalY()};
            RECT area{2, 0x34, 0xd1, 0x45};

            if (ButtonsLocked == 0 && PtInRect(&area, point) != 0)
            {
                GlobalLogPtr->SetUpPurchaseScreen(-1);
                return;
            }

            area.top = 0x46;
            area.bottom = 0x57;

            if (ButtonsLocked == 0 && PtInRect(&area, point) != 0)
            {
                GlobalLogPtr->SetUpRepairScreen(-1);
                return;
            }

            area.top = 0x10;
            area.bottom = 0x21;

            if (PtInRect(&area, point) != 0)
            {
                if (MPlayer == nullptr)
                {
                    StopSmackerMovies();
                    SoundSystem()->PlayDigitalSample(0x36, 1, nullptr, 0, 0);
                    GlobalLogPtr->SetUpMainScreen(0);
                    return;
                }

                CheckExit();
                return;
            }

            // The tabs.
            area = {0xc2, 0x67, 0xd1, 0xde};

            if (PtInRect(&area, point) != 0)
            {
                SoundSystem()->PlayDigitalSample(0x36, 1, nullptr, 0, 0);
                SetUpOperation();
                return;
            }

            area.top = 0xdf;
            area.bottom = 0x14a;

            if (PtInRect(&area, point) != 0)
            {
                SoundSystem()->PlayDigitalSample(0x36, 1, nullptr, 0, 0);
                SetUpMission();
                return;
            }

            // Launch.
            area = {0x205, 0x14a, 0x270, 0x15c};

            if (ButtonsLocked != 0 || PtInRect(&area, point) == 0)
            {
                break;
            }

            if (CurDeployTonnage == 0 ||
                ((MaxDeployTonnage < CurDeployTonnage || GlobalLogPtr->RequiredAssigned() == 0) &&
                 GlobalLogPtr->HammerDown == 0))
            {
                SoundSystem()->PlayBettySample(0x1b);
                return;
            }

            LaunchPressed = true;
            UpdateDisplay(0, 0, 0, 0, 0);

            if (MPlayer == nullptr)
            {
                // Everything the player owns must fit the save: at most 50 mechs and vehicles.
                const int32_t units = GlobalLogPtr->ForceVehicleList->NumVehicles +
                                      GlobalLogPtr->ForceMechList->NumMechs + GlobalLogPtr->VehicleList->NumVehicles +
                                      GlobalLogPtr->MechList->NumMechs;

                if (units < 0x33)
                {
                    SoundSystem()->PlayDigitalSample(0x3a, 1, nullptr, 0, 0);
                    Mission->StartScenario(Mission->Scenarios[Mission->CurrentScenario].data());
                    return;
                }

                SoundSystem()->PlayDigitalSample(0x33, 1, nullptr, 0, 0);
                char format[256];
                CLoadString(ThisInstance, 0x373, format, 0xfe);
                char text[256];
                std::snprintf(text, sizeof(text), format, units, 0x32, units - 0x32);
                ShowMessage(text);
                return;
            }

            ButtonsLocked = -1;
            MPlayer->SendReadyForBattle();
            char format[200];
            CLoadString(ThisInstance, 0x379, format, 199);
            char text[304];
            std::snprintf(text, sizeof(text), format, MPlayer->SessionManager->MyPlayer->Name);
            MPlayer->SendChat(0, text);
            return;
        }

        case 4:
        {
            MouseDown = 0;
            // The launch button shows up again once let go (unless a multiplayer launch locked it).
            LaunchPressed = false;
            GlobalX();
            GlobalY();
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
            if (event->Data == 6)
            {
                // The operation movie's start delay.
                PlayMovie = -1;
                Application->RemoveTimer(this, 6);
                SetUpOperation();
                return;
            }

            if (event->Data == 5)
            {
                // The chat button blinks (PaintLook draws it).
                ChatBlinkOn = ChatBlinkOn != 0 ? 0 : -1;
            }
            break;
        }
        default:
            break;
    }
}

auto MCBriefingScreen::ShowGuiWindow(int show) -> void
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
    GlobalLogPtr->AutoPlayMovie = 0;
    MovieOver = 0;

    if (MPlayer == nullptr)
    {
        OperationShown = true;

        if (PlayMovie == 0)
        {
            Application->AddTimer(this, 6, 500, 0, 0, 0);
        }
    }
    else
    {
        // Multiplayer: the tab is the chat, which stops every chat button blinking.
        MovieOver = -1;

        if (ChatBlinking != 0)
        {
            Application->RemoveTimer(this, 5);
            ChatTimerOn = 0;
            ChatBlinking = 0;
        }

        if (GlobalLogPtr->PurchaseScreen->ChatBlinking != 0)
        {
            Application->RemoveTimer(GlobalLogPtr->PurchaseScreen, 7);
            GlobalLogPtr->PurchaseScreen->ChatBlinking = 0;
        }

        if (GlobalLogPtr->RepairScreen->ChatBlinking != 0)
        {
            Application->RemoveTimer(GlobalLogPtr->RepairScreen, 8);
            GlobalLogPtr->RepairScreen->ChatBlinking = 0;
        }

        ChatBlinkOn = 0;
        GlobalLogPtr->ChatWindow->ShowGuiWindow(-1);
    }

    MissionPane->ShowGuiWindow(0);
    StopSmackerMovies();
    CurrentTab = 1;

    if (PlayMovie == 0 || MPlayer != nullptr)
    {
        return;
    }

    if (OperationPicture != nullptr)
    {
        OperationPictureShown = true;
        SoundSystem()->PlayBettySample(0x1a);

        while (SoundSystem()->IsChannelPlaying(0xe) != 0)
        {
            UpdateDisplay(0, 0, 0, 0, 0);
        }

        if (MPlayer != nullptr)
        {
            return;
        }
    }

    if (GlobalLogPtr->OperationCinema != nullptr)
    {
        std::string movieName;
        movieName = GamePath(MoviePath, GlobalLogPtr->OperationCinema, ".smk");
        Smacker = SmackOpen(movieName.c_str(), 0xfe000, -1);

        // Port fix: a missing movie is skipped (the original read the null handle's size).
        if (Smacker == nullptr)
        {
            return;
        }

        auto* window = new MCGuiSmackerWindow;
        RECT area{0xc, 0x6f, Smacker->Player->Width(), Smacker->Player->Height()};
        SmackerWindow = window;
        window->Init(&area, nullptr);
        AddChild(window);
        SmackerWindow->StartSmackerMovie(Smacker, 0);
        SmackerWindow->Draw();
    }
}

auto MCBriefingScreen::SetUpMission() -> void
{
    // The tab's art is drawn from currentTab (PaintLook).
    CurrentTab = 2;

    if (MPlayer == nullptr)
    {
        StopSmackerMovies();
    }
    else
    {
        GlobalLogPtr->ChatWindow->ShowGuiWindow(0);
    }

    MissionPane->SetDisplayPort(MissionPort, 0, 0);
    MissionPane->ShowGuiWindow(-1);
}

auto MCBriefingScreen::StopSmackerMovies() -> void
{
    if (SmackerWindow == nullptr)
    {
        return;
    }

    RemoveChild(SmackerWindow);
    MCGuiSmackerWindow* window = SmackerWindow;
    window->EndSmackerMovie();
    delete window;
    SmackerWindow = nullptr;
    PlayMovie = 0;
    Smacker = nullptr;
}

auto MCBriefingScreen::SetUpDeploy() -> void
{
    MCScrollPane* pane = DeployPane;

    for (int32_t count = pane->NumberOfChildren(); count > 0; count--)
    {
        pane->RemoveChild(pane->Child(0));
    }

    delete[] UndeployedMechs;
    UndeployedMechs = nullptr;

    // Blocks of units still waiting are rebuilt; placed units keep theirs (redrawn).
    NumUndeployed = 0;
    MCLogVehicle* firstVehicle = GlobalLogPtr->ForceVehicleList->Vehicles;

    for (MCLogMech* mech = GlobalLogPtr->ForceMechList->Mechs; mech != nullptr; mech = mech->Next)
    {
        if (mech->PilotIndex < 0)
        {
            continue;
        }

        if (mech->Deployed == 0)
        {
            delete mech->BriefBlock;
            mech->BriefBlock = nullptr;
            ++NumUndeployed;
        }
        else
        {
            mech->BriefBlock->DrawBackground();
        }
    }

    for (MCLogVehicle* vehicle = firstVehicle; vehicle != nullptr; vehicle = vehicle->Next)
    {
        if (vehicle->Deployed == 0)
        {
            delete vehicle->BriefBlock;
            vehicle->BriefBlock = nullptr;
        }
        else
        {
            vehicle->BriefBlock->DrawBackground();
        }
    }

    const int32_t mechCount = NumUndeployed;
    UndeployedMechs = new int32_t[mechCount];
    int32_t index = 0;
    int32_t filled = 0;

    for (MCLogMech* mech = GlobalLogPtr->ForceMechList->Mechs; mech != nullptr; mech = mech->Next, ++index)
    {
        if (mech->PilotIndex >= 0 && mech->Deployed == 0)
        {
            UndeployedMechs[filled++] = index;
        }
    }

    // Three blocks a row.
    auto* port = new MCLogPort;
    const int32_t blocks = GlobalLogPtr->ForceVehicleList->GetVehicleCount() + mechCount;
    int32_t height;

    if (blocks < 7)
    {
        height = pane->Height();
    }
    else
    {
        height = (blocks / 3 + 1) * 0x34;

        if (blocks % 3 == 0)
        {
            height -= 0x34;
        }
    }

    // Port: the pane's blocks are drawn into it each frame (the original painted each into this picture).
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
    pane->SetDisplayPort(port, -1, -1);
    int32_t block = 0;

    for (; block < mechCount; block++)
    {
        MCLogMech* mech = nullptr;
        GlobalLogPtr->ForceMechList->GetMechInfo(UndeployedMechs[block], mech);
        auto* brief = new MCMechBriefBlock;
        brief->Init(mech, pane, (block % 3) * 0x38 + 4, (block / 3) * 0x34 + 3);
    }

    for (MCLogVehicle* vehicle = GlobalLogPtr->ForceVehicleList->Vehicles; vehicle != nullptr; vehicle = vehicle->Next)
    {
        if (vehicle->Deployed != 0)
        {
            continue;
        }

        auto* brief = new MCMechBriefBlock;
        brief->Init(vehicle, pane, (block % 3) * 0x38 + 4, (block / 3) * 0x34 + 3);
        ++block;
    }

    pane->SetSliderPos(sliderPos);
}

// ---------------------------------------------------------------------------------------------------------------------
// MechBriefBlock
// ---------------------------------------------------------------------------------------------------------------------

MCMechBriefBlock::MCMechBriefBlock()
{
    Mech = nullptr;
    Vehicle = nullptr;
}

auto MCMechBriefBlock::Init(MCLogMech* newMech, MCLogObject* parent, int32_t xPos, int32_t yPos) -> void
{
    MCLogObject::Init(xPos, yPos, 0x34, 0x2e, nullptr, parent->Lport());
    Mech = newMech;
    newMech->BriefBlock = this;
    parent->AddChild(this);
    DrawBackground();
}

auto MCMechBriefBlock::Init(MCLogVehicle* newVehicle, MCLogObject* parent, int32_t xPos, int32_t yPos) -> void
{
    MCLogObject::Init(xPos, yPos, 0x34, 0x2e, nullptr, parent->Lport());
    Vehicle = newVehicle;
    newVehicle->BriefBlock = this;
    parent->AddChild(this);
    DrawBackground();
}

auto MCMechBriefBlock::Destroy() -> void
{
    Mech = nullptr;
    Vehicle = nullptr;
    MCLogObject::Destroy();
}

auto MCMechBriefBlock::HandleEvent(MCGuiEvent* event) -> void
{
    MCBriefingScreen* screen = GlobalLogPtr->BriefingScreen;

    if (LeftDragging == 0 && RightDragging == 0)
    {
        if (event->X < GlobalX() || event->X > GlobalX() + 1 + Width())
        {
            return;
        }

        if (event->Y < GlobalY() || event->Y > GlobalY() + 1 + Height())
        {
            return;
        }
    }

    // The parent is always a logistics object (the briefing screen or the deploy pane).
    auto* owner = static_cast<MCLogObject*>(Parent);

    if (owner != nullptr && LeftDragging == 0 && (event->Type == 8 || event->Type == 9))
    {
        owner->HandleEvent(event);
        return;
    }

    const int32_t type = event->Type;

    switch (type)
    {
        case 1:
        {
            if (RightDragging != 0)
            {
                return;
            }
            break;
        }
        case 3:
        {
            if (LeftDragging != 0)
            {
                return;
            }

            RightDragging = -1;
            break;
        }
        case 7:
        {
            if (LeftDragging != 0)
            {
                DragY = event->Y - 0x17;
                DragX = event->X - 0x1a;
                GlobalLogPtr->DragIcon->MoveTo(DragX, DragY, 0);
                return;
            }

            if (Application->GrabbedObject() == nullptr && event->Key == 0)
            {
                ShowHelp(GlobalX() < 0xd1 ? 0x22 : 0x21);
            }

            return;
        }
        case 4:
        case 6:
        {
            if (type == 6 && LeftDragging != 0)
            {
                return;
            }

            if (RightDragging != 0 && type == 4)
            {
                return;
            }

            if (Application->GrabbedObject() == nullptr)
            {
                RightDragging = 0;
                return;
            }

            Application->Release();
            RightDragging = 0;
            LeftDragging = 0;
            delete GlobalLogPtr->DragIcon;
            GlobalLogPtr->DragIcon = nullptr;

            // Places the unit in an empty slot: false when it is too heavy (the message is shown).
            auto placeInEmpty = [this](int32_t lance, int32_t slot) -> bool
            {
                auto& deploy = GlobalLogPtr->DeploySlots[lance][slot];

                if (Mech != nullptr)
                {
                    if (!FitsTonnage(Mech) && GlobalLogPtr->HammerDown == 0)
                    {
                        return false;
                    }

                    GlobalLogPtr->SendAddMechMessage(Mech, lance, slot);
                    SoundSystem()->PlayDigitalSample(0x34, 1, nullptr, 0, 0);
                    deploy.Unit = GlobalLogPtr->ForceMechList->GetMechIndex(Mech);
                    CurDeployTonnage = DeployTonnageWith(Mech, 1.0f);
                    Mech->Deployed = -1;
                    GlobalLogPtr->AssignedWarriorList->SetDeployed(Mech->PilotIndex, -1);
                    return true;
                }

                if (!FitsTonnage(Vehicle) && GlobalLogPtr->HammerDown == 0)
                {
                    return false;
                }

                GlobalLogPtr->SendAddVehicleMessage(Vehicle, lance, slot);
                SoundSystem()->PlayDigitalSample(0x34, 1, nullptr, 0, 0);
                deploy.Vehicle = GlobalLogPtr->ForceVehicleList->GetVehicleIndex(Vehicle);
                CurDeployTonnage = DeployTonnageWith(Vehicle, 1.0f);
                Vehicle->Deployed = -1;
                return true;
            };

            // Puts the block into the slot on the screen and redraws.
            auto settle = [this, screen](int32_t lance, int32_t slot)
            {
                if (Parent != nullptr)
                {
                    Parent->RemoveChild(this);
                }

                screen->AddChild(this);
                const RECT& area = screen->SlotRects[lance * 4 + slot];
                MoveTo(area.left, area.top, 0);
                DrawBackground();
                screen->CalcTonnages();
            };

            auto tooHeavy = [screen]()
            {
                ShowMessage(0x4cu);
                SoundSystem()->PlayBettySample(0);
                screen->DrawTonnageBar();
            };

            bool placed = false;
            int32_t firstLance = 0;
            POINT point{event->X, event->Y};

            if (type == 6)
            {
                // A right-button drop goes to the first free slot (of lance 1, 2 or 3 with those keys held).
                if (DraggedFromSlot != 0)
                {
                    firstLance = 3;
                }
                else
                {
                    int32_t lance = -1;

                    if ((MCInput::GetAsyncKeyState('1') & 0x8000) != 0)
                    {
                        lance = 0;
                    }
                    else if ((MCInput::GetAsyncKeyState('2') & 0x8000) != 0)
                    {
                        lance = 1;
                    }
                    else if ((MCInput::GetAsyncKeyState('3') & 0x8000) != 0)
                    {
                        lance = 2;
                    }

                    int32_t foundLance = -1;
                    int32_t foundSlot = -1;
                    const int32_t from = lance < 0 ? 0 : lance;
                    const int32_t to = lance < 0 ? 3 : lance + 1;

                    for (int32_t l = from; l < to && foundLance < 0; l++)
                    {
                        for (int32_t s = 0; s < 4; s++)
                        {
                            if (GlobalLogPtr->LocalDropSlot[l * 4 + s] == 0)
                            {
                                if (lance < 0)
                                {
                                    continue;
                                }
                                break;
                            }

                            const auto& deploy = GlobalLogPtr->DeploySlots[l][s];

                            if (deploy.Unit == -1 && deploy.Vehicle == -1)
                            {
                                foundLance = l;
                                foundSlot = s;
                                break;
                            }
                        }
                    }

                    // Port fix (OB-083): with no key held and no free slot the original went on to search a fourth
                    // lance past the end of the slot tables.
                    if (foundLance < 0)
                    {
                        firstLance = 3;
                    }
                    else if (placeInEmpty(foundLance, foundSlot))
                    {
                        settle(foundLance, foundSlot);
                        placed = true;
                    }
                    else
                    {
                        tooHeavy();
                        // Port fix (OB-083): the original went on testing the later lances' slots against an
                        // uninitialised point; no slot is under it here.
                        firstLance = 3;
                    }
                }
            }

            // A left-button drop goes to the slot under the mouse; whatever was there goes back to the pane.
            for (int32_t lance = firstLance; lance < 3 && !placed; lance++)
            {
                for (int32_t slot = 0; slot < 4; slot++)
                {
                    if (GlobalLogPtr->LocalDropSlot[lance * 4 + slot] == 0 ||
                        PtInRect(&screen->SlotRects[lance * 4 + slot], point) == 0)
                    {
                        continue;
                    }

                    auto& deploy = GlobalLogPtr->DeploySlots[lance][slot];
                    bool fits;

                    if (deploy.Unit > -1)
                    {
                        // A mech was there: it leaves the force's drop.
                        MCLogMech* occupant = nullptr;
                        GlobalLogPtr->ForceMechList->GetMechInfo(deploy.Unit, occupant);
                        CurDeployTonnage = DeployTonnageWith(occupant, -1.0f);
                        occupant->Deployed = 0;
                        delete occupant->BriefBlock;
                        occupant->BriefBlock = nullptr;
                        deploy.Unit = -1;

                        if (Mech != nullptr)
                        {
                            fits = FitsTonnage(Mech);

                            if (fits)
                            {
                                GlobalLogPtr->SendAddMechMessage(Mech, lance, slot);
                                SoundSystem()->PlayDigitalSample(0x34, 1, nullptr, 0, 0);
                                deploy.Unit = GlobalLogPtr->ForceMechList->GetMechIndex(Mech);
                                CurDeployTonnage = DeployTonnageWith(Mech, 1.0f);
                                Mech->Deployed = -1;
                            }
                        }
                        else
                        {
                            fits = FitsTonnage(Vehicle);

                            if (fits)
                            {
                                GlobalLogPtr->SendAddVehicleMessage(Vehicle, lance, slot);
                                SoundSystem()->PlayDigitalSample(0x34, 1, nullptr, 0, 0);
                                deploy.Unit = -1;
                                deploy.Vehicle = GlobalLogPtr->ForceVehicleList->GetVehicleIndex(Vehicle);
                                CurDeployTonnage = DeployTonnageWith(Vehicle, 1.0f);
                                Vehicle->Deployed = -1;
                            }
                        }
                    }
                    else if (deploy.Vehicle > -1)
                    {
                        // A vehicle was there.
                        MCLogVehicle* occupant = nullptr;
                        GlobalLogPtr->ForceVehicleList->GetVehicleInfo(deploy.Vehicle, occupant);
                        CurDeployTonnage = DeployTonnageWith(occupant, -1.0f);
                        occupant->Deployed = 0;
                        delete occupant->BriefBlock;
                        occupant->BriefBlock = nullptr;
                        deploy.Vehicle = -1;

                        if (Mech != nullptr)
                        {
                            fits = FitsTonnage(Mech);

                            if (fits)
                            {
                                GlobalLogPtr->SendAddMechMessage(Mech, lance, slot);
                                SoundSystem()->PlayDigitalSample(0x34, 1, nullptr, 0, 0);
                                deploy.Vehicle = -1;
                                deploy.Unit = GlobalLogPtr->ForceMechList->GetMechIndex(Mech);
                                CurDeployTonnage = DeployTonnageWith(Mech, 1.0f);
                                Mech->Deployed = -1;
                                GlobalLogPtr->AssignedWarriorList->SetDeployed(Mech->PilotIndex, -1);
                            }
                        }
                        else
                        {
                            fits = FitsTonnage(Vehicle);

                            if (fits)
                            {
                                GlobalLogPtr->SendAddVehicleMessage(Vehicle, lance, slot);
                                SoundSystem()->PlayDigitalSample(0x34, 1, nullptr, 0, 0);
                                deploy.Vehicle = GlobalLogPtr->ForceVehicleList->GetVehicleIndex(Vehicle);
                                CurDeployTonnage = DeployTonnageWith(Vehicle, 1.0f);
                                Vehicle->Deployed = -1;
                            }
                        }
                    }
                    else
                    {
                        fits = placeInEmpty(lance, slot);
                    }

                    if (fits)
                    {
                        settle(lance, slot);
                        placed = true;
                    }
                    else
                    {
                        tooHeavy();
                    }
                    break;
                }
            }

            if (placed)
            {
                screen->SetUpDeploy();
                return;
            }

            // Nowhere to go: back to the deploy pane.
            uint32_t sound = 0x34;

            if (DraggedFromSlot == 0)
            {
                MCScrollPane* pane = screen->DeployPane;
                const bool overPane = event->X > pane->GlobalX() && event->X < pane->GlobalX() + pane->Width() &&
                                      event->Y > pane->GlobalY() && event->Y < pane->GlobalY() + pane->Height();

                if (!overPane)
                {
                    sound = 0x33;
                }
            }

            SoundSystem()->PlayDigitalSample(sound, 1, nullptr, 0, 0);

            if (Mech != nullptr)
            {
                Mech->Deployed = 0;
                GlobalLogPtr->AssignedWarriorList->SetDeployed(Mech->PilotIndex, 0);
            }
            else
            {
                Vehicle->Deployed = 0;
            }

            if (Parent != nullptr)
            {
                Parent->RemoveChild(this);
            }

            screen->DeployPane->AddChild(this);
            screen->SetUpDeploy();
            return;
        }

        default:
            return;
    }

    // A button went down on the block: its briefing shows, and it can be dragged.
    MCBriefingBox* shown = screen->BriefingBox;

    if (shown != nullptr && shown->Parent != nullptr)
    {
        shown->Parent->RemoveChild(shown);
    }

    MCLogPart* part = Mech != nullptr ? static_cast<MCLogPart*>(Mech) : Vehicle;
    MCBriefingBox* box = part->BriefingBox;
    screen->AddChild(box);
    screen->BriefingBox = box;
    box->DrawBackground();

    if (part->LocalPart == 0 || screen->ButtonsLocked != 0)
    {
        return;
    }

    DraggedFromSlot = owner == screen ? 1 : 0;
    SoundSystem()->PlayDigitalSample(0x35, 1, nullptr, 0, 0);
    Application->Grab(this);

    if (event->Type == 1)
    {
        LeftDragging = -1;
    }

    DragX = GlobalX();
    DragY = GlobalY();
    auto* icon = new MCDragIcon;
    GlobalLogPtr->DragIcon = icon;
    icon->Begin(DragX, DragY, 0x34, 0x2e, [this](MCLogPort* surface) { OnBeginDrag(surface); });

    // The block's place is blanked (in the deploy pane, setUpDeploy below rebuilds it without the block).
    if (owner == screen)
    {
        screen->LiftFromSlot(this);
    }

    if (owner == screen)
    {
        // Out of its slot: the unit leaves the drop.
        bool found = false;

        for (int32_t lance = 0; lance < 3 && !found; lance++)
        {
            for (int32_t slot = 0; slot < 4; slot++)
            {
                if (GlobalLogPtr->LocalDropSlot[lance * 4 + slot] == 0 ||
                    PtInRect(&screen->SlotRects[lance * 4 + slot], POINT{event->X, event->Y}) == 0)
                {
                    continue;
                }

                auto& deploy = GlobalLogPtr->DeploySlots[lance][slot];

                if (Mech == nullptr)
                {
                    Vehicle->Deployed = 0;
                    deploy.Vehicle = -1;
                    CurDeployTonnage = DeployTonnageWith(Vehicle, -1.0f);
                }
                else
                {
                    Mech->Deployed = 0;
                    deploy.Unit = -1;
                    CurDeployTonnage = DeployTonnageWith(Mech, -1.0f);
                    GlobalLogPtr->AssignedWarriorList->SetDeployed(Mech->PilotIndex, 0);
                }

                screen->CalcTonnages();
                GlobalLogPtr->SendRemoveForceMessage(lance, slot);
                found = true;
                break;
            }
        }

        Assert(found, 0, "Could not find the slot this item occupied");
    }
    else
    {
        if (Mech == nullptr)
        {
            Vehicle->Deployed = -1;
        }
        else
        {
            // A mech with a damaged engine or a destroyed location can't drop.
            MCLogInventoryItem* engine = Mech->Inventory->Items;

            while (engine != nullptr && MasterComponentList[engine->MasterID].Form != MCComponentForm::Engine)
            {
                engine = engine->Next;
            }

            uint32_t refusal = 0;

            // Port fix: a mech without an engine item (the original dereferenced null) counts as undamaged.
            if (engine != nullptr && engine->Stats->Hits != 0)
            {
                refusal = 0x35d;
            }
            else
            {
                for (const auto& location : Mech->Internals)
                {
                    if (location.CurArmor == 0)
                    {
                        refusal = 0x364;
                        break;
                    }
                }
            }

            if (refusal != 0)
            {
                char text[256];
                CLoadString(ThisInstance, refusal, text, 0xfe);
                Application->Release();
                LeftDragging = 0;
                RightDragging = 0;
                screen->SetUpDeploy();
                ShowMessage(text);
                return;
            }

            Mech->Deployed = -1;
            GlobalLogPtr->AssignedWarriorList->SetDeployed(Mech->PilotIndex, -1);
        }

        screen->SetUpDeploy();
    }

    screen->AddChild(GlobalLogPtr->DragIcon);
    GlobalLogPtr->DragIcon->ShowGuiWindow(-1);
    GlobalLogPtr->DragIcon->SetDepth(100);
}

auto MCMechBriefBlock::DrawBackground() -> void
{
    // Port: the block is drawn each frame by its parent (PaintBlock): the screen into its slot, or the deploy pane.
    if (Parent != nullptr && Parent == GlobalLogPtr->BriefingScreen)
    {
        GlobalLogPtr->BriefingScreen->PlaceInSlot(this);
    }
}

auto MCMechBriefBlock::OnBeginDrag(MCLogPort* surface) -> void
{
    MCPane* target = surface->Frame();
    MCBriefingScreen* screen = GlobalLogPtr->BriefingScreen;

    if (Parent != nullptr && Parent == screen)
    {
        // In a drop slot: framed, over the empty slot.
        ::FillBox(target, 0, 0, 0x34, 0x2e, 0x10);

        if (screen->EmptySlot != nullptr)
        {
            screen->EmptySlot->CopyTo(target, 0, 0, -1);
        }

        PaintBlock(target, 0, 0, true);
        return;
    }

    // The deploy pane is colour 0x10 around its blocks.
    VfxPaneWipe(target, 0x10);
    PaintBlock(target, 0, 0, false);
}

auto MCMechBriefBlock::PaintBlock(MCPane* target, int32_t xPos, int32_t yPos, bool framed) -> void
{
    // Drawn in place: the original put the block together in a picture of its art and copied it keyed on 0xff.
    MCLogPort* art = LogArtf("%slogart\\%s", ArtPath, Mech == nullptr ? "lscupv00.tga" : "lscupm00.tga");
    MCLogBlockPort port(target, xPos, yPos, art != nullptr ? art->Width() : 1, art != nullptr ? art->Height() : 1,
                        true);

    if (art != nullptr)
    {
        VfxPaneCopy(art->Frame(), 0, 0, port.Frame(), 0, 0, -1);
    }

    char text[256];

    if (Mech == nullptr)
    {
        std::snprintf(text, sizeof(text), "%s", Vehicle->FileName);
        const int32_t textWidth = GreenFont->Width(reinterpret_cast<uint8_t*>(text));
        GreenFont->WriteString(port.Frame(), (Width() - textWidth) / 2, 3, reinterpret_cast<uint8_t*>(text), -1);
        MCLogBlockPort body(port.Frame(), 0, 0, port.Width(), port.Height(), true);

        for (int32_t location = 0; location < 5; location++)
        {
            GlobalLogPtr->DrawVehicleBodyLoc(Vehicle, location, &body, 0xd, 0xe);
        }
    }
    else
    {
        MCLogWarrior* warrior = nullptr;

        if (Mech->LocalPart == 0)
        {
            warrior = Mech->NetworkPilot;
        }
        else
        {
            GlobalLogPtr->AssignedWarriorList->GetWarriorInfo(Mech->PilotIndex, warrior);
        }

        // Port fix: a mech without a pilot shows no callsign (the original read through null).
        std::snprintf(text, sizeof(text), "%s", warrior != nullptr ? warrior->Callsign : "");
        const int32_t textWidth = GreenFont->Width(reinterpret_cast<uint8_t*>(text));
        GreenFont->WriteString(port.Frame(), (Width() - textWidth) / 2, 3, reinterpret_cast<uint8_t*>(text), -1);

        {
            MCLogBlockPort body(port.Frame(), 2, 0xe, 0x19, 0x1e, true);
            VfxPaneWipe(body.Frame(), 0x10);

            for (int32_t location = 0; location < 8; location++)
            {
                GlobalLogPtr->DrawMechBodyLoc(Mech, location, &body, 0, 0);
            }
        }

        // The pilot's picture.
        if (warrior == nullptr)
        {
            std::snprintf(text, sizeof(text), "%spilot%02d.tga", ArtPath, Mech->PilotIndex);
        }
        else
        {
            std::snprintf(text, sizeof(text), "%s%s", ArtPath, warrior->Picture);
        }

        if (MCLogPort* picture = LogArt(text))
        {
            picture->CopyTo(port.Frame(), 0x1c, 0xe, -1);
        }

        // The mech's status bar (green, yellow, red) and the pilot's health bar.
        VfxLineDraw(port.Frame(), 3, 0xb, 0x19, 0xb, 0x12);
        VfxLineDraw(port.Frame(), 3, 0xc, 0x19, 0xc, 0x12);
        const float status = Mech->StatusValue;

        if (status != 0.0f)
        {
            uint32_t color;

            if (!(status < 0.5))
            {
                color = 0xb;
            }
            else if (status > 0.2)
            {
                color = 0xf2;
            }
            else
            {
                color = 0xef;
            }

            const int32_t barEnd = static_cast<int32_t>(status * 23.0f) + 3;
            VfxLineDraw(port.Frame(), 3, 0xb, barEnd, 0xb, color);
            VfxLineDraw(port.Frame(), 3, 0xc, barEnd, 0xc, color);
        }

        if (warrior != nullptr && warrior->Health < 6.0f)
        {
            const auto healthEnd = static_cast<int32_t>(warrior->Health * 3.0f + 31.0f);
            VfxLineDraw(port.Frame(), healthEnd, 0xb, 0x2f, 0xb, 0x10);
            VfxLineDraw(port.Frame(), static_cast<int32_t>(warrior->Health * 3.0f + 31.0f), 0xc, 0x2f, 0xc, 0x10);
        }
    }

    if (MPlayer != nullptr)
    {
        // Another player's unit is darkened.
        const int32_t index = Mech != nullptr ? GlobalLogPtr->ForceMechList->GetMechIndex(Mech)
                                              : GlobalLogPtr->ForceVehicleList->GetVehicleIndex(Vehicle);

        if (index < 0)
        {
            GlobalLogPtr->Darken(0, LogisticFadetable, &port);
        }
    }

    if (framed)
    {
        // In a slot: a bevelled frame.
        VfxLineDraw(port.Frame(), 0, 0, Width() - 1, 0, 0x32);
        VfxLineDraw(port.Frame(), 0, 1, 0, Height() - 2, 0x32);
        VfxLineDraw(port.Frame(), 0, Height() - 1, Width() - 1, Height() - 1, 0x15);
        VfxLineDraw(port.Frame(), Width() - 1, 0, Width() - 1, Height() - 2, 0x15);
    }
}
