#include "stdafx.h"
#include "iface/iface.h"
#include "ai/move.h"
#include "ai/tacordr.h"
#include "appear/appear.h"
#include "camera/camera.h"
#include "color/color.h"
#include "gui/afont.h"
#include "gui/ahelp.h"
#include "gui/aport.h"
#include "gui/atextbox.h"
#include "iface/icallbk.h"
#include "iface/parser.h"
#include "lib/aerror.h"
#include "lib/cident.h"
#include "lib/cvmath.h"
#include "lib/file.h"
#include "lib/heap.h"
#include "lib/inifile.h"
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
#include "sound/soundsys.h"
#include "terrain/terrain.h"
#include "terrain/terrmap.h"
#include "vfx/vfx.h"
#include "vfx/vfxfuncs.h"

uint8_t lanceColorArray[8] = {0xee, 0xe5, 0x0e, 0xb5, 0x12, 0x00, 0x00, 0x00};
int32_t sx = 20;
int32_t sy = -10;
float slopeTest[8] = {0.0984914f, 0.3033467f, 0.5345111f, 0.8206788f, 1.2185035f, 1.8708684f, 3.2965581f, 10.1531706f};
uint8_t danceStep = 0;
int16_t danceFrames = 0;
InterfaceObject* theInterface = nullptr;
aCallback* scrollCallback = nullptr;
aCallback* moveCallback = nullptr;
aObject* dragTarget = nullptr;

namespace
{
    /// <summary>
    /// The port's value for the original's <c>GetSystemMetrics(SM_CXDRAG)</c>, the drag distance used when
    /// <c>iface.fit</c> has none: Windows' default.
    /// </summary>
    constexpr int16_t SystemDragWidth = 4;

    /// <summary>
    /// The building, turret, gate or terrain object the mouse last highlighted (<c>setSelected(1)</c>) in
    /// <see cref="InterfaceObject::UpdateMouseState"/>; unhighlighted when the mouse leaves it.
    /// </summary>
    /// <remarks>DAT_00808048; its name was lost.</remarks>
    GameObject* highlightedObject = nullptr;

    /// <summary>The modifier bits of a key binding (see <see cref="InterfaceObject"/>'s remarks).</summary>
    constexpr uint32_t KEY_SHIFT = 0x10000;
    constexpr uint32_t KEY_CTRL = 0x100000;
    constexpr uint32_t KEY_ALT = 0x1000000;

    /// <summary>A default key binding: the slot in <c>keys</c>, the scan code and the modifiers it needs.</summary>
    struct DefaultKey
    {
        int32_t slot;
        uint32_t code;
        uint32_t modifiers;
    };

