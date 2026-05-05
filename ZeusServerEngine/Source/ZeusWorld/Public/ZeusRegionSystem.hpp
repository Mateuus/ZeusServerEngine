#pragma once

#include "Vector3.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

namespace Zeus::Collision
{
class CollisionWorld;
class DynamicCollisionWorld;
}

namespace Zeus::World
{
class World;

struct RegionStreamingSettings
{
    /** Distancia em cm para o desejo de carregar regioes (XY only). */
    double StreamingRadiusCm = 7500.0;
    /** Margem extra antes de descarregar — evita load/unload em fronteira. */
    double UnloadHysteresisCm = 1500.0;
    int MaxLoadsPerTick = 4;
    int MaxUnloadsPerTick = 4;
    /** Tamanho da cell em cm; replicado do asset, mas pode ser sobreposto. */
    double RegionSizeCm = 5000.0;
    std::string PlayerProxyTag = "PlayerProxy";
};

struct RegionStreamingStats
{
    std::size_t RegionsActive = 0;
    std::size_t BodiesActive = 0;
    std::size_t PlayerCount = 0;
    std::int64_t LastLoadedRegionId = -1;
    std::int64_t LastUnloadedRegionId = -1;
    std::size_t TotalLoads = 0;
    std::size_t TotalUnloads = 0;
};

/**
 * Streaming server-side flat XY: scaneia actors com tag PlayerProxy a cada
 * tick e mantem activo apenas o conjunto de cells perto deles.
 */
class ZeusRegionSystem
{
public:
    void Configure(const RegionStreamingSettings& settings);

    void Tick(World& world,
        Zeus::Collision::CollisionWorld& cw,
        Zeus::Collision::DynamicCollisionWorld* dw,
        double deltaSeconds);

    const RegionStreamingStats& GetStats() const { return Stats; }
    const RegionStreamingSettings& GetSettings() const { return Settings; }

    /** Posicao do primeiro PlayerProxy detectado no ultimo tick (debug). */
    const Zeus::Math::Vector3& GetPrimaryPlayerLocation() const { return PrimaryPlayerLocation; }
    std::uint32_t GetPrimaryPlayerRegionId() const { return PrimaryPlayerRegionId; }

private:
    static std::uint32_t MakeRegionId(std::int32_t gx, std::int32_t gy, std::int32_t gz);

    RegionStreamingSettings Settings;
    RegionStreamingStats Stats;

    Zeus::Math::Vector3 PrimaryPlayerLocation = Zeus::Math::Vector3::Zero;
    std::uint32_t PrimaryPlayerRegionId = 0;
};
} // namespace Zeus::World
