#pragma once

#include "Actor.hpp"
#include "MathConstants.hpp"

namespace Zeus::Game::Movement
{
class MovementComponent;
}

namespace Zeus::Game::Entities
{
/**
 * `CharacterActor` representa um personagem server-side com cápsula lógica
 * (radius/halfHeight em centimetros). Ganha as tags `Character` e
 * `PlayerProxy` em `OnSpawned` para que o `ZeusRegionSystem` o detecte como
 * proxy de jogador.
 *
 * Em `OnSpawned` regista automaticamente um `MovementComponent` com tick
 * activo. O movimento por tick e' resolvido via `Actor::TickActorAndComponents`
 * que delega ao `MovementComponent::TickComponent`.
 *
 * Sem replicacao/predição nesta fase — a Parte 6 (rede) introduz S_SPAWN/
 * S_DESPAWN e a Parte 7 (input) introduzira a predição cliente.
 */
class CharacterActor : public Zeus::World::Actor
{
public:
    CharacterActor();
    ~CharacterActor() override;

    void OnSpawned() override;

    /** Acesso rapido ao componente de movimento registado em `OnSpawned`. */
    Zeus::Game::Movement::MovementComponent* GetMovementComponent() const { return MovementComponentPtr; }

    /** Cápsula logica em centimetros — usada pelo `MovementComponent` para sweeps. */
    double GetCapsuleRadiusCm() const { return CapsuleRadiusCm; }
    double GetCapsuleHalfHeightCm() const { return CapsuleHalfHeightCm; }

    void SetCapsuleSize(double radiusCm, double halfHeightCm);

private:
    double CapsuleRadiusCm = Zeus::Math::DefaultCapsuleRadiusCm;
    double CapsuleHalfHeightCm = Zeus::Math::DefaultCapsuleHeightCm * 0.5;

    /** Pointer raw cached — propriedade vive em `Actor::Components`. */
    Zeus::Game::Movement::MovementComponent* MovementComponentPtr = nullptr;
};
} // namespace Zeus::Game::Entities
