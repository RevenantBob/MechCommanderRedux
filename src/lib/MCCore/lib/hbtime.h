#pragma once

// Original source: mcx\lib\hbtime.cpp. The game calendar's date and time (the campaign's clock, from year 3000),
// convertible to and from a count of minutes.

/// <summary>A date and time to the minute. 6 bytes.</summary>
class HBDate
{
public:
    /// <summary>Midnight of year 3000 (all fields 0).</summary>
    /// <remarks>MCX.EXE @ 0x006b46e0</remarks>
    HBDate() = default;

    /// <summary>The date <paramref name="minutes"/> minutes after the start of year 3000.</summary>
    /// <remarks>MCX.EXE @ 0x006b4700</remarks>
    HBDate LongToHBDate(int32_t minutes);

    /// <summary>The minutes from the start of year 3000 to <paramref name="date"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006b4850</remarks>
    int32_t HBDateToLong(HBDate* date);

    /// <summary>
    /// Whether <paramref name="year"/> is a leap year. Original behaviour: century years are leap only when divisible
    /// by 1000, not 400.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006b4900</remarks>
    int IsLeapYear(int16_t year);

    /// <summary>The number of days in month <paramref name="month"/> (0 = January) of <paramref name="year"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006b4950</remarks>
    uint8_t MonthLength(uint8_t month, int16_t year);

    /// <summary>The hour, 0-23 with <paramref name="military"/>, else 0-11.</summary>
    /// <remarks>MCX.EXE @ 0x006b49b0</remarks>
    uint8_t GetHour(int military);

    /// <summary>The month's three-letter name (not terminated: it points into "JanFeb...").</summary>
    /// <remarks>MCX.EXE @ 0x006b49e0</remarks>
    const char* GetShortMonth(uint8_t month);

    /// <summary>The month's name padded to nine characters (not terminated).</summary>
    /// <remarks>MCX.EXE @ 0x006b4a00</remarks>
    const char* GetLongMonth(uint8_t month);

    /// <remarks>MCX.EXE @ 0x006b4a20</remarks>
    void operator=(HBDate date);

    /// <summary>Sets the date from a count of minutes.</summary>
    /// <remarks>MCX.EXE @ 0x006b4a50</remarks>
    void operator=(int32_t minutes);

    /// <summary>Adds a date's minutes.</summary>
    /// <remarks>MCX.EXE @ 0x006b4a90</remarks>
    void operator+=(HBDate date);

    /// <summary>Adds minutes.</summary>
    /// <remarks>MCX.EXE @ 0x006b4ae0</remarks>
    void operator+=(int32_t minutes);

    /// <summary>Subtracts a date's minutes (not below 0).</summary>
    /// <remarks>MCX.EXE @ 0x006b4b20</remarks>
    void operator-=(HBDate date);

    /// <summary>Subtracts minutes; nothing happens when the result would be negative.</summary>
    /// <remarks>MCX.EXE @ 0x006b4b80</remarks>
    void operator-=(int32_t minutes);

    /// <summary>Minute, 0-59.</summary>
    uint8_t minute = 0; // +0x00
    /// <summary>Hour, 0-23.</summary>
    uint8_t hour = 0; // +0x01
    /// <summary>Day of the month.</summary>
    uint8_t day = 0; // +0x02
    /// <summary>Month, 0 = January.</summary>
    uint8_t month = 0; // +0x03
    /// <summary>Year.</summary>
    int16_t year = 3000; // +0x04
};
