// UEREMCP — create_vfx_material orchestration (WS-08).

#pragma once

#include "CoreMinimal.h"
#include "UeremcpEnvelope.h"

struct UEREMCPMATERIAL_API FUeremcpMaterialCreateResult
{
	bool bSuccess = false;
	FString Status;
	FString Summary;
	FString PrimaryAsset;
	TArray<FUeremcpAssetRef> CreatedAssets;
	TArray<FUeremcpAssetRef> ModifiedAssets;
	TArray<FUeremcpAssetRef> Dependencies;
	TArray<FString> InterpretationNotes;
	TArray<FString> CapabilityNotes;
	int32 InternalOperations = 0;
};

namespace UeremcpMaterialService
{
	/** Execute create_vfx_material against the editor (MaterialEditingLibrary substrate). */
	UEREMCPMATERIAL_API FUeremcpMaterialCreateResult ExecuteCreateVfxMaterial(const FUeremcpRequest& Request);
}
