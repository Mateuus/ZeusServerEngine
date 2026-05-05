#include "ActorComponent.hpp"

namespace Zeus::World
{
ActorComponent::ActorComponent() = default;

ActorComponent::~ActorComponent() = default;

void ActorComponent::SetOwner(Actor* owner)
{
    Owner = owner;
}

void ActorComponent::SetName(std::string name)
{
    Name = std::move(name);
}

void ActorComponent::OnRegister() {}

void ActorComponent::BeginPlay() {}

void ActorComponent::TickComponent(double /*deltaSeconds*/) {}

void ActorComponent::EndPlay() {}

void ActorComponent::OnUnregister() {}

bool ActorComponent::IsTickEnabled() const
{
    return PrimaryComponentTick.bCanEverTick && PrimaryComponentTick.bTickEnabled;
}

void ActorComponent::SetTickEnabled(const bool enabled)
{
    if (!PrimaryComponentTick.bCanEverTick)
    {
        return;
    }
    PrimaryComponentTick.bTickEnabled = enabled;
}
} // namespace Zeus::World
