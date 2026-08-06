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
	 * Use when: inspect montage sections, notify tracks, timing, slots, or root motion.
	 * Inputs: action=inspect_montage and target.asset_path; specification has no required keys.
	 * Example: {"protocol_version":"1.0","action":"inspect_montage","target":{"asset_path":"/Game/Animation/AM_SwordCombo"},"specification":{}}
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
	 * Use when: read an Animation Blueprint state machine, transition graph, or diagnostics.
	 * Inputs: action=read_anim_bp and target.asset_path; specification has no required keys.
	 * Example: {"protocol_version":"1.0","action":"read_anim_bp","target":{"asset_path":"/Game/Characters/ABP_Rogue"},"specification":{}}
	 *
	 * Uses the shared UEdGraph reader; Blueprint DSL and AnimGraph/state-machine
	 * authoring remain unsupported.
	 *
	 * @param RequestJson Request envelope with action read_anim_bp and target.asset_path.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Animation")
	static FString ReadAnimBp(const FString& RequestJson);

	/**
	 * Replace a montage's section list from complete submitted state (ADR-0004).
	 *
	 * The Animation domain's first write path — InspectMontage and ReadAnimBp are
	 * read-only.
	 *
	 * Use when: define montage sections and their chaining (combo windows, loop
	 * segments, hit windows) in one call.
	 * Inputs: action=submit_montage_sections, target.asset_path to a UAnimMontage,
	 * specification.sections[{name, start_time, next_section?}] — the COMPLETE desired
	 * set; sections absent from it are removed. options.dry_run defaults TRUE because
	 * removal is destructive (ADR-0010); pass dry_run:false to write.
	 * options.save controls package save. expected_revision is honoured when supplied.
	 * Outputs: modified_and_validated only after a post-write re-read confirms every
	 * section's start time and chaining; otherwise failed_validation with the specific
	 * mismatches. Dry run returns no_change_required with diagnostics.montage_sections.plan.
	 * Do not use for: notifies, slots or segments — not covered; use Epic AnimationTools.
	 * Next tool: InspectMontage to verify independently.
	 * Example: {"protocol_version":"1.0","action":"submit_montage_sections","target":{"asset_path":"/Game/Anim/AM_Attack"},"options":{"dry_run":true},"specification":{"sections":[{"name":"Windup","start_time":0.0,"next_section":"Strike"},{"name":"Strike","start_time":0.35}]}}
	 *
	 * [VERIFIED: Engine/Classes/Animation/AnimMontage.h:697,819,906,912]
	 * [VERIFIED: Engine/Classes/Animation/AnimLinkableElement.h:77,83]
	 *
	 * @param RequestJson Request envelope; target.asset_path and specification.sections required.
	 * @return Response envelope with diagnostics.montage_sections.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Animation")
	static FString SubmitMontageSections(const FString& RequestJson);
};
