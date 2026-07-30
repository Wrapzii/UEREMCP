#include "UeremcpReferenceToolset.h"

#include "UeremcpEnvelope.h"
#include "UeremcpJobActions.h"

FString UUeremcpReferenceToolset::Ping()
{
	FUeremcpResponse Response;
	Response.ProtocolVersion = FUeremcpEnvelope::ProtocolVersion();
	Response.Status = TEXT("no_change_required");
	Response.Summary = FString::Printf(
		TEXT("UEREMCP reference toolset alive, protocol %s."),
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

FString UUeremcpReferenceToolset::GetJobResult(const FString& RequestJson)
{
	return FUeremcpJobActions::GetJobResult(RequestJson);
}

FString UUeremcpReferenceToolset::CancelJob(const FString& RequestJson)
{
	return FUeremcpJobActions::CancelJob(RequestJson);
}
