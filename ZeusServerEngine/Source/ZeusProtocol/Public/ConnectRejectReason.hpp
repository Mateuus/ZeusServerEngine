#pragma once

#include <cstdint>

namespace Zeus::Protocol
{
enum class ConnectRejectReason : std::uint16_t
{
    InvalidProtocolVersion = 1,
    ServerFull = 2,
    InvalidPacket = 3
};
} // namespace Zeus::Protocol
