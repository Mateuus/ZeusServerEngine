#pragma once

#include "TerrainCollisionAsset.hpp"
#include "ZsmCollisionLoader.hpp" // re-uses ZsmLoadResult

#include <cstdint>
#include <filesystem>

namespace Zeus::Collision
{
/**
 * Loader do formato `ZSMT v1` (`terrain_collision.zsm`). Espelha o
 * `ZsmCollisionLoader` mas escreve para um `TerrainCollisionAsset` que so contem
 * pecas de terreno (TriangleMesh / HeightField). Nao toca em Jolt.
 */
class ZsmTerrainLoader
{
public:
    ZsmLoadResult LoadFromFile(const std::filesystem::path& path, TerrainCollisionAsset& outAsset);
    ZsmLoadResult LoadFromBytes(const std::uint8_t* data, std::size_t size, TerrainCollisionAsset& outAsset);
};
} // namespace Zeus::Collision