    /// <summary>
    /// The bindings <see cref="InterfaceObject::init"/> sets, in its order. Codes are DirectInput scan codes (0x1xx
    /// = extended keys). Slot 20's code is -1 (no key).
    /// </summary>
    constexpr DefaultKey DefaultKeys[] = {
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
    void bindKey(uint32_t& key, uint32_t code, uint32_t modifiers)
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
    uint8_t* mechIconPartTable(uint8_t color)
    {
        const int32_t row = gamePalette->numBitmapHazeLevels;
        uint8_t* fades = gamePalette->fadePalettes;

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
    int16_t armorPercent(float current, uint8_t maximum)
    {
        // A location without armour divides by zero in MCX.EXE: __ftol returns 0x80000000, whose low half is 0.
        if (maximum == 0)
        {
            return 0;
        }

        return static_cast<int16_t>(std::floor(static_cast<double>(current) * 100.0 / static_cast<double>(maximum)));
    }

    /// <summary>A mech bar button's place in <see cref="aMechBar::PlaceButtons"/>'s sort (8 bytes).</summary>
    struct ButtonSortEntry
    {
        int16_t index;
        int32_t key;
    };

    /// <summary>The sort's comparison: by key, ascending.</summary>
    int compareButtons(const void* a, const void* b)
    {
        const auto keyA = static_cast<const ButtonSortEntry*>(a)->key;
        const auto keyB = static_cast<const ButtonSortEntry*>(b)->key;

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
    void showMoverTag(aFloatHelp* tag, GameObject* mover)
    {
        char text[100];
        sprintf(text, "%s\n%s", mover->getPilot()->callsign, static_cast<Mover*>(mover)->getIfaceName());
        tag->helpObject = mover;
        tag->setBackColor(0);
        tag->textColor = 0xb;
        tag->SetHelpText(text);
    }

    /// <summary>The mech bar's lance icon <paramref name="index"/>, or null without a mech bar.</summary>
    LanceIcon* barLanceIcon(int32_t index)
    {
        aMechBar* bar = theInterface->mechBar;
        return bar != nullptr ? bar->lanceIcons[index] : nullptr;
    }
} // namespace

// aMechIcon

auto aMechIcon::init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* bitmapName) -> int32_t
{
    int32_t result = aObject::init(xPos, yPos, width, height, bitmapName);

    if (result != 0)
    {
        return result;
    }

    objectType = 8;
    deadImage = new aPort;

    if (deadImage == nullptr)
    {
        return 3;
    }

    result = deadImage->init(10);

    if (result != 0)
    {
        return result;
    }

    for (int32_t i = 0; i < 8; i++)
    {
        partColor[i] = 0xff;
        partDamaged[i] = 0;
        partDirty[i] = 1;
    }

    setBackColor(0x10);
    VFX_pane_wipe(displayPort->frame(), 0x10);
    diagramX = 0;
    diagramY = 0;
    flashDamage = 0;
    mover = nullptr;
    damageShapes = nullptr;
    lastUpdateTime = MCPort::Milliseconds();
    return 0;
}

auto aMechIcon::destroy() -> void
{
    if (deadImage != nullptr)
    {
        deadImage->destroy();
        delete deadImage;
        deadImage = nullptr;
    }

    guiHeap->free(damageShapes);
    damageShapes = nullptr;
    aObject::destroy();
}

auto aMechIcon::draw() -> void
{
    auto* shown = static_cast<GameObject*>(mover);

    if (shown == nullptr)
    {
        return;
    }

    GetColors();
    DrawParts();

    // Destroyed or disabled: the "destroyed" image over the diagram.
    if (shown->status == 1 || shown->status == 2)
    {
        VFX_pane_copy(deadImage->frame(), 0, 0, displayPort->frame(), diagramX, diagramY, 0xfff);
    }
}

auto aMechIcon::enter() -> void
{
    auto* bar = static_cast<aMechBar*>(parent);
    bar->layout.highlightId = partId;
    bar->draw();
    aObject::enter();
}

auto aMechIcon::leave() -> void
{
    auto* bar = static_cast<aMechBar*>(parent);
    theInterface->floatingTags[0]->ShowGUIWindow(0);

    if (bar != nullptr)
    {
        bar->layout.highlightId = -1;
        bar->draw();
    }

    application->SetCurrentCursor(static_cast<CursorType>(0));
    aObject::leave();
}

auto aMechIcon::DrawParts() -> void
{
    for (int16_t i = 0; i < numParts; i++)
    {
        const uint8_t color = (flashDamage != 0 && partDamaged[i] != 0) ? 0x10 : partColor[i];

        if (color == 0xb)
        {
            AG_shape_draw(port()->frame(), damageShapes, i, diagramX, diagramY);
        }
        else
        {
            AG_shape_lookaside(mechIconPartTable(color));
            AG_shape_translate_draw(port()->frame(), damageShapes, i, diagramX, diagramY);
        }

        if (partDirty[i] != 0)
        {
            partDirty[i] = 0;
        }
    }

    if (mover == nullptr)
    {
        VFX_pane_copy(deadImage->frame(), 0, 0, displayPort->frame(), diagramX, diagramY, 0xfff);
    }
}

auto aMechIcon::GetColors() -> void
{
    auto* shown = static_cast<Mover*>(mover);

    if (shown == nullptr)
    {
        return;
    }

    const int8_t numLocations = shown->numBodyLocations;

    for (int32_t i = 0; i < numLocations; i++)
    {
        const BodyLocation& location = shown->bodyAt(i);
        uint8_t newColor;

        if (location.damageState == 2)
        {
            newColor = 0x19;
        }
        else
        {
            if (static_cast<float>(location.maxInternalStructure) != location.curInternalStructure)
            {
                partDamaged[i] = 1;
            }

            // A mech's torsos (1..3) show the worse of their front and rear armour.
            int16_t percent;

            if (shown->objectClass == BATTLEMECH && i > 0 && i < 4)
            {
                const int16_t front = armorPercent(shown->armor[i].curArmor, shown->armor[i].maxArmor);
                const int16_t rear = armorPercent(shown->armor[i + 7].curArmor, shown->armor[i + 7].maxArmor);
                percent = rear < front ? rear : front;
            }
            else
            {
                percent = armorPercent(shown->armor[i].curArmor, shown->armor[i].maxArmor);
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

        if (partColor[i] != newColor)
        {
            partColor[i] = newColor;
            partDirty[i] = 1;
        }
    }
}

auto aMechIcon::display() -> void
{
    if (showWindow == 0)
    {
        return;
    }

    if (IsHidden() != 0 && hideOffset == 0)
    {
        return;
    }

    if (lastUpdateTime + 500 < MCPort::Milliseconds())
    {
        draw();
        lastUpdateTime = MCPort::Milliseconds();
    }

    aObject::display();
}

auto aMechIcon::SetID(int32_t newPartId) -> void
{
    // A mover, or else a salvage craft among the scenario's objects.
    mover = objectList->findObjectFromPart(newPartId);

    if (mover == nullptr)
    {
        BaseObject* object = nullptr;

        while (scenario->scenarioObjectList->traverse(object) != nullptr)
        {
            if (object->partId == newPartId)
            {
                mover = object;
                break;
            }
        }
    }

    if (mover != nullptr)
    {
        partId = newPartId;
        numParts = static_cast<Mover*>(mover)->numBodyLocations;
    }
}

auto aMechIcon::SetFullUpdate(int fullUpdate) -> void
{
    for (int32_t& dirty : partDirty)
    {
        dirty = fullUpdate;
    }
}

// FriendlyMechIcon

auto FriendlyMechIcon::init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* bitmapName) -> int32_t
{
    showingWoundedPilot = 0;
    showingDeadPilot = 0;
    const int32_t result = aMechIcon::init(xPos, yPos, width, height, bitmapName);

    if (result != 0)
    {
        return result;
    }

    pilotImage = new aPort;
    diagramX = 6;
    diagramY = 0x10;

    for (int32_t i = 0; i < 8; i++)
    {
        partDamaged[i] = 0;
        partDirty[i] = 1;
    }

    lance = 5;
    isPoint = 0;
    return 0;
}

auto FriendlyMechIcon::destroy() -> void
{
    if (pilotImage != nullptr)
    {
        pilotImage->destroy();
        delete pilotImage;
        pilotImage = nullptr;
    }

    aMechIcon::destroy();
}

auto FriendlyMechIcon::enter() -> void
{
    aFloatHelp* tag = theInterface->floatingTags[0];
    auto* bar = static_cast<aMechBar*>(parent);
    auto* shown = static_cast<GameObject*>(mover);
    bar->layout.highlightId = partId;
    bar->draw();

    // The pilot's tag, shown when the mover was seen this turn.
    if (shown != nullptr && active != 0 && shown->getPilot() != nullptr)
    {
        showMoverTag(tag, shown);

        if (shown->getWindowsVisible() == turn)
        {
            tag->ShowGUIWindow(1);
        }
    }

    // The cursor for the current command.
    int32_t cursor;

    if (theInterface->AnySelected(0) == 0)
    {
        cursor = 0;
    }
    else
    {
        switch (theInterface->currentCommand)
        {
            case 0xb:
            case 0x10:
            {
                if (theInterface->AnySelected(1) == 0)
                {
                    aObject::enter();
                    return;
                }

                cursor = 6;
                break;
            }
            case 0xc:
            {
                if (theInterface->AnySelected(1) == 0)
                {
                    aObject::enter();
                    return;
                }

                cursor = 2;
                break;
            }
            case 0xd:
            {
                if (theInterface->AnySelected(1) == 0)
                {
                    aObject::enter();
                    return;
                }

                cursor = 3;
                break;
            }
            case 0xe:
            {
                if (theInterface->AnySelected(1) == 0)
                {
                    aObject::enter();
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
                if (theInterface->AnySelected(1) == 0)
                {
                    aObject::enter();
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

                if (theInterface->numSelectedMechs == 1 && static_cast<Mover*>(shown)->needsRefit(0) != 0)
                {
                    const int32_t selectedId = theInterface->numSelectedMechs < 1 ? -1 : theInterface->selectedMechs[0];
                    auto* selected = static_cast<GameObject*>(objectList->findObjectFromPart(selectedId));

                    if (selected != nullptr && selected->objectClass == GROUNDVEHICLE &&
                        selected->getRefitPoints() > 0.0f)
                    {
                        application->SetCurrentCursor(static_cast<CursorType>(10));
                        theInterface->currentCommand = 9;
                        theInterface->commandOneShot = 0;
                        aObject::enter();
                        return;
                    }
                }
                break;
            }
        }
    }

    application->SetCurrentCursor(static_cast<CursorType>(cursor));
    aObject::enter();
}

auto FriendlyMechIcon::draw() -> void
{
    auto* shown = static_cast<Mover*>(mover);
    DrawWeapon();
    aMechIcon::draw();
    FillBox(2, 2, 0x31, 9, lanceColorArray[lance]);

    // A vehicle with a name shows it instead of its pilot.
    if (shown->objectClass == GROUNDVEHICLE && shown->getIfaceName() != nullptr)
    {
        whiteFont->writeString(displayPort->frame(), 5, 3,
                               reinterpret_cast<uint8_t*>(const_cast<char*>(shown->getIfaceName())), -1);
        return;
    }

    DrawPilot();
}

auto FriendlyMechIcon::display() -> void
{
    if (active != 0)
    {
        aMechIcon::display();
    }
}

auto FriendlyMechIcon::drawBox(uint8_t color, int32_t left, int32_t top, int32_t right, int32_t bottom) -> void
{
    if (theInterface->mechBar->dancing != 0)
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
        right = width() - 1;
    }

    if (bottom == -1)
    {
        bottom = height() - 1;
    }

    VFX_line_draw(frame(), left, top, right, top, LD_DRAW, color);
    VFX_line_draw(frame(), left, top, left, bottom, LD_DRAW, color);
    VFX_line_draw(frame(), left, bottom, right, bottom, LD_DRAW, color);
    VFX_line_draw(frame(), right, top, right, bottom, LD_DRAW, color);
}

auto FriendlyMechIcon::DrawPilot() -> void
{
    auto* shown = static_cast<GameObject*>(mover);

    if (shown == nullptr || active == 0)
    {
        return;
    }

    MechWarrior* pilot = shown->getPilot();

    if (pilot == nullptr)
    {
        return;
    }

    // Wounded (6 or more wounds), then dead or gone: the portrait changes once.
    if (6.0f <= pilot->wounds && showingWoundedPilot == 0)
    {
        pilotImage->init(3);
        showingWoundedPilot = 1;
    }

    const int32_t status = shown->getPilot()->status;

    if ((status == 3 || status == 5 || status == 6) && showingDeadPilot == 0)
    {
        pilotImage->init(4);
        showingDeadPilot = 1;
    }

    // The health bar loses 3 pixels per wound from its right end.
    if (0.0f < shown->getPilot()->wounds)
    {
        const auto left = static_cast<int16_t>(47.0f - shown->getPilot()->wounds * 3.0f);
        FillBox(left, 0xb, 0x30, 0xd, 0x10);
    }

    aPort* target = displayPort;
    VFX_pane_copy(pilotImage->frame(), 0, 0, target->frame(), 0x1c, 0xe, 0xfff);

    if (shown->objectClass == BATTLEMECH && shown->getPilot()->callsign != nullptr)
    {
        whiteFont->writeString(target->frame(), 5, 3, reinterpret_cast<uint8_t*>(shown->getPilot()->callsign), -1);
    }
}

auto FriendlyMechIcon::DrawWeapon() -> void
{
    // The bar runs from x 2 on a mech (beside the portrait), from 0xd otherwise.
    int32_t start = 2;
    auto* shown = static_cast<Mover*>(mover);

    if (shown == nullptr || shown->objectClass != BATTLEMECH)
    {
        FillBox(2, 0xb, 0x2e, 0xc, 0x10);
        start = 0xd;
    }
    else
    {
        FillBox(2, 0xb, 0x1b, 0xc, 0x10);
    }

    if (shown == nullptr)
    {
        return;
    }

    const float effectiveness = shown->getTotalEffectiveness();
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
    aPort* target = displayPort;
    const int32_t end = length + start;
    VFX_line_draw(target->frame(), start, 0xb, start, 0xc, LD_DRAW, color);
    VFX_line_draw(target->frame(), start, 0xb, end, 0xb, LD_DRAW, color);
    VFX_line_draw(target->frame(), start + 1, 0xc, end, 0xc, LD_DRAW, color - 1);
    VFX_line_draw(target->frame(), end, 0xb, end, 0xc, LD_DRAW, color - 1);
}

auto FriendlyMechIcon::SetID(int32_t newPartId) -> void
{
    BaseObject* object = objectList->findObjectFromPart(newPartId);

    if (object == nullptr)
    {
        return;
    }

    const int32_t objectClass = object->objectClass;

    if (objectClass != BATTLEMECH && objectClass != GROUNDVEHICLE && objectClass != ELEMENTAL && objectClass != MOVER)
    {
        return;
    }

    File shapeFile;
    char shapeName[20];

    if (objectClass == BATTLEMECH)
    {
        diagramX = 2;
        diagramY = 0xe;
        sprintf(shapeName, "mi%02i", object->getObjectType()->iconNumber);
    }
    else
    {
        diagramX = 0xe;
        diagramY = 0xe;
        sprintf(shapeName, "vi%i", object->getObjectType()->iconNumber);
    }

    port()->init(const_cast<char*>("guiub00.tga"));

    FullPathFileName shapePath;
    shapePath.init(artPath, shapeName, ".shp");

    if (shapeFile.open(shapePath, READ, 0x32) != 0)
    {
        Fatal(0, "Unable to open damage display shape file");
    }

    if (damageShapes != nullptr)
    {
        guiHeap->free(damageShapes);
        damageShapes = nullptr;
    }

    damageShapes = guiHeap->malloc(shapeFile.getLength());

    if (damageShapes == nullptr)
    {
        shapeFile.close();
        Fatal(0, "Not enough memory for damage display shape file");
    }

    shapeFile.read(static_cast<uint8_t*>(damageShapes), static_cast<int32_t>(shapeFile.getLength()));
    shapeFile.close();

    auto* shown = static_cast<Mover*>(object);
    numParts = shown->numBodyLocations;
    mover = object;
    partId = newPartId;

    if (shown->getPilot() != nullptr && shown->getPilot()->picture != nullptr)
    {
        pilotImage->init(shown->getPilot()->picture);
    }

    isPoint = shown == shown->getPoint() ? 1 : 0;
    draw();
}

// aSalvageIcon

auto aSalvageIcon::display() -> void
{
    for (SalvageNode* node = objects; node != nullptr; node = node->next)
    {
        GameObject* object = node->object;

        if (object->onScreen() == 0)
        {
            continue;
        }

        const vector_2d screenPos = object->getScreenPos(0);

        if (displayPort != nullptr)
        {
            const auto yPos = static_cast<int32_t>(static_cast<float>(sy) + screenPos.y);
            const auto xPos = static_cast<int32_t>(screenPos.x + static_cast<float>(sx));
            displayPort->copyTo(framePane, xPos, yPos, 0);
        }
    }
}

// aMechBar

aMechBar::aMechBar()
{
    // The original sets the layout's fields (see aMechBarLayout's initializers) and clears the button and lance
    // arrays; videoId is left as allocated.
    layout.setSpacing(4, 4);
}

auto aMechBar::init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* bitmapName) -> int32_t
{
    aObject::init(xPos, yPos, width, height, bitmapName);
    // The bar draws on its parent: no bitmap of its own.
    guiHeap->free(port()->bitmap()->buffer);
    port()->bitmap()->buffer = nullptr;
    ShowGUIWindow(0);
    dancing = 0;
    layout.setSpacing(0x34, 0x2e);
    return 0;
}

auto aMechBar::destroy() -> void
{
    cleanUp();
    aObject::destroy();
}

auto aMechBar::display() -> void
{
    if (showWindow == 0)
    {
        return;
    }

    for (int32_t i = 0; i < numChildren; i++)
    {
        childList[i]->display();
    }

    if (dancing == 0)
    {
        // A separator after each lance icon shown, and around each button outside a lance.
        for (LanceIcon* lanceIcon : lanceIcons)
        {
            if (lanceIcon != nullptr && lanceIcon->IsShowing() != 0)
            {
                const int32_t lanceRight = lanceIcon->right();
                VFX_line_draw(frame(), lanceRight - 1, 0xf, lanceRight - 1, height() - 1, LD_DRAW, 0x10);
            }
        }

        for (int16_t i = 0; i < 0xc; i++)
        {
            FriendlyMechIcon* button = getButton(i);

            if (button == nullptr)
            {
                continue;
            }

            button->drawBox(0x35, -1, -1, -1, -1);

            if (button->IsShowing() != 0 && button->lance == 5)
            {
                const int32_t buttonRight = button->right();
                VFX_line_draw(frame(), buttonRight, 0xf, buttonRight, height() - 1, LD_DRAW, 0x10);
                VFX_line_draw(frame(), 0, 0xf, buttonRight, 0xf, LD_DRAW, 0x10);
            }
        }
    }

    // Each button's frame: red on the video pilot, else yellow when selected, else white under the mouse.
    for (int16_t i = 0; i < 0xc; i++)
    {
        FriendlyMechIcon* button = getButton(i);

        if (button == nullptr)
        {
            continue;
        }

        const int32_t buttonWidth = button->width();
        const int32_t buttonHeight = button->height();

        if (layout.videoId == button->partId)
        {
            button->drawBox(0xef, 1, 1, buttonWidth - 2, buttonHeight - 2);
        }
        else if (theInterface->IsSelected(button->partId) != 0)
        {
            button->drawBox(0x1f, 1, 1, buttonWidth - 2, buttonHeight - 2);
        }
        else if (layout.highlightId == button->partId)
        {
            button->drawBox(0xb, 1, 1, buttonWidth - 2, buttonHeight - 2);
        }
    }
}

auto aMechBar::handleEvent(aEvent* event) -> void
{
    // A broadcast (a resolution change): back to the bottom of the screen.
    if (event->type == 0x12)
    {
        moveTo(1, application->height() - height() - 1, 0);
    }

    event->target = mainHolder->GetActivePane();
    theInterface->handleEvent(event);
}

auto aMechBar::resize(int32_t width, int32_t height) -> void
{
    if (width <= 0 || height <= 0)
    {
        return;
    }

    if (width == winWidth && height == winHeight)
    {
        return;
    }

    winWidth = width;
    winHeight = height;
    framePane->x1 = framePane->x0 - 1 + width;
    framePane->y1 = framePane->y0 - 1 + height;
}

auto aMechBar::cleanUp() -> void
{
    for (int16_t i = static_cast<int16_t>(layout.numButtons - 1); i > -1; i--)
    {
        RemoveButton(i);
    }

    for (LanceIcon*& lanceIcon : lanceIcons)
    {
        if (lanceIcon != nullptr)
        {
            lanceIcon->destroy();
            delete lanceIcon;
        }

        lanceIcon = nullptr;
    }
}

auto aMechBar::AddButton(FriendlyMechIcon* button) -> int32_t
{
    if (layout.numButtons >= layout.maxButtons)
    {
        return static_cast<int32_t>(0xEEEE0001u);
    }

    button->moveTo((layout.spacingX + 1) * layout.numButtons + 2, 0xf, 0);
    buttons[layout.numButtons] = button;
    addChild(button);
    layout.numButtons++;
    return 0;
}

auto aMechBar::RemoveButton(int16_t index) -> int32_t
{
    if (index >= layout.numButtons)
    {
        return static_cast<int32_t>(0xEEEE0003u);
    }

    FriendlyMechIcon* button = buttons[index];
    button->destroy();
    delete button;
    layout.numButtons--;

    for (int32_t i = index; i < layout.numButtons; i++)
    {
        buttons[i] = buttons[i + 1];
    }

    buttons[layout.numButtons] = nullptr;
    return 0;
}

auto aMechBar::RemoveButton(uint32_t buttonPartId) -> int32_t
{
    for (int16_t i = 0; i < layout.maxButtons; i++)
    {
        FriendlyMechIcon* button = getButton(i);

        if (button != nullptr && static_cast<uint32_t>(button->partId) == buttonPartId)
        {
            return RemoveButton(i);
        }
    }

    return static_cast<int32_t>(0xEEEE0003u);
}

auto aMechBar::GetButtonFromID(int32_t buttonPartId) -> FriendlyMechIcon*
{
    int16_t i = 0;

    for (; i < layout.numButtons; i++)
    {
        if (getButton(i) != nullptr && getButton(i)->partId == buttonPartId)
        {
            break;
        }
    }

    if (i >= layout.numButtons)
    {
        return nullptr;
    }

    return buttons[i];
}

auto aMechBar::PlaceButtons(int animate) -> void
{
    // Sort the buttons by lance, then by x; a lance's point first; inactive movers last.
    ButtonSortEntry order[12];

    for (int16_t i = 0; i < 0xc; i++)
    {
        FriendlyMechIcon* button = getButton(i);

        if (button == nullptr)
        {
            order[i].index = -1;
            order[i].key = -999999;
            continue;
        }

        order[i].index = i;
        order[i].key = button->x() + button->lance * 10000;

        if (button->isPoint != 0)
        {
            order[i].key -= 1000;
        }

        if (button->active == 0)
        {
            order[i].key += 100000;
        }
    }

    std::qsort(order, 0xc, sizeof(ButtonSortEntry), compareButtons);

    for (LanceIcon* lanceIcon : lanceIcons)
    {
        if (lanceIcon != nullptr)
        {
            lanceIcon->ShowTest();
        }
    }

    // Each lance starts with its icon, one pixel after the previous group; an inactive mover goes off the bar.
    int32_t nextX = 0;
    int32_t lastLance = -1;

    for (const ButtonSortEntry& entry : order)
    {
        FriendlyMechIcon* button = getButton(entry.index);

        if (button == nullptr)
        {
            continue;
        }

        const int32_t buttonLance = button->lance;
        int32_t xPos = nextX;

        if (buttonLance != lastLance || buttonLance == 5)
        {
            if (nextX != 0)
            {
                xPos = nextX + 1;
            }

            lastLance = buttonLance;

            if (buttonLance < 4 && lanceIcons[buttonLance] != nullptr)
            {
                lanceIcons[buttonLance]->moveTo(xPos, 2, 0);
            }

            xPos++;
        }

        button->targetX = xPos;

        if (button->active == 0)
        {
            button->targetX = xPos + 1000;
        }

        button->shuffleStep = (xPos - button->x()) / theInterface->shuffleFrames;
        nextX = xPos - 1 + layout.spacingX;
    }

    if (animate == 0)
    {
        for (int16_t i = 0; i < 0xc; i++)
        {
            FriendlyMechIcon* button = getButton(i);

            if (button != nullptr)
            {
                button->moveTo(button->targetX, button->y(), 0);
            }
        }

        const int32_t linkWidth = layout.spacingX - 1;

        for (LanceIcon* lanceIcon : lanceIcons)
        {
            const int32_t numActive = lanceIcon->getNumActiveMovers();
            lanceIcon->resize(numActive * linkWidth + 3, lanceIcon->height());
        }
    }
    else if (moveCallback == nullptr)
    {
        danceStep = 0;
        danceFrames = 0;
        dancing = 1;
        moveCallback = new aCallback;
        moveCallback->setExec(DancingButtons);
        application->addCallback(moveCallback);
        soundSystem->playDigitalSample(0x42, 1, nullptr, 0, 0);
    }
}

auto aMechBar::InitLances() -> int32_t
{
    for (int16_t i = 0; i < 4; i++)
    {
        lanceIcons[i] = new LanceIcon;
        lanceIcons[i]->init(i);
        addChild(lanceIcons[i]);
    }

    PlaceButtons(0);
    return 0;
}

auto aMechBar::DestroyLances() -> void
{
    for (LanceIcon*& lanceIcon : lanceIcons)
    {
        if (lanceIcon != nullptr)
        {
            lanceIcon->destroy();
            delete lanceIcon;
            lanceIcon = nullptr;
        }
    }
}

auto aMechBar::GetLanceIconFromID(int32_t lanceId) -> LanceIcon*
{
    for (LanceIcon* lanceIcon : lanceIcons)
    {
        if (lanceIcon != nullptr && lanceIcon->lanceId == lanceId)
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
    const int32_t step = barLanceIcon(0)->height() / 3 + 1;
    aMechBar* bar = theInterface->mechBar;

    if (danceFrames == 3)
    {
        danceStep++;
    }

    if (danceFrames == theInterface->shuffleFrames + 3)
    {
        for (int16_t i = 0; i < 0xc; i++)
        {
            FriendlyMechIcon* button = bar->getButton(i);

            if (button != nullptr)
            {
                button->moveTo(button->targetX, button->y(), 0);
            }
        }

        danceStep++;
    }

    if (danceStep == 0)
    {
        for (int32_t i = 0; i < 4; i++)
        {
            LanceIcon* lanceIcon = barLanceIcon(i);
            lanceIcon->moveTo(lanceIcon->x(), lanceIcon->y() + step, 0);
            barLanceIcon(i)->setDepth(-10);
        }

        for (int16_t i = 0; i < 0xc; i++)
        {
            FriendlyMechIcon* button = bar->getButton(i);

            if (button != nullptr && button->targetX < button->x())
            {
                button->moveTo(button->x(), button->y() - step, 0);
                button->setDepth(10);
            }
        }
    }
    else if (danceStep == 1)
    {
        for (int16_t i = 0; i < 0xc; i++)
        {
            FriendlyMechIcon* button = bar->getButton(i);

            if (button != nullptr)
            {
                button->moveTo(button->x() + button->shuffleStep, button->y(), 0);
            }
        }

        for (int32_t i = 0; i < 4; i++)
        {
            barLanceIcon(i)->ShowGUIWindow(0);
        }

        danceFrames++;
        return;
    }
    else if (danceStep == 2)
    {
        for (int32_t i = 0; i < 4; i++)
        {
            LanceIcon* lanceIcon = barLanceIcon(i);

            if (lanceIcon->group != nullptr)
            {
                lanceIcon->numActiveMovers = lanceIcon->getNumActiveMovers();
            }

            lanceIcon->moveTo(lanceIcon->x(), lanceIcon->y() - step, 0);
            lanceIcon->ShowTest();
        }

        for (int16_t i = 0; i < 0xc; i++)
        {
            FriendlyMechIcon* button = bar->getButton(i);

            if (button != nullptr && button->y() != 0xf)
            {
                button->moveTo(button->x(), button->y() + step, 0);
            }
        }

        if (theInterface->shuffleFrames + 6 <= danceFrames)
        {
            // Done: everything at rest, the lance icons sized to their lances.
            for (int16_t i = 0; i < 0xc; i++)
            {
                FriendlyMechIcon* button = bar->getButton(i);

                if (button != nullptr)
                {
                    button->setDepth(0);
                }
            }

            for (int32_t i = 0; i < 4; i++)
            {
                LanceIcon* lanceIcon = barLanceIcon(i);
                lanceIcon->setDepth(0);
                lanceIcon->moveTo(lanceIcon->x(), 2, 0);
                const int32_t numActive = lanceIcon->getNumActiveMovers();
                const int32_t spacing = theInterface->mechBar->layout.spacingX;
                lanceIcon->resize(numActive * (spacing - 1) + 3, lanceIcon->height());
            }

            theInterface->mechBar->dancing = 0;
            application->removeCallback(moveCallback);
            theInterface->mechBar->draw();
            delete moveCallback;
            moveCallback = nullptr;
            soundSystem->playDigitalSample(0x43, 1, nullptr, 0, 0);
            danceFrames++;
            return;
        }
    }

    danceFrames++;
}

// LanceIcon

auto LanceIcon::init(int16_t lanceNumber) -> int32_t
{
    const int32_t result = aObject::init(0, 2, 0x25, 0xd, nullptr);

    if (result != 0)
    {
        return result;
    }

    numberImage = new aPort;
    Assert(numberImage != nullptr, 0, "Not enough memory for Lance icon ports");
    firstLinkImage = new aPort;
    Assert(firstLinkImage != nullptr, 0, "Not enough memory for Lance icon ports");
    shortLinkImage = new aPort;
    Assert(shortLinkImage != nullptr, 0, "Not enough memory for Lance icon ports");
    longLinkImage = new aPort;
    Assert(longLinkImage != nullptr, 0, "Not enough memory for Lance icon ports");
    lastLinkImage = new aPort;
    Assert(lastLinkImage != nullptr, 0, "Not enough memory for Lance icon ports");

    char numberName[16];
    sprintf(numberName, "guiubf%i.tga", lanceNumber + 1);
    Assert(numberImage->init(numberName) == 0, 0, "Can't load lance icon's gold star");
    Assert(firstLinkImage->init(const_cast<char*>("guiub01.tga")) == 0, 0, "Can't load lance icon's first curve");
    Assert(shortLinkImage->init(const_cast<char*>("guiub02.tga")) == 0, 0, "Can't load lance icon's short line");
    Assert(longLinkImage->init(const_cast<char*>("guiub03.tga")) == 0, 0, "Can't load lance icon's long line");
    Assert(lastLinkImage->init(const_cast<char*>("guiub04.tga")) == 0, 0, "Can't load lance icon's second curve");

    group = HomeCommander->getGroup(lanceNumber);
    lanceId = group->getId();
    return result;
}

auto LanceIcon::destroy() -> void
{
    for (aPort** image : {&numberImage, &firstLinkImage, &shortLinkImage, &longLinkImage, &lastLinkImage})
    {
        if (*image != nullptr)
        {
            (*image)->destroy();
            delete *image;
            *image = nullptr;
        }
    }
}

auto LanceIcon::display() -> void
{
    if (showWindow == 0)
    {
        return;
    }

    _pane* pane = framePane;
    numberImage->copyTo(pane, 0, 0, 1);
    const int32_t numberWidth = numberImage->width();
    firstLinkImage->copyTo(pane, numberWidth, 0, 1);
    const int32_t linkStart = numberWidth + firstLinkImage->width();
    shortLinkImage->copyTo(pane, linkStart, 4, 1);
    int32_t xPos = linkStart + shortLinkImage->width();
    int32_t linkEnd = xPos;

    for (int32_t i = 1; i < getNumActiveMovers(); i++)
    {
        longLinkImage->copyTo(framePane, xPos, 4, 1);
        xPos += longLinkImage->width();
        linkEnd += longLinkImage->width();
    }

    pane = framePane;
    lastLinkImage->copyTo(pane, xPos, 4, 1);

    // The lance colour under the links.
    _pane bar = *pane;
    bar.x0 = pane->x0 + linkStart;
    bar.y1 = pane->y0 + 10;
    bar.x1 = pane->x0 - 1 + linkEnd;
    bar.y0 = pane->y0 + 8;
    VFX_pane_wipe(&bar, lanceColorArray[lanceId]);
}

auto LanceIcon::handleEvent(aEvent* event) -> void
{
    TacticalOrder tacOrder;
    tacOrder.init();

    if (event->type == 1)
    {
        application->grab(this);
        tacOrder.destroy();
        return;
    }

    if (event->type != 4 || application->grabbedObject() != this)
    {
        tacOrder.destroy();
        return;
    }

    application->release();

    // The eject, power down and power up commands go to the whole lance.
    const int32_t command = theInterface->currentCommand;

    if (command == 1 || command == 0x15 || command == 0x16)
    {
        TacticalOrderCode code;

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

        tacOrder.init(ORDER_ORIGIN_PLAYER, code, 1);
        group->handleTacticalOrder(tacOrder, 1, nullptr, 0);
        theInterface->UpdateInterface();
        tacOrder.destroy();
        return;
    }

    // Otherwise the click selects the lance; shift adds it to the selection or takes it out.
    if (event->shiftKey == 0)
    {
        theInterface->DeselectEnemy();
        theInterface->ClearMechSelection();
        theInterface->commandParser->ClearSubjects();
        soundSystem->playDigitalSample(0x30, 1, nullptr, 0, 0);
        theInterface->SelectLance(group);
        theInterface->commandParser->AddSubject(group, 0);
        theInterface->UpdateInterface();
        tacOrder.destroy();
        return;
    }

    if (theInterface->IsSelected(group) != 0)
    {
        theInterface->DeselectLance(group);
        theInterface->commandParser->RemoveSubject(group);
        theInterface->UpdateInterface();
        tacOrder.destroy();
        return;
    }

    theInterface->DeselectEnemy();
    soundSystem->playDigitalSample(0x30, 1, nullptr, 0, 0);
    theInterface->SelectLance(group);
    theInterface->commandParser->AddSubject(group, 1);
    theInterface->UpdateInterface();
    tacOrder.destroy();
}

auto LanceIcon::enter() -> void
{
    for (uint32_t i = 0; static_cast<int32_t>(i) < getNumActiveMovers(); i++)
    {
        aFloatHelp* tag = static_cast<uint8_t>(i) < 0xc ? theInterface->floatingTags[static_cast<uint8_t>(i)] : nullptr;
        Mover* member = group->movers[i];

        if (member == nullptr || member->onScreen() == 0)
        {
            continue;
        }

        FriendlyMechIcon* button = theInterface->mechBar->GetButtonFromID(member->partId);

        if (button == nullptr || button->active == 0 || member->getPilot() == nullptr)
        {
            continue;
        }

        showMoverTag(tag, member);
        tag->ShowGUIWindow(1);
    }

    aObject::enter();
}

auto LanceIcon::leave() -> void
{
    theInterface->HideTags();
    aObject::leave();
}

auto LanceIcon::ShowTest() -> void
{
    if (group != nullptr && getNumActiveMovers() > 0)
    {
        ShowGUIWindow(1);
        return;
    }

    ShowGUIWindow(0);
}

auto LanceIcon::getNumActiveMovers() -> int32_t
{
    int32_t count = 0;

    for (int16_t i = 0; i < 0xc; i++)
    {
        FriendlyMechIcon* button = theInterface->mechBar->getButton(i);

        if (button != nullptr && button->lance == lanceId && button->active != 0)
        {
            count++;
        }
    }

    return count;
}

// InterfaceObject

InterfaceObject::InterfaceObject()
{
    std::fill(std::begin(keys), std::end(keys), 0u);
    currentCommand = -1;
    mouseObjectType = -1;
    mechBar = nullptr;
    commandParser = nullptr;
    unknown98 = 0;
    tacticalMap = nullptr;
    commandOneShot = 0;
    unknownA4 = 0;
    mouseObject = nullptr;
    std::fill(std::begin(selectedMechs), std::end(selectedMechs), 0);
    std::fill(std::begin(selectedLances), std::end(selectedLances), nullptr);
    numSelectedLances = 0;
    unknown108 = 1;
    numSelectedMechs = 0;
    mouseDown = 0;
    unknownB0 = 0;
    tacScrollSpeed = 1;
    dragDistance = 10;
    scrollSpeed = 4;
    scrollStart = 500;
}

auto InterfaceObject::operator new(size_t size) noexcept -> void*
{
    return guiHeap->malloc(static_cast<uint32_t>(size));
}

auto InterfaceObject::operator delete(void* ptr) -> void
{
    guiHeap->free(ptr);
}

auto InterfaceObject::init() -> int32_t
{
    scrollDirection = -1;
    tacScrollDirection = -1;

    // The mech bar, along the bottom of the screen.
    mechBar = new aMechBar;

    if (mechBar == nullptr)
    {
        return 3;
    }

    if (mechBar->layout.maxButtons < 0xd)
    {
        mechBar->layout.maxButtons = 0xc;
    }

    mechBar->init(0, 0, 1, 1, nullptr);
    mechBar->resize(0x280, 0x3d);
    mechBar->ShowGUIWindow(0);
    screenWindow->addChild(mechBar);
    mechBar->moveTo(0, application->height() - mechBar->height() - 1, 0);
    mechBar->setDepth(0x4b);

    // iface.fit's parameters.
    auto* ifaceFile = new FitIniFile;

    if (ifaceFile == nullptr)
    {
        return 3;
    }

    FullPathFileName fileName;
    fileName.init(interfacePath, "iface", ".fit");
    int32_t result = ifaceFile->open(fileName, READ, 0x32);

    if (result != 0)
    {
        return result;
    }

    result = ifaceFile->seekBlock("Parameters");

    if (result != 0)
    {
        return result;
    }

    if (ifaceFile->readIdShort("Drag Distance", dragDistance) != 0)
    {
        dragDistance = SystemDragWidth;
    }

    ifaceFile->readIdShort("Scroll Speed", scrollSpeed);
    ifaceFile->readIdShort("Tac Scroll Speed", tacScrollSpeed);
    ifaceFile->readIdShort("Scroll Start", scrollStart);

    if (ifaceFile->readIdShort("Shuffle Frames", shuffleFrames) != 0)
    {
        shuffleFrames = 0xf;
    }

    ifaceFile->close();
    delete ifaceFile;

    // The floating tags.
    for (aFloatHelp*& tag : floatingTags)
    {
        tag = new aFloatHelp;
        Assert(tag != nullptr, 0, "Not enough RAM for floating tags");
        Assert(tag->init(0, 0, 10, 10, nullptr) == 0, 0, "Error initializing floating tags");
        tag->setBackColor(0xf4);
        screenWindow->addChild(tag);
        tag->setDepth(0x28);
        tag->ShowGUIWindow(0);
    }

    // The default key bindings.
    for (const DefaultKey& binding : DefaultKeys)
    {
        bindKey(keys[binding.slot], binding.code, binding.modifiers);

        if (binding.slot == 63)
        {
            rotateKey = 0x38;
            selectedEnemy = nullptr;
            numReserveIcons = 0;
        }
    }

    std::fill(std::begin(reserveIcons), std::end(reserveIcons), nullptr);
    cursorOffset = 0;
    forceOrderActive = 0;
    forceOrderType = -1;
    return 0;
}

auto InterfaceObject::destroy() -> void
{
    if (mechBar != nullptr)
    {
        mechBar->destroy();
        delete mechBar;
        mechBar = nullptr;
    }

    if (commandParser != nullptr)
    {
        delete commandParser;
        commandParser = nullptr;
    }

    for (aFloatHelp*& tag : floatingTags)
    {
        if (tag != nullptr)
        {
            tag->destroy();
            delete tag;
            tag = nullptr;
        }
    }
}

namespace
{
    /// <summary>Whether <paramref name="object"/> is a mech, vehicle, elemental or plain mover.</summary>
    bool isMoverClass(const BaseObject* object)
    {
        const ObjectClass objectClass = object->objectClass;
        return objectClass == BATTLEMECH || objectClass == GROUNDVEHICLE || objectClass == ELEMENTAL ||
               objectClass == MOVER;
    }

    /// <summary>Whether the tactical map may show <paramref name="object"/>'s data: a revealed non-elemental mover.</summary>
    bool canShowInfo(GameObject* object)
    {
        return isMoverClass(object) && object->objectClass != ELEMENTAL && object->isRevealed() != 0;
    }

    /// <summary>A bridge (a misc terrain object of kind 5): clicking one moves onto it.</summary>
    bool isBridge(BaseObject* object)
    {
        return object->objectClass == MISCTERRAINOBJECT &&
               static_cast<MiscTerrainObject*>(object)->terrainObjectKind == 5;
    }

    /// <summary>The keys that pick a forced order (ctrl, F9-F12; slots 4-7).</summary>
    bool isForceOrderKey(const InterfaceObject* iface, const aEvent* event, int16_t scanCode, uint32_t key)
    {
        return (event->ctrlKey != 0 && (scanCode == 0x1d || scanCode == 0x11d)) || key == iface->keys[4] ||
               key == iface->keys[5] || key == iface->keys[7] || key == iface->keys[6];
    }

    /// <summary>
    /// The attack modifiers the aimed-shot and range commands put on an ATTACK_OBJECT order (the cases every
    /// object type's switch shares).
    /// </summary>
    /// <returns>False when <paramref name="command"/> is not one of them.</returns>
    bool setAttackModifier(TacticalOrder& order, int32_t command, bool withLongRange)
    {
        switch (command)
        {
            case 0xb:
            {
                if (!withLongRange)
                {
                    return false;
                }

                order.attackParams.range = -1;
                return true;
            }
            case 0xc:
            {
                order.attackParams.range = 2;
                return true;
            }
            case 0xd:
            {
                order.attackParams.range = 1;
                return true;
            }
            case 0xe:
            {
                order.attackParams.range = 0;
                return true;
            }
            case 0xf:
            {
                order.attackParams.pursue = 0;
                return true;
            }
            case 0x10:
            {
                order.attackParams.type = 3;
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
                static constexpr int32_t AimLocations[8] = {0, 2, 3, 1, 4, 5, 6, 7};
                order.attackParams.pursue = 0;
                order.attackParams.aimLocation = AimLocations[command - 0x17];
                return true;
            }

            default:
                return false;
        }
    }
}

auto InterfaceObject::handleEvent(aEvent* event) -> void
{
    TacticalOrder order;
    order.init();

    // Mission messages and keys.
    if (event->type > 8)
    {
        if (event->type > 0x1403)
        {
            if (event->type == 0x1404 && scenario != nullptr)
            {
                scenario->createPartObject(event->data);
            }

            order.destroy();
            return;
        }

        if (event->type == 0x1403)
        {
            if (scenario != nullptr)
            {
                scenario->destroyPartObject(event->data);
            }

            order.destroy();
            return;
        }

        if (event->type != 9)
        {
            if (event->type == 0x1402)
            {
                RemoveMech(event->data);
            }

            order.destroy();
            return;
        }

        // Key down: fold right alt/ctrl onto the left ones, then add the modifiers.
        const int16_t scanCode = event->scanCode;
        uint32_t key = static_cast<uint32_t>(static_cast<int32_t>(scanCode));

        if (scanCode == 0x138 || scanCode == 0x11d)
        {
            key = (key & (KEY_SHIFT | KEY_CTRL | KEY_ALT)) + static_cast<int16_t>(scanCode - 0x100);
        }

        key &= ~KEY_SHIFT;

        if (event->shiftKey != 0)
        {
            key += KEY_SHIFT;
        }

        key &= ~KEY_CTRL;

        if (event->ctrlKey != 0)
        {
            key += KEY_CTRL;
        }

        key &= ~KEY_ALT;

        if (event->altKey != 0)
        {
            key += KEY_ALT;
        }

        if (scenario != nullptr)
        {
            if (isForceOrderKey(this, event, scanCode, key))
            {
                if (forceOrderType == -1 && forceOrderActive == 0)
                {
                    forceOrderType = 0;
                }

                forceOrderActive = 1;

                for (int32_t i = 0; i < numSelectedMechs; i++)
                {
                    FriendlyMechIcon* button = mechBar->GetButtonFromID(selectedMechs[i]);

                    if (button != nullptr && button->mover != nullptr && isMoverClass(button->mover))
                    {
                        static_cast<Mover*>(button->mover)->unknown89C = 1;
                    }
                }
            }

            TacticalMap* tacMap = Terrain::terrainTacticalMap;

            if (key == keys[65])
            {
                scrollDirection = 0;
            }
            else if (key == keys[66])
            {
                scrollDirection = 4;
            }
            else if (key == keys[67])
            {
                scrollDirection = 6;
            }
            else if (key == keys[68])
            {
                scrollDirection = 2;
            }
            else if (key == keys[69] || key == keys[70] || key == keys[71] || key == keys[72])
            {
                if (Terrain::terrainTacticalMap != nullptr)
                {
                    Terrain::terrainTacticalMap->HideMe(0);
                    Terrain::terrainTacticalMap->SetDisplayType(TACMAP_MAP);
                    tacScrollDirection = key == keys[69] ? 0 : key == keys[70] ? 4 : key == keys[71] ? 6 : 2;
                }
            }
            else if (key == keys[52])
            {
                tacMapShown = 1;
            }
            else if (key == keys[57] || key == keys[59])
            {
                if (Terrain::terrainTacticalMap != nullptr)
                {
                    Terrain::terrainTacticalMap->HideMe(0);
                    Terrain::terrainTacticalMap->SetDisplayType(TACMAP_MAP);
                    aPostMessage(Terrain::terrainTacticalMap, 0x1a);
                }
            }
            else if (key == keys[58] || key == keys[60])
            {
                if (Terrain::terrainTacticalMap != nullptr)
                {
                    Terrain::terrainTacticalMap->HideMe(0);
                    Terrain::terrainTacticalMap->SetDisplayType(TACMAP_MAP);
                    aPostMessage(Terrain::terrainTacticalMap, 0x1b);
                }
            }
            else if (key == keys[61])
            {
                tacMapShown = 0;

                if (tacMap != nullptr)
                {
                    tacMap->HideMe(0);
                    Terrain::terrainTacticalMap->SetDisplayType(TACMAP_MAP);
                }
            }
            else if (key == keys[62])
            {
                tacMapShown = 0;

                if (tacMap != nullptr)
                {
                    tacMap->HideMe(0);
                    Terrain::terrainTacticalMap->SetDisplayType(static_cast<TacmapDisplayTypes>(3));
                }
            }
            else if (MPlayer != nullptr && key == keys[76])
            {
                // The chat line.
                tacMapShown = 0;

                if (tacMap != nullptr && scenario != nullptr && EventsToMissionResultsScreen == 0 && gameAsked == 0)
                {
                    tacMap->HideMe(0);
                    Terrain::terrainTacticalMap->SetDisplayType(static_cast<TacmapDisplayTypes>(3));
                    aObject* chatInput = Terrain::terrainTacticalMap->chatWindow->chatInput;

                    if (application->textObject() != chatInput)
                    {
                        application->setText(Terrain::terrainTacticalMap->chatWindow->chatInput);
                        FirstReturn = 1;
                    }
                }
            }
            else if (key == keys[64])
            {
                tacMapShown = 0;

                if (tacMap != nullptr)
                {
                    tacMap->HideMe(0);
                    Terrain::terrainTacticalMap->SetDisplayType(TACMAP_MISSION);
                }
            }
            else if (key == keys[63])
            {
                tacMapShown = 0;

                if (tacMap != nullptr)
                {
                    tacMap->HideMe(0);
                    Terrain::terrainTacticalMap->SetDisplayType(TACMAP_INFO);
                }
            }
            else if (key == keys[53] || key == keys[55])
            {
                // Zoom in.
                if (eye != nullptr && eye->cameraScale != 100)
                {
                    soundSystem->playDigitalSample(0x44, 1, nullptr, 0, 0);

                    if (only45Pixel == 0 && gamePaused == 0 && gameAsked == 0)
                    {
                        if (eye->cameraScale == 1)
                        {
                            eye->forceUpdate = 1;
                            Terrain::forceRedraw = 1;
                        }

                        eye->cameraScale = 100;
                    }

                    Terrain::terrainTacticalMap->toggleZoom();
                }
            }
            else if (key == keys[54] || key == keys[56])
            {
                // Zoom out.
                if (eye != nullptr && eye->cameraScale != 1)
                {
                    soundSystem->playDigitalSample(0x45, 1, nullptr, 0, 0);
                    bool zoomed = false;

                    if (only45Pixel == 0 && gamePaused == 0 && gameAsked == 0)
                    {
                        if (eye->cameraScale != 1 && eye->cameraScale == 100)
                        {
                            eye->forceUpdate = 1;
                            Terrain::forceRedraw = 1;
                            eye->cameraScale = 1;
                            eye->setPosition(eye->position);
                            Terrain::terrainTacticalMap->toggleZoom();
                            zoomed = true;
                        }
                        else
                        {
                            eye->cameraScale = 1;
                        }
                    }

                    if (!zoomed)
                    {
                        Terrain::terrainTacticalMap->toggleZoom();
                    }
                }
            }
            else if (key == keys[2] && application->grabbedObject() == nullptr)
            {
                currentCommand = 2;
                commandOneShot = 0;

                if (AnySelected(0) != 0)
                {
                    order.init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_STOP, 0);

                    if (commandParser != nullptr)
                    {
                        GetCommandParser()->SendTacOrder(order, 0);
                    }
                }
            }
            else if (key == keys[21])
            {
                currentCommand = 0x15;
                commandOneShot = 0;

                if (AnySelected(0) != 0)
                {
                    order.init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_POWERUP, 0);

                    if (commandParser != nullptr)
                    {
                        GetCommandParser()->SendTacOrder(order, 0);
                    }
                }
            }
            else if (key == keys[22])
            {
                currentCommand = 0x16;
                commandOneShot = 0;

                if (AnySelected(0) != 0)
                {
                    order.init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_POWERDOWN, 0);

                    if (commandParser != nullptr)
                    {
                        GetCommandParser()->SendTacOrder(order, 0);
                    }
                }
            }
            else if (key == keys[1])
            {
                currentCommand = 1;
                commandOneShot = 0;
            }
            else if (key == keys[3] || key == keys[8] || key == keys[5])
            {
                currentCommand = 3;
                commandOneShot = 0;
            }
            else if (key == keys[51])
            {
                currentCommand = 0x33;
                commandOneShot = 0;
            }
            else if (key == keys[11] && AnySelected(1) != 0)
            {
                currentCommand = 0xb;
                commandOneShot = 0;
            }
            else if (key == keys[12] && AnySelected(1) != 0)
            {
                currentCommand = 0xc;
                commandOneShot = 0;
            }
            else if (key == keys[13] && AnySelected(1) != 0)
            {
                currentCommand = 0xd;
                commandOneShot = 0;
            }
            else if (key == keys[16] && AnySelected(1) != 0)
            {
                currentCommand = 0x10;
                commandOneShot = 0;
            }
            else if (key == keys[14] && AnySelected(1) != 0)
            {
                currentCommand = 0xe;
                commandOneShot = 0;
            }
            else if (key == keys[15] && AnySelected(1) != 0)
            {
                currentCommand = 0xf;
                commandOneShot = 0;
            }
            else if (key == keys[17] || key == keys[18] || key == keys[7])
            {
                currentCommand = 0x11;
                commandOneShot = 0;
            }
            else if (key == keys[19])
            {
                currentCommand = 0x13;
                commandOneShot = 0;
            }
            else if (key == keys[23] && AnySelected(1) != 0)
            {
                currentCommand = 0x17;
                commandOneShot = 0;
            }
            else if (key == keys[24] && AnySelected(1) != 0)
            {
                currentCommand = 0x18;
                commandOneShot = 0;
            }
            else if (key == keys[25] && AnySelected(1) != 0)
            {
                currentCommand = 0x19;
                commandOneShot = 0;
            }
            else if (key == keys[26] && AnySelected(1) != 0)
            {
                currentCommand = 0x1a;
                commandOneShot = 0;
            }
            else if (key == keys[27] && AnySelected(1) != 0)
            {
                currentCommand = 0x1b;
                commandOneShot = 0;
            }
            else if (key == keys[28] && AnySelected(1) != 0)
            {
                currentCommand = 0x1c;
                commandOneShot = 0;
            }
            else if (key == keys[29] && AnySelected(1) != 0)
            {
                currentCommand = 0x1d;
                commandOneShot = 0;
            }
            else if (key == keys[30] && AnySelected(1) != 0)
            {
                currentCommand = 0x1e;
                commandOneShot = 0;
            }
            else if (key == keys[45] && AnySelected(0) != 0)
            {
                // Break up the selected movers' lances.
                for (int32_t i = 0; i < numSelectedMechs; i++)
                {
                    FriendlyMechIcon* button = mechBar->GetButtonFromID(selectedMechs[i]);
                    Mover* member = nullptr;

                    if (button != nullptr)
                    {
                        member = static_cast<Mover*>(button->mover);

                        if (button->isPoint != 0)
                        {
                            // A point takes its whole lance with it.
                            for (int16_t j = 0; j < 0xc; j++)
                            {
                                FriendlyMechIcon* other = mechBar->getButton(j);

                                if (other != nullptr && other != button && other->lance == button->lance)
                                {
                                    other->lance = 5;
                                }
                            }
                        }

                        button->lance = 5;
                    }

                    // Port fix: the original read the group of a null mover when the button was missing.
                    if (member != nullptr && member->group != nullptr)
                    {
                        member->group->remove(member);
                    }
                }

                for (int32_t i = 0; i < numSelectedLances; i++)
                {
                    DeselectLance(selectedLances[i]);
                }

                mechBar->PlaceButtons(1);
            }
            else if (key == keys[33] || key == keys[34] || key == keys[35] || key == keys[36])
            {
                // Select a lance.
                const int32_t lance = key == keys[33] ? 0 : key == keys[34] ? 1 : key == keys[35] ? 2 : 3;

                if (HomeCommander->getGroup(lance)->numMovers > 0)
                {
                    ClearMechSelection();
                    SelectLance(HomeCommander->getGroup(lance));
                    commandParser->AddSubject(HomeCommander->getGroup(lance), 0);
                    soundSystem->playDigitalSample(0x30, 1, nullptr, 0, 0);
                }
            }
            else if (key == keys[37] || key == keys[38] || key == keys[39] || key == keys[40])
            {
                // Add a lance to the selection.
                const int32_t lance = key == keys[37] ? 0 : key == keys[38] ? 1 : key == keys[39] ? 2 : 3;

                if (HomeCommander->getGroup(lance)->numMovers > 0)
                {
                    SelectLance(HomeCommander->getGroup(lance));
                    commandParser->AddSubject(HomeCommander->getGroup(lance), 1);
                    soundSystem->playDigitalSample(0x30, 1, nullptr, 0, 0);
                }
            }
            else if (key == keys[74])
            {
                currentCommand = 0x4a;
                commandOneShot = 0;
            }
            else if (Terrain::terrainTacticalMap != nullptr &&
                     (key == keys[46] || key == keys[47] || key == keys[49] || key == keys[48]))
            {
                // Arm an artillery strike.
                const int32_t button = key == keys[46] ? 0 : key == keys[47] ? 1 : key == keys[49] ? 2 : 3;
                Terrain::terrainTacticalMap->activateArtillery(button, 1);
            }
            else if (BunnyStrikesOn != 0 && key == keys[50])
            {
                currentCommand = 0x32;
                commandOneShot = 0;
            }
            else
            {
                if (key == keys[73])
                {
                    TogglePalette();
                }

                if ((key == keys[31] || key == keys[32]) && AnySelected(0) != 0 && numSelectedMechs == 1)
                {
                    BaseObject* selected = objectList->findObjectFromPart(selectedMechs[0]);

                    if (selected == nullptr || selected->objectClass != BATTLEMECH ||
                        static_cast<BattleMech*>(selected)->unknown8D8 == 0 ||
                        static_cast<BattleMech*>(selected)->unknown8DC < 1)
                    {
                        order.destroy();
                        return;
                    }

                    currentCommand = 0x1f;
                    commandOneShot = 0;
                }
            }
        }

        // A key other than a lance-link key ends the hidden-cursor state of a lance link.
        if (application->cursorHidden != 0 && application->cursorShape == 0xd && currentCommand != 0x29 &&
            currentCommand != 0x2a && currentCommand != 0x2b && currentCommand != 0x2c)
        {
            application->cursorHidden = 0;
        }

        order.destroy();
        return;
    }

    if (event->type == 8)
    {
        // Key up.
        const int16_t scanCode = event->scanCode;
        uint32_t key = static_cast<uint32_t>(static_cast<int32_t>(scanCode));

        if (scanCode == 0x138)
        {
            key = 0x38;
        }

        key &= ~KEY_SHIFT;

        if (event->shiftKey != 0)
        {
            key += KEY_SHIFT;
        }

        key &= ~KEY_CTRL;

        if (event->ctrlKey != 0)
        {
            key += KEY_CTRL;
        }

        key &= ~KEY_ALT;

        if (event->altKey != 0)
        {
            key += KEY_ALT;
        }

        if (scanCode == 0x1d || scanCode == 0x11d || key == keys[4] || key == keys[5] || key == keys[7] ||
            key == keys[6])
        {
            forceOrderActive = 0;
            forceOrderType = -1;

            for (int32_t i = 0; i < mechBar->layout.numButtons; i++)
            {
                FriendlyMechIcon* button = mechBar->getButton(static_cast<int16_t>(i));

                if (button != nullptr && button->mover != nullptr && isMoverClass(button->mover))
                {
                    static_cast<Mover*>(button->mover)->unknown89C = 0;
                }
            }
        }

        if (event->target == nullptr || event->target->parent != mainHolder)
        {
            if (currentCommand != 0x29 && currentCommand != 0x2a && currentCommand != 0x2b && currentCommand != 0x2c)
            {
                currentCommand = 0;
                commandOneShot = 0;
            }

            scrollDirection = -1;
            tacScrollDirection = -1;

            if (scanCode == rotateKey && eye != nullptr)
            {
                scrollWait = 0;
            }

            if (Terrain::terrainTacticalMap != nullptr)
            {
                if (static_cast<int16_t>(key) == static_cast<int16_t>(keys[52]) && tacMapShown != 0)
                {
                    Terrain::terrainTacticalMap->HideMe(Terrain::terrainTacticalMap->IsHidden() == 0);
                }

                if (Terrain::terrainTacticalMap != nullptr && key == keys[46])
                {
                    Terrain::terrainTacticalMap->activateArtillery(0, 0);
                }

                if (Terrain::terrainTacticalMap != nullptr && key == keys[47])
                {
                    Terrain::terrainTacticalMap->activateArtillery(1, 0);
                }

                if (Terrain::terrainTacticalMap != nullptr && key == keys[49])
                {
                    Terrain::terrainTacticalMap->activateArtillery(2, 0);
                }

                if (Terrain::terrainTacticalMap != nullptr && key == keys[48])
                {
                    Terrain::terrainTacticalMap->activateArtillery(3, 0);
                }
            }

            // The lance-link keys (ctrl+F1-F4) wait for a click on the lance's point.
            for (int32_t lance = 0; lance < 4; lance++)
            {
                if (key == keys[41 + lance] && AnySelected(0) != 0)
                {
                    currentCommand = 0x29 + lance;
                    commandOneShot = 0;
                    application->SetCurrentCursor(static_cast<CursorType>(0xd));
                    application->cursorHidden = 1;
                }
            }

            if (key == keys[75] && scenario != nullptr)
            {
                SelectVisibleMechs();
                order.destroy();
                return;
            }
        }

        order.destroy();
        return;
    }

    // The mouse position in the event's window, and the world point under it.
    auto windowPoint = [event](aObject* window)
    {
        const int32_t y = event->y - window->globalY();
        const int32_t x = event->x - window->globalX();
        return vector_2d(static_cast<float>(x), static_cast<float>(y));
    };

    // The command-mode reset after a lance link, then the usual redraw.
    auto endLanceLink = [&]()
    {
        currentCommand = 0;
        commandOneShot = 0;
        application->cursorHidden = 0;
        UpdateInterface();
        order.destroy();
    };

    auto sendOrder = [&](int sortMovers)
    {
        GetCommandParser()->SendTacOrder(order, sortMovers);
        UpdateInterface();
        order.destroy();
    };

    auto finish = [&]()
    {
        UpdateInterface();
        order.destroy();
    };

    // Sends the forced order to the selected movers' pilots (and to the server in multiplayer).
    auto queueForcedOrder = [&](TacticalOrder& forcedOrder)
    {
        forcedOrder.pack(nullptr, nullptr);

        if (MPlayer != nullptr && MPlayer->isServer == 0)
        {
            int32_t moverParts[12];

            for (int32_t i = 0; i < numSelectedMechs; i++)
            {
                moverParts[i] = 0;
                FriendlyMechIcon* button = mechBar->GetButtonFromID(selectedMechs[i]);

                if (button != nullptr)
                {
                    moverParts[i] = button->mover->partId;
                }
            }

            MPlayer->sendPlayerOrder(0, &forcedOrder, 0, numSelectedMechs, moverParts, 0, nullptr, 1);
        }

        for (int32_t i = 0; i < numSelectedMechs; i++)
        {
            FriendlyMechIcon* button = mechBar->GetButtonFromID(selectedMechs[i]);

            if (button == nullptr || button->mover == nullptr || !isMoverClass(button->mover))
            {
                continue;
            }

            auto* member = static_cast<Mover*>(button->mover);

            if (member->getPilot() == nullptr)
            {
                continue;
            }

            if (MPlayer != nullptr)
            {
                forcedOrder.id = 0;
                forcedOrder.setId(member->getPilot());
            }

            member->getPilot()->addQueuedTacOrder(forcedOrder);
            member->getPilot()->tacOrderQueueExecuting = 1;
        }
    };

    GameObject* clicked = nullptr;
    int32_t clickedPartId = 0;
    int32_t command = 0;
    Camera* camera = nullptr;
    aObject* target = nullptr;

    switch (event->type)
    {
        case 1:
        {
            // Left button down on the map: remember where, or give the forced order.
            if (event->target != mainHolder->GetActivePane())
            {
                order.destroy();
                return;
            }

            const int32_t x = event->x;
            const int32_t y = event->y;
            mouseDownX = static_cast<float>(x);
            mouseDownY = static_cast<float>(y);

            if (forceOrderActive == 0)
            {
                mouseDown = 1;
                order.destroy();
                return;
            }

            if (forceOrderType == -1)
            {
                soundSystem->playDigitalSample(0x46, 1, nullptr, 0, 0);
                order.destroy();
                return;
            }

            TacticalOrder forcedOrder;
            forcedOrder.init();
            target = event->target;
            vector_2d screenPos = windowPoint(target);
            // Port fix: zeroed; the original left the point uninitialised when the window has no camera.
            LocationNode node;
            node.location = vector_3d(0.0f, 0.0f, 0.0f);

            if (target != nullptr && target->GetCamera() != nullptr)
            {
                target->GetCamera()->inverseProject(screenPos, node.location);
            }

            node.next = nullptr;
            node.run = forceOrderType == 1;

            if (forceOrderType == 2)
            {
                forcedOrder.init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_JUMPTO_POINT, 0);
                forcedOrder.initWayPath(&node);
                forcedOrder.moveParams.wait = 0;
            }
            else
            {
                forcedOrder.init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_MOVETO_POINT, 0);
                forcedOrder.initWayPath(&node);
                forcedOrder.moveParams.wayPath.mode[0] = static_cast<uint8_t>(forceOrderType);
                forcedOrder.moveParams.wait = 0;

                if (currentCommand == 0x1f)
                {
                    forcedOrder.moveParams.mode = 1;
                }
            }

            queueForcedOrder(forcedOrder);
            forcedOrder.destroy();
            order.destroy();
            return;
        }

