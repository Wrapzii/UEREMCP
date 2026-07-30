#include "UeremcpBlueprintToolset.h"

#include "UeremcpEnvelope.h"

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
