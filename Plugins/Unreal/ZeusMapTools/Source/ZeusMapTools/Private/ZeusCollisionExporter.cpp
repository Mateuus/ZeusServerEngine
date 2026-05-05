// Copyright Zeus Server Engine. All rights reserved.

#include "ZeusCollisionExporter.h"

#include "ZeusMapTools.h"
#include "ZeusCollisionDebugDraw.h"
#include "ZeusCollisionJsonWriter.h"
#include "ZeusCollisionBinaryWriter.h"

#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/Selection.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

namespace
{
	/** Returns the actor / component label normalized to ASCII fallback if needed. */
	FString SafeName(const UObject* Object)
	{
		if (!Object)
		{
			return TEXT("None");
		}
		return Object->GetName();
	}

	/** Compute bounds from a UStaticMeshComponent's local body bounds applied to its world transform. */
	void ComputeWorldBounds(const UStaticMeshComponent* Component, FVector& OutCenter, FVector& OutExtent)
	{
		if (!Component)
		{
			OutCenter = FVector::ZeroVector;
			OutExtent = FVector::ZeroVector;
			return;
		}
		const FBoxSphereBounds Bounds = Component->Bounds;
		OutCenter = Bounds.Origin;
		OutExtent = Bounds.BoxExtent;
	}
}

FZeusExportResult FZeusCollisionExporter::ExportFromEditor(const FZeusExportOptions& Options) const
{
	UWorld* World = nullptr;

	if (GEditor)
	{
		World = GEditor->GetEditorWorldContext().World();
	}

	if (!World)
	{
		FZeusExportResult Result;
		Result.Warnings.Add(TEXT("No editor world available."));
		Result.Stats.WarningCount = Result.Warnings.Num();
		UE_LOG(LogZeusMapTools, Warning, TEXT("[ZeusMapTools] No editor world available; aborting export."));
		return Result;
	}

	return ExportFromWorld(World, Options);
}

FZeusExportResult FZeusCollisionExporter::ExportFromWorld(UWorld* World, const FZeusExportOptions& Options) const
{
	FZeusExportResult Result;
	Result.MapName = DeriveMapName(World, Options);

	if (!World)
	{
		Result.Warnings.Add(TEXT("ExportFromWorld called with null UWorld."));
		Result.Stats.WarningCount = Result.Warnings.Num();
		return Result;
	}

	TArray<AActor*> Actors;
	GatherActors(World, Options.bIncludeSelectedOnly, Actors);

	UE_LOG(LogZeusMapTools, Log,
		TEXT("[ZeusMapTools] Export starting map=%s actors=%d selectedOnly=%s"),
		*Result.MapName, Actors.Num(),
		Options.bIncludeSelectedOnly ? TEXT("true") : TEXT("false"));

	for (AActor* Actor : Actors)
	{
		ProcessActor(Actor, Options, Result);
	}

	Result.Stats.WarningCount = Result.Warnings.Num();
	for (const FZeusEntityExport& Entity : Result.Entities)
	{
		Result.Stats.WarningCount += Entity.Warnings.Num();
		for (const FZeusShapeExport& Shape : Entity.Shapes)
		{
			Result.Stats.WarningCount += Shape.Warnings.Num();
		}
	}

	if (Options.bDrawDebug)
	{
		FZeusCollisionDebugDraw::DrawResult(World, Result);
	}

	if (!Options.bValidateOnly && (Options.bWriteJson || Options.bWriteZsm))
	{
		WriteOutputs(Options, Result);
	}

	UE_LOG(LogZeusMapTools, Log,
		TEXT("[ZeusMapTools] Export done map=%s entities=%d shapes=%d (box=%d sphere=%d capsule=%d convex=%d) warnings=%d skipped=%d"),
		*Result.MapName,
		Result.Stats.EntityCount, Result.Stats.ShapeCount,
		Result.Stats.BoxCount, Result.Stats.SphereCount, Result.Stats.CapsuleCount, Result.Stats.ConvexCount,
		Result.Stats.WarningCount, Result.Stats.SkippedActorCount);

	return Result;
}

