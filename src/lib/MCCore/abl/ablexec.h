#pragma once

// The ABL interpreter's crunched code and runtime stack.
//
// ---- The runtime stack (64-bit design) -------------------------------------------------------------------------
//
// The original's StackItem was a 4-byte union of long, float, byte and Address, so a stack slot could hold an
// integer, a real or a pointer. The port keeps the union but makes it pointer-sized (8 bytes):
//
//  - Every slot is one StackItem. All indexing is in items, never bytes: data.offset of a variable, eternalOffset,
//    the frame header positions and the stack limit are item counts, as they already were in the original (which
//    multiplied by 4 = sizeof(StackItem)). Code that wrote `base + offset * 4` on bytes writes `base + offset`.
//  - integer / real / byte sit at offset 0 of the union (little-endian), so an `int32_t*` or `float*` to a slot
//    reads and writes the same value as `&slot.integer`. Variable addresses pushed with pushAddress point either to
//    a StackItem (a scalar local / static / eternal) or to an element in array memory; the interpreter fetches and
//    stores 4-byte integers and reals through them, which works for both.
//  - The push functions clear the whole item before storing, so the upper half of a slot never holds stale bits.
//  - Arrays are never stack items: a slot of array type holds `address`, pointing to a block of AblMemory
//    laid out with the ABL type sizes (4-byte integers and reals, 1-byte chars), exactly as in the original.
//  - A frame starts with a 4-item StackFrameHeader (function value, static link, dynamic link, return address);
//    parameters follow from item 4 and locals after them. The links are StackItem pointers, the return address a
//    code pointer, both held in `address`.
//  - The stack is `stack` (MAXSIZE_STACK items). The original compared tos against stack + 0xa000 bytes, i.e.
//    10240 of its 4-byte items; the port checks against stack + MAXSIZE_STACK and allocates at least that many
//    items, whatever byte size ABLi_init is given. Eternal variables occupy items 0 .. eternalOffset - 1.
//  - Static variables live in each ABLModule's staticData, an array of StackItems (arrays again as pointers).
//
// ---- The crunched code (64-bit design) -------------------------------------------------------------------------
//
// MCAblCodeWriter crunches each routine into a byte stream, then makes it a segment in AblMemory
// (MCAblDefinition's Routine.CodeSegment). The stream is:
//
//  - token             1 byte (an MCAblToken), written by MCAblCompiler::NextToken inside a code block.
//  - symbol operand    after an identifier, number, string or function-call token: an MCAblSymbol*. The original
//                      stored the 4-byte pointer; the port stores the 8-byte pointer (CODE_SYMBOL_PTR_SIZE), written
//                      and read with memcpy since it is unaligned. Code segments live only in memory for the run
//                      (they are rebuilt from source at load), so raw pointers are safe.
//  - statement marker  MCAblToken::StatementMarker ('C') inserted before a statement's first token: with
//                      IncludeDebugInfo, a uint8 file number and an int32 line number follow (5 bytes), then the
//                      displaced token. No pointer: same size as the original.
//  - address marker    MCAblToken::AddressMarker ('D') inserted before a token: an int32 then the displaced token.
//                      Once fixed up, the int32 is the offset from the marker's slot to the jump target
//                      (getCodeAddress returns slot + offset - 1). Relative, so segments can be copied. Before fixup
//                      the slot chains to the previous unfixed marker (switch statements chain their case exits); the
//                      original stored that char* in the 4 bytes. The port stores the marker's offset in the code,
//                      or -1 for none (CODE_ADDRESS_CHAIN_NULL).
//  - integer operand   an int32 (4 bytes).
//  - offset operand    an int32, the target minus the operand's own position.
//
// Debugger::sprintStatement walks this stream, so it skips symbol operands by CODE_SYMBOL_PTR_SIZE and address
// markers by CODE_ADDRESS_SIZE.

#include "abl/MCAblToken.h"
#include "abl/MCAblSymbolTable.h"
#include "platform/MCBlockStore.h"

struct MCAblParam;

/// <summary>A slot of the ABL runtime stack (and of a module's static data). See the file comment.</summary>
/// <remarks>4 bytes in the original; 8 in the port (the Address member).</remarks>
union MCStackItem
{
    int32_t Integer;
    float Real;
    uint8_t Byte;
    MCAddress Address;
};

typedef MCStackItem* MCStackItemPtr;

static_assert(sizeof(MCStackItem) == sizeof(void*), "StackItem is one pointer-sized slot");

/// <summary>The first four items of every stack frame.</summary>
struct MCStackFrameHeader
{
    /// <summary>A function's result.</summary>
    MCStackItem FunctionValue{}; // item 0
    /// <summary>The frame of the enclosing scope (for variables of outer levels).</summary>
    MCStackItem StaticLink{}; // item 1
    /// <summary>The caller's frame.</summary>
    MCStackItem DynamicLink{}; // item 2
    /// <summary>Where to continue in the caller's code.</summary>
    MCStackItem ReturnAddress{}; // item 3
};

typedef MCStackFrameHeader* MCStackFrameHeaderPtr;

