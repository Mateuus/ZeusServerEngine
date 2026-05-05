// Copyright Zeus Server Engine. All rights reserved.

#pragma once

#include "CoreMinimal.h"

struct FZeusExportResult;
struct FZeusExportOptions;

/**
 * Emits dynamic_collision.zsm using the ZSMD v1 layout. The header keeps the
 * same magic style as ZSMC ('Z','S','M','D'), version=1, headerSize=32 (until
 * mapName), and contains volumeCount/shapeCount/regionSize plus 3*u32 reserved.
 *
 * Per volume: name, actorName, kind (u8), eventTag, world transform,
 *             bounds (center+extent), region (u32 + i32x3) and a list of
 *             FZeusShapeExport identical to the static format.
 */
class FZeusCollisionDynamicWriter
{
public:
	static bool Write(const FString& OutputPath,
		const FZeusExportResult& Result,
		const FZeusExportOptions& Options);
};
