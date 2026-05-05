#include "Bounds.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>

namespace Zeus::Math
{
Aabb Aabb::Empty()
{
    const double inf = std::numeric_limits<double>::infinity();
    return Aabb{Vector3(inf, inf, inf), Vector3(-inf, -inf, -inf)};
}

Vector3 Aabb::GetCenter() const
{
    return (Min + Max) * 0.5;
}

Vector3 Aabb::GetExtent() const
{
    return (Max - Min) * 0.5;
}

bool Aabb::IsValid() const
{
    return Min.X <= Max.X && Min.Y <= Max.Y && Min.Z <= Max.Z;
}

bool Aabb::Contains(const Vector3& point) const
{
    return point.X >= Min.X && point.X <= Max.X && point.Y >= Min.Y && point.Y <= Max.Y && point.Z >= Min.Z &&
           point.Z <= Max.Z;
}

bool Aabb::Intersects(const Aabb& other) const
{
    return Min.X <= other.Max.X && Max.X >= other.Min.X && Min.Y <= other.Max.Y && Max.Y >= other.Min.Y &&
           Min.Z <= other.Max.Z && Max.Z >= other.Min.Z;
}

void Aabb::ExpandToInclude(const Vector3& point)
{
    Min.X = std::min(Min.X, point.X);
    Min.Y = std::min(Min.Y, point.Y);
    Min.Z = std::min(Min.Z, point.Z);
    Max.X = std::max(Max.X, point.X);
    Max.Y = std::max(Max.Y, point.Y);
    Max.Z = std::max(Max.Z, point.Z);
}

void Aabb::ExpandToInclude(const Aabb& other)
{
    if (!other.IsValid())
    {
        return;
    }
    if (!IsValid())
    {
        *this = other;
        return;
    }
    ExpandToInclude(other.Min);
    ExpandToInclude(other.Max);
}

std::string Aabb::ToString() const
{
    std::ostringstream oss;
    oss << "Aabb[min=" << Min.ToString() << ", max=" << Max.ToString() << "]";
    return oss.str();
}
} // namespace Zeus::Math
