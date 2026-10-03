#pragma once

#include "gui/awindow.h"

/// <summary>
/// A debug status window: a title window (close button, no zoom buttons) showing information on one object, redrawn
/// at most every 500 ms (see <see cref="DrawMechInfo"/>).
/// </summary>
/// <remarks>Original source: <c>iface\statwin.cpp</c>, 0x4c8 bytes.</remarks>
class InfoWindow : public aTitleWindow
{
public:
    /// <summary>Makes the window for the object with part id <paramref name="objectPartId"/>.</summary>
    /// <remarks>MCX.EXE @ 0x00694d00</remarks>
    int32_t init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, int32_t objectPartId);
    /// <summary>Shown while showing (the original redrew it at most every 500 ms; it draws itself each frame now).</summary>
    /// <remarks>MCX.EXE @ 0x00694d80</remarks>
    void display() override;

    /// <summary>The part id of the object shown.</summary>
    int32_t partId = 0; // +0x4c0
    /// <summary><c>GetTickCount</c> of the last redraw.</summary>
    uint32_t lastUpdateTime = 0; // +0x4c4
};

/// <summary>Paint routine of an <see cref="InfoWindow"/>: lists the damaged components of its object by location.</summary>
/// <remarks>MCX.EXE @ 0x00694de0</remarks>
void DrawMechInfo(aObject* window);
