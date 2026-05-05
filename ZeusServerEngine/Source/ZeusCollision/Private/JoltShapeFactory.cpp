#include "JoltShapeFactory.hpp"

#if defined(ZEUS_HAS_JOLT) && ZEUS_HAS_JOLT

#include "CollisionDebug.hpp"
#include "JoltConversion.hpp"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
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

JPH::ShapeRefC JoltShapeFactory::CreateMeshShape(const TerrainPiece& piece)
{
    if (piece.Kind != ETerrainPieceKind::TriangleMesh)
    {
        CollisionDebug::LogWarn("Collision",
            "[JoltShapeFactory] CreateMeshShape called on non-TriangleMesh piece");
        return {};
    }
    const auto& Vertices = piece.TriangleMesh.Vertices;
    const auto& Indices = piece.TriangleMesh.Indices;
    if (Vertices.size() < 3 || Indices.size() < 3 || (Indices.size() % 3) != 0)
    {
        std::ostringstream oss;
        oss << "[JoltShapeFactory] TriangleMesh '" << piece.Name
            << "' has invalid topology (verts=" << Vertices.size()
            << " indices=" << Indices.size() << ")";
        CollisionDebug::LogWarn("Collision", oss.str());
        return {};
    }

    JPH::VertexList JoltVertices;
    JoltVertices.reserve(Vertices.size());
    for (const Math::Vector3& V : Vertices)
    {
        JoltVertices.push_back(JPH::Float3(static_cast<float>(V.X * CmToMeters),
                                           static_cast<float>(V.Y * CmToMeters),
                                           static_cast<float>(V.Z * CmToMeters)));
    }

    JPH::IndexedTriangleList Triangles;
    Triangles.reserve(Indices.size() / 3);
    for (std::size_t t = 0; t + 2 < Indices.size(); t += 3)
    {
        Triangles.push_back(JPH::IndexedTriangle(
            Indices[t + 0], Indices[t + 1], Indices[t + 2], 0));
    }

    JPH::MeshShapeSettings Settings(std::move(JoltVertices), std::move(Triangles));
    Settings.SetEmbedded();
    JPH::Shape::ShapeResult Result = Settings.Create();
    if (Result.HasError())
    {
        LogShapeFailure("TriangleMesh", Result.GetError().c_str());
        return {};
    }
    return Result.Get();
}

JPH::ShapeRefC JoltShapeFactory::CreateHeightFieldShape(const TerrainPiece& piece)
{
    if (piece.Kind != ETerrainPieceKind::HeightField)
    {
        CollisionDebug::LogWarn("Collision",
            "[JoltShapeFactory] CreateHeightFieldShape called on non-HeightField piece");
        return {};
    }
    const TerrainHeightField& HF = piece.HeightField;
    const std::uint32_t SX = HF.SamplesX;
    const std::uint32_t SY = HF.SamplesY;
    if (SX < 2 || SY < 2 || SX != SY)
    {
        std::ostringstream oss;
        oss << "[JoltShapeFactory] HeightField '" << piece.Name
            << "' invalid dims (Jolt requires square SamplesX==SamplesY): "
            << SX << "x" << SY;
        CollisionDebug::LogWarn("Collision", oss.str());
        return {};
    }
    const std::size_t SampleCount = static_cast<std::size_t>(SX) * static_cast<std::size_t>(SY);
    if (HF.Heights.size() != SampleCount)
    {
        CollisionDebug::LogWarn("Collision",
            "[JoltShapeFactory] HeightField sample count mismatch.");
        return {};
    }

    // Heights chegam em "raw"; valor final em cm = h * HeightScaleCm.
    std::vector<float> HeightsM(SampleCount);
    for (std::size_t i = 0; i < SampleCount; ++i)
    {
        const double cm = static_cast<double>(HF.Heights[i]) * HF.HeightScaleCm;
        HeightsM[i] = static_cast<float>(cm * CmToMeters);
    }

    const JPH::Vec3 OffsetM(static_cast<float>(HF.OriginLocal.X * CmToMeters),
                            static_cast<float>(HF.OriginLocal.Y * CmToMeters),
                            static_cast<float>(HF.OriginLocal.Z * CmToMeters));
    const JPH::Vec3 ScaleM(static_cast<float>(HF.SampleSpacingCm * CmToMeters),
                           1.0f,
                           static_cast<float>(HF.SampleSpacingCm * CmToMeters));

    JPH::HeightFieldShapeSettings Settings(HeightsM.data(), OffsetM, ScaleM, SX);
    Settings.SetEmbedded();
    JPH::Shape::ShapeResult Result = Settings.Create();
    if (Result.HasError())
    {
        LogShapeFailure("HeightField", Result.GetError().c_str());
        return {};
    }
    return Result.Get();
}
} // namespace Zeus::Collision

#endif // ZEUS_HAS_JOLT
