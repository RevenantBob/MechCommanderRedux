#include "stdafx.h"
#include "logistics/invblock.h"
#include "gui/afont.h"
#include "gui/mchwcursor.h"
#include "gui/scrlpane.h"
#include "logistics/logbri.h"
#include "logistics/logdlg.h"
#include "logistics/loggen.h"
#include "logistics/logmain.h"
#include "logistics/logpur.h"
#include "logistics/logrep.h"
#include "logistics/mrblock.h"
#include "logistics/purchase.h"
#include "logistics/ticker.h"
#include "main/logistics.h"
#include "main/main.h"
#include "network/multplyr.h"
#include "object/MCMasterComponent.h"
#include "sound/soundsys.h"
#include "vfx/MCVfxFunctions.h"
#include "vfx/MCAgShape.h"

int32_t NumRestrictedComponents = 5;
int32_t RestrictedComps[5] = {15, 37, 38, 42, 43};
MCLogMech* GlobalMechPtr = nullptr;
MCLogInventoryItem* GlobalCompPtr = nullptr;
MCLogVehicle* GlobalVehicle = nullptr;
int Solo = 0;
int32_t IconFade[4][3] = {{242, 241, 240}, {245, 244, 243}, {239, 238, 237}, {19, 19, 19}};

namespace
{
    /// <summary>The drag state of one kind of inventory row.</summary>
    struct MCDragState
    {
        /// <summary>Nonzero while the row is dragged with the left button held (picked up by event 1).</summary>
        int32_t Dragging = 0;
        /// <summary>Nonzero while the row is carried after a right-button pick-up (event 3).</summary>
        int32_t Carrying = 0;
        /// <summary>Where the drag icon is (window coordinates).</summary>
        int32_t X = 0;
        int32_t Y = 0;
    };

    /// <summary>The mech rows' drag (0x00808060 dragging, 64/68 x/y, 6c carrying).</summary>
    MCDragState MechDrag;
    /// <summary>The pilot rows' drag (0x00808074 dragging, 78/7c x/y, 80 carrying).</summary>
    MCDragState PilotDrag;
    /// <summary>The vehicle rows' drag (0x00808084 dragging, 88/8c x/y, 90 carrying).</summary>
    MCDragState VehicleDrag;
    /// <summary>The component rows' drag (0x00808094 dragging, 98 carrying, 9c/a0 x/y).</summary>
    MCDragState CompDrag;

    /// <summary>
    /// The row the next <see cref="MCPilotInventoryBlock::Init"/> takes (0x00808070). Never reset: the inventory
    /// screen renumbers the rows itself.
    /// </summary>
    int32_t NextPilotRow = 0;

    /// <summary>The pilot a sell dialog is open for (0x00808054; <see cref="PilotSellCallback"/>).</summary>
    MCLogWarrior* PilotToSell = nullptr;

    void* LogAlloc(uint32_t size)
    {
        return GlobalLogPtr->LogisticsBlocks->Allocate(size);
    }

    void LogFree(void* block)
    {
        GlobalLogPtr->LogisticsBlocks->Free(block);
    }

    void FreePort(MCLogPort*& port)
    {
        if (port != nullptr)
        {
            delete port;
        }

        port = nullptr;
    }

    void PlaySample(uint32_t sampleId)
    {
        SoundSystem->PlayDigitalSample(sampleId, 1, nullptr, 0, 0);
    }

    /// <summary>A copy of <paramref name="text"/> in a logistics block.</summary>
    char* HeapString(const char* text)
    {
        size_t length = std::strlen(text) + 1;
        auto* copy = static_cast<char*>(LogAlloc(static_cast<uint32_t>(length)));
        std::memcpy(copy, text, length);
        return copy;
    }

    /// <summary>Copies <paramref name="text"/> into a stat text field.</summary>
    /// <remarks>Port fix: bounded (the original <c>strcpy</c> would run into the next field).</remarks>
    template <size_t Size> void SetText(char (&field)[Size], const char* text)
    {
        std::snprintf(field, Size, "%s", text);
    }

    /// <summary>
    /// Whether the event is over the pane's inside: right of its left edge, left of 0xd pixels short of its right
    /// edge (the scroll bar), between its top and bottom. The position comes from <paramref name="posPane"/>, the
    /// width and height from <paramref name="widthPane"/> and <paramref name="heightPane"/> (the component rows mix
    /// the inventory and the unit pane).
    /// </summary>
    bool OverPane(MCGuiObject* posPane, MCGuiObject* widthPane, MCGuiObject* heightPane, MCGuiEvent* event)
    {
        return posPane->GlobalX() < event->X && event->X < posPane->GlobalX() + widthPane->Width() - 0xd &&
               posPane->GlobalY() < event->Y && event->Y < posPane->GlobalY() + heightPane->Height();
    }

    bool OverPane(MCGuiObject* pane, MCGuiEvent* event)
    {
        return OverPane(pane, pane, pane, event);
    }

    /// <summary>The logistics screen being shown, as the inventory screen it is when a row gets events.</summary>
    MCLogInvScreen* InvScreen()
    {
        return static_cast<MCLogInvScreen*>(GlobalLogPtr->CurrentScreen);
    }

    bool OnRepairScreen()
    {
        return GlobalLogPtr->CurrentScreen == GlobalLogPtr->RepairScreen;
    }

    bool OnPurchaseScreen()
    {
        return GlobalLogPtr->CurrentScreen == GlobalLogPtr->PurchaseScreen;
    }

    /// <summary>
    /// Handles what every row does first: passes keys up to the parent when nothing is dragged, and when the mouse
    /// leaves the inventory pane vertically, clears the info block and hands the event to the screen.
    /// </summary>
    /// <returns>False when the event was handled (or the row is hidden) and the row should do nothing more.</returns>
    bool PreHandleEvent(MCGuiObject* row, const MCDragState& drag, MCGuiEvent* event)
    {
        if (row->Parent != nullptr && drag.Dragging == 0 && drag.Carrying == 0 &&
            (event->Type == 8 || event->Type == 9))
        {
            row->Parent->HandleEvent(event);
            return false;
        }

        MCLogInvScreen* screen = InvScreen();

        if (drag.Dragging == 0 && drag.Carrying == 0)
        {
            MCGuiObject* pane = screen->InventoryPane;

            if (event->Y < pane->GlobalY() || pane->GlobalY() + pane->Height() < event->Y)
            {
                screen->DrawBlankInvInfoBlock(-1);
                screen->HandleEvent(event);
                return false;
            }

            if (row->IsShowing() == 0)
            {
                return false;
            }
        }

        return true;
    }

    /// <summary>
    /// Makes the drag icon: a 0x20 square of the row at (2, 1) (<see cref="MCInventoryBlock::OnBeginDrag"/>), framed
    /// in colour 0xea, added to the screen and centred on the cursor.
    /// </summary>
    void MakeDragIcon(MCDragState& drag, MCInventoryBlock* row, MCGuiEvent* event)
    {
        drag.X = 2;
        drag.Y = row->ListIndex * row->WinHeight + 1;
        auto* icon = new MCDragIcon;
        GlobalLogPtr->DragIcon = icon;
        icon->Begin(drag.X, drag.Y, 0x20, 0x20, [row](MCLogPort* surface) { row->OnBeginDrag(surface); });
        InvScreen()->AddChild(GlobalLogPtr->DragIcon);
        GlobalLogPtr->DragIcon->MoveTo(event->X - 0xf, event->Y - 0xf, 0);
    }

    /// <summary>Shows the drag icon above everything.</summary>
    void RaiseDragIcon()
    {
        GlobalLogPtr->DragIcon->ShowGuiWindow(1);
        GlobalLogPtr->DragIcon->SetDepth(100);
    }

    /// <summary>Frees the drag icon.</summary>
    void DeleteDragIcon()
    {
        if (GlobalLogPtr->DragIcon != nullptr)
        {
            delete GlobalLogPtr->DragIcon;
        }

        GlobalLogPtr->DragIcon = nullptr;
    }

    /// <summary>
    /// Copies the "drop here" art over the screen at (2, 0x18a): the blank info box of tab <paramref name="tab"/>
    /// (the original loaded the same picture, <c>logart\lscii?.tga</c>, again).
    /// </summary>
    void DrawDropArt(MCLogInvScreen* screen, int32_t tab)
    {
        screen->Info.Blank(tab);
    }

    /// <summary>Shows the one-button message dialog with string <paramref name="id"/>.</summary>
    /// <param name="okayArt">Nonzero: first give the OK button the "okay" pictures and enable it.</param>
    void ShowMessage(uint32_t id, bool okayArt)
    {
        char text[256];
        CLoadString(ThisInstance, id, text, 0xfe);
        MCReusableDialog* dialog = GlobalLogPtr->MessageDialog;
        dialog->SetText(text);
        GlobalLogPtr->MessageDialog->SetTwoButton(0);
        dialog = GlobalLogPtr->MessageDialog;
        dialog->Callback = nullptr;

        if (okayArt)
        {
            char upArt[] = "bh_okay.tga";
            char downArt[] = "bg_okay.tga";
            dialog->OkButton->SetUpPicture(upArt);
            GlobalLogPtr->MessageDialog->OkButton->SetDownPicture(downArt);
            MCLogDialogButton* button = GlobalLogPtr->MessageDialog->OkButton;
            button->Disabled = 0;
        }

        GlobalLogPtr->MessageDialog->Activate();
    }

