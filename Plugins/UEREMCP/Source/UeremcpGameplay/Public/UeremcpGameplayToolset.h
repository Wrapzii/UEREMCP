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
	 * Preflight an RE-native create_spell request.
	 *
	 * Returns the normalized FREAbilityDef row plan and static Pattern B checks in
	 * one response. Until the ADR-0010 mutator queue is implemented, this tool
	 * reports partially_completed and performs no asset mutation.
	 *
	 * @param RequestJson Request envelope with action=create_spell.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Gameplay")
	static FString CreateSpell(const FString& RequestJson);
};
