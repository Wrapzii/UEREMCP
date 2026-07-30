#include "UeremcpBlueprintToolset.h"

#include "UeremcpBlueprintGraphReader.h"
#include "UeremcpBlueprintGraphWriter.h"
#include "UeremcpBlueprintMutatingGate.h"
#include "UeremcpEnvelope.h"

#include "Engine/Blueprint.h"
#include "UObject/SoftObjectPath.h"

namespace UeremcpBlueprintToolset
{
	static bool ParseAndValidateRequest(
		const FString& RequestJson,
		const FString& ExpectedAction,
		FUeremcpRequest& OutRequest,
		FString& OutError)
	{
		if (!FUeremcpEnvelope::ParseRequest(RequestJson, OutRequest, OutError))
		{
			return false;
		}

		if (!FUeremcpEnvelope::IsProtocolCompatible(OutRequest.ProtocolVersion))
		{
			OutError = FString::Printf(
				TEXT("Unsupported protocol_version '%s'; this server speaks %s."),
				*OutRequest.ProtocolVersion,
				*FUeremcpEnvelope::ProtocolVersion());
			return false;
		}

		if (!OutRequest.Action.Equals(ExpectedAction, ESearchCase::CaseSensitive))
		{
			OutError = FString::Printf(
				TEXT("expected action '%s', got '%s'"),
				*ExpectedAction,
				*OutRequest.Action);
			return false;
		}

		return true;
	}

	static UBlueprint* LoadBlueprintAsset(const FString& AssetPath, FString& OutError)
	{
		if (AssetPath.IsEmpty())
		{
			OutError = TEXT("target.asset_path is required");
			return nullptr;
		}

		UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
		if (!Blueprint)
		{
			const FSoftObjectPath SoftPath(AssetPath);
			Blueprint = Cast<UBlueprint>(SoftPath.TryLoad());
		}

		if (!Blueprint)
		{
			OutError = FString::Printf(TEXT("Blueprint not found at '%s'"), *AssetPath);
		}

		return Blueprint;
	}

	static FString ReadGraphSpecificationString(
		const FUeremcpRequest& Request,
		const TCHAR* Field,
		const FString& Fallback = FString())
	{
		if (Request.Specification.IsValid() && Request.Specification->HasField(Field))
		{
			return Request.Specification->GetStringField(Field);
		}
		return Fallback;
	}

	static bool ReadGraphSpecificationBool(
		const FUeremcpRequest& Request,
		const TCHAR* Field,
		bool bDefault)
	{
		if (Request.Specification.IsValid() && Request.Specification->HasField(Field))
		{
			return Request.Specification->GetBoolField(Field);
		}
		return bDefault;
	}

	static void AttachGraphDiagnostics(
		FUeremcpResponse& Response,
		const TSharedPtr<FJsonObject>& Graph)
	{
		if (!Graph.IsValid())
		{
			return;
		}
		if (!Response.ExtraFields.IsValid())
		{
			Response.ExtraFields = MakeShared<FJsonObject>();
		}
		TArray<TSharedPtr<FJsonValue>> Graphs;
		Graphs.Add(MakeShared<FJsonValueObject>(Graph));
		TSharedPtr<FJsonObject> Diagnostics = MakeShared<FJsonObject>();
		Diagnostics->SetArrayField(TEXT("graphs"), Graphs);
		Response.ExtraFields->SetObjectField(TEXT("diagnostics"), Diagnostics);
	}

	static void AttachSubmitValidation(
		FUeremcpResponse& Response,
		bool bStructurallyValid,
		const TArray<FString>& Performed,
		const TArray<FString>& Skipped,
		bool bRereadAfterWrite = false)
	{
		if (!Response.ExtraFields.IsValid())
		{
			Response.ExtraFields = MakeShared<FJsonObject>();
		}
		TSharedPtr<FJsonObject> Validation = MakeShared<FJsonObject>();
		Validation->SetBoolField(TEXT("structurally_valid"), bStructurallyValid);
		Validation->SetBoolField(TEXT("reread_after_write"), bRereadAfterWrite);

		TArray<TSharedPtr<FJsonValue>> Checks;
		for (const FString& Check : Performed)
		{
			Checks.Add(MakeShared<FJsonValueString>(Check));
		}
		Validation->SetArrayField(TEXT("checks_performed"), Checks);

		TArray<TSharedPtr<FJsonValue>> SkippedChecks;
		for (const FString& Check : Skipped)
		{
			SkippedChecks.Add(MakeShared<FJsonValueString>(Check));
		}
		Validation->SetArrayField(TEXT("checks_skipped"), SkippedChecks);
		Validation->SetBoolField(
			TEXT("reread_after_write"),
			Performed.Contains(TEXT("blueprint.reread_after_write")));
		Response.ExtraFields->SetObjectField(TEXT("validation"), Validation);
	}

