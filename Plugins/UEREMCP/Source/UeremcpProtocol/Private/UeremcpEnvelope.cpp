#include "UeremcpEnvelope.h"

#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	const TCHAR* GProtocolVersion = TEXT("1.0");

	bool MatchActionPattern(const FString& Action)
	{
		// ^[a-z][a-z0-9_]*$
		if (Action.IsEmpty())
		{
			return false;
		}
		const TCHAR First = Action[0];
		if (First < TEXT('a') || First > TEXT('z'))
		{
			return false;
		}
		for (int32 I = 1; I < Action.Len(); ++I)
		{
			const TCHAR C = Action[I];
			const bool bOk = (C >= TEXT('a') && C <= TEXT('z'))
				|| (C >= TEXT('0') && C <= TEXT('9'))
				|| C == TEXT('_');
			if (!bOk)
			{
				return false;
			}
		}
		return true;
	}

	bool MatchProtocolVersionPattern(const FString& Version)
	{
		// ^[0-9]+\.[0-9]+$
		int32 Dot = INDEX_NONE;
		if (!Version.FindChar(TEXT('.'), Dot) || Dot <= 0 || Dot >= Version.Len() - 1)
		{
			return false;
		}
		for (int32 I = 0; I < Version.Len(); ++I)
		{
			if (I == Dot)
			{
				continue;
			}
			if (Version[I] < TEXT('0') || Version[I] > TEXT('9'))
			{
				return false;
			}
		}
		return true;
	}

	bool GetMajor(const FString& Version, int32& OutMajor)
	{
		int32 Dot = INDEX_NONE;
		if (!Version.FindChar(TEXT('.'), Dot) || Dot <= 0)
		{
			return false;
		}
		OutMajor = FCString::Atoi(*Version.Left(Dot));
		return true;
	}

	void SetBoolDefault(const TSharedPtr<FJsonObject>& Options, const TCHAR* Field, bool Default, bool& Out)
	{
		if (Options.IsValid() && Options->HasField(Field))
		{
			Out = Options->GetBoolField(Field);
		}
		else
		{
			Out = Default;
		}
	}

	TSharedPtr<FJsonObject> AssetRefToJson(const FUeremcpAssetRef& Ref)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("asset_path"), Ref.AssetPath);
		if (!Ref.AssetClass.IsEmpty())
		{
			Obj->SetStringField(TEXT("asset_class"), Ref.AssetClass);
		}
		if (!Ref.Revision.IsEmpty())
		{
			Obj->SetStringField(TEXT("revision"), Ref.Revision);
		}
		if (!Ref.Role.IsEmpty())
		{
			Obj->SetStringField(TEXT("role"), Ref.Role);
		}
		return Obj;
	}

	void AppendAssetRefs(TSharedPtr<FJsonObject>& Parent, const TCHAR* Field, const TArray<FUeremcpAssetRef>& Refs)
	{
		if (Refs.Num() == 0)
		{
			return;
		}
		TArray<TSharedPtr<FJsonValue>> Arr;
		for (const FUeremcpAssetRef& Ref : Refs)
		{
			Arr.Add(MakeShared<FJsonValueObject>(AssetRefToJson(Ref)));
		}
		Parent->SetArrayField(Field, Arr);
	}
}

FString FUeremcpEnvelope::ProtocolVersion()
{
	return GProtocolVersion;
}

bool FUeremcpEnvelope::IsProtocolCompatible(const FString& Other)
{
	if (!MatchProtocolVersionPattern(Other))
	{
		return false;
	}
	int32 Ours = 0;
	int32 Theirs = 0;
	if (!GetMajor(ProtocolVersion(), Ours) || !GetMajor(Other, Theirs))
	{
		return false;
	}
	return Ours == Theirs;
}

bool FUeremcpEnvelope::IsValidMode(const FString& Mode)
{
	static const TArray<FString> Modes = {
		TEXT("create"), TEXT("create_or_update"), TEXT("replace"), TEXT("patch"),
		TEXT("rebuild_from_specification"), TEXT("repair"), TEXT("delete")
	};
	return Modes.Contains(Mode);
}

bool FUeremcpEnvelope::IsValidStatus(const FString& Status)
{
	static const TArray<FString> Statuses = {
		TEXT("created_and_validated"), TEXT("modified_and_validated"),
		TEXT("created_with_warnings"), TEXT("no_change_required"),
		TEXT("failed_validation"), TEXT("rolled_back"), TEXT("partially_completed"),
		TEXT("rejected"), TEXT("error")
	};
	return Statuses.Contains(Status);
}

