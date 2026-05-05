#pragma once

#include <cstdint>

namespace Zeus::Collision::Zsm
{
/**
 * Constantes oficiais do formato `static_collision.zsm` versão 1 (`ZSMC`).
 * Tudo é serializado em little-endian, campo a campo, com strings em UTF-8
 * prefixadas por um `uint16` de comprimento (ver `ZsmCollisionLoader`).
 */
inline constexpr std::uint8_t Magic0 = 'Z';
inline constexpr std::uint8_t Magic1 = 'S';
inline constexpr std::uint8_t Magic2 = 'M';
inline constexpr std::uint8_t Magic3 = 'C';

/** Versão suportada pelo loader actual. */
inline constexpr std::uint16_t SupportedVersion = 1;

/** Tamanho do header fixo (até ao `mapNameLen`). Não inclui a string de nome. */
inline constexpr std::uint16_t FixedHeaderSize = 32;

/** Bits do campo `Flags` no header. */
namespace Flags
{
    inline constexpr std::uint32_t HasRegions = 1u << 0;
    inline constexpr std::uint32_t ZstdCompressedReserved = 1u << 1;
}
} // namespace Zeus::Collision::Zsm
