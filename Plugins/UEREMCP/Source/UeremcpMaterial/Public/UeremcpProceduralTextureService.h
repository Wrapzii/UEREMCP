// UEREMCP — procedural texture generation (WS-08).
//
// Substrate: FImageUtils::CreateTexture2D [VERIFIED: ImageUtils.h:268]

#pragma once

#include "CoreMinimal.h"
#include "UeremcpEnvelope.h"

struct FUeremcpProceduralTextureRequest
{
	FString TargetAssetPath;
	FString GenerateKind;
	int32 Width = 512;
	int32 Height = 512;
	int32 Seed = 0;
	bool bDryRun = false;
	bool bSave = true;
	bool bValidate = true;
};

struct FUeremcpProceduralTextureResult
{
	bool bSuccess = false;
	bool bCreated = false;
	bool bReused = false;
	FString Status;
	FString Summary;
	FString PrimaryAsset;
	TArray<FUeremcpAssetRef> CreatedAssets;
	TArray<FString> CapabilityNotes;
	TArray<FString> InterpretationNotes;
	int32 InternalOperations = 0;
	int32 VerifiedWidth = 0;
	int32 VerifiedHeight = 0;
};

namespace UeremcpProceduralTextureService
{
	bool IsSupportedGenerateKind(const FString& Kind);

	/** Parse textures.generate object from create_vfx_material specification. */
	bool ParseGenerateSpec(
		const TSharedPtr<class FJsonObject>& GenerateObject,
		FString& OutKind,
		int32& OutWidth,
		int32& OutHeight,
		int32& OutSeed);

	FUeremcpProceduralTextureResult Execute(const FUeremcpProceduralTextureRequest& Request);

	FUeremcpProceduralTextureResult ExecuteFromEnvelope(const FUeremcpRequest& Request);
}
