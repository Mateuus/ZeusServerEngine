#include "MathUtils.hpp"

#include "MathConstants.hpp"

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
} // namespace Zeus::Math
