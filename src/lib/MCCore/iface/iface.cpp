#include "stdafx.h"
#include "iface/iface.h"
#include "ai/move.h"
#include "ai/tacordr.h"
#include "appear/MCAppearance.h"
#include "camera/camera.h"
#include "color/MCPalette.h"
#include "gui/afont.h"
#include "gui/ahelp.h"
#include "gui/aport.h"
#include "gui/atextbox.h"
#include "iface/icallbk.h"
#include "iface/parser.h"
#include "lib/MCFatal.h"
#include "lib/MCIDString.h"
#include "lib/MCFrameOfRef.h"
#include "lib/MCVector2D.h"
#include "lib/MCDice.h"
#include "lib/MCFile.h"
#include "lib/MCFitIniFile.h"
#include "linkup/dpplayer.h"
#include "linkup/sessionmanager.h"
#include "logistics/logbri.h"
#include "logistics/logmain.h"
#include "main/main.h"
#include "mission/mission.h"
#include "mission/scenario.h"
#include "network/multplyr.h"
#include "object/artlry.h"
#include "object/baseobj.h"
#include "object/bldng.h"
#include "object/bridge.h"
#include "object/comndr.h"
#include "object/gameobj.h"
#include "object/gate.h"
#include "object/group.h"
#include "object/mech.h"
#include "object/mover.h"
#include "object/object.h"
#include "object/objevnt.h"
#include "object/objque.h"
#include "object/objtype.h"
#include "object/tbldng.h"
#include "object/team.h"
#include "object/train.h"
#include "object/turret.h"
#include "object/warrior.h"
#include "platform/MCInput.h"
#include "platform/MCRenderer.h"
#include "sound/soundsys.h"
#include "terrain/terrain.h"
#include "terrain/terrmap.h"
#include "vfx/MCVfx.h"
#include "vfx/MCVfxFunctions.h"

uint8_t LanceColorArray[8] = {0xee, 0xe5, 0x0e, 0xb5, 0x12, 0x00, 0x00, 0x00};
int32_t Sx = 20;
int32_t Sy = -10;
float SlopeTest[8] = {0.0984914f, 0.3033467f, 0.5345111f, 0.8206788f, 1.2185035f, 1.8708684f, 3.2965581f, 10.1531706f};
uint8_t DanceStep = 0;
int16_t DanceFrames = 0;
MCInterfaceObject* TheInterface = nullptr;
MCGuiCallback* ScrollCallback = nullptr;
MCGuiCallback* MoveCallback = nullptr;
MCGuiObject* DragTarget = nullptr;

namespace
{
    /// <summary>
    /// The port's value for the original's <c>GetSystemMetrics(SM_CXDRAG)</c>, the drag distance used when
    /// <c>iface.fit</c> has none: Windows' default.
    /// </summary>
    constexpr int16_t SystemDragWidth = 4;

    /// <summary>
    /// The building, turret, gate or terrain object the mouse last highlighted (<c>setSelected(1)</c>) in
    /// <see cref="MCInterfaceObject::UpdateMouseState"/>; unhighlighted when the mouse leaves it.
    /// </summary>
    /// <remarks>0x00808048; its name was lost.</remarks>
    MCGameObject* HighlightedObject = nullptr;

    /// <summary>The modifier bits of a key binding (see <see cref="MCInterfaceObject"/>'s remarks).</summary>
    constexpr uint32_t KEY_SHIFT = 0x10000;
    constexpr uint32_t KEY_CTRL = 0x100000;
    constexpr uint32_t KEY_ALT = 0x1000000;

    /// <summary>A default key binding: the slot in <c>keys</c>, the scan code and the modifiers it needs.</summary>
    struct MCDefaultKey
    {
        int32_t Slot = 0;
        uint32_t Code = 0;
        uint32_t Modifiers = 0;
    };

    /// <summary>
    /// The bindings <see cref="MCInterfaceObject::Init"/> sets, in its order. Codes are DirectInput scan codes (0x1xx
    /// = extended keys). Slot 20's code is -1 (no key).
    /// </summary>
    constexpr MCDefaultKey DefaultKeys[] = {
        {1, 0x147, 0},
        {2, 0xe, 0},
        {3, 0x39, 0},
        {4, 0x43, 0},
        {5, 0x44, 0},
        {7, 0x57, 0},
        {6, 0x58, 0},
        {8, 0x39, KEY_CTRL},
        {51, 0x17, 0},
        {11, 0x18, 0},
        {16, 0x1e, 0},
        {12, 0x26, 0},
        {13, 0x32, 0},
        {14, 0x1f, 0},
        {15, 0x2e, 0},
        {17, 0x24, 0},
        {18, 0x24, KEY_CTRL},
        {31, 0x21, 0},
        {32, 0x21, KEY_CTRL},
        {19, 0x22, 0},
        {20, 0xffffffffu, 0},
        {21, 0x149, 0},
        {22, 0x151, 0},
        {23, 0x48, 0},
        {24, 0x47, 0},
        {25, 0x49, 0},
        {26, 0x4c, 0},
        {27, 0x4b, 0},
        {28, 0x4d, 0},
        {29, 0x4f, 0},
        {30, 0x51, 0},
        {74, 0x14, 0},
        {46, 2, 0},
        {47, 3, 0},
        {49, 4, 0},
        {48, 5, 0},
        {50, 0x30, 0},
        {41, 0x3b, KEY_CTRL},
        {42, 0x3c, KEY_CTRL},
        {43, 0x3d, KEY_CTRL},
        {44, 0x3e, KEY_CTRL},
        {33, 0x3b, 0},
        {34, 0x3c, 0},
        {35, 0x3d, 0},
        {36, 0x3e, 0},
        {37, 0x3b, KEY_SHIFT},
        {38, 0x3c, KEY_SHIFT},
        {39, 0x3d, KEY_SHIFT},
        {40, 0x3e, KEY_SHIFT},
        {45, 0x3f, 0},
        {53, 0x4e, 0},
        {54, 0x4a, 0},
        {55, 0xd, 0},
        {56, 0xc, 0},
        {57, 0x4e, KEY_CTRL},
        {58, 0x4a, KEY_CTRL},
        {59, 0xd, KEY_CTRL},
        {60, 0xc, KEY_CTRL},
        {52, 0x38, KEY_ALT},
        {61, 0x32, KEY_ALT},
        {62, 0x1f, KEY_ALT},
        {63, 0x20, KEY_ALT},
        {64, 0x30, KEY_ALT},
        {65, 0x148, 0},
        {66, 0x150, 0},
        {67, 0x14b, 0},
        {68, 0x14d, 0},
        {69, 0x148, KEY_CTRL},
        {70, 0x150, KEY_CTRL},
        {71, 0x14b, KEY_CTRL},
        {72, 0x14d, KEY_CTRL},
        {73, 0xf, 0},
        {75, 0x12, 0},
        {76, 0x1c, 0},
    };

    /// <summary>
    /// Sets a binding's key code, keeping its modifier bits, then sets <paramref name="modifiers"/> (the original
    /// assigns a bitfield's members one by one).
    /// </summary>
    void BindKey(uint32_t& key, uint32_t code, uint32_t modifiers)
    {
        key = (key & (KEY_SHIFT | KEY_CTRL | KEY_ALT)) + code;

        if (modifiers != 0)
        {
            key = (key & ~modifiers) + modifiers;
        }
    }

    /// <summary>
    /// The fade table a mech icon's part is drawn through for its colour code (rows of the fade palettes past the
    /// haze levels).
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

    /// <summary>A mech bar button's place in <see cref="MCMechBar::PlaceButtons"/>'s sort (8 bytes).</summary>
    struct MCButtonSortEntry
    {
        int16_t Index = 0;
        int32_t Key = 0;
    };

    /// <summary>The sort's comparison: by key, ascending.</summary>
    int CompareButtons(const void* a, const void* b)
    {
        const auto keyA = static_cast<const MCButtonSortEntry*>(a)->Key;
        const auto keyB = static_cast<const MCButtonSortEntry*>(b)->Key;

        if (keyA == keyB)
        {
            return 0;
        }

        if (keyB < keyA)
        {
            return 1;
        }

        return -1;
    }

    /// <summary>Shows <paramref name="mover"/>'s callsign and name on floating tag <paramref name="tag"/>.</summary>
    void ShowMoverTag(MCFloatHelp* tag, MCGameObject* mover)
    {
        char text[100];
        sprintf(text, "%s\n%s", mover->GetPilot()->Callsign, static_cast<MCMover*>(mover)->GetIfaceName());
        tag->HelpObject = mover;
        tag->SetBackColor(0);
        tag->TextColor = 0xb;
        tag->SetHelpText(text);
    }

    /// <summary>The mech bar's lance icon <paramref name="index"/>, or null without a mech bar.</summary>
    MCLanceIcon* BarLanceIcon(int32_t index)
    {
        MCMechBar* bar = TheInterface->MechBar;
        return bar != nullptr ? bar->LanceIcons[index] : nullptr;
    }

    /// <summary>aObject::FillBox on any port: wipes the rectangle (port coordinates) of <paramref name="target"/>.</summary>
    void FillPortBox(MCGuiPort* target, int16_t left, int16_t top, int16_t right, int16_t bottom, uint8_t color)
    {
        MCPane box = *target->Frame();
        box.X0 = left;
        box.Y0 = top;
        box.X1 = right;
        box.Y1 = bottom;
        VfxPaneWipe(&box, color);
    }

    /// <summary>Unregisters and frees a mech icon's damage shapes.</summary>
    void FreeDamageShapes(void*& shapes)
    {
        if (shapes != nullptr)
        {
            MCRenderer::UnregisterData(shapes);
            delete[] static_cast<uint8_t*>(shapes);
            shapes = nullptr;
        }
    }
} // namespace

// aMechIcon

auto MCMechIcon::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* bitmapName) -> int32_t
{
    int32_t result = MCGuiObject::Init(xPos, yPos, width, height, bitmapName);

    if (result != 0)
    {
        return result;
    }

    ObjectType = 8;
    DeadImage = new MCGuiPort;

    if (DeadImage == nullptr)
    {
        return 3;
    }

    result = DeadImage->Init(10);

    if (result != 0)
    {
        return result;
    }

    for (int32_t i = 0; i < 8; i++)
    {
        PartColor[i] = 0xff;
        PartDamaged[i] = 0;
        PartDirty[i] = 1;
    }

    SetBackColor(0x10);
    VfxPaneWipe(DisplayPort->Frame(), 0x10);
    DiagramX = 0;
    DiagramY = 0;
    FlashDamage = 0;
    Mover = nullptr;
    DamageShapes = nullptr;
    LastUpdateTime = MCPort::Milliseconds();
    return 0;
}

auto MCMechIcon::Destroy() -> void
{
    if (DeadImage != nullptr)
    {
        DeadImage->Destroy();
        delete DeadImage;
        DeadImage = nullptr;
    }

    FreeDamageShapes(DamageShapes);
    MCGuiObject::Destroy();
}

auto MCMechIcon::Draw() -> void
{
    // Port: the colours are brought up to date by UpdateModel (the original called GetColors here).
    DrawIcon(DisplayPort);
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
    bar->Layout.HighlightId = PartId;
    bar->Draw();
    MCGuiObject::Enter();
}

auto MCMechIcon::Leave() -> void
{
    auto* bar = static_cast<MCMechBar*>(Parent);
    TheInterface->FloatingTags[0]->ShowGuiWindow(0);

    if (bar != nullptr)
    {
        bar->Layout.HighlightId = -1;
        bar->Draw();
    }

    Application->SetCurrentCursor(static_cast<MCCursorType>(0));
    MCGuiObject::Leave();
}