        case 4:
        {
            // Left button up: ends a drag selection, or gives the order the click means.
            if (mouseDown == 0)
            {
                order.destroy();
                return;
            }

            target = event->target;
            mouseDown = 0;

            if (target != mainHolder->GetActivePane())
            {
                mainHolder->SetActivePane(target);
                UpdateMouseState(event);
                order.destroy();
                return;
            }

            if (dragTarget != nullptr)
            {
                application->cursorHidden = 0;

                if (event->shiftKey == 0)
                {
                    ClearMechSelection();
                    commandParser->ClearSubjects();
                    numSelectedMechs = 0;
                }

                DeselectEnemy();
                application->release();
                auto* dragWindow = static_cast<viewWindow*>(dragTarget);

                for (int16_t i = 0; i < mechBar->layout.numButtons; i++)
                {
                    const int32_t partId = mechBar->getButton(i)->partId;
                    BaseObject* member = mechBar->getButton(i)->mover;

                    if (member == nullptr || member->getAppearance() == nullptr || dragTarget == nullptr)
                    {
                        continue;
                    }

                    const vector_2d screenPos = member->getAppearance()->getScreenPos(dragTarget->GetCamera());
                    POINT point;
                    point.x = static_cast<int32_t>(screenPos.x);
                    point.y = static_cast<int32_t>(screenPos.y);
                    const float* box = dragWindow->selectionBox;
                    RECT rect;
                    rect.left = static_cast<int32_t>(box[2] <= box[0] ? box[2] : box[0]);
                    rect.right = static_cast<int32_t>(box[2] < box[0] ? box[0] : box[2]);
                    rect.top = static_cast<int32_t>(box[3] <= box[1] ? box[3] : box[1]);
                    rect.bottom = static_cast<int32_t>(box[3] < box[1] ? box[1] : box[3]);

                    if (PtInRect(&rect, point) != 0)
                    {
                        SelectMech(partId);
                        commandParser->AddSubject(partId, 1);
                    }
                }

                dragWindow->selectionBox[1] = 0.0f;
                dragWindow->selectionBox[0] = 0.0f;
                dragWindow->selectionBox[3] = 0.0f;
                dragWindow->selectionBox[2] = 0.0f;
                dragTarget = nullptr;

                if (currentCommand == 0x29 || currentCommand == 0x2a || currentCommand == 0x2b ||
                    currentCommand == 0x2c)
                {
                    endLanceLink();
                }
                else
                {
                    finish();
                }

                return;
            }

            command = currentCommand;

            if (target != nullptr)
            {
                camera = target->GetCamera();
            }

            if (currentCommand == 0x32 && camera != nullptr)
            {
                // The debug "bunny" strike: a large strike where the player clicked.
                vector_2d screenPos = windowPoint(target);
                vector_3d strikePos;
                camera->inverseProject(screenPos, strikePos);
                HomeCommander->setNumLargeStrikes(HomeCommander->numLargeStrikes + 1);
                CallArtillery(HomeCommander->id, 1, strikePos, 3, 0);

                for (ArtilleryButton* button : tacticalMap->artilleryButtons)
                {
                    button->draw();
                }

                order.destroy();
                return;
            }

            clicked = static_cast<GameObject*>(mouseObject);

            if (clicked != nullptr)
            {
                clickedPartId = clicked->partId;

                // Aimed shots only work on mechs; on anything else they are plain attacks.
                if (clicked->objectClass != BATTLEMECH && currentCommand >= 0x17 && currentCommand <= 0x1e)
                {
                    command = 0xb;
                }
            }
            break;
        }