    /// <summary>A sale's price: half the value, except in a multiplayer or solo game.</summary>
    int32_t SalePrice(int32_t value)
    {
        if (MPlayer == nullptr && Solo == 0)
        {
            value /= 2;
        }

        return value;
    }

    /// <summary>Opens the purchase dialog as a sale, on the current screen.</summary>
    void OpenSaleDialog(int32_t purchaseType, int32_t price, int32_t maxQuantity, char* title, char* subtitle,
                        MCLogPort* picture, void (*callback)(int, int32_t))
    {
        GlobalLogPtr->PurchaseDialog->Init(purchaseType, -price, maxQuantity, title, subtitle, picture);
        GlobalLogPtr->PurchaseDialog->SetCallback(callback);
        GlobalLogPtr->PurchaseDialog->SetPort(GlobalLogPtr->CurrentScreen->Lport());
        GlobalLogPtr->PurchaseDialog->Activate();
    }

    /// <summary>The string table index of the weight class of <paramref name="tonnage"/> (light .. assault).</summary>
    uint32_t WeightClassString(float tonnage)
    {
        if (tonnage < 40.0f)
        {
            return 0x4f;
        }

        if (tonnage < 60.0f)
        {
            return 0x50;
        }

        if (tonnage < 80.0f)
        {
            return 0x51;
        }

        return 0x52;
    }

    /// <summary>The rank name (string table 0x70..0x73) into <paramref name="text"/>.</summary>
    /// <remarks>Port fix: an out-of-range rank leaves the text empty (the original kept whatever was in the buffer).</remarks>
    void LoadRankName(int32_t rank, char* text)
    {
        text[0] = '\0';

        if (rank >= 0 && rank <= 3)
        {
            CLoadString(ThisInstance, 0x70 + static_cast<uint32_t>(rank), text, 0xfe);
        }
    }

    /// <summary>
    /// Where a pilot or component row is put together: a block of <paramref name="port"/> at row
    /// <paramref name="top"/>, the size of <paramref name="art"/>, with the art copied in. The original put the row
    /// together in a picture and copied it there (keyed on 0xff when <paramref name="keyed"/>); it is drawn in place.
    /// </summary>
    MCLogPort* NewRowPicture(MCLogPort* art, MCLogPort* port, int32_t top, bool keyed)
    {
        auto* row = new MCLogBlockPort(port->Frame(), 0, top, art->Width(), art->Height(), keyed);
        VfxPaneCopy(art->Frame(), 0, 0, row->Frame(), 0, 0, -1);
        return row;
    }

    void WriteText(MCGuiFont* font, MCLogPort* port, int32_t x, int32_t y, const char* text)
    {
        font->WriteString(port->Frame(), x, y, reinterpret_cast<uint8_t*>(const_cast<char*>(text)), -1);
    }

    /// <summary>
    /// The info box's picture of a mech or vehicle row: the 0x1e square at (3, 2) of the row (its art and
    /// <paramref name="picture"/>), drawn at (9, 0x191) of <paramref name="port"/>, as the original copied it out of
    /// the tab's picture.
    /// </summary>
    void DrawRowPicture(MCLogPort* picture, MCLogPort* port)
    {
        MCLogBlockPort square(port->Frame(), 9, 0x191, 0x1e, 0x1e, false);
        VfxPaneCopy(GlobalLogPtr->InvBlockPort->Frame(), 3, 2, square.Frame(), 0, 0, -1);

        if (picture != nullptr)
        {
            picture->CopyTo(square.Frame(), 2, 0, 1);
        }
    }

    /// <summary>The damage state of a diagram location from its armor left (-1 = undamaged).</summary>
    int32_t DamageState(int32_t percent, uint8_t internals)
    {
        if (percent == 0)
        {
            return internals == 0 ? 3 : 2;
        }

        if (percent < 0x1a)
        {
            return 2;
        }

        if (percent < 0x33)
        {
            return 1;
        }

        return percent < 0x4c ? 0 : -1;
    }

    /// <summary>
    /// Draws diagram shape <paramref name="location"/> of <paramref name="shapes"/> at (<paramref name="xPos"/>,
    /// <paramref name="yPos"/>) of <paramref name="port"/>, recoloured for <paramref name="state"/>.
    /// </summary>
    /// <remarks>
    /// The original drew the shape into a 0x19 x 0x1e picture wiped to 0xff, recoloured its pixels 0xe7, 0xe8 and 0xea
    /// to the state's <see cref="IconFade"/> colours in memory, and copied the picture keyed on 0xff. The port draws the
    /// shape in place through a table that does the recolouring, which comes out the same.
    /// </remarks>
    void DrawDiagram(void* shapes, int32_t location, int32_t state, MCLogPort* port, int32_t xPos, int32_t yPos)
    {
        MCLogBlockPort diagram(port->Frame(), xPos, yPos, 0x19, 0x1e, true);

        if (state < 0 || MCAgShapeIsAlpha(shapes, location))
        {
            AGShapeDraw(diagram.Frame(), shapes, location, 0, 0);
            return;
        }

        // Built once and registered: a renderer reads translate tables only from registered blocks (the GPU drew an
        // unregistered one as the identity, so no damage showed).
        static uint8_t (*recolor)[256] = []
        {
            static uint8_t tables[4][256];

            for (int32_t fade = 0; fade < 4; ++fade)
            {
                for (int32_t color = 0; color < 256; ++color)
                {
                    tables[fade][color] = static_cast<uint8_t>(color);
                }

                tables[fade][0xe7] = static_cast<uint8_t>(IconFade[fade][2]);
                tables[fade][0xe8] = static_cast<uint8_t>(IconFade[fade][1]);
                tables[fade][0xea] = static_cast<uint8_t>(IconFade[fade][0]);
            }

            MCRenderer::RegisterData(tables, sizeof(tables), MCDataKind::Tables);
            return tables;
        }();

        MCAgDrawShape(diagram.Frame(), shapes, location, 0, 0, MCShapeOp::Xlat, recolor[state]);
    }

    /// <summary>
    /// Adds one to every filled drop zone slot's first (<paramref name="vehicle"/> false) or second field: a unit
    /// joining the force goes in front of the others.
    /// </summary>
    void BumpDeploySlots(bool vehicle)
    {
        for (auto& lance : GlobalLogPtr->DeploySlots)
        {
            for (auto& slot : lance)
            {
                int32_t& value = vehicle ? slot.Vehicle : slot.Unit;

                if (value > -1)
                {
                    ++value;
                }
            }
        }
    }

    /// <summary>
    /// Mounts the dragged component on <paramref name="mech"/> (the one selected on the repair screen): undeploys
    /// it, and when it has the free tonnage adds the component (and a weapon's ammo) to its inventory.
    /// </summary>
    /// <returns>True when mounted; false after showing the "too heavy" message.</returns>
    bool MountComponent(MCLogInventoryItem* item, MCLogMech* mech)
    {
        if (mech->Deployed != 0)
        {
            mech->RepairBlock->UndeployMech();
        }

        uint8_t masterID = item->MasterID;
        MCMasterComponent& component = MasterComponentList[masterID];
        bool withAmmo =
            component.Form == MCComponentForm::WeaponBallistic || component.Form == MCComponentForm::WeaponMissile;
        double tons = component.Tonnage;

        if (withAmmo)
        {
            tons += MasterComponentList[component.AmmoMasterId].Tonnage;
        }

        if (tons <= static_cast<double>(mech->CurTonnage) - mech->UsedTonnage)
        {
            PlaySample(0x34);
            MCLogInventoryStat* stat = mech->Inventory->CreateStat(masterID, 0, 1, 1, 0xff);
            mech->Inventory->AddItem(masterID, stat, -1);

            if (withAmmo)
            {
                uint8_t ammoID = component.AmmoMasterId;
                stat = mech->Inventory->CreateStat(masterID, 0, 0, -1, 0xff);
                mech->Inventory->AddItem(ammoID, stat, -1);
            }

            float added = static_cast<float>(tons);
            mech->UsedTonnage += added;
            mech->WeaponTonnage += added;
            mech->RepairBlock->SetInventory(nullptr);
            mech->CalcBR();
            GlobalLogPtr->RepairScreen->SelectMech(mech);
            return true;
        }

        ShowMessage(99, true);
        return false;
    }
}

// Sell callbacks

auto MechSellCallback(int confirmed, int32_t) -> void
{
    MCLogistics* logistics = GlobalLogPtr;

    if (confirmed != 0)
    {
        MCInventoryList* spares = GlobalLogPtr->ComponentInventory;

        for (MCLogInventoryItem* item = GlobalMechPtr->Inventory->Items; item != nullptr; item = item->Next)
        {
            uint8_t masterID = item->MasterID;
            MCComponentForm form = MasterComponentList[masterID].Form;

            if (form != MCComponentForm::Sensor && form != MCComponentForm::WeaponEnergy &&
                form != MCComponentForm::WeaponBallistic && form != MCComponentForm::WeaponMissile &&
                form != MCComponentForm::Ecm && form != MCComponentForm::Probe && form != MCComponentForm::Jammer)
            {
                continue;
            }

            // Every undamaged copy goes back to the spare components.
            for (MCLogInventoryStat* stat = item->Stats; stat != nullptr; stat = stat->Next)
            {
                if (stat->Hits != 0)
                {
                    continue;
                }

                MCLogInventoryItem* stockItem = spares->GetItemInfo(spares->GetIndexFromMasterID(masterID));

                if (stockItem == nullptr)
                {
                    MCLogInventoryStat* newStat = spares->CreateStat(masterID, 0, 1, 0, 0xff);
                    spares->AddItem(masterID, newStat, -1);
                    stockItem = spares->GetItemInfo(spares->GetIndexFromMasterID(masterID));
                    auto* block = new MCCompInventoryBlock;
                    stockItem->InventoryBlock = block;
                    block->Init(stockItem);
                    stockItem->InventoryBlock->InventoryIndex = spares->NumItems - 1;
                }

                ++stockItem->Count;
            }
        }

        GlobalLogPtr->PurchaseScreen->CreateCompInvBlock();
        int32_t index = GlobalLogPtr->ForceMechList->GetMechIndex(GlobalMechPtr);
        GlobalLogPtr->ForceMechList->RemoveMech(static_cast<uint8_t>(index));
        GlobalLogPtr->ReorderMechs();
        ResourcePoints -= GlobalLogPtr->PurchaseDialog->UnitCost;
        return;
    }

    GlobalMechPtr->Assigned = 0;
    logistics->ReorderMechs();
    GlobalLogPtr->PurchaseScreen->CreateMechInvBlock();
    GlobalLogPtr->PurchaseScreen->SetUpMechInv(0, 1);
}

