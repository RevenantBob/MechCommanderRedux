#pragma once

// Original source: mcx\lib\cvmath.cpp: the random-number helpers, on the context's dice (MCPort::Rand).

/// <summary>A random number in [0, <paramref name="range"/>) from rand()'s 15 bits.</summary>
int32_t RandomNumber(int32_t range);

/// <summary>Whether a d100 roll comes under <paramref name="percent"/>.</summary>
bool RollDice(int32_t percent);

/// <summary>A random number in [-<paramref name="range"/>, <paramref name="range"/>).</summary>
int32_t SignedRandomNumber(int32_t range);
