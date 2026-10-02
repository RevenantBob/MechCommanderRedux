#pragma once

// Port-wide helpers shared by every reconstructed file: portable replacements for the MSVC CRT extensions the
// original called, and the marker for code that is not reconstructed yet.

/// <summary>
/// Marks a code path of the original that the port doesn't reproduce yet. It logs the place once per run and carries
/// on, so a missing piece shows up in the log instead of crashing. <c>grep MC_UNIMPLEMENTED</c> lists them all.
/// </summary>
#define MC_UNIMPLEMENTED(what) ::MCPort::ReportUnimplemented(__FILE__, __LINE__, what)

struct _SYSTEMTIME;

namespace MCPort
{
    /// <summary>Logs an <c>MC_UNIMPLEMENTED</c> path the first time it is reached.</summary>
    /// <param name="file">Source file of the marker.</param>
    /// <param name="line">Line of the marker.</param>
    /// <param name="what">What isn't reconstructed.</param>
    void ReportUnimplemented(const char* file, int line, const char* what);

    /// <summary>Case-insensitive ASCII string compare, as MSVC's <c>_stricmp</c>.</summary>
    int StrICmp(const char* a, const char* b);

    /// <summary>Case-insensitive ASCII compare of at most <paramref name="count"/> characters (<c>_strnicmp</c>).</summary>
    int StrNICmp(const char* a, const char* b, size_t count);

    /// <summary>Upper-cases a string in place (<c>_strupr</c>).</summary>
    char* StrUpr(char* text);

    /// <summary>Lower-cases a string in place (<c>_strlwr</c>).</summary>
    char* StrLwr(char* text);

    /// <summary>
    /// The original's integer-to-text conversion (<c>_itoa</c> / <c>_ltoa</c>): <paramref name="value"/> in
    /// <paramref name="radix"/>, with a sign only for radix 10.
    /// </summary>
    char* IToA(int32_t value, char* buffer, int radix);

    /// <summary>
    /// Copies at most <paramref name="size"/> - 1 characters and always terminates, where the original used an
    /// unchecked <c>strcpy</c> into a fixed buffer.
    /// </summary>
    void StrCopy(char* destination, size_t size, const char* source);

    /// <summary>Milliseconds since the port started, as the original's <c>timeGetTime</c> / <c>GetTickCount</c>.</summary>
    uint32_t Milliseconds();

    /// <summary>
    /// Port-only (tests): from now on <see cref="Milliseconds"/> and <see cref="PerformanceCounter"/> read a clock
    /// that only <see cref="AdvanceManualClock"/> moves, so a run doesn't depend on how fast the machine is.
    /// </summary>
    void UseManualClock();

    /// <summary>Port-only (tests): moves the manual clock on by <paramref name="nanoseconds"/>.</summary>
    void AdvanceManualClock(uint64_t nanoseconds);

    /// <summary>
    /// The machine's physical memory in bytes, as <c>GlobalMemoryStatus</c>'s <c>dwTotalPhys</c> (clamped to what a
    /// 32-bit field holds, as the original saw it).
    /// </summary>
    uint32_t TotalPhysicalMemory();

    /// <summary>The high-resolution counter (<c>QueryPerformanceCounter</c>).</summary>
    int64_t PerformanceCounter();

    /// <summary>Counts per second of <see cref="PerformanceCounter"/> (<c>QueryPerformanceFrequency</c>).</summary>
    int64_t PerformanceFrequency();

    /// <summary>The text caret's blink interval in milliseconds (<c>GetCaretBlinkTime</c>; Windows' default).</summary>
    constexpr uint32_t CaretBlinkTime()
    {
        return 530;
    }

    /// <summary>The current UTC date and time (<c>GetSystemTime</c>).</summary>
    void GetSystemTime(_SYSTEMTIME& time);

    /// <summary>
    /// A date and time as a Win32 FILETIME: 100-nanosecond ticks since 1601-01-01 (<c>SystemTimeToFileTime</c>).
    /// </summary>
    uint64_t SystemTimeToFileTime(const _SYSTEMTIME& time);

    /// <summary>A FILETIME tick count back to a date and time (<c>FileTimeToSystemTime</c>).</summary>
    void FileTimeToSystemTime(uint64_t fileTime, _SYSTEMTIME& time);

    /// <summary>The current local date and time (<c>GetLocalTime</c>).</summary>
    void GetLocalTime(_SYSTEMTIME& time);

    /// <summary>
    /// The CRT's <c>_strtime</c>: writes the local time as "HH:MM:SS" (9 bytes with the terminator) into
    /// <paramref name="buffer"/> and returns it.
    /// </summary>
    char* StrTime(char* buffer);

    /// <summary>
    /// <c>GetUserNameA</c>: the logged-in user's name (the <c>USERNAME</c> or <c>USER</c> environment variable) into
    /// <paramref name="buffer"/>, whose size <paramref name="size"/> holds on entry. On return it holds the length
    /// with the terminator, which is also the size needed when the buffer is too small.
    /// </summary>
    /// <returns>Whether the name was known and fitted.</returns>
    bool GetUserName(char* buffer, uint32_t* size);
}

/// <summary>StrCopy into a char array, with the size taken from the array.</summary>
template <size_t N> inline void MCStrCopy(char (&destination)[N], const char* source)
{
    MCPort::StrCopy(destination, N, source);
}
