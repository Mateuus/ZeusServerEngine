#pragma once

#include "CollisionShape.hpp"
#include "TerrainCollisionAsset.hpp"

#if defined(ZEUS_HAS_JOLT) && ZEUS_HAS_JOLT

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

namespace Zeus::Collision
{
/**
 * Conversão `CollisionShape -> JPH::Shape`. A unidade de Jolt é metros, por
 * isso a factory aplica `cm * 0.01` em todas as dimensões (raio, half-extents,
 * meias-alturas).
 *
 * O resultado é um `JPH::ShapeRefC` reference-counted; o caller é responsável
 * por mantê-lo vivo enquanto houver bodies que o usem.
 */
class JoltShapeFactory
{
public:
    /** Devolve um shape Jolt válido ou `nullptr` se não for possível converter. */
    static JPH::ShapeRefC CreateShape(const CollisionShape& shape);

    /** TriangleMesh em metros (ja com scale aplicado por vertice). */
    static JPH::ShapeRefC CreateMeshShape(const TerrainPiece& piece);

    /** HeightField regular grid; height samples ja em metros. */
    static JPH::ShapeRefC CreateHeightFieldShape(const TerrainPiece& piece);
};
} // namespace Zeus::Collision

#endif // ZEUS_HAS_JOLT
