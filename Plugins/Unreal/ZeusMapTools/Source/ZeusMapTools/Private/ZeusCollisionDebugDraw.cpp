// Copyright Zeus Server Engine. All rights reserved.

#include "ZeusCollisionDebugDraw.h"

#include "ZeusCollisionExportTypes.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

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

	const FTransform Combined = Shape.LocalTransform * Entity.EntityWorldTransform;
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
		for (int32 i = 0; i + 1 < Shape.Convex.Vertices.Num(); ++i)
		{
			const FVector A = Combined.TransformPosition(Shape.Convex.Vertices[i]);
			const FVector B = Combined.TransformPosition(Shape.Convex.Vertices[i + 1]);
			DrawDebugLine(World, A, B, Color, false, Duration, 0, Thickness);
		}
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