	static void AttachSubmitWriteEvidence(
		FUeremcpResponse& Response,
		const FUeremcpBlueprintReplaceGraphResult& WriteResult,
		bool bCompileRequested,
		bool bSaveRequested)
	{
		const TSharedPtr<FJsonObject>* Validation = nullptr;
		if (Response.ExtraFields.IsValid()
			&& Response.ExtraFields->TryGetObjectField(TEXT("validation"), Validation)
			&& Validation
			&& Validation->IsValid())
		{
			if (bCompileRequested)
			{
				(*Validation)->SetBoolField(TEXT("compiled"), WriteResult.bCompiled);
			}
			else
			{
				(*Validation)->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>());
			}
			if (bSaveRequested)
			{
				(*Validation)->SetBoolField(TEXT("saved"), WriteResult.bSaved);
			}
			else
			{
				(*Validation)->SetField(TEXT("saved"), MakeShared<FJsonValueNull>());
			}
		}

		AttachGraphDiagnostics(Response, WriteResult.RereadGraph);
	}

	static void AttachDiagnostic(
		FUeremcpResponse& Response,
		const FString& Severity,
		const FString& Code,
		const FString& Message,
		const FString& Remediation)
	{
		if (!Response.ExtraFields.IsValid())
		{
			Response.ExtraFields = MakeShared<FJsonObject>();
		}

		TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(TEXT("severity"), Severity);
		Item->SetStringField(TEXT("code"), Code);
		Item->SetStringField(TEXT("message"), Message);
		if (!Remediation.IsEmpty())
		{
			Item->SetStringField(TEXT("remediation"), Remediation);
		}

		TArray<TSharedPtr<FJsonValue>> Items;
		Items.Add(MakeShared<FJsonValueObject>(Item));
		TSharedPtr<FJsonObject> Diagnostics = MakeShared<FJsonObject>();
		Diagnostics->SetArrayField(TEXT("items"), Items);
		Response.ExtraFields->SetObjectField(TEXT("diagnostics"), Diagnostics);
	}
}

using namespace UeremcpBlueprintToolset;

FString UUeremcpBlueprintToolset::Ping()
{
	FUeremcpResponse Response;
	Response.Status = TEXT("no_change_required");
	Response.Summary = FString::Printf(
		TEXT("UEREMCP Blueprint toolset alive, protocol %s."),
		*FUeremcpEnvelope::ProtocolVersion());

	Response.Metrics.McpRoundTrips = 1;
	Response.Metrics.InternalOperations = 0;

	return FUeremcpEnvelope::SerializeResponse(Response);
}

FString UUeremcpBlueprintToolset::Echo(const FString& RequestJson)
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

	FUeremcpResponse Response;
	Response.RequestId = Request.RequestId;
	Response.Status = TEXT("no_change_required");
	Response.Summary = FString::Printf(
		TEXT("Echoed Blueprint-domain request for action '%s'. No editor state was touched."),
		*Request.Action);

	Response.UnderstoodAction = Request.Action;
	Response.UnderstoodTarget = Request.TargetAssetPath;

	Response.Metrics.McpRoundTrips = 1;
	Response.Metrics.InternalOperations = 0;

	return FUeremcpEnvelope::SerializeResponse(Response);
}

