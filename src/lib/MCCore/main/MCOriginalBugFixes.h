#pragma once

// Switches for port fixes that change what the game computes, off by default: the recorded runs (tools/ci/baseline.py)
// play out as MCX.EXE did. Each guards one quirk of the original; turn one on to see the game without it.

/// <summary>
/// A hot spot packet that holds fewer hot spots than the mech's weapons and other spots reads hot spot 0's offset
/// instead of past the packet.
/// </summary>
inline constexpr bool FixShortHotSpotPackets = false;

/// <summary>An ABL module's static arrays start zeroed instead of 0xff (the original heap's fill).</summary>
inline constexpr bool FixAblUninitializedStatics = false;

/// <summary>The mech actor's fields the original left uninitialised start at 0 instead of -1.</summary>
inline constexpr bool FixUninitializedMechActor = false;
