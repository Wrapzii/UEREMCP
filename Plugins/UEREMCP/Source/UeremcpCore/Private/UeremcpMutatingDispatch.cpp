#include "UeremcpMutatingDispatch.h"

#include "Dom/JsonObject.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "UeremcpAuditLog.h"
#include "UeremcpJob.h"
#include "UeremcpMutatorQueue.h"
#include "UeremcpPathPolicy.h"
#include "UeremcpPermissionPolicy.h"

namespace
{
	bool ParseOptionsObject(
		const FString& RequestJson,
		TSharedPtr<FJsonObject>& OutOptions,
		FString& OutError)
	{
		OutOptions.Reset();
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RequestJson);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			OutError = TEXT("request is not valid JSON");
			return false;
		}
		if (Root->HasTypedField<EJson::Object>(TEXT("options")))
		{
			OutOptions = Root->GetObjectField(TEXT("options"));
		}
		return true;
	}
}

FUeremcpMutatingDispatch::FUeremcpMutatingDispatch() = default;

FUeremcpMutatingDispatch::~FUeremcpMutatingDispatch()
{
	ReleaseMutator();
}

FUeremcpPermissionOptions FUeremcpMutatingDispatch::BuildPermissionOptions(
	const FString& RequestJson,
	const FUeremcpRequest& ParsedRequest,
	int32 PredictedDeletedAssetCount)
{
	FUeremcpPermissionOptions Options;
	Options.bDryRun = ParsedRequest.bDryRun;
	Options.PredictedDeletedAssetCount = PredictedDeletedAssetCount;

	TSharedPtr<FJsonObject> OptionsObject;
	FString OptionsError;
	if (ParseOptionsObject(RequestJson, OptionsObject, OptionsError) && OptionsObject.IsValid())
	{
		if (OptionsObject->HasField(TEXT("dry_run")))
		{
			Options.bDryRunWasExplicit = true;
			Options.bDryRun = OptionsObject->GetBoolField(TEXT("dry_run"));
		}
		if (OptionsObject->HasField(TEXT("allow_destructive")))
		{
			Options.bAllowDestructive = OptionsObject->GetBoolField(TEXT("allow_destructive"));
		}
	}

	return Options;
}

FString FUeremcpMutatingDispatch::MakeMutatorQueuedResponse(
	const FUeremcpRequest& Request,
	const FString& JobId,
	const FString& Reason)
{
	FUeremcpResponse Response;
	Response.ProtocolVersion = FUeremcpEnvelope::ProtocolVersion();
	Response.RequestId = Request.RequestId;
	Response.Status = TEXT("partially_completed");
	Response.Summary = Reason.IsEmpty()
		? TEXT("Waiting for the project mutator queue. Poll get_job_result with the returned job id.")
		: Reason;
	Response.bHasJob = true;
	Response.Job.JobId = JobId;
	Response.Job.State = TEXT("queued");
	Response.Job.ProgressMessage = TEXT("Waiting for mutator slot");
	Response.Job.PollAction = FUeremcpJobDefaults::PollAction();
	Response.Job.bCancellable = true;
	Response.Job.bHasCancellable = true;
	Response.Metrics.McpRoundTrips = 1;
	Response.Metrics.InternalOperations = 0;
	Response.CapabilityNotes.Add(
		TEXT("Mutator queue: RETRY this same tool call with the SAME request_id "
			 "(FIFO head acquires). Do NOT poll get_job_result forever — abandoned "
			 "waiters auto-clear after ~45s; orphaned active slots after ~180s."));
	Response.ErrorCode = TEXT("MUTATOR_BUSY");
	return FUeremcpEnvelope::SerializeResponse(Response);
}

