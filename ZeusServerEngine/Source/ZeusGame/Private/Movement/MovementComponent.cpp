#include "Movement/MovementComponent.hpp"

#include "Actor.hpp"
#include "ActorTick.hpp"
#include "CollisionWorld.hpp"
#include "MathConstants.hpp"
#include "PhysicsWorld.hpp"
#include "Vector3.hpp"

#include <algorithm>
#include <cmath>

namespace Zeus::Game::Movement
{
namespace
{
constexpr double kSkinCm = 0.5;             ///< distancia minima a manter da geometria.
constexpr double kFractionEpsilon = 0.001;  ///< proximidade da fraccao para tratar como "no contact".
constexpr double kFloorProbeCm = 5.0;       ///< raycast vertical para detectar chao apos sweep.
constexpr int kMaxSlideIterations = 2;      ///< numero maximo de slides apos um hit horizontal.

/** Devolve o vector projectado num plano com a normal dada (ja unit). */
Zeus::Math::Vector3 ProjectOnPlane(const Zeus::Math::Vector3& v, const Zeus::Math::Vector3& planeNormal)
{
    const double dot = Zeus::Math::Vector3::Dot(v, planeNormal);
    return v - planeNormal * dot;
}

/** Move uma fraccao do delta deixando uma "skin" reservada. */
double ClampedFraction(double fraction, double sweepLenCm)
{
    if (sweepLenCm <= 1e-6)
    {
        return 0.0;
    }
    const double skinFraction = kSkinCm / sweepLenCm;
    return std::clamp(fraction - skinFraction, 0.0, 1.0);
}
} // namespace

MovementComponent::MovementComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
    PrimaryComponentTick.bTickEnabled = true;
}

MovementComponent::~MovementComponent() = default;

void MovementComponent::OnRegister()
{
    Zeus::World::ActorComponent::OnRegister();
    PrimaryComponentTick.bTickEnabled = PrimaryComponentTick.bStartWithTickEnabled;
}

void MovementComponent::MoveByAxis(double forward, double right)
{
    forward = std::clamp(forward, -1.0, 1.0);
    right = std::clamp(right, -1.0, 1.0);
    AxisInput = Zeus::Math::Vector3(forward, right, 0.0);
}

void MovementComponent::Jump()
{
    bJumpQueued = true;
}

void MovementComponent::Stop()
{
    AxisInput = Zeus::Math::Vector3::Zero;
    bJumpQueued = false;
}

void MovementComponent::TeleportTo(const Zeus::Math::Vector3& positionCm)
{
    if (Zeus::World::Actor* owner = GetOwner())
    {
        owner->SetLocation(positionCm);
    }
    Velocity = Zeus::Math::Vector3::Zero;
    bIsGrounded = false;
}

bool MovementComponent::IsWalkable(const Zeus::Math::Vector3& normal) const
{
    if (normal.LengthSquared() < 1e-12)
    {
        return false;
    }
    const double cosLimit = std::cos(Parameters.MaxSlopeAngleDeg * Zeus::Math::DegToRad);
    return normal.Z >= cosLimit;
}

bool MovementComponent::TrySweep(const Zeus::Math::Vector3& fromCm, const Zeus::Math::Vector3& deltaCm,
    Zeus::Math::Vector3& outFinalCm, Zeus::Math::Vector3& outNormal, double& outFraction, bool& outHit)
{
    outFinalCm = fromCm;
    outNormal = Zeus::Math::Vector3::Zero;
    outFraction = 1.0;
    outHit = false;

    const double sweepLen = deltaCm.Length();
    if (sweepLen <= 1e-6)
    {
        return true;
    }
    if (CollisionWorld == nullptr)
    {
        outFinalCm = fromCm + deltaCm;
        return true;
    }

    Zeus::Collision::SweepHit hit;
    ++SweepsLastTick;
    const bool any = CollisionWorld->SweepCapsule(fromCm,
        Parameters.CapsuleRadiusCm, Parameters.CapsuleHalfHeightCm, deltaCm, hit);
    if (!any)
    {
        outFinalCm = fromCm + deltaCm;
        return true;
    }

    outHit = true;
    outNormal = hit.ImpactNormal;
    outFraction = hit.Fraction;
    const double appliedFraction = ClampedFraction(hit.Fraction, sweepLen);
    outFinalCm = fromCm + deltaCm * appliedFraction;
    return true;
}

