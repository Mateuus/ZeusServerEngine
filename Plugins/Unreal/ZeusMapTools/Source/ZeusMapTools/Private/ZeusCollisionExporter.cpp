// Copyright Zeus Server Engine. All rights reserved.

#include "ZeusCollisionExporter.h"

#include "ZeusMapTools.h"
#include "ZeusCollisionDebugDraw.h"
#include "ZeusCollisionJsonWriter.h"
#include "ZeusCollisionBinaryWriter.h"
#include "ZeusCollisionDynamicWriter.h"
#include "ZeusCollisionTerrainWriter.h"

#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/Selection.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Brush.h"
#include "Engine/BrushBuilder.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BrushComponent.h"
#include "GameFramework/Volume.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Rendering/PositionVertexBuffer.h"
#include "StaticMeshResources.h"
#include "Landscape.h"
#include "LandscapeProxy.h"
#include "LandscapeComponent.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "LandscapeInfo.h"

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

	bool ActorHasTag(const AActor* Actor, const TCHAR* Tag)
	{
		if (!Actor) return false;
		const FName Needle(Tag);
		for (const FName& T : Actor->Tags)
		{
			if (T == Needle) return true;
		}
		return false;
	}

	FString ExtractEventTag(const AActor* Actor)
	{
		if (!Actor) return FString();
		// Tag no formato "tag:nome" ganha prioridade.
		for (const FName& T : Actor->Tags)
		{
			const FString S = T.ToString();
			if (S.StartsWith(TEXT("tag:")))
			{
				return S.RightChop(4);
			}
		}
		return FString();
	}

	EZeusVolumeKind DeriveVolumeKind(const AActor* Actor)
	{
		if (ActorHasTag(Actor, TEXT("Zeus.Water")))      return EZeusVolumeKind::Water;
		if (ActorHasTag(Actor, TEXT("Zeus.Lava")))       return EZeusVolumeKind::Lava;
		if (ActorHasTag(Actor, TEXT("Zeus.KillVolume"))) return EZeusVolumeKind::KillVolume;
		if (ActorHasTag(Actor, TEXT("Zeus.SafeZone")))   return EZeusVolumeKind::SafeZone;
		// ATriggerVolume e qualquer AVolume sem tag explicita -> Trigger.
		return EZeusVolumeKind::Trigger;
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

	// 1) Volumes (AVolume com BrushComponent) -> ZSMD.
	if (Actor->IsA<AVolume>())
	{
		if (ProcessVolumeActor(Actor, Options, InOutResult))
		{
			return true;
		}
		++InOutResult.Stats.SkippedActorCount;
		return false;
	}

	// 2) Landscape (ALandscape ou ALandscapeProxy) -> ZSMT (HeightField).
	if (Actor->IsA<ALandscapeProxy>())
	{
		if (ProcessLandscapeActor(Actor, Options, InOutResult))
		{
			return true;
		}
		++InOutResult.Stats.SkippedActorCount;
		return false;
	}

	// 3) StaticMesh (default + tag Zeus.TriangleMesh).
	TArray<UStaticMeshComponent*> Components;
	Actor->GetComponents<UStaticMeshComponent>(Components);

	if (Components.Num() == 0)
	{
		++InOutResult.Stats.SkippedActorCount;
		return false;
	}

	const bool bForceTriangleMesh = ActorHasTag(Actor, TEXT("Zeus.TriangleMesh"))
		|| Options.bAllowComplexAsSimple;

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
		bool bExported = false;
		if (bForceTriangleMesh)
		{
			bExported = ProcessTriangleMeshActor(Actor, Component, Options, InOutResult);
		}
		if (!bExported)
		{
			bExported = ProcessStaticMeshComponent(Actor, Component, Options, InOutResult);
		}
		if (bExported)
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
		// Vertices stay in BodySetup/convex local space. Component scale is already
		// carried by EntityWorldTransform (GetComponentTransform); baking scale
		// here too would double-apply it in preview (DrawShape) and mismatch ZSM
		// vs intended hull when only Location/Rotation are used on the server.
		Shape.Convex.Vertices.Reserve(Verts.Num());
		for (const FVector& V : Verts)
		{
			Shape.Convex.Vertices.Add(V);
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

bool FZeusCollisionExporter::ProcessVolumeActor(AActor* Actor, const FZeusExportOptions& Options, FZeusExportResult& InOutResult) const
{
	AVolume* Volume = Cast<AVolume>(Actor);
	if (!Volume)
	{
		return false;
	}

	UBrushComponent* Brush = Volume->GetBrushComponent();
	if (!Brush)
	{
		const FString Warning = FString::Printf(TEXT("Volume %s has no BrushComponent; skipped."),
			*SafeName(Volume));
		InOutResult.Warnings.Add(Warning);
		return false;
	}

	UBodySetup* BrushBodySetup = Brush->BrushBodySetup;
	const FBoxSphereBounds Bounds = Brush->Bounds;

	FZeusVolumeExport Vol;
	Vol.VolumeName = Volume->GetActorNameOrLabel();
	Vol.ActorName = Volume->GetActorNameOrLabel();
	Vol.Kind = DeriveVolumeKind(Volume);
	Vol.EventTag = ExtractEventTag(Volume);
	Vol.WorldTransform = Volume->GetActorTransform();
	Vol.BoundsCenter = Bounds.Origin;
	Vol.BoundsExtent = Bounds.BoxExtent;
	Vol.Region = ComputeRegion(Vol.BoundsCenter, Options.RegionSizeCm);

	const FVector ActorScale = Volume->GetActorScale3D();

	if (BrushBodySetup)
	{
		const FKAggregateGeom& Agg = BrushBodySetup->AggGeom;
		for (const FKBoxElem& Box : Agg.BoxElems)
		{
			FZeusShapeExport Shape;
			Shape.Type = EZeusCollisionShapeType::Box;
			Shape.LocalTransform = FTransform(Box.Rotation.Quaternion(), Box.Center, FVector::OneVector);
			Shape.Box.HalfExtents = FVector(
				static_cast<double>(Box.X) * 0.5 * static_cast<double>(FMath::Abs(ActorScale.X)),
				static_cast<double>(Box.Y) * 0.5 * static_cast<double>(FMath::Abs(ActorScale.Y)),
				static_cast<double>(Box.Z) * 0.5 * static_cast<double>(FMath::Abs(ActorScale.Z)));
			Vol.Shapes.Add(MoveTemp(Shape));
		}
		for (const FKSphereElem& Sphere : Agg.SphereElems)
		{
			FZeusShapeExport Shape;
			Shape.Type = EZeusCollisionShapeType::Sphere;
			Shape.LocalTransform = FTransform(FQuat::Identity, Sphere.Center, FVector::OneVector);
			Shape.Sphere.Radius = static_cast<double>(Sphere.Radius)
				* static_cast<double>(FMath::Abs(ActorScale.X));
			Vol.Shapes.Add(MoveTemp(Shape));
		}
		for (const FKSphylElem& Sphyl : Agg.SphylElems)
		{
			FZeusShapeExport Shape;
			Shape.Type = EZeusCollisionShapeType::Capsule;
			Shape.LocalTransform = FTransform(Sphyl.Rotation.Quaternion(), Sphyl.Center, FVector::OneVector);
			Shape.Capsule.Radius = static_cast<double>(Sphyl.Radius)
				* static_cast<double>(FMath::Abs(ActorScale.X));
			Shape.Capsule.HalfHeight = static_cast<double>(Sphyl.Length) * 0.5
				* static_cast<double>(FMath::Abs(ActorScale.X));
			Vol.Shapes.Add(MoveTemp(Shape));
		}
	}

	if (Vol.Shapes.Num() == 0)
	{
		// Fallback: gera 1 box do BrushComponent's bounds (local).
		FZeusShapeExport Shape;
		Shape.Type = EZeusCollisionShapeType::Box;
		Shape.LocalTransform = FTransform::Identity;
		const FVector LocalExtent = Bounds.BoxExtent;
		Shape.Box.HalfExtents = LocalExtent;
		Vol.Shapes.Add(MoveTemp(Shape));
	}

	++InOutResult.Stats.VolumeCount;
	InOutResult.Volumes.Add(MoveTemp(Vol));
	return true;
}

bool FZeusCollisionExporter::ProcessTriangleMeshActor(AActor* Actor, UStaticMeshComponent* Component,
	const FZeusExportOptions& Options, FZeusExportResult& InOutResult) const
{
	if (!Actor || !Component)
	{
		return false;
	}
	UStaticMesh* Mesh = Component->GetStaticMesh();
	if (!Mesh) return false;

	FStaticMeshRenderData* RenderData = Mesh->GetRenderData();
	if (!RenderData || RenderData->LODResources.Num() == 0)
	{
		const FString Warning = FString::Printf(TEXT("Mesh %s has no RenderData LODs; triangle mesh skipped."),
			*SafeName(Mesh));
		InOutResult.Warnings.Add(Warning);
		return false;
	}
	const FStaticMeshLODResources& LOD = RenderData->LODResources[0];
	const FPositionVertexBuffer& PosBuf = LOD.VertexBuffers.PositionVertexBuffer;
	const FRawStaticIndexBuffer& IdxBuf = LOD.IndexBuffer;
	const uint32 NumVerts = PosBuf.GetNumVertices();
	const int32 NumIndices = IdxBuf.GetNumIndices();
	if (NumVerts < 3 || NumIndices < 3 || (NumIndices % 3) != 0)
	{
		const FString Warning = FString::Printf(TEXT("Mesh %s has invalid LOD0 topology (verts=%u idx=%d); skipped."),
			*SafeName(Mesh), NumVerts, NumIndices);
		InOutResult.Warnings.Add(Warning);
		return false;
	}

	FZeusTerrainPieceExport Piece;
	Piece.PieceName = Mesh->GetName();
	Piece.ActorName = Actor->GetActorNameOrLabel();
	Piece.ComponentName = Component->GetName();
	Piece.Kind = EZeusTerrainPieceKind::TriangleMesh;
	Piece.WorldTransform = Component->GetComponentTransform();
	const FBoxSphereBounds Bounds = Component->Bounds;
	Piece.BoundsCenter = Bounds.Origin;
	Piece.BoundsExtent = Bounds.BoxExtent;
	Piece.Region = ComputeRegion(Piece.BoundsCenter, Options.RegionSizeCm);

	const FVector Scale = Component->GetComponentScale();
	Piece.TriangleMesh.Vertices.Reserve(NumVerts);
	for (uint32 v = 0; v < NumVerts; ++v)
	{
		const FVector3f P = PosBuf.VertexPosition(v);
		// Aplicar scale do componente (rotacao+pos ficam no WorldTransform).
		Piece.TriangleMesh.Vertices.Add(FVector(
			static_cast<double>(P.X) * Scale.X,
			static_cast<double>(P.Y) * Scale.Y,
			static_cast<double>(P.Z) * Scale.Z));
	}

	TArray<uint32> Indices32;
	IdxBuf.GetCopy(Indices32);
	Piece.TriangleMesh.Indices = MoveTemp(Indices32);

	++InOutResult.Stats.TriangleMeshCount;
	++InOutResult.Stats.TerrainPieceCount;
	InOutResult.TerrainPieces.Add(MoveTemp(Piece));
	return true;
}

bool FZeusCollisionExporter::ProcessLandscapeActor(AActor* Actor, const FZeusExportOptions& Options,
	FZeusExportResult& InOutResult) const
{
	ALandscapeProxy* Landscape = Cast<ALandscapeProxy>(Actor);
	if (!Landscape)
	{
		return false;
	}
	const TArray<TObjectPtr<ULandscapeComponent>>& Components = Landscape->LandscapeComponents;
	if (Components.Num() == 0)
	{
		const FString Warning = FString::Printf(TEXT("Landscape %s has no LandscapeComponents; skipped."),
			*SafeName(Landscape));
		InOutResult.Warnings.Add(Warning);
		return false;
	}

	bool bAnyPiece = false;
	for (const TObjectPtr<ULandscapeComponent>& CompPtr : Components)
	{
		ULandscapeComponent* LC = CompPtr.Get();
		if (!LC)
		{
			continue;
		}
		ULandscapeHeightfieldCollisionComponent* CC = LC->GetCollisionComponent();
		if (!CC)
		{
			InOutResult.Warnings.Add(FString::Printf(
				TEXT("LandscapeComponent %s has no collision component; skipped."),
				*SafeName(LC)));
			continue;
		}

		// Não chamar GetCollisionSampleInfo(): em builds modulares Win64 o método não é
		// LANDSCAPE_API e o linker do plugin não resolve o símbolo. A fórmula espelha
		// ULandscapeHeightfieldCollisionComponent::GetCollisionSampleInfo() no motor.
		const int32 CollisionSizeVerts = CC->CollisionSizeQuads + 1;
		const int32 NumSamples = FMath::Square(CollisionSizeVerts);

		if (CollisionSizeVerts < 2 || NumSamples < 4)
		{
			InOutResult.Warnings.Add(FString::Printf(
				TEXT("Landscape collision %s has invalid sample info (verts=%d samples=%d); skipped."),
				*SafeName(CC), CollisionSizeVerts, NumSamples));
			continue;
		}

		const int32 Expected = CollisionSizeVerts * CollisionSizeVerts;
		if (NumSamples != Expected)
		{
			InOutResult.Warnings.Add(FString::Printf(
				TEXT("Landscape collision %s: NumSamples=%d != verts^2=%d; skipped."),
				*SafeName(CC), NumSamples, Expected));
			continue;
		}

		TArray<float> HeightFloats;
		HeightFloats.SetNumUninitialized(NumSamples);
		if (!CC->FillHeightTile(MakeArrayView(HeightFloats), 0, CollisionSizeVerts))
		{
			InOutResult.Warnings.Add(FString::Printf(
				TEXT("FillHeightTile failed for %s; skipped."), *SafeName(CC)));
			continue;
		}

		FZeusTerrainPieceExport Piece;
		Piece.PieceName = LC->GetName();
		Piece.ActorName = Landscape->GetActorNameOrLabel();
		Piece.ComponentName = LC->GetName();
		Piece.Kind = EZeusTerrainPieceKind::HeightField;
		Piece.WorldTransform = CC->GetComponentTransform();

		const FBoxSphereBounds Bounds = CC->Bounds;
		Piece.BoundsCenter = Bounds.Origin;
		Piece.BoundsExtent = Bounds.BoxExtent;
		Piece.Region = ComputeRegion(Piece.BoundsCenter, Options.RegionSizeCm);

		Piece.HeightField.SamplesX = static_cast<uint32>(CollisionSizeVerts);
		Piece.HeightField.SamplesY = static_cast<uint32>(CollisionSizeVerts);
		Piece.HeightField.OriginLocal = FVector::ZeroVector;
		// FillHeightTile devolve alturas já em cm (delta/local); HeightScaleCm=1 no loader.
		Piece.HeightField.HeightScaleCm = 1.0;

		FVector LocalSize = CC->CachedLocalBox.GetSize();
		if (LocalSize.X < KINDA_SMALL_NUMBER || LocalSize.Y < KINDA_SMALL_NUMBER)
		{
			LocalSize = Bounds.BoxExtent * 2.0f;
		}
		const double Dx = static_cast<double>(LocalSize.X);
		const double Dy = static_cast<double>(LocalSize.Y);
		const double Sx = CollisionSizeVerts > 1 ? Dx / static_cast<double>(CollisionSizeVerts - 1) : 100.0;
		const double Sy = CollisionSizeVerts > 1 ? Dy / static_cast<double>(CollisionSizeVerts - 1) : 100.0;
		Piece.HeightField.SampleSpacingCm = 0.5 * (Sx + Sy);

		Piece.HeightField.Heights.SetNum(NumSamples);
		for (int32 i = 0; i < NumSamples; ++i)
		{
			Piece.HeightField.Heights[i] = HeightFloats[i];
		}

		++InOutResult.Stats.HeightFieldCount;
		++InOutResult.Stats.TerrainPieceCount;
		InOutResult.TerrainPieces.Add(MoveTemp(Piece));
		bAnyPiece = true;
	}

	return bAnyPiece;
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

	if (Options.bWriteDynamicZsm && Result.Volumes.Num() > 0)
	{
		const FString DynPath = FPaths::Combine(OutDir, TEXT("dynamic_collision.zsm"));
		if (FZeusCollisionDynamicWriter::Write(DynPath, Result, Options))
		{
			UE_LOG(LogZeusMapTools, Log, TEXT("[ZeusMapTools] Wrote %s (%d volumes)"),
				*DynPath, Result.Volumes.Num());
		}
		else
		{
			UE_LOG(LogZeusMapTools, Error, TEXT("[ZeusMapTools] Failed to write dynamic ZSM %s"), *DynPath);
		}
	}

	if (Options.bWriteTerrainZsm && Result.TerrainPieces.Num() > 0)
	{
		const FString TerPath = FPaths::Combine(OutDir, TEXT("terrain_collision.zsm"));
		if (FZeusCollisionTerrainWriter::Write(TerPath, Result, Options))
		{
			UE_LOG(LogZeusMapTools, Log, TEXT("[ZeusMapTools] Wrote %s (%d pieces)"),
				*TerPath, Result.TerrainPieces.Num());
		}
		else
		{
			UE_LOG(LogZeusMapTools, Error, TEXT("[ZeusMapTools] Failed to write terrain ZSM %s"), *TerPath);
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
