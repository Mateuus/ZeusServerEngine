#pragma once

#include "DynamicCollisionAsset.hpp"
#include "ZsmCollisionLoader.hpp" // re-uses ZsmLoadResult

#include <cstdint>
#include <filesystem>

namespace Zeus::Collision
{
/**
 * Loader do formato `ZSMD v1` (`dynamic_collision.zsm`). Espelha o
 * `ZsmCollisionLoader`, mas escreve para um `DynamicCollisionAsset` (volumes,
 * sem creation de bodies). Engine-agnostic.
 */
class ZsmDynamicLoader
{
public:
    ZsmLoadResult LoadFromFile(const std::filesystem::path& path, DynamicCollisionAsset& outAsset);
    ZsmLoadResult LoadFromBytes(const std::uint8_t* data, std::size_t size, DynamicCollisionAsset& outAsset);
};
} // namespace Zeus::Collision
