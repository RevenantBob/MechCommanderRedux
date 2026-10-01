#pragma once

// Original source: mcx\lib\cvmath.h (inline vector math) and lib\cvmath.cpp (the random-number helpers and the unit
// vectors). World space is x/y on the ground and z up.

/// <summary>A 2D vector (screen or map coordinates).</summary>
class vector_2d
{
public:
    vector_2d() = default;
    vector_2d(float newX, float newY) : x(newX), y(newY) {}

    /// <summary>Sets both components to 0.</summary>
    void zero()
    {
        x = 0.0f;
        y = 0.0f;
    }

    /// <summary>The length.</summary>
    float magnitude() const { return std::sqrt(y * y + x * x); }

    vector_2d& operator+=(const vector_2d& v)
    {
        x += v.x;
        y += v.y;
        return *this;
    }

    vector_2d& operator-=(const vector_2d& v)
    {
        x -= v.x;
        y -= v.y;
        return *this;
    }

    vector_2d& operator*=(float scale)
    {
        x *= scale;
        y *= scale;
        return *this;
    }

    float x = 0.0f; // +0x00
    float y = 0.0f; // +0x04
};

/// <summary>The difference of two 2D vectors.</summary>
/// <remarks>MCX.EXE @ 0x00664e40</remarks>
inline vector_2d operator-(const vector_2d& a, const vector_2d& b)
{
    return vector_2d(a.x - b.x, a.y - b.y);
}

/// <summary>The sum of two 2D vectors.</summary>
inline vector_2d operator+(const vector_2d& a, const vector_2d& b)
{
    return vector_2d(a.x + b.x, a.y + b.y);
}

/// <summary>A 3D vector (world positions, velocities, directions).</summary>
class vector_3d
{
public:
    vector_3d() = default;
    vector_3d(float newX, float newY, float newZ) : x(newX), y(newY), z(newZ) {}

    /// <summary>Sets every component to 0.</summary>
    void zero()
    {
        x = 0.0f;
        y = 0.0f;
        z = 0.0f;
    }

    /// <summary>The length.</summary>
    /// <remarks>MCX.EXE @ 0x00664a30</remarks>
    float magnitude() const { return std::sqrt(z * z + y * y + x * x); }

    /// <summary>Scales to length 1 (a zero vector stays zero).</summary>
    /// <remarks>MCX.EXE @ 0x0066ea20</remarks>
    void normalize()
    {
        const float length = std::sqrt(z * z + y * y + x * x);

        if (length != 0.0f)
        {
            x = x / length;
            y = y / length;
            z = z / length;
        }
    }

    /// <remarks>MCX.EXE @ 0x006af3f0</remarks>
    vector_3d& operator+=(const vector_3d& v)
    {
        x = v.x + x;
        y = v.y + y;
        z = v.z + z;
        return *this;
    }

    vector_3d& operator-=(const vector_3d& v)
    {
        x = x - v.x;
        y = y - v.y;
        z = z - v.z;
        return *this;
    }

    /// <remarks>MCX.EXE @ 0x0063dee0</remarks>
    vector_3d& operator*=(const float& scale)
    {
        x = scale * x;
        y = y * scale;
        z = z * scale;
        return *this;
    }

    vector_3d& operator/=(const float& scale)
    {
        x = x / scale;
        y = y / scale;
        z = z / scale;
        return *this;
    }

    float x = 0.0f; // +0x00
    float y = 0.0f; // +0x04
    float z = 0.0f; // +0x08
};

/// <summary>The sum of two vectors.</summary>
/// <remarks>MCX.EXE @ 0x0063de20</remarks>
inline vector_3d operator+(const vector_3d& a, const vector_3d& b)
{
    return vector_3d(a.x + b.x, a.y + b.y, a.z + b.z);
}

/// <summary>The difference of two vectors.</summary>
/// <remarks>MCX.EXE @ 0x0063de60</remarks>
inline vector_3d operator-(const vector_3d& a, const vector_3d& b)
{
    return vector_3d(a.x - b.x, a.y - b.y, a.z - b.z);
}

/// <summary>A vector scaled.</summary>
/// <remarks>MCX.EXE @ 0x0063dea0</remarks>
inline vector_3d operator*(const vector_3d& v, const float& scale)
{
    return vector_3d(v.x * scale, v.y * scale, v.z * scale);
}

/// <summary>The dot product.</summary>
/// <remarks>MCX.EXE @ 0x00658270</remarks>
inline float operator|(const vector_3d& a, const vector_3d& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

/// <summary>The cross product a x b.</summary>
/// <remarks>MCX.EXE @ 0x006649e0</remarks>
inline vector_3d operator&(const vector_3d& a, const vector_3d& b)
{
    return vector_3d(b.z * a.y - b.y * a.z, b.x * a.z - b.z * a.x, b.y * a.x - b.x * a.y);
}

/// <summary>An orientation: three orthonormal axes.</summary>
class frame_of_ref
{
public:
    frame_of_ref() = default;
    frame_of_ref(const vector_3d& newI, const vector_3d& newJ, const vector_3d& newK) : i(newI), j(newJ), k(newK) {}

    /// <summary>Makes the axes the world's (UnitX, UnitY, UnitZ).</summary>
    /// <remarks>MCX.EXE @ 0x0065b550</remarks>
    frame_of_ref& reset_to_world_frame();

    /// <summary>acos of <paramref name="cosine"/> clamped to [-1, 1].</summary>
    /// <remarks>MCX.EXE @ 0x0067af50</remarks>
    float my_acos(float cosine)
    {
        if (cosine < -1.0)
        {
            cosine = -1.0f;
        }

        if (cosine > 1.0)
        {
            cosine = 1.0f;
        }

        return static_cast<float>(std::acos(static_cast<double>(cosine)));
    }

    /// <remarks>MCX.EXE @ 0x0067d670</remarks>
    frame_of_ref& operator=(const frame_of_ref&) = default;
    frame_of_ref(const frame_of_ref&) = default;

    vector_3d i; // +0x00
    vector_3d j; // +0x0c
    vector_3d k; // +0x18
};

/// <summary>(1, 0, 0).</summary>
extern vector_3d UnitX;
/// <summary>(0, 1, 0).</summary>
extern vector_3d UnitY;
/// <summary>(0, 0, 1).</summary>
extern vector_3d UnitZ;
/// <summary>(0, 0, 0).</summary>
extern vector_3d NULL_vector_3d;
/// <summary>The world frame (UnitX, UnitY, UnitZ).</summary>
extern frame_of_ref NULL_frame_of_ref;

inline frame_of_ref& frame_of_ref::reset_to_world_frame()
{
    i = UnitX;
    j = UnitY;
    k = UnitZ;
    return *this;
}

/// <summary>A random number in [0, <paramref name="range"/>) from rand()'s 15 bits.</summary>
/// <remarks>MCX.EXE @ 0x00644b60</remarks>
int32_t RandomNumber(int32_t range);

/// <summary>Whether a d100 roll comes under <paramref name="percent"/>.</summary>
/// <remarks>MCX.EXE @ 0x00644b80</remarks>
int RollDice(int32_t percent);

/// <summary>A random number in [-<paramref name="range"/>, <paramref name="range"/>).</summary>
/// <remarks>MCX.EXE @ 0x00644bb0</remarks>
int32_t SignedRandomNumber(int32_t range);
