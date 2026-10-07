#pragma once

// The ABL interpreter's crunched code and runtime stack.
//
// ---- The runtime stack (64-bit design) -------------------------------------------------------------------------
//
// The original's StackItem was a 4-byte union of long, float, byte and Address, so a stack slot could hold an
// integer, a real or a pointer. The port keeps the union but makes it pointer-sized (8 bytes):
//
//  - Every slot is one MCAblStackItem. All indexing is in items, never bytes: a variable's Offset, the eternal
//    count, the frame header positions and the stack limit are item counts, as they already were in the original
//    (which multiplied by 4 = sizeof(StackItem)).
//  - Integer / Real / Byte sit at offset 0 of the union (little-endian), so an `int32_t*` or `float*` to a slot reads
//    and writes the same value as `&slot.Integer`. Variable addresses the interpreter pushes point either to a stack
//    item (a scalar local / static / eternal) or to an element in array memory; the interpreter fetches and stores
//    4-byte integers and reals through them, which works for both.
//  - The push functions clear the whole item before storing, so the upper half of a slot never holds stale bits.
//  - Arrays are never stack items: a slot of array type holds `Address`, pointing to a block laid out with the ABL
//    type sizes (4-byte integers and reals, 1-byte chars), exactly as in the original.
//  - A frame starts with a 4-item MCAblStackFrameHeader (function value, static link, dynamic link, return address);
//    parameters follow from item 4 and locals after them. The links are stack item pointers, the return address a
//    code pointer, both held in `Address`.
//  - The stack is MCAblRuntime::MaxStackItems items (see there). Eternal variables occupy its first items.
//  - Static variables live in each MCAblModule's static data, a vector of stack items (arrays again as pointers).
//
// ---- The crunched code (64-bit design) -------------------------------------------------------------------------
//
// MCAblCodeWriter crunches each routine into a byte stream; the symbol table keeps it as a segment
// (MCAblDefinition's Routine.CodeSegment). The stream is:
//
//  - token             1 byte (an MCAblToken), written by MCAblCompiler::NextToken inside a code block.
//  - symbol operand    after an identifier, number, string or function-call token: an MCAblSymbol*. The original
//                      stored the 4-byte pointer; the port stores the 8-byte pointer (AblCodeSymbolSize), written and
//                      read with memcpy since it is unaligned. Code segments live only in memory for the run (they
//                      are rebuilt from source at load), so raw pointers are safe.
//  - statement marker  MCAblToken::StatementMarker ('C') inserted before a statement's first token: with debug info,
//                      a uint8 file number and an int32 line number follow (5 bytes), then the displaced token. No
//                      pointer: same size as the original.
//  - address marker    MCAblToken::AddressMarker ('D') inserted before a token: an int32 then the displaced token.
//                      Once fixed up, the int32 is the offset from the marker's slot to the jump target (the target
//                      is slot + offset - 1). Relative, so segments can be copied. Before fixup the slot chains to the
//                      previous unfixed marker (switch statements chain their case exits); the original stored that
//                      char* in the 4 bytes. The port stores the marker's offset in the code, or NoCodeMark.
//  - integer operand   an int32 (4 bytes).
//  - offset operand    an int32, the target minus the operand's own position.
//
// MCAblDebugger::StatementText walks this stream, so it skips symbol operands by AblCodeSymbolSize and address
// markers by AblCodeAddressSize.

#include "abl/MCAblSymbol.h"

/// <summary>A slot of the ABL runtime stack (and of a module's static data). See the file comment.</summary>
/// <remarks>4 bytes in the original; 8 in the port (the Address member).</remarks>
union MCAblStackItem
{
    int32_t Integer;
    float Real;
    uint8_t Byte;
    MCAddress Address;
};

static_assert(sizeof(MCAblStackItem) == sizeof(void*), "a stack item is one pointer-sized slot");

/// <summary>The first four items of every stack frame.</summary>
struct MCAblStackFrameHeader
{
    /// <summary>A function's result.</summary>
    MCAblStackItem FunctionValue{};
    /// <summary>The frame of the enclosing scope (for variables of outer levels).</summary>
    MCAblStackItem StaticLink{};
    /// <summary>The caller's frame.</summary>
    MCAblStackItem DynamicLink{};
    /// <summary>Where to continue in the caller's code.</summary>
    MCAblStackItem ReturnAddress{};
};

/// <summary>Bytes of a symbol operand in crunched code (4 in the original).</summary>
inline constexpr int32_t AblCodeSymbolSize = static_cast<int32_t>(sizeof(MCAblSymbol*));
/// <summary>Bytes of an address marker's operand in crunched code.</summary>
inline constexpr int32_t AblCodeAddressSize = 4;
/// <summary>Bytes of an integer or offset operand in crunched code.</summary>
inline constexpr int32_t AblCodeIntegerSize = 4;
/// <summary>Bytes a statement marker's debug info takes (file number + line number).</summary>
inline constexpr int32_t AblCodeStatementMarkerSize = 5;
