#pragma once

// The ABL interpreter's crunched code, memory and runtime stack.
//
// ---- Memory and addresses ---------------------------------------------------------------------------------------
//
// The original's Address was a char* anywhere: into the stack, a module's static data, an array's heap block, the
// code, or a string literal. The port's MCAblAddress names the memory instead: a space, a block in it and a byte
// offset. MCAblRuntime owns all of it and resolves an address into a bounds-checked span (MCAblRuntime::Bytes):
//
//  - Stack     the runtime stack; the offset counts bytes, so stack item i is at 8 * i.
//  - Block     a block of MCAblRuntime's memory: an array (local, eternal, a copy of an array argument, a static
//              array), a module instance's static variables (one item each), or a run's module parameters.
//  - Literal   the text of string literal or string constant symbol <block> (its LiteralText and the 0 after it).
//  - Code      code segment <block>: only a frame's return address.
//
// An address is 8 bytes, one stack item, as the original's 4-byte pointer was one 4-byte item.
//
// ---- The runtime stack (64-bit design) -------------------------------------------------------------------------
//
// The original's StackItem was a 4-byte union of long, float, byte and Address, so a stack slot could hold an
// integer, a real or a pointer. The port keeps the union, 8 bytes with an MCAblAddress:
//
//  - Every slot is one MCAblStackItem. Variable offsets, the eternal count, the frame header positions and the stack
//    limit are item counts, as they already were in the original (which multiplied by 4 = sizeof(StackItem)).
//  - Integer / Real / Byte sit at offset 0 of the union (little-endian), so a 4-byte integer or real read at a
//    variable's address reads the item's Integer or Real. Variable addresses the interpreter pushes point either to a
//    stack item (a scalar local / static / eternal) or to an element in array memory; the interpreter fetches and
//    stores 4-byte integers and reals through them, which works for both.
//  - The push functions clear the whole item before storing, so the upper half of a slot never holds stale bits.
//  - Arrays are never stack items: a slot of array type holds `Address`, a block laid out with the ABL type sizes
//    (4-byte integers and reals, 1-byte chars), exactly as in the original.
//  - A frame starts with a 4-item MCAblStackFrameHeader (function value, static link, dynamic link, return address);
//    parameters follow from item 4 and locals after them. The links are stack addresses, the return address a code
//    address.
//  - The stack is MCAblRuntime::MaxStackItems items (see there). Eternal variables occupy its first items.
//
// ---- The crunched code -----------------------------------------------------------------------------------------
//
// MCAblCodeWriter crunches each routine into a byte stream; the symbol table keeps it as a segment
// (MCAblDefinition's Routine.CodeSegment). The stream is:
//
//  - token             1 byte (an MCAblToken), written by MCAblCompiler::NextToken inside a code block.
//  - symbol operand    after an identifier, number, string or function-call token: an MCAblSymbolId, 4 bytes as the
//                      original's pointer was, written and read with memcpy since it is unaligned.
//  - statement marker  MCAblToken::StatementMarker ('C') inserted before a statement's first token: with debug info,
//                      a uint8 file number and an int32 line number follow (5 bytes), then the displaced token.
//  - address marker    MCAblToken::AddressMarker ('D') inserted before a token: an int32 then the displaced token.
//                      Once fixed up, the int32 is the offset from the marker's slot to the jump target (the target
//                      is slot + offset - 1). Before fixup the slot chains to the previous unfixed marker (switch
//                      statements chain their case exits); the original stored that char* in the 4 bytes. The port
//                      stores the marker's offset in the code, or NoCodeMark.
//  - integer operand   an int32 (4 bytes).
//  - offset operand    an int32, the target minus the operand's own position.
//
// The interpreter reads a segment by position (MCAblRuntime keeps the segment and the next byte's offset), and
// MCAblDebugger::StatementText walks the stream, skipping symbol operands by AblCodeSymbolSize and address markers by
// AblCodeAddressSize.

#include "abl/MCAblSymbol.h"

/// <summary>Which memory an <see cref="MCAblAddress"/> is in (see the file comment).</summary>
enum class MCAblSpace : uint8_t
{
    /// <summary>No memory: the original's null pointer.</summary>
    None = 0,
    Stack = 1,
    Block = 2,
    Literal = 3,
    Code = 4
};