auto PilotSellCallback(int confirmed, int32_t) -> void
{
    MCLogWarrior* warrior = PilotToSell;

    if (confirmed != 0)
    {
        int32_t row = warrior->InventoryBlock->ListIndex;
        SoundSystem->PlayPilotSpeech(warrior->PilotAudio, 2);
        warrior->Sold = 1;
        GlobalLogPtr->PurPilotList->SetPilotStatus(warrior->DescIndex, 3);
        GlobalLogPtr->AssignedWarriorList->RemoveWarrior(static_cast<uint8_t>(warrior->Id));
        GlobalLogPtr->ShiftPilots(row, -1);
        GlobalLogPtr->ReorderWarriors();
        ResourcePoints -= GlobalLogPtr->PurchaseDialog->UnitCost;
        return;
    }

    warrior->Assigned = 0;
    GlobalLogPtr->ShiftPilots(warrior->InventoryBlock->ListIndex, -1);
    GlobalLogPtr->ReorderWarriors();
    GlobalLogPtr->PurchaseScreen->CreatePilotInvBlock();
    GlobalLogPtr->PurchaseScreen->SetUpPilotInv(0, 1);
}

auto CompSellCallback(int confirmed, int32_t quantity) -> void
{
    MCLogInventoryItem* item = GlobalCompPtr;

    if (confirmed == 0)
    {
        return;
    }

    item->Count -= quantity;

    if (item->Count == 0)
    {
        GlobalLogPtr->PurchaseScreen->CreateCompInvBlock();
        GlobalLogPtr->PurchaseScreen->SetUpCompInv(0, 1);
    }
    else
    {
        item->InventoryBlock->DrawBackground();
    }

    ResourcePoints -= GlobalLogPtr->PurchaseDialog->UnitCost * quantity;
}

auto VehicleSellCallback(int confirmed, int32_t) -> void
{
    MCLogistics* logistics = GlobalLogPtr;

    if (confirmed != 0)
    {
        int32_t index = GlobalLogPtr->ForceVehicleList->GetVehicleIndex(GlobalVehicle);
        GlobalLogPtr->ForceVehicleList->RemoveVehicle(static_cast<uint8_t>(index));
        GlobalLogPtr->ReorderVehicles();
        ResourcePoints -= GlobalLogPtr->PurchaseDialog->UnitCost;
        return;
    }

    GlobalVehicle->Assigned = 0;
    logistics->ReorderVehicles();
    GlobalLogPtr->PurchaseScreen->CreateVhclInvBlock();
    GlobalLogPtr->PurchaseScreen->SetUpVhclInv(0, 1);
}

// DragIcon

auto MCDragIcon::Display() -> void
{
    if (IsShowing() == 0)
    {
        return;
    }

    // Port fix: with the system cursor, the icon rides on the cursor instead of being drawn into the frame, so
    // it follows the mouse as closely as the cursor does.
    if (MCHardwareCursorCarry(this, Lport()->Frame()))
    {
        return;
    }

    VfxPaneCopy(Lport()->Frame(), 0, 0, FramePane, 0, 0, -1);
}

auto MCDragIcon::Begin(int32_t xPos, int32_t yPos, int32_t width, int32_t height,
                       const std::function<void(MCLogPort* surface)>& render) -> void
{
    Init(xPos, yPos, width, height, nullptr, nullptr);
    MCPane* surface = Lport()->Frame();
    VfxPaneWipe(surface, 0);
    render(Lport());
    const int32_t right = width - 1;
    const int32_t bottom = height - 1;
    VfxLineDraw(surface, 0, 0, right, 0, 0xea);
    VfxLineDraw(surface, 0, 1, 0, bottom - 1, 0xea);
    VfxLineDraw(surface, right, 1, right, bottom - 1, 0xea);
    VfxLineDraw(surface, 0, bottom, right, bottom, 0xea);
}

auto MCDragIcon::DrawFrom(MCLogPort* surface, int32_t xPos, int32_t yPos,
                          const std::function<void(MCLogPort* port)>& draw) -> void
{
    MCPane* pane = surface->Frame();
    const MCPane whole = *pane;
    pane->X0 = whole.X0 - xPos;
    pane->Y0 = whole.Y0 - yPos;
    draw(surface);
    *pane = whole;
}

// InventoryBlock

MCInventoryBlock::~MCInventoryBlock()
{
    MCInventoryBlock::Destroy();
}

auto MCInventoryBlock::Init(int32_t xPos, int32_t yPos, MCLogPort* port) -> void
{
    MCLogObject::Init(xPos, yPos, 0xad, 0x2b, nullptr, port);
    Enabled = 1;
}

auto MCInventoryBlock::Destroy() -> void
{
    MCLogInvScreen::ForgetInfoSource(this);
    MCLogObject::Destroy();
}

auto MCInventoryBlock::DrawBackground(MCLogPort* port) -> void
{
    VfxPaneCopy(GlobalLogPtr->InvBlockPort->Frame(), 0, 0, port->Frame(), 0, ListIndex * WinHeight, -1);
}

auto MCInventoryBlock::DrawDisabled() -> void
{
}

auto MCInventoryBlock::SetEnabled(int32_t enable) -> void
{
    Enabled = enable;
    Draw();
}

auto MCInventoryBlock::DrawRow(MCLogPort* port, int32_t top) -> void
{
    VfxPaneCopy(GlobalLogPtr->InvBlockPort->Frame(), 0, 0, port->Frame(), 0, top, -1);
}

auto MCInventoryBlock::OnBeginDrag(MCLogPort* surface) -> void
{
    VfxPaneWipe(surface->Frame(), 0x10);
    MCDragIcon::DrawFrom(surface, 2, 1, [this](MCLogPort* port) { DrawRow(port, 0); });
}

// Info box

auto DrawInfoDescription(MCLogPort* port, int32_t width, int32_t height, char* description, int32_t xPos, int32_t yPos)
    -> void
{
    if (description == nullptr)
    {
        return;
    }

    // Drawn in place: the original wrote it into a picture wiped to 0xff and copied that keyed on 0xff.
    MCLogBlockPort picture(port->Frame(), xPos, yPos, width, height, true);
    Application->TextFormatter.Process(reinterpret_cast<uint8_t*>(description), &picture, 0, 0);
}

auto PrepareInfoDescription(char* description) -> void
{
    if (description != nullptr)
    {
        description[3] = '9';
    }
}

// MechInventoryBlock

MCMechInventoryBlock::~MCMechInventoryBlock()
{
    MCMechInventoryBlock::Destroy();
}

auto MCMechInventoryBlock::Init(MCLogMech* logMech) -> void
{
    DiagramPort = nullptr;
    Mech = logMech;
    MCInventoryBlock::Init(0, 0, GlobalLogPtr->PurchaseScreen->Lport());
    ListIndex = Mech->NameIndex;
}

auto MCMechInventoryBlock::Destroy() -> void
{
    if (DiagramPort != nullptr)
    {
        delete DiagramPort;
        DiagramPort = nullptr;
    }

    Mech = nullptr;
    MCInventoryBlock::Destroy();
}

auto MCMechInventoryBlock::Draw() -> void
{
    if (Enabled == 0)
    {
        DrawDisabled();
    }
}

auto MCMechInventoryBlock::DrawBackground() -> void
{
    MCLogMech* logMech = Mech;

    if (DiagramPort == nullptr)
    {
        auto* port = new MCLogPort;
        DiagramPort = port;
        port->Init(0x1c, 0x1e, 1);
        VfxPaneWipe(port->Frame(), 0x10);

        for (int32_t location = 0; location < 8; ++location)
        {
            GlobalLogPtr->DrawMechBodyLoc(logMech, location, port, 2, 0);
        }

        // The battle rating bar along the left edge: 26 pixels at 18010.
        int32_t bar = static_cast<int32_t>(static_cast<double>(logMech->BattleRating) * 0x1.d1c6674f499a1p-15 * 26.0);
        VfxLineDraw(port->Frame(), 0, 0x1b, 0, 0x1b - bar, 0xe4);
        VfxLineDraw(port->Frame(), 1, 0x1b, 1, 0x1b - bar, 0xe4);
    }
}

