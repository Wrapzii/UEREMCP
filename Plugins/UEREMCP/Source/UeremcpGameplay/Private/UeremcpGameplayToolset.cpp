#include "UeremcpGameplayToolset.h"

#include "Dom/JsonValue.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "UeremcpAuditLog.h"
#include "UeremcpEnvelope.h"
#include "UeremcpMutatorQueue.h"
#include "UeremcpPathPolicy.h"
#include "UeremcpPermissionPolicy.h"
#include "UeremcpSpellPlanner.h"

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

	EUeremcpPermissionTier RequiredTier = EUeremcpPermissionTier::Write;
	FString ProjectKey;
	bool bOwnsMutator = false;
	ON_SCOPE_EXIT
	{
		if (bOwnsMutator)
		{
			FUeremcpMutatorQueue::Release(ProjectKey, Request.RequestId);
		}
	};

	if (!Request.bDryRun)
	{
		if (Request.ProjectPath.IsEmpty())
		{
			return FUeremcpEnvelope::MakeRejection(
				Request.RequestId,
				TEXT("Non-dry create_spell requires project.path for per-project mutator ownership."));
		}
		const FUeremcpPathValidationResult ProjectPathResult =
			FUeremcpPathPolicy::ValidateProjectPathMatch(
				Request.ProjectPath,
				FPaths::GetProjectFilePath());
		if (!ProjectPathResult.bAllowed)
		{
			return FUeremcpEnvelope::MakeRejection(
				Request.RequestId,
				FString::Printf(TEXT("Project path rejected: %s"), *ProjectPathResult.Reason));
		}

		FUeremcpPermissionOptions PermissionOptions;
		PermissionOptions.bDryRun = false;
		// ParseRequest does not retain option presence; Core must supply it before
		// Gameplay supports destructive modes.
		// [VERIFIED: 1fd0eef:docs/proposals/ws-12-core-security-dispatcher-gate.md]
		PermissionOptions.bDryRunWasExplicit = false;
		const FUeremcpPermissionVerdict PermissionVerdict =
			FUeremcpPermissionPolicy::Evaluate(
				Request.Action,
				Request.Mode,
				PermissionOptions,
				false);
		if (!PermissionVerdict.bAllowed)
		{
			return FUeremcpEnvelope::MakeRejection(
				Request.RequestId,
				FString::Printf(TEXT("Permission denied: %s"), *PermissionVerdict.DenialReason));
		}
		RequiredTier = PermissionVerdict.RequiredTier;
		ProjectKey = Request.ProjectPath;

		if (!FUeremcpMutatorQueue::IsImplemented())
		{
			return FUeremcpEnvelope::MakeRejection(
				Request.RequestId,
				TEXT("Shared mutator queue is unavailable; create_spell fails closed without writing."));
		}

		const FUeremcpMutatorQueue::FAcquireResult Acquire =
			FUeremcpMutatorQueue::TryAcquire(
				ProjectKey,
				Request.RequestId,
				RequiredTier);
		if (!Acquire.bAcquired)
		{
			if (Acquire.bQueued)
			{
				// Core/Transport does not yet own this domain waiter's ADR-0009
				// lifecycle. Cancel before a terminal response so it cannot become
				// an abandoned FIFO head.
				// [VERIFIED: 1fd0eef:docs/proposals/ws-12-core-security-dispatcher-gate.md]
				FUeremcpMutatorQueue::CancelQueued(ProjectKey, Request.RequestId);
			}
			return FUeremcpEnvelope::MakeRejection(
				Request.RequestId,
				FString::Printf(
					TEXT("Mutator slot not acquired; no write occurred. %s%s"),
					*Acquire.Reason,
					Acquire.JobId.IsEmpty()
						? TEXT("")
						: *FString::Printf(
							TEXT(" Queue job %s was cancelled because Core polling is not wired."),
							*Acquire.JobId)));
		}
		bOwnsMutator = true;
	}

	FUeremcpResponse Response;
	Response.RequestId = Request.RequestId;
	Response.Status = TEXT("partially_completed");
	Response.Summary = Request.bDryRun
		? FString::Printf(
			TEXT("Prepared and statically validated RE spell row '%s' for '%s'; dry_run performed no mutation."),
			*Plan.RowName,
			*Request.TargetAssetPath)
		: FString::Printf(
			TEXT("Acquired the shared mutator for RE spell row '%s', but performed no DataTable mutation because the Core dispatcher gate is not integrated."),
			*Plan.RowName);
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
	for (const FString& DependencyPath : Plan.DependencyAssetPaths)
	{
		FUeremcpAssetRef Dependency;
		Dependency.AssetPath = DependencyPath;
		Dependency.Role = TEXT("spell_presentation");
		Response.Dependencies.Add(MoveTemp(Dependency));
	}
	Response.Metrics.McpRoundTrips = 1;
	Response.Metrics.InternalOperations = Request.bDryRun ? 2 : 3;
	Response.CapabilityNotes = {
		TEXT("Specification-to-FREAbilityDef planning and Pattern B static checks completed."),
		TEXT("No gameplay-tag INI mutation is performed; RE Element, EffectTag, and ImpactStatus fields are used."),
		TEXT("Multi-client replication proof remains WS-11/RB-14; this response does not claim it."),
	};
	Response.CapabilityNotes.Add(
		Request.bDryRun
			? TEXT("dry_run intentionally did not acquire the mutator queue or write assets.")
			: TEXT("Mutator ownership was held through terminal audit; DataTable write/save/re-read remain withheld pending the shared Core dispatcher."));

	bool bAuditAppended = false;
	FString AuditError;
	bool bMutatorReleased = !bOwnsMutator;
	if (bOwnsMutator)
	{
		FUeremcpAuditRecord AuditRecord;
		AuditRecord.RequestId = Request.RequestId;
		AuditRecord.IdempotencyKey = Request.IdempotencyKey;
		AuditRecord.Action = Request.Action;
		AuditRecord.Mode = Request.Mode;
		AuditRecord.Status = Response.Status;
		AuditRecord.TargetAssetPath = Request.TargetAssetPath;
		AuditRecord.bDryRun = false;
		AuditRecord.bAtomic = Request.bAtomic;
		AuditRecord.RequiredTier = RequiredTier;
		AuditRecord.ProjectPath = Request.ProjectPath;
		bAuditAppended = FUeremcpAuditLog::Append(
			AuditRecord,
			FUeremcpPathPolicy::RootsFromProject(),
			AuditError);
		++Response.Metrics.InternalOperations;
		if (!bAuditAppended)
		{
			Response.CapabilityNotes.Add(
				FString::Printf(TEXT("Terminal audit append failed: %s"), *AuditError));
		}

		bMutatorReleased =
			FUeremcpMutatorQueue::Release(ProjectKey, Request.RequestId);
		bOwnsMutator = !bMutatorReleased;
		++Response.Metrics.InternalOperations;
		if (!bMutatorReleased)
		{
			Response.Status = TEXT("failed_validation");
			Response.Summary =
				TEXT("No DataTable mutation occurred, but mutator ownership could not be released cleanly.");
			Response.CapabilityNotes.Add(
				TEXT("Mutator release failed; the request cannot report a clean terminal preflight."));
		}
	}

	TSharedPtr<FJsonObject> Validation = MakeShared<FJsonObject>();
	Validation->SetNullField(TEXT("compiled"));
	Validation->SetNullField(TEXT("saved"));
	Validation->SetBoolField(TEXT("structurally_valid"), true);
	Validation->SetNullField(TEXT("dependencies_resolved"));
	Validation->SetNullField(TEXT("reread_after_write"));
	Validation->SetNullField(TEXT("runtime_smoke_test"));
	Validation->SetNullField(TEXT("editor_validation_run"));
	TArray<FString> ChecksPerformed = Plan.StaticChecks;
	ChecksPerformed.Add(TEXT("deterministic_datatable_write_plan_prepared"));
	if (!Request.bDryRun)
	{
		ChecksPerformed.Add(TEXT("mutator_queue_acquired"));
		ChecksPerformed.Add(
			bAuditAppended
				? TEXT("terminal_audit_appended")
				: TEXT("terminal_audit_append_failed"));
		ChecksPerformed.Add(
			bMutatorReleased
				? TEXT("mutator_queue_released")
				: TEXT("mutator_queue_release_failed"));
	}
	Validation->SetArrayField(TEXT("checks_performed"), StringValues(ChecksPerformed));
	TArray<FString> ChecksSkipped = {
		TEXT("save: no mutation performed"),
		TEXT("reread_after_write: no mutation performed"),
		TEXT("dependency_resolution: requires editor asset load"),
		TEXT("runtime_smoke_test: WS-11/RB-14"),
	};
	ChecksSkipped.Insert(
		Request.bDryRun
			? TEXT("datatable_upsert: dry_run planning path does not mutate")
			: TEXT("datatable_upsert: shared Core dispatcher gate is not integrated"),
		0);
	Validation->SetArrayField(TEXT("checks_skipped"), StringValues(ChecksSkipped));

	TSharedPtr<FJsonObject> Rollback = MakeShared<FJsonObject>();
	Rollback->SetBoolField(TEXT("available"), false);
	Rollback->SetBoolField(TEXT("performed"), false);
	Rollback->SetStringField(TEXT("scope"), TEXT("none"));
	Rollback->SetStringField(
		TEXT("detail"),
		Request.bDryRun
			? TEXT("Dry-run preflight performed no mutation; nothing required discard.")
			: TEXT("Queue-gated preflight performed no mutation; rollback was not entered."));

	TSharedPtr<FJsonObject> TraceStep = MakeShared<FJsonObject>();
	TraceStep->SetStringField(TEXT("step"), TEXT("plan_create_spell"));
	TraceStep->SetBoolField(TEXT("ok"), true);
	TraceStep->SetStringField(
		TEXT("detail"),
		FString::Printf(
			TEXT("Prepared %d ordered guarded-write steps for %s without executing them."),
			WritePlan.OrderedSteps.Num(),
			*WritePlan.TableObjectPath));
	TArray<TSharedPtr<FJsonValue>> ExecutionTrace;
	ExecutionTrace.Add(MakeShared<FJsonValueObject>(TraceStep));
	TSharedPtr<FJsonObject> Diagnostics = MakeShared<FJsonObject>();
	Diagnostics->SetArrayField(TEXT("execution_trace"), ExecutionTrace);
	Diagnostics->SetBoolField(TEXT("truncated"), false);

	Response.ExtraFields = MakeShared<FJsonObject>();
	Response.ExtraFields->SetObjectField(TEXT("validation"), Validation);
	Response.ExtraFields->SetArrayField(
		TEXT("changes"),
		TArray<TSharedPtr<FJsonValue>>());
	Response.ExtraFields->SetObjectField(TEXT("rollback"), Rollback);
	Response.ExtraFields->SetObjectField(TEXT("diagnostics"), Diagnostics);

	return FUeremcpEnvelope::SerializeResponse(Response);
}
