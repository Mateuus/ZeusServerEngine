#include "RegionSystemTests.hpp"

#include "CollisionAsset.hpp"
#include "CollisionWorld.hpp"
#include "DebugPlayerActor.hpp"
#include "SpawnParameters.hpp"
#include "Vector3.hpp"
#include "World.hpp"
#include "ZeusLog.hpp"
#include "ZeusRegionSystem.hpp"

#include <cmath>
#include <sstream>

namespace Zeus::World
{
namespace
{
void Record(RegionTestReport& report, std::string name, bool bPassed, std::string detail = {})
{
    RegionTestResult r;
    r.Name = std::move(name);
    r.bPassed = bPassed;
    r.Detail = std::move(detail);
    if (r.bPassed)
    {
        ++report.PassedCount;
        ZeusLog::Info("RegionTest",
            std::string("[RegionTest] ").append(r.Name).append(": OK ").append(r.Detail));
    }
    else
    {
        ++report.FailedCount;
        ZeusLog::Warning("RegionTest",
            std::string("[RegionTest] ").append(r.Name).append(": FAIL ").append(r.Detail));
    }
    report.Scenarios.push_back(std::move(r));
}

Zeus::Collision::CollisionAsset BuildNineCellAsset(double regionSizeCm)
{
    Zeus::Collision::CollisionAsset asset;
    asset.MapName = "RegionTest";
    asset.RegionSizeCm = regionSizeCm;
    asset.Version = 1;

    auto computeRegion = [regionSizeCm](double cx, double cy) -> Zeus::Collision::EntityRegion {
        Zeus::Collision::EntityRegion R;
        R.RegionSizeCm = regionSizeCm;
        R.GridX = static_cast<std::int32_t>(std::floor(cx / regionSizeCm));
        R.GridY = static_cast<std::int32_t>(std::floor(cy / regionSizeCm));
        R.GridZ = 0;
        const std::uint32_t Hx = static_cast<std::uint32_t>(R.GridX) * 73856093u;
        const std::uint32_t Hy = static_cast<std::uint32_t>(R.GridY) * 19349663u;
        const std::uint32_t Hz = static_cast<std::uint32_t>(R.GridZ) * 83492791u;
        R.RegionId = (Hx ^ Hy ^ Hz);
        return R;
    };

    for (int gy = -1; gy <= 1; ++gy)
    {
        for (int gx = -1; gx <= 1; ++gx)
        {
            Zeus::Collision::CollisionEntity E;
            E.Name = std::string("Box_") + std::to_string(gx) + "_" + std::to_string(gy);
            E.ActorName = E.Name + "_Actor";
            E.ComponentName = "StaticMeshComponent0";
            const double cx = (static_cast<double>(gx) + 0.5) * regionSizeCm;
            const double cy = (static_cast<double>(gy) + 0.5) * regionSizeCm;
            E.WorldTransform = Zeus::Math::Transform(Zeus::Math::Vector3(cx, cy, 50.0),
                Zeus::Math::Quaternion::Identity, Zeus::Math::Vector3::One);
            E.BoundsCenter = Zeus::Math::Vector3(cx, cy, 50.0);
            E.BoundsExtent = Zeus::Math::Vector3(200.0, 200.0, 50.0);
            E.Region = computeRegion(cx, cy);

            Zeus::Collision::CollisionShape s;
            s.Type = Zeus::Collision::ECollisionShapeType::Box;
            s.LocalTransform = Zeus::Math::Transform::Identity;
            s.Box.HalfExtents = Zeus::Math::Vector3(200.0, 200.0, 50.0);
            E.Shapes.push_back(std::move(s));

            asset.Entities.push_back(std::move(E));
        }
    }
    asset.Stats.EntityCount = asset.Entities.size();
    asset.Stats.ShapeCount = asset.Entities.size();
    asset.Stats.BoxCount = asset.Entities.size();
    Zeus::Collision::RebuildEntityIndexByRegion(asset);
    return asset;
}
} // namespace

RegionTestReport RegionSystemTests::RunAll()
{
    RegionTestReport report;

    constexpr double regionSizeCm = 5000.0;

    auto asset = BuildNineCellAsset(regionSizeCm);

    Zeus::Collision::CollisionWorld cw;
    if (!cw.LoadFromAsset(std::move(asset)))
    {
        Record(report, "Setup CollisionAsset", false);
        return report;
    }
    if (!cw.InitPhysicsLazy())
    {
        Record(report, "Setup PhysicsLazy", false);
        return report;
    }

    World world;
    world.Initialize("RegionTestWorld");
    world.BeginPlay();

    SpawnParameters params;
    params.Name = "DebugPlayer";
    params.bAllowTick = true;
    params.bStartActive = true;
    params.Transform.Location = Zeus::Math::Vector3(0.0, 0.0, 100.0);
    DebugPlayerActor* dp = world.SpawnActorTyped<DebugPlayerActor>(params, true);
    if (dp == nullptr)
    {
        Record(report, "Spawn DebugPlayer", false);
        return report;
    }

    ZeusRegionSystem rs;
    RegionStreamingSettings settings;
    settings.RegionSizeCm = regionSizeCm;
    settings.StreamingRadiusCm = 2500.0; // 0.5 cell — so a propria cell deve carregar
    settings.UnloadHysteresisCm = 1500.0;
    settings.MaxLoadsPerTick = 10;
    settings.MaxUnloadsPerTick = 10;
    rs.Configure(settings);

    // Tick com player em (0,0): cell origem deve carregar.
    rs.Tick(world, cw, nullptr, 1.0 / 30.0);
    {
        const std::size_t loaded = cw.GetLoadedRegionCount();
        const bool bPass = loaded >= 1 && loaded <= 4;
        std::ostringstream det;
        det << "(loaded=" << loaded << ")";
        Record(report, "TC-RS-01 Initial load near origin", bPass, det.str());
    }

    // Move o player para (20000, 0): cells da esquerda devem unloaded.
    dp->SetWaypoints({Zeus::Math::Vector3(20000.0, 0.0, 100.0)}, 0.0);
    rs.Tick(world, cw, nullptr, 1.0 / 30.0);
    {
        const auto& loadedSet = cw.GetLoadedRegions();
        bool stillHasOriginCell = false;
        for (std::uint32_t rid : loadedSet)
        {
            // Cell origem (gx=0, gy=0) tem regionId = (0*73856093) ^ (0*19349663) ^ (0*83492791) = 0
            if (rid == 0u)
            {
                stillHasOriginCell = true;
                break;
            }
        }
        // Apos varios ticks o stream deve descarregar a cell da origem (limite de unloads/tick e 10).
        for (int i = 0; i < 5; ++i)
        {
            rs.Tick(world, cw, nullptr, 1.0 / 30.0);
        }
        bool finalHasOrigin = false;
        for (std::uint32_t rid : cw.GetLoadedRegions())
        {
            if (rid == 0u)
            {
                finalHasOrigin = true;
                break;
            }
        }
        std::ostringstream det;
        det << "(initial=" << stillHasOriginCell << " final=" << finalHasOrigin << ")";
        Record(report, "TC-RS-02 Origin cell unloaded after move", !finalHasOrigin, det.str());
    }

    cw.Shutdown();
    world.Shutdown();

    std::ostringstream summary;
    summary << "[RegionTest] Summary passed=" << report.PassedCount
            << " failed=" << report.FailedCount;
    if (report.FailedCount == 0)
    {
        ZeusLog::Info("RegionTest", summary.str());
    }
    else
    {
        ZeusLog::Warning("RegionTest", summary.str());
    }
    return report;
}
} // namespace Zeus::World
