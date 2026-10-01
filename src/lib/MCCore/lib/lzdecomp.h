#pragma once

/// <summary>
/// Unpacks FASA's LZW stream: codes of 9 to 12 bits packed little-endian, 256 = clear the dictionary, 257 = end of
/// data, new entries from 258. Used for FastFile entries and compressed packets.
/// </summary>
/// <param name="dest">Receives the unpacked bytes.</param>
/// <param name="src">The packed stream.</param>
/// <param name="srcLen">Length of the packed stream in bytes.</param>
/// <returns>The number of bytes written to <paramref name="dest"/>.</returns>
/// <remarks>
/// MCX.EXE @ 0x0064cc80 (hand-written assembly in the original). The original never checks the output's size; this
/// port writes at most <paramref name="destLen"/> bytes (a Port fix, for damaged data).
/// </remarks>
int32_t LZDecomp(uint8_t* dest, const uint8_t* src, uint32_t srcLen, uint32_t destLen = UINT32_MAX);
