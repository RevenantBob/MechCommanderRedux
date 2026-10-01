#pragma once

/// <summary>
/// Packs <paramref name="srcLen"/> bytes into the LZW stream <see cref="LZDecomp"/> reads: a clear code, 9- to
/// 12-bit codes, a clear whenever the dictionary fills, and an end code.
/// </summary>
/// <param name="dest">Receives the packed stream; size it generously (twice the input is always enough).</param>
/// <param name="src">The bytes to pack.</param>
/// <param name="srcLen">How many.</param>
/// <returns>The packed length in bytes.</returns>
/// <remarks>
/// MCX.EXE @ 0x0064c920 (assembly in the original). The port's encoder follows the same scheme; its output needn't
/// match the original's byte for byte, only unpack to the same data (the game only packs its own save data).
/// </remarks>
int32_t LZCompress(uint8_t* dest, const uint8_t* src, uint32_t srcLen);
