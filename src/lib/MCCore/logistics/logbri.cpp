#include "stdafx.h"
#include "logistics/logbri.h"
#include "gui/afont.h"
#include "gui/awindow.h"
#include "gui/scrlpane.h"
#include "gui/updisp.h"
#include "lib/aerror.h"
#include "lib/cident.h"
#include "lib/heap.h"
#include "lib/inifile.h"
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
#include "object/cmponent.h"
#include "platform/MCInput.h"
#include "platform/MCSmacker.h"
#include "sound/soundsys.h"
#include "vfx/vfxfuncs.h"

char artPath[80] = {}; // 80 bytes (0x007ab080..0x007ab0d0); gui\asystem.cpp's RealWinMain sets it.
int32_t curDeployTonnage = 0;
int32_t maxDeployTonnage = 0;

namespace
{
    /// <summary>The colour of the drop zone markers and their leader lines on the briefing map.</summary>
    constexpr uint32_t DropZoneColor = 0x1f;

    /// <summary>Set while the left button is down on the briefing screen (DAT_008080b0); only written.</summary>
    int32_t mouseDown = 0;

    /// <summary>Set while a unit block is dragged with the left button (DAT_008080b4).</summary>
    int32_t leftDragging = 0;
    /// <summary>Set while a unit block is dragged with the right button (DAT_008080b8).</summary>
    int32_t rightDragging = 0;
    /// <summary>The drag icon's position (DAT_008080bc / DAT_008080c0).</summary>
    int32_t dragX = 0;
    int32_t dragY = 0;
    /// <summary>Set when the dragged block was picked up from a drop slot, not the deploy pane (DAT_008080c4).</summary>
    int32_t draggedFromSlot = 0;

    /// <summary>Puts string <paramref name="id"/> on the ticker.</summary>
    void showHelp(uint32_t id)
    {
        char text[256];
        cLoadString(thisInstance, id, text, 0xfe);
        globalLogPtr->ticker->setString(text);
    }

    /// <summary>Shows <paramref name="text"/> in the one-button message dialog.</summary>
    void showMessage(char* text)
    {
        globalLogPtr->messageDialog->setText(text);
        globalLogPtr->messageDialog->setTwoButton(0);
        ReusableDialog* dialog = globalLogPtr->messageDialog;
        dialog->callback = nullptr;
        char upArt[] = "bh_okay.tga";
        char downArt[] = "bg_okay.tga";
        dialog->okButton->setUpPicture(upArt);
        globalLogPtr->messageDialog->okButton->setDownPicture(downArt);
        lDialogButton* button = globalLogPtr->messageDialog->okButton;
        button->disabled = 0;
        button->draw();
        globalLogPtr->messageDialog->activate();
    }

    /// <summary>Shows string <paramref name="id"/> in the one-button message dialog.</summary>
    void showMessage(uint32_t id)
    {
        char text[256];
        cLoadString(thisInstance, id, text, 0xfe);
        showMessage(text);
    }

    /// <summary>Makes <paramref name="port"/> a copy of <c>logart\</c><paramref name="name"/> (from the art cache), to paint on.</summary>
    void copyArt(lPort* port, const char* name)
    {
        lPort* art = logArtf("%slogart\\%s", artPath, name);

        if (art == nullptr)
        {
            port->init(1, 1, -1);
            return;
        }

        port->init(art->width(), art->height(), -1);
        VFX_pane_copy(art->frame(), 0, 0, port->frame(), 0, 0, -1);
    }

    /// <summary>
    /// Marks drop zone <paramref name="zone"/> on the briefing map (whose tac map picture fills (0x159, 0x1a) ..
    /// (0x273, 0x134), rotated 45 degrees): a leader line from the zone's label at height <paramref name="lineY"/> to
    /// a small cross at the zone. <paramref name="scale"/> is world units per map pixel.
    /// </summary>
    void drawDropZoneMarker(PANE* pane, const Logistics::DropZonePosition& zone, float scale, int32_t lineY)
    {
        constexpr float R = 0.70710677f;
        const float x = zone.x;
        const float yR = zone.y * R;
        const auto diff = static_cast<float>(yR - static_cast<double>(x) * R);
        const double across = (static_cast<double>(x) * R + yR) / scale;
        const auto down = static_cast<float>(diff / scale);
        const auto screenX = static_cast<float>(across + 490.0);
        const auto screenY = static_cast<float>(141.0 - down + 31.0);

        const auto bend = static_cast<int32_t>((screenX - 351.0f) * 0.5f + 351.0f);
        VFX_line_draw(pane, 0x157, lineY, bend, lineY, LD_DRAW, DropZoneColor);
        const auto pointX = static_cast<int32_t>(screenX);
        const auto pointY = static_cast<int32_t>(screenY);
        VFX_line_draw(pane, bend, lineY, pointX, pointY, LD_DRAW, DropZoneColor);
        VFX_line_draw(pane, static_cast<int32_t>(screenX - 2.0f), pointY, static_cast<int32_t>(screenX + 2.0f), pointY,
                      LD_DRAW, DropZoneColor);
        VFX_line_draw(pane, pointX, static_cast<int32_t>(screenY - 2.0f), pointX, static_cast<int32_t>(screenY + 2.0f),
                      LD_DRAW, DropZoneColor);
        const auto top = static_cast<int32_t>(screenY - 1.0f);
        const auto left = static_cast<int32_t>(screenX - 1.0f);
        AG_pixel_write(pane, left, top, DropZoneColor);
        const auto right = static_cast<int32_t>(screenX + 1.0f);
        AG_pixel_write(pane, right, top, DropZoneColor);
        const auto bottom = static_cast<int32_t>(screenY + 1.0f);
        AG_pixel_write(pane, right, bottom, DropZoneColor);
        AG_pixel_write(pane, left, bottom, DropZoneColor);
    }

    /// <summary>
    /// Adds up the tonnage of the units in <paramref name="lance"/>'s drop slots (the original's running
    /// <c>__ftol</c> of an int plus the float tonnage).
    /// </summary>
    int32_t lanceTonnage(int32_t lance)
    {
        int32_t tons = 0;

        for (const auto& slot : globalLogPtr->deploySlots[lance])
        {
            if (slot.unit >= 0)
            {
                LogMech* mech = nullptr;
                globalLogPtr->forceMechList->getMechInfo(slot.unit, mech);
                tons = static_cast<int32_t>(static_cast<float>(tons) + mech->curTonnage);
            }
            else if (slot.vehicle >= 0)
            {
                LogVehicle* vehicle = nullptr;
                globalLogPtr->forceVehicleList->getVehicleInfo(slot.vehicle, vehicle);
                tons = static_cast<int32_t>(static_cast<float>(tons) + vehicle->curTonnage);
            }
        }

        return tons;
    }

    /// <summary>The unit's tonnage added to (or, with <paramref name="sign"/> -1, taken from) <c>curDeployTonnage</c>.</summary>
    int32_t deployTonnageWith(const LogPart* part, float sign)
    {
        return static_cast<int32_t>(static_cast<float>(curDeployTonnage) + sign * part->curTonnage);
    }

    /// <summary>
    /// Adds <paramref name="layer"/> on top of <paramref name="layers"/>. An earlier copy of the same picture is dropped:
    /// it is hidden wherever the new one draws, and shows nothing where the new one is keyed.
    /// </summary>
    void pushLayer(std::vector<BriefingScreen::LookLayer>& layers, const BriefingScreen::LookLayer& layer)
    {
        std::erase_if(
            layers, [&layer](const BriefingScreen::LookLayer& old)
            { return old.art == layer.art && old.x == layer.x && old.y == layer.y && old.keyed == layer.keyed; });
        layers.push_back(layer);
    }

    /// <summary>Fills a <paramref name="width"/> by <paramref name="height"/> box of <paramref name="target"/> with <paramref name="color"/>.</summary>
    void fillBox(PANE* target, int32_t xPos, int32_t yPos, int32_t width, int32_t height, int32_t color)
    {
        auto* box = new lPort;
        box->init(width, height, 1);
        VFX_pane_wipe(box->frame(), color);
        VFX_pane_copy(box->frame(), 0, 0, target, xPos, yPos, -1);
        delete box;
    }

