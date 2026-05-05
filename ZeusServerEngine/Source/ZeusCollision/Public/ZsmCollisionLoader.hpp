#pragma once

#include "CollisionAsset.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace Zeus::Collision
{
/**
 * Resultado de uma chamada `LoadFromFile`/`LoadFromBytes`.
 * `bSuccess` indica se o ficheiro pôde ser parseado por completo. Mesmo num
 * sucesso podem existir `Warnings` (ex.: shapes desconhecidos saltados).
 */
struct ZsmLoadResult
{
    bool bSuccess = false;
    std::string Error;
    std::vector<std::string> Warnings;
};

/**
 * Loader do formato `ZSMC v1`. Não cria physics bodies; apenas popula um
 * `CollisionAsset` engine-agnostic.
 */
class ZsmCollisionLoader
{
public:
    ZsmLoadResult LoadFromFile(const std::filesystem::path& path, CollisionAsset& outAsset);
    ZsmLoadResult LoadFromBytes(const std::uint8_t* data, std::size_t size, CollisionAsset& outAsset);
};
} // namespace Zeus::Collision
