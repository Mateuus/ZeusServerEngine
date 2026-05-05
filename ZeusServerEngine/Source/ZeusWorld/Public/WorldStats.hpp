#pragma once

#include <cstddef>

namespace Zeus::World
{
struct WorldStats
{
    std::size_t ActorCount = 0;
    std::size_t MapInstanceCount = 0;
    double WorldTickMs = 0.0;
    std::size_t PendingDestroyCount = 0;
};
} // namespace Zeus::World
