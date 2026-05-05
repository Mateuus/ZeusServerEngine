#include "MapInstance.hpp"

#include "ZeusLog.hpp"

#include <utility>

namespace Zeus::World
{
MapInstance::MapInstance(const std::uint64_t mapInstanceId)
    : MapInstanceId(mapInstanceId)
{
}

bool MapInstance::Initialize(std::string mapName)
{
    MapName = std::move(mapName);
    State = EMapInstanceState::Loading;
    const bool ok = RuntimeWorld.Initialize(MapName);
    if (!ok)
    {
        State = EMapInstanceState::Unloaded;
        return false;
    }
    State = EMapInstanceState::Loaded;
    const std::string line = std::string("[MapInstance] Created map=") + MapName;
    ZeusLog::Info("MapInstance", std::string_view(line));
    return true;
}

void MapInstance::BeginPlay()
{
    if (State != EMapInstanceState::Loaded)
    {
        return;
    }
    RuntimeWorld.BeginPlay();
    State = EMapInstanceState::Running;
}

void MapInstance::Tick(const double deltaSeconds)
{
    if (State != EMapInstanceState::Running)
    {
        return;
    }
    RuntimeWorld.Tick(deltaSeconds);
}

void MapInstance::Shutdown()
{
    if (State == EMapInstanceState::Unloaded)
    {
        return;
    }
    State = EMapInstanceState::ShuttingDown;
    RuntimeWorld.Shutdown();
    State = EMapInstanceState::Unloaded;
}
} // namespace Zeus::World
