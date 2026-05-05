#pragma once

#include "MovementComponent.hpp"

#include <cstddef>

namespace Zeus::Collision
{
class CollisionWorld;
}

namespace Zeus::World
{
class World;
}

namespace Zeus::Game::Movement
{
/** Snapshot agregado de stats para o ConsoleLivePanel. */
struct MovementStats
{
    std::size_t CharacterCount = 0;
    std::size_t GroundedCount = 0;
    int SweepsLastTick = 0;
    double AvgVelocityCmS = 0.0;
};

/**
 * Façade thin do subsistema Movement. Nao substitui `World::Tick`: serve para
 * publicar configuracao a todos os `MovementComponent` e agregar stats por
 * tick para o overlay/diagnostico.
 *
 * O `MovementSystem` nao guarda lista de `MovementComponent`; usa
 * `World::ForEachActor` para refrescar parametros e calcular stats.
 */
class MovementSystem
{
public:
    MovementSystem();
    ~MovementSystem();

    void Configure(const MovementParameters& parameters);
    const MovementParameters& GetParameters() const { return Parameters; }

    void SetCollisionWorld(Zeus::Collision::CollisionWorld* world) { CollisionWorld = world; }
    Zeus::Collision::CollisionWorld* GetCollisionWorld() const { return CollisionWorld; }

    /** Aplica a configuracao actual a todos os `MovementComponent` do world. */
    void RefreshAllComponents(Zeus::World::World& world) const;

    /** Recalcula `MovementStats` percorrendo os actors do world. */
    void RefreshStats(Zeus::World::World& world);

    const MovementStats& GetStats() const { return Stats; }

private:
    MovementParameters Parameters;
    Zeus::Collision::CollisionWorld* CollisionWorld = nullptr;
    MovementStats Stats;
};
} // namespace Zeus::Game::Movement
