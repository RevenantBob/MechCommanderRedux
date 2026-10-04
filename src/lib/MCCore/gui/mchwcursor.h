#pragma once

// Port-only: the game's cursor shown as the system (hardware) cursor instead of drawn into the frame, so it follows
// the mouse at the desktop's rate. While something is dragged (the logistics screens' DragIcon), the dragged picture
// becomes part of the cursor too. PREFS "SoftwareCursor" (gSoftwareCursor) keeps the original's drawn cursor.

#include "platform/MCCursor.h"

class aObject;

/// <summary>
/// A cursor shape as a cursor picture: shape <paramref name="shapeNum"/> of <paramref name="shapeTable"/> as
/// <c>AG_shape_draw</c> draws it, its skipped pixels transparent, its hot spot the shape's origin.
/// </summary>
MCCursorImage MCCursorImageFromShape(void* shapeTable, int32_t shapeNum);

/// <summary>The whole of <paramref name="pane"/> as an opaque cursor picture with its hot spot at (hotX, hotY).</summary>
MCCursorImage MCCursorImageFromPane(PANE* pane, int32_t hotX, int32_t hotY);

/// <summary>Makes the system cursors of every loaded shape in <c>cursorShapes</c>; <c>aSystem::init</c> calls it.</summary>
void MCHardwareCursorPreload();

/// <summary>Forgets what the last frame's cursor carried; <c>UpdateDisplay</c> calls it before the GUI draws.</summary>
void MCHardwareCursorNewFrame();

/// <summary>
/// Called by a dragged object instead of drawing itself: when the system cursor is on and the mouse (where the game
/// last saw it) is over <paramref name="object"/>, takes the object's picture <paramref name="pixels"/> into this
/// frame's cursor, at the same place under the mouse.
/// </summary>
/// <returns>True when the cursor carries the object, which then must not draw itself.</returns>
bool MCHardwareCursorCarry(aObject* object, PANE* pixels);

/// <summary>
/// Shows the current cursor shape (<c>application->cursorShape</c>, -1 for none), over whatever
/// <see cref="MCHardwareCursorCarry"/> took this frame, as the system cursor.
/// </summary>
void MCHardwareCursorUpdate();
