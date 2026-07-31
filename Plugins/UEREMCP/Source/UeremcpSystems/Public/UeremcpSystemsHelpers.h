// UEREMCP — shared helpers for systems-domain services (WS-01).

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "UeremcpEnvelope.h"

namespace UeremcpSystems
{
	inline bool IsScratchPath(const FString& SoftPath)
	{
		return SoftPath.StartsWith(TEXT("/Game/__UeremcpTests/"))
			|| SoftPath.StartsWith(TEXT("/Game/__UeremcpPoc/"));
	}

	inline FString ResolveObjectPath(const FString& AssetPath)
	{
		if (AssetPath.Contains(TEXT(".")))
		{
			return AssetPath;
		}
		const FString AssetName = FPaths::GetBaseFilename(AssetPath);
		return FString::Printf(TEXT("%s.%s"), *AssetPath, *AssetName);
	}

	inline TSharedPtr<FJsonObject> SpecObject(const FUeremcpRequest& Request)
	{
		return Request.Specification.IsValid() ? Request.Specification : MakeShared<FJsonObject>();
	}

	inline void AddCommonCapabilityNotes(TArray<FString>& OutNotes)
	{
		OutNotes.Add(TEXT("MetaSound graph authoring is not implemented; SoundCue + attenuation only."));
		OutNotes.Add(TEXT("Multi-client replication runtime proof remains WS-11/RB-14 — never claimed by ValidateReplication."));
		OutNotes.Add(TEXT("World Partition HLOD/builder commandlets are blocked; repair uses CreateOrRepairWorldPartition only."));
		OutNotes.Add(TEXT("PCG graph primitives stay on PCGToolset — compose via ExecutePlan; do not duplicate (see docs/proposals/ws-01-pcg-coordination.md)."));
	}
}
