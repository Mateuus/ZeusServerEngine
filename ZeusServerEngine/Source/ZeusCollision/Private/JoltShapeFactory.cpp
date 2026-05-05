#include "JoltShapeFactory.hpp"

#if defined(ZEUS_HAS_JOLT) && ZEUS_HAS_JOLT

#include "CollisionDebug.hpp"
#include "JoltConversion.hpp"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>

#include <algorithm>
#include <sstream>

namespace Zeus::Collision
{
namespace
{
constexpr float MinDimensionMeters = 0.001f; // 1 mm

void LogShapeFailure(const char* shapeName, const char* error)
{
    std::ostringstream oss;
    oss << "[JoltShapeFactory] " << shapeName << " shape creation failed: " << error;
    CollisionDebug::LogWarn("Collision", oss.str());
}

JPH::ShapeRefC CreateBox(const CollisionShape& shape)
{
    const Math::Vector3& he = shape.Box.HalfExtents;
    const float hx = std::max(static_cast<float>(he.X * CmToMeters), MinDimensionMeters);
    const float hy = std::max(static_cast<float>(he.Y * CmToMeters), MinDimensionMeters);
    const float hz = std::max(static_cast<float>(he.Z * CmToMeters), MinDimensionMeters);
    JPH::BoxShapeSettings Settings(JPH::Vec3(hx, hy, hz));
    Settings.SetEmbedded();
    JPH::Shape::ShapeResult Result = Settings.Create();
    if (Result.HasError())
    {
        LogShapeFailure("Box", Result.GetError().c_str());
        return {};
    }
    return Result.Get();
}

JPH::ShapeRefC CreateSphere(const CollisionShape& shape)
{
    const float radius = std::max(static_cast<float>(shape.Sphere.Radius * CmToMeters),
        MinDimensionMeters);
    JPH::SphereShapeSettings Settings(radius);
    Settings.SetEmbedded();
    JPH::Shape::ShapeResult Result = Settings.Create();
    if (Result.HasError())
    {
        LogShapeFailure("Sphere", Result.GetError().c_str());
        return {};
    }
    return Result.Get();
}

JPH::ShapeRefC CreateCapsule(const CollisionShape& shape)
{
    const float radius = std::max(static_cast<float>(shape.Capsule.Radius * CmToMeters),
        MinDimensionMeters);
    const float halfHeight = std::max(static_cast<float>(shape.Capsule.HalfHeight * CmToMeters),
        MinDimensionMeters);
    JPH::CapsuleShapeSettings Settings(halfHeight, radius);
    Settings.SetEmbedded();
    JPH::Shape::ShapeResult Result = Settings.Create();
    if (Result.HasError())
    {
        LogShapeFailure("Capsule", Result.GetError().c_str());
        return {};
    }
    return Result.Get();
}

JPH::ShapeRefC CreateConvex(const CollisionShape& shape)
{
    if (shape.Convex.Vertices.size() < 4)
    {
        CollisionDebug::LogWarn("Collision",
            "[JoltShapeFactory] Convex with fewer than 4 vertices; skipped.");
        return {};
    }
    JPH::Array<JPH::Vec3> Points;
    Points.reserve(shape.Convex.Vertices.size());
    for (const Math::Vector3& V : shape.Convex.Vertices)
    {
        Points.push_back(ToJoltVec3(V));
    }
    JPH::ConvexHullShapeSettings Settings(Points);
    Settings.SetEmbedded();
    JPH::Shape::ShapeResult Result = Settings.Create();
    if (Result.HasError())
    {
        LogShapeFailure("Convex", Result.GetError().c_str());
        return {};
    }
    return Result.Get();
}
} // namespace

JPH::ShapeRefC JoltShapeFactory::CreateShape(const CollisionShape& shape)
{
    switch (shape.Type)
    {
    case ECollisionShapeType::Box:     return CreateBox(shape);
    case ECollisionShapeType::Sphere:  return CreateSphere(shape);
    case ECollisionShapeType::Capsule: return CreateCapsule(shape);
    case ECollisionShapeType::Convex:  return CreateConvex(shape);
    default:
    {
        std::ostringstream oss;
        oss << "[JoltShapeFactory] Unsupported shape type "
            << static_cast<unsigned>(shape.Type);
        CollisionDebug::LogWarn("Collision", oss.str());
        return {};
    }
    }
}
} // namespace Zeus::Collision

#endif // ZEUS_HAS_JOLT