auto MCMechIcon::DrawParts(MCGuiPort* target) -> void
{
    for (int16_t i = 0; i < NumParts; i++)
    {
        const uint8_t color = (FlashDamage != 0 && PartDamaged[i] != 0) ? 0x10 : PartColor[i];

        if (color == 0xb)
        {
            AGShapeDraw(target->Frame(), DamageShapes, i, DiagramX, DiagramY);
        }
        else
        {
            AGShapeLookaside(MechIconPartTable(color));
            AGShapeTranslateDraw(target->Frame(), DamageShapes, i, DiagramX, DiagramY);
        }

        // Port: the original cleared each part's dirty flag here; nothing reads it (every part is drawn each time).
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

    const int8_t numLocations = shown->NumBodyLocations;

    for (int32_t i = 0; i < numLocations; i++)
    {
        const MCBodyLocation& location = shown->BodyAt(i);
        uint8_t newColor;

        if (location.DamageState == 2)
        {
            newColor = 0x19;
        }
        else
        {
            if (static_cast<float>(location.MaxInternalStructure) != location.CurInternalStructure)
            {
                PartDamaged[i] = 1;
            }

            // A mech's torsos (1..3) show the worse of their front and rear armour.
            int16_t percent;

            if (shown->ObjectClass == BATTLEMECH && i > 0 && i < 4)
            {
                const int16_t front = ArmorPercent(shown->Armor[i].CurArmor, shown->Armor[i].MaxArmor);
                const int16_t rear = ArmorPercent(shown->Armor[i + 7].CurArmor, shown->Armor[i + 7].MaxArmor);
                percent = rear < front ? rear : front;
            }
            else
            {
                percent = ArmorPercent(shown->Armor[i].CurArmor, shown->Armor[i].MaxArmor);
            }

            if (percent >= 0x4c)
            {
                newColor = 0xb;
            }
            else if (percent >= 0x33)
            {
                newColor = 0xf2;
            }
            else if (percent >= 0x1a)
            {
                newColor = 0xeb;
            }
            else
            {
                newColor = 0xef;
            }
        }

        if (PartColor[i] != newColor)
        {
            PartColor[i] = newColor;
            PartDirty[i] = 1;
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
    Mover = ObjectList->FindObjectFromPart(newPartId);

    if (Mover == nullptr)
    {
        MCBaseObject* object = nullptr;

        while (Scenario->ScenarioObjectList->Traverse(object) != nullptr)
        {
            if (object->PartId == newPartId)
            {
                Mover = object;
                break;
            }
        }
    }

    if (Mover != nullptr)
    {
        PartId = newPartId;
        NumParts = static_cast<MCMover*>(Mover)->NumBodyLocations;
    }
}

auto MCMechIcon::SetFullUpdate(int fullUpdate) -> void
{
    for (int32_t& dirty : PartDirty)
    {
        dirty = fullUpdate;
    }
}

// FriendlyMechIcon

auto MCFriendlyMechIcon::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* bitmapName) -> int32_t
{
    ShowingWoundedPilot = 0;
    ShowingDeadPilot = 0;
    const int32_t result = MCMechIcon::Init(xPos, yPos, width, height, bitmapName);

    if (result != 0)
    {
        return result;
    }

    PilotImage = new MCGuiPort;
    DiagramX = 6;
    DiagramY = 0x10;

    for (int32_t i = 0; i < 8; i++)
    {
        PartDamaged[i] = 0;
        PartDirty[i] = 1;
    }

    Lance = 5;
    IsPoint = 0;
    return 0;
}

auto MCFriendlyMechIcon::Destroy() -> void
{
    if (PilotImage != nullptr)
    {
        PilotImage->Destroy();
        delete PilotImage;
        PilotImage = nullptr;
    }

    if (IconBackground != nullptr)
    {
        IconBackground->Destroy();
        delete IconBackground;
        IconBackground = nullptr;
    }

    MCMechIcon::Destroy();
}

auto MCFriendlyMechIcon::Enter() -> void
{
    MCFloatHelp* tag = TheInterface->FloatingTags[0];
    auto* bar = static_cast<MCMechBar*>(Parent);
    auto* shown = static_cast<MCGameObject*>(Mover);
    bar->Layout.HighlightId = PartId;
    bar->Draw();

    // The pilot's tag, shown when the mover was seen this turn.
    if (shown != nullptr && Active != 0 && shown->GetPilot() != nullptr)
    {
        ShowMoverTag(tag, shown);

        if (shown->GetWindowsVisible() == Turn)
        {
            tag->ShowGuiWindow(1);
        }
    }

    // The cursor for the current command.
    int32_t cursor;

    if (TheInterface->AnySelected(0) == 0)
    {
        cursor = 0;
    }
    else
    {
        switch (TheInterface->CurrentCommand)
        {
            case 0xb:
            case 0x10:
            {
                if (TheInterface->AnySelected(1) == 0)
                {
                    MCGuiObject::Enter();
                    return;
                }

                cursor = 6;
                break;
            }
            case 0xc:
            {
                if (TheInterface->AnySelected(1) == 0)
                {
                    MCGuiObject::Enter();
                    return;
                }

                cursor = 2;
                break;
            }
            case 0xd:
            {
                if (TheInterface->AnySelected(1) == 0)
                {
                    MCGuiObject::Enter();
                    return;
                }

                cursor = 3;
                break;
            }
            case 0xe:
            {
                if (TheInterface->AnySelected(1) == 0)
                {
                    MCGuiObject::Enter();
                    return;
                }

                cursor = 4;
                break;
            }
            case 0xf:
            case 0x17:
            case 0x18:
            case 0x19:
            case 0x1a:
            case 0x1b:
            case 0x1c:
            case 0x1d:
            case 0x1e:
            {
                if (TheInterface->AnySelected(1) == 0)
                {
                    MCGuiObject::Enter();
                    return;
                }

                cursor = 5;
                break;
            }
            case 0x33:
                cursor = 0xe;
                break;
            default:
            {
                // One selected refit vehicle over a mover needing a refit: the refit command.
                cursor = 0;

                if (TheInterface->NumSelectedMechs == 1 && static_cast<MCMover*>(shown)->NeedsRefit(0) != 0)
                {
                    const int32_t selectedId = TheInterface->NumSelectedMechs < 1 ? -1 : TheInterface->SelectedMechs[0];
                    auto* selected = static_cast<MCGameObject*>(ObjectList->FindObjectFromPart(selectedId));

                    if (selected != nullptr && selected->ObjectClass == GROUNDVEHICLE &&
                        selected->GetRefitPoints() > 0.0f)
                    {
                        Application->SetCurrentCursor(static_cast<MCCursorType>(10));
                        TheInterface->CurrentCommand = 9;
                        TheInterface->CommandOneShot = 0;
                        MCGuiObject::Enter();
                        return;
                    }
                }
                break;
            }
        }
    }

    Application->SetCurrentCursor(static_cast<MCCursorType>(cursor));
    MCGuiObject::Enter();
}

auto MCFriendlyMechIcon::Draw() -> void
{
    DrawIcon(DisplayPort);
}

auto MCFriendlyMechIcon::UpdateModel() -> void
{
    MCMechIcon::UpdateModel();
    auto* shown = static_cast<MCGameObject*>(Mover);

    if (shown == nullptr || Active == 0)
    {
        return;
    }

    MCMechWarrior* pilot = shown->GetPilot();

    if (pilot == nullptr)
    {
        return;
    }

    // Wounded (6 or more wounds), then dead or gone: the portrait changes once. (The original did this as it drew
    // the pilot.)
    if (6.0f <= pilot->Wounds && ShowingWoundedPilot == 0)
    {
        PilotImage->Init(3);
        ShowingWoundedPilot = 1;
    }

    const int32_t status = pilot->Status;

    if ((status == 3 || status == 5 || status == 6) && ShowingDeadPilot == 0)
    {
        PilotImage->Init(4);
        ShowingDeadPilot = 1;
    }
}

auto MCFriendlyMechIcon::DrawIcon(MCGuiPort* target) -> void
{
    // The icon's picture held its background, drawn over each time.
    if (IconBackground != nullptr)
    {
        IconBackground->CopyTo(target->Frame(), 0, 0, 0);
    }

    auto* shown = static_cast<MCMover*>(Mover);
    DrawWeapon(target);
    MCMechIcon::DrawIcon(target);
    FillPortBox(target, 2, 2, 0x31, 9, LanceColorArray[Lance]);

    // A vehicle with a name shows it instead of its pilot.
    if (shown->ObjectClass == GROUNDVEHICLE && shown->GetIfaceName() != nullptr)
    {
        WhiteFont->WriteString(target->Frame(), 5, 3,
                               reinterpret_cast<uint8_t*>(const_cast<char*>(shown->GetIfaceName())), -1);
        return;
    }

    DrawPilot(target);
}

auto MCFriendlyMechIcon::Display() -> void
{
    if (Active != 0)
    {
        MCMechIcon::Display();
    }
}

auto MCFriendlyMechIcon::DrawBox(uint8_t color, int32_t left, int32_t top, int32_t right, int32_t bottom) -> void
{
    if (TheInterface->MechBar->Dancing != 0)
    {
        return;
    }

    if (left == -1)
    {
        left = 0;
    }

    if (top == -1)
    {
        top = 0;
    }

    if (right == -1)
    {
        right = Width() - 1;
    }

    if (bottom == -1)
    {
        bottom = Height() - 1;
    }

    VfxLineDraw(Frame(), left, top, right, top, color);
    VfxLineDraw(Frame(), left, top, left, bottom, color);
    VfxLineDraw(Frame(), left, bottom, right, bottom, color);
    VfxLineDraw(Frame(), right, top, right, bottom, color);
}

auto MCFriendlyMechIcon::DrawPilot(MCGuiPort* target) -> void
{
    auto* shown = static_cast<MCGameObject*>(Mover);

    if (shown == nullptr || Active == 0)
    {
        return;
    }

    MCMechWarrior* pilot = shown->GetPilot();

    if (pilot == nullptr)
    {
        return;
    }

    // (The portrait was switched to the wounded or dead image here: see UpdateModel.)

    // The health bar loses 3 pixels per wound from its right end.
    if (0.0f < shown->GetPilot()->Wounds)
    {
        const auto left = static_cast<int16_t>(47.0f - shown->GetPilot()->Wounds * 3.0f);
        FillPortBox(target, left, 0xb, 0x30, 0xd, 0x10);
    }

    VfxPaneCopy(PilotImage->Frame(), 0, 0, target->Frame(), 0x1c, 0xe, 0xfff);

    if (shown->ObjectClass == BATTLEMECH && shown->GetPilot()->Callsign != nullptr)
    {
        WhiteFont->WriteString(target->Frame(), 5, 3, reinterpret_cast<uint8_t*>(shown->GetPilot()->Callsign), -1);
    }
}

auto MCFriendlyMechIcon::DrawWeapon(MCGuiPort* target) -> void
{
    // The bar runs from x 2 on a mech (beside the portrait), from 0xd otherwise.
    int32_t start = 2;
    auto* shown = static_cast<MCMover*>(Mover);

    if (shown == nullptr || shown->ObjectClass != BATTLEMECH)
    {
        FillPortBox(target, 2, 0xb, 0x2e, 0xc, 0x10);
        start = 0xd;
    }
    else
    {
        FillPortBox(target, 2, 0xb, 0x1b, 0xc, 0x10);
    }

    if (shown == nullptr)
    {
        return;
    }

    const float effectiveness = shown->GetTotalEffectiveness();
    int32_t length = static_cast<int16_t>(std::floor(static_cast<double>(effectiveness) * 25.0));

    if (length == 0 && 0.001 < static_cast<double>(effectiveness))
    {
        length = 3;
    }

    uint32_t color;

    if (effectiveness < 0.5f)
    {
        color = effectiveness <= 0.2f ? 0xef : 0xf2;
    }
    else
    {
        color = 0xe;
    }

    if (length == 0)
    {
        return;
    }

    // A two-pixel bar, lit on its top and left, shaded (colour - 1) on its bottom and right.
    const int32_t end = length + start;
    VfxLineDraw(target->Frame(), start, 0xb, start, 0xc, color);
    VfxLineDraw(target->Frame(), start, 0xb, end, 0xb, color);
    VfxLineDraw(target->Frame(), start + 1, 0xc, end, 0xc, color - 1);
    VfxLineDraw(target->Frame(), end, 0xb, end, 0xc, color - 1);
}

auto MCFriendlyMechIcon::SetID(int32_t newPartId) -> void
{
    MCBaseObject* object = ObjectList->FindObjectFromPart(newPartId);

    if (object == nullptr)
    {
        return;
    }

    const int32_t objectClass = object->ObjectClass;

    if (objectClass != BATTLEMECH && objectClass != GROUNDVEHICLE && objectClass != ELEMENTAL && objectClass != MOVER)
    {
        return;
    }

    MCFile shapeFile;
    char shapeName[20];

    if (objectClass == BATTLEMECH)
    {
        DiagramX = 2;
        DiagramY = 0xe;
        sprintf(shapeName, "mi%02i", object->GetObjectType()->IconNumber);
    }
    else
    {
        DiagramX = 0xe;
        DiagramY = 0xe;
        sprintf(shapeName, "vi%i", object->GetObjectType()->IconNumber);
    }

    // Port: the original loaded the background into the icon's own picture; it is kept apart, and the icon's view
    // takes its size.
    if (IconBackground == nullptr)
    {
        IconBackground = new MCGuiPort;
    }

    IconBackground->Init(const_cast<char*>("guiub00.tga"));
    Port()->InitView(IconBackground->Width(), IconBackground->Height());

    std::string shapePath;
    shapePath = GamePath(ArtPath, shapeName, ".shp");

    if (shapeFile.Open(shapePath) != 0)
    {
        Fatal(0, "Unable to open damage display shape file");
    }

    FreeDamageShapes(DamageShapes);

    // An empty file still fails, as it did when the GUI heap's malloc(0) returned null.
    if (shapeFile.GetLength() == 0)
    {
        shapeFile.Close();
        Fatal(0, "Not enough memory for damage display shape file");
    }

    DamageShapes = new uint8_t[shapeFile.GetLength()]{};
    shapeFile.Read(static_cast<uint8_t*>(DamageShapes), static_cast<int32_t>(shapeFile.GetLength()));
    MCRenderer::RegisterData(DamageShapes, shapeFile.GetLength(), MCDataKind::Shapes);
    shapeFile.Close();

    auto* shown = static_cast<MCMover*>(object);
    NumParts = shown->NumBodyLocations;
    Mover = object;
    PartId = newPartId;

    if (shown->GetPilot() != nullptr && shown->GetPilot()->Picture != nullptr)
    {
        PilotImage->Init(shown->GetPilot()->Picture);
    }

    IsPoint = shown == shown->GetPoint() ? 1 : 0;
    // The original drew the icon here; it draws itself each frame.
    UpdateModel();
}

// aSalvageIcon

auto MCSalvageIcon::Display() -> void
{
    for (SalvageNode* node = Objects; node != nullptr; node = node->Next)
    {
        MCGameObject* object = node->Object;

        if (object->OnScreen() == 0)
        {
            continue;
        }

        MCVector2D screenPos = object->GetScreenPos(0);

        // Port: from the main view's world surface to the view on the screen (through the zoom).
        if (MCViewWindow* view = MCMainView(); view != nullptr)
        {
            screenPos = view->WorldToWindow(screenPos);
        }

        if (DisplayPort != nullptr)
        {
            const auto yPos = static_cast<int32_t>(static_cast<float>(Sy) + screenPos.Y);
            const auto xPos = static_cast<int32_t>(screenPos.X + static_cast<float>(Sx));
            DisplayPort->CopyTo(FramePane, xPos, yPos, 0);
        }
    }
}

// aMechBar

MCMechBar::MCMechBar()
{
    // The original sets the layout's fields (see aMechBarLayout's initializers) and clears the button and lance
    // arrays; videoId is left as allocated.
    Layout.SetSpacing(4, 4);
}

auto MCMechBar::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* bitmapName) -> int32_t
{
    MCGuiObject::Init(xPos, yPos, width, height, bitmapName);
    // The bar draws on its parent: no bitmap of its own.
    MCRenderer::DestroyTexture(Port()->Bitmap());
    MCGuiPort::FreePixels(Port()->Bitmap()->Buffer);
    Port()->Bitmap()->Buffer = nullptr;
    ShowGuiWindow(0);
    Dancing = 0;
    Layout.SetSpacing(0x34, 0x2e);
    return 0;
}

auto MCMechBar::Destroy() -> void
{
    CleanUp();
    MCGuiObject::Destroy();
}

auto MCMechBar::Display() -> void
{
    if (ShowWindow == 0)
    {
        return;
    }

    for (int32_t i = 0; i < NumChildren; i++)
    {
        ChildList[i]->Display();
    }

    if (Dancing == 0)
    {
        // A separator after each lance icon shown, and around each button outside a lance.
        for (MCLanceIcon* lanceIcon : LanceIcons)
        {
            if (lanceIcon != nullptr && lanceIcon->IsShowing() != 0)
            {
                const int32_t lanceRight = lanceIcon->Right();
                VfxLineDraw(Frame(), lanceRight - 1, 0xf, lanceRight - 1, Height() - 1, 0x10);
            }
        }

        for (int16_t i = 0; i < 0xc; i++)
        {
            MCFriendlyMechIcon* button = GetButton(i);

            if (button == nullptr)
            {
                continue;
            }

            button->DrawBox(0x35, -1, -1, -1, -1);

            if (button->IsShowing() != 0 && button->Lance == 5)
            {
                const int32_t buttonRight = button->Right();
                VfxLineDraw(Frame(), buttonRight, 0xf, buttonRight, Height() - 1, 0x10);
                VfxLineDraw(Frame(), 0, 0xf, buttonRight, 0xf, 0x10);
            }
        }
    }

    // Each button's frame: red on the video pilot, else yellow when selected, else white under the mouse.
    for (int16_t i = 0; i < 0xc; i++)
    {
        MCFriendlyMechIcon* button = GetButton(i);

        if (button == nullptr)
        {
            continue;
        }

        const int32_t buttonWidth = button->Width();
        const int32_t buttonHeight = button->Height();

        if (Layout.VideoId == button->PartId)
        {
            button->DrawBox(0xef, 1, 1, buttonWidth - 2, buttonHeight - 2);
        }
        else if (TheInterface->IsSelected(button->PartId) != 0)
        {
            button->DrawBox(0x1f, 1, 1, buttonWidth - 2, buttonHeight - 2);
        }
        else if (Layout.HighlightId == button->PartId)
        {
            button->DrawBox(0xb, 1, 1, buttonWidth - 2, buttonHeight - 2);
        }
    }
}

auto MCMechBar::HandleEvent(MCGuiEvent* event) -> void
{
    // A broadcast (a resolution change): back to the bottom of the screen.
    if (event->Type == 0x12)
    {
        MoveTo(1, Application->Height() - Height() - 1, 0);
    }

    event->Target = MainHolder->GetActivePane();
    TheInterface->HandleEvent(event);
}

auto MCMechBar::Resize(int32_t width, int32_t height) -> void
{
    if (width <= 0 || height <= 0)
    {
        return;
    }

    if (width == WinWidth && height == WinHeight)
    {
        return;
    }

    WinWidth = width;
    WinHeight = height;
    FramePane->X1 = FramePane->X0 - 1 + width;
    FramePane->Y1 = FramePane->Y0 - 1 + height;
}

auto MCMechBar::CleanUp() -> void
{
    for (int16_t i = static_cast<int16_t>(Layout.NumButtons - 1); i > -1; i--)
    {
        RemoveButton(i);
    }

    for (MCLanceIcon*& lanceIcon : LanceIcons)
    {
        if (lanceIcon != nullptr)
        {
            lanceIcon->Destroy();
            delete lanceIcon;
        }

        lanceIcon = nullptr;
    }
}

auto MCMechBar::AddButton(MCFriendlyMechIcon* button) -> int32_t
{
    if (Layout.NumButtons >= Layout.MaxButtons)
    {
        return static_cast<int32_t>(0xEEEE0001u);
    }

    button->MoveTo((Layout.SpacingX + 1) * Layout.NumButtons + 2, 0xf, 0);
    Buttons[Layout.NumButtons] = button;
    AddChild(button);
    Layout.NumButtons++;
    return 0;
}

auto MCMechBar::RemoveButton(int16_t index) -> int32_t
{
    if (index >= Layout.NumButtons)
    {
        return static_cast<int32_t>(0xEEEE0003u);
    }

    MCFriendlyMechIcon* button = Buttons[index];
    button->Destroy();
    delete button;
    Layout.NumButtons--;

    for (int32_t i = index; i < Layout.NumButtons; i++)
    {
        Buttons[i] = Buttons[i + 1];
    }

    Buttons[Layout.NumButtons] = nullptr;
    return 0;
}

auto MCMechBar::RemoveButton(uint32_t buttonPartId) -> int32_t
{
    for (int16_t i = 0; i < Layout.MaxButtons; i++)
    {
        MCFriendlyMechIcon* button = GetButton(i);

        if (button != nullptr && static_cast<uint32_t>(button->PartId) == buttonPartId)
        {
            return RemoveButton(i);
        }
    }

    return static_cast<int32_t>(0xEEEE0003u);
}

auto MCMechBar::GetButtonFromID(int32_t buttonPartId) -> MCFriendlyMechIcon*
{
    int16_t i = 0;

    for (; i < Layout.NumButtons; i++)
    {
        if (GetButton(i) != nullptr && GetButton(i)->PartId == buttonPartId)
        {
            break;
        }
    }

    if (i >= Layout.NumButtons)
    {
        return nullptr;
    }

    return Buttons[i];
}

auto MCMechBar::PlaceButtons(int animate) -> void
{
    // Sort the buttons by lance, then by x; a lance's point first; inactive movers last.
    MCButtonSortEntry order[12];

    for (int16_t i = 0; i < 0xc; i++)
    {
        MCFriendlyMechIcon* button = GetButton(i);

        if (button == nullptr)
        {
            order[i].Index = -1;
            order[i].Key = -999999;
            continue;
        }

        order[i].Index = i;
        order[i].Key = button->X() + button->Lance * 10000;

        if (button->IsPoint != 0)
        {
            order[i].Key -= 1000;
        }

        if (button->Active == 0)
        {
            order[i].Key += 100000;
        }
    }

    std::qsort(order, 0xc, sizeof(MCButtonSortEntry), CompareButtons);

    for (MCLanceIcon* lanceIcon : LanceIcons)
    {
        if (lanceIcon != nullptr)
        {
            lanceIcon->ShowTest();
        }
    }

    // Each lance starts with its icon, one pixel after the previous group; an inactive mover goes off the bar.
    int32_t nextX = 0;
    int32_t lastLance = -1;

    for (const MCButtonSortEntry& entry : order)
    {
        MCFriendlyMechIcon* button = GetButton(entry.Index);

        if (button == nullptr)
        {
            continue;
        }

        const int32_t buttonLance = button->Lance;
        int32_t xPos = nextX;

        if (buttonLance != lastLance || buttonLance == 5)
        {
            if (nextX != 0)
            {
                xPos = nextX + 1;
            }

            lastLance = buttonLance;

            if (buttonLance < 4 && LanceIcons[buttonLance] != nullptr)
            {
                LanceIcons[buttonLance]->MoveTo(xPos, 2, 0);
            }

            xPos++;
        }

        button->TargetX = xPos;

        if (button->Active == 0)
        {
            button->TargetX = xPos + 1000;
        }

        button->ShuffleStep = (xPos - button->X()) / TheInterface->ShuffleFrames;
        nextX = xPos - 1 + Layout.SpacingX;
    }

    if (animate == 0)
    {
        for (int16_t i = 0; i < 0xc; i++)
        {
            MCFriendlyMechIcon* button = GetButton(i);

            if (button != nullptr)
            {
                button->MoveTo(button->TargetX, button->Y(), 0);
            }
        }

        const int32_t linkWidth = Layout.SpacingX - 1;

        for (MCLanceIcon* lanceIcon : LanceIcons)
        {
            const int32_t numActive = lanceIcon->GetNumActiveMovers();
            lanceIcon->Resize(numActive * linkWidth + 3, lanceIcon->Height());
        }
    }
    else if (MoveCallback == nullptr)
    {
        DanceStep = 0;
        DanceFrames = 0;
        Dancing = 1;
        MoveCallback = new MCGuiCallback;
        MoveCallback->SetExec(DancingButtons);
        Application->AddCallback(MoveCallback);
        SoundSystem->PlayDigitalSample(0x42, 1, nullptr, 0, 0);
    }
}

auto MCMechBar::InitLances() -> int32_t
{
    for (int16_t i = 0; i < 4; i++)
    {
        LanceIcons[i] = new MCLanceIcon;
        LanceIcons[i]->Init(i);
        AddChild(LanceIcons[i]);
    }

    PlaceButtons(0);
    return 0;
}

auto MCMechBar::DestroyLances() -> void
{
    for (MCLanceIcon*& lanceIcon : LanceIcons)
    {
        if (lanceIcon != nullptr)
        {
            lanceIcon->Destroy();
            delete lanceIcon;
            lanceIcon = nullptr;
        }
    }
}

auto MCMechBar::GetLanceIconFromID(int32_t lanceId) -> MCLanceIcon*
{
    for (MCLanceIcon* lanceIcon : LanceIcons)
    {
        if (lanceIcon != nullptr && lanceIcon->LanceId == lanceId)
        {
            return lanceIcon;
        }
    }

    return nullptr;
}

auto DancingButtons() -> void
{
    // Three phases: the lance icons drop out of sight while the buttons that move left rise; the buttons slide to
    // their places; then the lance icons come back up and the buttons drop into line.
    const int32_t step = BarLanceIcon(0)->Height() / 3 + 1;
    MCMechBar* bar = TheInterface->MechBar;

    if (DanceFrames == 3)
    {
        DanceStep++;
    }

    if (DanceFrames == TheInterface->ShuffleFrames + 3)
    {
        for (int16_t i = 0; i < 0xc; i++)
        {
            MCFriendlyMechIcon* button = bar->GetButton(i);

            if (button != nullptr)
            {
                button->MoveTo(button->TargetX, button->Y(), 0);
            }
        }

        DanceStep++;
    }

    if (DanceStep == 0)
    {
        for (int32_t i = 0; i < 4; i++)
        {
            MCLanceIcon* lanceIcon = BarLanceIcon(i);
            lanceIcon->MoveTo(lanceIcon->X(), lanceIcon->Y() + step, 0);
            BarLanceIcon(i)->SetDepth(-10);
        }

        for (int16_t i = 0; i < 0xc; i++)
        {
            MCFriendlyMechIcon* button = bar->GetButton(i);

            if (button != nullptr && button->TargetX < button->X())
            {
                button->MoveTo(button->X(), button->Y() - step, 0);
                button->SetDepth(10);
            }
        }
    }
    else if (DanceStep == 1)
    {
        for (int16_t i = 0; i < 0xc; i++)
        {
            MCFriendlyMechIcon* button = bar->GetButton(i);

            if (button != nullptr)
            {
                button->MoveTo(button->X() + button->ShuffleStep, button->Y(), 0);
            }
        }

        for (int32_t i = 0; i < 4; i++)
        {
            BarLanceIcon(i)->ShowGuiWindow(0);
        }

        DanceFrames++;
        return;
    }
    else if (DanceStep == 2)
    {
        for (int32_t i = 0; i < 4; i++)
        {
            MCLanceIcon* lanceIcon = BarLanceIcon(i);

            if (lanceIcon->Group != nullptr)
            {
                lanceIcon->NumActiveMovers = lanceIcon->GetNumActiveMovers();
            }

            lanceIcon->MoveTo(lanceIcon->X(), lanceIcon->Y() - step, 0);
            lanceIcon->ShowTest();
        }

        for (int16_t i = 0; i < 0xc; i++)
        {
            MCFriendlyMechIcon* button = bar->GetButton(i);

            if (button != nullptr && button->Y() != 0xf)
            {
                button->MoveTo(button->X(), button->Y() + step, 0);
            }
        }

        if (TheInterface->ShuffleFrames + 6 <= DanceFrames)
        {
            // Done: everything at rest, the lance icons sized to their lances.
            for (int16_t i = 0; i < 0xc; i++)
            {
                MCFriendlyMechIcon* button = bar->GetButton(i);

                if (button != nullptr)
                {
                    button->SetDepth(0);
                }
            }

            for (int32_t i = 0; i < 4; i++)
            {
                MCLanceIcon* lanceIcon = BarLanceIcon(i);
                lanceIcon->SetDepth(0);
                lanceIcon->MoveTo(lanceIcon->X(), 2, 0);
                const int32_t numActive = lanceIcon->GetNumActiveMovers();
                const int32_t spacing = TheInterface->MechBar->Layout.SpacingX;
                lanceIcon->Resize(numActive * (spacing - 1) + 3, lanceIcon->Height());
            }

            TheInterface->MechBar->Dancing = 0;
            Application->RemoveCallback(MoveCallback);
            TheInterface->MechBar->Draw();
            delete MoveCallback;
            MoveCallback = nullptr;
            SoundSystem->PlayDigitalSample(0x43, 1, nullptr, 0, 0);
            DanceFrames++;
            return;
        }
    }

    DanceFrames++;
}

// LanceIcon

auto MCLanceIcon::Init(int16_t lanceNumber) -> int32_t
{
    const int32_t result = MCGuiObject::Init(0, 2, 0x25, 0xd, nullptr);

    if (result != 0)
    {
        return result;
    }

    NumberImage = new MCGuiPort;
    Assert(NumberImage != nullptr, 0, "Not enough memory for Lance icon ports");
    FirstLinkImage = new MCGuiPort;
    Assert(FirstLinkImage != nullptr, 0, "Not enough memory for Lance icon ports");
    ShortLinkImage = new MCGuiPort;
    Assert(ShortLinkImage != nullptr, 0, "Not enough memory for Lance icon ports");
    LongLinkImage = new MCGuiPort;
    Assert(LongLinkImage != nullptr, 0, "Not enough memory for Lance icon ports");
    LastLinkImage = new MCGuiPort;
    Assert(LastLinkImage != nullptr, 0, "Not enough memory for Lance icon ports");

    char numberName[16];
    sprintf(numberName, "guiubf%i.tga", lanceNumber + 1);
    Assert(NumberImage->Init(numberName) == 0, 0, "Can't load lance icon's gold star");
    Assert(FirstLinkImage->Init(const_cast<char*>("guiub01.tga")) == 0, 0, "Can't load lance icon's first curve");
    Assert(ShortLinkImage->Init(const_cast<char*>("guiub02.tga")) == 0, 0, "Can't load lance icon's short line");
    Assert(LongLinkImage->Init(const_cast<char*>("guiub03.tga")) == 0, 0, "Can't load lance icon's long line");
    Assert(LastLinkImage->Init(const_cast<char*>("guiub04.tga")) == 0, 0, "Can't load lance icon's second curve");

    Group = HomeCommander->GetGroup(lanceNumber);
    LanceId = Group->GetId();
    return result;
}

auto MCLanceIcon::Destroy() -> void
{
    for (MCGuiPort** image : {&NumberImage, &FirstLinkImage, &ShortLinkImage, &LongLinkImage, &LastLinkImage})
    {
        if (*image != nullptr)
        {
            (*image)->Destroy();
            delete *image;
            *image = nullptr;
        }
    }
}

auto MCLanceIcon::Display() -> void
{
    if (ShowWindow == 0)
    {
        return;
    }

    MCPane* pane = FramePane;
    NumberImage->CopyTo(pane, 0, 0, 1);
    const int32_t numberWidth = NumberImage->Width();
    FirstLinkImage->CopyTo(pane, numberWidth, 0, 1);
    const int32_t linkStart = numberWidth + FirstLinkImage->Width();
    ShortLinkImage->CopyTo(pane, linkStart, 4, 1);
    int32_t xPos = linkStart + ShortLinkImage->Width();
    int32_t linkEnd = xPos;

    for (int32_t i = 1; i < GetNumActiveMovers(); i++)
    {
        LongLinkImage->CopyTo(FramePane, xPos, 4, 1);
        xPos += LongLinkImage->Width();
        linkEnd += LongLinkImage->Width();
    }

    pane = FramePane;
    LastLinkImage->CopyTo(pane, xPos, 4, 1);

    // The lance colour under the links.
    MCPane bar = *pane;
    bar.X0 = pane->X0 + linkStart;
    bar.Y1 = pane->Y0 + 10;
    bar.X1 = pane->X0 - 1 + linkEnd;
    bar.Y0 = pane->Y0 + 8;
    VfxPaneWipe(&bar, LanceColorArray[LanceId]);
}

auto MCLanceIcon::HandleEvent(MCGuiEvent* event) -> void
{
    MCTacticalOrder tacOrder;
    tacOrder.Init();

    if (event->Type == 1)
    {
        Application->Grab(this);
        tacOrder.Destroy();
        return;
    }

    if (event->Type != 4 || Application->GrabbedObject() != this)
    {
        tacOrder.Destroy();
        return;
    }

    Application->Release();

    // The eject, power down and power up commands go to the whole lance.
    const int32_t command = TheInterface->CurrentCommand;

    if (command == 1 || command == 0x15 || command == 0x16)
    {
        MCTacticalOrderCode code;

        if (command == 1)
        {
            code = TACTICAL_ORDER_EJECT;
        }
        else if (command == 0x15)
        {
            code = TACTICAL_ORDER_POWERUP;
        }
        else
        {
            code = TACTICAL_ORDER_POWERDOWN;
        }

        tacOrder.Init(ORDER_ORIGIN_PLAYER, code, 1);
        Group->HandleTacticalOrder(tacOrder, 1, nullptr, 0);
        TheInterface->UpdateInterface();
        tacOrder.Destroy();
        return;
    }

    // Otherwise the click selects the lance; shift adds it to the selection or takes it out.
    if (event->ShiftKey == 0)
    {
        TheInterface->DeselectEnemy();
        TheInterface->ClearMechSelection();
        TheInterface->CommandParser->ClearSubjects();
        SoundSystem->PlayDigitalSample(0x30, 1, nullptr, 0, 0);
        TheInterface->SelectLance(Group);
        TheInterface->CommandParser->AddSubject(Group, 0);
        TheInterface->UpdateInterface();
        tacOrder.Destroy();
        return;
    }

    if (TheInterface->IsSelected(Group) != 0)
    {
        TheInterface->DeselectLance(Group);
        TheInterface->CommandParser->RemoveSubject(Group);
        TheInterface->UpdateInterface();
        tacOrder.Destroy();
        return;
    }

    TheInterface->DeselectEnemy();
    SoundSystem->PlayDigitalSample(0x30, 1, nullptr, 0, 0);
    TheInterface->SelectLance(Group);
    TheInterface->CommandParser->AddSubject(Group, 1);
    TheInterface->UpdateInterface();
    tacOrder.Destroy();
}

auto MCLanceIcon::Enter() -> void
{
    for (uint32_t i = 0; static_cast<int32_t>(i) < GetNumActiveMovers(); i++)
    {
        MCFloatHelp* tag =
            static_cast<uint8_t>(i) < 0xc ? TheInterface->FloatingTags[static_cast<uint8_t>(i)] : nullptr;
        MCMover* member = Group->Movers[i];

        if (member == nullptr || member->OnScreen() == 0)
        {
            continue;
        }

        MCFriendlyMechIcon* button = TheInterface->MechBar->GetButtonFromID(member->PartId);

        if (button == nullptr || button->Active == 0 || member->GetPilot() == nullptr)
        {
            continue;
        }

        ShowMoverTag(tag, member);
        tag->ShowGuiWindow(1);
    }

    MCGuiObject::Enter();
}

auto MCLanceIcon::Leave() -> void
{
    TheInterface->HideTags();
    MCGuiObject::Leave();
}

auto MCLanceIcon::ShowTest() -> void
{
    if (Group != nullptr && GetNumActiveMovers() > 0)
    {
        ShowGuiWindow(1);
        return;
    }

    ShowGuiWindow(0);
}

auto MCLanceIcon::GetNumActiveMovers() -> int32_t
{
    int32_t count = 0;

    for (int16_t i = 0; i < 0xc; i++)
    {
        MCFriendlyMechIcon* button = TheInterface->MechBar->GetButton(i);

        if (button != nullptr && button->Lance == LanceId && button->Active != 0)
        {
            count++;
        }
    }

    return count;
}

// InterfaceObject

MCInterfaceObject::MCInterfaceObject()
{
    std::fill(std::begin(Keys), std::end(Keys), 0u);
    CurrentCommand = -1;
    MouseObjectType = -1;
    MechBar = nullptr;
    CommandParser = nullptr;
    TacticalMap = nullptr;
    CommandOneShot = 0;
    MouseObject = nullptr;
    std::fill(std::begin(SelectedMechs), std::end(SelectedMechs), 0);
    std::fill(std::begin(SelectedLances), std::end(SelectedLances), nullptr);
    NumSelectedLances = 0;
    NumSelectedMechs = 0;
    MouseDown = 0;
    TacScrollSpeed = 1;
    DragDistance = 10;
    ScrollSpeed = 4;
    ScrollStart = 500;
}

auto MCInterfaceObject::Init() -> int32_t
{
    ScrollDirection = -1;
    TacScrollDirection = -1;

    // The mech bar, along the bottom of the screen.
    MechBar = new MCMechBar;

    if (MechBar == nullptr)
    {
        return 3;
    }

    if (MechBar->Layout.MaxButtons < 0xd)
    {
        MechBar->Layout.MaxButtons = 0xc;
    }

    MechBar->Init(0, 0, 1, 1, nullptr);
    MechBar->Resize(0x280, 0x3d);
    MechBar->ShowGuiWindow(0);
    ScreenWindow->AddChild(MechBar);
    MechBar->MoveTo(0, Application->Height() - MechBar->Height() - 1, 0);
    MechBar->SetDepth(0x4b);

    // iface.fit's parameters.
    auto* ifaceFile = new MCFitIniFile;

    if (ifaceFile == nullptr)
    {
        return 3;
    }

    std::string fileName;
    fileName = GamePath(InterfacePath, "iface", ".fit");
    int32_t result = ifaceFile->Open(fileName);

    if (result != 0)
    {
        return result;
    }

    result = ifaceFile->SeekBlock("Parameters");

    if (result != 0)
    {
        return result;
    }

    if (ifaceFile->ReadIdShort("Drag Distance", DragDistance) != 0)
    {
        DragDistance = SystemDragWidth;
    }

    ifaceFile->ReadIdShort("Scroll Speed", ScrollSpeed);
    ifaceFile->ReadIdShort("Tac Scroll Speed", TacScrollSpeed);
    ifaceFile->ReadIdShort("Scroll Start", ScrollStart);

    if (ifaceFile->ReadIdShort("Shuffle Frames", ShuffleFrames) != 0)
    {
        ShuffleFrames = 0xf;
    }

    ifaceFile->Close();
    delete ifaceFile;

    // The floating tags.
    for (MCFloatHelp*& tag : FloatingTags)
    {
        tag = new MCFloatHelp;
        Assert(tag != nullptr, 0, "Not enough RAM for floating tags");
        Assert(tag->Init(0, 0, 10, 10, nullptr) == 0, 0, "Error initializing floating tags");
        tag->SetBackColor(0xf4);
        ScreenWindow->AddChild(tag);
        tag->SetDepth(0x28);
        tag->ShowGuiWindow(0);
    }

    // The default key bindings.
    for (const MCDefaultKey& binding : DefaultKeys)
    {
        BindKey(Keys[binding.Slot], binding.Code, binding.Modifiers);

        if (binding.Slot == 63)
        {
            RotateKey = 0x38;
            SelectedEnemy = nullptr;
            NumReserveIcons = 0;
        }
    }

    std::fill(std::begin(ReserveIcons), std::end(ReserveIcons), nullptr);
    CursorOffset = 0;
    ForceOrderActive = 0;
    ForceOrderType = -1;
    return 0;
}

auto MCInterfaceObject::Destroy() -> void
{
    if (MechBar != nullptr)
    {
        MechBar->Destroy();
        delete MechBar;
        MechBar = nullptr;
    }

    if (CommandParser != nullptr)
    {
        delete CommandParser;
        CommandParser = nullptr;
    }

    for (MCFloatHelp*& tag : FloatingTags)
    {
        if (tag != nullptr)
        {
            tag->Destroy();
            delete tag;
            tag = nullptr;
        }
    }
}

namespace
{
    /// <summary>Whether <paramref name="object"/> is a mech, vehicle, elemental or plain mover.</summary>
    bool IsMoverClass(const MCBaseObject* object)
    {
        const MCObjectClass objectClass = object->ObjectClass;
        return objectClass == BATTLEMECH || objectClass == GROUNDVEHICLE || objectClass == ELEMENTAL ||
               objectClass == MOVER;
    }

    /// <summary>Whether the tactical map may show <paramref name="object"/>'s data: a revealed non-elemental mover.</summary>
    bool CanShowInfo(MCGameObject* object)
    {
        return IsMoverClass(object) && object->ObjectClass != ELEMENTAL && object->IsRevealed() != 0;
    }

    /// <summary>A bridge (a misc terrain object of kind 5): clicking one moves onto it.</summary>
    bool IsBridge(MCBaseObject* object)
    {
        return object->ObjectClass == MISCTERRAINOBJECT &&
               static_cast<MCMiscTerrainObject*>(object)->TerrainObjectKind == 5;
    }

    /// <summary>The keys that pick a forced order (ctrl, F9-F12; slots 4-7).</summary>
    bool IsForceOrderKey(const MCInterfaceObject* iface, const MCGuiEvent* event, int16_t scanCode, uint32_t key)
    {
        return (event->CtrlKey != 0 && (scanCode == 0x1d || scanCode == 0x11d)) || key == iface->Keys[4] ||
               key == iface->Keys[5] || key == iface->Keys[7] || key == iface->Keys[6];
    }

    /// <summary>
    /// The attack modifiers the aimed-shot and range commands put on an ATTACK_OBJECT order (the cases every
    /// object type's switch shares).
    /// </summary>
    /// <returns>False when <paramref name="command"/> is not one of them.</returns>
    bool SetAttackModifier(MCTacticalOrder& order, int32_t command, bool withLongRange)
    {
        switch (command)
        {
            case 0xb:
            {
                if (!withLongRange)
                {
                    return false;
                }

                order.AttackParams.Range = -1;
                return true;
            }
            case 0xc:
            {
                order.AttackParams.Range = 2;
                return true;
            }
            case 0xd:
            {
                order.AttackParams.Range = 1;
                return true;
            }
            case 0xe:
            {
                order.AttackParams.Range = 0;
                return true;
            }
            case 0xf:
            {
                order.AttackParams.Pursue = 0;
                return true;
            }
            case 0x10:
            {
                order.AttackParams.Type = 3;
                return true;
            }
            case 0x17:
            case 0x18:
            case 0x19:
            case 0x1a:
            case 0x1b:
            case 0x1c:
            case 0x1d:
            case 0x1e:
            {
                static constexpr int32_t aimLocations[8] = {0, 2, 3, 1, 4, 5, 6, 7};
                order.AttackParams.Pursue = 0;
                order.AttackParams.AimLocation = aimLocations[command - 0x17];
                return true;
            }

            default:
                return false;
        }
    }
}

auto MCInterfaceObject::HandleEvent(MCGuiEvent* event) -> void
{
    MCTacticalOrder order;
    order.Init();

    // Mission messages and keys.
    if (event->Type > 8)
    {
        if (event->Type > 0x1403)
        {
            if (event->Type == 0x1404 && Scenario != nullptr)
            {
                Scenario->CreatePartObject(event->Data);
            }

            order.Destroy();
            return;
        }

        if (event->Type == 0x1403)
        {
            if (Scenario != nullptr)
            {
                Scenario->DestroyPartObject(event->Data);
            }

            order.Destroy();
            return;
        }

        if (event->Type != 9)
        {
            if (event->Type == 0x1402)
            {
                RemoveMech(event->Data);
            }

            order.Destroy();
            return;
        }

        // Key down: fold right alt/ctrl onto the left ones, then add the modifiers.
        const int16_t scanCode = event->ScanCode;
        uint32_t key = static_cast<uint32_t>(static_cast<int32_t>(scanCode));

        if (scanCode == 0x138 || scanCode == 0x11d)
        {
            key = (key & (KEY_SHIFT | KEY_CTRL | KEY_ALT)) + static_cast<int16_t>(scanCode - 0x100);
        }

        key &= ~KEY_SHIFT;

        if (event->ShiftKey != 0)
        {
            key += KEY_SHIFT;
        }

        key &= ~KEY_CTRL;

        if (event->CtrlKey != 0)
        {
            key += KEY_CTRL;
        }

        key &= ~KEY_ALT;

        if (event->AltKey != 0)
        {
            key += KEY_ALT;
        }

        if (Scenario != nullptr)
        {
            if (IsForceOrderKey(this, event, scanCode, key))
            {
                if (ForceOrderType == -1 && ForceOrderActive == 0)
                {
                    ForceOrderType = 0;
                }

                ForceOrderActive = 1;

                for (int32_t i = 0; i < NumSelectedMechs; i++)
                {
                    MCFriendlyMechIcon* button = MechBar->GetButtonFromID(SelectedMechs[i]);

                    if (button != nullptr && button->Mover != nullptr && IsMoverClass(button->Mover))
                    {
                        static_cast<MCMover*>(button->Mover)->DrawOrderLines = 1;
                    }
                }
            }

            MCTacticalMap* tacMap = MCTerrain::TerrainTacticalMap;

            if (key == Keys[65])
            {
                ScrollDirection = 0;
            }
            else if (key == Keys[66])
            {
                ScrollDirection = 4;
            }
            else if (key == Keys[67])
            {
                ScrollDirection = 6;
            }
            else if (key == Keys[68])
            {
                ScrollDirection = 2;
            }
            else if (key == Keys[69] || key == Keys[70] || key == Keys[71] || key == Keys[72])
            {
                if (MCTerrain::TerrainTacticalMap != nullptr)
                {
                    MCTerrain::TerrainTacticalMap->HideMe(0);
                    MCTerrain::TerrainTacticalMap->SetDisplayType(TACMAP_MAP);
                    TacScrollDirection = key == Keys[69] ? 0 : key == Keys[70] ? 4 : key == Keys[71] ? 6 : 2;
                }
            }
            else if (key == Keys[52])
            {
                TacMapShown = 1;
            }
            else if (key == Keys[57] || key == Keys[59])
            {
                if (MCTerrain::TerrainTacticalMap != nullptr)
                {
                    MCTerrain::TerrainTacticalMap->HideMe(0);
                    MCTerrain::TerrainTacticalMap->SetDisplayType(TACMAP_MAP);
                    APostMessage(MCTerrain::TerrainTacticalMap, 0x1a);
                }
            }
            else if (key == Keys[58] || key == Keys[60])
            {
                if (MCTerrain::TerrainTacticalMap != nullptr)
                {
                    MCTerrain::TerrainTacticalMap->HideMe(0);
                    MCTerrain::TerrainTacticalMap->SetDisplayType(TACMAP_MAP);
                    APostMessage(MCTerrain::TerrainTacticalMap, 0x1b);
                }
            }
            else if (key == Keys[61])
            {
                TacMapShown = 0;

                if (tacMap != nullptr)
                {
                    tacMap->HideMe(0);
                    MCTerrain::TerrainTacticalMap->SetDisplayType(TACMAP_MAP);
                }
            }
            else if (key == Keys[62])
            {
                TacMapShown = 0;

                if (tacMap != nullptr)
                {
                    tacMap->HideMe(0);
                    MCTerrain::TerrainTacticalMap->SetDisplayType(static_cast<MCTacmapDisplayTypes>(3));
                }
            }
            else if (MPlayer != nullptr && key == Keys[76])
            {
                // The chat line.
                TacMapShown = 0;

                if (tacMap != nullptr && Scenario != nullptr && EventsToMissionResultsScreen == 0 && GameAsked == 0)
                {
                    tacMap->HideMe(0);
                    MCTerrain::TerrainTacticalMap->SetDisplayType(static_cast<MCTacmapDisplayTypes>(3));
                    MCGuiObject* chatInput = MCTerrain::TerrainTacticalMap->ChatWindow->ChatInput;

                    if (Application->TextObject() != chatInput)
                    {
                        Application->SetText(MCTerrain::TerrainTacticalMap->ChatWindow->ChatInput);
                        FirstReturn = 1;
                    }
                }
            }
            else if (key == Keys[64])
            {
                TacMapShown = 0;

                if (tacMap != nullptr)
                {
                    tacMap->HideMe(0);
                    MCTerrain::TerrainTacticalMap->SetDisplayType(TACMAP_MISSION);
                }
            }
            else if (key == Keys[63])
            {
                TacMapShown = 0;

                if (tacMap != nullptr)
                {
                    tacMap->HideMe(0);
                    MCTerrain::TerrainTacticalMap->SetDisplayType(TACMAP_INFO);
                }
            }
            else if (key == Keys[53] || key == Keys[55])
            {
                ZoomIn();
            }
            else if (key == Keys[54] || key == Keys[56])
            {
                ZoomOut();
            }
            else if (key == Keys[2] && Application->GrabbedObject() == nullptr)
            {
                CurrentCommand = 2;
                CommandOneShot = 0;

                if (AnySelected(0) != 0)
                {
                    order.Init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_STOP, 0);

                    if (CommandParser != nullptr)
                    {
                        GetCommandParser()->SendTacOrder(order, 0);
                    }
                }
            }
            else if (key == Keys[21])
            {
                CurrentCommand = 0x15;
                CommandOneShot = 0;

                if (AnySelected(0) != 0)
                {
                    order.Init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_POWERUP, 0);

                    if (CommandParser != nullptr)
                    {
                        GetCommandParser()->SendTacOrder(order, 0);
                    }
                }
            }
            else if (key == Keys[22])
            {
                CurrentCommand = 0x16;
                CommandOneShot = 0;

                if (AnySelected(0) != 0)
                {
                    order.Init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_POWERDOWN, 0);

                    if (CommandParser != nullptr)
                    {
                        GetCommandParser()->SendTacOrder(order, 0);
                    }
                }
            }
            else if (key == Keys[1])
            {
                CurrentCommand = 1;
                CommandOneShot = 0;
            }
            else if (key == Keys[3] || key == Keys[8] || key == Keys[5])
            {
                CurrentCommand = 3;
                CommandOneShot = 0;
            }
            else if (key == Keys[51])
            {
                CurrentCommand = 0x33;
                CommandOneShot = 0;
            }
            else if (key == Keys[11] && AnySelected(1) != 0)
            {
                CurrentCommand = 0xb;
                CommandOneShot = 0;
            }
            else if (key == Keys[12] && AnySelected(1) != 0)
            {
                CurrentCommand = 0xc;
                CommandOneShot = 0;
            }
            else if (key == Keys[13] && AnySelected(1) != 0)
            {
                CurrentCommand = 0xd;
                CommandOneShot = 0;
            }
            else if (key == Keys[16] && AnySelected(1) != 0)
            {
                CurrentCommand = 0x10;
                CommandOneShot = 0;
            }
            else if (key == Keys[14] && AnySelected(1) != 0)
            {
                CurrentCommand = 0xe;
                CommandOneShot = 0;
            }
            else if (key == Keys[15] && AnySelected(1) != 0)
            {
                CurrentCommand = 0xf;
                CommandOneShot = 0;
            }
            else if (key == Keys[17] || key == Keys[18] || key == Keys[7])
            {
                CurrentCommand = 0x11;
                CommandOneShot = 0;
            }
            else if (key == Keys[19])
            {
                CurrentCommand = 0x13;
                CommandOneShot = 0;
            }
            else if (key == Keys[23] && AnySelected(1) != 0)
            {
                CurrentCommand = 0x17;
                CommandOneShot = 0;
            }
            else if (key == Keys[24] && AnySelected(1) != 0)
            {
                CurrentCommand = 0x18;
                CommandOneShot = 0;
            }
            else if (key == Keys[25] && AnySelected(1) != 0)
            {
                CurrentCommand = 0x19;
                CommandOneShot = 0;
            }
            else if (key == Keys[26] && AnySelected(1) != 0)
            {
                CurrentCommand = 0x1a;
                CommandOneShot = 0;
            }
            else if (key == Keys[27] && AnySelected(1) != 0)
            {
                CurrentCommand = 0x1b;
                CommandOneShot = 0;
            }
            else if (key == Keys[28] && AnySelected(1) != 0)
            {
                CurrentCommand = 0x1c;
                CommandOneShot = 0;
            }
            else if (key == Keys[29] && AnySelected(1) != 0)
            {
                CurrentCommand = 0x1d;
                CommandOneShot = 0;
            }
            else if (key == Keys[30] && AnySelected(1) != 0)
            {
                CurrentCommand = 0x1e;
                CommandOneShot = 0;
            }
            else if (key == Keys[45] && AnySelected(0) != 0)
            {
                // Break up the selected movers' lances.
                for (int32_t i = 0; i < NumSelectedMechs; i++)
                {
                    MCFriendlyMechIcon* button = MechBar->GetButtonFromID(SelectedMechs[i]);
                    MCMover* member = nullptr;

                    if (button != nullptr)
                    {
                        member = static_cast<MCMover*>(button->Mover);

                        if (button->IsPoint != 0)
                        {
                            // A point takes its whole lance with it.
                            for (int16_t j = 0; j < 0xc; j++)
                            {
                                MCFriendlyMechIcon* other = MechBar->GetButton(j);

                                if (other != nullptr && other != button && other->Lance == button->Lance)
                                {
                                    other->Lance = 5;
                                }
                            }
                        }

                        button->Lance = 5;
                    }

                    // Port fix: the original read the group of a null mover when the button was missing.
                    if (member != nullptr && member->Group != nullptr)
                    {
                        member->Group->Remove(member);
                    }
                }

                for (int32_t i = 0; i < NumSelectedLances; i++)
                {
                    DeselectLance(SelectedLances[i]);
                }

                MechBar->PlaceButtons(1);
            }
            else if (key == Keys[33] || key == Keys[34] || key == Keys[35] || key == Keys[36])
            {
                // Select a lance.
                const int32_t lance = key == Keys[33] ? 0 : key == Keys[34] ? 1 : key == Keys[35] ? 2 : 3;

                if (HomeCommander->GetGroup(lance)->NumMovers > 0)
                {
                    ClearMechSelection();
                    SelectLance(HomeCommander->GetGroup(lance));
                    CommandParser->AddSubject(HomeCommander->GetGroup(lance), 0);
                    SoundSystem->PlayDigitalSample(0x30, 1, nullptr, 0, 0);
                }
            }
            else if (key == Keys[37] || key == Keys[38] || key == Keys[39] || key == Keys[40])
            {
                // Add a lance to the selection.
                const int32_t lance = key == Keys[37] ? 0 : key == Keys[38] ? 1 : key == Keys[39] ? 2 : 3;

                if (HomeCommander->GetGroup(lance)->NumMovers > 0)
                {
                    SelectLance(HomeCommander->GetGroup(lance));
                    CommandParser->AddSubject(HomeCommander->GetGroup(lance), 1);
                    SoundSystem->PlayDigitalSample(0x30, 1, nullptr, 0, 0);
                }
            }
            else if (key == Keys[74])
            {
                CurrentCommand = 0x4a;
                CommandOneShot = 0;
            }
            else if (MCTerrain::TerrainTacticalMap != nullptr &&
                     (key == Keys[46] || key == Keys[47] || key == Keys[49] || key == Keys[48]))
            {
                // Arm an artillery strike.
                const int32_t button = key == Keys[46] ? 0 : key == Keys[47] ? 1 : key == Keys[49] ? 2 : 3;
                MCTerrain::TerrainTacticalMap->ActivateArtillery(button, 1);
            }
            else if (BunnyStrikesOn != 0 && key == Keys[50])
            {
                CurrentCommand = 0x32;
                CommandOneShot = 0;
            }
            else
            {
                if (key == Keys[73])
                {
                    TogglePalette();
                }

                if ((key == Keys[31] || key == Keys[32]) && AnySelected(0) != 0 && NumSelectedMechs == 1)
                {
                    MCBaseObject* selected = ObjectList->FindObjectFromPart(SelectedMechs[0]);

                    if (selected == nullptr || selected->ObjectClass != BATTLEMECH ||
                        static_cast<MCBattleMech*>(selected)->SecondStepPrinted == 0 ||
                        static_cast<MCBattleMech*>(selected)->FirstStepPrinted < 1)
                    {
                        order.Destroy();
                        return;
                    }

                    CurrentCommand = 0x1f;
                    CommandOneShot = 0;
                }
            }
        }

        // A key other than a lance-link key ends the hidden-cursor state of a lance link.
        if (Application->CursorHidden != 0 && Application->CursorShape == 0xd && CurrentCommand != 0x29 &&
            CurrentCommand != 0x2a && CurrentCommand != 0x2b && CurrentCommand != 0x2c)
        {
            Application->CursorHidden = 0;
        }

        order.Destroy();
        return;
    }

    if (event->Type == 8)
    {
        // Key up.
        const int16_t scanCode = event->ScanCode;
        uint32_t key = static_cast<uint32_t>(static_cast<int32_t>(scanCode));

        if (scanCode == 0x138)
        {
            key = 0x38;
        }

        key &= ~KEY_SHIFT;

        if (event->ShiftKey != 0)
        {
            key += KEY_SHIFT;
        }

        key &= ~KEY_CTRL;

        if (event->CtrlKey != 0)
        {
            key += KEY_CTRL;
        }

        key &= ~KEY_ALT;

        if (event->AltKey != 0)
        {
            key += KEY_ALT;
        }

        if (scanCode == 0x1d || scanCode == 0x11d || key == Keys[4] || key == Keys[5] || key == Keys[7] ||
            key == Keys[6])
        {
            ForceOrderActive = 0;
            ForceOrderType = -1;

            for (int32_t i = 0; i < MechBar->Layout.NumButtons; i++)
            {
                MCFriendlyMechIcon* button = MechBar->GetButton(static_cast<int16_t>(i));

                if (button != nullptr && button->Mover != nullptr && IsMoverClass(button->Mover))
                {
                    static_cast<MCMover*>(button->Mover)->DrawOrderLines = 0;
                }
            }
        }

        if (event->Target == nullptr || event->Target->Parent != MainHolder)
        {
            if (CurrentCommand != 0x29 && CurrentCommand != 0x2a && CurrentCommand != 0x2b && CurrentCommand != 0x2c)
            {
                CurrentCommand = 0;
                CommandOneShot = 0;
            }

            ScrollDirection = -1;
            TacScrollDirection = -1;

            if (scanCode == RotateKey && Eye != nullptr)
            {
                ScrollWait = 0;
            }

            if (MCTerrain::TerrainTacticalMap != nullptr)
            {
                if (static_cast<int16_t>(key) == static_cast<int16_t>(Keys[52]) && TacMapShown != 0)
                {
                    MCTerrain::TerrainTacticalMap->HideMe(MCTerrain::TerrainTacticalMap->IsHidden() == 0);
                }

                if (MCTerrain::TerrainTacticalMap != nullptr && key == Keys[46])
                {
                    MCTerrain::TerrainTacticalMap->ActivateArtillery(0, 0);
                }

                if (MCTerrain::TerrainTacticalMap != nullptr && key == Keys[47])
                {
                    MCTerrain::TerrainTacticalMap->ActivateArtillery(1, 0);
                }

                if (MCTerrain::TerrainTacticalMap != nullptr && key == Keys[49])
                {
                    MCTerrain::TerrainTacticalMap->ActivateArtillery(2, 0);
                }

                if (MCTerrain::TerrainTacticalMap != nullptr && key == Keys[48])
                {
                    MCTerrain::TerrainTacticalMap->ActivateArtillery(3, 0);
                }
            }

            // The lance-link keys (ctrl+F1-F4) wait for a click on the lance's point.
            for (int32_t lance = 0; lance < 4; lance++)
            {
                if (key == Keys[41 + lance] && AnySelected(0) != 0)
                {
                    CurrentCommand = 0x29 + lance;
                    CommandOneShot = 0;
                    Application->SetCurrentCursor(static_cast<MCCursorType>(0xd));
                    Application->CursorHidden = 1;
                }
            }

            if (key == Keys[75] && Scenario != nullptr)
            {
                SelectVisibleMechs();
                order.Destroy();
                return;
            }
        }

        order.Destroy();
        return;
    }

    // The mouse position in the event's window (port: in a view, on its world surface, through the zoom).
    auto windowPoint = [event](MCGuiObject* window) { return MCWindowPoint(window, event->X, event->Y); };

    // The command-mode reset after a lance link, then the usual redraw.
    auto endLanceLink = [&]()
    {
        CurrentCommand = 0;
        CommandOneShot = 0;
        Application->CursorHidden = 0;
        UpdateInterface();
        order.Destroy();
    };

    auto sendOrder = [&](int sortMovers)
    {
        GetCommandParser()->SendTacOrder(order, sortMovers);
        UpdateInterface();
        order.Destroy();
    };

    auto finish = [&]()
    {
        UpdateInterface();
        order.Destroy();
    };

    // Sends the forced order to the selected movers' pilots (and to the server in multiplayer).
    auto queueForcedOrder = [&](MCTacticalOrder& forcedOrder)
    {
        forcedOrder.Pack(nullptr, nullptr);

        if (MPlayer != nullptr && MPlayer->IsServer == 0)
        {
            int32_t moverParts[12];

            for (int32_t i = 0; i < NumSelectedMechs; i++)
            {
                moverParts[i] = 0;
                MCFriendlyMechIcon* button = MechBar->GetButtonFromID(SelectedMechs[i]);

                if (button != nullptr)
                {
                    moverParts[i] = button->Mover->PartId;
                }
            }

            MPlayer->SendPlayerOrder(0, &forcedOrder, 0, NumSelectedMechs, moverParts, 0, nullptr, 1);
        }

        for (int32_t i = 0; i < NumSelectedMechs; i++)
        {
            MCFriendlyMechIcon* button = MechBar->GetButtonFromID(SelectedMechs[i]);

            if (button == nullptr || button->Mover == nullptr || !IsMoverClass(button->Mover))
            {
                continue;
            }

            auto* member = static_cast<MCMover*>(button->Mover);

            if (member->GetPilot() == nullptr)
            {
                continue;
            }

            if (MPlayer != nullptr)
            {
                forcedOrder.Id = 0;
                forcedOrder.SetId(member->GetPilot());
            }

            member->GetPilot()->AddQueuedTacOrder(forcedOrder);
            member->GetPilot()->TacOrderQueueExecuting = 1;
        }
    };

    MCGameObject* clicked = nullptr;
    int32_t clickedPartId = 0;
    int32_t command = 0;
    MCCamera* camera = nullptr;
    MCGuiObject* target = nullptr;

    switch (event->Type)
    {
        case 1:
        {
            // Left button down on the map: remember where, or give the forced order.
            if (event->Target != MainHolder->GetActivePane())
            {
                order.Destroy();
                return;
            }

            const int32_t x = event->X;
            const int32_t y = event->Y;
            MouseDownX = static_cast<float>(x);
            MouseDownY = static_cast<float>(y);

            if (ForceOrderActive == 0)
            {
                MouseDown = 1;
                order.Destroy();
                return;
            }

            if (ForceOrderType == -1)
            {
                SoundSystem->PlayDigitalSample(0x46, 1, nullptr, 0, 0);
                order.Destroy();
                return;
            }

            MCTacticalOrder forcedOrder;
            forcedOrder.Init();
            target = event->Target;
            MCVector2D screenPos = windowPoint(target);
            // Port fix: zeroed; the original left the point uninitialised when the window has no camera.
            MCLocationNode node;
            node.Location = MCVector3D(0.0f, 0.0f, 0.0f);

            if (target != nullptr && target->GetCamera() != nullptr)
            {
                target->GetCamera()->InverseProject(screenPos, node.Location);
            }

            node.Next = nullptr;
            node.Run = ForceOrderType == 1;

            if (ForceOrderType == 2)
            {
                forcedOrder.Init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_JUMPTO_POINT, 0);
                forcedOrder.InitWayPath(&node);
                forcedOrder.MoveParams.Wait = 0;
            }
            else
            {
                forcedOrder.Init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_MOVETO_POINT, 0);
                forcedOrder.InitWayPath(&node);
                forcedOrder.MoveParams.WayPath.Mode[0] = static_cast<uint8_t>(ForceOrderType);
                forcedOrder.MoveParams.Wait = 0;

                if (CurrentCommand == 0x1f)
                {
                    forcedOrder.MoveParams.Mode = 1;
                }
            }

            queueForcedOrder(forcedOrder);
            forcedOrder.Destroy();
            order.Destroy();
            return;
        }

        case 4:
        {
            // Left button up: ends a drag selection, or gives the order the click means.
            if (MouseDown == 0)
            {
                order.Destroy();
                return;
            }

            target = event->Target;
            MouseDown = 0;

            if (target != MainHolder->GetActivePane())
            {
                MainHolder->SetActivePane(target);
                UpdateMouseState(event);
                order.Destroy();
                return;
            }

            if (DragTarget != nullptr)
            {
                Application->CursorHidden = 0;

                if (event->ShiftKey == 0)
                {
                    ClearMechSelection();
                    CommandParser->ClearSubjects();
                    NumSelectedMechs = 0;
                }

                DeselectEnemy();
                Application->Release();
                auto* dragWindow = static_cast<MCViewWindow*>(DragTarget);

                for (int16_t i = 0; i < MechBar->Layout.NumButtons; i++)
                {
                    const int32_t partId = MechBar->GetButton(i)->PartId;
                    MCBaseObject* member = MechBar->GetButton(i)->Mover;

                    if (member == nullptr || member->GetAppearance() == nullptr || DragTarget == nullptr)
                    {
                        continue;
                    }

                    // Port: the box is in the view's own coordinates; the mover's position is mapped there through
                    // the zoom.
                    const MCVector2D screenPos =
                        dragWindow->WorldToWindow(member->GetAppearance()->GetScreenPos(DragTarget->GetCamera()));
                    POINT point;
                    point.x = static_cast<int32_t>(screenPos.X);
                    point.y = static_cast<int32_t>(screenPos.Y);
                    const float* box = dragWindow->SelectionBox;
                    RECT rect;
                    rect.left = static_cast<int32_t>(box[2] <= box[0] ? box[2] : box[0]);
                    rect.right = static_cast<int32_t>(box[2] < box[0] ? box[0] : box[2]);
                    rect.top = static_cast<int32_t>(box[3] <= box[1] ? box[3] : box[1]);
                    rect.bottom = static_cast<int32_t>(box[3] < box[1] ? box[1] : box[3]);

                    if (PtInRect(&rect, point) != 0)
                    {
                        SelectMech(partId);
                        CommandParser->AddSubject(partId, 1);
                    }
                }

                dragWindow->SelectionBox[1] = 0.0f;
                dragWindow->SelectionBox[0] = 0.0f;
                dragWindow->SelectionBox[3] = 0.0f;
                dragWindow->SelectionBox[2] = 0.0f;
                DragTarget = nullptr;

                if (CurrentCommand == 0x29 || CurrentCommand == 0x2a || CurrentCommand == 0x2b ||
                    CurrentCommand == 0x2c)
                {
                    endLanceLink();
                }
                else
                {
                    finish();
                }

                return;
            }

            command = CurrentCommand;

            if (target != nullptr)
            {
                camera = target->GetCamera();
            }

            if (CurrentCommand == 0x32 && camera != nullptr)
            {
                // The debug "bunny" strike: a large strike where the player clicked.
                MCVector2D screenPos = windowPoint(target);
                MCVector3D strikePos;
                camera->InverseProject(screenPos, strikePos);
                HomeCommander->SetNumLargeStrikes(HomeCommander->NumLargeStrikes + 1);
                CallArtillery(HomeCommander->Id, 1, strikePos, 3, 0);

                for (MCArtilleryButton* button : TacticalMap->ArtilleryButtons)
                {
                    button->Draw();
                }

                order.Destroy();
                return;
            }

            clicked = static_cast<MCGameObject*>(MouseObject);

            if (clicked != nullptr)
            {
                clickedPartId = clicked->PartId;

                // Aimed shots only work on mechs; on anything else they are plain attacks.
                if (clicked->ObjectClass != BATTLEMECH && CurrentCommand >= 0x17 && CurrentCommand <= 0x1e)
                {
                    command = 0xb;
                }
            }
            break;
        }

        case 6:
        {
            ClearMechSelection();
            order.Destroy();
            return;
        }
        case 7:
        {
            // Mouse move: track what is under the mouse, and drag out a selection box.
            if (event->Target == MainHolder->GetActivePane())
            {
                if (MouseDown == 0)
                {
                    UpdateMouseState(event);

                    if (SelectedEnemy != nullptr)
                    {
                        SelectedEnemy->SetSelected(0);
                    }

                    if (MouseObjectType == 1)
                    {
                        SelectedEnemy = static_cast<MCGameObject*>(MouseObject);

                        if (SelectedEnemy != nullptr)
                        {
                            SelectedEnemy->SetSelected(1);
                        }
                    }
                }

                if (event->LeftButton != 0 && MouseDown != 0)
                {
                    int32_t x = event->X;
                    int32_t y = event->Y;
                    bool dragging = DragTarget != nullptr;

                    if (!dragging)
                    {
                        // x87: the distance is compared at extended precision.
                        const double dy = static_cast<double>(static_cast<float>(y)) - MouseDownY;
                        const double dx = static_cast<double>(x) - MouseDownX;
                        dragging = static_cast<double>(DragDistance) < std::sqrt(dx * dx + dy * dy);
                    }

                    if (dragging)
                    {
                        Application->SetCurrentCursor(static_cast<MCCursorType>(0));
                        MCGuiObject* dragWindow = DragTarget;
                        Application->CursorHidden = 1;

                        if (DragTarget == nullptr)
                        {
                            DragTarget = event->Target;
                            Application->Grab(DragTarget);
                            dragWindow = DragTarget;
                            static_cast<MCViewWindow*>(DragTarget)->SelectionBox[0] = MouseDownX;
                            static_cast<MCViewWindow*>(dragWindow)->SelectionBox[1] = MouseDownY;
                        }

                        if (x < dragWindow->GlobalX())
                        {
                            x = DragTarget->GlobalX();
                        }

                        if (y < DragTarget->GlobalY())
                        {
                            y = DragTarget->GlobalY();
                        }

                        if (DragTarget->GlobalX() + DragTarget->Width() <= x)
                        {
                            x = DragTarget->GlobalX() - 1 + DragTarget->Width();
                        }

                        if (DragTarget->GlobalY() + DragTarget->Height() <= y)
                        {
                            y = DragTarget->GlobalY() - 1 + DragTarget->Height();
                        }

                        const int32_t boxY = y - DragTarget->GlobalY();
                        const int32_t boxX = x - DragTarget->GlobalX();
                        static_cast<MCViewWindow*>(dragWindow)->SelectionBox[2] = static_cast<float>(boxX);
                        static_cast<MCViewWindow*>(dragWindow)->SelectionBox[3] = static_cast<float>(boxY);
                        order.Destroy();
                        return;
                    }
                }
            }

            order.Destroy();
            return;
        }

        default:
        {
            order.Destroy();
            return;
        }
    }

    // The click (event 4): what it means depends on what is under the mouse and the command mode.
    auto initAttack = [&]()
    {
        order.Init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_ATTACK_OBJECT, 0);
        order.Target = clicked;
        order.AttackParams.Type = 1;
        order.AttackParams.Method = 0;
        order.AttackParams.Range = -4;
        order.AttackParams.Pursue = 1;
    };

    // An order on the clicked object: its code, the way path's run flag and the wait flag.
    auto initObjectOrder = [&](MCTacticalOrderCode code, uint8_t run, int32_t wait)
    {
        order.Init(ORDER_ORIGIN_PLAYER, code, 0);
        order.Target = clicked;
        order.MoveParams.WayPath.Mode[0] = run;
        order.MoveParams.Wait = wait;
    };

    // Command 0x1f: move onto the object with move mode 1.
    auto initMoveMode1 = [&]()
    {
        order.Init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_MOVETO_OBJECT, 0);
        order.Target = clicked;
        order.MoveParams.WayPath.Mode[0] = 0;
        order.MoveParams.Wait = 0;
        order.MoveParams.Mode = 1;
    };

    // Jumps onto the clicked object, when every selected mover can.
    auto jumpToObject = [&]() -> bool
    {
        if (CanSelectionJumpTo(clicked->GetPosition(), clicked, ForceOrderActive) == 0)
        {
            return false;
        }

        order.Init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_JUMPTO_OBJECT, 0);
        order.Target = static_cast<MCGameObject*>(MouseObject);
        order.MoveParams.WayPath.Mode[0] = 2;
        order.MoveParams.Wait = 0;
        order.SetWayPoint(0, static_cast<MCGameObject*>(MouseObject)->GetPosition());
        return true;
    };

    // An order along a one-point way path at <c>mouseWorldPos</c>.
    auto initPointOrder = [&](MCTacticalOrderCode code, int run)
    {
        MCLocationNode node;
        node.Location = MouseWorldPos;
        node.Run = run;
        node.Next = nullptr;
        order.Init(ORDER_ORIGIN_PLAYER, code, 0);
        order.InitWayPath(&node);
        order.MoveParams.Wait = 0;
    };

    // Unprojects the click into <c>mouseWorldPos</c>; false without a camera.
    auto unprojectClick = [&]() -> bool
    {
        MCVector2D screenPos = windowPoint(target);

        if (camera == nullptr)
        {
            return false;
        }

        camera->InverseProject(screenPos, MouseWorldPos);
        return true;
    };

    auto showInfo = [&]()
    {
        MCTerrain::TerrainTacticalMap->HideMe(0);
        MCTerrain::TerrainTacticalMap->SetDisplayType(TACMAP_INFO);
        MCTerrain::TerrainTacticalMap->SetID(clickedPartId);
        finish();
    };

    auto followClicked = [&]()
    {
        if (camera == nullptr)
        {
            finish();
            return;
        }

        camera->ChangeTarget(clicked, 0);
        finish();
    };

    auto selectEnemyOnMap = [&](MCGameObject* enemy)
    {
        if (SelectedEnemy != nullptr)
        {
            SelectedEnemy->SetSelected(0);
        }

        SelectedEnemy = enemy;

        if (enemy != nullptr)
        {
            enemy->SetSelected(1);
        }

        MCTerrain::TerrainTacticalMap->SetID(enemy->PartId);
    };

    switch (MouseObjectType)
    {
        case 0:
        {
            // One of the player's movers.
            initAttack();

            if (SetAttackModifier(order, command, true))
            {
                sendOrder(0);
                return;
            }

            auto* member = static_cast<MCMover*>(clicked);

            switch (command)
            {
                case 1:
                case 2:
                case 0x15:
                case 0x16:
                {
                    // Eject, stop, power up and down go to the mover alone.
                    if (command == 1)
                    {
                        order.Init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_EJECT, 0);
                    }
                    else if (command == 2)
                    {
                        order.Init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_STOP, 0);
                    }
                    else if (command == 0x15)
                    {
                        order.Init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_POWERUP, 1);
                    }
                    else
                    {
                        order.Init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_POWERDOWN, 1);
                    }

                    if (MPlayer == nullptr || MPlayer->IsServer != 0)
                    {
                        member->HandleTacticalOrder(order, 1, 0);
                        finish();
                        return;
                    }

                    int32_t partId = member->PartId;

                    if (command == 2)
                    {
                        member->GetPilot()->ClearTacOrderQueue();
                    }

                    MPlayer->SendPlayerOrder(0, &order, 0, 1, &partId, 0, nullptr, 0);
                    finish();
                    return;
                }

                case 3:
                {
                    initObjectOrder(TACTICAL_ORDER_MOVETO_OBJECT, 1, 1);
                    sendOrder(1);
                    return;
                }
                case 9:
                {
                    order.Init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_REFIT, 0);
                    order.Target = clicked;
                    order.MoveParams.WayPath.Mode[0] = 1;
                    sendOrder(0);
                    return;
                }
                case 0x11:
                {
                    if (!jumpToObject())
                    {
                        finish();
                        return;
                    }

                    sendOrder(1);
                    return;
                }
                case 0x13:
                {
                    initObjectOrder(TACTICAL_ORDER_GUARD, 0, 1);
                    sendOrder(1);
                    return;
                }
                case 0x1f:
                {
                    initMoveMode1();
                    sendOrder(1);
                    return;
                }
                case 0x29:
                case 0x2a:
                case 0x2b:
                case 0x2c:
                {
                    // Link the selected movers into a lance, the clicked one its point.
                    SoundSystem->PlayBettySample(5);
                    const int32_t lance = CurrentCommand - 0x29;

                    if (IsSelected(clickedPartId) == 0)
                    {
                        SelectMech(clickedPartId);
                    }

                    const int16_t numMovers = NumSelectedMechs;
                    // Port fix: zeroed; the original left the entry of a missing button uninitialised.
                    auto** movers = new MCGameObject*[numMovers]();
                    // The original's point index is left at the command when the point is not found.
                    int32_t pointIndex = command;

                    for (int32_t i = 0; i < NumSelectedMechs; i++)
                    {
                        MCFriendlyMechIcon* button = MechBar->GetButtonFromID(SelectedMechs[i]);

                        if (button == nullptr)
                        {
                            continue;
                        }

                        movers[i] = static_cast<MCGameObject*>(button->Mover);

                        if (button->IsPoint != 0)
                        {
                            button->IsPoint = 0;
                        }

                        if (button->Mover == MouseObject)
                        {
                            button->IsPoint = 1;
                            pointIndex = i;
                        }
                    }

                    for (int32_t i = 0; i < 4; i++)
                    {
                        if (MechBar != nullptr && MechBar->LanceIcons[i] != nullptr &&
                            MechBar->LanceIcons[i]->Group != nullptr)
                        {
                            MechBar->LanceIcons[i]->NumActiveMovers = MechBar->LanceIcons[i]->Group->NumMovers;
                        }
                    }

                    SetUnit(lance, NumSelectedMechs, movers, pointIndex);
                    delete[] movers;
                    MechBar->PlaceButtons(1);
                    CurrentCommand = 0;
                    CommandOneShot = 0;
                    Application->CursorHidden = 0;
                    ClearMechSelection();
                    SelectLance(HomeCommander->GetGroup(lance));
                    CommandParser->AddSubject(HomeCommander->GetGroup(lance), 0);
                    UpdateInterface();
                    order.Destroy();
                    return;
                }

                case 0x33:
                {
                    showInfo();
                    return;
                }
                case 0x4a:
                {
                    followClicked();
                    return;
                }
                default:
                    break;
            }

            // Otherwise the click selects the mover; shift adds it to the selection or takes it out.
            int addToExisting;

            if (event->ShiftKey == 0)
            {
                ClearMechSelection();
                DeselectEnemy();
                CommandParser->ClearSubjects();

                if (AnySelected(0) == 0)
                {
                    MCTerrain::TerrainTacticalMap->SetID(clickedPartId);
                }

                SelectMech(clickedPartId);
                addToExisting = 0;
            }
            else
            {
                if (IsSelected(clickedPartId) != 0)
                {
                    DeselectMech(clickedPartId);
                    CommandParser->RemoveSubject(clickedPartId);
                    finish();
                    return;
                }

                SelectMech(clickedPartId);
                DeselectEnemy();
                addToExisting = 1;
            }

            CommandParser->AddSubject(clickedPartId, addToExisting);
            SoundSystem->PlayDigitalSample(0x10, 1, nullptr, 0, 0);
            finish();
            return;
        }

        case 1:
        case 6:
        {
            // An enemy.
            if (AnySelected(0) == 0)
            {
                // Nothing selected: the click shows the enemy on the tactical map.
                if (!CanShowInfo(clicked))
                {
                    finish();
                    return;
                }

                selectEnemyOnMap(clicked);
                const int32_t mode = CurrentCommand;

                if (mode == 0x33)
                {
                    MCTerrain::TerrainTacticalMap->HideMe(0);
                    MCTerrain::TerrainTacticalMap->SetDisplayType(TACMAP_INFO);
                }

                if (mode == 0x4a && camera != nullptr)
                {
                    camera->ChangeTarget(clicked, 0);
                    finish();
                    return;
                }

                finish();
                return;
            }

            order.Init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_ATTACK_OBJECT, 0);
            clicked = static_cast<MCGameObject*>(MouseObject);
            order.MoveParams.WayPath.Mode[0] = CurrentCommand == 3;
            order.AttackParams.Type = 1;
            order.AttackParams.Pursue = 1;
            order.Target = clicked;
            order.AttackParams.Method = 0;
            order.AttackParams.Range = -4;

            if (SetAttackModifier(order, command, false))
            {
                sendOrder(0);
                return;
            }

            switch (command)
            {
                case 3:
                {
                    initObjectOrder(TACTICAL_ORDER_MOVETO_OBJECT, 1, 1);
                    sendOrder(0);
                    return;
                }
                case 0x13:
                {
                    initObjectOrder(TACTICAL_ORDER_GUARD, 1, 1);
                    sendOrder(1);
                    return;
                }
                case 0x1f:
                {
                    initMoveMode1();
                    sendOrder(1);
                    return;
                }
                case 0x11:
                {
                    if (CanSelectionJumpTo(clicked->GetPosition(), nullptr, ForceOrderActive) == 0)
                    {
                        finish();
                        return;
                    }

                    if (IsMoverClass(MouseObject))
                    {
                        // A jump attack.
                        order.AttackParams.Method = 1;
                        sendOrder(0);
                        return;
                    }

                    if (camera == nullptr)
                    {
                        finish();
                        return;
                    }

                    {
                        MCVector2D screenPos = windowPoint(target);
                        MCLocationNode node;
                        camera->InverseProject(screenPos, node.Location);
                        node.Run = 0;
                        node.Next = nullptr;
                        order.Init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_JUMPTO_POINT, 0);
                        order.InitWayPath(&node);
                        order.MoveParams.Wait = 0;
                    }

                    sendOrder(1);
                    return;
                }
                case 0x29:
                case 0x2a:
                case 0x2b:
                case 0x2c:
                {
                    endLanceLink();
                    return;
                }
                case 0x33:
                {
                    if (!CanShowInfo(clicked))
                    {
                        finish();
                        return;
                    }

                    showInfo();
                    return;
                }
                case 0x4a:
                {
                    followClicked();
                    return;
                }
                default:
                {
                    if (CanCapture != 0)
                    {
                        if (CaptureBlocked != 0)
                        {
                            finish();
                            return;
                        }

                        order.Init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_CAPTURE, 0);
                        order.Target = clicked;
                        order.MoveParams.WayPath.Mode[0] = 1;
                        sendOrder(0);
                        return;
                    }

                    order.MoveParams.Wait = 0;
                    order.MoveParams.WayPath.Mode[0] = command == 3;
                    sendOrder(1);
                    return;
                }
            }
        }

        case 2:
        {
            // An allied mover.
            if (AnySelected(0) == 0)
            {
                selectEnemyOnMap(clicked);
                finish();
                return;
            }

            initAttack();
            order.MoveParams.WayPath.Mode[0] = 0;

            if (!SetAttackModifier(order, command, false))
            {
                switch (command)
                {
                    case 0x11:
                    {
                        if (CanSelectionJumpTo(clicked->GetPosition(), nullptr, ForceOrderActive) != 0)
                        {
                            order.AttackParams.Method = 1;
                        }
                        break;
                    }
                    case 0x1f:
                        initMoveMode1();
                        break;
                    case 0x33:
                    {
                        if (!CanShowInfo(clicked))
                        {
                            finish();
                            return;
                        }

                        showInfo();
                        return;
                    }
                    case 0x4a:
                    {
                        followClicked();
                        return;
                    }
                    case 0x29:
                    case 0x2a:
                    case 0x2b:
                    case 0x2c:
                    {
                        CurrentCommand = 0;
                        CommandOneShot = 0;
                        Application->CursorHidden = 0;
                        [[fallthrough]];
                    }
                    default:
                        initObjectOrder(TACTICAL_ORDER_MOVETO_OBJECT, command == 3, 1);
                        break;
                }
            }

            sendOrder(0);
            return;
        }

        case 3:
        {
            // A neutral object.
            if (AnySelected(0) == 0)
            {
                if (command == 0x33)
                {
                    MCTerrain::TerrainTacticalMap->HideMe(0);
                    MCTerrain::TerrainTacticalMap->SetDisplayType(TACMAP_INFO);
                }
                else if (command == 0x4a)
                {
                    followClicked();
                    return;
                }

                MCTerrain::TerrainTacticalMap->SetID(clicked->PartId);
                finish();
                return;
            }

            initAttack();

            if (SetAttackModifier(order, command, true))
            {
                sendOrder(0);
                return;
            }

            switch (command)
            {
                case 3:
                {
                    initObjectOrder(TACTICAL_ORDER_GUARD, 1, 0);
                    sendOrder(1);
                    return;
                }
                case 0x11:
                {
                    if (!jumpToObject())
                    {
                        finish();
                        return;
                    }

                    sendOrder(1);
                    return;
                }
                case 0x1f:
                {
                    initMoveMode1();
                    sendOrder(1);
                    return;
                }
                case 0x29:
                case 0x2a:
                case 0x2b:
                case 0x2c:
                {
                    endLanceLink();
                    return;
                }
                case 0x33:
                {
                    showInfo();
                    return;
                }
                case 0x4a:
                {
                    followClicked();
                    return;
                }
                default:
                {
                    // Including 0x13: guard it.
                    initObjectOrder(TACTICAL_ORDER_GUARD, 0, 1);
                    sendOrder(1);
                    return;
                }
            }
        }

        case 4:
        {
            // A building of the player's.
            switch (command)
            {
                case 3:
                {
                    initObjectOrder(TACTICAL_ORDER_MOVETO_OBJECT, 1, 0);
                    sendOrder(1);
                    return;
                }
                case 0x1f:
                {
                    initMoveMode1();
                    sendOrder(1);
                    return;
                }
                case 0x29:
                case 0x2a:
                case 0x2b:
                case 0x2c:
                {
                    endLanceLink();
                    return;
                }
                case 0x33:
                {
                    finish();
                    return;
                }
                case 0x4a:
                {
                    followClicked();
                    return;
                }
                default:
                {
                    initObjectOrder(TACTICAL_ORDER_MOVETO_OBJECT, 0, 0);
                    sendOrder(1);
                    return;
                }
            }
        }
        case 5:
        {
            // Any other object.
            initAttack();
            order.MoveParams.WayPath.Mode[0] = 0;

            if (SetAttackModifier(order, command, true))
            {
                sendOrder(0);
                return;
            }

            switch (command)
            {
                case 3:
                {
                    if (!IsBridge(clicked))
                    {
                        initObjectOrder(TACTICAL_ORDER_MOVETO_OBJECT, 1, 0);
                        sendOrder(1);
                        return;
                    }

                    // A bridge: move onto the point clicked (the attack order goes out unchanged without a camera).
                    if (unprojectClick())
                    {
                        initPointOrder(TACTICAL_ORDER_MOVETO_POINT, 1);
                        order.MoveParams.WayPath.Mode[0] = 1;
                    }

                    sendOrder(1);
                    return;
                }
                case 0xa:
                {
                    order.Init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_GETFIXED, 0);
                    order.Target = clicked;
                    order.MoveParams.WayPath.Mode[0] = 1;
                    sendOrder(0);
                    return;
                }
                case 0x11:
                {
                    if (!unprojectClick() || CanSelectionJumpTo(MouseWorldPos, nullptr, ForceOrderActive) == 0)
                    {
                        finish();
                        return;
                    }

                    initPointOrder(TACTICAL_ORDER_JUMPTO_POINT, 0);
                    sendOrder(1);
                    return;
                }
                case 0x13:
                {
                    initObjectOrder(TACTICAL_ORDER_GUARD, 0, 0);
                    sendOrder(1);
                    return;
                }
                case 0x1f:
                {
                    initMoveMode1();
                    sendOrder(1);
                    return;
                }
                case 0x29:
                case 0x2a:
                case 0x2b:
                case 0x2c:
                {
                    endLanceLink();
                    return;
                }
                case 0x33:
                {
                    finish();
                    return;
                }
                case 0x4a:
                {
                    followClicked();
                    return;
                }
                default:
                {
                    if (static_cast<uint8_t>(clicked->Status) == 2 || static_cast<uint8_t>(clicked->Status) == 1)
                    {
                        // Wrecked: move to it.
                        initObjectOrder(TACTICAL_ORDER_MOVETO_OBJECT, 0, 0);
                        sendOrder(0);
                        return;
                    }

                    if (!IsBridge(clicked) && clicked->GetAlignment() != HomeTeam->Alignment)
                    {
                        // Someone else's: attack it.
                        order.AttackParams.Range = -4;
                        sendOrder(0);
                        return;
                    }

                    // The player's own, or a bridge: move to the point clicked.
                    if (!unprojectClick())
                    {
                        finish();
                        return;
                    }

                    initPointOrder(TACTICAL_ORDER_MOVETO_POINT, command == 3);
                    order.MoveParams.WayPath.Mode[0] = command == 3;
                    sendOrder(0);
                    return;
                }
            }
        }

        case 7:
        {
            // The terrain.
            if (!unprojectClick())
            {
                finish();
                return;
            }

            switch (CurrentCommand)
            {
                case 0xb:
                case 0xc:
                case 0xd:
                case 0xe:
                case 0xf:
                case 0x10:
                case 0x17:
                case 0x18:
                case 0x19:
                case 0x1a:
                case 0x1b:
                case 0x1c:
                case 0x1d:
                case 0x1e:
                {
                    // Attack the point.
                    order.Init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_ATTACK_POINT, 0);
                    order.AttackParams.TargetPoint = MouseWorldPos;
                    order.Target = nullptr;
                    order.AttackParams.Type = 1;
                    order.AttackParams.Method = 0;
                    order.AttackParams.Range = -4;
                    order.AttackParams.Pursue = 1;

                    if (command >= 0xb && command <= 0x10)
                    {
                        SetAttackModifier(order, command, true);
                    }

                    sendOrder(1);
                    return;
                }
                default:
                    break;
            }

            initPointOrder(TACTICAL_ORDER_MOVETO_POINT, 0);
            order.MoveParams.WayPath.Mode[0] = 0;

            switch (command)
            {
                case 0:
                {
                    sendOrder(1);
                    return;
                }
                case 3:
                {
                    order.MoveParams.WayPath.Mode[0] = 1;
                    sendOrder(1);
                    return;
                }
                case 0x11:
                {
                    if (CanSelectionJumpTo(MouseWorldPos, nullptr, ForceOrderActive) == 0)
                    {
                        finish();
                        return;
                    }

                    initPointOrder(TACTICAL_ORDER_JUMPTO_POINT, 0);
                    order.MoveParams.WayPath.Mode[0] = 0;
                    sendOrder(1);
                    return;
                }
                case 0x13:
                {
                    initPointOrder(TACTICAL_ORDER_GUARD, 0);
                    order.MoveParams.WayPath.Mode[0] = 0;
                    sendOrder(1);
                    return;
                }
                case 0x1f:
                {
                    order.MoveParams.Mode = 1;
                    sendOrder(1);
                    return;
                }
                case 0x29:
                case 0x2a:
                case 0x2b:
                case 0x2c:
                {
                    endLanceLink();
                    return;
                }
                case 0x4a:
                {
                    // Move the camera to the point.
                    camera->ChangeTarget(nullptr, 0);
                    camera->SetPosition(MouseWorldPos);
                    finish();
                    return;
                }
                default:
                {
                    finish();
                    return;
                }
            }
        }

        default:
        {
            finish();
            return;
        }
    }
}

