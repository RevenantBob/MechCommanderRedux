#include "stdafx.h"
#include "X87Extended.h"

#include <bit>
#include <intrin.h>

namespace
{
    /// <summary>The top bit of a 64-bit word.</summary>
    inline constexpr uint64_t TOP_BIT = 0x8000000000000000ull;
    /// <summary>The bits of a double's and a float's mantissa.</summary>
    inline constexpr int32_t DOUBLE_BITS = 53;
    inline constexpr int32_t FLOAT_BITS = 24;
    /// <summary>The exponent bias and fraction bits of a double.</summary>
    inline constexpr int32_t DOUBLE_BIAS = 1023;
    inline constexpr int32_t DOUBLE_FRACTION_BITS = 52;
    /// <summary>The bias of the exponent <c>round</c> takes: its 128-bit value's top bit is 2^(exponent).</summary>
    inline constexpr int32_t WIDE_BIAS = 127;
    /// <summary>The integer indefinite fistp stores for a value that doesn't fit.</summary>
    inline constexpr int64_t INTEGER_INDEFINITE = static_cast<int64_t>(TOP_BIT);
}

X87Extended::X87Extended(int32_t value)
{
    const auto magnitude = static_cast<uint64_t>(std::abs(static_cast<int64_t>(value)));
    *this = round(value < 0, WIDE_BIAS, 0, magnitude);
}

X87Extended::X87Extended(float value) : X87Extended(static_cast<double>(value))
{
}

X87Extended::X87Extended(double value)
{
    uint64_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    const bool negative = (bits & TOP_BIT) != 0;
    const auto biased = static_cast<int32_t>(bits >> DOUBLE_FRACTION_BITS & 0x7ff);
    uint64_t fraction = bits & ((1ull << DOUBLE_FRACTION_BITS) - 1);
    ASSERT(biased != 0x7ff);

    if (biased != 0)
    {
        fraction |= 1ull << DOUBLE_FRACTION_BITS;
    }

    // A denormal's exponent is that of the smallest normal.
    const int32_t exponent = (biased == 0 ? 1 : biased) - DOUBLE_BIAS - DOUBLE_FRACTION_BITS;
    *this = round(negative, exponent + WIDE_BIAS, 0, fraction);
}

X87Extended X87Extended::round(bool negative, int32_t exponent, uint64_t high, uint64_t low)
{
    X87Extended result;
    result._negative = negative;

    if (high == 0 && low == 0)
    {
        return result;
    }

    const int32_t shift = high != 0 ? std::countl_zero(high) : 64 + std::countl_zero(low);

    if (shift >= 64)
    {
        high = low << (shift - 64);
        low = 0;
    }
    else if (shift > 0)
    {
        high = high << shift | low >> (64 - shift);
        low <<= shift;
    }

    exponent -= shift;

    if (low > TOP_BIT || (low == TOP_BIT && (high & 1) != 0))
    {
        high++;

        if (high == 0)
        {
            high = TOP_BIT;
            exponent++;
        }
    }

    result._mantissa = high;
    result._exponent = exponent;
    return result;
}

X87Extended X87Extended::add(const X87Extended& a, const X87Extended& b, bool subtract)
{
    X87Extended other = b;
    other._negative = b._negative != subtract;

    if (other._mantissa == 0)
    {
        // x + 0 is x; -0 + -0 is -0, -0 + +0 is +0.
        X87Extended result = a;
        result._negative = a._mantissa != 0 ? a._negative : a._negative && other._negative;
        return result;
    }

    if (a._mantissa == 0)
    {
        return other;
    }

    const bool aGreater =
        a._exponent > other._exponent || (a._exponent == other._exponent && a._mantissa >= other._mantissa);
    const X87Extended& greater = aGreater ? a : other;
    const X87Extended& lesser = aGreater ? other : a;
    // Both as 128-bit values with the top bit free for a carry: greater is (mantissa / 2) : (mantissa * 2^63).
    const uint64_t greaterHigh = greater._mantissa >> 1;
    const uint64_t greaterLow = greater._mantissa << 63;
    uint64_t lesserHigh = lesser._mantissa >> 1;
    uint64_t lesserLow = lesser._mantissa << 63;
    const int32_t distance = greater._exponent - lesser._exponent;

    // The lesser value is aligned, the bits shifted out kept as a sticky bit.
    if (distance >= 128)
    {
        lesserHigh = 0;
        lesserLow = 1;
    }
    else if (distance >= 64)
    {
        const bool lost = lesserLow != 0 || (distance > 64 && (lesserHigh << (128 - distance)) != 0);
        lesserLow = distance == 64 ? lesserHigh : lesserHigh >> (distance - 64);
        lesserHigh = 0;
        lesserLow |= lost ? 1 : 0;
    }
    else if (distance > 0)
    {
        const bool lost = (lesserLow << (64 - distance)) != 0;
        lesserLow = lesserLow >> distance | lesserHigh << (64 - distance);
        lesserHigh >>= distance;
        lesserLow |= lost ? 1 : 0;
    }

    uint64_t high;
    uint64_t low;

    if (greater._negative == lesser._negative)
    {
        low = greaterLow + lesserLow;
        high = greaterHigh + lesserHigh + (low < greaterLow ? 1 : 0);
    }
    else
    {
        low = greaterLow - lesserLow;
        high = greaterHigh - lesserHigh - (greaterLow < lesserLow ? 1 : 0);

        if (high == 0 && low == 0)
        {
            return X87Extended();
        }
    }

    // The value is (high:low) x 2^(greater exponent - 126).
    return round(greater._negative, greater._exponent + 1, high, low);
}

