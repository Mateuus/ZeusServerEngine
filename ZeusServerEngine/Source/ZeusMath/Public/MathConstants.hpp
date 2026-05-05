#pragma once

namespace Zeus::Math
{
/** Pi. */
inline constexpr double Pi = 3.14159265358979323846;

inline constexpr double DegToRad = Pi / 180.0;
inline constexpr double RadToDeg = 180.0 / Pi;

/**
 * Gravidade Zeus futura (alinhada à Unreal, unidades em cm/s²).
 * Não aplicada pelo motor nesta fase; apenas constante de referência.
 */
inline constexpr double DefaultGravityZ = -980.0;

/** Ângulo máximo (em graus) considerado caminhável pelo CharacterController. */
inline constexpr double DefaultMaxSlopeAngleDeg = 45.0;

/** Altura máxima de degrau em centímetros (referência). */
inline constexpr double DefaultStepHeightCm = 45.0;

/** Raio padrão da capsule do player em centímetros (referência). */
inline constexpr double DefaultCapsuleRadiusCm = 35.0;

/** Altura padrão da capsule do player em centímetros (referência). */
inline constexpr double DefaultCapsuleHeightCm = 180.0;
} // namespace Zeus::Math
