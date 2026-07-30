// UEREMCP — Niagara domain toolset (WS-07).

#include "UeremcpNiagaraToolset.h"

#include "UeremcpEnvelope.h"
#include "UeremcpNiagaraCapabilityNotes.h"
#include "UeremcpNiagaraInspect.h"

FString UUeremcpNiagaraToolset::Echo(const FString& RequestJson)
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
		TEXT("Niagara toolset echoed request for action '%s'. No editor state was touched."),
		*Request.Action);
	Response.UnderstoodAction = Request.Action;
	Response.UnderstoodTarget = Request.TargetAssetPath;
	Response.Metrics.McpRoundTrips = 1;
	Response.Metrics.InternalOperations = 0;

	return FUeremcpEnvelope::SerializeResponse(Response);
}

FString UUeremcpNiagaraToolset::InspectSystem(const FString& RequestJson)
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

	if (!Request.Action.Equals(TEXT("inspect_system"), ESearchCase::CaseSensitive))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(
				TEXT("inspect_system tool received action '%s'. Use action 'inspect_system' or call Echo for protocol checks."),
				*Request.Action));
	}

	if (Request.TargetAssetPath.IsEmpty())
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			TEXT("inspect_system requires target.asset_path (Niagara system package path, e.g. /Game/__UeremcpTests/NS_WS07_Probe)."));
	}

	if (!FUeremcpNiagaraInspect::IsAllowedProbePath(Request.TargetAssetPath))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			TEXT("inspect_system probes only assets under /Game/__UeremcpTests/."));
	}

	FUeremcpNiagaraInspectSpec Spec;
	FString SpecError;
	if (!FUeremcpNiagaraInspect::ParseSpecification(Request.Specification, Spec, SpecError))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(TEXT("Invalid inspect_system specification: %s"), *SpecError));
	}

	FUeremcpNiagaraInspectResult InspectResult;
	if (!FUeremcpNiagaraInspect::Run(Request, Spec, InspectResult))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			InspectResult.Error.IsEmpty() ? TEXT("inspect_system failed.") : InspectResult.Error);
	}

	FUeremcpResponse Response;
	Response.RequestId = Request.RequestId;
	Response.Status = TEXT("partially_completed");
	Response.Summary = InspectResult.Summary;
	Response.UnderstoodAction = Request.Action;
	Response.UnderstoodTarget = Request.TargetAssetPath;
	Response.PrimaryAsset = Request.TargetAssetPath;
	Response.CapabilityNotes = UeremcpNiagaraCapability::DefaultInspectCapabilityNotes();
	Response.Metrics.McpRoundTrips = 1;
	Response.Metrics.InternalOperations = InspectResult.InternalOperations;

	TSharedPtr<FJsonObject> Extra = MakeShared<FJsonObject>();

	TSharedPtr<FJsonObject> Diagnostics = MakeShared<FJsonObject>();
	const bool bIncludeGraphs = !Request.ResponseDetail.Equals(TEXT("minimal"), ESearchCase::IgnoreCase);
	if (bIncludeGraphs && InspectResult.Graphs.Num() > 0)
	{
		Diagnostics->SetArrayField(TEXT("graphs"), InspectResult.Graphs);
	}
	if (InspectResult.ExecutionTrace.Num() > 0
		&& (Request.ResponseDetail.Equals(TEXT("diagnostic"), ESearchCase::IgnoreCase)
			|| Request.ResponseDetail.Equals(TEXT("complete"), ESearchCase::IgnoreCase)))
	{
		Diagnostics->SetArrayField(TEXT("execution_trace"), InspectResult.ExecutionTrace);
	}
	if (Diagnostics->Values.Num() > 0)
	{
		Extra->SetObjectField(TEXT("diagnostics"), Diagnostics);
	}

	TSharedPtr<FJsonObject> Validation = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> ChecksPerformed;
	for (const FString& Check : InspectResult.ChecksPerformed)
	{
		ChecksPerformed.Add(MakeShared<FJsonValueString>(Check));
	}
	Validation->SetArrayField(TEXT("checks_performed"), ChecksPerformed);

	TArray<TSharedPtr<FJsonValue>> ChecksSkipped;
	for (const FString& Check : InspectResult.ChecksSkipped)
	{
		ChecksSkipped.Add(MakeShared<FJsonValueString>(Check));
	}
	Validation->SetArrayField(TEXT("checks_skipped"), ChecksSkipped);

	if (InspectResult.bCompiled.IsSet())
	{
		Validation->SetBoolField(TEXT("compiled"), InspectResult.bCompiled.GetValue());
	}
	else
	{
		Validation->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>());
	}
	Extra->SetObjectField(TEXT("validation"), Validation);

	Response.ExtraFields = Extra;
	return FUeremcpEnvelope::SerializeResponse(Response);
}