void FZeusCollisionExporter::GatherActors(UWorld* World, bool bSelectedOnly, TArray<AActor*>& OutActors) const
{
	OutActors.Reset();

	if (!World)
	{
		return;
	}

	if (bSelectedOnly && GEditor)
	{
		USelection* Selection = GEditor->GetSelectedActors();
		if (Selection)
		{
			TArray<UObject*> SelectedObjects;
			Selection->GetSelectedObjects(SelectedObjects);
			for (UObject* Object : SelectedObjects)
			{
				if (AActor* Actor = Cast<AActor>(Object))
				{
					if (Actor->GetWorld() == World)
					{
						OutActors.Add(Actor);
					}
				}
			}
		}
	}
	else
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			OutActors.Add(*It);
		}
	}
}

bool FZeusCollisionExporter::ProcessActor(AActor* Actor, const FZeusExportOptions& Options, FZeusExportResult& InOutResult) const
{
	if (!Actor)
	{
		return false;
	}

	TArray<UStaticMeshComponent*> Components;
	Actor->GetComponents<UStaticMeshComponent>(Components);

	if (Components.Num() == 0)
	{
		++InOutResult.Stats.SkippedActorCount;
		return false;
	}

	bool bAnyExported = false;
	for (UStaticMeshComponent* Component : Components)
	{
		if (!Component || !Component->IsRegistered())
		{
			continue;
		}
		if (!Component->GetStaticMesh())
		{
			continue;
		}
		if (ProcessStaticMeshComponent(Actor, Component, Options, InOutResult))
		{
			bAnyExported = true;
		}
	}

	if (!bAnyExported)
	{
		++InOutResult.Stats.SkippedActorCount;
	}
	return bAnyExported;
}

