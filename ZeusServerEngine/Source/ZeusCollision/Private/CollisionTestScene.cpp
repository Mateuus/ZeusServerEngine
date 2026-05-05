#include "CollisionTestScene.hpp"

#include "CollisionDebug.hpp"
#include "CollisionWorld.hpp"
#include "DynamicCollisionWorld.hpp"
#include "MathConstants.hpp"
#include "MathUtils.hpp"
#include "Quaternion.hpp"
#include "Rotator.hpp"
#include "Transform.hpp"
#include "Vector3.hpp"

#include <cmath>
#include <sstream>

namespace Zeus::Collision
{
namespace
{
CollisionShape MakeBox(double halfX, double halfY, double halfZ)
{
    CollisionShape Shape;
    Shape.Type = ECollisionShapeType::Box;
    Shape.Box.HalfExtents = Math::Vector3(halfX, halfY, halfZ);
    Shape.LocalTransform = Math::Transform::Identity;
    return Shape;
}

Math::Quaternion QuatFromYRoll(double pitchDegrees)
{
    const double half = pitchDegrees * 0.5 * Math::DegToRad;
    const double s = std::sin(half);
    const double c = std::cos(half);
    return Math::Quaternion(0.0, s, 0.0, c);
}

CollisionEntity MakeEntity(const std::string& name, const Math::Vector3& location,
    const Math::Quaternion& rotation, CollisionShape shape)
{
    CollisionEntity Entity;
    Entity.Name = name;
    Entity.ActorName = name + "_Actor";
    Entity.ComponentName = "StaticMeshComponent0";
    Entity.WorldTransform = Math::Transform(location, rotation, Math::Vector3::One);
    Entity.BoundsCenter = location;
    Entity.BoundsExtent = Math::Vector3(500.0, 500.0, 500.0);
    Entity.Region.RegionSizeCm = 5000.0;
    Entity.Shapes.push_back(std::move(shape));
    return Entity;
}

void RecordResult(TestSceneReport& Report, std::string name, bool bPassed, std::string detail = {})
{
    TestScenarioResult R;
    R.Name = std::move(name);
    R.bPassed = bPassed;
    R.Detail = std::move(detail);
    if (R.bPassed)
    {
        ++Report.PassedCount;
        CollisionDebug::LogInfo("CollisionTest",
            CollisionDebug::Concat("[CollisionTest] ", R.Name, ": OK ", R.Detail));
    }
    else
    {
        ++Report.FailedCount;
        CollisionDebug::LogWarn("CollisionTest",
            CollisionDebug::Concat("[CollisionTest] ", R.Name, ": FAIL ", R.Detail));
    }
    Report.Scenarios.push_back(std::move(R));
}
} // namespace

CollisionAsset CollisionTestScene::BuildProgrammaticAsset()
{
    CollisionAsset Asset;
    Asset.MapName = "TestWorld_Programmatic";
    Asset.Units = "centimeters";
    Asset.RegionSizeCm = 5000.0;
    Asset.Version = 1;
    Asset.Flags = 0;

    // Floor: 2000 x 2000 x 10 cm centred at origin (top of floor at z = +5).
    Asset.Entities.push_back(MakeEntity("Floor",
        Math::Vector3(0.0, 0.0, 0.0), Math::Quaternion::Identity,
        MakeBox(1000.0, 1000.0, 5.0)));

    // Wall: 50 x 500 x 300 cm at x = +500.
    Asset.Entities.push_back(MakeEntity("Wall",
        Math::Vector3(500.0, 0.0, 150.0), Math::Quaternion::Identity,
        MakeBox(25.0, 250.0, 150.0)));

    // Walkable ramp: 600 x 400 x 10 cm rotated 30° around Y at (-500, 200, 100).
    Asset.Entities.push_back(MakeEntity("RampValid",
        Math::Vector3(-500.0, 200.0, 100.0), QuatFromYRoll(30.0),
        MakeBox(300.0, 200.0, 5.0)));

    // Steep ramp: 400 x 400 x 10 cm rotated 70° around Y at (-500, -200, 200).
    Asset.Entities.push_back(MakeEntity("RampSteep",
        Math::Vector3(-500.0, -200.0, 200.0), QuatFromYRoll(70.0),
        MakeBox(200.0, 200.0, 5.0)));

    Asset.Stats.EntityCount = Asset.Entities.size();
    Asset.Stats.ShapeCount = Asset.Entities.size();
    Asset.Stats.BoxCount = Asset.Entities.size();

    // Hash do RegionId compatível com o exporter Unreal e o ZeusRegionSystem.
    auto computeRegion = [](const Math::Vector3& center, double regionSize) -> EntityRegion {
        EntityRegion R;
        R.RegionSizeCm = regionSize;
        R.GridX = static_cast<std::int32_t>(std::floor(center.X / regionSize));
        R.GridY = static_cast<std::int32_t>(std::floor(center.Y / regionSize));
        R.GridZ = static_cast<std::int32_t>(std::floor(center.Z / regionSize));
        const std::uint32_t Hx = static_cast<std::uint32_t>(R.GridX) * 73856093u;
        const std::uint32_t Hy = static_cast<std::uint32_t>(R.GridY) * 19349663u;
        const std::uint32_t Hz = static_cast<std::uint32_t>(R.GridZ) * 83492791u;
        R.RegionId = (Hx ^ Hy ^ Hz);
        return R;
    };
    for (CollisionEntity& E : Asset.Entities)
    {
        E.Region = computeRegion(E.BoundsCenter, Asset.RegionSizeCm);
    }
    RebuildEntityIndexByRegion(Asset);
    return Asset;
}

DynamicCollisionAsset CollisionTestScene::BuildProgrammaticDynamicAsset()
{
    DynamicCollisionAsset Asset;
    Asset.MapName = "TestWorld_Dynamic";
    Asset.RegionSizeCm = 5000.0;
    Asset.Version = 1;

    DynamicVolume V;
    V.Name = "TriggerA";
    V.ActorName = "TriggerA_Actor";
    V.Kind = EDynamicVolumeKind::Trigger;
    V.EventTag = "trigger_a";
    V.WorldTransform = Math::Transform(Math::Vector3(0.0, 0.0, 100.0),
        Math::Quaternion::Identity, Math::Vector3::One);
    V.BoundsCenter = Math::Vector3(0.0, 0.0, 100.0);
    V.BoundsExtent = Math::Vector3(200.0, 200.0, 150.0);

    CollisionShape Box;
    Box.Type = ECollisionShapeType::Box;
    Box.LocalTransform = Math::Transform::Identity;
    Box.Box.HalfExtents = Math::Vector3(200.0, 200.0, 150.0);
    V.Shapes.push_back(std::move(Box));

    V.Region.RegionSizeCm = Asset.RegionSizeCm;
    V.Region.GridX = 0;
    V.Region.GridY = 0;
    V.Region.GridZ = 0;
    V.Region.RegionId = 0;

    Asset.Volumes.push_back(std::move(V));
    Asset.Stats.VolumeCount = Asset.Volumes.size();
    Asset.Stats.ShapeCount = 1;
    Asset.Stats.TriggerCount = 1;
    RebuildDynamicVolumeIndexByRegion(Asset);
    return Asset;
}

TerrainCollisionAsset CollisionTestScene::BuildProgrammaticTerrainAsset()
{
    TerrainCollisionAsset Asset;
    Asset.MapName = "TestWorld_Terrain";
    Asset.RegionSizeCm = 5000.0;
    Asset.Version = 1;

    TerrainPiece Piece;
    Piece.Name = "FlatHeightField";
    Piece.ActorName = "Landscape0";
    Piece.ComponentName = "LandscapeComponent0";
    Piece.Kind = ETerrainPieceKind::HeightField;
    Piece.WorldTransform = Math::Transform(Math::Vector3(-3000.0, -3000.0, -50.0),
        Math::Quaternion::Identity, Math::Vector3::One);
    Piece.BoundsCenter = Math::Vector3(0.0, 0.0, -50.0);
    Piece.BoundsExtent = Math::Vector3(3000.0, 3000.0, 50.0);
    Piece.Region.RegionSizeCm = Asset.RegionSizeCm;

    constexpr std::uint32_t SX = 16;
    constexpr std::uint32_t SY = 16;
    Piece.HeightField.SamplesX = SX;
    Piece.HeightField.SamplesY = SY;
    Piece.HeightField.SampleSpacingCm = 400.0; // 16 samples * 400 cm = 6400 cm wide
    Piece.HeightField.OriginLocal = Math::Vector3::Zero;
    Piece.HeightField.HeightScaleCm = 1.0;
    Piece.HeightField.Heights.assign(SX * SY, 0.0f);

    Asset.Pieces.push_back(std::move(Piece));
    Asset.Stats.PieceCount = 1;
    Asset.Stats.HeightFieldCount = 1;
    Asset.Stats.TotalHeightSampleCount = SX * SY;
    RebuildTerrainPieceIndexByRegion(Asset);
    return Asset;
}

TestSceneReport CollisionTestScene::RunAll(CollisionWorld& world)
{
    TestSceneReport Report;

    if (!world.IsPhysicsBuilt())
    {
        RecordResult(Report, "Pre-check: physics built", false,
            "(world.PhysicsBuilt = false)");
        return Report;
    }

    PhysicsWorld& Physics = world.GetPhysicsWorld();

    // --- TC-01 Capsule vs floor (RaycastDown finds the floor). ----------------
    {
        const Math::Vector3 Origin(0.0, 0.0, 200.0);
        RaycastHit Hit;
        const bool bHit = Physics.RaycastDown(Origin, 500.0, Hit);
        const bool bPass = bHit && Hit.Distance > 100.0 && Hit.Distance < 250.0
            && Math::IsWalkableFloor(Hit.ImpactNormal, Math::DefaultMaxSlopeAngleDeg);
        std::ostringstream det;
        det << "(distance=" << Hit.Distance << ")";
        RecordResult(Report, "TC-01 Capsule vs floor (RaycastDown)", bPass, det.str());
    }

    // --- TC-02 Capsule overlaps wall. ----------------------------------------
    {
        const Math::Vector3 CapCenter(495.0, 0.0, 100.0);
        ContactInfo Contact;
        const bool bOverlap = Physics.CollideCapsule(CapCenter, 35.0, 90.0, Contact);
        RecordResult(Report, "TC-02 Capsule vs wall (CollideCapsule)", bOverlap);
    }

    // --- TC-03 Walkable ramp ~30°. -------------------------------------------
    {
        const Math::Vector3 Origin(-500.0, 200.0, 400.0);
        RaycastHit Hit;
        const bool bHit = Physics.RaycastDown(Origin, 800.0, Hit);
        const bool bWalkable = bHit && Math::IsWalkableFloor(Hit.ImpactNormal, Math::DefaultMaxSlopeAngleDeg);
        std::ostringstream det;
        det << "(hit=" << bHit << " normal=" << Hit.ImpactNormal.ToString() << ")";
        RecordResult(Report, "TC-03 Walkable ramp (~30 deg)", bHit && bWalkable, det.str());
    }

    // --- TC-04 Steep ramp ~70°. ----------------------------------------------
    {
        const Math::Vector3 Origin(-500.0, -200.0, 600.0);
        RaycastHit Hit;
        const bool bHit = Physics.RaycastDown(Origin, 800.0, Hit);
        const bool bWalkable = bHit && Math::IsWalkableFloor(Hit.ImpactNormal, Math::DefaultMaxSlopeAngleDeg);
        std::ostringstream det;
        det << "(hit=" << bHit << " walkable=" << bWalkable << ")";
        RecordResult(Report, "TC-04 Too steep ramp (~70 deg) blocks",
            bHit && !bWalkable, det.str());
    }

    // --- TC-05 Lateral raycast hits the wall. --------------------------------
    {
        const Math::Vector3 Origin(0.0, 0.0, 100.0);
        const Math::Vector3 Dir(1.0, 0.0, 0.0);
        RaycastHit Hit;
        const bool bHit = Physics.Raycast(Origin, Dir, 1000.0, Hit);
        const bool bPass = bHit && Hit.Distance > 400.0 && Hit.Distance < 600.0;
        std::ostringstream det;
        det << "(distance=" << Hit.Distance << ")";
        RecordResult(Report, "TC-05 Lateral raycast vs wall", bPass, det.str());
    }

    std::ostringstream summary;
    summary << "[CollisionTest] Summary passed=" << Report.PassedCount
            << " failed=" << Report.FailedCount;
    if (Report.FailedCount == 0)
    {
        CollisionDebug::LogInfo("CollisionTest", summary.str());
    }
    else
    {
        CollisionDebug::LogWarn("CollisionTest", summary.str());
    }
    return Report;
}

TestSceneReport CollisionTestScene::RunStreamingScenarios(CollisionWorld& world,
    DynamicCollisionWorld* dynamicWorld)
{
    TestSceneReport Report;

    // --- TC-06 LoadRegion / UnloadRegion (counters) --------------------------
    {
        const auto& asset = world.GetAsset();
        if (asset.EntityIndexByRegion.empty())
        {
            RecordResult(Report, "TC-06 LoadRegion / UnloadRegion", false,
                "(no regions in asset)");
        }
        else
        {
            const std::uint32_t regionId = asset.EntityIndexByRegion.begin()->first;
            const std::size_t expectedShapesInRegion = [&]() {
                std::size_t total = 0;
                for (std::size_t entityIdx : asset.EntityIndexByRegion.at(regionId))
                {
                    total += asset.Entities[entityIdx].Shapes.size();
                }
                return total;
            }();

            // Garante que esta carregada e tem bodies; o CoreServerApp em modo
            // build-all ja chamou BuildPhysicsWorld. Forcamos load de novo (idempotente).
            world.LoadRegion(regionId);
            const std::size_t bodiesAfterLoad = world.GetBodiesInRegion(regionId);
            world.UnloadRegion(regionId);
            const std::size_t bodiesAfterUnload = world.GetBodiesInRegion(regionId);

            const bool bPass = bodiesAfterLoad == expectedShapesInRegion && bodiesAfterUnload == 0;
            std::ostringstream det;
            det << "(load=" << bodiesAfterLoad << " expected=" << expectedShapesInRegion
                << " unload=" << bodiesAfterUnload << ")";
            RecordResult(Report, "TC-06 LoadRegion / UnloadRegion", bPass, det.str());

            // Restaura para nao deixar a regiao descarregada nos testes seguintes.
            world.LoadRegion(regionId);
        }
    }

    // --- TC-07 Load+Unload+Load consistency for RaycastDown ------------------
    {
        const auto& asset = world.GetAsset();
        std::uint32_t regionContainingFloor = 0;
        for (const CollisionEntity& E : asset.Entities)
        {
            if (E.Name == "Floor")
            {
                regionContainingFloor = E.Region.RegionId;
                break;
            }
        }
        const Math::Vector3 origin(0.0, 0.0, 200.0);
        RaycastHit hit1, hit2;
        const bool h1 = world.GetPhysicsWorld().RaycastDown(origin, 500.0, hit1);
        world.UnloadRegion(regionContainingFloor);
        world.LoadRegion(regionContainingFloor);
        const bool h2 = world.GetPhysicsWorld().RaycastDown(origin, 500.0, hit2);
        const bool bPass = h1 && h2 &&
            std::abs(hit1.Distance - hit2.Distance) < 1.0;
        std::ostringstream det;
        det << "(d1=" << hit1.Distance << " d2=" << hit2.Distance << ")";
        RecordResult(Report, "TC-07 Load+Unload+Load RaycastDown consistency", bPass, det.str());
    }

    // --- TC-08 Dynamic QueryAt returns Trigger -------------------------------
    if (dynamicWorld != nullptr && dynamicWorld->IsLoaded())
    {
        // Activa todas as regioes do asset dinamico (smoke).
        for (const auto& [regionId, _] : dynamicWorld->GetAsset().VolumeIndexByRegion)
        {
            dynamicWorld->LoadRegion(regionId);
        }
        const auto results = dynamicWorld->QueryAt(Math::Vector3(0.0, 0.0, 100.0));
        bool foundTrigger = false;
        for (const DynamicVolume* v : results)
        {
            if (v != nullptr && v->Kind == EDynamicVolumeKind::Trigger)
            {
                foundTrigger = true;
                break;
            }
        }
        std::ostringstream det;
        det << "(volumes=" << results.size() << ")";
        RecordResult(Report, "TC-08 Dynamic QueryAt finds Trigger", foundTrigger, det.str());
    }
    else
    {
        RecordResult(Report, "TC-08 Dynamic QueryAt finds Trigger", false,
            "(no dynamic asset)");
    }

    // --- TC-09 Terrain RaycastDown over HeightField returns up normal --------
    if (world.HasTerrain())
    {
        // Asset.Pieces[0] e o nosso HeightField plano centrado em (-3000, -3000, -50).
        // Origem ~ (0, 0, 1000) deve cair no centro do HeightField.
        const Math::Vector3 origin(0.0, 0.0, 1000.0);
        RaycastHit hit;
        const bool bHit = world.GetPhysicsWorld().RaycastDown(origin, 5000.0, hit);
        const bool bUp = bHit && Math::IsWalkableFloor(hit.ImpactNormal,
            Math::DefaultMaxSlopeAngleDeg);
        std::ostringstream det;
        det << "(hit=" << bHit << " normal=" << hit.ImpactNormal.ToString() << ")";
        RecordResult(Report, "TC-09 Terrain RaycastDown returns up normal",
            bHit && bUp, det.str());
    }
    else
    {
        RecordResult(Report, "TC-09 Terrain RaycastDown returns up normal", false,
            "(no terrain asset)");
    }

    std::ostringstream summary;
    summary << "[CollisionTest] Streaming summary passed=" << Report.PassedCount
            << " failed=" << Report.FailedCount;
    if (Report.FailedCount == 0)
    {
        CollisionDebug::LogInfo("CollisionTest", summary.str());
    }
    else
    {
        CollisionDebug::LogWarn("CollisionTest", summary.str());
    }
    return Report;
}
} // namespace Zeus::Collision
