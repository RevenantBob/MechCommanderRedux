#include "stdafx.h"
#include "gui/MCGuiTextObject.h"
#include "gui/MCGuiFont.h"
#include "gui/MCGuiSystem.h"
#include "vfx/MCVfxFunctions.h"

auto MCGuiTextObject::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* newText) -> int32_t
{
    if (const int32_t result = MCGuiObject::Init(xPos, yPos, width, height, nullptr); result != 0)
    {
        return result;
    }

    ReadOnly = false;
    TextFont = GreyFont;
    SetText(newText != nullptr ? std::string_view(newText) : std::string_view());
    BackgroundColor = 0;
    VfxPaneWipe(DisplayPort->Frame(), 0);
    return 0;
}

auto MCGuiTextObject::HandleEvent(MCGuiEvent* event) -> void
{
    // The event routine gets a copy taken before any editing.
    MCGuiEvent eventCopy = *event;

    if (!ReadOnly)
    {
        if (event->Type == MCGuiEventType::LeftButtonDown)
        {
            GuiSystem()->SetText(this);

            if (Parent != nullptr)
            {
                BringToFront(false);
                ARedrawScreen();
            }
        }
        else if (event->Type == MCGuiEventType::Character && GuiSystem()->TextObject() == this)
        {
            const uint8_t key = event->Key;

            if (key == '\b')
            {
                if (TextLength > 0)
                {
                    // Original behaviour (OB-067): clears the character after the caret (the terminator, but for one
                    // left by an earlier backspace), not the last one, so the deleted character stays on show until
                    // the next one typed replaces it.
                    if (std::cmp_greater(Text.size(), TextLength))
                    {
                        Text.resize(static_cast<size_t>(TextLength));
                    }

                    TextLength--;
                }
            }
            else
            {
                if (key == '\r' && Parent != nullptr)
                {
                    APostMessage(Parent, MCGuiEventType::TextEntered);
                }

                if (std::isprint(key) != 0)
                {
                    Text.resize(static_cast<size_t>(TextLength));
                    Text.push_back(static_cast<char>(key));
                    TextLength++;
                }
            }
        }
    }

    if (EventRoutine != nullptr)
    {
        EventRoutine(this, &eventCopy);
    }
}

auto MCGuiTextObject::Draw() -> void
{
    std::string shown = Text;
    VfxPaneWipe(DisplayPort->Frame(), BackgroundColor);
    DrawFramed(false, false);

    if (GuiSystem()->TextObject() == this)
    {
        // The caret.
        shown.resize(static_cast<size_t>(TextLength));
        shown.push_back('\x7f');
    }

    // Drop leading characters until the rest fits.
    std::string_view rest = shown;

    while (!rest.empty() && TextFont->Width(rest) > Width() - 6)
    {
        rest.remove_prefix(1);
    }

    if (!rest.empty())
    {
        TextFont->WriteString(DisplayPort->Frame(), 3, 3, rest);
    }
}

auto MCGuiTextObject::SetText(std::string_view newText) -> void
{
    // The text ends at its first NUL, as the original's C string.
    Text = newText.substr(0, newText.find('\0'));
    TextLength = static_cast<int32_t>(Text.size());
}
