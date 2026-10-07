#pragma once

// Original source: mcx\lib\hbtime.cpp. The game calendar's date and time (the campaign's clock, from year 3000),
// convertible to and from a count of minutes.

/// <summary>A date and time to the minute. 6 bytes.</summary>
class MCHBDate
{
public:
    /// <summary>Midnight of year 3000 (all fields 0).</summary>
    MCHBDate() = default;

    /// <summary>The date <paramref name="minutes"/> minutes after the start of year 3000.</summary>
    MCHBDate LongToHBDate(int32_t minutes);

    /// <summary>The minutes from the start of year 3000 to <paramref name="date"/>.</summary>
    int32_t HBDateToLong(MCHBDate* date);

    /// <summary>
    /// Whether <paramref name="year"/> is a leap year. Original behaviour: century years are leap only when divisible
    /// by 1000, not 400.
    /// </summary>
    int IsLeapYear(int16_t year);

    /// <summary>The number of days in month <paramref name="month"/> (0 = January) of <paramref name="year"/>.</summary>
    uint8_t MonthLength(uint8_t month, int16_t year);

    /// <summary>The hour, 0-23 with <paramref name="military"/>, else 0-11.</summary>
    uint8_t GetHour(int military);

    /// <summary>The month's three-letter name (not terminated: it points into "JanFeb...").</summary>
    const char* GetShortMonth(uint8_t month);

    /// <summary>The month's name padded to nine characters (not terminated).</summary>
    const char* GetLongMonth(uint8_t month);

    void operator=(MCHBDate date);

    /// <summary>Sets the date from a count of minutes.</summary>
    void operator=(int32_t minutes);

    /// <summary>Adds a date's minutes.</summary>
    void operator+=(MCHBDate date);

    /// <summary>Adds minutes.</summary>
    void operator+=(int32_t minutes);

    /// <summary>Subtracts a date's minutes (not below 0).</summary>
    void operator-=(MCHBDate date);

    /// <summary>Subtracts minutes; nothing happens when the result would be negative.</summary>
    void operator-=(int32_t minutes);

    /// <summary>Minute, 0-59.</summary>
    uint8_t Minute = 0;
    /// <summary>Hour, 0-23.</summary>
    uint8_t Hour = 0;
    /// <summary>Day of the month.</summary>
    uint8_t Day = 0;
    /// <summary>Month, 0 = January.</summary>
    uint8_t Month = 0;
    /// <summary>Year.</summary>
    int16_t Year = 3000;
};
