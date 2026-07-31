#pragma once

#include "CoreMinimal.h"
#include "UeremcpEnvelope.h"

struct FUeremcpAudioCuePlan
{
	TArray<FString> SoundWavePaths;
	float VolumeMultiplier = 1.0f;
	float PitchMultiplier = 1.0f;
	bool bCreateAttenuation = false;
	FString AttenuationAssetPath;
	FString ExistingAttenuationPath;
};

struct FUeremcpAudioInspection
{
	FString AssetClass;
	FString AssetPath;
	float VolumeMultiplier = 1.0f;
	float PitchMultiplier = 1.0f;
	FString AttenuationPath;
	TArray<FString> SoundWavePaths;
	FString ContentHash;
};

class UEREMCPSYSTEMS_API FUeremcpAudioService
{
public:
	static bool ParseCreateSpec(
		const TSharedPtr<FJsonObject>& Specification,
		const FString& TargetAssetPath,
		FUeremcpAudioCuePlan& OutPlan,
		FString& OutError);

	static bool ExecuteCreateAudioCue(
		const FUeremcpRequest& Request,
		const FUeremcpAudioCuePlan& Plan,
		bool bDryRun,
		FUeremcpResponse& OutResponse,
		FString& OutError);

	static bool Inspect(
		const FString& AssetPath,
		FUeremcpAudioInspection& OutInspection,
		FString& OutError);
};
