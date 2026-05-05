#pragma once

#include "CollisionEntity.hpp"
#include "Transform.hpp"
#include "Vector3.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace Zeus::Collision
{
/**
 * Tipo de piece. Estavel: novos kinds incrementam numero sem alterar versao do
 * ZSMT.
 */
enum class ETerrainPieceKind : std::uint8_t
{
    Unknown = 0,
    TriangleMesh = 1,
    HeightField = 2,
};

/**
 * Triangle mesh em local space (cm). Indices em triplos (3*N).
 * Vertices em double porque mapas grandes em cm extrapolam float.
 */
struct TerrainTriangleMesh
{
    std::vector<Math::Vector3> Vertices;
    std::vector<std::uint32_t> Indices;
};

/**
 * HeightField regular grid. As alturas finais (em cm, local space) sao
 * `Heights[y*SamplesX + x] * HeightScaleCm`; `OriginLocal` e o canto inferior
 * (X=0, Y=0).
 */
struct TerrainHeightField
{
    std::uint32_t SamplesX = 0;
    std::uint32_t SamplesY = 0;
    double SampleSpacingCm = 100.0;
    Math::Vector3 OriginLocal = Math::Vector3::Zero;
    double HeightScaleCm = 1.0;
    std::vector<float> Heights; // tamanho = SamplesX * SamplesY
};

/** Uma piece individual: TriangleMesh ou HeightField. */
struct TerrainPiece
{
    std::string Name;
    std::string ActorName;
    std::string ComponentName;
    ETerrainPieceKind Kind = ETerrainPieceKind::Unknown;

    Math::Transform WorldTransform = Math::Transform::Identity;
    Math::Vector3 BoundsCenter = Math::Vector3::Zero;
    Math::Vector3 BoundsExtent = Math::Vector3::Zero;

    EntityRegion Region;

    TerrainTriangleMesh TriangleMesh;
    TerrainHeightField HeightField;
};

struct TerrainAssetStats
{
    std::size_t PieceCount = 0;
    std::size_t TriangleMeshCount = 0;
    std::size_t HeightFieldCount = 0;
    std::size_t TotalVertexCount = 0;
    std::size_t TotalTriangleCount = 0;
    std::size_t TotalHeightSampleCount = 0;
    std::size_t WarningCount = 0;
};

/**
 * Asset agregado de terreno carregado de `terrain_collision.zsm`. Engine-
 * agnostic. `PieceIndexByRegion` e populado pelo loader e e a base do streaming.
 */
struct TerrainCollisionAsset
{
    std::uint16_t Version = 1;
    std::uint32_t Flags = 0;
    double RegionSizeCm = 5000.0;
    std::string MapName;

    std::vector<TerrainPiece> Pieces;
    TerrainAssetStats Stats;
    std::vector<std::string> Warnings;

    std::unordered_map<std::uint32_t, std::vector<std::size_t>> PieceIndexByRegion;
};

/** Reconstroi `PieceIndexByRegion` a partir de `Pieces`. Idempotente. */
void RebuildTerrainPieceIndexByRegion(TerrainCollisionAsset& asset);
} // namespace Zeus::Collision
