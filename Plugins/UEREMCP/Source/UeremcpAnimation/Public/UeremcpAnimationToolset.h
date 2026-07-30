// UEREMCP — Animation domain toolset (WS-10).

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "UeremcpAnimationToolset.generated.h"

/** Goal-level animation operations. Epic animation primitives remain internal. */
UCLASS()
class UEREMCPANIMATION_API UUeremcpAnimationToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	virtual FString GetToolsetVersion() const override { return TEXT("0.1.0"); }

	/**
	 * Inspects one AnimMontage in a single call: skeleton, slots, segments, sections,
	 * real notify objects, root-motion source flags, dependencies and revision.
	 *
	 * Until WS-01 accepts the non-graph asset-state response proposal, the tool
	 * returns an honest partial response with counts and revision; the domain
	 * service already produces the complete structured state.
	 *
	 * @param RequestJson Request envelope with action inspect_montage and target.asset_path.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Animation")
	static FString InspectMontage(const FString& RequestJson);

	/**
	 * Read-only AnimBlueprint graph retrieval in one call: skeleton, graph identity,
	 * ADR-0004 nodes/pins/links, diagnostics, extensions.animation and revision.
	 *
	 * Uses the shared UEdGraph reader; Blueprint DSL and AnimGraph/state-machine
	 * authoring remain unsupported.
	 *
	 * @param RequestJson Request envelope with action read_anim_bp and target.asset_path.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Animation")
	static FString ReadAnimBp(const FString& RequestJson);
};
