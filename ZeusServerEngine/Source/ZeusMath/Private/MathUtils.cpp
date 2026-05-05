#include "MathUtils.hpp"

#include "MathConstants.hpp"
#include "Vector3.hpp"

#include <algorithm>
#include <cmath>

namespace Zeus::Math
{
double DegreesToRadians(const double degrees)
{
    return degrees * DegToRad;
}

double RadiansToDegrees(const double radians)
{
    return radians * RadToDeg;
}

bool NearlyEqual(const double a, const double b, const double tolerance)
{
    return std::abs(a - b) <= tolerance;
}

double Clamp(const double value, const double minValue, const double maxValue)
{
    return std::min(maxValue, std::max(minValue, value));
}

bool IsWalkableFloor(const Vector3& floorNormal, const double maxSlopeAngleDeg)
{
    const double LengthSquared = floorNormal.LengthSquared();
    if (LengthSquared <= 1e-12)
    {
        return false;
    }
    const Vector3 Normalized = floorNormal.Normalized();
    const double Dot = Vector3::Dot(Normalized, Vector3::Up);
    const double MinDot = std::cos(maxSlopeAngleDeg * DegToRad);
    return Dot >= MinDot;
}
} // namespace Zeus::Math
