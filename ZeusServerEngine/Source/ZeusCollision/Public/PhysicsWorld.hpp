#pragma once

#include "CollisionShape.hpp"
#include "Transform.hpp"
#include "Vector3.hpp"

#include <cstdint>
#include <memory>

namespace Zeus::Collision
{
/** Resultado de um raycast em coordenadas Zeus (cm). */
struct RaycastHit
{
    bool bHit = false;
    Math::Vector3 ImpactPoint = Math::Vector3::Zero;
    Math::Vector3 ImpactNormal = Math::Vector3::Zero;
    double Distance = 0.0;
    std::uint32_t BodyId = 0;
};

/** Resumo de um teste de overlap (apenas indica se há contactos). */
struct ContactInfo
{
    Math::Vector3 ContactPoint = Math::Vector3::Zero;
    Math::Vector3 ContactNormal = Math::Vector3::Zero;
    std::uint32_t BodyId = 0;
};

/**
 * Wrapper sobre o `JPH::PhysicsSystem`. Esta camada é a única que conhece
 * Jolt no servidor. Tudo é exposto em centímetros; conversões cm <-> m vivem
 * em `JoltConversion.hpp`.
 *
 * Implementação interna em `PimplState` para evitar incluir cabeçalhos Jolt
 * em ficheiros que apenas usam o `CollisionWorld`.
 */
class PhysicsWorld
{
public:
    PhysicsWorld();
    ~PhysicsWorld();

    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;

    /** Inicializa Jolt (Factory/JobSystem/BroadPhase/PhysicsSystem). */
    bool Initialize();

    /** Liberta Jolt; seguro chamar mesmo se `Initialize` falhou. */
    void Shutdown();

    bool IsInitialized() const;

    /** Cria um body estático com o shape dado, no transform mundial Zeus (cm). */
    std::uint32_t AddStaticBody(const CollisionShape& shape, const Math::Transform& worldTransform);

    /** Faz Optimize do BroadPhase após adicionar todos os bodies estáticos. */
    void OptimizeBroadPhase();

    /** Roda 1 step (deltaSeconds) — útil só para smoke tests. */
    void Step(double deltaSeconds);

    /** Raycast em cm. Aceita origem + direção (não precisa estar normalizada) e distância máxima. */
    bool Raycast(const Math::Vector3& originCm, const Math::Vector3& directionUnnormalized,
        double maxDistanceCm, RaycastHit& outHit) const;

    /**
     * Raycast vertical para baixo (vetor `-Z`). Devolve `bHit=true` no primeiro
     * contacto, com normal e distância em cm.
     */
    bool RaycastDown(const Math::Vector3& originCm, double maxDistanceCm, RaycastHit& outHit) const;

    /**
     * Teste de overlap de uma cápsula em pé (Z up). Apenas devolve `true`/`false`
     * se há corpo a sobrepor; este método é suficiente para o smoke test desta
     * fase (sweep/slide ficam para a Parte 5).
     */
    bool CollideCapsule(const Math::Vector3& centerCm, double radiusCm, double halfHeightCm,
        ContactInfo& outFirstContact) const;

    /** Número de bodies registados (estáticos). */
    std::size_t GetBodyCount() const;

private:
    struct PimplState;
    std::unique_ptr<PimplState> State;
};
} // namespace Zeus::Collision
