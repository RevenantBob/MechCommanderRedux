#include "stdafx.h"
#include "vfx/vfxint.h"

// The game's own copy of VFX's shape encoder (mcx\vfx\encode_vfx.cpp). It is the vfxa.asm routine transcribed into
// inline assembly, with the asm's globals moved to the game's data (0x008022e8..0x0080235c): the prologue, the
// bounding-box pass, ScanLine (0x006b5973) and FlushPacket (0x006b5bdb) are instruction for instruction those of
// _VFX_shape_scan, ScanLine and FlushPacket, and 0x006b5db8 is the shared exit (store the result, return it).
// The port therefore runs the one encoder, in vfx/vfxa_shape.cpp.

int VfxShapeScan(MCPane* pane, uint8_t transparentColor, int hotX, int hotY, void* buffer)
{
    return VfxShapeScanAsm(pane, transparentColor, hotX, hotY, buffer);
}
