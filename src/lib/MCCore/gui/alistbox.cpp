#include "stdafx.h"
#include "gui/alistbox.h"
#include "gui/abutton.h"
#include "gui/afont.h"
#include "gui/aport.h"
#include "gui/ascroll.h"
#include "gui/atextbox.h"
#include "lib/heap.h"
#include "vfx/vfxfuncs.h"

namespace
{
    constexpr int32_t MaxItems = 100;
    constexpr int32_t ItemLength = 0x28;

    /// <summary>The message the drop button and <c>CloseListOnMousedown</c> post to the combo box.</summary>
    constexpr int32_t ToggleList = 0x15;

    /// <summary>Cuts <paramref name="text"/> to 39 characters in place and copies it into a string slot.</summary>
    void storeItemString(char* slot, char* text)
    {
        if (std::strlen(text) > 0x27)
        {
            text[0x27] = '\0';
        }

        std::strcpy(slot, text);
    }
}

/// <remarks>MCX.EXE @ 0x0060b2b0</remarks>
aListBox::aListBox() = default;

/// <remarks>MCX.EXE @ 0x0060b330</remarks>
auto aListBox::init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    int32_t result = aObject::init(xPos, yPos, width, height, name);

    if (result != 0)
    {
        return result;
    }

    itemFont = greyFont;
    scrollBar = new aScrollBar;
    itemHeight = itemFont->height() + 8;
    const std::div_t lines = std::div(height, itemHeight);

    if (lines.rem != 0)
    {
        resize(width, (itemHeight - lines.rem) + height);
    }

    itemStrings = static_cast<char*>(guiHeap->malloc(MaxItems * ItemLength));

    if (itemStrings == nullptr)
    {
        return -0x4522ffff;
    }

    std::memset(itemStrings, 0, MaxItems * ItemLength);
    setBackColor(0);
    return 0;
}

/// <remarks>MCX.EXE @ 0x0060b410</remarks>
auto aListBox::destroy() -> void
{
    if (itemStrings != nullptr)
    {
        guiHeap->free(itemStrings);
        itemStrings = nullptr;
    }

    if (scrollBar != nullptr)
    {
        // Only an activated bar was initialised.
        if (scrollBarActive != 0)
        {
            scrollBar->destroy();
        }

        delete scrollBar;
        scrollBar = nullptr;
    }

    numItems = 0;
    selectedItem = -1;
    aObject::destroy();
}

