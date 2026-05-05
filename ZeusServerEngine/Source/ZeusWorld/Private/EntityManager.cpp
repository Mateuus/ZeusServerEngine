#include "EntityManager.hpp"

#include <algorithm>
#include <utility>

namespace Zeus::World
{
EntityManager::EntityManager() = default;

EntityManager::~EntityManager() = default;

Actor* EntityManager::FindActor(const EntityId id)
{
    const auto it = Actors.find(id);
    if (it == Actors.end())
    {
        return nullptr;
    }
    return it->second.get();
}

const Actor* EntityManager::FindActor(const EntityId id) const
{
    const auto it = Actors.find(id);
    if (it == Actors.end())
    {
        return nullptr;
    }
    return it->second.get();
}

bool EntityManager::DestroyActor(const EntityId id)
{
    Actor* a = FindActor(id);
    if (a == nullptr)
    {
        return false;
    }
    a->Destroy();
    if (a->IsPendingDestroy() &&
        std::find(PendingDestroy.begin(), PendingDestroy.end(), id) == PendingDestroy.end())
    {
        PendingDestroy.push_back(id);
    }
    return true;
}

void EntityManager::TickActors(const double deltaSeconds)
{
    for (auto& kv : Actors)
    {
        Actor* actor = kv.second.get();
        if (actor == nullptr || actor->IsPendingDestroy())
        {
            continue;
        }
        if (actor->GetLifecycleState() != EActorLifecycleState::Active)
        {
            continue;
        }
        actor->TickActorAndComponents(deltaSeconds);
    }
}

void EntityManager::FlushPendingDestroy()
{
    for (const EntityId id : PendingDestroy)
    {
        const auto it = Actors.find(id);
        if (it == Actors.end())
        {
            continue;
        }
        Actor* a = it->second.get();
        if (a != nullptr && a->GetLifecycleState() != EActorLifecycleState::Destroyed)
        {
            a->EndPlay();
        }
        Actors.erase(it);
    }
    PendingDestroy.clear();
}

void EntityManager::BeginPlayAllActors()
{
    for (auto& kv : Actors)
    {
        Actor* a = kv.second.get();
        if (a == nullptr || a->IsPendingDestroy())
        {
            continue;
        }
        if (a->GetLifecycleState() == EActorLifecycleState::Spawning)
        {
            a->BeginPlay();
        }
    }
}

EntityId EntityManager::AllocateEntityId()
{
    return NextEntityId++;
}
} // namespace Zeus::World
