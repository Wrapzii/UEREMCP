// UEREMCP — structured animation inspection service (WS-10).
// Domain service: intentionally independent of ToolsetRegistry (ADR-0002).

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UAnimMontage;

struct UEREMCPANIMATION_API FUeremcpMontageInspection
{
	TSharedPtr<FJsonObject> State;
	FString ContentHash;
	int32 SlotCount = 0;
	int32 SegmentCount = 0;
	int32 SectionCount = 0;
	int32 NotifyCount = 0;
	TArray<FString> DependencyPaths;
};

class UEREMCPANIMATION_API FUeremcpAnimationService
{
public:
	/**
	 * Read complete montage-adjacent state into a stable JSON object.
	 *
	 * The implementation uses public montage, segment, section and notify APIs:
	 * [VERIFIED: Engine/Classes/Animation/AnimMontage.h:37-93,695-749]
	 * [VERIFIED: Engine/Classes/Animation/AnimCompositeBase.h:65-138]
	 * [VERIFIED: Editor/AnimationBlueprintLibrary/Public/AnimationBlueprintLibrary.h:230-269]
	 */
	static bool InspectMontage(
		const UAnimMontage* Montage,
		const FString& AssetPath,
		FUeremcpMontageInspection& OutInspection,
		FString& OutError);
};
