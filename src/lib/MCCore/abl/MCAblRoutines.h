#pragma once

// Running calls of ABL's standard routines: the language built-ins and the game ("heartbeat") functions unit brains
// and mission scripts call (MCAblCallCompiler.cpp compiles them). MCAblRoutineList.h declares each.

#include "abl/MCAblSymbol.h"

class MCAblRuntime;

/// <summary>Runs the standard routine <paramref name="key"/> (its call is the code being executed).</summary>
/// <returns>The routine's result type (null for none). A key without a routine is fatal, as in MCX.EXE (OB-050).</returns>
MCAblType* ExecStandardRoutineCall(MCAblRuntime& abl, MCAblRoutineKey key);

/// <summary>
/// Writes the mission script messages sent since the last world state to <c>scriptmsg.dbg</c> and the crash report
/// (ChunkDebugMsg).
/// </summary>
void DebugMissionScriptMessages();
