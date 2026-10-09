#include "stdafx.h"
#include "gui/alistbox.h"
#include "gui/MCGuiButton.h"
#include "gui/MCGuiFont.h"
#include "gui/MCGuiPort.h"
#include "gui/ascroll.h"
#include "gui/atextbox.h"
#include "vfx/MCVfxFunctions.h"

namespace
{
    constexpr int32_t MaxItems = 100;
    constexpr int32_t ItemLength = 0x28;

    /// <summary>The message the drop button and <c>CloseListOnMousedown</c> post to the combo box.</summary>
    constexpr int32_t ToggleList = 0x15;

    /// <summary>Cuts <paramref name="text"/> to 39 characters in place and copies it into a string slot.</summary>
    void StoreItemString(char* slot, char* text)
    {
        if (std::strlen(text) > 0x27)
        {
            text[0x27] = '\0';
        }

        std::strcpy(slot, text);
    }
}

MCGuiListBox::MCGuiListBox() = default;

auto MCGuiListBox::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* name) -> int32_t
{
    int32_t result = MCGuiObject::Init(xPos, yPos, width, height, name);

    if (result != 0)
    {
        return result;
    }

    ItemFont = GreyFont;
    ScrollBar = new MCGuiScrollBar;
    ItemHeight = ItemFont->Height() + 8;
    const std::div_t lines = std::div(height, ItemHeight);

    if (lines.rem != 0)
    {
        Resize(width, (ItemHeight - lines.rem) + height);
    }

    ItemStrings = std::make_unique<char[]>(MaxItems * ItemLength);
    SetBackColor(0);
    return 0;
}

auto MCGuiListBox::Destroy() -> void
{
    ItemStrings.reset();

    if (ScrollBar != nullptr)
    {
        // Only an activated bar was initialised.
        if (ScrollBarActive != 0)
        {
            ScrollBar->Destroy();
        }

        delete ScrollBar;
        ScrollBar = nullptr;
    }

    NumItems = 0;
    SelectedItem = -1;
    MCGuiObject::Destroy();
}

