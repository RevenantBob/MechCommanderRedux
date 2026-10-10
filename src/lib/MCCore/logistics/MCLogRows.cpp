#include "stdafx.h"
#include "logistics/MCLogRows.h"
#include "gui/MCGuiEvent.h"
#include "gui/MCGuiFont.h"
#include "gui/MCGuiGlobals.h"
#include "logistics/MCPurchaseDlg.h"
#include "logistics/MCPurchaseScreen.h"
#include "logistics/MCRepairScreen.h"
#include "logistics/MCReusableDialog.h"
#include "main/MCLogistics.h"
#include "main/main.h"
#include "mission/MCMission.h"
#include "object/MCMasterComponent.h"
#include "vfx/MCAgShape.h"
#include "vfx/MCVfxFunctions.h"

// AlphaTable row 0x100: the alpha colour greyed-out rows darken through.
char* LogisticFadetable = reinterpret_cast<char*>(AlphaTable.data()) + 0x100 * 256;

namespace
{
    /// <summary>The damage state of a diagram location from its armor left (-1 = undamaged).</summary>
    int32_t DiagramState(int32_t percent, uint8_t internals)
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

                tables[fade][0xe7] = IconFade[fade][2];
                tables[fade][0xe8] = IconFade[fade][1];
                tables[fade][0xea] = IconFade[fade][0];
            }

            MCRenderer::RegisterData(tables, sizeof(tables), MCDataKind::Tables);
            return tables;
        }();

        MCAgDrawShape(diagram.Frame(), shapes, location, 0, 0, MCShapeOp::Xlat, recolor[state]);
    }

    void DrawLine(MCPane* pane, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint8_t color)
    {
        VfxLineDraw(pane, x0, y0, x1, y1, color);
    }
}

auto OnRepairScreen() -> bool
{
    return GlobalLogPtr->CurrentScreen == GlobalLogPtr->RepairScreen.get();
}

auto OnPurchaseScreen() -> bool
{
    return GlobalLogPtr->CurrentScreen == GlobalLogPtr->PurchaseScreen.get();
}

auto ShowLogMessage(uint32_t stringId, bool okayArt) -> void
{
    ShowLogMessage(LoadGameString(stringId, 0xfe), okayArt);
}

auto ShowLogMessage(std::string_view text, bool okayArt) -> void
{
    MCReusableDialog* dialog = GlobalLogPtr->MessageDialog.get();
    dialog->SetText(text);
    dialog->SetTwoButton(false);
    dialog->Callback = nullptr;

    if (okayArt)
    {
        dialog->OkButton->SetUpPicture("bh_okay.tga");
        dialog->OkButton->SetDownPicture("bg_okay.tga");
        dialog->OkButton->Disabled = false;
    }

    dialog->Activate();
}

auto OpenPurchaseDialog(int32_t purchaseType, int32_t unitCost, int32_t maxQuantity, std::string_view title,
                        std::string_view subtitle, MCGuiPort* picture,
                        std::function<void(int32_t result, int32_t quantity)> callback) -> void
{
    MCPurchaseDlg* dialog = GlobalLogPtr->PurchaseDialog.get();
    dialog->Init(purchaseType, unitCost, maxQuantity, title, subtitle, picture);
    dialog->SetCallback(std::move(callback));
    dialog->Activate();
}

auto WeightClassString(float tonnage) -> uint32_t
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

auto ArmorClassString(float armorTonnage) -> uint32_t
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

auto OverPaneInside(MCGuiObject* posPane, MCGuiObject* widthPane, MCGuiObject* heightPane, MCGuiEvent* event) -> bool
{
    return posPane->GlobalX() < event->X && event->X < posPane->GlobalX() + widthPane->Width() - 0xd &&
           posPane->GlobalY() < event->Y && event->Y < posPane->GlobalY() + heightPane->Height();
}

auto OverPaneInside(MCGuiObject* pane, MCGuiEvent* event) -> bool
{
    return OverPaneInside(pane, pane, pane, event);
}

auto OverPaneArea(MCGuiObject* pane, int32_t xPos, int32_t yPos) -> bool
{
    return pane != nullptr && pane->GlobalX() <= xPos && xPos <= pane->GlobalX() + pane->Width() &&
           pane->GlobalY() <= yPos && yPos <= pane->GlobalY() + pane->Height();
}

auto OnRow(MCGuiObject* row, MCGuiEvent* event) -> bool
{
    return row->GlobalX() <= event->X && event->X <= row->GlobalX() + row->Width() && row->GlobalY() <= event->Y &&
           event->Y <= row->GlobalY() + row->Height();
}