    /// <summary>Whether <paramref name="part"/> fits under the drop tonnage limit.</summary>
    bool fitsTonnage(const LogPart* part)
    {
        return !(static_cast<float>(maxDeployTonnage) < static_cast<float>(curDeployTonnage) + part->curTonnage);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// BriefingScreen
// ---------------------------------------------------------------------------------------------------------------------

auto BriefingScreen::init() -> void
{
    currentTab = 1;
    curDeployTonnage = 0;
    briefingBox = nullptr;
    playMovie = 0;
    buttonsLocked = 0;
    chatBlinking = 0;
    chatBlinkOn = 0;
    chatTimerOn = 0;
    int32_t result = lObject::init(0, 0, 0x280, 0x1e0, nullptr, nullptr);
    Assert(result == 0, result, "Unable to init briefing screen", nullptr);
    // The original loaded the background (lsbbk00) as the screen's picture; the screen draws it each frame.
    char fileName[256];

    auto* pane = new ScrollPane;

    if (pane != nullptr)
    {
        pane->init();
    }

    missionPane = pane;
    Assert(pane != nullptr, 0, " Not enough memory for missionScroll ", nullptr);
    std::snprintf(fileName, sizeof(fileName), "%slogart\\lsbbk01.tga", artPath);
    pane->init(0xb9, 0xdb, 7, 0x6b, fileName);
    pane->setDisplayPort(nullptr, -1, -1);

    auto* deploy = new ScrollPane;

    if (deploy != nullptr)
    {
        deploy->init();
    }

    deployPane = deploy;
    Assert(deploy != nullptr, 0, " Not enough memory for deployScroll ", nullptr);
    deploy->init(0xc4, 0x7a, 7, 0x15e, static_cast<char*>(nullptr));

    missionPort = new lPort;
    Assert(missionPort != nullptr, 0, " Not enough memory for missionPort ", nullptr);
    missionPort->init(0xad, 0xdd, -1);
    VFX_pane_wipe(missionPort->frame(), 0x10);

    char artName[256];
    chatBlinkPort = new lPort;
    Assert(chatBlinkPort != nullptr, 0, " Not enough memory for chatBlinker ", nullptr);
    std::snprintf(artName, sizeof(artName), "%slogart\\lsbdw08.tga", artPath);
    chatBlinkPort->init(artName);
    chatRegularPort = new lPort;
    Assert(chatRegularPort != nullptr, 0, " Not enough memory for chatRegular ", nullptr);
    std::snprintf(artName, sizeof(artName), "%slogart\\lsbdw03.tga", artPath);
    chatRegularPort->init(artName);

    operationPicture = nullptr;
    undeployedMechs = nullptr;
    addChild(pane);
    addChild(deployPane);
    ShowGUIWindow(0);
    screenWindow->addChild(this);

    // Two columns of six slots; each lance is two rows.
    static constexpr int32_t SlotTops[6] = {0x28, 0x58, 0x98, 0xc8, 0x108, 0x138};

    for (int32_t i = 0; i < 12; i++)
    {
        const int32_t left = (i & 1) != 0 ? 0x115 : 0xdf;
        const int32_t top = SlotTops[i >> 1];
        slotRects[i] = {left, top, left + 0x34, top + 0x2e};
    }

    smacker = nullptr;
    smackerWindow = nullptr;
    emptySlot = nullptr;
}

auto BriefingScreen::drawBackground() -> void
{
    // Port: the screen is drawn each frame (PaintLook) from what is set here and after.
    lPort* back = logArtf("%slogart\\lsbbk00.tga", artPath);
    ClearLook();
    chrome.Clear();

    if (emptySlot == nullptr && back != nullptr)
    {
        emptySlot = new lPort;
        emptySlot->init(0x34, 0x2e, -1);
        VFX_pane_wipe(emptySlot->frame(), 0xff);
        VFX_pane_copy(back->frame(), slotRects[0].left, slotRects[0].top, emptySlot->frame(), 0, 0, -1);
    }

    // The slots that can't be filled are covered.
    for (int32_t i = 0; i < 12; i++)
    {
        if (globalLogPtr->localDropSlot[i] == 0)
        {
            CoverSlot(i, SlotCover::Covered);
        }
    }

    // The mission's tac map picture, turned 45 degrees onto the map area.
    FullPathFileName mapName;
    mapName.init(terrainPath, globalLogPtr->missionFileName, ".log.tga");
    FullPathFileName fitName;
    fitName.init(terrainPath, globalLogPtr->missionFileName, ".fit");
    FitIniFile terrainFile;
    int32_t result = terrainFile.open(fitName, READ, 0x32);
    Assert(result == 0, result, "Could not find terrain file from .TGA ", nullptr);
    result = terrainFile.seekBlock("TerrainData");
    Assert(result == 0, result, "Could not find TerrainData block in terrain .FIT file", nullptr);
    int32_t verticesBlockSide = 0;
    result = terrainFile.readIdLong("VerticesBlockSide", verticesBlockSide);
    Assert(result == 0, result, "Could not find variable VerticesBlockSide in terrain .FIT file", nullptr);
    int32_t blocksMapSide = 0;
    result = terrainFile.readIdLong("BlocksMapSide", blocksMapSide);
    Assert(result == 0, result, "Could not find variable BlocksMapSide in terrain .FIT file", nullptr);
    float metersPerVertex = 0.0f;
    result = terrainFile.readIdFloat("MetersPerVertex", metersPerVertex);
    Assert(result == 0, result, "Could not find variable MetersPerVertex in terrain .FIT file", nullptr);
    const int32_t mapSide = blocksMapSide * verticesBlockSide;
    terrainFile.close();
    delete mapPicture;
    mapPicture = new lPort;
    result = mapPicture->init(static_cast<char*>(mapName));

    if (result != 0)
    {
        Fatal(result, " Unable to create Port for TacMap ", nullptr);
    }

    labelShown[0] = true;

    if (MPlayer != nullptr)
    {
        // Each lance's label, with a line to its drop zone on the map.
        const double side = static_cast<double>(mapSide);
        markerScale = static_cast<float>(std::sqrt(side * side + side * side) * metersPerVertex *
                                         static_cast<double>(0.0017667845f));
        markedZone = MPlayer->homeTeam == 1 ? 3 : 0;
        labelShown[1] = true;
        labelShown[2] = true;
    }

    setUpMission();
}

auto BriefingScreen::ClearLook() -> void
{
    markedZone = -1;
    tonnageShown = false;
    launchArt = nullptr;
    launchOnTop = false;
    tabLayers.clear();
    operationLayers.clear();

    for (int32_t lance = 0; lance < 3; lance++)
    {
        labelShown[lance] = false;
        labelTons[lance] = -1;
    }

    for (int32_t i = 0; i < 12; i++)
    {
        for (const SlotLayer& layer : slotLayers[i])
        {
            delete layer.picture;
        }

        slotLayers[i].clear();
        slotBlocks[i] = nullptr;
        slotMarks[i] = 0;
        delete slotPictures[i];
        slotPictures[i] = nullptr;
    }

    boxBlank = false;
    delete boxRemnant;
    boxRemnant = nullptr;
    boxShown = nullptr;
    delete boxPicture;
    boxPicture = nullptr;
}

auto BriefingScreen::SlotAt(int32_t xPos, int32_t yPos) const -> int32_t
{
    for (int32_t i = 0; i < 12; i++)
    {
        if (slotRects[i].left == xPos && slotRects[i].top == yPos)
        {
            return i;
        }
    }

    return -1;
}

auto BriefingScreen::CoverSlot(int32_t slot, SlotCover cover, lPort* picture) -> void
{
    if (slot < 0)
    {
        delete picture;
        return;
    }

    slotLayers[slot].push_back({cover, picture});
}

auto BriefingScreen::PlaceInSlot(MechBriefBlock* block) -> void
{
    const int32_t slot = SlotAt(block->x(), block->y());

    if (slot < 0)
    {
        return;
    }

    if (slotBlocks[slot] != nullptr && slotBlocks[slot] != block)
    {
        // The new block was painted over the old one.
        LeaveRemnant(slot);
    }

    slotBlocks[slot] = block;
    slotMarks[slot] = slotLayers[slot].size();
    delete slotPictures[slot];
    auto* picture = new lPort;
    picture->init(0x34, 0x2e, -1);
    VFX_pane_wipe(picture->frame(), 0xff);
    block->PaintBlock(picture->frame(), 0, 0, true);
    slotPictures[slot] = picture;
}

auto BriefingScreen::LiftFromSlot(MechBriefBlock* block) -> void
{
    const int32_t slot = SlotAt(block->x(), block->y());

    if (slot < 0)
    {
        return;
    }

    CoverSlot(slot, SlotCover::Empty);

    if (slotBlocks[slot] == block)
    {
        slotBlocks[slot] = nullptr;
        delete slotPictures[slot];
        slotPictures[slot] = nullptr;
    }
}

auto BriefingScreen::ShowBox(BriefingBox* box) -> void
{
    boxBlank = false;
    delete boxRemnant;
    boxRemnant = nullptr;
    boxShown = box;
    delete boxPicture;
    boxPicture = new lPort;
    boxPicture->init(0x1ab, 0x6f, -1);
    box->PaintBox(boxPicture->frame(), 0, 0);
}

auto BriefingScreen::BlankBox() -> void
{
    boxBlank = true;
    delete boxRemnant;
    boxRemnant = nullptr;
    boxShown = nullptr;
    delete boxPicture;
    boxPicture = nullptr;
}

auto BriefingScreen::LeaveRemnant(int32_t slot) -> void
{
    // Under whatever was drawn over the slot since the block was.
    auto& layers = slotLayers[slot];
    const size_t mark = std::min(slotMarks[slot], layers.size());
    layers.insert(layers.begin() + static_cast<std::ptrdiff_t>(mark), {SlotCover::Remnant, slotPictures[slot]});
    slotPictures[slot] = nullptr;
    slotBlocks[slot] = nullptr;
}

auto BriefingScreen::removeChild(aObject* child) -> void
{
    for (int32_t i = 0; i < 12; i++)
    {
        if (child != nullptr && slotBlocks[i] == child)
        {
            LeaveRemnant(i);
        }
    }

    if (child != nullptr && child == boxShown)
    {
        delete boxRemnant;
        boxRemnant = boxPicture;
        boxPicture = nullptr;
        boxShown = nullptr;
    }

    lObject::removeChild(child);
}

auto BriefingScreen::PaintLook(PANE* target) -> void
{
    if (lPort* back = logArtf("%slogart\\lsbbk00.tga", artPath))
    {
        VFX_pane_copy(back->frame(), 0, 0, target, 0, 0, -1);
    }

    if (mapPicture != nullptr)
    {
        WINDOW* map = mapPicture->frame()->window;
        SCRNVERTEX vertices[4] = {};
        vertices[0] = {0x159, 0x1a, 0, 0, 0, 0};
        vertices[1] = {0x273, 0x1a, 0, map->x_max << 16, 0, 0};
        vertices[2] = {0x273, 0x134, 0, map->x_max << 16, map->y_max << 16, 0};
        vertices[3] = {0x159, 0x134, 0, 0, map->y_max << 16, 0};
        VFX_map_polygon(target, 4, vertices, map, MP_XP);
    }

    // The lance labels, with their tonnage.
    static constexpr int32_t LabelTops[3] = {0x18, 0x88, 0xf8};

    for (int32_t lance = 0; lance < 3; lance++)
    {
        if (!labelShown[lance])
        {
            continue;
        }

        if (lPort* art = logArtf("%slogart\\lsbdf0%d.tga", artPath, lance + 2))
        {
            art->copyTo(target, 0xd6, LabelTops[lance], -1);
        }

        if (labelTons[lance] >= 0)
        {
            char text[32];
            std::snprintf(text, sizeof(text), "%d", labelTons[lance]);
            blackFont->writeString(target, labelTextX[lance], LabelTops[lance] + 4, reinterpret_cast<uint8_t*>(text),
                                   -1);
        }
    }

    if (markedZone >= 0)
    {
        static constexpr int32_t MarkerLines[3] = {0x24, 0x94, 0x104};

        for (int32_t zone = 0; zone < 3; zone++)
        {
            drawDropZoneMarker(target, globalLogPtr->dropZonePositions[markedZone + zone], markerScale,
                               MarkerLines[zone]);
        }
    }

    // The tonnage bar and the launch button.
    if (launchArt != nullptr && !launchOnTop)
    {
        launchArt->copyTo(target, 0x20d, 0x148, -1);
    }

    if (tonnageShown)
    {
        PaintTonnageBar(target, shownMaxTonnage, shownTonnage, shownHammerDown);
    }

    if (launchArt != nullptr && launchOnTop)
    {
        launchArt->copyTo(target, 0x20d, 0x148, -1);
    }

    auto paintLayer = [this, target](const LookLayer& layer)
    {
        lPort* art = layer.art != nullptr ? layer.art : operationPicture;

        if (art != nullptr)
        {
            art->copyTo(target, layer.x, layer.y, layer.keyed ? -1 : 0);
        }
    };

    for (const LookLayer& layer : tabLayers)
    {
        paintLayer(layer);
    }

    for (const LookLayer& layer : operationLayers)
    {
        paintLayer(layer);
    }

    // The drop slots: what was drawn over each, and the block in it.
    for (int32_t i = 0; i < 12; i++)
    {
        const RECT& area = slotRects[i];
        const auto& layers = slotLayers[i];

        for (size_t n = 0; n <= layers.size(); n++)
        {
            if (slotBlocks[i] != nullptr && n == std::min(slotMarks[i], layers.size()))
            {
                slotBlocks[i]->PaintBlock(target, area.left, area.top, true);
            }

            if (n == layers.size())
            {
                break;
            }

            switch (layers[n].cover)
            {
                case SlotCover::Covered:
                {
                    if (lPort* art = logArtf("%slogart\\lsbdf06.tga", artPath))
                    {
                        art->copyTo(target, area.left + 1, area.top + 1, -1);
                    }
                    break;
                }
                case SlotCover::Blank:
                    fillBox(target, area.left, area.top, 0x32, 0x2c, 0x10);
                    break;
                case SlotCover::Empty:
                {
                    fillBox(target, area.left, area.top, 0x34, 0x2e, 0x10);

                    if (emptySlot != nullptr)
                    {
                        emptySlot->copyTo(target, area.left, area.top, -1);
                    }
                    break;
                }
                case SlotCover::Remnant:
                {
                    if (layers[n].picture != nullptr)
                    {
                        layers[n].picture->copyTo(target, area.left, area.top, -1);
                    }
                    break;
                }
            }
        }
    }

    // The briefing box area.
    if (boxBlank)
    {
        fillBox(target, 0xd3, 0x16f, 0x1aa, 0x6e, 0x10);
    }

    if (boxRemnant != nullptr)
    {
        VFX_pane_copy(boxRemnant->frame(), 0, 0, target, 0xd3, 0x16f, -1);
    }

    if (boxShown != nullptr)
    {
        boxShown->PaintBox(target, 0xd3, 0x16f);
    }
}

auto BriefingScreen::NewLookPicture() -> lPort*
{
    auto* picture = new lPort;
    picture->init(width(), height(), -1);
    PaintLook(picture->frame());
    globalLogPtr->drawScreenChrome(this, picture->frame());
    return picture;
}

auto BriefingScreen::display() -> void
{
    if (showWindow == 0)
    {
        return;
    }

    lObject::display();

    if (movieOver == 0)
    {
        if (globalLogPtr->autoPlayMovie != 0)
        {
            if (movieStarted != 0)
            {
                setUpOperation();
                return;
            }

            movieStarted = 1;
        }
    }
    else if (InDemo == 0)
    {
        // The movie is over: its window goes, and the last operation picture stays.
        playMovie = 0;
        removeChild(smackerWindow);
        pushLayer(operationLayers, {logArtf("%slogart\\lsb_op6.tga", artPath), 0xc, 0x6f, true});
    }
}

auto BriefingScreen::draw() -> void
{
    // The original drew nothing here; the screen now draws its picture and the shared places in the frame pass.
    if (lport()->viewOpen())
    {
        PaintLook(lport()->frame());
        globalLogPtr->drawScreenChrome(this, lport()->frame());
    }
}

auto BriefingScreen::destroy() -> void
{
    StopSmackerMovies();
    screenWindow->removeChild(this);
    ClearLook();
    delete mapPicture;
    mapPicture = nullptr;
    delete chatBlinkPort;
    chatBlinkPort = nullptr;
    delete chatRegularPort;
    chatRegularPort = nullptr;
    delete emptySlot;
    emptySlot = nullptr;
    delete[] undeployedMechs;
    undeployedMechs = nullptr;
    delete missionPort;
    missionPort = nullptr;

    if (smacker != nullptr)
    {
        // Port: the original freed the handle from the logistics heap; the port's handles come from SmackOpen.
        SmackClose(smacker);
        smacker = nullptr;
    }

    delete smackerWindow;
    smackerWindow = nullptr;
    delete deployPane;
    deployPane = nullptr;

    if (missionPane != nullptr)
    {
        missionPane->setDisplayPort(nullptr, 0, -1);
        delete missionPane;
        missionPane = nullptr;
    }

    delete operationPicture;
    operationPicture = nullptr;
    lObject::destroy();
}

auto BriefingScreen::mpCalcTonnages() -> void
{
    curDeployTonnage = 0;

    for (int32_t lance = 0; lance < 3; lance++)
    {
        int32_t tons = 0;
        bool occupied = lance == 1 && MPlayer->playersOnHomeTeam()->count == 2;

        for (int32_t slot = 0; slot < 4 && !occupied; slot++)
        {
            const auto& deploy = globalLogPtr->deploySlots[lance][slot];

            if (deploy.unit >= 0)
            {
                LogMech* mech = nullptr;
                globalLogPtr->forceMechList->getMechInfo(deploy.unit, mech);
                occupied = mech != nullptr;
            }
            else if (deploy.vehicle >= 0)
            {
                // Original behaviour (OB-082): the vehicle looked up is lance 0's, whatever the lance.
                LogVehicle* vehicle = nullptr;
                globalLogPtr->forceVehicleList->getVehicleInfo(globalLogPtr->deploySlots[0][slot].vehicle, vehicle);
                occupied = vehicle != nullptr;
            }
        }

        if (occupied)
        {
            tons = lanceTonnage(lance);
            curDeployTonnage += tons;
        }

        char text[256];
        std::snprintf(text, sizeof(text), "%d", tons);
        showLabel(lance, tons, blueFont->width(reinterpret_cast<uint8_t*>(text)));
    }

    drawTonnageBar();
}

auto BriefingScreen::calcTonnages() -> void
{
    if (MPlayer != nullptr)
    {
        mpCalcTonnages();
        return;
    }

    int32_t tons = lanceTonnage(0);
    curDeployTonnage = tons;
    char text[256];
    std::snprintf(text, sizeof(text), "%d", tons);
    showLabel(0, tons, blackFont->width(reinterpret_cast<uint8_t*>(text)));

    if (globalLogPtr->localDropSlot[4] != 0)
    {
        tons = lanceTonnage(1);
        std::snprintf(text, sizeof(text), "%d", tons);
        showLabel(1, tons, blueFont->width(reinterpret_cast<uint8_t*>(text)));
        curDeployTonnage += tons;

        if (globalLogPtr->localDropSlot[8] != 0)
        {
            tons = lanceTonnage(2);
            std::snprintf(text, sizeof(text), "%d", tons);
            showLabel(2, tons, blueFont->width(reinterpret_cast<uint8_t*>(text)));
            curDeployTonnage += tons;
        }
    }

    drawTonnageBar();
}

auto BriefingScreen::showLabel(int32_t lance, int32_t tons, int32_t textWidth) -> void
{
    labelShown[lance] = true;
    labelTons[lance] = tons;
    labelTextX[lance] = 0x13f - textWidth;
}

auto BriefingScreen::drawTonnageBar() -> void
{
    // Port: drawn each frame (PaintTonnageBar) with the figures of now.
    tonnageShown = true;
    shownMaxTonnage = maxDeployTonnage;
    shownTonnage = curDeployTonnage;
    shownHammerDown = globalLogPtr->hammerDown != 0;

    if (buttonsLocked == 0)
    {
        // The launch button: lit when the force can drop.
        bool ready = curDeployTonnage >= 1;

        if (ready && (maxDeployTonnage < curDeployTonnage || globalLogPtr->requiredAssigned() == 0))
        {
            ready = globalLogPtr->hammerDown != 0;
        }

        launchArt = logArtf(ready ? "%slogart\\lsbdf00.tga" : "%slogart\\lsbdf00a.tga", artPath);
        launchOnTop = true;
    }
    else
    {
        launchOnTop = false;
    }
}

auto BriefingScreen::PaintTonnageBar(PANE* target, int32_t maxTons, int32_t tons, bool hammerDown) -> void
{
    if (lPort* art = logArtf("%slogart\\lsbdf07.tga", artPath))
    {
        art->copyTo(target, 0x157, 0x13f, 0);
    }

    char text[256];
    std::snprintf(text, sizeof(text), "%d", maxTons);
    int32_t width = blueFont->width(reinterpret_cast<uint8_t*>(text));
    aFont* font = !hammerDown ? yellowDropFont : redFont;
    font->writeString(target, 0x1d9 - width, 0x13f, reinterpret_cast<uint8_t*>(text), -1);
    std::snprintf(text, sizeof(text), "%d", tons);

    if (maxTons < tons)
    {
        width = redFont->width(reinterpret_cast<uint8_t*>(text));
        font = redFont;
    }
    else
    {
        width = blueFont->width(reinterpret_cast<uint8_t*>(text));
        font = yellowDropFont;
    }

    font->writeString(target, 0x1d9 - width, 0x149, reinterpret_cast<uint8_t*>(text), -1);

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

        auto* bar = new lPort;
        bar->init(barWidth, 0xe, -1);

        if (lPort* art = logArtf("%slogart\\lsbdf05.tga", artPath))
        {
            art->copyTo(bar->frame(), 0, 0, 0);
        }

        bar->copyTo(target, 0x15b, 0x155, -1);
        delete bar;
    }
}

auto BriefingScreen::handleEvent(aEvent* event) -> void
{
    if (globalLogPtr->currentScreen != this)
    {
        return;
    }

    const int32_t xPos = event->x;
    const int32_t yPos = event->y;

    if (event->key == 0 && event->type != 0x13)
    {
        // The help line for whatever the mouse is over, and the highlighted screen button.
        application->SetCurrentCursor(static_cast<CursorType>(0));
        globalLogPtr->drawScreenButtons();
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
                    if (globalLogPtr->localDropSlot[lance * 4 + slot] == 0)
                    {
                        break;
                    }

                    if (PtInRect(&globalLogPtr->briefingScreen->slotRects[lance * 4 + slot], point) != 0)
                    {
                        const auto& deploy = globalLogPtr->deploySlots[lance][slot];
                        return deploy.unit < 0 && deploy.vehicle < 0 ? 0x20 : 0x21;
                    }
                }
            }

