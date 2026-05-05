#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace Zeus::Collision
{
class CollisionWorld;
}

namespace Zeus::Game::Movement
{
struct MovementTestResult
{
    std::string Name;
    bool bPassed = false;
    std::string Detail;
};

struct MovementTestReport
{
    std::vector<MovementTestResult> Scenarios;
    std::size_t PassedCount = 0;
    std::size_t FailedCount = 0;
};

/**
 * Smoke tests programaticos do `MovementComponent`. Constroi cenarios sobre o
 * `BuildProgrammaticAsset()` enriquecido (rampa walkable, rampa ingreme,
 * parede, degrau pequeno, degrau grande) — independentes do mapa real.
 *
 * IDs cobertos:
 *   TC-MOVE-001 — capsula assenta no chao em 1 tick.
 *   TC-MOVE-002 — andar em linha recta sobre chao plano.
 *   TC-MOVE-003 — sweep horizontal contra parede produz slide.
 *   TC-MOVE-004 — sobe rampa walkable (~30 graus).
 *   TC-MOVE-005 — rampa ingreme (~70 graus) bloqueia movimento.
 *   TC-MOVE-006 — step up sobre degrau de 30 cm sucede.
 *   TC-MOVE-007 — step up falha em degrau de 60 cm.
 *   TC-MOVE-008 — queda livre acumula velocity.Z.
 *   TC-MOVE-009 — Jump sobe e cai (toca chao de novo).
 *   TC-MOVE-010 — capsula sem chao continua a cair (sanity).
 */
class MovementTests
{
public:
    static MovementTestReport RunAll(Zeus::Collision::CollisionWorld& world);
};
} // namespace Zeus::Game::Movement
