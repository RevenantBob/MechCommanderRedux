#include "stdafx.h"
#include "MCPort.h"

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

namespace MCPort
{
    void ReportUnimplemented(const char* file, int line, const char* what)
    {
        static std::mutex lock;
        static std::set<std::pair<std::string, int>> reported;
        std::scoped_lock guard(lock);

        if (!reported.emplace(file, line).second)
        {
            return;
        }

        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Not reconstructed yet: %s (%s:%d)", what, file, line);
    }

    int StrICmp(const char* a, const char* b)
    {
        for (;; ++a, ++b)
        {
            const int ca = std::tolower(static_cast<unsigned char>(*a));
            const int cb = std::tolower(static_cast<unsigned char>(*b));

            if (ca != cb || ca == 0)
            {
                return ca - cb;
            }
        }
    }

    int StrNICmp(const char* a, const char* b, size_t count)
    {
        for (; count > 0; --count, ++a, ++b)
        {
            const int ca = std::tolower(static_cast<unsigned char>(*a));
            const int cb = std::tolower(static_cast<unsigned char>(*b));

            if (ca != cb || ca == 0)
            {
                return ca - cb;
            }
        }

        return 0;
    }

    char* StrUpr(char* text)
    {
        for (char* c = text; *c; ++c)
        {
            *c = static_cast<char>(std::toupper(static_cast<unsigned char>(*c)));
        }

        return text;
    }

    char* StrLwr(char* text)
    {
        for (char* c = text; *c; ++c)
        {
            *c = static_cast<char>(std::tolower(static_cast<unsigned char>(*c)));
        }

        return text;
    }

    char* IToA(int32_t value, char* buffer, int radix)
    {
        if (radix == 10)
        {
            std::snprintf(buffer, 12, "%d", value);
            return buffer;
        }

        uint32_t v = static_cast<uint32_t>(value);
        char digits[33];
        int n = 0;

        do
        {
            const uint32_t d = v % static_cast<uint32_t>(radix);
            digits[n++] = static_cast<char>(d < 10 ? '0' + d : 'a' + d - 10);
            v /= static_cast<uint32_t>(radix);
        } while (v != 0);

        for (int i = 0; i < n; ++i)
        {
            buffer[i] = digits[n - 1 - i];
        }

        buffer[n] = 0;
        return buffer;
    }

    void StrCopy(char* destination, size_t size, const char* source)
    {
        if (size == 0)
        {
            return;
        }

        const size_t length = std::min(std::strlen(source), size - 1);
        std::memmove(destination, source, length);
        destination[length] = 0;
    }

    namespace
    {
        /// <summary>Set by <see cref="UseManualClock"/>: the clocks read <see cref="ManualClockNs"/>.</summary>
        std::atomic<bool> ManualClock = false;
        /// <summary>The manual clock, in nanoseconds.</summary>
        std::atomic<uint64_t> ManualClockNs = 0;
        /// <summary>The thread that called <see cref="UseManualClock"/> (the game's).</summary>
        std::thread::id ManualClockThread;
        /// <summary>The game thread's reads since the clock last advanced.</summary>
        uint32_t ManualClockReads = 0;

        /// <summary>
        /// The manual clock, read. Reads don't move it, so it is the same however often the game looks; but a busy
        /// wait (the palette fade, the 50 ms waits) would never end, so past 100,000 reads on the game's thread
        /// without an advance, each read moves it on a microsecond.
        /// </summary>
        uint64_t ReadManualClock()
        {
            if (std::this_thread::get_id() == ManualClockThread && ++ManualClockReads > 100000)
            {
                return ManualClockNs += 1000;
            }

            return ManualClockNs.load();
        }
    }

    void UseManualClock()
    {
        ManualClockThread = std::this_thread::get_id();
        ManualClock = true;
    }

    void AdvanceManualClock(uint64_t nanoseconds)
    {
        ManualClockNs += nanoseconds;
        ManualClockReads = 0;
    }

    uint32_t ProcessId()
    {
#ifdef _WIN32
        return static_cast<uint32_t>(_getpid());
#else
        return static_cast<uint32_t>(getpid());
#endif
    }

    uint32_t Milliseconds()
    {
        if (ManualClock)
        {
            return static_cast<uint32_t>(ReadManualClock() / 1000000);
        }

        return static_cast<uint32_t>(SDL_GetTicks());
    }

    uint32_t TotalPhysicalMemory()
    {
        const uint64_t bytes = static_cast<uint64_t>(SDL_GetSystemRAM()) * 1024 * 1024;
        return static_cast<uint32_t>(std::min<uint64_t>(bytes, UINT32_MAX));
    }

