// UEREMCP — wire specification.features into master material graphs (WS-08).
//
// Substrate: UMaterialEditingLibrary [VERIFIED: MaterialEditingLibrary.h]
// Expression classes [VERIFIED: Engine/Public/Materials/MaterialExpression*.h]

#pragma once

#include "CoreMinimal.h"

class UMaterial;

struct FUeremcpFeatureGraphBuildResult
{
	bool bSuccess = false;
	FString Error;
	int32 InternalOperations = 0;
	TArray<FString> WiredFeatures;
	TArray<FString> SkippedFeatures;
};

namespace UeremcpMaterialFeatureGraph
{
	/**
	 * Build or rebuild a VFX master graph from feature tokens.
	 * Clears existing expressions when rebuilding an empty material only (new assets).
	 */
	FUeremcpFeatureGraphBuildResult BuildGraph(
		UMaterial* Material,
		const TArray<FString>& Features,
		bool bTrailPurpose);
}
