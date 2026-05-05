#include "Rotator.hpp"

#include "MathUtils.hpp"

#include <cmath>
#include <sstream>

namespace Zeus::Math
{
const Rotator Rotator::Zero{0.0, 0.0, 0.0};

Rotator Rotator::Normalized() const
{
    return Rotator(
        std::fmod(Pitch + 180.0, 360.0) - 180.0,
        std::fmod(Yaw + 180.0, 360.0) - 180.0,
        std::fmod(Roll + 180.0, 360.0) - 180.0);
}

bool Rotator::IsNearlyZero(const double tolerance) const
{
    return NearlyEqual(Pitch, 0.0, tolerance) && NearlyEqual(Yaw, 0.0, tolerance) && NearlyEqual(Roll, 0.0, tolerance);
}

std::string Rotator::ToString() const
{
    std::ostringstream oss;
    oss << "(P=" << Pitch << ", Y=" << Yaw << ", R=" << Roll << ")";
    return oss.str();
}
} // namespace Zeus::Math
