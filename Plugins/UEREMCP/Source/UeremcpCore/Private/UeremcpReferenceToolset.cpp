// UEREMCP — reference toolset implementation.
//
// SCAFFOLD — NOT YET COMPILED. See Plugins/UEREMCP/README.md.

#include "UeremcpReferenceToolset.h"

#include "UeremcpEnvelope.h"

FString UUeremcpReferenceToolset::Ping()
{
	FUeremcpResponse Response;
	Response.Status = TEXT("no_change_required");
	Response.Summary = FString::Printf(
		TEXT("UEREMCP reference toolset alive, protocol %s."),
		*FUeremcpEnvelope::ProtocolVersion());

	// Metrics are mandatory on every response (ADR-0003 rule 3). Even here — a
	// metric that is optional does not get measured, and round-trip reduction is
	// this project's headline claim.
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
		// A malformed request is `rejected`, not `error`, and never an exception.
		// The agent gets a usable explanation rather than a stack trace.
		return FUeremcpEnvelope::MakeRejection(
			/*RequestId*/ FString(),
			FString::Printf(TEXT("Malformed request envelope: %s"), *ParseError));
	}

	// Major-version mismatch is rejected outright, never best-effort parsed
	// (ADR-0003 rule 4).
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
		TEXT("Echoed request for action '%s'. No editor state was touched."),
		*Request.Action);

	// `understood` is how an agent sees a misinterpretation without paying for a
	// second round trip. Populating it is not optional politeness — see docs/WHY.md
	// on why front-loading context is close to free.
	Response.UnderstoodAction = Request.Action;
	Response.UnderstoodTarget = Request.TargetAssetPath;

	Response.Metrics.McpRoundTrips = 1;
	Response.Metrics.InternalOperations = 0;

	return FUeremcpEnvelope::SerializeResponse(Response);
}
