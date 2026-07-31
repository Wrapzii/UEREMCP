// UEREMCP — environment build service (WS-01).
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "UeremcpEnvelope.h"
#include "UeremcpSpline.h"

struct FUeremcpEnvironmentBuildSpec
{
	uint64 Seed = 1;
	int32 SizeX = 127; // verts = quads+1; 2 components * 63 quads = 126 → 127 verts
	int32 SizeY = 127;
	int32 SectionsPerComponent = 1;
	int32 QuadsPerSection = 63;
	float ScaleXY = 100.f;
	float ScaleZ = 100.f;
	float MountainAmplitude = 0.55f;
	float ValleyDepth = 0.12f;
	float RiverWidth = 600.f;
	float ForestBankWidth = 3500.f;
	int32 MaxFoliageInstances = 800;
	float FoliageSlopeLimitDegrees = 55.f; // valley shoulders exceed 32° with default carve
	float FoliageMinNormalizedHeight = 0.02f;
	float FoliageMaxNormalizedHeight = 0.90f;
	FString MeshPath; // optional static mesh for foliage; empty → skip instances, report gap
	FString RainSystemPath; // optional Niagara; empty → visible instanced-rain fallback
	FString WeatherFollow = TEXT("player_camera");
	int32 RainStreakCount = 256;
	bool bIncludeTerrain = true;
	bool bIncludeRiver = true;
	bool bIncludeForest = true;
	bool bIncludeRain = true;
	bool bIncludeLighting = true;
	bool bCaptureScreenshot = false; // opt-in; prefer CaptureWorldFrames (avoids MCP hang)
	bool bIncludeStructures = false;
	int32 StructureCount = 6;
	FString DestinationLevelPath; // /Game/__UeremcpPoc/MountainRiverRain
	FString FallbackPolicy = TEXT("prefer_real"); // prefer_real | allow_approximate
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

	/** Pure heightmap generation — unit-testable without editor world. */
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
