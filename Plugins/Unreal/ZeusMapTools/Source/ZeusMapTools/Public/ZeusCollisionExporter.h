// Copyright Zeus Server Engine. All rights reserved.

#pragma once

#include "CoreMinimal.h"

#include "ZeusCollisionExportTypes.h"

class AActor;
class UStaticMeshComponent;
class UWorld;

/**
 * Driver of the export pipeline. Iterates the editor world (or selected actors),
 * pulls simple collision out of UBodySetup::AggGeom, and emits debug_collision.json
 * and static_collision.zsm via the dedicated writers.
 *
 * The class lives in the ZeusMapTools editor module and never depends on Jolt nor
 * on any server-side code; the binary writer matches byte-for-byte the loader on
 * the server (ZsmCollisionFormat.hpp).
 */
class FZeusCollisionExporter
{
public:
	FZeusCollisionExporter() = default;

	/**
	 * Runs the exporter using the global editor world. Returns the populated
	 * result regardless of whether files were written (Validate/Preview modes
	 * skip writing).
	 */
	FZeusExportResult ExportFromEditor(const FZeusExportOptions& Options) const;

	/** Runs the exporter against a given world (used by the commandlet). */
	FZeusExportResult ExportFromWorld(UWorld* World, const FZeusExportOptions& Options) const;

private:
	void GatherActors(UWorld* World, bool bSelectedOnly, TArray<AActor*>& OutActors) const;
	bool ProcessActor(AActor* Actor, const FZeusExportOptions& Options, FZeusExportResult& InOutResult) const;
	bool ProcessStaticMeshComponent(AActor* Actor, UStaticMeshComponent* Component,
		const FZeusExportOptions& Options, FZeusExportResult& InOutResult) const;

	void WriteOutputs(const FZeusExportOptions& Options, const FZeusExportResult& Result) const;

	static FString ResolveOutputDir(const FZeusExportOptions& Options, const FString& MapName);
	static FString DeriveMapName(const UWorld* World, const FZeusExportOptions& Options);
	static FZeusEntityRegion ComputeRegion(const FVector& BoundsCenterCm, double RegionSizeCm);
};
