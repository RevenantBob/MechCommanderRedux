#include "stdafx.h"
#include "MCPort.h"
#include "main/MCGameContext.h"

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

namespace
{
    /// <summary>Character <paramref name="index"/> of <paramref name="text"/> lower-cased, or 0 past its end.</summary>
    int LowerAt(std::string_view text, size_t index)
    {
        return index < text.size() ? std::tolower(static_cast<unsigned char>(text[index])) : 0;
    }
}

namespace MCPort
{
    int StrICmp(std::string_view a, std::string_view b)
    {
        for (size_t index = 0;; ++index)
        {
            const int ca = LowerAt(a, index);
            const int cb = LowerAt(b, index);

            if (ca != cb || ca == 0)
            {
                return ca - cb;
            }
        }
    }

    int StrNICmp(std::string_view a, std::string_view b, size_t count)
    {
        for (size_t index = 0; index < count; ++index)
        {
            const int ca = LowerAt(a, index);
            const int cb = LowerAt(b, index);

            if (ca != cb || ca == 0)
            {
                return ca - cb;
            }
        }

        return 0;
    }

    void StrLwr(std::string& text)
    {
        for (char& c : text)
        {
            if (c == 0)
            {
                break;
            }

            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
    }

    std::string IToA(int32_t value, int radix)
    {
        // As _itoa: signed in base 10, the 32 bits unsigned in any other base (lower-case digits).
        std::array<char, 33> buffer{};
        const std::to_chars_result written =
            radix == 10
                ? std::to_chars(buffer.data(), buffer.data() + buffer.size(), value)
                : std::to_chars(buffer.data(), buffer.data() + buffer.size(), static_cast<uint32_t>(value), radix);
        return {buffer.data(), written.ptr};
    }

    int32_t Rand()
    {
        return MCGameContext::Current().Random().Next();
    }

    void SeedRand(uint32_t seed)
    {
        MCGameContext::Current().Random().Seed(seed);
    }

    uint32_t RandState()
    {
        return MCGameContext::Current().Random().State();
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
        return MCGameContext::Current().Clock().Milliseconds();
    }

    uint32_t TotalPhysicalMemory()
    {
        const uint64_t bytes = static_cast<uint64_t>(SDL_GetSystemRAM()) * 1024 * 1024;
        return static_cast<uint32_t>(std::min<uint64_t>(bytes, UINT32_MAX));
    }

    int64_t PerformanceCounter()
    {
        return MCGameContext::Current().Clock().PerformanceCounter();
    }

    int64_t PerformanceFrequency()
    {
        return MCGameContext::Current().Clock().PerformanceFrequency();
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
        FileTimeToSystemTime(MCGameContext::Current().Clock().UtcFileTime(), time);
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
        FileTimeToSystemTime(MCGameContext::Current().Clock().LocalFileTime(), time);
    }

    std::string StrTime()
    {
        _SYSTEMTIME time{};
        GetLocalTime(time);
        return std::format("{:02}:{:02}:{:02}", time.wHour % 100, time.wMinute % 100, time.wSecond % 100);
    }

    std::optional<std::string> GetUserName()
    {
        for (const std::string variable : {"USERNAME", "USER"})
        {
            const std::string name = SDL_getenv(variable.c_str()) != nullptr ? SDL_getenv(variable.c_str()) : "";

            if (!name.empty())
            {
                return name;
            }
        }

        return std::nullopt;
    }
}
