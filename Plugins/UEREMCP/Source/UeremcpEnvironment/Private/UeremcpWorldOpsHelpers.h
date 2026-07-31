// Shared landscape / foliage helpers for MCP-002 / MCP-003 / MCP-010.
#pragma once

#include "CoreMinimal.h"

class UWorld;
class AActor;

namespace UeremcpWorldOps
{
	UWorld* EditorWorld();

	/**
	 * Z of the landscape under a point, ignoring foliage and everything else.
	 * Visibility multi-trace first; falls back to ALandscapeProxy::GetHeightAtLocation
	 * (Editor heightfield) when collision is missing — common right after Import
	 * at small uniform scales.
	 * [VERIFIED: LandscapeProxy.h] Cast on hit actor / GetHeightAtLocation.
	 */
	bool LandscapeZAt(UWorld* World, const FVector& Location, float& OutZ);

	/** Remove instanced foliage whose world location falls inside any box. */
	int32 ClearFoliageInBoxes(UWorld* World, const TArray<FBox>& Volumes, bool bDryRun, int32& OutInspected);
}
