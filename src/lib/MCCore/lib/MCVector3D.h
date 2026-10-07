#pragma once

// Original source: mcx\lib\cvmath.h (inline vector math) and lib\cvmath.cpp (the unit vectors). World space is
// x/y on the ground and z up.

/// <summary>A 3D vector (world positions, velocities, directions).</summary>
class MCVector3D
{
public:
    MCVector3D() = default;
    constexpr MCVector3D(float newX, float newY, float newZ) : X(newX), Y(newY), Z(newZ) {}

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

/// <summary>(1, 0, 0).</summary>
inline constexpr MCVector3D UnitX(1.0f, 0.0f, 0.0f);
/// <summary>(0, 1, 0).</summary>
inline constexpr MCVector3D UnitY(0.0f, 1.0f, 0.0f);
/// <summary>(0, 0, 1).</summary>
inline constexpr MCVector3D UnitZ(0.0f, 0.0f, 1.0f);
/// <summary>(0, 0, 0).</summary>
inline constexpr MCVector3D NullVector3d(0.0f, 0.0f, 0.0f);
