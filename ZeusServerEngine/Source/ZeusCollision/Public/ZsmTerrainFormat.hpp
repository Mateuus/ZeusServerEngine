#pragma once

#include <cstdint>

namespace Zeus::Collision::ZsmTerrain
{
/**
 * Constantes oficiais do formato `terrain_collision.zsm` versão 1 (`ZSMT`).
 * Tudo serializado em little-endian, campo a campo. As coordenadas de
 * vertices estao em centimetros, em local space da peca; a conversao para
 * metros e a aplicacao do `WorldTransform` ficam a cargo do JoltShapeFactory.
 */
inline constexpr std::uint8_t Magic0 = 'Z';
inline constexpr std::uint8_t Magic1 = 'S';
inline constexpr std::uint8_t Magic2 = 'M';
inline constexpr std::uint8_t Magic3 = 'T';

/** Versao suportada pelo loader actual. */
inline constexpr std::uint16_t SupportedVersion = 1;

/** Tamanho do header fixo (ate ao mapName). 4 magic + 2 version + 2 hsize + 4 flags
 *  + 4 pieceCount + 4 regionSize + 4*4 reserved = 36. */
inline constexpr std::uint16_t FixedHeaderSize = 36;

namespace Flags
{
    inline constexpr std::uint32_t HasRegions = 1u << 0;
    inline constexpr std::uint32_t ZstdCompressedReserved = 1u << 1;
}

/** Tipo de peca de terreno. Numero estavel — novos kinds aumentam o numero sem
 *  bump de versao. */
namespace PieceKind
{
    inline constexpr std::uint8_t TriangleMesh = 1;
    inline constexpr std::uint8_t HeightField = 2;
}
} // namespace Zeus::Collision::ZsmTerrain
