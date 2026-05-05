#pragma once

#include "EntityManager.hpp"
#include "Level.hpp"
#include "SpawnParameters.hpp"

#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

namespace Zeus::World
{
class World
{
public:
    bool Initialize(std::string worldName);
    void Shutdown();

    void BeginPlay();
    void Tick(double deltaSeconds);

    EntityManager& GetEntityManager() { return Entities; }
    const EntityManager& GetEntityManager() const { return Entities; }

    double GetTimeSeconds() const { return TimeSeconds; }

    Actor* SpawnActor(const SpawnParameters& params);

    /** Spawn tipado para actors derivados (chama o construtor padrao do tipo). */
    template<typename TActor>
    TActor* SpawnActorTyped(const SpawnParameters& params, bool beginPlayImmediately = true)
    {
        static_assert(std::is_base_of_v<Actor, TActor>, "TActor must derive from Actor");
        return Entities.SpawnActor<TActor>(this, params,
            beginPlayImmediately && bBegunPlay);
    }

    bool DestroyActor(EntityId id);

    /** Forwarder publico para iteracao read-only sobre todos os actors vivos. */
    void ForEachActor(const std::function<void(Actor&)>& fn) { Entities.ForEachActor(fn); }
    void ForEachActor(const std::function<void(const Actor&)>& fn) const { Entities.ForEachActor(fn); }

    bool HasBegunPlay() const { return bBegunPlay; }

private:
    std::string Name;
    EntityManager Entities;
    std::vector<std::unique_ptr<Level>> Levels;
    double TimeSeconds = 0.0;
    bool bInitialized = false;
    bool bBegunPlay = false;
    bool bLoggedFirstTickSummary = false;
};
} // namespace Zeus::World
