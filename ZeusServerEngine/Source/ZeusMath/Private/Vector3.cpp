#include "Vector3.hpp"

#include "MathUtils.hpp"

#include <cmath>
#include <sstream>

namespace Zeus::Math
{
const Vector3 Vector3::Zero{0.0, 0.0, 0.0};
const Vector3 Vector3::One{1.0, 1.0, 1.0};
const Vector3 Vector3::Up{0.0, 0.0, 1.0};
const Vector3 Vector3::Forward{1.0, 0.0, 0.0};
const Vector3 Vector3::Right{0.0, 1.0, 0.0};

double Vector3::Length() const
{
    return std::sqrt(LengthSquared());
}

double Vector3::LengthSquared() const
{
    return X * X + Y * Y + Z * Z;
}

Vector3 Vector3::Normalized() const
{
    const double lenSq = LengthSquared();
    if (lenSq < 1e-20)
    {
        return Vector3::Zero;
    }
    const double inv = 1.0 / std::sqrt(lenSq);
    return Vector3(X * inv, Y * inv, Z * inv);
}

bool Vector3::IsNearlyZero(const double tolerance) const
{
    return NearlyEqual(X, 0.0, tolerance) && NearlyEqual(Y, 0.0, tolerance) && NearlyEqual(Z, 0.0, tolerance);
}

double Vector3::Dot(const Vector3& a, const Vector3& b)
{
    return a.X * b.X + a.Y * b.Y + a.Z * b.Z;
}

Vector3 Vector3::Cross(const Vector3& a, const Vector3& b)
{
    return Vector3(
        a.Y * b.Z - a.Z * b.Y,
        a.Z * b.X - a.X * b.Z,
        a.X * b.Y - a.Y * b.X);
}

double Vector3::Distance(const Vector3& a, const Vector3& b)
{
    return (a - b).Length();
}

double Vector3::DistanceSquared(const Vector3& a, const Vector3& b)
{
    return (a - b).LengthSquared();
}

std::string Vector3::ToString() const
{
    std::ostringstream oss;
    oss << "(" << X << ", " << Y << ", " << Z << ")";
    return oss.str();
}

Vector3 operator+(const Vector3& a, const Vector3& b)
{
    return Vector3(a.X + b.X, a.Y + b.Y, a.Z + b.Z);
}

Vector3 operator-(const Vector3& a, const Vector3& b)
{
    return Vector3(a.X - b.X, a.Y - b.Y, a.Z - b.Z);
}

Vector3 operator*(const Vector3& v, const double s)
{
    return Vector3(v.X * s, v.Y * s, v.Z * s);
}

Vector3 operator*(const double s, const Vector3& v)
{
    return v * s;
}

Vector3 operator/(const Vector3& v, const double s)
{
    return Vector3(v.X / s, v.Y / s, v.Z / s);
}

Vector3& operator+=(Vector3& a, const Vector3& b)
{
    a.X += b.X;
    a.Y += b.Y;
    a.Z += b.Z;
    return a;
}

Vector3& operator-=(Vector3& a, const Vector3& b)
{
    a.X -= b.X;
    a.Y -= b.Y;
    a.Z -= b.Z;
    return a;
}

Vector3& operator*=(Vector3& v, const double s)
{
    v.X *= s;
    v.Y *= s;
    v.Z *= s;
    return v;
}

Vector3& operator/=(Vector3& v, const double s)
{
    v.X /= s;
    v.Y /= s;
    v.Z /= s;
    return v;
}

bool operator==(const Vector3& a, const Vector3& b)
{
    return NearlyEqual(a.X, b.X) && NearlyEqual(a.Y, b.Y) && NearlyEqual(a.Z, b.Z);
}

bool operator!=(const Vector3& a, const Vector3& b)
{
    return !(a == b);
}
} // namespace Zeus::Math
