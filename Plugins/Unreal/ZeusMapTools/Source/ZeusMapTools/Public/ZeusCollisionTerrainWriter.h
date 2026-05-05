// Copyright Zeus Server Engine. All rights reserved.

#pragma once

#include "CoreMinimal.h"

struct FZeusExportResult;
struct FZeusExportOptions;

/**
 * Emits terrain_collision.zsm using the ZSMT v1 layout. Magic='Z','S','M','T',
 * version=1, headerSize=36, pieceCount + regionSize + 4*u32 reserved.
 *
 * Per piece: name, actorName, componentName, kind (u8), world transform,
 *            bounds (center+extent), region (u32 + i32x3), and one of:
 *            - TriangleMesh: vertexCount + N*Vec3 + indexCount + N*u32
 *            - HeightField:  samplesX/Y, sampleSpacingCm, originLocal,
 *                            heightScaleCm, samplesX*samplesY*float
 */
class FZeusCollisionTerrainWriter
{
public:
	static bool Write(const FString& OutputPath,
		const FZeusExportResult& Result,
		const FZeusExportOptions& Options);
};
