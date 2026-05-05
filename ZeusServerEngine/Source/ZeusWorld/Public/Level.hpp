#pragma once

#include "Bounds.hpp"

#include <cstdint>
#include <string>

namespace Zeus::World
{
class Level
{
public:
    Level(std::uint64_t levelId, std::string name);

    const std::string& GetName() const { return Name; }
    std::uint64_t GetLevelId() const { return LevelId; }

    bool IsLoaded() const { return bLoaded; }

    void Load();
    void Unload();
    void Tick(double deltaSeconds);

    const Zeus::Math::Aabb& GetBounds() const { return Bounds; }
    void SetBounds(const Zeus::Math::Aabb& bounds) { Bounds = bounds; }

private:
    std::string Name;
    std::uint64_t LevelId = 0;
    bool bLoaded = false;
    Zeus::Math::Aabb Bounds = Zeus::Math::Aabb::Empty();
};
} // namespace Zeus::World
