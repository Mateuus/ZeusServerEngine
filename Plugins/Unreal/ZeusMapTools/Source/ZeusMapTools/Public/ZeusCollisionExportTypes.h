// Copyright Zeus Server Engine. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/** Type of simple collision shape, mirroring ECollisionShapeType on the server. */
enum class EZeusCollisionShapeType : uint8
{
	Box      = 1,
	Sphere   = 2,
	Capsule  = 3,
	Convex   = 4,
	TriangleMesh_Reserved = 5
};

/** Box shape data (half extents, in centimeters). */
struct FZeusBoxShape
{
	FVector HalfExtents = FVector::ZeroVector;
};

/** Sphere shape data (radius, centimeters). */
struct FZeusSphereShape
{
	double Radius = 0.0;
};

/** Capsule shape data (radius + half height, centimeters). */
struct FZeusCapsuleShape
{
	double Radius = 0.0;
	double HalfHeight = 0.0;
};

/** Convex shape data (vertices in local space, centimeters). */
struct FZeusConvexShape
{
	TArray<FVector> Vertices;
};

/** A single simple collision element exported from a UStaticMesh's BodySetup. */
struct FZeusShapeExport
{
	EZeusCollisionShapeType Type = EZeusCollisionShapeType::Box;

	/**
	 * Local transform of the shape relative to the entity's world transform.
	 * Final world transform of the shape = EntityWorldTransform * LocalTransform.
	 * Translation is in centimeters.
	 */
	FTransform LocalTransform = FTransform::Identity;

	FZeusBoxShape Box;
	FZeusSphereShape Sphere;
	FZeusCapsuleShape Capsule;
	FZeusConvexShape Convex;

	TArray<FString> Warnings;
};

/** Region/cell metadata used to prepare for World Partition / streaming. */
struct FZeusEntityRegion
{
	uint32 RegionId = 0;
	int32 GridX = 0;
	int32 GridY = 0;
	int32 GridZ = 0;
	double RegionSizeCm = 5000.0;
};

/**
 * One exported entity (a single component of a single actor). The simple
 * collision shapes always come from the source UStaticMesh's UBodySetup
 * (FKAggregateGeom), never from triangle meshes nor from the visual selection
 * overlay.
 */
struct FZeusEntityExport
{
	FString EntityName;       // typically the StaticMesh asset name (SM_Wall_01)
	FString ActorName;        // owning actor's name in the level
	FString ComponentName;    // component name (StaticMeshComponent0, etc.)
	FString SourcePath;       // /Game/.../SM_Wall_01

	/**
	 * World transform of the source UStaticMeshComponent. Translation is in
	 * centimeters (Unreal native unit).
	 */
	FTransform EntityWorldTransform = FTransform::Identity;

	/** Component world bounds: center + half-extent (axis-aligned), in centimeters. */
	FVector BoundsCenter = FVector::ZeroVector;
	FVector BoundsExtent = FVector::ZeroVector;

	FZeusEntityRegion Region;

	TArray<FZeusShapeExport> Shapes;
	TArray<FString> Warnings;
};

/** Aggregate counts for the export run. */
struct FZeusExportStats
{
	int32 EntityCount = 0;
	int32 ShapeCount = 0;
	int32 BoxCount = 0;
	int32 SphereCount = 0;
	int32 CapsuleCount = 0;
	int32 ConvexCount = 0;
	int32 WarningCount = 0;
	int32 SkippedActorCount = 0;
};

/** Final result returned by the exporter. */
struct FZeusExportResult
{
	FString MapName;
	TArray<FZeusEntityExport> Entities;
	TArray<FString> Warnings;
	FZeusExportStats Stats;
};

/** Options for an export run. */
struct FZeusExportOptions
{
	bool bIncludeSelectedOnly = false;
	bool bWriteJson = true;
	bool bWriteZsm = true;
	bool bDrawDebug = false;
	bool bValidateOnly = false;
	bool bAllowComplexAsSimple = false;
	double RegionSizeCm = 5000.0;

	/**
	 * Output directory. If empty, defaults to:
	 *   <ProjectDir>/../Server/ZeusServerEngine/Data/Maps/<MapName>/
	 * The exporter creates the directory if it does not exist.
	 */
	FString OutputDir;

	FString MapNameOverride;
};
