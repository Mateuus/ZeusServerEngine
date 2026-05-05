#pragma once

#include <string>

namespace Zeus::Math
{
struct Rotator;

struct Quaternion
{
    double X = 0.0;
    double Y = 0.0;
    double Z = 0.0;
    double W = 1.0;

    constexpr Quaternion() = default;
    constexpr Quaternion(double x, double y, double z, double w)
        : X(x)
        , Y(y)
        , Z(z)
        , W(w)
    {
    }

    static const Quaternion Identity;

    Quaternion Normalized() const;
    bool IsNearlyIdentity(double tolerance = 1e-6) const;

    static Quaternion FromRotator(const Rotator& rotator);
    Rotator ToRotator() const;

    std::string ToString() const;
};

Quaternion operator*(const Quaternion& a, const Quaternion& b);
} // namespace Zeus::Math
