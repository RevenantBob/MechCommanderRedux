#pragma once

#include "gui/asystem.h"

/// <summary>An arrow of an <see cref="aScrollBar"/>: a plain object with the id its event routine reports.</summary>
/// <remarks>
/// 0x4b0 bytes, vtable 0x0077aca0 (aObject's, own destructor only). Its source file isn't named by the line tables;
/// it is only made by <c>aScrollBar::init</c>.
/// </remarks>
class aScrollButton : public aObject
{
public:
    /// <summary>Which arrow: 0x65 up, 0x66 down.</summary>
    uint8_t buttonId = 0; // +0x4ac
};

/// <summary>
/// The empty track of an <see cref="aScrollBar"/> above or below the thumb: an invisible object (no port) whose
/// clicks page the bar.
/// </summary>
/// <remarks>Original source: <c>gui\ascroll.cpp</c> and <c>gui\ascroll.h</c>, 0x4b0 bytes. Vtable 0x0077ab6c.</remarks>
class aScrollArea : public aObject
{
public:
    /// <summary>Places the area and makes its pane, without a port (aObject::init's fields, inlined).</summary>
    /// <remarks>MCX.EXE @ 0x0060d8d0</remarks>
    int32_t init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) override;
    /// <remarks>MCX.EXE @ 0x0060d9b0</remarks>
    void destroy() override;
    /// <summary>Resizes the pane only.</summary>
    /// <remarks>MCX.EXE @ 0x0060da10</remarks>
    void resize(int32_t newWidth, int32_t newHeight) override;
    /// <summary>Draws nothing.</summary>
    /// <remarks>MCX.EXE @ 0x0060d4e0 (gui\ascroll.h)</remarks>
    void draw() override {}
    /// <summary>Displays nothing.</summary>
    /// <remarks>MCX.EXE @ 0x0060d4f0 (gui\ascroll.h)</remarks>
    void display() override {}
    /// <summary>Passes the event to the event routine only.</summary>
    /// <remarks>MCX.EXE @ 0x0060d9f0</remarks>
    void handleEvent(aEvent* event) override;

    /// <summary>Which area: 0x67 above the thumb, 0x68 below.</summary>
    uint8_t areaId = 0; // +0x4ac
};

/// <summary>A vertical scroll bar 18 pixels wide: two arrows, the two track areas and the thumb.</summary>
/// <remarks>
/// Original source: <c>gui\ascroll.cpp</c>, 0x4c4 bytes. Vtable 0x0077aa38. It reports to its parent with
/// messages (<c>ScrollEventHandler</c>, <c>ScrollTabHandler</c>).
/// </remarks>
class aScrollBar : public aObject
{
public:
    /// <remarks>MCX.EXE @ 0x0060d100</remarks>
    aScrollBar();

    /// <summary>Makes the arrows (art packets 0x27/0x28), the areas and the thumb; 18 pixels wide whatever <paramref name="width"/>.</summary>
    /// <returns>0, or 0xeeee0002 (as a negative) when out of memory.</returns>
    /// <remarks>MCX.EXE @ 0x0060d160</remarks>
    int32_t init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) override;
    /// <remarks>MCX.EXE @ 0x0060d560</remarks>
    void destroy() override;
    /// <remarks>MCX.EXE @ 0x0060d6d0</remarks>
    void draw() override;
    /// <remarks>MCX.EXE @ 0x0060d630</remarks>
    void handleEvent(aEvent* event) override;

    /// <summary>Sets the range (0 hides the thumb) and lays the bar out.</summary>
    /// <remarks>MCX.EXE @ 0x0060d7b0</remarks>
    void SetScrollMax(int16_t newMax);
    /// <remarks>MCX.EXE @ 0x0060d7f0</remarks>
    void SetScrollPos(int16_t newPos);
    /// <summary>Places the thumb for the position and sizes the two areas around it.</summary>
    /// <remarks>MCX.EXE @ 0x0060d820</remarks>
    void ResizeAreas();

    int16_t scrollMax = 0;               // +0x4ac
    int16_t scrollPos = 0;               // +0x4ae
    aScrollButton* upButton = nullptr;   // +0x4b0
    aScrollButton* downButton = nullptr; // +0x4b4
    aScrollArea* upArea = nullptr;       // +0x4b8
    aScrollArea* downArea = nullptr;     // +0x4bc
    /// <summary>The thumb (a plain aObject painted by <c>ScrollTabPaint</c>).</summary>
    aObject* scrollTab = nullptr; // +0x4c0
};

/// <summary>The arrows' and areas' event routine: scrolls the parent bar and posts the change to its parent.</summary>
/// <remarks>MCX.EXE @ 0x0060cda0</remarks>
void ScrollEventHandler(aObject* obj, aEvent* event);
/// <summary>The thumb's event routine: drags it.</summary>
/// <remarks>MCX.EXE @ 0x0060ce80</remarks>
void ScrollTabHandler(aObject* obj, aEvent* event);
/// <summary>The thumb's paint routine.</summary>
/// <remarks>MCX.EXE @ 0x0060cfc0</remarks>
void ScrollTabPaint(aObject* obj);
