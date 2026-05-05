#include "Transform.hpp"

#include "MathUtils.hpp"

#include <sstream>

namespace Zeus::Math
{
namespace
{
Vector3 QuatRotateVector(const Quaternion& qIn, const Vector3& v)
{
    const Quaternion q = qIn.Normalized();
    const Quaternion pure{v.X, v.Y, v.Z, 0.0};
    const Quaternion qConj(-q.X, -q.Y, -q.Z, q.W);
    const Quaternion t = q * pure * qConj;
    return Vector3(t.X, t.Y, t.Z);
}
} // namespace

const Transform Transform::Identity{Vector3::Zero, Quaternion::Identity, Vector3::One};

Vector3 Transform::TransformPosition(const Vector3& point) const
{
    const Vector3 scaled{point.X * Scale3D.X, point.Y * Scale3D.Y, point.Z * Scale3D.Z};
    const Vector3 rotated = QuatRotateVector(Rotation, scaled);
    return rotated + Location;
}

Vector3 Transform::TransformVector(const Vector3& vector) const
{
    const Vector3 scaled{vector.X * Scale3D.X, vector.Y * Scale3D.Y, vector.Z * Scale3D.Z};
    return QuatRotateVector(Rotation, scaled);
}

Transform Transform::Inverse() const
{
    const Vector3 invScale(
        (std::abs(Scale3D.X) > 1e-12) ? 1.0 / Scale3D.X : 0.0,
        (std::abs(Scale3D.Y) > 1e-12) ? 1.0 / Scale3D.Y : 0.0,
        (std::abs(Scale3D.Z) > 1e-12) ? 1.0 / Scale3D.Z : 0.0);

    const Quaternion invRot(-Rotation.X, -Rotation.Y, -Rotation.Z, Rotation.W);
    const Quaternion invRotN = invRot.Normalized();

    const Vector3 scaledLoc(
        Location.X * invScale.X,
        Location.Y * invScale.Y,
        Location.Z * invScale.Z);
    const Vector3 invLoc = QuatRotateVector(invRotN, Vector3(-scaledLoc.X, -scaledLoc.Y, -scaledLoc.Z));

    return Transform(invLoc, invRotN, Vector3(invScale.X, invScale.Y, invScale.Z));
}

Transform Transform::Combine(const Transform& parent, const Transform& child)
{
    Transform out;
    out.Scale3D = Vector3(
        parent.Scale3D.X * child.Scale3D.X,
        parent.Scale3D.Y * child.Scale3D.Y,
        parent.Scale3D.Z * child.Scale3D.Z);
    out.Rotation = (parent.Rotation * child.Rotation).Normalized();
    out.Location = parent.TransformPosition(child.Location);
    return out;
}

std::string Transform::ToString() const
{
    std::ostringstream oss;
    oss << "T[loc=" << Location.ToString() << ", rot=" << Rotation.ToString() << ", scale=" << Scale3D.ToString() << "]";
    return oss.str();
}
} // namespace Zeus::Math