bool FZeusCollisionExporter::ProcessStaticMeshComponent(AActor* Actor, UStaticMeshComponent* Component,
	const FZeusExportOptions& Options, FZeusExportResult& InOutResult) const
{
	if (!Actor || !Component)
	{
		return false;
	}

	UStaticMesh* Mesh = Component->GetStaticMesh();
	if (!Mesh)
	{
		const FString Warning = FString::Printf(TEXT("Actor %s component %s has no UStaticMesh; skipped."),
			*SafeName(Actor), *SafeName(Component));
		InOutResult.Warnings.Add(Warning);
		return false;
	}

	UBodySetup* BodySetup = Mesh->GetBodySetup();
	if (!BodySetup)
	{
		const FString Warning = FString::Printf(TEXT("Mesh %s has no BodySetup (actor=%s); skipped."),
			*SafeName(Mesh), *SafeName(Actor));
		InOutResult.Warnings.Add(Warning);
		return false;
	}

	FZeusEntityExport Entity;
	Entity.EntityName = Mesh->GetName();
	Entity.ActorName = Actor->GetActorNameOrLabel();
	Entity.ComponentName = Component->GetName();
	Entity.SourcePath = Mesh->GetPathName();
	Entity.EntityWorldTransform = Component->GetComponentTransform();

	ComputeWorldBounds(Component, Entity.BoundsCenter, Entity.BoundsExtent);
	Entity.Region = ComputeRegion(Entity.BoundsCenter, Options.RegionSizeCm);

	const FKAggregateGeom& AggGeom = BodySetup->AggGeom;

	const FVector ComponentScale = Component->GetComponentTransform().GetScale3D();
	const bool bUniformScale = FMath::IsNearlyEqual(ComponentScale.X, ComponentScale.Y, KINDA_SMALL_NUMBER)
		&& FMath::IsNearlyEqual(ComponentScale.Y, ComponentScale.Z, KINDA_SMALL_NUMBER);
	const double MaxScale = static_cast<double>(FMath::Max3(FMath::Abs(ComponentScale.X),
		FMath::Abs(ComponentScale.Y), FMath::Abs(ComponentScale.Z)));

	for (const FKBoxElem& Box : AggGeom.BoxElems)
	{
		FZeusShapeExport Shape;
		Shape.Type = EZeusCollisionShapeType::Box;

		const FTransform LocalT(Box.Rotation.Quaternion(), Box.Center, FVector::OneVector);
		Shape.LocalTransform = LocalT;

		const double SX = static_cast<double>(Box.X) * 0.5 * static_cast<double>(FMath::Abs(ComponentScale.X));
		const double SY = static_cast<double>(Box.Y) * 0.5 * static_cast<double>(FMath::Abs(ComponentScale.Y));
		const double SZ = static_cast<double>(Box.Z) * 0.5 * static_cast<double>(FMath::Abs(ComponentScale.Z));
		Shape.Box.HalfExtents = FVector(SX, SY, SZ);

		Entity.Shapes.Add(MoveTemp(Shape));
		++InOutResult.Stats.BoxCount;
	}

	for (const FKSphereElem& Sphere : AggGeom.SphereElems)
	{
		FZeusShapeExport Shape;
		Shape.Type = EZeusCollisionShapeType::Sphere;

		const FTransform LocalT(FQuat::Identity, Sphere.Center, FVector::OneVector);
		Shape.LocalTransform = LocalT;

		double Radius = static_cast<double>(Sphere.Radius);
		if (bUniformScale)
		{
			Radius *= static_cast<double>(FMath::Abs(ComponentScale.X));
		}
		else
		{
			Radius *= MaxScale;
			Shape.Warnings.Add(TEXT("Sphere on non-uniform scaled component; using max scale axis."));
		}
		Shape.Sphere.Radius = Radius;

		Entity.Shapes.Add(MoveTemp(Shape));
		++InOutResult.Stats.SphereCount;
	}

	for (const FKSphylElem& Sphyl : AggGeom.SphylElems)
	{
		FZeusShapeExport Shape;
		Shape.Type = EZeusCollisionShapeType::Capsule;

		const FTransform LocalT(Sphyl.Rotation.Quaternion(), Sphyl.Center, FVector::OneVector);
		Shape.LocalTransform = LocalT;

		double Radius = static_cast<double>(Sphyl.Radius);
		double HalfHeight = static_cast<double>(Sphyl.Length) * 0.5;
		if (bUniformScale)
		{
			Radius *= static_cast<double>(FMath::Abs(ComponentScale.X));
			HalfHeight *= static_cast<double>(FMath::Abs(ComponentScale.X));
		}
		else
		{
			Radius *= MaxScale;
			HalfHeight *= MaxScale;
			Shape.Warnings.Add(TEXT("Capsule on non-uniform scaled component; using max scale axis."));
		}
		Shape.Capsule.Radius = Radius;
		Shape.Capsule.HalfHeight = HalfHeight;

		Entity.Shapes.Add(MoveTemp(Shape));
		++InOutResult.Stats.CapsuleCount;
	}

	for (const FKConvexElem& Convex : AggGeom.ConvexElems)
	{
		FZeusShapeExport Shape;
		Shape.Type = EZeusCollisionShapeType::Convex;

		Shape.LocalTransform = Convex.GetTransform();

		const TArray<FVector>& Verts = Convex.VertexData;
		if (Verts.Num() < 4)
		{
			Shape.Warnings.Add(FString::Printf(TEXT("Convex with too few vertices (%d); skipped."), Verts.Num()));
			continue;
		}
		Shape.Convex.Vertices.Reserve(Verts.Num());
		for (const FVector& V : Verts)
		{
			Shape.Convex.Vertices.Add(FVector(V.X * ComponentScale.X, V.Y * ComponentScale.Y, V.Z * ComponentScale.Z));
		}

		Entity.Shapes.Add(MoveTemp(Shape));
		++InOutResult.Stats.ConvexCount;
	}

	if (Entity.Shapes.Num() == 0)
	{
		const FString Warning = FString::Printf(TEXT("Mesh %s has no simple collision (actor=%s); skipped."),
			*SafeName(Mesh), *SafeName(Actor));
		InOutResult.Warnings.Add(Warning);
		return false;
	}

	InOutResult.Stats.ShapeCount += Entity.Shapes.Num();
	++InOutResult.Stats.EntityCount;
	InOutResult.Entities.Add(MoveTemp(Entity));
	return true;
}

