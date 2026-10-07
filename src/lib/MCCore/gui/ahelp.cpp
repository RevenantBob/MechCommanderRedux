#include "stdafx.h"
#include "gui/ahelp.h"
#include "appear/MCAppearance.h"
#include "camera/camera.h"
#include "engine/MCFont.h"
#include "gui/aport.h"
#include "main/main.h"
#include "object/bridge.h"
#include "platform/MCFrameLog.h"
#include "vfx/MCVfxFunctions.h"

auto MCFloatHelp::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    SetBackColor(0);
    TextColor = 0x1f;
    HelpText[0] = 0;
    HelpObject = nullptr;
    int32_t result = MCGuiObject::Init(xPos, yPos, width, height, name);
    ObjectType = 7;
    return result;
}

auto MCFloatHelp::TossBitmaps() -> void
{
    // Port: a tag draws itself through a view, which has no pixels to free.
    if (Port()->Frame()->Window->Buffer != nullptr)
    {
        MCRenderer::DestroyTexture(Port()->Frame()->Window);
        MCGuiPort::FreePixels(Port()->Frame()->Window->Buffer);
        Port()->Frame()->Window->Buffer = nullptr;
    }
}

auto MCFloatHelp::Draw() -> void
{
    VfxPaneWipe(DisplayPort->Frame(), BackColor());

    if (BackColor() != 0)
    {
        DrawBox(0, -1, -1, -1, -1);
    }

    if (HelpText[0] != 0 && LineFont != nullptr)
    {
        char* newline = strchr(HelpText, '\n');
        int16_t lineY = 2;
        LineFont->Scale = 1.0f;
        LineFont->Scaled = 0;
        LineFont->PrintToNewline(2, 2, HelpText, TextColor, DisplayPort->Frame());

        while (newline != nullptr)
        {
            uint8_t lineHeight = LineFont->FontHeight;

            if (LineFont->Scaled != 0)
            {
                lineHeight = static_cast<uint8_t>(static_cast<int32_t>(floor(lineHeight * LineFont->Scale)));
            }

            lineY = static_cast<int16_t>(lineY + 2 + lineHeight);
            LineFont->PrintToNewline(2, lineY, newline + 1, TextColor, DisplayPort->Frame());
            newline = strchr(newline + 1, '\n');
        }
    }
}

auto MCFloatHelp::Display() -> void
{
    if (GamePaused != 0 || ShowWindow == 0 || HelpObject == nullptr)
    {
        return;
    }

    float screenX;
    float screenY;
    // Port: the tag sits on the screen where the main view shows the object (through the zoom); its offsets are in
    // screen pixels.
    MCViewWindow* view = MCMainView();
    const auto shown = [view](MCVector2D point) { return view != nullptr ? view->WorldToScreen(point) : point; };

    if (HelpObject->ObjectClass == MISCTERRAINOBJECT)
    {
        MCVector2D screenPos = static_cast<MCMiscTerrainObject*>(HelpObject)->GetScreenPos();
        screenPos.Y += 90.0f;
        screenPos = shown(screenPos);
        screenX = screenPos.X;
        screenY = screenPos.Y;
    }
    else
    {
        if (HelpObject->GetWindowsVisible() != Turn)
        {
            return;
        }

        MCVector2D screenPos = HelpObject->GetScreenPos(0);

        if (MCAppearance* appearance = HelpObject->GetAppearance())
        {
            screenPos.Y = appearance->LowerRight.Y;
        }

        screenPos = shown(screenPos);
        screenX = screenPos.X;
        screenY = screenPos.Y;

        switch (HelpObject->ObjectClass)
        {
            case BATTLEMECH:
                screenY += 7.0f;
                break;
            case GROUNDVEHICLE:
            case BUILDING:
            case TREEBUILDING:
                screenY += 10.0f;
                break;
            default:
                break;
        }
    }

    int32_t halfWidth = Width() / 2;
    MoveTo(static_cast<int32_t>(screenX - static_cast<float>(halfWidth)), static_cast<int32_t>(screenY), 0);
    MCGuiObject::Display();
}

auto MCFloatHelp::SetHelpText(char* text) -> void
{
    if (MCFrameLog::Enabled() && std::strncmp(HelpText, text, 0x3f) != 0)
    {
        std::string shown = text;
        std::ranges::replace(shown, '\n', '/');
        MCFrameLog::Note(std::format("tag text now '{}'", shown));
    }

    if (strlen(text) < 0x40)
    {
        strcpy(HelpText, text);
    }
    else
    {
        strncpy(HelpText, text, 0x3f);
        // Port fix: the original wrote this terminator at helpText[64], the low byte of helpObject.
        HelpText[0x3f] = 0;
    }

    if (LineFont != nullptr)
    {
        int16_t numLines = 1;

        for (char* newline = strchr(text, '\n'); newline != nullptr; newline = strchr(newline + 1, '\n'))
        {
            numLines++;
        }

        LineFont->Scale = 1.0f;
        LineFont->Scaled = 0;

        int32_t textWidth = LineFont->PrintWidth(HelpText, true) + 4;

        if (Width() == textWidth)
        {
            int32_t lineHeight = 6;

            if (LineFont->Scaled != 0)
            {
                lineHeight = static_cast<int16_t>(static_cast<int32_t>(floor(LineFont->Scale * 6.0f)));
            }

            if (Height() == numLines * lineHeight)
            {
                return;
            }
        }

        int32_t fontHeight = LineFont->FontHeight;

        if (LineFont->Scaled != 0)
        {
            fontHeight = static_cast<int16_t>(static_cast<int32_t>(floor(fontHeight * LineFont->Scale)));
        }

        fontHeight &= 0xff;
        Resize(LineFont->PrintWidth(HelpText, true) + 4, (fontHeight + 2) * numLines);
    }
}
