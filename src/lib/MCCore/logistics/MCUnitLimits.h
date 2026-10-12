#pragma once

/// <summary>The most mechs and vehicles the player may own (inventory and force; a game rule).</summary>
inline constexpr int32_t MaxUnits = 50;

/// <summary>From this many units on, a purchase warns that the limit is near.</summary>
inline constexpr int32_t UnitWarning = 40;

/// <summary>The player's resource points.</summary>
extern int32_t ResourcePoints;

/// <summary>How many mechs and vehicles the player owns (inventory and force).</summary>
int32_t NumUnits();

/// <summary>How many units a purchase may buy: the room left under <see cref="MaxUnits"/>, or the stock when that is less.</summary>
int32_t MaxPurchase(int32_t available);

/// <summary>Warns (a dialog) when the player owns <see cref="MaxUnits"/> units or more.</summary>
/// <returns>True when no more may be bought.</returns>
bool CheckMaxUnits();

/// <summary>Warns (a dialog) when the player owns <see cref="UnitWarning"/> units or more.</summary>
void CheckNumUnits();
