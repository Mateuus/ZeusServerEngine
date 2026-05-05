#include "Level.hpp"

namespace Zeus::World
{
Level::Level(const std::uint64_t levelId, std::string name)
    : Name(std::move(name))
    , LevelId(levelId)
{
}

void Level::Load()
{
    bLoaded = true;
}

void Level::Unload()
{
    bLoaded = false;
}

void Level::Tick(double /*deltaSeconds*/)
{
}
} // namespace Zeus::World
