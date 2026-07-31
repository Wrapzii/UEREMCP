// UEREMCP — environment build service (WS-01 / WS-16 v2).
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "UeremcpEnvelope.h"
#include "UeremcpSpline.h"
#include "UeremcpEnvironmentSpecTypes.h"

struct FUeremcpEnvironmentBuildSpec
{
	int32 SchemaVersion = 2;
	uint64 Seed = 1;
	int32 SizeX = 127;
	int32 SizeY = 127;
	int32 SectionsPerComponent = 1;
	int32 QuadsPerSection = 63;
	float ScaleXY = 100.f;
	float ScaleZ = 100.f;
	EUeremcpTerrainProfile TerrainProfile = EUeremcpTerrainProfile::Mountains;
	float MountainAmplitude = 0.55f;
	float ValleyDepth = 0.12f;
	float RiverWidth = 600.f;
	float ForestBankWidth = 3500.f;
	int32 MaxFoliageInstances = 800;
	float FoliageSlopeLimitDegrees = 32.f;
	float FoliageMinNormalizedHeight = 0.02f;
	float FoliageMaxNormalizedHeight = 0.90f;
	FString VegetationMode = TEXT("none");
	FString MeshPath;
	FString WeatherFollow = TEXT("player_camera");
	int32 RainStreakCount = 256;
	bool bIncludeTerrain = false;
	bool bIncludeRiver = false;
	bool bIncludeForest = false;
	bool bIncludeRain = false;
	bool bIncludeLighting = false;
	bool bCaptureScreenshot = false;
	bool bIncludeStructures = false;
	int32 StructureCount = 6;
	FString DestinationLevelPath;
	FString FallbackPolicy = TEXT("prefer_real");
	FString LightingPreset = TEXT("overcast");
	FString ViewpointMode = TEXT("auto");
	TArray<FUeremcpWeatherPhenomenonSpec> WeatherPhenomena;
	TArray<FUeremcpStructurePlacementSpec> Structures;

	/** Legacy single-path rain override (v1 weather object). */
	FString RainSystemPath;

	bool HasAnyWeatherPhenomenon() const
	{
		return WeatherPhenomena.Num() > 0 || bIncludeRain;
	}

	bool WantsVegetation() const
	{
		return bIncludeForest
			|| VegetationMode.Equals(TEXT("forest"), ESearchCase::IgnoreCase)
			|| VegetationMode.Equals(TEXT("sparse"), ESearchCase::IgnoreCase);
	}
};

struct FUeremcpEnvironmentBuildResult
{
	FString Status = TEXT("failed_validation");
	FString Summary;
	TArray<FString> CapabilityNotes;
	TArray<FString> Warnings;
	TSharedPtr<FJsonObject> RealVsApproximated;
	TSharedPtr<FJsonObject> StructuralMetrics;
	TSharedPtr<FJsonObject> ChangeManifest;
	TArray<FString> ScreenshotPaths;
	FString Revision;
	int32 InternalOperations = 0;
	bool bApproximated = false;
};

namespace FUeremcpEnvironmentService
{
	bool ParseBuildSpec(const TSharedPtr<FJsonObject>& Spec, FUeremcpEnvironmentBuildSpec& Out, FString& OutError);

	bool ValidateIncludeDependencies(const FUeremcpEnvironmentBuildSpec& Spec, FString& OutError);

	void GenerateHeightmap(
		const FUeremcpEnvironmentBuildSpec& Spec,
		const FUeremcpSplinePath& River,
		TArray<uint16>& OutHeights,
		TSharedPtr<FJsonObject>& OutMetrics);

	FUeremcpEnvironmentBuildResult Build(
		const FUeremcpRequest& Request,
		const FUeremcpEnvironmentBuildSpec& Spec,
		bool bDryRun);

	FUeremcpEnvironmentBuildResult Inspect(const FString& LevelOrPackagePath);
	FUeremcpEnvironmentBuildResult Validate(const FString& LevelOrPackagePath, const TSharedPtr<FJsonObject>& Gates);
}
