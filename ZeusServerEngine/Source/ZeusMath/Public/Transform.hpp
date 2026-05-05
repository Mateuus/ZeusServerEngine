#pragma once

#include "Quaternion.hpp"
#include "Vector3.hpp"

#include <string>

namespace Zeus::Math
{
struct Transform
{
    Vector3 Location = Vector3::Zero;
    Quaternion Rotation = Quaternion::Identity;
    Vector3 Scale3D = Vector3::One;

    Transform() = default;
    explicit Transform(const Vector3& location)
        : Location(location)
    {
    }
    Transform(const Vector3& location, const Quaternion& rotation, const Vector3& scale)
        : Location(location)
        , Rotation(rotation)
        , Scale3D(scale)
    {
    }

    static const Transform Identity;

    Vector3 TransformPosition(const Vector3& point) const;
    Vector3 TransformVector(const Vector3& vector) const;

    Transform Inverse() const;

    static Transform Combine(const Transform& parent, const Transform& child);

    std::string ToString() const;
};
} // namespace Zeus::Math
