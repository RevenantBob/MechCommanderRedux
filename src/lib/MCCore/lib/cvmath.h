#pragma once

// Original source: mcx\lib\cvmath.h (inline vector math) and lib\cvmath.cpp (the random-number helpers and the unit
// vectors). World space is x/y on the ground and z up.

#include <cmath>

inline double AcosMatherr(double cosine)
{
    if (cosine >= -1.0 && cosine <= 1.0)
    {
        return std::acos(cosine);
    }

    return cosine >= 1.0 ? 0.0 : 3.14159265359;
}

/// <summary>A 2D vector (screen or map coordinates).</summary>
class MCVector2D
{
public:
    MCVector2D() = default;
    MCVector2D(float newX, float newY) : X(newX), Y(newY) {}

    /// <summary>Sets both components to 0.</summary>
    void Zero()
    {
        X = 0.0f;
        Y = 0.0f;
    }

    /// <summary>The length.</summary>
    float Magnitude() const { return std::sqrt(Y * Y + X * X); }

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

/// <summary>A 3D vector (world positions, velocities, directions).</summary>
class MCVector3D
{
public:
    MCVector3D() = default;
    MCVector3D(float newX, float newY, float newZ) : X(newX), Y(newY), Z(newZ) {}

    /// <summary>Sets every component to 0.</summary>
    void Zero()
    {
        X = 0.0f;
        Y = 0.0f;
        Z = 0.0f;
    }

    /// <summary>The length.</summary>
    double Magnitude() const
    {
        return std::sqrt((static_cast<double>(X) * X + static_cast<double>(Y) * Y) + static_cast<double>(Z) * Z);
    }

    /// <summary>Scales to length 1 (a zero vector stays zero).</summary>
    void Normalize()
    {
        const double length =
            std::sqrt((static_cast<double>(X) * X + static_cast<double>(Y) * Y) + static_cast<double>(Z) * Z);

        if (length > 0.0)
        {
            X = static_cast<float>(X / length);
            Y = static_cast<float>(Y / length);
            Z = static_cast<float>(Z / length);
        }
    }

    MCVector3D& operator+=(const MCVector3D& v)
    {
        X = v.X + X;
        Y = v.Y + Y;
        Z = v.Z + Z;
        return *this;
    }

    MCVector3D& operator-=(const MCVector3D& v)
    {
        X = X - v.X;
        Y = Y - v.Y;
        Z = Z - v.Z;
        return *this;
    }

    MCVector3D& operator*=(const float& scale)
    {
        X = scale * X;
        Y = Y * scale;
        Z = Z * scale;
        return *this;
    }

    MCVector3D& operator/=(const float& scale)
    {
        X = X / scale;
        Y = Y / scale;
        Z = Z / scale;
        return *this;
    }

    float X = 0.0f;
    float Y = 0.0f;
    float Z = 0.0f;
};

/// <summary>The sum of two vectors.</summary>
inline MCVector3D operator+(const MCVector3D& a, const MCVector3D& b)
{
    return MCVector3D(a.X + b.X, a.Y + b.Y, a.Z + b.Z);
}

/// <summary>The difference of two vectors.</summary>
inline MCVector3D operator-(const MCVector3D& a, const MCVector3D& b)
{
    return MCVector3D(a.X - b.X, a.Y - b.Y, a.Z - b.Z);
}

/// <summary>A vector scaled.</summary>
inline MCVector3D operator*(const MCVector3D& v, const float& scale)
{
    return MCVector3D(v.X * scale, v.Y * scale, v.Z * scale);
}

/// <summary>The dot product.</summary>
inline double operator|(const MCVector3D& a, const MCVector3D& b)
{
    return (static_cast<double>(a.Z) * b.Z + static_cast<double>(a.Y) * b.Y) + static_cast<double>(a.X) * b.X;
}

/// <summary>The cross product a x b.</summary>
inline MCVector3D operator&(const MCVector3D& a, const MCVector3D& b)
{
    return MCVector3D(static_cast<float>(static_cast<double>(b.Z) * a.Y - static_cast<double>(b.Y) * a.Z),
                      static_cast<float>(static_cast<double>(b.X) * a.Z - static_cast<double>(b.Z) * a.X),
                      static_cast<float>(static_cast<double>(b.Y) * a.X - static_cast<double>(b.X) * a.Y));
}

/// <summary>An orientation: three orthonormal axes.</summary>
class MCFrameOfRef
{
public:
    MCFrameOfRef() = default;
    MCFrameOfRef(const MCVector3D& newI, const MCVector3D& newJ, const MCVector3D& newK) : I(newI), J(newJ), K(newK) {}

    /// <summary>Makes the axes the world's (UnitX, UnitY, UnitZ).</summary>
    MCFrameOfRef& ResetToWorldFrame();

    /// <summary>acos of <paramref name="cosine"/> clamped to [-1, 1].</summary>
    double MyAcos(float cosine) const
    {
        if (cosine < -1.0)
        {
            cosine = -1.0f;
        }

        if (cosine > 1.0)
        {
            cosine = 1.0f;
        }

        return AcosMatherr(static_cast<double>(cosine));
    }

    MCFrameOfRef& operator=(const MCFrameOfRef&) = default;
    MCFrameOfRef(const MCFrameOfRef&) = default;

    MCVector3D I;
    MCVector3D J;
    MCVector3D K;
};

/// <summary>(1, 0, 0).</summary>
extern MCVector3D UnitX;
/// <summary>(0, 1, 0).</summary>
extern MCVector3D UnitY;
/// <summary>(0, 0, 1).</summary>
extern MCVector3D UnitZ;
/// <summary>(0, 0, 0).</summary>
extern MCVector3D NullVector3d;
/// <summary>The world frame (UnitX, UnitY, UnitZ).</summary>
extern MCFrameOfRef NullFrameOfRef;

inline MCFrameOfRef& MCFrameOfRef::ResetToWorldFrame()
{
    I = UnitX;
    J = UnitY;
    K = UnitZ;
    return *this;
}

/// <summary>A random number in [0, <paramref name="range"/>) from rand()'s 15 bits.</summary>
int32_t RandomNumber(int32_t range);

/// <summary>Whether a d100 roll comes under <paramref name="percent"/>.</summary>
int RollDice(int32_t percent);

/// <summary>A random number in [-<paramref name="range"/>, <paramref name="range"/>).</summary>
int32_t SignedRandomNumber(int32_t range);