FString UUeremcpBlueprintToolset::ReadGraph(const FString& RequestJson)
{
	FUeremcpRequest Request;
	FString Error;
	if (!ParseAndValidateRequest(RequestJson, TEXT("read_graph"), Request, Error))
	{
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, Error);
	}

	FUeremcpBlueprintMutatingGate ReadGate;
	FString BlockingResponse;
	if (!ReadGate.TryBeginRead(RequestJson, BlockingResponse))
	{
		return BlockingResponse;
	}

	FString LoadError;
	UBlueprint* Blueprint = LoadBlueprintAsset(Request.TargetAssetPath, LoadError);
	if (!Blueprint)
	{
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, LoadError);
	}

	FUeremcpBlueprintReadGraphOptions Options;
	Options.GraphId = !Request.TargetGraphId.IsEmpty()
		? Request.TargetGraphId
		: ReadGraphSpecificationString(Request, TEXT("graph_id"));
	Options.bIncludeDsl = ReadGraphSpecificationBool(Request, TEXT("include_dsl"), false);
	Options.ResponseDetail = Request.ResponseDetail;

	FUeremcpBlueprintReadGraphResult ReadResult;
	if (!FUeremcpBlueprintGraphReader::ReadGraph(
			Blueprint,
			Request.TargetAssetPath,
			Options,
			ReadResult))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			ReadResult.Error.IsEmpty() ? TEXT("read_graph failed") : ReadResult.Error);
	}

	const bool bCompletePayload =
		Request.ResponseDetail.Equals(TEXT("complete"), ESearchCase::IgnoreCase)
		|| Request.ResponseDetail.Equals(TEXT("diagnostic"), ESearchCase::IgnoreCase);

	FUeremcpResponse Response;
	Response.RequestId = Request.RequestId;
	Response.Status = TEXT("no_change_required");
	Response.Summary = FString::Printf(
		TEXT("Read Blueprint graph from '%s' (revision %s)."),
		*Request.TargetAssetPath,
		*ReadResult.ContentHash);
	Response.UnderstoodAction = Request.Action;
	Response.UnderstoodTarget = Request.TargetAssetPath;
	Response.PrimaryAsset = Request.TargetAssetPath;
	Response.Revision = ReadResult.ContentHash;
	Response.CapabilityNotes = FUeremcpBlueprintGraphReader::DefaultLossyAreas();
	Response.Metrics.McpRoundTrips = 1;
	Response.Metrics.InternalOperations = ReadResult.InternalOperations;

	Response.ExtraFields = MakeShared<FJsonObject>();

	if (bCompletePayload && ReadResult.Graph.IsValid())
	{
		TArray<TSharedPtr<FJsonValue>> Graphs;
		Graphs.Add(MakeShared<FJsonValueObject>(ReadResult.Graph));

		TSharedPtr<FJsonObject> Diagnostics = MakeShared<FJsonObject>();
		Diagnostics->SetArrayField(TEXT("graphs"), Graphs);
		Response.ExtraFields->SetObjectField(TEXT("diagnostics"), Diagnostics);
	}
	else if (ReadResult.Graph.IsValid())
	{
		TSharedPtr<FJsonObject> Diagnostics = MakeShared<FJsonObject>();
		Diagnostics->SetBoolField(TEXT("truncated"), true);
		Diagnostics->SetStringField(
			TEXT("truncation_hint"),
			TEXT("Set options.response_detail to complete for full graph payload in diagnostics.graphs."));
		Response.ExtraFields->SetObjectField(TEXT("diagnostics"), Diagnostics);
	}

	TSharedPtr<FJsonObject> Validation = MakeShared<FJsonObject>();
	Validation->SetBoolField(TEXT("structurally_valid"), true);
	TArray<TSharedPtr<FJsonValue>> Checks;
	Checks.Add(MakeShared<FJsonValueString>(TEXT("blueprint.graph_read")));
	Checks.Add(MakeShared<FJsonValueString>(TEXT("blueprint.diagnostics_walk")));
	Validation->SetArrayField(TEXT("checks_performed"), Checks);
	TArray<TSharedPtr<FJsonValue>> Skipped;
	Skipped.Add(MakeShared<FJsonValueString>(TEXT("blueprint.reread_after_write")));
	Validation->SetArrayField(TEXT("checks_skipped"), Skipped);
	Response.ExtraFields->SetObjectField(TEXT("validation"), Validation);

	return ReadGate.Complete(Response);
}