            return -1;
        };

        if (inside(2, 2, 0xd0, 0xd))
        {
            showHelp(0x1d);
        }
        else if (inside(2, 0x10, 0xd0, 0x21))
        {
            showHelp(0x286);
            lPort* highlight =
                MPlayer == nullptr ? globalLogPtr->screenButtonPorts[0][1] : globalLogPtr->screenButtonPorts[1][1];
            globalLogPtr->litScreenButton(this, 0, highlight);
        }
        else if (inside(2, 0x22, 0xd0, 0x33))
        {
            showHelp(0x1e);
        }
        else if (inside(2, 0x34, 0xd0, 0x45))
        {
            showHelp(0x41);

            if (buttonsLocked == 0)
            {
                globalLogPtr->litScreenButton(this, 2, globalLogPtr->screenButtonPorts[3][1]);
            }
        }
        else if (inside(2, 0x46, 0xd0, 0x57))
        {
            showHelp(0x42);

            if (buttonsLocked == 0)
            {
                globalLogPtr->litScreenButton(this, 3, globalLogPtr->screenButtonPorts[4][1]);
            }
        }
        else if (inside(0x20c, 2, 0x24d, 0xd))
        {
            showHelp(0x1f);
        }
        else
        {
            const int32_t slotText = inside(0xde, 0x1a, 0x14a, 0x164) ? slotHelp() : -1;

            if (slotText >= 0)
            {
                showHelp(static_cast<uint32_t>(slotText));
            }
            else if (inside(0x20d, 0x148, 0x26a, 0x15a))
            {
                showHelp(0x22);
            }
            else if (inside(0xd3, 0x16f, 0x27f, 0x1df))
            {
                showHelp(0x23);
            }
            else if (inside(7, 0x15d, 0xcb, 0x1d8))
            {
                showHelp(0x24);
            }
            else if (inside(0xc2, 0x67, 0xcf, 0xde))
            {
                showHelp(currentTab == 1 ? 0x26 : 0x27);
            }
            else if (inside(0xc2, 0xe0, 0xcf, 0x14a))
            {
                if (currentTab != 2)
                {
                    showHelp(0x28);
                }
            }
            else
            {
                globalLogPtr->ticker->setString(nullptr);
            }
        }
    }

    if (event->type == 9)
    {
        // Ctrl+Alt+= : a thousand resource points.
        const bool ctrlAlt = MCInput::GetAsyncKeyState(VK_CONTROL) != 0 && MCInput::GetAsyncKeyState(VK_MENU) != 0;

        if (MPlayer == nullptr && cheatsOn != 0 && event->key == 0xbb && ctrlAlt)
        {
            ResourcePoints += 1000;
        }
    }

    switch (event->type)
    {
        case 1:
        {
            mouseDown = -1;
            POINT point{xPos - globalX(), yPos - globalY()};
            RECT area{2, 0x34, 0xd1, 0x45};

            if (buttonsLocked == 0 && PtInRect(&area, point) != 0)
            {
                globalLogPtr->setUpPurchaseScreen(-1);
                return;
            }

            area.top = 0x46;
            area.bottom = 0x57;

            if (buttonsLocked == 0 && PtInRect(&area, point) != 0)
            {
                globalLogPtr->setUpRepairScreen(-1);
                return;
            }

            area.top = 0x10;
            area.bottom = 0x21;

            if (PtInRect(&area, point) != 0)
            {
                if (MPlayer == nullptr)
                {
                    StopSmackerMovies();
                    soundSystem->playDigitalSample(0x36, 1, nullptr, 0, 0);
                    globalLogPtr->setUpMainScreen(0);
                    return;
                }

                CheckExit();
                return;
            }

            // The tabs.
            area = {0xc2, 0x67, 0xd1, 0xde};

            if (PtInRect(&area, point) != 0)
            {
                soundSystem->playDigitalSample(0x36, 1, nullptr, 0, 0);
                setUpOperation();
                return;
            }

            area.top = 0xdf;
            area.bottom = 0x14a;

            if (PtInRect(&area, point) != 0)
            {
                soundSystem->playDigitalSample(0x36, 1, nullptr, 0, 0);
                setUpMission();
                return;
            }

            // Launch.
            area = {0x205, 0x14a, 0x270, 0x15c};

            if (buttonsLocked != 0 || PtInRect(&area, point) == 0)
            {
                break;
            }

            if (curDeployTonnage == 0 ||
                ((maxDeployTonnage < curDeployTonnage || globalLogPtr->requiredAssigned() == 0) &&
                 globalLogPtr->hammerDown == 0))
            {
                soundSystem->playBettySample(0x1b);
                return;
            }

            launchArt = logArtf("%slogart\\lsbdf01.tga", artPath);
            launchOnTop = true;
            UpdateDisplay(0, 0, 0, 0, 0);

            if (MPlayer == nullptr)
            {
                // Everything the player owns must fit the save: at most 50 mechs and vehicles.
                const int32_t units = globalLogPtr->forceVehicleList->numVehicles +
                                      globalLogPtr->forceMechList->numMechs + globalLogPtr->vehicleList->numVehicles +
                                      globalLogPtr->mechList->numMechs;

                if (units < 0x33)
                {
                    soundSystem->playDigitalSample(0x3a, 1, nullptr, 0, 0);
                    mission->StartScenario(mission->scenarios[mission->currentScenario]);
                    return;
                }

                soundSystem->playDigitalSample(0x33, 1, nullptr, 0, 0);
                char format[256];
                cLoadString(thisInstance, 0x373, format, 0xfe);
                char text[256];
                std::snprintf(text, sizeof(text), format, units, 0x32, units - 0x32);
                showMessage(text);
                return;
            }

            buttonsLocked = -1;
            MPlayer->sendReadyForBattle();
            char format[200];
            cLoadString(thisInstance, 0x379, format, 199);
            char text[304];
            std::snprintf(text, sizeof(text), format, MPlayer->sessionManager->myPlayer->name);
            MPlayer->sendChat(0, text);
            return;
        }

        case 4:
        {
            mouseDown = 0;
            globalX();
            globalY();
            return;
        }
        case 8:
        {
            if (event->key == 0x1b && movieOver == 0)
            {
                StopSmackerMovies();
                return;
            }

            globalLogPtr->processCheatCode(event->scanCode);
            return;
        }
        case 0x13:
        {
            if (event->data == 6)
            {
                // The operation movie's start delay.
                playMovie = -1;
                application->RemoveTimer(this, 6);
                setUpOperation();
                return;
            }

            if (event->data == 5)
            {
                // The chat button blinks.
                if (chatBlinkOn != 0)
                {
                    chatBlinkOn = 0;
                    pushLayer(tabLayers, {chatRegularPort, 0xc4, 0x65, true});
                    return;
                }

                chatBlinkOn = -1;
                pushLayer(tabLayers, {chatBlinkPort, 0xc5, 0x65, true});
            }
            break;
        }
        default:
            break;
    }
}

