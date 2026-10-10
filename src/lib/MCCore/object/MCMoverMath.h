#pragma once

#include "lib/MCFrameOfRef.h"
#include "lib/MCVector3D.h"

/// <summary>The angle constants and frame turns the movers' and turrets' code shares, as MCX.EXE computes them.</summary>
namespace MCMoverMath
{
    /// <summary>Half pi, as MCX.EXE stores it.</summary>
    inline constexpr double HalfPi = 0x1.921fb5443e88cp+0;
    /// <summary>Degrees to radians, as MCX.EXE stores it (a hair under pi / 180).</summary>
    inline constexpr double DegreesToRadians = 0x1.1df46a2526c7ap-6;
    /// <summary>Radians to degrees, as MCX.EXE stores it.</summary>
    inline constexpr double RadiansToDegrees = 0x1.ca5dc1a6402aap+5;

    /// <summary>Turns a frame about its k axis (MC2's inline frame_of_ref::rotate_about_k).</summary>
    inline void RotateAboutK(MCFrameOfRef& frame, float s, float c)
    {
        const MCVector3D oldI = frame.I;
        frame.I = frame.I * c + frame.J * s;
        frame.J = frame.J * c - oldI * s;
    }

    /// <summary>
    /// <see cref="RotateAboutK"/> with the cosine kept in double, as the tree deflection's inlined turn computes
    /// it.
    /// </summary>
    inline void RotateAboutKUnroundedCos(MCFrameOfRef& frame, float s, double c)
    {
        const MCVector3D oldI = frame.I;
        const MCVector3D oldJ = frame.J;
        const double sd = static_cast<double>(s);
        frame.I.X = static_cast<float>(c * oldI.X + sd * oldJ.X);
        frame.I.Y = static_cast<float>(c * oldI.Y) + s * oldJ.Y;
        frame.I.Z = static_cast<float>(c * oldI.Z) + s * oldJ.Z;
        frame.J.X = static_cast<float>(c * oldJ.X) - s * oldI.X;
        frame.J.Y = static_cast<float>(c * oldJ.Y) - s * oldI.Y;
        frame.J.Z = static_cast<float>(c * oldJ.Z - static_cast<double>(s * oldI.Z));
    }

    /// <summary>
    /// The sine and cosine of a facing snapped to the sprites' 32 directions (-45 and 45 are exact), as a mech turns
    /// its hot spot and jump jet offsets.
    /// </summary>
    inline void SnappedFacing(double facing, double& s, double& c)
    {
        const float rotation = -(static_cast<float>(static_cast<int32_t>(facing * (1.0 / 11.25))) * 11.25f);

        if (rotation == 45.0f)
        {
            s = 0.70710677f;
            c = 0.70710677f;
        }
        else if (rotation == -45.0f)
        {
            s = -0.70710677f;
            c = 0.70710677f;
        }
        else
        {
            s = std::sin(static_cast<double>(rotation) * DegreesToRadians);
            c = static_cast<float>(std::cos(static_cast<double>(static_cast<float>(rotation * DegreesToRadians))));
        }
    }

    /// <summary>The facing of <paramref name="frame"/> in degrees, from the world's x axis (negative for a negative
    /// y).</summary>
    inline double ExactFrameFacing(const MCFrameOfRef& frame)
    {
        const float cosine = UnitX.Z * frame.I.Z + UnitX.Y * frame.I.Y + UnitX.X * frame.I.X;
        double facing = MCFrameOfRef::MyAcos(cosine) * RadiansToDegrees;

        if (frame.I.Y < 0.0f)
        {
            facing = -facing;
        }

        return facing;
    }
}
