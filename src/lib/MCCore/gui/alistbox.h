#pragma once

#include "gui/asystem.h"

class aScrollBar;
class aTextObject;
class aToolButton;

/// <summary>A list of up to 100 strings (39 characters each) in the grey font, with a selection and an optional scroll bar.</summary>
/// <remarks>Original source: <c>gui\alistbox.cpp</c>, 0x4d0 bytes. Vtable 0x0077a53c.</remarks>
class aListBox : public aObject
{
public:
    /// <remarks>MCX.EXE @ 0x0060b2b0</remarks>
    aListBox();

    /// <summary>
    /// Like aObject::init; makes the (inactive) scroll bar and the string table, and rounds the height to whole
    /// lines.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0060b330</remarks>
    int32_t init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) override;
    /// <remarks>MCX.EXE @ 0x0060b410</remarks>
    void destroy() override;
    /// <summary>Draws the visible lines, the selected one highlighted.</summary>
    /// <remarks>MCX.EXE @ 0x0060ba10</remarks>
    void draw() override;
    /// <summary>Tracks the highlighted line under the mouse, selects on a click, scrolls on the scroll bar's messages.</summary>
    /// <remarks>MCX.EXE @ 0x0060b490</remarks>
    void handleEvent(aEvent* event) override;

    /// <summary>Appends <paramref name="text"/> (cut to 39 characters, in place) and grows the box or its scroll range.</summary>
    /// <remarks>MCX.EXE @ 0x0060bc40</remarks>
    int32_t AddItem(char* text);
    /// <remarks>MCX.EXE @ 0x0060bd20</remarks>
    int32_t ChangeItemString(int16_t item, char* text);
    /// <remarks>MCX.EXE @ 0x0060bd90</remarks>
    int32_t SelectItem(int16_t item);
    /// <summary>Adds the scroll bar at the right edge (once).</summary>
    /// <remarks>MCX.EXE @ 0x0060bdc0</remarks>
    int32_t ActivateScrollbar();
    /// <remarks>MCX.EXE @ 0x0060be80</remarks>
    char* GetItemString(int16_t item);

    int32_t numItems = 0; // +0x4ac
    /// <summary>The selected line, or -1.</summary>
    int32_t selectedItem = -1; // +0x4b0
    /// <summary>The line under the mouse, or -1.</summary>
    int32_t highlightedItem = -1; // +0x4b4
    /// <summary>The first line shown (the scroll position).</summary>
    int32_t topItem = 0; // +0x4b8
    /// <summary>The strings: 100 slots of 40 characters (GUI heap).</summary>
    char* itemStrings = nullptr;     // +0x4bc
    aFont* itemFont = nullptr;       // +0x4c0
    aScrollBar* scrollBar = nullptr; // +0x4c4
    /// <summary>The font's height + 8.</summary>
    int32_t itemHeight = 0; // +0x4c8
    /// <summary>Nonzero once <see cref="ActivateScrollbar"/> added the scroll bar.</summary>
    int32_t scrollBarActive = 0; // +0x4cc
};

/// <summary>A drop-down list: a text field, a drop button and a list box shown below while open.</summary>
/// <remarks>Original source: <c>gui\alistbox.cpp</c>. Vtable 0x0077a670. Its fields end at 0x4b8.</remarks>
class aComboBox : public aObject
{
public:
    /// <remarks>MCX.EXE @ 0x0060bee0</remarks>
    aComboBox();
    /// <remarks>MCX.EXE @ 0x0060bf40</remarks>
    ~aComboBox() override;

    /// <summary>Makes the list (hidden, closed by <c>CloseListOnMousedown</c>), the drop button and the text field.</summary>
    /// <remarks>MCX.EXE @ 0x0060bf60</remarks>
    int32_t init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) override;
    /// <remarks>MCX.EXE @ 0x0060c180</remarks>
    void destroy() override;
    /// <remarks>MCX.EXE @ 0x0060c2b0</remarks>
    void draw() override;
    /// <summary>Opens the list on the button's message (0x15); takes the list's selection into the text field.</summary>
    /// <remarks>MCX.EXE @ 0x0060c210</remarks>
    void handleEvent(aEvent* event) override;
    /// <remarks>MCX.EXE @ 0x0060c2d0</remarks>
    void resize(int32_t newWidth, int32_t newHeight) override;

    /// <summary>Changes a list string (and the text field when it is the selected one).</summary>
    /// <remarks>MCX.EXE @ 0x0060c360</remarks>
    int32_t ChangeItemString(int16_t item, char* text);
    /// <remarks>MCX.EXE @ 0x0060c3b0</remarks>
    int32_t SelectItem(int16_t item);

    aTextObject* textField = nullptr; // +0x4ac
    aListBox* listBox = nullptr;      // +0x4b0
    /// <summary>The drop button (an aToolButton, art packets 0xe/0xf).</summary>
    aToolButton* dropButton = nullptr; // +0x4b4
};

/// <summary>A combo box list's event routine: closes the list when the mouse is pressed outside it.</summary>
/// <remarks>MCX.EXE @ 0x0060beb0</remarks>
void CloseListOnMousedown(aObject* obj, aEvent* event);
