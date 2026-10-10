#include "stdafx.h"
#include "gui/MCFloatHelp.h"
#include "appear/MCAppearance.h"
#include "camera/MCCamera.h"
#include "engine/MCFont.h"
#include "gui/MCGuiSystem.h"
#include "main/MCMissionGlobals.h"
#include "object/MCMiscTerrainObject.h"
#include "object/MCMiscTerrainObjectType.h"
#include "platform/MCFrameLog.h"
#include "vfx/MCVfxFunctions.h"

namespace
{
    /// <summary>The line font's height at its current scale.</summary>
    int32_t ScaledFontHeight(MCFont* font)
    {
        int32_t height = font->FontHeight;

        if (font->Scaled != 0)
        {
            height = static_cast<int32_t>(std::floor(height * font->Scale));
        }

        return height & 0xff;
    }
}

auto MCFloatHelp::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* name) -> int32_t
{
    SetBackColor(0);
    TextColor = 0x1f;
    HelpText.clear();
    HelpObject = nullptr;
    const int32_t result = MCGuiObject::Init(xPos, yPos, width, height, name);
    ObjectType = TagObjectType;
    return result;
}

auto MCFloatHelp::TossBitmaps() -> void
{
    // Port: a tag draws itself through a view, which has no pixels to free.
    if (MCWindow* window = Port()->Frame()->Window; window->Buffer != nullptr)
    {
        MCRenderer::DestroyTexture(window);
        MCGuiPort::FreePixels(window->Buffer);
        window->Buffer = nullptr;
    }
}

auto MCFloatHelp::Draw() -> void
{
    VfxPaneWipe(DisplayPort->Frame(), BackColor());

    if (BackColor() != 0)
    {
        DrawBox(0, -1, -1, -1, -1);
    }

    MCFont* font = LineFont();

    if (HelpText.empty() || font == nullptr)
    {
        return;
    }

    font->Scale = 1.0f;
    font->Scaled = false;
    int32_t lineY = 2;

    for (const auto line : std::views::split(HelpText, '\n'))
    {
        font->PrintToNewline(2, lineY, std::string_view(line), TextColor, DisplayPort->Frame());
        lineY = static_cast<int16_t>(lineY + 2 + ScaledFontHeight(font));
    }
}

auto MCFloatHelp::Display() -> void
{
    if (GamePaused != 0 || !ShowWindow || HelpObject == nullptr)
    {
        return;
    }

    // Port: the tag sits on the screen where the main view shows the object (through the zoom); its offsets are in
    // screen pixels.
    MCViewWindow* view = MCMainView();
    const auto shown = [view](MCVector2D point) { return view != nullptr ? view->WorldToScreen(point) : point; };
    MCVector2D screenPos;

    if (HelpObject->ObjectClass == MCObjectClass::MiscTerrainObject)
    {
        screenPos = static_cast<MCMiscTerrainObject*>(HelpObject)->GetScreenPos();
        screenPos.Y += 90.0f;
        screenPos = shown(screenPos);
    }
    else
    {
        if (HelpObject->GetWindowsVisible() != Turn)
        {
            return;
        }

        screenPos = HelpObject->GetScreenPos(0);

        if (MCAppearance* appearance = HelpObject->GetAppearance())
        {
            screenPos.Y = appearance->LowerRight.Y;
        }

        screenPos = shown(screenPos);

        switch (HelpObject->ObjectClass)
        {
            case MCObjectClass::BattleMech:
                screenPos.Y += 7.0f;
                break;
            case MCObjectClass::GroundVehicle:
            case MCObjectClass::Building:
            case MCObjectClass::TreeBuilding:
                screenPos.Y += 10.0f;
                break;
            default:
                break;
        }
    }

    MoveTo(static_cast<int32_t>(screenPos.X - static_cast<float>(Width() / 2)), static_cast<int32_t>(screenPos.Y));
    MCGuiObject::Display();
}

auto MCFloatHelp::SetHelpText(std::string_view text) -> void
{
    text = text.substr(0, text.find('\0'));

    if (MCFrameLog::Enabled() && HelpText != text)
    {
        std::string logged(text);
        std::ranges::replace(logged, '\n', '/');
        MCFrameLog::Note(std::format("tag text now '{}'", logged));
    }

    HelpText = text;
    MCFont* font = LineFont();

    if (font == nullptr)
    {
        return;
    }

    const auto numLines = static_cast<int32_t>(std::ranges::count(HelpText, '\n')) + 1;
    font->Scale = 1.0f;
    font->Scaled = false;
    const int32_t textWidth = font->PrintWidth(HelpText, true) + 4;

    if (Width() == textWidth)
    {
        // (The original compared the height with 6 a line here, the font's height when it was written.)
        int32_t lineHeight = 6;

        if (font->Scaled != 0)
        {
            lineHeight = static_cast<int16_t>(static_cast<int32_t>(std::floor(font->Scale * 6.0f)));
        }

        if (Height() == numLines * lineHeight)
        {
            return;
        }
    }

    Resize(textWidth, (ScaledFontHeight(font) + 2) * numLines);
}