        case 6:
        {
            ClearMechSelection();
            order.destroy();
            return;
        }
        case 7:
        {
            // Mouse move: track what is under the mouse, and drag out a selection box.
            if (event->target == mainHolder->GetActivePane())
            {
                if (mouseDown == 0)
                {
                    UpdateMouseState(event);

                    if (selectedEnemy != nullptr)
                    {
                        selectedEnemy->setSelected(0);
                    }

                    if (mouseObjectType == 1)
                    {
                        selectedEnemy = static_cast<GameObject*>(mouseObject);

                        if (selectedEnemy != nullptr)
                        {
                            selectedEnemy->setSelected(1);
                        }
                    }
                }

                if (event->leftButton != 0 && mouseDown != 0)
                {
                    int32_t x = event->x;
                    int32_t y = event->y;
                    bool dragging = dragTarget != nullptr;

                    if (!dragging)
                    {
                        // x87: the distance is compared at extended precision.
                        const double dy = static_cast<double>(static_cast<float>(y)) - mouseDownY;
                        const double dx = static_cast<double>(x) - mouseDownX;
                        dragging = static_cast<double>(dragDistance) < std::sqrt(dx * dx + dy * dy);
                    }

                    if (dragging)
                    {
                        application->SetCurrentCursor(static_cast<CursorType>(0));
                        aObject* dragWindow = dragTarget;
                        application->cursorHidden = 1;

                        if (dragTarget == nullptr)
                        {
                            dragTarget = event->target;
                            application->grab(dragTarget);
                            dragWindow = dragTarget;
                            static_cast<viewWindow*>(dragTarget)->selectionBox[0] = mouseDownX;
                            static_cast<viewWindow*>(dragWindow)->selectionBox[1] = mouseDownY;
                        }

                        if (x < dragWindow->globalX())
                        {
                            x = dragTarget->globalX();
                        }

                        if (y < dragTarget->globalY())
                        {
                            y = dragTarget->globalY();
                        }

                        if (dragTarget->globalX() + dragTarget->width() <= x)
                        {
                            x = dragTarget->globalX() - 1 + dragTarget->width();
                        }

                        if (dragTarget->globalY() + dragTarget->height() <= y)
                        {
                            y = dragTarget->globalY() - 1 + dragTarget->height();
                        }

                        const int32_t boxY = y - dragTarget->globalY();
                        const int32_t boxX = x - dragTarget->globalX();
                        static_cast<viewWindow*>(dragWindow)->selectionBox[2] = static_cast<float>(boxX);
                        static_cast<viewWindow*>(dragWindow)->selectionBox[3] = static_cast<float>(boxY);
                        order.destroy();
                        return;
                    }
                }
            }

            order.destroy();
            return;
        }

