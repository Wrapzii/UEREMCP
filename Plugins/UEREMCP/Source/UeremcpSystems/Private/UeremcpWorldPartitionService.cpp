#include "UeremcpWorldPartitionService.h"

#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/SecureHash.h"
#include "UeremcpSystemsHelpers.h"
#include "WorldPartition/WorldPartition.h"

namespace
{
	UWorld* ResolveWorld(const FString& OptionalLevelPath, FString& OutError)
	{
		if (!OptionalLevelPath.IsEmpty())
		{
			const FString ObjectPath = UeremcpSystems::ResolveObjectPath(OptionalLevelPath);
			if (UWorld* Loaded = LoadObject<UWorld>(nullptr, *ObjectPath))
			{
				return Loaded;
			}
			OutError = FString::Printf(TEXT("Could not load level/world '%s'"), *OptionalLevelPath);
			return nullptr;
		}

		if (!GEditor)
		{
			OutError = TEXT("GEditor is unavailable; cannot inspect the editor world");
			return nullptr;
		}
		if (UWorld* EditorWorld = GEditor->GetEditorWorldContext().World())
		{
			return EditorWorld;
		}
		OutError = TEXT("No editor world is loaded");
		return nullptr;
	}

	void FillReport(UWorld* World, FUeremcpWorldPartitionReport& OutReport)
	{
		OutReport = FUeremcpWorldPartitionReport();
		if (!World)
		{
			return;
		}
		OutReport.bWorldLoaded = true;
		OutReport.WorldPath = World->GetPathName();
		OutReport.bIsPartitioned = World->IsPartitionedWorld();

		if (UWorldPartition* WP = World->GetWorldPartition())
		{
			OutReport.bStreamingEnabled = WP->IsStreamingEnabled();
#if WITH_EDITOR
			OutReport.bStreamingEnabledInEditor = WP->IsStreamingEnabledInEditor();
			OutReport.bCanGenerateStreaming = WP->CanGenerateStreaming();
			const FBox EditorBounds = WP->GetEditorWorldBounds();
			const FBox RuntimeBounds = WP->GetRuntimeWorldBounds();
			OutReport.EditorBounds = EditorBounds.ToString();
			OutReport.RuntimeBounds = RuntimeBounds.ToString();
			OutReport.ActorDescCount = static_cast<int32>(WP->GetActorDescContainerCount());
#endif
		}

		FSHA1 Sha;
		auto Feed = [&Sha](const FString& S)
		{
			const FTCHARToUTF8 Utf8(*S);
			Sha.Update(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
		};
		Feed(OutReport.WorldPath);
		Feed(OutReport.bIsPartitioned ? TEXT("1") : TEXT("0"));
		Feed(OutReport.bStreamingEnabled ? TEXT("1") : TEXT("0"));
		Feed(FString::FromInt(OutReport.ActorDescCount));
		Sha.Final();
		uint8 Digest[FSHA1::DigestSize];
		Sha.GetHash(Digest);
		OutReport.ContentHash = BytesToHex(Digest, FSHA1::DigestSize).ToLower();
	}
}

bool FUeremcpWorldPartitionService::Inspect(
	const FString& OptionalLevelPath,
	FUeremcpWorldPartitionReport& OutReport,
	FString& OutError)
{
	UWorld* World = ResolveWorld(OptionalLevelPath, OutError);
	if (!World)
	{
		return false;
	}
	FillReport(World, OutReport);
	return true;
}

bool FUeremcpWorldPartitionService::RepairOrCreate(
	const FString& OptionalLevelPath,
	bool bEnableStreaming,
	bool bDryRun,
	FUeremcpWorldPartitionReport& OutBefore,
	FUeremcpWorldPartitionReport& OutAfter,
	FString& OutError)
{
	UWorld* World = ResolveWorld(OptionalLevelPath, OutError);
	if (!World)
	{
		return false;
	}

	FillReport(World, OutBefore);

	AWorldSettings* WorldSettings = World->GetWorldSettings();
	if (!WorldSettings)
	{
		OutError = TEXT("World has no WorldSettings; cannot create or repair World Partition");
		return false;
	}

	if (bDryRun)
	{
		OutAfter = OutBefore;
		OutAfter.bIsPartitioned = true;
		OutAfter.bStreamingEnabled = bEnableStreaming;
		OutAfter.ContentHash = OutBefore.ContentHash;
		return true;
	}

#if WITH_EDITOR
	// [VERIFIED: WorldPartition.h:167] UWorldPartition::CreateOrRepairWorldPartition
	UWorldPartition* WP = UWorldPartition::CreateOrRepairWorldPartition(WorldSettings);
	if (!WP)
	{
		OutError = TEXT("CreateOrRepairWorldPartition returned null");
		return false;
	}
	WP->SetEnableStreaming(bEnableStreaming);
	World->MarkPackageDirty();
	FillReport(World, OutAfter);
	return true;
#else
	OutError = TEXT("World Partition repair requires WITH_EDITOR");
	return false;
#endif
}
