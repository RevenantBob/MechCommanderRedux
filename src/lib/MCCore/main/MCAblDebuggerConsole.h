#pragma once

class MCGuiEvent;
class MCGuiObject;

/// <summary>
/// The ABL debugger window's command box routine: on Enter it runs the typed line, an ABL debugger command (break
/// points, watches, trace, step, module selection, printing) or a multiplayer test command ("n..."), and clears the
/// box. The first event points the debugger at the scenario brain.
/// </summary>
void AblDebuggerEventRoutine(MCGuiObject* object, MCGuiEvent* event);
