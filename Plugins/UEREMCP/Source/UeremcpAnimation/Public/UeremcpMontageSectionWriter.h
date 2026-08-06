// UEREMCP — montage section authoring (WS-10).
//
// The Animation domain was read-only before this: InspectMontage and ReadAnimBp with
// no write path at all. This is the first mutation, deliberately scoped to montage
// composite sections — complete-state submission per ADR-0004, not per-section pokes.
//
// [VERIFIED: Engine/Classes/Animation/AnimMontage.h:697,819,906,912]
// [VERIFIED: Engine/Classes/Animation/AnimLinkableElement.h:77,83]

#pragma once

#include "CoreMinimal.h"

class UAnimMontage;

/** One desired section in the submitted complete state. */
struct UEREMCPANIMATION_API FUeremcpMontageSectionSpec
{
	/** FCompositeSection::SectionName [VERIFIED: AnimMontage.h:42]. Must be unique. */
	FString Name;

	/** Absolute start time in seconds; must lie within the montage play length. */
	float StartTime = 0.f;

	/**
	 * FCompositeSection::NextSectionName [VERIFIED: AnimMontage.h:53]. Empty means the
	 * section does not chain. Must name another submitted section when set.
	 */
	FString NextSection;

	bool bHasNextSection = false;
};

struct UEREMCPANIMATION_API FUeremcpMontageSectionPlan
{
	TArray<FString> ToAdd;
	TArray<FString> ToRemove;
	/** Present in both, but start time or next section differs. */
	TArray<FString> ToUpdate;
	/** Present in both and already identical. */
	TArray<FString> Unchanged;

	bool HasChanges() const
	{
		return ToAdd.Num() > 0 || ToRemove.Num() > 0 || ToUpdate.Num() > 0;
	}
};

struct UEREMCPANIMATION_API FUeremcpMontageSectionWriteResult
{
	FUeremcpMontageSectionPlan Plan;

	/** True when the write was applied rather than planned. */
	bool bApplied = false;

	/** True when a post-write re-read confirmed every submitted section (rule 6). */
	bool bRereadVerified = false;

	bool bSaved = false;

	/** Content hash after the write; empty when dry run. */
	FString Revision;

	/** Per-section reasons a submitted section did not verify. */
	TArray<FString> VerificationFailures;

	TArray<FString> Warnings;
};

class UEREMCPANIMATION_API FUeremcpMontageSectionWriter
{
public:
	/**
	 * Validates a submitted section set against the montage without mutating anything.
	 * Checks uniqueness, non-empty names, start times inside the play length, and that
	 * every next_section names a submitted section.
	 */
	static bool ValidateSpecs(
		const UAnimMontage* Montage,
		const TArray<FUeremcpMontageSectionSpec>& Specs,
		FString& OutError);

	/** Diffs submitted state against current sections. Pure; no mutation. */
	static void BuildPlan(
		const UAnimMontage* Montage,
		const TArray<FUeremcpMontageSectionSpec>& Specs,
		FUeremcpMontageSectionPlan& OutPlan);

	/**
	 * Replaces the montage's composite sections with the submitted set, then re-reads
	 * to confirm. Sections absent from Specs are removed — this is complete-state
	 * replacement, so callers must have gated on dry_run first.
	 *
	 * @param bSave Save the package after a successful write.
	 */
	static bool Apply(
		UAnimMontage* Montage,
		const TArray<FUeremcpMontageSectionSpec>& Specs,
		bool bSave,
		FUeremcpMontageSectionWriteResult& OutResult,
		FString& OutError);
};