auto MCInterfaceObject::ZoomIn(float factor, bool sound) -> void
{
    // Port: the view shows fewer lines of the world (the original switched the camera to scale 100); the tactical
    // map's zoom button follows.
    if (Eye == nullptr || Eye->Window == nullptr || GamePaused != 0 || GameAsked != 0)
    {
        return;
    }

    if (Eye->Window->ZoomBy(1.0f / factor))
    {
        if (sound)
        {
            SoundSystem->PlayDigitalSample(0x44, 1, nullptr, 0, 0);
        }

        Eye->ForceUpdate = 1;
        MCTerrain::ForceRedraw = 1;
    }
}

auto MCInterfaceObject::ZoomOut(float factor, bool sound) -> void
{
    // Port: the view shows more lines of the world (the original switched the camera to scale 1); the tactical map's
    // zoom button follows.
    if (Eye == nullptr || Eye->Window == nullptr || GamePaused != 0 || GameAsked != 0)
    {
        return;
    }

    if (Eye->Window->ZoomBy(factor))
    {
        if (sound)
        {
            SoundSystem->PlayDigitalSample(0x45, 1, nullptr, 0, 0);
        }

        Eye->ForceUpdate = 1;
        MCTerrain::ForceRedraw = 1;
    }
}

auto MCInterfaceObject::StartScenario() -> int32_t
{
    CommandParser = new MCParser;

    if (CommandParser == nullptr)
    {
        return 3;
    }

    CommandParser->Init();
    MouseObjectType = -1;
    MouseObject = nullptr;
    CurrentCommand = 0;
    CommandOneShot = 0;
    UpdateInterface();

    if (MainHolder != nullptr)
    {
        MainHolder->ShowGuiWindow(1);
    }

    if (ScrollCallback == nullptr)
    {
        ScrollCallback = new MCGuiCallback;
        ScrollCallback->SetExec(ScrollScreen);
        Application->AddCallback(ScrollCallback);
    }

    SalvageIcon = nullptr;
    SelectedEnemy = nullptr;
    MCTerrain::TerrainTacticalMap->SetDepth(0x50);
    MCMechBar* bar = MechBar;

    if (bar->GetButton(0) != nullptr)
    {
        MCTerrain::TerrainTacticalMap->SetID(bar->GetButton(0)->PartId);
    }

    bar->ShowGuiWindow(1);
    bar->InitLances();
    DragTarget = nullptr;
    ForceOrderActive = 0;
    // Slot 62 (alt+S by default) becomes alt+C in multiplayer.
    BindKey(Keys[62], MPlayer != nullptr ? 0x2e : 0x1f, 0);
    return 0;
}