auto MCMechInventoryBlock::DrawRow(MCLogPort* port, int32_t top) -> void
{
    MCInventoryBlock::DrawRow(port, top);
    MCLogMech* logMech = Mech;
    WriteText(YellowDropFont, port, 0x26, top + 7, logMech->FileName);
    char format[256];
    char text[256];
    CLoadString(ThisInstance, 0x4e, format, 0xfe);
    std::snprintf(text, sizeof(text), format, static_cast<double>(logMech->CurTonnage), logMech->WeightClassName);
    WriteText(BlueDropFont, port, 0x26, top + 0x15, text);

    if (DiagramPort != nullptr)
    {
        DiagramPort->CopyTo(port->Frame(), 5, top + 2, 1);
    }
}

auto MCMechInventoryBlock::DrawInfo(MCLogPort* port) -> void
{
    DrawRowPicture(DiagramPort, port);
    char tons[32];
    char text[84];
    CLoadString(ThisInstance, 0x6e, tons, 0x1e);
    MCLogMech* logMech = Mech;
    std::snprintf(text, sizeof(text), "%.0f %s", static_cast<double>(logMech->CurTonnage), tons);
    WriteText(YellowDropFont, port, 0x53, 0x193, text);
    WriteText(YellowDropFont, port, 0x53, 0x19c, logMech->WeightClassName);
    WriteText(YellowDropFont, port, 0xa8, 0x193, logMech->ChassisClassName);
    WriteText(YellowDropFont, port, 0xa8, 0x19c, logMech->ExtraName1);
    WriteText(YellowDropFont, port, 0xa8, 0x1a5, logMech->ExtraName2);
    std::snprintf(text, sizeof(text), "%d m/s", logMech->MaxRunSpeed);
    WriteText(YellowDropFont, port, 0x53, 0x1a5, text);
    DrawInfoDescription(port, 0xc3, 0x26, logMech->Description, 8, 0x1b3);
}

auto MCMechInventoryBlock::DeleteDiagram() -> void
{
    if (DiagramPort != nullptr)
    {
        delete DiagramPort;
    }

    DiagramPort = nullptr;
}

auto MCMechInventoryBlock::HandleEvent(MCGuiEvent* event) -> void
{
    if (!PreHandleEvent(this, MechDrag, event))
    {
        return;
    }

    MCLogInvScreen* screen = InvScreen();
    bool idle = MechDrag.Dragging == 0 && MechDrag.Carrying == 0;

    if (idle)
    {
        // The info block: the row's diagram, tonnage, classes, speed and description.
        screen->DrawBlankInvInfoBlock(-1);
        PrepareInfoDescription(Mech->Description);
        screen->ShowInfo(MCInvInfoBox::Kind::Mech, this);

        if (event->Type == 1 && MechDrag.Carrying == 0)
        {
            // Left button down: drag the mech (it joins the force while dragged).
            PlaySample(0x35);
            Application->SetCursorVisible(0);
            Application->Grab(this);
            MechDrag.Dragging = 1;
            MakeDragIcon(MechDrag, this, event);
            Mech->Assigned = 1;
            GlobalLogPtr->ReorderMechs();
            screen->CreateMechInvBlock();
            screen->SetUpMechInv(0, 0);
            RaiseDragIcon();
        }
    }

    char text[256];
    char title[372];

    switch (event->Type)
    {
        case 3:
        {
            // Right button down: pick the mech up.
            if (MechDrag.Dragging != 0)
            {
                break;
            }

            MechDrag.Carrying = 1;
            PlaySample(0x35);
            Application->SetCursorVisible(0);
            Application->Grab(this);
            MakeDragIcon(MechDrag, this, event);
            RaiseDragIcon();
            break;
        }

        case 4:
        {
            // Left button up: drop the dragged mech.
            if (MechDrag.Carrying != 0 || MechDrag.Dragging == 0)
            {
                break;
            }

            Application->SetCursorVisible(1);
            Application->Release();
            MechDrag.Dragging = 0;
            DeleteDragIcon();
            uint32_t sample = 0x33;

            if (OnRepairScreen())
            {
                DrawDropArt(screen, 0);

                if (OverPane(screen->UnitPane, event))
                {
                    if (GlobalLogPtr->ForceMechList->NumMechs + GlobalLogPtr->ForceVehicleList->NumVehicles > 0xf)
                    {
                        // The force is full.
                        PlaySample(0x33);
                        Mech->Assigned = 0;
                        GlobalLogPtr->ReorderMechs();
                        screen->CreateMechInvBlock();
                        screen->SetUpMechInv(0, 1);
                        ShowMessage(0x285, false);
                    }
                    else
                    {
                        PlaySample(0x34);
                        BumpDeploySlots(false);
                        MCLogMech* logMech = Mech;
                        GlobalLogPtr->RepairScreen->UnitPane->AddChild(logMech->RepairBlock);
                        GlobalLogPtr->RepairScreen->AddMechToList(logMech);
                        GlobalLogPtr->RepairScreen->SelectMech(logMech);
                    }
                    break;
                }

                if (OverPane(screen->InventoryPane, event))
                {
                    sample = 0x34;
                }
            }
            else
            {
                if (OverPane(screen->UnitPane, event))
                {
                    // Dropped on the store: offer to sell it.
                    PlaySample(0x34);
                    MCLogMech* logMech = Mech;
                    GlobalMechPtr = logMech;

                    if (logMech->Required != 0)
                    {
                        break;
                    }

                    CLoadString(ThisInstance, WeightClassString(logMech->CurTonnage), text, 0xfe);
                    std::snprintf(title, sizeof(title), "%.0f Ton %s 'Mech", static_cast<double>(logMech->CurTonnage),
                                  text);
                    logMech->CalcMechCost(0);
                    int32_t price = SalePrice(logMech->ResourcePoints);
                    auto* picture = new MCLogPort;
                    MCLogPort* diagram = DiagramPort;
                    picture->Init(diagram->Width(), diagram->Height(), 1);
                    VfxPaneWipe(picture->Frame(), 0x10);
                    diagram->CopyTo(picture->Frame(), 2, 0, 1);
                    OpenSaleDialog(1, price, 1, logMech->FileName, title, picture, MechSellCallback);
                    delete picture;
                    break;
                }

                if (OverPane(screen->InventoryPane, event))
                {
                    sample = 0x34;
                }
            }

            // Back to the inventory.
            PlaySample(sample);
            Mech->Assigned = 0;
            GlobalLogPtr->ReorderMechs();
            screen->CreateMechInvBlock();
            screen->SetUpMechInv(0, 1);
            break;
        }

        case 6:
        {
            // Right button up: put down the carried mech.
            if (Application->GrabbedObject() == nullptr || MechDrag.Dragging != 0)
            {
                break;
            }

            PlaySample(0x34);
            MechDrag.Carrying = 0;
            Application->SetCursorVisible(1);
            Application->Release();
            DeleteDragIcon();
            MCLogMech* logMech = Mech;
            logMech->Assigned = 1;
            GlobalLogPtr->ReorderMechs();
            screen->CreateMechInvBlock();
            screen->SetUpMechInv(0, 1);

            if (OnRepairScreen())
            {
                if (GlobalLogPtr->ForceMechList->NumMechs + GlobalLogPtr->ForceVehicleList->NumVehicles < 0x10)
                {
                    DrawDropArt(screen, 0);
                    BumpDeploySlots(false);
                    GlobalLogPtr->RepairScreen->UnitPane->AddChild(logMech->RepairBlock);
                    GlobalLogPtr->RepairScreen->AddMechToList(logMech);
                    GlobalLogPtr->RepairScreen->SelectMech(logMech);
                }
                else
                {
                    PlaySample(0x33);
                    logMech->Assigned = 0;
                    GlobalLogPtr->ReorderMechs();
                    screen->CreateMechInvBlock();
                    screen->SetUpMechInv(0, 1);
                    ShowMessage(0x285, true);
                }
                break;
            }

            GlobalMechPtr = logMech;

            if (logMech->Required != 0)
            {
                break;
            }

            CLoadString(ThisInstance, WeightClassString(logMech->CurTonnage), text, 0xfe);
            std::snprintf(title, sizeof(title), "%.0f Ton %s 'Mech", static_cast<double>(logMech->CurTonnage), text);
            logMech->CalcMechCost(0);
            int32_t price = SalePrice(logMech->ResourcePoints);
            auto* picture = new MCLogPort;
            MCLogPort* diagram = DiagramPort;
            picture->Init(diagram->Width(), diagram->Height(), 1);
            VfxPaneWipe(picture->Frame(), 0x10);
            diagram->CopyTo(picture->Frame(), 2, 0, 1);
            OpenSaleDialog(1, price, 1, logMech->FileName, title, picture, MechSellCallback);
            delete picture;
            break;
        }

        case 7:
        {
            // Mouse move: the dragged icon follows; otherwise the ticker shows the row's help.
            if (MechDrag.Dragging == 0)
            {
                if (event->Key == 0)
                {
                    CLoadString(ThisInstance, OnPurchaseScreen() ? 0x2d : 0x31, text, 0xfe);
                    GlobalLogPtr->Ticker->SetString(text);
                }
            }
            else
            {
                MechDrag.X = event->X - 0xf;
                MechDrag.Y = event->Y - 0xf;
                GlobalLogPtr->DragIcon->MoveTo(MechDrag.X, MechDrag.Y, 0);
            }
            break;
        }
    }
}

// Logistics diagrams (their code sits in invblock.cpp)

