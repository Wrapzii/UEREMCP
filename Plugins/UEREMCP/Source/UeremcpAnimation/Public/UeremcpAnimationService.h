// UEREMCP — structured animation inspection service (WS-10).
// Domain service: intentionally independent of ToolsetRegistry (ADR-0002).

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UAnimBlueprint;
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

struct UEREMCPANIMATION_API FUeremcpAnimBlueprintInspection
{
	TSharedPtr<FJsonObject> Inventory;
	FString ContentHash;
	int32 GraphCount = 0;
	int32 AnimGraphCount = 0;
	int32 StateMachineCount = 0;
	int32 NodeCount = 0;
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

	/**
	 * Read-only AnimBP graph inventory (Phase 4b scaffold).
	 *
	 * Enumerates animation graphs and Blueprint graphs with names, graph_type,
	 * node counts, and fidelity flags. Does not yet emit full ADR-0004 node/link
	 * payloads — that awaits shared EdGraph serialization with WS-06.
	 *
	 * [VERIFIED: AnimationBlueprintLibrary.h:681]
	 * [VERIFIED: Engine/Classes/Engine/Blueprint.h:1107]
	 * [VERIFIED: AnimGraph/Public/AnimationGraph.h:20]
	 * [VERIFIED: AnimGraph/Public/AnimationStateMachineGraph.h:16]
	 */
	static bool InspectAnimBlueprint(
		UAnimBlueprint* AnimBlueprint,
		const FString& AssetPath,
		FUeremcpAnimBlueprintInspection& OutInspection,
		FString& OutError);
};