auto MCInterfaceObject::EndScenario() -> void
{
    ClearMechSelection();
    HighlightedObject = nullptr;

    if (CommandParser != nullptr)
    {
        // The original freed it without its destructor, which does nothing.
        delete CommandParser;
        CommandParser = nullptr;
    }

    MCMechBar* bar = MechBar;

    for (int32_t i = 0; i < 12; i++)
    {
        if (bar != nullptr && i < 4 && bar->LanceIcons[i] != nullptr)
        {
            bar->LanceIcons[i]->Destroy();
        }
    }

    bar->CleanUp();
    bar->ShowGuiWindow(0);
    bar->DestroyLances();
    HideTags();

    if (SalvageIcon != nullptr)
    {
        SalvageIcon->Destroy();
        delete SalvageIcon;
        SalvageIcon = nullptr;
    }

    if (MainHolder != nullptr)
    {
        MainHolder->ShowGuiWindow(0);
    }

    Application->RemoveCallback(ScrollCallback);

    if (ScrollCallback != nullptr)
    {
        delete ScrollCallback;
        ScrollCallback = nullptr;
    }

    NumReserveIcons = 0;

    for (MCFriendlyMechIcon*& icon : ReserveIcons)
    {
        if (icon != nullptr)
        {
            icon->Destroy();
            delete icon;
            icon = nullptr;
        }
    }
}

