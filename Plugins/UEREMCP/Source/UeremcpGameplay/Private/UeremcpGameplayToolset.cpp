#include "UeremcpGameplayToolset.h"

#include "Dom/JsonValue.h"
#include "UeremcpAbilityVariation.h"
#include "UeremcpAbilityTableMutator.h"
#include "UeremcpEnvelope.h"
#include "UeremcpIdempotency.h"
#include "UeremcpMutatingDispatch.h"
#include "UeremcpPathPolicy.h"
#include "UeremcpSpellPlanner.h"
#include "UObject/UObjectGlobals.h"

namespace
{
TArray<TSharedPtr<FJsonValue>> StringValues(const TArray<FString>& Values)
{
	TArray<TSharedPtr<FJsonValue>> Result;
	Result.Reserve(Values.Num());
	for (const FString& Value : Values)
	{
		Result.Add(MakeShared<FJsonValueString>(Value));
	}
	return Result;
}

bool IsVerifiedTerminalStatus(const FString& Status)
{
	return Status == TEXT("created_and_validated")
		|| Status == TEXT("modified_and_validated")
		|| Status == TEXT("no_change_required");
}

void SetJsonNull(const TSharedPtr<FJsonObject>& Object, const FStringView Field)
{
	// UE 5.8 expresses JSON null through generic SetField.
	// [VERIFIED: JsonObject.h:127-135; JsonValue.h FJsonValueNull]
	Object->SetField(Field, MakeShared<FJsonValueNull>());
}
}

