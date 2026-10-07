#include "stdafx.h"
#include "lib/hbtime.h"

namespace
{
    constexpr int32_t MINUTES_PER_DAY = 1440;
    constexpr int32_t MINUTES_PER_YEAR = 525600;
    constexpr int32_t MINUTES_PER_LEAP_YEAR = 527040;
}

MCHBDate MCHBDate::LongToHBDate(int32_t minutes)
{
    MCHBDate date;
    int16_t newYear = 3000;

    // Original behaviour: in a leap year a remainder between 525600 and 527039 minutes goes negative, and the date
    // then keeps only the (truncated) minute.
    while (minutes > MINUTES_PER_YEAR - 1 || (minutes > MINUTES_PER_LEAP_YEAR - 1 && IsLeapYear(newYear)))
    {
        if (IsLeapYear(newYear))
        {
            minutes -= MINUTES_PER_LEAP_YEAR;
        }
        else
        {
            minutes -= MINUTES_PER_YEAR;
        }

        ++newYear;
    }

    uint8_t newMonth = 0;

    if (MonthLength(0, newYear) * MINUTES_PER_DAY <= minutes)
    {
        do
        {
            minutes -= MonthLength(newMonth, newYear) * MINUTES_PER_DAY;
            ++newMonth;
        } while (MonthLength(newMonth, newYear) * MINUTES_PER_DAY <= minutes);
    }

    // Original behaviour: the day comes out counted from 0, while HBDateToLong counts it from 1.
    if (minutes > MINUTES_PER_DAY - 1)
    {
        date.Day = static_cast<uint8_t>(date.Day + static_cast<uint32_t>(minutes) / MINUTES_PER_DAY);
        minutes = static_cast<int32_t>(static_cast<uint32_t>(minutes) % MINUTES_PER_DAY);
    }

    if (minutes > 59)
    {
        date.Hour = static_cast<uint8_t>(date.Hour + static_cast<uint32_t>(minutes) / 60);
        minutes = static_cast<int32_t>(static_cast<uint32_t>(minutes) % 60);
    }

    date.Minute = static_cast<uint8_t>(minutes);
    date.Month = newMonth;
    date.Year = newYear;
    return date;
}

int32_t MCHBDate::HBDateToLong(MCHBDate* date)
{
    int32_t minutes = date->Minute + ((date->Hour - 24) + date->Day * 24) * 60;

    // Original behaviour: each month before the date's adds the length of the date's own month.
    for (uint32_t m = date->Month; m != 0; --m)
    {
        minutes += MonthLength(date->Month, date->Year) * MINUTES_PER_DAY;
    }

    // Original behaviour: each year counts as a leap year when the year after it is one.
    for (int16_t y = date->Year; y > 3000; --y)
    {
        minutes += IsLeapYear(y) ? MINUTES_PER_LEAP_YEAR : MINUTES_PER_YEAR;
    }

    return minutes;
}

int MCHBDate::IsLeapYear(int16_t year)
{
    if (year % 4 != 0)
    {
        return 0;
    }

    if (year % 100 == 0 && year % 1000 != 0)
    {
        return 0;
    }

    return 1;
}

uint8_t MCHBDate::MonthLength(uint8_t month, int16_t year)
{
    static constexpr uint8_t lengths[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if (month == 1 && IsLeapYear(year))
    {
        return 29;
    }

    // Port fix: the original read past its table for months over 11.
    return month < 12 ? lengths[month] : 0;
}

uint8_t MCHBDate::GetHour(int military)
{
    if (military)
    {
        return Hour;
    }

    return Hour % 12;
}

const char* MCHBDate::GetShortMonth(uint8_t month)
{
    return "JanFebMarAprMayJunJulAugSepOctNovDec" + month * 3;
}

const char* MCHBDate::GetLongMonth(uint8_t month)
{
    return "January  February March    April    May      June     July     August   SeptemberOctober  November "
           "December " +
           month * 9;
}

void MCHBDate::operator=(MCHBDate date)
{
    Minute = date.Minute;
    Hour = date.Hour;
    Day = date.Day;
    Month = date.Month;
    Year = date.Year;
}

void MCHBDate::operator=(int32_t minutes)
{
    *this = LongToHBDate(minutes);
}

void MCHBDate::operator+=(MCHBDate date)
{
    const int32_t mine = HBDateToLong(this);
    const int32_t theirs = HBDateToLong(&date);
    *this = LongToHBDate(theirs + mine);
}

void MCHBDate::operator+=(int32_t minutes)
{
    *this = LongToHBDate(HBDateToLong(this) + minutes);
}

void MCHBDate::operator-=(MCHBDate date)
{
    int32_t difference = HBDateToLong(this) - HBDateToLong(&date);

    if (difference < 0)
    {
        difference = 0;
    }

    *this = LongToHBDate(difference);
}

void MCHBDate::operator-=(int32_t minutes)
{
    const int32_t difference = HBDateToLong(this) - minutes;

    if (difference >= 0)
    {
        *this = LongToHBDate(difference);
    }
}
