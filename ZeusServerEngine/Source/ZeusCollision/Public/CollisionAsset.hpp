#pragma once

#include "CollisionEntity.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace Zeus::Collision
{
/** Estatísticas agregadas sobre o asset (somadas durante a carga). */
struct CollisionAssetStats
{
    std::size_t EntityCount = 0;
    std::size_t ShapeCount = 0;
    std::size_t BoxCount = 0;
    std::size_t SphereCount = 0;
    std::size_t CapsuleCount = 0;
    std::size_t ConvexCount = 0;
    std::size_t WarningCount = 0;
};

/**
 * Representação em memória de um `static_collision.zsm` (versão 1).
 *
 * Este tipo não conhece Jolt nem qualquer engine. É consumido pelo
 * `CollisionWorld` para criar bodies estáticos no `PhysicsWorld`.
 */
struct CollisionAsset
{
    std::string MapName;
    std::uint16_t Version = 1;
    std::string Units = "centimeters";
    double RegionSizeCm = 5000.0;
    std::uint32_t Flags = 0;

    std::vector<CollisionEntity> Entities;
    CollisionAssetStats Stats;
    std::vector<std::string> Warnings;

    /**
     * Index auxiliar populado pelo loader/`LoadFromAsset`: mapeia `RegionId`
     * para os indices de `Entities` que pertencem a essa regiao. Custo `O(N)` no
     * arranque, mas torna `LoadRegion`/`UnloadRegion` em runtime `O(K)` onde K
     * sao apenas as entidades da regiao a (des)carregar.
     */
    std::unordered_map<std::uint32_t, std::vector<std::size_t>> EntityIndexByRegion;
};

/** Reconstroi `EntityIndexByRegion` a partir de `Entities` (idempotente). */
void RebuildEntityIndexByRegion(CollisionAsset& asset);
} // namespace Zeus::Collision
