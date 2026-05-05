#pragma once

#include "Actor.hpp"
#include "EntityId.hpp"
#include "SpawnParameters.hpp"

#include <cstddef>
#include <functional>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace Zeus::World
{
class World;

class EntityManager
{
public:
    EntityManager();
    ~EntityManager();

    template<typename TActor>
    TActor* SpawnActor(World* owningWorld, const SpawnParameters& params, const bool beginPlayImmediately)
    {
        static_assert(std::is_base_of_v<Actor, TActor>, "TActor must derive from Actor");
        const EntityId id = AllocateEntityId();
        auto actor = std::make_unique<TActor>();
        TActor* raw = actor.get();
        raw->SetEntityId(id);
        raw->SetName(params.Name);
        raw->SetTransform(params.Transform);
        raw->SetOwningWorld(owningWorld);

        raw->GetPrimaryActorTick().bCanEverTick = params.bAllowTick;
        raw->GetPrimaryActorTick().bStartWithTickEnabled = params.bAllowTick && params.bStartActive;
        raw->GetPrimaryActorTick().bTickEnabled = raw->GetPrimaryActorTick().bStartWithTickEnabled;
        raw->GetPrimaryActorTick().TickIntervalSeconds = 0.0;
        raw->GetPrimaryActorTick().TimeSinceLastTick = 0.0;

        raw->OnSpawned();
        if (beginPlayImmediately)
        {
            raw->BeginPlay();
        }

        Actors.emplace(id, std::move(actor));
        return raw;
    }

    Actor* FindActor(EntityId id);
    const Actor* FindActor(EntityId id) const;

    bool DestroyActor(EntityId id);

    void TickActors(double deltaSeconds);
    void FlushPendingDestroy();

    std::size_t GetActorCount() const { return Actors.size(); }
    std::size_t GetPendingDestroyCount() const { return PendingDestroy.size(); }

    void BeginPlayAllActors();

    /** Iteracao read-write sobre todos os actors vivos (ignora PendingDestroy). */
    void ForEachActor(const std::function<void(Actor&)>& fn);
    /** Iteracao read-only sobre todos os actors vivos. */
    void ForEachActor(const std::function<void(const Actor&)>& fn) const;

private:
    EntityId AllocateEntityId();

    EntityId NextEntityId = 1;
    std::unordered_map<EntityId, std::unique_ptr<Actor>> Actors;
    std::vector<EntityId> PendingDestroy;
};
} // namespace Zeus::World
