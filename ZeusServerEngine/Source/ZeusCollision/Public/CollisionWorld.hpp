#pragma once

#include "CollisionAsset.hpp"
#include "PhysicsWorld.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Zeus::Collision
{
// Forward declaration; o terrain partilha o `BodiesByRegion` mas vive em
// header proprio (`TerrainCollisionAsset.hpp`).
struct TerrainCollisionAsset;

/**
 * Camada lógica que orquestra o `CollisionAsset` (dados) e o `PhysicsWorld`
 * (Jolt). Responsável por:
 *   - Carregar `static_collision.zsm` para um `CollisionAsset`.
 *   - Inicializar o `PhysicsWorld` e construir bodies estáticos.
 *   - Expor queries simples (RaycastDown, CollideCapsule).
 *   - Manter stats agregadas e o número de bodies criados.
 *
 * Métodos `LoadRegion`/`UnloadRegion` reservados para a Parte 4.5
 * (`ZeusRegionSystem` / World Partition server-side).
 */
class CollisionWorld
{
public:
    CollisionWorld();
    ~CollisionWorld();

    CollisionWorld(const CollisionWorld&) = delete;
    CollisionWorld& operator=(const CollisionWorld&) = delete;

    /**
     * Carrega um ficheiro `.zsm` para o `CollisionAsset` interno.
     * Se o ficheiro não existir, devolve `false` mas não falha o servidor;
     * o caller decide se isso é fatal.
     */
    bool LoadFromZsm(const std::filesystem::path& path);

    /** Carrega um `CollisionAsset` já populado em memória (usado em smoke tests). */
    bool LoadFromAsset(CollisionAsset asset);

    /**
     * Carrega um `TerrainCollisionAsset` (`terrain_collision.zsm` v1 / ZSMT).
     * O terrain partilha `BodiesByRegion` com o asset estatico — bodies criados
     * a partir do terrain entram na lista da regiao correspondente.
     */
    bool LoadFromTerrainZsm(const std::filesystem::path& path);
    bool LoadFromTerrainAsset(TerrainCollisionAsset asset);

    /**
     * Inicializa Jolt e cria todos os static bodies a partir do asset corrente.
     * Equivalente a `LoadRegion` para todas as regioes — atalho "sem streaming".
     * Devolve `false` se nenhum body foi criado (asset vazio).
     */
    bool BuildPhysicsWorld();

    /**
     * Inicializa Jolt sem criar bodies. Usado em modo streaming, antes de o
     * `ZeusRegionSystem` chamar `LoadRegion` selectivamente.
     */
    bool InitPhysicsLazy();

    /** Carrega bodies de uma regiao (idempotente). */
    bool LoadRegion(std::uint32_t regionId);

    /** Liberta bodies de uma regiao (idempotente). */
    bool UnloadRegion(std::uint32_t regionId);

    bool IsRegionLoaded(std::uint32_t regionId) const;
    std::size_t GetLoadedRegionCount() const { return LoadedRegions.size(); }
    const std::unordered_set<std::uint32_t>& GetLoadedRegions() const { return LoadedRegions; }
    std::size_t GetBodiesInRegion(std::uint32_t regionId) const;

    void Shutdown();

    bool IsLoaded() const { return bAssetLoaded; }
    bool IsPhysicsBuilt() const { return PhysicsBuilt; }
    bool HasTerrain() const { return bTerrainLoaded; }

    std::size_t GetEntityCount() const;
    std::size_t GetShapeCount() const;
    std::size_t GetStaticBodyCount() const;

    /**
     * Façade de queries — `MovementComponent` e gameplay devem depender só do
     * `CollisionWorld`, não do `PhysicsWorld` directamente.
     */
    bool Raycast(const Math::Vector3& originCm, const Math::Vector3& directionUnnormalized,
        double maxDistanceCm, RaycastHit& outHit) const;
    bool RaycastDown(const Math::Vector3& originCm, double maxDistanceCm, RaycastHit& outHit) const;
    bool CollideCapsule(const Math::Vector3& centerCm, double radiusCm, double halfHeightCm,
        ContactInfo& outFirstContact) const;
    bool SweepCapsule(const Math::Vector3& centerStartCm, double radiusCm, double halfHeightCm,
        const Math::Vector3& sweepCm, SweepHit& outHit) const;

    const CollisionAsset& GetAsset() const { return Asset; }
    const TerrainCollisionAsset* GetTerrainAsset() const { return Terrain.get(); }
    PhysicsWorld& GetPhysicsWorld() { return *Physics; }
    const PhysicsWorld& GetPhysicsWorld() const { return *Physics; }

private:
    /**
     * Cria os bodies Jolt para uma entidade (todos os shapes) e devolve os
     * bodyIds gerados — usado por `LoadRegion` e por `BuildPhysicsWorld`.
     */
    void BuildBodiesForEntity(const CollisionEntity& entity, std::vector<std::uint32_t>& outBodyIds);

    /**
     * Cria os bodies Jolt para uma piece de terreno (TriangleMesh ou
     * HeightField) e empurra o id resultante para `outBodyIds`.
     */
    void BuildBodiesForTerrainPiece(std::size_t pieceIndex, std::vector<std::uint32_t>& outBodyIds);

    CollisionAsset Asset;
    std::unique_ptr<TerrainCollisionAsset> Terrain;
    std::unique_ptr<PhysicsWorld> Physics;

    /** regionId -> bodyIds Jolt vivos. */
    std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> BodiesByRegion;
    std::unordered_set<std::uint32_t> LoadedRegions;

    bool bAssetLoaded = false;
    bool bTerrainLoaded = false;
    bool PhysicsBuilt = false;
};
} // namespace Zeus::Collision