void MovementComponent::IntegrateGravity(double deltaSeconds)
{
    if (bIsGrounded && Velocity.Z <= 0.0)
    {
        Velocity.Z = 0.0;
        return;
    }
    Velocity.Z += Parameters.GravityZ * deltaSeconds;
}

void MovementComponent::StepHorizontal(double deltaSeconds)
{
    if (deltaSeconds <= 0.0)
    {
        return;
    }

    Zeus::World::Actor* owner = GetOwner();
    if (owner == nullptr)
    {
        return;
    }

    Zeus::Math::Vector3 desired(AxisInput.X * Parameters.DefaultSpeedCmS * deltaSeconds,
                                AxisInput.Y * Parameters.DefaultSpeedCmS * deltaSeconds, 0.0);
    if (desired.LengthSquared() < 1e-8)
    {
        return;
    }

    Zeus::Math::Vector3 position = owner->GetLocation();
    int iterations = 0;
    while (iterations <= kMaxSlideIterations && desired.LengthSquared() > 1e-10)
    {
        Zeus::Math::Vector3 hitNormal;
        Zeus::Math::Vector3 newPos;
        double fraction = 1.0;
        bool bHit = false;
        TrySweep(position, desired, newPos, hitNormal, fraction, bHit);
        position = newPos;

        if (!bHit)
        {
            break;
        }

        // Step up: tentar subir StepHeightCm e voltar a tentar o que falta horizontalmente.
        const bool walkable = IsWalkable(hitNormal);
        const Zeus::Math::Vector3 leftover = desired * (1.0 - fraction);
        if (!walkable && bIsGrounded && leftover.LengthSquared() > 1e-10
            && Parameters.StepHeightCm > 0.0)
        {
            const Zeus::Math::Vector3 up(0.0, 0.0, Parameters.StepHeightCm);
            Zeus::Math::Vector3 upPos;
            Zeus::Math::Vector3 upNormal;
            double upFrac = 1.0;
            bool upHit = false;
            TrySweep(position, up, upPos, upNormal, upFrac, upHit);

            Zeus::Math::Vector3 stepHorizPos;
            Zeus::Math::Vector3 stepHorizNormal;
            double stepHorizFrac = 1.0;
            bool stepHorizHit = false;
            TrySweep(upPos, leftover, stepHorizPos, stepHorizNormal, stepHorizFrac, stepHorizHit);

            const double stepDownLen = Parameters.StepHeightCm + 5.0;
            const Zeus::Math::Vector3 down(0.0, 0.0, -stepDownLen);
            Zeus::Math::Vector3 downPos;
            Zeus::Math::Vector3 downNormal;
            double downFrac = 1.0;
            bool downHit = false;
            TrySweep(stepHorizPos, down, downPos, downNormal, downFrac, downHit);

            if (downHit && IsWalkable(downNormal) && downPos.Z >= position.Z - 1.0)
            {
                position = downPos;
                FloorNormal = downNormal;
                bIsGrounded = true;
                desired = Zeus::Math::Vector3::Zero;
                break;
            }
        }

        // Slide: projecta o leftover no plano da normal.
        if (leftover.LengthSquared() < 1e-10)
        {
            break;
        }
        Zeus::Math::Vector3 slide = ProjectOnPlane(leftover, hitNormal);
        if (slide.LengthSquared() < 1e-10)
        {
            break;
        }
        desired = slide;
        ++iterations;
    }

    owner->SetLocation(position);
}

