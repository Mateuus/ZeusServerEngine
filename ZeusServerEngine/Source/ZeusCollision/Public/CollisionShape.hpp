#pragma once

#include "CollisionShapeType.hpp"
#include "Transform.hpp"
#include "Vector3.hpp"

#include <vector>

namespace Zeus::Collision
{
/** Dados específicos de uma Box (half extents em centímetros). */
struct BoxShapeData
{
    Math::Vector3 HalfExtents = Math::Vector3::Zero;
};

/** Dados específicos de uma Sphere (raio em centímetros). */
struct SphereShapeData
{
    double Radius = 0.0;
};

/** Dados específicos de uma Capsule (raio + meia-altura em centímetros). */
struct CapsuleShapeData
{
    double Radius = 0.0;
    double HalfHeight = 0.0;
};

/** Vértices locais (centímetros) usados para gerar um Convex Hull. */
struct ConvexShapeData
{
    std::vector<Math::Vector3> Vertices;
};

/**
 * Shape simples carregado do .zsm. Não conhece Jolt nem dependências externas.
 * O transform local é relativo à `WorldTransform` da entidade dona; o transform
 * mundial final do shape é `EntityWorldTransform * LocalTransform`.
 */
struct CollisionShape
{
    ECollisionShapeType Type = ECollisionShapeType::Unknown;
    Math::Transform LocalTransform = Math::Transform::Identity;

    BoxShapeData Box;
    SphereShapeData Sphere;
    CapsuleShapeData Capsule;
    ConvexShapeData Convex;
};
} // namespace Zeus::Collision
