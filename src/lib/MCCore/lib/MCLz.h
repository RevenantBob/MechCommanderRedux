#pragma once

// FASA's LZW stream, used for FastFile entries and compressed packets: codes of 9 to 12 bits packed little-endian,
// 256 = clear the dictionary, 257 = end of data, new entries from 258. Hand-written assembly in the original.

/// <summary>Unpacks <paramref name="packed"/> into <paramref name="dest"/>.</summary>
/// <returns>The number of bytes written (never more than <paramref name="dest"/> holds).</returns>
/// <remarks>The original never checks the output's size (a Port fix, for damaged data).</remarks>
int32_t LZDecomp(std::span<uint8_t> dest, std::span<const uint8_t> packed);

/// <summary>
/// Packs <paramref name="data"/>: a clear code, 9- to 12-bit codes, a clear whenever the dictionary fills, an end code
/// and three bytes of padding (the decoder stops three bytes before the end).
/// </summary>
/// <remarks>
/// The port's encoder follows the original's scheme; its output needn't match the original's byte for byte, only
/// unpack to the same data (the game only packs its own save data).
/// </remarks>
std::vector<uint8_t> LZCompress(std::span<const uint8_t> data);
