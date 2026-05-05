#pragma once

#include "CollisionAsset.hpp"

#include <string>
#include <string_view>

namespace Zeus::Collision
{
/**
 * Helpers de logging dedicados ao módulo de colisão. Apenas envolvem
 * `Zeus::ZeusLog` com a categoria fixa `"Collision"`/`"CollisionTest"` para
 * manter os logs uniformes (ver `Docs/COLLISION.md`).
 */
namespace CollisionDebug
{
    void LogInfo(std::string_view category, std::string_view message);
    void LogWarn(std::string_view category, std::string_view message);
    void LogError(std::string_view category, std::string_view message);

    /** Resumo formatado das estatísticas do asset, escrito em nível Info. */
    void LogAssetSummary(const CollisionAsset& asset, std::string_view sourcePath);

    /** Helper para serializar `std::format`-like sem depender de `<format>`. */
    std::string Concat(std::string_view a, std::string_view b);
    std::string Concat(std::string_view a, std::string_view b, std::string_view c);
    std::string Concat(std::string_view a, std::string_view b, std::string_view c, std::string_view d);
}
} // namespace Zeus::Collision
