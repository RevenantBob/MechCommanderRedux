#pragma once

// Compiling calls of ABL's standard routines: the language built-ins and the game ("heartbeat") functions unit brains
// and mission scripts call (MCAblRoutines.h runs them).

#include "abl/MCAblSymbol.h"

class MCAblCompiler;

/// <summary>
/// Compiles the argument list of a call of standard routine <paramref name="key"/> (after its name) and checks the
/// argument types.
/// </summary>
/// <returns>The call's result type, or null.</returns>
MCAblType* CompileStandardRoutineCall(MCAblCompiler& compiler, MCAblRoutineKey key);
