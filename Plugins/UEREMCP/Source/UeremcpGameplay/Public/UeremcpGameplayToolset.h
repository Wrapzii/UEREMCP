// UEREMCP — RE-native gameplay toolset (WS-09).

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "UeremcpGameplayToolset.generated.h"

/** One goal-level gameplay surface; Epic GAS/DataTable primitives remain internal. */
UCLASS()
class UEREMCPGAMEPLAY_API UUeremcpGameplayToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	virtual FString GetToolsetVersion() const override { return TEXT("0.1.0-preflight"); }

	/**
	 * Goal-level RE-native create_spell request.
	 *
	 * Dry-run returns a normalized FREAbilityDef plan and Pattern B checks with
	 * status partially_completed and no mutation. Non-dry requests admit through
	 * FUeremcpMutatingDispatch, upsert one row under /Game/__UeremcpTests/, save,
	 * re-read, and report honest *_validated / rolled_back / failed_validation
	 * statuses. Also registered with FUeremcpPlanExecutor for execute_plan.
	 *
	 * @param RequestJson Request envelope with action=create_spell.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Gameplay")
	static FString CreateSpell(const FString& RequestJson);

	/**
	 * Clone one RE ability row into a scratch target row, changing only selected
	 * presentation soft paths, then re-read and compare protected gameplay fields.
	 *
	 * @param RequestJson Request envelope with action=create_spell_variation.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Gameplay")
	static FString CreateSpellVariation(const FString& RequestJson);
};