X87Extended operator+(const X87Extended& a, const X87Extended& b)
{
    return X87Extended::add(a, b, false);
}

X87Extended operator-(const X87Extended& a, const X87Extended& b)
{
    return X87Extended::add(a, b, true);
}

X87Extended operator*(const X87Extended& a, const X87Extended& b)
{
    const bool negative = a._negative != b._negative;

    if (a._mantissa == 0 || b._mantissa == 0)
    {
        X87Extended zero;
        zero._negative = negative;
        return zero;
    }

    uint64_t high;
    const uint64_t low = _umul128(a._mantissa, b._mantissa, &high);
    // The product is (high:low) x 2^(a exponent + b exponent - 126).
    return X87Extended::round(negative, a._exponent + b._exponent + 1, high, low);
}

X87Extended operator/(const X87Extended& a, const X87Extended& b)
{
    ASSERT(b._mantissa != 0);
    const bool negative = a._negative != b._negative;

    if (a._mantissa == 0)
    {
        X87Extended zero;
        zero._negative = negative;
        return zero;
    }

    // The dividend as a 128-bit value below divisor x 2^64, so that the quotient fits 64 bits.
    const int32_t shift = a._mantissa >= b._mantissa ? 1 : 0;
    const uint64_t dividendHigh = a._mantissa >> shift;
    const uint64_t dividendLow = shift != 0 ? a._mantissa << 63 : 0;
    uint64_t remainder;
    const uint64_t high = _udiv128(dividendHigh, dividendLow, b._mantissa, &remainder);
    uint64_t low = _udiv128(remainder, 0, b._mantissa, &remainder);

    if (remainder != 0)
    {
        low |= 1;
    }

    // The quotient is (high:low) x 2^(a exponent - b exponent - 128 + shift).
    return X87Extended::round(negative, a._exponent - b._exponent - 1 + shift, high, low);
}

X87Extended X87Extended::abs() const
{
    X87Extended result = *this;
    result._negative = false;
    return result;
}

void X87Extended::roundMantissa(int32_t bits, uint64_t* mantissa, int32_t* exponent) const
{
    const int32_t dropped = 64 - bits;
    uint64_t kept = _mantissa >> dropped;
    const uint64_t rest = _mantissa & ((1ull << dropped) - 1);
    const uint64_t half = 1ull << (dropped - 1);
    *exponent = _exponent;

    if (rest > half || (rest == half && (kept & 1) != 0))
    {
        kept++;

        if (kept == 1ull << bits)
        {
            kept >>= 1;
            (*exponent)++;
        }
    }

    *mantissa = kept;
}

float X87Extended::toFloat() const
{
    if (_mantissa == 0)
    {
        return _negative ? -0.0f : 0.0f;
    }

    uint64_t mantissa;
    int32_t exponent;
    roundMantissa(FLOAT_BITS, &mantissa, &exponent);
    ASSERT(exponent >= -126 && exponent <= 127);
    const float magnitude = std::ldexp(static_cast<float>(mantissa), exponent - (FLOAT_BITS - 1));
    return _negative ? -magnitude : magnitude;
}

double X87Extended::toDouble() const
{
    if (_mantissa == 0)
    {
        return _negative ? -0.0 : 0.0;
    }

    uint64_t mantissa;
    int32_t exponent;
    roundMantissa(DOUBLE_BITS, &mantissa, &exponent);
    ASSERT(exponent >= -1022 && exponent <= 1023);
    const double magnitude = std::ldexp(static_cast<double>(mantissa), exponent - (DOUBLE_BITS - 1));
    return _negative ? -magnitude : magnitude;
}

int64_t X87Extended::truncate() const
{
    if (_mantissa == 0 || _exponent < 0)
    {
        return 0;
    }

    if (_exponent > 62)
    {
        // Only -2^63 itself fits; it is the indefinite's own value.
        return INTEGER_INDEFINITE;
    }

    const auto magnitude = static_cast<int64_t>(_mantissa >> (63 - _exponent));
    return _negative ? -magnitude : magnitude;
}
