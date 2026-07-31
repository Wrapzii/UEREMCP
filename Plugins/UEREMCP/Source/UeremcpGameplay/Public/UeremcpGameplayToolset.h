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
	 * Use when: create a gameplay spell/ability row with delivery, costs, impact, and networking.
	 * Inputs: action=create_spell; specification.row_name, element, element_color,
	 * delivery, and networking are required.
	 * Dry-run returns a normalized FREAbilityDef plan and Pattern B checks with
	 * status partially_completed and no mutation. Non-dry requests admit through
	 * FUeremcpMutatingDispatch, upsert one row under /Game/__UeremcpTests/, save,
	 * re-read, and report honest *_validated / rolled_back / failed_validation
	 * statuses. Also registered with FUeremcpPlanExecutor for execute_plan.
	 * Example: {"protocol_version":"1.0","action":"create_spell","options":{"dry_run":true},"specification":{"row_name":"IceBarrier","element":"Frost","element_color":[0.2,0.7,1.0,1.0],"delivery":{"type":"self_cast"},"networking":{"pattern":"B","authority":"server","cast_path":"AuthorityCastAbility"}}}
	 *
	 * @param RequestJson Request envelope with action=create_spell.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Gameplay")
	static FString CreateSpell(const FString& RequestJson);

	/**
	 * Clone one RE ability row into a scratch target row, changing only selected
	 * presentation soft paths, then re-read and compare protected gameplay fields.
	 *
	 * Use when: make a VFX-only variation of an existing spell without changing gameplay.
	 * Inputs: action=create_spell_variation; specification.source_binding, target_row,
	 * presentation_asset, and verification_mode are required.
	 * Example: {"protocol_version":"1.0","action":"create_spell_variation","options":{"dry_run":true},"specification":{"source_binding":{"ability_table":"/Game/Data/DT_Abilities","source_row":"FireBolt","vfx_phase":"projectile"},"target_row":"IceBoltVisual","presentation_asset":"/Game/VFX/NS_IceBolt","verification_mode":"protected_fields_equal"}}
	 *
	 * @param RequestJson Request envelope with action=create_spell_variation.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Gameplay")
	static FString CreateSpellVariation(const FString& RequestJson);
};
