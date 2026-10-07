#pragma once

// The tactical interface's event routines and debug cheats (iface\icallbk.cpp).

class MCGuiObject;
class MCGuiEvent;

/// <summary>
/// The event routine of each mech bar icon: a click selects the mover (shift toggles it) or, with an attack
/// command active, gives the selection that order on the mover.
/// </summary>
void MechIconHandleEvent(MCGuiObject* icon, MCGuiEvent* event);

/// <summary>Paint routine of the drag-selection box: a transparent pane with a one-pixel frame.</summary>
void PaintSelBox(MCGuiObject* box);

/// <summary>An event routine that passes the event to <c>theInterface</c>. The original name was lost.</summary>
void InterfaceHandleEvent(MCGuiObject* object, MCGuiEvent* event);

/// <summary>Per-frame callback: updates the mouse state once the mission is past its first turn.</summary>
void UpdateMouseStateCallback();

/// <summary>
/// Cheat: restores every live mech bar mover's armour, internal structure, weapons and ammunition.
/// </summary>
void HealAll();

/// <summary>Cheat: sets every live mech bar mover's pilot gunnery skill (+0x24 of the warrior) to 120.</summary>
void DeadEye();

/// <summary>Cheat/test: replaces every live mech bar mover's weapons with component 0x9a.</summary>
void Test3();
