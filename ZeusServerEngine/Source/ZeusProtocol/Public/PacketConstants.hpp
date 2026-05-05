#pragma once

#include <cstddef>
#include <cstdint>

namespace Zeus::Protocol
{
/** FourCC "ZEUS" em wire little-endian (primeiro byte = 'Z'). */
inline constexpr std::uint32_t ZEUS_PACKET_MAGIC =
    (static_cast<std::uint32_t>('Z')) | (static_cast<std::uint32_t>('E') << 8) |
    (static_cast<std::uint32_t>('U') << 16) | (static_cast<std::uint32_t>('S') << 24);

inline constexpr std::uint16_t ZEUS_PROTOCOL_VERSION = 2;

/** Tamanho fixo do cabeçalho binário na Parte 2 (wire LE). */
inline constexpr std::size_t ZEUS_WIRE_HEADER_BYTES = 32;

/** Limite total do datagrama (evitar fragmentação UDP inicial). */
inline constexpr std::size_t ZEUS_MAX_PACKET_BYTES = 1200;

inline constexpr std::size_t ZEUS_MAX_PAYLOAD_BYTES = ZEUS_MAX_PACKET_BYTES - ZEUS_WIRE_HEADER_BYTES;
} // namespace Zeus::Protocol
