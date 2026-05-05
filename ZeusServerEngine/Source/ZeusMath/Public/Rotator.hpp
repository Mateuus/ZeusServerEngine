#pragma once

#include <string>

namespace Zeus::Math
{
/**
 * Euler em graus (Pitch/Yaw/Roll), API próxima da Unreal.
 * Para interpolação e transformações internas preferir Quaternion.
 */
struct Rotator
{
    double Pitch = 0.0;
    double Yaw = 0.0;
    double Roll = 0.0;

    constexpr Rotator() = default;
    constexpr Rotator(double pitch, double yaw, double roll)
        : Pitch(pitch)
        , Yaw(yaw)
        , Roll(roll)
    {
    }

    static const Rotator Zero;

    Rotator Normalized() const;
    bool IsNearlyZero(double tolerance = 1e-6) const;

    std::string ToString() const;
};
} // namespace Zeus::Math
