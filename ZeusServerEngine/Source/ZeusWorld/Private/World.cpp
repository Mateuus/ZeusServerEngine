#include "World.hpp"

#include "Level.hpp"

#include "ZeusLog.hpp"

#include <utility>

namespace Zeus::World
{
bool World::Initialize(std::string worldName)
{
    if (bInitialized)
    {
        return true;
    }
    Name = std::move(worldName);
    Levels.clear();
    Levels.push_back(std::make_unique<Level>(1ULL, std::string("PersistentLevel")));
    bInitialized = true;
    bBegunPlay = false;
    bLoggedFirstTickSummary = false;
    TimeSeconds = 0.0;
    ZeusLog::Info("World", std::string("[World] Initialized name=").append(Name));
    return true;
}

void World::Shutdown()
{
    if (!bInitialized)
    {
        return;
    }
    Entities.FlushPendingDestroy();
    for (auto& level : Levels)
    {
        if (level != nullptr)
        {
            level->Unload();
        }
    }
    Levels.clear();
    bInitialized = false;
    bBegunPlay = false;
    bLoggedFirstTickSummary = false;
    TimeSeconds = 0.0;
    Name.clear();
}

void World::BeginPlay()
{
    if (!bInitialized || bBegunPlay)
    {
        return;
    }
    for (auto& level : Levels)
    {
        if (level != nullptr)
        {
            level->Load();
        }
    }
    bBegunPlay = true;
    ZeusLog::Info("World", "[World] BeginPlay");
    Entities.BeginPlayAllActors();
}

void World::Tick(const double deltaSeconds)
{
    if (!bInitialized || !bBegunPlay)
    {
        return;
    }
    TimeSeconds += deltaSeconds;
    for (auto& level : Levels)
    {
        if (level != nullptr && level->IsLoaded())
        {
            level->Tick(deltaSeconds);
        }
    }
    Entities.TickActors(deltaSeconds);
    Entities.FlushPendingDestroy();

    if (!bLoggedFirstTickSummary)
    {
        bLoggedFirstTickSummary = true;
        const std::string line = std::string("[World] Tick actors=") + std::to_string(Entities.GetActorCount());
        ZeusLog::Info("World", std::string_view(line));
    }
}

Actor* World::SpawnActor(const SpawnParameters& params)
{
    Actor* spawned = Entities.SpawnActor<Actor>(this, params, bBegunPlay);
    if (spawned != nullptr)
    {
        const std::string line = std::string("[World] Spawned Actor entityId=") + std::to_string(spawned->GetEntityId()) +
                                 std::string(" name=") + spawned->GetName();
        ZeusLog::Info("World", std::string_view(line));
    }
    return spawned;
}

bool World::DestroyActor(const EntityId id)
{
    return Entities.DestroyActor(id);
}
} // namespace Zeus::World