bool FUeremcpEnvelope::IsValidResponseDetail(const FString& Detail)
{
	static const TArray<FString> Details = {
		TEXT("minimal"), TEXT("summary"), TEXT("diagnostic"), TEXT("complete")
	};
	return Details.Contains(Detail);
}

bool FUeremcpEnvelope::IsValidRevisionConflictPolicy(const FString& Policy)
{
	static const TArray<FString> Policies = {
		TEXT("reject"), TEXT("return_conflict"), TEXT("merge"), TEXT("replace"), TEXT("force")
	};
	return Policies.Contains(Policy);
}

bool FUeremcpEnvelope::ParseRequest(const FString& Json, FUeremcpRequest& OutRequest, FString& OutError)
{
	OutRequest = FUeremcpRequest();
	OutError.Reset();

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = TEXT("request is not a JSON object");
		return false;
	}

	// additionalProperties: false — reject unknown top-level keys.
	static const TSet<FString> Allowed = {
		TEXT("protocol_version"), TEXT("request_id"), TEXT("action"), TEXT("project"),
		TEXT("target"), TEXT("mode"), TEXT("specification"), TEXT("options"),
		TEXT("expected_revision"), TEXT("idempotency_key")
	};
	for (const auto& Pair : Root->Values)
	{
		if (!Allowed.Contains(FString(Pair.Key)))
		{
			OutError = FString::Printf(TEXT("unknown top-level field '%s'"), *FString(Pair.Key));
			return false;
		}
	}

	if (!Root->HasField(TEXT("protocol_version")) || !Root->HasField(TEXT("action")))
	{
		OutError = TEXT("missing required field protocol_version or action");
		return false;
	}

	OutRequest.ProtocolVersion = Root->GetStringField(TEXT("protocol_version"));
	if (!MatchProtocolVersionPattern(OutRequest.ProtocolVersion))
	{
		OutError = FString::Printf(
			TEXT("protocol_version '%s' does not match MAJOR.MINOR"), *OutRequest.ProtocolVersion);
		return false;
	}

	OutRequest.Action = Root->GetStringField(TEXT("action"));
	if (!MatchActionPattern(OutRequest.Action))
	{
		OutError = FString::Printf(
			TEXT("action '%s' does not match ^[a-z][a-z0-9_]*$"), *OutRequest.Action);
		return false;
	}

	if (Root->HasField(TEXT("request_id")) && !Root->HasTypedField<EJson::Null>(TEXT("request_id")))
	{
		OutRequest.RequestId = Root->GetStringField(TEXT("request_id"));
		if (OutRequest.RequestId.IsEmpty() || OutRequest.RequestId.Len() > 128)
		{
			OutError = TEXT("request_id must be 1..128 characters");
			return false;
		}
	}

	if (Root->HasTypedField<EJson::Object>(TEXT("project")))
	{
		const TSharedPtr<FJsonObject> Project = Root->GetObjectField(TEXT("project"));
		Project->TryGetStringField(TEXT("path"), OutRequest.ProjectPath);
		Project->TryGetStringField(TEXT("engine_version"), OutRequest.EngineVersion);
	}

	if (Root->HasTypedField<EJson::Object>(TEXT("target")))
	{
		const TSharedPtr<FJsonObject> Target = Root->GetObjectField(TEXT("target"));
		static const TSet<FString> TargetAllowed = {
			TEXT("asset_path"), TEXT("object_path"), TEXT("graph_id"), TEXT("actor_label")
		};
		for (const auto& Pair : Target->Values)
		{
			if (!TargetAllowed.Contains(FString(Pair.Key)))
			{
				OutError = FString::Printf(TEXT("unknown target field '%s'"), *FString(Pair.Key));
				return false;
			}
		}
		Target->TryGetStringField(TEXT("asset_path"), OutRequest.TargetAssetPath);
		Target->TryGetStringField(TEXT("object_path"), OutRequest.TargetObjectPath);
		Target->TryGetStringField(TEXT("graph_id"), OutRequest.TargetGraphId);
		Target->TryGetStringField(TEXT("actor_label"), OutRequest.TargetActorLabel);
	}

	if (Root->HasField(TEXT("mode")) && !Root->HasTypedField<EJson::Null>(TEXT("mode")))
	{
		OutRequest.Mode = Root->GetStringField(TEXT("mode"));
		if (!IsValidMode(OutRequest.Mode))
		{
			OutError = FString::Printf(TEXT("invalid mode '%s'"), *OutRequest.Mode);
			return false;
		}
	}

	if (Root->HasTypedField<EJson::Object>(TEXT("specification")))
	{
		OutRequest.Specification = Root->GetObjectField(TEXT("specification"));
	}
	else if (Root->HasField(TEXT("specification")) && !Root->HasTypedField<EJson::Null>(TEXT("specification")))
	{
		OutError = TEXT("specification must be an object");
		return false;
	}

	if (Root->HasTypedField<EJson::Object>(TEXT("options")))
	{
		const TSharedPtr<FJsonObject> Options = Root->GetObjectField(TEXT("options"));
		static const TSet<FString> OptAllowed = {
			TEXT("dry_run"), TEXT("atomic"), TEXT("rollback_on_failure"), TEXT("compile"),
			TEXT("validate"), TEXT("save"), TEXT("response_detail"), TEXT("timeout_ms"),
			TEXT("on_revision_conflict"), TEXT("continue_on_error"), TEXT("allow_destructive"),
			TEXT("on_unsupported")
		};
		for (const auto& Pair : Options->Values)
		{
			if (!OptAllowed.Contains(FString(Pair.Key)))
			{
				OutError = FString::Printf(TEXT("unknown options field '%s'"), *FString(Pair.Key));
				return false;
			}
		}

		SetBoolDefault(Options, TEXT("dry_run"), false, OutRequest.bDryRun);
		SetBoolDefault(Options, TEXT("atomic"), true, OutRequest.bAtomic);
		SetBoolDefault(Options, TEXT("rollback_on_failure"), true, OutRequest.bRollbackOnFailure);
		SetBoolDefault(Options, TEXT("compile"), true, OutRequest.bCompile);
		SetBoolDefault(Options, TEXT("validate"), true, OutRequest.bValidate);
		SetBoolDefault(Options, TEXT("save"), true, OutRequest.bSave);
		SetBoolDefault(Options, TEXT("continue_on_error"), false, OutRequest.bContinueOnError);
		SetBoolDefault(Options, TEXT("allow_destructive"), false, OutRequest.bAllowDestructive);

		if (Options->HasField(TEXT("on_unsupported")))
		{
			OutRequest.OnUnsupported = Options->GetStringField(TEXT("on_unsupported"));
			if (!OutRequest.OnUnsupported.Equals(TEXT("fail"), ESearchCase::IgnoreCase)
				&& !OutRequest.OnUnsupported.Equals(TEXT("partial"), ESearchCase::IgnoreCase))
			{
				OutError = FString::Printf(
					TEXT("invalid on_unsupported '%s'; expected fail or partial"),
					*OutRequest.OnUnsupported);
				return false;
			}
		}

		if (Options->HasField(TEXT("response_detail")))
		{
			OutRequest.ResponseDetail = Options->GetStringField(TEXT("response_detail"));
			if (!IsValidResponseDetail(OutRequest.ResponseDetail))
			{
				OutError = FString::Printf(
					TEXT("invalid response_detail '%s'"), *OutRequest.ResponseDetail);
				return false;
			}
		}
		if (Options->HasField(TEXT("timeout_ms")))
		{
			OutRequest.TimeoutMs = static_cast<int32>(Options->GetNumberField(TEXT("timeout_ms")));
			if (OutRequest.TimeoutMs < 0)
			{
				OutError = TEXT("timeout_ms must be >= 0");
				return false;
			}
		}
		if (Options->HasField(TEXT("on_revision_conflict")))
		{
			OutRequest.OnRevisionConflict = Options->GetStringField(TEXT("on_revision_conflict"));
			if (!IsValidRevisionConflictPolicy(OutRequest.OnRevisionConflict))
			{
				OutError = FString::Printf(
					TEXT("invalid on_revision_conflict '%s'"), *OutRequest.OnRevisionConflict);
				return false;
			}
		}
	}

	if (Root->HasField(TEXT("expected_revision")))
	{
		if (Root->HasTypedField<EJson::Null>(TEXT("expected_revision")))
		{
			OutRequest.bHasExpectedRevision = false;
			OutRequest.ExpectedRevision.Reset();
		}
		else
		{
			OutRequest.ExpectedRevision = Root->GetStringField(TEXT("expected_revision"));
			OutRequest.bHasExpectedRevision = true;
		}
	}

	if (Root->HasField(TEXT("idempotency_key")) && !Root->HasTypedField<EJson::Null>(TEXT("idempotency_key")))
	{
		OutRequest.IdempotencyKey = Root->GetStringField(TEXT("idempotency_key"));
		if (OutRequest.IdempotencyKey.Len() > 128)
		{
			OutError = TEXT("idempotency_key maxLength is 128");
			return false;
		}
	}

	return true;
}