auto MCInterfaceObject::AddMech(int32_t partId, int32_t lance, int active, int onBar) -> int32_t
{
    auto* icon = new MCFriendlyMechIcon;
    icon->Init(0, 0, 0x34, 0x2e, nullptr);
    icon->SetID(partId);
    icon->Active = active;
    icon->ShowGuiWindow(active);

    if (onBar != 0)
    {
        MCMechBar* bar = MechBar;
        icon->Lance = lance;
        bar->AddButton(icon);
        bar->Draw();
        icon->SetEventRoutine(MechIconHandleEvent);
        return 0;
    }

    icon->Lance = 5;
    ReserveIcons[NumReserveIcons] = icon;
    NumReserveIcons++;
    return 0;
}

auto MCInterfaceObject::ActivateMech(int32_t partId) -> void
{
    MCMechBar* bar = MechBar;
    MCFriendlyMechIcon* icon = bar->GetButtonFromID(partId);

    if (icon != nullptr)
    {
        icon->Active = 1;
        icon->ShowGuiWindow(1);
        icon->SetID(icon->PartId);
        bar->PlaceButtons(1);
    }
}

auto MCInterfaceObject::RemoveMech(int32_t partId) -> void
{
    if (IsOurs(static_cast<int16_t>(partId)) != 0)
    {
        MCMechBar* bar = MechBar;
        MCFriendlyMechIcon* icon = bar->GetButtonFromID(partId);

        if (icon != nullptr)
        {
            if (icon->IsPoint == 0)
            {
                // Drop it from the selection and tell the mover.
                const int16_t count = NumSelectedMechs;
                int16_t index = 0;

                while (index < count && SelectedMechs[index] != partId)
                {
                    index++;
                }

                if (index < count)
                {
                    for (int32_t i = index; i < NumSelectedMechs - 1; i++)
                    {
                        SelectedMechs[i] = SelectedMechs[i + 1];
                    }

                    SelectedMechs[NumSelectedMechs] = 0;
                    NumSelectedMechs--;
                }

                if (ObjectList != nullptr)
                {
                    MCBaseObject* object = ObjectList->FindObjectFromPart(partId);
                    MCObjectEvent deselect;
                    deselect.Init(0x1d, nullptr);

                    if (object != nullptr)
                    {
                        object->HandleEvent(&deselect);
                    }
                }
            }
            else
            {
                // The lance's point died: once no member is left standing, the lance goes from the selection.
                MCLanceIcon* lanceIcon = bar != nullptr ? bar->GetLanceIconFromID(icon->Lance) : nullptr;
                Assert(lanceIcon != nullptr, partId, " InterfaceObject.RemoveMech: NULL lanceIcon ");
                lanceIcon->Linked = 0;
                MCMoverGroup* group = lanceIcon->Group;

                for (int16_t i = 0; i < group->NumMovers; i++)
                {
                    if (group->Movers[i]->IsDisabled() == 0)
                    {
                        goto removeSubject;
                    }
                }

                DeselectLance(group);
                CommandParser->RemoveSubject(group);
            }
        }
    removeSubject:
        if (CommandParser != nullptr)
        {
            CommandParser->RemoveSubject(partId);
        }
    }

    if (SelectedEnemy != nullptr && SelectedEnemy->PartId == partId)
    {
        SelectedEnemy = nullptr;
    }
}