    int64_t PerformanceCounter()
    {
        if (ManualClock)
        {
            return static_cast<int64_t>(ReadManualClock());
        }

        return static_cast<int64_t>(SDL_GetPerformanceCounter());
    }

    int64_t PerformanceFrequency()
    {
        if (ManualClock)
        {
            return 1000000000;
        }

        return static_cast<int64_t>(SDL_GetPerformanceFrequency());
    }

    namespace
    {
        /// <summary>Days from 1601-01-01 (the FILETIME epoch) to 1970-01-01.</summary>
        constexpr int64_t FileTimeEpochDays = 134774;
        /// <summary>FILETIME ticks (100 ns) in a day.</summary>
        constexpr uint64_t TicksPerDay = 864000000000ull;
    }

    void GetSystemTime(_SYSTEMTIME& time)
    {
        using namespace std::chrono;
        const auto sinceUnix =
            duration_cast<duration<int64_t, std::ratio<1, 10000000>>>(system_clock::now().time_since_epoch());
        FileTimeToSystemTime(
            static_cast<uint64_t>(sinceUnix.count() + FileTimeEpochDays * static_cast<int64_t>(TicksPerDay)), time);
    }

    uint64_t SystemTimeToFileTime(const _SYSTEMTIME& time)
    {
        using namespace std::chrono;
        const sys_days date = year{time.wYear} / month{time.wMonth} / day{time.wDay};
        const int64_t days = date.time_since_epoch().count() + FileTimeEpochDays;
        return static_cast<uint64_t>(days) * TicksPerDay + time.wHour * 36000000000ull + time.wMinute * 600000000ull +
               time.wSecond * 10000000ull + time.wMilliseconds * 10000ull;
    }

    void FileTimeToSystemTime(uint64_t fileTime, _SYSTEMTIME& time)
    {
        using namespace std::chrono;
        const sys_days date{days{static_cast<int64_t>(fileTime / TicksPerDay) - FileTimeEpochDays}};
        const year_month_day ymd{date};
        uint64_t ticks = fileTime % TicksPerDay;
        time.wYear = static_cast<uint16_t>(static_cast<int>(ymd.year()));
        time.wMonth = static_cast<uint16_t>(static_cast<unsigned>(ymd.month()));
        time.wDay = static_cast<uint16_t>(static_cast<unsigned>(ymd.day()));
        time.wDayOfWeek = static_cast<uint16_t>(weekday{date}.c_encoding());
        time.wHour = static_cast<uint16_t>(ticks / 36000000000ull);
        ticks %= 36000000000ull;
        time.wMinute = static_cast<uint16_t>(ticks / 600000000ull);
        ticks %= 600000000ull;
        time.wSecond = static_cast<uint16_t>(ticks / 10000000ull);
        time.wMilliseconds = static_cast<uint16_t>(ticks % 10000000ull / 10000ull);
    }

    void GetLocalTime(_SYSTEMTIME& time)
    {
        using namespace std::chrono;
        const auto local = current_zone()->to_local(system_clock::now());
        const auto sinceUnix = duration_cast<duration<int64_t, std::ratio<1, 10000000>>>(local.time_since_epoch());
        FileTimeToSystemTime(
            static_cast<uint64_t>(sinceUnix.count() + FileTimeEpochDays * static_cast<int64_t>(TicksPerDay)), time);
    }

    char* StrTime(char* buffer)
    {
        _SYSTEMTIME time{};
        GetLocalTime(time);
        buffer[0] = static_cast<char>('0' + time.wHour / 10);
        buffer[1] = static_cast<char>('0' + time.wHour % 10);
        buffer[2] = ':';
        buffer[3] = static_cast<char>('0' + time.wMinute / 10);
        buffer[4] = static_cast<char>('0' + time.wMinute % 10);
        buffer[5] = ':';
        buffer[6] = static_cast<char>('0' + time.wSecond / 10);
        buffer[7] = static_cast<char>('0' + time.wSecond % 10);
        buffer[8] = 0;
        return buffer;
    }

    bool GetUserName(char* buffer, uint32_t* size)
    {
        const char* name = SDL_getenv("USERNAME");

        if (name == nullptr || *name == 0)
        {
            name = SDL_getenv("USER");
        }

        if (name == nullptr || *name == 0)
        {
            return false;
        }

        const auto needed = static_cast<uint32_t>(std::strlen(name) + 1);
        const uint32_t available = *size;

        *size = needed;
        if (needed > available)
        {
            return false;
        }

        std::memcpy(buffer, name, needed);
        return true;
    }
}
