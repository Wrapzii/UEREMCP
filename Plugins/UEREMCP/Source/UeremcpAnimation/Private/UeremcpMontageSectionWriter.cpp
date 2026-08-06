#include "UeremcpMontageSectionWriter.h"

#include "Animation/AnimMontage.h"
#include "UeremcpAnimationService.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

namespace UeremcpMontageSections
{
	static const FUeremcpMontageSectionSpec* FindSpec(
		const TArray<FUeremcpMontageSectionSpec>& Specs,
		const FString& Name)
	{
		for (const FUeremcpMontageSectionSpec& Spec : Specs)
		{
			if (Spec.Name.Equals(Name, ESearchCase::CaseSensitive))
			{
				return &Spec;
			}
		}
		return nullptr;
	}

	/** Tolerance for start-time comparison; montage times are float seconds. */
	static constexpr float TimeTolerance = 0.0001f;
}

bool FUeremcpMontageSectionWriter::ValidateSpecs(
	const UAnimMontage* Montage,
	const TArray<FUeremcpMontageSectionSpec>& Specs,
	FString& OutError)
{
	if (!Montage)
	{
		OutError = TEXT("montage is null");
		return false;
	}

	if (Specs.Num() == 0)
	{
		// A montage with no sections is not playable; refuse rather than brick the asset.
		OutError = TEXT("specification.sections must contain at least one section; a montage with no sections cannot be played.");
		return false;
	}

	// [VERIFIED: Engine/Classes/Animation/AnimSequenceBase.h:86]
	const float PlayLength = Montage->GetPlayLength();

	TSet<FString> SeenNames;
	for (const FUeremcpMontageSectionSpec& Spec : Specs)
	{
		if (Spec.Name.IsEmpty())
		{
			OutError = TEXT("every section requires a non-empty name.");
			return false;
		}
		if (SeenNames.Contains(Spec.Name))
		{
			OutError = FString::Printf(
				TEXT("duplicate section name '%s'; AddAnimCompositeSection requires unique names."),
				*Spec.Name);
			return false;
		}
		SeenNames.Add(Spec.Name);

		if (Spec.StartTime < 0.f)
		{
			OutError = FString::Printf(
				TEXT("section '%s' has negative start_time %f."),
				*Spec.Name,
				Spec.StartTime);
			return false;
		}
		if (PlayLength > 0.f && Spec.StartTime > PlayLength)
		{
			OutError = FString::Printf(
				TEXT("section '%s' start_time %f exceeds montage play length %f."),
				*Spec.Name,
				Spec.StartTime,
				PlayLength);
			return false;
		}
	}

	for (const FUeremcpMontageSectionSpec& Spec : Specs)
	{
		if (Spec.bHasNextSection
			&& !Spec.NextSection.IsEmpty()
			&& !SeenNames.Contains(Spec.NextSection))
		{
			OutError = FString::Printf(
				TEXT("section '%s' chains to '%s', which is not in the submitted set; "
					 "complete-state submission means the chain target must be submitted too."),
				*Spec.Name,
				*Spec.NextSection);
			return false;
		}
	}

	return true;
}

void FUeremcpMontageSectionWriter::BuildPlan(
	const UAnimMontage* Montage,
	const TArray<FUeremcpMontageSectionSpec>& Specs,
	FUeremcpMontageSectionPlan& OutPlan)
{
	using namespace UeremcpMontageSections;

	if (!Montage)
	{
		return;
	}

	TSet<FString> SubmittedNames;
	for (const FUeremcpMontageSectionSpec& Spec : Specs)
	{
		SubmittedNames.Add(Spec.Name);
	}

	// [VERIFIED: Engine/Classes/Animation/AnimMontage.h:697]
	TSet<FString> ExistingNames;
	for (const FCompositeSection& Section : Montage->CompositeSections)
	{
		const FString ExistingName = Section.SectionName.ToString();
		ExistingNames.Add(ExistingName);

		const FUeremcpMontageSectionSpec* Spec = FindSpec(Specs, ExistingName);
		if (!Spec)
		{
			OutPlan.ToRemove.Add(ExistingName);
			continue;
		}

		// [VERIFIED: Engine/Classes/Animation/AnimLinkableElement.h:77]
		const float ExistingStart = Section.GetTime(EAnimLinkMethod::Absolute);
		const FString ExistingNext = Section.NextSectionName.IsNone()
			? FString()
			: Section.NextSectionName.ToString();
		const FString DesiredNext = Spec->bHasNextSection ? Spec->NextSection : FString();

		const bool bTimeDiffers = !FMath::IsNearlyEqual(ExistingStart, Spec->StartTime, TimeTolerance);
		const bool bNextDiffers = !ExistingNext.Equals(DesiredNext, ESearchCase::CaseSensitive);

		if (bTimeDiffers || bNextDiffers)
		{
			OutPlan.ToUpdate.Add(ExistingName);
		}
		else
		{
			OutPlan.Unchanged.Add(ExistingName);
		}
	}

	for (const FUeremcpMontageSectionSpec& Spec : Specs)
	{
		if (!ExistingNames.Contains(Spec.Name))
		{
			OutPlan.ToAdd.Add(Spec.Name);
		}
	}
}

