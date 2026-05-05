#include "PhysicsWorld.hpp"

#if defined(ZEUS_HAS_JOLT) && ZEUS_HAS_JOLT

#include "CollisionDebug.hpp"
#include "JoltConversion.hpp"
#include "JoltShapeFactory.hpp"

JPH_SUPPRESS_WARNINGS
#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollidePointResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/ShapeFilter.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <sstream>
#include <thread>

namespace Zeus::Collision
{
namespace
{
namespace Layers
{
    static constexpr JPH::ObjectLayer NonMoving = 0;
    static constexpr JPH::ObjectLayer Moving = 1;
    static constexpr JPH::ObjectLayer NumLayers = 2;
}

namespace BroadPhaseLayers
{
    static constexpr JPH::BroadPhaseLayer NonMoving(0);
    static constexpr JPH::BroadPhaseLayer Moving(1);
    static constexpr JPH::uint NumLayers = 2;
}

class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
{
public:
    BPLayerInterfaceImpl()
    {
        mObjectToBroadPhase[Layers::NonMoving] = BroadPhaseLayers::NonMoving;
        mObjectToBroadPhase[Layers::Moving]    = BroadPhaseLayers::Moving;
    }

    JPH::uint GetNumBroadPhaseLayers() const override
    {
        return BroadPhaseLayers::NumLayers;
    }

    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override
    {
        return mObjectToBroadPhase[inLayer];
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override
    {
        switch (static_cast<JPH::BroadPhaseLayer::Type>(inLayer))
        {
        case static_cast<JPH::BroadPhaseLayer::Type>(BroadPhaseLayers::NonMoving): return "NonMoving";
        case static_cast<JPH::BroadPhaseLayer::Type>(BroadPhaseLayers::Moving):    return "Moving";
        default: return "Unknown";
        }
    }
#endif

private:
    JPH::BroadPhaseLayer mObjectToBroadPhase[Layers::NumLayers];
};

class ObjectVsBroadPhaseLayerFilterImpl final : public JPH::ObjectVsBroadPhaseLayerFilter
{
public:
    bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override
    {
        switch (inLayer1)
        {
        case Layers::NonMoving:
            return inLayer2 == BroadPhaseLayers::Moving;
        case Layers::Moving:
            return true;
        default:
            return false;
        }
    }
};

class ObjectLayerPairFilterImpl final : public JPH::ObjectLayerPairFilter
{
public:
    bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::ObjectLayer inLayer2) const override
    {
        switch (inLayer1)
        {
        case Layers::NonMoving:
            return inLayer2 == Layers::Moving;
        case Layers::Moving:
            return true;
        default:
            return false;
        }
    }
};

void TraceImpl(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char buffer[1024];
    std::vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    CollisionDebug::LogInfo("Collision",
        CollisionDebug::Concat("[Jolt] ", buffer));
}

#ifdef JPH_ENABLE_ASSERTS
bool AssertFailedImpl(const char* expr, const char* msg, const char* file, JPH::uint line)
{
    std::ostringstream oss;
    oss << "[Jolt] Assert failed: " << expr;
    if (msg)
    {
        oss << " (" << msg << ")";
    }
    oss << " at " << file << ":" << line;
    CollisionDebug::LogError("Collision", oss.str());
    return true; // trigger debug breakpoint
}
#endif

std::atomic<int> g_FactoryRefCount{0};
} // namespace

struct PhysicsWorld::PimplState
{
    bool bInitialized = false;
    bool bOwnsFactory = false;

    std::unique_ptr<JPH::TempAllocatorImpl> TempAllocator;
    std::unique_ptr<JPH::JobSystemThreadPool> JobSystem;
    BPLayerInterfaceImpl BroadPhaseLayerInterface;
    ObjectVsBroadPhaseLayerFilterImpl ObjectVsBroadPhaseLayerFilter;
    ObjectLayerPairFilterImpl ObjectLayerPairFilter;
    std::unique_ptr<JPH::PhysicsSystem> PhysicsSystem;

