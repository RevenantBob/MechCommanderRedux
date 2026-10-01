#include "stdafx.h"
#include "gui/ahelp.h"
#include "appear/appear.h"
#include "engine/font.h"
#include "gui/aport.h"
#include "lib/heap.h"
#include "main/main.h"
#include "object/bridge.h"
#include "vfx/vfxfuncs.h"

auto aFloatHelp::init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    setBackColor(0);
    textColor = 0x1f;
    helpText[0] = 0;
    helpObject = nullptr;
    int32_t result = aObject::init(xPos, yPos, width, height, name);
    objectType = 7;
    return result;
}

auto aFloatHelp::tossBitmaps() -> void
{
    guiHeap->free(port()->frame()->window->buffer);
    port()->frame()->window->buffer = nullptr;
}

auto aFloatHelp::draw() -> void
{
    VFX_pane_wipe(displayPort->frame(), backColor());

    if (backColor() != 0)
    {
        drawBox(0, -1, -1, -1, -1);
    }

    if (helpText[0] != 0 && lineFont != nullptr)
    {
        char* newline = strchr(helpText, '\n');
        int16_t lineY = 2;
        lineFont->scale = 1.0f;
        lineFont->scaled = 0;
        lineFont->printToNewline(2, 2, helpText, textColor, displayPort->frame());

        while (newline != nullptr)
        {
            uint8_t lineHeight = lineFont->fontHeight;

            if (lineFont->scaled != 0)
            {
                lineHeight = static_cast<uint8_t>(static_cast<int32_t>(floor(lineHeight * lineFont->scale)));
            }

            lineY = static_cast<int16_t>(lineY + 2 + lineHeight);
            lineFont->printToNewline(2, lineY, newline + 1, textColor, displayPort->frame());
            newline = strchr(newline + 1, '\n');
        }
    }
}

auto aFloatHelp::display() -> void
{
    if (gamePaused != 0 || showWindow == 0 || helpObject == nullptr)
    {
        return;
    }

    float screenX;
    float screenY;

    if (helpObject->objectClass == MISCTERRAINOBJECT)
    {
        vector_2d screenPos = static_cast<MiscTerrainObject*>(helpObject)->getScreenPos();
        screenX = screenPos.x;
        screenY = screenPos.y + 90.0f;
    }
    else
    {
        if (helpObject->getWindowsVisible() != turn)
        {
            return;
        }

        vector_2d screenPos = helpObject->getScreenPos(0);
        screenX = screenPos.x;
        screenY = screenPos.y;

        if (Appearance* appearance = helpObject->getAppearance())
        {
            screenY = appearance->lowerRight.y;
        }

        switch (helpObject->objectClass)
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

    int32_t halfWidth = width() / 2;
    moveTo(static_cast<int32_t>(screenX - static_cast<float>(halfWidth)), static_cast<int32_t>(screenY), 0);
    aObject::display();
}

auto aFloatHelp::SetHelpText(char* text) -> void
{
    if (strlen(text) < 0x40)
    {
        strcpy(helpText, text);
    }
    else
    {
        strncpy(helpText, text, 0x3f);
        // Port fix: the original wrote this terminator at helpText[64], the low byte of helpObject.
        helpText[0x3f] = 0;
    }

    if (lineFont != nullptr)
    {
        int16_t numLines = 1;

        for (char* newline = strchr(text, '\n'); newline != nullptr; newline = strchr(newline + 1, '\n'))
        {
            numLines++;
        }

        lineFont->scale = 1.0f;
        lineFont->scaled = 0;

        int32_t textWidth = lineFont->printWidth(helpText, -1) + 4;

        if (width() == textWidth)
        {
            int32_t lineHeight = 6;

            if (lineFont->scaled != 0)
            {
                lineHeight = static_cast<int16_t>(static_cast<int32_t>(floor(lineFont->scale * 6.0f)));
            }

            if (height() == numLines * lineHeight)
            {
                draw();
                return;
            }
        }

        int32_t fontHeight = lineFont->fontHeight;

        if (lineFont->scaled != 0)
        {
            fontHeight = static_cast<int16_t>(static_cast<int32_t>(floor(fontHeight * lineFont->scale)));
        }

        fontHeight &= 0xff;
        resize(lineFont->printWidth(helpText, -1) + 4, (fontHeight + 2) * numLines);
    }

    draw();
}