        default:
        {
            order.destroy();
            return;
        }
    }

    // The click (event 4): what it means depends on what is under the mouse and the command mode.
    auto initAttack = [&]()
    {
        order.init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_ATTACK_OBJECT, 0);
        order.target = clicked;
        order.attackParams.type = 1;
        order.attackParams.method = 0;
        order.attackParams.range = -4;
        order.attackParams.pursue = 1;
    };

    // An order on the clicked object: its code, the way path's run flag and the wait flag.
    auto initObjectOrder = [&](TacticalOrderCode code, uint8_t run, int32_t wait)
    {
        order.init(ORDER_ORIGIN_PLAYER, code, 0);
        order.target = clicked;
        order.moveParams.wayPath.mode[0] = run;
        order.moveParams.wait = wait;
    };

    // Command 0x1f: move onto the object with move mode 1.
    auto initMoveMode1 = [&]()
    {
        order.init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_MOVETO_OBJECT, 0);
        order.target = clicked;
        order.moveParams.wayPath.mode[0] = 0;
        order.moveParams.wait = 0;
        order.moveParams.mode = 1;
    };

    // Jumps onto the clicked object, when every selected mover can.
    auto jumpToObject = [&]() -> bool
    {
        if (canSelectionJumpTo(clicked->getPosition(), clicked, forceOrderActive) == 0)
        {
            return false;
        }

        order.init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_JUMPTO_OBJECT, 0);
        order.target = static_cast<GameObject*>(mouseObject);
        order.moveParams.wayPath.mode[0] = 2;
        order.moveParams.wait = 0;
        order.setWayPoint(0, static_cast<GameObject*>(mouseObject)->getPosition());
        return true;
    };

    // An order along a one-point way path at <c>mouseWorldPos</c>.
    auto initPointOrder = [&](TacticalOrderCode code, int run)
    {
        LocationNode node;
        node.location = mouseWorldPos;
        node.run = run;
        node.next = nullptr;
        order.init(ORDER_ORIGIN_PLAYER, code, 0);
        order.initWayPath(&node);
        order.moveParams.wait = 0;
    };

    // Unprojects the click into <c>mouseWorldPos</c>; false without a camera.
    auto unprojectClick = [&]() -> bool
    {
        vector_2d screenPos = windowPoint(target);

        if (camera == nullptr)
        {
            return false;
        }

        camera->inverseProject(screenPos, mouseWorldPos);
        return true;
    };

    auto showInfo = [&]()
    {
        Terrain::terrainTacticalMap->HideMe(0);
        Terrain::terrainTacticalMap->SetDisplayType(TACMAP_INFO);
        Terrain::terrainTacticalMap->SetID(clickedPartId);
        finish();
    };

    auto followClicked = [&]()
    {
        if (camera == nullptr)
        {
            finish();
            return;
        }

        camera->changeTarget(clicked, 0);
        finish();
    };

    auto selectEnemyOnMap = [&](GameObject* enemy)
    {
        if (selectedEnemy != nullptr)
        {
            selectedEnemy->setSelected(0);
        }

        selectedEnemy = enemy;

        if (enemy != nullptr)
        {
            enemy->setSelected(1);
        }

        Terrain::terrainTacticalMap->SetID(enemy->partId);
    };

    switch (mouseObjectType)
    {
        case 0:
        {
            // One of the player's movers.
            initAttack();

            if (setAttackModifier(order, command, true))
            {
                sendOrder(0);
                return;
            }

            auto* member = static_cast<Mover*>(clicked);

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
                        order.init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_EJECT, 0);
                    }
                    else if (command == 2)
                    {
                        order.init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_STOP, 0);
                    }
                    else if (command == 0x15)
                    {
                        order.init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_POWERUP, 1);
                    }
                    else
                    {
                        order.init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_POWERDOWN, 1);
                    }

                    if (MPlayer == nullptr || MPlayer->isServer != 0)
                    {
                        member->handleTacticalOrder(order, 1, 0);
                        finish();
                        return;
                    }

                    int32_t partId = member->partId;

                    if (command == 2)
                    {
                        member->getPilot()->clearTacOrderQueue();
                    }

                    MPlayer->sendPlayerOrder(0, &order, 0, 1, &partId, 0, nullptr, 0);
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
                    order.init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_REFIT, 0);
                    order.target = clicked;
                    order.moveParams.wayPath.mode[0] = 1;
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
                    soundSystem->playBettySample(5);
                    const int32_t lance = currentCommand - 0x29;

                    if (IsSelected(clickedPartId) == 0)
                    {
                        SelectMech(clickedPartId);
                    }

                    const int16_t numMovers = numSelectedMechs;
                    // Port fix: zeroed; the original left the entry of a missing button uninitialised.
                    auto** movers = new GameObject*[numMovers]();
                    // The original's point index is left at the command when the point is not found.
                    int32_t pointIndex = command;

                    for (int32_t i = 0; i < numSelectedMechs; i++)
                    {
                        FriendlyMechIcon* button = mechBar->GetButtonFromID(selectedMechs[i]);

                        if (button == nullptr)
                        {
                            continue;
                        }

                        movers[i] = static_cast<GameObject*>(button->mover);

                        if (button->isPoint != 0)
                        {
                            button->isPoint = 0;
                        }

                        if (button->mover == mouseObject)
                        {
                            button->isPoint = 1;
                            pointIndex = i;
                        }
                    }

                    for (int32_t i = 0; i < 4; i++)
                    {
                        if (mechBar != nullptr && mechBar->lanceIcons[i] != nullptr &&
                            mechBar->lanceIcons[i]->group != nullptr)
                        {
                            mechBar->lanceIcons[i]->numActiveMovers = mechBar->lanceIcons[i]->group->numMovers;
                        }
                    }

                    setUnit(lance, numSelectedMechs, movers, pointIndex);
                    delete[] movers;
                    mechBar->PlaceButtons(1);
                    currentCommand = 0;
                    commandOneShot = 0;
                    application->cursorHidden = 0;
                    ClearMechSelection();
                    SelectLance(HomeCommander->getGroup(lance));
                    commandParser->AddSubject(HomeCommander->getGroup(lance), 0);
                    UpdateInterface();
                    order.destroy();
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

            if (event->shiftKey == 0)
            {
                ClearMechSelection();
                DeselectEnemy();
                commandParser->ClearSubjects();

                if (AnySelected(0) == 0)
                {
                    Terrain::terrainTacticalMap->SetID(clickedPartId);
                }

                SelectMech(clickedPartId);
                addToExisting = 0;
            }
            else
            {
                if (IsSelected(clickedPartId) != 0)
                {
                    DeselectMech(clickedPartId);
                    commandParser->RemoveSubject(clickedPartId);
                    finish();
                    return;
                }

                SelectMech(clickedPartId);
                DeselectEnemy();
                addToExisting = 1;
            }

            commandParser->AddSubject(clickedPartId, addToExisting);
            soundSystem->playDigitalSample(0x10, 1, nullptr, 0, 0);
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
                if (!canShowInfo(clicked))
                {
                    finish();
                    return;
                }

                selectEnemyOnMap(clicked);
                const int32_t mode = currentCommand;

                if (mode == 0x33)
                {
                    Terrain::terrainTacticalMap->HideMe(0);
                    Terrain::terrainTacticalMap->SetDisplayType(TACMAP_INFO);
                }

                if (mode == 0x4a && camera != nullptr)
                {
                    camera->changeTarget(clicked, 0);
                    finish();
                    return;
                }

                finish();
                return;
            }

            order.init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_ATTACK_OBJECT, 0);
            clicked = static_cast<GameObject*>(mouseObject);
            order.moveParams.wayPath.mode[0] = currentCommand == 3;
            order.attackParams.type = 1;
            order.attackParams.pursue = 1;
            order.target = clicked;
            order.attackParams.method = 0;
            order.attackParams.range = -4;

            if (setAttackModifier(order, command, false))
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
                    if (canSelectionJumpTo(clicked->getPosition(), nullptr, forceOrderActive) == 0)
                    {
                        finish();
                        return;
                    }

                    if (isMoverClass(mouseObject))
                    {
                        // A jump attack.
                        order.attackParams.method = 1;
                        sendOrder(0);
                        return;
                    }

                    if (camera == nullptr)
                    {
                        finish();
                        return;
                    }

                    {
                        vector_2d screenPos = windowPoint(target);
                        LocationNode node;
                        camera->inverseProject(screenPos, node.location);
                        node.run = 0;
                        node.next = nullptr;
                        order.init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_JUMPTO_POINT, 0);
                        order.initWayPath(&node);
                        order.moveParams.wait = 0;
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
                    if (!canShowInfo(clicked))
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
                    if (canCapture != 0)
                    {
                        if (captureBlocked != 0)
                        {
                            finish();
                            return;
                        }

                        order.init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_CAPTURE, 0);
                        order.target = clicked;
                        order.moveParams.wayPath.mode[0] = 1;
                        sendOrder(0);
                        return;
                    }

                    order.moveParams.wait = 0;
                    order.moveParams.wayPath.mode[0] = command == 3;
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
            order.moveParams.wayPath.mode[0] = 0;

            if (!setAttackModifier(order, command, false))
            {
                switch (command)
                {
                    case 0x11:
                    {
                        if (canSelectionJumpTo(clicked->getPosition(), nullptr, forceOrderActive) != 0)
                        {
                            order.attackParams.method = 1;
                        }
                        break;
                    }
                    case 0x1f:
                        initMoveMode1();
                        break;
                    case 0x33:
                    {
                        if (!canShowInfo(clicked))
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
                        currentCommand = 0;
                        commandOneShot = 0;
                        application->cursorHidden = 0;
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
                    Terrain::terrainTacticalMap->HideMe(0);
                    Terrain::terrainTacticalMap->SetDisplayType(TACMAP_INFO);
                }
                else if (command == 0x4a)
                {
                    followClicked();
                    return;
                }

                Terrain::terrainTacticalMap->SetID(clicked->partId);
                finish();
                return;
            }

            initAttack();

            if (setAttackModifier(order, command, true))
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
            order.moveParams.wayPath.mode[0] = 0;

            if (setAttackModifier(order, command, true))
            {
                sendOrder(0);
                return;
            }

            switch (command)
            {
                case 3:
                {
                    if (!isBridge(clicked))
                    {
                        initObjectOrder(TACTICAL_ORDER_MOVETO_OBJECT, 1, 0);
                        sendOrder(1);
                        return;
                    }

                    // A bridge: move onto the point clicked (the attack order goes out unchanged without a camera).
                    if (unprojectClick())
                    {
                        initPointOrder(TACTICAL_ORDER_MOVETO_POINT, 1);
                        order.moveParams.wayPath.mode[0] = 1;
                    }

                    sendOrder(1);
                    return;
                }
                case 0xa:
                {
                    order.init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_GETFIXED, 0);
                    order.target = clicked;
                    order.moveParams.wayPath.mode[0] = 1;
                    sendOrder(0);
                    return;
                }
                case 0x11:
                {
                    if (!unprojectClick() || canSelectionJumpTo(mouseWorldPos, nullptr, forceOrderActive) == 0)
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
                    if (static_cast<uint8_t>(clicked->status) == 2 || static_cast<uint8_t>(clicked->status) == 1)
                    {
                        // Wrecked: move to it.
                        initObjectOrder(TACTICAL_ORDER_MOVETO_OBJECT, 0, 0);
                        sendOrder(0);
                        return;
                    }

                    if (!isBridge(clicked) && clicked->getAlignment() != homeTeam->alignment)
                    {
                        // Someone else's: attack it.
                        order.attackParams.range = -4;
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
                    order.moveParams.wayPath.mode[0] = command == 3;
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

            switch (currentCommand)
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
                    order.init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_ATTACK_POINT, 0);
                    order.attackParams.targetPoint = mouseWorldPos;
                    order.target = nullptr;
                    order.attackParams.type = 1;
                    order.attackParams.method = 0;
                    order.attackParams.range = -4;
                    order.attackParams.pursue = 1;

                    if (command >= 0xb && command <= 0x10)
                    {
                        setAttackModifier(order, command, true);
                    }

                    sendOrder(1);
                    return;
                }
                default:
                    break;
            }

            initPointOrder(TACTICAL_ORDER_MOVETO_POINT, 0);
            order.moveParams.wayPath.mode[0] = 0;

            switch (command)
            {
                case 0:
                {
                    sendOrder(1);
                    return;
                }
                case 3:
                {
                    order.moveParams.wayPath.mode[0] = 1;
                    sendOrder(1);
                    return;
                }
                case 0x11:
                {
                    if (canSelectionJumpTo(mouseWorldPos, nullptr, forceOrderActive) == 0)
                    {
                        finish();
                        return;
                    }

                    initPointOrder(TACTICAL_ORDER_JUMPTO_POINT, 0);
                    order.moveParams.wayPath.mode[0] = 0;
                    sendOrder(1);
                    return;
                }
                case 0x13:
                {
                    initPointOrder(TACTICAL_ORDER_GUARD, 0);
                    order.moveParams.wayPath.mode[0] = 0;
                    sendOrder(1);
                    return;
                }
                case 0x1f:
                {
                    order.moveParams.mode = 1;
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
                    camera->changeTarget(nullptr, 0);
                    camera->setPosition(mouseWorldPos);
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

auto InterfaceObject::StartScenario() -> int32_t
{
    commandParser = new Parser;

    if (commandParser == nullptr)
    {
        return 3;
    }

    commandParser->init();
    unknownA4 = 0;
    mouseObjectType = -1;
    mouseObject = nullptr;
    currentCommand = 0;
    commandOneShot = 0;
    UpdateInterface();

    if (mainHolder != nullptr)
    {
        mainHolder->ShowGUIWindow(1);
    }

    if (scrollCallback == nullptr)
    {
        scrollCallback = new aCallback;
        scrollCallback->setExec(ScrollScreen);
        application->addCallback(scrollCallback);
    }

    salvageIcon = nullptr;
    selectedEnemy = nullptr;
    Terrain::terrainTacticalMap->setDepth(0x50);
    aMechBar* bar = mechBar;

    if (bar->getButton(0) != nullptr)
    {
        Terrain::terrainTacticalMap->SetID(bar->getButton(0)->partId);
    }

    bar->ShowGUIWindow(1);
    bar->InitLances();
    dragTarget = nullptr;
    forceOrderActive = 0;
    // Slot 62 (alt+S by default) becomes alt+C in multiplayer.
    bindKey(keys[62], MPlayer != nullptr ? 0x2e : 0x1f, 0);
    return 0;
}

auto InterfaceObject::EndScenario() -> void
{
    ClearMechSelection();
    highlightedObject = nullptr;

    if (commandParser != nullptr)
    {
        // The original frees it with the global operator delete (no destructor).
        ::operator delete(commandParser);
        commandParser = nullptr;
    }

    aMechBar* bar = mechBar;

    for (int32_t i = 0; i < 12; i++)
    {
        if (bar != nullptr && i < 4 && bar->lanceIcons[i] != nullptr)
        {
            bar->lanceIcons[i]->destroy();
        }
    }

    bar->cleanUp();
    bar->ShowGUIWindow(0);
    bar->DestroyLances();
    HideTags();

    if (salvageIcon != nullptr)
    {
        salvageIcon->destroy();
        delete salvageIcon;
        salvageIcon = nullptr;
    }

    if (mainHolder != nullptr)
    {
        mainHolder->ShowGUIWindow(0);
    }

    application->removeCallback(scrollCallback);

    if (scrollCallback != nullptr)
    {
        delete scrollCallback;
        scrollCallback = nullptr;
    }

    numReserveIcons = 0;

    for (FriendlyMechIcon*& icon : reserveIcons)
    {
        if (icon != nullptr)
        {
            icon->destroy();
            delete icon;
            icon = nullptr;
        }
    }
}

auto InterfaceObject::AddMech(int32_t partId, int32_t lance, int active, int onBar) -> int32_t
{
    auto* icon = new FriendlyMechIcon;
    icon->init(0, 0, 0x34, 0x2e, nullptr);
    icon->SetID(partId);
    icon->active = active;
    icon->ShowGUIWindow(active);

    if (onBar != 0)
    {
        aMechBar* bar = mechBar;
        icon->lance = lance;
        bar->AddButton(icon);
        bar->draw();
        icon->setEventRoutine(mechIconHandleEvent);
        return 0;
    }

    icon->lance = 5;
    reserveIcons[numReserveIcons] = icon;
    numReserveIcons++;
    return 0;
}

auto InterfaceObject::ActivateMech(int32_t partId) -> void
{
    aMechBar* bar = mechBar;
    FriendlyMechIcon* icon = bar->GetButtonFromID(partId);

    if (icon != nullptr)
    {
        icon->active = 1;
        icon->ShowGUIWindow(1);
        icon->SetID(icon->partId);
        bar->PlaceButtons(1);
    }
}

auto InterfaceObject::RemoveMech(int32_t partId) -> void
{
    if (IsOurs(static_cast<int16_t>(partId)) != 0)
    {
        aMechBar* bar = mechBar;
        FriendlyMechIcon* icon = bar->GetButtonFromID(partId);

        if (icon != nullptr)
        {
            if (icon->isPoint == 0)
            {
                // Drop it from the selection and tell the mover.
                const int16_t count = numSelectedMechs;
                int16_t index = 0;

                while (index < count && selectedMechs[index] != partId)
                {
                    index++;
                }

                if (index < count)
                {
                    for (int32_t i = index; i < numSelectedMechs - 1; i++)
                    {
                        selectedMechs[i] = selectedMechs[i + 1];
                    }

                    selectedMechs[numSelectedMechs] = 0;
                    numSelectedMechs--;
                }

                if (objectList != nullptr)
                {
                    BaseObject* object = objectList->findObjectFromPart(partId);
                    ObjectEvent deselect;
                    deselect.init(0x1d, nullptr);

                    if (object != nullptr)
                    {
                        object->handleEvent(&deselect);
                    }
                }
            }
            else
            {
                // The lance's point died: once no member is left standing, the lance goes from the selection.
                LanceIcon* lanceIcon = bar != nullptr ? bar->GetLanceIconFromID(icon->lance) : nullptr;
                Assert(lanceIcon != nullptr, partId, " InterfaceObject.RemoveMech: NULL lanceIcon ");
                lanceIcon->unknown4CC = 0;
                MoverGroup* group = lanceIcon->group;

                for (int16_t i = 0; i < group->numMovers; i++)
                {
                    if (group->movers[i]->isDisabled() == 0)
                    {
                        goto removeSubject;
                    }
                }

                DeselectLance(group);
                commandParser->RemoveSubject(group);
            }
        }
    removeSubject:
        if (commandParser != nullptr)
        {
            commandParser->RemoveSubject(partId);
        }
    }

    if (selectedEnemy != nullptr && selectedEnemy->partId == partId)
    {
        selectedEnemy = nullptr;
    }
}

auto InterfaceObject::UpdateInterface() -> void
{
    mechBar->draw();
}

auto InterfaceObject::IsSelected(int32_t partId) -> int
{
    for (int16_t i = 0; i < numSelectedMechs; i++)
    {
        if (selectedMechs[i] == partId)
        {
            return 1;
        }
    }

    return 0;
}

auto InterfaceObject::IsSelected(MoverGroup* group) -> int
{
    for (int16_t i = 0; i < numSelectedLances; i++)
    {
        if (selectedLances[i] == group)
        {
            return 1;
        }
    }

    return 0;
}

auto InterfaceObject::SelectMech(int32_t partId) -> void
{
    ObjectEvent select;
    const int16_t index = numSelectedMechs;

    if (index >= 0xc)
    {
        return;
    }

    if (objectList != nullptr)
    {
        auto* object = static_cast<GameObject*>(objectList->findObjectFromPart(partId));

        // Port fix: the original asks a missing object whether it is disabled.
        if (object != nullptr && object->isDisabled() != 0)
        {
            return;
        }

        select.init(0x1c, nullptr);
        select.unknown54 = index;

        if (object != nullptr)
        {
            object->handleEvent(&select);
        }
    }

    selectedMechs[index] = partId;
    numSelectedMechs++;
}

auto InterfaceObject::SelectVisibleMechs() -> void
{
    ClearMechSelection();
    ObjectQueueNode* list = homeTeam->alignment == -1 ? clanMechList : innerSphereMechList;

    if (list == nullptr)
    {
        return;
    }

    for (BaseObject* object = list->head; object != nullptr; object = object->next)
    {
        if (!isMoverClass(object))
        {
            continue;
        }

        auto* mover = static_cast<Mover*>(object);

        if (mover->netPlayerId != -1 && mover->getWindowsVisible() == turn && mover->isDisabled() == 0)
        {
            const int32_t partId = mover->partId;
            SelectMech(partId);
            commandParser->AddSubject(partId, 1);
        }
    }
}

auto InterfaceObject::DeselectMech(int32_t partId) -> void
{
    ObjectEvent deselect;
    const int16_t count = numSelectedMechs;
    int16_t index = 0;

    while (index < count && selectedMechs[index] != partId)
    {
        index++;
    }

    if (index < count)
    {
        for (int32_t i = index; i < numSelectedMechs - 1; i++)
        {
            selectedMechs[i] = selectedMechs[i + 1];
        }

        selectedMechs[numSelectedMechs] = 0;
        numSelectedMechs--;
    }

    // A mover taken out of a selected lance breaks the lance up: its other members stay selected on their own.
    aMechBar* bar = mechBar;
    FriendlyMechIcon* icon = bar->GetButtonFromID(partId);

    if (icon != nullptr && bar != nullptr)
    {
        LanceIcon* lanceIcon = bar->GetLanceIconFromID(icon->lance);

        if (lanceIcon != nullptr)
        {
            MoverGroup* group = lanceIcon->group;

            if (IsSelected(group) != 0)
            {
                DeselectLance(group);
                commandParser->RemoveSubject(group);

                for (int16_t i = 0; i < group->numMovers; i++)
                {
                    Mover* member = group->movers[i];

                    if (member != icon->mover)
                    {
                        SelectMech(member->partId);
                        commandParser->AddSubject(member->partId, 1);
                    }
                }
            }
        }
    }

    if (objectList != nullptr)
    {
        BaseObject* object = objectList->findObjectFromPart(partId);
        deselect.init(0x1d, nullptr);

        if (object != nullptr)
        {
            object->handleEvent(&deselect);
        }
    }
}

auto InterfaceObject::SelectEnemy(int32_t partId) -> void
{
    ObjectEvent select;

    if (objectList != nullptr)
    {
        BaseObject* object = objectList->findObjectFromPart(partId);
        select.init(0x1c, nullptr);
        select.unknown54 = 0;

        if (object != nullptr)
        {
            object->handleEvent(&select);
        }
    }
}

auto InterfaceObject::DeselectEnemy() -> void
{
    ObjectEvent deselect;

    if (selectedEnemy != nullptr)
    {
        deselect.init(0x1d, nullptr);
        selectedEnemy->handleEvent(&deselect);
    }
}

auto InterfaceObject::SelectLance(MoverGroup* group) -> void
{
    LanceIcon* lanceIcon = mechBar->GetLanceIconFromID(group->getId());

    if (lanceIcon->unknown4CC == 0)
    {
        // Not a linked lance: select its movers one by one.
        for (int32_t i = 0; i < group->numMovers; i++)
        {
            if (group->movers[i] != nullptr)
            {
                const int32_t partId = group->movers[i]->partId;

                if (IsSelected(partId) == 0)
                {
                    SelectMech(partId);
                    commandParser->AddSubject(partId, 1);
                }
            }
        }
    }
    else if (numSelectedLances < 4 && IsSelected(group) == 0)
    {
        selectedLances[numSelectedLances] = group;
        numSelectedLances++;

        for (int32_t i = 0; i < group->numMovers; i++)
        {
            if (group->movers[i] != nullptr)
            {
                const int32_t partId = group->movers[i]->partId;

                if (IsSelected(partId) == 0)
                {
                    SelectMech(partId);
                }
            }
        }
    }
}

auto InterfaceObject::DeselectLance(MoverGroup* group) -> void
{
    if (group == nullptr)
    {
        return;
    }

    if (IsSelected(group) != 0)
    {
        const int16_t count = numSelectedLances;
        int32_t index = 0;

        while (index < count && selectedLances[index] != group)
        {
            index++;
        }

        numSelectedLances = static_cast<int16_t>(count - 1);

        for (; index < static_cast<int16_t>(count - 1); index++)
        {
            selectedLances[index] = selectedLances[index + 1];
        }

        selectedLances[index] = nullptr;
    }

    for (int32_t i = 0; i < group->numMovers; i++)
    {
        DeselectMech(group->movers[i] != nullptr ? group->movers[i]->partId : -1);
    }
}

auto InterfaceObject::ClearMechSelection() -> void
{
    ObjectEvent deselect;

    for (int32_t i = 0; i < 0xc; i++)
    {
        FriendlyMechIcon* icon = mechBar->getButton(static_cast<int16_t>(i));

        if (scenarioCallback != nullptr && icon != nullptr && icon->mover != nullptr)
        {
            deselect.init(0x1d, nullptr);
            icon->mover->handleEvent(&deselect);
        }

        selectedMechs[i] = 0;
    }

    numSelectedMechs = 0;
    numSelectedLances = 0;

    for (MoverGroup*& lance : selectedLances)
    {
        lance = nullptr;
    }

    commandParser->ClearSubjects();
}

auto InterfaceObject::IsOurs(int16_t partId) -> int
{
    aMechBar* bar = mechBar;

    for (int16_t i = 0; i < bar->layout.numButtons; i++)
    {
        if (bar->getButton(i) != nullptr && bar->getButton(i)->partId == partId)
        {
            return 1;
        }
    }

    return 0;
}

auto InterfaceObject::ObjectAttacked(int32_t) -> void
{
}

namespace
{
    /// <summary>
    /// Writes into <paramref name="name"/> the name of the network player whose mover roster holds
    /// <paramref name="object"/>, or an empty string.
    /// </summary>
    void netPlayerName(const BaseObject* object, char* name)
    {
        int32_t player = -1;

        for (int32_t p = 0; p < 6 && player == -1; p++)
        {
            for (int32_t i = 0; i < 12; i++)
            {
                const Mover* mover = MPlayer->playerMoverRoster[p][i];

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

        if (player != -1 && MPlayer->sessionManager->GetPlayerNumber(player) != nullptr)
        {
            sprintf(name, "%s", MPlayer->sessionManager->GetPlayerNumber(player)->name);
        }
    }
}

auto InterfaceObject::UpdateMouseState(aEvent* event) -> void
{
    aFloatHelp* tag = floatingTags[0];
    ObjectEvent objectEvent;
    aEvent cursorEvent;
    char text[256];
    // Port fix: cleared. For a terrain object of an unknown kind the original formats the tag from this buffer
    // without loading anything into it.
    char format[256] = {};
    cursorOffset = 0;

    if (screenWindow == nullptr || objectList == nullptr)
    {
        return;
    }

    if (event == nullptr)
    {
        // The per-frame call: a mouse event at the cursor, over the window under it (the map, not the mech bar).
        const MCPoint cursor = MCInput::GetCursorPos();
        cursorEvent.clear();
        cursorEvent.x = cursor.x;
        cursorEvent.y = cursor.y;
        cursorEvent.target = screenWindow->findObject(cursor.x, cursor.y);

        if (cursorEvent.target == mechBar)
        {
            cursorEvent.target = mainHolder->GetActivePane();
        }

        if (cursorEvent.target == nullptr)
        {
            return;
        }

        event = &cursorEvent;
    }

    aObject* window = event->target;

    if (window->objectType == 7)
    {
        return;
    }

    if (window == nullptr || window->parent != mechBar)
    {
        HideTags();
        mechBar->layout.highlightId = -1;
    }

    // The floating tag, in the colours the object calls for.
    auto showTag = [&](uint8_t backColor, uint8_t textColor)
    {
        tag->setBackColor(backColor);
        tag->textColor = textColor;
        tag->SetHelpText(text);
        tag->ShowGUIWindow(1);
    };

    // What is under the mouse.
    if (window->GetCamera() == nullptr)
    {
        if (window->objectType == 8)
        {
            // A mech icon: its mover.
            mouseObjectType = 0;
            mouseObject = static_cast<aMechIcon*>(window)->mover;
        }
    }
    else
    {
        objectEvent.init(0, event);
        auto* object = static_cast<GameObject*>(objectList->findObjectFromEvent(&objectEvent));
        mouseObject = object;

        if (object == nullptr)
        {
            mouseObjectType = 7;
        }
        else
        {
            int tagged = 0;
            const int32_t contactType = object->getContactType(homeTeam->id, tagged);

            if (tagged == 0 && contactType == 2)
            {
                mouseObjectType = 4;
            }
            else if (homeTeam->lineOfSight(object->getPosition()) == 0)
            {
                mouseObjectType = 7;
            }
            else
            {
                const auto isScrap = [object]()
                {
                    return object->objectClass == MISCTERRAINOBJECT &&
                           static_cast<MiscTerrainObject*>(object)->terrainObjectKind == 5;
                };

                switch (static_cast<int32_t>(object->objectClass))
                {
                    case BATTLEMECH:
                    case GROUNDVEHICLE:
                    {
                        auto* mover = static_cast<Mover*>(object);
                        tag->helpObject = object;

                        if (mover->netPlayerId >= 0 && mover->isCaptureable() == 0)
                        {
                            // The player's own: pilot and mover, and the mech bar highlights its icon.
                            if (mover->getAwake() != 0)
                            {
                                if (MPlayer != nullptr)
                                {
                                    netPlayerName(object, format);
                                    sprintf(text, "%s\n%s\n%s", mover->getPilot()->callsign, mover->getIfaceName(),
                                            format);
                                }
                                else
                                {
                                    sprintf(text, "%s\n%s", mover->getPilot()->callsign, mover->getIfaceName());
                                }
                            }
                            else
                            {
                                cLoadString(thisInstance, mover->isCaptureable() != 0 ? 0x99 : 0x9a, format, 0xfe);
                                sprintf(text, format, mover->getIfaceName());
                            }

                            mouseObjectType = 0;
                            showTag(0, 0xb);
                            mechBar->layout.highlightId = object->partId;
                            mechBar->draw();
                            break;
                        }

                        if ((mover->isCaptured() != 0 && mover->getAlignment() == homeTeam->alignment) ||
                            alliedTeam == mover->getTeam())
                        {
                            // Captured by the player, or an ally.
                            sprintf(text, "%s", mover->getIfaceName());
                            mouseObjectType = 3;
                            showTag(0x1f, 0xc);
                            break;
                        }

                        if (MPlayer != nullptr && mover->getAlignment() == homeTeam->alignment)
                        {
                            // A teammate's mover.
                            netPlayerName(object, format);
                            sprintf(text, "%s\n%s\n%s", mover->getPilot()->callsign, mover->getIfaceName(), format);
                            mouseObjectType = 3;
                            showTag(0, 0xb);
                            break;
                        }

                        if (contactType != 1)
                        {
                            break;
                        }

                        // An enemy in sight.
                        if (mover->isDisabled() != 0)
                        {
                            mouseObjectType = 2;
                        }
                        else if (mover->isDestroyed() == 0)
                        {
                            mouseObjectType = 1;
                        }

                        if (mover->isCaptureable() != 0)
                        {
                            cLoadString(thisInstance, 0x99, format, 0xfe);
                            sprintf(text, format, mover->getIfaceName());
                        }
                        else if (mover->getAwake() != 0)
                        {
                            if (MPlayer != nullptr)
                            {
                                netPlayerName(object, format);
                                sprintf(text, "%s\n%s", mover->getIfaceName(), format);
                            }
                            else
                            {
                                sprintf(text, "%s", mover->getIfaceName());
                            }
                        }
                        else
                        {
                            cLoadString(thisInstance, object->objectClass == BATTLEMECH ? 0x9c : 0x9d, format, 0xfe);
                            sprintf(text, format, mover->getIfaceName());
                        }

                        showTag(0, 0xef);
                        break;
                    }

                    case ARTILLERY:
                    case DEBRIS:
                    case 0x14:
                    {
                        mouseObjectType = 7;
                        mouseObject = nullptr;
                        break;
                    }
                    case BUILDING:
                    case MISCTERRAINOBJECT:
                    case TREEBUILDING:
                    case TURRET:
                    case GATE:
                    {
                        if (object->objectClass == BUILDING)
                        {
                            sprintf(text, "%s", static_cast<Building*>(object)->name);
                        }

                        if (object->objectClass == TREEBUILDING)
                        {
                            sprintf(text, "%s", static_cast<TreeBuilding*>(object)->name);
                        }

                        if (object->objectClass == TURRET)
                        {
                            sprintf(text, "%s", static_cast<Turret*>(object)->name);
                        }

                        if (object->objectClass == GATE)
                        {
                            sprintf(text, "%s", static_cast<Gate*>(object)->name);
                        }

                        if (object->objectClass == MISCTERRAINOBJECT)
                        {
                            uint32_t stringId = 0;

                            switch (static_cast<MiscTerrainObject*>(object)->terrainObjectKind)
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
                                cLoadString(thisInstance, stringId, format, 0xfe);
                            }

                            sprintf(text, format);
                        }

                        if (highlightedObject != nullptr)
                        {
                            highlightedObject->setSelected(0);
                            highlightedObject = nullptr;
                        }

                        // A turret shows its tag only while deployed (or fixed).
                        if (object->objectClass != TURRET || static_cast<Turret*>(object)->weaponDeployed != 0 ||
                            static_cast<Turret*>(object)->fixedTurret != 0)
                        {
                            highlightedObject = object;
                            object->setSelected(1);
                            tag->helpObject = object;

                            if (object->isCaptured() != 0 && object->getAlignment() == homeTeam->alignment)
                            {
                                showTag(0x1f, 0xc);
                            }
                            else if (object->getAlignment() == homeTeam->alignment)
                            {
                                showTag(0, 0xb);
                            }
                            else if (object->getAlignment() != homeTeam->alignment &&
                                     homeTeam->lineOfSight(object->getPosition()) != 0)
                            {
                                showTag(0, 0xef);
                            }
                        }

                        [[fallthrough]];
                    }
                    default:
                    {
                        if (object->getAlignment() == homeTeam->alignment || object->isDestroyed() != 0 || isScrap())
                        {
                            mouseObjectType = 5;
                        }
                        else
                        {
                            mouseObjectType = 6;
                        }
                        break;
                    }
                    case CAMERADRONE:
                    {
                        mouseObjectType = 5;
                        cLoadString(thisInstance, 0x96, format, 0xfe);
                        sprintf(text, format);
                        tag->helpObject = object;

                        if (object->getAlignment() == homeTeam->alignment)
                        {
                            showTag(0, 0xb);
                        }
                        else if (object->getAlignment() != homeTeam->alignment &&
                                 homeTeam->lineOfSight(object->getPosition()) != 0)
                        {
                            showTag(0, 0xef);
                        }
                        break;
                    }
                    case TRAINCAR:
                    {
                        tag->helpObject = object;
                        sprintf(text, "%s", static_cast<TrainCar*>(object)->name);
                        tag->setBackColor(0x1f);
                        tag->textColor = 0xc;

                        if (object->isCaptured() != 0)
                        {
                            showTag(0x1f, 0xc);
                        }
                        else if (object->getAlignment() == homeTeam->alignment)
                        {
                            showTag(0, 0xb);
                        }
                        else if (object->getAlignment() != homeTeam->alignment &&
                                 homeTeam->lineOfSight(object->getPosition()) != 0)
                        {
                            showTag(0, 0xef);
                        }

                        mouseObjectType = object->getAlignment() == homeTeam->alignment ? 3 : 1;
                        break;
                    }
                }
            }
        }
    }

    const int32_t mouseY = event->y - window->globalY();
    const int32_t mouseX = event->x - window->globalX();
    vector_2d mousePos(static_cast<float>(mouseX), static_cast<float>(mouseY));
    auto setCursor = [](int32_t cursor) { application->SetCurrentCursor(static_cast<CursorType>(cursor)); };

    // A forced order (see handleEvent): a move, a move-and-attack (command 3) or a jump to the point.
    if (forceOrderActive != 0)
    {
        bool allowed = window->GetCamera() != nullptr;

        for (int32_t i = 0; allowed && i < numSelectedMechs; i++)
        {
            BaseObject* object = objectList->findObjectFromPart(selectedMechs[i]);

            // A mover whose order queue is full takes no more.
            if (isMoverClass(object) && static_cast<Mover*>(object)->getPilot() != nullptr &&
                static_cast<Mover*>(object)->getPilot()->getTacOrderQueue(nullptr) >= 0xf)
            {
                allowed = false;
            }
        }

        if (allowed)
        {
            if (currentCommand != 0x1f && currentCommand != 3 && currentCommand != 0x11 &&
                application->cursorHidden == 0)
            {
                currentCommand = 0;
                commandOneShot = 0;
            }

            vector_3d point;

            if (mouseObject != nullptr)
            {
                point = static_cast<GameObject*>(mouseObject)->getPosition();
            }
            else
            {
                window->GetCamera()->inverseProject(mousePos, point);
            }

            const int32_t command = currentCommand;

            if (command != 0x11)
            {
                if (GameMap->cellPassable(point) != 0)
                {
                    setCursorOffset(mousePos);

                    if (currentCommand == 3)
                    {
                        setCursor(0x10);
                        forceOrderType = 1;
                        return;
                    }

                    setCursor(0xf);
                    forceOrderType = 0;
                    return;
                }

                setCursor(8);
                forceOrderType = command == 3 ? 1 : 0;
                return;
            }

            if (canSelectionJumpTo(point, nullptr, forceOrderActive) != 0)
            {
                setCursorOffset(mousePos);
                setCursor(0x11);
                forceOrderType = 2;
                return;
            }
        }

        setCursor(8);
        forceOrderType = -1;
        return;
    }

    // Refit and repair modes follow what the mouse is over.
    if (currentCommand == 9 && refitCheck(static_cast<GameObject*>(mouseObject)) == 0)
    {
        currentCommand = 0;
        commandOneShot = 0;
    }
    else if (currentCommand == 0 && refitCheck(static_cast<GameObject*>(mouseObject)) != 0)
    {
        currentCommand = 9;
        commandOneShot = 0;
    }

    if (currentCommand == 10 && getFixedCheck(static_cast<GameObject*>(mouseObject)) == 0)
    {
        currentCommand = 0;
        commandOneShot = 0;
    }
    else if (currentCommand == 0 && getFixedCheck(static_cast<GameObject*>(mouseObject)) != 0)
    {
        currentCommand = 10;
        commandOneShot = 0;
    }

    // Port: the original asserts that mouseObject and homeTeam are null or readable (" Mouseobject is bad!!! ",
    // " homeTeam is bad!!! "), probing them with Win32's IsBadReadPtr. The port has no memory probe and treats a
    // non-null pointer as readable (as aObject does), so both asserts always pass and are left out.
    auto* object = static_cast<GameObject*>(mouseObject);
    canCapture = 0;
    captureBlocked = 0;

    if (object != nullptr && object->isCaptureable() != 0 && (currentCommand == 0 || currentCommand == 3) &&
        object->getAlignment() != homeTeam->alignment && homeTeam->lineOfSight(object->getPosition()) != 0)
    {
        canCapture = 1;
        captureBlocked = object->getCaptureBlocker(homeTeam->alignment) != nullptr ? 1 : 0;
    }

    if (highlightedObject != nullptr && (object == nullptr || object->isBuilding() == 0))
    {
        highlightedObject->setSelected(0);
        highlightedObject = nullptr;
    }

    Terrain::terrainTacticalMap->updateOrderPalette();

    if (window->GetCamera() == nullptr && window->objectType != 8)
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
        else if (mouseObject != nullptr && mouseObject->objectClass == BATTLEMECH)
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
        setCursorOffset(mousePos);
        setCursor(currentCommand == 3 ? 0x10 : 0xf);
    };

    auto jumpCursor = [&](vector_3d point)
    {
        if (canSelectionJumpTo(point, nullptr, forceOrderActive) != 0)
        {
            setCursorOffset(mousePos);
            setCursor(0x11);
        }
        else
        {
            setCursor(8);
        }
    };

    // The world point under the mouse; false without a camera.
    auto mousePoint = [&](vector_3d& point)
    {
        Camera* camera = window->GetCamera();

        if (camera == nullptr)
        {
            return false;
        }

        camera->inverseProject(mousePos, point);
        return true;
    };

    auto captureCursor = [&]() { setCursor(captureBlocked != 0 ? 0xc : 0xb); };
    // Over an ally, or (from the enemy case) over something not revealed: the plain command cursors.
    auto allyCursor = [&]()
    {
        switch (currentCommand)
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
                vector_3d point;

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
                setCursor(mouseObjectType == 3 ? 0xe : 0xf);
                return;
            }
            default:
            {
                if (AnySelected(0) == 0)
                {
                    return;
                }

                if (canCapture != 0)
                {
                    captureCursor();
                    return;
                }

                vector_3d point;

                if (!mousePoint(point))
                {
                    return;
                }

                if (GameMap->cellPassable(point) != 0)
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

    if (currentCommand == 0x33)
    {
        // Command 0x33 wants a revealed mover (not an elemental).
        if (object != nullptr && isMoverClass(object) && object->objectClass != ELEMENTAL && object->isRevealed() != 0)
        {
            setCursor(0xe);
        }
        else
        {
            setCursor(8);
        }

        return;
    }

    if (AnySelected(0) == 0 || currentCommand == 0x4a)
    {
        setCursor(0);
        return;
    }

    switch (mouseObjectType)
    {
        case 0:
        {
            // A mover of the player's (or its icon).
            switch (currentCommand)
            {
                case 3:
                {
                    setCursorOffset(mousePos);
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
                    vector_3d point;

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
            switch (currentCommand)
            {
                case 3:
                {
                    setCursorOffset(mousePos);
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
                    if (mouseObject != nullptr)
                    {
                        // Jumping onto a mover attacks it (the attack cursor); onto anything else, to its place.
                        const vector_3d point = static_cast<GameObject*>(mouseObject)->getPosition();

                        if (isMoverClass(mouseObject))
                        {
                            setCursor(canSelectionJumpTo(point, nullptr, forceOrderActive) != 0 ? 1 : 8);
                            return;
                        }

                        jumpCursor(point);
                        return;
                    }
                    else
                    {
                        vector_3d point;

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
                    setCursor(mouseObjectType == 1 ? 0xe : 0xf);
                    return;
                }
                default:
                {
                    if (canCapture != 0)
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
            switch (currentCommand)
            {
                case 3:
                {
                    setCursorOffset(mousePos);
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
            vector_3d point;

            if (!mousePoint(point))
            {
                return;
            }

            if (GameMap->cellPassable(point) != 0)
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
            if (currentCommand == 10)
            {
                setCursor(10);
                return;
            }

            if (AnySelected(0) == 0 || mouseObjectType != 5)
            {
                allyCursor();
                return;
            }

            if (object->isRevealed() == 0)
            {
                moveCursor();
                return;
            }

            int32_t cursor = 8;

            switch (currentCommand)
            {
                case 3:
                {
                    setCursorOffset(mousePos);
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
                    if (canSelectionJumpTo(object->getPosition(), nullptr, forceOrderActive) != 0)
                    {
                        setCursorOffset(mousePos);
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
                    if (static_cast<uint8_t>(object->status) == 2 || static_cast<uint8_t>(object->status) == 1 ||
                        (object->objectClass == MISCTERRAINOBJECT &&
                         static_cast<MiscTerrainObject*>(object)->terrainObjectKind == 5) ||
                        object->getAlignment() == homeTeam->alignment)
                    {
                        setCursorOffset(mousePos);
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

            if (canCapture != 0)
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
    /// The strike types <see cref="InterfaceObject::CallStrike"/> takes (artillery object type numbers), each with
    /// the <c>CallArtillery</c> strike type for team 0 and team 1.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0079622c, 7 entries of 3 longs.</remarks>
    struct StrikeTypeEntry
    {
        int32_t objectType;
        int32_t strikeType[2];
    };

    constexpr StrikeTypeEntry StrikeTypes[7] = {
        {0xf9, {0, 4}},  {0xf8, {1, 5}},  {0xfa, {2, 6}},  {0x1fc, {0, 4}},
        {0x1fb, {1, 5}}, {0x1fd, {2, 6}}, {0x204, {3, 7}},
    };

    /// <summary>
    /// Whether <paramref name="mover"/> can jump from where it will be (its last queued order's point when
    /// <paramref name="fromWayPoint"/> and it has queued orders, else where it is) to <paramref name="position"/>.
    /// </summary>
    bool inJumpRange(Mover* mover, const vector_3d& position, bool fromWayPoint)
    {
        vector_3d from;
        const int32_t numQueued = mover->getPilot()->getTacOrderQueue(nullptr);

        if (numQueued > 0 && fromWayPoint)
        {
            _QueuedTacOrder queue[MAX_QUEUED_TACORDERS_PER_WARRIOR];
            mover->getPilot()->getTacOrderQueue(queue);
            from = queue[numQueued - 1].point;
        }
        else
        {
            from = mover->getPosition();
        }

        // The x87 code keeps dx and dy at full precision and rounds dz to a float.
        const double dx = static_cast<double>(from.x) - position.x;
        const double dy = static_cast<double>(from.y) - position.y;
        const float dz = from.z - position.z;
        const auto distance = static_cast<float>(std::sqrt(dy * dy + static_cast<double>(dz) * dz + dx * dx));
        return !(mover->getJumpRange(nullptr, nullptr) < distance);
    }
} // namespace

auto InterfaceObject::CallStrike(int strikeType, vector_3d* position, GameObject* target, int forCommander,
                                 int forClans, float delay) -> void
{
    if (strikeType != 0xf8 && strikeType != 0xf9 && strikeType != 0xfa && strikeType != 0x1fb && strikeType != 0x1fc &&
        strikeType != 0x1fd && strikeType != 0x204)
    {
        return;
    }

    vector_3d targetPosition;

    if (position == nullptr)
    {
        if (target == nullptr)
        {
            return;
        }

        targetPosition = target->getPosition();
        position = &targetPosition;
    }

    const vector_3d location = *position;

    int32_t commanderId = 0;
    Team* team = innerSphereTeam;

    if (forCommander != 0)
    {
        commanderId = HomeCommander->getId();
        team = HomeCommander->getTeam();
    }
    else if (forClans != 0)
    {
        if (MPlayer != nullptr)
        {
            Fatal(0, " Iface.CallStrike: Need more info than clanStrike in MPlayer ");
        }

        team = clanTeam;
        commanderId = 1;
    }

    // Artillery and sensor strikes must be aimed at a point the team can see; the others can go anywhere.
    int32_t artilleryType = 0;

    for (const StrikeTypeEntry& entry : StrikeTypes)
    {
        if (entry.objectType == strikeType)
        {
            artilleryType = entry.strikeType[team->id];
            break;
        }
    }

    const bool needsSight = artilleryType == 0 || artilleryType == 1 || artilleryType == 4 || artilleryType == 5;

    if (needsSight && team->lineOfSight(location) == 0)
    {
        soundSystem->playDigitalSample(0x33, 1, nullptr, 0, 0);
        return;
    }

    const auto seconds = static_cast<int32_t>(delay);

    if (MPlayer != nullptr && MPlayer->isServer == 0)
    {
        MPlayer->sendPlayerArtillery(MPlayer->serverID, artilleryType, location, seconds);
        return;
    }

    CallArtillery(commanderId, artilleryType, location, seconds, 0);
}

auto InterfaceObject::AddSalvageIcon(GameObject* object) -> void
{
    auto* node = new aSalvageIcon::SalvageNode;

    if (node != nullptr)
    {
        node->object = object;
        node->next = salvageIcon->objects;
        salvageIcon->objects = node;
    }
}

auto InterfaceObject::HideTags() -> void
{
    for (aFloatHelp* tag : floatingTags)
    {
        tag->ShowGUIWindow(0);
    }
}

auto InterfaceObject::WhackTags() -> void
{
    HideTags();

    for (aFloatHelp* tag : floatingTags)
    {
        tag->tossBitmaps();
    }
}

auto InterfaceObject::canSelectionJump() -> int
{
    int result = 0;

    for (int32_t i = 0; i < numSelectedMechs; i++)
    {
        BaseObject* object = objectList->findObjectFromPart(selectedMechs[i]);

        if (!isMoverClass(object) || static_cast<Mover*>(object)->canJump() == 0)
        {
            return 0;
        }

        result = 1;
    }

    return result;
}

auto InterfaceObject::canSelectionJumpTo(vector_3d position, GameObject* target, int fromWayPoint) -> int
{
    int result = 1;

    // Not onto one of the player's own movers.
    if (target != nullptr && isMoverClass(target) && target->getTeam() == homeTeam)
    {
        return 0;
    }

    if (GameMap->cellPassable(position) == 0)
    {
        return 0;
    }

    const int16_t numMovers = numSelectedMechs;

    for (int32_t i = 0; i < numMovers; i++)
    {
        BaseObject* object = objectList->findObjectFromPart(selectedMechs[i]);

        if (isMoverClass(object) && static_cast<Mover*>(object)->getPilot() != nullptr &&
            !inJumpRange(static_cast<Mover*>(object), position, fromWayPoint != 0))
        {
            return 0;
        }
    }

    // The lances' movers jump from where they are or their last way point.
    for (int32_t i = 0; i < numSelectedLances; i++)
    {
        MoverGroup* group = selectedLances[i];

        if (group == nullptr)
        {
            continue;
        }

        for (int32_t j = 0; j < group->numMovers; j++)
        {
            Mover* mover = group->movers[j];

            if (isMoverClass(mover) && mover->getPilot() != nullptr && !inJumpRange(mover, position, true))
            {
                result = 0;
                break;
            }
        }
    }

    return result;
}

auto InterfaceObject::AnySelected(int needsCommand) -> int
{
    int result = 0;

    if (numSelectedMechs == 0 && numSelectedLances == 0)
    {
        return 0;
    }

    if (needsCommand == 0)
    {
        return 1;
    }

    // A live mover with weapons can take an attack command.
    for (int32_t i = 0; i < numSelectedMechs; i++)
    {
        auto* object = static_cast<GameObject*>(objectList->findObjectFromPart(selectedMechs[i]));

        if (object != nullptr && object->isDisabled() == 0 && isMoverClass(object) &&
            static_cast<Mover*>(object)->numWeapons != 0)
        {
            return 1;
        }
    }

    for (int32_t i = 0; i < numSelectedLances; i++)
    {
        MoverGroup* group = selectedLances[i];

        if (group == nullptr)
        {
            continue;
        }

        for (int32_t j = 0; j < group->numMovers; j++)
        {
            if (group->movers[j] != nullptr && group->movers[j]->numWeapons != 0)
            {
                result = 1;
                break;
            }
        }
    }

    return result;
}

auto InterfaceObject::setUnit(int32_t groupId, int32_t numMovers, GameObject** movers, int32_t pointIndex) -> void
{
    auto** moverList = reinterpret_cast<Mover**>(movers);
    HomeCommander->setGroup(groupId, numMovers, moverList, pointIndex);

    if (MPlayer != nullptr)
    {
        MPlayer->sendPlayerMoverGroup(MPlayer->allPlayerGroupID, groupId, numMovers, moverList, pointIndex);
    }

    // Relink every icon to its lance and mark the points.
    for (int32_t i = 0; i < 0xc; i++)
    {
        if (mechBar->getButton(static_cast<int16_t>(i)) != nullptr)
        {
            mechBar->getButton(static_cast<int16_t>(i))->lance = 5;
        }
    }

    for (int32_t lance = 0; lance < 4; lance++)
    {
        MoverGroup* group = HomeCommander->getGroup(lance);

        for (int32_t i = 0; i < group->numMovers; i++)
        {
            if (group->movers[i] != nullptr)
            {
                mechBar->GetButtonFromID(group->movers[i]->partId)->lance = lance;
            }
        }

        Mover* point = group->getPoint();

        if (point != nullptr)
        {
            mechBar->GetButtonFromID(point->partId)->isPoint = 1;
        }

        // Original behaviour: marks the lance icon of groupId each time, not that of the lance just relinked.
        mechBar->GetLanceIconFromID(groupId)->unknown4CC = 1;
    }
}

auto InterfaceObject::setPoint(int32_t partId, int isPoint) -> void
{
    aMechBar* bar = mechBar;

    if (bar->GetButtonFromID(partId) != nullptr)
    {
        bar->GetButtonFromID(partId)->isPoint = isPoint;
    }
}

auto InterfaceObject::setCursorOffset(vector_2d screenPos) -> void
{
    if (Terrain::terrainTacticalMap != nullptr && Terrain::terrainTacticalMap->mouseInside != 0)
    {
        cursorOffset = 6;
    }

    if (numSelectedMechs == 0)
    {
        cursorOffset = 6;
    }

    // The centre of the selected movers on screen.
    const int32_t count = numSelectedMechs;
    float sumX = 0.0f;
    float sumY = 0.0f;

    for (int32_t i = 0; i < count; i++)
    {
        auto* object = static_cast<GameObject*>(objectList->findObjectFromPart(selectedMechs[i]));

        if (object != nullptr)
        {
            sumX += object->getScreenPos(0).x;
            sumY += object->getScreenPos(0).y;
        }
    }

    // The x87 code stores centreX as a float and keeps centreY at full precision.
    const float centerX = sumX / static_cast<float>(count);
    const double centerY = static_cast<double>(sumY) / static_cast<double>(count);
    const auto slope = static_cast<float>(std::fabs(static_cast<double>(screenPos.y) - centerY) /
                                          std::fabs(static_cast<double>(screenPos.x) - centerX));
    int32_t index = 0;

    while (index < 8 && slope > slopeTest[index])
    {
        index++;
    }

    // Written so that a NaN centre (nothing selected) takes the original's branches.
    if (centerY <= screenPos.y)
    {
        if (screenPos.x <= centerX)
        {
            cursorOffset = 0x20 - index;

            if (cursorOffset == 0x20)
            {
                cursorOffset = 0;
            }

            return;
        }

        cursorOffset = index + 0x10;
        return;
    }

    if (centerX <= screenPos.x)
    {
        cursorOffset = 0x10 - index;
        return;
    }

    cursorOffset = index;
}

auto InterfaceObject::refitCheck(GameObject* target) -> int
{
    int result = 0;

    if (numSelectedMechs != 1)
    {
        return 0;
    }

    BaseObject* object = objectList->findObjectFromPart(selectedMechs[0]);

    if (object != nullptr && object->objectClass == GROUNDVEHICLE)
    {
        auto* vehicle = static_cast<Mover*>(object);

        if (0.0f < vehicle->getRefitPoints() && target != nullptr && isMoverClass(target) &&
            static_cast<Mover*>(target)->needsRefit(vehicle->ammoTruck) != 0 &&
            static_cast<Mover*>(target)->netPlayerId > -1)
        {
            result = 1;
        }
    }

    return result;
}

auto InterfaceObject::getFixedCheck(GameObject* target) -> int
{
    int result = 0;

    if (target == nullptr || target->objectClass != TREEBUILDING)
    {
        return 0;
    }

    if (!(0.0f < target->getRefitPoints()))
    {
        return 0;
    }

    if (homeTeam->alignment != target->getAlignment() || numSelectedMechs != 1)
    {
        return 0;
    }

    BaseObject* object = objectList->findObjectFromPart(selectedMechs[0]);

    if (object == nullptr || !isMoverClass(object))
    {
        return 0;
    }

    auto* mover = static_cast<Mover*>(object);
    const int32_t mechBay = static_cast<TreeBuilding*>(target)->mechBay;

    if (mover->needsRefit(0) != 0 &&
        ((mover->objectClass == BATTLEMECH && mechBay != 0) || (mover->objectClass == GROUNDVEHICLE && mechBay == 0)))
    {
        vector_3d bayPosition = target->getPosition();

        if (mover->distanceFrom(bayPosition) < 100.0f)
        {
            result = 1;
        }
    }

    return result;
}

auto InterfaceObject::GetMechIconFromID(int32_t partId) -> FriendlyMechIcon*
{
    FriendlyMechIcon* icon = mechBar->GetButtonFromID(partId);

    if (icon != nullptr)
    {
        return icon;
    }

    for (int32_t i = 0; i < numReserveIcons; i++)
    {
        if (reserveIcons[i] != nullptr && reserveIcons[i]->partId == partId)
        {
            return reserveIcons[i];
        }
    }

    return nullptr;
}