auto MCLogistics::DrawMechBodyLoc(MCLogMech* mech, int32_t location, MCLogPort* port, int32_t xPos, int32_t yPos)
    -> void
{
    const MCLogMech::ArmorPoints& armor = mech->Armor[location];
    int32_t percent = static_cast<int32_t>(static_cast<double>(armor.CurArmor) / armor.MaxArmor * 100.0f);

    if (location >= 1 && location <= 3)
    {
        // A torso counts its weaker side, front or rear.
        const MCLogMech::ArmorPoints& rear = mech->Armor[location + 7];
        int32_t rearPercent = static_cast<int32_t>(static_cast<double>(rear.CurArmor) / rear.MaxArmor * 100.0f);

        if (rearPercent < percent)
        {
            percent = rearPercent;
        }
    }

    int32_t state = DamageState(percent, mech->Internals[location].CurArmor);
    DrawDiagram(GlobalLogPtr->MechIconShapes[mech->NameIndex], location, state, port, xPos, yPos);
}

auto MCLogistics::DrawVehicleBodyLoc(MCLogVehicle* vehicle, int32_t location, MCLogPort* port, int32_t xPos,
                                     int32_t yPos) -> void
{
    uint8_t maxArmor = vehicle->MaxArmorPoints[location];

    if (maxArmor == 0)
    {
        return;
    }

    int32_t percent = static_cast<int32_t>(static_cast<double>(vehicle->CurArmorPoints[location]) / maxArmor * 100.0f);
    int32_t state = DamageState(percent, vehicle->CurInternalStructure[location]);
    DrawDiagram(GlobalLogPtr->VehicleIconShapes[vehicle->NameIndex], location, state, port, xPos, yPos);
}

// PilotInventoryBlock

MCPilotInventoryBlock::~MCPilotInventoryBlock()
{
    MCPilotInventoryBlock::Destroy();
}

auto MCPilotInventoryBlock::Init(MCLogWarrior* logWarrior) -> void
{
    Mech = nullptr;
    Vehicle = nullptr;
    Warrior = logWarrior;
    MCInventoryBlock::Init(0, 0, GlobalLogPtr->PurchaseScreen->Lport());
    ListIndex = NextPilotRow;
    ++NextPilotRow;
    char fileName[256];
    std::snprintf(fileName, sizeof(fileName), "%slogart\\%02s", ArtPath, Warrior->Picture);
    auto* port = new MCLogPort;
    PortraitPort = port;
    port->Init(fileName);
}

auto MCPilotInventoryBlock::Destroy() -> void
{
    Warrior = nullptr;

    if (PortraitPort != nullptr)
    {
        delete PortraitPort;
        PortraitPort = nullptr;
    }

    MCInventoryBlock::Destroy();
}

auto MCPilotInventoryBlock::Draw() -> void
{
    if (Enabled == 0)
    {
        DrawDisabled();
    }
}

auto MCPilotInventoryBlock::DrawBackground() -> void
{
    // On the repair screen a pilot can only go to the selected mech, and only when it has none.
    if (OnRepairScreen())
    {
        MCLogMech* selected = GlobalLogPtr->RepairScreen->SelectedMech;
        GreyedOut = 1;

        if (selected != nullptr && selected->PilotIndex < 0)
        {
            GreyedOut = 0;
        }
    }
    else
    {
        GreyedOut = 0;
    }

    MoveTo(0, WinHeight * ListIndex, 0);
}

auto MCPilotInventoryBlock::DrawRow(MCLogPort* port, int32_t top) -> void
{
    MCLogPort* row = NewRowPicture(GlobalLogPtr->InvBlockPort, port, top, true);
    PortraitPort->CopyTo(row->Frame(), 3, 2, 1);
    WriteText(YellowDropFont, row, 0x26, 7, Warrior->Callsign);
    char text[256];
    char pilot[256];
    LoadRankName(Warrior->Rank, text);
    CLoadString(ThisInstance, 0x287, pilot, 0xfe);
    std::strcat(text, " ");
    std::strcat(text, pilot);
    WriteText(BlueDropFont, row, 0x26, 0x15, text);

    if (GreyedOut != 0)
    {
        GlobalLogPtr->Darken(0, LogisticFadetable, row);
    }

    delete row;
}

auto MCPilotInventoryBlock::DrawInfo(MCLogPort* port) -> void
{
    MCLogWarrior* logWarrior = Warrior;
    GlobalLogPtr->DrawPilotSkillBar(logWarrior, 3, 0x56, 0x192, 0, 0x36, WinHeight, port);
    GlobalLogPtr->DrawPilotSkillBar(logWarrior, 0, 0x56, 0x19b, 0, 0x36, WinHeight, port);
    GlobalLogPtr->DrawPilotSkillBar(logWarrior, 1, 0x56, 0x1a4, 0, 0x36, WinHeight, port);
    GlobalLogPtr->DrawPilotSkillBar(logWarrior, 2, 0x56, 0x1ad, 0, 0x36, WinHeight, port);
    PortraitPort->CopyTo(port->Frame(), 9, 0x196, 1);
    char text[256];
    LoadRankName(logWarrior->Rank, text);
    WriteText(YellowDropFont, port, 0x9c, 0x19a, text);
    // One pip per point of health left.
    int32_t x = 0xf;

    for (int32_t pip = 0; static_cast<float>(pip) < logWarrior->Health; ++pip, x += 3)
    {
        AGPixelWrite(port->Frame(), x, 0x192, 0xcf);
        AGPixelWrite(port->Frame(), x + 1, 0x192, 0xcf);
        AGPixelWrite(port->Frame(), x + 1, 0x193, 0xee);
        AGPixelWrite(port->Frame(), x, 0x193, 0xcf);
    }

    DrawInfoDescription(port, 0xc3, 0x22, logWarrior->Description, 7, 0x1b8);
}

