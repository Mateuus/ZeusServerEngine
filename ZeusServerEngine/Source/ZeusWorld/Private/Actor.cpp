#include "Actor.hpp"

#include "ActorComponent.hpp"

#include <algorithm>
#include <utility>

namespace Zeus::World
{
Actor::Actor() = default;

Actor::~Actor()
{
    for (auto& c : Components)
    {
        if (c != nullptr)
        {
            c->OnUnregister();
        }
    }
}

void Actor::SetEntityId(const EntityId id)
{
    Id = id;
}

void Actor::SetName(std::string name)
{
    Name = std::move(name);
}

void Actor::SetTransform(const Zeus::Math::Transform& transform)
{
    ActorTransform = transform;
}

Zeus::Math::Vector3 Actor::GetLocation() const
{
    return ActorTransform.Location;
}

void Actor::SetLocation(const Zeus::Math::Vector3& location)
{
    ActorTransform.Location = location;
}

Zeus::Math::Quaternion Actor::GetRotation() const
{
    return ActorTransform.Rotation;
}

void Actor::SetRotation(const Zeus::Math::Quaternion& rotation)
{
    ActorTransform.Rotation = rotation.Normalized();
}

Zeus::Math::Vector3 Actor::GetScale3D() const
{
    return ActorTransform.Scale3D;
}

void Actor::SetScale3D(const Zeus::Math::Vector3& scale)
{
    ActorTransform.Scale3D = scale;
}

void Actor::SetBounds(const Zeus::Math::Aabb& bounds)
{
    ActorBounds = bounds;
}

void Actor::Destroy()
{
    if (bPendingDestroy || LifecycleState == EActorLifecycleState::Destroyed)
    {
        return;
    }
    bPendingDestroy = true;
    if (LifecycleState != EActorLifecycleState::Uninitialized)
    {
        LifecycleState = EActorLifecycleState::PendingDestroy;
    }
}

void Actor::OnSpawned()
{
    LifecycleState = EActorLifecycleState::Spawning;
}

void Actor::BeginPlay()
{
    InternalBeginPlay();
}

void Actor::Tick(double /*deltaSeconds*/) {}

void Actor::TickActorAndComponents(const double deltaSeconds)
{
    if (IsPendingDestroy() || LifecycleState != EActorLifecycleState::Active)
    {
        return;
    }

    ActorTickFunction& actorTick = PrimaryActorTick;
    if (actorTick.bCanEverTick && actorTick.bTickEnabled)
    {
        bool shouldTickActor = true;
        if (actorTick.TickIntervalSeconds > 0.0)
        {
            actorTick.TimeSinceLastTick += deltaSeconds;
            if (actorTick.TimeSinceLastTick < actorTick.TickIntervalSeconds)
            {
                shouldTickActor = false;
            }
            else
            {
                actorTick.TimeSinceLastTick = 0.0;
            }
        }
        if (shouldTickActor)
        {
            Tick(deltaSeconds);
        }
    }

    for (auto& c : Components)
    {
        if (c == nullptr)
        {
            continue;
        }
        ActorTickFunction& t = c->GetPrimaryComponentTick();
        if (!t.bCanEverTick || !t.bTickEnabled)
        {
            continue;
        }
        if (t.TickIntervalSeconds > 0.0)
        {
            t.TimeSinceLastTick += deltaSeconds;
            if (t.TimeSinceLastTick < t.TickIntervalSeconds)
            {
                continue;
            }
            t.TimeSinceLastTick = 0.0;
        }
        c->TickComponent(deltaSeconds);
    }
}

void Actor::EndPlay()
{
    InternalEndPlay();
}

void Actor::AddTag(std::string tag)
{
    Tags.push_back(std::move(tag));
}

bool Actor::HasTag(const std::string& tag) const
{
    return std::find(Tags.begin(), Tags.end(), tag) != Tags.end();
}

void Actor::SetOwningWorld(World* world)
{
    OwningWorld = world;
}

void Actor::AddComponent(std::unique_ptr<ActorComponent> component)
{
    if (component == nullptr)
    {
        return;
    }
    ActorComponent* raw = component.get();
    raw->SetOwner(this);
    raw->OnRegister();
    Components.push_back(std::move(component));

    if (LifecycleState == EActorLifecycleState::Active)
    {
        raw->BeginPlay();
    }
}

void Actor::SetLifecycleState(const EActorLifecycleState state)
{
    LifecycleState = state;
}

void Actor::InternalBeginPlay()
{
    if (LifecycleState == EActorLifecycleState::Active || LifecycleState == EActorLifecycleState::Destroyed)
    {
        return;
    }
    LifecycleState = EActorLifecycleState::Active;
    for (auto& c : Components)
    {
        if (c != nullptr)
        {
            c->BeginPlay();
        }
    }
}

void Actor::InternalEndPlay()
{
    if (LifecycleState == EActorLifecycleState::Destroyed || LifecycleState == EActorLifecycleState::Uninitialized)
    {
        return;
    }
    for (auto& c : Components)
    {
        if (c != nullptr)
        {
            c->EndPlay();
        }
    }
    LifecycleState = EActorLifecycleState::Destroyed;
}
} // namespace Zeus::World
