#pragma once

#include "MapInstance.hpp"
#include "WorldStats.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace Zeus::World
{
class WorldRuntime
{
public:
    WorldRuntime();
    ~WorldRuntime();

    bool Initialize();
    void Shutdown();

    MapInstance* CreateMapInstance(const std::string& mapName);
    MapInstance* GetMainMapInstance();

    void Tick(double deltaSeconds);

    std::size_t GetMapInstanceCount() const { return MapInstances.size(); }
    const WorldStats& GetLastStats() const { return LastStats; }

private:
    void RefreshStatsFromMaps();

    std::vector<std::unique_ptr<MapInstance>> MapInstances;
    std::uint64_t NextMapInstanceId = 1;
    bool bInitialized = false;
    double StatsLogAccumulatorSeconds = 0.0;
    WorldStats LastStats{};
};
} // namespace Zeus::World
