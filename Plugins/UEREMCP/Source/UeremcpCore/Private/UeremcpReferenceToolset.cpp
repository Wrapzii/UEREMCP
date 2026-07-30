#include "UeremcpReferenceToolset.h"

#include "UeremcpEnvelope.h"
#include "UeremcpIntentRouter.h"
#include "UeremcpJobActions.h"
#include "UeremcpPlanActions.h"

namespace
{
	FString FinishRouterResponse(
		const FString& RequestId,
		const FString& Action,
		const FUeremcpIntentRouterResult& RouterResult)
	{
		FUeremcpResponse Response;
		Response.ProtocolVersion = FUeremcpEnvelope::ProtocolVersion();
		Response.RequestId = RequestId;
		Response.Status = RouterResult.Status;
		Response.Summary = RouterResult.Summary;
		Response.UnderstoodAction = Action;
		Response.CapabilityNotes = RouterResult.CapabilityNotes;
		Response.Metrics.McpRoundTrips = 1;
		Response.Metrics.InternalOperations = 0;
		if (RouterResult.Payload.IsValid())
		{
			Response.ExtraFields = MakeShared<FJsonObject>();
			Response.ExtraFields->SetObjectField(TEXT("result"), RouterResult.Payload);
		}
		return FUeremcpEnvelope::SerializeResponse(Response);
	}

	bool ParseEnvelopeOrReject(
		const FString& RequestJson,
		const FString& ExpectedAction,
		FUeremcpRequest& OutRequest,
		FString& OutRejectionJson)
	{
		FString ParseError;
		if (!FUeremcpEnvelope::ParseRequest(RequestJson, OutRequest, ParseError))
		{
			OutRejectionJson = FUeremcpEnvelope::MakeRejection(
				FString(),
				FString::Printf(TEXT("Malformed request envelope: %s"), *ParseError));
			return false;
		}
		if (!FUeremcpEnvelope::IsProtocolCompatible(OutRequest.ProtocolVersion))
		{
			OutRejectionJson = FUeremcpEnvelope::MakeRejection(
				OutRequest.RequestId,
				FString::Printf(
					TEXT("Unsupported protocol_version '%s'; this server speaks %s."),
					*OutRequest.ProtocolVersion,
					*FUeremcpEnvelope::ProtocolVersion()));
			return false;
		}
		if (!OutRequest.Action.Equals(ExpectedAction, ESearchCase::IgnoreCase))
		{
			OutRejectionJson = FUeremcpEnvelope::MakeRejection(
				OutRequest.RequestId,
				FString::Printf(
					TEXT("Expected action=%s, got '%s'."),
					*ExpectedAction,
					*OutRequest.Action));
			return false;
		}
		return true;
	}
}

FString UUeremcpReferenceToolset::GetStarted(const FString& RequestJson)
{
	FUeremcpRequest Request;
	FString Rejection;
	if (!ParseEnvelopeOrReject(RequestJson, TEXT("get_started"), Request, Rejection))
	{
		return Rejection;
	}
	FString Detail = TEXT("summary");
	if (Request.Specification.IsValid())
	{
		Request.Specification->TryGetStringField(TEXT("detail"), Detail);
	}
	return FinishRouterResponse(
		Request.RequestId, Request.Action, FUeremcpIntentRouter::GetStarted(Detail));
}

FString UUeremcpReferenceToolset::ResolveIntent(const FString& RequestJson)
{
	FUeremcpRequest Request;
	FString Rejection;
	if (!ParseEnvelopeOrReject(RequestJson, TEXT("resolve_intent"), Request, Rejection))
	{
		return Rejection;
	}
	if (!Request.Specification.IsValid())
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId, TEXT("specification.intent is required"));
	}
	FString Intent;
	if (!Request.Specification->TryGetStringField(TEXT("intent"), Intent) || Intent.IsEmpty())
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId, TEXT("specification.intent is required"));
	}
	FString Mode = TEXT("recommend");
	Request.Specification->TryGetStringField(TEXT("mode"), Mode);
	FString ExpectedHash;
	Request.Specification->TryGetStringField(TEXT("expected_registry_hash"), ExpectedHash);
	int32 MaxSteps = 6;
	if (Request.Specification->HasField(TEXT("max_steps")))
	{
		MaxSteps = static_cast<int32>(Request.Specification->GetNumberField(TEXT("max_steps")));
	}
	const TSharedPtr<FJsonObject>* Context = nullptr;
	TSharedPtr<FJsonObject> ContextObj;
	if (Request.Specification->TryGetObjectField(TEXT("context"), Context) && Context)
	{
		ContextObj = *Context;
	}
	return FinishRouterResponse(
		Request.RequestId,
		Request.Action,
		FUeremcpIntentRouter::ResolveIntent(Intent, Mode, ContextObj, ExpectedHash, MaxSteps));
}

FString UUeremcpReferenceToolset::DescribeOperation(const FString& RequestJson)
{
	FUeremcpRequest Request;
	FString Rejection;
	if (!ParseEnvelopeOrReject(RequestJson, TEXT("describe_operation"), Request, Rejection))
	{
		return Rejection;
	}
	FString Tool;
	if (!Request.Specification.IsValid()
		|| !Request.Specification->TryGetStringField(TEXT("tool"), Tool)
		|| Tool.IsEmpty())
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId, TEXT("specification.tool is required"));
	}
	return FinishRouterResponse(
		Request.RequestId, Request.Action, FUeremcpIntentRouter::DescribeOperation(Tool));
}

FString UUeremcpReferenceToolset::Ping()
{
	FUeremcpResponse Response;
	Response.ProtocolVersion = FUeremcpEnvelope::ProtocolVersion();
	Response.Status = TEXT("no_change_required");
	Response.Summary = FString::Printf(
		TEXT("UEREMCP reference toolset alive, protocol %s. START HERE: GetStarted then ResolveIntent."),
		*FUeremcpEnvelope::ProtocolVersion());
	Response.Metrics.McpRoundTrips = 1;
	Response.Metrics.InternalOperations = 0;
	return FUeremcpEnvelope::SerializeResponse(Response);
}

FString UUeremcpReferenceToolset::Echo(const FString& RequestJson)
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
	Response.ProtocolVersion = FUeremcpEnvelope::ProtocolVersion();
	Response.RequestId = Request.RequestId;
	Response.Status = TEXT("no_change_required");
	Response.Summary = FString::Printf(
		TEXT("Echoed request for action '%s'. No editor state was touched."),
		*Request.Action);
	Response.UnderstoodAction = Request.Action;
	Response.UnderstoodTarget = Request.TargetAssetPath;
	Response.Metrics.McpRoundTrips = 1;
	Response.Metrics.InternalOperations = 0;
	return FUeremcpEnvelope::SerializeResponse(Response);
}

FString UUeremcpReferenceToolset::ExecutePlan(const FString& RequestJson)
{
	return FUeremcpPlanActions::ExecutePlan(RequestJson);
}

FString UUeremcpReferenceToolset::GetJobResult(const FString& RequestJson)
{
	return FUeremcpJobActions::GetJobResult(RequestJson);
}

FString UUeremcpReferenceToolset::CancelJob(const FString& RequestJson)
{
	return FUeremcpJobActions::CancelJob(RequestJson);
}
