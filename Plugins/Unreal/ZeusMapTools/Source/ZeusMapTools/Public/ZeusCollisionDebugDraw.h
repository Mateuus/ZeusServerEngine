// Copyright Zeus Server Engine. All rights reserved.

#pragma once

#include "CoreMinimal.h"

class UWorld;
struct FZeusExportResult;
struct FZeusEntityExport;
struct FZeusShapeExport;

/**
 * Helpers that draw the collision the exporter sees in the editor viewport.
 *
 * Color rules (must match Docs/COLLISION.md):
 *   Yellow = component bounds (visual reference, NOT the real source).
 *   Green  = simple collision shapes that will be exported (real source).
 *   Red    = actor/component without valid simple collision (export skipped).
 *   Blue   = region/cell bounds (preparing for World Partition broadphase).
 */
class FZeusCollisionDebugDraw
{
public:
	/** Lifetime of debug lines, in seconds. */
	static float Duration;

	static void DrawResult(UWorld* World, const FZeusExportResult& Result);

private:
	static void DrawEntity(UWorld* World, const FZeusEntityExport& Entity);
	static void DrawShape(UWorld* World, const FZeusEntityExport& Entity, const FZeusShapeExport& Shape);
	static void DrawRegion(UWorld* World, const FZeusEntityExport& Entity);
};
