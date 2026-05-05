#pragma once

#include <cstdint>

namespace Zeus::Protocol
{
enum class ENetChannel : std::uint8_t
{
    Realtime = 0,
    Gameplay = 1,
    Chat = 2,
    Visual = 3,
    Loading = 4
};
} // namespace Zeus::Protocol
