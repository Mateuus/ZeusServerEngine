#pragma once

#include <cstdint>

namespace Zeus::World
{
using EntityId = std::uint64_t;

inline constexpr EntityId InvalidEntityId = 0;
} // namespace Zeus::World
