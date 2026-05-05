#pragma once

#include "CollisionEntity.hpp"
#include "CollisionShape.hpp"
#include "Transform.hpp"
#include "Vector3.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace Zeus::Collision
{
/** Categoria do volume dinamico. */
enum class EDynamicVolumeKind : std::uint8_t
{
    Unknown = 0,
    Trigger = 1,
    Water = 2,
    Lava = 3,
    KillVolume = 4,
    SafeZone = 5,
};

/** Volume dinamico engine-agnostic. Os shapes sao locais ao volume. */
struct DynamicVolume
{
    std::string Name;
    std::string ActorName;
    EDynamicVolumeKind Kind = EDynamicVolumeKind::Unknown;

    /** Tag livre (ex.: "spawn_zone_a") para `FindByEventTag`. */
    std::string EventTag;

    Math::Transform WorldTransform = Math::Transform::Identity;
    Math::Vector3 BoundsCenter = Math::Vector3::Zero;
    Math::Vector3 BoundsExtent = Math::Vector3::Zero;

    EntityRegion Region;

    std::vector<CollisionShape> Shapes;
};

struct DynamicAssetStats
{
    std::size_t VolumeCount = 0;
    std::size_t ShapeCount = 0;
    std::size_t TriggerCount = 0;
    std::size_t WaterCount = 0;
    std::size_t LavaCount = 0;
    std::size_t KillVolumeCount = 0;
    std::size_t SafeZoneCount = 0;
    std::size_t UnknownKindCount = 0;
    std::size_t WarningCount = 0;
};

struct DynamicCollisionAsset
{
    std::uint16_t Version = 1;
    std::uint32_t Flags = 0;
    double RegionSizeCm = 5000.0;
    std::string MapName;

    std::vector<DynamicVolume> Volumes;
    DynamicAssetStats Stats;
    std::vector<std::string> Warnings;

    std::unordered_map<std::uint32_t, std::vector<std::size_t>> VolumeIndexByRegion;
};

void RebuildDynamicVolumeIndexByRegion(DynamicCollisionAsset& asset);
} // namespace Zeus::Collision
