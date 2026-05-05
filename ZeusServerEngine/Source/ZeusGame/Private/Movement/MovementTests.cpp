#include "Movement/MovementTests.hpp"

#include "Entities/CharacterActor.hpp"
#include "Movement/MovementComponent.hpp"

#include "CollisionAsset.hpp"
#include "CollisionTestScene.hpp"
#include "CollisionWorld.hpp"
#include "MathConstants.hpp"
#include "Quaternion.hpp"
#include "SpawnParameters.hpp"
#include "Transform.hpp"
#include "Vector3.hpp"
#include "World.hpp"
#include "ZeusLog.hpp"

#include <cmath>
#include <sstream>

namespace Zeus::Game::Movement
{
namespace
{
void Record(MovementTestReport& report, std::string name, bool bPassed, std::string detail = {})
{
    MovementTestResult r;
    r.Name = std::move(name);
    r.bPassed = bPassed;
    r.Detail = std::move(detail);
    if (r.bPassed)
    {
        ++report.PassedCount;
        ZeusLog::Info("MovementTest",
            std::string("[MovementTest] ").append(r.Name).append(": OK ").append(r.Detail));
    }
    else
    {
        ++report.FailedCount;
        ZeusLog::Warning("MovementTest",
            std::string("[MovementTest] ").append(r.Name).append(": FAIL ").append(r.Detail));
    }
    report.Scenarios.push_back(std::move(r));
}

Zeus::Collision::CollisionShape MakeBox(double hx, double hy, double hz)
{
    Zeus::Collision::CollisionShape s;
    s.Type = Zeus::Collision::ECollisionShapeType::Box;
    s.LocalTransform = Zeus::Math::Transform::Identity;
    s.Box.HalfExtents = Zeus::Math::Vector3(hx, hy, hz);
    return s;
}

Zeus::Collision::CollisionEntity MakeEntity(const std::string& name,
    const Zeus::Math::Vector3& location, const Zeus::Math::Quaternion& rotation,
    Zeus::Collision::CollisionShape shape)
{
    Zeus::Collision::CollisionEntity e;
    e.Name = name;
    e.ActorName = name + "_Actor";
    e.ComponentName = "StaticMeshComponent0";
    e.WorldTransform = Zeus::Math::Transform(location, rotation, Zeus::Math::Vector3::One);
    e.BoundsCenter = location;
    e.BoundsExtent = Zeus::Math::Vector3(500.0, 500.0, 500.0);
    e.Region.RegionSizeCm = 5000.0;
    e.Shapes.push_back(std::move(shape));
    return e;
}

/** Estende `BuildProgrammaticAsset` com degraus de 30 cm e 60 cm. */
Zeus::Collision::CollisionAsset BuildMovementAsset()
{
    Zeus::Collision::CollisionAsset asset = Zeus::Collision::CollisionTestScene::BuildProgrammaticAsset();

    // StepSmall: top em z=30 (degrau de 25 cm sobre o floor cuja top esta em z=5).
    asset.Entities.push_back(MakeEntity("StepSmall",
        Zeus::Math::Vector3(700.0, 500.0, 15.0), Zeus::Math::Quaternion::Identity,
        MakeBox(200.0, 200.0, 15.0)));
    // StepBig: top em z=70 (degrau de 65 cm > StepHeight 45 cm).
    asset.Entities.push_back(MakeEntity("StepBig",
        Zeus::Math::Vector3(700.0, -500.0, 35.0), Zeus::Math::Quaternion::Identity,
        MakeBox(200.0, 200.0, 35.0)));

    auto computeRegion = [](const Zeus::Math::Vector3& center, double regionSize) -> Zeus::Collision::EntityRegion {
        Zeus::Collision::EntityRegion R;
        R.RegionSizeCm = regionSize;
        R.GridX = static_cast<std::int32_t>(std::floor(center.X / regionSize));
        R.GridY = static_cast<std::int32_t>(std::floor(center.Y / regionSize));
        R.GridZ = static_cast<std::int32_t>(std::floor(center.Z / regionSize));
        const std::uint32_t Hx = static_cast<std::uint32_t>(R.GridX) * 73856093u;
        const std::uint32_t Hy = static_cast<std::uint32_t>(R.GridY) * 19349663u;
        const std::uint32_t Hz = static_cast<std::uint32_t>(R.GridZ) * 83492791u;
        R.RegionId = (Hx ^ Hy ^ Hz);
        return R;
    };
    for (auto& e : asset.Entities)
    {
        e.Region = computeRegion(e.BoundsCenter, asset.RegionSizeCm);
    }
    asset.Stats.EntityCount = asset.Entities.size();
    asset.Stats.ShapeCount = asset.Entities.size();
    asset.Stats.BoxCount = asset.Entities.size();
    Zeus::Collision::RebuildEntityIndexByRegion(asset);
    return asset;
}

bool IsWalkableLocal(const Zeus::Math::Vector3& normal, double maxDeg)
{
    if (normal.LengthSquared() < 1e-12)
    {
        return false;
    }
    return normal.Z >= std::cos(maxDeg * Zeus::Math::DegToRad);
}

Zeus::Game::Entities::CharacterActor* SpawnFreshCharacter(Zeus::World::World& world,
    Zeus::Collision::CollisionWorld& cw,
    const Zeus::Math::Vector3& location)
{
    Zeus::World::SpawnParameters params;
    params.Name = "TestCharacter";
    params.Transform.Location = location;
    params.bAllowTick = true;
    params.bStartActive = true;
    auto* actor = world.SpawnActorTyped<Zeus::Game::Entities::CharacterActor>(params, true);
    if (actor != nullptr && actor->GetMovementComponent() != nullptr)
    {
        actor->GetMovementComponent()->SetCollisionWorld(&cw);
    }
    return actor;
}

void TickActor(Zeus::Game::Entities::CharacterActor& actor, double dt, int frames)
{
    for (int i = 0; i < frames; ++i)
    {
        actor.TickActorAndComponents(dt);
    }
}
} // namespace

MovementTestReport MovementTests::RunAll(Zeus::Collision::CollisionWorld& /*unused*/)
{
    MovementTestReport report;

    Zeus::Collision::CollisionWorld cw;
    if (!cw.LoadFromAsset(BuildMovementAsset()))
    {
        Record(report, "Setup MovementAsset", false);
        return report;
    }
    if (!cw.BuildPhysicsWorld())
    {
        Record(report, "Setup PhysicsWorld", false);
        return report;
    }

    Zeus::World::World world;
    world.Initialize("MovementTestWorld");
    world.BeginPlay();

    constexpr double dt = 1.0 / 30.0;

    // --- TC-MOVE-001 ---------------------------------------------------------
    {
        auto* a = SpawnFreshCharacter(world, cw, Zeus::Math::Vector3(0.0, 0.0, 200.0));
        if (a == nullptr || a->GetMovementComponent() == nullptr)
        {
            Record(report, "TC-MOVE-001 Capsule settles on floor", false, "(spawn failed)");
        }
        else
        {
            TickActor(*a, dt, 60); // 2 segundos
            const Zeus::Math::Vector3 p = a->GetLocation();
            const bool grounded = a->GetMovementComponent()->IsGrounded();
            const bool zOk = std::abs(p.Z - 95.0) < 5.0;
            std::ostringstream det;
            det << "(grounded=" << grounded << " z=" << p.Z << ")";
            Record(report, "TC-MOVE-001 Capsule settles on floor", grounded && zOk, det.str());
            a->Destroy();
        }
    }

    // --- TC-MOVE-002 ---------------------------------------------------------
    {
        auto* a = SpawnFreshCharacter(world, cw, Zeus::Math::Vector3(0.0, 0.0, 96.0));
        if (a == nullptr || a->GetMovementComponent() == nullptr)
        {
            Record(report, "TC-MOVE-002 Walk straight on flat floor", false, "(spawn failed)");
        }
        else
        {
            // Settle 1 frame
            a->TickActorAndComponents(dt);
            a->GetMovementComponent()->MoveByAxis(1.0, 0.0);
            TickActor(*a, dt, 30); // 1 segundo
            const Zeus::Math::Vector3 p = a->GetLocation();
            const bool xOk = p.X > 400.0 && p.X < 700.0;
            const bool yOk = std::abs(p.Y) < 1.0;
            const bool zOk = std::abs(p.Z - 95.0) < 3.0;
            const bool grounded = a->GetMovementComponent()->IsGrounded();
            std::ostringstream det;
            det << "(x=" << p.X << " y=" << p.Y << " z=" << p.Z << " grounded=" << grounded << ")";
            Record(report, "TC-MOVE-002 Walk straight on flat floor",
                xOk && yOk && zOk && grounded, det.str());
            a->Destroy();
        }
    }

    // --- TC-MOVE-003 ---------------------------------------------------------
    {
        auto* a = SpawnFreshCharacter(world, cw, Zeus::Math::Vector3(400.0, 0.0, 96.0));
        if (a == nullptr || a->GetMovementComponent() == nullptr)
        {
            Record(report, "TC-MOVE-003 Sweep against wall slides", false, "(spawn failed)");
        }
        else
        {
            a->TickActorAndComponents(dt);
            a->GetMovementComponent()->MoveByAxis(1.0, 0.0);
            TickActor(*a, dt, 60);
            const Zeus::Math::Vector3 p = a->GetLocation();
            const bool blocked = p.X < 475.0; // capsule radius 35 + wall front face at x=475
            const bool yClose = std::abs(p.Y) < 5.0;
            const bool zOk = std::abs(p.Z - 95.0) < 3.0;
            std::ostringstream det;
            det << "(x=" << p.X << " y=" << p.Y << " z=" << p.Z << ")";
            Record(report, "TC-MOVE-003 Sweep against wall slides",
                blocked && yClose && zOk, det.str());
            a->Destroy();
        }
    }

    // --- TC-MOVE-004 ---------------------------------------------------------
    {
        // Settle sobre a rampa walkable (~30 graus) — verificar grounded e walkable normal.
        auto* a = SpawnFreshCharacter(world, cw, Zeus::Math::Vector3(-500.0, 200.0, 400.0));
        if (a == nullptr || a->GetMovementComponent() == nullptr)
        {
            Record(report, "TC-MOVE-004 Settle on walkable ramp", false, "(spawn failed)");
        }
        else
        {
            TickActor(*a, dt, 90); // 3s — tempo para assentar.
            const auto& n = a->GetMovementComponent()->GetFloorNormal();
            const bool grounded = a->GetMovementComponent()->IsGrounded();
            const bool walkable = grounded
                && IsWalkableLocal(n, a->GetMovementComponent()->GetParameters().MaxSlopeAngleDeg);
            std::ostringstream det;
            det << "(grounded=" << grounded << " normal=" << n.ToString() << ")";
            Record(report, "TC-MOVE-004 Settle on walkable ramp", walkable, det.str());
            a->Destroy();
        }
    }

    // --- TC-MOVE-005 ---------------------------------------------------------
    {
        // Settle perto da rampa ingreme: deve eventualmente cair para o floor (z~95)
        // pois a rampa ~70 graus nao e walkable e nao snapa la.
        auto* a = SpawnFreshCharacter(world, cw, Zeus::Math::Vector3(-500.0, -200.0, 600.0));
        if (a == nullptr || a->GetMovementComponent() == nullptr)
        {
            Record(report, "TC-MOVE-005 Steep ramp blocks ground snap", false, "(spawn failed)");
        }
        else
        {
            TickActor(*a, dt, 120); // 4s
            const auto& n = a->GetMovementComponent()->GetFloorNormal();
            const bool grounded = a->GetMovementComponent()->IsGrounded();
            // A rampa ingreme nao da grounded; eventualmente assenta no floor da entidade Floor (z~95).
            const Zeus::Math::Vector3 p = a->GetLocation();
            const bool floorReached = grounded && std::abs(p.Z - 95.0) < 10.0
                && IsWalkableLocal(n, a->GetMovementComponent()->GetParameters().MaxSlopeAngleDeg);
            std::ostringstream det;
            det << "(z=" << p.Z << " grounded=" << grounded << " normal=" << n.ToString() << ")";
            Record(report, "TC-MOVE-005 Steep ramp blocks ground snap", floorReached, det.str());
            a->Destroy();
        }
    }

    // --- TC-MOVE-006 ---------------------------------------------------------
    {
        // Step up 30 cm (StepSmall em (700, 500, 15) — top z=30, degrau ~25 cm).
        auto* a = SpawnFreshCharacter(world, cw, Zeus::Math::Vector3(450.0, 500.0, 96.0));
        if (a == nullptr || a->GetMovementComponent() == nullptr)
        {
            Record(report, "TC-MOVE-006 Step up over 30 cm", false, "(spawn failed)");
        }
        else
        {
            a->TickActorAndComponents(dt);
            a->GetMovementComponent()->MoveByAxis(1.0, 0.0);
            TickActor(*a, dt, 90); // 3s — tempo para subir.
            const Zeus::Math::Vector3 p = a->GetLocation();
            // O top do step pequeno esta em z=30; capsula ali fica em z = 30 + 90 = 120.
            const bool zOk = p.Z > 110.0 && p.Z < 135.0;
            const bool xOk = p.X > 500.0;
            std::ostringstream det;
            det << "(x=" << p.X << " z=" << p.Z << ")";
            Record(report, "TC-MOVE-006 Step up over 30 cm", xOk && zOk, det.str());
            a->Destroy();
        }
    }

    // --- TC-MOVE-007 ---------------------------------------------------------
    {
        // StepBig em (700, -500, 35) — top z=70, degrau ~65 cm > StepHeight 45 cm.
        auto* a = SpawnFreshCharacter(world, cw, Zeus::Math::Vector3(450.0, -500.0, 96.0));
        if (a == nullptr || a->GetMovementComponent() == nullptr)
        {
            Record(report, "TC-MOVE-007 Step up fails over 60 cm", false, "(spawn failed)");
        }
        else
        {
            a->TickActorAndComponents(dt);
            a->GetMovementComponent()->MoveByAxis(1.0, 0.0);
            TickActor(*a, dt, 60);
            const Zeus::Math::Vector3 p = a->GetLocation();
            const bool zStays = std::abs(p.Z - 95.0) < 6.0;
            // Bloqueado em x ~ 500-200-radius-skin = 300-35-0.5 ~= 264.
            const bool xBlocked = p.X < 470.0;
            std::ostringstream det;
            det << "(x=" << p.X << " z=" << p.Z << ")";
            Record(report, "TC-MOVE-007 Step up fails over 60 cm", xBlocked && zStays, det.str());
            a->Destroy();
        }
    }

    // --- TC-MOVE-008 ---------------------------------------------------------
    {
        // Queda livre — capsula bem alta sem chao por debaixo (na verdade sobre o floor,
        // mas com gap suficiente para acumular velocity.Z negativa antes de tocar).
        auto* a = SpawnFreshCharacter(world, cw, Zeus::Math::Vector3(0.0, 0.0, 1500.0));
        if (a == nullptr || a->GetMovementComponent() == nullptr)
        {
            Record(report, "TC-MOVE-008 Free fall accumulates velocity", false, "(spawn failed)");
        }
        else
        {
            TickActor(*a, dt, 5); // 5 frames — pouco para chegar ao chao.
            const auto* mc = a->GetMovementComponent();
            const bool fallVelocity = mc->GetVelocity().Z < -100.0;
            const bool airborne = !mc->IsGrounded();
            const Zeus::Math::Vector3 p = a->GetLocation();
            const bool descended = p.Z < 1500.0;
            std::ostringstream det;
            det << "(vz=" << mc->GetVelocity().Z << " z=" << p.Z << " grounded=" << mc->IsGrounded() << ")";
            Record(report, "TC-MOVE-008 Free fall accumulates velocity",
                fallVelocity && airborne && descended, det.str());
            a->Destroy();
        }
    }

    // --- TC-MOVE-009 ---------------------------------------------------------
    {
        auto* a = SpawnFreshCharacter(world, cw, Zeus::Math::Vector3(0.0, 0.0, 96.0));
        if (a == nullptr || a->GetMovementComponent() == nullptr)
        {
            Record(report, "TC-MOVE-009 Jump goes up and lands", false, "(spawn failed)");
        }
        else
        {
            a->TickActorAndComponents(dt); // settle
            const bool initiallyGrounded = a->GetMovementComponent()->IsGrounded();
            a->GetMovementComponent()->Jump();
            // Tick uns frames para apanhar o pico
            TickActor(*a, dt, 10);
            const double zAtPeak = a->GetLocation().Z;
            const bool wentUp = zAtPeak > 100.0;
            // Tick mais para aterrar
            TickActor(*a, dt, 90);
            const bool grounded = a->GetMovementComponent()->IsGrounded();
            const Zeus::Math::Vector3 p = a->GetLocation();
            const bool landed = grounded && std::abs(p.Z - 95.0) < 5.0;
            std::ostringstream det;
            det << "(initial=" << initiallyGrounded << " peakZ=" << zAtPeak
                << " final=(z=" << p.Z << " grounded=" << grounded << "))";
            Record(report, "TC-MOVE-009 Jump goes up and lands",
                initiallyGrounded && wentUp && landed, det.str());
            a->Destroy();
        }
    }

    // --- TC-MOVE-010 ---------------------------------------------------------
    {
        // Spawn fora do floor (HalfX=1000, HalfY=1000); (3000, 3000, 200) nao tem chao por debaixo.
        auto* a = SpawnFreshCharacter(world, cw, Zeus::Math::Vector3(3000.0, 3000.0, 200.0));
        if (a == nullptr || a->GetMovementComponent() == nullptr)
        {
            Record(report, "TC-MOVE-010 Capsule keeps falling without floor", false, "(spawn failed)");
        }
        else
        {
            TickActor(*a, dt, 30); // 1 segundo
            const auto* mc = a->GetMovementComponent();
            const bool airborne = !mc->IsGrounded();
            const bool falling = mc->GetVelocity().Z < -50.0;
            const Zeus::Math::Vector3 p = a->GetLocation();
            std::ostringstream det;
            det << "(z=" << p.Z << " vz=" << mc->GetVelocity().Z << " grounded=" << mc->IsGrounded() << ")";
            Record(report, "TC-MOVE-010 Capsule keeps falling without floor",
                airborne && falling, det.str());
            a->Destroy();
        }
    }

    world.Shutdown();
    cw.Shutdown();

    std::ostringstream summary;
    summary << "[MovementTest] Summary passed=" << report.PassedCount
            << " failed=" << report.FailedCount;
    if (report.FailedCount == 0)
    {
        ZeusLog::Info("MovementTest", summary.str());
    }
    else
    {
        ZeusLog::Warning("MovementTest", summary.str());
    }
    return report;
}
} // namespace Zeus::Game::Movement
