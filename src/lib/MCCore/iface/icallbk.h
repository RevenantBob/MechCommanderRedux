#pragma once

// The tactical interface's event routines and debug cheats (iface\icallbk.cpp).

class aObject;
class aEvent;

/// <summary>
/// The event routine of each mech bar icon: a click selects the mover (shift toggles it) or, with an attack
/// command active, gives the selection that order on the mover.
/// </summary>
/// <remarks>MCX.EXE @ 0x006c7f90</remarks>
void mechIconHandleEvent(aObject* icon, aEvent* event);

/// <summary>Paint routine of the drag-selection box: a transparent pane with a one-pixel frame.</summary>
/// <remarks>MCX.EXE @ 0x006c87f0</remarks>
void PaintSelBox(aObject* box);

/// <summary>An event routine that passes the event to <c>theInterface</c>. The original name was lost.</summary>
/// <remarks>MCX.EXE @ 0x006c8880</remarks>
void interfaceHandleEvent(aObject* object, aEvent* event);

/// <summary>Per-frame callback: updates the mouse state once the mission is past its first turn.</summary>
/// <remarks>MCX.EXE @ 0x006c88a0</remarks>
void UpdateMouseStateCallback();

/// <summary>
/// Cheat: restores every live mech bar mover's armour, internal structure, weapons and ammunition.
/// </summary>
/// <remarks>MCX.EXE @ 0x006c88c0</remarks>
void HealAll();

/// <summary>Cheat: sets every live mech bar mover's pilot gunnery skill (+0x24 of the warrior) to 120.</summary>
/// <remarks>MCX.EXE @ 0x006c8a20</remarks>
void DeadEye();

/// <summary>Cheat/test: replaces every live mech bar mover's weapons with component 0x9a.</summary>
/// <remarks>MCX.EXE @ 0x006c8ac0</remarks>
void Test3();
