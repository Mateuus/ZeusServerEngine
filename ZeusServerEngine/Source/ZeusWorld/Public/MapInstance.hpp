#pragma once

#include "World.hpp"

#include <cstdint>
#include <string>

namespace Zeus::World
{
enum class EMapInstanceState
{
    Unloaded,
    Loading,
    Loaded,
    Running,
    ShuttingDown
};

class MapInstance
{
public:
    explicit MapInstance(std::uint64_t mapInstanceId);

    bool Initialize(std::string mapName);
    void BeginPlay();
    void Tick(double deltaSeconds);
    void Shutdown();

    World& GetWorld() { return RuntimeWorld; }
    const World& GetWorld() const { return RuntimeWorld; }

    EMapInstanceState GetState() const { return State; }
    std::uint64_t GetMapInstanceId() const { return MapInstanceId; }
    const std::string& GetMapName() const { return MapName; }

private:
    std::uint64_t MapInstanceId = 0;
    std::string MapName;
    World RuntimeWorld;
    EMapInstanceState State = EMapInstanceState::Unloaded;
};
} // namespace Zeus::World
