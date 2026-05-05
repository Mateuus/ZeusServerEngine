#pragma once

#include <cstdint>

namespace Zeus::Protocol
{
enum class ConnectRejectReason : std::uint16_t
{
    InvalidProtocolVersion = 1,
    ServerFull = 2,
    InvalidPacket = 3,
    /** `C_CONNECT_RESPONSE` com nonces incoerentes com a sessão em `Connecting`. */
    InvalidHandshake = 4
};
} // namespace Zeus::Protocol
