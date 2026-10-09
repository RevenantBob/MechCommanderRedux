#include "stdafx.h"
#include "logistics/purchase.h"
#include "gui/afont.h"
#include "gui/scrlpane.h"
#include "lib/MCFatal.h"
#include "lib/MCIDString.h"
#include "lib/MCFitIniFile.h"
#include "logistics/invblock.h"
#include "logistics/logbri.h"
#include "logistics/logdlg.h"
#include "logistics/loggen.h"
#include "logistics/logmain.h"
#include "logistics/logpur.h"
#include "logistics/logrep.h"
#include "logistics/ticker.h"
#include "main/logistics.h"
#include "main/main.h"
#include "mission/scenario.h"
#include "object/MCMasterComponent.h"
#include "object/MCBattleMech.h"
#include "object/MCBattleMechType.h"
#include "object/MCMechGameSystem.h"
#include "object/MCObjectType.h"
#include "sound/soundsys.h"
#include "vfx/MCVfxFunctions.h"
#include "object/MCObjectTypeManager.h"

int32_t ResourcePoints = 0;
MCMechPurchaseBlock* GlobalMechPurchaseBlock = nullptr;
MCPilotPurchaseBlock* GlobalPilotPurchaseBlock = nullptr;
MCLogInventoryItem* GlobalItemPtr = nullptr;
MCVehiclePurchaseBlock* GlobalVehicleBlockPtr = nullptr;
int32_t BodyTrans[8] = {7, 6, 4, 5, 0, 1, 2, 3};
char ObjectDesc[] = "desc.fit";

namespace
{
    /// <summary>The drag state of one kind of shop row.</summary>
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

    /// <summary>The mech rows' drag (0x008086f0 dragging, f4/f8 x/y, fc carrying).</summary>
    MCDragState MechDrag;
    /// <summary>The vehicle rows' drag (0x00808704 dragging, 708/70c x/y, 710 carrying).</summary>
    MCDragState VehicleDrag;
    /// <summary>The component rows' drag (0x00808718 dragging, 71c/720 x/y, 728 carrying).</summary>
    MCDragState CompDrag;
    /// <summary>The pilot rows' drag (0x0080872c dragging, 730/734 x/y, 738 carrying).</summary>
    MCDragState PilotDrag;

    /// <summary>The body location blocks of a mech profile, in location order.</summary>
    const char* const BodyLocationNames[8] = {"Head",    "CenterTorso", "LeftTorso", "RightTorso",
                                              "LeftArm", "RightArm",    "LeftLeg",   "RightLeg"};

    /// <summary>The armor locations of a mech profile's MaxArmorPoints and CurArmorPoints blocks.</summary>
    const char* const ArmorLocationNames[11] = {
        "Head",    "CenterTorso", "LeftTorso",       "RightTorso",    "LeftArm",       "RightArm",
        "LeftLeg", "RightLeg",    "RearCenterTorso", "RearLeftTorso", "RearRightTorso"};

    void* LogAlloc(uint32_t size)
    {
        return GlobalLogPtr->LogisticsBlocks->Allocate(size);
    }