FString UUeremcpGameplayToolset::CreateSpell(const FString& RequestJson)
{
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
	if (Request.Action != TEXT("create_spell"))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(TEXT("CreateSpell received action '%s'; expected 'create_spell'."), *Request.Action));
	}
	if (Request.Mode != TEXT("create") && Request.Mode != TEXT("create_or_update"))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			TEXT("create_spell currently supports mode create or create_or_update only."));
	}
	if (Request.TargetAssetPath.IsEmpty())
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			TEXT("create_spell requires target.asset_path naming the FREAbilityDef DataTable."));
	}
	if (Request.RequestId.IsEmpty())
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			TEXT("create_spell requires request_id so the shared mutator queue can own and release the future write."));
	}

	// Shared ADR-0010 path gate; domains do not fork path policy.
	const FUeremcpPathValidationResult PathResult =
		FUeremcpPathPolicy::ValidateSoftPath(Request.TargetAssetPath, true);
	if (!PathResult.bAllowed)
	{
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, PathResult.Reason);
	}
	if (!Request.TargetAssetPath.StartsWith(TEXT("/Game/__UeremcpTests/")))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			TEXT("create_spell preflight is restricted to /Game/__UeremcpTests/ until production DataTable policy is approved."));
	}

	FUeremcpSpellPlan Plan;
	FString PlanError;
	if (!FUeremcpSpellPlanner::BuildPlan(Request.Specification, Plan, PlanError))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(TEXT("Invalid create_spell specification: %s"), *PlanError));
	}
	for (const FString& DependencyPath : Plan.DependencyAssetPaths)
	{
		const FUeremcpPathValidationResult DependencyPathResult =
			FUeremcpPathPolicy::ValidateSoftPath(DependencyPath, false);
		if (!DependencyPathResult.bAllowed)
		{
			return FUeremcpEnvelope::MakeRejection(
				Request.RequestId,
				FString::Printf(
					TEXT("Invalid presentation dependency '%s': %s"),
					*DependencyPath,
					*DependencyPathResult.Reason));
		}
	}
	TArray<FUeremcpAssetRef> ResolvedDependencies;
	TArray<FUeremcpAssetRef> UnresolvedDependencies;
	for (const FString& DependencyPath : Plan.DependencyAssetPaths)
	{
		FUeremcpAssetRef Dependency;
		Dependency.AssetPath = DependencyPath;
		Dependency.Role = TEXT("spell_presentation");
		UObject* DependencyObject = StaticLoadObject(
			UObject::StaticClass(),
			nullptr,
			*DependencyPath,
			nullptr,
			LOAD_NoWarn);
		if (DependencyObject)
		{
			Dependency.AssetClass = DependencyObject->GetClass()->GetPathName();
			ResolvedDependencies.Add(MoveTemp(Dependency));
		}
		else
		{
			UnresolvedDependencies.Add(MoveTemp(Dependency));
		}
	}
	const bool bDependenciesResolved = UnresolvedDependencies.IsEmpty();

	FUeremcpAbilityTableWriteOptions WriteOptions;
	WriteOptions.RequestId = Request.RequestId;
	WriteOptions.Mode = Request.Mode;
	WriteOptions.bDryRun = Request.bDryRun;
	WriteOptions.bAtomic = Request.bAtomic;
	WriteOptions.bSave = Request.bSave;
	WriteOptions.bValidate = Request.bValidate;
	WriteOptions.bRollbackOnFailure = Request.bRollbackOnFailure;
	WriteOptions.TimeoutMs = Request.TimeoutMs;
	WriteOptions.OnRevisionConflict = Request.OnRevisionConflict;
	WriteOptions.ExpectedRevision = Request.ExpectedRevision;
	WriteOptions.bHasExpectedRevision = Request.bHasExpectedRevision;
	WriteOptions.IdempotencyKey = Request.IdempotencyKey;

	FUeremcpAbilityTableWritePlan WritePlan;
	if (!FUeremcpSpellPlanner::BuildTableWritePlan(
		Request.TargetAssetPath,
		WriteOptions,
		Plan,
		WritePlan,
		PlanError))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(TEXT("Invalid ability-table write plan: %s"), *PlanError));
	}

	// Session-scoped ADR-0006 replay via Protocol store
	// [VERIFIED: UeremcpIdempotency.h:15-35]. Disk durability remains WS-05.
	// Dry-run never reads or writes the store so planning cannot poison retries.
	if (!Request.bDryRun && !Request.IdempotencyKey.IsEmpty())
	{
		FString ReplayJson;
		if (FUeremcpIdempotencyStore::Get().TryGetReplay(
			Request.IdempotencyKey,
			Request.RequestId,
			ReplayJson))
		{
			return ReplayJson;
		}
	}

	FUeremcpMutatingDispatch MutatingDispatch;
	if (!Request.bDryRun)
	{
		FString BlockingResponse;
		if (!MutatingDispatch.TryBegin(
			RequestJson,
			false,
			0,
			false,
			BlockingResponse))
		{
			return BlockingResponse;
		}
	}

	FUeremcpAbilityTableMutationResult MutationResult;
	FString MutationError;
	const bool bMutationSucceeded =
		Request.bDryRun
			|| (bDependenciesResolved
				&& FUeremcpAbilityTableMutator::Execute(
				WritePlan,
				Plan,
				MutationResult,
				MutationError));
	if (!Request.bDryRun && !bDependenciesResolved)
	{
		MutationError = FString::Printf(
			TEXT("%d presentation dependency asset(s) could not be loaded"),
			UnresolvedDependencies.Num());
	}

	FUeremcpResponse Response;
	Response.RequestId = Request.RequestId;
	if (Request.bDryRun)
	{
		Response.Status = TEXT("partially_completed");
		Response.Summary = FString::Printf(
			TEXT("Prepared and statically validated RE spell row '%s' for '%s'; dry_run performed no mutation."),
			*Plan.RowName,
			*Request.TargetAssetPath);
	}
	else if (bMutationSucceeded && MutationResult.bNoChange)
	{
		Response.Status = TEXT("no_change_required");
		Response.Summary = FString::Printf(
			TEXT("RE spell row '%s' already matched the normalized specification."),
			*Plan.RowName);
	}
	else if (bMutationSucceeded && MutationResult.bCreatedTable)
	{
		Response.Status = TEXT("created_and_validated");
		Response.Summary = FString::Printf(
			TEXT("Created, saved, re-read, and validated RE spell row '%s'."),
			*Plan.RowName);
	}
	else if (bMutationSucceeded)
	{
		Response.Status = TEXT("modified_and_validated");
		Response.Summary = FString::Printf(
			TEXT("Updated, saved, re-read, and validated RE spell row '%s'."),
			*Plan.RowName);
	}
	else if (MutationResult.bPersisted)
	{
		Response.Status = TEXT("created_with_warnings");
		Response.Summary = FString::Printf(
			TEXT("RE spell row persisted with a terminal warning: %s"),
			*MutationError);
	}
	else if (MutationResult.bRolledBack)
	{
		Response.Status = TEXT("rolled_back");
		Response.Summary = FString::Printf(
			TEXT("RE spell row mutation failed and was rolled back: %s"),
			*MutationError);
	}
	else
	{
		Response.Status = TEXT("failed_validation");
		Response.Summary = FString::Printf(
			TEXT("RE spell row mutation failed before persistence: %s"),
			*MutationError);
	}
	Response.UnderstoodAction = Request.Action;
	Response.UnderstoodTarget = Request.TargetAssetPath;
	Response.InterpretationNotes.Add(FString::Printf(
		TEXT("row_name=%s; AbilityId=%s; deterministic target retained without auto-suffixing"),
		*Plan.RowName,
		*Plan.RowName));
	Response.InterpretationNotes.Add(FString::Printf(
		TEXT("table_object=%s; row_struct=%s; mode=%s; dry_run=%s; atomic=%s; save=%s; validate=%s; rollback_on_failure=%s; timeout_ms=%d; on_revision_conflict=%s; expected_revision=%s; idempotency_key=%s"),
		*WritePlan.TableObjectPath,
		*WritePlan.RowStructPath,
		*WritePlan.Mode,
		WritePlan.bDryRun ? TEXT("true") : TEXT("false"),
		WritePlan.bAtomic ? TEXT("true") : TEXT("false"),
		WritePlan.bSave ? TEXT("true") : TEXT("false"),
		WritePlan.bValidate ? TEXT("true") : TEXT("false"),
		WritePlan.bRollbackOnFailure ? TEXT("true") : TEXT("false"),
		WritePlan.TimeoutMs,
		*WritePlan.OnRevisionConflict,
		WritePlan.bHasExpectedRevision ? *WritePlan.ExpectedRevision : TEXT("<absent>"),
		WritePlan.IdempotencyKey.IsEmpty() ? TEXT("<absent>") : *WritePlan.IdempotencyKey));
	Response.PrimaryAsset = Request.TargetAssetPath;
	Response.Dependencies = ResolvedDependencies;
	Response.UnresolvedDependencies = UnresolvedDependencies;
	Response.Metrics.McpRoundTrips = 1;
	Response.Metrics.InternalOperations = Request.bDryRun ? 2 : 8;
	Response.Metrics.AssetsAffected = MutationResult.bPersisted ? 1 : 0;
	Response.CapabilityNotes = {
		TEXT("Specification-to-FREAbilityDef planning and Pattern B static checks completed."),
		TEXT("No gameplay-tag INI mutation is performed; RE Element, EffectTag, and ImpactStatus fields are used."),
		TEXT("Multi-client replication proof remains WS-11/RB-14; this response does not claim it."),
	};
	Response.CapabilityNotes.Add(
		Request.bDryRun
			? TEXT("dry_run intentionally did not acquire the mutator queue or write assets.")
			: TEXT("FUeremcpMutatingDispatch held permission, path, queue, audit, and release around the complete DataTable executor."));
	if (!bDependenciesResolved)
	{
		Response.CapabilityNotes.Add(FString::Printf(
			TEXT("%d presentation dependency asset(s) were unresolved."),
			UnresolvedDependencies.Num()));
	}

	if (!Request.bDryRun)
	{
		FUeremcpAssetRef Asset;
		Asset.AssetPath = Request.TargetAssetPath;
		Asset.AssetClass = TEXT("/Script/Engine.DataTable");
		Asset.Revision = MutationResult.RevisionAfter;
		Asset.Role = TEXT("ability_table");
		if (MutationResult.bNoChange)
		{
			Response.ReusedAssets.Add(Asset);
		}
		else if (MutationResult.bPersisted && MutationResult.bCreatedTable)
		{
			Response.CreatedAssets.Add(Asset);
		}
		else if (MutationResult.bPersisted)
		{
			Response.ModifiedAssets.Add(Asset);
		}
		Response.Revision = MutationResult.RevisionAfter;
	}

	TSharedPtr<FJsonObject> Validation = MakeShared<FJsonObject>();
	SetJsonNull(Validation, TEXT("compiled"));
	if (Request.bDryRun)
	{
		SetJsonNull(Validation, TEXT("saved"));
	}
	else
	{
		Validation->SetBoolField(TEXT("saved"), MutationResult.bSaved);
	}
	Validation->SetBoolField(TEXT("structurally_valid"), true);
	Validation->SetBoolField(
		TEXT("dependencies_resolved"),
		bDependenciesResolved);
	if (Request.bDryRun)
	{
		SetJsonNull(Validation, TEXT("reread_after_write"));
	}
	else
	{
		Validation->SetBoolField(
			TEXT("reread_after_write"),
			MutationResult.bRereadAfterWrite);
	}
	SetJsonNull(Validation, TEXT("runtime_smoke_test"));
	SetJsonNull(Validation, TEXT("editor_validation_run"));
	TArray<FString> ChecksPerformed = Plan.StaticChecks;
	ChecksPerformed.Add(TEXT("deterministic_datatable_write_plan_prepared"));
	ChecksPerformed.Add(TEXT("presentation_dependency_resolution_completed"));
	if (!Request.bDryRun)
	{
		ChecksPerformed.Add(TEXT("core_mutating_dispatch_admitted"));
		if (MutationResult.bSaved)
		{
			ChecksPerformed.Add(TEXT("datatable_package_saved_in_sandbox"));
		}
		if (MutationResult.bRereadAfterWrite)
		{
			ChecksPerformed.Add(TEXT("normalized_row_reread_matches"));
		}
		if (MutationResult.bPersisted)
		{
			ChecksPerformed.Add(TEXT("sandbox_changes_persisted"));
		}
		if (MutationResult.bRolledBack)
		{
			ChecksPerformed.Add(TEXT("sandbox_changes_rolled_back"));
		}
	}
	Validation->SetArrayField(TEXT("checks_performed"), StringValues(ChecksPerformed));
	TArray<FString> ChecksSkipped = {
		TEXT("runtime_smoke_test: WS-11/RB-14"),
	};
	if (Request.bDryRun)
	{
		ChecksSkipped.Insert(TEXT("reread_after_write: no mutation performed"), 0);
		ChecksSkipped.Insert(TEXT("save: no mutation performed"), 0);
		ChecksSkipped.Insert(
			TEXT("datatable_upsert: dry_run planning path does not mutate"),
			0);
	}
	else if (!bMutationSucceeded)
	{
		ChecksSkipped.Insert(
			FString::Printf(TEXT("datatable_executor: %s"), *MutationError),
			0);
	}
	Validation->SetArrayField(TEXT("checks_skipped"), StringValues(ChecksSkipped));

	TSharedPtr<FJsonObject> Rollback = MakeShared<FJsonObject>();
	Rollback->SetBoolField(TEXT("available"), MutationResult.bRolledBack);
	Rollback->SetBoolField(TEXT("performed"), MutationResult.bRolledBack);
	Rollback->SetStringField(
		TEXT("scope"),
		MutationResult.bRolledBack ? TEXT("full") : TEXT("none"));
	Rollback->SetStringField(
		TEXT("detail"),
		Request.bDryRun
			? TEXT("Dry-run preflight performed no mutation; nothing required discard.")
			: MutationResult.bRolledBack
				? TEXT("FileSandbox changes were discarded and the in-memory row was restored.")
				: TEXT("No rollback was required."));

	TSharedPtr<FJsonObject> TraceStep = MakeShared<FJsonObject>();
	TraceStep->SetStringField(TEXT("step"), TEXT("plan_create_spell"));
	TraceStep->SetBoolField(
		TEXT("ok"),
		Request.bDryRun || bMutationSucceeded);
	TraceStep->SetStringField(
		TEXT("detail"),
		Request.bDryRun
			? FString::Printf(
				TEXT("Prepared %d ordered guarded-write steps for %s without executing them."),
				WritePlan.OrderedSteps.Num(),
				*WritePlan.TableObjectPath)
			: FString::Printf(
				TEXT("Executed guarded DataTable path for %s: saved=%s reread=%s persisted=%s."),
				*WritePlan.TableObjectPath,
				MutationResult.bSaved ? TEXT("true") : TEXT("false"),
				MutationResult.bRereadAfterWrite ? TEXT("true") : TEXT("false"),
				MutationResult.bPersisted ? TEXT("true") : TEXT("false")));
	TArray<TSharedPtr<FJsonValue>> ExecutionTrace;
	ExecutionTrace.Add(MakeShared<FJsonValueObject>(TraceStep));
	TSharedPtr<FJsonObject> Diagnostics = MakeShared<FJsonObject>();
	Diagnostics->SetArrayField(TEXT("execution_trace"), ExecutionTrace);
	Diagnostics->SetBoolField(TEXT("truncated"), false);

	Response.ExtraFields = MakeShared<FJsonObject>();
	Response.ExtraFields->SetObjectField(TEXT("validation"), Validation);
	TArray<TSharedPtr<FJsonValue>> ChangeValues;
	if (MutationResult.bPersisted)
	{
		TSharedPtr<FJsonObject> Change = MakeShared<FJsonObject>();
		Change->SetStringField(
			TEXT("kind"),
			MutationResult.bCreatedTable ? TEXT("created") : TEXT("modified"));
		Change->SetStringField(TEXT("asset_path"), Request.TargetAssetPath);
		Change->SetStringField(TEXT("asset_class"), TEXT("/Script/Engine.DataTable"));
		if (MutationResult.RevisionBefore.IsEmpty())
		{
			SetJsonNull(Change, TEXT("revision_before"));
		}
		else
		{
			Change->SetStringField(
				TEXT("revision_before"),
				MutationResult.RevisionBefore);
		}
		Change->SetStringField(
			TEXT("revision_after"),
			MutationResult.RevisionAfter);
		Change->SetStringField(
			TEXT("detail"),
			FString::Printf(
				TEXT("Sandbox reported %d persisted file change(s)."),
				MutationResult.SandboxedFiles.Num()));
		ChangeValues.Add(MakeShared<FJsonValueObject>(Change));
	}
	Response.ExtraFields->SetArrayField(
		TEXT("changes"),
		ChangeValues);
	Response.ExtraFields->SetObjectField(TEXT("rollback"), Rollback);
	Response.ExtraFields->SetObjectField(TEXT("diagnostics"), Diagnostics);

	const FString TerminalJson = Request.bDryRun
		? FUeremcpEnvelope::SerializeResponse(Response)
		: MutatingDispatch.Complete(Response);
	if (!Request.bDryRun
		&& !Request.IdempotencyKey.IsEmpty()
		&& IsVerifiedTerminalStatus(Response.Status))
	{
		FUeremcpIdempotencyStore::Get().Put(Request.IdempotencyKey, TerminalJson);
	}
	return TerminalJson;
}