auto MCInterfaceObject::UpdateInterface() -> void
{
    MechBar->Draw();
}

auto MCInterfaceObject::IsSelected(int32_t partId) -> int
{
    for (int16_t i = 0; i < NumSelectedMechs; i++)
    {
        if (SelectedMechs[i] == partId)
        {
            return 1;
        }
    }

    return 0;
}

auto MCInterfaceObject::IsSelected(MCMoverGroup* group) -> int
{
    for (int16_t i = 0; i < NumSelectedLances; i++)
    {
        if (SelectedLances[i] == group)
        {
            return 1;
        }
    }

    return 0;
}

auto MCInterfaceObject::SelectMech(int32_t partId) -> void
{
    MCObjectEvent select;
    const int16_t index = NumSelectedMechs;

    if (index >= 0xc)
    {
        return;
    }

    if (ObjectList != nullptr)
    {
        auto* object = static_cast<MCGameObject*>(ObjectList->FindObjectFromPart(partId));

        // Port fix: the original asks a missing object whether it is disabled.
        if (object != nullptr && object->IsDisabled() != 0)
        {
            return;
        }

        select.Init(0x1c, nullptr);
        select.SelectionIndex = index;

        if (object != nullptr)
        {
            object->HandleEvent(&select);
        }
    }

    SelectedMechs[index] = partId;
    NumSelectedMechs++;
}

auto MCInterfaceObject::SelectVisibleMechs() -> void
{
    ClearMechSelection();
    MCObjectQueueNode* list = HomeTeam->Alignment == -1 ? ClanMechList : InnerSphereMechList;

    if (list == nullptr)
    {
        return;
    }

    for (MCBaseObject* object = list->Head; object != nullptr; object = object->Next)
    {
        if (!IsMoverClass(object))
        {
            continue;
        }

        auto* mover = static_cast<MCMover*>(object);

        if (mover->NetPlayerId != -1 && mover->GetWindowsVisible() == Turn && mover->IsDisabled() == 0)
        {
            const int32_t partId = mover->PartId;
            SelectMech(partId);
            CommandParser->AddSubject(partId, 1);
        }
    }
}

auto MCInterfaceObject::DeselectMech(int32_t partId) -> void
{
    MCObjectEvent deselect;
    const int16_t count = NumSelectedMechs;
    int16_t index = 0;

    while (index < count && SelectedMechs[index] != partId)
    {
        index++;
    }

    if (index < count)
    {
        for (int32_t i = index; i < NumSelectedMechs - 1; i++)
        {
            SelectedMechs[i] = SelectedMechs[i + 1];
        }

        SelectedMechs[NumSelectedMechs] = 0;
        NumSelectedMechs--;
    }

    // A mover taken out of a selected lance breaks the lance up: its other members stay selected on their own.
    MCMechBar* bar = MechBar;
    MCFriendlyMechIcon* icon = bar->GetButtonFromID(partId);

    if (icon != nullptr && bar != nullptr)
    {
        MCLanceIcon* lanceIcon = bar->GetLanceIconFromID(icon->Lance);

        if (lanceIcon != nullptr)
        {
            MCMoverGroup* group = lanceIcon->Group;

            if (IsSelected(group) != 0)
            {
                DeselectLance(group);
                CommandParser->RemoveSubject(group);

                for (int16_t i = 0; i < group->NumMovers; i++)
                {
                    MCMover* member = group->Movers[i];

                    if (member != icon->Mover)
                    {
                        SelectMech(member->PartId);
                        CommandParser->AddSubject(member->PartId, 1);
                    }
                }
            }
        }
    }

    if (ObjectList != nullptr)
    {
        MCBaseObject* object = ObjectList->FindObjectFromPart(partId);
        deselect.Init(0x1d, nullptr);

        if (object != nullptr)
        {
            object->HandleEvent(&deselect);
        }
    }
}

auto MCInterfaceObject::SelectEnemy(int32_t partId) -> void
{
    MCObjectEvent select;

    if (ObjectList != nullptr)
    {
        MCBaseObject* object = ObjectList->FindObjectFromPart(partId);
        select.Init(0x1c, nullptr);
        select.SelectionIndex = 0;

        if (object != nullptr)
        {
            object->HandleEvent(&select);
        }
    }
}

auto MCInterfaceObject::DeselectEnemy() -> void
{
    MCObjectEvent deselect;

    if (SelectedEnemy != nullptr)
    {
        deselect.Init(0x1d, nullptr);
        SelectedEnemy->HandleEvent(&deselect);
    }
}

auto MCInterfaceObject::SelectLance(MCMoverGroup* group) -> void
{
    MCLanceIcon* lanceIcon = MechBar->GetLanceIconFromID(group->GetId());

    if (lanceIcon->Linked == 0)
    {
        // Not a linked lance: select its movers one by one.
        for (int32_t i = 0; i < group->NumMovers; i++)
        {
            if (group->Movers[i] != nullptr)
            {
                const int32_t partId = group->Movers[i]->PartId;

                if (IsSelected(partId) == 0)
                {
                    SelectMech(partId);
                    CommandParser->AddSubject(partId, 1);
                }
            }
        }
    }
    else if (NumSelectedLances < 4 && IsSelected(group) == 0)
    {
        SelectedLances[NumSelectedLances] = group;
        NumSelectedLances++;

        for (int32_t i = 0; i < group->NumMovers; i++)
        {
            if (group->Movers[i] != nullptr)
            {
                const int32_t partId = group->Movers[i]->PartId;

                if (IsSelected(partId) == 0)
                {
                    SelectMech(partId);
                }
            }
        }
    }
}

auto MCInterfaceObject::DeselectLance(MCMoverGroup* group) -> void
{
    if (group == nullptr)
    {
        return;
    }

    if (IsSelected(group) != 0)
    {
        const int16_t count = NumSelectedLances;
        int32_t index = 0;

        while (index < count && SelectedLances[index] != group)
        {
            index++;
        }

        NumSelectedLances = static_cast<int16_t>(count - 1);

        for (; index < static_cast<int16_t>(count - 1); index++)
        {
            SelectedLances[index] = SelectedLances[index + 1];
        }

        SelectedLances[index] = nullptr;
    }

    for (int32_t i = 0; i < group->NumMovers; i++)
    {
        DeselectMech(group->Movers[i] != nullptr ? group->Movers[i]->PartId : -1);
    }
}

auto MCInterfaceObject::ClearMechSelection() -> void
{
    MCObjectEvent deselect;

    for (int32_t i = 0; i < 0xc; i++)
    {
        MCFriendlyMechIcon* icon = MechBar->GetButton(static_cast<int16_t>(i));

        if (ScenarioCallback != nullptr && icon != nullptr && icon->Mover != nullptr)
        {
            deselect.Init(0x1d, nullptr);
            icon->Mover->HandleEvent(&deselect);
        }

        SelectedMechs[i] = 0;
    }

    NumSelectedMechs = 0;
    NumSelectedLances = 0;

    for (MCMoverGroup*& lance : SelectedLances)
    {
        lance = nullptr;
    }

    CommandParser->ClearSubjects();
}

