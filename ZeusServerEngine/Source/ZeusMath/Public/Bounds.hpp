#pragma once

#include "Vector3.hpp"

#include <string>

namespace Zeus::Math
{
struct Aabb
{
    Vector3 Min;
    Vector3 Max;

    static Aabb Empty();

    Vector3 GetCenter() const;
    Vector3 GetExtent() const;
    bool IsValid() const;

    bool Contains(const Vector3& point) const;
    bool Intersects(const Aabb& other) const;

    void ExpandToInclude(const Vector3& point);
    void ExpandToInclude(const Aabb& other);

    std::string ToString() const;
};

struct SphereBounds
{
    Vector3 Center = Vector3::Zero;
    double Radius = 0.0;
};
} // namespace Zeus::Math
