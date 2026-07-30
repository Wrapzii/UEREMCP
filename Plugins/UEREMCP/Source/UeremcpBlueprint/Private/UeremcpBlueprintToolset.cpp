#include "UeremcpBlueprintToolset.h"

#include "UeremcpBlueprintGraphReader.h"
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
		const TArray<FString>& Skipped)
	{
		if (!Response.ExtraFields.IsValid())
		{
			Response.ExtraFields = MakeShared<FJsonObject>();
		}
		TSharedPtr<FJsonObject> Validation = MakeShared<FJsonObject>();
		Validation->SetBoolField(TEXT("structurally_valid"), bStructurallyValid);

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
		Response.ExtraFields->SetObjectField(TEXT("validation"), Validation);
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
		TEXT("Read Blueprint graph from '%s' (revision %s). Round-trip replace not implemented (P2)."),
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

	return FUeremcpEnvelope::SerializeResponse(Response);
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

	if (!Request.Mode.Equals(TEXT("replace"), ESearchCase::CaseSensitive))
	{
		Response.Status = TEXT("rejected");
		Response.Summary = FString::Printf(
			TEXT("submit_graph mode '%s' is not implemented; the current P2 slice accepts unchanged replace submissions only."),
			*Request.Mode);
		Response.CapabilityNotes = {
			TEXT("submit_graph.changed_replace_not_implemented"),
			TEXT("submit_graph.patch_not_implemented"),
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

	FString LoadError;
	UBlueprint* Blueprint = LoadBlueprintAsset(Request.TargetAssetPath, LoadError);
	if (!Blueprint)
	{
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, LoadError);
	}

	FString SubmittedGraphId;
	SubmittedGraph->TryGetStringField(TEXT("graph_id"), SubmittedGraphId);
	FUeremcpBlueprintReadGraphOptions ReadOptions;
	ReadOptions.GraphId = !Request.TargetGraphId.IsEmpty()
		? Request.TargetGraphId
		: (!SubmittedGraphId.IsEmpty()
			? SubmittedGraphId
			: ReadGraphSpecificationString(Request, TEXT("graph_id")));
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

	if (SubmittedHash.Equals(Current.ContentHash, ESearchCase::CaseSensitive))
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
		return FUeremcpEnvelope::SerializeResponse(Response);
	}

	Response.Status = TEXT("rejected");
	Response.Summary = TEXT(
		"Changed graph replacement is not implemented in this P2 slice; no mutation was performed.");
	Response.CapabilityNotes = {
		TEXT("submit_graph.unchanged_replace_supported"),
		TEXT("submit_graph.changed_replace_not_implemented"),
		TEXT("submit_graph.patch_not_implemented"),
	};
	AttachSubmitValidation(
		Response,
		true,
		{TEXT("blueprint.current_graph_read"), TEXT("blueprint.submitted_graph_hash_compare")},
		{TEXT("blueprint.graph_write"), TEXT("blueprint.compile"), TEXT("blueprint.reread_after_write")});
	return FUeremcpEnvelope::SerializeResponse(Response);
}
