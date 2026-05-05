#pragma once

namespace Zeus::World
{
struct ActorTickFunction
{
    bool bCanEverTick = false;
    bool bStartWithTickEnabled = false;
    bool bTickEnabled = false;
    double TickIntervalSeconds = 0.0;
    double TimeSinceLastTick = 0.0;
};
} // namespace Zeus::World