auto MCGuiListBox::HandleEvent(MCGuiEvent* event) -> void
{
    const int32_t visibleItems = Height() / ItemHeight;

    // Tells an active scroll bar the new top line without it posting back.
    auto setBarPosition = [this](int32_t position)
    {
        if (ScrollBarActive != 0)
        {
            MCGuiEvent barEvent;
            barEvent.Clear();
            barEvent.Type = 0x6b;
            barEvent.LParam = position;
            ScrollBar->HandleEvent(&barEvent);
        }
    };

    switch (event->Type)
    {
        case 1:
        {
            GuiSystem()->SetText(this);
            GuiSystem()->Grab(this);
            const int32_t item = (event->Y - GlobalY()) / ItemHeight + TopItem;
            SelectedItem = item;

            if (HighlightedItem == item)
            {
                break;
            }

            HighlightedItem = item;
            break;
        }

        case 4:
            GuiSystem()->Release();
            break;
        case 7:
        {
            if (GuiSystem()->GrabbedObject() != this)
            {
                break;
            }

            const int32_t mouseY = event->Y;

            if (mouseY < GlobalY())
            {
                // Dragged above the box: scroll up a line.
                if (TopItem < 1)
                {
                    break;
                }

                const int32_t item = TopItem - 1;
                TopItem = item;

                if (ScrollBarActive != 0)
                {
                    APostMessage(ScrollBar, 0x65);
                }

                SelectedItem = item;

                if (HighlightedItem != item)
                {
                    HighlightedItem = item;
                }

                break;
            }

            if (mouseY > GlobalY() + Height())
            {
                // Dragged below the box: scroll down a line.
                if (TopItem + visibleItems >= NumItems)
                {
                    break;
                }

                const int32_t top = TopItem + 1;
                TopItem = top;

                if (ScrollBarActive != 0)
                {
                    APostMessage(ScrollBar, 0x66);
                }

                const int32_t item = top + visibleItems - 1;
                SelectedItem = item;

                if (HighlightedItem != item)
                {
                    HighlightedItem = item;
                }

                break;
            }

            const int32_t item = (mouseY - GlobalY()) / ItemHeight + TopItem;
            SelectedItem = item;

            if (HighlightedItem == item)
            {
                break;
            }

            HighlightedItem = item;
            break;
        }

        case 5:
        case 9:
        {
            // Types 5 and 9 share the key handling (the binary's jump table).
            switch (event->Key)
            {
                case 0x21: // Page Up
                {
                    TopItem -= Height() / ItemHeight;

                    if (TopItem < 0)
                    {
                        TopItem = 0;
                    }

                    setBarPosition(TopItem);

                    if (SelectedItem > 0)
                    {
                        SelectedItem -= Height() / ItemHeight;

                        if (SelectedItem < 0)
                        {
                            SelectedItem = 0;
                        }

                        HighlightedItem = SelectedItem;
                    }

                    break;
                }
                case 0x22: // Page Down
                {
                    const int32_t count = NumItems;
                    TopItem += visibleItems;

                    if (TopItem + visibleItems > count)
                    {
                        TopItem = count - visibleItems;
                    }

                    setBarPosition(TopItem);

                    if (SelectedItem < count && SelectedItem != -1)
                    {
                        SelectedItem += visibleItems;

                        if (SelectedItem >= count)
                        {
                            SelectedItem = count - 1;
                        }

                        HighlightedItem = SelectedItem;
                    }

                    break;
                }

                case 0x23: // End
                {
                    const int32_t top = NumItems - visibleItems;
                    TopItem = top;
                    setBarPosition(top);
                    SelectedItem = NumItems - 1;
                    HighlightedItem = NumItems - 1;
                    break;
                }

                case 0x24: // Home
                {
                    TopItem = 0;
                    setBarPosition(0);
                    SelectedItem = 0;
                    HighlightedItem = 0;
                    break;
                }
                case 0x26: // Up
                {
                    if (HighlightedItem < 1)
                    {
                        break;
                    }

                    SelectedItem = SelectedItem - 1;

                    if (SelectedItem < TopItem)
                    {
                        TopItem = SelectedItem;

                        if (ScrollBarActive != 0)
                        {
                            APostMessage(ScrollBar, 0x65);
                        }
                    }

                    HighlightedItem = SelectedItem;
                    break;
                }
                case 0x28: // Down
                {
                    if (HighlightedItem >= NumItems - 1 || HighlightedItem == -1)
                    {
                        break;
                    }

                    SelectedItem = SelectedItem + 1;

                    if (SelectedItem >= TopItem + visibleItems)
                    {
                        TopItem = TopItem + 1;

                        if (ScrollBarActive != 0)
                        {
                            APostMessage(ScrollBar, 0x66);
                        }
                    }

                    HighlightedItem = SelectedItem;
                    break;
                }
                default:
                    break;
            }
            break;
        }
        case 2:
        case 0x6c:
        {
            // The scroll bar moved (type 2 shares the case in the binary's jump table).
            int16_t position = ScrollBar->ScrollPos;

            if (position > NumItems - visibleItems)
            {
                position = static_cast<int16_t>(static_cast<int16_t>(NumItems) - static_cast<int16_t>(visibleItems));
            }

            TopItem = position;
            break;
        }

        default:
            break;
    }

    MCGuiObject::HandleEvent(event);
}

