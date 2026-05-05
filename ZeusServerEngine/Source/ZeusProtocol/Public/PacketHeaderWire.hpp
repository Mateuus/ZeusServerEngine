#pragma once

#include "PacketConstants.hpp"
#include "PacketHeader.hpp"
#include "ZeusResult.hpp"

#include <cstddef>
#include <cstdint>

namespace Zeus::Protocol
{
/** Serializa `PacketHeader` para 32 bytes LE (wire). */
ZeusResult SerializePacketHeader(const PacketHeader& h, std::uint8_t out[ZEUS_WIRE_HEADER_BYTES]);

/** Lê 32 bytes LE para `PacketHeader` (sem validar magic/versão). */
ZeusResult DeserializePacketHeader(const std::uint8_t* data, std::size_t length, PacketHeader& out);
} // namespace Zeus::Protocol
