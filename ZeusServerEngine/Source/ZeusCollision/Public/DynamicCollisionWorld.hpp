#pragma once

#include "DynamicCollisionAsset.hpp"
#include "Vector3.hpp"

#include <cstdint>
#include <filesystem>
#include <unordered_set>
#include <vector>

namespace Zeus::Collision
{
struct DynamicWorldStats
{
    std::size_t LoadedRegions = 0;
    std::size_t ActiveVolumes = 0;
    std::size_t LastQueryHits = 0;
};

/**
 * Runtime de volumes dinamicos. Nao cria bodies Jolt — volumes sao apenas dados
 * logicos consultados via `QueryAt`. Streaming acompanha o do `CollisionWorld`
 * via `LoadRegion`/`UnloadRegion`.
 */
class DynamicCollisionWorld
{
public:
    DynamicCollisionWorld();
    ~DynamicCollisionWorld();

    DynamicCollisionWorld(const DynamicCollisionWorld&) = delete;
    DynamicCollisionWorld& operator=(const DynamicCollisionWorld&) = delete;

    bool LoadFromZsm(const std::filesystem::path& path);
    bool LoadFromAsset(DynamicCollisionAsset asset);

    bool LoadRegion(std::uint32_t regionId);
    bool UnloadRegion(std::uint32_t regionId);
    bool IsRegionLoaded(std::uint32_t regionId) const;

    /** Devolve todos os volumes activos cuja AABB contem o ponto (em cm). */
    std::vector<const DynamicVolume*> QueryAt(const Math::Vector3& point) const;

    /** Procura o primeiro volume com a `EventTag` indicada. */
    const DynamicVolume* FindByEventTag(const std::string& tag) const;

    bool IsLoaded() const { return bAssetLoaded; }
    const DynamicCollisionAsset& GetAsset() const { return Asset; }
    std::size_t GetActiveVolumeCount() const;
    const DynamicWorldStats& GetStats() const { return Stats; }

    void Shutdown();

private:
    bool VolumeContainsPoint(const DynamicVolume& vol, const Math::Vector3& worldPoint) const;

    DynamicCollisionAsset Asset;
    bool bAssetLoaded = false;

    std::unordered_set<std::uint32_t> LoadedRegions;
    /** indices em `Asset.Volumes` que estao activos (regioes carregadas). */
    std::unordered_set<std::size_t> ActiveVolumeIndices;

    DynamicWorldStats Stats;
};
} // namespace Zeus::Collision
