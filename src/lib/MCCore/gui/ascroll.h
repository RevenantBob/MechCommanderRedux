#pragma once

#include "gui/asystem.h"

/// <summary>An arrow of an <see cref="MCGuiScrollBar"/>: a plain object with the id its event routine reports.</summary>
/// <remarks>
/// 0x4b0 bytes, vtable 0x0077aca0 (aObject's, own destructor only). Its source file isn't named by the line tables;
/// it is only made by <c>aScrollBar::init</c>.
/// </remarks>
class MCGuiScrollButton : public MCGuiObject
{
public:
    /// <summary>Port: an arrow only shows its background picture.</summary>
    bool DrawsLive() override { return true; }

    /// <summary>Which arrow: 0x65 up, 0x66 down.</summary>
    uint8_t ButtonId = 0;
};

/// <summary>
/// The empty track of an <see cref="MCGuiScrollBar"/> above or below the thumb: an invisible object (no port) whose
/// clicks page the bar.
/// </summary>
/// <remarks>Original source: <c>gui\ascroll.cpp</c> and <c>gui\ascroll.h</c>, 0x4b0 bytes. Vtable 0x0077ab6c.</remarks>
class MCGuiScrollArea : public MCGuiObject
{
public:
    /// <summary>Places the area and makes its pane, without a port (aObject::init's fields, inlined).</summary>
    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) override;
    void Destroy() override;
    /// <summary>Resizes the pane only.</summary>
    void Resize(int32_t newWidth, int32_t newHeight) override;
    /// <summary>Draws nothing.</summary>
    void Draw() override {}
    /// <summary>Displays nothing.</summary>
    void Display() override {}
    /// <summary>Passes the event to the event routine only.</summary>
    void HandleEvent(MCGuiEvent* event) override;

    /// <summary>Which area: 0x67 above the thumb, 0x68 below.</summary>
    uint8_t AreaId = 0;
};

/// <summary>A vertical scroll bar 18 pixels wide: two arrows, the two track areas and the thumb.</summary>
/// <remarks>
/// Original source: <c>gui\ascroll.cpp</c>, 0x4c4 bytes. Vtable 0x0077aa38. It reports to its parent with
/// messages (<c>ScrollEventHandler</c>, <c>ScrollTabHandler</c>).
/// </remarks>
class MCGuiScrollBar : public MCGuiObject
{
public:
    MCGuiScrollBar();

    /// <summary>Makes the arrows (art packets 0x27/0x28), the areas and the thumb; 18 pixels wide whatever <paramref name="width"/>.</summary>
    /// <returns>0, or 0xeeee0002 (as a negative) when out of memory.</returns>
    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) override;
    void Destroy() override;
    void Draw() override;
    void HandleEvent(MCGuiEvent* event) override;
    /// <summary>Port: the bar draws itself each frame (its thumb is placed by <see cref="ResizeAreas"/>).</summary>
    bool DrawsLive() override { return true; }

    /// <summary>Sets the range (0 hides the thumb) and lays the bar out.</summary>
    void SetScrollMax(int16_t newMax);
    void SetScrollPos(int16_t newPos);
    /// <summary>Places the thumb for the position and sizes the two areas around it.</summary>
    void ResizeAreas();

    int16_t ScrollMax = 0;
    int16_t ScrollPos = 0;
    MCGuiScrollButton* UpButton = nullptr;
    MCGuiScrollButton* DownButton = nullptr;
    MCGuiScrollArea* UpArea = nullptr;
    MCGuiScrollArea* DownArea = nullptr;
    /// <summary>The thumb (a plain aObject painted by <c>ScrollTabPaint</c>).</summary>
    MCGuiObject* ScrollTab = nullptr;
};

/// <summary>The arrows' and areas' event routine: scrolls the parent bar and posts the change to its parent.</summary>
void ScrollEventHandler(MCGuiObject* obj, MCGuiEvent* event);
/// <summary>The thumb's event routine: drags it.</summary>
void ScrollTabHandler(MCGuiObject* obj, MCGuiEvent* event);
/// <summary>The thumb's paint routine.</summary>
void ScrollTabPaint(MCGuiObject* obj);