bool FUeremcpMutatingDispatch::TryBegin(
	const FString& RequestJson,
	const bool bTargetExists,
	const int32 PredictedDeletedAssetCount,
	const bool bReadOnlyOperation,
	FString& OutBlockingResponseJson)
{
	OutBlockingResponseJson.Reset();
	ReleaseMutator();
	bGateOpen = false;

	FString ParseError;
	if (!FUeremcpEnvelope::ParseRequest(RequestJson, Request, ParseError))
	{
		OutBlockingResponseJson = FUeremcpEnvelope::MakeRejection(FString(), ParseError);
		return false;
	}

	if (!FUeremcpEnvelope::IsProtocolCompatible(Request.ProtocolVersion))
	{
		OutBlockingResponseJson = FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(
				TEXT("Unsupported protocol_version '%s'"), *Request.ProtocolVersion));
		return false;
	}

	PathRoots = FUeremcpPathPolicy::RootsFromProject();
	ProjectKey = !Request.ProjectPath.IsEmpty()
		? Request.ProjectPath
		: FPaths::GetProjectFilePath();

	if (!Request.ProjectPath.IsEmpty())
	{
		const FUeremcpPathValidationResult ProjectMatch =
			FUeremcpPathPolicy::ValidateProjectPathMatch(
				Request.ProjectPath,
				FPaths::GetProjectFilePath());
		if (!ProjectMatch.bAllowed)
		{
			OutBlockingResponseJson = FUeremcpEnvelope::MakeRejection(
				Request.RequestId,
				ProjectMatch.Reason);
			FUeremcpResponse RejectResponse;
			RejectResponse.RequestId = Request.RequestId;
			RejectResponse.Status = TEXT("rejected");
			RejectResponse.Summary = ProjectMatch.Reason;
			TArray<FString> Notes;
			AppendAuditForResponse(RejectResponse, Notes);
			return false;
		}
	}

	const FUeremcpPermissionOptions Options = BuildPermissionOptions(
		RequestJson,
		Request,
		PredictedDeletedAssetCount);
	Verdict = FUeremcpPermissionPolicy::Evaluate(
		Request.Action,
		Request.Mode,
		Options,
		bTargetExists,
		nullptr);

	if (!Verdict.bAllowed)
	{
		OutBlockingResponseJson = FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			Verdict.DenialReason);
		FUeremcpResponse RejectResponse;
		RejectResponse.RequestId = Request.RequestId;
		RejectResponse.Status = TEXT("rejected");
		RejectResponse.Summary = Verdict.DenialReason;
		TArray<FString> Notes;
		AppendAuditForResponse(RejectResponse, Notes);
		return false;
	}

	const EUeremcpPermissionTier EffectiveTier = bReadOnlyOperation
		? EUeremcpPermissionTier::Read
		: Verdict.RequiredTier;

	if (!Request.TargetAssetPath.IsEmpty())
	{
		const FUeremcpPathValidationResult TargetPath = FUeremcpPathPolicy::ValidateSoftPath(
			Request.TargetAssetPath,
			EffectiveTier != EUeremcpPermissionTier::Read,
			&PathRoots);
		if (!TargetPath.bAllowed)
		{
			OutBlockingResponseJson = FUeremcpEnvelope::MakeRejection(
				Request.RequestId,
				TargetPath.Reason);
			FUeremcpResponse RejectResponse;
			RejectResponse.RequestId = Request.RequestId;
			RejectResponse.Status = TEXT("rejected");
			RejectResponse.Summary = TargetPath.Reason;
			TArray<FString> Notes;
			AppendAuditForResponse(RejectResponse, Notes);
			return false;
		}
	}

	if (EffectiveTier == EUeremcpPermissionTier::Read)
	{
		bGateOpen = true;
		return true;
	}

	// Drop abandoned FIFO entries before acquire so CreateLandscape / AttachWeather /
	// ScatterFoliage cannot hang forever behind timed-out retries with new request ids.
	{
		bool bClearedActive = false;
		const int32 Cleared = FUeremcpMutatorQueue::ClearStale(ProjectKey, bClearedActive);
		(void)Cleared;
		(void)bClearedActive;
	}

	const FUeremcpMutatorQueue::FAcquireResult Acquire =
		FUeremcpMutatorQueue::TryAcquire(ProjectKey, Request.RequestId, EffectiveTier);
	if (Acquire.bQueued)
	{
		OutBlockingResponseJson = MakeMutatorQueuedResponse(
			Request,
			Acquire.JobId,
			Acquire.Reason);
		return false;
	}
	if (!Acquire.bAcquired)
	{
		OutBlockingResponseJson = FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			Acquire.Reason.IsEmpty()
				? TEXT("mutator slot unavailable")
				: Acquire.Reason);
		return false;
	}

	bMutatorHeld = true;
	bGateOpen = true;
	return true;
}

FString FUeremcpMutatingDispatch::Complete(const FUeremcpResponse& Response)
{
	FUeremcpResponse FinalResponse = Response;
	FinalResponse.ProtocolVersion = FUeremcpEnvelope::ProtocolVersion();
	if (FinalResponse.RequestId.IsEmpty())
	{
		FinalResponse.RequestId = Request.RequestId;
	}

	TArray<FString> AuditNotes;
	if (!AppendAuditForResponse(FinalResponse, AuditNotes))
	{
		for (const FString& Note : AuditNotes)
		{
			FinalResponse.CapabilityNotes.AddUnique(Note);
		}
	}

	ReleaseMutator();
	bGateOpen = false;
	return FUeremcpEnvelope::SerializeResponse(FinalResponse);
}

void FUeremcpMutatingDispatch::ReleaseMutator()
{
	if (bMutatorHeld && !ProjectKey.IsEmpty() && !Request.RequestId.IsEmpty())
	{
		FUeremcpMutatorQueue::Release(ProjectKey, Request.RequestId);
	}
	bMutatorHeld = false;
}

bool FUeremcpMutatingDispatch::AppendAuditForResponse(
	const FUeremcpResponse& Response,
	TArray<FString>& OutNotes)
{
	if (!FUeremcpAuditLog::IsImplemented())
	{
		OutNotes.Add(TEXT("Audit log unavailable; terminal outcome not persisted."));
		return false;
	}

	FUeremcpAuditRecord Record;
	Record.TimestampUtc = FDateTime::UtcNow().ToIso8601();
	Record.RequestId = Response.RequestId.IsEmpty() ? Request.RequestId : Response.RequestId;
	Record.IdempotencyKey = Request.IdempotencyKey;
	Record.Action = Request.Action;
	Record.Mode = Request.Mode;
	Record.Status = Response.Status;
	Record.TargetAssetPath = Request.TargetAssetPath;
	Record.bDryRun = Verdict.bEffectiveDryRun;
	Record.bAtomic = Request.bAtomic;
	Record.RequiredTier = Verdict.RequiredTier;
	Record.ProjectPath = ProjectKey;
	Record.RevisionBefore = Request.ExpectedRevision;

	for (const FUeremcpAssetRef& Asset : Response.CreatedAssets)
	{
		Record.CreatedAssets.Add(Asset.AssetPath);
	}
	for (const FUeremcpAssetRef& Asset : Response.ModifiedAssets)
	{
		Record.ModifiedAssets.Add(Asset.AssetPath);
	}
	for (const FUeremcpAssetRef& Asset : Response.DeletedAssets)
	{
		Record.DeletedAssets.Add(Asset.AssetPath);
	}
	Record.RevisionAfter = Response.Revision;

	FString AuditError;
	if (!FUeremcpAuditLog::Append(Record, PathRoots, AuditError))
	{
		OutNotes.Add(FString::Printf(
			TEXT("Audit append failed: %s"),
			*AuditError));
		return false;
	}

	return true;
}