bool FUeremcpEnvelope::ValidateResponse(const FUeremcpResponse& Response, FString& OutError)
{
	OutError.Reset();
	const FString Version = Response.ProtocolVersion.IsEmpty()
		? ProtocolVersion()
		: Response.ProtocolVersion;
	if (!MatchProtocolVersionPattern(Version))
	{
		OutError = TEXT("response protocol_version invalid");
		return false;
	}
	if (!IsValidStatus(Response.Status))
	{
		OutError = FString::Printf(TEXT("invalid status '%s'"), *Response.Status);
		return false;
	}
	if (Response.Summary.IsEmpty())
	{
		OutError = TEXT("summary is required");
		return false;
	}
	if (Response.Metrics.McpRoundTrips < 0 || Response.Metrics.InternalOperations < 0)
	{
		OutError = TEXT("metrics counters must be >= 0");
		return false;
	}
	if (Response.bHasJob)
	{
		if (!Response.Job.IsValid(OutError))
		{
			return false;
		}
		// In-flight job handles on the initiating call use partially_completed
		// (ADR-0009). Terminal poll results may carry job.state completed|failed|
		// cancelled with a matching envelope status.
		if ((Response.Job.State.Equals(TEXT("running")) || Response.Job.State.Equals(TEXT("queued")))
			&& !Response.Status.Equals(TEXT("partially_completed"))
			&& !Response.Status.Equals(TEXT("error")))
		{
			OutError = TEXT("in-flight job handle requires status partially_completed (or error)");
			return false;
		}
	}
	return true;
}