auto WriteText(MCGuiFont* font, MCLogPort* port, int32_t xPos, int32_t yPos, std::string_view text) -> void
{
    font->WriteString(port->Frame(), xPos, yPos, text);
}

auto WriteText(MCGuiFont* font, MCLogPort* port, int32_t xPos, int32_t yPos, const char* text) -> void
{
    font->WriteString(port->Frame(), xPos, yPos, text);
}

auto RowPicture(MCLogPort* art, MCLogPort* port, int32_t top, bool keyed) -> std::unique_ptr<MCLogBlockPort>
{
    auto row = std::make_unique<MCLogBlockPort>(port->Frame(), 0, top, art->Width(), art->Height(), keyed);
    art->CopyTo(row->Frame(), 0, 0, false);
    return row;
}

auto CopyArt(MCLogPort* port, int32_t xPos, int32_t yPos, std::string_view name) -> void
{
    LogScreenArt(name)->CopyTo(port->Frame(), xPos, yPos, true);
}

auto WipeBox(MCLogPort* port, int32_t xPos, int32_t yPos, int32_t width, int32_t height, int32_t color) -> void
{
    MCLogBlockPort box(port->Frame(), xPos, yPos, width, height, false);
    VfxPaneWipe(box.Frame(), static_cast<uint32_t>(color));
}

auto DrawInfoDescription(MCLogPort* port, int32_t width, int32_t height, std::string_view description, int32_t xPos,
                         int32_t yPos) -> void
{
    if (description.empty())
    {
        return;
    }

    // Drawn in place: the original wrote it into a picture wiped to 0xff and copied that keyed on 0xff.
    MCLogBlockPort picture(port->Frame(), xPos, yPos, width, height, true);
    GuiSystem()->TextFormatter.Process(description, &picture, 0, 0);
}

auto DrawInfoDescription(MCLogPort* port, int32_t width, int32_t height, const char* description, int32_t xPos,
                         int32_t yPos) -> void
{
    if (description != nullptr)
    {
        DrawInfoDescription(port, width, height, std::string_view(description), xPos, yPos);
    }
}

auto PrepareInfoDescription(char* description) -> void
{
    if (description != nullptr)
    {
        description[3] = '9';
    }
}

auto PrepareInfoDescription(std::string& description) -> void
{
    if (description.size() > 3)
    {
        description[3] = '9';
    }
}

auto ComponentForm(uint8_t masterID) -> MCComponentForm
{
    return MasterComponentList[masterID].Form;
}

auto IsWeapon(MCComponentForm form) -> bool
{
    return form == MCComponentForm::WeaponEnergy || form == MCComponentForm::WeaponBallistic ||
           form == MCComponentForm::WeaponMissile;
}

auto IsEquipment(MCComponentForm form) -> bool
{
    return form == MCComponentForm::Sensor || form == MCComponentForm::Ecm || form == MCComponentForm::Probe;
}

auto UsesAmmo(uint8_t masterID) -> bool
{
    return ComponentForm(masterID) == MCComponentForm::WeaponBallistic ||
           ComponentForm(masterID) == MCComponentForm::WeaponMissile;
}

auto WeaponWorth(const MCMasterComponent& component) -> int16_t
{
    auto perTen = static_cast<int16_t>(static_cast<int32_t>(static_cast<double>(component.Damage) * 10.0 /
                                                            static_cast<double>(component.RecycleTime)));
    return static_cast<int16_t>(
        static_cast<int32_t>(static_cast<double>(perTen) * static_cast<double>(component.WeaponRange[3]) *
                             static_cast<double>(0.041666668f)));
}

auto RepairShade(uint32_t percent) -> int32_t
{
    if (percent >= 0x33)
    {
        return percent < 0x4c ? 2 : 1;
    }

    if (percent >= 0x1a)
    {
        return 3;
    }

    return percent != 0 ? 4 : 0;
}

auto PercentLeft(uint8_t current, uint8_t maximum) -> uint32_t
{
    // Port fix: a zero maximum counts as 0% (the original divided by zero).
    if (maximum == 0)
    {
        return 0;
    }

    return static_cast<uint32_t>(current) * 100 / maximum;
}

auto ArmorShadeTable(int32_t shade) -> uint8_t*
{
    static constexpr int32_t tables[5] = {4, 0, 1, 2, 3};
    return GlobalLogPtr->ShapeLookaside[static_cast<size_t>(tables[shade])].data();
}

auto InternalShadeTable(int32_t shade) -> uint8_t*
{
    static constexpr int32_t tables[5] = {9, 5, 6, 7, 8};
    return GlobalLogPtr->ShapeLookaside[static_cast<size_t>(tables[shade])].data();
}

