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
};
