#pragma once

/// <summary>
/// Unpacks <paramref name="srcLen"/> bytes of LZW data (9- to 12-bit codes, LSB first; 0x100 clears the dictionary,
/// 0x101 ends) into <paramref name="dest"/>.
/// </summary>
/// <returns>The number of bytes unpacked.</returns>
/// <remarks>
/// Hand-written assembly in the original. The caller sizes <paramref name="dest"/> for the unpacked data; nothing
/// bounds the writes.
/// </remarks>
int32_t LZDecomp(uint8_t* dest, const uint8_t* src, uint32_t srcLen);