namespace
{
	FUeremcpNextActionsProvider& NextActionsProvider()
	{
		static FUeremcpNextActionsProvider Provider;
		return Provider;
	}
}

void FUeremcpEnvelope::SetNextActionsProvider(FUeremcpNextActionsProvider Provider)
{
	NextActionsProvider() = MoveTemp(Provider);
}

void FUeremcpEnvelope::ClearNextActionsProvider()
{
	NextActionsProvider().Unbind();
}

FString FUeremcpEnvelope::SerializeResponse(const FUeremcpResponse& Response)
{
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(
		TEXT("protocol_version"),
		Response.ProtocolVersion.IsEmpty() ? ProtocolVersion() : Response.ProtocolVersion);

	if (!Response.RequestId.IsEmpty())
	{
		Root->SetStringField(TEXT("request_id"), Response.RequestId);
	}
	Root->SetStringField(TEXT("status"), Response.Status);
	Root->SetStringField(TEXT("summary"), Response.Summary);

	if (!Response.UnderstoodAction.IsEmpty() || !Response.UnderstoodTarget.IsEmpty()
		|| !Response.UnderstoodTemplate.IsEmpty() || Response.InterpretationNotes.Num() > 0)
	{
		TSharedPtr<FJsonObject> Understood = MakeShared<FJsonObject>();
		if (!Response.UnderstoodAction.IsEmpty())
		{
			Understood->SetStringField(TEXT("action"), Response.UnderstoodAction);
		}
		if (!Response.UnderstoodTarget.IsEmpty())
		{
			Understood->SetStringField(TEXT("resolved_target"), Response.UnderstoodTarget);
		}
		if (!Response.UnderstoodTemplate.IsEmpty())
		{
			Understood->SetStringField(TEXT("template_used"), Response.UnderstoodTemplate);
		}
		if (Response.InterpretationNotes.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> Notes;
			for (const FString& Note : Response.InterpretationNotes)
			{
				Notes.Add(MakeShared<FJsonValueString>(Note));
			}
			Understood->SetArrayField(TEXT("interpretation_notes"), Notes);
		}
		Root->SetObjectField(TEXT("understood"), Understood);
	}

	if (!Response.PrimaryAsset.IsEmpty()
		|| Response.CreatedAssets.Num() > 0
		|| Response.ModifiedAssets.Num() > 0
		|| Response.DeletedAssets.Num() > 0
		|| Response.ReusedAssets.Num() > 0
		|| Response.Dependencies.Num() > 0
		|| Response.UnresolvedDependencies.Num() > 0)
	{
		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		if (!Response.PrimaryAsset.IsEmpty())
		{
			Result->SetStringField(TEXT("primary_asset"), Response.PrimaryAsset);
		}
		AppendAssetRefs(Result, TEXT("created_assets"), Response.CreatedAssets);
		AppendAssetRefs(Result, TEXT("modified_assets"), Response.ModifiedAssets);
		AppendAssetRefs(Result, TEXT("deleted_assets"), Response.DeletedAssets);
		AppendAssetRefs(Result, TEXT("reused_assets"), Response.ReusedAssets);
		AppendAssetRefs(Result, TEXT("dependencies"), Response.Dependencies);
		AppendAssetRefs(Result, TEXT("unresolved_dependencies"), Response.UnresolvedDependencies);
		Root->SetObjectField(TEXT("result"), Result);
	}

	if (!Response.Revision.IsEmpty())
	{
		Root->SetStringField(TEXT("revision"), Response.Revision);
	}

	if (Response.bHasJob)
	{
		TSharedPtr<FJsonObject> JobObj = MakeShared<FJsonObject>();
		JobObj->SetStringField(TEXT("job_id"), Response.Job.JobId);
		JobObj->SetStringField(TEXT("state"), Response.Job.State);
		if (Response.Job.bHasProgress)
		{
			JobObj->SetNumberField(TEXT("progress"), Response.Job.Progress);
		}
		if (!Response.Job.ProgressMessage.IsEmpty())
		{
			JobObj->SetStringField(TEXT("progress_message"), Response.Job.ProgressMessage);
		}
		if (Response.Job.bHasCancellable)
		{
			JobObj->SetBoolField(TEXT("cancellable"), Response.Job.bCancellable);
		}
		const FString PollAction = Response.Job.PollAction.IsEmpty()
			? FString(FUeremcpJobDefaults::PollAction())
			: Response.Job.PollAction;
		JobObj->SetStringField(TEXT("poll_action"), PollAction);
		Root->SetObjectField(TEXT("job"), JobObj);
	}

	TSharedPtr<FJsonObject> Metrics = MakeShared<FJsonObject>();
	Metrics->SetNumberField(TEXT("mcp_round_trips"), Response.Metrics.McpRoundTrips);
	Metrics->SetNumberField(TEXT("internal_operations"), Response.Metrics.InternalOperations);
	if (Response.Metrics.TimingMs.Num() > 0)
	{
		TSharedPtr<FJsonObject> Timing = MakeShared<FJsonObject>();
		for (const auto& Pair : Response.Metrics.TimingMs)
		{
			Timing->SetNumberField(Pair.Key, Pair.Value);
		}
		Metrics->SetObjectField(TEXT("timing_ms"), Timing);
	}
	if (Response.Metrics.AssetsAffected > 0)
	{
		Metrics->SetNumberField(TEXT("assets_affected"), Response.Metrics.AssetsAffected);
	}
	if (Response.Metrics.bReplayed)
	{
		Metrics->SetBoolField(TEXT("replayed"), true);
	}
	Root->SetObjectField(TEXT("metrics"), Metrics);

	if (Response.CapabilityNotes.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> Notes;
		for (const FString& Note : Response.CapabilityNotes)
		{
			Notes.Add(MakeShared<FJsonValueString>(Note));
		}
		Root->SetArrayField(TEXT("capability_notes"), Notes);
	}

	{
		// Serve the next step with the result. Domains do not populate this;
		// asking every domain to know the whole graph is how the graph goes
		// stale in nine places at once.
		TArray<TSharedPtr<FJsonObject>> Suggestions = Response.NextActions;
		if (Suggestions.Num() == 0 && NextActionsProvider().IsBound())
		{
			Suggestions = NextActionsProvider().Execute(
				Response.UnderstoodAction, Response.PrimaryAsset, Response.Status);
		}
		if (Suggestions.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> Arr;
			for (const TSharedPtr<FJsonObject>& S : Suggestions)
			{
				if (S.IsValid()) Arr.Add(MakeShared<FJsonValueObject>(S));
			}
			Root->SetArrayField(TEXT("next_actions"), Arr);
		}
	}

	if (Response.ExtraFields.IsValid())
	{
		for (const auto& Pair : Response.ExtraFields->Values)
		{
			if (!Root->HasField(Pair.Key))
			{
				Root->SetField(Pair.Key, Pair.Value);
			}
		}
	}

	FString Out;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
	return Out;
}