auto MCPilotInventoryBlock::HandleEvent(MCGuiEvent* event) -> void
{
    if (!PreHandleEvent(this, PilotDrag, event))
    {
        return;
    }

    MCLogInvScreen* screen = InvScreen();
    bool idle = PilotDrag.Dragging == 0 && PilotDrag.Carrying == 0;
    char text[256];

    if (idle)
    {
        // The info block: skills, portrait, rank, wounds and description.
        screen->DrawBlankInvInfoBlock(-1);
        PrepareInfoDescription(Warrior->Description);
        screen->ShowInfo(MCInvInfoBox::Kind::Pilot, this);
    }

    switch (event->Type)
    {
        case 1:
        {
            // Left button down: drag the pilot.
            if (GreyedOut != 0 || PilotDrag.Carrying != 0)
            {
                break;
            }

            SoundSystem->PlayPilotSpeech(Warrior->PilotAudio, 10);
            Application->SetCursorVisible(0);
            Application->Grab(this);
            PilotDrag.Dragging = 1;
            MakeDragIcon(PilotDrag, this, event);
            MCLogWarrior* logWarrior = Warrior;
            logWarrior->Assigned = 1;
            GlobalLogPtr->ReorderWarriors();
            GlobalLogPtr->ShiftPilots(logWarrior->InventoryBlock->ListIndex, 1);
            screen->CreatePilotInvBlock();
            screen->SetUpPilotInv(0, 0);
            RaiseDragIcon();
            break;
        }

        case 3:
        {
            // Right button down: pick the pilot up.
            if (GreyedOut != 0 || PilotDrag.Dragging != 0)
            {
                break;
            }

            PilotDrag.Carrying = 1;
            SoundSystem->PlayPilotSpeech(Warrior->PilotAudio, 10);
            Application->SetCursorVisible(0);
            Application->Grab(this);
            MakeDragIcon(PilotDrag, this, event);
            RaiseDragIcon();
            break;
        }

        case 4:
        {
            // Left button up: drop the dragged pilot.
            if (PilotDrag.Carrying != 0 || Application->GrabbedObject() == nullptr)
            {
                break;
            }

            Application->SetCursorVisible(1);
            Application->Release();
            PilotDrag.Dragging = 0;
            DeleteDragIcon();
            DrawDropArt(screen, 1);

            if (OverPane(screen->UnitPane, event))
            {
                MCLogWarrior* logWarrior = Warrior;

                if (OnPurchaseScreen())
                {
                    // Dropped on the store: offer to sell the pilot.
                    PilotToSell = logWarrior;
                    auto* picture = new MCLogPort;
                    std::snprintf(text, sizeof(text), "%slogart\\%s", ArtPath, logWarrior->Picture);
                    picture->Init(text);
                    int32_t price = SalePrice(GlobalLogPtr->PilotCosts[logWarrior->Rank]);
                    CLoadString(ThisInstance, 0x5f, text, 0xfe);
                    GlobalLogPtr->PurchaseDialog->Init(3, -price, 1, logWarrior->Callsign, text, picture);
                    delete picture;
                    GlobalLogPtr->PurchaseDialog->SetPort(GlobalLogPtr->CurrentScreen->Lport());
                    GlobalLogPtr->PurchaseDialog->SetCallback(PilotSellCallback);
                    GlobalLogPtr->PurchaseDialog->Activate();
                    break;
                }

                // Dropped on a mech: it must be the selected one.
                MCScrollPane* pane = screen->UnitPane;
                int32_t index = (event->Y - pane->GlobalY() + pane->GetScrollOffset()) / 0x70;

                if (index < pane->NumberOfChildren() && index < GlobalLogPtr->ForceMechList->GetMechCount())
                {
                    MCLogMech* target = nullptr;
                    GlobalLogPtr->ForceMechList->GetMechInfo(index, target);

                    if (target != nullptr && target == GlobalLogPtr->RepairScreen->SelectedMech)
                    {
                        MCPilotInventoryBlock* block = logWarrior->InventoryBlock;
                        block->Mech = target;
                        int32_t row = block->ListIndex;
                        GlobalLogPtr->SetPilot(index, row);
                        target->RepairBlock->DrawBR(nullptr);
                        GlobalLogPtr->SetPilot(index, row);
                        screen->CreatePilotInvBlock();
                        screen->SetUpPilotInv(0, 1);
                        SoundSystem->PlayPilotSpeech(logWarrior->PilotAudio, 2);
                        break;
                    }
                }
            }

            // Back to the inventory.
            MCLogWarrior* logWarrior = Warrior;
            logWarrior->Assigned = 0;
            GlobalLogPtr->ShiftPilots(logWarrior->InventoryBlock->ListIndex, -1);
            GlobalLogPtr->ReorderWarriors();
            screen->CreatePilotInvBlock();
            screen->SetUpPilotInv(0, 1);
            PlaySample(OverPane(screen->InventoryPane, event) ? 0x34 : 0x33);
            break;
        }

        case 6:
        {
            // Right button up: put down the carried pilot.
            if (PilotDrag.Dragging != 0)
            {
                break;
            }

            PilotDrag.Carrying = 0;

            if (Application->GrabbedObject() == nullptr)
            {
                break;
            }

            Application->SetCursorVisible(1);
            Application->Release();
            DeleteDragIcon();
            MCLogWarrior* logWarrior = Warrior;
            logWarrior->Assigned = 1;
            GlobalLogPtr->ReorderWarriors();
            GlobalLogPtr->ShiftPilots(logWarrior->InventoryBlock->ListIndex, 1);
            screen->CreatePilotInvBlock();
            screen->SetUpPilotInv(0, 1);
            DrawDropArt(screen, 1);

            if (OnPurchaseScreen())
            {
                PilotToSell = logWarrior;
                auto* picture = new MCLogPort;
                std::snprintf(text, sizeof(text), "%slogart\\%s", ArtPath, logWarrior->Picture);
                picture->Init(text);
                int32_t price = SalePrice(GlobalLogPtr->PilotCosts[logWarrior->Rank]);
                CLoadString(ThisInstance, 0x5f, text, 0xfe);
                GlobalLogPtr->PurchaseDialog->Init(3, -price, 1, logWarrior->Callsign, text, picture);
                delete picture;
                GlobalLogPtr->PurchaseDialog->SetPort(GlobalLogPtr->CurrentScreen->Lport());
                GlobalLogPtr->PurchaseDialog->SetCallback(PilotSellCallback);
                GlobalLogPtr->PurchaseDialog->Activate();
                break;
            }

            MCLogMech* selected = GlobalLogPtr->RepairScreen->SelectedMech;

            if (selected != nullptr)
            {
                // Onto the selected mech.
                MCPilotInventoryBlock* block = logWarrior->InventoryBlock;
                block->Mech = selected;
                int32_t row = block->ListIndex;
                GlobalLogPtr->SetPilot(selected->RepairBlock->SlotIndex, row);
                MCMechRepairBlock* repairBlock = selected->RepairBlock;
                repairBlock->DrawBR(nullptr);
                GlobalLogPtr->SetPilot(repairBlock->SlotIndex, row);
                screen->CreatePilotInvBlock();
                screen->SetUpPilotInv(0, 1);
                SoundSystem->PlayPilotSpeech(logWarrior->PilotAudio, 2);
                break;
            }

            // No mech selected: back to the inventory. The block is this one still (the lists were only renumbered).
            logWarrior = Warrior;
            logWarrior->Assigned = 0;
            GlobalLogPtr->ShiftPilots(logWarrior->InventoryBlock->ListIndex, -1);
            GlobalLogPtr->ReorderWarriors();
            screen->CreatePilotInvBlock();
            screen->SetUpPilotInv(0, 1);
            PlaySample(OverPane(screen->InventoryPane, event) ? 0x34 : 0x33);
            break;
        }

        case 7:
        {
            // Mouse move: the dragged icon follows; otherwise the ticker shows the row's help.
            if (PilotDrag.Dragging == 0)
            {
                if (event->Key == 0)
                {
                    CLoadString(ThisInstance, OnPurchaseScreen() ? 0x2e : 0x33, text, 0xfe);
                    GlobalLogPtr->Ticker->SetString(text);
                }
            }
            else
            {
                PilotDrag.X = event->X - 0xf;
                PilotDrag.Y = event->Y - 0xf;
                GlobalLogPtr->DragIcon->MoveTo(PilotDrag.X, PilotDrag.Y, 0);
            }
            break;
        }
    }
}

// VehicleInventoryBlock

MCVehicleInventoryBlock::~MCVehicleInventoryBlock()
{
    MCVehicleInventoryBlock::Destroy();
}

auto MCVehicleInventoryBlock::Init(MCLogVehicle* logVehicle) -> void
{
    Vehicle = logVehicle;
    MCInventoryBlock::Init(0, 0, GlobalLogPtr->PurchaseScreen->Lport());
    ListIndex = Vehicle->NameIndex;
    char text[256];
    CLoadString(ThisInstance, WeightClassString(Vehicle->CurTonnage), text, 0xfe);
    WeightClassText = HeapString(text);
    // The armor rating from the chassis (armor) tonnage.
    float armor = Vehicle->ArmorTonnage;
    uint32_t id;

    if (armor <= 2.0f)
    {
        id = 100;
    }
    else if (armor <= 7.0f)
    {
        id = 0x4f;
    }
    else if (armor <= 12.0f)
    {
        id = 0x65;
    }
    else if (armor <= 17.0f)
    {
        id = 0x51;
    }
    else
    {
        id = 0x66;
    }

    CLoadString(ThisInstance, id, text, 0xf);
    ArmorText = HeapString(text);
}

auto MCVehicleInventoryBlock::Destroy() -> void
{
    Vehicle = nullptr;
    LogFree(WeightClassText);
    WeightClassText = nullptr;
    LogFree(ArmorText);
    ArmorText = nullptr;
    MCInventoryBlock::Destroy();
}

auto MCVehicleInventoryBlock::Draw() -> void
{
}

auto MCVehicleInventoryBlock::HandleEvent(MCGuiEvent* event) -> void
{
    if (!PreHandleEvent(this, VehicleDrag, event))
    {
        return;
    }

    MCLogInvScreen* screen = InvScreen();
    bool idle = VehicleDrag.Dragging == 0 && VehicleDrag.Carrying == 0;
    char text[256];

    if (idle)
    {
        // The info block: diagram, tonnage, classes, speed and description.
        screen->DrawBlankInvInfoBlock(-1);
        PrepareInfoDescription(Vehicle->Description);
        screen->ShowInfo(MCInvInfoBox::Kind::Vehicle, this);
    }

    switch (event->Type)
    {
        case 1:
        {
            if (VehicleDrag.Carrying != 0)
            {
                break;
            }

            [[fallthrough]];
        }
        case 3:
        {
            // Left button down drags the vehicle (it joins the force while dragged); right button down picks it up.
            if (VehicleDrag.Dragging != 0)
            {
                break;
            }

            PlaySample(0x35);
            Application->SetCursorVisible(0);
            Application->Grab(this);
            MakeDragIcon(VehicleDrag, this, event);

            if (event->Type == 1)
            {
                VehicleDrag.Dragging = 1;
                Vehicle->Assigned = 1;
                GlobalLogPtr->ReorderVehicles();
                screen->CreateVhclInvBlock();
                screen->SetUpVhclInv(0, 0);
            }
            else
            {
                VehicleDrag.Carrying = 1;
            }

            RaiseDragIcon();
            [[fallthrough]];
        }
        case 7:
        {
            // Mouse move: the dragged icon follows; otherwise the ticker shows the row's help.
            if (VehicleDrag.Dragging == 0)
            {
                if (event->Key == 0)
                {
                    CLoadString(ThisInstance, OnPurchaseScreen() ? 0x2d : 0x47, text, 0xfe);
                    GlobalLogPtr->Ticker->SetString(text);
                }
            }
            else
            {
                VehicleDrag.Y = event->Y - 0xf;
                VehicleDrag.X = event->X - 0xf;
                GlobalLogPtr->DragIcon->MoveTo(VehicleDrag.X, VehicleDrag.Y, 0);
            }
            break;
        }

        case 4:
        {
            // Left button up: drop the dragged vehicle.
            if (VehicleDrag.Carrying != 0 || VehicleDrag.Dragging == 0)
            {
                break;
            }

            Application->SetCursorVisible(1);
            Application->Release();
            VehicleDrag.Dragging = 0;
            DeleteDragIcon();
            MCLogVehicle* logVehicle = Vehicle;

            if (OnRepairScreen())
            {
                if (GlobalLogPtr->ForceMechList->NumMechs + GlobalLogPtr->ForceVehicleList->NumVehicles > 0xf)
                {
                    // The force is full.
                    PlaySample(0x33);
                    logVehicle->Assigned = 0;
                    GlobalLogPtr->ReorderVehicles();
                    screen->CreateVhclInvBlock();
                    screen->SetUpVhclInv(0, 1);
                    ShowMessage(0x285, true);
                    break;
                }

                DrawDropArt(screen, 3);

                if (OverPane(screen->UnitPane, event))
                {
                    PlaySample(0x34);
                    GlobalLogPtr->RepairScreen->UnitPane->AddChild(logVehicle->RepairBlock);
                    // The vehicle path bumps the drop slots' second field.
                    BumpDeploySlots(true);
                    GlobalLogPtr->RepairScreen->AddVehicleToList(logVehicle);
                    GlobalLogPtr->RepairScreen->SelectVehicle(logVehicle);
                    break;
                }

                if (OverPane(screen->InventoryPane, event))
                {
                    PlaySample(0x34);
                    logVehicle->Assigned = 0;
                }
                else
                {
                    PlaySample(0x33);
                    logVehicle->Assigned = 0;
                }
            }
            else if (OverPane(screen->UnitPane, event))
            {
                GlobalVehicle = logVehicle;

                if (logVehicle->Required == 0)
                {
                    // Dropped on the store: offer to sell it.
                    PlaySample(0x34);
                    int32_t price = SalePrice(logVehicle->VehicleResourcePoints);
                    OpenSaleDialog(7, price, 1, logVehicle->FileName, nullptr, PicturePort, VehicleSellCallback);
                    break;
                }

                PlaySample(OverPane(screen->InventoryPane, event) ? 0x34 : 0x33);
                logVehicle->Assigned = 0;
            }
            else
            {
                PlaySample(0x33);
                logVehicle->Assigned = 0;
            }

            GlobalLogPtr->ReorderVehicles();
            screen->CreateVhclInvBlock();
            screen->SetUpVhclInv(0, 1);
            break;
        }

        case 6:
        {
            // Right button up: put down the carried vehicle.
            if (VehicleDrag.Dragging != 0)
            {
                break;
            }

            VehicleDrag.Carrying = 0;

            if (Application->GrabbedObject() == nullptr)
            {
                break;
            }

            Application->SetCursorVisible(1);
            Application->Release();
            DeleteDragIcon();
            MCLogVehicle* logVehicle = Vehicle;
            logVehicle->Assigned = 1;
            GlobalLogPtr->ReorderVehicles();
            screen->CreateVhclInvBlock();
            screen->SetUpVhclInv(0, 1);

            if (OnRepairScreen())
            {
                if (GlobalLogPtr->ForceMechList->NumMechs + GlobalLogPtr->ForceVehicleList->NumVehicles < 0x10)
                {
                    DrawDropArt(screen, 3);
                    PlaySample(0x34);
                    GlobalLogPtr->RepairScreen->UnitPane->AddChild(logVehicle->RepairBlock);
                    BumpDeploySlots(true);
                    GlobalLogPtr->RepairScreen->AddVehicleToList(logVehicle);
                    GlobalLogPtr->RepairScreen->SelectVehicle(logVehicle);
                    break;
                }

                PlaySample(0x33);
                logVehicle->Assigned = 0;
                GlobalLogPtr->ReorderVehicles();
                screen->CreateVhclInvBlock();
                screen->SetUpVhclInv(0, 1);
                ShowMessage(0x285, true);
                break;
            }

            GlobalVehicle = logVehicle;

            if (logVehicle->Required == 0)
            {
                PlaySample(0x34);
                int32_t price = SalePrice(logVehicle->VehicleResourcePoints);
                OpenSaleDialog(7, price, 1, logVehicle->FileName, nullptr, PicturePort, VehicleSellCallback);
                break;
            }

            PlaySample(0x33);
            logVehicle->Assigned = 0;
            GlobalLogPtr->ReorderVehicles();
            screen->CreateVhclInvBlock();
            screen->SetUpVhclInv(0, 1);
            break;
        }
    }
}

