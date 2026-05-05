#include "DynamicCollisionWorld.hpp"

#include "CollisionDebug.hpp"
#include "Quaternion.hpp"
#include "Transform.hpp"
#include "Vector3.hpp"
#include "ZsmDynamicLoader.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace Zeus::Collision
{
namespace
{
/** Inverte a rotacao de um quaternion unit-norm (conjugado). */
Math::Quaternion InverseRotation(const Math::Quaternion& q)
{
    const Math::Quaternion qn = q.Normalized();
    return Math::Quaternion(-qn.X, -qn.Y, -qn.Z, qn.W);
}

/** Transforma `worldPoint` para o frame local de uma transform (rotacao apenas, scale 1). */
Math::Vector3 WorldToLocal(const Math::Transform& T, const Math::Vector3& worldPoint)
{
    const Math::Quaternion invRot = InverseRotation(T.Rotation);
    const Math::Transform invT(Math::Vector3::Zero, invRot, Math::Vector3::One);
    return invT.TransformVector(worldPoint - T.Location);
}

bool ContainsLocalAabb(const Math::Vector3& localPoint, const Math::Vector3& halfExtents)
{
    return std::abs(localPoint.X) <= halfExtents.X &&
           std::abs(localPoint.Y) <= halfExtents.Y &&
           std::abs(localPoint.Z) <= halfExtents.Z;
}

bool ContainsSphere(const Math::Vector3& localPoint, double radius)
{
    return localPoint.LengthSquared() <= (radius * radius);
}

bool ContainsCapsule(const Math::Vector3& localPoint, double radius, double halfHeight)
{
    // Capsule eixo Z. Clamp a Z em [-halfHeight, halfHeight] e mede distancia ao eixo.
    const double cz = std::clamp(localPoint.Z, -halfHeight, halfHeight);
    const double dx = localPoint.X;
    const double dy = localPoint.Y;
    const double dz = localPoint.Z - cz;
    return (dx * dx + dy * dy + dz * dz) <= (radius * radius);
}
} // namespace

DynamicCollisionWorld::DynamicCollisionWorld() = default;
DynamicCollisionWorld::~DynamicCollisionWorld()
{
    Shutdown();
}

bool DynamicCollisionWorld::LoadFromZsm(const std::filesystem::path& path)
{
    Asset = {};
    bAssetLoaded = false;
    LoadedRegions.clear();
    ActiveVolumeIndices.clear();

    std::ostringstream oss;
    oss << "[Collision] Loading dynamic " << path.string();
    CollisionDebug::LogInfo("Collision", oss.str());

    ZsmDynamicLoader Loader;
    ZsmLoadResult Result = Loader.LoadFromFile(path, Asset);
    if (!Result.bSuccess)
    {
        std::ostringstream err;
        err << "[Collision] Failed to load dynamic " << path.string() << ": " << Result.Error;
        CollisionDebug::LogWarn("Collision", err.str());
        return false;
    }
    for (const std::string& Warning : Result.Warnings)
    {
        CollisionDebug::LogWarn("Collision", CollisionDebug::Concat("[Collision] ", Warning));
    }
    bAssetLoaded = true;

    std::ostringstream summary;
    summary << "[Collision] Dynamic loaded volumes=" << Asset.Stats.VolumeCount
            << " (Trigger=" << Asset.Stats.TriggerCount
            << " Water=" << Asset.Stats.WaterCount
            << " Lava=" << Asset.Stats.LavaCount
            << " Kill=" << Asset.Stats.KillVolumeCount
            << " SafeZone=" << Asset.Stats.SafeZoneCount
            << ") shapes=" << Asset.Stats.ShapeCount;
    CollisionDebug::LogInfo("Collision", summary.str());
    return true;
}

bool DynamicCollisionWorld::LoadFromAsset(DynamicCollisionAsset asset)
{
    Asset = std::move(asset);
    bAssetLoaded = true;
    LoadedRegions.clear();
    ActiveVolumeIndices.clear();
    RebuildDynamicVolumeIndexByRegion(Asset);
    return true;
}

bool DynamicCollisionWorld::LoadRegion(std::uint32_t regionId)
{
    if (!bAssetLoaded)
    {
        return false;
    }
    if (LoadedRegions.count(regionId) != 0)
    {
        return true;
    }
    const auto it = Asset.VolumeIndexByRegion.find(regionId);
    if (it == Asset.VolumeIndexByRegion.end())
    {
        LoadedRegions.insert(regionId); // mesmo sem volumes, marcamos como carregada
        Stats.LoadedRegions = LoadedRegions.size();
        return true;
    }
    for (std::size_t volIdx : it->second)
    {
        ActiveVolumeIndices.insert(volIdx);
    }
    LoadedRegions.insert(regionId);
    Stats.LoadedRegions = LoadedRegions.size();
    Stats.ActiveVolumes = ActiveVolumeIndices.size();
    return true;
}

bool DynamicCollisionWorld::UnloadRegion(std::uint32_t regionId)
{
    if (!bAssetLoaded || LoadedRegions.count(regionId) == 0)
    {
        return false;
    }
    LoadedRegions.erase(regionId);
    const auto it = Asset.VolumeIndexByRegion.find(regionId);
    if (it != Asset.VolumeIndexByRegion.end())
    {
        for (std::size_t volIdx : it->second)
        {
            ActiveVolumeIndices.erase(volIdx);
        }
    }
    Stats.LoadedRegions = LoadedRegions.size();
    Stats.ActiveVolumes = ActiveVolumeIndices.size();
    return true;
}

bool DynamicCollisionWorld::IsRegionLoaded(std::uint32_t regionId) const
{
    return LoadedRegions.count(regionId) != 0;
}

bool DynamicCollisionWorld::VolumeContainsPoint(const DynamicVolume& vol, const Math::Vector3& worldPoint) const
{
    // Optimizacao: AABB world-space (BoundsCenter +/- BoundsExtent) descarta cedo.
    const Math::Vector3& C = vol.BoundsCenter;
    const Math::Vector3& E = vol.BoundsExtent;
    if (E.LengthSquared() > 0.0)
    {
        if (std::abs(worldPoint.X - C.X) > E.X) return false;
        if (std::abs(worldPoint.Y - C.Y) > E.Y) return false;
        if (std::abs(worldPoint.Z - C.Z) > E.Z) return false;
    }

    if (vol.Shapes.empty())
    {
        // Sem shapes detalhados: bound box ja confirma.
        return true;
    }

    for (const CollisionShape& Shape : vol.Shapes)
    {
        const Math::Transform& VolT = vol.WorldTransform;
        const Math::Transform& SL = Shape.LocalTransform;
        const Math::Vector3 shapeWorldLoc = VolT.TransformPosition(SL.Location);
        const Math::Quaternion shapeWorldRot = (VolT.Rotation * SL.Rotation).Normalized();
        const Math::Transform ShapeT(shapeWorldLoc, shapeWorldRot, Math::Vector3::One);

        const Math::Vector3 localPoint = WorldToLocal(ShapeT, worldPoint);

        switch (Shape.Type)
        {
        case ECollisionShapeType::Box:
            if (ContainsLocalAabb(localPoint, Shape.Box.HalfExtents)) return true;
            break;
        case ECollisionShapeType::Sphere:
            if (ContainsSphere(localPoint, Shape.Sphere.Radius)) return true;
            break;
        case ECollisionShapeType::Capsule:
            if (ContainsCapsule(localPoint, Shape.Capsule.Radius, Shape.Capsule.HalfHeight)) return true;
            break;
        case ECollisionShapeType::Convex:
        {
            // Aproximacao com bounds AABB local (suficiente para volumes de gameplay).
            // 4.5b: half-space check completo.
            Math::Vector3 minB( 1e9,  1e9,  1e9);
            Math::Vector3 maxB(-1e9, -1e9, -1e9);
            for (const Math::Vector3& v : Shape.Convex.Vertices)
            {
                minB.X = std::min(minB.X, v.X); minB.Y = std::min(minB.Y, v.Y); minB.Z = std::min(minB.Z, v.Z);
                maxB.X = std::max(maxB.X, v.X); maxB.Y = std::max(maxB.Y, v.Y); maxB.Z = std::max(maxB.Z, v.Z);
            }
            if (localPoint.X >= minB.X && localPoint.X <= maxB.X &&
                localPoint.Y >= minB.Y && localPoint.Y <= maxB.Y &&
                localPoint.Z >= minB.Z && localPoint.Z <= maxB.Z)
            {
                return true;
            }
            break;
        }
        default:
            break;
        }
    }
    return false;
}

std::vector<const DynamicVolume*> DynamicCollisionWorld::QueryAt(const Math::Vector3& point) const
{
    std::vector<const DynamicVolume*> result;
    if (!bAssetLoaded)
    {
        return result;
    }
    for (std::size_t idx : ActiveVolumeIndices)
    {
        if (idx >= Asset.Volumes.size())
        {
            continue;
        }
        const DynamicVolume& Vol = Asset.Volumes[idx];
        if (VolumeContainsPoint(Vol, point))
        {
            result.push_back(&Vol);
        }
    }
    return result;
}

const DynamicVolume* DynamicCollisionWorld::FindByEventTag(const std::string& tag) const
{
    if (!bAssetLoaded || tag.empty())
    {
        return nullptr;
    }
    for (const DynamicVolume& Vol : Asset.Volumes)
    {
        if (Vol.EventTag == tag)
        {
            return &Vol;
        }
    }
    return nullptr;
}

std::size_t DynamicCollisionWorld::GetActiveVolumeCount() const
{
    return ActiveVolumeIndices.size();
}

void DynamicCollisionWorld::Shutdown()
{
    Asset = {};
    bAssetLoaded = false;
    LoadedRegions.clear();
    ActiveVolumeIndices.clear();
    Stats = {};
}
} // namespace Zeus::Collision