    void LogFree(void* block)
    {
        GlobalLogPtr->LogisticsBlocks->Free(block);
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

    void WriteText(MCGuiFont* font, MCLogPort* port, int32_t x, int32_t y, const char* text)
    {
        font->WriteString(port->Frame(), x, y, reinterpret_cast<uint8_t*>(const_cast<char*>(text)), -1);
    }

    /// <summary>Loads "<c>artPath</c>logart\..." (<paramref name="format"/> with the art path and a number) into <paramref name="port"/>.</summary>
    void LoadArt(MCLogPort* port, const char* format, int32_t number)
    {
        char fileName[256];
        std::snprintf(fileName, sizeof(fileName), format, ArtPath, number);
        port->Init(fileName);
    }

    /// <summary>Shows the one-button message dialog with string <paramref name="id"/> and the "okay" button art.</summary>
    void ShowMessage(uint32_t id)
    {
        char text[256];
        CLoadString(ThisInstance, id, text, 0xfe);
        MCReusableDialog* dialog = GlobalLogPtr->MessageDialog;
        dialog->SetText(text);
        GlobalLogPtr->MessageDialog->SetTwoButton(0);
        dialog = GlobalLogPtr->MessageDialog;
        dialog->Callback = nullptr;
        char upArt[] = "bh_okay.tga";
        char downArt[] = "bg_okay.tga";
        dialog->OkButton->SetUpPicture(upArt);
        GlobalLogPtr->MessageDialog->OkButton->SetDownPicture(downArt);
        MCLogDialogButton* button = GlobalLogPtr->MessageDialog->OkButton;
        button->Disabled = 0;
        GlobalLogPtr->MessageDialog->Activate();
    }

    /// <summary>How many mechs and vehicles the player owns (inventory and force).</summary>
    int32_t NumUnits()
    {
        return GlobalLogPtr->ForceVehicleList->NumVehicles + GlobalLogPtr->ForceMechList->NumMechs +
               GlobalLogPtr->VehicleList->NumVehicles + GlobalLogPtr->MechList->NumMechs;
    }

    /// <summary>How many units a purchase may buy: the room left under 50 units, or the stock when that is less.</summary>
    int32_t MaxPurchase(int32_t available)
    {
        int32_t room = 0x32 - GlobalLogPtr->ForceVehicleList->NumVehicles - GlobalLogPtr->ForceMechList->NumMechs -
                       GlobalLogPtr->VehicleList->NumVehicles - GlobalLogPtr->MechList->NumMechs;

        if (available < room && available > -1)
        {
            room = available;
        }

        return room;
    }

    /// <summary>Opens the purchase dialog on the current screen.</summary>
    void OpenPurchaseDialog(int32_t purchaseType, int32_t cost, int32_t maxQuantity, char* title, char* subtitle,
                            MCLogPort* picture, void (*callback)(int, int32_t))
    {
        GlobalLogPtr->PurchaseDialog->Init(purchaseType, cost, maxQuantity, title, subtitle, picture);
        GlobalLogPtr->PurchaseDialog->SetPort(GlobalLogPtr->CurrentScreen->Lport());
        GlobalLogPtr->PurchaseDialog->SetCallback(callback);
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

    /// <summary>The string table index of the armor rating of <paramref name="armorTonnage"/>.</summary>
    uint32_t ArmorClassString(float armorTonnage)
    {
        if (armorTonnage <= 2.0f)
        {
            return 100;
        }

        if (armorTonnage <= 7.0f)
        {
            return 0x4f;
        }

        if (armorTonnage <= 12.0f)
        {
            return 0x65;
        }

        if (armorTonnage <= 17.0f)
        {
            return 0x51;
        }

        return 0x66;
    }

    /// <summary>
    /// Whether the event is over a pane's inside (0xd pixels short of its right edge). The shop rows take the
    /// position from the purchase screen's pane and the size from the repair screen's.
    /// </summary>
    bool OverPane(MCGuiObject* posPane, MCGuiObject* sizePane, MCGuiEvent* event)
    {
        return posPane->GlobalX() < event->X && event->X < posPane->GlobalX() + sizePane->Width() - 0xd &&
               posPane->GlobalY() < event->Y && event->Y < posPane->GlobalY() + sizePane->Height();
    }

    bool OverInventory(MCGuiEvent* event)
    {
        return OverPane(GlobalLogPtr->PurchaseScreen->InventoryPane, GlobalLogPtr->RepairScreen->InventoryPane, event);
    }

    bool OverStore(MCGuiEvent* event)
    {
        return OverPane(GlobalLogPtr->PurchaseScreen->UnitPane, GlobalLogPtr->RepairScreen->UnitPane, event);
    }

    /// <summary>Whether the event is on <paramref name="row"/> (edges included).</summary>
    bool OnRow(MCGuiObject* row, MCGuiEvent* event)
    {
        return row->GlobalX() <= event->X && event->X <= row->GlobalX() + row->Width() && row->GlobalY() <= event->Y &&
               event->Y <= row->GlobalY() + row->Height();
    }

    /// <summary>
    /// Makes the drag icon: a 0x20 square of the row (<paramref name="render"/>, the row's <c>OnBeginDrag</c>), framed
    /// in colour 0xea, added to the purchase screen at (<paramref name="drag"/>.x, .y).
    /// </summary>
    void MakeDragIcon(const MCDragState& drag, const std::function<void(MCLogPort* surface)>& render)
    {
        auto* icon = new MCDragIcon;
        GlobalLogPtr->DragIcon = icon;
        icon->Begin(drag.X, drag.Y, 0x20, 0x20, render);
        GlobalLogPtr->PurchaseScreen->AddChild(GlobalLogPtr->DragIcon);
        GlobalLogPtr->DragIcon->ShowGuiWindow(1);
        GlobalLogPtr->DragIcon->SetDepth(100);
        GlobalLogPtr->DragIcon->MoveTo(drag.X, drag.Y, 0);
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
    /// Reads description <paramref name="descIndex"/> of the object description file: "%fc4" (a colour code) and
    /// the text, a logistics block. Null when the file has no such block.
    /// </summary>
    char* LoadDescriptionText(int32_t descIndex)
    {
        auto* file = new MCFitIniFile;
        char text[1024];
        std::snprintf(text, sizeof(text), "%s%s", ObjectPath, ObjectDesc);
        int32_t result = file->Open(text);
        Assert(result == 0, result, "Could not open description file");
        std::snprintf(text, sizeof(text), "Desc%d", descIndex);
        char* description = nullptr;

        if (file->SeekBlock(text) == 0)
        {
            result = file->ReadIdString("DescString", text, 0x3ff);
            Assert(result == 0 || static_cast<uint32_t>(result) == 0xfada0003, result,
                   "Could not read description string");
            size_t length = std::strlen(text) + 1;
            description = static_cast<char*>(LogAlloc(static_cast<uint32_t>(length + 4)));
            std::snprintf(description, length + 4, "%%fc4%s", text);
            description[length + 3] = '\0';
        }

        delete file;
        return description;
    }

    /// <summary>
    /// Opens a profile: <paramref name="dir"/> name.fit (tried twice), then the save-temp folder's name.fit, and for
    /// a mech or pilot the save-temp folder's bare name.
    /// </summary>
    void OpenProfile(MCFitIniFile* file, std::string& path, const char* dir, const char* name, bool bareName,
                     const char* error)
    {
        path = GamePath(dir, name, ".fit");

        if (file->Open(path) == 0)
        {
            return;
        }

        path = GamePath(ProfilePath, name, ".fit");

        if (file->Open(path) == 0)
        {
            return;
        }

        path = GamePath(SaveTempPath, name, ".fit");
        int32_t result = file->Open(path);

        if (result == 0 || !bareName)
        {
            Assert(result == 0, result, error);
            return;
        }

        path = GamePath(SaveTempPath, name);
        result = file->Open(path);
        Assert(result == 0, result, error);
    }

    /// <summary>
    /// Reads the Item:n blocks of a profile's inventory (NumOther others, NumWeapons weapons, NumAmmo ammo) into
    /// <paramref name="inventory"/>.
    /// </summary>
    /// <returns>The sum of the items' resource points.</returns>
    int32_t ReadInventory(MCFitIniFile* file, MCInventoryList* inventory, uint8_t numOther, uint8_t numWeapons,
                          uint8_t numAmmo)
    {
        int32_t cost = 0;
        char block[32];
        int32_t item = 0;

        for (; item < numOther; ++item)
        {
            std::snprintf(block, sizeof(block), "Item:%d", item);
            int32_t result = file->SeekBlock(block);
            Assert(result == 0, result, "Could not read 'other' item in mech file");
            uint8_t masterID = 0;
            result = file->ReadIdUChar("MasterID", masterID);
            Assert(result == 0, result, "Could not read 'other' item's MasterID in mech file");
            MCLogInventoryStat* stat = inventory->CreateStat(static_cast<uint8_t>(item), 0, 0, 1, 0xff);
            inventory->AddItem(masterID, stat, -1);
            cost += MasterComponentList[masterID].ResourcePoints;
        }

        for (; item < numOther + numWeapons; ++item)
        {
            std::snprintf(block, sizeof(block), "Item:%d", item);
            int32_t result = file->SeekBlock(block);
            Assert(result == 0, result, "Could not read 'weapon' item in mech file");
            uint8_t masterID = 0;
            result = file->ReadIdUChar("MasterID", masterID);
            Assert(result == 0, result, "Could not read 'weapon' item's MasterID in mech file");
            uint8_t facesForward = 0;
            result = file->ReadIdUChar("FacesForward", facesForward);
            Assert(result == 0, result, "Could not read 'weapon' item's FacesForward in mech file");
            MCLogInventoryStat* stat = inventory->CreateStat(static_cast<uint8_t>(item), 0, facesForward, 1, 0xff);
            inventory->AddItem(masterID, stat, -1);
            cost += MasterComponentList[masterID].ResourcePoints;
        }

        for (; item < numOther + numWeapons + numAmmo; ++item)
        {
            std::snprintf(block, sizeof(block), "Item:%d", item);
            int32_t result = file->SeekBlock(block);
            Assert(result == 0, result, "Could not read 'ammo' item in mech file");
            uint8_t masterID = 0;
            result = file->ReadIdUChar("MasterID", masterID);
            Assert(result == 0, result, "Could not read 'ammo' item's MasterID in mech file");
            int32_t amount = 0;

            if (file->ReadIdLong("Amount", amount) != 0)
            {
                uint8_t smallAmount = 0;
                result = file->ReadIdUChar("Amount", smallAmount);
                Assert(result == 0, result, "Could not read 'ammo' item's Amount in mech file");
                amount = smallAmount;
            }

            MCLogInventoryStat* stat =
                inventory->CreateStat(static_cast<uint8_t>(item), 0, 0, static_cast<int16_t>(amount), 0xff);
            inventory->AddItem(masterID, stat, -1);
            cost += MasterComponentList[masterID].ResourcePoints;
        }

        return cost;
    }

    /// <summary>
    /// Writes a unit's weapons (short, medium, then long range, as "count name") and its sensors, ECM and probes
    /// down the right of the row in the green font.
    /// </summary>
    /// <param name="text">The row's text buffer (the caller may show what is left in it).</param>
    void DrawInventoryList(MCInventoryList* inventory, MCLogPort* port, char* text, size_t textSize)
    {
        std::vector<int32_t> shortRange;
        std::vector<int32_t> mediumRange;
        std::vector<int32_t> longRange;
        int32_t index = 0;

        for (MCLogInventoryItem* item = inventory->Items; item != nullptr; item = item->Next, ++index)
        {
            const MCMasterComponent& component = MasterComponentList[item->MasterID];
            MCComponentForm form = component.Form;

            if (form != MCComponentForm::WeaponEnergy && form != MCComponentForm::WeaponBallistic &&
                form != MCComponentForm::WeaponMissile && form != MCComponentForm::Weapon)
            {
                continue;
            }

            if (component.WeaponRange[3] < 76.0f)
            {
                shortRange.push_back(index);
            }
            else if (component.WeaponRange[3] < 151.0f)
            {
                mediumRange.push_back(index);
            }
            else
            {
                longRange.push_back(index);
            }
        }

        int32_t line = 0;

        for (const auto* group : {&shortRange, &mediumRange, &longRange})
        {
            for (int32_t position : *group)
            {
                MCLogInventoryItem* item = inventory->GetItemInfo(position);
                std::snprintf(text, textSize, "%d %s", item->Count, item->Name);
                WriteText(GreenFont, port, 0x14a, (GreenFont->Height() + 1) * line + 4, text);
                ++line;
            }
        }

        for (MCLogInventoryItem* item = inventory->Items; item != nullptr; item = item->Next)
        {
            MCComponentForm form = MasterComponentList[item->MasterID].Form;

            if (form == MCComponentForm::Sensor || form == MCComponentForm::Ecm || form == MCComponentForm::Probe)
            {
                std::snprintf(text, textSize, "%d %s", item->Count, item->Name);
                WriteText(GreenFont, port, 0x14a, (GreenFont->Height() + 1) * line + 4, text);
                ++line;
            }
        }
    }

    /// <summary>
    /// Where a shop row is put together: a block of <paramref name="port"/> at row <paramref name="top"/>, the size of
    /// <paramref name="art"/>, with the art copied in. The original put the row together in a picture and copied it
    /// there (keyed on 0xff when <paramref name="keyed"/>); it is drawn in place.
    /// </summary>
    MCLogPort* NewRowPicture(MCLogPort* art, MCLogPort* port, int32_t top, bool keyed)
    {
        auto* row = new MCLogBlockPort(port->Frame(), 0, top, art->Width(), art->Height(), keyed);
        art->CopyTo(row->Frame(), 0, 0, 0);
        return row;
    }

    /// <summary>
    /// Copies the art "<c>artPath</c>logart\..." (<paramref name="format"/> with the art path and a number) keyed into
    /// <paramref name="port"/> at (<paramref name="xPos"/>, <paramref name="yPos"/>).
    /// </summary>
    void CopyArt(MCLogPort* port, int32_t xPos, int32_t yPos, const char* format, int32_t number = 0)
    {
        if (MCLogPort* art = LogArtf(format, ArtPath, number))
        {
            art->CopyTo(port->Frame(), xPos, yPos, 1);
        }
    }

    /// <summary>Fills a <paramref name="width"/> x <paramref name="height"/> box of <paramref name="port"/> with <paramref name="color"/>.</summary>
    void FillBox(MCLogPort* port, int32_t xPos, int32_t yPos, int32_t width, int32_t height, int32_t color)
    {
        MCLogBlockPort box(port->Frame(), xPos, yPos, width, height, false);
        VfxPaneWipe(box.Frame(), color);
    }

    /// <summary>A text field set from a string table entry, a logistics block.</summary>
    /// <remarks>Port fix: the previous text is freed (the rows' drawBackground made a new one on every draw).</remarks>
    void SetHeapText(char*& field, const char* text)
    {
        if (field != nullptr)
        {
            LogFree(field);
        }

        field = HeapString(text);
    }
}

// Unit limits and purchase callbacks

auto CheckMaxUnits() -> int
{
    if (NumUnits() > 0x31)
    {
        PlaySample(0x33);
        ShowMessage(0x374);
        return 1;
    }

    return 0;
}

auto CheckNumUnits() -> void
{
    int32_t units = NumUnits();

    if (units <= 0x27)
    {
        return;
    }

    char text[256];

    if (units == 0x32)
    {
        CLoadString(ThisInstance, 0x374, text, 0xfe);
    }
    else
    {
        char format[256];
        CLoadString(ThisInstance, 0x372, format, 0xfe);
        std::snprintf(text, sizeof(text), format, units, 0x32);
    }

    PlaySample(0x33);
    MCReusableDialog* dialog = GlobalLogPtr->MessageDialog;
    dialog->SetText(text);
    GlobalLogPtr->MessageDialog->SetTwoButton(0);
    dialog = GlobalLogPtr->MessageDialog;
    dialog->Callback = nullptr;
    char upArt[] = "bh_okay.tga";
    char downArt[] = "bg_okay.tga";
    dialog->OkButton->SetUpPicture(upArt);
    GlobalLogPtr->MessageDialog->OkButton->SetDownPicture(downArt);
    MCLogDialogButton* button = GlobalLogPtr->MessageDialog->OkButton;
    button->Disabled = 0;
    GlobalLogPtr->MessageDialog->Activate();
}

auto MechPurchaseCallback(int confirmed, int32_t quantity) -> void
{
    if (confirmed == 0 || GlobalMechPurchaseBlock == nullptr || quantity == 0)
    {
        return;
    }

    MCMechPurchaseBlock* block = GlobalMechPurchaseBlock;

    for (int32_t count = quantity; count > 0; --count)
    {
        MCPurMechData* data = block->PurMech->Variants[block->CurVariant];
        GlobalLogPtr->MechList->AddMech(data->FileName, 0, 1, 1);
    }

    GlobalLogPtr->ReorderMechs();
    GlobalLogPtr->PurchaseScreen->CreateMechInvBlock();
    GlobalLogPtr->PurchaseScreen->SetUpMechInv(1, 1);
    GlobalMechPurchaseBlock->PurMech->Variants[GlobalMechPurchaseBlock->CurVariant]->NumAvailable -= quantity;
    GlobalMechPurchaseBlock->DrawBackground(GlobalMechPurchaseBlock->Row);
    ResourcePoints -= GlobalLogPtr->PurchaseDialog->UnitCost * quantity;
    CheckNumUnits();
}

auto PilotPurchaseCallback(int confirmed, int32_t) -> void
{
    if (confirmed == 0)
    {
        return;
    }

    float scrollPos = GlobalLogPtr->PurchaseScreen->UnitPane->ScrollPos;
    GlobalLogPtr->WarriorList->AddWarrior(GlobalPilotPurchaseBlock->Pilot->FileName, 1);
    GlobalLogPtr->ReorderWarriors();
    GlobalLogPtr->PurchaseScreen->CreatePilotInvBlock();
    GlobalLogPtr->PurchaseScreen->SetUpPilotInv(1, 1);
    MCPurPilotData* pilot = GlobalPilotPurchaseBlock->Pilot;
    pilot->Health = 0;
    GlobalLogPtr->PurPilotList->SetPilotStatus(pilot->DescIndex, 1);
    GlobalLogPtr->PurchaseScreen->RemovePilot(GlobalPilotPurchaseBlock->Pilot->Block->Row);
    GlobalLogPtr->PurchaseScreen->SetUpPilotPurchase();
    ResourcePoints -= GlobalLogPtr->PurchaseDialog->UnitCost;
    SoundSystem->PlayPilotSpeech(GlobalPilotPurchaseBlock->Pilot->PilotAudio, 2);
    GlobalLogPtr->PurchaseScreen->UnitPane->SetScrollPos(scrollPos);
}

auto CompPurchaseCallback(int confirmed, int32_t quantity) -> void
{
    MCLogInventoryItem* bought = GlobalItemPtr;

    if (confirmed == 0)
    {
        return;
    }

    bought->Count -= quantity;
    bought->PurchaseBlock->DrawBackground(bought->PurchaseBlock->Row, bought->MasterID);
    MCInventoryList* spares = GlobalLogPtr->ComponentInventory;
    MCLogInventoryItem* stockItem = spares->Items;

    while (stockItem != nullptr && stockItem->MasterID != GlobalItemPtr->MasterID)
    {
        stockItem = stockItem->Next;
    }

    if (stockItem != nullptr)
    {
        if (stockItem->Count == 0)
        {
            stockItem->Count = quantity;
            GlobalLogPtr->PurchaseScreen->CreateCompInvBlock();
            GlobalLogPtr->PurchaseScreen->SetUpCompInv(0, 1);
        }
        else
        {
            stockItem->Count += quantity;
            stockItem->InventoryBlock->DrawBackground();
        }
    }
    else
    {
        // A new spare component: its first copy and its inventory row.
        MCLogInventoryStat* stat = spares->CreateStat(spares->NextStatID, 0, 0, 1, 0xff);
        GlobalLogPtr->ComponentInventory->AddItem(GlobalItemPtr->MasterID, stat, -1);
        MCInventoryList* list = GlobalLogPtr->ComponentInventory;
        stockItem = list->GetItemInfo(list->GetIndexFromMasterID(GlobalItemPtr->MasterID));
        stockItem->Count = quantity;
        auto* block = new MCCompInventoryBlock;
        stockItem->InventoryBlock = block;
        block->Init(stockItem);
        stockItem->InventoryBlock->InventoryIndex = GlobalLogPtr->ComponentInventory->NumItems - 1;
        GlobalLogPtr->PurchaseScreen->CreateCompInvBlock();
        GlobalLogPtr->PurchaseScreen->SetUpCompInv(0, 1);
    }

    ResourcePoints -= GlobalLogPtr->PurchaseDialog->UnitCost * quantity;
}

auto VehiclePurchaseCallback(int confirmed, int32_t quantity) -> void
{
    if (confirmed == 0)
    {
        return;
    }

    for (int32_t count = quantity; count > 0; --count)
    {
        GlobalLogPtr->VehicleList->AddVehicle(GlobalVehicleBlockPtr->PurVehicle->Data->FileName, 0, 1, 1);
    }

    GlobalLogPtr->ReorderVehicles();
    GlobalLogPtr->PurchaseScreen->CreateVhclInvBlock();
    GlobalLogPtr->PurchaseScreen->SetUpVhclInv(1, 1);
    MCVehiclePurchaseBlock* block = GlobalVehicleBlockPtr;
    block->PurVehicle->Data->NumAvailable -= quantity;
    block->DrawBackground(block->Row);
    ResourcePoints -= GlobalLogPtr->PurchaseDialog->UnitCost * quantity;
    CheckNumUnits();
}

// PurMechList

MCPurMechList::MCPurMechList()
{
    Init();
}

auto MCPurMechList::Init() -> void
{
    First = nullptr;
    Count = 0;
}

auto MCPurMechList::Destroy() -> void
{
    for (MCPurMech* purMech = First; purMech != nullptr; purMech = First)
    {
        First = purMech->Next;

        for (MCPurMechData*& data : purMech->Variants)
        {
            if (data == nullptr)
            {
                continue;
            }

            if (data->Inventory != nullptr)
            {
                data->Inventory->Destroy();
                delete data->Inventory;
                data->Inventory = nullptr;
            }

            if (data->Description != nullptr)
            {
                LogFree(data->Description);
                data->Description = nullptr;
            }

            LogFree(data);
            data = nullptr;
        }

        if (purMech->Block != nullptr)
        {
            delete purMech->Block;
            purMech->Block = nullptr;
        }

        delete purMech;
    }

    First = nullptr;
    Count = 0;
}

auto MCPurMechList::AddMech(MCPurMech* purMech, char* fileName, int32_t variant) -> int32_t
{
    std::string path;
    auto* file = new MCFitIniFile;
    Assert(file != nullptr, 0, " no RAM for mech file ");
    OpenProfile(file, path, ProfilePath, fileName, true, " could not open mech file ");

    void* memory = LogAlloc(sizeof(MCPurMechData));
    Assert(memory != nullptr, 0, "Not enough memory for LogMech");
    auto* data = new (memory) MCPurMechData;
    std::strncpy(data->FileName, fileName, 0xb);
    data->Inventory = new MCInventoryList;
    Assert(data != nullptr, 0, "Not enough memory for InventoryList");

    int32_t result = file->SeekBlock("Header");
    Assert(result == 0, result, "Could not find header in mech file");
    char fileType[20];
    result = file->ReadIdString("FileType", fileType, 0x14);
    Assert(result == 0, result, "Could not find filetype string in mech file");
    Assert(std::strcmp(fileType, "MechProfile") == 0, 0, "File is not a mech file");
    result = file->SeekBlock("General");
    Assert(result == 0, result, "Could not find general block in mech file");
    result = file->ReadIdFloat("CurTonnage", data->CurTonnage);
    Assert(result == 0, result, "Could not find curTonnage in mech file");
    char text[256];
    result = file->ReadIdString("MechType", text, 0x28);
    Assert(result == 0, result, "Could not read MechType in mech file");
    result = file->ReadIdLong("NameIndex", data->NameIndex);
    Assert(result == 0, result, " AddPurMech: could not find NameIndex ");
    std::strcpy(data->Name, text);

    if (file->ReadIdLong("ResourcePoints", data->Cost) != 0)
    {
        data->Cost = 100;
    }

    if (file->ReadIdLong("ChassisBR", data->ChassisBR) != 0)
    {
        data->ChassisBR = 100;
    }

    data->Description = nullptr;
    data->DescIndex = -1;
    file->ReadIdLong("DescIndex", data->DescIndex);
    data->LoadDescription(data->DescIndex);
    CLoadString(ThisInstance, static_cast<uint32_t>(data->DescIndex + 300), text, 0x28);
    std::strncpy(data->Name, text, 0x28);
    data->Name[0x28] = '\0';

    result = file->SeekBlock("Engine");
    Assert(result == 0, result, "Could not find Engine block in mech file");
    result = file->ReadIdUChar("MaxRunSpeed", data->MaxRunSpeed);
    Assert(result == 0, result, "Could not read MaxRunSpeed in mech file");
    result = file->SeekBlock("Armor");
    Assert(result == 0, result, "Could not find Armor block in mech file");
    result = file->ReadIdFloat("Tonnage", data->ArmorTonnage);
    Assert(result == 0, result, "Could not read Tonnage in mech file");
    result = file->SeekBlock("MaxArmorPoints");
    Assert(result == 0, result, "Could not find MaxArmorPoints block in mech file");

    for (int32_t location = 0; location < 11; ++location)
    {
        result = file->ReadIdUChar(ArmorLocationNames[location], data->Armor[location].MaxArmor);
        Assert(result == 0, result, "Could not read armor in maxArmor block in mech file");
    }

    result = file->SeekBlock("CurArmorPoints");
    Assert(result == 0, result, "Could not find CurArmorPoints block in mech file");
    int32_t armorPoints = 0;

    for (int32_t location = 0; location < 11; ++location)
    {
        result = file->ReadIdUChar(ArmorLocationNames[location], data->Armor[location].CurArmor);
        Assert(result == 0, result, "Could not read armor in curArmorPoins block in mech file");
        armorPoints += data->Armor[location].CurArmor;
    }

    // Each armor point adds 40 resource points.
    data->Cost += armorPoints * 0x28;

    result = file->SeekBlock("InventoryInfo");
    Assert(result == 0, result, "Could not find InventoryInfo block in vehicle file");
    result = file->ReadIdUChar("NumOther", data->NumOther);
    Assert(result == 0, result, "Could not read NumOther in mech file");
    result = file->ReadIdUChar("NumWeapons", data->NumWeapons);
    Assert(result == 0, result, "Could not read NumWeapons in mech file");
    result = file->ReadIdUChar("NumAmmo", data->NumAmmo);
    Assert(result == 0, result, "Could not read NumAmmo in mech file");
    std::memset(data->CriticalSlots, 0xff, sizeof(data->CriticalSlots));
    data->Cost += ReadInventory(file, data->Inventory, data->NumOther, data->NumWeapons, data->NumAmmo);

    // The body locations: internal structure and the critical slots (component copy, damage).
    float internalPoints = 0.0f;

    for (int32_t location = 0; location < 8; ++location)
    {
        result = file->SeekBlock(BodyLocationNames[location]);
        Assert(result == 0, result, "Could not find BodyLocation block in mech file");
        result = file->ReadIdUChar("CurInternalStructure", data->CurInternalStructure[location]);
        Assert(result == 0, result, "Could not read CurInternalStructure in mech file");
        internalPoints = static_cast<float>(data->CurInternalStructure[location]) + internalPoints;

        for (int32_t slot = 0; slot < NumLocationCriticalSpaces[location]; ++slot)
        {
            std::snprintf(text, sizeof(text), "Component:%d", slot);
            uint8_t component[2] = {};
            result = file->ReadIdUCharArray(text, component, 2);
            Assert(result == 0, result, "Could not read component in mech file");
            data->CriticalSlots[location][slot].MasterId = component[0];
            data->CriticalSlots[location][slot].Damage = component[1];

            if (component[0] != 0xff)
            {
                if (component[1] != 0)
                {
                    data->Inventory->HitItem(component[0], component[1]);
                }

                // The slot index is passed as the location (faithful).
                data->Inventory->SetStatLoc(component[0], slot);
            }
        }
    }

    // Each internal structure point adds 50 resource points.
    data->Cost = static_cast<int32_t>(static_cast<double>(internalPoints) * 50.0f + data->Cost);
    data->CalcBR();
    purMech->Variants[variant] = data;
    file->Close();
    delete file;
    return 0;
}

auto MCPurMechList::AddMech(char* fileName0, int32_t count0, char* fileName2, int32_t count2, char* fileName1,
                            int32_t count1) -> int32_t
{
    auto* purMech = new MCPurMech;
    Assert(purMech != nullptr, 0, " Not enough memory to allocate PurMech");
    auto* block = new MCMechPurchaseBlock;
    purMech->Block = block;
    Assert(block != nullptr, 0, " Not enough memory for repair block ");
    block->CurVariant = -1;
    AddMech(purMech, fileName0, 0);
    purMech->Variants[0]->NumAvailable = count0;
    AddMech(purMech, fileName1, 1);
    purMech->Variants[1]->NumAvailable = count1;
    AddMech(purMech, fileName2, 2);
    purMech->Variants[2]->NumAvailable = count2;

    // Show the first variant in stock.
    if (count0 == 0 && count1 != 0)
    {
        purMech->Block->CurVariant = 1;
    }
    else if (count0 == 0 && count2 != 0)
    {
        purMech->Block->CurVariant = 2;
    }
    else
    {
        purMech->Block->CurVariant = 0;
    }

    purMech->Block->Init(purMech);
    purMech->Next = First;
    First = purMech;
    ++Count;
    return 0;
}

auto MCPurMechList::ModMech(char* fileName, int32_t delta0, int32_t delta2, int32_t delta1) -> int32_t
{
    for (int32_t index = 0; index < Count; ++index)
    {
        MCPurMech* purMech = nullptr;
        GetMechInfo(index, purMech);

        if (std::strcmp(purMech->Variants[0]->FileName, fileName) != 0)
        {
            continue;
        }

        int32_t& stock0 = purMech->Variants[0]->NumAvailable;
        stock0 += delta0;

        if (stock0 < 0)
        {
            stock0 = 0;
        }

        int32_t& stock1 = purMech->Variants[1]->NumAvailable;
        stock1 += delta1;

        if (stock1 < 0)
        {
            stock1 = 0;
        }

        int32_t& stock2 = purMech->Variants[2]->NumAvailable;
        stock2 += delta2;

        if (stock2 < 0)
        {
            stock2 = 0;
        }

        return 0;
    }

    return -1;
}

auto MCPurMechList::RemoveMech(uint8_t) -> int32_t
{
    return 0;
}

auto MCPurMechList::GetMechInfo(int32_t index, MCPurMech*& purMech) -> int32_t
{
    if (Count <= index)
    {
        return -1;
    }

    MCPurMech* node = First;

    for (; index > 0; --index)
    {
        node = node->Next;
    }

    purMech = node;
    return 0;
}

auto MCPurMechList::GetMechCount() -> int32_t
{
    return Count;
}

// MechPurchaseBlock

MCMechPurchaseBlock::~MCMechPurchaseBlock()
{
    MCMechPurchaseBlock::Destroy();
}

auto MCMechPurchaseBlock::Init(MCPurMech* newPurMech) -> void
{
    PicturePort = nullptr;
    DiagramPort = nullptr;
    PurMech = newPurMech;
    MCLogObject::Init(0, 0, 0x19a, 0x70, nullptr, GlobalLogPtr->PurchaseScreen->Lport());
    NameIndex = PurMech->Variants[CurVariant]->NameIndex;
}

auto MCMechPurchaseBlock::Destroy() -> void
{
    PurMech = nullptr;

    if (PicturePort != nullptr)
    {
        delete PicturePort;
        PicturePort = nullptr;
    }

    if (DiagramPort != nullptr)
    {
        delete DiagramPort;
        DiagramPort = nullptr;
    }

    LogFree(WeightClassText);
    WeightClassText = nullptr;
    LogFree(ArmorText);
    ArmorText = nullptr;
    LogFree(InternalText);
    InternalText = nullptr;
    MCLogObject::Destroy();
}

auto MCMechPurchaseBlock::HandleEvent(MCGuiEvent* event) -> void
{
    // The position within the row.
    int32_t localX = event->X - GlobalX();
    int32_t localY = event->Y - GlobalY();

    if (GlobalLogPtr->CurrentScreen == GlobalLogPtr->RepairScreen)
    {
        return;
    }

    char text[256];
    bool showHelp = false;

    if (Parent == nullptr)
    {
        showHelp = MechDrag.Dragging == 0;
    }
    else if (MechDrag.Dragging == 0)
    {
        if (MechDrag.Carrying == 0 && (event->Type == 8 || event->Type == 9))
        {
            Parent->HandleEvent(event);
            return;
        }

        showHelp = true;
    }

    if (showHelp && event->Key == 0)
    {
        // The ticker explains the variant buttons, the stock bar or the row.
        POINT point = {localX, localY};
        RECT buttonA = {0x9a, 5, 0xab, 0x15};
        RECT buttonW = {0xac, 5, 0xbd, 0x15};
        RECT buttonJ = {0xbe, 5, 0xd0, 0x15};
        RECT bar = {0xd9, 5, 0xe1, 0x6a};
        uint32_t id = 0x30;

        if (PtInRect(&buttonA, point))
        {
            id = 0x44;
        }
        else if (PtInRect(&buttonW, point))
        {
            id = 0x45;
        }
        else if (PtInRect(&buttonJ, point))
        {
            id = 0x46;
        }
        else if (PtInRect(&bar, point))
        {
            id = 0x37;
        }

        CLoadString(ThisInstance, id, text, 0xfe);
        GlobalLogPtr->Ticker->SetString(text);
    }

    int32_t type = event->Type;

    switch (type)
    {
        case 1:
        {
            if (MechDrag.Carrying != 0)
            {
                return;
            }

            [[fallthrough]];
        }
        case 3:
        {
            if (MechDrag.Dragging != 0 || CheckMaxUnits() != 0)
            {
                break;
            }

            if (localX > 0x9a && localX < 0xd0 && localY > 5 && localY < 0x16)
            {
                // A variant button.
                CurVariant = localX < 0xac ? 0 : localX < 0xbe ? 1 : 2;
                DrawBackground(Row);
                PlaySample(0xf);
                return;
            }

            if (OnRow(this, event) && PurMech->Variants[CurVariant]->NumAvailable != 0)
            {
                // Pick the mech up (left button drags, right button carries).
                if (type == 1)
                {
                    MechDrag.Dragging = 1;
                }
                else
                {
                    MechDrag.Carrying = 1;
                }

                PlaySample(0x35);
                Application->SetCursorVisible(0);
                Application->Grab(this);
                MechDrag.X = event->X - 0x10;
                MechDrag.Y = event->Y - 0x10;
                MakeDragIcon(MechDrag, [this](MCLogPort* surface) { OnBeginDrag(surface); });
                return;
            }

            PlaySample(0x33);
            return;
        }

        case 4:
        {
            if (MechDrag.Carrying != 0)
            {
                return;
            }

            [[fallthrough]];
        }
        case 6:
        {
            if (MechDrag.Dragging != 0 && type == 6)
            {
                return;
            }

            if (Application->GrabbedObject() == nullptr)
            {
                return;
            }

            Application->SetCursorVisible(1);
            Application->Release();
            MechDrag.Carrying = 0;
            MechDrag.Dragging = 0;
            DeleteDragIcon();

            if (type != 6 && !OverInventory(event))
            {
                // Dropped back on the store: nothing happens.
                PlaySample(OverStore(event) ? 0x34 : 0x33);
                return;
            }

            // Buy it.
            MCPurMechData* data = PurMech->Variants[CurVariant];

            if (ResourcePoints < data->Cost)
            {
                PlaySample(0x33);
                ShowMessage(0x4d);
                return;
            }

            PlaySample(0x34);
            GlobalMechPurchaseBlock = this;
            CLoadString(ThisInstance, WeightClassString(data->CurTonnage), text, 0xfe);
            char title[512];
            std::snprintf(title, sizeof(title), "%.0f Ton %s 'Mech", static_cast<double>(data->CurTonnage), text);
            OpenPurchaseDialog(0, data->Cost, MaxPurchase(data->NumAvailable), data->Name, title, DiagramPort,
                               MechPurchaseCallback);
            break;
        }

        case 7:
        {
            if (MechDrag.Dragging != 0)
            {
                MechDrag.X = event->X - 0xf;
                MechDrag.Y = event->Y - 0xf;
                GlobalLogPtr->DragIcon->MoveTo(MechDrag.X, MechDrag.Y, 0);
            }
            break;
        }
    }
}

auto MCMechPurchaseBlock::Draw() -> void
{
}

auto MCMechPurchaseBlock::DrawBackground(int32_t) -> void
{
    MCPurMechData* data = PurMech->Variants[CurVariant];

    if (PicturePort == nullptr)
    {
        // The mech's picture: its shadow (shapes 0xb..0x12 through the shadow table), then the mech (0..7).
        auto* picture = new MCLogPort;
        PicturePort = picture;
        picture->Init(0x4b, 100, 1);
        VfxPaneWipe(picture->Frame(), 0x10);
        VfxShapeLookaside(GlobalLogPtr->ShapeLookaside[5]);

        for (int32_t shape = 0; shape < 8; ++shape)
        {
            VfxShapeTranslateDraw(picture->Frame(), GlobalLogPtr->MechRepShapes[data->NameIndex], shape + 0xb, 0, 0);
        }

        VfxShapeLookaside(GlobalLogPtr->ShapeLookaside[0]);

        for (int32_t shape = 0; shape < 8; ++shape)
        {
            VfxShapeTranslateDraw(picture->Frame(), GlobalLogPtr->MechRepShapes[data->NameIndex], shape, 0, 0);
        }
    }

    if (DiagramPort == nullptr)
    {
        auto* diagram = new MCLogPort;
        DiagramPort = diagram;
        diagram->Init(0x1e, 0x1e, 1);
        VfxPaneWipe(diagram->Frame(), 0x10);

        for (int32_t location : BodyTrans)
        {
            AGShapeDraw(diagram->Frame(), GlobalLogPtr->MechIconShapes[data->NameIndex], location, 3, 0);
        }
    }

    // The class texts.
    char text[256];
    CLoadString(ThisInstance, WeightClassString(data->CurTonnage), text, 0xfe);
    SetHeapText(WeightClassText, text);
    data = PurMech->Variants[CurVariant];
    CLoadString(ThisInstance, ArmorClassString(data->ArmorTonnage), text, 0xfe);
    SetHeapText(ArmorText, text);
    int32_t internals = 0;

    for (uint8_t points : PurMech->Variants[CurVariant]->CurInternalStructure)
    {
        internals += points;
    }

    uint32_t internalId = internals < 0x24   ? 100u
                          : internals < 0x38 ? 0x4fu
                          : internals < 0x51 ? 0x65u
                          : internals < 0x79 ? 0x51u
                                             : 0x66u;
    CLoadString(ThisInstance, internalId, text, 0xfe);
    SetHeapText(InternalText, text);
    SetBar();
    MCPurMechData* current = PurMech->Variants[CurVariant];

    if (current->Description == nullptr && current->DescIndex > -1)
    {
        current->LoadDescription(current->DescIndex);
    }

    PrepareInfoDescription(current->Description);
}

auto MCMechPurchaseBlock::DrawRow(MCLogPort* port, int32_t top) -> void
{
    MCPurchaseScreen* screen = GlobalLogPtr->PurchaseScreen;
    MCLogPort* row0 = NewRowPicture(screen->MechTabPort, port, top, false);
    MCPurMechData* data = PurMech->Variants[CurVariant];
    CopyArt(row0, 5, 4, "%slogart\\lspflma%02d.tga", data->NameIndex);
    CopyArt(row0, 0x13a, 6, "%slogart\\lscdsm%02d.tga", data->NameIndex);

    if (PicturePort != nullptr)
    {
        PicturePort->CopyTo(row0->Frame(), 0xed, 6, 1);
    }

    char text[256];
    char tons[32];
    CLoadString(ThisInstance, 0x6e, tons, 0x1e);
    std::snprintf(text, sizeof(text), "%.0f %s", static_cast<double>(data->CurTonnage), tons);
    WriteText(YellowDropFont, row0, 0x51, 0x23, text);
    WriteText(YellowDropFont, row0, 0x51, 0x2c, WeightClassText);
    WriteText(YellowDropFont, row0, 0xa7, 0x23, ArmorText);
    WriteText(YellowDropFont, row0, 0xa7, 0x2c, InternalText);
    std::snprintf(text, sizeof(text), "%d m/s", data->MaxRunSpeed);
    WriteText(YellowDropFont, row0, 0x51, 0x35, text);

    // The weapons and equipment, and the jump jets' rating.
    int32_t jumpJets = 0;

    for (MCLogInventoryItem* item = data->Inventory->Items; item != nullptr; item = item->Next)
    {
        if (MasterComponentList[item->MasterID].Form == MCComponentForm::JumpJet)
        {
            jumpJets = item->Count;
        }
    }

    DrawInventoryList(data->Inventory, row0, text, sizeof(text));

    // An unlisted rating (jump jets beyond 8) shows whatever the text buffer last held.
    if (jumpJets == 0)
    {
        CLoadString(ThisInstance, 0x6c, text, 0xfe);
    }
    else
    {
        switch (jumpJets * 2 / 3)
        {
            case 0:
            case 1:
                CLoadString(ThisInstance, 0x56, text, 0xfe);
                break;
            case 2:
            case 3:
                CLoadString(ThisInstance, 0x55, text, 0xfe);
                break;
            case 4:
                CLoadString(ThisInstance, 0x50, text, 0xfe);
                break;
            case 5:
                CLoadString(ThisInstance, 0x6d, text, 0xfe);
                break;
            default:
                break;
        }
    }

    WriteText(YellowDropFont, row0, 0xa7, 0x35, text);

    // The battle rating bar: 80 pixels at 18010, bottom at y 0x58.
    MCPane* frame = row0->Frame();
    int32_t bar = static_cast<int32_t>(static_cast<double>(data->BattleRating) * 0x1.d1c6674f499a1p-15 * 80.0);
    int32_t barTop = 0x58 - bar;
    int32_t topLine = 0x57 - bar;
    VfxLineDraw(frame, 0xdb, 0x58, 0xdf, 0x58, 0xe5);
    VfxLineDraw(frame, 0xda, topLine, 0xe0, topLine, 0xe3);
    VfxLineDraw(frame, 0xda, 0x57, 0xda, barTop, 0xe3);
    VfxLineDraw(frame, 0xe0, 0x57, 0xe0, barTop, 0xe5);

    for (int32_t x = 0xdb; x <= 0xdf; ++x)
    {
        VfxLineDraw(frame, x, 0x57, x, barTop, 0xe4);
    }

    VfxLineDraw(frame, 0xdb, 0x56 - bar, 0xdf, 0x56 - bar, 0x10);
    VfxPixelWrite(frame, 0xdf, 0x57, 0xe5);
    VfxPixelWrite(frame, 0xdb, barTop, 0xe3);
    VfxPixelWrite(frame, 0xda, topLine, 0x10);
    VfxPixelWrite(frame, 0xe0, topLine, 0x10);
    delete row0;

    // The variant buttons (A, W, J), and the variant's name art over the row.
    int32_t variant = CurVariant;
    MCPurMech* mech = PurMech;
    int32_t y = top;

    if (variant == 0)
    {
        CopyArt(port, 5, y + 4, "%slogart\\lspflma%02d.tga", mech->Variants[0]->NameIndex);
        CopyArt(port, 0x9a, y + 5, "%slogart\\lspbim04.tga");
    }
    else
    {
        CopyArt(port, 0x9a, y + 5,
                mech->Variants[0]->NumAvailable == 0 ? "%slogart\\lspbim07.tga" : "%slogart\\lspbim01.tga");
    }

    if (variant == 2)
    {
        CopyArt(port, 5, y + 4, "%slogart\\lspflmj%02d.tga", mech->Variants[2]->NameIndex);
        CopyArt(port, 0xbe, y + 5, "%slogart\\lspbim06.tga");
    }
    else
    {
        CopyArt(port, 0xbe, y + 5,
                mech->Variants[2]->NumAvailable == 0 ? "%slogart\\lspbim09.tga" : "%slogart\\lspbim03.tga");
    }

    if (variant == 1)
    {
        CopyArt(port, 5, y + 4, "%slogart\\lspflmw%02d.tga", mech->Variants[1]->NameIndex);
        CopyArt(port, 0xac, y + 5, "%slogart\\lspbim05.tga");
    }
    else
    {
        CopyArt(port, 0xac, y + 5,
                mech->Variants[1]->NumAvailable == 0 ? "%slogart\\lspbim08.tga" : "%slogart\\lspbim02.tga");
    }

    MCPurMechData* shown = mech->Variants[variant];

    if (shown->NumAvailable != 0)
    {
        if (DiagramPort != nullptr)
        {
            VfxPaneCopy(DiagramPort->Frame(), 0, 0, port->Frame(), 7, y + 0x22, -1);
        }
    }
    else
    {
        // Sold out: the "sold out" name art, a blank diagram and picture, and the sold-out mark.
        static const char* const soldOut[3] = {"%slogart\\lspfdma%02d.tga", "%slogart\\lspfdmw%02d.tga",
                                               "%slogart\\lspfdmj%02d.tga"};
        CopyArt(port, 5, y + 4, soldOut[variant], shown->NameIndex);
        ::FillBox(port, 7, y + 0x22, 0x1e, 0x1e, 0x10);
        ::FillBox(port, 0xed, y + 5, 0x4b, 100, 0x10);
        AGShapeDraw(port->Frame(), GlobalLogPtr->MechRepShapes[shown->NameIndex], 0x13, 0xed, y + 6);
    }

    // Stock and price.
    if (shown->NumAvailable < 0)
    {
        char format[256];
        CLoadString(ThisInstance, 0x385, format, 0xfe);
        std::snprintf(text, sizeof(text), format, shown->NumAvailable);
    }
    else
    {
        std::snprintf(text, sizeof(text), "%d", shown->NumAvailable);
    }

    WriteText(YellowDropFont, port, 0x29, y + 0x12, text);
    std::snprintf(text, sizeof(text), "%d", shown->Cost);
    WriteText(YellowDropFont, port, 0x52, y + 0x12, text);
    DrawInfoDescription(port, 0xc6, 0x25, shown->Description, 6, y + 0x44);
}

auto MCMechPurchaseBlock::OnBeginDrag(MCLogPort* surface) -> void
{
    // The square at (6, 0x21) of the row, over the store's colour 0x10.
    VfxPaneWipe(surface->Frame(), 0x10);
    MCDragIcon::DrawFrom(surface, 6, 0x21, [this](MCLogPort* port) { DrawRow(port, 0); });
}

auto MCMechPurchaseBlock::SetBar() -> void
{
}

// PurVehicleList

MCPurVehicleList::MCPurVehicleList()
{
    Init();
}

auto MCPurVehicleList::Init() -> void
{
    First = nullptr;
    Count = 0;
}

auto MCPurVehicleList::Destroy() -> void
{
    for (MCPurVehicle* vehicle = First; vehicle != nullptr; vehicle = First)
    {
        MCPurVehicleData* data = vehicle->Data;
        First = vehicle->Next;

        if (data != nullptr)
        {
            if (data->Name != nullptr)
            {
                LogFree(data->Name);
                data->Name = nullptr;
            }

            if (data->Inventory != nullptr)
            {
                data->Inventory->Destroy();
                delete data->Inventory;
                data->Inventory = nullptr;
            }

            if (data->Description != nullptr)
            {
                LogFree(data->Description);
                data->Description = nullptr;
            }
        }

        if (vehicle->Block != nullptr)
        {
            delete vehicle->Block;
            vehicle->Block = nullptr;
        }

        delete vehicle->Data;
        LogFree(vehicle);
    }

    First = nullptr;
    Count = 0;
}

auto MCPurVehicleList::ModVehicle(char* fileName, int32_t delta) -> int32_t
{
    for (int32_t index = 0; index < Count; ++index)
    {
        MCPurVehicle* vehicle = nullptr;
        GetVehicleInfo(index, vehicle);

        if (std::strcmp(vehicle->Data->FileName, fileName) != 0)
        {
            continue;
        }

        int32_t& stock = vehicle->Data->NumAvailable;
        stock += delta;

        if (stock < 0)
        {
            stock = 0;
        }

        return 0;
    }

    return -1;
}

auto MCPurVehicleList::AddVehicle(char* fileName, int32_t numAvailable) -> int32_t
{
    std::string path;
    auto* file = new MCFitIniFile;
    Assert(file != nullptr, 0, " no RAM for scenario file ");
    OpenProfile(file, path, ProfilePath, fileName, false, " could not open vehicle file in scenario ");

    void* memory = LogAlloc(sizeof(MCPurVehicle));
    Assert(memory != nullptr, 0, "Not enough memory for LogVehicle");
    auto* vehicle = new (memory) MCPurVehicle;
    auto* data = new MCPurVehicleData;
    vehicle->Data = data;

    if (file->SeekBlock("General") != 0)
    {
        // Port fix: a file without a General block is a raw record the original read over the 0xc-byte
        // PurVehicle (0xd0 bytes per record) and then never added; the port just drops it.
        delete data;
        LogFree(vehicle);
        delete file;
        return 0;
    }

    int32_t result = file->SeekBlock("Header");

    if (result != 0)
    {
        Assert(0, result, "Could not find Header block in vehicle file");
    }

    char text[256];
    result = file->ReadIdString("FileType", text, 0x7f);
    Assert(result == 0, result, "Could not read FileType in vehicle file");
    Assert(std::strcmp(text, "GroundVehicleProfile") == 0, 0, "File is not a vehicle file");
    result = file->SeekBlock("General");
    Assert(result == 0, result, "Could not find General block in vehicle file");
    result = file->ReadIdLong("NameIndex", data->NameIndex);
    Assert(result == 0, result, "Could not read NameIndex in vehicle file");
    result = file->ReadIdFloat("CurTonnage", data->CurTonnage);
    Assert(result == 0, result, "Could not read CurTonnage in vehicle file");

    if (file->ReadIdLong("ResourcePoints", data->BaseCost) != 0)
    {
        data->BaseCost = 100;
    }

    data->DescIndex = -1;
    data->Description = nullptr;
    file->ReadIdLong("DescIndex", data->DescIndex);
    data->LoadDescription(data->DescIndex);
    CLoadString(ThisInstance, static_cast<uint32_t>(data->DescIndex + 700), text, 0x7f);
    data->Name = HeapString(text);

    result = file->SeekBlock("Engine");
    Assert(result == 0, result, "Could not find engine block in vehicle file");
    result = file->ReadIdUChar("MaxMoveSpeed", data->MaxMoveSpeed);
    Assert(result == 0, result, "Could not read MaxMoveSpeed in vehicle file");
    result = file->SeekBlock("Armor");
    Assert(result == 0, result, "Could not find armor block in vehicle file");
    result = file->ReadIdFloat("Tonnage", data->ArmorTonnage);
    Assert(result == 0, result, "Could not read Tonnage in vehicle file");
    result = file->SeekBlock("InventoryInfo");
    Assert(result == 0, result, "Could not find InventoryInfo block in vehicle file");
    result = file->ReadIdUChar("NumOther", data->NumOther);
    Assert(result == 0, result, "Could not read NumOther in vehicle file");
    result = file->ReadIdUChar("NumWeapons", data->NumWeapons);
    Assert(result == 0, result, "Could not read NumWeapons in vehicle file");
    result = file->ReadIdUChar("NumAmmo", data->NumAmmo);
    Assert(result == 0, result, "Could not read NumAmmo in vehicle file");
    data->Inventory = new MCInventoryList;
    Assert(data->Inventory != nullptr, result, " invalid vehicle file: no inventory ");
    ReadInventory(file, data->Inventory, data->NumOther, data->NumWeapons, data->NumAmmo);
    data->NumAvailable = numAvailable;
    std::strncpy(data->FileName, fileName, 9);

    auto* block = new MCVehiclePurchaseBlock;
    vehicle->Block = block;
    block->Init(vehicle);
    vehicle->CalcVehicleCost();

    // Insert in tonnage order (before the first that is as heavy or heavier).
    MCPurVehicle* previous = nullptr;
    MCPurVehicle* node = First;

    while (node != nullptr && node->Data->CurTonnage < vehicle->Data->CurTonnage)
    {
        previous = node;
        node = node->Next;
    }

    if (previous == nullptr)
    {
        First = vehicle;
    }
    else
    {
        previous->Next = vehicle;
    }

    vehicle->Next = node;
    ++Count;
    file->Close();
    delete file;
    return 0;
}

auto MCPurVehicleList::RemoveVehicle(uint8_t) -> int32_t
{
    return 0;
}

auto MCPurVehicleList::GetVehicleInfo(int32_t index, MCPurVehicle*& vehicle) -> int32_t
{
    if (Count <= index)
    {
        return -1;
    }

    MCPurVehicle* node = First;

    for (; index > 0; --index)
    {
        node = node->Next;
    }

    vehicle = node;
    return 0;
}

auto MCPurVehicleList::GetVehicleCount() -> int32_t
{
    return Count;
}

// VehiclePurchaseBlock

MCVehiclePurchaseBlock::~MCVehiclePurchaseBlock()
{
    MCVehiclePurchaseBlock::Destroy();
}

auto MCVehiclePurchaseBlock::Init(MCPurVehicle* newPurVehicle) -> void
{
    PicturePort = nullptr;
    PurVehicle = newPurVehicle;
    MCLogObject::Init(0, 0, 0x19a, 0x70, nullptr, GlobalLogPtr->PurchaseScreen->Lport());
    MCPurVehicleData* data = PurVehicle->Data;
    NameIndex = data->NameIndex;
    char text[256];
    CLoadString(ThisInstance, WeightClassString(data->CurTonnage), text, 0xf);
    WeightClassText = HeapString(text);
    CLoadString(ThisInstance, ArmorClassString(PurVehicle->Data->ArmorTonnage), text, 0xf);
    ArmorText = HeapString(text);
}

auto MCVehiclePurchaseBlock::Destroy() -> void
{
    // The work port is only ever alive inside drawBackground.
    _OwnPort = nullptr;
    PurVehicle = nullptr;
    LogFree(WeightClassText);
    WeightClassText = nullptr;
    LogFree(ArmorText);
    ArmorText = nullptr;

    if (PicturePort != nullptr)
    {
        delete PicturePort;
        PicturePort = nullptr;
    }

    MCLogObject::Destroy();
}

auto MCVehiclePurchaseBlock::HandleEvent(MCGuiEvent* event) -> void
{
    int32_t localX = event->X - GlobalX();
    int32_t localY = event->Y - GlobalY();

    if (GlobalLogPtr->CurrentScreen == GlobalLogPtr->RepairScreen)
    {
        return;
    }

    if (Parent != nullptr && VehicleDrag.Dragging == 0 && VehicleDrag.Carrying == 0 &&
        (event->Type == 8 || event->Type == 9))
    {
        Parent->HandleEvent(event);
        return;
    }

    int32_t type = event->Type;
    char text[256];

    switch (type)
    {
        case 1:
        {
            if (VehicleDrag.Carrying != 0)
            {
                return;
            }

            [[fallthrough]];
        }
        case 3:
        {
            if ((VehicleDrag.Dragging != 0 && type == 3) || CheckMaxUnits() != 0)
            {
                return;
            }

            if ((localX < 0x94 || localX > 0xc9 || localY < 5 || localY > 0x15) && OnRow(this, event) &&
                PurVehicle->Data->NumAvailable != 0)
            {
                // Pick the vehicle up (left button drags, right button carries).
                if (type == 1)
                {
                    VehicleDrag.Dragging = 1;
                }
                else
                {
                    VehicleDrag.Carrying = 1;
                }

                PlaySample(0x35);
                Application->SetCursorVisible(0);
                Application->Grab(this);
                VehicleDrag.X = event->X - 0x10;
                VehicleDrag.Y = event->Y - 0x10;
                MakeDragIcon(VehicleDrag, [this](MCLogPort* surface) { OnBeginDrag(surface); });
                return;
            }
            break;
        }

        case 4:
        {
            if (VehicleDrag.Carrying != 0)
            {
                return;
            }

            [[fallthrough]];
        }
        case 6:
        {
            if (VehicleDrag.Dragging != 0 && type == 6)
            {
                return;
            }

            VehicleDrag.Carrying = 0;

            if (Application->GrabbedObject() == nullptr)
            {
                return;
            }

            Application->Release();
            Application->SetCursorVisible(1);
            VehicleDrag.Dragging = 0;
            DeleteDragIcon();

            if (type != 6 && !OverInventory(event))
            {
                if (OverStore(event))
                {
                    PlaySample(0x34);
                    return;
                }
                break;
            }

            // Buy it.
            MCPurVehicleData* data = PurVehicle->Data;
            int32_t cost = data->Cost;

            if (cost > ResourcePoints)
            {
                PlaySample(0x33);
                ShowMessage(0x4d);
                return;
            }

            PlaySample(0x34);
            int32_t maxQuantity = MaxPurchase(data->NumAvailable);
            GlobalVehicleBlockPtr = this;
            OpenPurchaseDialog(6, cost, maxQuantity, data->Name, nullptr, PicturePort, VehiclePurchaseCallback);
            return;
        }

        case 7:
        {
            if (VehicleDrag.Dragging != 0)
            {
                VehicleDrag.Y = event->Y - 0xf;
                VehicleDrag.X = event->X - 0xf;
                GlobalLogPtr->DragIcon->MoveTo(VehicleDrag.X, VehicleDrag.Y, 0);
                return;
            }

            if (event->Key == 0)
            {
                CLoadString(ThisInstance, 0x30, text, 0xfe);
                GlobalLogPtr->Ticker->SetString(text);
            }

            return;
        }

        default:
            return;
    }

    PlaySample(0x33);
}

auto MCVehiclePurchaseBlock::DrawBackground(int32_t) -> void
{
    MCPurVehicleData* data = PurVehicle->Data;

    if (data->NumAvailable == 0)
    {
        PrepareInfoDescription(data->Description);
        return;
    }

    // The diagram, kept for the purchase dialog (the original parked the row's picture in picturePort first).
    if (PicturePort != nullptr)
    {
        delete PicturePort;
    }

    auto* diagram = new MCLogPort;
    PicturePort = diagram;
    diagram->Init(0x1e, 0x1e, 1);
    VfxPaneWipe(diagram->Frame(), 0x10);

    for (int32_t location = 0; location < 5; ++location)
    {
        AGShapeDraw(diagram->Frame(), GlobalLogPtr->VehicleIconShapes[data->NameIndex], location, 4, 0);
    }

    PrepareInfoDescription(data->Description);
}

auto MCVehiclePurchaseBlock::DrawRow(MCLogPort* port, int32_t top) -> void
{
    MCPurchaseScreen* screen = GlobalLogPtr->PurchaseScreen;
    MCLogPort* work = NewRowPicture(screen->VehicleTabPort, port, top, false);
    char tons[256];
    CLoadString(ThisInstance, 0x6e, tons, 0xfe);
    MCPurVehicleData* data = PurVehicle->Data;
    char text[256];
    std::snprintf(text, sizeof(text), "%.0f %s", static_cast<double>(data->CurTonnage), tons);
    WriteText(YellowDropFont, work, 0x52, 0x24, text);
    WriteText(YellowDropFont, work, 0x52, 0x2d, WeightClassText);
    WriteText(YellowDropFont, work, 0xa7, 0x24, ArmorText);
    std::snprintf(text, sizeof(text), "%d m/s", data->MaxMoveSpeed);
    WriteText(YellowDropFont, work, 0x52, 0x36, text);
    DrawInventoryList(data->Inventory, work, text, sizeof(text));

    const char* stock = text;
    char soldOutText[256];

    if (data->NumAvailable == 0)
    {
        // Sold out: the "sold out" name art, a blank picture and the sold-out mark.
        CopyArt(work, 5, 4, "%slogart\\lspfdv%02d.tga", data->NameIndex);
        ::FillBox(work, 0xed, 6, 0x4b, 100, 0x10);
        AGShapeDraw(work->Frame(), GlobalLogPtr->VehicleRepShapes[data->NameIndex], 6, 0xed, 6);
        stock = "0";
    }
    else
    {
        int32_t index = data->NameIndex;
        CopyArt(work, 5, 4, "%slogart\\lspflv%02d.tga", index);
        // The picture.
        MCLogBlockPort picture(work->Frame(), 0xed, 6, 0x4b, 100, true);
        VfxPaneWipe(picture.Frame(), 0x10);
        VfxShapeLookaside(GlobalLogPtr->ShapeLookaside[0]);

        for (int32_t shape = 0; shape < 5; ++shape)
        {
            VfxShapeTranslateDraw(picture.Frame(), GlobalLogPtr->VehicleRepShapes[index], shape, 0, 0);
        }

        for (int32_t location = 0; location < 5; ++location)
        {
            AGShapeDraw(work->Frame(), GlobalLogPtr->VehicleIconShapes[index], location, 9, 0x22);
        }

        if (data->NumAvailable < 1)
        {
            CLoadString(ThisInstance, 0x385, soldOutText, 0xfe);
            stock = soldOutText;
        }
        else
        {
            std::snprintf(text, sizeof(text), "%d", data->NumAvailable);
        }
    }

    WriteText(YellowDropFont, work, 0x25, 0x12, stock);
    std::snprintf(text, sizeof(text), "%d", data->Cost);
    WriteText(YellowDropFont, work, 0x52, 0x12, text);
    DrawInfoDescription(work, 0xc6, 0x26, data->Description, 6, 0x43);
    delete work;
}

auto MCVehiclePurchaseBlock::OnBeginDrag(MCLogPort* surface) -> void
{
    // The square at (6, 0x21) of the row, over the store's colour 0x10.
    VfxPaneWipe(surface->Frame(), 0x10);
    MCDragIcon::DrawFrom(surface, 6, 0x21, [this](MCLogPort* port) { DrawRow(port, 0); });
}

auto MCVehiclePurchaseBlock::SetBar() -> void
{
}

// CompPurchaseBlock

MCCompPurchaseBlock::~MCCompPurchaseBlock()
{
    MCCompPurchaseBlock::Destroy();
}

auto MCCompPurchaseBlock::Init(MCLogInventoryItem* newItem) -> void
{
    Item = newItem;
    MCLogObject::Init(0, 0, 0x19a, 0x70, nullptr, GlobalLogPtr->PurchaseScreen->Lport());
    const MCMasterComponent& component = MasterComponentList[Item->MasterID];
    MCComponentForm form = component.Form;
    char format[256];
    CLoadString(ThisInstance, 0x27f, format, 0xfe);
    std::snprintf(WeightText, sizeof(WeightText), format, static_cast<double>(component.Tonnage));
    char text[256];

    if (form != MCComponentForm::WeaponEnergy && form != MCComponentForm::WeaponBallistic &&
        form != MCComponentForm::WeaponMissile)
    {
        CLoadString(ThisInstance, 0x6c, text, 0xfe);

        if (form == MCComponentForm::Probe)
        {
            std::snprintf(RangeText, sizeof(RangeText), "%s", text);
        }
        else
        {
            // Original behaviour (OB-078): other equipment than ECM and sensors formats the item pointer's bits as
            // the range, which read "0.0 m".
            float range = 0.0f;

            if (form == MCComponentForm::Ecm || form == MCComponentForm::Sensor)
            {
                range = MasterComponentList[Item->MasterID].RangeOrHeat;
            }

            std::snprintf(RangeText, sizeof(RangeText), "%.1f m", static_cast<double>(range));
        }

        std::snprintf(DamageText, sizeof(DamageText), "%s", text);
        std::snprintf(RecycleText, sizeof(RecycleText), "%s", text);
        return;
    }

    // Weapons: range, damage and recycle time with their rating words.
    float range = component.WeaponRange[3];
    CLoadString(ThisInstance, range < 76.0f ? 0x55 : range < 151.0f ? 0x50 : 0x6d, text, 0xfe);
    std::snprintf(RangeText, sizeof(RangeText), "%s", text);
    float damage = component.Damage;

    if (component.WeaponFlags == 4)
    {
        damage = static_cast<float>(damage * 3.0);
    }

    uint32_t damageId = damage < 1.0f   ? 100u
                        : damage < 3.0f ? 0x4fu
                        : damage < 5.0f ? 0x65u
                        : damage < 7.0f ? 0x51u
                        : damage < 9.0f ? 0x66u
                                        : 0x67u;
    CLoadString(ThisInstance, damageId, text, 0xfe);
    std::snprintf(DamageText, sizeof(DamageText), "%.2f (%s)", static_cast<double>(damage), text);
    float recycle = component.RecycleTime;
    uint32_t recycleId = recycle < 2.0f   ? 0x68u
                         : recycle < 3.0f ? 0x69u
                         : recycle < 5.0f ? 0x65u
                         : recycle < 8.0f ? 0x6au
                                          : 0x6bu;
    CLoadString(ThisInstance, recycleId, text, 0xfe);
    std::snprintf(RecycleText, sizeof(RecycleText), "%.2f s (%s)", static_cast<double>(recycle), text);
}

auto MCCompPurchaseBlock::Destroy() -> void
{
    MCLogObject::Destroy();
}

auto MCCompPurchaseBlock::HandleEvent(MCGuiEvent* event) -> void
{
    if (GlobalLogPtr->CurrentScreen == GlobalLogPtr->RepairScreen)
    {
        return;
    }

    if (Parent != nullptr && CompDrag.Dragging == 0 && CompDrag.Carrying == 0 && (event->Type == 8 || event->Type == 9))
    {
        Parent->HandleEvent(event);
        return;
    }

    int32_t type = event->Type;
    char text[256];

    switch (type)
    {
        case 1:
        case 3:
        {
            if (CompDrag.Dragging != 0 || CompDrag.Carrying != 0)
            {
                return;
            }

            if (OnRow(this, event) && Item->Count != 0)
            {
                // Pick the component up (left button drags, right button carries). The cursor stays shown.
                if (type == 1)
                {
                    CompDrag.Dragging = 1;
                }
                else
                {
                    CompDrag.Carrying = 1;
                }

                PlaySample(0x35);
                Application->SetCursorVisible(1);
                Application->Grab(this);
                CompDrag.Y = event->Y - 0x10;
                CompDrag.X = event->X - 0x10;
                MakeDragIcon(CompDrag, [this](MCLogPort* surface) { OnBeginDrag(surface); });
                return;
            }
            break;
        }

        case 4:
        {
            if (CompDrag.Carrying != 0)
            {
                return;
            }

            [[fallthrough]];
        }
        case 6:
        {
            if (CompDrag.Dragging != 0 && type == 6)
            {
                return;
            }

            if (Application->GrabbedObject() == nullptr)
            {
                return;
            }

            Application->SetCursorVisible(1);
            Application->Release();
            CompDrag.Dragging = 0;
            CompDrag.Carrying = 0;
            DeleteDragIcon();

            if (type != 6 && !OverInventory(event))
            {
                if (OverStore(event))
                {
                    PlaySample(0x34);
                    return;
                }
                break;
            }

            // Buy some.
            MCLogInventoryItem* bought = Item;
            MCMasterComponent& component = MasterComponentList[bought->MasterID];

            if (component.ResourcePoints <= ResourcePoints)
            {
                PlaySample(0x34);
                GlobalItemPtr = bought;
                auto* picture = new MCLogPort;
                LoadArt(picture, "%slogart\\lscicc%02d.tga", bought->RangeIndex);
                GlobalLogPtr->PurchaseDialog->Init(4, component.ResourcePoints, bought->Count, component.Name.c_str(),
                                                   nullptr, picture);
                delete picture;
                GlobalLogPtr->PurchaseDialog->SetPort(GlobalLogPtr->CurrentScreen->Lport());
                GlobalLogPtr->PurchaseDialog->SetCallback(CompPurchaseCallback);
                GlobalLogPtr->PurchaseDialog->Activate();
                return;
            }

            ShowMessage(0x4d);
            break;
        }

        case 7:
        {
            if (CompDrag.Dragging != 0)
            {
                CompDrag.Y = event->Y - 0xf;
                CompDrag.X = event->X - 0xf;
                GlobalLogPtr->DragIcon->MoveTo(CompDrag.X, CompDrag.Y, 0);
                return;
            }

            if (event->Key == 0)
            {
                CLoadString(ThisInstance, 0x30, text, 0xfe);
                GlobalLogPtr->Ticker->SetString(text);
            }

            return;
        }

        default:
            return;
    }

    PlaySample(0x33);
}

auto MCCompPurchaseBlock::DrawBackground(int32_t, int32_t) -> void
{
    PrepareInfoDescription(Item->Description);
}

auto MCCompPurchaseBlock::DrawRow(MCLogPort* port, int32_t top) -> void
{
    MCPurchaseScreen* screen = GlobalLogPtr->PurchaseScreen;
    MCLogPort* work = NewRowPicture(screen->CompTabPort, port, top, true);
    MCLogInventoryItem* shown = Item;
    char text[1024];

    if (shown->Count < 0)
    {
        char format[256];
        CLoadString(ThisInstance, 0x385, format, 0xfe);
        std::snprintf(text, sizeof(text), format, shown->Count);
    }
    else
    {
        std::snprintf(text, sizeof(text), "%d", shown->Count);
    }

    WriteText(YellowDropFont, work, 0x26, 0x12, text);
    std::snprintf(text, sizeof(text), "%d", MasterComponentList[shown->MasterID].ResourcePoints);
    WriteText(YellowDropFont, work, 0x52, 0x12, text);
    int32_t picture = Item->RangeIndex;

    if (Item->Count == 0)
    {
        // Sold out: a blank icon, the "sold out" picture and name art.
        ::FillBox(work, 7, 0x22, 0x1e, 0x1e, 0x10);
        CopyArt(work, 0xed, 6, "%slogart\\lspidc%02d.tga", picture);
        CopyArt(work, 5, 4, "%slogart\\lspfdc%02d.tga", picture);
    }
    else
    {
        CopyArt(work, 7, 0x22, "%slogart\\lscicc%02d.tga", picture);
        CopyArt(work, 0xed, 6, "%slogart\\lspilc%02d.tga", picture);
        CopyArt(work, 5, 4, "%slogart\\lspflc%02d.tga", picture);
    }

    WriteText(YellowDropFont, work, 0x52, 0x35, RangeText);
    WriteText(YellowDropFont, work, 0x52, 0x2c, DamageText);
    WriteText(YellowDropFont, work, 0x52, 0x23, RecycleText);
    DrawInfoDescription(work, 0xc6, 0x25, Item->Description, 6, 0x44);
    delete work;
}

auto MCCompPurchaseBlock::OnBeginDrag(MCLogPort* surface) -> void
{
    // The square at (6, 0x21) of the row, over the store's colour 0x10.
    VfxPaneWipe(surface->Frame(), 0x10);
    MCDragIcon::DrawFrom(surface, 6, 0x21, [this](MCLogPort* port) { DrawRow(port, 0); });
}

// PilotPurchaseBlock

MCPilotPurchaseBlock::~MCPilotPurchaseBlock()
{
    MCPilotPurchaseBlock::Destroy();
}

auto MCPilotPurchaseBlock::Init(MCPurPilotData* newPilot) -> void
{
    Pilot = newPilot;
    MCLogObject::Init(0, 0, 0x19a, 0x70, nullptr, GlobalLogPtr->PurchaseScreen->Lport());
}

auto MCPilotPurchaseBlock::Destroy() -> void
{
    Pilot = nullptr;
    MCLogObject::Destroy();
}

auto MCPilotPurchaseBlock::HandleEvent(MCGuiEvent* event) -> void
{
    if (GlobalLogPtr->CurrentScreen == GlobalLogPtr->RepairScreen)
    {
        return;
    }

    if (Parent != nullptr && PilotDrag.Dragging == 0 && PilotDrag.Carrying == 0 &&
        (event->Type == 8 || event->Type == 9))
    {
        Parent->HandleEvent(event);
        return;
    }

    int32_t type = event->Type;
    char text[256];

    switch (type)
    {
        case 1:
        {
            if (PilotDrag.Carrying != 0)
            {
                return;
            }

            [[fallthrough]];
        }
        case 3:
        {
            if (PilotDrag.Dragging == 0 && OnRow(this, event))
            {
                // Pick the pilot up (left button drags, right button carries). The cursor stays shown.
                if (type == 1)
                {
                    PilotDrag.Dragging = 1;
                }
                else
                {
                    PilotDrag.Carrying = 1;
                }

                SoundSystem->PlayPilotSpeech(Pilot->PilotAudio, 10);
                Application->Grab(this);
                PilotDrag.Y = event->Y - 0x10;
                PilotDrag.X = event->X - 0x10;
                MakeDragIcon(PilotDrag, [this](MCLogPort* surface) { OnBeginDrag(surface); });
                return;
            }
            break;
        }

        case 4:
        {
            if (PilotDrag.Carrying != 0)
            {
                return;
            }

            [[fallthrough]];
        }
        case 6:
        {
            if (PilotDrag.Dragging != 0 && type == 6)
            {
                break;
            }

            PilotDrag.Carrying = 0;

            if (Application->GrabbedObject() == nullptr)
            {
                break;
            }

            Application->Release();
            PilotDrag.Dragging = 0;
            DeleteDragIcon();

            if (type != 6 && !OverInventory(event))
            {
                PlaySample(OverStore(event) ? 0x34 : 0x33);
                return;
            }

            // Hire the pilot.
            MCPurPilotData* hired = Pilot;

            if (ResourcePoints < hired->Cost)
            {
                PlaySample(0x33);
                ShowMessage(0x4d);
                return;
            }

            GlobalPilotPurchaseBlock = this;
            auto* picture = new MCLogPort;
            LoadArt(picture, "%slogart\\pilot%02d.tga", hired->NameIndex);
            GlobalLogPtr->PurchaseDialog->Init(2, hired->Cost, 1, hired->Callsign, nullptr, picture);
            delete picture;
            GlobalLogPtr->PurchaseDialog->SetPort(GlobalLogPtr->CurrentScreen->Lport());
            GlobalLogPtr->PurchaseDialog->SetCallback(PilotPurchaseCallback);
            GlobalLogPtr->PurchaseDialog->Activate();
            break;
        }

        case 7:
        {
            if (PilotDrag.Dragging != 0)
            {
                PilotDrag.Y = event->Y - 0xf;
                PilotDrag.X = event->X - 0xf;
                GlobalLogPtr->DragIcon->MoveTo(PilotDrag.X, PilotDrag.Y, 0);
                return;
            }

            if (event->Key == 0)
            {
                CLoadString(ThisInstance, 0x30, text, 0xfe);
                GlobalLogPtr->Ticker->SetString(text);
            }

            return;
        }
    }
}

auto MCPilotPurchaseBlock::DrawBackground(int32_t) -> void
{
    PrepareInfoDescription(Pilot->Description);
}

auto MCPilotPurchaseBlock::DrawRow(MCLogPort* port, int32_t top) -> void
{
    MCPurPilotData* shown = Pilot;

    // A hired pilot (health cleared by PilotPurchaseCallback) is not drawn.
    if (shown->Health == 0)
    {
        return;
    }

    MCPurchaseScreen* screen = GlobalLogPtr->PurchaseScreen;
    MCLogPort* work = NewRowPicture(screen->PilotTabPort, port, top, true);
    CopyArt(work, 5, 4, "%slogart\\lspflp%02d.tga", shown->NameIndex);
    CopyArt(work, 7, 0x26, "%slogart\\pilot%02d.tga", shown->NameIndex);
    char text[256];
    std::snprintf(text, sizeof(text), "%d", shown->Cost);
    WriteText(YellowDropFont, work, 0x1f, 0x12, text);

    // An out-of-range rank shows the text buffer's last contents (the price).
    if (shown->Rank >= 0 && shown->Rank <= 3)
    {
        CLoadString(ThisInstance, 0x70 + static_cast<uint32_t>(shown->Rank), text, 0xfe);
    }

    WriteText(YellowDropFont, work, 0x9a, 0x2a, text);
    GlobalLogPtr->DrawPilotSkillBar(shown->Gunnery, 0x54, 0x22, 0, 0x36, 4, work);
    GlobalLogPtr->DrawPilotSkillBar(shown->Piloting, 0x54, 0x2b, 0, 0x36, 4, work);
    GlobalLogPtr->DrawPilotSkillBar(shown->Jumping, 0x54, 0x34, 0, 0x36, 4, work);
    GlobalLogPtr->DrawPilotSkillBar(shown->Sensors, 0x54, 0x3d, 0, 0x36, 4, work);
    // One pip per point of health.
    int32_t x = 0xe;

    for (int32_t pip = Pilot->Health; pip > 0; --pip, x += 3)
    {
        VfxPixelWrite(work->Frame(), x, 0x22, 0xcf);
        VfxPixelWrite(work->Frame(), x + 1, 0x22, 0xcf);
        VfxPixelWrite(work->Frame(), x + 1, 0x23, 0xee);
        VfxPixelWrite(work->Frame(), x, 0x23, 0xcf);
    }

    DrawInfoDescription(work, 0xc6, 0x25, Pilot->Description, 8, 0x48);
    delete work;
}

auto MCPilotPurchaseBlock::OnBeginDrag(MCLogPort* surface) -> void
{
    // The square at (6, 0x25) of the row, over the pilot list's colour 0xff.
    VfxPaneWipe(surface->Frame(), 0xff);
    MCDragIcon::DrawFrom(surface, 6, 0x25, [this](MCLogPort* port) { DrawRow(port, 0); });
}

// PurPilotList

auto MCPurPilotList::Destroy() -> void
{
    for (MCPurPilotData* node = First; node != nullptr; node = First)
    {
        MCPilotPurchaseBlock* block = node->Block;
        First = node->Next;

        if (block != nullptr)
        {
            delete block;
            node->Block = nullptr;
        }

        if (node->Description != nullptr)
        {
            LogFree(node->Description);
            node->Description = nullptr;
        }

        delete node;
    }

    First = nullptr;
    Count = 0;
}

auto MCPurPilotList::AddPilot(char* fileName, int32_t status) -> int32_t
{
    std::string path;
    auto* file = new MCFitIniFile;
    Assert(file != nullptr, 0, " no RAM for pilot file ");
    OpenProfile(file, path, WarriorPath, fileName, true, " could not open scenario file ");

    auto* data = new MCPurPilotData;
    auto* block = new MCPilotPurchaseBlock;
    data->Block = block;
    block->Init(data);
    std::strncpy(data->FileName, fileName, sizeof(data->FileName));
    int32_t result = file->SeekBlock("General");
    Assert(result == 0, result, " could not find general block in pilot file ");
    result = file->ReadIdLong("NameIndex", data->NameIndex);
    Assert(result == 0, result, "could not read NameIndex in pilot profile");
    char callsign[0x80];
    result = file->ReadIdString("Callsign", callsign, 0x14);
    Assert(result == 0, result, " could not read callsign in pilot file ");
    std::strcpy(data->Callsign, callsign);
    result = file->ReadIdString("pilotAudio", data->PilotAudio, 0xff);
    Assert(result == 0, result, " Could not find pilotAudio in General Block ");
    data->DescIndex = -1;
    data->Description = nullptr;
    file->ReadIdLong("DescIndex", data->DescIndex);
    data->LoadDescription(data->DescIndex);
    result = file->SeekBlock("Skills");
    Assert(result == 0, result, " could not find skills block in pilot file ");
    result = file->ReadIdChar("Piloting", data->Piloting);
    Assert(result == 0, result, " could not read Piloting in pilot file ");
    result = file->ReadIdChar("Gunnery", data->Gunnery);
    Assert(result == 0, result, " could not read Gunnery in pilot file ");
    result = file->ReadIdChar("Jumping", data->Jumping);
    Assert(result == 0, result, " could not read Jumping in pilot file ");
    result = file->ReadIdChar("Sensors", data->Sensors);
    Assert(result == 0, result, " could not read Sensors in pilot file ");
    result = file->SeekBlock("Status");
    Assert(result == 0, result, " could not find status block in pilot file ");
    result = file->ReadIdChar("Wounds", data->Health);
    Assert(result == 0, result, " could not read Wounds in pilot file ");
    data->Health = static_cast<char>(6 - data->Health);
    data->Rank = 0;
    data->CalcRank();
    data->Cost = GlobalLogPtr->PilotCosts[data->Rank];
    data->Status = status;

    // Insert by rank, then (among the pilots from there on) by callsign.
    MCPurPilotData* previous = nullptr;
    MCPurPilotData* node = First;

    while (node != nullptr && node->Rank < data->Rank)
    {
        previous = node;
        node = node->Next;
    }
    while (node != nullptr && std::strcmp(node->Callsign, data->Callsign) < 0)
    {
        previous = node;
        node = node->Next;
    }

    data->Next = node;

    if (previous != nullptr)
    {
        previous->Next = data;
    }
    else
    {
        First = data;
    }

    ++Count;
    file->Close();
    delete file;
    return 0;
}

auto MCPurPilotList::RemovePilot(int32_t index) -> int32_t
{
    if (Count <= index)
    {
        return -1;
    }

    MCPurPilotData* previous = nullptr;
    MCPurPilotData* node = First;

    for (; index > 0; --index)
    {
        previous = node;
        node = node->Next;
    }

    if (previous != nullptr)
    {
        previous->Next = node->Next;
    }
    else
    {
        First = node->Next;
    }

    delete node;
    --Count;
    return 0;
}

auto MCPurPilotList::SetPilotStatus(int32_t pilotId, int32_t status) -> void
{
    MCPurPilotData* node = First;

    for (int32_t index = 0; index < Count; ++index, node = node->Next)
    {
        if (node->DescIndex == pilotId)
        {
            node->Status = status;
            return;
        }
    }
}

auto MCPurPilotList::GetPilotInfo(int32_t index, MCPurPilotData*& pilot) -> int32_t
{
    if (Count <= index)
    {
        return -1;
    }

    MCPurPilotData* node = First;

    for (; index > 0; --index)
    {
        node = node->Next;
    }

    pilot = node;
    return 0;
}

auto MCPurPilotList::GetVisiblePilotCount() -> int32_t
{
    int32_t visible = 0;

    for (MCPurPilotData* node = First; node != nullptr; node = node->Next)
    {
        if (node->Status == 0)
        {
            ++visible;
        }
    }

    return visible;
}

// Data records

auto MCPurMechData::LoadDescription(int32_t index) -> void
{
    // The original loops three times over this same record; only the first pass can load.
    if (index < 0)
    {
        return;
    }

    if (Description == nullptr)
    {
        Description = LoadDescriptionText(DescIndex);
    }
}

auto MCPurMechData::CalcBR() -> int32_t
{
    BattleRating = ChassisBR;

    for (MCLogInventoryItem* item = Inventory->Items; item != nullptr; item = item->Next)
    {
        BattleRating = static_cast<int32_t>(
            static_cast<double>(MasterComponentList[item->MasterID].BattleRating) * item->Count + BattleRating);
    }

    return BattleRating;
}

auto MCPurPilotData::CalcRank() -> void
{
    // The skills weighted (piloting, jumping, sensors, gunnery); the rank is the first scale entry above it.
    double weighted =
        (static_cast<double>(Gunnery) * SkillWeightings[3] + static_cast<double>(Sensors) * SkillWeightings[2] +
         static_cast<double>(Jumping) * SkillWeightings[1] + static_cast<double>(Piloting) * SkillWeightings[0]) /
        (static_cast<double>(SkillWeightings[3]) + SkillWeightings[2] + SkillWeightings[1] + SkillWeightings[0]);

    for (int32_t level = 0; level < 4; ++level)
    {
        if (weighted < WarriorRankScale[level])
        {
            Rank = level;
            return;
        }
    }
}

auto MCPurPilotData::LoadDescription(int32_t index) -> void
{
    if (index > -1 && Description == nullptr)
    {
        Description = LoadDescriptionText(DescIndex);
    }
}

auto MCPurVehicleData::LoadDescription(int32_t index) -> void
{
    if (index > -1 && Description == nullptr)
    {
        Description = LoadDescriptionText(DescIndex);
    }
}

auto MCPurVehicle::CalcVehicleCost() -> void
{
    MCPurVehicleData* vehicle = Data;
    vehicle->Cost = vehicle->BaseCost;

    for (MCLogInventoryItem* item = vehicle->Inventory->Items; item != nullptr; item = item->Next)
    {
        vehicle->Cost += MasterComponentList[item->MasterID].ResourcePoints * item->Count;
    }
}