FString FUeremcpEnvelope::MakeRejection(const FString& RequestId, const FString& Reason)
{
	FUeremcpResponse Response;
	Response.ProtocolVersion = ProtocolVersion();
	Response.RequestId = RequestId;
	Response.Status = TEXT("rejected");
	Response.Summary = Reason;
	Response.Metrics.McpRoundTrips = 1;
	Response.Metrics.InternalOperations = 0;

	// Echo the contract on every rejection.
	//
	// Agent-facing tools take a single opaque `requestJson` string, so MCP cannot
	// publish a JSON Schema for the envelope the way it does for Epic's typed
	// tools. Without this, a rejection is purely subtractive — it says which
	// field is wrong and never which fields are right — so an agent discovers
	// the shape one failed round trip at a time, or not at all.
	//
	// Measured 2026-07-30: reaching one successful dry-run call took 3 rejections
	// plus reading three schema files straight from the repo. An agent with no
	// repo access cannot get there.
	//
	// Rides in capability_notes rather than a new envelope field (ADR-0003), and
	// only on the error path, where per WHY.md payload is nearly free and the
	// round trip it saves is not.
	Response.CapabilityNotes.Add(TEXT(
		"envelope: top-level fields are protocol_version (required), action (required), "
		"request_id, target, specification, options, mode, project, expected_revision, "
		"idempotency_key. No others are accepted."));
	Response.CapabilityNotes.Add(TEXT(
		"common mistake: dry_run is options.dry_run, NOT top-level."));
	Response.CapabilityNotes.Add(TEXT(
		"minimal example: {\"protocol_version\":\"1.0\",\"action\":\"<action>\","
		"\"target\":{\"asset_path\":\"/Game/__UeremcpTests/Foo\"},"
		"\"options\":{\"dry_run\":true},\"specification\":{}}"));
	Response.CapabilityNotes.Add(TEXT(
		"next: fix the rejected field, or call UeremcpCore.UeremcpReferenceToolset.GetStarted "
		"then ResolveIntent for a worked request_json."));

	return SerializeResponse(Response);
}

