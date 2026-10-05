#pragma once

/// <summary>
/// An item of the salvage an object leaves: ABL setsalvage adds them, getsalvage reads them back.
/// </summary>
/// <remarks>Has no out-of-line code, so the line tables don't name its file; the original chained them in a list
/// (8 bytes each, allocated by ABL with operator new(8)).</remarks>
struct MCSalvageItem
{
    /// <summary>The item's id (setsalvage's second argument).</summary>
    uint8_t ItemId = 0;
    /// <summary>How many (setsalvage's third argument).</summary>
    uint8_t NumItems = 0;
};
