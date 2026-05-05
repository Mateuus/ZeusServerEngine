#include "ZeusRegionSystem.hpp"

#include "Actor.hpp"
#include "CollisionWorld.hpp"
#include "DynamicCollisionWorld.hpp"
#include "World.hpp"
#include "ZeusLog.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <unordered_set>

namespace Zeus::World
{
namespace
{
struct CellCoord
{
    std::int32_t X;
    std::int32_t Y;
    std::int32_t Z;
    bool operator==(const CellCoord& o) const { return X == o.X && Y == o.Y && Z == o.Z; }
};

struct CellHash
{
    std::size_t operator()(const CellCoord& c) const noexcept
    {
        const std::uint32_t Hx = static_cast<std::uint32_t>(c.X) * 73856093u;
        const std::uint32_t Hy = static_cast<std::uint32_t>(c.Y) * 19349663u;
        const std::uint32_t Hz = static_cast<std::uint32_t>(c.Z) * 83492791u;
        return static_cast<std::size_t>(Hx ^ Hy ^ Hz);
    }
};

CellCoord WorldToCell(const Zeus::Math::Vector3& worldCm, double cellSizeCm)
{
    const double sz = cellSizeCm > 0.0 ? cellSizeCm : 5000.0;
    return CellCoord{
        static_cast<std::int32_t>(std::floor(worldCm.X / sz)),
        static_cast<std::int32_t>(std::floor(worldCm.Y / sz)),
        0
    };
}

double CellCenterDistanceXYCm(const CellCoord& c, const Zeus::Math::Vector3& worldCm, double cellSizeCm)
{
    const double sz = cellSizeCm > 0.0 ? cellSizeCm : 5000.0;
    const double cx = (static_cast<double>(c.X) + 0.5) * sz;
    const double cy = (static_cast<double>(c.Y) + 0.5) * sz;
    const double dx = cx - worldCm.X;
    const double dy = cy - worldCm.Y;
    return std::sqrt(dx * dx + dy * dy);
}
} // namespace

std::uint32_t ZeusRegionSystem::MakeRegionId(std::int32_t gx, std::int32_t gy, std::int32_t gz)
{
    const std::uint32_t Hx = static_cast<std::uint32_t>(gx) * 73856093u;
    const std::uint32_t Hy = static_cast<std::uint32_t>(gy) * 19349663u;
    const std::uint32_t Hz = static_cast<std::uint32_t>(gz) * 83492791u;
    return (Hx ^ Hy ^ Hz);
}

void ZeusRegionSystem::Configure(const RegionStreamingSettings& settings)
{
    Settings = settings;
    if (Settings.RegionSizeCm <= 0.0)
    {
        Settings.RegionSizeCm = 5000.0;
    }
    if (Settings.MaxLoadsPerTick <= 0)
    {
        Settings.MaxLoadsPerTick = 1;
    }
    if (Settings.MaxUnloadsPerTick <= 0)
    {
        Settings.MaxUnloadsPerTick = 1;
    }
}

void ZeusRegionSystem::Tick(World& world,
    Zeus::Collision::CollisionWorld& cw,
    Zeus::Collision::DynamicCollisionWorld* dw,
    double /*deltaSeconds*/)
{
    std::vector<Zeus::Math::Vector3> playerLocations;
    world.ForEachActor([this, &playerLocations](const Actor& a) {
        if (a.HasTag(Settings.PlayerProxyTag))
        {
            playerLocations.push_back(a.GetLocation());
        }
    });

    Stats.PlayerCount = playerLocations.size();
    if (!playerLocations.empty())
    {
        PrimaryPlayerLocation = playerLocations.front();
        const CellCoord cc = WorldToCell(PrimaryPlayerLocation, Settings.RegionSizeCm);
        PrimaryPlayerRegionId = MakeRegionId(cc.X, cc.Y, cc.Z);
    }

    // 1) Cells desejadas (uniao dos halos por player).
    const double radius = Settings.StreamingRadiusCm;
    const int r = static_cast<int>(std::ceil(radius / Settings.RegionSizeCm));
    std::unordered_set<CellCoord, CellHash> desired;

    for (const Zeus::Math::Vector3& p : playerLocations)
    {
        const CellCoord center = WorldToCell(p, Settings.RegionSizeCm);
        for (int dy = -r; dy <= r; ++dy)
        {
            for (int dx = -r; dx <= r; ++dx)
            {
                CellCoord c{center.X + dx, center.Y + dy, 0};
                if (CellCenterDistanceXYCm(c, p, Settings.RegionSizeCm) <= radius)
                {
                    desired.insert(c);
                }
            }
        }
    }

    // 2) LoadRegion para cells novas (limite por tick).
    int loadsThisTick = 0;
    for (const CellCoord& c : desired)
    {
        if (loadsThisTick >= Settings.MaxLoadsPerTick) break;
        const std::uint32_t regionId = MakeRegionId(c.X, c.Y, c.Z);
        if (!cw.IsRegionLoaded(regionId))
        {
            cw.LoadRegion(regionId);
            if (dw != nullptr)
            {
                dw->LoadRegion(regionId);
            }
            ++loadsThisTick;
            ++Stats.TotalLoads;
            Stats.LastLoadedRegionId = static_cast<std::int64_t>(regionId);
        }
    }

    // 3) UnloadRegion para regions fora do radius + hysteresis.
    const double unloadRadius = Settings.StreamingRadiusCm + Settings.UnloadHysteresisCm;
    std::vector<std::uint32_t> toUnload;
    for (std::uint32_t regionId : cw.GetLoadedRegions())
    {
        bool keep = false;
        for (const Zeus::Math::Vector3& p : playerLocations)
        {
            // Estimativa: usa distancia entre centro da region (em xy) e player.
            // Nao temos acesso directo a (gx,gy) a partir de regionId; em vez
            // disso recriamos comparando contra cada cell desejada com hysteresis.
            const CellCoord center = WorldToCell(p, Settings.RegionSizeCm);
            const int rh = r + 1; // 1 cell extra de hysteresis no espaco de cells
            for (int dy = -rh; dy <= rh && !keep; ++dy)
            {
                for (int dx = -rh; dx <= rh && !keep; ++dx)
                {
                    CellCoord c{center.X + dx, center.Y + dy, 0};
                    if (CellCenterDistanceXYCm(c, p, Settings.RegionSizeCm) <= unloadRadius)
                    {
                        if (MakeRegionId(c.X, c.Y, c.Z) == regionId)
                        {
                            keep = true;
                        }
                    }
                }
            }
            if (keep) break;
        }
        if (!keep)
        {
            toUnload.push_back(regionId);
        }
    }

    int unloadsThisTick = 0;
    for (std::uint32_t regionId : toUnload)
    {
        if (unloadsThisTick >= Settings.MaxUnloadsPerTick) break;
        cw.UnloadRegion(regionId);
        if (dw != nullptr)
        {
            dw->UnloadRegion(regionId);
        }
        ++unloadsThisTick;
        ++Stats.TotalUnloads;
        Stats.LastUnloadedRegionId = static_cast<std::int64_t>(regionId);
    }

    // 4) Stats para o painel.
    Stats.RegionsActive = cw.GetLoadedRegionCount();
    Stats.BodiesActive = cw.GetStaticBodyCount();
}
} // namespace Zeus::World
