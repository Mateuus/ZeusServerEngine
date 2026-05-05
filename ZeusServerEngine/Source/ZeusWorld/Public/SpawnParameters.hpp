#pragma once

#include "Transform.hpp"

#include <string>

namespace Zeus::World
{
struct SpawnParameters
{
    std::string Name;
    Zeus::Math::Transform Transform = Zeus::Math::Transform::Identity;
    bool bStartActive = true;
    bool bAllowTick = false;
};
} // namespace Zeus::World
