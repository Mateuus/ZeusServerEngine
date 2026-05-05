#pragma once

#include "ActorComponent.hpp"
#include "Vector3.hpp"

namespace Zeus::Collision
{
class CollisionWorld;
}

namespace Zeus::Game::Movement
{
/**
 * Configuracao por personagem (capsula + parametros de movimento). E injectada
 * pelo `MovementSystem` ou directamente pelo dono do componente.
 */
struct MovementParameters
{
    double GravityZ = -980.0;          ///< cm/s^2 (Zeus convention).
    double MaxSlopeAngleDeg = 45.0;    ///< limite walkable.
    double StepHeightCm = 45.0;        ///< degrau maximo subido sem saltar.
    double DefaultSpeedCmS = 600.0;    ///< velocidade alvo de `MoveByAxis`.
    double JumpVelocityCmS = 600.0;    ///< velocidade vertical aplicada por `Jump`.
    double CapsuleRadiusCm = 35.0;
    double CapsuleHalfHeightCm = 90.0;
};

/**
 * Componente server-only que resolve o movimento de uma capsula contra o
 * `CollisionWorld`. Pipeline por tick:
 *   1. Integra gravidade em `velocity.Z`.
 *   2. Sweep horizontal em `MoveByAxis * speed`. Em colisao tenta slide e
 *      step up (ate `StepHeightCm`).
 *   3. Sweep vertical (`velocity.Z * dt`). Snap to ground em chao walkable.
 *
 * Toda a comunicacao com a colisao passa pelo `CollisionWorld::SweepCapsule`
 * (Jolt CastShape) — o componente nao depende directamente do `PhysicsWorld`.
 */
class MovementComponent : public Zeus::World::ActorComponent
{
public:
    MovementComponent();
    ~MovementComponent() override;

    void OnRegister() override;
    void TickComponent(double deltaSeconds) override;

    /** Liga este componente a um `CollisionWorld` para queries. */
    void SetCollisionWorld(Zeus::Collision::CollisionWorld* world) { CollisionWorld = world; }
    Zeus::Collision::CollisionWorld* GetCollisionWorld() const { return CollisionWorld; }

    void SetParameters(const MovementParameters& params) { Parameters = params; }
    const MovementParameters& GetParameters() const { return Parameters; }

    /**
     * Aceita input em coordenadas ZeusWorld (X = forward, Y = right). Apenas
     * regista a intencao; o movimento real acontece em `TickComponent`.
     * Valores tipicos sao [-1, 1] e sao clampados.
     */
    void MoveByAxis(double forward, double right);

    /** Marca um pedido de salto a aplicar no proximo tick (se grounded). */
    void Jump();

    /** Limpa o input acumulado (mas mantem velocity vertical). */
    void Stop();

    bool IsGrounded() const { return bIsGrounded; }
    const Zeus::Math::Vector3& GetVelocity() const { return Velocity; }
    const Zeus::Math::Vector3& GetFloorNormal() const { return FloorNormal; }
    int GetSweepsLastTick() const { return SweepsLastTick; }

    /** Conveniencia: forca um teleport limpo da posicao do owner Actor. */
    void TeleportTo(const Zeus::Math::Vector3& positionCm);

private:
    /** Sub-passes do pipeline. */
    void IntegrateGravity(double deltaSeconds);
    void StepHorizontal(double deltaSeconds);
    void StepVertical(double deltaSeconds);

    bool TrySweep(const Zeus::Math::Vector3& fromCm, const Zeus::Math::Vector3& deltaCm,
        Zeus::Math::Vector3& outFinalCm, Zeus::Math::Vector3& outNormal, double& outFraction,
        bool& outHit);

    bool IsWalkable(const Zeus::Math::Vector3& normal) const;

    MovementParameters Parameters;
    Zeus::Collision::CollisionWorld* CollisionWorld = nullptr;

    Zeus::Math::Vector3 AxisInput = Zeus::Math::Vector3::Zero;
    Zeus::Math::Vector3 Velocity = Zeus::Math::Vector3::Zero;
    Zeus::Math::Vector3 FloorNormal = Zeus::Math::Vector3(0.0, 0.0, 1.0);
    bool bIsGrounded = false;
    bool bJumpQueued = false;
    int SweepsLastTick = 0;
};
} // namespace Zeus::Game::Movement
