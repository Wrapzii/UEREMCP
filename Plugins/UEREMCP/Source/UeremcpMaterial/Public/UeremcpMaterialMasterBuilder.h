// UEREMCP — ensure VFX master materials exist via MaterialEditingLibrary (WS-08).
//
// Equivalent substrate to Epic MaterialTools.create_material + expression wiring
// [VERIFIED: material.py + MaterialEditingLibrary.h].

#pragma once

#include "CoreMinimal.h"

class UMaterial;

struct FUeremcpMaterialMasterBuildRequest
{
	FString MasterPackagePath;
	TArray<FString> Features;
	bool bTrailPurpose = false;
};

struct FUeremcpMaterialMasterBuildResult
{
	bool bSuccess = false;
	bool bCreated = false;
	FString MasterPackagePath;
	FString Error;
	int32 InternalOperations = 0;
	TArray<FString> WiredFeatures;
	TArray<FString> SkippedFeatures;
};

namespace UeremcpMaterialMasterBuilder
{
	/**
	 * Load or create a feature-driven VFX master under /Game/__UeremcpTests/Materials/Masters/.
	 * Master package path must include the feature signature (see UeremcpMaterialFeatures).
	 */
	FUeremcpMaterialMasterBuildResult EnsureMasterMaterial(const FUeremcpMaterialMasterBuildRequest& Request);
}
