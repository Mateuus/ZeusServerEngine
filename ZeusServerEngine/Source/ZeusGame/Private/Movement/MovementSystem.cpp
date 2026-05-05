#include "Movement/MovementSystem.hpp"

#include "Actor.hpp"
#include "World.hpp"

namespace Zeus::Game::Movement
{
MovementSystem::MovementSystem() = default;
MovementSystem::~MovementSystem() = default;

void MovementSystem::Configure(const MovementParameters& parameters)
{
    Parameters = parameters;
}

void MovementSystem::RefreshAllComponents(Zeus::World::World& world) const
{
    world.ForEachActor([&](Zeus::World::Actor& actor) {
        if (auto* mc = actor.FindComponent<MovementComponent>())
        {
            mc->SetParameters(Parameters);
            if (CollisionWorld != nullptr && mc->GetCollisionWorld() == nullptr)
            {
                mc->SetCollisionWorld(CollisionWorld);
            }
        }
    });
}

void MovementSystem::RefreshStats(Zeus::World::World& world)
{
    Stats = MovementStats{};
    double accumVel = 0.0;
    world.ForEachActor([&](Zeus::World::Actor& actor) {
        const auto* mc = actor.FindComponent<MovementComponent>();
        if (mc == nullptr)
        {
            return;
        }
        ++Stats.CharacterCount;
        if (mc->IsGrounded())
        {
            ++Stats.GroundedCount;
        }
        accumVel += mc->GetVelocity().Length();
        Stats.SweepsLastTick += mc->GetSweepsLastTick();
    });
    if (Stats.CharacterCount > 0)
    {
        Stats.AvgVelocityCmS = accumVel / static_cast<double>(Stats.CharacterCount);
    }
}
} // namespace Zeus::Game::Movement
