#pragma once

// Original source: mcx\logistics.cpp (DropSlot).

class MCLogPart;

/// <summary>A multiplayer drop slot: its lance and position, and the mech or vehicle placed there.</summary>
/// <remarks>Original source: <c>logistics.cpp</c>, 0xc bytes.</remarks>
struct MCDropSlot
{
    int32_t Lance = 0;
    int32_t Slot = 0;
    /// <summary>The unit placed there (a view: its player's mech or vehicle list owns it), or null.</summary>
    MCLogPart* Part = nullptr;
};
