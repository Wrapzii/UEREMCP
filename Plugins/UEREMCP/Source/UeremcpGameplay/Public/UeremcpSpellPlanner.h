// UEREMCP — deterministic RE spell-row planning (WS-09).

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

struct UEREMCPGAMEPLAY_API FUeremcpSpellPlan
{
	FString RowName;
	TSharedPtr<FJsonObject> RowPayload;
	TArray<FString> DependencyAssetPaths;
	TArray<FString> StaticChecks;
};

/** Ordered write intent prepared without loading or mutating editor assets. */
struct UEREMCPGAMEPLAY_API FUeremcpAbilityTableWritePlan
{
	FString TablePackagePath;
	FString TableObjectPath;
	FString RowStructPath;
	FString RowName;
	FString Mode;
	bool bDryRun = false;
	TArray<FString> OrderedSteps;
};

/**
 * Pure planner behind create_spell.
 *
 * This layer deliberately has no ToolsetRegistry dependency (ADR-0002). It maps
 * the semantic contract to FREAbilityDef property names verified in the RE header
 * [VERIFIED: REAbilityTypes.h:85-247]. Mutation remains outside this class.
 */
class UEREMCPGAMEPLAY_API FUeremcpSpellPlanner
{
public:
	/** Parse, normalize, and statically validate one create_spell specification. */
	static bool BuildPlan(
		const TSharedPtr<FJsonObject>& Specification,
		FUeremcpSpellPlan& OutPlan,
		FString& OutError);

	/**
	 * Prepare the exact DataTable write intent behind the WS-03/WS-12 gates.
	 * Package-to-object naming uses FPackageName::GetLongPackageAssetName
	 * [VERIFIED: PackageName.h:178-184]. This method performs no editor reads or writes.
	 */
	static bool BuildTableWritePlan(
		const FString& TargetPackagePath,
		const FString& Mode,
		bool bDryRun,
		const FUeremcpSpellPlan& SpellPlan,
		FUeremcpAbilityTableWritePlan& OutWritePlan,
		FString& OutError);
};
