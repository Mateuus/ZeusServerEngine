#include "Entities/CharacterActor.hpp"

#include "Movement/MovementComponent.hpp"

#include <memory>

namespace Zeus::Game::Entities
{
CharacterActor::CharacterActor() = default;
CharacterActor::~CharacterActor() = default;

void CharacterActor::OnSpawned()
{
    Zeus::World::Actor::OnSpawned();

    AddTag("Character");
    AddTag("PlayerProxy");

    auto component = std::make_unique<Zeus::Game::Movement::MovementComponent>();
    component->SetName("MovementComponent");
    Zeus::Game::Movement::MovementParameters params;
    params.CapsuleRadiusCm = CapsuleRadiusCm;
    params.CapsuleHalfHeightCm = CapsuleHalfHeightCm;
    component->SetParameters(params);

    MovementComponentPtr = component.get();
    AddComponent(std::move(component));
}

void CharacterActor::SetCapsuleSize(double radiusCm, double halfHeightCm)
{
    if (radiusCm > 0.0)
    {
        CapsuleRadiusCm = radiusCm;
    }
    if (halfHeightCm > 0.0)
    {
        CapsuleHalfHeightCm = halfHeightCm;
    }
    if (MovementComponentPtr != nullptr)
    {
        Zeus::Game::Movement::MovementParameters params = MovementComponentPtr->GetParameters();
        params.CapsuleRadiusCm = CapsuleRadiusCm;
        params.CapsuleHalfHeightCm = CapsuleHalfHeightCm;
        MovementComponentPtr->SetParameters(params);
    }
}
} // namespace Zeus::Game::Entities
