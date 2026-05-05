#pragma once

#include "Quaternion.hpp"
#include "Transform.hpp"
#include "Vector3.hpp"

#if defined(ZEUS_HAS_JOLT) && ZEUS_HAS_JOLT

#include <Jolt/Jolt.h>
#include <Jolt/Math/Quat.h>
#include <Jolt/Math/Real.h>
#include <Jolt/Math/Vec3.h>

namespace Zeus::Collision
{
/**
 * Conversão exclusiva entre o mundo do Zeus (centímetros, double) e o mundo do
 * Jolt (metros, float). NÃO replicar estas regras em ZeusMath/ZeusWorld; toda
 * conversão acontece aqui na borda.
 *
 * Regras oficiais:
 *   ZeusToJolt = cm * 0.01
 *   JoltToZeus = m  * 100.0
 */
inline constexpr double CmToMeters = 0.01;
inline constexpr double MetersToCm = 100.0;

inline JPH::Vec3 ToJoltVec3(const Math::Vector3& cm)
{
    return JPH::Vec3(static_cast<float>(cm.X * CmToMeters),
                     static_cast<float>(cm.Y * CmToMeters),
                     static_cast<float>(cm.Z * CmToMeters));
}

inline JPH::RVec3 ToJoltPosition(const Math::Vector3& cm)
{
    return JPH::RVec3(static_cast<JPH::Real>(cm.X * CmToMeters),
                      static_cast<JPH::Real>(cm.Y * CmToMeters),
                      static_cast<JPH::Real>(cm.Z * CmToMeters));
}

inline Math::Vector3 FromJoltVec3(const JPH::Vec3& meters)
{
    return Math::Vector3(static_cast<double>(meters.GetX()) * MetersToCm,
                         static_cast<double>(meters.GetY()) * MetersToCm,
                         static_cast<double>(meters.GetZ()) * MetersToCm);
}

inline Math::Vector3 FromJoltPosition(const JPH::RVec3& meters)
{
    return Math::Vector3(static_cast<double>(meters.GetX()) * MetersToCm,
                         static_cast<double>(meters.GetY()) * MetersToCm,
                         static_cast<double>(meters.GetZ()) * MetersToCm);
}

inline JPH::Quat ToJoltQuat(const Math::Quaternion& q)
{
    return JPH::Quat(static_cast<float>(q.X),
                     static_cast<float>(q.Y),
                     static_cast<float>(q.Z),
                     static_cast<float>(q.W));
}

inline Math::Quaternion FromJoltQuat(const JPH::Quat& q)
{
    return Math::Quaternion(static_cast<double>(q.GetX()),
                            static_cast<double>(q.GetY()),
                            static_cast<double>(q.GetZ()),
                            static_cast<double>(q.GetW()));
}
} // namespace Zeus::Collision

#else  // ZEUS_HAS_JOLT

namespace Zeus::Collision
{
inline constexpr double CmToMeters = 0.01;
inline constexpr double MetersToCm = 100.0;
}

#endif // ZEUS_HAS_JOLT
