#pragma once

#include "gui/awindow.h"

/// <summary>
/// A debug status window: a title window (close button, no zoom buttons) showing information on one object, redrawn
/// at most every 500 ms (see <see cref="DrawMechInfo"/>).
/// </summary>
/// <remarks>Original source: <c>iface\statwin.cpp</c>, 0x4c8 bytes.</remarks>
class MCInfoWindow : public MCGuiTitleWindow
{
public:
    /// <summary>Makes the window for the object with part id <paramref name="objectPartId"/>.</summary>
    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, int32_t objectPartId);
    /// <summary>Shown while showing (the original redrew it at most every 500 ms; it draws itself each frame now).</summary>
    void Display() override;

    /// <summary>The part id of the object shown.</summary>
    int32_t PartId = 0;
    /// <summary><c>GetTickCount</c> of the last redraw.</summary>
    uint32_t LastUpdateTime = 0;
};

/// <summary>Paint routine of an <see cref="MCInfoWindow"/>: lists the damaged components of its object by location.</summary>
void DrawMechInfo(MCGuiObject* window);