/// <summary>An address in ABL memory: a space, a block in it and a byte offset (see the file comment).</summary>
/// <remarks>
/// No member initializers, so it can sit in MCAblStackItem's union: a value-initialised address (all zero) is none.
/// </remarks>
struct MCAblAddress
{
    /// <summary>The most blocks a space can number (the block takes the address's low 24 bits).</summary>
    static constexpr int32_t MaxBlocks = 1 << 24;

    /// <summary>The space in the top 8 bits, the block in the low 24.</summary>
    uint32_t SpaceAndBlock;
    /// <summary>Bytes from the start of the block.</summary>
    int32_t Offset;

    /// <summary>Byte <paramref name="offset"/> of block <paramref name="block"/> of <paramref name="space"/>.</summary>
    static constexpr MCAblAddress Of(MCAblSpace space, int32_t block, int32_t offset)
    {
        return MCAblAddress{(static_cast<uint32_t>(space) << 24) | (static_cast<uint32_t>(block) & 0xffffffu), offset};
    }

    /// <summary>Stack item <paramref name="item"/>.</summary>
    static constexpr MCAblAddress StackItem(int32_t item);

    /// <summary>Which memory it is in.</summary>
    constexpr MCAblSpace Space() const { return static_cast<MCAblSpace>(SpaceAndBlock >> 24); }

    /// <summary>Which block of the space.</summary>
    constexpr int32_t Block() const { return static_cast<int32_t>(SpaceAndBlock & 0xffffffu); }

    /// <summary>Whether it is no address (the original's null).</summary>
    constexpr bool IsNull() const { return Space() == MCAblSpace::None; }

    /// <summary>The address <paramref name="bytes"/> further on.</summary>
    constexpr MCAblAddress operator+(int32_t bytes) const { return MCAblAddress{SpaceAndBlock, Offset + bytes}; }

    constexpr bool operator==(const MCAblAddress&) const = default;
};

static_assert(sizeof(MCAblAddress) == 8, "an address is one stack item");
static_assert(std::is_trivially_copyable_v<MCAblAddress>);

/// <summary>A slot of the ABL runtime stack (and of a module's static data). See the file comment.</summary>
/// <remarks>4 bytes in the original; 8 in the port (the Address member).</remarks>
union MCAblStackItem
{
    int32_t Integer;
    float Real;
    uint8_t Byte;
    MCAblAddress Address;
};

static_assert(sizeof(MCAblStackItem) == sizeof(MCAblAddress), "a stack item is one address-sized slot");

constexpr MCAblAddress MCAblAddress::StackItem(int32_t item)
{
    return Of(MCAblSpace::Stack, 0, item * static_cast<int32_t>(sizeof(MCAblStackItem)));
}

/// <summary>Where each item of a frame's header is, counted from the frame's first item.</summary>
enum class MCAblFrameItem : int32_t
{
    /// <summary>A function's result.</summary>
    FunctionValue = 0,
    /// <summary>The frame of the enclosing scope (for variables of outer levels): a stack address, or none.</summary>
    StaticLink = 1,
    /// <summary>The caller's frame: a stack address.</summary>
    DynamicLink = 2,
    /// <summary>Where to continue in the caller's code: a code address.</summary>
    ReturnAddress = 3
};

/// <summary>Items in a frame's header (parameters start after it).</summary>
inline constexpr int32_t AblFrameHeaderItems = 4;

/// <summary>Bytes of a symbol operand in crunched code (4, as in the original).</summary>
inline constexpr int32_t AblCodeSymbolSize = static_cast<int32_t>(sizeof(MCAblSymbolId));
/// <summary>Bytes of an address marker's operand in crunched code.</summary>
inline constexpr int32_t AblCodeAddressSize = 4;
/// <summary>Bytes of an integer or offset operand in crunched code.</summary>
inline constexpr int32_t AblCodeIntegerSize = 4;
/// <summary>Bytes a statement marker's debug info takes (file number + line number).</summary>
inline constexpr int32_t AblCodeStatementMarkerSize = 5;
