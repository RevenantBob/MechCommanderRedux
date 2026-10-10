#include "stdafx.h"
#include "MCPort.h"
#include "main/MCGameContext.h"

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

namespace MCPort
{
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
        // As _itoa: signed in base 10, the 32 bits unsigned in any other base (lower-case digits).
        constexpr size_t longest = 33;
        const std::to_chars_result written =
            radix == 10 ? std::to_chars(buffer, buffer + longest, value)
                        : std::to_chars(buffer, buffer + longest, static_cast<uint32_t>(value), radix);
        *written.ptr = '\0';
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
