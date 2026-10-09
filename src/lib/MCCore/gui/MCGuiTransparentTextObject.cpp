#include "stdafx.h"
#include "gui/MCGuiTransparentTextObject.h"
#include "camera/MCCamera.h"
#include "gui/MCGuiFont.h"
#include "gui/MCGuiSystem.h"
#include "vfx/MCVfxFunctions.h"

auto MCGuiTransparentTextObject::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* newText)
    -> int32_t
{
    if (const int32_t result = MCGuiObject::Init(xPos, yPos, width, height, nullptr); result != 0)
    {
        return result;
    }

    VfxPaneWipe(DisplayPort->Frame(), 0xff);
    TextColor = 0xfd;
    SetText(newText != nullptr ? std::string_view(newText) : std::string_view());
    return 0;
}

auto MCGuiTransparentTextObject::Draw() -> void
{
    const int32_t lineHeight = LgGreyFont->Height();
    VfxPaneWipe(DisplayPort->Frame(), 0xff);
    int32_t lineY = 0;

    for (const auto line : std::views::split(Text, '\n'))
    {
        LgGreyFont->WriteStringToNewline(Port()->Frame(), 0, lineY, std::string_view(line));
        lineY += lineHeight + 1;
    }

    // Everything drawn (not the 0xff background) becomes the text colour.
    std::span<uint8_t> pixels(Port()->Buffer(), static_cast<size_t>(WinHeight * WinWidth));

    for (uint8_t& pixel : pixels)
    {
        if (pixel != 0xff)
        {
            pixel = TextColor;
        }
    }
}

auto MCGuiTransparentTextObject::Display() -> void
{
    if (!IsShowing() || GlobalPane == nullptr)
    {
        return;
    }

    CopySprite(GlobalPane, DisplayPort->Bitmap(), WinX, WinY, WinWidth, WinHeight, 0, 1);
    DisplayChildren();
}

auto MCGuiTransparentTextObject::SetText(std::string_view newText) -> void
{
    Text = newText.substr(0, newText.find('\0'));
    const int32_t lineHeight = LgGreyFont->Height();
    int32_t textHeight = 0;
    int32_t textWidth = 0;

    // The widest line, and a line height (plus one) for each.
    if (!Text.empty())
    {
        for (const auto line : std::views::split(Text, '\n'))
        {
            textWidth = std::max(textWidth, LgGreyFont->Width(std::string_view(line)));
            textHeight += lineHeight + 1;
        }
    }

    Resize(textWidth, textHeight);
    Draw();
}
