#pragma once

#include "CollisionAsset.hpp"
#include "PhysicsWorld.hpp"

#include <cstddef>
#include <filesystem>
#include <memory>

namespace Zeus::Collision
{
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
     * Inicializa Jolt e cria todos os static bodies a partir do asset corrente.
     * Devolve `false` se nenhum body foi criado (asset vazio).
     */
    bool BuildPhysicsWorld();

    void Shutdown();

    bool IsLoaded() const { return bAssetLoaded; }
    bool IsPhysicsBuilt() const { return PhysicsBuilt; }

    std::size_t GetEntityCount() const;
    std::size_t GetShapeCount() const;
    std::size_t GetStaticBodyCount() const;

    const CollisionAsset& GetAsset() const { return Asset; }
    PhysicsWorld& GetPhysicsWorld() { return *Physics; }
    const PhysicsWorld& GetPhysicsWorld() const { return *Physics; }

private:
    CollisionAsset Asset;
    std::unique_ptr<PhysicsWorld> Physics;
    bool bAssetLoaded = false;
    bool PhysicsBuilt = false;
};
} // namespace Zeus::Collision