void MovementComponent::StepVertical(double deltaSeconds)
{
    if (deltaSeconds <= 0.0)
    {
        return;
    }
    Zeus::World::Actor* owner = GetOwner();
    if (owner == nullptr)
    {
        return;
    }

    if (bJumpQueued && bIsGrounded)
    {
        Velocity.Z = Parameters.JumpVelocityCmS;
        bIsGrounded = false;
    }
    bJumpQueued = false;

    Zeus::Math::Vector3 deltaV(0.0, 0.0, Velocity.Z * deltaSeconds);
    if (deltaV.LengthSquared() < 1e-10)
    {
        return;
    }

    // Substep: garante que cada sub-sweep nao excede metade do radius da
    // capsula para evitar tunneling em quedas rapidas.
    const double maxStepCm = std::max(1.0, Parameters.CapsuleRadiusCm * 0.5);
    const double totalLen = std::abs(deltaV.Z);
    const int substeps = std::max(1, static_cast<int>(std::ceil(totalLen / maxStepCm)));
    const Zeus::Math::Vector3 stepDelta = deltaV * (1.0 / static_cast<double>(substeps));

    Zeus::Math::Vector3 position = owner->GetLocation();
    bool stoppedByFloor = false;
    bool stoppedByCeiling = false;
    Zeus::Math::Vector3 contactNormal(0.0, 0.0, 1.0);

    for (int i = 0; i < substeps; ++i)
    {
        Zeus::Math::Vector3 newPos;
        Zeus::Math::Vector3 hitNormal;
        double fraction = 1.0;
        bool bHit = false;
        TrySweep(position, stepDelta, newPos, hitNormal, fraction, bHit);
        position = newPos;
        if (bHit)
        {
            const bool descending = stepDelta.Z < 0.0;
            contactNormal = hitNormal;
            if (descending && IsWalkable(hitNormal))
            {
                stoppedByFloor = true;
            }
            else if (!descending)
            {
                stoppedByCeiling = true;
            }
            else
            {
                // Hit nao walkable em descida — paramos a integracao vertical
                // mas mantemo-nos airborne (a capsula vai escorregar lateralmente
                // no proximo tick via slide horizontal).
                stoppedByFloor = false;
            }
            break;
        }
    }

    if (stoppedByFloor)
    {
        FloorNormal = contactNormal;
        bIsGrounded = true;
        Velocity.Z = 0.0;
    }
    else if (stoppedByCeiling)
    {
        Velocity.Z = 0.0;
        bIsGrounded = false;
    }
    else
    {
        bIsGrounded = false;
        // Hit em superficie nao-walkable durante descida: faz slide lateral
        // pela tangente da superficie para evitar que a capsula fique presa
        // na rampa ingreme. A direcao do slide vem da normal projetada no
        // plano XY (sempre aponta "morro abaixo" da rampa).
        if (contactNormal.LengthSquared() > 0.5 && contactNormal.Z < 0.999
            && stepDelta.Z < 0.0)
        {
            Zeus::Math::Vector3 lateralDir(contactNormal.X, contactNormal.Y, 0.0);
            if (lateralDir.LengthSquared() > 1e-6)
            {
                lateralDir = lateralDir.Normalized();
                // Magnitude do slide: 30% do |Velocity.Z| * dt como impulso
                // lateral, suficiente para a capsula escorregar para fora.
                const double slideAmount = std::abs(Velocity.Z) * deltaSeconds * 0.3;
                const Zeus::Math::Vector3 slideVec(
                    lateralDir.X * slideAmount,
                    lateralDir.Y * slideAmount, 0.0);
                Zeus::Math::Vector3 slidePos;
                Zeus::Math::Vector3 slideNormal;
                double slideFrac = 1.0;
                bool slideHit = false;
                TrySweep(position, slideVec, slidePos, slideNormal, slideFrac, slideHit);
                position = slidePos;
            }
        }
    }

    owner->SetLocation(position);

    if (CollisionWorld != nullptr && !bIsGrounded && Velocity.Z <= 0.0)
    {
        Zeus::Collision::RaycastHit floorHit;
        const Zeus::Math::Vector3 origin = position - Zeus::Math::Vector3(0.0, 0.0, Parameters.CapsuleHalfHeightCm);
        if (CollisionWorld->RaycastDown(origin, kFloorProbeCm, floorHit))
        {
            if (IsWalkable(floorHit.ImpactNormal))
            {
                FloorNormal = floorHit.ImpactNormal;
                bIsGrounded = true;
                Velocity.Z = 0.0;
            }
        }
    }
}

void MovementComponent::TickComponent(double deltaSeconds)
{
    Zeus::World::ActorComponent::TickComponent(deltaSeconds);

    SweepsLastTick = 0;
    if (deltaSeconds <= 0.0)
    {
        return;
    }

    IntegrateGravity(deltaSeconds);
    StepHorizontal(deltaSeconds);
    StepVertical(deltaSeconds);
}
} // namespace Zeus::Game::Movement