auto BriefingScreen::ShowGUIWindow(int show) -> void
{
    const bool paneShown = movieOver != 0;
    showWindow = show;

    if (paneShown)
    {
        missionPane->ShowGUIWindow(show);
    }

    deployPane->ShowGUIWindow(show);
}

auto BriefingScreen::setUpOperation() -> void
{
    globalLogPtr->autoPlayMovie = 0;
    movieOver = 0;
    const char* tabName;

    if (MPlayer == nullptr)
    {
        pushLayer(operationLayers, {logArtf("%slogart\\lsb_op0.tga", artPath), 0xc, 0x6f, true});
        tabName = "lsbdw00.tga";

        if (playMovie == 0)
        {
            application->AddTimer(this, 6, 500, 0, 0, 0);
        }
    }
    else
    {
        // Multiplayer: the tab is the chat, which stops every chat button blinking.
        movieOver = -1;

        if (chatBlinking != 0)
        {
            application->RemoveTimer(this, 5);
            chatTimerOn = 0;
            chatBlinking = 0;
        }

        if (globalLogPtr->purchaseScreen->chatBlinking != 0)
        {
            application->RemoveTimer(globalLogPtr->purchaseScreen, 7);
            globalLogPtr->purchaseScreen->chatBlinking = 0;
        }

        if (globalLogPtr->repairScreen->chatBlinking != 0)
        {
            application->RemoveTimer(globalLogPtr->repairScreen, 8);
            globalLogPtr->repairScreen->chatBlinking = 0;
        }

        chatBlinkOn = 0;
        tabName = "lsbdw02.tga";
        globalLogPtr->chatWindow->ShowGUIWindow(-1);
    }

    pushLayer(tabLayers, {logArtf("%slogart\\%s", artPath, tabName), 0xc4, 0x65, false});
    missionPane->ShowGUIWindow(0);
    StopSmackerMovies();
    currentTab = 1;

    if (playMovie == 0 || MPlayer != nullptr)
    {
        return;
    }

    if (operationPicture != nullptr)
    {
        pushLayer(operationLayers, {nullptr, 0xc, 0x6f, true});
        soundSystem->playBettySample(0x1a);

        while (soundSystem->isChannelPlaying(0xe) != 0)
        {
            UpdateDisplay(0, 0, 0, 0, 0);
        }

        if (MPlayer != nullptr)
        {
            return;
        }
    }

    if (globalLogPtr->operationCinema != nullptr)
    {
        FullPathFileName movieName;
        movieName.init(moviePath, globalLogPtr->operationCinema, ".smk");
        smacker = SmackOpen(movieName, 0xfe000, -1);

        // Port fix: a missing movie is skipped (the original read the null handle's size).
        if (smacker == nullptr)
        {
            return;
        }

        auto* window = new aSmackerWindow;
        RECT area{0xc, 0x6f, smacker->Player->Width(), smacker->Player->Height()};
        smackerWindow = window;
        window->init(&area, nullptr);
        addChild(window);
        smackerWindow->startSmackerMovie(smacker, 0);
        smackerWindow->draw();
    }
}

