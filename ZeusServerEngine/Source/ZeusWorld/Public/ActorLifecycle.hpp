#pragma once

namespace Zeus::World
{
enum class EActorLifecycleState
{
    Uninitialized,
    Spawning,
    Active,
    PendingDestroy,
    Destroyed
};
} // namespace Zeus::World
