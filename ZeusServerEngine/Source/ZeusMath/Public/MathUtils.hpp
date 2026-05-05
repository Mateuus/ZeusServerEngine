#pragma once

namespace Zeus::Math
{
double DegreesToRadians(double degrees);
double RadiansToDegrees(double radians);
bool NearlyEqual(double a, double b, double tolerance = 1e-6);
double Clamp(double value, double minValue, double maxValue);
} // namespace Zeus::Math