/// <summary>Items in the ABL stack (the original's limit: 0xa000 bytes of 4-byte items).</summary>
inline constexpr int32_t MAXSIZE_STACK = 0xa000 / 4;
/// <summary>Bytes of a symbol operand in crunched code (4 in the original).</summary>
inline constexpr int32_t CODE_SYMBOL_PTR_SIZE = static_cast<int32_t>(sizeof(MCAblSymbol*));
/// <summary>Bytes of an address marker's operand in crunched code.</summary>
inline constexpr int32_t CODE_ADDRESS_SIZE = 4;
/// <summary>Bytes of an integer or offset operand in crunched code.</summary>
inline constexpr int32_t CODE_INTEGER_SIZE = 4;
/// <summary>Bytes a statement marker's debug info takes (file number + line number).</summary>
inline constexpr int32_t CODE_STATEMENT_MARKER_SIZE = 5;
/// <summary>An unfixed address marker's chain value meaning "no previous marker".</summary>
inline constexpr int32_t CODE_ADDRESS_CHAIN_NULL = -1;

/// <summary>Nonzero to put file and line numbers in statement markers.</summary>
extern int IncludeDebugInfo;
/// <summary>The next code byte to execute.</summary>
extern char* CodeSegmentPtr;
/// <summary>Where the statement being executed starts (for the debugger).</summary>
extern char* StatementStartPtr;
/// <summary>The code token being executed.</summary>
extern MCAblToken CodeToken;
/// <summary>The ABL stack (an unnamed global of the original, @ 0x007c3e44).</summary>
extern MCStackItemPtr Stack;
/// <summary>Top of stack.</summary>
extern MCStackItemPtr Tos;
/// <summary>The current frame.</summary>
extern MCStackItemPtr StackFrameBasePtr;
/// <summary>The executing module's static data.</summary>
extern MCStackItemPtr StaticDataPtr;
/// <summary>The value the last module or function execution returned.</summary>
extern MCStackItem ReturnValue;
/// <summary>Statements executed in the current execution (the ABLModule::execute result).</summary>
extern int32_t ExecStatementCount;
/// <summary>Line of the statement being executed (from its marker).</summary>
extern int32_t ExecLineNumber;
/// <summary>Set by a tactical-order routine to leave the running routine at once.</summary>
extern int ExitFromTacOrder;
/// <summary>The current routine's scope level while executing (1 for a module's code).</summary>
extern int32_t Level;
/// <summary>The source file of the statement being executed (from its marker), or -1.</summary>
extern int32_t ExecFileNumber;
/// <summary>
/// The memory ABL owns between ABLi_init and ABLi_close: the stack, the registries, code segments, static data and
/// array blocks. ABLi_close clears it; many blocks (static arrays) are only ever freed that way.
/// </summary>
/// <remarks>
/// Port: replaces the original's three ABL heaps (AblSymbolTableHeap, AblStackHeap, AblCodeHeap, sized from the
/// mission files). Nothing runs out, so the "unable to malloc" paths are gone. The symbols and types are
/// MCAblSymbolTable's.
/// </remarks>
extern MCBlockStore AblMemory;

/// <summary>Reads a symbol operand.</summary>
MCAblSymbol* GetCodeSymTableNodePtr();

/// <summary>At a statement marker with debug info, reads its file (into ExecFileNumber) and line.</summary>
/// <returns>The line, or -1.</returns>
int32_t GetCodeStatementMarker();

/// <summary>At an address marker, reads its target.</summary>
/// <returns>The target, or null when codeToken isn't an address marker.</returns>
char* GetCodeAddressMarker();

/// <summary>Reads an int32 operand.</summary>
int32_t GetCodeInteger();

/// <summary>Reads an offset operand as the address it points to.</summary>
char* GetCodeAddress();

/// <summary>Pops the top item.</summary>
void Pop();

/// <summary>Reads the next code token into codeToken.</summary>
void GetCodeToken();

/// <summary>Pushes an integer (stack overflow is a runtime error).</summary>
void PushInteger(int32_t value);

/// <summary>Pushes a real.</summary>
void PushReal(float value);

/// <summary>Pushes a char or boolean.</summary>
void PushByte(char value);

/// <summary>Pushes an address.</summary>
void PushAddress(MCAddress address);

/// <summary>
/// Pushes a frame header for a call from level <paramref name="oldLevel"/> to a routine at
/// <paramref name="newLevel"/> (-1 for a routine in another module: no static link).
/// </summary>
void PushStackFrameHeader(int32_t oldLevel, int32_t newLevel);

/// <summary>Pushes a local of <paramref name="typePtr"/>: zero, or a new, zeroed array block in AblMemory.</summary>
void AllocLocal(MCAblType* typePtr);

/// <summary>Frees a local array's block (reference parameters are left alone).</summary>
void FreeLocal(MCAblSymbol* idPtr);

/// <summary>Enters a routine: traces it, jumps to its code and allocates its locals.</summary>
void RoutineEntry(MCAblSymbol* routineIdPtr);

/// <summary>Leaves a routine: frees its array parameters and locals, pops its frame and returns to the caller's code.</summary>
void RoutineExit(MCAblSymbol* routineIdPtr);

/// <summary>Runs a routine (a module's main code): its <c>init</c> function first if the module wasn't initialised.</summary>
void Execute(MCAblSymbol* routineIdPtr);

/// <summary>
/// Enters module <paramref name="moduleIdPtr"/>'s frame and runs only its function <paramref name="childRoutineIdPtr"/>
/// (after <c>init</c> on the first execution).
/// </summary>
void ExecuteChild(MCAblSymbol* moduleIdPtr, MCAblSymbol* childRoutineIdPtr, MCAblParam* paramList);
