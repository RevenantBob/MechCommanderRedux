#pragma once

/// <summary>The error <see cref="LZCompress"/> returns when it can't allocate its dictionary.</summary>
inline constexpr int32_t LZ_NO_RAM_FOR_TABLE = static_cast<int32_t>(0xCBCB0001);

/// <summary>
/// Packs <paramref name="srcLen"/> bytes into <paramref name="dest"/> as LZW (9- to 12-bit codes, LSB first, starting
/// with a clear code and ending with the end code), the format <c>LZDecomp</c> reads.
/// </summary>
/// <returns>The packed length, or <see cref="LZ_NO_RAM_FOR_TABLE"/>.</returns>
/// <remarks>
/// <paramref name="dest"/> needs <see cref="LZCompressBound"/> bytes: each code is stored as three bytes, so the
/// writes run up to two bytes past the packed length.
/// </remarks>
int32_t LZCompress(uint8_t* dest, const uint8_t* src, int32_t srcLen);

/// <summary>The most bytes <see cref="LZCompress"/> writes for <paramref name="srcLen"/> bytes of input.</summary>
/// <remarks>Not in the original, whose callers guessed (twice the input).</remarks>
size_t LZCompressBound(int32_t srcLen);
