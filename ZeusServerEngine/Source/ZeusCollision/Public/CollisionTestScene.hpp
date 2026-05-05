#pragma once

#include "CollisionAsset.hpp"
#include "DynamicCollisionAsset.hpp"
#include "TerrainCollisionAsset.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace Zeus::Collision
{
class CollisionWorld;
class DynamicCollisionWorld;

/** Resultado de um único cenário do smoke test. */
struct TestScenarioResult
{
    std::string Name;
    bool bPassed = false;
    std::string Detail;
};

/** Resumo da bateria de smoke tests. */
struct TestSceneReport
{
    std::vector<TestScenarioResult> Scenarios;
    std::size_t PassedCount = 0;
    std::size_t FailedCount = 0;
};

/**
 * Smoke tests determinísticos para validar o pipeline `CollisionAsset ->
 * Jolt`. Não substituem testes futuros do `CharacterMovementComponent`.
 *
 * Cenários:
 *   TC-01 capsule cai sobre o chão (RaycastDown).
 *   TC-02 capsule sobreposta a uma parede (CollideCapsule).
 *   TC-03 rampa válida (≈30°) é caminhável (IsWalkableFloor).
 *   TC-04 rampa íngreme (≈70°) bloqueia (IsWalkableFloor).
 *   TC-05 raycast lateral atinge a parede (Raycast).
 */
class CollisionTestScene
{
public:
    /** Constrói um asset programático com chão + parede + rampa válida + rampa íngreme. */
    static CollisionAsset BuildProgrammaticAsset();

    /** Asset dinamico com 1 trigger ([±200,±200,±150]) e EventTag "trigger_a". */
    static DynamicCollisionAsset BuildProgrammaticDynamicAsset();

    /**
     * Terrain asset com 1 HeightField plano (10x10) — `RaycastDown` deve
     * devolver normal Z=+1 sobre ele.
     */
    static TerrainCollisionAsset BuildProgrammaticTerrainAsset();

    /** Roda os cenarios no `CollisionWorld` corrente. Loga os resultados. */
    static TestSceneReport RunAll(CollisionWorld& world);

    /** Cenarios adicionais para o streaming/dynamic/terrain (TC-06..TC-09). */
    static TestSceneReport RunStreamingScenarios(CollisionWorld& world,
        DynamicCollisionWorld* dynamicWorld);
};
} // namespace Zeus::Collision
