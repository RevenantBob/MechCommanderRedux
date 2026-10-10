#include "stdafx.h"
#include "iface/MCMechIcon.h"
#include "color/MCPalette.h"
#include "gui/MCFloatHelp.h"
#include "iface/MCMechBar.h"
#include "iface/MCTacticalInterface.h"
#include "mission/MCScenario.h"
#include "object/MCMover.h"
#include "object/MCObjectQueue.h"
#include "object/MCObjectSystem.h"
#include "vfx/MCVfx.h"
#include "vfx/MCVfxFunctions.h"

namespace
{
    /// <summary>
    /// The fade table a mech icon's part is drawn through for its colour code (rows of the fade palettes past the haze
    /// levels).
    /// </summary>
    uint8_t* MechIconPartTable(uint8_t color)
    {
        const int32_t row = GamePalette()->NumBitmapHazeLevels;
        uint8_t* fades = GamePalette()->FadePalettes.data();

        switch (color)
        {
            case 0xeb:
                return fades + (row + 0x11) * 0x200;
            case 0xef:
                return fades + row * 0x200 + 0x2300;
            case 0xf2:
                return fades + row * 0x200 + 0x2100;
            default:
                return fades + (row + 0x12) * 0x200;
        }
    }

    /// <summary>The percentage of <paramref name="current"/> out of <paramref name="maximum"/>, floored.</summary>
    int16_t ArmorPercent(float current, uint8_t maximum)
    {
        // A location without armour divides by zero in MCX.EXE: __ftol returns 0x80000000, whose low half is 0.
        if (maximum == 0)
        {
            return 0;
        }

        return static_cast<int16_t>(std::floor(static_cast<double>(current) * 100.0 / static_cast<double>(maximum)));
    }
}

auto MCMechIcon::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* bitmapName) -> int32_t
{
    int32_t result = MCGuiObject::Init(xPos, yPos, width, height, bitmapName);

    if (result != 0)
    {
        return result;
    }

    ObjectType = 8;
    DeadImage = MCMakeGui<MCGuiPort>();
    result = DeadImage->Init(10);

    if (result != 0)
    {
        return result;
    }

    PartColor.fill(0xff);
    SetBackColor(0x10);
    VfxPaneWipe(DisplayPort->Frame(), 0x10);
    DiagramX = 0;
    DiagramY = 0;
    Mover = nullptr;
    DamageShapes = {};
    return 0;
}

auto MCMechIcon::Destroy() -> void
{
    DeadImage.reset();
    DamageShapes = {};
    MCGuiObject::Destroy();
}

auto MCMechIcon::Draw() -> void
{
    // Port: the colours are brought up to date by UpdateModel (the original called GetColors here).
    DrawIcon(DisplayPort.get());
}

auto MCMechIcon::UpdateModel() -> void
{
    GetColors();
}

auto MCMechIcon::DrawIcon(MCGuiPort* target) -> void
{
    auto* shown = static_cast<MCGameObject*>(Mover);

    if (shown == nullptr)
    {
        return;
    }

    DrawParts(target);

    // Destroyed or disabled: the "destroyed" image over the diagram.
    if (shown->Status == 1 || shown->Status == 2)
    {
        VfxPaneCopy(DeadImage->Frame(), 0, 0, target->Frame(), DiagramX, DiagramY, 0xfff);
    }
}

auto MCMechIcon::Enter() -> void
{
    auto* bar = static_cast<MCMechBar*>(Parent);
    bar->HighlightId = PartId;
    bar->Draw();
    MCGuiObject::Enter();
}

auto MCMechIcon::Leave() -> void
{
    auto* bar = static_cast<MCMechBar*>(Parent);
    TacticalInterface()->FloatingTags[0]->ShowGuiWindow(false);

    if (bar != nullptr)
    {
        bar->HighlightId = -1;
        bar->Draw();
    }

    GuiSystem()->SetCurrentCursor(static_cast<MCCursorType>(MCInterfaceCursor::Normal));
    MCGuiObject::Leave();
}

auto MCMechIcon::DrawParts(MCGuiPort* target) -> void
{
    for (int32_t i = 0; i < NumParts; i++)
    {
        const uint8_t color = PartColor[i];

        if (color == 0xb)
        {
            AGShapeDraw(target->Frame(), DamageShapes.Data(), i, DiagramX, DiagramY);
        }
        else
        {
            AGShapeLookaside(MechIconPartTable(color));
            AGShapeTranslateDraw(target->Frame(), DamageShapes.Data(), i, DiagramX, DiagramY);
        }
    }

    if (Mover == nullptr)
    {
        VfxPaneCopy(DeadImage->Frame(), 0, 0, target->Frame(), DiagramX, DiagramY, 0xfff);
    }
}

auto MCMechIcon::GetColors() -> void
{
    auto* shown = static_cast<MCMover*>(Mover);

    if (shown == nullptr)
    {
        return;
    }

    const int8_t numLocations = shown->NumBodyLocations();

    for (int32_t i = 0; i < numLocations; i++)
    {
        const MCBodyLocation& location = shown->BodyAt(i);

        if (location.DamageState == 2)
        {
            PartColor[i] = 0x19;
            continue;
        }

        // A mech's torsos (1..3) show the worse of their front and rear armour.
        int16_t percent;

        if (shown->ObjectClass == MCObjectClass::BattleMech && i > 0 && i < 4)
        {
            const int16_t front = ArmorPercent(shown->Armor[i].CurArmor, shown->Armor[i].MaxArmor);
            const int16_t rear = ArmorPercent(shown->Armor[i + 7].CurArmor, shown->Armor[i + 7].MaxArmor);
            percent = std::min(front, rear);
        }
        else
        {
            percent = ArmorPercent(shown->Armor[i].CurArmor, shown->Armor[i].MaxArmor);
        }

        if (percent >= 0x4c)
        {
            PartColor[i] = 0xb;
        }
        else if (percent >= 0x33)
        {
            PartColor[i] = 0xf2;
        }
        else if (percent >= 0x1a)
        {
            PartColor[i] = 0xeb;
        }
        else
        {
            PartColor[i] = 0xef;
        }
    }
}

auto MCMechIcon::Display() -> void
{
    if (ShowWindow == 0)
    {
        return;
    }

    if (IsHidden() != 0 && HideOffset == 0)
    {
        return;
    }

    // Port: the original redrew its picture every 500 ms here; the icon draws itself each frame, from a model
    // brought up to date first.
    UpdateModel();
    MCGuiObject::Display();
}

auto MCMechIcon::SetID(int32_t newPartId) -> void
{
    // A mover, or else a salvage craft among the scenario's objects.
    Mover = ObjectList()->FindObjectFromPart(newPartId);

    if (Mover == nullptr)
    {
        Mover = Scenario()->ScenarioObjectList->FindIf([newPartId](MCBaseObject* object)
                                                       { return object->PartId == newPartId; });
    }

    if (Mover != nullptr)
    {
        PartId = newPartId;
        NumParts = static_cast<MCMover*>(Mover)->NumBodyLocations();
    }
}

auto MCMechIcon::FillPortBox(MCGuiPort* target, int16_t left, int16_t top, int16_t right, int16_t bottom, uint8_t color)
    -> void
{
    MCPane box = *target->Frame();
    box.X0 = left;
    box.Y0 = top;
    box.X1 = right;
    box.Y1 = bottom;
    VfxPaneWipe(&box, color);
}
