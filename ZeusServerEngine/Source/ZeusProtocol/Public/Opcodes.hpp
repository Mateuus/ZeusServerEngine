#pragma once

#include <cstdint>

namespace Zeus::Protocol
{
enum class EOpcode : std::uint16_t
{
    Unknown = 0,

    C_CONNECT_REQUEST = 1000,
    S_CONNECT_OK = 1001,
    S_CONNECT_REJECT = 1002,

    C_PING = 1010,
    S_PONG = 1011,

    C_DISCONNECT = 1020,
    S_DISCONNECT_OK = 1021,

    C_TEST_RELIABLE = 1100,
    S_TEST_RELIABLE = 1101,

    C_TEST_ORDERED = 1110,
    S_TEST_ORDERED = 1111
};
} // namespace Zeus::Protocol