bool FUeremcpMontageSectionWriter::Apply(
	UAnimMontage* Montage,
	const TArray<FUeremcpMontageSectionSpec>& Specs,
	bool bSave,
	FUeremcpMontageSectionWriteResult& OutResult,
	FString& OutError)
{
	using namespace UeremcpMontageSections;

	if (!ValidateSpecs(Montage, Specs, OutError))
	{
		return false;
	}

	BuildPlan(Montage, Specs, OutResult.Plan);

	Montage->Modify();

	// Remove first, descending, so surviving indices stay valid.
	// [VERIFIED: Engine/Classes/Animation/AnimMontage.h:912]
	for (int32 Index = Montage->CompositeSections.Num() - 1; Index >= 0; --Index)
	{
		const FString ExistingName = Montage->CompositeSections[Index].SectionName.ToString();
		if (!FindSpec(Specs, ExistingName))
		{
			if (!Montage->DeleteAnimCompositeSection(Index))
			{
				OutResult.Warnings.Add(FString::Printf(
					TEXT("DeleteAnimCompositeSection failed for '%s' at index %d."),
					*ExistingName,
					Index));
			}
		}
	}

	// Add anything missing. [VERIFIED: Engine/Classes/Animation/AnimMontage.h:906]
	for (const FUeremcpMontageSectionSpec& Spec : Specs)
	{
		// [VERIFIED: Engine/Classes/Animation/AnimMontage.h:819]
		if (Montage->GetSectionIndex(FName(*Spec.Name)) != INDEX_NONE)
		{
			continue;
		}
		if (Montage->AddAnimCompositeSection(FName(*Spec.Name), Spec.StartTime) == INDEX_NONE)
		{
			OutError = FString::Printf(
				TEXT("AddAnimCompositeSection returned INDEX_NONE for '%s'; the name is not unique."),
				*Spec.Name);
			return false;
		}
	}

	// Then set times and chaining on the full submitted set. Adding can reorder the
	// array, so this pass looks each section up by name rather than caching indices.
	for (const FUeremcpMontageSectionSpec& Spec : Specs)
	{
		const int32 Index = Montage->GetSectionIndex(FName(*Spec.Name));
		if (Index == INDEX_NONE)
		{
			OutResult.Warnings.Add(FString::Printf(
				TEXT("section '%s' vanished between add and update."),
				*Spec.Name));
			continue;
		}

		FCompositeSection& Section = Montage->CompositeSections[Index];
		// [VERIFIED: Engine/Classes/Animation/AnimLinkableElement.h:83]
		Section.SetTime(Spec.StartTime, EAnimLinkMethod::Absolute);
		Section.NextSectionName = (Spec.bHasNextSection && !Spec.NextSection.IsEmpty())
			? FName(*Spec.NextSection)
			: NAME_None;
	}

	Montage->PostEditChange();
	Montage->MarkPackageDirty();
	OutResult.bApplied = true;

	// Rule 6: re-read and confirm rather than trusting the setters returned.
	OutResult.bRereadVerified = true;
	for (const FUeremcpMontageSectionSpec& Spec : Specs)
	{
		const int32 Index = Montage->GetSectionIndex(FName(*Spec.Name));
		if (Index == INDEX_NONE)
		{
			OutResult.VerificationFailures.Add(FString::Printf(
				TEXT("section '%s' is absent after write."),
				*Spec.Name));
			OutResult.bRereadVerified = false;
			continue;
		}

		const FCompositeSection& Section = Montage->CompositeSections[Index];
		const float ActualStart = Section.GetTime(EAnimLinkMethod::Absolute);
		if (!FMath::IsNearlyEqual(ActualStart, Spec.StartTime, TimeTolerance))
		{
			OutResult.VerificationFailures.Add(FString::Printf(
				TEXT("section '%s' start_time is %f after write, expected %f."),
				*Spec.Name,
				ActualStart,
				Spec.StartTime));
			OutResult.bRereadVerified = false;
		}

		const FString ActualNext = Section.NextSectionName.IsNone()
			? FString()
			: Section.NextSectionName.ToString();
		const FString DesiredNext = Spec.bHasNextSection ? Spec.NextSection : FString();
		if (!ActualNext.Equals(DesiredNext, ESearchCase::CaseSensitive))
		{
			OutResult.VerificationFailures.Add(FString::Printf(
				TEXT("section '%s' next_section is '%s' after write, expected '%s'."),
				*Spec.Name,
				*ActualNext,
				*DesiredNext));
			OutResult.bRereadVerified = false;
		}
	}

	// Removals must also have taken effect.
	for (const FString& Removed : OutResult.Plan.ToRemove)
	{
		if (Montage->GetSectionIndex(FName(*Removed)) != INDEX_NONE)
		{
			OutResult.VerificationFailures.Add(FString::Printf(
				TEXT("section '%s' still present after removal."),
				*Removed));
			OutResult.bRereadVerified = false;
		}
	}

	// Fresh content hash from the same service read_montage uses, so the revision the
	// caller gets back is comparable with an InspectMontage revision.
	if (UPackage* Package = Montage->GetOutermost())
	{
		FUeremcpMontageInspection Inspection;
		FString InspectError;
		if (FUeremcpAnimationService::InspectMontage(
				Montage, Package->GetName(), Inspection, InspectError))
		{
			OutResult.Revision = Inspection.ContentHash;
		}
		else
		{
			OutResult.Warnings.Add(FString::Printf(
				TEXT("post-write inspection failed, revision withheld: %s"),
				*InspectError));
		}

		if (bSave && OutResult.bRereadVerified)
		{
			const FString FileName = FPackageName::LongPackageNameToFilename(
				Package->GetName(), FPackageName::GetAssetPackageExtension());
			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
			SaveArgs.SaveFlags = SAVE_NoError;
			OutResult.bSaved = UPackage::SavePackage(Package, nullptr, *FileName, SaveArgs);
			if (!OutResult.bSaved)
			{
				OutResult.Warnings.Add(TEXT("SavePackage failed; the change is in memory but not on disk."));
			}
		}
	}

	return true;
}
