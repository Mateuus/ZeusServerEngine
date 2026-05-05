#include "CollisionAsset.hpp"

namespace Zeus::Collision
{
void RebuildEntityIndexByRegion(CollisionAsset& asset)
{
    asset.EntityIndexByRegion.clear();
    asset.EntityIndexByRegion.reserve(asset.Entities.size());
    for (std::size_t i = 0; i < asset.Entities.size(); ++i)
    {
        const std::uint32_t regionId = asset.Entities[i].Region.RegionId;
        asset.EntityIndexByRegion[regionId].push_back(i);
    }
}
} // namespace Zeus::Collision
