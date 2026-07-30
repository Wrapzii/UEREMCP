#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "UeremcpSpellPlanner.h"

/** Materialized composite source -> scratch ability-row variation. */
struct UEREMCPGAMEPLAY_API FUeremcpAbilityVariationPlan
{
	FString SourceTablePath;
	FString SourceRow;
	FString TargetRow;
	FString VfxPhase;
	FString PresentationAsset;
	FUeremcpSpellPlan SpellPlan;
	TSharedPtr<FJsonObject> SourceProtectedFields;
	TArray<FString> ProtectedFieldNames;
};

/**
 * RE-native C5 contract. Reads an FREAbilityDef source row, clones it, changes
 * only AbilityId and selected VFX soft paths, and can re-read the target to prove
 * protected gameplay equality.
 */
class UEREMCPGAMEPLAY_API FUeremcpAbilityVariation
{
public:
	static bool BuildPlan(
		const TSharedPtr<FJsonObject>& Specification,
		FUeremcpAbilityVariationPlan& OutPlan,
		FString& OutError);

	static bool VerifyTarget(
		const FString& TargetTablePath,
		const FUeremcpAbilityVariationPlan& Plan,
		TSharedPtr<FJsonObject>& OutTargetProtectedFields,
		FString& OutError);
};
