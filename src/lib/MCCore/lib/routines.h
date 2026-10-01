#pragma once

// Original source: mcx\lib\routines.cpp. Memory fill helpers; the original picked 8-byte (MMX-era FPU) or 4-byte
// stores by the global Processor type, which only changed speed.

/// <summary>Sets <paramref name="length"/> bytes at <paramref name="buffer"/> to 0.</summary>
/// <remarks>MCX.EXE @ 0x0064d9e0</remarks>
void memclear(void* buffer, int length);

/// <summary>Sets <paramref name="length"/> bytes at <paramref name="buffer"/> to 0xff.</summary>
/// <remarks>MCX.EXE @ 0x0064da60</remarks>
void memfill(void* buffer, int length);
