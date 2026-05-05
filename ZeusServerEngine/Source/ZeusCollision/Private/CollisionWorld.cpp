#include "CollisionWorld.hpp"

#include "CollisionDebug.hpp"
#include "Quaternion.hpp"
#include "Transform.hpp"
#include "Vector3.hpp"
#include "ZsmCollisionLoader.hpp"

#include <sstream>
#include <utility>

namespace Zeus::Collision
{
namespace
{
Math::Transform ComposeShapeWorld(const Math::Transform& entity, const Math::Transform& shapeLocal)
{
    return Math::Transform::Combine(entity, shapeLocal);
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
    CollisionDebug::LogAssetSummary(Asset, "<in-memory>");
    return true;
}

bool CollisionWorld::BuildPhysicsWorld()
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

    if (!bAssetLoaded)
    {
        CollisionDebug::LogWarn("Collision", "[Collision] BuildPhysicsWorld called without loaded asset");
        return false;
    }

    CollisionDebug::LogInfo("Collision", "[Collision] Building Jolt static bodies...");

    std::size_t Created = 0;
    std::size_t Skipped = 0;
    for (const CollisionEntity& Entity : Asset.Entities)
    {
        for (const CollisionShape& Shape : Entity.Shapes)
        {
            const Math::Transform World = ComposeShapeWorld(Entity.WorldTransform, Shape.LocalTransform);
            const std::uint32_t BodyId = Physics->AddStaticBody(Shape, World);
            if (BodyId != 0)
            {
                ++Created;
            }
            else
            {
                ++Skipped;
            }
        }
    }
    Physics->OptimizeBroadPhase();

    PhysicsBuilt = (Created > 0);
    std::ostringstream oss;
    oss << "[Collision] Static bodies created=" << Created << " skipped=" << Skipped;
    CollisionDebug::LogInfo("Collision", oss.str());
    return PhysicsBuilt;
}

void CollisionWorld::Shutdown()
{
    if (Physics)
    {
        Physics->Shutdown();
        Physics.reset();
    }
    PhysicsBuilt = false;
    bAssetLoaded = false;
    Asset = {};
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
} // namespace Zeus::Collision