FString FUeremcpEnvelope::MakeUnverified(const FString& RequestId, const FString& Summary,
                                         const TArray<FString>& CapabilityNotes)
{
	FUeremcpResponse Response;
	Response.ProtocolVersion = ProtocolVersion();
	Response.RequestId = RequestId;
	Response.Status = TEXT("partially_completed");
	Response.Summary = Summary;
	Response.CapabilityNotes = CapabilityNotes;
	Response.Metrics.McpRoundTrips = 1;
	Response.Metrics.InternalOperations = 0;
	return SerializeResponse(Response);
}

FString FUeremcpEnvelope::MakeJobTimeoutResponse(
	const FString& RequestId,
	const FString& JobId,
	const FString& ProgressMessage,
	int32 McpRoundTrips)
{
	FUeremcpResponse Response;
	Response.ProtocolVersion = ProtocolVersion();
	Response.RequestId = RequestId;
	Response.Status = TEXT("partially_completed");
	Response.Summary = ProgressMessage.IsEmpty()
		? TEXT("Operation exceeded timeout_ms; continuing in-process. Poll get_job_result.")
		: ProgressMessage;
	Response.bHasJob = true;
	Response.Job.JobId = JobId;
	Response.Job.State = TEXT("running");
	Response.Job.ProgressMessage = ProgressMessage;
	Response.Job.PollAction = FUeremcpJobDefaults::PollAction();
	// ADR-0009: do not claim cancellable until cooperative cancel is wired.
	Response.Job.bCancellable = false;
	Response.Job.bHasCancellable = true;
	Response.Metrics.McpRoundTrips = FMath::Max(1, McpRoundTrips);
	Response.Metrics.InternalOperations = 0;
	Response.CapabilityNotes.Add(
		TEXT("Long-running job: Epic MCP progress heartbeats are not percent-complete; "
			 "use job.progress / job.progress_message."));
	return SerializeResponse(Response);
}
