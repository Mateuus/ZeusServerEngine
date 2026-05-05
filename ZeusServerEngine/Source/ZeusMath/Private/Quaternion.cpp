#include "Quaternion.hpp"

#include "MathUtils.hpp"
#include "Rotator.hpp"

#include <cmath>
#include <sstream>

namespace Zeus::Math
{
const Quaternion Quaternion::Identity{0.0, 0.0, 0.0, 1.0};

Quaternion Quaternion::Normalized() const
{
    const double lenSq = X * X + Y * Y + Z * Z + W * W;
    if (lenSq < 1e-20)
    {
        return Quaternion::Identity;
    }
    const double inv = 1.0 / std::sqrt(lenSq);
    return Quaternion(X * inv, Y * inv, Z * inv, W * inv);
}

bool Quaternion::IsNearlyIdentity(const double tolerance) const
{
    const Quaternion n = Normalized();
    return NearlyEqual(n.X, 0.0, tolerance) && NearlyEqual(n.Y, 0.0, tolerance) && NearlyEqual(n.Z, 0.0, tolerance) &&
           NearlyEqual(n.W, 1.0, tolerance);
}

Quaternion Quaternion::FromRotator(const Rotator& rotator)
{
    const double pitch = DegreesToRadians(rotator.Pitch) * 0.5;
    const double yaw = DegreesToRadians(rotator.Yaw) * 0.5;
    const double roll = DegreesToRadians(rotator.Roll) * 0.5;

    const double sp = std::sin(pitch);
    const double cp = std::cos(pitch);
    const double sy = std::sin(yaw);
    const double cy = std::cos(yaw);
    const double sr = std::sin(roll);
    const double cr = std::cos(roll);

    // Ordem aproximada Roll (X) → Pitch (Y) → Yaw (Z), compatível com conversão inversa abaixo.
    const double x = sr * cp * cy - cr * sp * sy;
    const double y = cr * sp * cy + sr * cp * sy;
    const double z = cr * cp * sy - sr * sp * cy;
    const double w = cr * cp * cy + sr * sp * sy;
    return Quaternion(x, y, z, w).Normalized();
}

Rotator Quaternion::ToRotator() const
{
    const Quaternion q = Normalized();

    const double sinrCosp = 2.0 * (q.W * q.X + q.Y * q.Z);
    const double cosrCosp = 1.0 - 2.0 * (q.X * q.X + q.Y * q.Y);
    const double roll = RadiansToDegrees(std::atan2(sinrCosp, cosrCosp));

    double sinp = 2.0 * (q.W * q.Y - q.Z * q.X);
    double pitch;
    if (std::abs(sinp) >= 1.0)
    {
        pitch = std::copysign(90.0, sinp);
    }
    else
    {
        pitch = RadiansToDegrees(std::asin(sinp));
    }

    const double sinyCosp = 2.0 * (q.W * q.Z + q.X * q.Y);
    const double cosyCosp = 1.0 - 2.0 * (q.Y * q.Y + q.Z * q.Z);
    const double yaw = RadiansToDegrees(std::atan2(sinyCosp, cosyCosp));

    return Rotator(pitch, yaw, roll);
}

std::string Quaternion::ToString() const
{
    std::ostringstream oss;
    oss << "(X=" << X << ", Y=" << Y << ", Z=" << Z << ", W=" << W << ")";
    return oss.str();
}

Quaternion operator*(const Quaternion& a, const Quaternion& b)
{
    return Quaternion(
        a.W * b.X + a.X * b.W + a.Y * b.Z - a.Z * b.Y,
        a.W * b.Y - a.X * b.Z + a.Y * b.W + a.Z * b.X,
        a.W * b.Z + a.X * b.Y - a.Y * b.X + a.Z * b.W,
        a.W * b.W - a.X * b.X - a.Y * b.Y - a.Z * b.Z);
}
} // namespace Zeus::Math