auto MCGuiListBox::Draw() -> void
{
    char* strings = ItemStrings.get();
    VfxPaneWipe(DisplayPort->Frame(), BackgroundColor);
    MCGuiObject::Draw();
    VfxLineDraw(DisplayPort->Frame(), 0, 0, Width() - 1, 0, 0xf);
    VfxLineDraw(DisplayPort->Frame(), Width() - 1, 0, Width() - 1, Height() - 1, 0xf);
    VfxLineDraw(DisplayPort->Frame(), 0, Height() - 1, Width() - 1, Height() - 1, 0xf);
    VfxLineDraw(DisplayPort->Frame(), 0, 0, 0, Height() - 1, 0xf);

    int16_t item = static_cast<int16_t>(TopItem);
    char* itemText = strings + TopItem * ItemLength;
    int32_t lineY = 0;

    if (ItemHeight > Height())
    {
        return;
    }

    do
    {
        if (item >= NumItems)
        {
            return;
        }

        if (SelectedItem == item)
        {
            // The selected line's inside is filled with colour 0xb. (The original wrote the port's pixels directly;
            // the box draws itself now, so it wipes the same rectangle.)
            MCPane line = *DisplayPort->Frame();
            line.X0 = 1;
            line.Y0 = lineY + 1;
            line.X1 = Width() - 2;
            line.Y1 = lineY + ItemHeight - 1;
            VfxPaneWipe(&line, 0xb);
        }

        ItemFont->WriteString(DisplayPort->Frame(), 2, lineY + 4, itemText, -1);
        VfxLineDraw(DisplayPort->Frame(), 1, lineY, Width() - 2, lineY, 0xf);
        itemText += ItemLength;
        lineY += ItemHeight;
        item++;
    } while (lineY + ItemHeight <= Height());
}

auto MCGuiListBox::AddItem(char* text) -> int32_t
{
    const int32_t index = NumItems;

    if (index > MaxItems - 1)
    {
        return -0x1111ffff;
    }

    StoreItemString(ItemStrings.get() + index * ItemLength, text);
    NumItems = index + 1;

    if (ScrollBarActive == 0)
    {
        // No scroll bar: the box grows to show every line.
        const int32_t newHeight = (ItemFont->Height() + 8) * (index + 1);
        Resize(Width(), newHeight);
    }
    else
    {
        const int32_t visibleItems = Height() / ItemHeight;

        if (visibleItems < NumItems)
        {
            ScrollBar->SetScrollMax(
                static_cast<int16_t>(static_cast<int16_t>(NumItems) - static_cast<int16_t>(visibleItems)));
            ScrollBar->SetScrollPos(static_cast<int16_t>(TopItem));
        }
    }

    return 0;
}

auto MCGuiListBox::ChangeItemString(int16_t item, char* text) -> int32_t
{
    if (item >= NumItems)
    {
        return -0x1111fffd;
    }

    StoreItemString(ItemStrings.get() + item * ItemLength, text);
    return 0;
}

auto MCGuiListBox::SelectItem(int16_t item) -> int32_t
{
    if (item >= NumItems)
    {
        return -0x1111fffd;
    }

    SelectedItem = item;
    return 0;
}

auto MCGuiListBox::ActivateScrollbar() -> int32_t
{
    const int32_t boxHeight = Height();
    const int32_t lineHeight = ItemHeight;

    if (ScrollBarActive != 0)
    {
        return 0;
    }

    const int32_t result = ScrollBar->Init(10, 10, 0, Height(), nullptr);

    if (result != 0)
    {
        return result;
    }

    AddChild(ScrollBar);
    ScrollBar->MoveTo(Width() + 1, 0, 0);
    ScrollBarActive = 1;
    const int32_t visibleItems = boxHeight / lineHeight;

    if (visibleItems < NumItems)
    {
        ScrollBar->SetScrollMax(
            static_cast<int16_t>(static_cast<int16_t>(NumItems) - static_cast<int16_t>(visibleItems)));
        ScrollBar->SetScrollPos(static_cast<int16_t>(TopItem));
        return 0;
    }

    ScrollBar->ShowGuiWindow(0);
    return 0;
}

auto MCGuiListBox::GetItemString(int16_t item) -> char*
{
    if (item >= NumItems)
    {
        return nullptr;
    }

    return ItemStrings.get() + item * ItemLength;
}

