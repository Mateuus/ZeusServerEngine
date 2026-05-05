#include "WorldRuntime.hpp"

#include "PlatformTime.hpp"
#include "ZeusLog.hpp"

namespace Zeus::World
{
WorldRuntime::WorldRuntime() = default;

WorldRuntime::~WorldRuntime()
{
    Shutdown();
}

bool WorldRuntime::Initialize()
{
    if (bInitialized)
    {
        return true;
    }
    bInitialized = true;
    StatsLogAccumulatorSeconds = 0.0;
    ZeusLog::Info("WorldRuntime", "[WorldRuntime] Initialized");
    return true;
}

void WorldRuntime::Shutdown()
{
    if (!bInitialized)
    {
        return;
    }
    for (auto& m : MapInstances)
    {
        if (m != nullptr)
        {
            m->Shutdown();
        }
    }
    MapInstances.clear();
    bInitialized = false;
    ZeusLog::Info("WorldRuntime", "[WorldRuntime] Shutdown");
}

MapInstance* WorldRuntime::CreateMapInstance(const std::string& mapName)
{
    auto inst = std::make_unique<MapInstance>(NextMapInstanceId++);
    MapInstance* raw = inst.get();
    if (!raw->Initialize(mapName))
    {
        return nullptr;
    }
    MapInstances.push_back(std::move(inst));
    return raw;
}

MapInstance* WorldRuntime::GetMainMapInstance()
{
    if (MapInstances.empty())
    {
        return nullptr;
    }
    return MapInstances.front().get();
}

void WorldRuntime::RefreshStatsFromMaps()
{
    LastStats.MapInstanceCount = MapInstances.size();
    LastStats.ActorCount = 0;
    LastStats.PendingDestroyCount = 0;
    for (const auto& m : MapInstances)
    {
        if (m == nullptr)
        {
            continue;
        }
        const World& w = m->GetWorld();
        LastStats.ActorCount += w.GetEntityManager().GetActorCount();
        LastStats.PendingDestroyCount += w.GetEntityManager().GetPendingDestroyCount();
    }
}

void WorldRuntime::Tick(const double deltaSeconds)
{
    if (!bInitialized)
    {
        return;
    }

    const double t0 = Zeus::Platform::NowMonotonicSeconds();
    for (auto& m : MapInstances)
    {
        if (m != nullptr)
        {
            m->Tick(deltaSeconds);
        }
    }
    const double t1 = Zeus::Platform::NowMonotonicSeconds();
    LastStats.WorldTickMs = (t1 - t0) * 1000.0;
    RefreshStatsFromMaps();

    StatsLogAccumulatorSeconds += deltaSeconds;
    if (StatsLogAccumulatorSeconds >= 5.0)
    {
        StatsLogAccumulatorSeconds = 0.0;
        const std::string line = std::string("[WorldRuntime] stats actors=") + std::to_string(LastStats.ActorCount) +
                                  " maps=" + std::to_string(LastStats.MapInstanceCount) +
                                  " pendingDestroy=" + std::to_string(LastStats.PendingDestroyCount) +
                                  " worldTickMs=" + std::to_string(LastStats.WorldTickMs);
        ZeusLog::Info("WorldRuntime", std::string_view(line));
    }
}
} // namespace Zeus::World
