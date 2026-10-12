#pragma once

// Port-wide helpers shared by every reconstructed file: portable replacements for the MSVC CRT extensions the
// original called.

struct _SYSTEMTIME;

namespace MCPort
{
    /// <summary>
    /// Case-insensitive ASCII string compare, as MSVC's <c>_stricmp</c>: each string ends at its end or its first null.
    /// </summary>
    int StrICmp(std::string_view a, std::string_view b);

    /// <summary>Case-insensitive ASCII compare of at most <paramref name="count"/> characters (<c>_strnicmp</c>).</summary>
    int StrNICmp(std::string_view a, std::string_view b, size_t count);

    /// <summary>Lower-cases a string in place (<c>_strlwr</c>), up to its first null.</summary>
    void StrLwr(std::string& text);

    /// <summary>
    /// The original's integer-to-text conversion (<c>_itoa</c> / <c>_ltoa</c>): <paramref name="value"/> in
    /// <paramref name="radix"/>, with a sign only for radix 10.
    /// </summary>
    std::string IToA(int32_t value, int radix);

    /// <summary>
    /// Milliseconds since the port started, as the original's <c>timeGetTime</c> / <c>GetTickCount</c>. This and the
    /// other clocks below read the current context's <c>MCClock</c> (main/MCGameContext.h).
    /// </summary>
    uint32_t Milliseconds();

    /// <summary>The running process's ID (no two running processes share one).</summary>
    uint32_t ProcessId();

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

    /// <summary>The CRT's <c>_strtime</c>: the local time as "HH:MM:SS".</summary>
    std::string StrTime();

    /// <summary>
    /// <c>GetUserNameA</c>: the logged-in user's name (the <c>USERNAME</c> or <c>USER</c> environment variable), none
    /// when neither is set.
    /// </summary>
    std::optional<std::string> GetUserName();

    /// <summary>
    /// The original's <c>rand</c>: the next value of the current context's <c>MCRandom</c> (the CRT's generator,
    /// <c>MCCrtRandom</c>, in the game), in [0, 0x7fff].
    /// </summary>
    int32_t Rand();

    /// <summary>The original's <c>srand</c>: restarts the context's generator at <paramref name="seed"/>.</summary>
    void SeedRand(uint32_t seed);

    /// <summary>Port-only (tests): the context's generator state, which decides every later <see cref="Rand"/>.</summary>
    uint32_t RandState();
}

/// <summary>Whether two strings match with ASCII case ignored (the port's one case-insensitive compare).</summary>
constexpr bool MCIEquals(std::string_view a, std::string_view b)
{
    constexpr auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; };
    return std::ranges::equal(a, b, {}, lower, lower);
}

/// <summary>Whether <paramref name="text"/> starts with <paramref name="prefix"/>, ASCII case ignored.</summary>
constexpr bool MCIStartsWith(std::string_view text, std::string_view prefix)
{
    return text.size() >= prefix.size() && MCIEquals(text.substr(0, prefix.size()), prefix);
}

/// <summary>
/// A string as MSVC's printf prints a <c>%s</c> argument, "(null)" for a null one (no data): for <c>std::format</c>
/// where the original's <c>sprintf</c> could get a null name.
/// </summary>
constexpr std::string_view MCPrintfText(std::string_view text)
{
    return text.data() != nullptr ? text : std::string_view("(null)");
}

/// <summary>
/// Formats <paramref name="args"/> with a printf format the game loads at run time (its string table's), which
/// <c>std::format</c> can't take. Strings go in as <c>const char*</c>.
/// </summary>
template <typename... Args> std::string MCFormatPrintf(const std::string& format, Args... args)
{
    static_assert((!std::is_same_v<Args, std::string> && ...), "pass a std::string's c_str() to a printf format");
    static_assert((!std::is_same_v<Args, std::string_view> && ...),
                  "a string_view is no C string: pass a C string to a printf format");
    const int size = std::snprintf(nullptr, 0, format.c_str(), args...);

    if (size <= 0)
    {
        return {};
    }

    std::string text(static_cast<size_t>(size), '\0');
    std::snprintf(text.data(), text.size() + 1, format.c_str(), args...);
    return text;
}
