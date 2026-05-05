#include "CollisionWorld.hpp"

#include "CollisionDebug.hpp"
#include "CollisionShapeType.hpp"
#include "Quaternion.hpp"
#include "TerrainCollisionAsset.hpp"
#include "Transform.hpp"
#include "Vector3.hpp"
#include "ZsmCollisionLoader.hpp"
#include "ZsmTerrainLoader.hpp"

#include <algorithm>
#include <sstream>
#include <utility>

namespace Zeus::Collision
{
namespace
{
/** World point expressed in body frame (rotation only; scale must be baked in pw). */
Math::Vector3 WorldPointToBodyLocal(const Math::Vector3& bodyLocation, const Math::Quaternion& bodyRotation,
    const Math::Vector3& worldPoint)
{
    const Math::Quaternion q = bodyRotation.Normalized();
    const Math::Transform invRot(Math::Vector3::Zero,
        Math::Quaternion(-q.X, -q.Y, -q.Z, q.W),
        Math::Vector3::One);
    return invRot.TransformVector(worldPoint - bodyLocation);
}
} // namespace

CollisionWorld::CollisionWorld()
    : Physics(std::make_unique<PhysicsWorld>())
{
}

CollisionWorld::~CollisionWorld()
{
    Shutdown();
}

bool CollisionWorld::LoadFromZsm(const std::filesystem::path& path)
{
    Asset = {};
    bAssetLoaded = false;

    std::ostringstream oss;
    oss << "[Collision] Loading " << path.string();
    CollisionDebug::LogInfo("Collision", oss.str());

    ZsmCollisionLoader Loader;
    ZsmLoadResult Result = Loader.LoadFromFile(path, Asset);

    if (!Result.bSuccess)
    {
        std::ostringstream err;
        err << "[Collision] Failed to load " << path.string() << ": " << Result.Error;
        CollisionDebug::LogWarn("Collision", err.str());
        return false;
    }

    for (const std::string& Warning : Result.Warnings)
    {
        CollisionDebug::LogWarn("Collision", CollisionDebug::Concat("[Collision] ", Warning));
    }

    bAssetLoaded = true;
    CollisionDebug::LogAssetSummary(Asset, path.string());
    return true;
}

bool CollisionWorld::LoadFromAsset(CollisionAsset asset)
{
    Asset = std::move(asset);
    bAssetLoaded = true;
    RebuildEntityIndexByRegion(Asset);
    CollisionDebug::LogAssetSummary(Asset, "<in-memory>");
    return true;
}

bool CollisionWorld::LoadFromTerrainZsm(const std::filesystem::path& path)
{
    Terrain = std::make_unique<TerrainCollisionAsset>();
    bTerrainLoaded = false;

    std::ostringstream oss;
    oss << "[Collision] Loading terrain " << path.string();
    CollisionDebug::LogInfo("Collision", oss.str());

    ZsmTerrainLoader Loader;
    ZsmLoadResult Result = Loader.LoadFromFile(path, *Terrain);
    if (!Result.bSuccess)
    {
        std::ostringstream err;
        err << "[Collision] Failed to load terrain " << path.string() << ": " << Result.Error;
        CollisionDebug::LogWarn("Collision", err.str());
        Terrain.reset();
        return false;
    }
    for (const std::string& Warning : Result.Warnings)
    {
        CollisionDebug::LogWarn("Collision", CollisionDebug::Concat("[Collision] ", Warning));
    }
    bTerrainLoaded = true;
    std::ostringstream summary;
    summary << "[Collision] Terrain loaded pieces=" << Terrain->Pieces.size()
            << " (TriangleMesh=" << Terrain->Stats.TriangleMeshCount
            << " HeightField=" << Terrain->Stats.HeightFieldCount << ")";
    CollisionDebug::LogInfo("Collision", summary.str());
    return true;
}

bool CollisionWorld::LoadFromTerrainAsset(TerrainCollisionAsset asset)
{
    Terrain = std::make_unique<TerrainCollisionAsset>(std::move(asset));
    bTerrainLoaded = true;
    return true;
}

bool CollisionWorld::InitPhysicsLazy()
{
    if (!Physics)
    {
        Physics = std::make_unique<PhysicsWorld>();
    }
    if (!Physics->IsInitialized() && !Physics->Initialize())
    {
        CollisionDebug::LogError("Collision", "[Collision] PhysicsWorld initialization failed");
        return false;
    }
    PhysicsBuilt = true; // Jolt esta vivo, mesmo sem bodies criados ainda.
    return true;
}

bool CollisionWorld::BuildPhysicsWorld()
{
    if (!InitPhysicsLazy())
    {
        return false;
    }

    if (!bAssetLoaded && !bTerrainLoaded)
    {
        CollisionDebug::LogWarn("Collision",
            "[Collision] BuildPhysicsWorld called without loaded asset");
        return false;
    }

    CollisionDebug::LogInfo("Collision", "[Collision] Building Jolt static bodies (all regions)...");

    std::unordered_set<std::uint32_t> regionIds;
    if (bAssetLoaded)
    {
        for (const auto& [regionId, _] : Asset.EntityIndexByRegion)
        {
            regionIds.insert(regionId);
        }
    }
    if (bTerrainLoaded && Terrain)
    {
        for (const auto& [regionId, _] : Terrain->PieceIndexByRegion)
        {
            regionIds.insert(regionId);
        }
    }

    std::size_t totalCreated = 0;
    for (std::uint32_t regionId : regionIds)
    {
        const std::size_t before = Physics->GetBodyCount();
        LoadRegion(regionId);
        totalCreated += Physics->GetBodyCount() - before;
    }
    Physics->OptimizeBroadPhase();

    PhysicsBuilt = (totalCreated > 0) || (regionIds.size() == 0);
    std::ostringstream oss;
    oss << "[Collision] Static bodies created=" << totalCreated
        << " regionsLoaded=" << LoadedRegions.size();
    CollisionDebug::LogInfo("Collision", oss.str());
    return PhysicsBuilt;
}

void CollisionWorld::BuildBodiesForTerrainPiece(std::size_t pieceIndex, std::vector<std::uint32_t>& outBodyIds)
{
    if (!Terrain || pieceIndex >= Terrain->Pieces.size() || !Physics || !Physics->IsInitialized())
    {
        return;
    }
    const TerrainPiece& Piece = Terrain->Pieces[pieceIndex];
    const std::uint32_t bodyId = Physics->AddStaticBodyForTerrain(Piece);
    if (bodyId != 0)
    {
        outBodyIds.push_back(bodyId);
    }
}

void CollisionWorld::BuildBodiesForEntity(const CollisionEntity& Entity, std::vector<std::uint32_t>& outBodyIds)
{
    for (const CollisionShape& Shape : Entity.Shapes)
    {
        const Math::Transform& E = Entity.WorldTransform;
        const Math::Transform& SL = Shape.LocalTransform;
        const Math::Vector3 bodyLocation = E.TransformPosition(SL.Location);
        const Math::Quaternion bodyRotation = (E.Rotation * SL.Rotation).Normalized();
        const Math::Transform bodyWorld(bodyLocation, bodyRotation, Math::Vector3::One);

        CollisionShape shapeForBody = Shape;
        if (Shape.Type == ECollisionShapeType::Convex)
        {
            for (Math::Vector3& v : shapeForBody.Convex.Vertices)
            {
                const Math::Vector3 pw = E.TransformPosition(SL.TransformPosition(v));
                v = WorldPointToBodyLocal(bodyLocation, bodyRotation, pw);
            }
        }

        const std::uint32_t BodyId = Physics->AddStaticBody(shapeForBody, bodyWorld);
        if (BodyId != 0)
        {
            outBodyIds.push_back(BodyId);
        }
    }
}

bool CollisionWorld::LoadRegion(std::uint32_t regionId)
{
    if (!Physics || !Physics->IsInitialized())
    {
        return false;
    }
    if (LoadedRegions.count(regionId) != 0)
    {
        return true; // idempotente
    }

    std::vector<std::uint32_t> bodyIds;

    if (bAssetLoaded)
    {
        const auto it = Asset.EntityIndexByRegion.find(regionId);
        if (it != Asset.EntityIndexByRegion.end())
        {
            for (std::size_t entityIdx : it->second)
            {
                if (entityIdx < Asset.Entities.size())
                {
                    BuildBodiesForEntity(Asset.Entities[entityIdx], bodyIds);
                }
            }
        }
    }

    if (bTerrainLoaded && Terrain)
    {
        const auto itT = Terrain->PieceIndexByRegion.find(regionId);
        if (itT != Terrain->PieceIndexByRegion.end())
        {
            for (std::size_t pieceIdx : itT->second)
            {
                BuildBodiesForTerrainPiece(pieceIdx, bodyIds);
            }
        }
    }

    BodiesByRegion[regionId] = std::move(bodyIds);
    LoadedRegions.insert(regionId);
    Physics->OptimizeBroadPhaseIfNeeded();

    std::ostringstream oss;
    oss << "[Collision] Region " << regionId << " loaded bodies="
        << BodiesByRegion[regionId].size();
    CollisionDebug::LogInfo("Collision", oss.str());
    return true;
}

bool CollisionWorld::UnloadRegion(std::uint32_t regionId)
{
    if (!Physics || !Physics->IsInitialized())
    {
        return false;
    }
    if (LoadedRegions.count(regionId) == 0)
    {
        return false;
    }

    auto it = BodiesByRegion.find(regionId);
    std::size_t removed = 0;
    if (it != BodiesByRegion.end())
    {
        for (std::uint32_t bodyId : it->second)
        {
            if (Physics->RemoveStaticBody(bodyId))
            {
                ++removed;
            }
        }
        BodiesByRegion.erase(it);
    }
    LoadedRegions.erase(regionId);
    Physics->OptimizeBroadPhaseIfNeeded();

    std::ostringstream oss;
    oss << "[Collision] Region " << regionId << " unloaded bodies=" << removed;
    CollisionDebug::LogInfo("Collision", oss.str());
    return true;
}

bool CollisionWorld::IsRegionLoaded(std::uint32_t regionId) const
{
    return LoadedRegions.count(regionId) != 0;
}

std::size_t CollisionWorld::GetBodiesInRegion(std::uint32_t regionId) const
{
    const auto it = BodiesByRegion.find(regionId);
    return it == BodiesByRegion.end() ? 0 : it->second.size();
}

void CollisionWorld::Shutdown()
{
    if (Physics)
    {
        Physics->Shutdown();
        Physics.reset();
    }
    BodiesByRegion.clear();
    LoadedRegions.clear();
    PhysicsBuilt = false;
    bAssetLoaded = false;
    bTerrainLoaded = false;
    Asset = {};
    Terrain.reset();
}

std::size_t CollisionWorld::GetEntityCount() const
{
    return Asset.Entities.size();
}

std::size_t CollisionWorld::GetShapeCount() const
{
    return Asset.Stats.ShapeCount;
}

std::size_t CollisionWorld::GetStaticBodyCount() const
{
    return Physics ? Physics->GetBodyCount() : 0;
}

bool CollisionWorld::Raycast(const Math::Vector3& originCm, const Math::Vector3& directionUnnormalized,
    double maxDistanceCm, RaycastHit& outHit) const
{
    if (!Physics)
    {
        outHit = RaycastHit{};
        return false;
    }
    return Physics->Raycast(originCm, directionUnnormalized, maxDistanceCm, outHit);
}

bool CollisionWorld::RaycastDown(const Math::Vector3& originCm, double maxDistanceCm, RaycastHit& outHit) const
{
    if (!Physics)
    {
        outHit = RaycastHit{};
        return false;
    }
    return Physics->RaycastDown(originCm, maxDistanceCm, outHit);
}

bool CollisionWorld::CollideCapsule(const Math::Vector3& centerCm, double radiusCm, double halfHeightCm,
    ContactInfo& outFirstContact) const
{
    if (!Physics)
    {
        outFirstContact = ContactInfo{};
        return false;
    }
    return Physics->CollideCapsule(centerCm, radiusCm, halfHeightCm, outFirstContact);
}

bool CollisionWorld::SweepCapsule(const Math::Vector3& centerStartCm, double radiusCm, double halfHeightCm,
    const Math::Vector3& sweepCm, SweepHit& outHit) const
{
    if (!Physics)
    {
        outHit = SweepHit{};
        return false;
    }
    return Physics->SweepCapsule(centerStartCm, radiusCm, halfHeightCm, sweepCm, outHit);
}
} // namespace Zeus::Collision
