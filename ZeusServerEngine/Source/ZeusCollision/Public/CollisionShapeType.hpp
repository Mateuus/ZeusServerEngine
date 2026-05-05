#pragma once

#include <cstdint>

namespace Zeus::Collision
{
/**
 * Tipos oficiais de shape simples exportados pela Unreal e usados pelo servidor.
 * O valor numérico é serializado byte-a-byte no formato ZSMC v1, por isso novos
 * tipos só podem ser adicionados após a Box=1, mantendo a ordem estável.
 */
enum class ECollisionShapeType : std::uint8_t
{
    Unknown               = 0,
    Box                   = 1,
    Sphere                = 2,
    Capsule               = 3,
    Convex                = 4,
    TriangleMesh_Reserved = 5,
};

/** Conversão segura a partir do byte serializado (devolve `Unknown` se inválido). */
inline ECollisionShapeType ShapeTypeFromByte(std::uint8_t Value)
{
    switch (Value)
    {
    case 1: return ECollisionShapeType::Box;
    case 2: return ECollisionShapeType::Sphere;
    case 3: return ECollisionShapeType::Capsule;
    case 4: return ECollisionShapeType::Convex;
    case 5: return ECollisionShapeType::TriangleMesh_Reserved;
    default: return ECollisionShapeType::Unknown;
    }
}
} // namespace Zeus::Collision