auto MCVehicleInventoryBlock::DrawBackground() -> void
{
    MCLogVehicle* logVehicle = Vehicle;

    if (PicturePort == nullptr)
    {
        auto* port = new MCLogPort;
        PicturePort = port;
        port->Init(0x1c, 0x1e, 1);
        VfxPaneWipe(port->Frame(), 0x10);

        for (int32_t location = 0; location < 5; ++location)
        {
            GlobalLogPtr->DrawVehicleBodyLoc(logVehicle, location, port, 0, 0);
        }
    }
}

auto MCVehicleInventoryBlock::DrawRow(MCLogPort* port, int32_t top) -> void
{
    MCInventoryBlock::DrawRow(port, top);
    MCLogVehicle* logVehicle = Vehicle;
    WriteText(YellowDropFont, port, 0x26, top + 7, logVehicle->FileName);
    char format[256];
    char text[256];
    CLoadString(ThisInstance, 0x53, format, 0xfe);
    std::snprintf(text, sizeof(text), format, static_cast<double>(logVehicle->CurTonnage), WeightClassText);
    WriteText(BlueDropFont, port, 0x26, top + 0x15, text);

    if (PicturePort != nullptr)
    {
        PicturePort->CopyTo(port->Frame(), 5, top + 2, 1);
    }
}

auto MCVehicleInventoryBlock::DrawInfo(MCLogPort* port) -> void
{
    DrawRowPicture(PicturePort, port);
    char tons[256];
    char text[256];
    CLoadString(ThisInstance, 0x6e, tons, 0xfe);
    std::snprintf(text, sizeof(text), "%.0f %s", static_cast<double>(Vehicle->CurTonnage), tons);
    WriteText(YellowDropFont, port, 0x53, 0x193, text);
    WriteText(YellowDropFont, port, 0x53, 0x19c, WeightClassText);
    WriteText(YellowDropFont, port, 0xa9, 0x193, ArmorText);
    std::snprintf(text, sizeof(text), "%d m/s", Vehicle->MaxMoveSpeed);
    WriteText(YellowDropFont, port, 0x53, 0x1a5, text);
    DrawInfoDescription(port, 0xc3, 0x26, Vehicle->Description, 8, 0x1b3);
}

// CompInventoryBlock

MCCompInventoryBlock::~MCCompInventoryBlock()
{
    MCCompInventoryBlock::Destroy();
}

auto MCCompInventoryBlock::Init(MCLogInventoryItem* newItem) -> void
{
    Item = newItem;
    MCInventoryBlock::Init(0, 0, GlobalLogPtr->PurchaseScreen->Lport());
    MCMasterComponent* list = MasterComponentList.data();
    MCMasterComponent& component = list[Item->MasterID];
    MCComponentForm form = component.Form;
    Tonnage = component.Tonnage;

    if (form == MCComponentForm::WeaponBallistic || form == MCComponentForm::WeaponMissile)
    {
        Tonnage = list[component.AmmoMasterId].Tonnage + Tonnage;
    }

    char text[256];
    CLoadString(ThisInstance, 0x6e, text, 0xfe);
    std::snprintf(WeightText, sizeof(WeightText), "%.1f %s", static_cast<double>(Tonnage), text);

    if (form == MCComponentForm::WeaponEnergy || form == MCComponentForm::WeaponBallistic ||
        form == MCComponentForm::WeaponMissile)
    {
        // Weapons: the long range as a word, damage and recycle time.
        float range = component.WeaponRange[3];
        CLoadString(ThisInstance, range < 76.0f ? 0x55 : range < 151.0f ? 0x50 : 0x6d, text, 0xfe);
        SetText(RangeText, text);
        double damage = component.Damage;

        if (component.WeaponFlags == 4)
        {
            damage *= 3.0;
        }

        std::snprintf(DamageText, sizeof(DamageText), "%.2f", damage);
        std::snprintf(RecycleText, sizeof(RecycleText), "%.2f s", static_cast<double>(component.RecycleTime));
    }
    else
    {
        CLoadString(ThisInstance, 0x6c, text, 0xfe);

        if (form == MCComponentForm::Probe)
        {
            SetText(RangeText, text);
        }
        else
        {
            // Original behaviour (OB-078): other equipment than ECM and sensors formats the item pointer's bits as
            // the range; for a heap address that is a denormal, so it read "0.0 m".
            float range = 0.0f;

            if (form == MCComponentForm::Ecm || form == MCComponentForm::Sensor)
            {
                range = component.RangeOrHeat;
            }

            std::snprintf(RangeText, sizeof(RangeText), "%.1f m", static_cast<double>(range));
        }

        SetText(DamageText, text);
        SetText(RecycleText, text);
    }

    // The icon: the range colour's block background, name, a caption and the component picture.
    MCMasterComponent& again = MasterComponentList[Item->MasterID];
    form = again.Form;
    float range = 0.0f;
    Tonnage = again.Tonnage;

    if (form == MCComponentForm::WeaponBallistic || form == MCComponentForm::WeaponEnergy ||
        form == MCComponentForm::WeaponMissile)
    {
        range = again.WeaponRange[3];
    }

    if (form == MCComponentForm::WeaponBallistic || form == MCComponentForm::WeaponMissile)
    {
        Tonnage = MasterComponentList[again.AmmoMasterId].Tonnage + Tonnage;
    }

    auto* icon = new MCLogPort;
    IconPort = icon;
    auto* own = new MCLogPort;
    _OwnPort = own;
    const char* background = range < 76.0f ? "greeninv.tga" : range < 151.0f ? "blueinv.tga" : "redinv.tga";
    std::snprintf(text, sizeof(text), "%slogart\\%s", ArtPath, background);
    icon->Init(text);
    own->Init(text);
    WriteText(YellowDropFont, icon, 0x26, 7, again.Name.c_str());
    CLoadString(ThisInstance, 0x37d, text, 0xfe);
    WriteText(BlueDropFont, icon, 0x26, 0x15, text);
    std::snprintf(text, sizeof(text), "%slogart\\lscicc%02d.tga", ArtPath, Item->RangeIndex);
    auto* picture = new MCLogPort;
    picture->Init(text);
    picture->CopyTo(icon->Frame(), 3, 2, 1);
    delete picture;

    if (again.TechBase == 1)
    {
        // Clan technology: a small mark in the range colour.
        int32_t color = again.WeaponRange[3] < 76.0f ? 0xe : again.WeaponRange[3] < 151.0f ? 0xe5 : 0xee;
        VfxLineDraw(icon->Frame(), 5, 7, 5, 8, color);
        VfxLineDraw(icon->Frame(), 6, 5, 6, 8, color);
        VfxLineDraw(icon->Frame(), 7, 4, 7, 8, color);
        VfxLineDraw(icon->Frame(), 8, 5, 8, 8, color);
        VfxLineDraw(icon->Frame(), 9, 7, 9, 8, color);
    }
}