auto CloseListOnMousedown(MCGuiObject* obj, MCGuiEvent* event) -> void
{
    if (event->Type == 1)
    {
        APostMessage(obj->Parent, ToggleList);
    }
}

MCGuiComboBox::MCGuiComboBox() = default;

MCGuiComboBox::~MCGuiComboBox()
{
    Destroy();
}

auto MCGuiComboBox::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* name) -> int32_t
{
    const int32_t result = MCGuiObject::Init(xPos, yPos, width, height, name);

    if (result != 0)
    {
        return result;
    }

    // The original uses each new object unchecked.
    ListBox = new MCGuiListBox;
    ListBox->Init(10, 10, width, height, nullptr);
    ListBox->ShowGuiWindow(0);
    AddChild(ListBox);
    ListBox->MoveTo(2, height + 2, 0);
    ListBox->SetEventRoutine(CloseListOnMousedown);

    // The drop button is sized (height, width): its art sets the real size.
    DropButton = new MCGuiToolButton;
    DropButton->Init(10, 10, height, width, nullptr);
    DropButton->SetUpPicture(0xe);
    DropButton->SetDownPicture(0xf);
    DropButton->Callback()->SetMessage(this, ToggleList);
    AddChild(DropButton);
    const int32_t buttonWidth = DropButton->Width();
    DropButton->MoveTo(this->Width() - (buttonWidth + 2), 2, 0);

    TextField = new MCGuiTextObject;
    TextField->Init(10, 10, (width - (buttonWidth + 2)) - 2, height - 4, nullptr);
    AddChild(TextField);
    TextField->MoveTo(2, 2, 0);
    TextField->ReadOnly = 1;
    SetBackColor(3);
    return 0;
}

auto MCGuiComboBox::Destroy() -> void
{
    auto release = [](auto*& child)
    {
        if (child != nullptr)
        {
            child->Destroy();
            delete child;
            child = nullptr;
        }
    };

    release(TextField);
    release(ListBox);
    release(DropButton);
    MCGuiObject::Destroy();
}

auto MCGuiComboBox::HandleEvent(MCGuiEvent* event) -> void
{
    if (event->Type != ToggleList)
    {
        return;
    }

    MCGuiListBox* list = ListBox;

    if (list->IsShowing() != 0)
    {
        // Close the list and take its selection.
        list->ShowGuiWindow(0);
        TextField->SetText(list->GetItemString(static_cast<int16_t>(list->SelectedItem)));
        DropButton->Pushed = 0;
        return;
    }

    list->ShowGuiWindow(1);
    DropButton->Pushed = 1;
}

auto MCGuiComboBox::Draw() -> void
{
    DrawFramed(0, 1);
    MCGuiObject::Draw();
}

auto MCGuiComboBox::Resize(int32_t newWidth, int32_t newHeight) -> void
{
    MCGuiObject::Resize(newWidth, newHeight);
    ListBox->Resize(newWidth, ListBox->Height());
    ListBox->MoveTo(2, newHeight + 2, 0);
    const int32_t buttonX = newWidth - (DropButton->Width() + 2);
    DropButton->MoveTo(buttonX, 2, 0);
    TextField->Resize(buttonX - 2, newHeight - 4);
}

auto MCGuiComboBox::ChangeItemString(int16_t item, char* text) -> int32_t
{
    MCGuiListBox* list = ListBox;
    const int32_t result = list->ChangeItemString(item, text);

    if (result != 0)
    {
        return result;
    }

    if (item == list->SelectedItem)
    {
        TextField->SetText(text);
    }

    return 0;
}

auto MCGuiComboBox::SelectItem(int16_t item) -> int32_t
{
    MCGuiListBox* list = ListBox;
    const int32_t result = list->SelectItem(item);

    if (result != 0)
    {
        return result;
    }

    TextField->SetText(list->GetItemString(item));
    return 0;
}