auto BriefingScreen::setUpMission() -> void
{
    pushLayer(tabLayers, {logArtf("%slogart\\%s", artPath, MPlayer == nullptr ? "lsbdw01.tga" : "lsbdw03.tga"), 0xc4,
                          0x65, false});
    currentTab = 2;

    if (MPlayer == nullptr)
    {
        StopSmackerMovies();
    }
    else
    {
        globalLogPtr->chatWindow->ShowGUIWindow(0);
    }

    missionPane->setDisplayPort(missionPort, 0, 0);
    missionPane->ShowGUIWindow(-1);
}

auto BriefingScreen::StopSmackerMovies() -> void
{
    if (smackerWindow == nullptr)
    {
        return;
    }

    removeChild(smackerWindow);
    aSmackerWindow* window = smackerWindow;
    window->endSmackerMovie();
    delete window;
    smackerWindow = nullptr;
    playMovie = 0;
    smacker = nullptr;
}

auto BriefingScreen::setUpDeploy() -> void
{
    ScrollPane* pane = deployPane;

    for (int32_t count = pane->numberOfChildren(); count > 0; count--)
    {
        pane->removeChild(pane->child(0));
    }

    delete[] undeployedMechs;
    undeployedMechs = nullptr;

    // Blocks of units still waiting are rebuilt; placed units keep theirs (redrawn).
    numUndeployed = 0;
    LogVehicle* firstVehicle = globalLogPtr->forceVehicleList->vehicles;

    for (LogMech* mech = globalLogPtr->forceMechList->mechs; mech != nullptr; mech = mech->next)
    {
        if (mech->pilotIndex < 0)
        {
            continue;
        }

        if (mech->deployed == 0)
        {
            delete mech->briefBlock;
            mech->briefBlock = nullptr;
            ++numUndeployed;
        }
        else
        {
            mech->briefBlock->drawBackground();
        }
    }

    for (LogVehicle* vehicle = firstVehicle; vehicle != nullptr; vehicle = vehicle->next)
    {
        if (vehicle->deployed == 0)
        {
            delete vehicle->briefBlock;
            vehicle->briefBlock = nullptr;
        }
        else
        {
            vehicle->briefBlock->drawBackground();
        }
    }

    const int32_t mechCount = numUndeployed;
    undeployedMechs = new int32_t[mechCount];
    int32_t index = 0;
    int32_t filled = 0;

    for (LogMech* mech = globalLogPtr->forceMechList->mechs; mech != nullptr; mech = mech->next, ++index)
    {
        if (mech->pilotIndex >= 0 && mech->deployed == 0)
        {
            undeployedMechs[filled++] = index;
        }
    }

    // Three blocks a row.
    auto* port = new lPort;
    const int32_t blocks = globalLogPtr->forceVehicleList->getVehicleCount() + mechCount;
    int32_t height;

    if (blocks < 7)
    {
        height = pane->height();
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
    port->initView(0xc5, height);
    port->DrawContent = [pane](aPort* view)
    {
        VFX_pane_wipe(view->frame(), 0x10);
        const int32_t scroll = pane->getScrollOffset();

        for (int32_t i = 0; i < pane->numberOfChildren(); i++)
        {
            auto* brief = static_cast<MechBriefBlock*>(pane->child(i));
            brief->PaintBlock(view->frame(), brief->x(), brief->y() + scroll, false);
        }
    };

    const int32_t sliderPos = pane->sliderPos;
    pane->setDisplayPort(port, -1, -1);
    int32_t block = 0;

    for (; block < mechCount; block++)
    {
        LogMech* mech = nullptr;
        globalLogPtr->forceMechList->getMechInfo(undeployedMechs[block], mech);
        auto* brief = new MechBriefBlock;
        brief->init(mech, pane, (block % 3) * 0x38 + 4, (block / 3) * 0x34 + 3);
    }

    for (LogVehicle* vehicle = globalLogPtr->forceVehicleList->vehicles; vehicle != nullptr; vehicle = vehicle->next)
    {
        if (vehicle->deployed != 0)
        {
            continue;
        }

        auto* brief = new MechBriefBlock;
        brief->init(vehicle, pane, (block % 3) * 0x38 + 4, (block / 3) * 0x34 + 3);
        ++block;
    }

    pane->setSliderPos(sliderPos);
}

// ---------------------------------------------------------------------------------------------------------------------
// MechBriefBlock
// ---------------------------------------------------------------------------------------------------------------------

MechBriefBlock::MechBriefBlock()
{
    mech = nullptr;
    vehicle = nullptr;
}

auto MechBriefBlock::init(LogMech* newMech, lObject* parent, int32_t xPos, int32_t yPos) -> void
{
    lObject::init(xPos, yPos, 0x34, 0x2e, nullptr, parent->lport());
    mech = newMech;
    newMech->briefBlock = this;
    parent->addChild(this);
    drawBackground();
}

auto MechBriefBlock::init(LogVehicle* newVehicle, lObject* parent, int32_t xPos, int32_t yPos) -> void
{
    lObject::init(xPos, yPos, 0x34, 0x2e, nullptr, parent->lport());
    vehicle = newVehicle;
    newVehicle->briefBlock = this;
    parent->addChild(this);
    drawBackground();
}

auto MechBriefBlock::destroy() -> void
{
    mech = nullptr;
    vehicle = nullptr;
    lObject::destroy();
}

auto MechBriefBlock::handleEvent(aEvent* event) -> void
{
    BriefingScreen* screen = globalLogPtr->briefingScreen;

    if (leftDragging == 0 && rightDragging == 0)
    {
        if (event->x < globalX() || event->x > globalX() + 1 + width())
        {
            return;
        }

        if (event->y < globalY() || event->y > globalY() + 1 + height())
        {
            return;
        }
    }

    // The parent is always a logistics object (the briefing screen or the deploy pane).
    auto* owner = static_cast<lObject*>(parent);

    if (owner != nullptr && leftDragging == 0 && (event->type == 8 || event->type == 9))
    {
        owner->handleEvent(event);
        return;
    }

    const int32_t type = event->type;

    switch (type)
    {
        case 1:
        {
            if (rightDragging != 0)
            {
                return;
            }
            break;
        }
        case 3:
        {
            if (leftDragging != 0)
            {
                return;
            }

            rightDragging = -1;
            break;
        }
        case 7:
        {
            if (leftDragging != 0)
            {
                dragY = event->y - 0x17;
                dragX = event->x - 0x1a;
                globalLogPtr->dragIcon->moveTo(dragX, dragY, 0);
                return;
            }

            if (application->grabbedObject() == nullptr && event->key == 0)
            {
                showHelp(globalX() < 0xd1 ? 0x22 : 0x21);
            }

            return;
        }
        case 4:
        case 6:
        {
            if (type == 6 && leftDragging != 0)
            {
                return;
            }

            if (rightDragging != 0 && type == 4)
            {
                return;
            }

            if (application->grabbedObject() == nullptr)
            {
                rightDragging = 0;
                return;
            }

            application->release();
            rightDragging = 0;
            leftDragging = 0;
            delete globalLogPtr->dragIcon;
            globalLogPtr->dragIcon = nullptr;

            // Places the unit in an empty slot: false when it is too heavy (the message is shown).
            auto placeInEmpty = [this](int32_t lance, int32_t slot) -> bool
            {
                auto& deploy = globalLogPtr->deploySlots[lance][slot];

                if (mech != nullptr)
                {
                    if (!fitsTonnage(mech) && globalLogPtr->hammerDown == 0)
                    {
                        return false;
                    }

                    globalLogPtr->SendAddMechMessage(mech, lance, slot);
                    soundSystem->playDigitalSample(0x34, 1, nullptr, 0, 0);
                    deploy.unit = globalLogPtr->forceMechList->getMechIndex(mech);
                    curDeployTonnage = deployTonnageWith(mech, 1.0f);
                    mech->deployed = -1;
                    globalLogPtr->assignedWarriorList->setDeployed(mech->pilotIndex, -1);
                    return true;
                }

                if (!fitsTonnage(vehicle) && globalLogPtr->hammerDown == 0)
                {
                    return false;
                }

                globalLogPtr->SendAddVehicleMessage(vehicle, lance, slot);
                soundSystem->playDigitalSample(0x34, 1, nullptr, 0, 0);
                deploy.vehicle = globalLogPtr->forceVehicleList->getVehicleIndex(vehicle);
                curDeployTonnage = deployTonnageWith(vehicle, 1.0f);
                vehicle->deployed = -1;
                return true;
            };

            // Puts the block into the slot on the screen and redraws.
            auto settle = [this, screen](int32_t lance, int32_t slot)
            {
                if (parent != nullptr)
                {
                    parent->removeChild(this);
                }

                screen->addChild(this);
                const RECT& area = screen->slotRects[lance * 4 + slot];
                moveTo(area.left, area.top, 0);
                drawBackground();
                screen->calcTonnages();
            };

            auto tooHeavy = [screen]()
            {
                showMessage(0x4cu);
                soundSystem->playBettySample(0);
                screen->drawTonnageBar();
            };

            bool placed = false;
            int32_t firstLance = 0;
            POINT point{event->x, event->y};

            if (type == 6)
            {
                // A right-button drop goes to the first free slot (of lance 1, 2 or 3 with those keys held).
                if (draggedFromSlot != 0)
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
                            if (globalLogPtr->localDropSlot[l * 4 + s] == 0)
                            {
                                if (lance < 0)
                                {
                                    continue;
                                }
                                break;
                            }

                            const auto& deploy = globalLogPtr->deploySlots[l][s];

                            if (deploy.unit == -1 && deploy.vehicle == -1)
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
                    if (globalLogPtr->localDropSlot[lance * 4 + slot] == 0 ||
                        PtInRect(&screen->slotRects[lance * 4 + slot], point) == 0)
                    {
                        continue;
                    }

                    // The slot is blanked.
                    screen->CoverSlot(lance * 4 + slot, BriefingScreen::SlotCover::Empty);

                    auto& deploy = globalLogPtr->deploySlots[lance][slot];
                    bool fits;

                    if (deploy.unit > -1)
                    {
                        // A mech was there: it leaves the force's drop.
                        LogMech* occupant = nullptr;
                        globalLogPtr->forceMechList->getMechInfo(deploy.unit, occupant);
                        curDeployTonnage = deployTonnageWith(occupant, -1.0f);
                        occupant->deployed = 0;
                        delete occupant->briefBlock;
                        occupant->briefBlock = nullptr;
                        deploy.unit = -1;

                        if (mech != nullptr)
                        {
                            fits = fitsTonnage(mech);

                            if (fits)
                            {
                                globalLogPtr->SendAddMechMessage(mech, lance, slot);
                                soundSystem->playDigitalSample(0x34, 1, nullptr, 0, 0);
                                deploy.unit = globalLogPtr->forceMechList->getMechIndex(mech);
                                curDeployTonnage = deployTonnageWith(mech, 1.0f);
                                mech->deployed = -1;
                            }
                        }
                        else
                        {
                            fits = fitsTonnage(vehicle);

                            if (fits)
                            {
                                globalLogPtr->SendAddVehicleMessage(vehicle, lance, slot);
                                soundSystem->playDigitalSample(0x34, 1, nullptr, 0, 0);
                                deploy.unit = -1;
                                deploy.vehicle = globalLogPtr->forceVehicleList->getVehicleIndex(vehicle);
                                curDeployTonnage = deployTonnageWith(vehicle, 1.0f);
                                vehicle->deployed = -1;
                            }
                        }
                    }
                    else if (deploy.vehicle > -1)
                    {
                        // A vehicle was there.
                        LogVehicle* occupant = nullptr;
                        globalLogPtr->forceVehicleList->getVehicleInfo(deploy.vehicle, occupant);
                        curDeployTonnage = deployTonnageWith(occupant, -1.0f);
                        occupant->deployed = 0;
                        delete occupant->briefBlock;
                        occupant->briefBlock = nullptr;
                        deploy.vehicle = -1;

                        if (mech != nullptr)
                        {
                            fits = fitsTonnage(mech);

                            if (fits)
                            {
                                globalLogPtr->SendAddMechMessage(mech, lance, slot);
                                soundSystem->playDigitalSample(0x34, 1, nullptr, 0, 0);
                                deploy.vehicle = -1;
                                deploy.unit = globalLogPtr->forceMechList->getMechIndex(mech);
                                curDeployTonnage = deployTonnageWith(mech, 1.0f);
                                mech->deployed = -1;
                                globalLogPtr->assignedWarriorList->setDeployed(mech->pilotIndex, -1);
                            }
                        }
                        else
                        {
                            fits = fitsTonnage(vehicle);

                            if (fits)
                            {
                                globalLogPtr->SendAddVehicleMessage(vehicle, lance, slot);
                                soundSystem->playDigitalSample(0x34, 1, nullptr, 0, 0);
                                deploy.vehicle = globalLogPtr->forceVehicleList->getVehicleIndex(vehicle);
                                curDeployTonnage = deployTonnageWith(vehicle, 1.0f);
                                vehicle->deployed = -1;
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
                screen->setUpDeploy();
                return;
            }

            // Nowhere to go: back to the deploy pane.
            uint32_t sound = 0x34;

            if (draggedFromSlot == 0)
            {
                ScrollPane* pane = screen->deployPane;
                const bool overPane = event->x > pane->globalX() && event->x < pane->globalX() + pane->width() &&
                                      event->y > pane->globalY() && event->y < pane->globalY() + pane->height();

                if (!overPane)
                {
                    sound = 0x33;
                }
            }

            soundSystem->playDigitalSample(sound, 1, nullptr, 0, 0);

            if (mech != nullptr)
            {
                mech->deployed = 0;
                globalLogPtr->assignedWarriorList->setDeployed(mech->pilotIndex, 0);
            }
            else
            {
                vehicle->deployed = 0;
            }

            if (parent != nullptr)
            {
                parent->removeChild(this);
            }

            screen->deployPane->addChild(this);
            screen->setUpDeploy();
            return;
        }

        default:
            return;
    }

    // A button went down on the block: its briefing shows, and it can be dragged.
    BriefingBox* shown = screen->briefingBox;

    if (shown != nullptr && shown->parent != nullptr)
    {
        shown->parent->removeChild(shown);
    }

    LogPart* part = mech != nullptr ? static_cast<LogPart*>(mech) : vehicle;
    BriefingBox* box = part->briefingBox;
    screen->addChild(box);
    screen->briefingBox = box;
    box->drawBackground();

    if (part->localPart == 0 || screen->buttonsLocked != 0)
    {
        return;
    }

    draggedFromSlot = owner == screen ? 1 : 0;
    soundSystem->playDigitalSample(0x35, 1, nullptr, 0, 0);
    application->grab(this);

    if (event->type == 1)
    {
        leftDragging = -1;
    }

    dragX = globalX();
    dragY = globalY();
    auto* icon = new DragIcon;
    globalLogPtr->dragIcon = icon;
    icon->Begin(dragX, dragY, 0x34, 0x2e, [this](lPort* surface) { OnBeginDrag(surface); });

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
                if (globalLogPtr->localDropSlot[lance * 4 + slot] == 0 ||
                    PtInRect(&screen->slotRects[lance * 4 + slot], POINT{event->x, event->y}) == 0)
                {
                    continue;
                }

                auto& deploy = globalLogPtr->deploySlots[lance][slot];

                if (mech == nullptr)
                {
                    vehicle->deployed = 0;
                    deploy.vehicle = -1;
                    curDeployTonnage = deployTonnageWith(vehicle, -1.0f);
                }
                else
                {
                    mech->deployed = 0;
                    deploy.unit = -1;
                    curDeployTonnage = deployTonnageWith(mech, -1.0f);
                    globalLogPtr->assignedWarriorList->setDeployed(mech->pilotIndex, 0);
                }

                screen->calcTonnages();
                globalLogPtr->SendRemoveForceMessage(lance, slot);
                found = true;
                break;
            }
        }

        Assert(found, 0, "Could not find the slot this item occupied", nullptr);
    }
    else
    {
        if (mech == nullptr)
        {
            vehicle->deployed = -1;
        }
        else
        {
            // A mech with a damaged engine or a destroyed location can't drop.
            _LogInventoryItem* engine = mech->inventory->items;

            while (engine != nullptr && MasterComponentList[engine->masterID].form != COMPONENT_FORM_ENGINE)
            {
                engine = engine->next;
            }

            uint32_t refusal = 0;

            // Port fix: a mech without an engine item (the original dereferenced null) counts as undamaged.
            if (engine != nullptr && engine->stats->hits != 0)
            {
                refusal = 0x35d;
            }
            else
            {
                for (const auto& location : mech->internals)
                {
                    if (location.curArmor == 0)
                    {
                        refusal = 0x364;
                        break;
                    }
                }
            }

            if (refusal != 0)
            {
                char text[256];
                cLoadString(thisInstance, refusal, text, 0xfe);
                application->release();
                leftDragging = 0;
                rightDragging = 0;
                screen->setUpDeploy();
                showMessage(text);
                return;
            }

            mech->deployed = -1;
            globalLogPtr->assignedWarriorList->setDeployed(mech->pilotIndex, -1);
        }

        screen->setUpDeploy();
    }

    screen->addChild(globalLogPtr->dragIcon);
    globalLogPtr->dragIcon->ShowGUIWindow(-1);
    globalLogPtr->dragIcon->setDepth(100);
}

auto MechBriefBlock::drawBackground() -> void
{
    // Port: the block is drawn each frame by its parent (PaintBlock): the screen into its slot, or the deploy pane.
    if (parent != nullptr && parent == globalLogPtr->briefingScreen)
    {
        globalLogPtr->briefingScreen->PlaceInSlot(this);
    }
}

auto MechBriefBlock::OnBeginDrag(lPort* surface) -> void
{
    PANE* target = surface->frame();
    BriefingScreen* screen = globalLogPtr->briefingScreen;

    if (parent != nullptr && parent == screen)
    {
        // In a drop slot: framed, over the empty slot.
        fillBox(target, 0, 0, 0x34, 0x2e, 0x10);

        if (screen->emptySlot != nullptr)
        {
            screen->emptySlot->copyTo(target, 0, 0, -1);
        }

        PaintBlock(target, 0, 0, true);
        return;
    }

    // The deploy pane is colour 0x10 around its blocks.
    VFX_pane_wipe(target, 0x10);
    PaintBlock(target, 0, 0, false);
}

auto MechBriefBlock::PaintBlock(PANE* target, int32_t xPos, int32_t yPos, bool framed) -> void
{
    auto* port = new lPort;
    char text[256];

    if (mech == nullptr)
    {
        copyArt(port, "lscupv00.tga");
        std::snprintf(text, sizeof(text), "%s", vehicle->fileName);
        const int32_t textWidth = greenFont->width(reinterpret_cast<uint8_t*>(text));
        greenFont->writeString(port->frame(), (width() - textWidth) / 2, 3, reinterpret_cast<uint8_t*>(text), -1);
        auto* body = new lPort;
        body->init(port->width(), port->height(), -1);
        VFX_pane_wipe(body->frame(), 0xff);

        for (int32_t location = 0; location < 5; location++)
        {
            globalLogPtr->drawVehicleBodyLoc(vehicle, location, body, 0xd, 0xe);
        }

        body->copyTo(port->frame(), 0, 0, -1);
        delete body;
    }
    else
    {
        copyArt(port, "lscupm00.tga");
        LogWarrior* warrior = nullptr;

        if (mech->localPart == 0)
        {
            warrior = mech->networkPilot;
        }
        else
        {
            globalLogPtr->assignedWarriorList->getWarriorInfo(mech->pilotIndex, warrior);
        }

        // Port fix: a mech without a pilot shows no callsign (the original read through null).
        std::snprintf(text, sizeof(text), "%s", warrior != nullptr ? warrior->callsign : "");
        const int32_t textWidth = greenFont->width(reinterpret_cast<uint8_t*>(text));
        greenFont->writeString(port->frame(), (width() - textWidth) / 2, 3, reinterpret_cast<uint8_t*>(text), -1);

        auto* body = new lPort;
        body->init(0x19, 0x1e, -1);
        VFX_pane_wipe(body->frame(), 0x10);

        for (int32_t location = 0; location < 8; location++)
        {
            globalLogPtr->drawMechBodyLoc(mech, location, body, 0, 0);
        }

        body->copyTo(port->frame(), 2, 0xe, -1);

        // The pilot's picture.
        if (warrior == nullptr)
        {
            std::snprintf(text, sizeof(text), "%spilot%02d.tga", artPath, mech->pilotIndex);
        }
        else
        {
            std::snprintf(text, sizeof(text), "%s%s", artPath, warrior->picture);
        }

        if (lPort* picture = logArt(text))
        {
            picture->copyTo(port->frame(), 0x1c, 0xe, -1);
        }

        delete body;

        // The mech's status bar (green, yellow, red) and the pilot's health bar.
        VFX_line_draw(port->frame(), 3, 0xb, 0x19, 0xb, LD_DRAW, 0x12);
        VFX_line_draw(port->frame(), 3, 0xc, 0x19, 0xc, LD_DRAW, 0x12);
        const float status = mech->statusValue;

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
            VFX_line_draw(port->frame(), 3, 0xb, barEnd, 0xb, LD_DRAW, color);
            VFX_line_draw(port->frame(), 3, 0xc, barEnd, 0xc, LD_DRAW, color);
        }

        if (warrior != nullptr && warrior->health < 6.0f)
        {
            const auto healthEnd = static_cast<int32_t>(warrior->health * 3.0f + 31.0f);
            VFX_line_draw(port->frame(), healthEnd, 0xb, 0x2f, 0xb, LD_DRAW, 0x10);
            VFX_line_draw(port->frame(), static_cast<int32_t>(warrior->health * 3.0f + 31.0f), 0xc, 0x2f, 0xc, LD_DRAW,
                          0x10);
        }
    }

    if (MPlayer != nullptr)
    {
        // Another player's unit is darkened.
        const int32_t index = mech != nullptr ? globalLogPtr->forceMechList->getMechIndex(mech)
                                              : globalLogPtr->forceVehicleList->getVehicleIndex(vehicle);

        if (index < 0)
        {
            globalLogPtr->darken(0, g_logistic_fadetable, port);
        }
    }

    if (framed)
    {
        // In a slot: a bevelled frame.
        VFX_line_draw(port->frame(), 0, 0, width() - 1, 0, LD_DRAW, 0x32);
        VFX_line_draw(port->frame(), 0, 1, 0, height() - 2, LD_DRAW, 0x32);
        VFX_line_draw(port->frame(), 0, height() - 1, width() - 1, height() - 1, LD_DRAW, 0x15);
        VFX_line_draw(port->frame(), width() - 1, 0, width() - 1, height() - 2, LD_DRAW, 0x15);
    }

    port->copyTo(target, xPos, yPos, -1);

    delete port;
}
