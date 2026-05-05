#pragma once

namespace Zeus::Math
{
struct Vector3;

double DegreesToRadians(double degrees);
double RadiansToDegrees(double radians);
bool NearlyEqual(double a, double b, double tolerance = 1e-6);
double Clamp(double value, double minValue, double maxValue);

/**
 * Devolve `true` se a normal de um piso permite andar dado um ângulo máximo de
 * inclinação. Aproximação:
 *   walkable <=> dot(normalize(floorNormal), Up) >= cos(maxSlopeDeg)
 *
 * Aceita normais não-normalizadas: a normalização é interna. Para vector zero,
 * devolve `false`.
 */
bool IsWalkableFloor(const Vector3& floorNormal, double maxSlopeAngleDeg);
} // namespace Zeus::Math