auto DrawTonnageBar(MCPane* pane, int32_t xPos, int32_t fill) -> void
{
    DrawLine(pane, xPos + 1, 9, xPos + 0x35, 9, 0xed);
    DrawLine(pane, xPos, 10, xPos + 0x35, 10, 0xce);
    DrawLine(pane, xPos, 11, xPos + 0x35, 11, 0xce);
    DrawLine(pane, xPos + 1, 12, xPos + 0x35, 12, 0xae);
    DrawLine(pane, xPos + 0x36, 10, xPos + 0x36, 11, 0xae);

    if (fill == 0)
    {
        return;
    }

    int32_t end = fill + xPos;
    DrawLine(pane, xPos + 1, 9, end, 9, 0xe3);
    DrawLine(pane, xPos, 10, xPos, 11, 0xe3);
    DrawLine(pane, xPos + 1, 12, end, 12, 0xe5);
    DrawLine(pane, end + 1, 10, end + 1, 11, 0xe5);
    DrawLine(pane, xPos + 1, 10, end, 10, 0xe4);
    DrawLine(pane, xPos + 1, 11, end, 11, 0xe4);
    DrawLine(pane, end + 2, 10, end + 2, 11, 0x10);
    VfxPixelWrite(pane, end + 1, 9, 0x10);
    VfxPixelWrite(pane, end + 1, 12, 0x10);
}

auto DrawInventoryList(MCInventoryList* inventory, MCLogPort* port) -> std::optional<std::string>
{
    std::vector<int32_t> shortRange;
    std::vector<int32_t> mediumRange;
    std::vector<int32_t> longRange;
    int32_t index = 0;

    for (const std::unique_ptr<MCLogInventoryItem>& item : inventory->Items)
    {
        const int32_t position = index++;
        const MCMasterComponent& component = MasterComponentList[item->MasterID];
        MCComponentForm form = component.Form;

        if (!IsWeapon(form) && form != MCComponentForm::Weapon)
        {
            continue;
        }

        if (component.WeaponRange[3] < 76.0f)
        {
            shortRange.push_back(position);
        }
        else if (component.WeaponRange[3] < 151.0f)
        {
            mediumRange.push_back(position);
        }
        else
        {
            longRange.push_back(position);
        }
    }

    std::optional<std::string> last;
    int32_t line = 0;

    auto writeLine = [&](MCLogInventoryItem* item)
    {
        last = std::format("{} {}", item->Count, std::string_view(item->Name));
        WriteText(GreenFont, port, 0x14a, (GreenFont->Height() + 1) * line + 4, *last);
        ++line;
    };

    for (const auto* group : {&shortRange, &mediumRange, &longRange})
    {
        for (int32_t position : *group)
        {
            writeLine(inventory->GetItemInfo(position));
        }
    }

    for (const std::unique_ptr<MCLogInventoryItem>& item : inventory->Items)
    {
        if (IsEquipment(MasterComponentList[item->MasterID].Form))
        {
            writeLine(item.get());
        }
    }

    return last;
}

// Logistics diagrams and the mech's condition (their code sits with the rows that draw them)

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

    int32_t state = DiagramState(percent, mech->Internals[location].CurArmor);
    DrawDiagram(GlobalLogPtr->MechIconShapes[mech->NameIndex].Data(), location, state, port, xPos, yPos);
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
    int32_t state = DiagramState(percent, vehicle->CurInternalStructure[location]);
    DrawDiagram(GlobalLogPtr->VehicleIconShapes[vehicle->NameIndex].Data(), location, state, port, xPos, yPos);
}

auto MCLogistics::DrawPilotSkillBar(MCLogWarrior* warrior, int32_t skill, int32_t xPos, int32_t yPos, int32_t row,
                                    int32_t width, int32_t rowHeight, MCLogPort* port) -> void
{
    DrawPilotSkillBar(warrior->Skills[skill], xPos, yPos, row, width, rowHeight, port);
}

