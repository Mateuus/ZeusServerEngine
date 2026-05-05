#pragma once

#include "EntityManager.hpp"
#include "Level.hpp"
#include "SpawnParameters.hpp"

#include <memory>
#include <string>
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
    bool DestroyActor(EntityId id);

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