auto MCInterfaceObject::IsOurs(int16_t partId) -> int
{
    MCMechBar* bar = MechBar;

    for (int16_t i = 0; i < bar->Layout.NumButtons; i++)
    {
        if (bar->GetButton(i) != nullptr && bar->GetButton(i)->PartId == partId)
        {
            return 1;
        }
    }

    return 0;
}

auto MCInterfaceObject::ObjectAttacked(int32_t) -> void
{
}

namespace
{
    /// <summary>
    /// Writes into <paramref name="name"/> the name of the network player whose mover roster holds
    /// <paramref name="object"/>, or an empty string.
    /// </summary>
    void NetPlayerName(const MCBaseObject* object, char* name)
    {
        int32_t player = -1;

        for (int32_t p = 0; p < 6 && player == -1; p++)
        {
            for (int32_t i = 0; i < 12; i++)
            {
                const MCMover* mover = MPlayer->PlayerMoverRoster[p][i];

                if (mover == nullptr)
                {
                    break;
                }

                if (mover == object)
                {
                    player = p;
                    break;
                }
            }
        }

        name[0] = '\0';

        if (player != -1 && MPlayer->SessionManager->GetPlayerNumber(player) != nullptr)
        {
            sprintf(name, "%s", MPlayer->SessionManager->GetPlayerNumber(player)->Name);
        }
    }
}

auto MCInterfaceObject::UpdateMouseState(MCGuiEvent* event) -> void
{
    MCFloatHelp* tag = FloatingTags[0];
    MCObjectEvent objectEvent;
    MCGuiEvent cursorEvent;
    char text[256];
    // Port fix: cleared. For a misc terrain object whose kind has no tag string (not 5-9) the original formats the tag from this buffer
    // without loading anything into it.
    char format[256] = {};
    CursorOffset = 0;

    if (ScreenWindow == nullptr || ObjectList == nullptr)
    {
        return;
    }

    if (event == nullptr)
    {
        // The per-frame call: a mouse event at the cursor, over the window under it (the map, not the mech bar).
        const MCPoint cursor = MCInput::GetCursorPos();
        cursorEvent.Clear();
        cursorEvent.X = cursor.x;
        cursorEvent.Y = cursor.y;
        cursorEvent.Target = ScreenWindow->FindObject(cursor.x, cursor.y);

        if (cursorEvent.Target == MechBar)
        {
            cursorEvent.Target = MainHolder->GetActivePane();
        }

        if (cursorEvent.Target == nullptr)
        {
            return;
        }

        event = &cursorEvent;
    }

    MCGuiObject* window = event->Target;

    if (window->ObjectType == 7)
    {
        return;
    }

    if (window == nullptr || window->Parent != MechBar)
    {
        HideTags();
        MechBar->Layout.HighlightId = -1;
    }

    // The floating tag, in the colours the object calls for.
    auto showTag = [&](uint8_t backColor, uint8_t textColor)
    {
        tag->SetBackColor(backColor);
        tag->TextColor = textColor;
        tag->SetHelpText(text);
        tag->ShowGuiWindow(1);
    };

    // What is under the mouse.
    if (window->GetCamera() == nullptr)
    {
        if (window->ObjectType == 8)
        {
            // A mech icon: its mover.
            MouseObjectType = 0;
            MouseObject = static_cast<MCMechIcon*>(window)->Mover;
        }
    }
    else
    {
        objectEvent.Init(0, event);
        auto* object = static_cast<MCGameObject*>(ObjectList->FindObjectFromEvent(&objectEvent));
        MouseObject = object;

        if (object == nullptr)
        {
            MouseObjectType = 7;
        }
        else
        {
            int tagged = 0;
            const int32_t contactType = object->GetContactType(HomeTeam->Id, tagged);

            if (tagged == 0 && contactType == 2)
            {
                MouseObjectType = 4;
            }
            else if (HomeTeam->LineOfSight(object->GetPosition()) == 0)
            {
                MouseObjectType = 7;
            }
            else
            {
                const auto isScrap = [object]()
                {
                    return object->ObjectClass == MISCTERRAINOBJECT &&
                           static_cast<MCMiscTerrainObject*>(object)->TerrainObjectKind == 5;
                };

                switch (static_cast<int32_t>(object->ObjectClass))
                {
                    case BATTLEMECH:
                    case GROUNDVEHICLE:
                    {
                        auto* mover = static_cast<MCMover*>(object);
                        tag->HelpObject = object;

                        if (mover->NetPlayerId >= 0 && mover->IsCaptureable() == 0)
                        {
                            // The player's own: pilot and mover, and the mech bar highlights its icon.
                            if (mover->GetAwake() != 0)
                            {
                                if (MPlayer != nullptr)
                                {
                                    NetPlayerName(object, format);
                                    sprintf(text, "%s\n%s\n%s", mover->GetPilot()->Callsign, mover->GetIfaceName(),
                                            format);
                                }
                                else
                                {
                                    sprintf(text, "%s\n%s", mover->GetPilot()->Callsign, mover->GetIfaceName());
                                }
                            }
                            else
                            {
                                CLoadString(ThisInstance, mover->IsCaptureable() != 0 ? 0x99 : 0x9a, format, 0xfe);
                                sprintf(text, format, mover->GetIfaceName());
                            }

                            MouseObjectType = 0;
                            showTag(0, 0xb);
                            MechBar->Layout.HighlightId = object->PartId;
                            MechBar->Draw();
                            break;
                        }

                        if ((mover->IsCaptured() != 0 && mover->GetAlignment() == HomeTeam->Alignment) ||
                            AlliedTeam == mover->GetTeam())
                        {
                            // Captured by the player, or an ally.
                            sprintf(text, "%s", mover->GetIfaceName());
                            MouseObjectType = 3;
                            showTag(0x1f, 0xc);
                            break;
                        }

                        if (MPlayer != nullptr && mover->GetAlignment() == HomeTeam->Alignment)
                        {
                            // A teammate's mover.
                            NetPlayerName(object, format);
                            sprintf(text, "%s\n%s\n%s", mover->GetPilot()->Callsign, mover->GetIfaceName(), format);
                            MouseObjectType = 3;
                            showTag(0, 0xb);
                            break;
                        }

                        if (contactType != 1)
                        {
                            break;
                        }

                        // An enemy in sight.
                        if (mover->IsDisabled() != 0)
                        {
                            MouseObjectType = 2;
                        }
                        else if (mover->IsDestroyed() == 0)
                        {
                            MouseObjectType = 1;
                        }

                        if (mover->IsCaptureable() != 0)
                        {
                            CLoadString(ThisInstance, 0x99, format, 0xfe);
                            sprintf(text, format, mover->GetIfaceName());
                        }
                        else if (mover->GetAwake() != 0)
                        {
                            if (MPlayer != nullptr)
                            {
                                NetPlayerName(object, format);
                                sprintf(text, "%s\n%s", mover->GetIfaceName(), format);
                            }
                            else
                            {
                                sprintf(text, "%s", mover->GetIfaceName());
                            }
                        }
                        else
                        {
                            CLoadString(ThisInstance, object->ObjectClass == BATTLEMECH ? 0x9c : 0x9d, format, 0xfe);
                            sprintf(text, format, mover->GetIfaceName());
                        }

                        showTag(0, 0xef);
                        break;
                    }

                    case ARTILLERY:
                    case DEBRIS:
                    case 0x14:
                    {
                        MouseObjectType = 7;
                        MouseObject = nullptr;
                        break;
                    }
                    case BUILDING:
                    case MISCTERRAINOBJECT:
                    case TREEBUILDING:
                    case TURRET:
                    case GATE:
                    {
                        if (object->ObjectClass == BUILDING)
                        {
                            sprintf(text, "%s", static_cast<MCBuilding*>(object)->Name.c_str());
                        }

                        if (object->ObjectClass == TREEBUILDING)
                        {
                            sprintf(text, "%s", static_cast<MCTreeBuilding*>(object)->Name.c_str());
                        }

                        if (object->ObjectClass == TURRET)
                        {
                            sprintf(text, "%s", static_cast<MCTurret*>(object)->Name.c_str());
                        }

                        if (object->ObjectClass == GATE)
                        {
                            sprintf(text, "%s", static_cast<MCGate*>(object)->Name.c_str());
                        }

                        if (object->ObjectClass == MISCTERRAINOBJECT)
                        {
                            uint32_t stringId = 0;

                            switch (static_cast<MCMiscTerrainObject*>(object)->TerrainObjectKind)
                            {
                                case 5:
                                    stringId = 0x9e;
                                    break;
                                case 6:
                                    stringId = 0x9f;
                                    break;
                                case 7:
                                    stringId = 0xa0;
                                    break;
                                case 8:
                                    stringId = 0xa1;
                                    break;
                                case 9:
                                    stringId = 0xa2;
                                    break;
                                default:
                                    break;
                            }

                            if (stringId != 0)
                            {
                                CLoadString(ThisInstance, stringId, format, 0xfe);
                            }

                            sprintf(text, format);
                        }

                        if (HighlightedObject != nullptr)
                        {
                            HighlightedObject->SetSelected(0);
                            HighlightedObject = nullptr;
                        }

                        // A turret shows its tag only while deployed (or fixed).
                        if (object->ObjectClass != TURRET || static_cast<MCTurret*>(object)->WeaponDeployed != 0 ||
                            static_cast<MCTurret*>(object)->FixedTurret != 0)
                        {
                            HighlightedObject = object;
                            object->SetSelected(1);
                            tag->HelpObject = object;

                            if (object->IsCaptured() != 0 && object->GetAlignment() == HomeTeam->Alignment)
                            {
                                showTag(0x1f, 0xc);
                            }
                            else if (object->GetAlignment() == HomeTeam->Alignment)
                            {
                                showTag(0, 0xb);
                            }
                            else if (object->GetAlignment() != HomeTeam->Alignment &&
                                     HomeTeam->LineOfSight(object->GetPosition()) != 0)
                            {
                                showTag(0, 0xef);
                            }
                        }

                        [[fallthrough]];
                    }
                    default:
                    {
                        if (object->GetAlignment() == HomeTeam->Alignment || object->IsDestroyed() != 0 || isScrap())
                        {
                            MouseObjectType = 5;
                        }
                        else
                        {
                            MouseObjectType = 6;
                        }
                        break;
                    }
                    case CAMERADRONE:
                    {
                        MouseObjectType = 5;
                        CLoadString(ThisInstance, 0x96, format, 0xfe);
                        sprintf(text, format);
                        tag->HelpObject = object;

                        if (object->GetAlignment() == HomeTeam->Alignment)
                        {
                            showTag(0, 0xb);
                        }
                        else if (object->GetAlignment() != HomeTeam->Alignment &&
                                 HomeTeam->LineOfSight(object->GetPosition()) != 0)
                        {
                            showTag(0, 0xef);
                        }
                        break;
                    }
                    case TRAINCAR:
                    {
                        tag->HelpObject = object;
                        sprintf(text, "%s", static_cast<MCTrainCar*>(object)->Name.c_str());
                        tag->SetBackColor(0x1f);
                        tag->TextColor = 0xc;

                        if (object->IsCaptured() != 0)
                        {
                            showTag(0x1f, 0xc);
                        }
                        else if (object->GetAlignment() == HomeTeam->Alignment)
                        {
                            showTag(0, 0xb);
                        }
                        else if (object->GetAlignment() != HomeTeam->Alignment &&
                                 HomeTeam->LineOfSight(object->GetPosition()) != 0)
                        {
                            showTag(0, 0xef);
                        }

                        MouseObjectType = object->GetAlignment() == HomeTeam->Alignment ? 3 : 1;
                        break;
                    }
                }
            }
        }
    }

    // Port: in a view, on its world surface (through the zoom).
    MCVector2D mousePos = MCWindowPoint(window, event->X, event->Y);
    auto setCursor = [](int32_t cursor) { Application->SetCurrentCursor(static_cast<MCCursorType>(cursor)); };

    // A forced order (see handleEvent): a move, a move-and-attack (command 3) or a jump to the point.
    if (ForceOrderActive != 0)
    {
        bool allowed = window->GetCamera() != nullptr;

        for (int32_t i = 0; allowed && i < NumSelectedMechs; i++)
        {
            MCBaseObject* object = ObjectList->FindObjectFromPart(SelectedMechs[i]);

            // A mover whose order queue is full takes no more.
            if (IsMoverClass(object) && static_cast<MCMover*>(object)->GetPilot() != nullptr &&
                static_cast<MCMover*>(object)->GetPilot()->GetTacOrderQueue(nullptr) >= 0xf)
            {
                allowed = false;
            }
        }

        if (allowed)
        {
            if (CurrentCommand != 0x1f && CurrentCommand != 3 && CurrentCommand != 0x11 &&
                Application->CursorHidden == 0)
            {
                CurrentCommand = 0;
                CommandOneShot = 0;
            }

            MCVector3D point;

            if (MouseObject != nullptr)
            {
                point = static_cast<MCGameObject*>(MouseObject)->GetPosition();
            }
            else
            {
                window->GetCamera()->InverseProject(mousePos, point);
            }

            const int32_t command = CurrentCommand;

            if (command != 0x11)
            {
                if (GameMap->CellPassable(point) != 0)
                {
                    SetCursorOffset(mousePos);

                    if (CurrentCommand == 3)
                    {
                        setCursor(0x10);
                        ForceOrderType = 1;
                        return;
                    }

                    setCursor(0xf);
                    ForceOrderType = 0;
                    return;
                }

                setCursor(8);
                ForceOrderType = command == 3 ? 1 : 0;
                return;
            }

            if (CanSelectionJumpTo(point, nullptr, ForceOrderActive) != 0)
            {
                SetCursorOffset(mousePos);
                setCursor(0x11);
                ForceOrderType = 2;
                return;
            }
        }

        setCursor(8);
        ForceOrderType = -1;
        return;
    }

    // Refit and repair modes follow what the mouse is over.
    if (CurrentCommand == 9 && RefitCheck(static_cast<MCGameObject*>(MouseObject)) == 0)
    {
        CurrentCommand = 0;
        CommandOneShot = 0;
    }
    else if (CurrentCommand == 0 && RefitCheck(static_cast<MCGameObject*>(MouseObject)) != 0)
    {
        CurrentCommand = 9;
        CommandOneShot = 0;
    }

    if (CurrentCommand == 10 && GetFixedCheck(static_cast<MCGameObject*>(MouseObject)) == 0)
    {
        CurrentCommand = 0;
        CommandOneShot = 0;
    }
    else if (CurrentCommand == 0 && GetFixedCheck(static_cast<MCGameObject*>(MouseObject)) != 0)
    {
        CurrentCommand = 10;
        CommandOneShot = 0;
    }

    // Port: the original asserts that mouseObject and homeTeam are null or readable (" Mouseobject is bad!!! ",
    // " homeTeam is bad!!! "), probing them with Win32's IsBadReadPtr. The port has no memory probe and treats a
    // non-null pointer as readable (as aObject does), so both asserts always pass and are left out.
    auto* object = static_cast<MCGameObject*>(MouseObject);
    CanCapture = 0;
    CaptureBlocked = 0;

    if (object != nullptr && object->IsCaptureable() != 0 && (CurrentCommand == 0 || CurrentCommand == 3) &&
        object->GetAlignment() != HomeTeam->Alignment && HomeTeam->LineOfSight(object->GetPosition()) != 0)
    {
        CanCapture = 1;
        CaptureBlocked = object->GetCaptureBlocker(HomeTeam->Alignment) != nullptr ? 1 : 0;
    }

    if (HighlightedObject != nullptr && (object == nullptr || object->IsBuilding() == 0))
    {
        HighlightedObject->SetSelected(0);
        HighlightedObject = nullptr;
    }

    MCTerrain::TerrainTacticalMap->UpdateOrderPalette();

    if (window->GetCamera() == nullptr && window->ObjectType != 8)
    {
        return;
    }

    // The cursor for the command over what is under the mouse.
    auto rangeCursor = [&](int32_t cursor) { setCursor(AnySelected(1) != 0 ? cursor : 8); };
    auto aimedCursor = [&]()
    {
        if (AnySelected(1) == 0)
        {
            setCursor(8);
        }
        else if (MouseObject != nullptr && MouseObject->ObjectClass == BATTLEMECH)
        {
            setCursor(5);
        }
        else
        {
            setCursor(1);
        }
    };

    auto moveCursor = [&]()
    {
        SetCursorOffset(mousePos);
        setCursor(CurrentCommand == 3 ? 0x10 : 0xf);
    };

    auto jumpCursor = [&](MCVector3D point)
    {
        if (CanSelectionJumpTo(point, nullptr, ForceOrderActive) != 0)
        {
            SetCursorOffset(mousePos);
            setCursor(0x11);
        }
        else
        {
            setCursor(8);
        }
    };

    // The world point under the mouse; false without a camera.
    auto mousePoint = [&](MCVector3D& point)
    {
        MCCamera* camera = window->GetCamera();

        if (camera == nullptr)
        {
            return false;
        }

        camera->InverseProject(mousePos, point);
        return true;
    };

    auto captureCursor = [&]() { setCursor(CaptureBlocked != 0 ? 0xc : 0xb); };
    // Over an ally, or (from the enemy case) over something not revealed: the plain command cursors.
    auto allyCursor = [&]()
    {
        switch (CurrentCommand)
        {
            case 0xb:
            case 0x10:
            {
                rangeCursor(6);
                return;
            }
            case 0xc:
            {
                rangeCursor(2);
                return;
            }
            case 0xd:
            {
                rangeCursor(3);
                return;
            }
            case 0xe:
            {
                rangeCursor(4);
                return;
            }
            case 0xf:
            {
                rangeCursor(5);
                return;
            }
            case 0x11:
            {
                MCVector3D point;

                // Port fix: the original projects through the window's camera without checking there is one.
                if (!mousePoint(point))
                {
                    return;
                }

                jumpCursor(point);
                return;
            }

            case 0x13:
            {
                setCursor(7);
                return;
            }
            case 0x17:
            case 0x18:
            case 0x19:
            case 0x1a:
            case 0x1b:
            case 0x1c:
            case 0x1d:
            case 0x1e:
            {
                aimedCursor();
                return;
            }
            case 0x33:
            {
                setCursor(MouseObjectType == 3 ? 0xe : 0xf);
                return;
            }
            default:
            {
                if (AnySelected(0) == 0)
                {
                    return;
                }

                if (CanCapture != 0)
                {
                    captureCursor();
                    return;
                }

                MCVector3D point;

                if (!mousePoint(point))
                {
                    return;
                }

                if (GameMap->CellPassable(point) != 0)
                {
                    moveCursor();
                }
                else
                {
                    setCursor(8);
                }

                return;
            }
        }
    };

    if (CurrentCommand == 0x33)
    {
        // Command 0x33 wants a revealed mover (not an elemental).
        if (object != nullptr && IsMoverClass(object) && object->ObjectClass != ELEMENTAL && object->IsRevealed() != 0)
        {
            setCursor(0xe);
        }
        else
        {
            setCursor(8);
        }

        return;
    }

    if (AnySelected(0) == 0 || CurrentCommand == 0x4a)
    {
        setCursor(0);
        return;
    }

    switch (MouseObjectType)
    {
        case 0:
        {
            // A mover of the player's (or its icon).
            switch (CurrentCommand)
            {
                case 3:
                {
                    SetCursorOffset(mousePos);
                    setCursor(0x10);
                    return;
                }
                case 9:
                {
                    setCursor(10);
                    return;
                }
                case 0xb:
                case 0x10:
                {
                    rangeCursor(6);
                    return;
                }
                case 0xc:
                {
                    rangeCursor(2);
                    return;
                }
                case 0xd:
                {
                    rangeCursor(3);
                    return;
                }
                case 0xe:
                {
                    rangeCursor(4);
                    return;
                }
                case 0xf:
                {
                    rangeCursor(5);
                    return;
                }
                case 0x11:
                {
                    MCVector3D point;

                    if (!mousePoint(point))
                    {
                        return;
                    }

                    jumpCursor(point);
                    return;
                }

                case 0x13:
                {
                    setCursor(7);
                    return;
                }
                case 0x17:
                case 0x18:
                case 0x19:
                case 0x1a:
                case 0x1b:
                case 0x1c:
                case 0x1d:
                case 0x1e:
                {
                    aimedCursor();
                    return;
                }
                case 0x33:
                {
                    setCursor(0xe);
                    return;
                }
                default:
                {
                    setCursor(0);
                    return;
                }
            }
        }
        case 1:
        case 6:
        {
            // An enemy.
            switch (CurrentCommand)
            {
                case 3:
                {
                    SetCursorOffset(mousePos);
                    setCursor(0x10);
                    return;
                }
                case 0xc:
                {
                    rangeCursor(2);
                    return;
                }
                case 0xd:
                {
                    rangeCursor(3);
                    return;
                }
                case 0xe:
                {
                    rangeCursor(4);
                    return;
                }
                case 0xf:
                {
                    rangeCursor(5);
                    return;
                }
                case 0x10:
                {
                    rangeCursor(6);
                    return;
                }
                case 0x11:
                {
                    if (MouseObject != nullptr)
                    {
                        // Jumping onto a mover attacks it (the attack cursor); onto anything else, to its place.
                        const MCVector3D point = static_cast<MCGameObject*>(MouseObject)->GetPosition();

                        if (IsMoverClass(MouseObject))
                        {
                            setCursor(CanSelectionJumpTo(point, nullptr, ForceOrderActive) != 0 ? 1 : 8);
                            return;
                        }

                        jumpCursor(point);
                        return;
                    }
                    else
                    {
                        MCVector3D point;

                        if (!mousePoint(point))
                        {
                            return;
                        }

                        jumpCursor(point);
                        return;
                    }
                }
                case 0x13:
                {
                    setCursor(7);
                    return;
                }
                case 0x17:
                case 0x18:
                case 0x19:
                case 0x1a:
                case 0x1b:
                case 0x1c:
                case 0x1d:
                case 0x1e:
                {
                    aimedCursor();
                    return;
                }
                case 0x33:
                {
                    setCursor(MouseObjectType == 1 ? 0xe : 0xf);
                    return;
                }
                default:
                {
                    if (CanCapture != 0)
                    {
                        captureCursor();
                    }
                    else
                    {
                        rangeCursor(1);
                    }

                    return;
                }
            }
        }
        case 2:
        {
            // A disabled enemy.
            switch (CurrentCommand)
            {
                case 3:
                {
                    SetCursorOffset(mousePos);
                    setCursor(0x10);
                    return;
                }
                case 0xb:
                case 0x10:
                {
                    rangeCursor(6);
                    return;
                }
                case 0xc:
                {
                    rangeCursor(2);
                    return;
                }
                case 0xd:
                {
                    rangeCursor(3);
                    return;
                }
                case 0xe:
                {
                    rangeCursor(4);
                    return;
                }
                case 0xf:
                {
                    rangeCursor(5);
                    return;
                }
                case 0x17:
                case 0x18:
                case 0x19:
                case 0x1a:
                case 0x1b:
                case 0x1c:
                case 0x1d:
                case 0x1e:
                {
                    aimedCursor();
                    return;
                }
                case 0x33:
                {
                    setCursor(0xe);
                    return;
                }
                default:
                {
                    setCursor(8);
                    return;
                }
            }
        }
        case 3:
        {
            allyCursor();
            return;
        }
        case 4:
        {
            // Terrain (or an unseen contact): a move if the cell is passable.
            MCVector3D point;

            if (!mousePoint(point))
            {
                return;
            }

            if (GameMap->CellPassable(point) != 0)
            {
                moveCursor();
            }
            else
            {
                setCursor(8);
            }

            return;
        }

        case 5:
        case 7:
        {
            // A building or other object (5), or nothing (7).
            if (CurrentCommand == 10)
            {
                setCursor(10);
                return;
            }

            if (AnySelected(0) == 0 || MouseObjectType != 5)
            {
                allyCursor();
                return;
            }

            if (object->IsRevealed() == 0)
            {
                moveCursor();
                return;
            }

            int32_t cursor = 8;

            switch (CurrentCommand)
            {
                case 3:
                {
                    SetCursorOffset(mousePos);
                    cursor = 0x10;
                    break;
                }
                case 0xc:
                    cursor = AnySelected(1) != 0 ? 2 : 8;
                    break;
                case 0xd:
                    cursor = AnySelected(1) != 0 ? 3 : 8;
                    break;
                case 0xe:
                    cursor = AnySelected(1) != 0 ? 4 : 8;
                    break;
                case 0xf:
                    cursor = AnySelected(1) != 0 ? 5 : 8;
                    break;
                case 0x11:
                {
                    if (CanSelectionJumpTo(object->GetPosition(), nullptr, ForceOrderActive) != 0)
                    {
                        SetCursorOffset(mousePos);
                        cursor = 0x11;
                    }
                    break;
                }
                case 0x13:
                    cursor = 7;
                    break;
                default:
                {
                    // Walking onto it: the move cursor for a wreck, scrap or the player's own; else the attack cursor.
                    if (static_cast<uint8_t>(object->Status) == 2 || static_cast<uint8_t>(object->Status) == 1 ||
                        (object->ObjectClass == MISCTERRAINOBJECT &&
                         static_cast<MCMiscTerrainObject*>(object)->TerrainObjectKind == 5) ||
                        object->GetAlignment() == HomeTeam->Alignment)
                    {
                        SetCursorOffset(mousePos);
                        cursor = 0xf;
                    }
                    else
                    {
                        cursor = 1;
                    }
                    break;
                }
            }

            setCursor(cursor);

            if (CanCapture != 0)
            {
                captureCursor();
            }

            return;
        }

        default:
            return;
    }
}

