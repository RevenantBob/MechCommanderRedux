#pragma once

#include "abl/MCAblSymbol.h"
#include "abl/MCAblToken.h"

/// <summary>
/// Where an address marker's operand is in the code being written (a byte offset), or <see cref="NoCodeMark"/>.
/// </summary>
using MCAblCodeMark = int32_t;

/// <summary>No address marker (the original's null slot pointer).</summary>
inline constexpr MCAblCodeMark NoCodeMark = -1;

/// <summary>
/// Writes a routine's crunched code (the original's code buffer and crunch* functions; the stream's layout is in
/// ablexec.h). The compiler writes one routine at a time and makes it a segment with <see cref="CreateSegment"/>.
/// </summary>
/// <remarks>
/// The buffer grows: the original's fixed buffer (the scenario's AblMaxCodeBlockSize) and its CODE_SEGMENT_OVERFLOW
/// error are gone. Marks are offsets into the buffer, not pointers, so they survive its growth.
/// </remarks>
class MCAblCodeWriter
{
public:
    /// <param name="debugInfo">Whether statement markers carry the file and line (IncludeDebugInfo).</param>
    explicit MCAblCodeWriter(bool debugInfo);

    /// <summary>
    /// Whether anything is written (the original's crunch flag; off while a disabled <c>print</c>, <c>assert</c> or
    /// <c>concat</c> call is compiled).
    /// </summary>
    bool Enabled = true;

    /// <summary>Writes a token.</summary>
    void WriteToken(MCAblToken token);

    /// <summary>Writes a symbol operand.</summary>
    void WriteSymbol(MCAblSymbol* symbol);

    /// <summary>
    /// Puts a statement marker before the token just written, with <paramref name="fileNumber"/> and
    /// <paramref name="lineNumber"/> when markers carry debug info.
    /// </summary>
    void InsertStatementMarker(int32_t fileNumber, int32_t lineNumber);

    /// <summary>Takes back the statement marker just inserted, with its token.</summary>
    void RemoveStatementMarker();

    /// <summary>
    /// Puts an address marker before the token just written. Until <see cref="FixupAddressMarker"/>, its operand
    /// chains to <paramref name="chain"/> (another unfixed marker, or none).
    /// </summary>
    /// <returns>The marker's operand, or none when nothing is written.</returns>
    MCAblCodeMark InsertAddressMarker(MCAblCodeMark chain);

    /// <summary>Points the marker at <paramref name="mark"/> to the end of the code written so far.</summary>
    /// <returns>The marker it chained to.</returns>
    MCAblCodeMark FixupAddressMarker(MCAblCodeMark mark);

    /// <summary>Writes an int32 operand.</summary>
    void WriteInteger(int32_t value);

    /// <summary>Writes an offset operand: <paramref name="target"/> minus the operand's own position.</summary>
    void WriteOffset(MCAblCodeMark target);

    /// <summary>The end of the code written so far (a branch target).</summary>
    MCAblCodeMark Position() const { return static_cast<MCAblCodeMark>(_Code.size()); }

    /// <summary>Copies the code to a new segment and empties the buffer for the next routine.</summary>
    /// <returns>The segment: the code and one more byte, a TKN_NONE (OB-108).</returns>
    MCAddress CreateSegment();

private:
    /// <summary>Appends the bytes of <paramref name="value"/>.</summary>
    template <typename T> void Append(const T& value)
    {
        const auto* bytes = reinterpret_cast<const char*>(&value);
        _Code.insert(_Code.end(), bytes, bytes + sizeof(T));
    }

    std::vector<char> _Code;
    bool _DebugInfo = false;
};
