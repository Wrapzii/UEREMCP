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

/** Envelope controls captured now so guarded mutation cannot reinterpret them later. */
struct UEREMCPGAMEPLAY_API FUeremcpAbilityTableWriteOptions
{
	FString RequestId;
	FString Mode;
	bool bDryRun = false;
	bool bAtomic = true;
	bool bSave = true;
	bool bValidate = true;
	bool bRollbackOnFailure = true;
	int32 TimeoutMs = 0;
	FString OnRevisionConflict = TEXT("reject");
	FString ExpectedRevision;
	bool bHasExpectedRevision = false;
	FString IdempotencyKey;
};

/** Ordered write intent prepared without loading or mutating editor assets. */
struct UEREMCPGAMEPLAY_API FUeremcpAbilityTableWritePlan
{
	FString TablePackagePath;
	FString TableObjectPath;
	FString RowStructPath;
	FString RowName;
	FString RequestId;
	FString Mode;
	bool bDryRun = false;
	bool bAtomic = true;
	bool bSave = true;
	bool bValidate = true;
	bool bRollbackOnFailure = true;
	int32 TimeoutMs = 0;
	FString OnRevisionConflict = TEXT("reject");
	FString ExpectedRevision;
	bool bHasExpectedRevision = false;
	FString IdempotencyKey;
	bool bCanClaimValidatedMutation = false;
	TArray<FString> OrderedSteps;
	TArray<FString> RequiredRuntimeGates;
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
	 * Prepare the exact DataTable write intent behind the WS-12 queue gate.
	 * Package-to-object naming uses FPackageName::GetLongPackageAssetName
	 * [VERIFIED: PackageName.h:178-184]. This method performs no editor reads or writes.
	 */
	static bool BuildTableWritePlan(
		const FString& TargetPackagePath,
		const FUeremcpAbilityTableWriteOptions& Options,
		const FUeremcpSpellPlan& SpellPlan,
		FUeremcpAbilityTableWritePlan& OutWritePlan,
		FString& OutError);
};
