#pragma once

#include "CollisionShape.hpp"
#include "Transform.hpp"
#include "Vector3.hpp"

#include <cstdint>
#include <memory>

namespace Zeus::Collision
{
struct TerrainPiece;
}

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
 * Resultado de um shape cast (sweep). `Distance` é a distância em cm percorrida
 * antes do primeiro contacto; `Normal` é a normal de contacto (do mundo para a
 * cápsula). `Fraction` permite reusar o vector de sweep original.
 */
struct SweepHit
{
    bool bHit = false;
    double Fraction = 0.0;
    double Distance = 0.0;
    Math::Vector3 ImpactPoint = Math::Vector3::Zero;
    Math::Vector3 ImpactNormal = Math::Vector3::Zero;
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

    /**
     * Cria um body estatico para uma piece de terreno (TriangleMesh ou
     * HeightField). Envolve internamente o `JoltShapeFactory::CreateMeshShape`/
     * `CreateHeightFieldShape`.
     */
    std::uint32_t AddStaticBodyForTerrain(const TerrainPiece& piece);

    /**
     * Remove e destroi um body criado anteriormente por `AddStaticBody`.
     * Devolve `false` se o id nao existe ou se o sistema nao esta inicializado.
     */
    bool RemoveStaticBody(std::uint32_t bodyId);

    /** Faz Optimize do BroadPhase após adicionar todos os bodies estáticos. */
    void OptimizeBroadPhase();

    /**
     * Versao com debounce: so executa o `OptimizeBroadPhase` se o numero de
     * `Add/RemoveStaticBody` desde o ultimo optimize ultrapassar o threshold
     * interno. Usada pelo `ZeusRegionSystem` para evitar custo por tick.
     */
    void OptimizeBroadPhaseIfNeeded();

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

    /**
     * Sweep (shape cast) de uma capsula em pe (eixo Z) ao longo de `sweepCm`.
     * Coordenadas em centimetros; `centerStartCm` e o centro da capsula no
     * inicio do sweep. O caller pode usar o `Fraction` para mover a capsula ate
     * just-before-impact (`pos + sweepCm * fraction * 0.999`).
     */
    bool SweepCapsule(const Math::Vector3& centerStartCm, double radiusCm, double halfHeightCm,
        const Math::Vector3& sweepCm, SweepHit& outHit) const;

    /** Número de bodies registados (estáticos). */
    std::size_t GetBodyCount() const;

private:
    struct PimplState;
    std::unique_ptr<PimplState> State;
};
} // namespace Zeus::Collision
