#pragma once

// The GUI's input (gui\asystem.cpp): window messages become GUI events, which go to the object holding the mouse or
// the keyboard, the modal object or the object under the cursor; the global keys and the cheat codes.

#include "gui/MCGuiEvent.h"

/// <summary>
/// Sends <paramref name="event"/> where it belongs: timer events and posted messages to the tactical interface; keys
/// to the text object; anything else to the object holding the mouse, the results screen, or the object under the
/// cursor, after the global keys (pause, escape, cheats). A modal object only takes events for itself and its children.
/// </summary>
void DispatchGuiEvent(MCGuiEvent* event);

/// <summary>Turns a window message into a GUI event and dispatches it (the mouse wheel is handled here).</summary>
/// <returns>1 when the wheel was handled, else 0.</returns>
int32_t TranslateGuiMessage(uint32_t message, uint32_t wParam, int32_t lParam);

/// <summary>The window procedure (WM_DESTROY, WM_SIZE, WM_PAINT, activation, Alt+Enter, ...).</summary>
int32_t WindowProc(uint32_t message, uint32_t wParam, int32_t lParam);

/// <summary>Switches from full screen to a window.</summary>
void InitWindowMode();
/// <summary>Switches from a window to full screen.</summary>
void InitFullScreen();

/// <summary>Sends mouse button changes and moves as events (a frame callback).</summary>
void CheckMouse();

/// <summary>Scrolls the map when the cursor is at the screen's edge (or by the scroll keys), and the tactical map by
/// its buttons.</summary>
void ScrollScreen();

/// <summary>Where the cursor was when the current message was sent, in window coordinates.</summary>
tagPOINT GetMessageCursorLoc();
