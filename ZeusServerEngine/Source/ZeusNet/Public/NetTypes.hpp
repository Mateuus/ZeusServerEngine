#pragma once

#include <cstdint>

namespace Zeus::Net
{
using ConnectionId = std::uint32_t;
using SessionId = std::uint64_t;

inline constexpr ConnectionId kInvalidConnectionId = 0;
} // namespace Zeus::Net