FString UUeremcpBlueprintToolset::SubmitGraph(const FString& RequestJson)
{
	FUeremcpRequest Request;
	FString Error;
	if (!ParseAndValidateRequest(RequestJson, TEXT("submit_graph"), Request, Error))
	{
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, Error);
	}

	FUeremcpResponse Response;
	Response.RequestId = Request.RequestId;
	Response.UnderstoodAction = Request.Action;
	Response.UnderstoodTarget = Request.TargetAssetPath;
	Response.PrimaryAsset = Request.TargetAssetPath;
	Response.Metrics.McpRoundTrips = 1;

	if (Request.Mode.Equals(TEXT("patch"), ESearchCase::CaseSensitive))
	{
		Response.Status = TEXT("rejected");
		Response.Summary =
			TEXT("submit_graph mode 'patch' is unavailable because the Blueprint domain schema does not yet define typed semantic operation payloads; no mutation was performed.");
		Response.CapabilityNotes = {
			TEXT("submit_graph.patch_contract_undefined"),
			TEXT("submit_graph.replace_supported"),
			TEXT("submit_graph.no_mutation"),
		};
		AttachDiagnostic(
			Response,
			TEXT("error"),
			TEXT("blueprint.patch_contract_undefined"),
			TEXT("ADR-0004 names semantic patching, but submit_graph.schema.json currently constrains only operation names and does not define their required operands or verification semantics."),
			TEXT("Submit a complete graph with mode=replace, or wait for an accepted typed Blueprint patch specification."));
		AttachSubmitValidation(
			Response,
			false,
			{TEXT("blueprint.submit_mode_dispatch")},
			{
				TEXT("blueprint.current_graph_read"),
				TEXT("blueprint.expected_revision_compare"),
				TEXT("blueprint.graph_write"),
				TEXT("blueprint.compile"),
				TEXT("blueprint.reread_after_write"),
			});
		return FUeremcpEnvelope::SerializeResponse(Response);
	}

	if (!Request.Mode.Equals(TEXT("replace"), ESearchCase::CaseSensitive))
	{
		Response.Status = TEXT("rejected");
		Response.Summary = FString::Printf(
			TEXT("submit_graph mode '%s' is unsupported; the current Blueprint implementation supports replace on scratch assets only."),
			*Request.Mode);
		Response.CapabilityNotes = {
			TEXT("submit_graph.unsupported_mode"),
		};
		return FUeremcpEnvelope::SerializeResponse(Response);
	}

	const TSharedPtr<FJsonObject>* SubmittedGraphPtr = nullptr;
	if (!Request.Specification.IsValid()
		|| !Request.Specification->TryGetObjectField(TEXT("graph"), SubmittedGraphPtr)
		|| !SubmittedGraphPtr
		|| !SubmittedGraphPtr->IsValid())
	{
		Response.Status = TEXT("rejected");
		Response.Summary = TEXT("specification.graph is required for mode=replace.");
		return FUeremcpEnvelope::SerializeResponse(Response);
	}
	const TSharedPtr<FJsonObject> SubmittedGraph = *SubmittedGraphPtr;

	FString SubmittedGraphId;
	SubmittedGraph->TryGetStringField(TEXT("graph_id"), SubmittedGraphId);
	const FString TargetGraphId = !Request.TargetGraphId.IsEmpty()
		? Request.TargetGraphId
		: (!SubmittedGraphId.IsEmpty()
			? SubmittedGraphId
			: ReadGraphSpecificationString(Request, TEXT("graph_id")));

	FString ValidateError;
	TArray<FString> ValidateNotes;
	if (!FUeremcpBlueprintGraphWriter::ValidateSubmittedGraphForReplace(
			SubmittedGraph,
			Request.TargetAssetPath,
			TargetGraphId,
			ValidateError,
			ValidateNotes,
			false))
	{
		Response.Status = TEXT("failed_validation");
		Response.Summary = ValidateError;
		Response.CapabilityNotes = ValidateNotes;
		TArray<FString> PerformedChecks = {TEXT("blueprint.submitted_graph_structure")};
		if (ValidateNotes.Contains(TEXT("submit_graph.dsl_required")))
		{
			PerformedChecks.Add(TEXT("blueprint.dsl_resolution"));
		}
		AttachSubmitValidation(
			Response,
			false,
			PerformedChecks,
			{
				TEXT("blueprint.current_graph_read"),
				TEXT("blueprint.expected_revision_compare"),
				TEXT("blueprint.submitted_graph_hash_compare"),
				TEXT("blueprint.graph_write"),
				TEXT("blueprint.compile"),
				TEXT("blueprint.reread_after_write"),
			});
		return FUeremcpEnvelope::SerializeResponse(Response);
	}

	FString LoadError;
	UBlueprint* Blueprint = LoadBlueprintAsset(Request.TargetAssetPath, LoadError);
	if (!Blueprint)
	{
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, LoadError);
	}

	FUeremcpBlueprintReadGraphOptions ReadOptions;
	ReadOptions.GraphId = TargetGraphId;
	ReadOptions.ResponseDetail = TEXT("complete");

	FUeremcpBlueprintReadGraphResult Current;
	if (!FUeremcpBlueprintGraphReader::ReadGraph(
			Blueprint,
			Request.TargetAssetPath,
			ReadOptions,
			Current))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			Current.Error.IsEmpty() ? TEXT("failed to read current graph") : Current.Error);
	}
	Response.Revision = Current.ContentHash;
	Response.Metrics.InternalOperations = Current.InternalOperations;

	const bool bBypassConflict =
		Request.OnRevisionConflict.Equals(TEXT("replace"), ESearchCase::CaseSensitive)
		|| Request.OnRevisionConflict.Equals(TEXT("force"), ESearchCase::CaseSensitive);
	if (Request.bHasExpectedRevision
		&& !Request.ExpectedRevision.Equals(Current.ContentHash, ESearchCase::CaseSensitive)
		&& !bBypassConflict)
	{
		Response.Status = TEXT("rejected");
		Response.Summary = FString::Printf(
			TEXT("expected_revision '%s' does not match current revision '%s'; no mutation was performed."),
			*Request.ExpectedRevision,
			*Current.ContentHash);
		Response.CapabilityNotes.Add(TEXT("revision_conflict.no_mutation"));
		if (Request.OnRevisionConflict.Equals(TEXT("return_conflict"), ESearchCase::CaseSensitive))
		{
			AttachGraphDiagnostics(Response, Current.Graph);
		}
		AttachSubmitValidation(
			Response,
			true,
			{TEXT("blueprint.current_graph_read"), TEXT("blueprint.expected_revision_compare")},
			{TEXT("blueprint.graph_write"), TEXT("blueprint.compile"), TEXT("blueprint.reread_after_write")});
		return FUeremcpEnvelope::SerializeResponse(Response);
	}

	FString SubmittedHashError;
	const FString SubmittedHash =
		FUeremcpBlueprintGraphReader::ComputeContentHash(SubmittedGraph, &SubmittedHashError);
	if (SubmittedHash.IsEmpty())
	{
		Response.Status = TEXT("failed_validation");
		Response.Summary = FString::Printf(
			TEXT("Submitted graph could not be hashed: %s"),
			*SubmittedHashError);
		AttachSubmitValidation(
			Response,
			false,
			{TEXT("blueprint.current_graph_read")},
			{TEXT("blueprint.graph_write"), TEXT("blueprint.compile"), TEXT("blueprint.reread_after_write")});
		return FUeremcpEnvelope::SerializeResponse(Response);
	}

	if (SubmittedHash.Equals(Current.ContentHash, ESearchCase::CaseSensitive)
		&& !FUeremcpBlueprintGraphWriter::WriteIntentDiffers(SubmittedGraph, Current.Graph))
	{
		Response.Status = TEXT("no_change_required");
		Response.Summary = FString::Printf(
			TEXT("Submitted Blueprint graph already matches '%s' at revision %s; no mutation or compile was needed."),
			*Request.TargetAssetPath,
			*Current.ContentHash);
		Response.CapabilityNotes = FUeremcpBlueprintGraphReader::DefaultLossyAreas();
		AttachSubmitValidation(
			Response,
			true,
			{
				TEXT("blueprint.current_graph_read"),
				TEXT("blueprint.expected_revision_compare"),
				TEXT("blueprint.submitted_graph_hash_compare"),
			},
			{TEXT("blueprint.graph_write"), TEXT("blueprint.compile"), TEXT("blueprint.reread_after_write")});
		AttachGraphDiagnostics(Response, Current.Graph);
		return FUeremcpEnvelope::SerializeResponse(Response);
	}

	if (!FUeremcpBlueprintGraphWriter::IsScratchAssetPath(Request.TargetAssetPath))
	{
		Response.Status = TEXT("rejected");
		Response.Summary = FString::Printf(
			TEXT("Changed graph replace for '%s' is restricted to /Game/__UeremcpTests/ or /Game/__UeremcpPoc/ scratch assets; no mutation was performed."),
			*Request.TargetAssetPath);
		Response.CapabilityNotes = {
			TEXT("submit_graph.scratch_path_only"),
			TEXT("submit_graph.unchanged_replace_supported"),
		};
		AttachSubmitValidation(
			Response,
			true,
			{
				TEXT("blueprint.current_graph_read"),
				TEXT("blueprint.expected_revision_compare"),
				TEXT("blueprint.submitted_graph_hash_compare"),
			},
			{TEXT("blueprint.graph_write"), TEXT("blueprint.compile"), TEXT("blueprint.reread_after_write")});
		return FUeremcpEnvelope::SerializeResponse(Response);
	}

	FUeremcpBlueprintReplaceGraphOptions WriteOptions;
	WriteOptions.AssetPath = Request.TargetAssetPath;
	WriteOptions.GraphId = ReadOptions.GraphId;
	WriteOptions.bDryRun = Request.bDryRun;
	WriteOptions.bCompile = Request.bCompile;
	WriteOptions.bValidate = Request.bValidate;
	WriteOptions.bSave = Request.bSave;
	const TSharedPtr<FJsonObject>* ExpectedAfterWrite = nullptr;
	if (Request.Specification->TryGetObjectField(TEXT("expected_after_write"), ExpectedAfterWrite)
		&& ExpectedAfterWrite
		&& ExpectedAfterWrite->IsValid())
	{
		WriteOptions.ExpectedAfterWrite = *ExpectedAfterWrite;
	}

	TOptional<FUeremcpBlueprintMutatingGate> MutatingGate;
	if (!Request.bDryRun)
	{
		MutatingGate.Emplace();
		FString DispatchBlockingResponse;
		if (!MutatingGate->TryBeginMutating(RequestJson, true, DispatchBlockingResponse))
		{
			return DispatchBlockingResponse;
		}
	}

	auto FinishSubmitResponse = [&MutatingGate](const FUeremcpResponse& Resp) -> FString
	{
		if (MutatingGate.IsSet() && MutatingGate->IsActive())
		{
			return MutatingGate->Complete(Resp);
		}
		return FUeremcpEnvelope::SerializeResponse(Resp);
	};

	FUeremcpBlueprintReplaceGraphResult WriteResult;
	if (!FUeremcpBlueprintGraphWriter::ReplaceGraph(
			Blueprint,
			SubmittedGraph,
			WriteOptions,
			WriteResult))
	{
		Response.Status = TEXT("failed_validation");
		Response.Summary = WriteResult.Error.IsEmpty()
			? TEXT("Graph replace failed before validation completed.")
			: WriteResult.Error;
		Response.CapabilityNotes = WriteResult.CapabilityNotes;
		Response.CapabilityNotes.Append(WriteResult.LossyAreas);
		Response.Metrics.InternalOperations += WriteResult.InternalOperations;
		TArray<FString> FailedPerformedChecks = {
			TEXT("blueprint.current_graph_read"),
			TEXT("blueprint.submitted_graph_hash_compare"),
		};
		TArray<FString> FailedSkippedChecks = {
			TEXT("blueprint.graph_write"),
			TEXT("blueprint.compile"),
			TEXT("blueprint.reread_after_write"),
		};
		if (WriteResult.bRereadAfterWrite)
		{
			FailedPerformedChecks.Add(TEXT("blueprint.graph_write"));
			FailedPerformedChecks.Add(TEXT("blueprint.compile"));
			FailedPerformedChecks.Add(TEXT("blueprint.reread_after_write"));
			FailedSkippedChecks.Reset();
			if (WriteResult.bExpectedStructureChecked)
			{
				FailedPerformedChecks.Add(TEXT("blueprint.expected_nodes_and_links"));
			}
		}
		AttachSubmitValidation(
			Response,
			false,
			FailedPerformedChecks,
			FailedSkippedChecks,
			WriteResult.bRereadAfterWrite);
		return FinishSubmitResponse(Response);
	}

	Response.Metrics.InternalOperations += WriteResult.InternalOperations;
	Response.CapabilityNotes = WriteResult.LossyAreas;
	Response.CapabilityNotes.Append(WriteResult.CapabilityNotes);

	if (Request.bDryRun)
	{
		Response.Status = TEXT("partially_completed");
		Response.Summary = FString::Printf(
			TEXT("dry_run: would replace graph '%s' on scratch asset '%s' via write_graph_dsl (%d bytes DSL); no mutation, compile, or re-read was performed."),
			ReadOptions.GraphId.IsEmpty() ? TEXT("EventGraph") : *ReadOptions.GraphId,
			*Request.TargetAssetPath,
			WriteResult.DslUsed.Len());
		Response.Revision = Current.ContentHash;
		AttachSubmitValidation(
			Response,
			true,
			{
				TEXT("blueprint.current_graph_read"),
				TEXT("blueprint.expected_revision_compare"),
				TEXT("blueprint.submitted_graph_hash_compare"),
				TEXT("blueprint.dsl_resolution"),
			},
			{TEXT("blueprint.graph_write"), TEXT("blueprint.compile"), TEXT("blueprint.reread_after_write")});
		return FUeremcpEnvelope::SerializeResponse(Response);
	}

	Response.Revision = WriteResult.RevisionAfter;

	TArray<FString> PerformedChecks = {
		TEXT("blueprint.current_graph_read"),
		TEXT("blueprint.expected_revision_compare"),
		TEXT("blueprint.submitted_graph_hash_compare"),
		TEXT("blueprint.graph_write"),
		TEXT("blueprint.reread_after_write"),
	};
	TArray<FString> SkippedChecks;
	if (Request.bCompile)
	{
		PerformedChecks.Add(TEXT("blueprint.compile"));
	}
	else
	{
		SkippedChecks.Add(TEXT("blueprint.compile"));
	}

	const bool bHashMatches = WriteResult.RereadHash.Equals(SubmittedHash, ESearchCase::CaseSensitive);
	const bool bExpectedStructureValidated =
		WriteResult.bExpectedStructureChecked && WriteResult.bExpectedStructureMatches;
	if (bExpectedStructureValidated)
	{
		PerformedChecks.Add(TEXT("blueprint.expected_nodes_and_links"));
	}
	if (Request.bValidate && Request.bCompile && (bHashMatches || bExpectedStructureValidated))
	{
		Response.Status = TEXT("modified_and_validated");
		if (bHashMatches)
		{
			Response.Summary = FString::Printf(
				TEXT("Replaced graph on scratch asset '%s'; compile succeeded and re-read semantic hash matches submitted graph."),
				*Request.TargetAssetPath);
			PerformedChecks.Add(TEXT("blueprint.submitted_vs_reread_hash_compare"));
		}
		else
		{
			Response.Summary = FString::Printf(
				TEXT("Replaced graph on scratch asset '%s'; compile succeeded and programmatic re-read confirmed all expected nodes and links."),
				*Request.TargetAssetPath);
			PerformedChecks.Add(TEXT("blueprint.submitted_vs_reread_hash_compare"));
			Response.CapabilityNotes.Add(TEXT("submit_graph.reread_hash_mismatch_structure_validated"));
		}
	}
	else
	{
		Response.Status = TEXT("partially_completed");
		if (!Request.bValidate)
		{
			Response.Summary = FString::Printf(
				TEXT("Replaced graph on scratch asset '%s' but options.validate=false; compile/re-read were not used for a validated claim."),
				*Request.TargetAssetPath);
			SkippedChecks.Add(TEXT("blueprint.submitted_vs_reread_hash_compare"));
		}
		else if (!Request.bCompile)
		{
			Response.Summary = FString::Printf(
				TEXT("Replaced graph on scratch asset '%s' but options.compile=false; cannot claim modified_and_validated."),
				*Request.TargetAssetPath);
			SkippedChecks.Add(TEXT("blueprint.submitted_vs_reread_hash_compare"));
		}
		else
		{
			Response.Summary = FString::Printf(
				TEXT("Replaced graph on scratch asset '%s' and re-read, but re-read hash '%s' does not match submitted hash '%s'."),
				*Request.TargetAssetPath,
				*WriteResult.RereadHash,
				*SubmittedHash);
			PerformedChecks.Add(TEXT("blueprint.submitted_vs_reread_hash_compare"));
			Response.CapabilityNotes.Add(TEXT("submit_graph.reread_hash_mismatch"));
		}
	}

	if (Request.bValidate && !Request.bCompile)
	{
		SkippedChecks.Add(TEXT("blueprint.submitted_vs_reread_hash_compare"));
	}

	if (MutatingGate.IsSet() && MutatingGate->IsActive())
	{
		PerformedChecks.Add(TEXT("core_mutating_dispatch_admitted"));
		Response.CapabilityNotes.Add(
			TEXT("FUeremcpMutatingDispatch owns permission, path, queue, audit, and release when enabled."));
	}

	AttachSubmitValidation(
		Response,
		WriteResult.bSuccess,
		PerformedChecks,
		SkippedChecks,
		WriteResult.bRereadAfterWrite);
	AttachSubmitWriteEvidence(
		Response,
		WriteResult,
		Request.bCompile,
		Request.bSave);
	return FinishSubmitResponse(Response);
}
