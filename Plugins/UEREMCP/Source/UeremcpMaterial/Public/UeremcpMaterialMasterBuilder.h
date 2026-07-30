// UEREMCP — ensure VFX master materials exist via MaterialEditingLibrary (WS-08).
//
// Equivalent substrate to Epic MaterialTools.create_material + expression wiring
// [VERIFIED: material.py + MaterialEditingLibrary.h].

#pragma once

#include "CoreMinimal.h"

class UMaterial;

struct FUeremcpMaterialMasterBuildResult
{
	bool bSuccess = false;
	bool bCreated = false;
	FString MasterPackagePath;
	FString Error;
	int32 InternalOperations = 0;
};

namespace UeremcpMaterialMasterBuilder
{
	/**
	 * Load or create a minimal unlit additive master with exposed VFX parameters.
	 * Masters live only under /Game/__UeremcpTests/Materials/Masters/.
	 */
	FUeremcpMaterialMasterBuildResult EnsureMasterMaterial(const FString& MasterPackagePath);
}
