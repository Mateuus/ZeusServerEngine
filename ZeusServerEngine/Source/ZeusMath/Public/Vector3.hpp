#pragma once

#include <string>

namespace Zeus::Math
{
/**
 * Vetor 3D em centímetros (1 unidade Zeus = 1 cm Unreal).
 * Eixos: X Forward, Y Right, Z Up.
 */
struct Vector3
{
    double X = 0.0;
    double Y = 0.0;
    double Z = 0.0;

    constexpr Vector3() = default;
    constexpr Vector3(double x, double y, double z)
        : X(x)
        , Y(y)
        , Z(z)
    {
    }

    static const Vector3 Zero;
    static const Vector3 One;
    static const Vector3 Up;
    static const Vector3 Forward;
    static const Vector3 Right;

    double Length() const;
    double LengthSquared() const;

    Vector3 Normalized() const;
    bool IsNearlyZero(double tolerance = 1e-6) const;

    static double Dot(const Vector3& a, const Vector3& b);
    static Vector3 Cross(const Vector3& a, const Vector3& b);
    static double Distance(const Vector3& a, const Vector3& b);
    static double DistanceSquared(const Vector3& a, const Vector3& b);

    std::string ToString() const;
};

Vector3 operator+(const Vector3& a, const Vector3& b);
Vector3 operator-(const Vector3& a, const Vector3& b);
Vector3 operator*(const Vector3& v, double s);
Vector3 operator*(double s, const Vector3& v);
Vector3 operator/(const Vector3& v, double s);
Vector3& operator+=(Vector3& a, const Vector3& b);
Vector3& operator-=(Vector3& a, const Vector3& b);
Vector3& operator*=(Vector3& v, double s);
Vector3& operator/=(Vector3& v, double s);
bool operator==(const Vector3& a, const Vector3& b);
bool operator!=(const Vector3& a, const Vector3& b);
} // namespace Zeus::Math