auto MCLogistics::DrawPilotSkillBar(int32_t value, int32_t xPos, int32_t yPos, int32_t row, int32_t width,
                                    int32_t rowHeight, MCLogPort* port) -> void
{
    MCPane* pane = port->Frame();
    int32_t top = row * rowHeight + yPos;
    int32_t right = xPos + width;
    // The empty bar.
    DrawLine(pane, xPos, top, right - 1, top, 0x32);
    DrawLine(pane, xPos, top + 1, xPos, top + 2, 0x32);
    DrawLine(pane, xPos + 1, top + 3, right - 1, top + 3, 0x14);
    DrawLine(pane, right, top + 1, right, top + 2, 0x14);
    DrawLine(pane, xPos + 1, top + 1, right - 1, top + 1, 0x33);
    DrawLine(pane, xPos + 1, top + 2, right - 1, top + 2, 0x33);

    if (value == 0)
    {
        return;
    }

    // The filled part.
    double scale =
        static_cast<double>(width) / (static_cast<double>(MaxPilotSkill) - static_cast<double>(MinPilotSkill));
    int32_t end =
        static_cast<int32_t>((static_cast<double>(value) - static_cast<double>(MinPilotSkill)) * scale) + xPos;
    DrawLine(pane, xPos + 1, top, end - 1, top, 0xe3);
    DrawLine(pane, xPos, top + 1, xPos, top + 2, 0xe3);
    DrawLine(pane, xPos + 1, top + 3, end - 1, top + 3, 0xe5);
    DrawLine(pane, end, top + 1, end, top + 2, 0xe5);
    DrawLine(pane, xPos + 1, top + 1, end - 1, top + 1, 0xe4);
    DrawLine(pane, xPos + 1, top + 2, end - 1, top + 2, 0xe4);
    DrawLine(pane, end + 1, top + 1, end + 1, top + 2, 0x10);
    VfxPixelWrite(pane, end, top, 0x10);
    VfxPixelWrite(pane, end, top + 3, 0x10);
}

auto MCLogMech::CalcStatus() -> float
{
    StatusValue = 0.0f;

    if (PilotIndex < 0 && NetworkPilot == nullptr)
    {
        return 0.0f;
    }

    float pilotFactor = 0.0f;
    MCLogWarrior* warrior = nullptr;

    if (!LocalPart)
    {
        warrior = NetworkPilot;
    }
    else
    {
        GlobalLogPtr->AssignedWarriorList->GetWarriorInfo(PilotIndex, warrior);
    }

    if (warrior != nullptr)
    {
        pilotFactor = static_cast<float>(static_cast<double>(warrior->Skills[3]) * 0.02);
    }

    // The firepower left: the undamaged weapons' worth over all weapons' worth (0/0 without a pilot).
    double working = 0.0;
    double total = 0.0;

    for (const std::unique_ptr<MCLogInventoryItem>& item : Inventory->Items)
    {
        const MCMasterComponent& component = MasterComponentList[item->MasterID];

        if (!IsWeapon(component.Form) || item->Stats.empty())
        {
            continue;
        }

        int16_t value = WeaponWorth(component);

        for (const std::unique_ptr<MCLogInventoryStat>& stat : item->Stats)
        {
            if (stat->Hits == 0)
            {
                working += static_cast<double>(value) * pilotFactor;
            }

            total += static_cast<double>(value) * pilotFactor;
        }
    }

    auto firepower = static_cast<float>(working / total);

    if (std::isnan(firepower) || firepower == 0.0f)
    {
        return StatusValue;
    }

    // The body: the head's armor, the weaker of two torso locations, the legs' and the side torsos' armor.
    auto head = static_cast<float>(static_cast<double>(Armor[0].CurArmor) / Armor[0].MaxArmor * 0.6 + 0.4);
    double weakCurrent = Armor[1].CurArmor;
    double weakMaximum = Armor[1].MaxArmor;

    if (static_cast<double>(Armor[8].CurArmor) < weakCurrent)
    {
        weakCurrent = Armor[8].CurArmor;
        weakMaximum = Armor[8].MaxArmor;
    }

    auto legs = static_cast<float>(static_cast<double>(Armor[5].CurArmor + Armor[4].CurArmor) /
                                       (Armor[5].MaxArmor + Armor[4].MaxArmor) * 0.25 +
                                   0.75);
    double sides = static_cast<double>(Armor[9].CurArmor + Armor[10].CurArmor + Armor[3].CurArmor + Armor[2].CurArmor) /
                       (Armor[10].MaxArmor + Armor[9].MaxArmor + Armor[3].MaxArmor + Armor[2].MaxArmor) * 0.25 +
                   0.75;
    double body = sides * ((weakCurrent / weakMaximum + 1.0) * 0.5) * legs * legs * head;

    if (std::isnan(body) || body == 0.0)
    {
        return StatusValue;
    }

    // The pilot's wounds.
    static constexpr float woundFactors[7] = {1.0f, 0.95f, 0.85f, 0.75f, 0.5f, 0.3f, 0.0f};
    float woundFactor = head;

    if (warrior != nullptr)
    {
        auto wounds = static_cast<int32_t>(warrior->Wounds);
        // Port fix: wounds past the table read 0 (the original read the stack beyond it).
        woundFactor = wounds >= 0 && wounds < 7 ? woundFactors[wounds] : 0.0f;
    }

    StatusValue = static_cast<float>(static_cast<double>(woundFactor) * body * firepower);
    return StatusValue;
}
