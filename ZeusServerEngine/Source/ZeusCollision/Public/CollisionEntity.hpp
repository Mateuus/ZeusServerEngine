#pragma once

#include "CollisionShape.hpp"
#include "Transform.hpp"
#include "Vector3.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace Zeus::Collision
{
/**
 * Metadados de região/cell já preparados para o futuro `ZeusRegionSystem`/World
 * Partition; nesta fase são apenas calculados a partir do centro do bounds.
 */
struct EntityRegion
{
    std::uint32_t RegionId = 0;
    std::int32_t GridX = 0;
    std::int32_t GridY = 0;
    std::int32_t GridZ = 0;
    double RegionSizeCm = 5000.0;
};

/**
 * Uma entidade de colisão estática carregada do .zsm. Engine-agnostic.
 * `WorldTransform` em cm; `BoundsCenter`/`BoundsExtent` são axis-aligned bounds
 * em cm.
 */
struct CollisionEntity
{
    std::string Name;          // tipicamente o nome da UStaticMesh (ex.: SM_Wall_01)
    std::string ActorName;     // nome do actor que originou a entidade
    std::string ComponentName; // nome do componente (ex.: StaticMeshComponent0)

    Math::Transform WorldTransform = Math::Transform::Identity;
    Math::Vector3 BoundsCenter = Math::Vector3::Zero;
    Math::Vector3 BoundsExtent = Math::Vector3::Zero;

    EntityRegion Region;

    std::vector<CollisionShape> Shapes;
};
} // namespace Zeus::Collision
