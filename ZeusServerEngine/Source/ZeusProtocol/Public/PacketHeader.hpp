#pragma once

#include "PacketConstants.hpp"

#include <cstdint>

namespace Zeus::Protocol
{
/** Campos lógicos do cabeçalho (espelho do wire LE; ver `Docs/NETWORK.md`). */
struct PacketHeader
{
    std::uint32_t magic = ZEUS_PACKET_MAGIC;
    std::uint16_t version = ZEUS_PROTOCOL_VERSION;
    std::uint16_t headerSize = static_cast<std::uint16_t>(ZEUS_WIRE_HEADER_BYTES);

    std::uint16_t opcode = 0;
    std::uint8_t channel = 0;
    std::uint8_t delivery = 0;

    std::uint32_t sequence = 0;
    std::uint32_t ack = 0;
    std::uint32_t ackBits = 0;

    std::uint32_t connectionId = 0;
    std::uint16_t payloadSize = 0;
    std::uint16_t flags = 0;
};
} // namespace Zeus::Protocol