void FZeusCollisionExporter::WriteOutputs(const FZeusExportOptions& Options, const FZeusExportResult& Result) const
{
	const FString OutDir = ResolveOutputDir(Options, Result.MapName);
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*OutDir))
	{
		if (!PlatformFile.CreateDirectoryTree(*OutDir))
		{
			UE_LOG(LogZeusMapTools, Error, TEXT("[ZeusMapTools] Failed to create output dir: %s"), *OutDir);
			return;
		}
	}

	if (Options.bWriteJson)
	{
		const FString JsonPath = FPaths::Combine(OutDir, TEXT("debug_collision.json"));
		if (FZeusCollisionJsonWriter::Write(JsonPath, Result, Options))
		{
			UE_LOG(LogZeusMapTools, Log, TEXT("[ZeusMapTools] Wrote %s"), *JsonPath);
		}
		else
		{
			UE_LOG(LogZeusMapTools, Error, TEXT("[ZeusMapTools] Failed to write JSON %s"), *JsonPath);
		}
	}

	if (Options.bWriteZsm)
	{
		const FString ZsmPath = FPaths::Combine(OutDir, TEXT("static_collision.zsm"));
		if (FZeusCollisionBinaryWriter::Write(ZsmPath, Result, Options))
		{
			UE_LOG(LogZeusMapTools, Log, TEXT("[ZeusMapTools] Wrote %s"), *ZsmPath);
		}
		else
		{
			UE_LOG(LogZeusMapTools, Error, TEXT("[ZeusMapTools] Failed to write ZSM %s"), *ZsmPath);
		}
	}
}

FString FZeusCollisionExporter::ResolveOutputDir(const FZeusExportOptions& Options, const FString& MapName)
{
	if (!Options.OutputDir.IsEmpty())
	{
		return FPaths::ConvertRelativePathToFull(Options.OutputDir);
	}

	const FString ProjectDir = FPaths::ProjectDir();
	const FString DefaultDir = FPaths::Combine(ProjectDir, TEXT(".."), TEXT(".."), TEXT("Server"),
		TEXT("ZeusServerEngine"), TEXT("Data"), TEXT("Maps"), MapName);
	return FPaths::ConvertRelativePathToFull(DefaultDir);
}

FString FZeusCollisionExporter::DeriveMapName(const UWorld* World, const FZeusExportOptions& Options)
{
	if (!Options.MapNameOverride.IsEmpty())
	{
		return Options.MapNameOverride;
	}
	if (!World)
	{
		return TEXT("UnknownMap");
	}
	const FString MapName = World->GetMapName();
	if (MapName.IsEmpty())
	{
		return TEXT("UnknownMap");
	}
	return MapName;
}

FZeusEntityRegion FZeusCollisionExporter::ComputeRegion(const FVector& BoundsCenterCm, double RegionSizeCm)
{
	FZeusEntityRegion Region;
	Region.RegionSizeCm = RegionSizeCm;

	const double Size = (RegionSizeCm > 0.0) ? RegionSizeCm : 5000.0;
	Region.GridX = static_cast<int32>(FMath::FloorToDouble(BoundsCenterCm.X / Size));
	Region.GridY = static_cast<int32>(FMath::FloorToDouble(BoundsCenterCm.Y / Size));
	Region.GridZ = static_cast<int32>(FMath::FloorToDouble(BoundsCenterCm.Z / Size));

	const uint32 Hx = static_cast<uint32>(Region.GridX) * 73856093u;
	const uint32 Hy = static_cast<uint32>(Region.GridY) * 19349663u;
	const uint32 Hz = static_cast<uint32>(Region.GridZ) * 83492791u;
	Region.RegionId = (Hx ^ Hy ^ Hz);
	return Region;
}
