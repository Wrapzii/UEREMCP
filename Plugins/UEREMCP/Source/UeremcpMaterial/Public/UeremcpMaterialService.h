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
	FString Revision;
	TArray<FUeremcpAssetRef> CreatedAssets;
	TArray<FUeremcpAssetRef> ModifiedAssets;
	TArray<FUeremcpAssetRef> ReusedAssets;
	TArray<FUeremcpAssetRef> Dependencies;
	TArray<FString> InterpretationNotes;
	TArray<FString> CapabilityNotes;
	int32 InternalOperations = 0;
};

/** One scalar/vector/texture override on an existing MaterialInstanceConstant. */
struct UEREMCPMATERIAL_API FUeremcpMaterialInstanceParameterDelta
{
	FString Name;
	FString Type;
	TSharedPtr<FJsonValue> Before;
	TSharedPtr<FJsonValue> After;
	bool bApplied = false;
	FString Error;
};

struct UEREMCPMATERIAL_API FUeremcpMaterialInstanceUpdateResult
{
	bool bSuccess = false;
	FString Status;
	FString Summary;
	TArray<FUeremcpMaterialInstanceParameterDelta> Deltas;
	TArray<FString> Errors;
	TArray<FUeremcpAssetRef> ModifiedAssets;
	TArray<FString> InterpretationNotes;
	TArray<FString> CapabilityNotes;
	int32 InternalOperations = 0;
	bool bSaved = false;
	TSharedPtr<FJsonObject> ParameterChangesJson;
};

namespace UeremcpMaterialService
{
	/** Execute create_vfx_material against the editor (MaterialEditingLibrary substrate). */
	UEREMCPMATERIAL_API FUeremcpMaterialCreateResult ExecuteCreateVfxMaterial(const FUeremcpRequest& Request);

	/** Apply scalar/vector/texture overrides to an existing MaterialInstanceConstant. */
	UEREMCPMATERIAL_API FUeremcpMaterialInstanceUpdateResult ExecuteUpdateMaterialInstanceParameters(
		const FUeremcpRequest& Request);
}