FString UUeremcpGameplayToolset::CreateSpellVariation(const FString& RequestJson)
{
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
			TEXT("Unsupported protocol_version for create_spell_variation."));
	}
	if (Request.Action != TEXT("create_spell_variation"))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			TEXT("CreateSpellVariation requires action=create_spell_variation."));
	}
	if (Request.Mode != TEXT("create") && Request.Mode != TEXT("create_or_update"))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			TEXT("create_spell_variation supports create or create_or_update only."));
	}
	if (Request.TargetAssetPath.IsEmpty()
		|| (!Request.TargetAssetPath.StartsWith(TEXT("/Game/__UeremcpPoc/"))
			&& !Request.TargetAssetPath.StartsWith(TEXT("/Game/__UeremcpTests/"))))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			TEXT("target ability table must be under /Game/__UeremcpPoc/ or /Game/__UeremcpTests/."));
	}

	const FUeremcpPathValidationResult TargetPathResult =
		FUeremcpPathPolicy::ValidateSoftPath(Request.TargetAssetPath, true);
	if (!TargetPathResult.bAllowed)
	{
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, TargetPathResult.Reason);
	}

	FUeremcpAbilityVariationPlan VariationPlan;
	FString VariationError;
	if (!FUeremcpAbilityVariation::BuildPlan(
		Request.Specification,
		VariationPlan,
		VariationError))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(TEXT("Invalid composite variation: %s"), *VariationError));
	}
	const FUeremcpPathValidationResult SourcePathResult =
		FUeremcpPathPolicy::ValidateSoftPath(VariationPlan.SourceTablePath, false);
	const FUeremcpPathValidationResult PresentationPathResult =
		FUeremcpPathPolicy::ValidateSoftPath(VariationPlan.PresentationAsset, false);
	if (!SourcePathResult.bAllowed || !PresentationPathResult.bAllowed)
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			!SourcePathResult.bAllowed ? SourcePathResult.Reason : PresentationPathResult.Reason);
	}

	UObject* PresentationObject = StaticLoadObject(
		UObject::StaticClass(),
		nullptr,
		*VariationPlan.PresentationAsset,
		nullptr,
		LOAD_NoWarn);
	const bool bPresentationResolved = PresentationObject != nullptr;

	FUeremcpAbilityTableWriteOptions WriteOptions;
	WriteOptions.RequestId = Request.RequestId;
	WriteOptions.Mode = Request.Mode;
	WriteOptions.bDryRun = Request.bDryRun;
	WriteOptions.bAtomic = Request.bAtomic;
	WriteOptions.bSave = Request.bSave;
	WriteOptions.bValidate = Request.bValidate;
	WriteOptions.bRollbackOnFailure = Request.bRollbackOnFailure;
	WriteOptions.TimeoutMs = Request.TimeoutMs;
	WriteOptions.OnRevisionConflict = Request.OnRevisionConflict;
	WriteOptions.ExpectedRevision = Request.ExpectedRevision;
	WriteOptions.bHasExpectedRevision = Request.bHasExpectedRevision;
	WriteOptions.IdempotencyKey = Request.IdempotencyKey;

	FUeremcpAbilityTableWritePlan WritePlan;
	if (!FUeremcpSpellPlanner::BuildTableWritePlan(
		Request.TargetAssetPath,
		WriteOptions,
		VariationPlan.SpellPlan,
		WritePlan,
		VariationError))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(TEXT("Invalid variation write plan: %s"), *VariationError));
	}

	FUeremcpMutatingDispatch MutatingDispatch;
	if (!Request.bDryRun)
	{
		FString BlockingResponse;
		if (!MutatingDispatch.TryBegin(RequestJson, false, 0, false, BlockingResponse))
		{
			return BlockingResponse;
		}
	}

	FUeremcpAbilityTableMutationResult MutationResult;
	FString MutationError;
	const bool bMutationSucceeded =
		Request.bDryRun
		|| (bPresentationResolved
			&& FUeremcpAbilityTableMutator::Execute(
				WritePlan,
				VariationPlan.SpellPlan,
				MutationResult,
				MutationError));
	if (!Request.bDryRun && !bPresentationResolved)
	{
		MutationError = TEXT("variation presentation_asset could not be loaded");
	}

	TSharedPtr<FJsonObject> TargetProtectedFields;
	bool bProtectedFieldsEqual = false;
	if (!Request.bDryRun && bMutationSucceeded)
	{
		bProtectedFieldsEqual = FUeremcpAbilityVariation::VerifyTarget(
			Request.TargetAssetPath,
			VariationPlan,
			TargetProtectedFields,
			MutationError);
	}

	FUeremcpResponse Response;
	Response.RequestId = Request.RequestId;
	Response.UnderstoodAction = Request.Action;
	Response.UnderstoodTarget = Request.TargetAssetPath;
	Response.PrimaryAsset = Request.TargetAssetPath;
	Response.Metrics.McpRoundTrips = 1;
	Response.Metrics.InternalOperations = Request.bDryRun ? 3 : 10;
	Response.Metrics.AssetsAffected = MutationResult.bPersisted ? 1 : 0;
	Response.InterpretationNotes = {
		FString::Printf(
			TEXT("source_binding=ability_table:%s source_row:%s vfx_phase:%s"),
			*VariationPlan.SourceTablePath,
			*VariationPlan.SourceRow,
			*VariationPlan.VfxPhase),
		FString::Printf(
			TEXT("target_row=%s; relationship=clone_source_row_and_override_presentation_only"),
			*VariationPlan.TargetRow),
	};
	Response.CapabilityNotes = {
		TEXT("FREAbilityDef is the gameplay owner; the production source row is read-only."),
		TEXT("RE Pattern B remains owned by the unchanged CastAbility/AuthorityCastAbility path."),
		TEXT("Multi-client replication observation remains RB-14/WS-11 and is not claimed here."),
	};

	if (Request.bDryRun)
	{
		Response.Status = TEXT("partially_completed");
		Response.Summary = TEXT("Validated the composite source binding; dry_run did not create a target row.");
	}
	else if (!bMutationSucceeded)
	{
		Response.Status = MutationResult.bRolledBack ? TEXT("rolled_back") : TEXT("failed_validation");
		Response.Summary = FString::Printf(TEXT("Composite variation mutation failed: %s"), *MutationError);
	}
	else if (!bProtectedFieldsEqual)
	{
		Response.Status = TEXT("failed_validation");
		Response.Summary = FString::Printf(
			TEXT("Target row was written but C5 protected-field verification failed: %s"),
			*MutationError);
	}
	else if (MutationResult.bNoChange)
	{
		Response.Status = TEXT("no_change_required");
		Response.Summary = TEXT("Target variation row already matched and protected gameplay fields re-verified.");
	}
	else
	{
		Response.Status = MutationResult.bCreatedTable
			? TEXT("created_and_validated")
			: TEXT("modified_and_validated");
		Response.Summary = TEXT("Created the presentation variation and verified networking/damage fields unchanged.");
	}

	FUeremcpAssetRef PresentationDependency;
	PresentationDependency.AssetPath = VariationPlan.PresentationAsset;
	PresentationDependency.Role = TEXT("variation_presentation");
	if (PresentationObject)
	{
		PresentationDependency.AssetClass = PresentationObject->GetClass()->GetPathName();
		Response.Dependencies.Add(PresentationDependency);
	}
	else
	{
		Response.UnresolvedDependencies.Add(PresentationDependency);
	}

	if (!Request.bDryRun)
	{
		FUeremcpAssetRef TableAsset;
		TableAsset.AssetPath = Request.TargetAssetPath;
		TableAsset.AssetClass = TEXT("/Script/Engine.DataTable");
		TableAsset.Revision = MutationResult.RevisionAfter;
		TableAsset.Role = TEXT("target_ability_table");
		if (MutationResult.bNoChange)
		{
			Response.ReusedAssets.Add(TableAsset);
		}
		else if (MutationResult.bPersisted && MutationResult.bCreatedTable)
		{
			Response.CreatedAssets.Add(TableAsset);
		}
		else if (MutationResult.bPersisted)
		{
			Response.ModifiedAssets.Add(TableAsset);
		}
		Response.Revision = MutationResult.RevisionAfter;
	}

	const TSharedPtr<FJsonObject> Validation = MakeShared<FJsonObject>();
	SetJsonNull(Validation, TEXT("compiled"));
	Validation->SetBoolField(TEXT("structurally_valid"), true);
	Validation->SetBoolField(TEXT("dependencies_resolved"), bPresentationResolved);
	if (Request.bDryRun)
	{
		SetJsonNull(Validation, TEXT("saved"));
		SetJsonNull(Validation, TEXT("reread_after_write"));
	}
	else
	{
		Validation->SetBoolField(TEXT("saved"), MutationResult.bSaved);
		Validation->SetBoolField(TEXT("reread_after_write"), MutationResult.bRereadAfterWrite);
	}
	Validation->SetBoolField(TEXT("protected_fields_equal"), bProtectedFieldsEqual);
	Validation->SetObjectField(
		TEXT("source_protected_fields"),
		VariationPlan.SourceProtectedFields);
	if (TargetProtectedFields.IsValid())
	{
		Validation->SetObjectField(TEXT("target_protected_fields"), TargetProtectedFields);
	}
	TArray<FString> Checks = VariationPlan.SpellPlan.StaticChecks;
	if (bProtectedFieldsEqual)
	{
		Checks.Add(TEXT("gameplay_protected_fields_equal"));
		Checks.Add(TEXT("impact_damage_status_duration_aoe_equal"));
		Checks.Add(TEXT("projectile_physics_and_spawn_entity_equal"));
	}
	Validation->SetArrayField(TEXT("checks_performed"), StringValues(Checks));

	const TSharedPtr<FJsonObject> Binding = MakeShared<FJsonObject>();
	Binding->SetStringField(TEXT("ability_table"), VariationPlan.SourceTablePath);
	Binding->SetStringField(TEXT("source_row"), VariationPlan.SourceRow);
	Binding->SetStringField(TEXT("vfx_phase"), VariationPlan.VfxPhase);
	Binding->SetStringField(TEXT("target_ability_table"), Request.TargetAssetPath);
	Binding->SetStringField(TEXT("target_row"), VariationPlan.TargetRow);
	Binding->SetStringField(TEXT("presentation_asset"), VariationPlan.PresentationAsset);
	Binding->SetStringField(TEXT("relationship"), TEXT("cloned_row_presentation_override"));

	Response.ExtraFields = MakeShared<FJsonObject>();
	Response.ExtraFields->SetObjectField(TEXT("validation"), Validation);
	Response.ExtraFields->SetObjectField(TEXT("gameplay_binding"), Binding);

	const FString TerminalJson = Request.bDryRun
		? FUeremcpEnvelope::SerializeResponse(Response)
		: MutatingDispatch.Complete(Response);
	return TerminalJson;
}