auto MCCompInventoryBlock::Destroy() -> void
{
    Item = nullptr;

    if (IconPort != nullptr)
    {
        delete IconPort;
        IconPort = nullptr;
    }

    if (_OwnPort != nullptr)
    {
        delete _OwnPort;
        _OwnPort = nullptr;
    }

    MCInventoryBlock::Destroy();
}

auto MCCompInventoryBlock::HandleEvent(MCGuiEvent* event) -> void
{
    if (!PreHandleEvent(this, CompDrag, event))
    {
        return;
    }

    MCLogInvScreen* screen = InvScreen();
    char text[256];

    if (CompDrag.Carrying == 0 && CompDrag.Dragging == 0)
    {
        // The info block: picture, range, damage, recycle time and description.
        screen->DrawBlankInvInfoBlock(-1);
        PrepareInfoDescription(Item->Description);
        screen->ShowComponentInfo(this, false);
    }

    // Where a drop that didn't mount or sell ends: the sound, then the copy goes back to the row.
    auto returnToInventory = [&](uint32_t sample)
    {
        PlaySample(sample);

        if (!OnPurchaseScreen() && CompDrag.Carrying == 0)
        {
            ++Item->Count;
        }

        if (Item->Count > 1)
        {
            DrawBackground();
        }
        else
        {
            screen->CreateCompInvBlock();
            screen->SetUpCompInv(0, 1);
        }

        CompDrag.Carrying = 0;
    };

    auto openSale = [&]()
    {
        PlaySample(0x34);
        auto* picture = new MCLogPort;
        MCLogInventoryItem* sold = Item;
        std::snprintf(text, sizeof(text), "%slogart\\lscicc%02d.tga", ArtPath, sold->RangeIndex);
        picture->Init(text);
        MCMasterComponent& component = MasterComponentList[sold->MasterID];
        int32_t price = SalePrice(component.ResourcePoints);
        GlobalCompPtr = sold;
        GlobalLogPtr->PurchaseDialog->Init(5, -price, sold->Count, component.Name.c_str(), nullptr, picture);
        delete picture;
        GlobalLogPtr->PurchaseDialog->SetPort(GlobalLogPtr->CurrentScreen->Lport());
        GlobalLogPtr->PurchaseDialog->SetCallback(CompSellCallback);
        GlobalLogPtr->PurchaseDialog->Activate();
    };

    switch (event->Type)
    {
        case 1:
        {
            // Left button down: drag one copy (off the row on the repair screen).
            if (CantMount != 0 || CompDrag.Carrying != 0)
            {
                break;
            }

            PlaySample(0x35);
            Application->SetCursorVisible(0);
            Application->Grab(this);
            CompDrag.Dragging = 1;
            MakeDragIcon(CompDrag, this, event);

            if (screen != GlobalLogPtr->PurchaseScreen)
            {
                if (--Item->Count != 0)
                {
                    DrawBackground();
                }
                else
                {
                    screen->CreateCompInvBlock();
                    screen->SetUpCompInv(0, 0);
                }
            }

            RaiseDragIcon();
            break;
        }

        case 3:
        {
            // Right button down: pick one copy up.
            if (CantMount != 0 || CompDrag.Dragging != 0)
            {
                break;
            }

            CompDrag.Carrying = 1;
            PlaySample(0x35);
            Application->SetCursorVisible(0);
            Application->Grab(this);
            MakeDragIcon(CompDrag, this, event);

            if (screen != GlobalLogPtr->PurchaseScreen && --Item->Count != 0)
            {
                DrawBackground();
            }

            RaiseDragIcon();
            break;
        }

        case 4:
        {
            // Left button up: drop the dragged copy.
            if (CompDrag.Dragging == 0)
            {
                break;
            }

            Application->SetCursorVisible(1);
            Application->Release();
            CompDrag.Dragging = 0;
            DeleteDragIcon();

            if (OnRepairScreen())
            {
                DrawDropArt(screen, 2);
                MCScrollPane* unitPane = screen->UnitPane;

                if (OverPane(unitPane, event))
                {
                    // Onto a mech: it must be the selected one.
                    int32_t index = (event->Y - unitPane->GlobalY() + unitPane->GetScrollOffset()) / 0x70;

                    if (index < unitPane->NumberOfChildren() && index < GlobalLogPtr->ForceMechList->NumMechs)
                    {
                        MCLogMech* target = nullptr;
                        GlobalLogPtr->ForceMechList->GetMechInfo(index, target);

                        if (target != nullptr && target == GlobalLogPtr->RepairScreen->SelectedMech &&
                            MountComponent(Item, target))
                        {
                            break;
                        }
                    }

                    returnToInventory(0x33);
                    break;
                }

                returnToInventory(
                    OverPane(screen->InventoryPane, screen->InventoryPane, screen->UnitPane, event) ? 0x34 : 0x33);
                break;
            }

            if (OverPane(screen->UnitPane, event))
            {
                openSale();
                break;
            }

            returnToInventory(OverPane(screen->InventoryPane, screen->UnitPane, screen->UnitPane, event) ? 0x34 : 0x33);
            break;
        }

        case 6:
        {
            // Right button up: put down the carried copy.
            if (CompDrag.Carrying == 0)
            {
                break;
            }

            Application->SetCursorVisible(1);
            Application->Release();
            DeleteDragIcon();
            screen->CreateCompInvBlock();
            screen->SetUpCompInv(0, 1);

            if (OnRepairScreen())
            {
                // Onto the selected mech.
                // Original behaviour (OB-079): when it can't be mounted, the copy taken off the row at the pick-up is
                // not given back (the count is only restored while nothing is carried), so it is lost.
                MCLogMech* selected = GlobalLogPtr->RepairScreen->SelectedMech;

                if (selected != nullptr && MountComponent(Item, selected))
                {
                    CompDrag.Carrying = 0;
                    break;
                }

                returnToInventory(0x33);
                break;
            }

            openSale();
            CompDrag.Carrying = 0;
            break;
        }

        case 7:
        {
            // Mouse move: the dragged icon follows (a carried one only updates its position); otherwise the ticker
            // shows the row's help.
            if (CompDrag.Dragging != 0)
            {
                CompDrag.Y = event->Y - 0xf;
                CompDrag.X = event->X - 0xf;
                GlobalLogPtr->DragIcon->MoveTo(CompDrag.X, CompDrag.Y, 0);
            }
            else if (CompDrag.Carrying != 0)
            {
                CompDrag.X = event->X - 0xf;
                CompDrag.Y = event->Y - 0xf;
            }
            else if (event->Key == 0)
            {
                CLoadString(ThisInstance, OnPurchaseScreen() ? 0x2d : 0x32, text, 0xfe);
                GlobalLogPtr->Ticker->SetString(text);
            }
            break;
        }
    }
}

auto MCCompInventoryBlock::DrawBackground() -> void
{
    if (ListIndex < 0)
    {
        ShowGuiWindow(0);
        return;
    }

    ShowGuiWindow(1);
    CantMount = 0;

    if (!OnPurchaseScreen())
    {
        // On the repair screen: can it go on the selected mech?
        MCLogMech* selected = GlobalLogPtr->RepairScreen->SelectedMech;
        uint32_t masterID = Item->MasterID;
        MCComponentForm form = MasterComponentList[masterID].Form;

        if (selected == nullptr)
        {
            CantMount = 1;
        }
        else if (static_cast<double>(selected->CurTonnage) - selected->UsedTonnage < Tonnage)
        {
            CantMount = 1;
        }
        else
        {
            // One ECM, sensor or probe per mech.
            if (form == MCComponentForm::Ecm || form == MCComponentForm::Sensor || form == MCComponentForm::Probe)
            {
                for (MCLogInventoryItem* mounted = selected->Inventory->Items; mounted != nullptr;
                     mounted = mounted->Next)
                {
                    if (form == MasterComponentList[mounted->MasterID].Form)
                    {
                        CantMount = 1;
                    }
                }
            }

            // Some components only fit the mechs of name index 5, 0xe and 0x10.
            for (int32_t i = 0; CantMount == 0 && i < NumRestrictedComponents; ++i)
            {
                if (RestrictedComps[i] == static_cast<int32_t>(masterID) && selected->NameIndex != 0xe &&
                    selected->NameIndex != 0x10 && selected->NameIndex != 5)
                {
                    CantMount = 1;
                }
            }
        }
    }
}

auto MCCompInventoryBlock::DrawRow(MCLogPort* port, int32_t top) -> void
{
    // Put together in place, as the original did in the block's own picture, then copied opaque.
    MCLogPort* row = NewRowPicture(IconPort, port, top, false);
    char text[256];
    std::snprintf(text, sizeof(text), "%d", Item->Count);
    WriteText(BlueDropFont, row, 0x67, 0x15, text);

    if (CantMount != 0)
    {
        GlobalLogPtr->Darken(0, LogisticFadetable, row);
    }

    delete row;
}
