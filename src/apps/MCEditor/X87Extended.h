#pragma once

// The port's own: the editor's x87 runs at 64-bit precision, so its floating-point code rounds every operation to a
// 64-bit mantissa, which no C++ type on x64 does. This type computes those operations exactly as the FPU did (round
// to nearest even), for the code whose results depend on it.

/// <summary>An x87 extended-precision value: a 64-bit mantissa, rounded to nearest even after every operation.</summary>
/// <remarks>
/// Only finite values in the normal range are supported (the editor's code never leaves it); zero is +0 or -0.
/// </remarks>
class X87Extended
{
public:
    /// <summary>Zero.</summary>
    constexpr X87Extended() = default;
    /// <summary><paramref name="value"/>, exact (fild).</summary>
    explicit X87Extended(int32_t value);
    /// <summary><paramref name="value"/>, exact (fld dword).</summary>
    explicit X87Extended(float value);
    /// <summary><paramref name="value"/>, exact (fld qword).</summary>
    explicit X87Extended(double value);

    /// <summary>The sum, rounded to 64 bits (fadd).</summary>
    friend X87Extended operator+(const X87Extended& a, const X87Extended& b);
    /// <summary>The difference, rounded to 64 bits (fsub).</summary>
    friend X87Extended operator-(const X87Extended& a, const X87Extended& b);
    /// <summary>The product, rounded to 64 bits (fmul).</summary>
    friend X87Extended operator*(const X87Extended& a, const X87Extended& b);
    /// <summary>The quotient, rounded to 64 bits (fdiv); the divisor isn't zero.</summary>
    friend X87Extended operator/(const X87Extended& a, const X87Extended& b);

    /// <summary>The absolute value (fabs).</summary>
    X87Extended abs() const;
    /// <summary>The value rounded to a float (fstp dword).</summary>
    float toFloat() const;
    /// <summary>The value rounded to a double (fstp qword).</summary>
    double toDouble() const;
    /// <summary>
    /// The value truncated toward zero as a 64-bit integer (__ftol's fistp qword; callers keep its low word, eax), the
    /// integer indefinite 0x8000000000000000 when it doesn't fit.
    /// </summary>
    int64_t truncate() const;

private:
    /// <summary>
    /// The value (<paramref name="high"/>:<paramref name="low"/>) x 2^(<paramref name="exponent"/> - 127), rounded to a
    /// 64-bit mantissa.
    /// </summary>
    static X87Extended round(bool negative, int32_t exponent, uint64_t high, uint64_t low);
    /// <summary>The sum of two values, the second negated when <paramref name="subtract"/>.</summary>
    static X87Extended add(const X87Extended& a, const X87Extended& b, bool subtract);
    /// <summary>The mantissa rounded to <paramref name="bits"/> bits and the exponent of its top bit.</summary>
    void roundMantissa(int32_t bits, uint64_t* mantissa, int32_t* exponent) const;

    /// <summary>Whether the value is negative.</summary>
    bool _negative = false;
    /// <summary>The power of two of the mantissa's top bit.</summary>
    int32_t _exponent = 0;
    /// <summary>The mantissa, its top bit set; 0 for zero.</summary>
    uint64_t _mantissa = 0;
};