namespace
{
    /// <summary>
    /// The strike types <see cref="MCInterfaceObject::CallStrike"/> takes (artillery object type numbers), each with
    /// the <c>CallArtillery</c> strike type for team 0 and team 1.
    /// </summary>
    struct MCStrikeTypeEntry
    {
        int32_t ObjectType = 0;
        int32_t StrikeType[2]{};
    };

    constexpr MCStrikeTypeEntry StrikeTypes[7] = {
        {0xf9, {0, 4}},  {0xf8, {1, 5}},  {0xfa, {2, 6}},  {0x1fc, {0, 4}},
        {0x1fb, {1, 5}}, {0x1fd, {2, 6}}, {0x204, {3, 7}},
    };

    /// <summary>
    /// Whether <paramref name="mover"/> can jump from where it will be (its last queued order's point when
    /// <paramref name="fromWayPoint"/> and it has queued orders, else where it is) to <paramref name="position"/>.
    /// </summary>
    bool InJumpRange(MCMover* mover, const MCVector3D& position, bool fromWayPoint)
    {
        MCVector3D from;
        const int32_t numQueued = mover->GetPilot()->GetTacOrderQueue(nullptr);

        if (numQueued > 0 && fromWayPoint)
        {
            MCQueuedTacOrder queue[MAX_QUEUED_TACORDERS_PER_WARRIOR];
            mover->GetPilot()->GetTacOrderQueue(queue);
            from = queue[numQueued - 1].Point;
        }
        else
        {
            from = mover->GetPosition();
        }

        // The x87 code keeps dx and dy at full precision and rounds dz to a float.
        const double dx = static_cast<double>(from.X) - position.X;
        const double dy = static_cast<double>(from.Y) - position.Y;
        const float dz = from.Z - position.Z;
        const auto distance = static_cast<float>(std::sqrt(dy * dy + static_cast<double>(dz) * dz + dx * dx));
        return !(mover->GetJumpRange(nullptr, nullptr) < distance);
    }
} // namespace

auto MCInterfaceObject::CallStrike(int strikeType, MCVector3D* position, MCGameObject* target, int forCommander,
                                   int forClans, float delay) -> void
{
    if (strikeType != 0xf8 && strikeType != 0xf9 && strikeType != 0xfa && strikeType != 0x1fb && strikeType != 0x1fc &&
        strikeType != 0x1fd && strikeType != 0x204)
    {
        return;
    }

    MCVector3D targetPosition;

    if (position == nullptr)
    {
        if (target == nullptr)
        {
            return;
        }

        targetPosition = target->GetPosition();
        position = &targetPosition;
    }

    const MCVector3D location = *position;

    int32_t commanderId = 0;
    MCTeam* team = InnerSphereTeam;

    if (forCommander != 0)
    {
        commanderId = HomeCommander->GetId();
        team = HomeCommander->GetTeam();
    }
    else if (forClans != 0)
    {
        if (MPlayer != nullptr)
        {
            Fatal(0, " Iface.CallStrike: Need more info than clanStrike in MPlayer ");
        }

        team = ClanTeam;
        commanderId = 1;
    }

    // Artillery and sensor strikes must be aimed at a point the team can see; the others can go anywhere.
    int32_t artilleryType = 0;

    for (const MCStrikeTypeEntry& entry : StrikeTypes)
    {
        if (entry.ObjectType == strikeType)
        {
            artilleryType = entry.StrikeType[team->Id];
            break;
        }
    }

    const bool needsSight = artilleryType == 0 || artilleryType == 1 || artilleryType == 4 || artilleryType == 5;

    if (needsSight && team->LineOfSight(location) == 0)
    {
        SoundSystem->PlayDigitalSample(0x33, 1, nullptr, 0, 0);
        return;
    }

    const auto seconds = static_cast<int32_t>(delay);

    if (MPlayer != nullptr && MPlayer->IsServer == 0)
    {
        MPlayer->SendPlayerArtillery(MPlayer->ServerID, artilleryType, location, seconds);
        return;
    }

    CallArtillery(commanderId, artilleryType, location, seconds, 0);
}

auto MCInterfaceObject::AddSalvageIcon(MCGameObject* object) -> void
{
    auto* node = new MCSalvageIcon::SalvageNode;

    if (node != nullptr)
    {
        node->Object = object;
        node->Next = SalvageIcon->Objects;
        SalvageIcon->Objects = node;
    }
}

auto MCInterfaceObject::HideTags() -> void
{
    for (MCFloatHelp* tag : FloatingTags)
    {
        tag->ShowGuiWindow(0);
    }
}

auto MCInterfaceObject::WhackTags() -> void
{
    HideTags();

    for (MCFloatHelp* tag : FloatingTags)
    {
        tag->TossBitmaps();
    }
}

auto MCInterfaceObject::CanSelectionJump() -> int
{
    int result = 0;

    for (int32_t i = 0; i < NumSelectedMechs; i++)
    {
        MCBaseObject* object = ObjectList->FindObjectFromPart(SelectedMechs[i]);

        if (!IsMoverClass(object) || static_cast<MCMover*>(object)->CanJump() == 0)
        {
            return 0;
        }

        result = 1;
    }

    return result;
}

auto MCInterfaceObject::CanSelectionJumpTo(MCVector3D position, MCGameObject* target, int fromWayPoint) -> int
{
    int result = 1;

    // Not onto one of the player's own movers.
    if (target != nullptr && IsMoverClass(target) && target->GetTeam() == HomeTeam)
    {
        return 0;
    }

    if (GameMap->CellPassable(position) == 0)
    {
        return 0;
    }

    const int16_t numMovers = NumSelectedMechs;

    for (int32_t i = 0; i < numMovers; i++)
    {
        MCBaseObject* object = ObjectList->FindObjectFromPart(SelectedMechs[i]);

        if (IsMoverClass(object) && static_cast<MCMover*>(object)->GetPilot() != nullptr &&
            !InJumpRange(static_cast<MCMover*>(object), position, fromWayPoint != 0))
        {
            return 0;
        }
    }

    // The lances' movers jump from where they are or their last way point.
    for (int32_t i = 0; i < NumSelectedLances; i++)
    {
        MCMoverGroup* group = SelectedLances[i];

        if (group == nullptr)
        {
            continue;
        }

        for (int32_t j = 0; j < group->NumMovers; j++)
        {
            MCMover* mover = group->Movers[j];

            if (IsMoverClass(mover) && mover->GetPilot() != nullptr && !InJumpRange(mover, position, true))
            {
                result = 0;
                break;
            }
        }
    }

    return result;
}

auto MCInterfaceObject::AnySelected(int needsCommand) -> int
{
    int result = 0;

    if (NumSelectedMechs == 0 && NumSelectedLances == 0)
    {
        return 0;
    }

    if (needsCommand == 0)
    {
        return 1;
    }

    // A live mover with weapons can take an attack command.
    for (int32_t i = 0; i < NumSelectedMechs; i++)
    {
        auto* object = static_cast<MCGameObject*>(ObjectList->FindObjectFromPart(SelectedMechs[i]));

        if (object != nullptr && object->IsDisabled() == 0 && IsMoverClass(object) &&
            static_cast<MCMover*>(object)->NumWeapons != 0)
        {
            return 1;
        }
    }

    for (int32_t i = 0; i < NumSelectedLances; i++)
    {
        MCMoverGroup* group = SelectedLances[i];

        if (group == nullptr)
        {
            continue;
        }

        for (int32_t j = 0; j < group->NumMovers; j++)
        {
            if (group->Movers[j] != nullptr && group->Movers[j]->NumWeapons != 0)
            {
                result = 1;
                break;
            }
        }
    }

    return result;
}

auto MCInterfaceObject::SetUnit(int32_t groupId, int32_t numMovers, MCGameObject** movers, int32_t pointIndex) -> void
{
    auto** moverList = reinterpret_cast<MCMover**>(movers);
    HomeCommander->SetGroup(groupId, numMovers, moverList, pointIndex);

    if (MPlayer != nullptr)
    {
        MPlayer->SendPlayerMoverGroup(MPlayer->AllPlayerGroupID, groupId, numMovers, moverList, pointIndex);
    }

    // Relink every icon to its lance and mark the points.
    for (int32_t i = 0; i < 0xc; i++)
    {
        if (MechBar->GetButton(static_cast<int16_t>(i)) != nullptr)
        {
            MechBar->GetButton(static_cast<int16_t>(i))->Lance = 5;
        }
    }

    for (int32_t lance = 0; lance < 4; lance++)
    {
        MCMoverGroup* group = HomeCommander->GetGroup(lance);

        for (int32_t i = 0; i < group->NumMovers; i++)
        {
            if (group->Movers[i] != nullptr)
            {
                MechBar->GetButtonFromID(group->Movers[i]->PartId)->Lance = lance;
            }
        }

        MCMover* point = group->GetPoint();

        if (point != nullptr)
        {
            MechBar->GetButtonFromID(point->PartId)->IsPoint = 1;
        }

        // Original behaviour: marks the lance icon of groupId each time, not that of the lance just relinked.
        MechBar->GetLanceIconFromID(groupId)->Linked = 1;
    }
}

auto MCInterfaceObject::SetPoint(int32_t partId, int isPoint) -> void
{
    MCMechBar* bar = MechBar;

    if (bar->GetButtonFromID(partId) != nullptr)
    {
        bar->GetButtonFromID(partId)->IsPoint = isPoint;
    }
}

auto MCInterfaceObject::SetCursorOffset(MCVector2D screenPos) -> void
{
    if (MCTerrain::TerrainTacticalMap != nullptr && MCTerrain::TerrainTacticalMap->MouseInside != 0)
    {
        CursorOffset = 6;
    }

    if (NumSelectedMechs == 0)
    {
        CursorOffset = 6;
    }

    // The centre of the selected movers on screen.
    const int32_t count = NumSelectedMechs;
    float sumX = 0.0f;
    float sumY = 0.0f;

    for (int32_t i = 0; i < count; i++)
    {
        auto* object = static_cast<MCGameObject*>(ObjectList->FindObjectFromPart(SelectedMechs[i]));

        if (object != nullptr)
        {
            sumX += object->GetScreenPos(0).X;
            sumY += object->GetScreenPos(0).Y;
        }
    }

    // The x87 code stores centreX as a float and keeps centreY at full precision.
    const float centerX = sumX / static_cast<float>(count);
    const double centerY = static_cast<double>(sumY) / static_cast<double>(count);
    const auto slope = static_cast<float>(std::fabs(static_cast<double>(screenPos.Y) - centerY) /
                                          std::fabs(static_cast<double>(screenPos.X) - centerX));
    int32_t index = 0;

    while (index < 8 && slope > SlopeTest[index])
    {
        index++;
    }

    // Written so that a NaN centre (nothing selected) takes the original's branches.
    if (centerY <= screenPos.Y)
    {
        if (screenPos.X <= centerX)
        {
            CursorOffset = 0x20 - index;

            if (CursorOffset == 0x20)
            {
                CursorOffset = 0;
            }

            return;
        }

        CursorOffset = index + 0x10;
        return;
    }

    if (centerX <= screenPos.X)
    {
        CursorOffset = 0x10 - index;
        return;
    }

    CursorOffset = index;
}

auto MCInterfaceObject::RefitCheck(MCGameObject* target) -> int
{
    int result = 0;

    if (NumSelectedMechs != 1)
    {
        return 0;
    }

    MCBaseObject* object = ObjectList->FindObjectFromPart(SelectedMechs[0]);

    if (object != nullptr && object->ObjectClass == GROUNDVEHICLE)
    {
        auto* vehicle = static_cast<MCMover*>(object);

        if (0.0f < vehicle->GetRefitPoints() && target != nullptr && IsMoverClass(target) &&
            static_cast<MCMover*>(target)->NeedsRefit(vehicle->AmmoTruck) != 0 &&
            static_cast<MCMover*>(target)->NetPlayerId > -1)
        {
            result = 1;
        }
    }

    return result;
}

auto MCInterfaceObject::GetFixedCheck(MCGameObject* target) -> int
{
    int result = 0;

    if (target == nullptr || target->ObjectClass != TREEBUILDING)
    {
        return 0;
    }

    if (!(0.0f < target->GetRefitPoints()))
    {
        return 0;
    }

    if (HomeTeam->Alignment != target->GetAlignment() || NumSelectedMechs != 1)
    {
        return 0;
    }

    MCBaseObject* object = ObjectList->FindObjectFromPart(SelectedMechs[0]);

    if (object == nullptr || !IsMoverClass(object))
    {
        return 0;
    }

    auto* mover = static_cast<MCMover*>(object);
    const int32_t mechBay = static_cast<MCTreeBuilding*>(target)->MechBay;

    if (mover->NeedsRefit(0) != 0 &&
        ((mover->ObjectClass == BATTLEMECH && mechBay != 0) || (mover->ObjectClass == GROUNDVEHICLE && mechBay == 0)))
    {
        MCVector3D bayPosition = target->GetPosition();

        if (mover->DistanceFrom(bayPosition) < 100.0f)
        {
            result = 1;
        }
    }

    return result;
}

auto MCInterfaceObject::GetMechIconFromID(int32_t partId) -> MCFriendlyMechIcon*
{
    MCFriendlyMechIcon* icon = MechBar->GetButtonFromID(partId);

    if (icon != nullptr)
    {
        return icon;
    }

    for (int32_t i = 0; i < NumReserveIcons; i++)
    {
        if (ReserveIcons[i] != nullptr && ReserveIcons[i]->PartId == partId)
        {
            return ReserveIcons[i];
        }
    }

    return nullptr;
}
