#pragma once

#include "CoreMinimal.h"
#include "UeremcpEnvelope.h"

struct FUeremcpWorldPartitionReport
{
	bool bWorldLoaded = false;
	bool bIsPartitioned = false;
	bool bStreamingEnabled = false;
	bool bStreamingEnabledInEditor = false;
	bool bCanGenerateStreaming = false;
	int32 ActorDescCount = 0;
	FString WorldPath;
	FString EditorBounds;
	FString RuntimeBounds;
	FString ContentHash;
};

class UEREMCPSYSTEMS_API FUeremcpWorldPartitionService
{
public:
	static bool Inspect(
		const FString& OptionalLevelPath,
		FUeremcpWorldPartitionReport& OutReport,
		FString& OutError);

	static bool RepairOrCreate(
		const FString& OptionalLevelPath,
		bool bEnableStreaming,
		bool bDryRun,
		FUeremcpWorldPartitionReport& OutBefore,
		FUeremcpWorldPartitionReport& OutAfter,
		FString& OutError);
};