/// <remarks>MCX.EXE @ 0x0060b490</remarks>
auto aListBox::handleEvent(aEvent* event) -> void
{
    const int32_t visibleItems = height() / itemHeight;

    // Tells an active scroll bar the new top line without it posting back.
    auto setBarPosition = [this](int32_t position)
    {
        if (scrollBarActive != 0)
        {
            aEvent barEvent;
            barEvent.clear();
            barEvent.type = 0x6b;
            barEvent.lParam = position;
            scrollBar->handleEvent(&barEvent);
        }
    };

    switch (event->type)
    {
        case 1:
        {
            application->setText(this);
            application->grab(this);
            const int32_t item = (event->y - globalY()) / itemHeight + topItem;
            selectedItem = item;

            if (highlightedItem == item)
            {
                break;
            }

            highlightedItem = item;
            break;
        }

        case 4:
            application->release();
            break;
        case 7:
        {
            if (application->grabbedObject() != this)
            {
                break;
            }

            const int32_t mouseY = event->y;

            if (mouseY < globalY())
            {
                // Dragged above the box: scroll up a line.
                if (topItem < 1)
                {
                    break;
                }

                const int32_t item = topItem - 1;
                topItem = item;

                if (scrollBarActive != 0)
                {
                    aPostMessage(scrollBar, 0x65);
                }

                selectedItem = item;

                if (highlightedItem != item)
                {
                    highlightedItem = item;
                }

                break;
            }

            if (mouseY > globalY() + height())
            {
                // Dragged below the box: scroll down a line.
                if (topItem + visibleItems >= numItems)
                {
                    break;
                }

                const int32_t top = topItem + 1;
                topItem = top;

                if (scrollBarActive != 0)
                {
                    aPostMessage(scrollBar, 0x66);
                }

                const int32_t item = top + visibleItems - 1;
                selectedItem = item;

                if (highlightedItem != item)
                {
                    highlightedItem = item;
                }

                break;
            }

            const int32_t item = (mouseY - globalY()) / itemHeight + topItem;
            selectedItem = item;

            if (highlightedItem == item)
            {
                break;
            }

            highlightedItem = item;
            break;
        }

        case 5:
        case 9:
        {
            // Types 5 and 9 share the key handling (the binary's jump table).
            switch (event->key)
            {
                case 0x21: // Page Up
                {
                    topItem -= height() / itemHeight;

                    if (topItem < 0)
                    {
                        topItem = 0;
                    }

                    setBarPosition(topItem);

                    if (selectedItem > 0)
                    {
                        selectedItem -= height() / itemHeight;

                        if (selectedItem < 0)
                        {
                            selectedItem = 0;
                        }

                        highlightedItem = selectedItem;
                    }

                    break;
                }
                case 0x22: // Page Down
                {
                    const int32_t count = numItems;
                    topItem += visibleItems;

                    if (topItem + visibleItems > count)
                    {
                        topItem = count - visibleItems;
                    }

                    setBarPosition(topItem);

                    if (selectedItem < count && selectedItem != -1)
                    {
                        selectedItem += visibleItems;

                        if (selectedItem >= count)
                        {
                            selectedItem = count - 1;
                        }

                        highlightedItem = selectedItem;
                    }

                    break;
                }

                case 0x23: // End
                {
                    const int32_t top = numItems - visibleItems;
                    topItem = top;
                    setBarPosition(top);
                    selectedItem = numItems - 1;
                    highlightedItem = numItems - 1;
                    break;
                }

                case 0x24: // Home
                {
                    topItem = 0;
                    setBarPosition(0);
                    selectedItem = 0;
                    highlightedItem = 0;
                    break;
                }
                case 0x26: // Up
                {
                    if (highlightedItem < 1)
                    {
                        break;
                    }

                    selectedItem = selectedItem - 1;

                    if (selectedItem < topItem)
                    {
                        topItem = selectedItem;

                        if (scrollBarActive != 0)
                        {
                            aPostMessage(scrollBar, 0x65);
                        }
                    }

                    highlightedItem = selectedItem;
                    break;
                }
                case 0x28: // Down
                {
                    if (highlightedItem >= numItems - 1 || highlightedItem == -1)
                    {
                        break;
                    }

                    selectedItem = selectedItem + 1;

                    if (selectedItem >= topItem + visibleItems)
                    {
                        topItem = topItem + 1;

                        if (scrollBarActive != 0)
                        {
                            aPostMessage(scrollBar, 0x66);
                        }
                    }

                    highlightedItem = selectedItem;
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
            int16_t position = scrollBar->scrollPos;

            if (position > numItems - visibleItems)
            {
                position = static_cast<int16_t>(static_cast<int16_t>(numItems) - static_cast<int16_t>(visibleItems));
            }

            topItem = position;
            break;
        }

        default:
            break;
    }

    aObject::handleEvent(event);
}

/// <remarks>MCX.EXE @ 0x0060ba10</remarks>
auto aListBox::draw() -> void
{
    char* strings = itemStrings;
    VFX_pane_wipe(displayPort->frame(), backgroundColor);
    aObject::draw();
    VFX_line_draw(displayPort->frame(), 0, 0, width() - 1, 0, LD_DRAW, 0xf);
    VFX_line_draw(displayPort->frame(), width() - 1, 0, width() - 1, height() - 1, LD_DRAW, 0xf);
    VFX_line_draw(displayPort->frame(), 0, height() - 1, width() - 1, height() - 1, LD_DRAW, 0xf);
    VFX_line_draw(displayPort->frame(), 0, 0, 0, height() - 1, LD_DRAW, 0xf);

    int16_t item = static_cast<int16_t>(topItem);
    char* itemText = strings + topItem * ItemLength;
    int32_t lineY = 0;

    if (itemHeight > height())
    {
        return;
    }

    do
    {
        if (item >= numItems)
        {
            return;
        }

        if (selectedItem == item)
        {
            // The selected line's inside is filled with colour 0xb. (The original wrote the port's pixels directly;
            // the box draws itself now, so it wipes the same rectangle.)
            _pane line = *displayPort->frame();
            line.x0 = 1;
            line.y0 = lineY + 1;
            line.x1 = width() - 2;
            line.y1 = lineY + itemHeight - 1;
            VFX_pane_wipe(&line, 0xb);
        }

        itemFont->writeString(displayPort->frame(), 2, lineY + 4, reinterpret_cast<uint8_t*>(itemText), -1);
        VFX_line_draw(displayPort->frame(), 1, lineY, width() - 2, lineY, LD_DRAW, 0xf);
        itemText += ItemLength;
        lineY += itemHeight;
        item++;
    } while (lineY + itemHeight <= height());
}

/// <remarks>MCX.EXE @ 0x0060bc40</remarks>
auto aListBox::AddItem(char* text) -> int32_t
{
    const int32_t index = numItems;

    if (index > MaxItems - 1)
    {
        return -0x1111ffff;
    }

    storeItemString(itemStrings + index * ItemLength, text);
    numItems = index + 1;

    if (scrollBarActive == 0)
    {
        // No scroll bar: the box grows to show every line.
        const int32_t newHeight = (itemFont->height() + 8) * (index + 1);
        resize(width(), newHeight);
    }
    else
    {
        const int32_t visibleItems = height() / itemHeight;

        if (visibleItems < numItems)
        {
            scrollBar->SetScrollMax(
                static_cast<int16_t>(static_cast<int16_t>(numItems) - static_cast<int16_t>(visibleItems)));
            scrollBar->SetScrollPos(static_cast<int16_t>(topItem));
        }
    }

    return 0;
}

/// <remarks>MCX.EXE @ 0x0060bd20</remarks>
auto aListBox::ChangeItemString(int16_t item, char* text) -> int32_t
{
    if (item >= numItems)
    {
        return -0x1111fffd;
    }

    storeItemString(itemStrings + item * ItemLength, text);
    return 0;
}

/// <remarks>MCX.EXE @ 0x0060bd90</remarks>
auto aListBox::SelectItem(int16_t item) -> int32_t
{
    if (item >= numItems)
    {
        return -0x1111fffd;
    }

    selectedItem = item;
    return 0;
}

/// <remarks>MCX.EXE @ 0x0060bdc0</remarks>
auto aListBox::ActivateScrollbar() -> int32_t
{
    const int32_t boxHeight = height();
    const int32_t lineHeight = itemHeight;

    if (scrollBarActive != 0)
    {
        return 0;
    }

    const int32_t result = scrollBar->init(10, 10, 0, height(), nullptr);

    if (result != 0)
    {
        return result;
    }

    addChild(scrollBar);
    scrollBar->moveTo(width() + 1, 0, 0);
    scrollBarActive = 1;
    const int32_t visibleItems = boxHeight / lineHeight;

    if (visibleItems < numItems)
    {
        scrollBar->SetScrollMax(
            static_cast<int16_t>(static_cast<int16_t>(numItems) - static_cast<int16_t>(visibleItems)));
        scrollBar->SetScrollPos(static_cast<int16_t>(topItem));
        return 0;
    }

    scrollBar->ShowGUIWindow(0);
    return 0;
}

/// <remarks>MCX.EXE @ 0x0060be80</remarks>
auto aListBox::GetItemString(int16_t item) -> char*
{
    if (item >= numItems)
    {
        return nullptr;
    }

    return itemStrings + item * ItemLength;
}

/// <remarks>MCX.EXE @ 0x0060beb0</remarks>
auto CloseListOnMousedown(aObject* obj, aEvent* event) -> void
{
    if (event->type == 1)
    {
        aPostMessage(obj->parent, ToggleList);
    }
}

/// <remarks>MCX.EXE @ 0x0060bee0</remarks>
aComboBox::aComboBox() = default;

/// <remarks>MCX.EXE @ 0x0060bf40</remarks>
aComboBox::~aComboBox()
{
    destroy();
}

/// <remarks>MCX.EXE @ 0x0060bf60</remarks>
auto aComboBox::init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    const int32_t result = aObject::init(xPos, yPos, width, height, name);

    if (result != 0)
    {
        return result;
    }

    // The original uses each new object unchecked.
    listBox = new aListBox;
    listBox->init(10, 10, width, height, nullptr);
    listBox->ShowGUIWindow(0);
    addChild(listBox);
    listBox->moveTo(2, height + 2, 0);
    listBox->setEventRoutine(CloseListOnMousedown);

    // The drop button is sized (height, width): its art sets the real size.
    dropButton = new aToolButton;
    dropButton->init(10, 10, height, width, nullptr);
    dropButton->setUpPicture(0xe);
    dropButton->setDownPicture(0xf);
    dropButton->callback()->setMessage(this, ToggleList);
    addChild(dropButton);
    const int32_t buttonWidth = dropButton->width();
    dropButton->moveTo(this->width() - (buttonWidth + 2), 2, 0);

    textField = new aTextObject;
    textField->init(10, 10, (width - (buttonWidth + 2)) - 2, height - 4, nullptr);
    addChild(textField);
    textField->moveTo(2, 2, 0);
    textField->readOnly = 1;
    setBackColor(3);
    return 0;
}

/// <remarks>MCX.EXE @ 0x0060c180</remarks>
auto aComboBox::destroy() -> void
{
    auto release = [](auto*& child)
    {
        if (child != nullptr)
        {
            child->destroy();
            delete child;
            child = nullptr;
        }
    };

    release(textField);
    release(listBox);
    release(dropButton);
    aObject::destroy();
}

/// <remarks>MCX.EXE @ 0x0060c210</remarks>
auto aComboBox::handleEvent(aEvent* event) -> void
{
    if (event->type != ToggleList)
    {
        return;
    }

    aListBox* list = listBox;

    if (list->IsShowing() != 0)
    {
        // Close the list and take its selection.
        list->ShowGUIWindow(0);
        textField->setText(list->GetItemString(static_cast<int16_t>(list->selectedItem)));
        dropButton->pushed = 0;
        return;
    }

    list->ShowGUIWindow(1);
    dropButton->pushed = 1;
}

/// <remarks>MCX.EXE @ 0x0060c2b0</remarks>
auto aComboBox::draw() -> void
{
    drawFramed(0, 1);
    aObject::draw();
}

/// <remarks>MCX.EXE @ 0x0060c2d0</remarks>
auto aComboBox::resize(int32_t newWidth, int32_t newHeight) -> void
{
    aObject::resize(newWidth, newHeight);
    listBox->resize(newWidth, listBox->height());
    listBox->moveTo(2, newHeight + 2, 0);
    const int32_t buttonX = newWidth - (dropButton->width() + 2);
    dropButton->moveTo(buttonX, 2, 0);
    textField->resize(buttonX - 2, newHeight - 4);
}

/// <remarks>MCX.EXE @ 0x0060c360</remarks>
auto aComboBox::ChangeItemString(int16_t item, char* text) -> int32_t
{
    aListBox* list = listBox;
    const int32_t result = list->ChangeItemString(item, text);

    if (result != 0)
    {
        return result;
    }

    if (item == list->selectedItem)
    {
        textField->setText(text);
    }

    return 0;
}

/// <remarks>MCX.EXE @ 0x0060c3b0</remarks>
auto aComboBox::SelectItem(int16_t item) -> int32_t
{
    aListBox* list = listBox;
    const int32_t result = list->SelectItem(item);

    if (result != 0)
    {
        return result;
    }

    textField->setText(list->GetItemString(item));
    return 0;
}
