#pragma once

class MCMover;

/// <summary>
/// One round of repairs on <paramref name="mover"/> from <paramref name="refitPoints"/>: a share of RefitAmount
/// spread over the armor and internal structure that need it (not destroyed arms), then the ammunition (only the
/// ammunition with <paramref name="ammoOnly"/>). <paramref name="pointsUsed"/> gets the points spent, rounded to
/// quarters.
/// </summary>
/// <returns>1 when the mover needs nothing more (or there were no points), else 0.</returns>
int32_t DoRefit(MCMover* mover, float refitPoints, float& pointsUsed, int ammoOnly);
