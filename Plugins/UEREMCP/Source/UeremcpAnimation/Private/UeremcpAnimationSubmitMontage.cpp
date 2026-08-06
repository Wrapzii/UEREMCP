// UEREMCP — submit_montage_sections toolset entry point (WS-10).
//
// Kept out of UeremcpAnimationToolset.cpp so the domain's first write path is
// reviewable on its own; the read tools stay untouched.

#include "UeremcpAnimationToolset.h"

#include "Animation/AnimMontage.h"
#include "Misc/PackageName.h"
#include "UeremcpAnimationService.h"
#include "UeremcpEnvelope.h"
#include "UeremcpMontageSectionWriter.h"

namespace UeremcpSubmitMontage
{
	/** Mirrors the helper in UeremcpAnimationToolset.cpp; package name → object path. */
	static FString ResolveObjectPath(const FString& AssetPath)
	{
		if (FPackageName::IsValidObjectPath(AssetPath))
		{
			return AssetPath;
		}
		if (FPackageName::IsValidLongPackageName(AssetPath))
		{
			return FString::Printf(
				TEXT("%s.%s"),
				*AssetPath,
				*FPackageName::GetLongPackageAssetName(AssetPath));
		}
		return AssetPath;
	}

	static TArray<TSharedPtr<FJsonValue>> ToJsonStringArray(const TArray<FString>& Values)
	{
		TArray<TSharedPtr<FJsonValue>> Json;
		for (const FString& Value : Values)
		{
			Json.Add(MakeShared<FJsonValueString>(Value));
		}
		return Json;
	}

	static TSharedPtr<FJsonObject> PlanToJson(const FUeremcpMontageSectionPlan& Plan)
	{
		TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetArrayField(TEXT("to_add"), ToJsonStringArray(Plan.ToAdd));
		Json->SetArrayField(TEXT("to_update"), ToJsonStringArray(Plan.ToUpdate));
		Json->SetArrayField(TEXT("to_remove"), ToJsonStringArray(Plan.ToRemove));
		Json->SetArrayField(TEXT("unchanged"), ToJsonStringArray(Plan.Unchanged));
		Json->SetBoolField(TEXT("has_changes"), Plan.HasChanges());
		return Json;
	}

	static const TArray<FString>& CapabilityNotes()
	{
		static const TArray<FString> Notes = {
			TEXT("montage_sections.complete_state_replacement_unlisted_sections_are_removed"),
			TEXT("montage_sections.dry_run_defaults_true_because_removal_is_destructive"),
			TEXT("montage_sections.notifies_slots_and_segments_are_not_covered"),
			TEXT("montage_sections.modified_and_validated_requires_post_write_reread"),
		};
		return Notes;
	}
}

