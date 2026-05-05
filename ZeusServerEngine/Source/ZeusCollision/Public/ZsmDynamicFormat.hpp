#pragma once

#include <cstdint>

namespace Zeus::Collision::ZsmDynamic
{
/**
 * Constantes oficiais do formato `dynamic_collision.zsm` versao 1 (`ZSMD`).
 * Tudo serializado em little-endian, campo a campo. Os shapes dentro de cada
 * volume sao reutilizados do enum `ECollisionShapeType` de `static_collision`.
 */
inline constexpr std::uint8_t Magic0 = 'Z';
inline constexpr std::uint8_t Magic1 = 'S';
inline constexpr std::uint8_t Magic2 = 'M';
inline constexpr std::uint8_t Magic3 = 'D';

/** Versao suportada pelo loader actual. */
inline constexpr std::uint16_t SupportedVersion = 1;

/** Tamanho do header fixo (ate ao mapName).
 * 4 magic + 2 version + 2 hsize + 4 flags + 4 volumeCount + 4 shapeCount
 * + 4 regionSize + 4*3 reserved = 32. */
inline constexpr std::uint16_t FixedHeaderSize = 32;

namespace Flags
{
    inline constexpr std::uint32_t HasRegions = 1u << 0;
    inline constexpr std::uint32_t ZstdCompressedReserved = 1u << 1;
}

/** Tipo de volume. Numero estavel — novos kinds aumentam o numero. */
namespace VolumeKind
{
    inline constexpr std::uint8_t Trigger = 1;
    inline constexpr std::uint8_t Water = 2;
    inline constexpr std::uint8_t Lava = 3;
    inline constexpr std::uint8_t KillVolume = 4;
    inline constexpr std::uint8_t SafeZone = 5;
}
} // namespace Zeus::Collision::ZsmDynamic
