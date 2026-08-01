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

	/** Opt-in for scale_xy != scale_z. Mismatch bakes wrong slopes in permanently. */
	bool bAllowNonUniformScale = false;
	/** Opt-in for scale_z above the needle threshold. */
	bool bAllowExtremeScaleZ = false;
	/** realistic | hero | reference | blockout | approximate. Gates primitive output. */
	FString Quality = TEXT("blockout");

	bool DemandsRealism() const
	{
		return Quality.Equals(TEXT("realistic"), ESearchCase::IgnoreCase)
			|| Quality.Equals(TEXT("hero"), ESearchCase::IgnoreCase)
			|| Quality.Equals(TEXT("reference"), ESearchCase::IgnoreCase);
	}
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
	bool bIncludeLake = false;
	bool bIncludeOcean = false;
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
	/** Optional exact actor label for water (default UEREMCP_River|Lake|Ocean). */
	FString WaterActorLabel;
	FString WaterBodyType = TEXT("river");
	FVector LakeCenter = FVector(8500.f, 3200.f, -80.f);
	float LakeRadiusCm = 2500.f;
	FVector OceanCenter = FVector(0.f, -16000.f, -120.f);
	FVector2D OceanExtentsCm = FVector2D(60000.f, 30000.f);
	TArray<FUeremcpWeatherPhenomenonSpec> WeatherPhenomena;
	TArray<FUeremcpStructurePlacementSpec> Structures;

	/** Legacy single-path rain override (v1 weather object). */
	FString RainSystemPath;

	bool HasAnyWeatherPhenomenon() const
	{
		return WeatherPhenomena.Num() > 0 || bIncludeRain;
	}

	/**
	 * Names the foliage group this scatter owns, so several species can coexist.
	 *
	 * Empty means the legacy single shared group. Set it (vegetation.group) and a
	 * scatter replaces only its OWN instances, leaving other species, the
	 * terrain, and structures alone.
	 */
	FString FoliageGroup;

	FString FoliageActorLabel() const
	{
		return FoliageGroup.IsEmpty()
			? FString(TEXT("UEREMCP_Forest"))
			: FString::Printf(TEXT("UEREMCP_Forest_%s"), *FoliageGroup);
	}

	bool WantsVegetation() const
	{
		return bIncludeForest
			|| VegetationMode.Equals(TEXT("forest"), ESearchCase::IgnoreCase)
			|| VegetationMode.Equals(TEXT("sparse"), ESearchCase::IgnoreCase);
	}

	bool WantsAnyWater() const
	{
		return bIncludeRiver || bIncludeLake || bIncludeOcean;
	}

	FString ResolvedWaterLabel() const
	{
		if (!WaterActorLabel.IsEmpty())
		{
			return WaterActorLabel;
		}
		if (bIncludeOcean || WaterBodyType.Equals(TEXT("ocean"), ESearchCase::IgnoreCase))
		{
			return TEXT("UEREMCP_Ocean");
		}
		if (bIncludeLake || WaterBodyType.Equals(TEXT("lake"), ESearchCase::IgnoreCase))
		{
			return TEXT("UEREMCP_Lake");
		}
		return TEXT("UEREMCP_River");
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

	/** MCP-011 structured rejection fields. Copied onto the response envelope. */
	FString ErrorCode;
	TSharedPtr<FJsonObject> NextArgs;
};

/** Optional structured detail returned alongside a parse/validation failure. */
struct FUeremcpEnvironmentRejection
{
	FString Message;
	FString Code;
	TSharedPtr<FJsonObject> NextArgs;
};

namespace FUeremcpEnvironmentService
{
	bool ParseBuildSpec(const TSharedPtr<FJsonObject>& Spec, FUeremcpEnvironmentBuildSpec& Out, FString& OutError);

	/** Like ParseBuildSpec but fills Code/NextArgs for known recoverable rejections. */
	bool ParseBuildSpec(
		const TSharedPtr<FJsonObject>& Spec,
		FUeremcpEnvironmentBuildSpec& Out,
		FUeremcpEnvironmentRejection& OutRejection);

	/** P0-3 / P0-5: uniform scale, needle threshold, and the realism gate. */
	bool ValidateScaleAndQuality(const FUeremcpEnvironmentBuildSpec& Spec, FString& OutError);

	bool ValidateScaleAndQuality(
		const FUeremcpEnvironmentBuildSpec& Spec,
		FUeremcpEnvironmentRejection& OutRejection);

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
