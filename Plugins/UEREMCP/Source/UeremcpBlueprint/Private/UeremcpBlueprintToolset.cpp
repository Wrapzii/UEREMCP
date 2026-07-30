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
	Response.Status = TEXT("partially_completed");
	Response.Summary = TEXT(
		"submit_graph is not implemented (P2). Use read_graph for inspection; replace/patch write path pending.");
	Response.UnderstoodAction = Request.Action;
	Response.UnderstoodTarget = Request.TargetAssetPath;
	Response.CapabilityNotes = {
		TEXT("submit_graph.replace_not_implemented"),
		TEXT("submit_graph.patch_not_implemented"),
	};
	Response.Metrics.McpRoundTrips = 1;
	Response.Metrics.InternalOperations = 0;

	return FUeremcpEnvelope::SerializeResponse(Response);
}
