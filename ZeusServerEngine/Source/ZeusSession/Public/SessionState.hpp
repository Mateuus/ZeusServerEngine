#pragma once

#include <cstdint>

namespace Zeus::Session
{
enum class SessionState : std::uint8_t
{
    Disconnected = 0,
    Connecting = 1,
    Connected = 2,
    Authenticated = 3,
    LoadingWorld = 4,
    InWorld = 5,
    Disconnecting = 6
};
} // namespace Zeus::Session