    std::vector<JPH::BodyID> Bodies;
    std::vector<JPH::ShapeRefC> ShapeRefs;
};

PhysicsWorld::PhysicsWorld() = default;

PhysicsWorld::~PhysicsWorld()
{
    Shutdown();
}

bool PhysicsWorld::Initialize()
{
    if (State && State->bInitialized)
    {
        return true;
    }

    State = std::make_unique<PimplState>();

    JPH::RegisterDefaultAllocator();
    JPH::Trace = TraceImpl;
#ifdef JPH_ENABLE_ASSERTS
    JPH::AssertFailed = AssertFailedImpl;
#endif

    if (JPH::Factory::sInstance == nullptr)
    {
        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();
        State->bOwnsFactory = true;
        g_FactoryRefCount.fetch_add(1, std::memory_order_relaxed);
    }
    else
    {
        State->bOwnsFactory = false;
        g_FactoryRefCount.fetch_add(1, std::memory_order_relaxed);
    }

    constexpr std::size_t TempAllocatorSize = 10 * 1024 * 1024; // 10 MB
    State->TempAllocator = std::make_unique<JPH::TempAllocatorImpl>(TempAllocatorSize);

    const int NumThreads = std::max(1, static_cast<int>(std::thread::hardware_concurrency()) - 1);
    State->JobSystem = std::make_unique<JPH::JobSystemThreadPool>(
        JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, NumThreads);

    constexpr JPH::uint MaxBodies = 4096;
    constexpr JPH::uint NumBodyMutexes = 0; // default
    constexpr JPH::uint MaxBodyPairs = 1024;
    constexpr JPH::uint MaxContactConstraints = 1024;

    State->PhysicsSystem = std::make_unique<JPH::PhysicsSystem>();
    State->PhysicsSystem->Init(MaxBodies, NumBodyMutexes, MaxBodyPairs, MaxContactConstraints,
        State->BroadPhaseLayerInterface,
        State->ObjectVsBroadPhaseLayerFilter,
        State->ObjectLayerPairFilter);

    // Z is up; gravity in m/s² (Jolt unit). 9.81 m/s² == 981 cm/s².
    State->PhysicsSystem->SetGravity(JPH::Vec3(0.0f, 0.0f, -9.81f));

    State->bInitialized = true;
    CollisionDebug::LogInfo("Collision", "[PhysicsWorld] Initialized Jolt physics system");
    return true;
}

void PhysicsWorld::Shutdown()
{
    if (!State)
    {
        return;
    }

    if (State->PhysicsSystem)
    {
        for (const JPH::BodyID& Id : State->Bodies)
        {
            State->PhysicsSystem->GetBodyInterface().RemoveBody(Id);
            State->PhysicsSystem->GetBodyInterface().DestroyBody(Id);
        }
        State->Bodies.clear();
        State->ShapeRefs.clear();
        State->PhysicsSystem.reset();
    }

    State->JobSystem.reset();
    State->TempAllocator.reset();

    if (State->bOwnsFactory)
    {
        if (g_FactoryRefCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
        {
            JPH::UnregisterTypes();
            delete JPH::Factory::sInstance;
            JPH::Factory::sInstance = nullptr;
        }
    }
    else
    {
        g_FactoryRefCount.fetch_sub(1, std::memory_order_acq_rel);
    }

    State->bInitialized = false;
    State.reset();
    CollisionDebug::LogInfo("Collision", "[PhysicsWorld] Shutdown");
}

bool PhysicsWorld::IsInitialized() const
{
    return State && State->bInitialized;
}

std::uint32_t PhysicsWorld::AddStaticBody(const CollisionShape& shape, const Math::Transform& worldTransform)
{
    if (!IsInitialized())
    {
        return 0;
    }

    JPH::ShapeRefC ShapeRef = JoltShapeFactory::CreateShape(shape);
    if (ShapeRef == nullptr)
    {
        return 0;
    }

    // Final shape transform = entity world * shape local; the caller composed
    // it already (CollisionWorld::BuildPhysicsWorld). worldTransform is in cm.
    const JPH::RVec3 Position = ToJoltPosition(worldTransform.Location);
    const JPH::Quat Rotation = ToJoltQuat(worldTransform.Rotation).Normalized();

    JPH::BodyCreationSettings Settings(ShapeRef, Position, Rotation,
        JPH::EMotionType::Static, Layers::NonMoving);
    Settings.mFriction = 0.5f;
    Settings.mRestitution = 0.0f;

    JPH::BodyInterface& BodyInterface = State->PhysicsSystem->GetBodyInterface();
    const JPH::BodyID Id = BodyInterface.CreateAndAddBody(Settings, JPH::EActivation::DontActivate);
    if (Id.IsInvalid())
    {
        return 0;
    }

    State->ShapeRefs.push_back(ShapeRef);
    State->Bodies.push_back(Id);
    return Id.GetIndexAndSequenceNumber();
}

void PhysicsWorld::OptimizeBroadPhase()
{
    if (!IsInitialized())
    {
        return;
    }
    State->PhysicsSystem->OptimizeBroadPhase();
}

void PhysicsWorld::Step(double deltaSeconds)
{
    if (!IsInitialized())
    {
        return;
    }
    constexpr int CollisionSteps = 1;
    State->PhysicsSystem->Update(static_cast<float>(deltaSeconds), CollisionSteps,
        State->TempAllocator.get(), State->JobSystem.get());
}

bool PhysicsWorld::Raycast(const Math::Vector3& originCm, const Math::Vector3& directionUnnormalized,
    double maxDistanceCm, RaycastHit& outHit) const
{
    outHit = RaycastHit{};
    if (!IsInitialized())
    {
        return false;
    }

    const Math::Vector3 dirN = directionUnnormalized.Normalized();
    if (dirN.LengthSquared() <= 1e-12)
    {
        return false;
    }

    const JPH::RVec3 origin = ToJoltPosition(originCm);
    const JPH::Vec3 direction(static_cast<float>(dirN.X * maxDistanceCm * CmToMeters),
                              static_cast<float>(dirN.Y * maxDistanceCm * CmToMeters),
                              static_cast<float>(dirN.Z * maxDistanceCm * CmToMeters));

    const JPH::RRayCast Ray(origin, direction);
    JPH::RayCastResult Result;
    if (!State->PhysicsSystem->GetNarrowPhaseQuery().CastRay(Ray, Result))
    {
        return false;
    }

    outHit.bHit = true;
    const JPH::RVec3 ImpactPos = Ray.GetPointOnRay(Result.mFraction);
    outHit.ImpactPoint = FromJoltPosition(ImpactPos);
    outHit.Distance = static_cast<double>(Result.mFraction) * maxDistanceCm;
    outHit.BodyId = Result.mBodyID.GetIndexAndSequenceNumber();

    JPH::BodyLockRead Lock(State->PhysicsSystem->GetBodyLockInterface(), Result.mBodyID);
    if (Lock.Succeeded())
    {
        const JPH::Body& Body = Lock.GetBody();
        const JPH::Vec3 NormalMeters = Body.GetWorldSpaceSurfaceNormal(Result.mSubShapeID2, ImpactPos);
        outHit.ImpactNormal = FromJoltVec3(NormalMeters); // unit vector — scale doesn't matter
        // Re-normalize after the cm scale: a normal vector should remain unit; FromJoltVec3 scales
        // by 100 which is fine since callers only use direction.
        const Math::Vector3 N = outHit.ImpactNormal.Normalized();
        outHit.ImpactNormal = N;
    }
    return true;
}

bool PhysicsWorld::RaycastDown(const Math::Vector3& originCm, double maxDistanceCm, RaycastHit& outHit) const
{
    return Raycast(originCm, Math::Vector3(0.0, 0.0, -1.0), maxDistanceCm, outHit);
}

bool PhysicsWorld::CollideCapsule(const Math::Vector3& centerCm, double radiusCm, double halfHeightCm,
    ContactInfo& outFirstContact) const
{
    outFirstContact = ContactInfo{};
    if (!IsInitialized())
    {
        return false;
    }
    const float radius = static_cast<float>(std::max(0.001, radiusCm * CmToMeters));
    const float halfHeight = static_cast<float>(std::max(0.001, halfHeightCm * CmToMeters));
    JPH::CapsuleShapeSettings CapSettings(halfHeight, radius);
    CapSettings.SetEmbedded();
    JPH::Shape::ShapeResult CapResult = CapSettings.Create();
    if (CapResult.HasError())
    {
        return false;
    }
    JPH::ShapeRefC CapShape = CapResult.Get();

    JPH::RMat44 Center = JPH::RMat44::sTranslation(ToJoltPosition(centerCm));
    JPH::CollideShapeSettings Settings;
    Settings.mMaxSeparationDistance = 0.0f;

    JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> Collector;
    State->PhysicsSystem->GetNarrowPhaseQuery().CollideShape(
        CapShape, JPH::Vec3::sReplicate(1.0f), Center, Settings, JPH::RVec3::sZero(), Collector);

    if (Collector.mHits.empty())
    {
        return false;
    }

    const JPH::CollideShapeResult& First = Collector.mHits.front();
    outFirstContact.ContactPoint = FromJoltPosition(First.mContactPointOn2);
    outFirstContact.ContactNormal = FromJoltVec3(First.mPenetrationAxis.Normalized());
    outFirstContact.ContactNormal = outFirstContact.ContactNormal.Normalized();
    outFirstContact.BodyId = First.mBodyID2.GetIndexAndSequenceNumber();
    return true;
}

std::size_t PhysicsWorld::GetBodyCount() const
{
    return State ? State->Bodies.size() : 0;
}
} // namespace Zeus::Collision

#else // ZEUS_HAS_JOLT == 0

#include "CollisionDebug.hpp"

namespace Zeus::Collision
{
struct PhysicsWorld::PimplState {};

PhysicsWorld::PhysicsWorld() = default;
PhysicsWorld::~PhysicsWorld() = default;

bool PhysicsWorld::Initialize()
{
    CollisionDebug::LogWarn("Collision", "[PhysicsWorld] Built without Jolt; physics disabled.");
    return false;
}

void PhysicsWorld::Shutdown() {}
bool PhysicsWorld::IsInitialized() const { return false; }
std::uint32_t PhysicsWorld::AddStaticBody(const CollisionShape&, const Math::Transform&) { return 0; }
void PhysicsWorld::OptimizeBroadPhase() {}
void PhysicsWorld::Step(double) {}
bool PhysicsWorld::Raycast(const Math::Vector3&, const Math::Vector3&, double, RaycastHit& out) const
{
    out = RaycastHit{};
    return false;
}
bool PhysicsWorld::RaycastDown(const Math::Vector3&, double, RaycastHit& out) const
{
    out = RaycastHit{};
    return false;
}
bool PhysicsWorld::CollideCapsule(const Math::Vector3&, double, double, ContactInfo& out) const
{
    out = ContactInfo{};
    return false;
}
std::size_t PhysicsWorld::GetBodyCount() const { return 0; }
} // namespace Zeus::Collision

#endif // ZEUS_HAS_JOLT
