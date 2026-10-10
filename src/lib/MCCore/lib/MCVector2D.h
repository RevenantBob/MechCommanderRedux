#pragma once

// Original source: mcx\lib\cvmath.h (inline vector math).

/// <summary>A 2D vector (screen or map coordinates).</summary>
class MCVector2D
{
public:
    MCVector2D() = default;
    constexpr MCVector2D(float newX, float newY) : X(newX), Y(newY) {}

    MCVector2D& operator+=(const MCVector2D& v)
    {
        X += v.X;
        Y += v.Y;
        return *this;
    }

    MCVector2D& operator-=(const MCVector2D& v)
    {
        X -= v.X;
        Y -= v.Y;
        return *this;
    }

    MCVector2D& operator*=(float scale)
    {
        X *= scale;
        Y *= scale;
        return *this;
    }

    float X = 0.0f;
    float Y = 0.0f;
};

/// <summary>The difference of two 2D vectors.</summary>
inline MCVector2D operator-(const MCVector2D& a, const MCVector2D& b)
{
    return MCVector2D(a.X - b.X, a.Y - b.Y);
}

/// <summary>The sum of two 2D vectors.</summary>
inline MCVector2D operator+(const MCVector2D& a, const MCVector2D& b)
{
    return MCVector2D(a.X + b.X, a.Y + b.Y);
}
