// Copyright Zeus Server Engine. All rights reserved.

#include "ZeusCollisionDebugDraw.h"

#include "ZeusCollisionExportTypes.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

namespace ZeusCollisionDebugDrawPrivate
{
/** Centimetres; planar / half-space tests for hull-edge detection. */
constexpr float PlanarEpsilonCm = 1.0f;
/** Centimetres; matching exported convex corners to an axis-aligned box. */
constexpr float CornerEpsilonCm = 2.0f;

/**
 * If the eight local vertices are exactly the corners of their axis-aligned
 * bounding box, draw a single debug box (full wireframe). Covers common
 * LevelPrototyping cubes without relying on vertex order.
 */
static bool TryDrawConvexAsAxisAlignedBox(UWorld* World, const FTransform& Combined,
	const TArray<FVector>& LocalVerts, const FColor& Color, float Duration, float Thickness)
{
	if (!World || LocalVerts.Num() != 8)
	{
		return false;
	}

	FVector Min = LocalVerts[0];
	FVector Max = LocalVerts[0];
	for (int32 i = 1; i < 8; ++i)
	{
		Min = Min.ComponentMin(LocalVerts[i]);
		Max = Max.ComponentMax(LocalVerts[i]);
	}

	const FVector Extent = Max - Min;
	if (Extent.X < KINDA_SMALL_NUMBER || Extent.Y < KINDA_SMALL_NUMBER || Extent.Z < KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const FVector Corners[8] = {
		FVector(Min.X, Min.Y, Min.Z),
		FVector(Max.X, Min.Y, Min.Z),
		FVector(Min.X, Max.Y, Min.Z),
		FVector(Max.X, Max.Y, Min.Z),
		FVector(Min.X, Min.Y, Max.Z),
		FVector(Max.X, Min.Y, Max.Z),
		FVector(Min.X, Max.Y, Max.Z),
		FVector(Max.X, Max.Y, Max.Z),
	};

	bool bUsed[8] = {};
	for (const FVector& V : LocalVerts)
	{
		bool bMatched = false;
		for (int32 c = 0; c < 8; ++c)
		{
			if (bUsed[c])
			{
				continue;
			}
			if (V.Equals(Corners[c], CornerEpsilonCm))
			{
				bUsed[c] = true;
				bMatched = true;
				break;
			}
		}
		if (!bMatched)
		{
			return false;
		}
	}

	const FVector LocalCenter = (Min + Max) * 0.5f;
	const FVector HalfExtents = Extent * 0.5f;
	DrawDebugBox(World, Combined.TransformPosition(LocalCenter), HalfExtents, Combined.GetRotation(),
		Color, false, Duration, 0, Thickness);
	return true;
}

static FVector CanonicalizeHullNormal(FVector N)
{
	if (N.X < -KINDA_SMALL_NUMBER)
	{
		return -N;
	}
	if (FMath::Abs(N.X) <= KINDA_SMALL_NUMBER && N.Y < -KINDA_SMALL_NUMBER)
	{
		return -N;
	}
	if (FMath::Abs(N.X) <= KINDA_SMALL_NUMBER && FMath::Abs(N.Y) <= KINDA_SMALL_NUMBER
		&& N.Z < -KINDA_SMALL_NUMBER)
	{
		return -N;
	}
	return N;
}

/**
 * Draw all segments that are true edges of the 3D convex hull (two distinct
 * supporting planes through the segment). Vertex count is small for editor
 * exports, so O(n^4) is acceptable.
 */
static void DrawConvexHullEdgesSupportingPlanes(UWorld* World, const TArray<FVector>& V,
	const FColor& Color, float Duration, float Thickness)
{
	const int32 N = V.Num();
	if (!World || N < 2)
	{
		return;
	}

	for (int32 i = 0; i < N; ++i)
	{
		for (int32 j = i + 1; j < N; ++j)
		{
			const FVector& Vi = V[i];
			const FVector& Vj = V[j];
			const FVector E = Vj - Vi;
			if (E.SizeSquared() < FMath::Square(0.01f))
			{
				continue;
			}

			TArray<FVector, TInlineAllocator<8>> DistinctNormals;
			for (int32 k = 0; k < N; ++k)
			{
				if (k == i || k == j)
				{
					continue;
				}

				FVector Nvec = FVector::CrossProduct(E, V[k] - Vi);
				const float Nlen2 = Nvec.SizeSquared();
				if (Nlen2 < FMath::Square(0.01f))
				{
					continue;
				}
				Nvec.Normalize();

				bool bAllNonNeg = true;
				bool bAllNonPos = true;
				for (int32 m = 0; m < N; ++m)
				{
					const float d = FVector::DotProduct(Nvec, V[m] - Vi);
					if (d < -PlanarEpsilonCm)
					{
						bAllNonNeg = false;
					}
					if (d > PlanarEpsilonCm)
					{
						bAllNonPos = false;
					}
				}
				if (!bAllNonNeg && !bAllNonPos)
				{
					continue;
				}

				Nvec = CanonicalizeHullNormal(Nvec);
				bool bDup = false;
				for (const FVector& Existing : DistinctNormals)
				{
					if (FMath::Abs(FVector::DotProduct(Nvec, Existing)) > 0.995f)
					{
						bDup = true;
						break;
					}
				}
				if (!bDup)
				{
					DistinctNormals.Add(Nvec);
				}
			}

			if (DistinctNormals.Num() < 2)
			{
				continue;
			}

			DrawDebugLine(World, Vi, Vj, Color, false, Duration, 0, Thickness);
		}
	}
}
} // namespace ZeusCollisionDebugDrawPrivate

float FZeusCollisionDebugDraw::Duration = 12.0f;

void FZeusCollisionDebugDraw::DrawResult(UWorld* World, const FZeusExportResult& Result)
{
	if (!World)
	{
		return;
	}
	for (const FZeusEntityExport& Entity : Result.Entities)
	{
		DrawEntity(World, Entity);
	}
}

void FZeusCollisionDebugDraw::DrawEntity(UWorld* World, const FZeusEntityExport& Entity)
{
	if (!World)
	{
		return;
	}

	DrawDebugBox(World, Entity.BoundsCenter, Entity.BoundsExtent, FQuat::Identity,
		FColor::Yellow, false, Duration, 0, 1.5f);

	if (Entity.Shapes.Num() == 0)
	{
		DrawDebugBox(World, Entity.BoundsCenter, Entity.BoundsExtent, FQuat::Identity,
			FColor::Red, false, Duration, 0, 2.5f);
		return;
	}

	for (const FZeusShapeExport& Shape : Entity.Shapes)
	{
		DrawShape(World, Entity, Shape);
	}

	DrawRegion(World, Entity);
}

void FZeusCollisionDebugDraw::DrawShape(UWorld* World, const FZeusEntityExport& Entity, const FZeusShapeExport& Shape)
{
	if (!World)
	{
		return;
	}

	// Unreal: outer * inner applies inner first — same as Entity.Transform(Shape.Transform(v)).
	const FTransform Combined = Entity.EntityWorldTransform * Shape.LocalTransform;
	const FVector Position = Combined.GetLocation();
	const FQuat Rotation = Combined.GetRotation();

	const FColor Color = Shape.Warnings.Num() > 0 ? FColor::Yellow : FColor::Green;
	constexpr float Thickness = 2.0f;

	switch (Shape.Type)
	{
	case EZeusCollisionShapeType::Box:
	{
		DrawDebugBox(World, Position, Shape.Box.HalfExtents, Rotation,
			Color, false, Duration, 0, Thickness);
		break;
	}
	case EZeusCollisionShapeType::Sphere:
	{
		DrawDebugSphere(World, Position, static_cast<float>(Shape.Sphere.Radius), 16,
			Color, false, Duration, 0, Thickness);
		break;
	}
	case EZeusCollisionShapeType::Capsule:
	{
		const float Radius = static_cast<float>(Shape.Capsule.Radius);
		const float HalfHeight = static_cast<float>(Shape.Capsule.HalfHeight);
		DrawDebugCapsule(World, Position, HalfHeight, Radius, Rotation,
			Color, false, Duration, 0, Thickness);
		break;
	}
	case EZeusCollisionShapeType::Convex:
	{
		using namespace ZeusCollisionDebugDrawPrivate;
		const TArray<FVector>& LocalVerts = Shape.Convex.Vertices;
		if (TryDrawConvexAsAxisAlignedBox(World, Combined, LocalVerts, Color, Duration, Thickness))
		{
			break;
		}
		TArray<FVector> WorldVerts;
		WorldVerts.Reserve(LocalVerts.Num());
		for (const FVector& Lv : LocalVerts)
		{
			WorldVerts.Add(Combined.TransformPosition(Lv));
		}
		DrawConvexHullEdgesSupportingPlanes(World, WorldVerts, Color, Duration, Thickness);
		break;
	}
	default:
		break;
	}
}

void FZeusCollisionDebugDraw::DrawRegion(UWorld* World, const FZeusEntityExport& Entity)
{
	if (!World)
	{
		return;
	}
	const double Size = Entity.Region.RegionSizeCm > 0.0 ? Entity.Region.RegionSizeCm : 5000.0;
	const FVector RegionCenter(
		(Entity.Region.GridX + 0.5) * Size,
		(Entity.Region.GridY + 0.5) * Size,
		(Entity.Region.GridZ + 0.5) * Size);
	const FVector RegionExtent(Size * 0.5, Size * 0.5, Size * 0.5);
	DrawDebugBox(World, RegionCenter, RegionExtent, FQuat::Identity,
		FColor::Blue, false, Duration, 0, 1.0f);
}