FString UUeremcpAnimationToolset::SubmitMontageSections(const FString& RequestJson)
{
	using namespace UeremcpSubmitMontage;

	FUeremcpRequest Request;
	FString ParseError;
	if (!FUeremcpEnvelope::ParseRequest(RequestJson, Request, ParseError))
	{
		return FUeremcpEnvelope::MakeRejection(
			FString(),
			FString::Printf(TEXT("Malformed request envelope: %s"), *ParseError));
	}
	if (!FUeremcpEnvelope::IsProtocolCompatible(Request.ProtocolVersion))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(
				TEXT("Unsupported protocol_version '%s'; this server speaks %s."),
				*Request.ProtocolVersion,
				*FUeremcpEnvelope::ProtocolVersion()));
	}
	if (!Request.Action.Equals(TEXT("submit_montage_sections"), ESearchCase::CaseSensitive))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(
				TEXT("SubmitMontageSections received action '%s'; expected 'submit_montage_sections'."),
				*Request.Action));
	}
	if (Request.TargetAssetPath.IsEmpty())
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			TEXT("submit_montage_sections requires target.asset_path."));
	}

	const TArray<TSharedPtr<FJsonValue>>* SectionValues = nullptr;
	if (!Request.Specification.IsValid()
		|| !Request.Specification->TryGetArrayField(TEXT("sections"), SectionValues)
		|| SectionValues->Num() == 0)
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			TEXT("submit_montage_sections requires a non-empty specification.sections array "
				 "carrying the COMPLETE desired section set."));
	}

	TArray<FUeremcpMontageSectionSpec> Specs;
	for (const TSharedPtr<FJsonValue>& SectionValue : *SectionValues)
	{
		const TSharedPtr<FJsonObject> SectionObject = SectionValue.IsValid()
			? SectionValue->AsObject()
			: nullptr;
		if (!SectionObject.IsValid())
		{
			return FUeremcpEnvelope::MakeRejection(
				Request.RequestId,
				TEXT("every entry in specification.sections must be an object."));
		}

		FUeremcpMontageSectionSpec Spec;
		if (!SectionObject->TryGetStringField(TEXT("name"), Spec.Name) || Spec.Name.IsEmpty())
		{
			return FUeremcpEnvelope::MakeRejection(
				Request.RequestId,
				TEXT("every section requires a non-empty 'name'."));
		}

		double StartTime = 0.0;
		if (SectionObject->TryGetNumberField(TEXT("start_time"), StartTime))
		{
			Spec.StartTime = static_cast<float>(StartTime);
		}

		if (SectionObject->TryGetStringField(TEXT("next_section"), Spec.NextSection))
		{
			Spec.bHasNextSection = true;
		}

		Specs.Add(MoveTemp(Spec));
	}

	const FString ObjectPath = ResolveObjectPath(Request.TargetAssetPath);
	const FString AssetPath = FPackageName::ObjectPathToPackageName(ObjectPath);
	UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *ObjectPath);
	if (!Montage)
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(
				TEXT("Target '%s' is missing or is not an AnimMontage."),
				*Request.TargetAssetPath));
	}

	// ADR-0006 conflict check, before anything is touched.
	if (Request.bHasExpectedRevision && !Request.ExpectedRevision.IsEmpty())
	{
		FUeremcpMontageInspection Current;
		FString CurrentError;
		if (FUeremcpAnimationService::InspectMontage(Montage, AssetPath, Current, CurrentError)
			&& !Current.ContentHash.Equals(Request.ExpectedRevision, ESearchCase::CaseSensitive))
		{
			return FUeremcpEnvelope::MakeRejection(
				Request.RequestId,
				FString::Printf(
					TEXT("expected_revision '%s' does not match current revision '%s'; "
						 "re-read with InspectMontage and resubmit."),
					*Request.ExpectedRevision,
					*Current.ContentHash));
		}
	}

	FString ValidateError;
	if (!FUeremcpMontageSectionWriter::ValidateSpecs(Montage, Specs, ValidateError))
	{
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, ValidateError);
	}

	FUeremcpResponse Response;
	Response.RequestId = Request.RequestId;
	Response.UnderstoodAction = Request.Action;
	Response.UnderstoodTarget = AssetPath;
	Response.PrimaryAsset = AssetPath;
	Response.CapabilityNotes = CapabilityNotes();
	Response.Metrics.McpRoundTrips = 1;
	Response.ExtraFields = MakeShared<FJsonObject>();

	// Destructive by construction — unlisted sections are removed — so dry_run is the
	// default and the caller opts in explicitly (AGENTS.md rule 8, ADR-0010).
	if (Request.bDryRun)
	{
		FUeremcpMontageSectionPlan Plan;
		FUeremcpMontageSectionWriter::BuildPlan(Montage, Specs, Plan);

		Response.Status = TEXT("no_change_required");
		Response.Summary = FString::Printf(
			TEXT("Dry run on '%s': %d to add, %d to update, %d to remove, %d unchanged. "
				 "No mutation performed; resubmit with options.dry_run=false to apply."),
			*Request.TargetAssetPath,
			Plan.ToAdd.Num(),
			Plan.ToUpdate.Num(),
			Plan.ToRemove.Num(),
			Plan.Unchanged.Num());
		Response.Metrics.InternalOperations = 1;

		TSharedPtr<FJsonObject> Sections = MakeShared<FJsonObject>();
		Sections->SetObjectField(TEXT("plan"), PlanToJson(Plan));
		Sections->SetBoolField(TEXT("applied"), false);

		TSharedPtr<FJsonObject> Diagnostics = MakeShared<FJsonObject>();
		Diagnostics->SetObjectField(TEXT("montage_sections"), Sections);
		Response.ExtraFields->SetObjectField(TEXT("diagnostics"), Diagnostics);

		TSharedPtr<FJsonObject> Validation = MakeShared<FJsonObject>();
		Validation->SetBoolField(TEXT("structurally_valid"), true);
		Validation->SetArrayField(
			TEXT("checks_performed"),
			ToJsonStringArray({
				TEXT("animation.montage_sections.specs_validated"),
				TEXT("animation.montage_sections.plan_built"),
			}));
		Validation->SetArrayField(
			TEXT("checks_skipped"),
			ToJsonStringArray({ TEXT("animation.montage_sections.reread_after_write") }));
		Response.ExtraFields->SetObjectField(TEXT("validation"), Validation);

		return FUeremcpEnvelope::SerializeResponse(Response);
	}

	FUeremcpMontageSectionWriteResult WriteResult;
	FString ApplyError;
	if (!FUeremcpMontageSectionWriter::Apply(Montage, Specs, Request.bSave, WriteResult, ApplyError))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			ApplyError.IsEmpty() ? TEXT("submit_montage_sections failed") : ApplyError);
	}

	Response.Status = WriteResult.bRereadVerified
		? TEXT("modified_and_validated")
		: TEXT("failed_validation");
	Response.Summary = WriteResult.bRereadVerified
		? FString::Printf(
			TEXT("Montage '%s' now has %d section(s): %d added, %d updated, %d removed. "
				 "Confirmed by post-write re-read.%s"),
			*Request.TargetAssetPath,
			Specs.Num(),
			WriteResult.Plan.ToAdd.Num(),
			WriteResult.Plan.ToUpdate.Num(),
			WriteResult.Plan.ToRemove.Num(),
			WriteResult.bSaved ? TEXT(" Package saved.") : TEXT(" Package NOT saved."))
		: FString::Printf(
			TEXT("Montage '%s' was modified but the post-write re-read did not confirm it: %s"),
			*Request.TargetAssetPath,
			*FString::Join(WriteResult.VerificationFailures, TEXT("; ")));
	Response.Revision = WriteResult.Revision;
	Response.Metrics.InternalOperations = Specs.Num() + WriteResult.Plan.ToRemove.Num();
	Response.Metrics.AssetsAffected = 1;

	{
		FUeremcpAssetRef Modified;
		Modified.AssetPath = AssetPath;
		Modified.AssetClass = TEXT("/Script/Engine.AnimMontage");
		Modified.Revision = WriteResult.Revision;
		Modified.Role = TEXT("montage");
		Response.ModifiedAssets.Add(MoveTemp(Modified));
	}

	for (const FString& Warning : WriteResult.Warnings)
	{
		Response.InterpretationNotes.Add(Warning);
	}

	TSharedPtr<FJsonObject> Sections = MakeShared<FJsonObject>();
	Sections->SetObjectField(TEXT("plan"), PlanToJson(WriteResult.Plan));
	Sections->SetBoolField(TEXT("applied"), WriteResult.bApplied);
	Sections->SetBoolField(TEXT("saved"), WriteResult.bSaved);
	Sections->SetArrayField(
		TEXT("verification_failures"),
		ToJsonStringArray(WriteResult.VerificationFailures));

	TSharedPtr<FJsonObject> Diagnostics = MakeShared<FJsonObject>();
	Diagnostics->SetObjectField(TEXT("montage_sections"), Sections);
	Response.ExtraFields->SetObjectField(TEXT("diagnostics"), Diagnostics);

	TSharedPtr<FJsonObject> Validation = MakeShared<FJsonObject>();
	Validation->SetBoolField(TEXT("structurally_valid"), WriteResult.bRereadVerified);
	Validation->SetBoolField(TEXT("reread_after_write"), WriteResult.bRereadVerified);
	Validation->SetArrayField(
		TEXT("checks_performed"),
		ToJsonStringArray({
			TEXT("animation.montage_sections.specs_validated"),
			TEXT("animation.montage_sections.plan_built"),
			TEXT("animation.montage_sections.sections_written"),
			TEXT("animation.montage_sections.reread_after_write"),
		}));
	Response.ExtraFields->SetObjectField(TEXT("validation"), Validation);

	return FUeremcpEnvelope::SerializeResponse(Response);
}
