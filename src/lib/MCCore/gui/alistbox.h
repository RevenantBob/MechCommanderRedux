#pragma once

#include "gui/asystem.h"

class MCGuiScrollBar;
class MCGuiTextObject;
class MCGuiToolButton;

/// <summary>A list of up to 100 strings (39 characters each) in the grey font, with a selection and an optional scroll bar.</summary>
/// <remarks>Original source: <c>gui\alistbox.cpp</c>, 0x4d0 bytes. Vtable 0x0077a53c.</remarks>
class MCGuiListBox : public MCGuiObject
{
public:
    MCGuiListBox();

    /// <summary>
    /// Like aObject::init; makes the (inactive) scroll bar and the string table, and rounds the height to whole
    /// lines.
    /// </summary>
    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) override;
    void Destroy() override;
    /// <summary>Draws the visible lines, the selected one highlighted.</summary>
    void Draw() override;
    /// <summary>Port: draws itself each frame from its items, top line and selection.</summary>
    bool DrawsLive() override { return true; }
    /// <summary>Tracks the highlighted line under the mouse, selects on a click, scrolls on the scroll bar's messages.</summary>
    void HandleEvent(MCGuiEvent* event) override;

    /// <summary>Appends <paramref name="text"/> (cut to 39 characters, in place) and grows the box or its scroll range.</summary>
    int32_t AddItem(char* text);
    int32_t ChangeItemString(int16_t item, char* text);
    int32_t SelectItem(int16_t item);
    /// <summary>Adds the scroll bar at the right edge (once).</summary>
    int32_t ActivateScrollbar();
    char* GetItemString(int16_t item);

    int32_t NumItems = 0;
    /// <summary>The selected line, or -1.</summary>
    int32_t SelectedItem = -1;
    /// <summary>The line under the mouse, or -1.</summary>
    int32_t HighlightedItem = -1;
    /// <summary>The first line shown (the scroll position).</summary>
    int32_t TopItem = 0;
    /// <summary>The strings: 100 slots of 40 characters.</summary>
    std::unique_ptr<char[]> ItemStrings;
    MCGuiFont* ItemFont = nullptr;
    MCGuiScrollBar* ScrollBar = nullptr;
    /// <summary>The font's height + 8.</summary>
    int32_t ItemHeight = 0;
    /// <summary>Nonzero once <see cref="ActivateScrollbar"/> added the scroll bar.</summary>
    int32_t ScrollBarActive = 0;
};

/// <summary>A drop-down list: a text field, a drop button and a list box shown below while open.</summary>
/// <remarks>Original source: <c>gui\alistbox.cpp</c>. Vtable 0x0077a670. Its fields end at 0x4b8.</remarks>
class MCGuiComboBox : public MCGuiObject
{
public:
    MCGuiComboBox();
    ~MCGuiComboBox() override;

    /// <summary>Makes the list (hidden, closed by <c>CloseListOnMousedown</c>), the drop button and the text field.</summary>
    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) override;
    void Destroy() override;
    void Draw() override;
    /// <summary>Port: draws itself each frame (its frame; the list, button and text field draw themselves).</summary>
    bool DrawsLive() override { return true; }
    /// <summary>Opens the list on the button's message (0x15); takes the list's selection into the text field.</summary>
    void HandleEvent(MCGuiEvent* event) override;
    void Resize(int32_t newWidth, int32_t newHeight) override;

    /// <summary>Changes a list string (and the text field when it is the selected one).</summary>
    int32_t ChangeItemString(int16_t item, char* text);
    int32_t SelectItem(int16_t item);

    MCGuiTextObject* TextField = nullptr;
    MCGuiListBox* ListBox = nullptr;
    /// <summary>The drop button (an aToolButton, art packets 0xe/0xf).</summary>
    MCGuiToolButton* DropButton = nullptr;
};

/// <summary>A combo box list's event routine: closes the list when the mouse is pressed outside it.</summary>
void CloseListOnMousedown(MCGuiObject* obj, MCGuiEvent* event);
