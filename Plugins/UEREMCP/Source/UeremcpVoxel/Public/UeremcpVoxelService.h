// UEREMCP — Voxel terrain service (interior dungeon open-world ops).
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class AVoxelWorld;

struct FUeremcpVoxelOpResult
{
	bool bOk = false;
	FString Status = TEXT("failed");
	FString Summary;
	FString ErrorCode;
	TArray<FString> Warnings;
	TArray<FString> CapabilityNotes;
	TSharedPtr<FJsonObject> NextArgs;
	int32 InternalOperations = 0;
	TSharedPtr<FJsonObject> Extra;
};

class FUeremcpVoxelService
{
public:
	static AVoxelWorld* FindVoxelWorld(const FString& ActorLabel, FString& OutError);

	static bool ParseVectorArray(const TArray<TSharedPtr<FJsonValue>>* Arr, TArray<FVector>& Out, FString& OutError);
	static bool ParseVector3(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key, FVector& Out, FString& OutError);
	static bool ParseVector3FromArray(const TArray<TSharedPtr<FJsonValue>>& Arr, FVector& Out, FString& OutError);

	static FUeremcpVoxelOpResult CarveSpline(AVoxelWorld* World, const TSharedPtr<FJsonObject>& Spec, bool bDryRun);
	static FUeremcpVoxelOpResult FlattenArea(AVoxelWorld* World, const TSharedPtr<FJsonObject>& Spec, bool bDryRun);
	static FUeremcpVoxelOpResult SmoothRegion(AVoxelWorld* World, const TSharedPtr<FJsonObject>& Spec, bool bDryRun);
	static FUeremcpVoxelOpResult TerrainStamp(AVoxelWorld* World, const TSharedPtr<FJsonObject>& Spec, bool bDryRun);
	static FUeremcpVoxelOpResult NoiseSculpt(AVoxelWorld* World, const TSharedPtr<FJsonObject>& Spec, bool bDryRun);
	static FUeremcpVoxelOpResult PaintMaterial(AVoxelWorld* World, const TSharedPtr<FJsonObject>& Spec, bool bDryRun);
	static FUeremcpVoxelOpResult GenerateWaterBody(const TSharedPtr<FJsonObject>& Spec, bool bDryRun);
	static FUeremcpVoxelOpResult ProceduralScatter(const TSharedPtr<FJsonObject>& Spec, bool bDryRun);
	static FUeremcpVoxelOpResult GeneratePois(AVoxelWorld* World, const TSharedPtr<FJsonObject>& Spec, bool bDryRun);
	static FUeremcpVoxelOpResult ComposeInteriorTerrain(AVoxelWorld* World, const TSharedPtr<FJsonObject>& Spec, bool bDryRun);
};
